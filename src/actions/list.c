#include <dirent.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "command_actions.h"
#include "control_handler.h"
#include "error.h"
#include "ftp_status_codes.h"
#include "interprocess_handler.h"
#include "security.h"

typedef struct
{
    bool all;
    bool human;
    bool recursive;
    const char *path;
} list_flags_t;

void handle_list_command(cftp_command_t *command,
                         connection_t *connection,
                         int description);
static bool parse_list_flags(cftp_command_t *cmd, list_flags_t *flags);
static void send_list_command_output(connection_t *connection,
                                     const char *params,
                                     int description,
                                     int hidden,
                                     bool human);
static void format_unix_list_entry(connection_t *connection,
                                   char *buf,
                                   size_t bufsize,
                                   const char *name,
                                   const struct stat *st,
                                   bool human);
static const char *human_readable_size(off_t size, char *buf, size_t buflen);
static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx);
static void close_on_listcb(struct bufferevent *bev, void *ctx);

static bool parse_list_flags(cftp_command_t *cmd, list_flags_t *flags)
{
    memset(flags, 0, sizeof(list_flags_t));

    for (int i = 0; i < cmd->argc; i++)
    {
        IF_MATCHES(cmd->args[i], "-a")
        flags->all = true;
        ELSE IF_MATCHES(cmd->args[i], "-h") flags->human = true;
        ELSE IF_MATCHES(cmd->args[i], "-R") flags->recursive = true;
        ELSE IF(!flags->path) flags->path = cmd->args[i];
        ELSE return false;
    }

    return 1;
}

static void close_on_listcb(struct bufferevent *bev, void *ctx)
{
    if (!bev) return;

    DEBG("Sent Directory OK");
    connection_t *connection = (connection_t *)ctx;
    connection->control_write_cb = close_data_connection_on_writecb;
    send_control_message(
        connection, FTP_STATUS_DATA_CONNECTION_CLOSING, "Directory send OK");
}

void handle_list_command(cftp_command_t *command,
                         connection_t *connection,
                         int description)
{
    list_flags_t args;
    if (!parse_list_flags(command, &args))
    {
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(connection,
                             FTP_STATUS_SYNTAX_ERROR_PARAMS,
                             "Failed to parse parameters");
        return;
    }

    if (!connection->data_bev)
    {
        ERROR("Failed to open data connection !");
        send_control_message(connection,
                             FTP_STATUS_CANNOT_OPEN_DATA,
                             "Cannot open data connection");
        return;
    }

    char path[PATH_MAX] = ".";

    if (args.path && strlen(args.path) > 0)
        snprintf(path, sizeof(path), "%s", args.path);

    if (!is_path_safe(path))
    {
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(
            connection, FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM, "Invalid path");
        return;
    }

    DIR *dir = opendir(path);
    if (!dir)
    {
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(
            connection, FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM, "Invalid path");
        ERROR("Directory %s does not exists or %s cannot access",
              path,
              connection->username);
        return;
    }

    if (connection->data_tls_required)
    {
        strncpy(connection->path, path, PATH_MAX);
        connection->description = description;
        connection->hidden = args.all;
        connection->data_tls_event_connected_cb = tls_on_bev_event_connected;
        connection->human = args.human;
    }

    send_control_message(connection,
                         FTP_STATUS_FILE_STATUS_OKAY,
                         "Here comes the directory listings");

    if (!connection->data_tls_required)
        send_list_command_output(
            connection, path, description, args.all, args.human);
}

static void tls_on_bev_event_connected(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;
    if (!connection || !bev)
    {
        ERROR("Invalid connection object");
        return;
    }

    send_list_command_output(connection,
                             connection->path,
                             connection->description,
                             connection->hidden,
                             connection->human);
}

static void send_list_command_output(connection_t *connection,
                                     const char *params,
                                     int description,
                                     int hidden,
                                     bool human)
{
    char path[PATH_MAX] = {0};
    strncpy(path, params, PATH_MAX);

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

        char line[PATH_MAX];
        if (description)
            format_unix_list_entry(
                connection, line, sizeof(line), entry->d_name, &st, human);
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
                                   const struct stat *st,
                                   bool human)
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

    char sizebuf[32];
    if (human)
        human_readable_size(st->st_size, sizebuf, sizeof(sizebuf));
    else
        snprintf(sizebuf, sizeof(sizebuf), "%8" PRId64, (long)st->st_size);

    snprintf(buf,
             bufsize,
             "%s %2" PRId64 " %-8s %-8s %10s %s %s\r\n",
             perms,
             (unsigned long)st->st_nlink,
             username,
             groupname,
             sizebuf,
             timebuf,
             name);
}

static const char *human_readable_size(off_t size, char *buf, size_t buflen)
{
    const char *units[] = {"B", "K", "M", "G", "T", "P"};
    int unit = 0;
    double s = (double)size;

    while (s >= 1024.0 && unit < 5)
    {
        s /= 1024.0;
        unit++;
    }

    snprintf(buf, buflen, "%.1f%s", s, units[unit]);
    return buf;
}
