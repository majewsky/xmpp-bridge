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

#ifndef XMPP_BRIDGE_H
#define XMPP_BRIDGE_H

#include <stdbool.h>
#include <stddef.h>

#include <strophe.h>

/***** config.c *****/

struct Config {
    const char* jid;
    const char* password;
    const char* peer_jid;
    bool        show_delayed_messages;
    xmpp_ctx_t* ctx;
    bool        connected;
};

bool config_init(struct Config* cfg, int argc, char** argv);

/***** io.c *****/

struct ReadBuffer {
    int fd;
    char* buffer;
    size_t size, capacity;
    bool eof;
};

///Setup an empty @a buffer to read from the given @a fd.
void readbuffer_init(struct ReadBuffer* buffer, int fd);

///Perform a single read() on the given @a fd, but timeout after @usec microseconds.
///On error, return false. The error is reported to stderr.
///Otherwise, return true. On EOF, also set @a buffer->eof.
bool readbuffer_read(struct ReadBuffer* buffer, int usec);

///If @a buffer contains a whole line (including the "\n"), remove the line
///from the buffer and return it (with the "\n" trimmed). Returns NULL if no
///full line is available. The caller must free() the returned pointer.
char* readbuffer_getline(struct ReadBuffer* buffer);

/***** jid.c *****/

bool validate_jid(const char* jid);

///Return whether the actual JID matches the expected one.
///If the @a actual_jid has a resource, then the @a expected_jid must also have
///that resource. If the @a actual_jid does not have a resource, then the @a
///expected_jid may have any (or no resource).
bool match_jid(const char* actual_jid, const char* expected_jid);

#endif // XMPP_BRIDGE_H
