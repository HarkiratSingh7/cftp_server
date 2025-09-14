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

void cftp_send_file(connection_t *connection, const char *params);
static void send_next_chunk(struct bufferevent *bev, void *ctx);
static void ftp_send_file_with_evbuffer(connection_t *connection,
                                        const char *params);
static void ftp_send_file_plain(connection_t *connection, const char *params);
static void download_completion_on_plain_connection_cb(struct bufferevent *bev,
                                                       void *ctx);
static void close_on_retrcb(struct bufferevent *bev, void *ctx);

void cftp_send_file(connection_t *connection, const char *params)
{
    IF(connection->data_tls_required)
    ftp_send_file_with_evbuffer(connection, params);
    ELSE ftp_send_file_plain(connection, params);
}

static void ftp_send_file_with_evbuffer(connection_t *connection,
                                        const char *filepath)
{
    if (!connection->data_bev) return;
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) return;  // Handle error

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        close(fd);
        return;
    }

    send_control_message(
        connection, FTP_STATUS_FILE_STATUS_OKAY, "Sending file");

    file_stream_t *fs = calloc(1, sizeof(file_stream_t));

    fs->fd = fd;
    fs->filesize = st.st_size;

    connection->data_stream = fs;
    connection->data_write_cb = send_next_chunk;
    bufferevent_setwatermark(connection->data_bev, EV_WRITE, 128 * 1024, 0);
    bufferevent_enable(connection->data_bev, EV_WRITE);
    send_next_chunk(connection->data_bev, connection);  // kickstart
}

static void ftp_send_file_plain(connection_t *connection, const char *filepath)
{
    if (!connection->data_bev) return;

    int fd = open(filepath, O_RDONLY);
    if (fd < 0)
    {
        ERROR("Error occurred while opening %s", filepath);
        send_control_message(connection,
                             FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                             "Failed to open file");
        return;
    }

    struct stat st;
    if (fstat(fd, &st) < 0)
    {
        ERROR("Error occurred while stating %s", filepath);
        close(fd);
        send_control_message(connection,
                             FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                             "Failed to open file");
        return;
    }

    INFO("Sending file");
    send_control_message(
        connection, FTP_STATUS_FILE_STATUS_OKAY, "Sending file");

    connection->data_write_cb = download_completion_on_plain_connection_cb;
    evbuffer_add_file(
        bufferevent_get_output(connection->data_bev), fd, 0, st.st_size);
}

static void send_next_chunk(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    file_stream_t *fs = connection->data_stream;

    ssize_t n = read(fs->fd, fs->buffer, FILE_CHUNK_SIZE);
    if (fs->offset + n == fs->filesize)
    {
        /* Set connection write back to close the connection */
        connection->data_write_cb = close_on_retrcb;
    }
    if (n <= 0)
    {
        DEBG("Read failed or EOF");
        close(fs->fd);
        fs->fd = -1;
        return;
    }

    bufferevent_write(bev, fs->buffer, n);
    fs->offset += n;
}

static void download_completion_on_plain_connection_cb(struct bufferevent *bev,
                                                       void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    struct evbuffer *output = bufferevent_get_output(bev);

    if (evbuffer_get_length(output) == 0)
    {
        INFO("File transfer complete.");
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(connection,
                             FTP_STATUS_DATA_CONNECTION_CLOSING,
                             "Transfer complete");
    }
}

static void close_on_retrcb(struct bufferevent *bev, void *ctx)
{
    if (!bev) return;

    DEBG("Sent File OK");
    connection_t *connection = (connection_t *)ctx;
    connection->control_write_cb = close_data_connection_on_writecb;
    send_control_message(
        connection, FTP_STATUS_DATA_CONNECTION_CLOSING, "Transfer complete");
}
