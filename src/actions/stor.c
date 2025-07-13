#include <dirent.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <unistd.h>

#include "command_actions.h"
#include "control_handler.h"
#include "data_handler.h"
#include "error.h"
#include "ftp_status_codes.h"

void cftp_recv_file_with_evbuffer(connection_t *connection,
                                  const char *filepath);
static void on_stor_read(struct bufferevent *bev, void *ctx);
static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx);
static void on_eof_event_cb(struct bufferevent *bev, void *ctx);

void cftp_recv_file_with_evbuffer(connection_t *connection,
                                  const char *filepath)
{
    if (!connection->data_bev)
    {
        ERROR("Failed to open data connection !");
        send_control_message(connection,
                             FTP_STATUS_CANNOT_OPEN_DATA,
                             "Cannot open data connection");
        return;
    }

    int fd = open(filepath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0)
    {
        ERROR("Failed to open file: %s", filepath);
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(connection,
                             FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                             "Failed to open file for writing");
        return;
    }

    connection->upload_fd = fd;
    connection->data_eof_event_cb = on_eof_event_cb;

    if (connection->data_tls_required)
        connection->data_tls_event_connected_cb = tls_on_bev_event_connected;
    else
        connection->data_read_cb = on_stor_read;

    send_control_message(
        connection, FTP_STATUS_FILE_STATUS_OKAY, "Read to receive");
}

static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection || !bev)
    {
        ERROR("Invalid connection object !");
        return;
    }

    DEBG("TLS Handshake successful for %s data connection",
         connection->username);
    connection->data_read_cb = on_stor_read;
}

static void on_stor_read(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection)
    {
        ERROR("Invalid connection object !");
        return;
    }

    struct evbuffer *input = bufferevent_get_input(bev);
    while (evbuffer_get_length(input) > 0)
        evbuffer_write(input, connection->upload_fd);
}

static void on_eof_event_cb(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;

    if (!connection || !connection->data_bev || !bev)
    {
        ERROR("Invalid connection object !");
        return;
    }

    if (connection->upload_fd >= 0)
    {
        struct evbuffer *input = bufferevent_get_input(connection->data_bev);
        while (evbuffer_get_length(input) > 0)
            evbuffer_write(input, connection->upload_fd);

        fsync(connection->upload_fd);
        close(connection->upload_fd);
        DEBG("Received full file !");
        connection->upload_fd = -1;
        send_control_message(connection,
                             FTP_STATUS_DATA_CONNECTION_CLOSING,
                             "Transfer complete");
        return;
    }

    send_control_message(
        connection, FTP_STATUS_INSUFFICIENT_STORAGE, "An error occurred");
}
