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
#include <unistd.h>

#include <strophe.h>

/***** config.c *****/

struct Config {
    const char* jid;
    const char* password;
    const char* peer_jid;
    bool        show_delayed_messages;
    bool        drop_privileges;
    xmpp_ctx_t* ctx;
    bool        connected;
    bool        connecting;
    struct IO*  io;
};

///Read config from environment
bool config_init(struct Config* cfg);
///Check argc/argv for configuration options. This consumes them, leaving only
///the positional arguments after them in argc/argv.
bool config_consume_options(struct Config* cfg, int* argc, char*** argv);

/***** security.c *****/

///Setup the security context for the application. Returns false on error.
bool sec_init(const struct Config* cfg);

/***** subprocess.c *****/

///If argc/argv are non-empty, launch a child process with that command line,
///and setup stdin/stdout as a bidirectional pipe to the child process.
///On success, return the PID of the child process in @a pid, or 0 if no child
///process was launched because argv was empty.
bool subprocess_init(int argc, char** argv, pid_t* pid);

/***** io.c *****/

struct Buffer {
    char* buffer;
    size_t size, capacity;
};

struct IO {
    int in_fd, out_fd;
    struct Buffer in_buf, out_buf;
    bool eof;
};

///Setup an empty @a buffer to read from the given @a fd.
///@return false on error
bool io_init(struct IO* io, int in_fd, int out_fd);

///Wait for at most @a usec milliseconds, and perform a single read() on the @a
///in_fd and a write() on the @a out_fd if they become available within the
///wait period.
///On error, return false. The error is reported to stderr.
///Otherwise, return true. On EOF of @a in_fd, also set @a eof.
bool io_select(struct IO* io, int usec);

///Copy the given data into the write buffer of the given @a io. The data will
///be written on the IO's out_fd when the out_fd is available for writing the
///next time.
void io_write(struct IO* io, const char* data, size_t count);

///If @a buffer contains a whole line (including the "\n"), remove the line
///from the buffer and return it (with the "\n" trimmed). If multiple whole
///lines are available, remove them all from the buffer and return them
///(separated by "\n", but with the trailing "\n" removed).
///
///Returns NULL if no full line is available. The caller must free() the
///returned pointer.
char* io_getlines(struct IO* io);

/***** jid.c *****/

bool validate_jid(const char* jid);

///Return whether the actual JID matches the expected one.
///If the @a actual_jid has a resource, then the @a expected_jid must also have
///that resource. If the @a actual_jid does not have a resource, then the @a
///expected_jid may have any (or no resource).
bool match_jid(const char* actual_jid, const char* expected_jid);

#endif // XMPP_BRIDGE_H
