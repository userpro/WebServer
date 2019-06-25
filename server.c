#include "server.h"
#include "config.h"
#include "http.h"
#include "log.h"
#include "utils.h"

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define MAX_PROCESS 4
#define REQUEST_QUEUE_LENGTH 10
#define EPOLL_EVENTS_SZIE 128
#define EPOLL_WAIT_US 10

static sem_t sem_id;

static struct epoll_event events[EPOLL_EVENTS_SZIE];
static pid_t child_pids[MAX_PROCESS];

/**
 * 注册epoll事件
 *
 * @param epoll_fd  epoll句柄
 * @param fd        socket句柄
 * @param state     监听状态
 */
static int epoll_register(int epoll_fd, int fd, int state) {
    struct epoll_event event;
    event.events = state;
    event.data.fd = fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
}

/**
 * 注销epoll事件
 *
 * @param epoll_fd  epoll句柄
 * @param fd        socket句柄
 * @param state     监听状态
 */
static int epoll_cancel(int epoll_fd, int fd, int state) {
    struct epoll_event event;
    event.events = state;
    event.data.fd = fd;
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, &event);
}

void server_handle(int sock_fd) {
    int err;
    pthread_t ntid;
    int* params_sock_fd = (int*) malloc(sizeof(int));
    *params_sock_fd = sock_fd;
    err = pthread_create(&ntid, NULL, do_it, (void*) params_sock_fd);
    if (err != 0) {
        LOGW("pthread create error.");
        return;
    }

    err = pthread_detach(ntid);
    if (err != 0) {
        LOGW("pthread detach error.");
        return;
    }
}

int server_init(int port, int request_queue_length) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LOGE("[socket]");
    }

    int opt = 1;
    setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock_fd, (struct sockaddr*) &addr, sizeof(addr)) == -1) {
        LOGE("[bind]");
    }

    if (listen(sock_fd, request_queue_length) == -1) {
        LOGE("[listen]");
    }

    return sock_fd;
}

void server_worker(int worker_number) {
    int i;
    pid_t child_pid = -1;
    for (i = 0; i < worker_number; i++) {
        if ((child_pid = fork()) == 0) {
            // 子进程退出循环
            break;
        } else if (child_pid == -1) {
            LOGE("fork failed.");
        } else {
            LOGI("fork success, pid: %d", child_pid);
            child_pids[i] = child_pid;
        }
    }
}

void server_signal(int signo) {
    pid_t pid;
    int stat;
    while ((pid = waitpid(-1, &stat, WNOHANG)) > 0) {
        LOGI("child %d exit", pid);
    }
    return;
}

int server_fastcgi() {
    int sock_fd;
    struct sockaddr_in server;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        LOGW("socket failed.\n");
        return -1;
    }

    server.sin_family = AF_INET;
    server.sin_port = htons(FASTCGI_PORT);
    server.sin_addr.s_addr = htonl(FASTCGI_IP);

    if (connect(sock_fd, (struct sockaddr*) &server, sizeof(server)) < 0) {
        LOGW("connect failed.\n");
        return -1;
    }

    return sock_fd;
}

int server_start() {
    // 避免僵尸进程
    signal(SIGCHLD, &server_signal);

    if (sem_init(&sem_id, 1, 1) < 0) {
        LOGW("sem_init");
    }

    int listen_sock_fd = server_init(SERVER_PORT, REQUEST_QUEUE_LENGTH);
    set_nonblock(listen_sock_fd);

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        LOGE("[epoll_create]");
    }

    // 创建工作进程
    server_worker(MAX_PROCESS);

    if (epoll_register(epoll_fd, listen_sock_fd, EPOLLIN) == -1) {
        LOGW("epoll_ctl add failed.");
    }

    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    for (;;) {
        if (sem_trywait(&sem_id) != 0) {
            continue;
        }
        int nready = epoll_wait(epoll_fd, events, EPOLL_EVENTS_SZIE, -1);
        if (nready == -1) {
            LOGW("epoll_wait failed");
        }
        LOGI("epoll wait %d", nready);
        sem_post(&sem_id);

        int i;
        for (i = 0; i < nready; i++) {
            int sock_fd = events[i].data.fd;
            if (sock_fd == listen_sock_fd) {
                if (sem_trywait(&sem_id) == 0) {
                    int conn_sock_fd = accept(listen_sock_fd,
                                              (struct sockaddr*) &client_addr,
                                              &client_addr_len);

                    if (conn_sock_fd < 0) {
                        LOGW("accept error");
                        sem_post(&sem_id);
                        continue;
                    }
                    LOGI("Accept");
                    set_nonblock(conn_sock_fd);
                    if (epoll_register(epoll_fd,
                                       conn_sock_fd,
                                       EPOLLIN | EPOLLONESHOT) == -1) {
                        LOGW("epoll_ctl add failed.");
                    }
                    sem_post(&sem_id);
                }
            } else {
                LOGI("server_handle");
                server_handle(sock_fd);
            }
        }
    }
    if (epoll_cancel(epoll_fd, listen_sock_fd, EPOLLIN) == -1) {
        LOGW("epoll_ctl del failed.");
    }
    close(epoll_fd);
    close(listen_sock_fd);
    return 0;
}
