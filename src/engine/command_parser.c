#include "command_parser.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "auth.h"
#include "command_actions.h"
#include "connection.h"
#include "control_handler.h"
#include "data_handler.h"
#include "error.h"
#include "ftp_status_codes.h"
#include "interprocess_handler.h"
#include "security.h"
#include "structures/hashmap.h"

/* This must follow: https://datatracker.ietf.org/doc/html/rfc959 */

static struct hash_table *command_registry = NULL;

static int register_command(const char *command,
                            command_execution_cb authenticated_cb,
                            command_execution_cb non_authenticated_cb,
                            connection_t *connection);
inline static void parse_text_command(const char *input,
                                      cftp_command_t *cmd_out);

void execute_root_command(const char *input, struct bufferevent *bev)
{
    char username_buf[MAX_ARG_LEN];

    cftp_command_t cmd;
    parse_text_command(input, &cmd);

    if (cmd.argc != 1)
    {
        ERROR("Invalid command format: %s", input);
        write_text_response(bev, "500 Invalid UID format\r\n");
        destroy_command(&cmd);
        return;
    }

    if (strcmp(cmd.command, "UID") == 0)
    {
        uid_t requested_uid = 0;

        if (sscanf(cmd.args[0], "%u", &requested_uid) != 1)
        {
            ERROR("Invalid UID format: %s", input);
            write_text_response(bev, "500 Invalid UID format\r\n");
            destroy_command(&cmd);
            return;
        }

        struct passwd *pw = getpwuid(requested_uid);
        if (pw)
            snprintf(username_buf, sizeof(username_buf), "%s", pw->pw_name);
        else
            snprintf(username_buf, sizeof(username_buf), "unknown");

        DEBG("[PARENT] Found username '%s'. Sending back to child.\n",
             username_buf);
        bufferevent_write(bev, username_buf, strlen(username_buf) + 1);
    }
    else if (strcmp(cmd.command, "GID") == 0)
    {
        gid_t requested_gid = 0;

        if (sscanf(cmd.args[0], "%u", &requested_gid) != 1)
        {
            ERROR("Invalid GID format: %s", input);
            write_text_response(bev, "500 Invalid GID format\r\n");
            destroy_command(&cmd);
            return;
        }

        struct group *grp = getgrgid(requested_gid);
        if (grp)
            snprintf(username_buf, sizeof(username_buf), "%s", grp->gr_name);
        else
            snprintf(username_buf, sizeof(username_buf), "unknown");

        DEBG("[PARENT] Found username '%s'. Sending back to child.\n",
             username_buf);
        bufferevent_write(bev, username_buf, strlen(username_buf) + 1);
    }

    destroy_command(&cmd);
}

void execute_ftp_command(const char *input, connection_t *connection)
{
    /*  Parse the command and parameters */
    cftp_command_t cmd;
    parse_text_command(input, &cmd);

    DEBG("Got command %s", input);
    command_cb *command_cb =
        get_ptr_to_value_by_key(command_registry, cmd.command);
    if (command_cb)
    {
        if (connection->authenticated)
            command_cb->authenticated_cb(&cmd, connection);
        else
            command_cb->non_authenticated_cb(&cmd, connection);
    }
    else
        cftp_invalid_action(&cmd, connection);

    destroy_command(&cmd);
}

inline static void parse_text_command(const char *input,
                                      cftp_command_t *cmd_out)
{
    memset(cmd_out, 0, sizeof(*cmd_out));

    while (isspace((unsigned char)*input)) input++;

    size_t i = 0;
    while (*input && !isspace((unsigned char)*input) &&
           i < sizeof(cmd_out->command) - 1)
    {
        cmd_out->command[i++] = toupper((unsigned char)*input++);
    }
    cmd_out->command[i] = '\0';

    while (isspace((unsigned char)*input)) input++;

    int argc = 0;
    while (*input && argc < MAX_ARGS)
    {
        while (isspace((unsigned char)*input)) input++;

        if (*input == '\0') break;

        char *arg = malloc(MAX_ARG_LEN);
        if (!arg) break;

        size_t j = 0;
        bool in_quotes = false;

        while (*input && j < MAX_ARG_LEN - 1)
        {
            if (*input == '"' && !in_quotes)
            {
                in_quotes = true;
                input++;
                continue;
            }
            else if (*input == '"' && in_quotes)
            {
                in_quotes = false;
                input++;
                continue;
            }
            else if (!in_quotes && isspace((unsigned char)*input))
            {
                break;
            }
            else if (*input == '\\' && input[1] != '\0')
            {
                arg[j++] = input[1];
                input += 2;
            }
            else
            {
                arg[j++] = *input++;
            }
        }

        arg[j] = '\0';
        cmd_out->args[argc++] = arg;

        while (isspace((unsigned char)*input)) input++;
    }

    cmd_out->argc = argc;
    cmd_out->args[argc] = NULL;
}

void destroy_command(cftp_command_t *command)
{
    for (int i = 0; i < command->argc; ++i) free(command->args[i]);
}

// inline static void parse_text_command(const char *input,
//                                       char *command_out,
//                                       size_t cmd_size,
//                                       char *params_out,
//                                       size_t param_size)
// {
//     while (isspace((unsigned char)*input)) input++;

//     size_t i = 0;
//     while (*input && !isspace((unsigned char)*input) && i < cmd_size - 1)
//     {
//         command_out[i++] = toupper((unsigned char)*input++);
//     }
//     command_out[i] = '\0';

//     while (isspace((unsigned char)*input)) input++;

//     strncpy(params_out, input, param_size - 1);
//     params_out[param_size - 1] = '\0';

//     /*  Trim trailing whitespace from params */
//     size_t len = strlen(params_out);
//     while (len > 0 && isspace((unsigned char)params_out[len - 1]))
//         params_out[--len] = '\0';
// }

static int register_command(const char *command,
                            command_execution_cb authenticated_cb,
                            command_execution_cb non_authenticated_cb,
                            connection_t *connection)
{
    command_cb *cmd_cbs = (command_cb *)malloc(sizeof(command_cb));
    cmd_cbs->connection = connection;
    cmd_cbs->authenticated_cb = authenticated_cb;
    cmd_cbs->non_authenticated_cb = non_authenticated_cb;
    return insert_entry(command_registry, command, cmd_cbs);
}

void initialize_execution_engine(connection_t *connection)
{
    command_registry = create_hash_table();

    for (size_t i = 0; i < sizeof(command_actions) / sizeof(command_action);
         i++)
    {
        if (!register_command(command_actions[i].action,
                              command_actions[i].authenticated_cb,
                              command_actions[i].non_authenticated_cb,
                              connection))
        {
            ERROR("Critical error occurred while registering commands");
            exit(-1);
        }
        else
            DEBG("Registered command %s", command_actions[i].action);
    }

    DEBG("Done registering commands !");
}
