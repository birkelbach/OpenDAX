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
 *  Main header file for the OpenDAX OPC UA server module
 */

#ifndef __DAXOPCUA_H
#define __DAXOPCUA_H

#include <opendax.h>
#include <open62541/server.h>

/* this is the size of the _tag_desc CDT type */
#define TAG_TYPE_SIZE 47

/* This structure is being used to assign context to the node inside the
   OPC UA server as well as used as user data for the dax event for the
   the write event on the tag that matches the node. */
typedef struct {
    UA_Server *server;
    UA_NodeId nodeId;
    tag_handle h;
    int skip;          /* Used to stop a write event callback loop between OPCUA and OpenDAX */
} node_context_t;


/* This structure is used as the node to a linked list of data types
*/
typedef struct datatype_t {
    char *name;                     /* Name of the type */
    tag_type dax_type;               /* OpenDAX type id */
    UA_DataType datatype;
    size_t typesize;
    struct datatype_t *next;        /* Pointer to next struct in the list */
} datatype_t;


/* Configuration Handling - opcuaopt.c */
int opcua_configure(dax_state *ds, int argc, char *argv[]);

/* Data Handling - opcuadatabase.c */
int get_ua_base_type(tag_type type);
int addTagVariable(UA_Server *server, dax_tag *tag);

/* Compound Data Type Handling - opcuatype.c */
datatype_t * getTypePointer(UA_Server *server, tag_type type);


/* The binary encoding id's for the datatypes */
#define Point_binary_encoding_id        14


static UA_DataTypeMember Point_members[3] = {
    /* x */
    {
        UA_TYPENAME("x")           /* .memberName */
        &UA_TYPES[UA_TYPES_DOUBLE], /* .memberType */
        0,                         /* .padding */
        false,                     /* .isArray */
        false                      /* .isOptional */
    },
    /* y */
    {
        UA_TYPENAME("y")           /* .memberName */
        &UA_TYPES[UA_TYPES_DOUBLE], /* .memberType */
        0,                         /* .padding */
        false,                     /* .isArray */
        false                      /* .isOptional */
    },
    /* z */
    {
        UA_TYPENAME("z")           /* .memberName */
        &UA_TYPES[UA_TYPES_DOUBLE], /* .memberType */
        0,                         /* .padding */
        false,                     /* .isArray */
        false                      /* .isOptional */
    }
};

static const UA_DataType PointType = {
        UA_TYPENAME("Point")                /* .tyspeName */
        {1, UA_NODEIDTYPE_NUMERIC, {4242}}, /* .typeId */
        {1, UA_NODEIDTYPE_NUMERIC, {DAX_CUSTOM}}, /* .binaryEncodingId, the numeric
                                            identifier used on the wire (the
                                            namespaceindex is from .typeId) */
        24,                      /* .memSize */
        UA_DATATYPEKIND_STRUCTURE,          /* .typeKind */
        true,                               /* .pointerFree */
        false,                              /* .overlayable (depends on endianness and
                                            the absence of padding) */
        3,                                  /* .membersSize */
        Point_members
};




#endif /* __DAXOPCUA_H */