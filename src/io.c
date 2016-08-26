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

void readbuffer_init(struct ReadBuffer* buf, int fd) {
    buf->fd       = fd;
    buf->buffer   = NULL; //allocated on first use
    buf->size     = 0;
    buf->capacity = 0;
    buf->eof      = false;
}

bool readbuffer_read(struct ReadBuffer* buf, int usec) {
    if (buf->eof) {
        return false;
    }

    //wait for fd to become available for reading
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(buf->fd, &fds);
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
    if (buf->capacity < buf->size + READ_SIZE) {
        buf->capacity = buf->size + READ_SIZE;
        buf->buffer   = realloc(buf->buffer, buf->capacity + 1); //+1 for NUL byte
    }

    //read into the buffer
    const ssize_t bytes_read = read(buf->fd,
                                    buf->buffer + buf->size,
                                    buf->capacity - buf->size);
    if (bytes_read == -1) {
        if (errno == EINTR) {
            //restart call (the select() will return immediately, so restarting
            //at the top is not so bad)
            return readbuffer_read(buf, usec);
        }
        perror("read()");
        return false;
    }
    else if (bytes_read == 0) {
        //EOF reached - return true once more to send the last (potentially
        //unterminated) line
        buf->eof = true;
        return true;
    }
    else {
        buf->size += bytes_read;
        buf->buffer[buf->size] = '\0'; //ensure that buffer is NUL-terminated
        return true;
    }
}

char* readbuffer_getline(struct ReadBuffer* buf) {
    if (buf->buffer == NULL) {
        return NULL;
    }

    //find line terminator
    char* nl_pos = strchr(buf->buffer, '\n');
    if (nl_pos == NULL) {
        //no full line - wait for the rest
        if (!buf->eof) {
            return NULL;
        }

        //after EOF, return the rest of the buffer
        if (buf->size == 0) {
            return NULL;
        }
        char* result = buf->buffer;
        buf->buffer = NULL;
        buf->size = buf->capacity = 0;
        return result;
    }

    //prepare return value
    const size_t result_size = nl_pos - buf->buffer;
    char* result = (char*) malloc(result_size + 1); //+1 for NUL byte
    memcpy(result, buf->buffer, result_size);
    result[result_size] = '\0';

    //remove this line from the buffer
    const size_t remove_size = result_size + 1; //+1 for '\n'
    buf->size -= remove_size;
    memmove(buf->buffer, buf->buffer + remove_size, buf->size + 1); //+1 for NUL byte
    //TODO: shrink buffer?

    //return only non-empty lines
    if (result_size == 0) {
        free(result);
        return NULL;
    }
    return result;
}
