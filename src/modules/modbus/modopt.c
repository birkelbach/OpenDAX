/* Dax Modbus - Modbus (tm) Communications Module
 * Copyright (C) 2006 Phil Birkelbach
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
 * Source file containing the configuration options
 */

#define _GNU_SOURCE
#include <common.h>
#include <opendax.h>
#include <libdaxlua.h>


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>
#include <getopt.h>
#include <termios.h>
#include <signal.h>

#include "modmain.h"


static void printconfig(void);

config_t config;
extern dax_state *ds;

static void
__init_config(void)
{
    config.servercount = 0;
    config.serversize = 0;
    config.servers = NULL;
    config.threads = NULL;
}

static void
__initserver(mb_server_t *s)
{
    s->name = NULL;
    s->enable = 1;
    s->nodes = NULL;
};


// static inline int
// _get_serial_config(lua_State *L, mb_port *p)
// {
//     char *string;
//     char *device;
//     int baudrate;
//     short databits;
//     short stopbits;
//     short parity;
//     int result;

//     if(!lua_istable(L, -1)) {
//         luaL_error(L, "_get_serial_config() the top of the Lua stack is not a table");
//     }

//     lua_getfield(L, -1, "device");
//     device = (char *)lua_tostring(L, -1);
//     if(device == NULL) {
//         dax_log(DAX_LOG_WARN, "No device given for serial port %s, Using /dev/serial", p->name);
//         device = strdup("/dev/serial");
//     }

//     lua_getfield(L, -2, "baudrate");
//     baudrate = (int)lua_tonumber(L, -1);
//     if(baudrate == 0) {
//         dax_log(DAX_LOG_WARN, "Unknown Baudrate, Using 9600");
//         baudrate = 9600;
//     }

//     lua_getfield(L, -3, "databits");
//     databits = (short)lua_tonumber(L, -1);
//     if(databits < 7 || databits > 8) {
//         dax_log(DAX_LOG_WARN, "Unknown databits - %d, Using 8", databits);
//         databits = 8;
//     }

//     lua_getfield(L, -4, "stopbits");
//     stopbits = (unsigned int)lua_tonumber(L, -1);
//     if(stopbits != 1 && stopbits != 2) {
//         dax_log(DAX_LOG_WARN, "Unknown stopbits - %d, Using 1", stopbits);
//         stopbits = 1;
//     }

//     lua_getfield(L, -5, "parity");
//     if(lua_isnumber(L, -1)) {
//         parity = (unsigned char)lua_tonumber(L, -1);
//         if(parity != MB_ODD && parity != MB_EVEN && parity != MB_NONE) {
//             dax_log(DAX_LOG_WARN, "Unknown Parity %d, using NONE", parity);
//         }
//     } else {
//         string = (char *)lua_tostring(L, -1);
//         if(string) {
//             if(strcasecmp(string, "NONE") == 0) parity = MB_NONE;
//             else if(strcasecmp(string, "EVEN") == 0) parity = MB_EVEN;
//             else if(strcasecmp(string, "ODD") == 0) parity = MB_ODD;
//             else {
//                 dax_log(DAX_LOG_WARN, "Unknown Parity %s, using NONE", string);
//                 parity = MB_NONE;
//             }
//         } else {
//             dax_log(DAX_LOG_WARN, "Parity not given, using NONE");
//             parity = MB_NONE;
//         }
//     }
//     result = mb_set_serial_port(p, device, baudrate, databits, parity, stopbits);
//     lua_pop(L, 5);
//     return result;
// }

// static inline int
// _get_network_config(lua_State *L, mb_port *p)
// {
//     char *ipaddress;
//     char *string;
//     unsigned int bindport;    /* IP port to bind to */
//     unsigned char socket;     /* either UDP_SOCK or TCP_SOCK */
//     int result;

//     lua_getfield(L, -1, "ipaddress");
//     ipaddress = (char *)lua_tostring(L, -1);
//     lua_pop(L, 1);

//     lua_getfield(L, -1, "bindport");
//     bindport = (unsigned int)lua_tonumber(L, -1);
//     if(bindport == 0) bindport = 502;
//     lua_pop(L, 1);

//     lua_getfield(L, -1, "socket");
//     string = (char *)lua_tostring(L, -1);
//     if(string) {
//         if(strcasecmp(string, "TCP") == 0) socket = TCP_SOCK;
//         else if(strcasecmp(string, "UDP") == 0) socket = UDP_SOCK;
//         else {
//             dax_log(DAX_LOG_WARN, "Unknown Socket Type %s, using TCP", string);
//             socket = TCP_SOCK;
//         }
//     } else {
//         dax_log(DAX_LOG_WARN, "Socket Type not given, using TCP");
//         socket = TCP_SOCK;
//     }
//     lua_pop(L, 1);
//     if(ipaddress != NULL) {
//         result = mb_set_network_port(p, ipaddress, bindport, socket);
//     } else {
//         result = mb_set_network_port(p, "0.0.0.0", bindport, socket);
//     }
//     return result;
// }


static int
_add_server(lua_State *L)
{
    int luatype;
    mb_server_t *server;
    mb_server_t **newservers;
    const char *string, *name;
    int tmp, maxfailures, inhibit;
    unsigned char devtype, protocol, type;
    mb_type_t *udata;


    if(!lua_istable(L, -1)) {
        luaL_error(L, "add_server() received an argument that is not a table");
    }

    /* This logic allocates the port array if it does not already exist */
    if(config.servers == NULL) {
        config.servers = malloc(sizeof(mb_server_t) * DEFAULT_SERVERS);
        config.serversize = DEFAULT_SERVERS;
        if(config.servers == NULL) {
            dax_log(DAX_LOG_FATAL, "Unable to allocate port array");
            kill(getpid(), SIGQUIT);
        }
    }
    /* Check to makes sure that we have some ports left */
    if(config.servercount >= config.serversize) {
        /* Double the size of the array */
        newservers = realloc(config.servers, sizeof(mb_server_t) * config.serversize * 2);
        if(newservers != NULL) {
            config.servers = newservers;
            config.serversize *= 2;
        } else {
            dax_log(DAX_LOG_FATAL, "Unable to reallocate port array");
            kill(getpid(), SIGQUIT);
        }
    }
    /* Assign the pointer to p to make things simpler */
    server = &config.servers[config.servercount];

    dax_log(DAX_LOG_MINOR, "Adding a server at index = %d", config.servercount);

    luatype = lua_getfield(L, -1, "name");
    if(luatype == LUA_TNIL) {
        server->name = NULL;
    } else {
        server->name = strdup((char *)lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    lua_getfield(L, -1, "enable");
    server->enable = (char)lua_toboolean(L, -1);
    lua_pop(L, 1);

    /* IP address or hostname */
    luatype = lua_getfield(L, -1, "host");
    if(luatype == LUA_TNIL) {
        server->host = strdup("127.0.0.1");
    } else {
        server->host = strdup(lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    /* TCP port or protocol name */
    luatype = lua_getfield(L, -1, "protocol");
    if(luatype == LUA_TNIL) {
        server->protocol = strdup("502");
    } else {
        server->protocol = strdup((char *)lua_tostring(L, -1));
    }
    lua_pop(L,1);

    /* Allocates the node array for this server */
    server->nodes = malloc(sizeof(mb_node_def) * MB_MAX_SLAVE_NODES);
    if(server->nodes == NULL) {
        dax_log(DAX_LOG_FATAL, "Unable to allocate node definitions for server[%d]", config.servercount);
        kill(getpid(), SIGQUIT);
    }
    bzero(server->nodes, sizeof(mb_node_def) * MB_MAX_SLAVE_NODES);

    /* Setup and return the userdata that we use to identify this server in Lua */
    udata = (mb_type_t *)lua_newuserdata(L, sizeof(mb_type_t));
    luaL_getmetatable(L, "OpenDAX.mbserver");
    lua_setmetatable(L, -2);
    udata->type = MB_SERVER;
    udata->index = config.servercount;

    /* If everything worked then we increment the count and return */
    config.servercount++;

    return 1;

}


/* Lua interface function for adding a modbus command to a port.
   Accepts two arguments.  The first is the port to assign the command
   too, and the second is a table describing the command. */
// static int
// _add_command(lua_State *L)
// {
//     mb_cmd *c;
//     const char *string;
//     int result;
//     uint8_t node, function;
//     uint16_t reg, length;
//     int p; /* Port ID */
//     mb_port *port;

//     p = (int)lua_tonumber(L, 1);
//     p--; /* Lua has indexes that are 1+ our actual array indexes */
//     if(p < 0 || p >= config.portcount) {
//         luaL_error(L, "Unknown Port ID : %d", p+1);
//     }
//     if(!lua_istable(L, 2)) {
//         luaL_error(L, "add_command() received an argument that is not a table");
//     }
//     port = config.ports[p];
//     if(port->type != MB_MASTER) {
//         dax_log(DAX_LOG_WARN, "Adding commands only makes sense for a Master or Client port");
//         return 0;
//     }
//     dax_log(DAX_LOG_DEBUG, "Adding a command to port %d", p);

//     /* Allocate the new command and add it to the port */
//     c = mb_new_cmd(config.ports[p]);
//     if(c == NULL) {
//         luaL_error(L, "Can't allocate memory for the command");
//     }

//     lua_getfield(L, -1, "node");
//     node = (uint8_t)lua_tonumber(L, -1);

//     lua_getfield(L, -2, "fcode");
//     function = (uint8_t)lua_tonumber(L, -1);

//     lua_getfield(L, -3, "register");
//     reg = (uint16_t)lua_tonumber(L, -1);

//     lua_getfield(L, -4, "length");
//     length = (uint16_t)lua_tonumber(L, -1);

//     if(mb_set_command(c, node, function, reg, length)) {
//         dax_log(DAX_LOG_ERROR, "Unable to set command");
//     }
//     lua_pop(L, 4);

//     /* mode is how the command will be sent...
//      *    CONTINUOUS = sent periodically at each "interval" of the ports scanrate
//      *    TRIGGER = is sent when the trigger tag is set
//      *    CHANGE = sent when the data tag has changed
//      *    WRITE = sent when another client module writes to the data tag regarless of chagne
//      */
//     lua_getfield(L, -1, "mode");
//     string = (char *)lua_tostring(L, -1);
//     if(string != NULL) {
//         if(strcasestr(string, "CONT") != NULL) c->mode = MB_CONTINUOUS;
//         if(strcasestr(string, "TRIGGER") != NULL) c->mode |= MB_TRIGGER;
//         if(strcasestr(string, "CHANGE") != NULL) {
//             if(mb_is_write_cmd(c)) {
//                 c->mode |= MB_ONCHANGE;
//             } else {
//                 dax_log(DAX_LOG_ERROR, "ON CHANGE command trigger only makes sense for write commands");
//             }
//         }
//         if(strcasestr(string, "WRITE") != NULL) {
//             if(mb_is_write_cmd(c)) {
//                 c->mode |= MB_ONWRITE;
//             } else {
//                 dax_log(DAX_LOG_ERROR, "ON WRITE command trigger only makes sense for write commands");
//             }
//         }
//     }
//     if(c->mode == 0) {
//         dax_log(DAX_LOG_WARN, "Command Mode not given, assuming CONTINUOUS");
//         c->mode = MB_CONTINUOUS;
//     }
//     lua_pop(L, 1);

//     lua_getfield(L, -1, "enable");
//     if(lua_toboolean(L, -1)) {
//         c->enable = 1;
//     } else {
//         c->enable = 0;
//     }
//     lua_pop(L, 1);

//     /* We'll need ip address and port if we are a TCP port */
//     if(port->protocol == MB_TCP) {
//         lua_getfield(L, -1, "ipaddress");
//         string = lua_tostring(L, -1);
//         if(string != NULL) {
//             result = inet_aton(string, &c->ip_address);
//         }
//         /* inet_aton will return 0 if the address is malformed */
//         if(string == NULL || !result) {
//             dax_log(DAX_LOG_INFO, "Using Default IP address of '127.0.0.1'\n");
//             inet_aton("127.0.0.1", &c->ip_address);
//         }
//         lua_pop(L, 1);

//         lua_getfield(L, -1, "port");
//         c->port = lua_tonumber(L, -1);
//         if(c->port == 0) {
//             c->port = 502; /* 502 is the default modbus port */
//         }
//         lua_pop(L, 1);
//     }

//     lua_getfield(L, -1, "tagname");
//     string = (char *)lua_tostring(L, -1);
//     if(string != NULL) {
//         c->data_tag = strdup(string);
//     } else {
//         dax_log(DAX_LOG_ERROR, "No Tagname Given for Command on Port %d", p);
//     }
//     lua_pop(L,1);
//     lua_getfield(L, -1, "tagcount");
//     c->tagcount = lua_tointeger(L, -1);
//     if(c->tagcount == 0) {
//         dax_log(DAX_LOG_ERROR, "No tag count given.  Using 1 as default");
//         c->tagcount = 1;
//     }
//     lua_pop(L,1);

//     if(c->mode & MB_TRIGGER) {
//         lua_getfield(L, -1, "trigger");
//         string = (char *)lua_tostring(L, -1);
//         if(string != NULL) {
//             c->trigger_tag = strdup(string);
//         } else {
//             dax_log(DAX_LOG_ERROR, "No Tagname Given for Trigger on Port %d", p);
//             c->mode &= ~MB_TRIGGER;
//         }
//         lua_pop(L,1);
//     }

//     lua_getfield(L, -1, "interval");
//     mb_set_interval(c, (int)lua_tonumber(L, -1));
//     lua_pop(L,1);
//     return 0;
// }

/* This returns a pointer to the node stucture for the given port.
   Allocates the node if that has not been done yet.  Returns NULL
   on failure and a valid pointer otherwise */
// static mb_node_def *
// _get_node(mb_port *port, int nodeid) {

//     if(port->nodes[nodeid] == NULL) { /* If it hasn't been allocated yet */
//         port->nodes[nodeid] = malloc(sizeof(mb_node_def));
//         if(port->nodes[nodeid] != NULL) {

//             /* Initialize 'name' to NULL so later we can know if it was set */
//             port->nodes[nodeid]->hold_name = NULL;
//             port->nodes[nodeid]->input_name = NULL;
//             port->nodes[nodeid]->coil_name = NULL;
//             port->nodes[nodeid]->disc_name = NULL;
//             port->nodes[nodeid]->hold_size = 0;
//             port->nodes[nodeid]->input_size = 0;
//             port->nodes[nodeid]->coil_size = 0;
//             port->nodes[nodeid]->disc_size = 0;
//             port->nodes[nodeid]->read_callback = LUA_REFNIL;
//             port->nodes[nodeid]->write_callback = LUA_REFNIL;
//         }
//     }
//     return port->nodes[nodeid];

// }

/* Lua interface function for adding a modbus slave register tag
   to a port.  Accepts five arguments.
   Arguments:
      port id
      node / unit id
      tag name
      size (in registers or bits)
      register type [COIL, DISCRETE, HOLDING or INPUT]. */
static int
_add_register(lua_State *L)
{
    const char *tagname;
    int mbreg;
    int mbstart;
    unsigned int size;
    int nodeid;
    mb_type_t *mbtype;

    if(lua_isuserdata(L, 1)) {
        mbtype = luaL_checkudata(L, 1, "OpenDAX.mbserver");
    }
    if(mbtype->type == MB_SERVER) {
        printf("add_register called on server %d\n", mbtype->index);
    } else if(mbtype->type == MB_SLAVE) {
        printf("add_register called on slave %d\n", mbtype->index);
    } else {
        luaL_error(L, "First argument to add_register should be a server or a slave id");
    }

    // mb_port *port;
    // mb_node_def *node;

    // p = lua_tointeger(L, 1);
    // p--; /* Lua has indexes that are 1+ our actual array indexes */
    // if(p < 0 || p >= config.portcount) {
    //     luaL_error(L, "Unknown Port ID : %d", p);
    // }
    // port = config.ports[p];

    // dax_log(DAX_LOG_DEBUG, "Adding a register to port %s", port->name);

    // nodeid = lua_tointeger(L, 2);
    // if(nodeid <0 || nodeid >= MB_MAX_SLAVE_NODES) {
    //     luaL_error(L, "Invalid node id given for register on Port %s", port->name);
    // }

    // tagname = (char *)lua_tostring(L, 3);
    // if(tagname == NULL) {
    //     luaL_error(L, "No tagname Given for register on Port %s", port->name);
    // }

    // size = lua_tointeger(L, 4);
    // if(size == 0 || size > 65535) {
    //     luaL_error(L, "Register size must be between 1-65535 on Port %s", port->name);
    // }

    // node = _get_node(port, nodeid);
    // if(node == NULL) {
    //     luaL_error(L, "Unable to allocate memory for node on port %s", port->name);
    // }

    // mbreg = lua_tointeger(L, 5);
    // switch(mbreg) {
    //     case MB_REG_HOLDING:
    //         node->hold_name = strdup(tagname);
    //         node->hold_size = size;
    //         break;
    //     case MB_REG_INPUT:
    //         node->input_name = strdup(tagname);
    //         node->input_size = size;
    //         break;
    //     case MB_REG_COIL:
    //         node->coil_name = strdup(tagname);
    //         node->coil_size = size;
    //         break;
    //     case MB_REG_DISC:
    //         node->disc_name = strdup(tagname);
    //         node->disc_size = size;
    //         break;
    //     default:
    //         luaL_error(L, "Invalid register type given on Port %s", port->name);
    // }
    return 0;
}

/* Lua interface function for adding a modbus slave read callback function
   to a port.  Accepts five arguments.
   Arguments:
      port id
      node / unit id
      function
*/
// static int
// _add_read_callback(lua_State *L)
// {
//     int nodeid;
//     int p;
//     mb_port *port;
//     mb_node_def *node;

//     p = lua_tointeger(L, 1);
//     p--; /* Lua has indexes that are 1+ our actual array indexes */
//     if(p < 0 || p >= config.portcount) {
//         luaL_error(L, "Unknown Port ID : %d", p);
//     }
//     port = config.ports[p];
//     if(port->type != MB_SLAVE) {
//         dax_log(DAX_LOG_WARN, "Adding registers only makes sense for a Slave or Server port");
//         return 0;
//     }
//     dax_log(DAX_LOG_DEBUG, "Adding a register to port %s", port->name);

//     nodeid = lua_tointeger(L, 2);
//     if(nodeid <0 || nodeid >= MB_MAX_SLAVE_NODES) {
//         luaL_error(L, "Invalid node id given for register on Port %s", port->name);
//     }

//     node = _get_node(port, nodeid);
//     if(node == NULL) {
//         luaL_error(L, "Unable to allocate memory for node on port %s", port->name);
//     }

//     lua_settop(L, 3); /*put the function at the top of the stack */
//     if(! lua_isfunction(L, -1)) {
//         luaL_error(L, "callback should be a function ");
//     }
//     /* Pop the function off the stack and write it to the regsitry and assign
//        the reference to read_callback */
//     node->read_callback = luaL_ref(L, LUA_REGISTRYINDEX);

//     return 0;
// }

/* Lua interface function for adding a modbus slave read callback function
   to a port.  Accepts five arguments.
   Arguements:
      port id
      node / unit id
      function
*/
// static int
// _add_write_callback(lua_State *L)
// {
//     int nodeid;
//     int p;
//     mb_port *port;
//     mb_node_def *node;

//     p = lua_tointeger(L, 1);
//     p--; /* Lua has indexes that are 1+ our actual array indexes */
//     if(p < 0 || p >= config.portcount) {
//         luaL_error(L, "Unknown Port ID : %d", p);
//     }
//     port = config.ports[p];
//     if(port->type != MB_SLAVE) {
//         dax_log(DAX_LOG_WARN, "Adding registers only makes sense for a Slave or Server port");
//         return 0;
//     }
//     dax_log(DAX_LOG_DEBUG, "Adding a register to port %s", port->name);

//     nodeid = lua_tointeger(L, 2);
//     if(nodeid <0 || nodeid >= MB_MAX_SLAVE_NODES) {
//         luaL_error(L, "Invalid node id given for register on Port %s", port->name);
//     }

//     node = _get_node(port, nodeid);
//     if(node == NULL) {
//         luaL_error(L, "Unable to allocate memory for node on port %s", port->name);
//     }

//     lua_settop(L, 3); /*put the function at the top of the stack */
//     if(! lua_isfunction(L, -1)) {
//         luaL_error(L, "callback should be a function ");
//     }
//     /* Pop the function off the stack and write it to the regsitry and assign
//        the reference to .write_callback */
//     node->write_callback = luaL_ref(L, LUA_REGISTRYINDEX);

//     return 0;
// }


/* This function should be called from main() to configure the program.
 * First the defaults are set then the configuration file is parsed then
 * the command line is handled.  This gives the command line priority.  */
int
modbus_configure(int argc, const char *argv[])
{
    int flags, result = 0;
    lua_State *L;

    __init_config();
    flags = CFG_CMDLINE | CFG_MODCONF | CFG_ARG_REQUIRED;
    //result += dax_add_attribute(ds, "tagname", "tagname", 't', flags, "modbus");
    result += dax_add_attribute(ds, "user", "user", 'u', flags, NULL);
    result += dax_add_attribute(ds, "group", "group", 'g', flags, NULL);

    L = dax_get_luastate(ds);
    lua_pushinteger(L, MB_REG_HOLDING);
    lua_setglobal(L, "HOLDING");
    lua_pushinteger(L, MB_REG_INPUT);
    lua_setglobal(L, "INPUT");
    lua_pushinteger(L, MB_REG_COIL);
    lua_setglobal(L, "COIL");
    lua_pushinteger(L, MB_REG_DISC);
    lua_setglobal(L, "DISCRETE");
    luaL_newmetatable(L, "OpenDAX.mbserver");

    dax_set_luafunction(ds, (void *)_add_server, "add_server");
    // dax_set_luafunction(ds, (void *)_add_command, "add_command");
    dax_set_luafunction(ds, (void *)_add_register, "add_register");
    // dax_set_luafunction(ds, (void *)_add_read_callback, "add_read_callback");
    // dax_set_luafunction(ds, (void *)_add_write_callback, "add_write_callback");

    result = dax_configure(ds, argc, (char **)argv, CFG_CMDLINE | CFG_MODCONF);

    dax_clear_luafunction(ds, "add_server");
    // dax_clear_luafunction(ds, "add_command");
    dax_clear_luafunction(ds, "add_register");
    // dax_clear_luafunction(ds, "add_read_callback");
    // dax_clear_luafunction(ds, "add_write_callback");

    /* Add functions that make sense for any callbacks that might be configured.
       The callback functions will live in the configuration Lua state */
    daxlua_register_function(L, "tag_get");
    daxlua_register_function(L, "tag_handle");
    daxlua_register_function(L, "tag_read");
    daxlua_register_function(L, "tag_write");
    /* TODO: Should add atomic operations */
    daxlua_register_function(L, "log");
    daxlua_register_function(L, "sleep");

    daxlua_set_constants(L);
    /* register the libraries that we need*/
    luaL_requiref(L, "_G", luaopen_base, 1);
    luaL_requiref(L, "math", luaopen_math, 1);
    luaL_requiref(L, "table", luaopen_table, 1);
    luaL_requiref(L, "string", luaopen_string, 1);
    luaL_requiref(L, "utf8", luaopen_utf8, 1);
    lua_pop(L, 5);


    printconfig();

    return result;
}


static void
__print_serverconfig(void) {
   for(int n=0; n<config.servercount; n++) {
        fprintf(stderr, "Server: %s\n", config.servers[n].name);
        fprintf(stderr, "  Host: %s\n", config.servers[n].host);
        fprintf(stderr, "  Protocol: %s\n", config.servers[n].protocol);
    }

}

/* TODO: Really should print out more than this*/
static void
printconfig(void)
{
    int n;

    fprintf(stderr, "\n----------Modbus Configuration-----------\n\n");
    __print_serverconfig();

}
