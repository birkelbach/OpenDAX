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
    static tag_type last_type;
    static uint32_t offset;

    datatype_t *dt = (datatype_t *)udata;

    if(dt->datatype.membersSize == 0) { /* Indicates that this is the first call for this type. */
        /* Reset the static data on first call */
        last_type = 0;
        offset = 0;
    }

    DF("Add membmer %s to index %d", member.name, dt->datatype.membersSize);
    dt->datatype.members[dt->datatype.membersSize].memberName = member.name;
    assert(IS_CUSTOM(member.type) == 0); // TODO FIX THIS
    dt->datatype.members[dt->datatype.membersSize].memberType = &UA_TYPES[get_ua_base_type(member.type)];
    dt->datatype.members[dt->datatype.membersSize].padding = 0;
    dt->datatype.members[dt->datatype.membersSize].isArray = false;
    dt->datatype.members[dt->datatype.membersSize].isOptional = false;

    dt->datatype.membersSize++;
    dt->datatype.memSize += dax_get_typesize(ds, member.type); // TODO This ain't right, BOOLs will fall apart
}

/* Adds a datatype node to the opcua server that mirrors the given type */
static datatype_t *
__add_type(UA_Server *server, tag_type type) {
    int count;
    int result;
    datatype_t *new;
    UA_StatusCode scode;
    static UA_UInt32 encodingid;

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

        char *dn = malloc(strlen(new->name) + 6);
        sprintf(dn, "%s Type", new->name);
        char *dtn = malloc(strlen(new->name) + 6);
        sprintf(dtn, "%s.type", new->name);

        new->datatype.typeName = dn;
        new->datatype.typeId = UA_NODEID_STRING(1, dtn);
        new->datatype.binaryEncodingId = UA_NODEID_NUMERIC(1, ++encodingid);
        new->datatype.typeKind = UA_DATATYPEKIND_STRUCTURE;
        new->datatype.pointerFree = true;
        new->datatype.overlayable = false;

        /* Add to our linked list - This may be redundant since we have a linked list in the server config*/
        new->next = datatypes;
        datatypes = new;

        /* This code has the main server loop write the data type definition to the
           server's configuration linked list */
        pthread_mutex_lock(&dtlock);
        dtdata = new;
        pthread_cond_wait(&dtcond, &dtlock);
        pthread_mutex_unlock(&dtlock);


        /* Add Data Type Node */
        UA_DataTypeAttributes attr = UA_DataTypeAttributes_default;
        attr.displayName = UA_LOCALIZEDTEXT("en-US", dn);
        scode = UA_Server_addDataTypeNode(server,
                                          new->datatype.typeId,
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_STRUCTURE),
                                          UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                          UA_QUALIFIEDNAME(1, dtn), attr, NULL, NULL);
        if(scode != UA_STATUSCODE_GOOD) {
            dax_log(DAX_LOG_ERROR, "Unable to add data type %s", new->name);
        } else {
            dax_log(DAX_LOG_INFO, "Added data type %s", new->name);
        }
    }
    return new;
}

/* Returns a pointer to the datatype_t structure that represents the structured datatype
   that represents the CDT given by type.  If it cannot be found then the __add_type function
   is called to add the type to the opcua server and that pointer is returned.  */
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