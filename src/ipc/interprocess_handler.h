#ifndef INTERPROCESS_HANDLER_H
#define INTERPROCESS_HANDLER_H

#include "connection.h"

/*
    * Interprocess communication
    * This is used to communicate with the main process for authentication and
    * other control operations.

    interprocess_fd is the file descriptor for the socket pair
*/
void register_interprocess_fd_on_server(int interprocess_fd);
void register_interprocess_fd_on_child(int interprocess_fd,
                                       connection_t *connection);

void ask_root_for_username(connection_t *connection,
                           uint32_t uid,
                           char *buffer,
                           size_t buffer_size);
void ask_root_for_groupname(connection_t *connection,
                            uint32_t gid,
                            char *buffer,
                            size_t buffer_size);

#endif
