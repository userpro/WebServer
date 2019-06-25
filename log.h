#ifndef LOG_H_
#define LOG_H_

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BASIC_LOG_FORMAT "[file: %s / func: %s / Line: %d / pid: %d]"

#define LOGI(format, ...)                       \
    do {                                        \
        fprintf(stdout,                         \
                BASIC_LOG_FORMAT " ",           \
                __FILE__,                       \
                __func__,                       \
                __LINE__,                       \
                getpid());                      \
        fprintf(stdout, format, ##__VA_ARGS__); \
        fprintf(stdout, "\n");                  \
    } while (0)

#define LOGW(format, ...)                           \
    do {                                            \
        fprintf(stderr,                             \
                BASIC_LOG_FORMAT " <warning: %s> ", \
                __FILE__,                           \
                __func__,                           \
                __LINE__,                           \
                getpid(),                           \
                strerror(errno));                   \
        fprintf(stderr, format, ##__VA_ARGS__);     \
        fprintf(stderr, "\n");                      \
    } while (0)

#define LOGE(format, ...)                         \
    do {                                          \
        fprintf(stderr,                           \
                BASIC_LOG_FORMAT " <error: %s> ", \
                __FILE__,                         \
                __func__,                         \
                __LINE__,                         \
                getpid(),                         \
                strerror(errno));                 \
        fprintf(stderr, format, ##__VA_ARGS__);   \
        fprintf(stderr, "\n");                    \
        exit(EXIT_FAILURE);                       \
    } while (0)

#endif  // LOG_H_