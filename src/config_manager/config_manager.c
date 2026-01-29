#include "config_manager.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../core/error.h"
#include "configurations.h"

/* Returns:
 *  0  -> created defaults successfully
 *  1  -> config already exists (nothing done)
 * -1  -> error (check errno)
 */
static int seed_default_config_if_missing(const char *path_opt)
{
    const char *path =
        (path_opt && path_opt) ? path_opt : CFTP_SERVER_CONFIG_FILE;

    /* If file already exists, do nothing */
    struct stat st;
    if (stat(path, &st) == 0) return 1;
    if (errno != ENOENT) return -1;

    /* Build default content (vsftpd-style directive=value with # comments) */
    const char *content =
        "# cftp server configuration (directive=value, '#' for comments)\n"
        "\n# Limits and timeouts\n"
        "max_connections=10000\n"
        "connection_accept_timeout=60\n"
        "data_connection_accept_timeout=9\n"
        "\n# Ports range (IANA dynamic/private ports)\n"
        "passive_port_start=40000\n"
        "passive_port_end=41000\n"
        "port=21\n"
        "\n# Identity\n"
        "server_name=Harkirat's CFTP Server\n"
        "\n# TLS settings\n"
        "\n# Certificate paths (adjust per distro)\n"
        "ssl_cert_file=/etc/ssl/certs/cftp_server.crt\n"
        "ssl_key_file=/etc/ssl/private/cftp_server.key\n";

    /* Create temp file in same directory as target: <path>.tmp.XXXXXX */
    char tmp_path[PATH_MAX];
    size_t n = strnlen(path, sizeof(tmp_path) - 1);
    if (n >= sizeof(tmp_path) - 16)
    {
        errno = ENAMETOOLONG;
        return -1;
    }
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", path);

    /* mkstemp creates and opens a unique file; ensure same filesystem for
     * atomic rename */
    int fd = mkstemp(tmp_path);
    if (fd < 0) return -1;

    /* Set mode to 0644 for a non-secret config (service can read without
     * elevated perms) */
    if (fchmod(fd, 0644) != 0)
    {
        int e = errno;
        close(fd);
        unlink(tmp_path);
        errno = e;
        return -1;
    }

    /* Write all content */
    size_t to_write = strlen(content);
    const char *p = content;
    while (to_write > 0)
    {
        ssize_t w = write(fd, p, to_write);
        if (w < 0)
        {
            int e = errno;
            close(fd);
            unlink(tmp_path);
            errno = e;
            return -1;
        }
        p += w;
        to_write -= (size_t)w;
    }

    /* Flush data to disk */
    if (fsync(fd) != 0)
    {
        int e = errno;
        close(fd);
        unlink(tmp_path);
        errno = e;
        return -1;
    }
    if (close(fd) != 0)
    {
        int e = errno;
        unlink(tmp_path);
        errno = e;
        return -1;
    }

    /* Atomically move into place */
    if (rename(tmp_path, path) != 0)
    {
        int e = errno;
        unlink(tmp_path);
        errno = e;
        return -1;
    }

    /* fsync the containing directory to make the rename durable */
    char dirbuf[PATH_MAX];
    strncpy(dirbuf, path, sizeof(dirbuf) - 1);
    dirbuf[sizeof(dirbuf) - 1] = '\0';
    char *slash = strrchr(dirbuf, '/');
    const char *dirpath = "/";
    if (slash && slash != dirbuf)
    {
        *slash = '\0';
        dirpath = dirbuf;
    }
    int dfd = open(dirpath, O_RDONLY | O_DIRECTORY);
    if (dfd >= 0)
    {
        (void)fsync(dfd);
        close(dfd);
    }
    return 0;
}

static char *trim_left(char *s)
{
    while (*s && isspace((unsigned char)*s)) s++;
    return s;
}

static void trim_right_inplace(char *s)
{
    size_t n = strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1]))
    {
        s[--n] = '\0';
    }
}

static void trim_inplace(char *s)
{
    char *p = trim_left(s);
    if (p != s) memmove(s, p, strlen(p) + 1);
    trim_right_inplace(s);
}

static int equals_icase(const char *a, const char *b)
{
    for (; *a && *b; a++, b++)
    {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    }
    return *a == '\0' && *b == '\0';
}

static int parse_int(const char *v, int *out)
{
    if (!v || !*v) return 0;
    char *end = NULL;
    errno = 0;
    long val = strtol(v, &end, 10);
    if (errno != 0 || end == v || *trim_left(end) != '\0') return 0;
    if (val < INT_MIN || val > INT_MAX) return 0;
    *out = (int)val;
    return 1;
}

static void assign_kv(configurations_t *cfg,
                      const char *k,
                      const char *v,
                      int line_no)
{
    int iv;
    if (equals_icase(k, "max_connections"))
    {
        if (parse_int(v, &iv) && iv > 0) cfg->max_connections = iv;
    }
    else if (equals_icase(k, "connection_accept_timeout"))
    {
        if (parse_int(v, &iv) && iv >= 0) cfg->connection_accept_timeout = iv;
    }
    else if (equals_icase(k, "data_connection_accept_timeout"))
    {
        if (parse_int(v, &iv) && iv >= 0)
            cfg->data_connection_accept_timeout = iv;
    }
    else if (equals_icase(k, "passive_port_start"))
    {
        if (parse_int(v, &iv) && iv >= 1024 && iv <= 65535)
            cfg->passive_port_start = iv;
    }
    else if (equals_icase(k, "passive_port_end"))
    {
        if (parse_int(v, &iv) && iv >= 1024 && iv <= 65535)
            cfg->passive_port_end = iv;
    }
    else if (equals_icase(k, "port"))
    {
        if (parse_int(v, &iv) && iv >= 20 && iv <= 65535)
            cfg->port = iv;  // be lenient, allow 20+
    }
    else if (equals_icase(k, "server_name"))
    {
        if (v)
        {
            snprintf(cfg->server_name, sizeof(cfg->server_name), "%s", v);
            trim_right_inplace(cfg->server_name);
        }
    }
    else if (equals_icase(k, "ssl_cert_file"))
    {
        if (v)
        {
            snprintf(cfg->ssl_cert_file, sizeof(cfg->ssl_cert_file), "%s", v);
            trim_right_inplace(cfg->ssl_cert_file);
        }
    }
    else if (equals_icase(k, "ssl_key_file"))
    {
        if (v)
        {
            snprintf(cfg->ssl_key_file, sizeof(cfg->ssl_key_file), "%s", v);
            trim_right_inplace(cfg->ssl_key_file);
        }
    }
    else
    {
        /* Unknown key: ignore gracefully */
        WARN("Unknown config key '%s' at line %d", k, line_no);
        return;
    }

    DEBG("Config '%s' set to '%s' at line %d", k, v, line_no);
}

static void post_validate(configurations_t *cfg)
{
    /* Ensure passive port range sane and within 1..65535 */
    if (cfg->passive_port_start < 1) cfg->passive_port_start = 1;
    if (cfg->passive_port_end > 65535) cfg->passive_port_end = 65535;
    if (cfg->passive_port_end < cfg->passive_port_start)
    {
        /* swap to recover */
        int t = cfg->passive_port_start;
        cfg->passive_port_start = cfg->passive_port_end;
        cfg->passive_port_end = t;
    }
}

/* Public API: if file_path is NULL or empty, use CFTP_SERVER_CONFIG_FILE */
void read_configurations(const char *file_path, configurations_t *config)
{
    const char *path =
        (file_path && file_path) ? file_path : CFTP_SERVER_CONFIG_FILE;

    fill_default_configurations(config);
    seed_default_config_if_missing(file_path);

    FILE *fp = fopen(path, "r");
    if (!fp)
    {
        ERROR("Failed to open config file '%s': %s. Using defaults.",
              path,
              strerror(errno));
        return;
    }

    char line[256] = {0};
    int line_no = 0;
    while (fgets(line, sizeof(line), fp))
    {
        line_no++;

        /* Normalize line endings and trim */
        trim_inplace(line);
        if (strnlen(line, sizeof(line)) == 0) continue; /* skip empty */
        if (line[0] == '#') continue;                   /* skip comments */

        /* Split key=value on first '=' */
        char *eq = strchr(line, '=');
        if (!eq)
        {
            /* Allow lines without '=', ignore */
            continue;
        }

        *eq = '\0';
        char *key = line;
        char *val = eq + 1;

        /* vsftpd-style forbids spaces, but be liberal and trim anyway */
        trim_inplace(key);
        trim_inplace(val);

        /* Ignore if key empty */
        if (!key || strlen(key) == 0) continue;

        assign_kv(config, key, val, line_no);
    }

    fclose(fp);
    post_validate(config);
}

void fill_default_configurations(configurations_t *config)
{
    if (!config)
    {
        ERROR("Null configuration pointer");
        return;
    }

    memset(config, 0, sizeof(*config));

    config->max_connections = 10000;
    config->connection_accept_timeout = 60;
    config->data_connection_accept_timeout = 9;
    config->passive_port_start = 40000;
    config->passive_port_end = 41000;
    config->port = 21;
    snprintf(config->server_name,
             sizeof(config->server_name),
             "Harkirat's FTP Server");
    snprintf(config->ssl_cert_file,
             sizeof(config->ssl_cert_file),
             "/etc/ssl/certs/cftp_server.crt");
    snprintf(config->ssl_key_file,
             sizeof(config->ssl_key_file),
             "/etc/ssl/private/cftp_server.key");
}
