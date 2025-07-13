#ifndef CONNECTION_H
#define CONNECTION_H

#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/listener.h>
#include <openssl/ssl.h>

typedef void (*accept_callback_t)(struct evconnlistener *listener,
                                  evutil_socket_t fd,
                                  struct sockaddr *addr,
                                  int len,
                                  void *ctx);
typedef void (*data_callback_t)(struct bufferevent *bev, void *ctx);

void control_connection_accept_cb(struct evconnlistener *listener,
                                  evutil_socket_t fd,
                                  struct sockaddr *addr,
                                  int len,
                                  void *ctx);
void tls_data_connection_upgrade_cb(struct evconnlistener *listener,
                                    evutil_socket_t fd,
                                    struct sockaddr *addr,
                                    int len,
                                    void *ctx);

/* Server stuff */

void start_server_listener(struct event_base *base,
                           void *ctx,
                           int port,
                           accept_callback_t accept_cb);

void close_data_connection_on_writecb(struct bufferevent *bev, void *ctx);

typedef enum transfer_mode
{
    TRANSFER_MODE_ASCII,
    TRANSFER_MODE_BINARY,
    TRANSFER_MODE_EBCDIC
} transfer_mode_t;

#define FILE_CHUNK_SIZE 0x4000000 /* 64 MB */

typedef struct
{
    int fd;
    off_t offset;
    off_t filesize;
    char buffer[FILE_CHUNK_SIZE];
} file_stream_t;

typedef struct
{
    /* User meta */
    char username[256];            /* Username for the authenticated user */
    uint32_t uid;                  /* User ID for the authenticated user*/
    uint32_t gid;                  /* Group ID for the authenticated use*/
    transfer_mode_t transfer_mode; /* Transfer mode for the connection */
    data_callback_t control_write_cb;
    char source_ip[INET6_ADDRSTRLEN];

    /* File descriptors */
    int control_fd;      /* File descriptor for control connection*/
    int data_fd;         /* File descriptor for data connection*/
    int interprocess_fd; /* File descriptor for communication with main process
                          */

    int authenticated;   /* Authentication status*/
    char error_buf[256]; /* Buffer for error messages*/
    int data_port;       /* Port for data connection*/
    int control_active;  /* Flag to indicate if control connection is active*/
    volatile int
        data_active;     /* Flag to indicate if data connection is active */
    int upgraded_to_tls; /* Flag to indicate if the connection is upgrading to
                            TLS */
    int data_tls_required;
    data_callback_t data_read_cb;
    data_callback_t data_write_cb;
    data_callback_t data_tls_event_connected_cb;
    data_callback_t data_eof_event_cb;
    file_stream_t *data_stream;
    char path[PATH_MAX];
    int description;
    int hidden;
    int human;

    /* Server structures */
    SSL_CTX *ssl_ctx;        /* SSL context for secure connections */
    SSL *ssl;                /* SSL structure for the connection */
    int fd;                  /* File descriptor for the connection */
    struct bufferevent *bev; /* Buffer event for the connection */
    struct event_base *base; /* Event base for the connection */

    /* data channels */
    int passive_fd;
    struct evconnlistener
        *pasv_listener; /* Listener for passive data connections */
    struct event *data_event;
    struct bufferevent *data_bev; /* Buffer event for passive data connection */
    SSL *data_ssl; /* SSL structure for passive data connection */
    int upload_fd;

    /* Interprocess Communication */
    struct bufferevent *interprocess_bev; /* Buffer event for interprocess
                                              communication */

    struct event *timeout_event;
} connection_t;

int get_random_unused_port(
    void); /*TODO: Deprecate this method and add a configurable port limit */

void on_read(struct bufferevent *bev, void *cookie);
void fill_source_ip(struct sockaddr *addr, char ip_str[]);
void disable_connection_cb(struct bufferevent *bev, void *ctx);

#endif
