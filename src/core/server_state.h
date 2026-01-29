/*
    This file contains the structures for maintaining server states.
*/

#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <event2/event.h>
#include <stdint.h>

#include "../config_manager/config_manager.h"
#include "connection.h"

typedef struct
{
    uint16_t start;
    uint16_t end;
    int n; /* inclusive count */
} pasv_port_range_t;

typedef struct
{
    configurations_t config; /* Server configurations */
    pasv_port_range_t pasv_range;
    uint32_t current_connections; /* Current number of active connections */
    char server_version[64];      /* Version of the server */
    int is_running;          /* Flag to indicate if the server is running */
    SSL_CTX *ssl_ctx;        /* SSL context for secure connections */
    struct event_base *base; /* Event base for managing events */
} server_state_t;

void init_server_state(void);
void destroy_server_state(void);
void connections_init_pasv_range(int start, int end);

#endif
