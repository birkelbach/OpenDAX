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

static void
__beforeReadTagCallback(UA_Server *server,
               const UA_NodeId *sessionId, void *sessionContext,
               const UA_NodeId *nodeid, void *nodeContext,
               const UA_NumericRange *range, const UA_DataValue *data) {
    int result;
    tag_handle *h;
    h = (tag_handle *)nodeContext;
    result = dax_tag_read(ds, *h, data->value.data);
    if(result) dax_log(DAX_LOG_ERROR, "Failure to read tag");
    //DF("Read callback called - %s", (char *)nodeContext);
}

static void
__afterWriteTagCallback(UA_Server *server,
               const UA_NodeId *sessionId, void *sessionContext,
               const UA_NodeId *nodeId, void *nodeContext,
               const UA_NumericRange *range, const UA_DataValue *data) {
    int result;
    tag_handle *h;
    h = (tag_handle *)nodeContext;
    result = dax_tag_write(ds, *h, data->value.data);
    if(result) dax_log(DAX_LOG_ERROR, "Failure to write tag");
}


// static void
// __readCallback(dax_state *ds, void *udata) {

// }


static void
__addValueCallback(UA_Server *server, UA_NodeId nodeid, uint16_t attr) {
    UA_ValueCallback callback ;

    //if(attr & TAG_ATTR_VIRTUAL) {
        callback.onRead = __beforeReadTagCallback;
    //} else {
    //    callback.onRead = NULL;
    //}
    //if(attr & TAG_ATTR_READONLY || attr & TAG_ATTR_SPECIAL) {
    //    callback.onWrite = NULL;
    //} else {
        callback.onWrite = __afterWriteTagCallback;
    //}

    UA_Server_setVariableNode_valueCallback(server, nodeid, callback);
}


int
addTagVariable(UA_Server *server, dax_tag *tag) {
    tag_handle *h;
    //dax_id id;
    //int result;
    dax_log(DAX_LOG_DEBUG, "Adding variable for tag %s", tag->name);
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    /* TODO: Get description from configuration if available
             Deal with inclusion / exclusion list */
    attr.description = UA_LOCALIZEDTEXT("en-US","TODO");
    attr.displayName = UA_LOCALIZEDTEXT("en-US",tag->name);

    switch(tag->type) {
        case DAX_BOOL:
            attr.dataType = UA_TYPES[UA_TYPES_BOOLEAN].typeId;
            break;
        case DAX_BYTE:
            attr.dataType = UA_TYPES[UA_TYPES_BYTE].typeId;
            break;
        case DAX_SINT:
            attr.dataType = UA_TYPES[UA_TYPES_SBYTE].typeId;
            break;
        case DAX_CHAR:
            return ERR_NOTIMPLEMENTED;
            break;
        case DAX_INT:
            attr.dataType = UA_TYPES[UA_TYPES_INT16].typeId;
            break;
        case DAX_WORD:
        case DAX_UINT:
            attr.dataType = UA_TYPES[UA_TYPES_UINT16].typeId;
            break;
        case DAX_DINT:
            attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
            break;
        case DAX_DWORD:
        case DAX_UDINT:
            attr.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
            break;
        case DAX_REAL:
            attr.dataType = UA_TYPES[UA_TYPES_FLOAT].typeId;
            break;
        case DAX_LINT:
            attr.dataType = UA_TYPES[UA_TYPES_INT64].typeId;
            break;
        case DAX_LWORD:
        case DAX_ULINT:
            attr.dataType = UA_TYPES[UA_TYPES_UINT64].typeId;
            break;
        case DAX_TIME:
            return ERR_NOTIMPLEMENTED;
            break;
        case DAX_LREAL:
            attr.dataType = UA_TYPES[UA_TYPES_DOUBLE].typeId;
            break;
        default:
            return ERR_NOTIMPLEMENTED;
    }
    /* TODO: Handle Arrays and CDTs */

    if(tag->attr & TAG_ATTR_READONLY || tag->attr & TAG_ATTR_SPECIAL) {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ;
    } else {
        attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;
    }
    /* Add the variable node to the information model */
    UA_NodeId myIntegerNodeId = UA_NODEID_STRING(1, tag->name);
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, tag->name);
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, myIntegerNodeId, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, NULL);

    __addValueCallback(server, myIntegerNodeId, tag->attr);
    h = malloc(sizeof(tag_handle));
    dax_tag_handle(ds, h, tag->name, 0);
    UA_Server_setNodeContext(server, myIntegerNodeId, h);

    //result = dax_event_add(ds, h, EVENT_WRITE, NULL, &id, __readCallback, void *udata, NULL);

    return 0;
}