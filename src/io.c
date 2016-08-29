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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>

#define READ_SIZE 4096

void io_init(struct IO* io, int in_fd, int out_fd) {
    io->in_fd           = in_fd;
    io->out_fd          = out_fd;
    io->in_buf.buffer   = NULL; //allocated on first use
    io->in_buf.size     = 0;
    io->in_buf.capacity = 0;
    io->eof             = false;
}

bool io_read(struct IO* io, int usec) {
    if (io->eof) {
        return false;
    }

    //wait for fd to become available for reading
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(io->in_fd, &fds);
    struct timeval tv;
    tv.tv_sec  = usec / 1000000;
    tv.tv_usec = usec % 1000000;

    const int retval = select(1, &fds, NULL, NULL, &tv);
    if (retval == -1) {
        perror("select()");
        return false;
    }
    else if (retval == 0) {
        //nothing to read
        return true;
    }

    //make sure that the buffer has at least READ_SIZE capacity
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
            //restart call (the select() will return immediately, so restarting
            //at the top is not so bad)
            return io_read(io, usec);
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

char* io_getline(struct IO* io) {
    if (io->in_buf.buffer == NULL) {
        return NULL;
    }

    //find line terminator
    char* nl_pos = strchr(io->in_buf.buffer, '\n');
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
    //TODO: shrink buffer?

    //return only non-empty lines
    if (result_size == 0) {
        free(result);
        return NULL;
    }
    return result;
}

void io_write(struct IO* io, const char* data, size_t count) {
    //TODO: stub
    write(io->out_fd, data, count);
}
