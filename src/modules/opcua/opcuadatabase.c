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
 *  tag data handling source file for the OpenDAX OPC UA server module
 */

#include <common.h>
#include "daxopcua.h"

extern dax_state *ds;


static int
__get_base_type(tag_type type) {
   switch(type) {
        case DAX_BOOL:
            return UA_TYPES_BOOLEAN;
        case DAX_BYTE:
            return UA_TYPES_BYTE;
        case DAX_SINT:
            return UA_TYPES_SBYTE;
        case DAX_CHAR:
            return ERR_NOTIMPLEMENTED;
        case DAX_INT:
            return UA_TYPES_INT16;
        case DAX_WORD:
        case DAX_UINT:
            return UA_TYPES_UINT16;
        case DAX_DINT:
            return UA_TYPES_INT32;
        case DAX_DWORD:
        case DAX_UDINT:
            return UA_TYPES_UINT32;
        case DAX_REAL:
            return UA_TYPES_FLOAT;
        case DAX_LINT:
            return UA_TYPES_INT64;
        case DAX_LWORD:
        case DAX_ULINT:
            return UA_TYPES_UINT64;
        case DAX_TIME:
            //return ERR_NOTIMPLEMENTED;
            return UA_TYPES_DATETIME;
        case DAX_LREAL:
            return UA_TYPES_DOUBLE;
        default:
            return ERR_NOTIMPLEMENTED;
    }
}

static void
__update_variable_node(node_context_t *nc) {
    int result;
    int type;
    uint64_t dt;
    uint64_t *daxt;
    UA_Variant value;
    uint8_t buff[nc->h.size];

    UA_Variant_init(&value);

    type = __get_base_type(nc->h.type);
    if(type > 0) {
        result = dax_tag_read(ds, nc->h, buff);
        if(! result) {
            if(type == UA_TYPES_DATETIME) {
                daxt = (uint64_t *)buff;
                dt = UA_DateTime_fromUnixTime(*daxt/1000) + ((*daxt)%1000 * 10000);
                UA_Variant_setScalar(&value, &dt, &UA_TYPES[type]);
            } else {
                UA_Variant_setScalar(&value, buff, &UA_TYPES[type]);
            }
            nc->skip = 1; /* Stops us from getting into an event loop */
            UA_Server_writeValue(nc->server, nc->nodeId, value);
        } else {
            dax_log(DAX_LOG_ERROR, "Unable to read tag");
        }
    } /* TODO: deal with the other data types */
}


static void
__beforeReadTagCallback(UA_Server *server,
               const UA_NodeId *sessionId, void *sessionContext,
               const UA_NodeId *nodeid, void *nodeContext,
               const UA_NumericRange *range, const UA_DataValue *data) {

    node_context_t *nc = (node_context_t *)nodeContext;
    __update_variable_node(nc);

}

static void
__afterWriteTagCallback(UA_Server *server,
               const UA_NodeId *sessionId, void *sessionContext,
               const UA_NodeId *nodeId, void *nodeContext,
               const UA_NumericRange *range, const UA_DataValue *data) {
    int result;
    node_context_t *nc = (node_context_t *)nodeContext;
    if(nc->skip) {
        nc->skip = 0;
    } else {
        result = dax_tag_write(ds, nc->h, data->value.data);
        nc->skip = 1;
        if(result) dax_log(DAX_LOG_ERROR, "Failure to write tag");
    }
}


static void
__writeEventCallback(dax_state *ds, void *udata) {
    node_context_t *nc = (node_context_t *)udata;

    if(nc->skip) {
        nc->skip = 0;
    } else {
        __update_variable_node(nc);
    }
}

/* The pointer that gets freed here is also assigned to the node context so be careful */
static void
__freeEventCallback(void *udata) {
    node_context_t *nc = (node_context_t *)udata;
    free(nc->nodeId.identifier.string.data);
    free(udata);
}


static void
__addValueCallback(UA_Server *server, UA_NodeId nodeid, uint16_t attr) {
    UA_ValueCallback callback ;

    if(attr & TAG_ATTR_VIRTUAL) {
        callback.onRead = __beforeReadTagCallback;
    } else {
        callback.onRead = NULL;
    }
    if(attr & TAG_ATTR_READONLY || attr & TAG_ATTR_SPECIAL) {
        callback.onWrite = NULL;
    } else {
        callback.onWrite = __afterWriteTagCallback;
    }

    UA_Server_setVariableNode_valueCallback(server, nodeid, callback);
}

int
addTagVariable(UA_Server *server, dax_tag *tag) {
    dax_id id;
    int result;

    dax_log(DAX_LOG_DEBUG, "Adding variable for tag %s", tag->name);
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    /* TODO: Get description from configuration if available
             Deal with inclusion / exclusion list */
    attr.description = UA_LOCALIZEDTEXT("en-US","TODO");
    attr.displayName = UA_LOCALIZEDTEXT("en-US",tag->name);

    result = __get_base_type(tag->type);
    if(result > 0) {
        attr.dataType = UA_TYPES[result].typeId;
    } else {
        return result;
    }

    /* TODO: Handle Arrays and CDTs */

    if(tag->attr & TAG_ATTR_READONLY || tag->attr & TAG_ATTR_SPECIAL) {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
    } else {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }
    /* Add the variable node to the information model */
    node_context_t *nc = malloc(sizeof(node_context_t));
    nc->nodeId = UA_NODEID_STRING_ALLOC(1, tag->name);
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, tag->name);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, nc->nodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

    __addValueCallback(server, nc->nodeId, tag->attr);

    dax_tag_handle(ds, &nc->h, tag->name, 0);
    nc->server = server;
    nc->skip = 0;
    UA_Server_setNodeContext(server, nc->nodeId, nc);
    result = dax_event_add(ds, &nc->h, EVENT_WRITE, NULL, &id, __writeEventCallback, nc, __freeEventCallback);
    __update_variable_node(nc);

    return 0;
}