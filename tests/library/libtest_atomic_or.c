/*  OpenDAX - An open source data acquisition and control system
 *  Copyright (c) 2021 Phil Birkelbach
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
 */

/*
 *  This test checks the atomic complement operation
 */

#include <common.h>
#include <opendax.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "libtest_common.h"

static int
_bool_test(dax_state *ds) {
    int result;
    tag_handle h, h2;
    dax_byte temp[3];
    dax_byte temp1[3];
    dax_byte output[3];

    result = dax_tag_add(ds, &h, "bool_test", DAX_BOOL, 24, 0);
    if(result) return -1;
    /* Here we complement all 16 bits */
    temp[0] = 0xAA; temp[1] = 0x55;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    temp1[0] = 0x55; temp1[1] = 0xAA;
    result = dax_atomic_op(ds, h, temp1, ATOMIC_OP_OR);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    DF("in = %X %X", temp[0], temp[1]);
    DF("out= %X %X", output[0], output[1]);
    if(output[0] != 0xFF || output[1] != 0xFF) {
        return -1;
    }

    /* No we do a partial subset of the bits */
    result = dax_tag_handle(ds, &h2, "bool_test[3]", 10);
    temp[0] = 0x55; temp[1] = 0x55;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    temp1[0] = 0x55; temp1[1] = 0x55;
    result = dax_atomic_op(ds, h2, temp1, ATOMIC_OP_OR);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    DF("in     = %X %X", temp[0], temp[1]);
    DF("output = %X %X", output[0], output[1]);
    if(output[0] != 0xFD || output[1] != 0x5F) return -1;

    /* No we do a partial subset of the bits Even %8 but offset by odd amount*/
    result = dax_tag_handle(ds, &h2, "bool_test[5]", 16);
    temp[0] = 0x00; temp[1] = 0x00; temp[2] = 0x00;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    temp1[0] = 0xFF; temp1[1] = 0xFF; temp1[2] = 0xFF;
    result = dax_atomic_op(ds, h2, temp1, ATOMIC_OP_OR);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    DF("in     = %X %X %X", temp1[0], temp1[1], temp1[2]);
    DF("output = %X %X %X", output[0], output[1], output[2]);
    if(output[0] != 0xE0 || output[1] != 0xFF || output[2] != 0x1F) return -1;

    return 0;
}

static int
_byte_test(dax_state *ds) {
    int result;
    tag_handle h;
    dax_byte temp[4];
    dax_byte output[4];

    result = dax_tag_add(ds, &h, "byte_test", DAX_BYTE, 4, 0);
    if(result) return -1;
    temp[0] = 0xAA; temp[1] = 0x55; temp[2] = 0x0F; temp[3] = 0xF0;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    result = dax_atomic_op(ds, h, NULL, ATOMIC_OP_NOT);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    DF("in = %X %X %X %X", temp[0], temp[1], temp[2], temp[3]);
    DF("out= %X %X %X %X", output[0], output[1], output[2], output[3]);
    if(output[0] != 0x55 || output[1] != 0xAA || output[2] != 0xF0 || output[3] != 0x0F) {
        //DF("WHYYYYYYY");
        return -1;
    }
    return 0;
}

static int
_sint_test(dax_state *ds) {
    int result;
    tag_handle h;
    dax_sint temp[4];
    dax_sint output[4];

    result = dax_tag_add(ds, &h, "sint_test", DAX_SINT, 4, 0);
    if(result) return -1;
    temp[0] = 0xAA; temp[1] = 0x55; temp[2] = 0x0F; temp[3] = 0xF0;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    result = dax_atomic_op(ds, h, NULL, ATOMIC_OP_NOT);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    for(int n=0;n<4;n++) {
        if(output[n] != ~temp[n]) {
            DF("WHYYYYYYY");
            return -1;
        }
    }
    return 0;
}

static int
_dint_test(dax_state *ds) {
    int result;
    tag_handle h;
    dax_dint temp[4];
    dax_dint output[4];

    result = dax_tag_add(ds, &h, "dint_test", DAX_DINT, 4, 0);
    if(result) return -1;
    temp[0] = 1234; temp[1] = -3453; temp[2] = -1; temp[3] = 0;
    result = dax_write_tag(ds, h, &temp);
    if(result) return -1;
    result = dax_atomic_op(ds, h, NULL, ATOMIC_OP_NOT);
    if(result) return -1;
    result = dax_read_tag(ds, h, &output);
    if(result) return -1;
    for(int n=0;n<4;n++) {
        if(output[n] != ~temp[n]) {
            DF("WHYYYYYYY");
            return -1;
        }
    }
    return 0;
}

/* Since complementing a floating point value doesn't make any sense we should
 * get illegal operation errors back from the server */
static int
_real_test(dax_state *ds) {
    int result;
    tag_handle h;
    dax_real temp1[4];
    dax_lreal temp2[4];

    result = dax_tag_add(ds, &h, "real_test", DAX_REAL, 4, 0);
    if(result) return -1;
    temp1[0] = 3.141592; temp1[1] = -43234.23455; temp1[2] = -1; temp1[3] = 0;
    result = dax_write_tag(ds, h, &temp1);
    if(result) return -1;
    result = dax_atomic_op(ds, h, NULL, ATOMIC_OP_NOT);
    if(result != ERR_ILLEGAL) return -1;

    result = dax_tag_add(ds, &h, "lreal_test", DAX_LREAL, 4, 0);
    if(result) return -1;
    temp2[0] = 3.141592; temp2[1] = -43234.23455; temp2[2] = -1; temp2[3] = 0;
    result = dax_write_tag(ds, h, &temp2);
    if(result) return -1;
    result = dax_atomic_op(ds, h, NULL, ATOMIC_OP_NOT);
    if(result != ERR_ILLEGAL) return -1;

    return 0;
}


int
do_test(int argc, char *argv[])
{
    dax_state *ds;
    int result = 0;


    ds = dax_init("test");
    dax_init_config(ds, "test");

    dax_configure(ds, argc, argv, CFG_CMDLINE);
    result = dax_connect(ds);
    if(result) {
        return -1;
    }
    if(_bool_test(ds)) return -1;
    //if(_byte_test(ds)) return -1;
    //if(_sint_test(ds)) return -1;
    //if(_dint_test(ds)) return -1;
    //if(_real_test(ds)) return -1;
    dax_disconnect(ds);

    return 0;
}

/* main inits and then calls run */
int
main(int argc, char *argv[])
{
    if(run_test(do_test, argc, argv, 0)) {
        exit(-1);
    } else {
        exit(0);
    }
}
