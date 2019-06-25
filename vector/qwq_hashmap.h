#ifndef QWQ_HASHMAP_H_
#define QWQ_HASHMAP_H_

enum HASHMAP_TYPE {
    QWQ_STRING = 1,
    QWQ_OBJECT = 2,
    QWQ_UNKNOWN = -1,
};

typedef struct _qwq_hashmap_key {
    enum HASHMAP_TYPE type;
    void* data;
    unsigned long (*hashcode)(void*);
    int (*cmp)(void*, void*);
    void (*free)(void*);
} qwq_hashmap_key;

typedef struct _qwq_hashmap_val {
    enum HASHMAP_TYPE type;
    void* data;
    void (*free)(void*);
} qwq_hashmap_val;

typedef struct _qwq_hashmap_kv {
    int index;
    qwq_hashmap_key* key;
    qwq_hashmap_val* val;
    struct _qwq_hashmap_kv *prev, *next, *collision_next;
} qwq_hashmap_kv;

typedef struct _qwq_hashmap {
    int count;
    int capacity;
    int threshold;
    qwq_hashmap_kv **data, *head;
} qwq_hashmap;

/* 所有函数都不会修改或释放传入的内存 */

qwq_hashmap_key* qwq_hashmap_make_key(enum HASHMAP_TYPE type,
                                      void* data,
                                      unsigned long (*hashcode)(void*),
                                      int (*cmp)(void*, void*),
                                      void (*free)(void*));

qwq_hashmap_val* qwq_hashmap_make_val(enum HASHMAP_TYPE type,
                                      void* data,
                                      void (*free)(void*));

// 删除管理结果及数据
void qwq_hashmap_del_key(qwq_hashmap_key* qwq_key);
void qwq_hashmap_del_val(qwq_hashmap_val* qwq_val);

// 仅释放对应的数据结构 不释放其持有的数据 也不释放下一级数据结构
void qwq_hashmap_just_del_key(qwq_hashmap_key* key);
void qwq_hashmap_just_del_val(qwq_hashmap_val* val);

void qwq_hashmap_iter_kv(qwq_hashmap* qmap,
                         void* (*func)(qwq_hashmap_kv*, void** func_params),
                         void** func_params);
qwq_hashmap* qwq_hashmap_new1();
qwq_hashmap* qwq_hashmap_new2(int capacity);
int qwq_hashmap_set(qwq_hashmap** _qmap,
                    qwq_hashmap_key* key,
                    qwq_hashmap_val* val);

void* qwq_hashmap_get(qwq_hashmap* qmap, qwq_hashmap_key* key);

int qwq_hashmap_update(qwq_hashmap* qmap,
                       qwq_hashmap_key* key,
                       qwq_hashmap_val* new_val);

int qwq_hashmap_delete(qwq_hashmap* qmap, qwq_hashmap_key* key);
int qwq_hashmap_merge(qwq_hashmap* dst, qwq_hashmap* src);
int qwq_hashmap_has(qwq_hashmap* qmap, qwq_hashmap_key* key);

int qwq_hashmap_clear(qwq_hashmap* qmap);
void qwq_hashmap_destroy(qwq_hashmap* qmap);

#endif