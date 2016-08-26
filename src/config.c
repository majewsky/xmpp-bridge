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

#include <string.h>

bool config_init(struct Config* cfg, int argc, char** argv) {
    //get arguments (TODO: validate JIDs)
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <own-jid> <pass> <peer-jid>\n", argv[0]);
        return false;
    }

    bool valid = true;
    if (!validate_jid(cfg->jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid JID\n", cfg->jid);
        valid = false;
    }
    if (!validate_jid(cfg->peer_jid)) {
        fprintf(stderr, "FATAL: '%s' is not a valid JID\n", cfg->peer_jid);
        valid = false;
    }

    //initialize remaining fields of `cfg`
    cfg->ctx = NULL;
    cfg->connected = false;

    return valid;
}
