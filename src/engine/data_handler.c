#include "data_handler.h"

#include <arpa/inet.h>
#include <event2/buffer.h>
#include <openssl/err.h>
#include <unistd.h>

#include "control_handler.h"
#include "error.h"
#include "ftp_status_codes.h"

/*!
 * @brief Write callback for data connection
 */
static void data_connection_write_cb(struct bufferevent *bev, void *ctx);
static void data_connection_read_cb(struct bufferevent *bev, void *ctx);
static void data_connection_event_cb(struct bufferevent *bev,
                                     short events,
                                     void *ctx);

void data_connection_accept_cb(struct evconnlistener *listener,
                               evutil_socket_t fd,
                               struct sockaddr *addr,
                               int unused,
                               void *ctx)
{
    /* First of all the source incoming IP must be same ! */
    connection_t *connection = (connection_t *)ctx;
    char ip_str[INET6_ADDRSTRLEN];
    fill_source_ip(addr, ip_str);

    if (strncmp(connection->source_ip, ip_str, sizeof(INET6_ADDRSTRLEN)) != 0)
    {
        ERROR("This is not allowed here. Come from same machine !");
        close(fd);
        return;
    }

    /* Listener must be closed, we only allow 1 data connection */
    INFO("Got a data connection from %s", ip_str);
    evconnlistener_free(listener);

    connection->data_active = 0; /* Set later */
    connection->pasv_listener = NULL;

    struct event_base *base = connection->base;
    connection->data_fd = fd;

    /*  Create a new bufferevent for the data connection */
    if (!connection->data_tls_required)
    {
        INFO("Got plaintext data !");
        connection->data_bev = bufferevent_socket_new(
            base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    }
    else
    {
        INFO("Got encrypted data !");
        connection->data_ssl = SSL_new(connection->ssl_ctx);
        connection->data_bev = bufferevent_openssl_socket_new(
            base,
            fd,
            connection->data_ssl,
            BUFFEREVENT_SSL_ACCEPTING,
            BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);
    }

    if (!connection->data_bev)
    {
        ERROR("Failed to create bufferevent for data connection");
        return;
    }

    /*  Set callbacks for the passive data connection */
    bufferevent_setcb(connection->data_bev,
                      data_connection_read_cb,
                      data_connection_write_cb,
                      data_connection_event_cb,
                      connection);
    bufferevent_enable(connection->data_bev, EV_READ | EV_WRITE);

    if (!connection->data_tls_required)
        __sync_add_and_fetch_8(&connection->data_active, 1);

    INFO("Data connection established on fd %d", fd);
}

void data_connection_listener_config(connection_t *connection, int extended)
{
    if (!connection)
    {
        ERROR("Invalid connection object passed !");
        return;
    }

    INFO("Inside data connection listener config");

    if (connection->pasv_listener)
    {
        evconnlistener_free(connection->pasv_listener);
        connection->pasv_listener = NULL;
    }

    struct sockaddr_in ctrl_addr = {0};
    socklen_t len = sizeof(ctrl_addr);
    // memset(&ctrl_addr, 0, sizeof(ctrl_addr));

    if (getsockname(connection->fd, (struct sockaddr *)&ctrl_addr, &len) == 0)
    {
        int pasv_port = get_random_unused_port();

        struct sockaddr_in pasv_addr = {0};
        len = sizeof(pasv_addr);

        pasv_addr.sin_family = AF_INET;
        pasv_addr.sin_addr.s_addr = ctrl_addr.sin_addr.s_addr;
        pasv_addr.sin_port = htons(pasv_port);

        connection->pasv_listener =
            evconnlistener_new_bind(connection->base,
                                    data_connection_accept_cb,
                                    connection,
                                    LEV_OPT_CLOSE_ON_FREE | LEV_OPT_REUSEABLE,
                                    1,
                                    (struct sockaddr *)&pasv_addr,
                                    len);

        if (!connection->pasv_listener)
        {
            send_control_message(connection,
                                 FTP_STATUS_CANNOT_OPEN_DATA,
                                 "Can't open passive connection");
            return;
        }

        char response[256];

        if (extended)
        {
            snprintf(response,
                     sizeof(response),
                     "Entering Extended Passive Mode (|||%" PRIu32 "|)",
                     pasv_port);
            send_control_message(
                connection, FTP_STATUS_ENTERING_EPSV_MODE, response);
        }
        else
        {
            uint32_t ip_net_order = ctrl_addr.sin_addr.s_addr;
            uint32_t ip_host_order = ntohl(ip_net_order);
            unsigned char ip_bytes[4];
            ip_bytes[0] = (ip_host_order >> 24) & 0xFF;
            ip_bytes[1] = (ip_host_order >> 16) & 0xFF;
            ip_bytes[2] = (ip_host_order >> 8) & 0xFF;
            ip_bytes[3] = ip_host_order & 0xFF;

            snprintf(response,
                     sizeof(response),
                     "Entering Passive Mode. %d,%d,%d,%d,%d,%d",
                     ip_bytes[0],
                     ip_bytes[1],
                     ip_bytes[2],
                     ip_bytes[3],
                     pasv_port / 256,
                     pasv_port % 256);
            char ip_str[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &ctrl_addr.sin_addr, ip_str, sizeof(ip_str));
            send_control_message(
                connection, FTP_STATUS_ENTERING_PASSIVE_MODE, response);
        }
    }
    else
    {
        ERROR("Unable to open data channel, getsockname failed !");
        send_control_message(connection,
                             FTP_STATUS_CANNOT_OPEN_DATA,
                             "Can't open passive connection");
    }
}

static void data_connection_read_cb(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection)
    {
        ERROR("Got invalid connection during data connection read callback !");
        return;
    }

    if (connection->data_read_cb) connection->data_read_cb(bev, ctx);
}

static void data_connection_write_cb(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection)
    {
        ERROR("Got invalid connection during data connection write callback !");
        return;
    }

    if (connection->data_write_cb) connection->data_write_cb(bev, ctx);
}

static void data_connection_event_cb(struct bufferevent *bev,
                                     short events,
                                     void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection)
    {
        ERROR("Invalid connection object !");
        return;
    }

    /* Multiple events can occur together */
    DEBG("Data event occurred: %" PRId16, events);

    /* Data connection event for TLS based connections */
    if (events & BEV_EVENT_CONNECTED)
    {
        DEBG("BEV_EVENT_CONNECTED for data channel for %s",
             connection->username);
        __sync_add_and_fetch_8(&connection->data_active, 1);
        /* Enable the read, write callbacks for TLS, for plaintext they are
         * always enabled*/
        if (connection->data_tls_required)
        {
            DEBG("TLS Handshake successful for %s data connection",
                 connection->username);
            bufferevent_enable(
                bev, EV_READ | EV_WRITE); /* Should be disabled earlier */

            if (connection->data_tls_event_connected_cb)
            {
                DEBG("Invoking data_tls_event_connected_cb for %s",
                     connection->username);
                connection->data_tls_event_connected_cb(bev, ctx);
            }
        }
    }

    if (events & BEV_EVENT_TIMEOUT)
        ERROR("Timeout occurred on data connecton (read or write) for %s",
              connection->username);

    if (events & BEV_EVENT_ERROR)
    {
        uint64_t error;

        if (connection->data_tls_required)
        {
            while ((error = bufferevent_get_openssl_error(bev)))
            {
                const char *message = ERR_reason_error_string(error);
                ERROR("OPENSSL ERROR: %s occurred for %s",
                      message,
                      connection->username);
            }
        }

        int sock = bufferevent_getfd(bev);
        socklen_t error_length = sizeof(error);
        if (sock >= 0 &&
            getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &error_length) ==
                0 &&
            error != 0)
            ERROR("Socket error on fd %d for %s user: %s",
                  sock,
                  connection->username,
                  strerror(error));
    }

    if (events & BEV_EVENT_EOF)
    {
        DEBG("Client closed the data connection (EOF) for %s",
             connection->username);

        /* Call any EOF callback here once */
        if (connection->data_eof_event_cb)
        {
            DEBG("Invoking data_eof_event_cb for %s", connection->username);
            connection->data_eof_event_cb(bev, ctx);
        }
    }

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR | BEV_EVENT_TIMEOUT))
    {
        DEBG("Destroying data connection for %s", connection->username);

        close_data_connection(connection);
        return;
    }
}

void close_data_connection(connection_t *connection)
{
    if (!connection->data_bev)
    {
        ERROR("close called on already invalid data bev !");

        connection->data_bev = NULL;
        connection->data_tls_event_connected_cb = NULL;
        connection->data_eof_event_cb = NULL;
        connection->data_read_cb = NULL;
        connection->data_write_cb = NULL;
        connection->data_active = 0;
        connection->upload_fd = -1;
        return;
    }

    bufferevent_setcb(connection->data_bev, NULL, NULL, NULL, NULL);
    bufferevent_disable(connection->data_bev, EV_READ | EV_WRITE);
    bufferevent_free(connection->data_bev);

    connection->data_bev = NULL;
    connection->data_tls_event_connected_cb = NULL;
    connection->data_eof_event_cb = NULL;
    connection->data_read_cb = NULL;
    connection->data_write_cb = NULL;
    connection->data_active = 0;
    connection->upload_fd = -1;

    if (connection->data_stream)
    {
        free(connection->data_stream);
        connection->data_stream = NULL;
    }

    DEBG("Data connection closed for %s", connection->username);
}
