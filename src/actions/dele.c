#include <dirent.h>
#include <stdbool.h>
#include <sys/stat.h>
#include <unistd.h>

#include "command_actions.h"
#include "control_handler.h"
#include "error.h"
#include "ftp_status_codes.h"
#include "security.h"

typedef struct
{
    bool recursive;
    bool force;
    const char *target;
} dele_args_t;

static int delete_directory_recursively(const char *path);
static bool parse_dele_params(cftp_command_t *command, dele_args_t *args);
void handle_dele_command(cftp_command_t *command, connection_t *connection);

void handle_dele_command(cftp_command_t *command, connection_t *connection)
{
    dele_args_t args;
    if (!parse_dele_params(command, &args))
    {
        send_control_message(connection,
                             FTP_STATUS_SYNTAX_ERROR_PARAMS,
                             "Invalid arguments for DELE.");
        return;
    }
    if (!is_path_safe(args.target))
    {
        send_control_message(
            connection, FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM, "Invalid path");
        return;
    }

    struct stat path_stat;
    if (stat(args.target, &path_stat) != 0)
    {
        if (args.force)
        {
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_OK,
                                 "Force delete: file does not exist.");
            return;
        }
        else
        {
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                                 "File not found.");
            return;
        }
    }

    int delete_res = 0;
    if (S_ISDIR(path_stat.st_mode))
    {
        if (!args.recursive)
        {
            send_control_message(
                connection,
                FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                "Target is a directory. Use -r to delete recursively.");
            return;
        }
        delete_res = delete_directory_recursively(args.target);
    }
    else
    {
        delete_res = unlink(args.target);
    }

    if (delete_res != 0)
    {
        ERROR("Delete failed for '%s': %s, user: %s",
              args.target,
              strerror(errno),
              connection->username);
        if (args.force)
        {
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_OK,
                                 "Force delete: error ignored.");
        }
        else
        {
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                                 "Failed to delete.");
        }
        return;
    }

    DEBG("Deleted target: %s for %s", args.target, connection->username);
    send_control_message(
        connection, FTP_STATUS_FILE_ACTION_OK, "Delete successful.");
}

static bool parse_dele_params(cftp_command_t *command, dele_args_t *args)
{
    memset(args, 0, sizeof(dele_args_t));

    for (int i = 0; i < command->argc; i++)
    {
        IF_MATCHES(command->args[i], "-r")
        args->recursive = true;
        ELSE IF_MATCHES(command->args[i], "-f") args->force = true;
        ELSE IF(!args->target) args->target = command->args[i];
        ELSE return false;
    }

    return args->target != NULL;
}

static int delete_directory_recursively(const char *path)
{
    DIR *dir = opendir(path);
    if (!dir) return -1;

    struct dirent *entry;
    char filepath[PATH_MAX];

    while ((entry = readdir(dir)) != NULL)
    {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(filepath, &st) != 0) continue;

        if (S_ISDIR(st.st_mode))
        {
            if (delete_directory_recursively(filepath) != 0)
            {
                closedir(dir);
                return -1;
            }
        }
        else
        {
            unlink(filepath); /* Delete if file */
        }
    }

    closedir(dir);
    return rmdir(path);
}
