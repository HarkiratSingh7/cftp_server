#ifndef COMMAND_PARSER_H
#define COMMAND_PARSER_H

#include <event2/bufferevent.h>

#include "connection.h"

typedef void (*command_execution_cb)(const char *input,
                                     const char *params,
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

void initialize_execution_engine(connection_t *connection);

#endif
