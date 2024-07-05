/* modbus.h - Modbus (tm) Communications Library
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
 * This is the public header file for the modbus library functions
 */

#ifndef __MODBUS_H
#define __MODBUS_H

#include <arpa/inet.h>
#include <modbus/modbus.h>
#include <stdint.h>

/* calculates the number of bytes needed for x number of bits */
#define BYTE_COUNT(x) ((x)%8==0 ? (x)/8 : (x)/8+1)

/* Parity */
#define MB_NONE 0
#define MB_EVEN 1
#define MB_ODD  2

/* Library Errors */
// #define MB_ERR_GENERIC    -1
// #define MB_ERR_ALLOC      -2      /* Unable to Allocate Storage */
// #define MB_ERR_BAUDRATE   -3      /* Bad Baudrate */
// #define MB_ERR_DATABITS   -4      /* Wrong number of databits */
// #define MB_ERR_STOPBITS   -5      /* Wrong number of stopbits */
// #define MB_ERR_SOCKET     -6      /* Bad socket identifier */
// #define MB_ERR_PORTTYPE   -7      /* Bad port type - Master/Slave */
// #define MB_ERR_PROTOCOL   -8      /* Bad protocol - RTU/ASCII/TCP */
// #define MB_ERR_FUNCTION   -9      /* Bad Function Code Given */
// #define MB_ERR_OPEN       -10     /* Failure to Open Port */
// #define MB_ERR_PORTFAIL   -11     /* Major Port Failure */
// #define MB_ERR_RECV_FAIL  -12     /* Recieve Failure */
// #define MB_ERR_NO_SOCKET  -13     /* The socket does not exist */
// #define MB_ERR_BAD_ARG    -14     /* Bad Argument */
// #define MB_ERR_STOPPED    -15     /* Port was stopped externally */
// #define MB_ERR_OVERFLOW   -16     /* Buffer Overflow */

/* Modbus Errors */
#define ME_EXCEPTION      0x80
#define ME_WRONG_FUNCTION 1
#define ME_BAD_ADDRESS    2
#define ME_WRONG_DEVICE   3
#define ME_CHECKSUM       4
#define ME_TIMEOUT        8

/* Command Methods */
#define MB_CONTINUOUS  0x01   /* Command is sent periodically */
#define MB_CONDITIONAL 0X02   /* Command is sent only if it is different */
#define	MB_ONCHANGE    0x04   /* Command is sent when the data tag changes */
#define MB_ONWRITE     0x08   /* Command is sent when written */
#define MB_TRIGGER     0x10   /* Command is sent when trigger tag is set */

/* Port Attribute Flags */
// #define MB_FLAGS_STOP_LOOP    0x01

/* Register Identifications */
#define MB_REG_HOLDING 1
#define MB_REG_INPUT   2
#define MB_REG_COIL    3
#define MB_REG_DISC    4


/* Number of slave nodes allocated */
 #define MB_MAX_SLAVE_NODES 248
/* Maximum size of the receive buffer */
// #define MB_BUFF_SIZE 256
/* Starting number of connections in the pool */
// #define MB_INIT_CONNECTION_SIZE 16
/* Maximum number of connections that can be in the pool */
// #define MB_MAX_CONNECTION_SIZE 2048


#define DEFAULT_SERVERS 4
#define DEFAULT_SLAVES 4
#define DEFAULT_CLIENTS 4
#define DEFAULT_MASTERS 4

#define HOLD_REG 0
#define INPUT_REG 1
#define COIL_REG 2
#define DISC_REG 3

#define MB_SLAVE 0
#define MB_SERVER 1
#define MB_MASTER 2
#define MB_CLIENT 3

/* This is used as user data to identify slaves / servers / clients and masters in the
   Lua configuration script.  */
typedef struct {
    int type;
    int index;
} mb_type_t;

typedef struct mb_cmd {
    unsigned char enable;    /* 0=disable, 1=enable */
    unsigned char mode;      /* MB_CONTINUOUS, MB_ONCHANGE, MB_ONWRITE, MB_TRIGGER */

    char *host;
    char *protocol;

    uint8_t node;            /* Modbus device ID */
    uint8_t function;        /* Function Code */
    uint16_t m_register;     /* Modbus Register */
    uint16_t length;         /* length of modbus data */

    unsigned int interval;   /* number of port scans between messages */
    uint8_t *data;           /* pointer to the actual modbus data that this command refers */
    int datasize;            /* size of the *data memory area */
    unsigned int icount;     /* number of intervals passed */
    unsigned int requests;   /* total number of times this command has been sent */
    unsigned int responses;  /* number of valid modbus responses (exceptions included) */
    unsigned int timeouts;   /* number of times this command has timed out */
    unsigned int crcerrors;  /* number of checksum errors */
    unsigned int exceptions; /* number of modbus exceptions recieved from slave */
    uint8_t lasterror;       /* last error on command */
    uint16_t lastcrc;        /* used to determine if a conditional message should be sent */
    unsigned char firstrun;  /* Indicates that this command has been sent once */

    char *trigger_tag;       /* Tagname for tag that will be used to trigger this command must be BOOL */
    char *data_tag;          /* Tagname for the tag that will represent the data for this command. */
    uint32_t tagcount;       /* Number of tag items to read/write */
    tag_handle data_h;       /* Handle to data tag */

    struct mb_cmd* next;
} mb_cmd_t;

/* This holds all of the information to define a register set for a single unit id */
typedef struct {
    char *hold_name;       /* holding register tag name */
    uint16_t hold_size;    /* size of the holding register table */
    uint16_t hold_start;   /* starting address for the holding registers */
    tag_index hold_idx;    /* tag index */
    char *input_name;
    uint16_t input_size;   /* size of the input registers table */
    uint16_t input_start;  /* starting address for the input registers */
    tag_index input_idx;
    char *coil_name;
    uint16_t coil_size;    /* size of the coils */
    uint16_t coil_start;   /* starting address for the coils */
    tag_index coil_idx;
    uint8_t *coil_buffer;
    char *disc_name;
    uint16_t disc_size;    /* size of the discretes */
    uint16_t disc_start;   /* starting address for the discretes */
    tag_index disc_idx;
    uint8_t *disc_buffer;
    int read_callback;
    int write_callback;
    modbus_mapping_t *mb_mapping;
} mb_node_t;


typedef struct mb_server {
    char *name;               /* Port name if needed : Maybe we don't need this */
    uint8_t enable;     /* 0=Pause, 1=Run */
    tag_handle h;
    char *host;
    char *protocol;
    pthread_t thread;
    pthread_mutex_t lock;

    mb_node_t *nodes[MB_MAX_SLAVE_NODES];      /* Individual node units */
} mb_server_t;

typedef struct mb_slave {
    char *name;               /* Port name if needed : Maybe we don't need this */
    uint8_t enable;     /* 0=Pause, 1=Run */
    tag_handle h;

    char *device;             /* device filename of the serial port */
    int baudrate;
    short databits;
    short stopbits;
    short parity;             /* 0=NONE, 1=EVEN OR 2=ODD */

    pthread_t thread;
    mb_node_t *nodes[MB_MAX_SLAVE_NODES];      /* Individual node units */
} mb_slave_t;

typedef struct mb_client {
    char *name;               /* Port name if needed : Maybe we don't need this */
    uint8_t enable;     /* 0=Pause, 1=Run */
    uint8_t persist;

    pthread_t thread;
    mb_cmd_t *cmds;
} mb_client_t;

typedef struct mb_master {
    char *name;               /* Port name if needed : Maybe we don't need this */
    uint8_t enable;     /* 0=Pause, 1=Run */
    uint8_t persist;

    char *device;             /* device filename of the serial port */
    int baudrate;
    short databits;
    short stopbits;
    short parity;             /* 0=NONE, 1=EVEN OR 2=ODD */

    pthread_t thread;
    mb_cmd_t *cmds;
} mb_master_t;


typedef struct config {
    int servercount;      /* Number of ports that are assigned */
    int serversize;       /* Number of ports that are allocated */
    mb_server_t *servers;
    int slavecount;      /* Number of ports that are assigned */
    int slavesize;       /* Number of ports that are allocated */
    mb_slave_t *slaves;
    int clientcount;      /* Number of ports that are assigned */
    int clientsize;       /* Number of ports that are allocated */
    mb_client_t *clients;
    int mastercount;      /* Number of ports that are assigned */
    int mastersize;       /* Number of ports that are allocated */
    mb_master_t *masters;
} config_t;


/* Internal struct that defines a single Modbus(tm) Port */
// typedef struct mb_port {
//     char *name;               /* Port name if needed : Maybe we don't need this */
//     unsigned int flags;       /* Port Attribute Flags */
//     char *device;             /* device filename of the serial port */
//     unsigned char enable;     /* 0=Pause, 1=Run */
//     unsigned char type;       /* 0=Master, 1=Slave */
//     unsigned char devtype;    /* 0=serial, 1=network */
//     unsigned char protocol;   /* MB_RTU, MB_ASCII or MB_TCP*/

//     int baudrate;
//     short databits;
//     short stopbits;
//     short parity;             /* 0=NONE, 1=EVEN OR 2=ODD */
//     char ipaddress[16];
//     unsigned int bindport;    /* IP port to bind to */
//     unsigned char socket;     /* either UDP_SOCK or TCP_SOCK */

//     int delay;       /* Intercommand delay */
//     int frame;       /* Interbyte timeout */
//     int retries;     /* Number of retries to try */
//     int scanrate;    /* Scanrate in mSeconds */
//     int timeout;     /* Response timeout */
//     int maxattempts; /* Number of failed attempts to allow before closing and exiting the port */

//     mb_node_def **nodes; /* Individual node units */

//     fd_set fdset;
//     int maxfd;
//     client_buffer *buff_head; /* Head of a linked list of client connection buffers */

//     struct mb_cmd *commands;  /* Linked list of Modbus commands */
//     int fd;                   /* File descriptor to the port */
//     int ctrl_flags;
//     int dienow;
//     unsigned int attempt;        /* Attempt counter */
//     unsigned char running;       /* Flag to indicate the port is running */
//     unsigned char inhibit;       /* When set the port will not be started */
//     unsigned int inhibit_time;   /* Number of seconds before the port will be retried */
//     unsigned int inhibit_temp;

//     tcp_connection *connections;  /* Network connection pool */
//     int connection_size;
//     int connection_count;
//     uint8_t persist;              /* If true the port(s) stay open */
//     uint8_t scanning;             /* A flag to tell us if we are currently scanning the port */

//     pthread_mutex_t send_lock;
//     tag_handle command_h;         /* Handle to command tag */
//     mb_cmd *cmd;                  /* Pointer to the asynchronous command structure */

//     /* These are callback function pointers for the port message data */
//     void (*out_callback)(struct mb_port *port, uint8_t *buff, unsigned int);
//     void (*in_callback)(struct mb_port *port, uint8_t *buff, unsigned int);
// } mb_port;


// typedef struct event_ud {
//     mb_port *port;
//     mb_cmd *cmd;
//     tag_handle h;
// } event_ud;

// typedef struct enable_ud {
//     mb_port *port;
//     tag_handle h;
// } enable_ud;

// /* Create a New Modbus Port with the given name */
// mb_port *mb_new_port(const char *name, unsigned int flags);
// /* Frees the memory allocated with mb_new_port() */
// void mb_destroy_port(mb_port *port);
// /* Port Configuration Functions */
// int mb_set_serial_port(mb_port *port, const char *device, int baudrate, short databits, short parity, short stopbits);
// int mb_set_network_port(mb_port *port, const char *ipaddress, unsigned int bindport, unsigned char socket);
// int mb_set_protocol(mb_port *port, unsigned char type, unsigned char protocol);
// int mb_set_scan_rate(mb_port *port, int rate);
// int mb_set_maxfailures(mb_port *port, int maxfailures, int inhibit);

// int mb_open_port(mb_port *port);
// int mb_close_port(mb_port *port);
// int mb_get_connection(mb_port *mp, struct in_addr address, uint16_t port);
// /* Set callback functions that are called any time data is read or written over the port */
// void mb_set_msgout_callback(mb_port *, void (*outfunc)(mb_port *,uint8_t *,unsigned int));
// void mb_set_msgin_callback(mb_port *, void (*infunc)(mb_port *,uint8_t *,unsigned int));

// /* Create a new command and add it to the port */
// mb_cmd *mb_new_cmd(mb_port *port);
// /* Free the memory allocated with mb_new_cmd() */
// void mb_destroy_cmd(mb_cmd *cmd);
// int mb_set_command(mb_cmd *cmd, uint8_t node, uint8_t function, uint16_t reg, uint16_t length);
// int mb_set_interval(mb_cmd *cmd, int interval);
// void mb_set_mode(mb_cmd *cmd, unsigned char mode);

// int mb_is_write_cmd(mb_cmd *cmd);
// int mb_is_read_cmd(mb_cmd *cmd);

// /* End New Interface */
// int mb_run_port(mb_port *);
// int mb_send_command(mb_port *, mb_cmd *);

// void mb_print_portconfig(FILE *fd, mb_port *mp);

// /* Port Functions - defined in modports.c */
// int add_cmd(mb_port *p, mb_cmd *mc);

// /* Serial Slave loop function */
// int slave_loop(mb_port *port);

// /* Protocol Functions - defined in modbus.c */
// int create_response(mb_port * port, unsigned char *buff, int size);

/* Configuration Functions - modopt.c*/
int modbus_configure(int, const char **);

/* Lua interface functions - mbluaif.c */
int run_lua_callback(int fidx, uint8_t node, uint8_t function, uint16_t index, uint16_t count);

/* Database functions - database.c */
void get_bytes_from_bits(uint8_t *src, uint16_t count, uint8_t *dest);
void slave_write_database(tag_index idx, int reg, int offset, int count, void *data);
void slave_read_database(tag_index idx, int reg, int offset, int count, void *data);


int start_servers(void);

#endif
