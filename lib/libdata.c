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

 * This file contains some of the database handling code for the library
 */

#include <libdax.h>
#include <dax/libcommon.h>

/* Tag Cache Handling Code
 * The tag cache is a doubly linked circular list
 * the head pointer is fixed at the top of the list
 * tag searched for will bubble up one item in the list
 * each time it's found.  This will make the top of the list
 * the most searched for tags and the bottom the lesser used tags.
 */

/* TODO: I need a way to invalidate the tag cache so that changes
   to the tag can be put into the tag cache */

/* This is the structure for our tag cache */
typedef struct Tag_Cnode {
    tag_idx_t idx;
    unsigned int type;
    unsigned int count;
    struct Tag_Cnode *next;
    struct Tag_Cnode *prev;
    char name[DAX_TAGNAME_SIZE + 1];
} tag_cnode;

static tag_cnode *_cache_head; /* First node in the cache list */
static int _cache_limit;       /* Total number of nodes that we'll allocate */
static int _cache_count;       /* How many nodes we actually have */

int
init_tag_cache(void)
{
    _cache_head = NULL;
    _cache_limit = strtol(dax_get_attr("cachesize"), NULL, 0);
    _cache_count = 0;
    return 0;
}

/****** FOR TESTING ONLY ********
static void
print_cache(void)
{
    int n;
    tag_cnode *this;
    this = _cache_head;
    printf("_cache_head->handle = {%d}\n", _cache_head->handle);
    for(n=0; n<_cache_count; n++) {
        printf("{%d} - %s\t\t", this->handle, this->name);
        printf("this->prev = {%d}", this->prev->handle);
        printf(" : this->next = {%d}\n", this->next->handle);
        this = this->next;
    }
    printf("\n{ count = %d, head = %p }\n", _cache_count, _cache_head);
    //printf("{ head->handle = %d, tail->handle = %d head->prev = %p, tail->next = %p}\n", 
    //       _cache_head->handle, _cache_tail->handle, _cache_head->prev, _cache_tail->next);
}
*/
 
/* This function assigns the data to *tag and bubbles
   this up one node in the list */
static inline void
_cache_hit(tag_cnode *this, dax_tag *tag)
{
    tag_cnode *before, *after;
    
    /* Store the return values in tag */
    strcpy(tag->name, this->name);
    tag->idx = this->idx;
    tag->type = this->type;
    tag->count = this->count;
    
    /* Bubble up:
     'after' is set to the node that we swap with
     'before' is set to the node that will be before
     us after the swap.  The special case is when our
     node will become the first one, then we move the head. */
    if(this != _cache_head) { /* Do nothing if we are already at the top */
        /* If there are only two then we do it the easy way */
        if(_cache_count == 2) {
            _cache_head = _cache_head->next;
        } else {
            if(this == _cache_head->next) _cache_head = this;
            after = this->prev;
            before = after->prev;
            before->next = this;
            after->prev = this;
            after->next = this->next;
            this->next->prev = after;
            this->next = after;
            this->prev = before;
        }
    }
    //--print_cache();
}

/* Used to check if a tag with the given handle is in the 
 * cache.  Returns a pointer to the tag if found and
 * returns ERR_NOTFOUND otherwise */
int
check_cache_index(tag_idx_t idx, dax_tag *tag)
{
    tag_cnode *this;
    
    if(_cache_head != NULL) {
        this = _cache_head;
    } else {
        return ERR_NOTFOUND;
    }
    if(_cache_head->idx != idx) {
        this = this->next;
        
        while(this != _cache_head && this->idx != idx) {
            this = this->next;
        }
        if(this == _cache_head) {
            return ERR_NOTFOUND;
        }
    }
    
    _cache_hit(this, tag);
    
    return 0;
}

/* Used to check if a tag with the given name is in the cache
 * If it is found then a pointer to the tag is returned and
 * ERR_NOTFOUND otherwise */
int
check_cache_name(char *name, dax_tag *tag)
{
    tag_cnode *this;
    
    if(_cache_head != NULL) {
        this = _cache_head;
    } else {
        return ERR_NOTFOUND;
    }
    if( strcmp(_cache_head->name, name) ) {
        this = this->next;
        
        while(this != _cache_head && strcmp(this->name, name)) {
            this = this->next;
        }
        if(this == _cache_head) {
            return ERR_NOTFOUND;
        }
    }
    
    _cache_hit(this, tag);
    
    return 0;
}

/* Adds a tag to the cache */
int
cache_tag_add(dax_tag *tag)
{
    tag_cnode *new;
    
    if(_cache_head == NULL) { /* First one */
        //--printf("adding {%d} to the beginning\n", tag->handle);
        new = malloc(sizeof(tag_cnode));
        if(new) {
            new->next = new;
            new->prev = new;
            _cache_head = new;
            _cache_count++;
        } else {
            return ERR_ALLOC;
        }
    } else if(_cache_count < _cache_limit) { /* Add to end */
        //--printf("adding {%d} to the end\n", tag->handle);
        new = malloc(sizeof(tag_cnode));
        if(new) {
            new->next = _cache_head;
            new->prev = _cache_head->prev;
            _cache_head->prev->next = new;
            _cache_head->prev = new;
            _cache_count++;
        } else {
            return ERR_ALLOC;
        }
    } else {
        //--printf("Just putting {%d}  last\n", tag->handle);
        new = _cache_head->prev;
    }
    strcpy(new->name, tag->name);
    new->idx = tag->idx;
    new->type = tag->type;
    new->count = tag->count;
    //--print_cache();
    
    return 0;
}

/* Type specific reading and writing functions.  These should be the most common
 * methods to read and write tags to the sever.*/

/* This is a recursive function that traverses the *data and makes the proper
 * data conversions based on the data type. */
static inline int
_read_format(type_t type, int count, void *data, int offset)
{
    int n, pos, result;
    char *newdata;
    datatype *dtype = NULL;
    cdt_member *this = NULL;
    
    newdata = (char *)data + offset;
    if(IS_CUSTOM(type)) {
        /* iterate through the list */
        dtype = get_cdt_pointer(type);
        pos = offset;
        if(dtype != NULL) {
            this = dtype->members;
        } else {
            return ERR_NOTFOUND;
        }
        while(this != NULL) {
            result = _read_format(this->type, this->count, data, pos);
            if(result) return result;
            if(IS_CUSTOM(this->type)) {
                pos += (get_typesize(this->type) * this->count);
            } else {
                /* This gets the size in bits */
                pos += TYPESIZE(this->type) * this->count / 8;
            }
            this = this->next;
        }
    } else {
        switch(type) {
            case DAX_BOOL:
            case DAX_BYTE:
            case DAX_SINT:
                /* Since there are no conversions for byte level tags we do nothing */
                break;
            case DAX_WORD:
            case DAX_UINT:
                for(n = 0; n < count; n++) {
                    ((dax_uint_t *)newdata)[n] = stom_uint(((dax_uint_t *)newdata)[n]);
                }
                break;
            case DAX_INT:
                for(n = 0; n < count; n++) {
                    ((dax_int_t *)newdata)[n] = stom_int(((dax_int_t *)newdata)[n]);
                }
                break;
            case DAX_DWORD:
            case DAX_UDINT:
            case DAX_TIME:
                for(n = 0; n < count; n++) {
                    ((dax_udint_t *)newdata)[n] = stom_udint(((dax_udint_t *)newdata)[n]);
                }
                break;
            case DAX_DINT:
                for(n = 0; n < count; n++) {
                    ((dax_dint_t *)newdata)[n] = stom_dint(((dax_dint_t *)newdata)[n]);
                }
                break;
            case DAX_REAL:
                for(n = 0; n < count; n++) {
                    ((dax_real_t *)newdata)[n] = stom_real(((dax_real_t *)newdata)[n]);
                }
                break;
            case DAX_LWORD:
            case DAX_ULINT:
                for(n = 0; n < count; n++) {
                    ((dax_ulint_t *)newdata)[n] = stom_ulint(((dax_ulint_t *)newdata)[n]);
                }
                break;
            case DAX_LINT:
                for(n = 0; n < count; n++) {
                    ((dax_lint_t *)newdata)[n] = stom_lint(((dax_lint_t *)newdata)[n]);
                }
                break;
            case DAX_LREAL:
                for(n = 0; n < count; n++) {
                    ((dax_lreal_t *)newdata)[n] = stom_lreal(((dax_lreal_t *)newdata)[n]);
                }
                break;
            default:
                return ERR_ARG;
                break;
        }    
    }
    return 0;
}


int
dax_read_tag(handle_t handle, void *data)
{
    int result, n, i;
    
    result = dax_read(handle.index, handle.byte, data, handle.size);
    if(result) return result;

    /* The only time that the bit index should be greater than 0 is if
     * the tag datatype is BOOL.  If not the bytes should be aligned.
     * If there is a bit index then we need to 'realign' the bits so that
     * the bits that the handle point to start at the top of the *data buffer */
    if(handle.type == DAX_BOOL && handle.bit > 0) {
        i = handle.bit;
        for(n = 0; n < handle.count; n++) {
            if( (0x01 << i % 8) & ((char *)data)[i / 8] ) {
                ((char *)data)[n / 8] |= (1 << (n % 8));
            }
            i++;
        }
    } else {
        return _read_format(handle.type, handle.count, data, 0);
    }
    return 0;
}

//int
//dax_read_tag(tag_idx_t idx, int index, void *data, int count, type_t type)
//{
//    int bytes, result, offset, n, i;
//    char *buff;
//    
//    if(type == DAX_BOOL) {
//        bytes = (index + count - 1)  / 8 - index / 8 + 1;
//        offset = index / 8;
//    } else {
//        bytes = (TYPESIZE(type) / 8) * count;
//        offset = (TYPESIZE(type) / 8) * index;
//    }
//    buff = alloca(bytes);
//    bzero(buff, bytes);
//    bzero(data, (count - 1) / 8 + 1);
//    result = dax_read(idx, offset, buff, bytes);
//    
//    if(result) return result;
//    
//    switch(type) {
//        case DAX_BOOL:
//            i = index % 8;
//            for(n = 0; n < count; n++) {
//                if( (0x01 << i % 8) & buff[i / 8] ) {
//                    ((char *)data)[n / 8] |= (1 << (n % 8));
//                }
//                i++;
//            }
//            break;
//        case DAX_BYTE:
//        case DAX_SINT:
//            memcpy(data, buff, bytes);
//            break;
//        case DAX_WORD:
//        case DAX_UINT:
//            for(n = 0; n < count; n++) {
//                ((dax_uint_t *)data)[n] = stom_uint(((dax_uint_t *)buff)[n]);
//            }
//            break;
//        case DAX_INT:
//            for(n = 0; n < count; n++) {
//                ((dax_int_t *)data)[n] = stom_int(((dax_int_t *)buff)[n]);
//            }
//            break;
//        case DAX_DWORD:
//        case DAX_UDINT:
//        case DAX_TIME:
//            for(n = 0; n < count; n++) {
//                ((dax_udint_t *)data)[n] = stom_udint(((dax_udint_t *)buff)[n]);
//            }
//            break;
//        case DAX_DINT:
//            for(n = 0; n < count; n++) {
//                ((dax_dint_t *)data)[n] = stom_dint(((dax_dint_t *)buff)[n]);
//            }
//            break;
//        case DAX_REAL:
//            for(n = 0; n < count; n++) {
//                ((dax_real_t *)data)[n] = stom_real(((dax_real_t *)buff)[n]);
//            }
//            break;
//        case DAX_LWORD:
//        case DAX_ULINT:
//            for(n = 0; n < count; n++) {
//                ((dax_ulint_t *)data)[n] = stom_ulint(((dax_ulint_t *)buff)[n]);
//            }
//            break;
//        case DAX_LINT:
//            for(n = 0; n < count; n++) {
//                ((dax_lint_t *)data)[n] = stom_lint(((dax_lint_t *)buff)[n]);
//            }
//            break;
//        case DAX_LREAL:
//            for(n = 0; n < count; n++) {
//                ((dax_lreal_t *)data)[n] = stom_lreal(((dax_lreal_t *)buff)[n]);
//            }
//            break;
//        default:
//            return ERR_ARG;
//            break;
//    }    
//    return 0;
//}
//

static inline int
_write_format(type_t type, int count, void *data, int offset)
{
    int n, pos, result;
    char *newdata;
    datatype *dtype = NULL;
    cdt_member *this = NULL;
    
    newdata = (char *)data + offset;
    if(IS_CUSTOM(type)) {
        /* iterate through the list */
        dtype = get_cdt_pointer(type);
        pos = offset;
        if(dtype != NULL) {
            this = dtype->members;
        } else {
            return ERR_NOTFOUND;
        }
        while(this != NULL) {
            result = _write_format(this->type, this->count, newdata, pos);
            if(result) return result;
            if(IS_CUSTOM(this->type)) {
                pos += (get_typesize(this->type) * this->count);
            } else {
                /* This gets the size in bits */
                pos += TYPESIZE(this->type) * this->count / 8;
            }
            this = this->next;
        }
    } else {
        switch(type) {
            case DAX_BOOL:
            case DAX_BYTE:
            case DAX_SINT:
                break;
            case DAX_WORD:
            case DAX_UINT:
                for(n = 0; n < count; n++) {
                    ((dax_uint_t *)newdata)[n] = mtos_uint(((dax_uint_t *)newdata)[n]);
                }
                break;
            case DAX_INT:
                for(n = 0; n < count; n++) {
                    ((dax_int_t *)newdata)[n] = mtos_int(((dax_int_t *)newdata)[n]);
                }
                break;
            case DAX_DWORD:
            case DAX_UDINT:
            case DAX_TIME:
                for(n = 0; n < count; n++) {
                    ((dax_udint_t *)newdata)[n] = mtos_udint(((dax_udint_t *)newdata)[n]);
                }
                break;
            case DAX_DINT:
                for(n = 0; n < count; n++) {
                    ((dax_dint_t *)newdata)[n] = mtos_dint(((dax_dint_t *)newdata)[n]);
                }
                break;
            case DAX_REAL:
                for(n = 0; n < count; n++) {
                    ((dax_real_t *)newdata)[n] = mtos_real(((dax_real_t *)newdata)[n]);
                }
                break;
            case DAX_LWORD:
            case DAX_ULINT:
                for(n = 0; n < count; n++) {
                    ((dax_ulint_t *)newdata)[n] = mtos_ulint(((dax_ulint_t *)newdata)[n]);
                }
                break;
            case DAX_LINT:
                for(n = 0; n < count; n++) {
                    ((dax_lint_t *)newdata)[n] = mtos_lint(((dax_lint_t *)newdata)[n]);
                }
                break;
            case DAX_LREAL:
                for(n = 0; n < count; n++) {
                    ((dax_lreal_t *)newdata)[n] = mtos_lreal(((dax_lreal_t *)newdata)[n]);
                }
                break;
            default:
                return ERR_ARG;
                break;
        }
    }

    return 0;
}



int
dax_write_tag(handle_t handle, void *data)
{
    int i, n, result = 0;
    char *mask = NULL;
    
    if(handle.type == DAX_BOOL && handle.bit > 0) {
        mask = malloc(handle.size);
        if(mask == NULL) return ERR_ALLOC;
        bzero(mask, handle.size);
            
        i = handle.bit % 8;
        for(n = 0; n < handle.count; n++) {
            if( (0x01 << n % 8) & ((char *)data)[n / 8] ) {
                ((char *)data)[i / 8] |= (1 << (i % 8));
            }
            mask[i / 8] |= (1 << (i % 8));
            i++;
        }
    } else {
        result =  _write_format(handle.type, handle.count, data, 0);
    }
    if(result) return result;
    if(handle.type == DAX_BOOL && handle.bit > 0) {
        result = dax_mask(handle.index, handle.byte, data, mask, handle.size);
        free(mask);
    } else {    
        result = dax_write(handle.index, handle.byte, data, handle.size);
    }
    return result;

    
    return 0;
}

//int
//dax_write_tag(tag_idx_t idx, int index, void *data, int count, type_t type)
//{
//    int bytes, result, offset, n, i;
//    char *buff, *mask;
//    
//    if(type == DAX_BOOL) {
//        bytes = (index + count - 1)  / 8 - index / 8 + 1;
//        offset = index / 8;
//    } else {
//        bytes = (TYPESIZE(type) / 8) * count;
//        offset = (TYPESIZE(type) / 8) * index;
//    }
//    buff = alloca(bytes);
//    
//    switch(type) {
//        case DAX_BOOL:
//            mask = alloca(bytes);
//            bzero(buff, bytes);
//            bzero(mask, bytes);
//            
//            i = index % 8;
//            for(n = 0; n < count; n++) {
//                if( (0x01 << n % 8) & ((char *)data)[n / 8] ) {
//                    buff[i / 8] |= (1 << (i % 8));
//                }
//                mask[i / 8] |= (1 << (i % 8));
//                i++;
//            }            
//            break;
//        case DAX_BYTE:
//        case DAX_SINT:
//            memcpy(buff, data, bytes);
//            break;
//        case DAX_WORD:
//        case DAX_UINT:
//            for(n = 0; n < count; n++) {
//                ((dax_uint_t *)buff)[n] = mtos_uint(((dax_uint_t *)data)[n]);
//            }
//            break;
//        case DAX_INT:
//            for(n = 0; n < count; n++) {
//                ((dax_int_t *)buff)[n] = mtos_int(((dax_int_t *)data)[n]);
//            }
//            break;
//        case DAX_DWORD:
//        case DAX_UDINT:
//        case DAX_TIME:
//            for(n = 0; n < count; n++) {
//                ((dax_udint_t *)buff)[n] = mtos_udint(((dax_udint_t *)data)[n]);
//            }
//            break;
//        case DAX_DINT:
//            for(n = 0; n < count; n++) {
//                ((dax_dint_t *)buff)[n] = mtos_dint(((dax_dint_t *)data)[n]);
//            }
//            break;
//        case DAX_REAL:
//            for(n = 0; n < count; n++) {
//                ((dax_real_t *)buff)[n] = mtos_real(((dax_real_t *)data)[n]);
//            }
//            break;
//        case DAX_LWORD:
//        case DAX_ULINT:
//            for(n = 0; n < count; n++) {
//                ((dax_ulint_t *)buff)[n] = mtos_ulint(((dax_ulint_t *)data)[n]);
//            }
//            break;
//        case DAX_LINT:
//            for(n = 0; n < count; n++) {
//                ((dax_lint_t *)buff)[n] = mtos_lint(((dax_lint_t *)data)[n]);
//            }
//            break;
//        case DAX_LREAL:
//            for(n = 0; n < count; n++) {
//                ((dax_lreal_t *)buff)[n] = mtos_lreal(((dax_lreal_t *)data)[n]);
//            }
//            break;
//        default:
//            return ERR_ARG;
//            break;
//    }
//    if(type == DAX_BOOL) {
//        result = dax_mask(idx, offset, buff, mask, bytes);
//    } else {    
//        result = dax_write(idx, offset, buff, bytes);
//    }
//    return result;
//}


int
dax_mask_tag(handle_t handle, void *data, void*mask)
{
    
    return 0;
}

//int
//dax_mask_tag(tag_idx_t idx, int index, void *data, void *mask, int count, type_t type)
//{
//    int bytes, result, offset, n, i;
//    char *buff, *maskbuff;
//    
//    if(type == DAX_BOOL) {
//        bytes = (index + count - 1)  / 8 - index / 8 + 1;
//        offset = index / 8;
//    } else {
//        bytes = (TYPESIZE(type) / 8) * count;
//        offset = (TYPESIZE(type) / 8) * index;
//    }
//    buff = alloca(bytes);
//    maskbuff = alloca(bytes);
//    
//    switch(type) {
//        case DAX_BOOL:
//            bzero(buff, bytes);
//            bzero(maskbuff, bytes);
//            
//            i = index % 8;
//            for(n = 0; n < count; n++) {
//                if( (0x01 << n % 8) & ((char *)data)[n / 8] ) {
//                    buff[i / 8] |= (1 << (i % 8));
//                }
//                maskbuff[i / 8] |= (1 << (i % 8));
//                i++;
//            }            
//            break;
//        case DAX_BYTE:
//        case DAX_SINT:
//            memcpy(buff, data, bytes);
//            memcpy(maskbuff, mask, bytes);
//            break;
//        case DAX_WORD:
//        case DAX_UINT:
//            for(n = 0; n < count; n++) {
//                ((dax_uint_t *)buff)[n] = mtos_uint(((dax_uint_t *)data)[n]);
//                ((dax_uint_t *)maskbuff)[n] = mtos_uint(((dax_uint_t *)mask)[n]);
//            }
//            break;
//        case DAX_INT:
//            for(n = 0; n < count; n++) {
//                ((dax_int_t *)buff)[n] = mtos_int(((dax_int_t *)data)[n]);
//                ((dax_int_t *)maskbuff)[n] = mtos_int(((dax_int_t *)mask)[n]);
//            }
//            break;
//        case DAX_DWORD:
//        case DAX_UDINT:
//        case DAX_TIME:
//            for(n = 0; n < count; n++) {
//                ((dax_udint_t *)buff)[n] = mtos_udint(((dax_udint_t *)data)[n]);
//                ((dax_udint_t *)maskbuff)[n] = mtos_udint(((dax_udint_t *)mask)[n]);
//            }
//            break;
//        case DAX_DINT:
//            for(n = 0; n < count; n++) {
//                ((dax_dint_t *)buff)[n] = mtos_dint(((dax_dint_t *)data)[n]);
//                ((dax_dint_t *)maskbuff)[n] = mtos_dint(((dax_dint_t *)mask)[n]);
//            }
//            break;
//        case DAX_REAL:
//            for(n = 0; n < count; n++) {
//                ((dax_real_t *)buff)[n] = mtos_real(((dax_real_t *)data)[n]);
//                ((dax_real_t *)maskbuff)[n] = mtos_real(((dax_real_t *)mask)[n]);
//            }
//            break;
//        case DAX_LWORD:
//        case DAX_ULINT:
//            for(n = 0; n < count; n++) {
//                ((dax_ulint_t *)buff)[n] = mtos_ulint(((dax_ulint_t *)data)[n]);
//                ((dax_ulint_t *)maskbuff)[n] = mtos_ulint(((dax_ulint_t *)mask)[n]);
//            }
//            break;
//        case DAX_LINT:
//            for(n = 0; n < count; n++) {
//                ((dax_lint_t *)buff)[n] = mtos_lint(((dax_lint_t *)data)[n]);
//                ((dax_lint_t *)maskbuff)[n] = mtos_lint(((dax_lint_t *)mask)[n]);
//            }
//            break;
//        case DAX_LREAL:
//            for(n = 0; n < count; n++) {
//                ((dax_lreal_t *)buff)[n] = mtos_lreal(((dax_lreal_t *)data)[n]);
//                ((dax_lreal_t *)maskbuff)[n] = mtos_lreal(((dax_lreal_t *)mask)[n]);
//            }
//            break;
//        default:
//            return ERR_ARG;
//            break;
//    }
//    result = dax_mask(idx, offset, buff, maskbuff, bytes);
//    return result;
//}

