/*  OpenDAX - An open source data acquisition and control system
 *  Copyright (c) 2007 Phil Birkelbach
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *

 * This file contains some of the misc functions for the library
 */

#include <libdax.h>
#include <common.h>
#include <stdarg.h>
#include <signal.h>
#include <time.h>


/* The following two functions are utility functions for converting dax
 * values to strings and strings to dax values.
 */

/*!
 * Convert an OpenDAX value to a readable string.  Only a single value will be
 * converted even though val can be an entire array.
 *
 * @param buff Pointer to the string buffer that will be filled in by this function.
 * @param size Size of the buffer.  This is passed to prevent overflow.
 * @param type The data type of the value in buff
 * @param val Pointer to the actual value
 * @param index The index into val.  This is essentially an array index.
 * @returns Zero on success or an error code if appropriate
 */
int
dax_val_to_string(char *buff, int size, tag_type type, void *val, int index)
{
    int ms;
    long int sec;
    struct tm *tm;
    char tstr[16];

    if(val==NULL) return ERR_EMPTY;
    switch (type) {
     /* Each number has to be cast to the right datatype then dereferenced */
        case DAX_BOOL:
            if((0x01 << (index % 8)) & ((uint8_t *)val)[index / 8]) {
                snprintf(buff, size, "1");
            } else {
                snprintf(buff, size, "0");
            }
            break;
        case DAX_BYTE:
            snprintf(buff, size, "%u", ((dax_byte *)val)[index]);
            break;
        case DAX_SINT:
            snprintf(buff, size, "%hhd", ((dax_sint *)val)[index]);
            break;
        case DAX_CHAR:
            snprintf(buff, size, "%c", ((dax_char *)val)[index]);
            break;
        case DAX_WORD:
        case DAX_UINT:
            snprintf(buff, size, "%d", ((dax_uint *)val)[index]);
            break;
        case DAX_INT:
            snprintf(buff, size, "%hd", ((dax_int *)val)[index]);
            break;
        case DAX_DWORD:
        case DAX_UDINT:
            snprintf(buff, size, "%u", ((dax_udint *)val)[index]);
            break;
        case DAX_DINT:
            snprintf(buff, size, "%d", ((dax_dint *)val)[index]);
            break;
        case DAX_REAL:
            snprintf(buff, size, "%.8g", ((dax_real *)val)[index]);
            break;
        case DAX_LWORD:
        case DAX_ULINT:
            snprintf(buff, size, "%lu", ((dax_ulint *)val)[index]);
            break;
        case DAX_TIME:
            ms = ((dax_lint *)val)[index] % 1000;
            sprintf(tstr, ".%03d", ms);
            sec = ((dax_lint *)val)[index] / 1000;
            tm = gmtime(&sec);
            strftime(buff, size, "%FT%T", tm);
            strncat(buff, tstr, size);
            break;
        case DAX_LINT:
            snprintf(buff, size, "%ld", ((dax_lint *)val)[index]);
            break;
        case DAX_LREAL:
            snprintf(buff, size, "%.16g", ((dax_lreal *)val)[index]);
            break;
    }
    // TODO: return errors if needed
    return 0;
}

/*!
 * Convert the given string into an OpenDAX value.
 *
 * @param instr Pointer to the string that we wish to convert
 * @param type The data type of the value that we want returned
 * @param buff Pointer to the location where we want this function
 *             to put the value.  The caller should take care to make
 *             sure that this buffer is large enough to store the value.
 * @param mask If the type is a BOOL we can have this function set
 *              this to be a mask suitable for use in writing the
 *              correct bit based on the index.  If set to NULL it
 *              will be ignored.
 * @param index The index into buff where the value will be written.
 *               buff is first cast into the appropriate type and then
 *               indexed with this value to find the location that the
 *               converted value will be written.
 */
int
dax_string_to_val(char *instr, tag_type type, void *buff, void *mask, int index)
{
    long temp;
    long long ltemp;
    int retval = 0;
    int result, ms, year;
    struct tm tm = {0,0,0,0,0,0,0,0,0};

    switch (type) {
        case DAX_BOOL:
            temp = strtol(instr, NULL, 0);
            if(temp == 0) {
                ((uint8_t *)buff)[index / 8] &= ~(0x01 << (index % 8));
            } else {
                ((uint8_t *)buff)[index / 8] |= (0x01 << (index % 8));
            }
            if(mask) {
                ((uint8_t *)mask)[index / 8] |= (0x01 << (index % 8));
            }
            break;
        case DAX_BYTE:
            temp =  strtol(instr, NULL, 0);
            if(temp < DAX_BYTE_MIN) {
                ((dax_byte *)buff)[index] = DAX_BYTE_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(temp > DAX_BYTE_MAX) {
                ((dax_byte *)buff)[index] = DAX_BYTE_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_byte *)buff)[index] = temp;
            if(mask) ((dax_byte *)mask)[index] = 0xFF;
            break;
        case DAX_CHAR:
            if(instr[1] == 0) { /* One character */
                ((dax_char *)buff)[index] = instr[0];
                if(mask) ((dax_sint *)mask)[index] = 0xFF;
                break;
            } /* Else we fall through and treat it exactly like a SINT */
        case DAX_SINT:
            temp =  strtol(instr, NULL, 0);
            if(temp < DAX_SINT_MIN) {
                ((dax_sint *)buff)[index] = DAX_SINT_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(temp > DAX_SINT_MAX) {
                ((dax_sint *)buff)[index] = DAX_SINT_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_sint *)buff)[index] = temp;
            if(mask) ((dax_sint *)mask)[index] = 0xFF;
            break;
        case DAX_WORD:
        case DAX_UINT:
            temp =  strtoul(instr, NULL, 0);
            if(temp < DAX_UINT_MIN) {
                ((dax_uint *)buff)[index] = DAX_UINT_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(temp > DAX_UINT_MAX) {
                ((dax_uint *)buff)[index] = DAX_UINT_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_uint *)buff)[index] = temp;
            if(mask) ((dax_uint *)mask)[index] = 0xFFFF;
            break;
        case DAX_INT:
            temp =  strtol(instr, NULL, 0);
            if(temp < DAX_INT_MIN) {
                ((dax_int *)buff)[index] = DAX_INT_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(temp > DAX_INT_MAX) {
                ((dax_int *)buff)[index] = DAX_INT_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_int *)buff)[index] = temp;
            if(mask) ((dax_int *)mask)[index] = 0xFFFF;
            break;
        case DAX_DWORD:
        case DAX_UDINT:
            ltemp =  strtol(instr, NULL, 0);
            if(ltemp < DAX_UDINT_MIN) {
                ((dax_udint *)buff)[index] = DAX_UDINT_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(ltemp > DAX_UDINT_MAX) {
                ((dax_udint *)buff)[index] = DAX_UDINT_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_udint *)buff)[index] = ltemp;
            if(mask) ((dax_udint *)mask)[index] = 0xFFFFFFFF;
            break;
        case DAX_DINT:
            temp =  strtol(instr, NULL, 0);
            if(temp < DAX_DINT_MIN) {
                ((dax_dint *)buff)[index] = DAX_DINT_MIN;
                retval = ERR_UNDERFLOW;
            }
            else if(temp > DAX_DINT_MAX) {
                ((dax_dint *)buff)[index] = DAX_DINT_MAX;
                retval = ERR_OVERFLOW;
            }
            else
                ((dax_dint *)buff)[index] = temp;
            if(mask) ((dax_dint *)mask)[index] = 0xFFFFFFFF;
            break;
        case DAX_REAL:
            ((dax_real *)buff)[index] = strtof(instr, NULL);
            if(mask) ((uint32_t *)mask)[index] = 0xFFFFFFFF;
            break;
        case DAX_LWORD:
        case DAX_ULINT:
            if(instr[0]=='-') {
                ((dax_ulint *)buff)[index] = 0x0000000000000000;
                retval = ERR_UNDERFLOW;
                break;
            }
            errno = 0;
            ((dax_ulint *)buff)[index] = (dax_ulint)strtoull(instr, NULL, 0);
            if(mask) ((dax_ulint *)mask)[index] = DAX_64_ONES;
            if(errno == ERANGE) {
                retval = ERR_OVERFLOW;
            }
            break;
        case DAX_LINT:
            errno = 0;
            ((dax_lint *)buff)[index] = (dax_lint)strtoll(instr, NULL, 0);
            if(mask) ((dax_lint *)mask)[index] = DAX_64_ONES;
            if(errno == ERANGE) {
                if(instr[0]=='-') {
                    retval = ERR_UNDERFLOW;
                } else {
                    retval = ERR_OVERFLOW;
                }
            }
            break;
        case DAX_TIME:
            /* We use sscanf because strptime doesn't do the milliseconds */
            result = sscanf(instr, "%d-%d-%dT%d:%d:%d.%d", &year, &tm.tm_mon, &tm.tm_mday,
                                                           &tm.tm_hour, &tm.tm_min, &tm.tm_sec, &ms);
            if(result < 6) { /* It didnt' work */
                errno = 0;
                ((dax_lint *)buff)[index] = (dax_lint)strtoll(instr, NULL, 0);
                if(mask) ((dax_lint *)mask)[index] = DAX_64_ONES;
                if(errno == ERANGE) {
                    if(instr[0]=='-') {
                        retval = ERR_UNDERFLOW;
                    } else {
                        retval = ERR_OVERFLOW;
                    }
                }
            } else {
                if(result == 6) ms = 0;
                tm.tm_year = year - 1900; /* Year is since 1900 in struct tm */
                tm.tm_mon--; /* Month is 0-30 in struct tm */
                ((dax_lint *)buff)[index] = ((dax_lint)timegm(&tm) * 1000) + ms;
                if(mask) ((dax_lint *)mask)[index] = DAX_64_ONES;
            }
            break;
        case DAX_LREAL:
            ((dax_lreal *)buff)[index] = strtod(instr, NULL);
            if(mask) ((uint64_t *)mask)[index] = DAX_64_ONES;
            break;
    }
    return retval;
}


/*!
 * Set the callback function that will be called when the server disconnects
 *
 * @param ds   Pointer to DAX State object
 * @param f    The function to assign to the callback
 */

void
dax_set_disconnect_callback(dax_state *ds, void (*f)(int result)) {
    ds->disconnect_callback = f;
}