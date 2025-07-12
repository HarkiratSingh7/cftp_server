#include "logger.h"

#include <event2/bufferevent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"

#define LOGGER_EVENT_DELAY_S 0x1
#define LOGGER_GARBAGE_FREE 0x5
#define LOGGER_HALF_BUFFER 0x100
#define LOGGER_ACCEPTABLE_DELTA (1e7)

#define TRUNC_LOG_WARN "[.. Log truncated due to too much length]"
#define TRUNC_LOG_LEN 43

int enabled_logs = 0xF;
static void print_log_event(const char *message, int is_newline);

struct logger_state
{
    // struct event *logging_event;
    logging_function_cb log_func;
    int enabled_logs;
};

struct logger_args
{
    struct event *ev;
    int is_newline;
    char *message;
};

static struct logger_state logger_g;

static void print_log_event(const char *message, int is_newline)
{
    logger_g.log_func(message);
    if (is_newline)
        logger_g.log_func("\n");
    else
        fflush(stdout);
}

void initialize_logger(logging_function_cb log_func)
{
    logger_g.enabled_logs =
        INF_LEVEL | DBG_LEVEL | WRN_LEVEL | ERR_LEVEL | PRN_LEVEL;

    logger_g.log_func = log_func;
}

static const char *get_log_str(enum logtype type)
{
    switch (type)
    {
        case ERR_LEVEL:
            return KYEL "ERROR";
        case INF_LEVEL:
            return KGRN "INFO";
        case WRN_LEVEL:
            return KYEL "WARNING";
        case DBG_LEVEL:
            return KBLU "DEBUG";
        case PRN_LEVEL:
            return KCYN;
        default:
            return "INVALID";
    }
}

__attribute__((format(printf, 5, 6))) void print_log(
    enum logtype type,
    const char *source_file,
    const unsigned int line_number,
    const char *function_name,
    const char *format,
    ...)
{
    if ((logger_g.enabled_logs & type) == 0x0) return;

    char *message1 = calloc(1, LOGGER_HALF_BUFFER * 2 + NORMAL_LEN);
    int res1 = 0, res2 = 0;

    memset(message1, 0, LOGGER_HALF_BUFFER * 2 + NORMAL_LEN);

    if ((type & PRN_LEVEL) == 0)
        res1 = snprintf(message1,
                        LOGGER_HALF_BUFFER,
                        "%s: %s:%d: %s: ",
                        get_log_str(type),
                        source_file,
                        line_number,
                        function_name);
    else
        res1 = snprintf(message1, LOGGER_HALF_BUFFER, "%s", get_log_str(type));

    char *message2 = message1 + res1;

    va_list valist;
    va_start(valist, format);
    res2 = vsnprintf(message2, LOGGER_HALF_BUFFER, format, valist);
    va_end(valist);

    if ((res1 > res2 ? res1 : res2) + 1 >= LOGGER_HALF_BUFFER)
        res2 = LOGGER_HALF_BUFFER +
               sprintf(message2 + LOGGER_HALF_BUFFER - TRUNC_LOG_LEN,
                       "... %s",
                       TRUNC_LOG_WARN);

    char *message3 = message2 + res2;
    snprintf(message3, NORMAL_LEN, "%s", KNRM);

    print_log_event(message1, !(type & PRN_LEVEL));
}
