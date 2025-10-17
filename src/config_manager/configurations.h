#ifndef CONFIGURATIONS_H
#define CONFIGURATIONS_H

#include <limits.h>
#include <stdint.h>

typedef struct
{
    uint32_t max_connections;      /* Maximum number of connections allowed */
    int connection_accept_timeout; /* Timeout duration in seconds */
    int data_connection_accept_timeout; /* Timeout duration in seconds */
    int port;                     /* Port number for the server to listen on */
    int passive_port_start;       /* Start of the passive port range */
    int passive_port_end;         /* End of the passive port range */
    char server_name[256];        /* Name of the server */
    char ssl_cert_file[PATH_MAX]; /* Path to the SSL certificate file */
    char ssl_key_file[PATH_MAX];  /* Path to the SSL key file */
} configurations_t;

#endif /* CONFIGURATIONS_H */
