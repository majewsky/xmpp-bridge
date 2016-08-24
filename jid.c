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

bool validate_jid(const char* jid) {
    if (jid == NULL) {
        return false;
    }

    //expect an "@" character somewhere
    return strchr(jid, '@') != NULL;
    //TODO: could validate way more
}

bool match_jid(const char* actual_jid, const char* expected_jid) {
    if (actual_jid == NULL || expected_jid == NULL) {
        //this should not happen
        return false;
    }

    //two cases for exact match:
    //1. if expected_jid has a resource, actual_jid must have the same
    //2. if both have no resource, they must be equal, too
    if (strchr(expected_jid, '/') != NULL || strchr(actual_jid, '/') == NULL) {
        return strcmp(actual_jid, expected_jid) == 0;
    }

    //actual_jid has a resource, but expected_jid hasn't - ignore the resource
    char* truncated_jid = strdup(actual_jid);
    char* slash_pos = strchr(truncated_jid, '/');
    *slash_pos = '\0';
    const bool result = strcmp(expected_jid, truncated_jid) == 0;
    free(truncated_jid);
    return result;
}
