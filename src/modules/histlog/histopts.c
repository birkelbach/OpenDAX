/*  OpenDAX - An open source data acquisition and control system
 *  Copyright (c) 2007 Phil Birkelbach
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
 *  Configuration options source file for the opendax historical logging
 *  module.
 */

#include <string.h>
#include <opendax.h>
#include "histlog.h"

extern dax_state *ds;

tag_config *tag_list = NULL;

static int
_add_tag(lua_State *L) {
    int arg_count;
    tag_config *tag;
    const char *tagname;
    uint32_t trigger;
    double trigger_value;
    const char *attributes;

    arg_count = lua_gettop(L);
    if(arg_count < 3) {
        luaL_error(L, "add_tag() - Must have at least three arguments");
        return 0;
    }
    tagname = lua_tolstring(L, 1, NULL);
    trigger = lua_tointeger(L, 2);
    trigger_value = lua_tonumber(L, 3);
    if(arg_count >= 4) {
        attributes = lua_tolstring(L, 4, NULL);
    }
    tag = malloc(sizeof(tag_config));
    if(tag == NULL) {
        luaL_error(L, "Unable to allocate tag configuration object");
        return 0;
    }
    tag->name = strdup(tagname);
    tag->status = 0;
    tag->trigger = trigger;
    tag->trigger_value = trigger_value;
    tag->attributes = attributes;
    tag->cmpvalue = NULL;
    tag->lastvalue = NULL;
    tag->lasttimestamp = 0.0;
    tag->next = tag_list;
    /* Cheese it onto the list backwards*/
    tag_list = tag;

    return 0;
}


int
histlog_configure(int argc,char *argv[]) {
    int flags;
    int result = 0;
    lua_State *L;

    /* Create and initialize the configuration subsystem in the library */
    dax_init_config(ds, "histlog");

    flags = CFG_CMDLINE | CFG_MODCONF | CFG_ARG_REQUIRED;
    result += dax_add_attribute(ds, "plugin","plugin", 'p', flags, NULL);
    L = dax_get_luastate(ds);

    /* Add globals to the Lua Configuration State. */
    /* Recording Triggers */
    lua_pushinteger(L, ON_CHANGE);  lua_setglobal(L, "CHANGE");
    lua_pushinteger(L, ON_WRITE);  lua_setglobal(L, "WRITE");

    dax_set_luafunction(ds, (void *)_add_tag, "add_tag");

    /* Execute the configuration */
    dax_configure(ds, argc, argv, CFG_CMDLINE | CFG_MODCONF);
    /* We need to clear all of the functions that we add in case the plugin
     * want to re-run the configuration */
    dax_clear_luafunction(ds, "add_tag");
    /* We don't free the configuration because the plugin might want to use
     * parts of the Lua configuration file for specific configuration or
     * custom functions */
    return 0;
}
