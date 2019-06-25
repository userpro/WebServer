#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qwq_string.h"
#include "qwq_utils.h"

#define DEFAULT_CAPACITY 8

static inline int _qwq_str_cmp(const char* s1,
                               const char* s2,
                               int len,
                               int ignore_case) {
    int i;
    if (ignore_case) {
        for (i = 0; i < len; i++) {
            if (tolower(s1[i]) != tolower(s2[i])) return -1;
        }
    } else {
        for (i = 0; i < len; i++) {
            if (s1[i] != s2[i]) return -1;
        }
    }
    return 0;
}

qwq_string* cstr_to_qwq_string1(const char* s, int len) {
    qwq_string* res = NULL;
    res = qwq_string_new2(qwq_utils_roundup_pow_of_two(len));
    if (!res) {
        return NULL;
    }

    if (qwq_string_cat1(res, s, len) < 0) {
        qwq_string_destroy(res);
        return NULL;
    }
    return res;
}

qwq_string* cstr_to_qwq_string2(char* s, int len) {
    qwq_string* res = NULL;
    res = (qwq_string*) malloc(sizeof(qwq_string));
    if (!res) {
        return NULL;
    }
    res->length = len + 1;
    res->capacity = res->length;
    res->data = s;
    return res;
}

inline const char* qwq_string_to_cstr(qwq_string* qwq) {
    return (const char*) qwq->data;
}

char* qwq_string_to_cstr2(qwq_string* qwq) {
    char* data;
    data = (char*) malloc(sizeof(char) * (qwq->length + 1));
    memcpy(data, qwq->data, qwq->length);
    return data;
}

qwq_string* qwq_string_new1() {
    return qwq_string_new2(DEFAULT_CAPACITY);
}

qwq_string* qwq_string_new2(int capacity) {
    qwq_string* res = NULL;
    res = (qwq_string*) malloc(sizeof(qwq_string));
    if (!res) {
        return NULL;
    }

    res->length = 0;
    res->capacity = capacity;
    if (capacity <= 0) {
        res->capacity = 0;
        res->data = NULL;
        return res;
    }
    res->data = (char*) malloc(sizeof(char) * res->capacity);
    if (!res->data) {
        free(res);
        return NULL;
    }
    res->data[0] = '\0';

    return res;
}

qwq_string* qwq_string_new3(const char* s, int len) {
    qwq_string* qwq = NULL;
    qwq = qwq_string_new2(len + 1);  // +1 for '\0'
    if (!qwq) {
        return NULL;
    }
    if (qwq_string_cat1(qwq, s, len) < 0) {
        qwq_string_destroy(qwq);
        return NULL;
    }
    return qwq;
}

qwq_string* qwq_string_clone(qwq_string* qwq) {
    qwq_string* nqwq = NULL;
    nqwq = qwq_string_new1();
    if (!nqwq) {
        return NULL;
    }
    if (qwq_string_cat2(nqwq, qwq) < 0) {
        qwq_string_destroy(nqwq);
        return NULL;
    }
    return nqwq;
}

int qwq_string_clone_to_cstr(qwq_string* qwq, char* s, int len) {
    if (qwq->length >= len) {
        return -1;
    }
    memcpy(s, qwq->data, len);
    s[len] = '\0';
    return 0;
}

int qwq_string_resize(qwq_string* qwq, int new_cap) {
    if (new_cap <= 0) {
        return -1;
    }
    if (new_cap == qwq->capacity) {
        return 0;
    }
    char* old_data = qwq->data;
    qwq->data = (char*) realloc(old_data, new_cap * sizeof(char));
    if (!old_data) {
        return -1;
    }

    qwq->capacity = new_cap;
    qwq->length = qwq->length > qwq->capacity ? qwq->capacity - 1 : qwq->length;
    qwq->data[qwq->length] = '\0';
    return 0;
}

int qwq_string_cat1(qwq_string* dst, const char* src, int src_len) {
    if (src_len < 0) {
        return -1;
    }
    if (src_len == 0) {
        return 0;
    }
    int total_len;
    total_len = dst->length + src_len;
    if (total_len + 1 > dst->capacity) {  // +1 for '\0'
        if (qwq_string_resize(
                    dst, qwq_utils_roundup_pow_of_two(total_len + 1)) < 0) {
            return -1;
        }
    }

    memcpy(dst->data + dst->length, src, src_len);
    dst->length = total_len;
    dst->data[dst->length] = '\0';
    return 0;
}

int qwq_string_cat2(qwq_string* dst, qwq_string* src) {
    return src->length == 0
                   ? 0
                   : qwq_string_cat1(dst, (const char*) src->data, src->length);
}

inline int qwq_string_cmp1(qwq_string* dst,
                           const char* src,
                           int src_len,
                           int ignore_case) {
    return (dst->length != src_len || dst->length == 0)
                   ? -1
                   : _qwq_str_cmp(dst->data, src, dst->length, ignore_case);
}

inline int qwq_string_cmp2(qwq_string* dst, qwq_string* src, int ignore_case) {
    return (dst->length != src->length || dst->length == 0)
                   ? -1
                   : _qwq_str_cmp(
                             dst->data, src->data, dst->length, ignore_case);
}

int qwq_string_spilt1(const char* src,
                      int src_len,
                      const char delimiter,
                      qwq_string*** qwq_arr,
                      int* qwq_arr_len) {
    qwq_string* qwq = cstr_to_qwq_string1(src, src_len);
    if (!qwq) {
        return -1;
    }

    int ret = qwq_string_spilt2(qwq, delimiter, qwq_arr, qwq_arr_len);
    qwq_string_destroy(qwq);
    return ret;
}

int qwq_string_spilt2(qwq_string* qwq,
                      const char delimiter,
                      qwq_string*** qwq_arr,
                      int* qwq_arr_len) {
    int i;
    // 计算有几个字串
    int src_len, delimiter_cnt, tmp_qwq_arr_cnt;
    delimiter_cnt = 0;
    src_len = qwq->length;
    for (i = 0; i < src_len; i++) {
        if (qwq->data[i] == delimiter) {
            delimiter_cnt++;
        }
    }
    tmp_qwq_arr_cnt = delimiter_cnt + 1;

    // 生成分割后字串数组
    qwq_string** tmp_qwq_arr = NULL;
    tmp_qwq_arr = (qwq_string**) calloc(tmp_qwq_arr_cnt, sizeof(qwq_string*));
    if (!tmp_qwq_arr) {
        return -1;
    }

    // 按delimiter分割成子字符串
    int st, tmp_qwq_arr_idx, err;
    int sub_str_len;
    st = 0;
    err = 0;
    tmp_qwq_arr_idx = 0;
    for (i = 0; i <= src_len; i++) {
        if (qwq->data[i] == delimiter || i == src_len) {
            sub_str_len = i - st;
            tmp_qwq_arr[tmp_qwq_arr_idx] =
                    qwq_string_new3(qwq->data + st, sub_str_len);
            if (!tmp_qwq_arr[tmp_qwq_arr_idx]) {
                err = 1;
                break;
            }

            st = i + 1;
            tmp_qwq_arr_idx++;
        }
    }

    if (err) {
        qwq_string_array_destroy(tmp_qwq_arr, tmp_qwq_arr_cnt);
        free(tmp_qwq_arr);
        return -1;
    }

    *qwq_arr = tmp_qwq_arr;
    *qwq_arr_len = tmp_qwq_arr_cnt;

    return 0;
}

int qwq_string_strip(qwq_string* qwq) {
    int len;
    char *s1 = NULL, *s2 = NULL;
    s1 = qwq->data;
    s2 = qwq->data + (qwq->length - 1);
    while (*s1 == ' ' && s1 <= s2) s1++;
    while (*s2 == ' ' && s1 <= s2) s2--;
    len = (s2 - s1) + 1;
    if (qwq->length == len) {
        return 0;
    }
    qwq->length = len;
    memmove(qwq->data, s1, qwq->length);
    qwq->data[qwq->length] = '\0';
    return 0;
}

int qwq_string_find1(qwq_string* dst, const char* src, int src_len) {
    int idx;
    qwq_string* qwq_s = NULL;
    qwq_s = cstr_to_qwq_string1(src, src_len);
    if (!qwq_s) {
        return -1;
    }
    idx = qwq_string_find2(dst, qwq_s);
    qwq_string_destroy(qwq_s);
    return idx;
}

int qwq_string_find2(qwq_string* dst, qwq_string* src) {
    const char *s1 = NULL, *s2 = NULL;
    char* res = NULL;
    int idx;
    if (src->length < 1) {
        return -1;
    }

    s1 = qwq_string_to_cstr(dst);
    s2 = qwq_string_to_cstr(src);

    if (src->length == 1) {
        res = strchr(s1, s2[0]);
    } else {
        res = strstr(s1, s2);
    }
    if (!res) {
        idx = -1;
    } else {
        idx = res - s1;
    }

    return idx;
}

inline int qwq_string_has_suffix1(qwq_string* dst,
                                  const char* src,
                                  int src_len,
                                  int ignore_case) {
    return dst->length < src_len
                   ? -1
                   : _qwq_str_cmp(
                             (const char*) (dst->data + dst->length - src_len),
                             src,
                             src_len,
                             ignore_case);
}
inline int qwq_string_has_suffix2(qwq_string* dst,
                                  qwq_string* src,
                                  int ignore_case) {
    return dst->length < src->length
                   ? -1
                   : _qwq_str_cmp((const char*) (dst->data + dst->length -
                                                 src->length),
                                  (const char*) src->data,
                                  src->length,
                                  ignore_case);
}
inline int qwq_string_has_prefix1(qwq_string* dst,
                                  const char* src,
                                  int src_len,
                                  int ignore_case) {
    return dst->length < src_len ? -1
                                 : _qwq_str_cmp((const char*) dst->data,
                                                (const char*) src,
                                                src_len,
                                                ignore_case);
}
inline int qwq_string_has_prefix2(qwq_string* dst,
                                  qwq_string* src,
                                  int ignore_case) {
    return dst->length < src->length ? -1
                                     : _qwq_str_cmp((const char*) dst->data,
                                                    (const char*) src->data,
                                                    src->length,
                                                    ignore_case);
}

qwq_string* qwq_string_get_suffix(qwq_string* qwq, const char delimiter) {
    char* idx = NULL;
    idx = qwq->data + qwq->length - 1;
    while (idx >= 0 && *idx != delimiter) idx--;
    if (idx < 0) {
        return NULL;
    }
    idx++;  // 不包括delimiter
    int len = qwq->length - (idx - qwq->data);
    qwq_string* res;
    res = qwq_string_new2(len);
    if (!res) {
        return NULL;
    }
    if (qwq_string_cat1(res, idx, len) < 0) {
        qwq_string_destroy(res);
        return NULL;
    }
    return res;
}
qwq_string* qwq_string_get_prefix(qwq_string* qwq, const char delimiter) {
    char *idx = NULL, *end = NULL;
    idx = qwq->data;
    end = qwq->data + qwq->length;
    while (idx < end && *idx != delimiter) idx++;
    if (idx >= end) {
        return NULL;
    }
    idx--;  // 不包括delimiter
    int len = idx - qwq->data + 1;
    qwq_string* res;
    res = qwq_string_new2(len);
    if (!res) {
        return NULL;
    }
    if (qwq_string_cat1(res, qwq->data, len) < 0) {
        qwq_string_destroy(res);
        return NULL;
    }
    return res;
}

inline void qwq_string_clear(qwq_string* qwq) {
    qwq->length = 0;
}

inline void qwq_string_array_destroy(qwq_string** qwq_arr, int len) {
    int i;
    for (i = 0; i < len; i++) {
        if (qwq_arr[i]) qwq_string_destroy(qwq_arr[i]);
    }
}

inline void qwq_string_destroy(qwq_string* qwq) {
    free(qwq->data);
    free(qwq);
}

unsigned long qwq_string_hash(qwq_string* qwq) {
    unsigned long h = 0, g;
    char* arKey = qwq->data;
    char* arEnd = qwq->data + qwq->length;

    while (arKey < arEnd) {
        h = (h << 4) + *arKey++;
        if ((g = (h & 0xF0000000))) {
            h = h ^ (g >> 24);
            h = h ^ g;
        }
    }

    return h;
}

void qwq_string_print(qwq_string* qwq) {
    if (!qwq) {
        return;
    }
    printf("%s", qwq_string_to_cstr(qwq));
}

/* 为了兼容hashmap */

unsigned long qwq_string_hmap_hash(void* qwq_obj) {
    return qwq_string_hash((qwq_string*) qwq_obj);
}

int qwq_string_hmap_cmp(void* dst, void* src) {
    return qwq_string_cmp2((qwq_string*) dst, (qwq_string*) src, 0);
}

void qwq_string_hmap_free(void* qwq_obj) {
    qwq_string_destroy((qwq_string*) qwq_obj);
}

#undef DEFAULT_CAPACITY