#ifndef QWQ_STRING_H_
#define QWQ_STRING_H_

typedef struct _qwq_string {
    int length;
    int capacity;
    char* data;
} qwq_string;

qwq_string* cstr_to_qwq_string1(const char* s, int len);  // 栈上字符串
qwq_string* cstr_to_qwq_string2(char* s, int len);        // 堆上字符串
const char* qwq_string_to_cstr(qwq_string* qwq);

qwq_string* qwq_string_clone(qwq_string* qwq);
int qwq_string_clone_to_cstr(qwq_string* qwq, char* s, int len);
int qwq_string_resize(qwq_string* qwq, int cap);

qwq_string* qwq_string_new1();
qwq_string* qwq_string_new2(int capacity);
qwq_string* qwq_string_new3(const char* s, int len);

int qwq_string_cat1(qwq_string* dst, const char* src, int src_len);
int qwq_string_cat2(qwq_string* dst, qwq_string* src);
// ignore_case 非0开启
int qwq_string_cmp1(qwq_string* dst,
                    const char* src,
                    int src_len,
                    int ignore_case);
int qwq_string_cmp2(qwq_string* dst, qwq_string* src, int ignore_case);
int qwq_string_spilt1(const char* src,
                      int src_len,
                      const char delimiter,
                      qwq_string*** qwq_arr,
                      int* qwq_arr_len);
int qwq_string_spilt2(qwq_string* qwq,
                      const char delimiter,
                      qwq_string*** qwq_arr,
                      int* qwq_len);

int qwq_string_strip(qwq_string* qwq);
int qwq_string_find1(qwq_string* dst, const char* src, int src_len);
int qwq_string_find2(qwq_string* dst, qwq_string* src);
int qwq_string_has_suffix1(qwq_string* dst,
                           const char* src,
                           int src_len,
                           int ignore_case);
int qwq_string_has_suffix2(qwq_string* dst, qwq_string* src, int ignore_case);
int qwq_string_has_prefix1(qwq_string* dst,
                           const char* src,
                           int src_len,
                           int ignore_case);
int qwq_string_has_prefix2(qwq_string* dst, qwq_string* src, int ignore_case);
qwq_string* qwq_string_get_suffix(qwq_string* qwq, const char delimiter);
qwq_string* qwq_string_get_prefix(qwq_string* qwq, const char delimiter);

void qwq_string_clear(qwq_string* qwq);
// 回收动态分配的 qwq_string 数组(主要配合split2函数使用)
void qwq_string_array_destroy(qwq_string** qwq, int len);
void qwq_string_destroy(qwq_string* qwq);

unsigned long qwq_string_hash(qwq_string* qwq);
void qwq_string_print(qwq_string* qwq);

// 为了兼容hashmap
unsigned long qwq_string_hmap_hash(void* qwq_obj);
int qwq_string_hmap_cmp(void* dst, void* src);
void qwq_string_hmap_free(void* qwq_obj);

#endif