/*
    This file contains the structures for maintaining server states.
*/

#ifndef SERVER_STATE_H
#define SERVER_STATE_H

#include <event2/event.h>

#include "connection.h"

typedef struct
{
    int max_connections;     /* Maximum number of connections allowed */
    int current_connections; /* Current number of active connections */
    char server_name[256];   /* Name of the server */
    char server_version[64]; /* Version of the server */
    int is_running;          /* Flag to indicate if the server is running */
    int port;                /* Port on which the server is listening */
    SSL_CTX *ssl_ctx;        /* SSL context for secure connections */
    struct event_base *base; /* Event base for managing events */
} server_state_t;

void init_server_state(int max_connections,
                       const char *server_name,
                       const char *server_version,
                       int port);

#endif
