#include "server_state.h"

#include <event2/bufferevent.h>
#include <limits.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "error.h"

server_state_t g_server_state;

#ifndef SERVER_VERSION
#error "SERVER_VERSION not defined"
#endif

void init_server_state()
{
    read_configurations(NULL, &g_server_state.config);
    g_server_state.current_connections = 0;

    snprintf(g_server_state.server_version,
             sizeof(g_server_state.server_version),
             "%s",
             SERVER_VERSION);
    INFO("Starting CFTP Server `%s` of version %s",
         g_server_state.config.server_name,
         g_server_state.server_version);

    g_server_state.is_running = 0;
    g_server_state.ssl_ctx = SSL_CTX_new(
        TLS_server_method()); /* SSL context will be initialized later */

    if (!SSL_CTX_use_certificate_file(g_server_state.ssl_ctx,
                                      g_server_state.config.ssl_cert_file,
                                      SSL_FILETYPE_PEM) ||
        !SSL_CTX_use_PrivateKey_file(g_server_state.ssl_ctx,
                                     g_server_state.config.ssl_key_file,
                                     SSL_FILETYPE_PEM))
    {
        ERROR("Failed to load cert or key");
        exit(-1);
    }

    connections_init_pasv_range(g_server_state.config.passive_port_start,
                                g_server_state.config.passive_port_end);

    g_server_state.base = event_base_new();
    if (!g_server_state.base)
    {
        ERROR("Failed to create event base");
        exit(-1);
    }
}

void destroy_server_state()
{
    SSL_CTX_free(g_server_state.ssl_ctx);
    event_base_free(g_server_state.base);
}

void connections_init_pasv_range(int start, int end)
{
    if (end < start)
    {
        int t = start;
        start = end;
        end = t;
    }
    if (start < 1) start = 1;
    if (end > 65535) end = 65535;

    g_server_state.pasv_range.start = start;
    g_server_state.pasv_range.end = end;
    g_server_state.pasv_range.n = end - start + 1;

    INFO("Passive port range: [%d..%d] containing %d ports",
         g_server_state.pasv_range.start,
         g_server_state.pasv_range.end,
         g_server_state.pasv_range.n);
}
