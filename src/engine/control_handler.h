#ifndef CONTROL_HANDLER_H
#define CONTROL_HANDLER_H

#include <event2/bufferevent_ssl.h>
#include <openssl/ssl.h>

#include "connection.h"

#define MAX_RESPONSE_BUFFER_LENGTH 0x100

/*!
 * @brief This method handles control connection and is called from accept
 * callback.
 * @param fd The associated fd with socket where connection is accepted.
 * @param connection Expects a connection object here.
 */
void start_control_connection_loop(evutil_socket_t fd,
                                   connection_t *connection,
                                   int pipe);

/*!
 * @brief Control function to upgrade to TLS channel on demand.
 * @param connection The connection object which has to be upgraded.
 */
void upgrade_to_tls(connection_t *connection);

/*!
 * @brief Write a text response to connection.
 * @param bev Bufferevent object.
 * @param text Text message.
 * @deprecated This method is deprecated. Instead use send_control_message.
 */
void write_text_response(struct bufferevent *bev, const char *text);

/*!
 * @brief Sends a control message to client via control channel.
 * @param bev Bufferevent object.
 * @param status_code FTP response status code.
 * @param text The actual text message.
 * @details Takes care of appending CRLF to the response message endings.
 */
void send_control_message(connection_t *connection,
                          uint32_t status_code,
                          const char *text);
void terminate_process_on_timeout(evutil_socket_t fd, short what, void *arg);
#endif
