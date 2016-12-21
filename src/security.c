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

#define _GNU_SOURCE //setres[ug]id

#include "xmpp-bridge.h"

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

static bool sec_drop_privileges() {
    //find UID for "nobody"
    struct passwd* pw = getpwnam("nobody");
    if (pw == NULL) {
        perror("Cannot find ID for user \"nobody\"");
        return false;
    }
    const uid_t uid = pw->pw_uid;

    //find UID for "nobody"
    struct group* gr = getgrnam("nobody");
    if (gr == NULL) {
        perror("Cannot find ID for group \"nobody\"");
        return false;
    }
    const gid_t gid = gr->gr_gid;

    //drop privileges
    const int result1 = setresgid(gid, gid, gid);
    if (result1 < 0) {
        perror("Cannot change user to \"nobody\"");
        return false;
    }
    const int result2 = setresuid(uid, uid, uid);
    if (result2 < 0) {
        perror("Cannot change group to \"nobody\"");
        return false;
    }

    //TODO: seteuid, setreuid, setresuid
    //TODO: setegid, setregid, setresgid
    return true;
}

bool sec_init(const struct Config* cfg) {
    if (cfg->drop_privileges) {
        if (!sec_drop_privileges()) {
            return false;
        }
    }

    return true;
}
