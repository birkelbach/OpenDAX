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

int opcua_configure(dax_state *ds, int argc, char *argv[]);

int addTagVariable(UA_Server *server, dax_tag *tag);

#endif /* __DAXOPCUA_H */