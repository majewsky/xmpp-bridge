/*******************************************************************************
*
* Copyright 2016 Stefan Majewsky <majewsky@gmx.net>
*
* This program is free software: you can redistribute it and/or modify it
* under the terms of the GNU General Public License as published
* by the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*
*******************************************************************************/

#include "xmpp-bridge.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define READ_SIZE   (1<<12)
#define GROW_STEP   (1<<12)
#define SHRINK_STEP (1<<20)

static void buf_try_shrink(struct Buffer* buf) {
    //shrink buffer if it has grown very large
    if (buf->capacity > buf->size + SHRINK_STEP) {
        //leave some room to grow again without reallocation
        //(NOTE: GROW_STEP needs to be at least 1 to account for the trailing
        //NUL byte in in_buf that's not counted by in_buf.size)
        buf->capacity = buf->size + GROW_STEP;
        buf->buffer   = realloc(buf->buffer, buf->capacity);
    }
}

bool io_init(struct IO* io, int in_fd, int out_fd) {
    io->in_fd            = in_fd;
    io->out_fd           = out_fd;
    io->in_buf.buffer    = NULL; //allocated on first use
    io->in_buf.size      = 0;
    io->in_buf.capacity  = 0;
    io->out_buf.buffer   = NULL; //allocated on first use
    io->out_buf.size     = 0;
    io->out_buf.capacity = 0;
    io->eof              = false;

    //try to make out_fd nonblocking, which will be useful
    const int flags = fcntl(out_fd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl() on stdout");
        return false;
    }
    if (flags & O_NONBLOCK) {
        //nothing to do
        return true;
    }
    const int result = fcntl(out_fd, F_SETFL, flags | O_NONBLOCK);
    if (result == -1) {
        perror("Cannot set stdout as non-blocking");
        fputs("Non-fatal: Will continue, but might block during writes to stdout.\n", stderr);
    }
    return true;
}

static bool io_perform_read(struct IO* io) {
    //make sure that the buffer has at least READ_SIZE additional capacity
    if (io->in_buf.capacity < io->in_buf.size + READ_SIZE) {
        io->in_buf.capacity = io->in_buf.size + READ_SIZE;
        io->in_buf.buffer   = realloc(io->in_buf.buffer, io->in_buf.capacity + 1); //+1 for NUL byte
    }

    //read into the buffer
    const ssize_t bytes_read = read(io->in_fd,
                                    io->in_buf.buffer + io->in_buf.size,
                                    io->in_buf.capacity - io->in_buf.size);
    if (bytes_read == -1) {
        if (errno == EINTR) {
            //restart call
            return io_perform_read(io);
        }
        perror("read()");
        return false;
    }
    else if (bytes_read == 0) {
        //EOF reached - return true once more to send the last (potentially
        //unterminated) line
        io->eof = true;
        return true;
    }
    else {
        io->in_buf.size += bytes_read;
        io->in_buf.buffer[io->in_buf.size] = '\0'; //ensure that buffer is NUL-terminated
        return true;
    }
}

static bool io_perform_write(struct IO* io, size_t write_count) {
    if (write_count == 0) {
        //invalid argument
        return false;
    }

    //bound `write_count` to the amount of available data
    if (write_count > io->out_buf.size) {
        write_count = io->out_buf.size;
    }

    const ssize_t bytes_written = write(io->out_fd, io->out_buf.buffer, write_count);
    if (bytes_written == -1) {
        if (errno == EINTR) {
            //restart call
            return io_perform_write(io, write_count);
        }
        else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            //write too large - restart call with smaller write_count
            return io_perform_write(io, write_count / 2);
        }
        else {
                perror("write()");
                return false;
        }
    }

    //remove the bytes_written from the buffer
    io->out_buf.size -= bytes_written;
    memmove(io->out_buf.buffer, io->out_buf.buffer + bytes_written, io->out_buf.size);
    buf_try_shrink(&(io->out_buf));
    return true;
}

bool io_select(struct IO* io, int usec) {
    if (io->eof) {
        return false;
    }

    //wait for in_fd to become available for reading, and for out_fd to become
    //available for writing (if there is stuff in the write buffer)
    fd_set in_fds, out_fds;
    FD_ZERO(&in_fds);
    FD_ZERO(&out_fds);
    FD_SET(io->in_fd, &in_fds);
    int fd_count = 1;
    if (io->out_buf.size > 0) {
        FD_SET(io->out_fd, &out_fds);
        fd_count = 2;
    }

    struct timeval tv;
    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;

    const int retval = select(fd_count, &in_fds, &out_fds, NULL, &tv);
    if (retval == -1) {
        perror("select()");
        return false;
    }

    //perform all IO operations that have become possible
    if (FD_ISSET(io->in_fd, &in_fds)) {
        if (!io_perform_read(io)) {
            return false;
        }
    }
    if (FD_ISSET(io->out_fd, &out_fds)) {
        if (!io_perform_write(io, sysconf(_SC_PAGESIZE))) {
            return false;
        }
    }
    return true;
}

char* io_getlines(struct IO* io) {
    if (io->in_buf.buffer == NULL) {
        return NULL;
    }

    //find line terminator
    char* nl_pos = strrchr(io->in_buf.buffer, '\n');
    if (nl_pos == NULL) {
        //no full line - wait for the rest
        if (!io->eof) {
            return NULL;
        }

        //after EOF, return the rest of the buffer
        if (io->in_buf.size == 0) {
            return NULL;
        }
        char* result = io->in_buf.buffer;
        io->in_buf.buffer = NULL;
        io->in_buf.size = io->in_buf.capacity = 0;
        return result;
    }

    //prepare return value
    const size_t result_size = nl_pos - io->in_buf.buffer;
    char* result = (char*) malloc(result_size + 1); //+1 for NUL byte
    memcpy(result, io->in_buf.buffer, result_size);
    result[result_size] = '\0';

    //remove this line from the buffer
    const size_t remove_size = result_size + 1; //+1 for '\n'
    io->in_buf.size -= remove_size;
    memmove(io->in_buf.buffer, io->in_buf.buffer + remove_size, io->in_buf.size + 1); //+1 for NUL byte
    buf_try_shrink(&(io->in_buf));

    //return only non-empty lines
    if (result_size == 0) {
        free(result);
        return NULL;
    }
    return result;
}

void io_write(struct IO* io, const char* data, size_t count) {
    //extend write buffer if necessary
    if (io->out_buf.capacity < io->out_buf.size + count) {
        //grow a bit bigger than needed to avoid repeated reallocation
        io->out_buf.capacity = io->out_buf.size + count + GROW_STEP;
        io->out_buf.buffer   = realloc(io->out_buf.buffer, io->out_buf.capacity);
    }

    //append data to buffer
    memcpy(io->out_buf.buffer + io->out_buf.size, data, count);
    io->out_buf.size += count;
}
