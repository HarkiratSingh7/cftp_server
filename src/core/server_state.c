#include "server_state.h"

#include <event2/bufferevent.h>
#include <openssl/ssl.h>

#include "error.h"

server_state_t g_server_state;

void init_server_state(int max_connections,
                       const char *server_name,
                       const char *server_version,
                       int port)
{
    g_server_state.max_connections = max_connections;
    g_server_state.current_connections = 0;
    snprintf(g_server_state.server_name,
             sizeof(g_server_state.server_name),
             "%s",
             server_name);
    snprintf(g_server_state.server_version,
             sizeof(g_server_state.server_version),
             "%s",
             server_version);
    g_server_state.is_running = 0;
    g_server_state.port = port;
    g_server_state.ssl_ctx = SSL_CTX_new(
        TLS_server_method()); /* SSL context will be initialized later */

    if (!SSL_CTX_use_certificate_file(
            g_server_state.ssl_ctx, "certs/server.crt", SSL_FILETYPE_PEM) ||
        !SSL_CTX_use_PrivateKey_file(
            g_server_state.ssl_ctx, "certs/server.key", SSL_FILETYPE_PEM))
    {
        ERROR("Failed to load cert or key");
        exit(-1);
    }

    g_server_state.base = event_base_new();
    if (!g_server_state.base)
    {
        ERROR("Failed to create event base");
        exit(-1);
    }

    g_server_state.connection_timeout = 60;
    g_server_state.data_connection_timeout = 9;
}

void destroy_server_state()
{
    SSL_CTX_free(g_server_state.ssl_ctx);
    event_base_free(g_server_state.base);
}
