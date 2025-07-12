#include <openssl/err.h>
#include <openssl/ssl.h>
#include <signal.h>
#include <sys/wait.h>

#include "connection.h"
#include "error.h"
#include "server_state.h"

extern server_state_t g_server_state;

static void print_log_to_console(const char *message);
static void sigchld_handler(int signo);
static void setup_sigchld_handler(void);

static void print_log_to_console(const char *message) { printf("%s", message); }

static void sigchld_handler(int signo)
{
    (void)signo;  // unused
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

static void setup_sigchld_handler()
{
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }
}

int main()
{
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
    initialize_logger(print_log_to_console);
    setup_sigchld_handler();

    init_server_state(1000, "CFTP Server", "1.0", 21);

    start_server_listener(g_server_state.base,
                          g_server_state.ssl_ctx,
                          g_server_state.port,
                          control_connection_accept_cb);
    event_base_dispatch(g_server_state.base);

    event_base_free(g_server_state.base);
    SSL_CTX_free(g_server_state.ssl_ctx);

    return 0;
}
