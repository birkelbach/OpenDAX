/*  OpenDAX - An open source data acquisition and control system
 *  Copyright (c) 2024 Phil Birkelbach
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  custom data type handling source file for the OpenDAX OPC UA server module
 */

#include "daxopcua.h"
#include <common.h>
#include <assert.h>

extern dax_state *ds;
extern pthread_cond_t dtcond;
extern pthread_mutex_t dtlock;
extern datatype_t *dtdata;

static datatype_t *datatypes;

static void
__type_iterator_callback(cdt_iter member, void *udata) {
    datatype_t *dt = (datatype_t *)udata;

    DF("Add membmer %s to index %d", member.name, dt->datatype.membersSize);
    dt->datatype.members[dt->datatype.membersSize].memberName = member.name;
    assert(IS_CUSTOM(member.type) == 0); // TODO FIX THIS
    dt->datatype.members[dt->datatype.membersSize].memberType = &UA_TYPES[get_ua_base_type(member.type)];
    dt->datatype.members[dt->datatype.membersSize].padding = 0;
    dt->datatype.members[dt->datatype.membersSize].isArray = false;
    dt->datatype.members[dt->datatype.membersSize].isOptional = false;

    dt->datatype.membersSize++;
}

static datatype_t *
__add_type(UA_Server *server, tag_type type) {
    int count;
    int result;
    datatype_t *new;

    count = dax_cdt_member_count(ds, type);
    if(count < 0) {
        return NULL;
    }

    new = malloc(sizeof(datatype_t));
    if(new != NULL) {
        new->name = strdup(dax_type_to_string(ds, type));
        new->dax_type = type;
        new->datatype.members = malloc(sizeof(UA_DataTypeMember) * count);
        if(new->datatype.members == NULL) {
            free(new);
            return NULL;
        }
        new->datatype.membersSize = 0;
        new->datatype.memSize = 0;
        result = dax_cdt_iter(ds, type, new, __type_iterator_callback);
        if(result) {
            dax_log(DAX_LOG_ERROR, "Failed retrieving data type members - %d", result);
            free(new->datatype.members);
            free(new);
            return NULL;
        }
        new->datatype.typeName = new->name;
        new->datatype.typeId = UA_NODEID_STRING(1, "3D.PointVar");
        new->datatype.binaryEncodingId = UA_NODEID_NUMERIC(1, type - DAX_CUSTOM);
        new->datatype.typeKind = UA_DATATYPEKIND_STRUCTURE;
        new->datatype.pointerFree = true;
        new->datatype.overlayable = false;

        /* Add it to the head of the linked list */
        DF("count = %d", new->datatype.membersSize);
        //new->member_count = count;

        new->next = datatypes;
        datatypes = new;

        pthread_mutex_lock(&dtlock);
        dtdata = new;
        DF("Waiting");
        pthread_cond_wait(&dtcond, &dtlock);
        pthread_mutex_unlock(&dtlock);

    }
    return new;
}

datatype_t *
getTypePointer(UA_Server *server, tag_type type)
{
    datatype_t *this;

    this = datatypes;
    while(this != NULL) {
        if(this->dax_type == type) {
            return this; /* We found it and we are done*/
        }
        this = this->next;
    }
    /* If we get here, the type was not found in our list.
       We will try to add it. */

    return __add_type(server, type);
}