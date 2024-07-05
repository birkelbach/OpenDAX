/* mbluaif.c - Modbus module for OpenDAX
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
 */

#include <opendax.h>
#include <common.h>

extern dax_state *ds;

int
run_lua_callback(int fidx, uint8_t node, uint8_t function, uint16_t index, uint16_t count) {
    lua_State *L;

    if(fidx != LUA_REFNIL) {
        L = dax_get_luastate(ds);

        lua_settop(L, 0); /* Delete the stack */
        /* Get the filter function from the registry and push it onto the stack */
        lua_rawgeti(L, LUA_REGISTRYINDEX, fidx);
        lua_pushinteger(L, node);     /* First argument is the node number */
        lua_pushinteger(L, function); /* Second argument is the function code */
        lua_pushinteger(L, index);    /* Third argument is the requested register (0 based)  */
        lua_pushinteger(L, count);    /* fourth argument is the count */
        if(lua_pcall(L, 4, 1, 0) != LUA_OK) {
            dax_log(DAX_LOG_ERROR, "callback funtion for %s", lua_tostring(L, -1));
        } else { /* Success */
            return lua_tointeger(L, -1);
        }
    }
    return 0;
}