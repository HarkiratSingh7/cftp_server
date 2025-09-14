#ifndef DATA_HANDLER_H
#define DATA_HANDLER_H

#include <event2/bufferevent_ssl.h>

#include "connection.h"

/*!
 * @brief Accept callback for data connection.
 * @param listener Listener object.
 * @param fd Accepted socket for data connection.
 * @param addr sa object :/
 * @param unused unused
 */
void data_connection_accept_cb(struct evconnlistener *listener,
                               evutil_socket_t fd,
                               struct sockaddr *addr,
                               int unused,
                               void *ctx);

/*!
 * @brief Sets up a evconnlistener intended for data connections.
 * @param connection The connection structure
 * @param extended Whether to respond to EPSV or PASV commands.
 */
void data_connection_listener_config(connection_t *connection, int extended);

void close_data_connection(connection_t *connection);

#endif
