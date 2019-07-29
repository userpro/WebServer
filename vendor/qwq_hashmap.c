#include <stdlib.h>

#include "qwq_hashmap.h"
#include "qwq_utils.h"

/*
    kv
   /  \
  /    \
key    val
 |      |
 |      |
data   data
 */

#define DEFAULT_CAPACITY 8
#define MAX_LOAD_THRESHOLD(cap) (cap >> 1)

static qwq_hashmap_kv* qwq_hashmap_new_kv(qwq_hashmap_key* key,
                                          qwq_hashmap_val* val) {
    qwq_hashmap_kv* qwq_kv = (qwq_hashmap_kv*) malloc(sizeof(qwq_hashmap_kv));
    if (!qwq_kv) {
        return NULL;
    }
    qwq_kv->index = -1;
    qwq_kv->key = key;
    qwq_kv->val = val;
    qwq_kv->prev = qwq_kv->next = qwq_kv->collision_next = NULL;
    return qwq_kv;
}

// 释放kv及数据
inline void qwq_hashmap_del_key(qwq_hashmap_key* qwq_key) {
    if (qwq_key) return;
    if (qwq_key->free) qwq_key->free(qwq_key->data);
    free(qwq_key);
}

inline void qwq_hashmap_del_val(qwq_hashmap_val* qwq_val) {
    if (qwq_val) return;
    if (qwq_val->free) qwq_val->free(qwq_val->data);
    free(qwq_val);
}

static inline void* qwq_hashmap_del_kv(qwq_hashmap_kv* qwq_kv,
                                       void** func_params) {
    if (!qwq_kv) return NULL;
    if (qwq_kv->key) qwq_hashmap_del_key(qwq_kv->key);
    if (qwq_kv->val) qwq_hashmap_del_val(qwq_kv->val);
    free(qwq_kv);
    return NULL;
}

inline void qwq_hashmap_just_del_key(qwq_hashmap_key* key) {
    if (key) free(key);
}
inline void qwq_hashmap_just_del_val(qwq_hashmap_val* val) {
    if (val) free(val);
}

static inline void* qwq_hashmap_just_del_kv(qwq_hashmap_kv* kv,
                                            void** func_params) {
    if (kv) free(kv);
    return NULL;
}

static int qwq_hashmap_resize(qwq_hashmap** old_qwq, int new_cap) {
    // LOGI("resize %d", new_cap);
    if (new_cap < (*old_qwq)->capacity) {
        return -1;
    }
    if (new_cap == (*old_qwq)->capacity) {
        return 0;
    }
    qwq_hashmap* new_qwq = NULL;
    new_qwq = qwq_hashmap_new2(new_cap);
    if (!new_qwq) {
        return -1;
    }

    qwq_hashmap_kv *kv_list = NULL, *tmp_kv = NULL;
    kv_list = (*old_qwq)->head;
    while (kv_list) {
        tmp_kv = kv_list;
        kv_list = kv_list->next;

        if (qwq_hashmap_set(&new_qwq, tmp_kv->key, tmp_kv->val) < 0) {
            qwq_hashmap_iter_kv(new_qwq, qwq_hashmap_just_del_kv, NULL);
            free(new_qwq->data);
            free(new_qwq);
            return -1;
        }
    }
    qwq_hashmap_iter_kv(*old_qwq, qwq_hashmap_just_del_kv, NULL);
    free((*old_qwq)->data);
    free((*old_qwq));
    *old_qwq = new_qwq;
    return 0;
}

qwq_hashmap_key* qwq_hashmap_make_key(enum HASHMAP_TYPE type,
                                      void* data,
                                      unsigned long (*hashcode)(void*),
                                      int (*cmp)(void*, void*),
                                      void (*free)(void*)) {
    qwq_hashmap_key* key = NULL;
    key = (qwq_hashmap_key*) malloc(sizeof(qwq_hashmap_key));
    if (!key) {
        return NULL;
    }
    key->type = type;
    key->data = data;
    key->hashcode = hashcode;
    key->cmp = cmp;
    key->free = free;
    return key;
}

qwq_hashmap_val* qwq_hashmap_make_val(enum HASHMAP_TYPE type,
                                      void* data,
                                      void (*free)(void*)) {
    qwq_hashmap_val* val = NULL;
    val = (qwq_hashmap_val*) malloc(sizeof(qwq_hashmap_val));
    if (!val) {
        return NULL;
    }
    val->type = type;
    val->data = data;
    val->free = free;
    return val;
}

void qwq_hashmap_iter_kv(qwq_hashmap* qmap,
                         void* (*func)(qwq_hashmap_kv*, void** func_params),
                         void** func_params) {
    qwq_hashmap_kv *kv_list = NULL, *tmp_kv = NULL;
    kv_list = qmap->head;
    while (kv_list) {
        tmp_kv = kv_list;
        kv_list = kv_list->next;
        func(tmp_kv, func_params);
    }
}

qwq_hashmap* qwq_hashmap_new1() {
    return qwq_hashmap_new2(DEFAULT_CAPACITY);
}

qwq_hashmap* qwq_hashmap_new2(int capacity) {
    qwq_hashmap* qwq = NULL;
    qwq = (qwq_hashmap*) malloc(sizeof(qwq_hashmap));
    if (!qwq) {
        return NULL;
    }
    qwq->count = 0;
    qwq->capacity = capacity;
    qwq->threshold = MAX_LOAD_THRESHOLD(qwq->capacity);
    qwq->head = NULL;
    qwq->data =
            (qwq_hashmap_kv**) calloc(sizeof(qwq_hashmap_kv*), qwq->capacity);
    if (!qwq->data) {
        free(qwq);
        return NULL;
    }
    return qwq;
}

/*
    成功 => 0
    失败 => -1
    重复 => -2
*/
int qwq_hashmap_set(qwq_hashmap** _qmap,
                    qwq_hashmap_key* key,
                    qwq_hashmap_val* val) {
    // LOGI("qwq_hashmap_set");
    if (!_qmap || !key || !val) {
        return -1;
    }
    if (!key->hashcode || !key->cmp) {
        return -1;
    }
    qwq_hashmap* qmap = *_qmap;
    if (!qmap) {
        return -1;
    }

    int hash_idx, final_idx;
    hash_idx = key->hashcode(key->data) % qmap->capacity;
    // LOGI("capacity: %d  hash_idx: %d", qmap->capacity, hash_idx);

    qwq_hashmap_kv* new_qwq_kv = NULL;
    new_qwq_kv = qwq_hashmap_new_kv(key, val);
    if (!new_qwq_kv) {
        return -1;
    }

    // 没有发生冲突
    if (!qmap->data[hash_idx]) {
        final_idx = hash_idx;
        new_qwq_kv->index = final_idx;
        qmap->data[final_idx] = new_qwq_kv;
    } else {           // 发生冲突
        int tail_idx;  // 碰撞链最后一个
        // 查询 collision list
        qwq_hashmap_kv* nx_kv = NULL;
        nx_kv = qmap->data[hash_idx];
        while (nx_kv) {
            // 如果找到重复 key, 则返回重复 key 值错误
            if (key->cmp(key->data, nx_kv->key->data) == 0) {
                qwq_hashmap_just_del_kv(new_qwq_kv, NULL);
                return -2;
            }
            tail_idx = nx_kv->index;
            nx_kv = nx_kv->collision_next;
        }

        // LOGI("tail_idx: %d", tail_idx);
        final_idx = (tail_idx + 1) % qmap->capacity;
        // 从最后发生碰撞的地方向后寻找
        while (qmap->data[final_idx] && final_idx != hash_idx) {
            final_idx = (final_idx + 1) % qmap->capacity;
        }
        new_qwq_kv->index = final_idx;
        qmap->data[final_idx] = new_qwq_kv;
        qmap->data[tail_idx]->collision_next = new_qwq_kv;
    }

    // 维护 qwq_hashmap_kv list
    QWQ_DLINKLIST_INS_FRT(qmap->head, qmap->data[final_idx]);

    qmap->count++;
    if (qmap->count > qmap->threshold) {
        if (qwq_hashmap_resize(
                    _qmap, qwq_utils_roundup_pow_of_two(qmap->capacity)) < 0) {
            free(new_qwq_kv);
            return -1;
        }
    }

    return 0;
}

void* qwq_hashmap_get(qwq_hashmap* qmap, qwq_hashmap_key* key) {
    if (!qmap || qmap->count <= 0) {
        return NULL;
    }
    if (!key || !key->hashcode || !key->cmp) {
        return NULL;
    }

    int hash_idx = key->hashcode(key->data) % qmap->capacity;
    if (!qmap->data[hash_idx]) {
        return NULL;
    }

    qwq_hashmap_kv* nx_kv = qmap->data[hash_idx];
    while (nx_kv) {
        // 如果找到相同 key, 则返回 val
        if (key->cmp(key->data, nx_kv->key->data) == 0) {
            return nx_kv->val;
        }
        nx_kv = nx_kv->collision_next;
    }
    return NULL;
}

int qwq_hashmap_update(qwq_hashmap* qmap,
                       qwq_hashmap_key* key,
                       qwq_hashmap_val* new_val) {
    if (!qmap || qmap->count <= 0) {
        return -1;
    }
    if (!key->hashcode || !key->cmp) {
        return -1;
    }

    int hash_idx;
    hash_idx = key->hashcode(key->data) % qmap->capacity;
    if (!qmap->data[hash_idx]) {
        return -1;
    }

    qwq_hashmap_kv *nx_kv = NULL, *tmp_kv = NULL;
    nx_kv = qmap->data[hash_idx];
    while (nx_kv) {
        tmp_kv = nx_kv;
        nx_kv = nx_kv->collision_next;
        // 如果找到重复 key, 则替换 val
        if (key->cmp(key->data, tmp_kv->key->data) == 0) {
            qwq_hashmap_del_val(tmp_kv->val);
            tmp_kv->val = new_val;
            return 0;
        }
    }

    return -1;
}

int qwq_hashmap_delete(qwq_hashmap* qmap, qwq_hashmap_key* key) {
    if (!qmap || qmap->count <= 0) {
        return -1;
    }
    if (!key->hashcode || !key->cmp) {
        return -1;
    }

    int hash_idx;
    hash_idx = key->hashcode(key->data) % qmap->capacity;
    if (!qmap->data[hash_idx]) {
        return -1;
    }

    qwq_hashmap_kv* nx_kv;
    nx_kv = qmap->data[hash_idx];
    while (nx_kv) {
        // 如果找到 key, 则删除 val
        if (key->cmp(key->data, nx_kv->key->data) == 0) {
            QWQ_DLINKLIST_DEL(qmap->head, nx_kv);
            qwq_hashmap_del_kv(nx_kv, NULL);
            qmap->count--;
            qmap->data[hash_idx] = NULL;
            return 0;
        }
        nx_kv = nx_kv->collision_next;
    }

    return -1;
}

static void* qwq_hashmap_merge_helper(qwq_hashmap_kv* kv, void** func_params) {
    if (!func_params) {
        return NULL;
    }
    qwq_hashmap* qmap = (qwq_hashmap*) (*func_params);
    int err = qwq_hashmap_set(&qmap, kv->key, kv->val);
    if (err == 2) {
        err = qwq_hashmap_update(qmap, kv->key, kv->val);
    }
    if (err) {
        return NULL;
    }
    *func_params = qmap;
    return qmap;
}

int qwq_hashmap_merge(qwq_hashmap* dst, qwq_hashmap* src) {
    if (!dst) return -1;
    if (!src) return 0;
    qwq_hashmap_iter_kv(src, qwq_hashmap_merge_helper, (void*) &dst);
    return 0;
}

// 存在返回0 不存在返回负数
int qwq_hashmap_has(qwq_hashmap* qmap, qwq_hashmap_key* key) {
    qwq_hashmap_kv *kv_list = NULL, *tmp_kv = NULL;
    kv_list = qmap->head;
    while (kv_list) {
        tmp_kv = kv_list;
        kv_list = kv_list->next;
        if (key->cmp(key->data, tmp_kv->key->data) == 0) {
            return 0;
        }
    }
    return -1;
}

int qwq_hashmap_clear(qwq_hashmap* qmap) {
    qwq_hashmap_iter_kv(qmap, qwq_hashmap_del_kv, NULL);
    return 0;
}

void qwq_hashmap_destroy(qwq_hashmap* qmap) {
    qwq_hashmap_clear(qmap);
    free(qmap->data);
    free(qmap);
}

#undef DEFAULT_CAPACITY
#undef MAX_LOAD_THRESHOLD