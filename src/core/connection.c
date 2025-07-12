#include "connection.h"

#include <arpa/inet.h>
#include <event2/buffer.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "command_parser.h"
#include "control_handler.h"
#include "data_handler.h"
#include "error.h"
#include "interprocess_handler.h"
#include "server_state.h"

void control_connection_accept_cb(struct evconnlistener *listener,
                                  evutil_socket_t fd,
                                  struct sockaddr *addr,
                                  int len,
                                  void *ctx)
{
    INFO("Accepted new connection on fd %d", fd);
    SSL_CTX *ssl_ctx = (SSL_CTX *)ctx;

    /* first create a socket pair with the main process */
    int rpc_fd[2];
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, rpc_fd) < 0)
    {
        ERROR("Failed to create socket pair: %s", strerror(errno));
        close(fd);
        return;
    }

    pid_t child = fork();
    if (child == 0)
    {
        evconnlistener_free(listener);
        connection_t *conn = calloc(1, sizeof(connection_t));
        conn->ssl_ctx = ssl_ctx;
        conn->control_active = 1;
        conn->interprocess_fd = rpc_fd[1];
        fill_source_ip(addr, conn->source_ip);
        INFO("Control connection with %s", conn->source_ip);
        close(rpc_fd[0]);
        register_interprocess_fd_on_child(
            rpc_fd[1], conn); /* register the child side of the socket pair */
        start_control_connection_loop(fd, conn);
        exit(0);
    }

    close(rpc_fd[1]);
    register_interprocess_fd_on_server(
        rpc_fd[0]); /* register the parent side of the socket pair */
    close(fd);      /* parent closes client's socket */
}

void on_read(struct bufferevent *bev, void *cookie)
{
    connection_t *connection = (connection_t *)cookie;
    char input[1024] = {0};
    struct evbuffer *input_buf = bufferevent_get_input(bev);
    evbuffer_remove(input_buf, input, sizeof(input) - 1);

    INFO("Received: %s", input);

    if (cookie)
        execute_ftp_command(input, connection);
    else
        execute_root_command(input, bev);
}

void start_server_listener(struct event_base *base,
                           void *ctx,
                           int port,
                           accept_callback_t accept_cb)
{
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    struct evconnlistener *listener =
        evconnlistener_new_bind(base,
                                accept_cb,
                                ctx,
                                LEV_OPT_REUSEABLE | LEV_OPT_CLOSE_ON_FREE,
                                -1,
                                (struct sockaddr *)&sin,
                                sizeof(sin));

    if (!listener)
    {
        ERROR("Could not create listener on port %d", port);
        exit(1);
    }

    INFO("Listening on port %d", port);
}

int get_random_unused_port()
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = 0; /* Let kernel assign */

    bind(sock, (struct sockaddr *)&addr, sizeof(addr));
    getsockname(sock, (struct sockaddr *)&addr, &len);
    close(sock);
    return ntohs(addr.sin_port);
}

void fill_source_ip(struct sockaddr *addr, char ip_str[])
{
    if (addr->sa_family == AF_INET)
    {
        struct sockaddr_in *sin = (struct sockaddr_in *)addr;
        inet_ntop(AF_INET, &(sin->sin_addr), ip_str, INET_ADDRSTRLEN);
        INFO("Client connected from IPv4: %s:%d\n",
             ip_str,
             ntohs(sin->sin_port));
    }
    else if (addr->sa_family == AF_INET6)
    {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)addr;
        inet_ntop(AF_INET6, &(sin6->sin6_addr), ip_str, INET6_ADDRSTRLEN);
        INFO("Client connected from IPv6: [%s]:%d\n",
             ip_str,
             ntohs(sin6->sin6_port));
    }
    else
        INFO("Unknown address family %d\n", addr->sa_family);
}

void close_data_connection_on_writecb(struct bufferevent *bev, void *ctx)
{
    DEBG("Triggered close !");
    connection_t *connection = (connection_t *)ctx;
    close_data_connection(connection);
}
