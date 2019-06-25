#ifndef UTILS_H_
#define UTILS_H_

#include <sys/types.h>

ssize_t readn(int fd, void* ptr, size_t n);
ssize_t writen(int fd, const void* ptr, size_t n);
int set_nonblock(int fd);

#endif