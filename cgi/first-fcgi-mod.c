#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fastcgi.h"
#include "../log.h"
#include "../vendor/qwq_hashmap.h"
#include "../vendor/qwq_string.h"

void* first_resp_handle(int connfd, qwq_fastcgi* fcgi, void** func_params) {
    LOGI("first_resp_handle");
    qwq_hashmap* qmap = fcgi->params;
    qwq_string* q1 = qwq_string_new3("a", 1);
    qwq_string* q2 = qwq_string_new3("b", 1);
    qwq_hashmap_key* key1 = qwq_string_make_hmap_key(q1);
    qwq_hashmap_key* key2 = qwq_string_make_hmap_key(q2);
    if (qwq_hashmap_has(qmap, key1) != 0 && qwq_hashmap_has(qmap, key2) != 0) {
        LOGI("NOPPP 1");
        fcgi->std_out = qwq_string_new3("not find a or b", 15);
        fcgi_send_stdout(connfd, fcgi);
        fcgi_send_end(connfd);
        return NULL;
    }
    qwq_hashmap_val* val1 = (qwq_hashmap_val*) qwq_hashmap_get(qmap, key1);
    if (val1->type != QWQ_STRING) {
        LOGI("NOPPP 2");
        fcgi_send_end(connfd);
        return NULL;
    }

    qwq_hashmap_val* val2 = (qwq_hashmap_val*) qwq_hashmap_get(qmap, key2);
    if (val2->type != QWQ_STRING) {
        LOGI("NOPPP 3");
        fcgi_send_end(connfd);
        return NULL;
    }

    int a, b, c;
    a = atoi(qwq_string_to_cstr(val1->data));
    b = atoi(qwq_string_to_cstr(val2->data));
    c = a + b;
    char res[10];
    sprintf(res, "%d", c);
    LOGI("res: %s", res);
    fcgi->std_out = qwq_string_new3(res, strlen(res));
    fcgi_send_stdout(connfd, fcgi);
    fcgi_send_end(connfd);
    return NULL;
}

int main() {
    qwq_fastcgi* fcgi = qwq_fastcgi_new();
    fcgi_accpet(first_resp_handle, NULL);
    qwq_fastcgi_destroy(fcgi);
    return 0;
}