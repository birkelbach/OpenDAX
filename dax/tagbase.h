/*  OpenDAX - An open source distributed control system 
 *  Copyright (c) 1997 Phil Birkelbach
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
 
 * This is the header file for the tagname database handling routines
 */

#include <opendax.h>
#include <sys/types.h>

#ifndef __TAGBASE_H
#define __TAGBASE_H

#ifndef DAX_TAGNAME_SIZE
 #define DAX_TAGNAME_SIZE 32
#endif

/* This is the initial size of the tagname list array */
#ifndef DAX_TAGLIST_SIZE
 #define DAX_TAGLIST_SIZE 1024
#endif

/* This is the size that the tagname list array will grow when
   the size is exceeded */
#ifndef DAX_TAGLIST_INC
 #define DAX_TAGLIST_INC 1024
#endif

/* The initial size of the database */
#ifndef DAX_DATABASE_SIZE
 #define DAX_DATABASE_SIZE 1024
#endif

/* This is the increment by which the database will grow when
   the size is exceeded */
#ifndef DAX_DATABASE_INC
 #define DAX_DATABASE_INC 1024
#endif


typedef struct Dcs_Tag {
    handle_t handle;
    char name[DAX_TAGNAME_SIZE + 1];
    unsigned int type;
    unsigned int count;
} dax_tag;


void initialize_tagbase(void);
handle_t tag_add(char *name,unsigned int type, unsigned int count);
int tag_del(char *name);
handle_t tag_get_handle(char *name);
int tag_get_type(handle_t handle);
int tag_read_bytes(handle_t handle, void *data,size_t size);
int tag_write_bytes(handle_t handle, void *data, size_t size);
int tag_mask_write(handle_t handle, void *data, void *mask, size_t size);

/* debug stuff */
void tags_list(void);
void print_database(void);

#endif /* !__TAGBASE_H */
