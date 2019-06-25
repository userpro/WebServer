#ifndef SERVER_H_
#define SERVER_H_

int server_init(int port, int request_queue_length);
void server_handle(int sock_fd);
void server_worker(int worker_number);
void server_signal(int signo);
int server_fastcgi();
int server_start();

#endif  // SERVER_H_