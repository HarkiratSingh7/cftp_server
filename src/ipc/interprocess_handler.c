#include "interprocess_handler.h"

#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <unistd.h>

#include "connection.h"
#include "error.h"
#include "server_state.h"

void event_cb(struct bufferevent *bev, short events, void *ctx);
static void ask_custom_command(connection_t *connection,
                               char *buffer,
                               size_t buffer_size);

extern server_state_t g_server_state;

/* IPC structure for maintaining state when child tries to wait for reply */
typedef struct
{
    char *buffer;
    size_t buffer_size;
    int reply_received;
} reply_context_t;

void register_interprocess_fd_on_server(int interprocess_fd)
{
    DEBG("Registered interprocess fd: %d", interprocess_fd);
    struct bufferevent *bev = bufferevent_socket_new(
        g_server_state.base, interprocess_fd, BEV_OPT_CLOSE_ON_FREE);
    if (!bev)
    {
        ERROR("bufferevent_socket_new in server");
        return;
    }

    __sync_add_and_fetch(&g_server_state.current_connections, 1);
    bufferevent_setcb(bev, on_read, NULL, event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_WRITE);
}

void register_interprocess_fd_on_child(int interprocess_fd,
                                       connection_t *connection)
{
    DEBG("Registered interprocess fd for child: %d", interprocess_fd);
    struct event_base *base =
        event_base_new(); /* Nested event base for ipc purpose only */

    connection->interprocess_bev =
        bufferevent_socket_new(base, interprocess_fd, BEV_OPT_CLOSE_ON_FREE);
    if (!connection->interprocess_bev)
    {
        ERROR("bufferevent_socket_new in child");
        return;
    }

    bufferevent_setcb(connection->interprocess_bev, NULL, NULL, event_cb, NULL);
    bufferevent_enable(connection->interprocess_bev, EV_READ | EV_WRITE);
}

/**
 * [BOTH] Called on events like errors or connection close.
 */
void event_cb(struct bufferevent *bev, short events, void *ctx)
{
    if (!ctx) return;

    if (events & BEV_EVENT_ERROR) ERROR("Error from bufferevent");

    if (events & (BEV_EVENT_EOF | BEV_EVENT_ERROR))
    {
        bufferevent_disable(bev, EV_READ | EV_WRITE);
        bufferevent_setcb(bev, NULL, NULL, NULL, ctx);
        bufferevent_free(bev);
        __sync_add_and_fetch(&g_server_state.current_connections, 1);
        if (getuid() != 0)
        {
            ERROR(
                "Interprocess communication error, exiting because main "
                "process exited");
            exit(-1);
        }
    }
}

void on_parent_dead(evutil_socket_t fd,
                    short events,
                    void *ctx __attribute__((unused)))
{
    INFO("IPC pipe event occurred %" PRId16, events);
    char dummy[1];
    ssize_t n = read(fd, dummy, sizeof(dummy));
    if (n == 0)
    {
        // Parent closed its end (EOF)
        ERROR("Parent process died. Exiting child.");
        exit(1);
    }
    else if (n < 0)
    {
        perror("read");
        exit(1);
    }
}

/* This callback fires when the parent's reply arrives */
static void ipc_reply_cb(struct bufferevent *bev, void *ctx)
{
    reply_context_t *reply_ctx = (reply_context_t *)ctx;

    size_t n =
        bufferevent_read(bev, reply_ctx->buffer, reply_ctx->buffer_size - 1);
    reply_ctx->buffer[n] = '\0';

    reply_ctx->reply_received = 1;
    DEBG("IPC callback received reply: %s", reply_ctx->buffer);

    event_base_loopexit(bufferevent_get_base(bev), NULL);
}

static void ask_custom_command(connection_t *connection,
                               char *buffer,
                               size_t buffer_size)
{
    if (!connection || !buffer || buffer_size == 0)
    {
        return;
    }

    reply_context_t reply_ctx = {
        .buffer = buffer, .buffer_size = buffer_size, .reply_received = 0};

    bufferevent_setcb(
        connection->interprocess_bev, ipc_reply_cb, NULL, event_cb, &reply_ctx);

    bufferevent_write(connection->interprocess_bev, buffer, strlen(buffer));
    DEBG("Sent custom command, now waiting for reply...");

    /*TODO: Add a timeout here */

    event_base_dispatch(bufferevent_get_base(connection->interprocess_bev));

    if (reply_ctx.reply_received)
        DEBG("ask_custom_command finished with reply: %s", reply_ctx.buffer);
    else
        ERROR("ask_custom_command failed: no reply received.");

    bufferevent_setcb(
        connection->interprocess_bev, NULL, NULL, event_cb, connection);
}

void ask_root_for_username(connection_t *connection,
                           uint32_t uid,
                           char *buffer,
                           size_t buffer_size)
{
    snprintf(buffer, buffer_size, "UID %u", uid);
    ask_custom_command(connection, buffer, buffer_size);
}

void ask_root_for_groupname(connection_t *connection,
                            uint32_t gid,
                            char *buffer,
                            size_t buffer_size)
{
    snprintf(buffer, buffer_size, "GID %u", gid);
    ask_custom_command(connection, buffer, buffer_size);
}
