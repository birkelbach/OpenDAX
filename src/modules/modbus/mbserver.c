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

#include <opendax.h>
#include <modbus/modbus.h>
#include "modmain.h"


extern config_t config;
extern dax_state *ds;



static int
__server_thread(mb_server_t *server) {
    modbus_t *ctx = NULL;
    modbus_mapping_t *mb_mapping;
    int server_socket = -1;

    uint8_t query[MODBUS_TCP_MAX_ADU_LENGTH];
    int master_socket;
    int rc;
    fd_set refset;
    fd_set rdset;
    int header_length;
    /* Maximum file descriptor number */
    int fdmax;

    if(server->name != NULL) {
        dax_log(DAX_LOG_MAJOR, "Starting Server - %s %s:%s", server->name, server->host, server->protocol);
    }
    ctx = modbus_new_tcp_pi(server->host, server->protocol);
    header_length = modbus_get_header_length(ctx);

    mb_mapping = modbus_mapping_new(16, 16, 16, 16);
    if (mb_mapping == NULL) {
        dax_log(DAX_LOG_ERROR, "Failed to allocate the mapping: %s", modbus_strerror(errno));
        modbus_free(ctx);
        return -1;
    }

    server_socket = modbus_tcp_pi_listen(ctx, 8);
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
                if (rc > 0) {
                    if(query[header_length] == 0x03) {
                        mb_mapping->tab_registers[0]++;
                    }
                    modbus_reply(ctx, query, rc, mb_mapping);
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


int
start_servers(void) {
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    for(int n=0; n<config.servercount; n++) {
        if(pthread_create(&config.servers[n].thread, &attr, (void *)&__server_thread, (void *)&config.servers[n])) {
            dax_log(DAX_LOG_ERROR, "Unable to start thread for port - %s", config.servers[n].name);
        } else {
            dax_log(DAX_LOG_MAJOR, "Started Thread for server - %s", config.servers[n].name);
        }
    }
    return 0;
}
