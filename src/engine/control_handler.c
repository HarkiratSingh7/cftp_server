#include "control_handler.h"

#include <event2/buffer.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "command_parser.h"
#include "error.h"
#include "ftp_status_codes.h"

static void on_event(struct bufferevent *bev, short events, void *ctx);
static void on_write(struct bufferevent *bev, void *ctx);

static void on_event(struct bufferevent *bev, short events, void *ctx)
{
    if (events & BEV_EVENT_CONNECTED)
    {
        DEBG("TLS handshake complete");

        /*  Only now send the greeting */

        /*  bufferevent_write(conn->bev, "234 AUTH TLS successful\r\n", 27); */
    }
    else if (events & BEV_EVENT_EOF || events & BEV_EVENT_ERROR)
    {
        ERROR("Closing connection and freeing bufferevent");
        bufferevent_free(bev);
    }
}

static void on_write(struct bufferevent *bev, void *ctx)
{
    if (!bev || !ctx) return;

    connection_t *connection = (connection_t *)ctx;

    if (connection->control_write_cb) connection->control_write_cb(bev, ctx);

    connection->control_write_cb = NULL;
}

void start_control_connection_loop(evutil_socket_t fd, connection_t *connection)
{
    if (!connection)
    {
        ERROR("Connection object is NULL !");
        return;
    }

    connection->base = event_base_new();
    connection->fd = fd;

    struct bufferevent *bev = bufferevent_socket_new(
        connection->base, fd, BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);

    if (!bev)
    {
        ERROR("An error occurred while creating bufferevent object !");
        return;
    }

    connection->bev = bev;

    initialize_execution_engine(connection);

    const char *welcome = "220 Welcome to CFTP Server\r\n";
    bufferevent_write(bev, welcome, strlen(welcome));
    bufferevent_setcb(bev, on_read, on_write, on_event, connection);
    bufferevent_enable(bev, EV_READ | EV_WRITE);

    event_base_dispatch(connection->base);
    event_base_free(connection->base);
    free(connection);
}

void upgrade_to_tls(connection_t *connection)
{
    if (!connection)
    {
        ERROR("Got invalid connection while proceeding to upgrade to TLS !");
        return;
    }

    /*  Create new SSL object */
    send_control_message(
        connection, FTP_STATUS_AUTH_TLS_OK, "AUTH TLS Success");

    connection->ssl = SSL_new(connection->ssl_ctx);

    /* This old bufferevent must be disabled then only filter will apply */
    bufferevent_disable(connection->bev, EV_READ | EV_WRITE);

    /*  Wrap the fd with SSL-enabled bufferevent */
    struct bufferevent *bev_ssl = bufferevent_openssl_filter_new(
        connection->base,
        connection->bev,
        connection->ssl,
        BUFFEREVENT_SSL_ACCEPTING,
        BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);

    /* Do not free the old bufferevent here ! It is still used !*/

    connection->bev = bev_ssl;
    bufferevent_setcb(connection->bev, on_read, on_write, on_event, connection);
    bufferevent_enable(connection->bev, EV_READ | EV_WRITE);
    connection->upgraded_to_tls = 1;
}

void write_text_response(struct bufferevent *bev, const char *text)
{
    if (bev)
        bufferevent_write(bev, text, strlen(text));
    else
        ERROR("Connection buffer event is NULL");
}

void send_control_message(connection_t *connection,
                          uint32_t status_code,
                          const char *text)
{
    if (!connection || !connection->bev)
    {
        ERROR("Got invalid connection while sending a control message !");
        return;
    }

    char buffer[MAX_RESPONSE_BUFFER_LENGTH];
    if (status_code)
        snprintf(
            buffer, sizeof(buffer), "%" PRIu32 " %s\r\n", status_code, text);
    else
        snprintf(buffer, sizeof(buffer), "%s\r\n", text);

    bufferevent_write(connection->bev, buffer, strnlen(buffer, sizeof(buffer)));
}
