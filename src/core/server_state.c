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

static int try_bind_port(int port);

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

    if (connections_init_pasv_range(g_server_state.config.passive_port_start,
                                    g_server_state.config.passive_port_end) < 0)
    {
        ERROR("Failed to initialize passive port range");
        exit(-1);
    }

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

int connections_init_pasv_range(int start, int end)
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

    st_free(&g_server_state.pasv_range.st);

    int *base =
        (int *)malloc(sizeof(int) * (size_t)g_server_state.pasv_range.n);
    if (!base) return -1;
    for (int i = 0; i < g_server_state.pasv_range.n; ++i) base[i] = 1;

    int rc = st_build(
        &g_server_state.pasv_range.st, base, g_server_state.pasv_range.n);
    free(base);
    if (rc != 0) return rc;

    if (st_total(&g_server_state.pasv_range.st) != g_server_state.pasv_range.n)
        return -1;
    return 0;
}

int select_leftmost_available_port(void)
{
    if (g_server_state.pasv_range.n <= 0) return -1;
    int *tmp = malloc(sizeof(int) * (size_t)g_server_state.pasv_range.n);
    int tcnt = 0;
    if (!tmp) return -1;

    while (st_total(&g_server_state.pasv_range.st) > 0)
    {
        int idx = st_find_leftmost_positive(&g_server_state.pasv_range.st);
        if (idx < 0) break;
        int port = g_server_state.pasv_range.start + idx;

        if (try_bind_port(port) == 0)
        {
            st_update(&g_server_state.pasv_range.st, idx, 0);  // reserve one
            for (int i = 0; i < tcnt; ++i)
                st_update(&g_server_state.pasv_range.st, tmp[i], 1);
            free(tmp);
            return port;
        }
        else
        {
            st_update(&g_server_state.pasv_range.st, idx, 0);  // temp hide
            tmp[tcnt++] = idx;
        }
    }
    for (int i = 0; i < tcnt; ++i)
        st_update(&g_server_state.pasv_range.st, tmp[i], 1);
    free(tmp);
    return -1;
}

static int try_bind_port(int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    int one = 1;
    (void)setsockopt(sock,
                     SOL_SOCKET,
                     SO_REUSEADDR,
                     &one,
                     sizeof(one)); /* test robustness */

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    /* For tests, bind to loopback to avoid external conflicts; switch to
     * INADDR_ANY in prod */
#ifndef DEBUG_TRY_BIND
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
#else
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
#endif
    addr.sin_port = htons((uint16_t)port);

    int ok = (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    close(sock);
    return ok ? 0 : -1;
}

void release_port(int port)
{
    if (port < g_server_state.pasv_range.start ||
        port > g_server_state.pasv_range.end)
        return;
    int idx = port - g_server_state.pasv_range.start;
    st_update(&g_server_state.pasv_range.st, idx, 1);
}

void connections_shutdown(void)
{
    st_free(&g_server_state.pasv_range.st);
    g_server_state.pasv_range.start = 0;
    g_server_state.pasv_range.end = -1;
    g_server_state.pasv_range.n = 0;
}
