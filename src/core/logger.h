/*
    Author: Harkirat Singh (HarkiratSingh7)
    Description: Mutlithreaded logger, processes can dump the output to this
   logger.

    Description of logging queue -> Intelligent CFTP Logging Queue

*/

#ifndef LOGGER_H
#define LOGGER_H

/* Yummy colors */
#define NORMAL_LEN 8
#define KNRM "\033[1;0m"
#define KRED "\x1B[31m"
#define KGRN "\033[1;32m"
#define KYEL "\033[1;33m"
#define KBLU "\033[1;34m"
#define KMAG "\x1B[35m"
#define KCYN "\033[1;36m"
#define KWHT "\x1B[37m"

/* A logging function to decide, should be responsible for flushing */
typedef void (*logging_function_cb)(const char *);

enum logtype
{
    ERR_LEVEL = 0x1,
    INF_LEVEL = 0x2,
    WRN_LEVEL = 0x4,
    DBG_LEVEL = 0x8,
    PRN_LEVEL = 0x10
};

void initialize_logger(logging_function_cb log_func);

/*
    Adds a log
*/
void print_log(enum logtype type,
               const char *source_file,
               const unsigned int line_number,
               const char *function_name,
               const char *format,
               ...);

#endif
