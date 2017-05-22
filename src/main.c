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

#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strophe.h>

#ifdef RELEASE
#   define MY_LOG_LEVEL XMPP_LEVEL_INFO
#else
#   define MY_LOG_LEVEL XMPP_LEVEL_DEBUG
#endif

void send_presence(xmpp_conn_t* conn, const struct Config* cfg) {
    //send <presence/> to appear online to contacts
    xmpp_stanza_t* pres = xmpp_stanza_new(cfg->ctx);
    xmpp_stanza_set_name(pres, "presence");
    xmpp_send(conn, pres);
    xmpp_stanza_release(pres);
}

int message_handler(xmpp_conn_t* const conn, xmpp_stanza_t* const stanza, void* const userdata) {
    (void) conn;
    //userdata contains a Config struct
    struct Config* cfg = (struct Config*) userdata;

    if (!cfg->show_delayed_messages) {
        //check for delayed messages formatted according to XEP-0091
        xmpp_stanza_t* delay_marker = xmpp_stanza_get_child_by_name(stanza, "x");
        if (delay_marker != NULL) {
            const char* ns = xmpp_stanza_get_ns(delay_marker);
            if (ns != NULL && strcmp(ns, "jabber:x:delay") == 0) {
                return 1;
            }
        }
        //check for delayed messages formatted according to XEP-0203
        delay_marker = xmpp_stanza_get_child_by_name(stanza, "delay");
        if (delay_marker != NULL) {
            const char* ns = xmpp_stanza_get_ns(delay_marker);
            if (ns != NULL && strcmp(ns, "urn:xmpp:delay") == 0) {
                return 1;
            }
        }
    }

    //check JID of sender
    const char* other_jid = xmpp_stanza_get_attribute(stanza, "from");
    if (!match_jid(other_jid, cfg->peer_jid)) {
        return 1;
    }

    //check if there is a body
    xmpp_stanza_t* body = xmpp_stanza_get_child_by_name(stanza, "body");
    if (body == NULL) {
        return 1;
    }
    char* message = xmpp_stanza_get_text(body);
    if (message == NULL) {
        return 1;
    }

    //put message text into write queue (ensure trailing newline)
    const size_t len  = strlen(message);
    if (message[len - 1] == '\n') {
        io_write(cfg->io, message, len);
    }
    else {
        message[len] = '\n';
        io_write(cfg->io, message, len + 1);
        message[len] = '\0';
    }
    xmpp_free(cfg->ctx, message);

    return 1;
}

void conn_handler(xmpp_conn_t* const conn, const xmpp_conn_event_t event, const int error, xmpp_stream_error_t* const stream_error, void* const userdata) {
    if (error != 0) {
        fprintf(stderr, "ERROR: received error %d from libstrophe\n", error);
    }
    if (stream_error != NULL) {
        char* buf = NULL;
        size_t buflen = 0;
        if (xmpp_stanza_to_text(stream_error->stanza, &buf, &buflen) != 0) {
            fprintf(stderr, "ERROR: received error from server: %s\n", stream_error->text);
        } else {
            fprintf(stderr, "ERROR: received error from server: %s\n", buf);
        }
        free(buf);
    }

    //userdata contains a Config struct
    struct Config* cfg = (struct Config*) userdata;
    cfg->connecting = false; //initial connection is over

    if (event == XMPP_CONN_CONNECT) {
        xmpp_handler_add(conn, message_handler, NULL, "message", "chat", cfg);
        send_presence(conn, cfg);
        cfg->connected = true;
    } else {
        cfg->connected = false;
    }
}

#define STDIN  0
#define STDOUT 1

int main(int argc, char** argv) {
    //read arguments
    struct Config cfg;
    if (!config_init(&cfg)) {
        return 1;
    }
    if (!config_consume_options(&cfg, &argc, &argv)) {
        return 1;
    }

    //fork child process, if requested
    pid_t child_pid;
    if (!subprocess_init(argc, argv, &child_pid)) {
        return 1;
    }
    //TODO: kill child_pid on exit

    //initialize file handles
    struct IO io;
    io_init(&io, STDIN, STDOUT);
    cfg.io = &io;

    //drop privileges
    if (!sec_init(&cfg)) {
        return 1;
    }

    //initialize libstrophe context
    xmpp_initialize();
    xmpp_log_t* log = xmpp_get_default_logger(MY_LOG_LEVEL);
    cfg.ctx = xmpp_ctx_new(NULL, log);

    //initialize connection object
    xmpp_conn_t* conn = xmpp_conn_new(cfg.ctx);
#ifdef XMPP_CONN_FLAG_MANDATORY_TLS
    xmpp_conn_set_flags(conn, XMPP_CONN_FLAG_MANDATORY_TLS); //there's just no excuse not to do TLS
#endif
    xmpp_conn_set_jid(conn, cfg.jid);
    xmpp_conn_set_pass(conn, cfg.password);

    //enter the first event loop which creates the connection and waits until
    //conn_handler is called
    cfg.connecting = true;
    if (xmpp_connect_client(conn, NULL, 0, conn_handler, &cfg) != 0) {
        fprintf(stderr, "FATAL: failed to connect to %s\n", cfg.jid);
        return 1;
    }
    while (cfg.connecting) {
        xmpp_run_once(cfg.ctx, 100);
    }

    //enter the second event loop which sends and receives messages
    bool stay_in_loop = true;

    while (stay_in_loop && cfg.connected) {
        const bool success = io_select(&io, 100000); //timeout = 100 ms
        if (!success) {
            //error -> shutdown
            xmpp_disconnect(conn);
            stay_in_loop = false;
        } else {
            //check if one or multiple full lines were received
            char* str = io_getlines(&io);
            if (str == NULL) {
                if (io.eof) {
                    //EOF has been reached - commence normal shutdown
                    xmpp_disconnect(conn);
                    stay_in_loop = false;
                }
            } else {
                //send message
                xmpp_stanza_t* reply = xmpp_stanza_new(cfg.ctx);
                xmpp_stanza_set_name(reply, "message");
                xmpp_stanza_set_type(reply, "chat");
                xmpp_stanza_set_attribute(reply, "from", cfg.jid);
                xmpp_stanza_set_attribute(reply, "to", cfg.peer_jid);

                xmpp_stanza_t* body = xmpp_stanza_new(cfg.ctx);
                xmpp_stanza_set_name(body, "body");

                xmpp_stanza_t* text = xmpp_stanza_new(cfg.ctx);
                xmpp_stanza_set_text(text, str);

                xmpp_stanza_add_child(body, text);
                xmpp_stanza_add_child(reply, body);

                xmpp_send(conn, reply);
                xmpp_stanza_release(reply);
                free(str);
            }
        }

        //send queued messages and watch for incoming messages, but don't block
        xmpp_run_once(cfg.ctx, 0);
    }

    //wait for disconnect to finish
    while (cfg.connected) {
        xmpp_run_once(cfg.ctx, 100);
    }

    //free resources
    xmpp_conn_release(conn);
    xmpp_ctx_free(cfg.ctx);
    xmpp_shutdown();
    //TODO: which of these can throw errors?

    //if child process was launched, wait on it and propagate its exit code
    if (child_pid == 0) {
        return 0;
    }
    int wstatus;
    if (waitpid(child_pid, &wstatus, 0) < 0) {
        perror("wait() on child process");
        return 1;
    }
    if (WIFEXITED(wstatus)) {
        return WEXITSTATUS(wstatus);
    } else {
        return 1;
    }
}
