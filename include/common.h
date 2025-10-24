/*****************************************************************************/
/*    vsock-shell - Common utilities implementation                         */
/*****************************************************************************/
#ifndef VSOCK_SHELL_COMMON_H
#define VSOCK_SHELL_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <libgen.h>
#include <ctype.h>
#include <limits.h>

/* Constants */
#define MAX_PATH_LENGTH 300
#define MAX_STRING_LENGTH 30

/* Logging macros - 使用 VSOCK_ 前缀避免与系统头文件冲突 */
#define VSOCK_LOG_ERROR(format, ...) \
    do { \
        syslog(LOG_ERR, "ERROR %s:%d " format, \
               basename(__FILE__), __LINE__, ##__VA_ARGS__); \
    } while (0)

#define VSOCK_LOG_FATAL(format, ...) \
    do { \
        syslog(LOG_ERR, "FATAL %s:%d " format, \
               basename(__FILE__), __LINE__, ##__VA_ARGS__); \
        fprintf(stderr, "FATAL %s:%d " format "\n", \
                basename(__FILE__), __LINE__, ##__VA_ARGS__); \
        exit(EXIT_FAILURE); \
    } while (0)

#define VSOCK_LOG_INFO(format, ...) \
    do { \
        syslog(LOG_INFO, "INFO %s:%d " format, \
               basename(__FILE__), __LINE__, ##__VA_ARGS__); \
    } while (0)

/* Utility functions */
static inline int parse_integer(const char *str)
{
    char *endptr;
    long value;
    
    if (!str) {
        return 0;
    }
    
    value = strtol(str, &endptr, 10);
    
    if (*endptr != '\0' || value < 0 || value > INT_MAX) {
        VSOCK_LOG_FATAL("Invalid integer: %s", str);
    }
    
    return (int)value;
}

static inline int ip_string_to_int(int *inet_addr, const char *ip_string)
{
    int a, b, c, d;
    
    if (!ip_string || !inet_addr) {
        return -1;
    }
    
    if (sscanf(ip_string, "%d.%d.%d.%d", &a, &b, &c, &d) != 4) {
        return -1;
    }
    
    if (a < 0 || a > 255 || b < 0 || b > 255 || 
        c < 0 || c > 255 || d < 0 || d > 255) {
        return -1;
    }
    
    *inet_addr = (a << 24) | (b << 16) | (c << 8) | d;
    return 0;
}

/* Suppress unused parameter warnings */
#define UNUSED(x) (void)(x)

#endif /* VSOCK_SHELL_COMMON_H */
