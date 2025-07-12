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
#include "error.h"
#include "ftp_status_codes.h"
#include "interprocess_handler.h"

void handle_LIST_command(connection_t *connection,
                         const char *params,
                         int description,
                         int hidden);
static void send_list_command_output(connection_t *connection,
                                     const char *params,
                                     int description,
                                     int hidden);
static void format_unix_list_entry(connection_t *connection,
                                   char *buf,
                                   size_t bufsize,
                                   const char *name,
                                   const struct stat *st);

static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx);
static void close_on_listcb(struct bufferevent *bev, void *ctx);

static void close_on_listcb(struct bufferevent *bev, void *ctx)
{
    DEBG("Sent Directory OK");
    connection_t *connection = (connection_t *)ctx;
    connection->control_write_cb = close_data_connection_on_writecb;
    send_control_message(
        connection, FTP_STATUS_DATA_CONNECTION_CLOSING, "Directory send OK");
}

void handle_LIST_command(connection_t *connection,
                         const char *params,
                         int description,
                         int hidden)
{
    if (!connection->data_bev)
    {
        ERROR("Failed to open data connection !");
        send_control_message(connection,
                             FTP_STATUS_CANNOT_OPEN_DATA,
                             "Cannot open data connection");
        return;
    }

    char path[PATH_MAX] = ".";
    if (params && strlen(params) > 0)
    {
        /*  Prevent traversal outside the chroot */
        if (strstr(params, "..") != NULL)
        {
            connection->control_write_cb = close_data_connection_on_writecb;
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                                 "Invalid path");
            return;
        }
        snprintf(path, sizeof(path), "%s", params);
    }

    DIR *dir = opendir(path);
    if (!dir)
    {
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(
            connection, FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM, "Invalid path");
        return;
    }

    if (connection->data_tls_required)
    {
        connection->params = params;
        connection->description = description;
        connection->hidden = hidden;
        connection->data_tls_event_connected_cb = tls_on_bev_event_connected;
    }

    send_control_message(connection,
                         FTP_STATUS_FILE_STATUS_OKAY,
                         "Here comes the directory listings");

    if (!connection->data_tls_required)
        send_list_command_output(connection, params, description, hidden);
}

static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection)
    {
        ERROR("Invalid connection object");
        return;
    }

    send_list_command_output(connection,
                             connection->params,
                             connection->description,
                             connection->hidden);
}

static void send_list_command_output(connection_t *connection,
                                     const char *params,
                                     int description,
                                     int hidden)
{
    char path[PATH_MAX] = ".";
    if (params && strlen(params) > 0)
    {
        /*  Prevent traversal outside the chroot */
        if (strstr(params, "..") != NULL)
        {
            connection->control_write_cb = close_data_connection_on_writecb;
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                                 "Invalid path");
            return;
        }
        snprintf(path, sizeof(path), "%s", params);
    }
    DIR *dir = opendir(path);
    struct dirent *entry;
    struct evbuffer *evbuf = evbuffer_new();

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(fullpath, &st) == -1) continue;

        if (hidden == 0 && entry->d_name[0] == '.') continue;

        char line[4096];
        if (description)
            format_unix_list_entry(
                connection, line, sizeof(line), entry->d_name, &st);
        else
            snprintf(line, sizeof(line), "%s\r\n", entry->d_name);
        DEBG("Got line %s", line);
        evbuffer_add(evbuf, line, strlen(line));
    }

    closedir(dir);
    if (evbuffer_get_length(evbuf) == 0)
    {
        DEBG("Got nothing to send !");
        connection->control_write_cb = close_data_connection_on_writecb;
        connection->data_tls_event_connected_cb =
            close_data_connection_on_writecb;
        send_control_message(connection,
                             FTP_STATUS_DATA_CONNECTION_CLOSING,
                             "Directory send OK");
        evbuffer_free(evbuf);
        return;
    }
    connection->data_write_cb = close_on_listcb;
    bufferevent_write_buffer(connection->data_bev, evbuf);

    DEBG("Sent directory listing to data connection");
    evbuffer_free(evbuf);
}

static void format_unix_list_entry(connection_t *connection,
                                   char *buf,
                                   size_t bufsize,
                                   const char *name,
                                   const struct stat *st)
{
    char perms[11] = "----------";

    if (S_ISDIR(st->st_mode))
        perms[0] = 'd';
    else if (S_ISLNK(st->st_mode))
        perms[0] = 'l';

    static const char rwx[] = {'r', 'w', 'x'};
    for (int i = 0; i < 9; ++i)
        if (st->st_mode & (1 << (8 - i))) perms[i + 1] = rwx[i % 3];

    struct tm *tm = localtime(&st->st_mtime);
    char timebuf[32];
    strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);

    char username[256];
    ask_root_for_username(connection, st->st_uid, username, sizeof(username));

    char groupname[256];
    ask_root_for_groupname(
        connection, st->st_gid, groupname, sizeof(groupname));

    snprintf(buf,
             bufsize,
             "%s 1 %s %s %8ld %s %s\r\n",
             perms,
             username,
             groupname,
             (long)st->st_size,
             timebuf,
             name);
}
