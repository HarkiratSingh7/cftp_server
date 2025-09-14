#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <event2/bufferevent.h>

#include "connection.h"

#define MAX_ARGS 256
#define MAX_ARG_LEN 4096
#define MAX_COMMAND_LENGTH 4096

typedef struct
{
    char command[MAX_ARG_LEN];
    char *args[MAX_ARGS + 1];
    int argc;
} cftp_command_t;

typedef void (*command_execution_cb)(cftp_command_t *command,
                                     connection_t *connection);

typedef struct
{
    connection_t *connection;
    command_execution_cb authenticated_cb; /* Callback to executed if user is
                                              successfully authenticated*/
    command_execution_cb non_authenticated_cb; /* Callback to executed if user
                                                  is not authenticated */
} command_cb;

void execute_ftp_command(const char *input, connection_t *connection);
void execute_root_command(const char *input, struct bufferevent *bev);
void destroy_command(cftp_command_t *command);
void initialize_execution_engine(connection_t *connection);

#endif
