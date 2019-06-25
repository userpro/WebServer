#ifndef HTTP_1_H
#define HTTP_1_H

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "./vector/qwq_hashmap.h"
#include "./vector/qwq_string.h"

typedef struct _http_request {
    qwq_string *method, *url, *proto;
    qwq_hashmap* header;
    qwq_string* data;
} req_t;

typedef struct _http_response {
    req_t* req;
    qwq_string* full_path;
    int code;
    const char* file_type;
    qwq_hashmap* header;
    qwq_string* data;
} res_t;

req_t* request_init();
void request_do(int client, req_t* req, res_t* res);
void request_free(req_t* req);

res_t* response_init(req_t* req);
void response_do(int client, req_t* req, res_t* res);
int response_fastcgi(int client, req_t* req, res_t* res);
void response_free(res_t* res);

//业务处理
void* do_it(void* data);  //传递socket描述符

void response_b(int client, res_t* res);
void response_b_200_content(int client,
                            res_t* res,
                            char* data,
                            long int data_len);
void response_b_200_file(int client, res_t* res);
void response_b_404(int client, res_t* res);
void response_b_unimplement(int client, res_t* res);
void response_b_cannot_execute(int client, res_t* res);
void response_b_bad_request(int client, res_t* res);

#endif