#include "command_parser.h"

#include <ctype.h>
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
#include "hashmap.h"
#include "interprocess_handler.h"
#include "security.h"

/* This must follow: https://datatracker.ietf.org/doc/html/rfc959 */

static struct hash_table *command_registry = NULL;

static int register_command(const char *command,
                            command_execution_cb authenticated_cb,
                            command_execution_cb non_authenticated_cb,
                            connection_t *connection);

inline static void parse_text_command(const char *input,
                                      char *command_out,
                                      size_t cmd_size,
                                      char *params_out,
                                      size_t param_size);

void execute_root_command(const char *input, struct bufferevent *bev)
{
    char command[256] = {0};
    char params[1024] = {0};
    char username_buf[256];

    /*  Parse the command and parameters */
    parse_text_command(input, command, sizeof(command), params, sizeof(params));

    if (strcmp(command, "UID") == 0)
    {
        uid_t requested_uid = 0;

        if (sscanf(params, "%u", &requested_uid) != 1)
        {
            ERROR("Invalid UID format: %s", params);
            write_text_response(bev, "500 Invalid UID format\r\n");
            return;
        }

        struct passwd *pw = getpwuid(requested_uid);
        if (pw)
            snprintf(username_buf, sizeof(username_buf), "%s", pw->pw_name);
        else
            snprintf(username_buf, sizeof(username_buf), "unknown");

        INFO("[PARENT] Found username '%s'. Sending back to child.\n",
             username_buf);
        bufferevent_write(bev, username_buf, strlen(username_buf) + 1);
    }
    else if (strcmp(command, "GID") == 0)
    {
        gid_t requested_gid = 0;

        if (sscanf(params, "%u", &requested_gid) != 1)
        {
            ERROR("Invalid GID format: %s", params);
            write_text_response(bev, "500 Invalid GID format\r\n");
            return;
        }

        struct group *grp = getgrgid(requested_gid);
        if (grp)
            snprintf(username_buf, sizeof(username_buf), "%s", grp->gr_name);
        else
            snprintf(username_buf, sizeof(username_buf), "unknown");

        INFO("[PARENT] Found username '%s'. Sending back to child.\n",
             username_buf);
        bufferevent_write(bev, username_buf, strlen(username_buf) + 1);
    }
}

void execute_ftp_command(const char *input, connection_t *connection)
{
    char command[MAX_COMMAND_LENGTH] = {0};
    char params[MAX_COMMAND_LENGTH] = {0};

    /*  Parse the command and parameters */
    parse_text_command(input, command, sizeof(command), params, sizeof(params));
    INFO("Got command %s", input);
    command_cb *command_cb = get_ptr_to_value_by_key(command_registry, command);
    if (command_cb)
    {
        if (connection->authenticated)
            command_cb->authenticated_cb(command, params, connection);
        else
            command_cb->non_authenticated_cb(command, params, connection);
    }
    else
        cftp_invalid_action(command, params, connection);
}

inline static void parse_text_command(const char *input,
                                      char *command_out,
                                      size_t cmd_size,
                                      char *params_out,
                                      size_t param_size)
{
    while (isspace((unsigned char)*input)) input++;

    size_t i = 0;
    while (*input && !isspace((unsigned char)*input) && i < cmd_size - 1)
    {
        command_out[i++] = toupper((unsigned char)*input++);
    }
    command_out[i] = '\0';

    while (isspace((unsigned char)*input)) input++;

    strncpy(params_out, input, param_size - 1);
    params_out[param_size - 1] = '\0';

    /*  Trim trailing whitespace from params */
    size_t len = strlen(params_out);
    while (len > 0 && isspace((unsigned char)params_out[len - 1]))
        params_out[--len] = '\0';
}

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
            INFO("Registered command %s", command_actions[i].action);
    }

    INFO("Done registering commands !");
}
