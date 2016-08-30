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

#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define IS_STRING_EMPTY(x) ((x) == NULL || *(x) == '\0')

bool config_init(struct Config* cfg, int argc, char** argv) {
    //initialize fields of `cfg`
    cfg->jid = cfg->password = cfg->peer_jid = false;
    cfg->show_delayed_messages = false;
    cfg->drop_privileges = geteuid() == 0; //by default, only when started as root
    cfg->ctx = NULL;
    cfg->connected = false;

    unsigned int pos = 0;
    for (int idx = 1; idx < argc; ++idx) {
        char* arg = argv[idx];
        //check if `arg` is an option
        if (strcmp(arg, "--show-delayed") == 0) {
            cfg->show_delayed_messages = true;
            continue;
        }
        if (strcmp(arg, "--drop-privileges") == 0) {
            cfg->drop_privileges = true;
            continue;
        }
        if (strcmp(arg, "--no-drop-privileges") == 0) {
            cfg->drop_privileges = false;
            continue;
        }
        //`arg` is a positional argument
        switch (++pos) {
            case 1:
                cfg->peer_jid = arg;
                break;
            case 2:
                cfg->jid = arg;
                break;
            case 3:
                cfg->password = arg;
                break;
            default:
                //too many positional arguments
                fprintf(stderr, "Usage: %s <peer-jid> <own-jid> <pass>\n", argv[0]);
                return false;
        }
    }

    //retrieve omitted positional arguments from their corresponding environment variables
    if (IS_STRING_EMPTY(cfg->jid)) {
        cfg->jid = getenv("XMPPBRIDGE_JID");
    }
    if (IS_STRING_EMPTY(cfg->password)) {
        cfg->password = getenv("XMPPBRIDGE_PASSWORD");
    }
    if (IS_STRING_EMPTY(cfg->peer_jid)) {
        cfg->peer_jid = getenv("XMPPBRIDGE_PEER_JID");
    }

    //validate arguments
    bool valid = true;
    if (!validate_jid(cfg->jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid JID\n", cfg->jid);
        valid = false;
    }
    if (!validate_jid(cfg->peer_jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid peer JID\n", cfg->peer_jid);
        valid = false;
    }
    if (IS_STRING_EMPTY(cfg->password)) {
        fprintf(stderr, "FATAL: no password given and $XMPPBRIDGE_PASSWORD is not set\n");
        valid = false;
    }

    return valid;
}
