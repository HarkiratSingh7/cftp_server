#include "command_actions.h"

#include <dirent.h>
#include <event2/buffer.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <fcntl.h>
#include <openssl/err.h>
#include <sys/stat.h>
#include <unistd.h>

#include "auth.h"
#include "control_handler.h"
#include "data_handler.h"
#include "error.h"
#include "ftp_status_codes.h"
#include "interprocess_handler.h"
#include "security.h"

extern void cftp_send_file(connection_t *connection, const char *params);
extern void cftp_recv_file_with_evbuffer(connection_t *connection,
                                         const char *filepath);
extern void handle_LIST_command(connection_t *connection,
                                const char *params,
                                int description,
                                int hidden);

static void close_connection_cb(struct bufferevent *bev, void *ctx);

static void handle_mdtm_command(connection_t *conn, const char *arg);
static void handle_cwd_command(connection_t *conn, const char *params);

static void handle_cwd_command(connection_t *conn, const char *params)
{
    char target_path[PATH_MAX];
    struct stat st;

    if (!params || strlen(params) == 0)
    {
        write_text_response(conn->bev, "550 Missing path.\r\n");
        return;
    }

    if (params && strlen(params) > 0)
    {
        /*  Prevent traversal outside the chroot */
        if (strstr(params, "..") != NULL)
        {
            write_text_response(conn->bev, "550 Invalid path\r\n");
            return;
        }
    }
    if (!validate_params(params, conn->error_buf))
    {
        write_text_response(conn->bev, conn->error_buf);
        return;
    }

    snprintf(target_path, sizeof(target_path), "%s", params);

    if (stat(target_path, &st) < 0 || !S_ISDIR(st.st_mode))
    {
        write_text_response(conn->bev, "550 Not a directory.\r\n");
        return;
    }

    if (chdir(target_path) < 0)
    {
        write_text_response(conn->bev, "550 Failed to change directory.\r\n");
        return;
    }

    write_text_response(conn->bev, "250 Directory successfully changed.\r\n");
}

static void handle_mdtm_command(connection_t *conn, const char *arg)
{
    struct stat st;
    if (stat(arg, &st) != 0 || S_ISDIR(st.st_mode))
    {
        write_text_response(conn->bev,
                            "550 Could not get modification time.\r\n");
        return;
    }

    struct tm *gmt = gmtime(&st.st_mtime);
    char buf[64];
    strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", gmt);
    snprintf(buf, sizeof(buf), "213 %s\r\n", buf);
    INFO("Sending %s", buf);
    write_text_response(conn->bev, buf);
}

static void close_connection_cb(struct bufferevent *bev, void *ctx)
{
    connection_t *connection = (connection_t *)ctx;

    if (!connection) return;

    INFO("Disabling connection !");
    bufferevent_disable(bev, EV_READ | EV_WRITE);
    // bufferevent_free(bev);
}

/* NULL Checks must be covered by caller */

void cftp_syst_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    send_control_message(connection,
                         FTP_STATUS_NAME_SYSTEM_TYPE,
                         "UNIX Type: L8"); /* Don't know but still :
                                             https://cr.yp.to/ftp/syst.html */
}

void cftp_quit_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    /* ADD TO A TIMER Instead of freeing now ! Also disable read write from here
     * only*/
    connection->control_write_cb = close_connection_cb;
    send_control_message(connection, FTP_STATUS_SERVICE_CLOSING, "Goodbye");
}

void cftp_auth_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    IF_MATCHES(params, "TLS")
    upgrade_to_tls(connection);
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "Invalid parameter");
}

void cftp_pbsz_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    send_control_message(connection, FTP_STATUS_COMMAND_OK, "PBSZ=0");
}

void cftp_prot_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    IF_MATCHES(params, "P")
    {
        INFO("Data connection TLS required !");
        connection->data_tls_required = 1;
        send_control_message(
            connection, FTP_STATUS_COMMAND_OK, "PROT now Private");
    }
    ELSE send_control_message(connection,
                              FTP_STATUS_COMMAND_NOT_IMPLEMENTED_PERM,
                              "Unsupported PROT level");
}

void cftp_noop_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    send_control_message(
        connection, FTP_STATUS_COMMAND_OK, "NOOP command successful");
}

void cftp_feat_action(const char *input,
                      const char *params,
                      connection_t *connection)
{
    const char *features =
        "211-Features:\r\n"
        " EPSV\r\n"
        " PASV\r\n"
        " AUTH\r\n"
        " SIZE\r\n"
        " MDTM\r\n"
        " MLSD\r\n"
        "211 End";
    send_control_message(connection, 0, features);
}

void cftp_invalid_action(const char *input,
                         const char *params,
                         connection_t *connection)
{
    send_control_message(
        connection, FTP_STATUS_COMMAND_NOT_IMPLEMENTED_PERM, "Unknown command");
}

void cftp_non_authenticated(const char *input,
                            const char *params,
                            connection_t *connection)
{
    send_control_message(connection, FTP_STATUS_NOT_LOGGED_IN, "Not logged in");
}

void cftp_type_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    IF_MATCHES(params, "I")
    {
        connection->transfer_mode = TRANSFER_MODE_BINARY;
        send_control_message(
            connection, FTP_STATUS_COMMAND_OK, "Type set to I");
    }
    ELSE IF_MATCHES(params, "A")
    {
        connection->transfer_mode = TRANSFER_MODE_ASCII;
        send_control_message(
            connection, FTP_STATUS_COMMAND_OK, "Type set to A");
    }
    ELSE send_control_message(
        connection, FTP_STATUS_UNSUPPORTED_TYPE, "Unsupported type");
}

void cftp_epsv_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    data_connection_listener_config(connection, 1);
}

void cftp_pasv_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    data_connection_listener_config(connection, 0);
}

void cftp_nlst_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    handle_LIST_command(connection, params, 0, 1);
}

void cftp_list_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    handle_LIST_command(connection, params, 1, 1);
}

void cftp_size_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    IF(strnlen(params, MAX_COMMAND_LENGTH) != 0)
    {
        struct stat st;
        if (stat(params, &st) < 0 || S_ISDIR(st.st_mode))
        {
            send_control_message(connection,
                                 FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                                 "File not found");
            return;
        }

        char response[MAX_COMMAND_LENGTH];
        snprintf(response, sizeof(response), "%" PRIu64, st.st_size);
        send_control_message(connection, FTP_STATUS_FILE_STATUS, response);
    }
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "File name not specified");
}

void cftp_retr_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    IF(strnlen(params, MAX_COMMAND_LENGTH) != 0)
    cftp_send_file(connection, params);
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "File path not provided");
}

void cftp_stor_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    IF(strnlen(params, MAX_COMMAND_LENGTH) != 0)
    cftp_recv_file_with_evbuffer(connection, params);
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "File path not provided");
}

void cftp_mdtm_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    IF(strnlen(params, MAX_COMMAND_LENGTH) != 0)
    handle_mdtm_command(connection, params);
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "File path not provided");
}

void cftp_cwd_authenticated_action(const char *input,
                                   const char *params,
                                   connection_t *connection)
{
    IF(strnlen(params, MAX_COMMAND_LENGTH) != 0)
    handle_cwd_command(connection, params);
    ELSE send_control_message(
        connection, FTP_STATUS_SYNTAX_ERROR_PARAMS, "Path not provided");
}

void cftp_pwd_authenticated_action(const char *input,
                                   const char *params,
                                   connection_t *connection)
{
    char cwd[PATH_MAX];
    IF(getcwd(cwd, sizeof(cwd)) != NULL)
    {
        char response[PATH_MAX + 64];
        snprintf(
            response, sizeof(response), "\"%s\" is current directory", cwd);
        send_control_message(connection, FTP_STATUS_PATHNAME_CREATED, response);
    }
    ELSE send_control_message(connection,
                              FTP_STATUS_FILE_ACTION_NOT_TAKEN_PERM,
                              "Failed to get current directory");
}

void cftp_abor_authenticated_action(const char *input,
                                    const char *params,
                                    connection_t *connection)
{
    if (connection->data_active)
    {
        close_data_connection(connection);
        send_control_message(connection,
                             FTP_STATUS_ACTION_ABORTED,
                             "closed data connection; tansfer aborted");
    }
    else
    {
        connection->control_write_cb = close_data_connection_on_writecb;
        send_control_message(connection,
                             FTP_STATUS_DATA_CONNECTION_CLOSING,
                             "No transfer in progress");
    }
}

void cftp_authenticated(const char *input,
                        const char *params,
                        connection_t *connection)
{
    send_control_message(
        connection, FTP_STATUS_USER_LOGGED_IN, "Already logged in");
}

void cftp_user_authenticated(const char *input,
                             const char *params,
                             connection_t *connection)
{
    if (user_exists(params, connection))
        send_control_message(connection,
                             FTP_STATUS_USER_NAME_OK,
                             "User name okay, need password");
    else
        send_control_message(
            connection, FTP_STATUS_NOT_LOGGED_IN, "User not found");
}

void cftp_pass_authenticated(const char *input,
                             const char *params,
                             connection_t *connection)
{
    if (authenticate_and_switch_user(
            connection->username, params, connection->error_buf))
    {
        send_control_message(
            connection, FTP_STATUS_USER_LOGGED_IN, "User logged in");
        connection->authenticated = 1;
    }
    else
    {
        ERROR("%s", connection->error_buf);
        connection->control_write_cb = close_connection_cb;
        send_control_message(
            connection, FTP_STATUS_NOT_LOGGED_IN, "Invalid credentials");
    }
}
