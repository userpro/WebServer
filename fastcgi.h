#ifndef FCGI_H_
#define FCGI_H_
#include "./vendor/qwq_hashmap.h"
#include "./vendor/qwq_mix.h"
#include "./vendor/qwq_string.h"

// 消息头(qwq)
typedef struct _qwq_fcgi_header
{
    unsigned char version;
    unsigned char type;
    unsigned int request_id;
    unsigned int content_length;
    unsigned char padding_length;
    unsigned char reserved;
} qwq_fcgi_header;

// 请求开始发送的消息体(qwq)
typedef struct _qwq_fcgi_begin_request_body
{
    unsigned int role;
    unsigned char flags;
    unsigned char reserved[5];
} qwq_fcgi_begin_request_body;

// 请求结束发送的消息体(qwq)
typedef struct _qwq_fcgi_end_request_body
{
    unsigned int app_status;
    unsigned char protocol_status;
    unsigned char reserved[3];
} qwq_fcgi_end_request_body;

typedef struct _qwq_fastcgi
{
    qwq_fcgi_header header;
    qwq_fcgi_begin_request_body bg_body;
    qwq_hashmap *params;
    qwq_string *std_in, *std_out;
    qwq_string *data;
} qwq_fastcgi;

qwq_fastcgi *qwq_fastcgi_new();
void qwq_fastcgi_clear(qwq_fastcgi *qwq);
void qwq_fastcgi_destroy(qwq_fastcgi *qwq);

void fcgi_send_params(int connfd, qwq_fastcgi *fcgi);
void fcgi_send_data(int connfd, qwq_fastcgi *fcgi);
void fcgi_send_stdin(int connfd, qwq_fastcgi *fcgi);
void fcgi_send_stdout(int connfd, qwq_fastcgi *fcgi);
void fcgi_send_end(int connfd);

void fcgi_recv(int connfd,
               void *(*response_handle)(int, qwq_fastcgi *, void **func_params),
               void **func_params);
int fcgi_listen();
int fcgi_accpet(void *(*response_handle)(int, qwq_fastcgi *, void **func_params),
                void **func_params);

#endif