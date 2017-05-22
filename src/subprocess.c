/*******************************************************************************
*
* Copyright 2017 Stefan Majewsky <majewsky@gmx.net>
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

#include <memory.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#define STDIN  0
#define STDOUT 1

#define MUST_SUCCEED(action) if ((action) < 0) { perror(#action); return false; }

static bool subprocess_setup_child(int argc, char** argv, int fds[4]);

bool subprocess_init(int argc, char** argv, pid_t* pid) {
    if (argc == 0) {
        *pid = 0;
        return true;
    }

    int fds[4];
    MUST_SUCCEED(pipe(fds));
    MUST_SUCCEED(pipe(fds + 2));

    MUST_SUCCEED(close(STDIN));
    MUST_SUCCEED(close(STDOUT));

    MUST_SUCCEED(*pid = fork());

    if (*pid == 0) {
        //CHILD
        subprocess_setup_child(argc, argv, fds);
        //if this returns, something went wrong
        exit(255);

    } else {
        //PARENT
        MUST_SUCCEED(dup2(fds[2], STDIN));
        MUST_SUCCEED(dup2(fds[1], STDOUT));
        for (int i = 0; i < 4; ++i) {
            MUST_SUCCEED(close(fds[i]));
        }

        return true;
    }
}

bool subprocess_setup_child(int argc, char** argv, int fds[4]) {
    MUST_SUCCEED(dup2(fds[0], STDIN));
    MUST_SUCCEED(dup2(fds[3], STDOUT));
    for (int i = 0; i < 4; ++i) {
        MUST_SUCCEED(close(fds[i]));
    }

    //prepare an argv[] that is nul-terminated
    char** argv2 = (char**) malloc(sizeof(char*) * (argc + 1));
    if (argv2 == NULL) {
        perror("malloc()");
        return false;
    }
    memcpy(argv2, argv, sizeof(char*) * argc);
    argv2[argc] = NULL;

    MUST_SUCCEED(execvp(argv2[0], argv2));
    return false;
}
