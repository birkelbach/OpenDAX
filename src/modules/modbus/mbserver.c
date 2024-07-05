/* mbserver.c - Modbus (tm) Communications Library
 * Copyright (C) 2010 Phil Birkelbach
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 * Source file for TCP Server functionality
 */

#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <common.h>
#include <opendax.h>
#include <modbus/modbus.h>
#include "modmain.h"


extern config_t config;
extern dax_state *ds;
extern pthread_barrier_t startup_barrier;

static int header_length;

static void
__server_enable_callback(dax_state *_ds, void *ud) {
    //uint8_t bits;
    mb_server_t *server = (mb_server_t *)ud;
    /* TODO This isn't working */
    //dax_event_get_data(_ds, &bits, 1);
    dax_read_tag(ds, server->h, &server->enable);
}


static void
__send_reply(mb_node_t **nodes, int rc, modbus_t *ctx, uint8_t *query) {
    uint8_t unitid;
    uint8_t fc;
    uint16_t index, count;
    mb_node_t *node;
    int result;

    unitid = query[header_length -1];
    fc = query[header_length];
    index = query[header_length+1]<<8 | query[header_length+2];
    count = query[header_length+3]<<8 | query[header_length+4];

    if(nodes[unitid] != NULL) {
        node = nodes[unitid];
    } else if(nodes[0] != NULL) {
        node = nodes[0];
    } else { /* No node is defined so we'll just bail out here */
        return;
    }

    if(fc==1 || fc==2 || fc==3 || fc==4) {
        result = run_lua_callback(nodes[unitid]->read_callback, unitid, fc, index, count);
        if(result) {
            modbus_reply_exception(ctx, query, result);
            return;
        }
    }

    switch(fc) {
        case 1:
            if(node->coil_idx) {
                /* We have to read into this temporary buffer because libmodbus doesn't store bits the
                   same way that we do so we have to use conversion functions */
                slave_read_database(node->coil_idx, MB_REG_COIL, 0, node->coil_size, node->coil_buffer);
                modbus_set_bits_from_bytes(node->mb_mapping->tab_bits, 0, node->coil_size, node->coil_buffer);
            }
            modbus_reply(ctx, query, rc, node->mb_mapping);
            break;
        case 2:
            if(node->disc_idx) {
                slave_read_database(node->disc_idx, MB_REG_DISC, 0, node->disc_size, node->disc_buffer);
                modbus_set_bits_from_bytes(node->mb_mapping->tab_input_bits, 0, node->disc_size, node->disc_buffer);
            }
            modbus_reply(ctx, query, rc, node->mb_mapping);
            break;
        case 3:
            if(node->hold_idx) {
                slave_read_database(node->hold_idx, MB_REG_HOLDING, 0, node->hold_size, node->mb_mapping->tab_registers);
            }
            modbus_reply(ctx, query, rc, node->mb_mapping);
            break;
        case 4:
            if(node->input_idx) {
                slave_read_database(node->input_idx, MB_REG_HOLDING, 0, node->input_size, node->mb_mapping->tab_input_registers);
            }
            modbus_reply(ctx, query, rc, node->mb_mapping);
            break;
        case 5:
        case 15:
            modbus_reply(ctx, query, rc, node->mb_mapping);
            if(node->coil_idx) {
                get_bytes_from_bits(node->mb_mapping->tab_bits, node->coil_size, node->coil_buffer);
                /* We have to read into this temporary buffer because libmodbus doesn't store bits the
                   same way that we do so we have to use conversion functions */
                slave_write_database(node->coil_idx, MB_REG_COIL, 0, node->coil_size, node->coil_buffer);
            }
        case 6:
        case 16:
            modbus_reply(ctx, query, rc, node->mb_mapping);
            if(node->hold_idx) {
                slave_write_database(node->hold_idx, MB_REG_HOLDING, 0, node->hold_size, node->mb_mapping->tab_registers);
            }
        default:
           modbus_reply_exception(ctx, query, MODBUS_EXCEPTION_ILLEGAL_FUNCTION);
           return;
    }
    if(fc==5 || fc==6 || fc==15 || fc==16) {
        result = run_lua_callback(nodes[unitid]->write_callback, unitid, fc, index, count);
        if(result) {
            modbus_reply_exception(ctx, query, result);
            return;
        }
    }

}

/* main server thread */
static int
__server_thread(mb_server_t *server) {
    modbus_t *ctx = NULL;
    //modbus_mapping_t *mb_mapping;
    int server_socket = -1;

    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    /* Maximum file descriptor number */
    int fdmax;

    if(server->name != NULL) {
        dax_log(DAX_LOG_MAJOR, "Starting Server - %s %s:%s", server->name, server->host, server->protocol);
    }
    ctx = modbus_new_tcp_pi(server->host, server->protocol);
    header_length = modbus_get_header_length(ctx);

    server_socket = modbus_tcp_pi_listen(ctx, 8);
    //pthread_barrier_wait(&startup_barrier);
    if (server_socket == -1) {
        dax_log(DAX_LOG_ERROR, "Unable to listen TCP connection");
        modbus_free(ctx);
        return -1;
    }

    /* Clear the reference set of socket */
    FD_ZERO(&refset);
    /* Add the server socket */
    FD_SET(server_socket, &refset);

    /* Keep track of the max file descriptor */
    fdmax = server_socket;

    while(1) {
        rdset = refset;
        if (select(fdmax + 1, &rdset, NULL, NULL, NULL) == -1) {
            if(errno != EINTR) {
                dax_log(DAX_LOG_ERROR, "Server select() failure - %s", strerror(errno));
            }
            return -1;
        }

        /* Run through the existing connections looking for data to be
         * read */
        for (master_socket = 0; master_socket <= fdmax; master_socket++) {

            if (!FD_ISSET(master_socket, &rdset)) {
                continue;
            }

            if (master_socket == server_socket) {
                /* A client is asking a new connection */
                socklen_t addrlen;
                struct sockaddr_in clientaddr;
                int newfd;

                /* Handle new connections */
                addrlen = sizeof(clientaddr);
                memset(&clientaddr, 0, sizeof(clientaddr));
                newfd = accept(server_socket, (struct sockaddr *) &clientaddr, &addrlen);
                if (newfd == -1) {
                    dax_log(DAX_LOG_ERROR, "Server accept returned error - %s", strerror(errno));
                } else {
                    FD_SET(newfd, &refset);

                    if (newfd > fdmax) {
                        /* Keep track of the maximum */
                        fdmax = newfd;
                    }
                    dax_log(DAX_LOG_MINOR, "New connection from %s:%d on socket %d",
                           inet_ntoa(clientaddr.sin_addr),
                           clientaddr.sin_port,
                           newfd);
                }
            } else {
                modbus_set_socket(ctx, master_socket);
                rc = modbus_receive(ctx, query);
                if (rc > 0 && server->enable) {
                    __send_reply(server->nodes, rc, ctx, query);

                } else if (rc == -1) {
                    /* This example server in ended on connection closing or
                     * any errors. */
                    dax_log(DAX_LOG_MINOR, "Connection closed on socket %d", master_socket);
                    close(master_socket);

                    /* Remove from reference set */
                    FD_CLR(master_socket, &refset);

                    if (master_socket == fdmax) {
                        fdmax--;
                    }
                }
            }
        }
    }
    return 0;
}

/* Creates all of the tags in the given node array */
/* nodes is the array in the server or slave object
   name is just here for logging errors */
static int
__create_server_tags(mb_server_t *server) {
    int result;
    mb_node_t *node;
    tag_handle h;
    char tagname[DAX_TAGNAME_SIZE+1];
    dax_id id;

    for(int n=0; n<MB_MAX_SLAVE_NODES; n++) {
        node = server->nodes[n]; /* Just for convenience */
        if(node != NULL) {
            if(node->hold_name != NULL) {
                result = dax_tag_add(ds, &h, node->hold_name, DAX_UINT, node->hold_size, 0);
                if(result) {
                    dax_log(DAX_LOG_ERROR, "Unable to add tag %s as holding register for %s", node->hold_name, server->name);
                    free(node->hold_name);
                    node->hold_name = NULL; /* so we know we don't have a tag */
                    node->hold_size = 0;
                } else {
                    node->hold_idx = h.index;
                }
            }
            if(node->input_name != NULL) {
                result = dax_tag_add(ds, &h, node->input_name, DAX_UINT, node->input_size, 0);
                if(result) {
                    dax_log(DAX_LOG_ERROR, "Unable to add tag %s as input register for %s", node->input_name, server->name);
                    free(node->input_name);
                    node->input_name = NULL; /* so we know we don't have a tag */
                    node->input_size = 0;
                } else {
                    node->input_idx = h.index;
                }
            }
            if(node->coil_name != NULL) {
                result = dax_tag_add(ds, &h, node->coil_name, DAX_BOOL, node->coil_size, 0);
                if(result) {
                    dax_log(DAX_LOG_ERROR, "Unable to add tag %s as coil register for %s", node->coil_name, server->name);
                    free(node->coil_name);
                    node->coil_name = NULL; /* so we know we don't have a tag */
                    node->coil_size = 0;
                } else {
                    node->coil_idx = h.index;
                    node->coil_buffer = malloc(node->coil_size);
                    if(node->coil_buffer == NULL) {
                        dax_log(DAX_LOG_ERROR, "Unable to allocate buffer for coils %s", server->name);
                        free(node->coil_name);
                        node->coil_name = NULL; /* so we know we don't have a tag */
                        node->coil_size = 0;
                    }
                }
            }
            if(node->disc_name != NULL) {
                result = dax_tag_add(ds, &h, node->disc_name, DAX_BOOL, node->disc_size, 0);
                if(result) {
                    dax_log(DAX_LOG_ERROR, "Unable to add tag %s as discrete inputs register for %s", node->disc_name, server->name);
                    free(node->disc_name);
                    node->disc_name = NULL; /* so we know we don't have a tag */
                    node->disc_size = 0;
                } else {
                    node->disc_idx = h.index;
                    node->disc_buffer = malloc(node->coil_size);
                    if(node->disc_buffer == NULL) {
                        dax_log(DAX_LOG_ERROR, "Unable to allocate buffer for discretes %s", server->name);
                        free(node->disc_name);
                        node->disc_name = NULL; /* so we know we don't have a tag */
                        node->disc_size = 0;
                    }
                }
            }
            node->mb_mapping = modbus_mapping_new_start_address(
                                        node->coil_start,
                                        node->coil_size,
                                        node->disc_start,
                                        node->disc_size,
                                        node->hold_start,
                                        node->hold_size,
                                        node->input_start,
                                        node->input_size);
            if (node->mb_mapping == NULL) {
                dax_log(DAX_LOG_ERROR, "Failed to allocate the mapping: %s", modbus_strerror(errno));
                return -1;
            }

        }
    }
    snprintf(tagname, DAX_TAGNAME_SIZE, "%s_en", server->name);
    if(dax_tag_add(ds, &server->h, tagname, DAX_BOOL, 1, 0)) {
        dax_log(DAX_LOG_ERROR, "Unable to create server enable tag '%s'", tagname);
    } else {
        dax_tag_write(ds, server->h, &server->enable);
        dax_event_add(ds, &server->h, EVENT_CHANGE, NULL, &id, __server_enable_callback, server, NULL);
    }
    return 0;
}

int
start_servers(void) {
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for(int n=0; n<config.servercount; n++) {
        if(__create_server_tags(&config.servers[n]) == 0) {

            if(pthread_create(&config.servers[n].thread, &attr, (void *)&__server_thread, (void *)&config.servers[n])) {
                dax_log(DAX_LOG_ERROR, "Unable to start thread for port - %s", config.servers[n].name);
            } else {
                dax_log(DAX_LOG_MAJOR, "Started Thread for server - %s", config.servers[n].name);
            }
        }
    }
    return 0;
}
