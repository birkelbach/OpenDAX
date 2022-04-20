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
 *  This test verifies that a blank tagname given to dax_tag_handle() will fail
 */

#include <common.h>
#include <opendax.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "libtest_common.h"


int
do_test(int argc, char *argv[])
{
    dax_state *ds;
    int result = 0;
    tag_handle h;
    dax_dint temp, n;

    ds = dax_init("test");
    dax_init_config(ds, "test");

    dax_configure(ds, argc, argv, CFG_CMDLINE);
    result = dax_connect(ds);
    if(result) {
        return -1;
    }
    result = 0;
    result += dax_tag_add(ds, &h, "TEST1", DAX_DINT | DAX_QUEUE, 1, 0);
    result += dax_tag_add(ds, &h, "TEST2", DAX_DINT | DAX_QUEUE, 1, 0);
    result += dax_tag_add(ds, &h, "TEST3", DAX_DINT | DAX_QUEUE, 1, 0);
    result += dax_tag_add(ds, &h, "TEST4", DAX_DINT | DAX_QUEUE, 1, 0);
    if(result) return result;

    result += dax_tag_handle(ds, &h, "TEST1", 0);
    result += dax_tag_handle(ds, &h, "TEST2", 0);
    result += dax_tag_handle(ds, &h, "TEST3", 0);
    result += dax_tag_handle(ds, &h, "TEST4", 0);
    if(result) return result;
    /* This should fail */
    result = dax_tag_handle(ds, &h, "", 0);
    if(result != ERR_NOTFOUND) {
        printf("Looks like we duplicated the error\n");
        return -1;
    }
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
