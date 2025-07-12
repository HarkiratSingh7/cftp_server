/*
    Author: Harkirat Singh
    Description: Provide error handling (logging) facilities
*/

#ifndef _CFTP_ERROR_H
#define _CFTP_ERROR_H

#include <stddef.h>

#include "logger.h"

/* Log levels static variable */
// #ifdef _TEST
// extern int enabled_logs = 0xF;
// #else
// extern int enabled_logs = (ERR_LEVEL | WRN_LEVEL);
// #endif
extern int enabled_logs;

/* Error Codes for internal function logics */
#define SUCCESS_ERR 0x0
#define FAILURE_ERR -1
#define FAILURE_CORRUPT_ERR 0x2

#define LOGM(t, ...)                                                   \
    do                                                                 \
    {                                                                  \
        if (enabled_logs & (t))                                        \
        {                                                              \
            print_log((t), __FILE__, __LINE__, __func__, __VA_ARGS__); \
        }                                                              \
    } while (0)

#define ERROR(...) LOGM(ERR_LEVEL, __VA_ARGS__)
#define INFO(...) LOGM(INF_LEVEL, __VA_ARGS__)
#define WARN(...) LOGM(WRN_LEVEL, __VA_ARGS__)
#define DEBG(...) LOGM(DBG_LEVEL, __VA_ARGS__)
#define PRINT(...) LOGM(PRN_LEVEL, __VA_ARGS__)

// #define LOGM(t, format...)                                          \
//     {                                                               \
//         if (enabled_logs & (t))                                     \
//         {                                                           \
//             print_log((t), __FILE__, __LINE__, __func__, ##format); \
//         }                                                           \
//     }

// #define ERROR(a...) LOGM(ERR_LEVEL, ##a)
// #define INFO(a...) LOGM(INF_LEVEL, ##a)
// #define WARN(a...) LOGM(WRN_LEVEL, ##a)
// #define DEBG(a...) LOGM(DBG_LEVEL, ##a)
// #define PRINT(a...) LOGM(PRN_LEVEL, ##a)

void hexdump(void *memory, size_t length);

#endif  //_CFTP_ERROR_H
