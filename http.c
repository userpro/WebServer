#include <ctype.h>
#include <fcntl.h>
#include <string.h>

#include "./vector/qwq_hashmap.h"
#include "./vector/qwq_mix.h"
#include "./vector/qwq_string.h"
#include "./vector/qwq_utils.h"

#include "config.h"
#include "fastcgi.h"
#include "http.h"
#include "log.h"
#include "server.h"
#include "utils.h"

#include <sys/sendfile.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#define BUFF_SIZE 1024

static const char* wwwroot = "./www";
static const int wwwroot_len = 5;

static char* get_file_type(qwq_string* url) {
    if (qwq_string_has_suffix1(url, ".jpg", 4, 1) == 0)
        return "image/jpg";  // 查看路径最后一个.文件
    if (qwq_string_has_suffix1(url, ".png", 4, 1) == 0)
        return "image/png";  // 查看路径最后一个.文件
    return "text/html";
}

static int get_file_access(qwq_string* fp) {
    return access(qwq_string_to_cstr(fp), R_OK | F_OK)
                   ? 404
                   : 200;  // 查看文件路径是否可读，成功返回0
}

static void request_print(req_t* req) {
    LOGI("request_print");

    printf("method: ");
    qwq_string_print(req->method);
    printf("\nurl: ");
    qwq_string_print(req->url);
    printf("\nproto: ");
    qwq_string_print(req->proto);
    puts("");
    qwq_string* header =
            qwq_hashmap_to_qwq_string(req->header, ": ", 2, "\r\n", 2);
    qwq_string_print(header);
    qwq_string_destroy(header);
    puts("");
    qwq_string_print(req->data);
    puts("");
}

static int parse_request_first_line(req_t* req, qwq_string* qwq_str) {
    // LOGI("parse_request_first_line");
    int arr_len;
    qwq_string** qwq_str_arr = NULL;
    if (qwq_string_spilt2(qwq_str, ' ', &qwq_str_arr, &arr_len) < 0) {
        LOGW("qwq_string_split2 error.");
        return -1;
    }

    if (arr_len < 3) {
        LOGW("invalid http header.");
        return -1;
    }

    req->method = qwq_str_arr[0];
    req->url = qwq_str_arr[1];
    req->proto = qwq_str_arr[2];
    free(qwq_str_arr);
    return 0;
}

static int parse_request_line(req_t* req, qwq_string* qwq_str) {
    // LOGI("parse_request_line");
    int arr_len;
    qwq_string** qwq_str_arr = NULL;
    if (qwq_string_spilt2(qwq_str, ':', &qwq_str_arr, &arr_len) < 0) {
        LOGW("qwq_string_split2 error.");
        return -1;
    }

    if (arr_len < 2) {
        LOGW("invalid http header.");
        qwq_string_array_destroy(qwq_str_arr, arr_len);
        free(qwq_str_arr);
        return -1;
    }

    if (qwq_string_strip(qwq_str_arr[1]) < 0) {
        LOGW("qwq_string_strip error.");
        qwq_string_array_destroy(qwq_str_arr, arr_len);
        free(qwq_str_arr);
        return -1;
    }

    qwq_hashmap_key* nkey = qwq_string_make_hmap_key(qwq_str_arr[0]);
    qwq_hashmap_val* nval = qwq_string_make_hmap_val(qwq_str_arr[1]);

    int err = qwq_hashmap_set(&req->header, nkey, nval);
    // -2 是 key 重复
    if (err == -2) {
        err = qwq_hashmap_update(req->header, nkey, nval);
    }
    if (err) {
        qwq_hashmap_just_del_key(nkey);
        qwq_hashmap_just_del_val(nval);
        qwq_string_array_destroy(qwq_str_arr, arr_len);
        free(qwq_str_arr);
        LOGW("qwq_hashmap_set/update error = %d", err);
        return -1;
    }

    return 0;
}

static int parse_request(int client, req_t* req, res_t* res) {
    qwq_string* qwq_str = NULL;
    int nread, nrecv, content, first_line;
    char c;
    char read_buf[BUFF_SIZE];

    req->header = qwq_hashmap_new1();
    content = first_line = 0;
    while ((nread = readn(client, read_buf, BUFF_SIZE - 1)) > 0) {
        read_buf[nread] = '\0';
        if (!content) {  // 协议部分
            int line_len;
            char *start, *end;
            start = end = read_buf;
            while (start < read_buf + nread &&
                   (end = strchr(start, '\r')) != NULL) {
                // 检测 "\r\n" 结尾
                if (*(end + 1) == '\0') {
                    nrecv = recv(client, &c, 1, MSG_PEEK);
                    if ((nrecv > 0) && (c == '\n')) recv(client, &c, 1, 0);
                }
                line_len = end - start;

                // 正文内容开始
                if (line_len == 0) {
                    content = 1;
                    start = end + 2;
                    break;
                }

                qwq_str = qwq_string_new3(start, line_len);
                if (!qwq_str) {
                    LOGW("qwq_string_new3 error.");
                    continue;
                }
                // 打印输出
                // LOGI("line: %s", qwq_string_to_cstr(qwq_str));

                if (!first_line) {
                    first_line = 1;
                    if (parse_request_first_line(req, qwq_str) < 0) {
                        LOGW("parse_request_first_line error.");
                    }
                } else {
                    if (parse_request_line(req, qwq_str) < 0) {
                        LOGW("parse_request_line error.");
                    }
                }
                qwq_string_destroy(qwq_str);
                qwq_str = NULL;

                *end = '\0';
                *(end + 1) = '\0';
                start = end + 2;
            }
        } else {  // 数据部分
            qwq_str = qwq_string_new3(read_buf, nread);
            if (!qwq_str) {
                LOGW("qwq_string_new3 error.");
                continue;
            }
        }
    }
    req->data = qwq_str;

    if (!req->method) {
        return -1;
    }
    // 根据request内容 执行相应操作
    request_do(client, req, res);
    // request_print(req);
    return 0;
}

req_t* request_init() {
    req_t* req;
    req = (req_t*) malloc(sizeof(req_t));
    if (!req) {
        return NULL;
    }
    req->header = NULL;
    req->method = req->url = req->proto = req->data = NULL;
    return req;
}

void request_do(int client, req_t* req, res_t* res) {
    // CGI
    if (qwq_string_find1(req->url, "/cgi", 4) != -1) {
        response_fastcgi(client, req, res);
    } else {
        response_do(client, req, res);
    }
}

void request_free(req_t* req) {
    if (req->method) qwq_string_destroy(req->method);
    if (req->url) qwq_string_destroy(req->url);
    if (req->proto) qwq_string_destroy(req->proto);
    if (req->header) qwq_hashmap_destroy(req->header);
    if (req->data) qwq_string_destroy(req->data);
    free(req);
}

res_t* response_init(req_t* req) {
    res_t* res;
    res = (res_t*) malloc(sizeof(res_t));
    if (!res) {
        return NULL;
    }
    res->req = req;
    res->full_path = NULL;
    res->code = 404;
    res->file_type = NULL;
    res->header = NULL;
    res->data = NULL;
    return res;
}

void response_do(int client, req_t* req, res_t* res) {
    qwq_string* full_path = NULL;
    full_path = qwq_string_new1();
    if (!full_path) {
        LOGW("qwq_string_new1 error.");
        return;
    }
    if (qwq_string_cat1(full_path, wwwroot, wwwroot_len) < 0) {
        qwq_string_destroy(full_path);
        LOGW("qwq_string_cat1 error.");
        return;
    }
    if (qwq_string_cat2(full_path, req->url) < 0) {
        qwq_string_destroy(full_path);
        LOGW("qwq_string_cat1 error.");
        return;
    }
    if (qwq_string_has_suffix1(req->url, "/", 1, 0) == 0) {
        if (qwq_string_cat1(full_path, "index.html", 10) < 0) {
            qwq_string_destroy(full_path);
            LOGW("qwq_string_cat1 error.");
            return;
        }
    }
    res->full_path = full_path;
    res->code = get_file_access(res->full_path);
    res->file_type = get_file_type(req->url);

    // 响应结果返回客户端
    response_b(client, res);
}

static void* response_fastcgi_helper(int client,
                                     qwq_fastcgi* fcgi,
                                     void** func_params) {
    const char* result = qwq_string_to_cstr(fcgi->std_out);
    qwq_string_print(fcgi->std_out);
    int res = atoi(result);
    int* ret = (int*) *func_params;
    *ret = res;
    return NULL;
}

// GET  /cgi/application_name?params1=value1&params2=value2
// POST /cgi/
int response_fastcgi(int client, req_t* req, res_t* res) {
    LOGI("response_fastcgi");
    int fcgi_fd = server_fastcgi();
    if (fcgi_fd == -1) {
        LOGW("fastcgi connect error.");
        return -1;
    }

    qwq_fastcgi* fcgi_req = NULL;
    fcgi_req = qwq_fastcgi_new();
    if (!fcgi_req) {
        close(fcgi_fd);
        return -1;
    }
    // LOGI("qwq_fastcgi_new");

    // 发送fastcgi请求
    if (qwq_string_cmp1(req->method, "GET", 3, 1) == 0) {
        // LOGI("fastcgi GET");
        qwq_string* params = NULL;
        params = qwq_string_get_suffix(req->url, '?');
        if (!params) {
            LOGW("qwq_string_get_suffix error.");
            close(fcgi_fd);
            return -1;
        }

        qwq_hashmap* qmap = NULL;
        qmap = qwq_hashmap_new1();
        if (!qmap) {
            close(fcgi_fd);
            return -1;
        }

        // step1: a=1&b=2&c=3 -> [a=1, b=2, c=3]
        // LOGI("step 1");
        int params_len;
        qwq_string** params_arr = NULL;
        if (qwq_string_spilt2(params, '&', &params_arr, &params_len) < 0) {
            qwq_string_destroy(params);
            close(fcgi_fd);
            return -1;
        }

        // step2: a=1 -> { a : 1 }
        // LOGI("step 2");
        int i, err, params_kv_len;
        for (i = 0; i < params_len; i++) {
            qwq_string** params_kv_arr = NULL;
            if (qwq_string_spilt2(
                        params_arr[i], '=', &params_kv_arr, &params_kv_len) <
                0) {
                qwq_string_array_destroy(params_arr, params_len);
                free(params_arr);
                break;
            }
            qwq_hashmap_key* key = qwq_string_make_hmap_key(params_kv_arr[0]);
            qwq_hashmap_val* val = qwq_string_make_hmap_val(params_kv_arr[1]);
            if (!key || !val) {
                qwq_string_array_destroy(params_arr, params_len);
                free(params_arr);
                free(params_kv_arr);
                break;
            }

            err = qwq_hashmap_set(&qmap, key, val);
            if (err == -2) {
                err = qwq_hashmap_update(qmap, key, val);
            }
            if (err) {
                qwq_hashmap_just_del_key(key);
                qwq_hashmap_just_del_val(val);
                qwq_string_array_destroy(params_arr, params_len);
                free(params_arr);
                free(params_kv_arr);
                LOGW("qwq_hashmap error.");
                break;
            }
            free(params_kv_arr);
        }
        // LOGI("clear step 1");
        // 清除 step1
        qwq_string_array_destroy(params_arr, params_len);
        free(params_arr);

        if (qwq_hashmap_merge(qmap, req->header) < 0) {
            LOGW("qwq_hashmap_merge error.");
            return -1;
        }

        fcgi_req->params = qmap;
        fcgi_send_params(fcgi_fd, fcgi_req);
        fcgi_send_end(fcgi_fd);
        qwq_string_destroy(params);

    } else if (qwq_string_cmp1(req->method, "POST", 4, 1) == 0) {
        fcgi_req->data = req->data;
        fcgi_send_data(fcgi_fd, fcgi_req);
        fcgi_send_end(fcgi_fd);
    } else {
        response_b_unimplement(client, res);
        qwq_fastcgi_destroy(fcgi_req);
        close(fcgi_fd);
        return 0;
    }
    // LOGI("send fastcgi request");

    // 接收数据并发送会客户端
    int* result = (int*) malloc(sizeof(int));
    fcgi_recv(fcgi_fd, response_fastcgi_helper, (void**) &result);
    LOGI("result: %d", *result);
    char result_str[10];
    sprintf(result_str, "%d", *result);
    response_b_200_content(client, res, result_str, strlen(result_str));
    free(result);

    qwq_fastcgi_destroy(fcgi_req);
    close(fcgi_fd);
    return 0;
}

void response_free(res_t* res) {
    if (res->full_path) qwq_string_destroy(res->full_path);
    if (res->header) qwq_hashmap_destroy(res->header);
    if (res->data) qwq_string_destroy(res->data);
    free(res);
}

void response_b(int client, res_t* res) {
    switch (res->code) {
    case 200:
        response_b_200_file(client, res);
        break;
    case 404:
        response_b_404(client, res);
        break;
    default:
        response_b_bad_request(client, res);
        break;
    }
}

void response_b_200_content(int client,
                            res_t* res,
                            char* data,
                            long int data_len) {
    LOGI("response_b_200_content");
    char buf[BUFF_SIZE];

    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %ld\r\n", data_len);
    send(client, buf, strlen(buf), 0);

    // HTTP头发送完成
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    send(client, data, data_len, 0);
}

void response_b_200_file(int client, res_t* res) {
    LOGI("response_b_200_file");
    char buf[BUFF_SIZE];

    sprintf(buf, "%s 200 OK\r\n", qwq_string_to_cstr(res->req->proto));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n", res->file_type);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);

    // 设置Content-Length
    int file_handle = open(qwq_string_to_cstr(res->full_path), O_RDONLY);
    if (file_handle < 0) {
        LOGW("open error!");
        return;
    }

    struct stat fstatbuf;
    if (fstat(file_handle, &fstatbuf) < 0) {
        LOGW("fstat error!");
        return;
    }

    sprintf(buf, "Content-Length: %ld\r\n", fstatbuf.st_size);
    send(client, buf, strlen(buf), 0);

    // HTTP头发送完成
    strcpy(buf, "\r\n");
    send(client, buf, strlen(buf), 0);

    // 输出文件内容
    if (sendfile(client, file_handle, NULL, fstatbuf.st_size) < 0) {
        LOGW("sendfile error!");
    }
    close(file_handle);
    LOGI("response 200");
}

void response_b_404(int client, res_t* res) {
    char buf[BUFF_SIZE];
    char rep_404[] =
            "<HTML><TITLE>Not Found</TITLE>\r\n"
            "<BODY><P>404 Not Found<br>The server could not fulfill\r\n"
            "your request because the resource specified\r\n"
            "is unavailable or nonexistent.\r\n"
            "</BODY></HTML>\r\n";

    sprintf(buf, "%s 404 NOT FOUND\r\n", qwq_string_to_cstr(res->req->proto));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n", res->file_type);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %d\r\n", (int) strlen(rep_404));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "%s", rep_404);
    send(client, buf, strlen(buf), 0);
    LOGI("response 404");
}

void response_b_unimplement(int client, res_t* res) {
    char buf[BUFF_SIZE];
    char rep_unimpl[] =
            "<HTML><HEAD><TITLE>Method Not Implemented\r\n"
            "</TITLE></HEAD>\r\n"
            "<BODY><P>HTTP request method not supported.\r\n"
            "</BODY></HTML>\r\n";

    sprintf(buf,
            "%s 501 Method Not Implemented\r\n",
            qwq_string_to_cstr(res->req->proto));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: %s\r\n", res->file_type);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %d\r\n", (int) strlen(rep_unimpl));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "%s", rep_unimpl);
    send(client, buf, strlen(buf), 0);
    LOGI("response unimplement");
}

void response_b_cannot_execute(int client, res_t* res) {
    char buf[BUFF_SIZE];
    char rep_cannot_exec[] = "<P>Error prohibited CGI execution.\r\n";

    sprintf(buf,
            "%s 500 Internal Server Error\r\n",
            qwq_string_to_cstr(res->req->proto));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-type: %s\r\n", res->file_type);
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %d\r\n", (int) strlen(rep_cannot_exec));
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "%s", rep_cannot_exec);
    send(client, buf, strlen(buf), 0);
    LOGI("response cannot_execute");
}

void response_b_bad_request(int client, res_t* res) {
    char buf[BUFF_SIZE];
    char rep_bad_requset[] =
            "<P>Your browser sent a bad request, "
            "such as a POST without a Content-Length.\r\n";

    sprintf(buf, "%s 400 BAD REQUEST\r\n", qwq_string_to_cstr(res->req->proto));
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Content-type: %s\r\n", res->file_type);
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(client, buf, strlen(buf), 0);
    sprintf(buf, "Content-Length: %d\r\n", (int) strlen(rep_bad_requset));
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "\r\n");
    send(client, buf, sizeof(buf), 0);
    sprintf(buf, "%s", rep_bad_requset);
    send(client, buf, sizeof(buf), 0);
    LOGI("response bad_request");
}

void* do_it(void* data) {
    int sockfd = *(int*) data;
    free(data);

    req_t* request = NULL;
    res_t* response = NULL;
    request = request_init();
    response = response_init(request);
    parse_request(sockfd, request, response);

    request_free(request);
    response_free(response);
    close(sockfd);
    return (void*) 0;
}