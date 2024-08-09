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
 *  Main source code file for the OpenDAX OPC UA server module
 */

#include <opendax.h>
#include <common.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#include "daxopcua.h"


dax_state *ds;
UA_Boolean running = true;
UA_Server *server;

pthread_cond_t dtcond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t dtlock = PTHREAD_MUTEX_INITIALIZER;
datatype_t *dtdata;


static void __getout(int sign) {
    running = false;
}



/* This is a wrapper function that can be setup in the Open62541 configuration so that it calls
   our dax_vlog function for logging. */
static void
__log_wrapper(void *context, UA_LogLevel level, UA_LogCategory category, const char *msg, va_list args)
{
    uint32_t topic;

    switch(level) {
        case UA_LOGLEVEL_TRACE:
            return; /* Not gonna log this */
            break;
        case UA_LOGLEVEL_DEBUG:
            topic = DAX_LOG_DEBUG;
            return; /* Not this one either but maybe we'll do some config for this */
            break;
        case UA_LOGLEVEL_INFO:
            topic = DAX_LOG_INFO;
            break;
        case UA_LOGLEVEL_WARNING:
            topic = DAX_LOG_WARN;
            break;
        case UA_LOGLEVEL_ERROR:
            topic = DAX_LOG_ERROR;
            break;
        case UA_LOGLEVEL_FATAL:
            topic = DAX_LOG_FATAL;
            break;
        default:
            topic = DAX_LOG_DEBUG;
            break;
    }
    dax_vlog(topic, msg, args);
}

static int
__add_tags(UA_Server *server)
{
    int result;
    tag_index lastindex;
    dax_tag tag;
    tag_handle h;

    result = dax_tag_handle(ds, &h, (char *)"_lastindex", 0);
    if(result) {
        dax_log(DAX_LOG_FATAL, "Unable to find handle for _lastindex");
        return result;
    }
    result = dax_tag_read(ds, h, &lastindex);
    if(result) {
        dax_log(DAX_LOG_FATAL, "Unable to read _lastindex");
        return result;
    }
    for(tag_index n = 0; n<=lastindex; n++) {
        result = dax_tag_byindex(ds, &tag, n);
        if(result == ERR_OK) {
            addTagVariable(server, &tag);
        }
    }
    return 0;
}

static void *
__event_thread(void *data) {

    if(__add_tags(server)) {
        exit(-1);
    }

    while(running) {
        dax_event_wait(ds, 500, NULL);
    }
    dax_log(DAX_LOG_MAJOR, "Event thread shutting down");
    return NULL;
}

static void
__add_tag_event_callback(dax_state *ds, void *udata) {
    uint8_t buff[TAG_TYPE_SIZE];
    dax_tag tag;
    UA_Server *server = (UA_Server *)udata;

    dax_event_get_data(ds, buff, TAG_TYPE_SIZE);

    tag.idx = *((tag_index *)buff);
    tag.type = *((tag_type *)&buff[4]);
    tag.count = *((dax_udint *)&buff[8]);
    tag.attr = *((uint16_t *)&buff[12]);
    memcpy(tag.name, &buff[14], DAX_TAGNAME_SIZE + 1);

    addTagVariable(server, &tag);
}

static void
__del_tag_event_callback(dax_state *ds, void *udata) {
    uint8_t buff[TAG_TYPE_SIZE];
    dax_tag tag;
    UA_Server *server = (UA_Server *)udata;
    UA_NodeId nid;
    tag_handle h;

    dax_event_get_data(ds, buff, TAG_TYPE_SIZE);

    tag.idx = *((tag_index *)buff);
    tag.type = *((tag_type *)&buff[4]);
    tag.count = *((dax_udint *)&buff[8]);
    tag.attr = *((uint16_t *)&buff[12]);
    memcpy(tag.name, &buff[14], DAX_TAGNAME_SIZE + 1);
    /* We are just reading the tag to remove it from the cache */
    h.index = tag.idx;
    h.bit = 0;
    h.byte = 0;
    h.count = tag.count;
    h.type = tag.type;
    h.size = 1;
    dax_tag_read(ds, h, buff);

    nid = UA_NODEID_STRING_ALLOC(1, tag.name);
    UA_Server_deleteNode(server, nid, false);
}


int
main(int argc, char *argv[]) {
    int result;
    pthread_attr_t attr;
    pthread_t eventThread;
    tag_handle h;
    dax_id id;
    //uint64_t loopcount = 0;

    signal(SIGINT, __getout);
    signal(SIGTERM, __getout);


    ds = dax_init("daxopcua");
    if(ds == NULL) {
        /* dax_fatal() logs an error and causes a quit
         * signal to be sent to the module */
        dax_log(DAX_LOG_FATAL, "Unable to Allocate DaxState Object\n");
        exit(-1);
    }

    result = opcua_configure(ds, argc, argv);
    if(result) {
        dax_log(DAX_LOG_FATAL, "Fatal error in configuration");
        exit(result);
    }

    /* Check for OpenDAX and register the module */
    if( dax_connect(ds) ) {
        dax_log(DAX_LOG_FATAL, "Unable to find OpenDAX");
        exit(-1);
    }

    dax_set_disconnect_callback(ds, __getout);

    UA_ServerConfig *config = (UA_ServerConfig *)UA_malloc(sizeof(UA_ServerConfig));
    memset(config, 0, sizeof(UA_ServerConfig));
    config->logging = (UA_Logger*)UA_malloc(sizeof(UA_Logger));
    config->logging->log = __log_wrapper;
    config->logging->clear = NULL;
    config->logging->context = NULL;
    UA_ServerConfig_setDefault(config);
    UA_ApplicationDescription_clear(&config->applicationDescription);
    config->applicationDescription.applicationUri = UA_String_fromChars("urn.opendax.server.application");
    config->applicationDescription.productUri = UA_String_fromChars("https://opendax.org");
    config->applicationDescription.applicationName = UA_LOCALIZEDTEXT_ALLOC("en", "OpenDAX OPC UA Server");
    config->applicationDescription.applicationType = UA_APPLICATIONTYPE_SERVER;

    server = UA_Server_newWithConfig(config);

    dax_tag_handle(ds, &h, "_tag_added", 0);
    dax_event_add(ds, &h, EVENT_CHANGE, NULL, &id, __add_tag_event_callback, server, NULL);
    dax_event_options(ds, id, EVENT_OPT_SEND_DATA);

    dax_tag_handle(ds, &h, "_tag_deleted", 0);
    dax_event_add(ds, &h, EVENT_CHANGE, NULL, &id, __del_tag_event_callback, server, NULL);
    dax_event_options(ds, id, EVENT_OPT_SEND_DATA);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if(pthread_create(&eventThread, &attr, __event_thread, NULL)) {
        dax_log(DAX_LOG_FATAL, "Unable to start main event thread");
        exit(-1);
    } else {
        dax_log(DAX_LOG_MAJOR, "Main event thread started");
    }

    dax_set_running(ds, 1);
    dax_log(DAX_LOG_MAJOR, "OPC UA Module Starting");


    // if(UA_Server_addTimedCallback(server, __startup_callback, NULL, 2e6L, NULL)) {
    //     dax_log(DAX_LOG_ERROR, "unable to add startup callback");
    // }


    /* Server Main Loop */
    UA_Server_run_startup(server);
    /* Should the server networklayer block (with a timeout) until a message
       arrives or should it return immediately? */
    UA_Boolean waitInternal = true;
    while(running) {

        UA_Server_run_iterate(server, waitInternal);

        /* This is where we add a custom data type to the server configuration */
        pthread_mutex_lock(&dtlock);
        if(dtdata != NULL) {
            UA_DataTypeArray *cdt;
            UA_ServerConfig *cfg;
            cdt = malloc(sizeof(UA_DataTypeArray));
            cfg = UA_Server_getConfig(server);
            cdt->next = cfg->customDataTypes;
            /* this cast is to get around the const keyword in the struct def */
            *(size_t *)&cdt->typesSize = 1;
            cdt->types = &dtdata->datatype;
            cdt->cleanup = UA_FALSE;
            cfg->customDataTypes = cdt;

            DF("Condition");
            dtdata = NULL;
            pthread_cond_signal(&dtcond);
        }
        pthread_mutex_unlock(&dtlock);

    }
    UA_Server_run_shutdown(server);

    UA_Server_delete(server);
    UA_free(config->logging);
    UA_free(config);

    return EXIT_SUCCESS;
}