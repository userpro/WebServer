#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "./vector/qwq_hashmap.h"
#include "./vector/qwq_string.h"
#include "./vector/qwq_utils.h"

#include "config.h"
#include "fastcgi.h"
#include "log.h"
#include "utils.h"

#include <netinet/in.h>
#include <sys/socket.h>

#define FCGI_VERSION_1 1      //版本号
#define FCGI_HEADER_LENGTH 8  // 消息头长度固定为8个字节
#define BUFF_SIZE 64
#ifndef FASTCGI_IP
#define FASTCGI_IP INADDR_ANY
#endif
#ifndef FASTCGI_PORT
#define FASTCGI_PORT 28888
#endif

#define GET_PARAMS_DELIMITER "="
#define GET_PARAMS_LINE_DELIMITER "&"

// 消息类型
enum fcgi_request_type {
    FCGI_BEGIN_REQUEST = 1,
    FCGI_ABORT_REQUEST = 2,
    FCGI_END_REQUEST = 3,
    FCGI_PARAMS = 4,
    FCGI_STDIN = 5,
    FCGI_STDOUT = 6,
    FCGI_STDERR = 7,
    FCGI_DATA = 8,
    FCGI_GET_VALUES = 9,
    FCGI_GET_VALUES_RESULT = 10,
    FCGI_UNKOWN_TYPE = 11
};

// 服务器希望fastcgi程序充当的角色
enum fcgi_role { FCGI_RESPONDER = 1, FCGI_AUTHORIZER = 2, FCGI_FILTER = 3 };

// 消息头
typedef struct _fcgi_header {
    unsigned char version;
    unsigned char type;
    unsigned char request_id_b1;
    unsigned char request_id_b0;
    unsigned char content_length_b1;
    unsigned char content_length_b0;
    unsigned char padding_length;
    unsigned char reserved;
} fcgi_header;

// 请求开始发送的消息体
typedef struct _fcgi_begin_request_body {
    unsigned char role_b1;
    unsigned char role_b0;
    unsigned char flags;
    unsigned char reserved[5];
} fcgi_begin_request_body;

// 请求结束发送的消息体
typedef struct _fcgi_end_request_body {
    unsigned char app_status_b3;
    unsigned char app_status_b2;
    unsigned char app_status_b1;
    unsigned char app_status_b0;
    unsigned char protocol_status;
    unsigned char reserved[3];
} fcgi_end_request_body;

// protocol_status
enum protocol_status {
    FCGI_REQUEST_COMPLETE = 0,
    FCGI_CANT_MPX_CONN = 1,
    FCGI_OVERLOADED = 2,
    FCGI_UNKNOWN_ROLE = 3
};

static void* qwq_hmap_kv_to_fcgi_params(qwq_hashmap_kv* kv,
                                        void** func_params) {
    if (!func_params) {
        return NULL;
    }
    qwq_string* qwq_str = (qwq_string*) *func_params;
    if (kv->key->type != QWQ_STRING || kv->val->type != QWQ_STRING) {
        return NULL;
    }
    int32_t key_len = -1, val_len = -1;
    qwq_string *key_data = (qwq_string*) kv->key->data,
               *val_data = (qwq_string*) kv->val->data;
    key_len = key_data->length, val_len = val_data->length;
    // 插入key长度信息
    if (key_len >= 0x80) {
        key_len |= 0x80000000;  // 最高位标志4字节
        if (qwq_string_cat1(qwq_str, (const char*) &key_len, 4) < 0) {
            return NULL;
        }
    } else {
        char data = (char) key_len;
        if (qwq_string_cat1(qwq_str, &data, 1) < 0) {
            return NULL;
        }
    }
    // 插入val长度信息
    if (val_len >= 0x80) {
        val_len |= 0x80000000;  // 最高位标志4字节
        if (qwq_string_cat1(qwq_str, (const char*) &val_len, 4) < 0) {
            return NULL;
        }
    } else {
        char data = (char) val_len;
        if (qwq_string_cat1(qwq_str, &data, 1) < 0) {
            return NULL;
        }
    }

    // 插入key数据
    if (qwq_string_cat2(qwq_str, key_data) < 0) {
        return NULL;
    }
    // 插入val数据
    if (qwq_string_cat2(qwq_str, val_data) < 0) {
        return NULL;
    }
    return (void*) qwq_str;
}

static qwq_string* qwq_hmap_to_fcgi_params(qwq_hashmap* qmap) {
    qwq_string* qwq_str = qwq_string_new1();
    qwq_hashmap_iter_kv(qmap, qwq_hmap_kv_to_fcgi_params, (void*) &qwq_str);
    return qwq_str;
}

qwq_fastcgi* qwq_fastcgi_new() {
    qwq_fastcgi* qwq = (qwq_fastcgi*) malloc(sizeof(qwq_fastcgi));
    if (!qwq) {
        return NULL;
    }
    qwq->std_in = qwq->std_out = NULL;
    qwq->data = NULL;
    qwq->params = NULL;
    return qwq;
}

void qwq_fastcgi_clear(qwq_fastcgi* qwq) {
    memset(&qwq->header, 0, sizeof(qwq->header));
    memset(&qwq->bg_body, 0, sizeof(qwq->bg_body));
    if (qwq->std_in) qwq_string_clear(qwq->std_in);
    if (qwq->std_out) qwq_string_clear(qwq->std_out);
    if (qwq->data) qwq_string_clear(qwq->data);
    if (qwq->params) qwq_hashmap_clear(qwq->params);
}

void qwq_fastcgi_destroy(qwq_fastcgi* qwq) {
    if (qwq->std_in) qwq_string_destroy(qwq->std_in);
    if (qwq->std_out) qwq_string_destroy(qwq->std_out);
    if (qwq->data) qwq_string_destroy(qwq->data);
    if (qwq->params) qwq_hashmap_destroy(qwq->params);
    free(qwq);
}

static int parse_fcgi_header(int fd, qwq_fastcgi* qwq) {
    char header_buffer[FCGI_HEADER_LENGTH * 2] = {0};
    // 读取消息头
    int nread;
    nread = readn(fd, header_buffer, FCGI_HEADER_LENGTH);
    if (nread <= 0) {
        return -1;
    }

    fcgi_header* header = (fcgi_header*) header_buffer;

    qwq->header.type = header->type;
    qwq->header.request_id =
            (header->request_id_b1 << 8) + header->request_id_b0;
    qwq->header.content_length =
            (header->content_length_b1 << 8) + header->content_length_b0;
    qwq->header.padding_length = header->padding_length;
    return 0;
}

static int parse_fcgi_begin_request(int fd, qwq_fastcgi* qwq) {
    int read_buf[BUFF_SIZE] = {0};
    int nread;
    fcgi_begin_request_body* tmp_bgbody = NULL;

    //读取开始请求的请求体
    nread = readn(fd, read_buf, sizeof(fcgi_begin_request_body));
    if (nread <= 0) {
        return -1;
    }

    tmp_bgbody = (fcgi_begin_request_body*) read_buf;
    qwq->bg_body.role = (tmp_bgbody->role_b1 << 8) + tmp_bgbody->role_b0;
    qwq->bg_body.flags = tmp_bgbody->flags;
    memcpy(qwq->bg_body.reserved,
           tmp_bgbody->reserved,
           sizeof(tmp_bgbody->reserved));
    return 0;
}

static int parse_fcgi_params(int fd, qwq_fastcgi* qwq) {
    // LOGI("begin read params...\n");
    char read_buf[BUFF_SIZE];
    int content_len = qwq->header.content_length;

    // 消息头中的contentLen = 0 表明此类消息已发送完毕
    if (content_len == 0) {
        // LOGI("read params end...\n");
        return 0;
    }

    // 检测是存在hashmap
    if (!qwq->params) {
        qwq->params = qwq_hashmap_new1();
        if (!qwq->params) {
            LOGW("qwq_hashmap_new1 error.");
            return -1;
        }
    }
    // 循环读取键值对
    qwq_hashmap* qmap = qwq->params;
    qwq_string *key = NULL, *val = NULL;
    int nread, param_key_len, param_val_len, err;
    char c;
    err = 0;
    while (content_len > 0) {
        // LOGI("1 content-length: %d", content_len);
        /**
         * FCGI_PARAMS
         * 以键值对的方式传送，键和值之间没有'=',每个键值对之前会分别用1或4个字节来标识键和值的长度
         * 例如: \x0B\x02SERVER_PORT80\x0B\x0ESERVER_ADDR199.170.183.42
         * 上面的长度是十六进制的  \x0B = 11  正好为字符串 "SERVER_PORT"
         * 的长度， \x02 = 2 为字符串 "80" 的长度
         */

        //先读取一个字节，这个字节标识 paramName 的长度
        nread = readn(fd, &c, 1);
        content_len -= nread;
        if ((c & 0x80) !=
            0)  //如果 c 的值大于 128，则该 paramName 的长度用四个字节表示
        {
            nread = readn(fd, read_buf, 3);
            content_len -= nread;
            param_key_len = ((c & 0x7f) << 24) + (read_buf[0] << 16) +
                            (read_buf[1] << 8) + read_buf[2];
        } else {
            param_key_len = c;
        }

        // LOGI("3 content-length: %d, param_key_len: %d",
        //      content_len,
        //      param_key_len);

        // 同样的方式获取paramValue的长度
        nread = readn(fd, &c, 1);
        content_len -= nread;
        if ((c & 0x80) != 0) {
            nread = readn(fd, read_buf, 3);
            content_len -= nread;
            param_val_len = ((c & 0x7f) << 24) + (read_buf[0] << 16) +
                            (read_buf[1] << 8) + read_buf[2];
        } else {
            param_val_len = c;
        }

        // LOGI("4 content-length: %d, param_val_len: %d",
        //      content_len,
        //      param_val_len);

        //读取paramName
        key = qwq_string_new2(param_key_len);
        if (!key) {
            err = -1;
            break;
        }
        nread = readn(fd, key->data, param_key_len);
        key->length = nread;
        content_len -= nread;

        // LOGI("5 content-length: %d", content_len);

        //读取paramValue
        val = qwq_string_new2(param_val_len);
        if (!val) {
            err = -1;
            break;
        }
        nread = readn(fd, val->data, param_val_len);
        val->length = nread;
        content_len -= nread;

        // LOGI("6 content-length: %d", content_len);

        qwq_hashmap_key* nkey = qwq_string_make_hmap_key(key);
        qwq_hashmap_val* nval = qwq_string_make_hmap_val(val);
        err = qwq_hashmap_set(&qmap, nkey, nval);
        if (err == -2) {  // 重复 key
            err = qwq_hashmap_update(qmap, nkey, nval);
        }
        if (err < 0) {
            qwq_hashmap_del_key(nkey);
            qwq_hashmap_del_val(nval);
            LOGW("qwq_hashmap_set/update error = %d", err);
        }

        key = NULL;
        val = NULL;
    }

    if (qwq->header.padding_length > 0) {
        nread = readn(fd, read_buf, qwq->header.padding_length);
    }

    if (err < 0) {
        if (key) qwq_string_destroy(key);
        if (val) qwq_string_destroy(val);
        if (qmap) qwq_hashmap_destroy(qmap);
        qwq->params = NULL;
        return -1;
    }
    // LOGI("fcgi_parse_params end");

    qwq->params = qmap;
    return 0;
}

static int parse_fcgi_std_inout(int fd, qwq_fastcgi* qwq, qwq_string** data) {
    LOGI("begin read post...\n");
    char read_buf[BUFF_SIZE];
    int nread;

    int content_len = qwq->header.content_length;
    if (content_len == 0) {
        LOGI("read post end....\n");
        return 0;
    }

    if (!(*data)) {
        *data = qwq_string_new2(content_len);
        if (!*data) {
            LOGW("qwq_string_new2 error.");
            return -1;
        }
    }
    qwq_string* qwq_str = *data;

    if (content_len > 0) {
        while (content_len > 0) {
            if (content_len > BUFF_SIZE) {
                nread = readn(fd, read_buf, BUFF_SIZE);
            } else {
                nread = readn(fd, read_buf, content_len);
            }

            content_len -= nread;
            if (qwq_string_cat1(qwq_str, read_buf, nread) < 0) {
                qwq_string_destroy(qwq_str);
                *data = NULL;
                return -1;
            }
            // fwrite(read_buf, sizeof(char), nread, stdout);
        }
        // printf("\n");
    }

    if (qwq->header.padding_length > 0) {
        nread = readn(fd, read_buf, qwq->header.padding_length);
    }
    return 0;
}

static int parse_fcgi_stdin(int fd, qwq_fastcgi* qwq) {
    return parse_fcgi_std_inout(fd, qwq, &qwq->std_in);
}

static int parse_fcgi_stdout(int fd, qwq_fastcgi* qwq) {
    return parse_fcgi_std_inout(fd, qwq, &qwq->std_out);
}

static int parse_fcgi_data(int fd, qwq_fastcgi* qwq) {
    return parse_fcgi_stdin(fd, qwq);
}

static void fcgi_send(int connfd,
                      enum fcgi_request_type type,
                      qwq_string* data) {
    LOGI("type: %d", type);
    fcgi_header header;
    header.version = FCGI_VERSION_1;
    header.type = type;
    if (!data) {
        //回写一个空的 FCGI_[TYPE] 表明 该类型消息已发送结束
        header.content_length_b1 = 0;
        header.content_length_b0 = 0;
        header.padding_length = 0;
        writen(connfd, &header, sizeof(header));
        return;
    }
    char buffer[BUFF_SIZE];

    header.content_length_b1 = (data->length >> 8) & 0xff;
    header.content_length_b0 = data->length & 0xff;
    header.padding_length = (data->length % 8) > 0 ? 8 - (data->length % 8)
                                                   : 0;  // 让数据 8 字节对齐

    writen(connfd, &header, sizeof(header));   // 发送header
    writen(connfd, data->data, data->length);  // 发送数据

    if (header.padding_length > 0) {
        writen(connfd,
               buffer,
               header.padding_length);  //填充数据随便写什么，数据会被服务器忽略
    }
}

void fcgi_send_params(int connfd, qwq_fastcgi* fcgi) {
    qwq_string* data = qwq_hmap_to_fcgi_params(fcgi->params);
    fcgi_send(connfd, FCGI_PARAMS, data);
    qwq_string_destroy(data);
}

void fcgi_send_data(int connfd, qwq_fastcgi* fcgi) {
    fcgi_send(connfd, FCGI_DATA, fcgi->data);
}

void fcgi_send_stdin(int connfd, qwq_fastcgi* fcgi) {
    fcgi_send(connfd, FCGI_STDIN, fcgi->std_in);
}

void fcgi_send_stdout(int connfd, qwq_fastcgi* fcgi) {
    fcgi_send(connfd, FCGI_STDOUT, fcgi->std_out);
}

void fcgi_send_end(int connfd) {
    fcgi_send(connfd, FCGI_END_REQUEST, NULL);

    // fcgi_end_request_body end_req_body;
    // end_req_body.protocol_status = FCGI_REQUEST_COMPLETE;
    // writen(connfd, &end_req_body, sizeof(end_req_body));
}

void fcgi_recv(int connfd,
               void* (*response_handle)(int, qwq_fastcgi*, void** func_params),
               void** func_params) {
    // set_nonblock(connfd);
    qwq_fastcgi* fcgi = qwq_fastcgi_new();
    if (!fcgi) {
        LOGW("qwq_fastcgi_new error.");
        return;
    }
    int f_end = 0;
    while (1) {
        if (f_end) {
            break;
        }
        if (parse_fcgi_header(connfd, fcgi) < 0) {
            break;
        }

        LOGI("version = %d, type = %d, requestId = %d, contentLen = %d, "
             "paddingLength = %d\n",
             fcgi->header.version,
             fcgi->header.type,
             fcgi->header.request_id,
             fcgi->header.content_length,
             fcgi->header.padding_length);

        switch (fcgi->header.type) {
        case FCGI_BEGIN_REQUEST:
            LOGI("FCGI_BEGIN_REQUEST");
            if (parse_fcgi_begin_request(connfd, fcgi) < 0) {
                LOGW("parse_fcgi_begin_request error.");
            }
            break;

        case FCGI_PARAMS:
            LOGI("FCGI_PARAMS");
            if (parse_fcgi_params(connfd, fcgi) < 0) {
                LOGW("parse_fcgi_params error.");
            }
            break;

        case FCGI_STDIN:
            LOGI("FCGI_STDIN");
            if (parse_fcgi_stdin(connfd, fcgi) < 0) {
                LOGW("parse_fcgi_stdin error.");
            }
            break;

        case FCGI_DATA:
            LOGI("FCGI_DATA");
            if (parse_fcgi_data(connfd, fcgi) < 0) {
                LOGW("parse_fcgi_data error.");
            }
            break;

        case FCGI_STDOUT:
            LOGI("FCGI_STDOUT");
            if (parse_fcgi_stdout(connfd, fcgi) < 0) {
                LOGW("parse_fcgi_stdin error.");
            }
            break;

        case FCGI_END_REQUEST:
            LOGI("FCGI_END_REQUEST");
            if (response_handle) response_handle(connfd, fcgi, func_params);
            f_end = 1;
            break;
        }
    }
    qwq_fastcgi_destroy(fcgi);
    close(connfd);
}

int fcgi_listen() {
    struct sockaddr_in servaddr;
    int sock_fd, sock_len, ret;
    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd == -1) {
        LOGE("socket");
    }

    sock_len = sizeof(struct sockaddr_in);
    memset(&servaddr, 0, sock_len);

    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(FASTCGI_PORT);
    servaddr.sin_addr.s_addr = htonl(FASTCGI_IP);

    ret = bind(sock_fd, (struct sockaddr*) &servaddr, sock_len);
    if (ret == -1) {
        LOGE("bind");
    }

    ret = listen(sock_fd, 16);
    if (ret == -1) {
        LOGE("listen");
    }

    return sock_fd;
}

int fcgi_accpet(void* (*response_handle)(int, qwq_fastcgi*, void** func_params),
                void** func_params) {
    int servfd, connfd;
    servfd = fcgi_listen();
    set_nonblock(servfd);
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    while (1) {
        connfd = accept(
                servfd, (struct sockaddr*) &client_addr, &client_addr_len);
        if (connfd == -1) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            LOGW("accept");
            break;
        }

        fcgi_recv(connfd, response_handle, func_params);
    }

    close(servfd);

    return 0;
}

#if 0
int main() {
    fcgi_accpet(NULL);
    return 0;
}
#endif