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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IS_STRING_EMPTY(x) ((x) == NULL || *(x) == '\0')

bool config_init(struct Config* cfg) {
    //initialize fields of `cfg`
    cfg->jid = getenv("XMPPBRIDGE_JID");
    cfg->password = getenv("XMPPBRIDGE_PASSWORD");
    cfg->peer_jid = getenv("XMPPBRIDGE_PEER_JID");
    cfg->show_delayed_messages = false;
    cfg->drop_privileges = geteuid() == 0; //by default, only when started as root
    cfg->ctx = NULL;
    cfg->connected = false;
    cfg->connecting = false;

    //validate input
    bool valid = true;

    if (IS_STRING_EMPTY(cfg->jid)) {
        fprintf(stderr, "FATAL: $XMPPBRIDGE_JID is not set\n");
        valid = false;
    }
    else if (!validate_jid(cfg->jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid JID\n", cfg->jid);
        valid = false;
    }

    if (IS_STRING_EMPTY(cfg->peer_jid)) {
        fprintf(stderr, "FATAL: $XMPPBRIDGE_PEER_JID is not set\n");
        valid = false;
    }
    else if (!validate_jid(cfg->peer_jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid peer JID\n", cfg->peer_jid);
        valid = false;
    }

    if (IS_STRING_EMPTY(cfg->password)) {
        fprintf(stderr, "FATAL: $XMPPBRIDGE_PASSWORD is not set\n");
        valid = false;
    }

    return valid;
}

bool config_consume_options(struct Config* cfg, int* argc, char*** argv) {
    //consume argv[0] - we don't need our program name
    (*argc)--;
    (*argv)++;

    while (*argc > 0) {
        //get next option argument
        char* arg = (*argv)[0];
        if (strncmp(arg, "--", 2) != 0) {
            return true;
        }
        //consume it
        (*argc)--;
        (*argv)++;

        //recognize option
        if (strcmp(arg, "--show-delayed") == 0) {
            cfg->show_delayed_messages = true;
        }
        else if (strcmp(arg, "--drop-privileges") == 0) {
            cfg->drop_privileges = true;
        }
        else if (strcmp(arg, "--no-drop-privileges") == 0) {
            cfg->drop_privileges = false;
        }
        else {
            fprintf(stderr,
                "FATAL: unrecognized option: \"%s\"\n"
                "See `man 1 xmpp-bridge` for usage.\n",
                arg
            );
            return false;
        }
    }

    return true;
}
