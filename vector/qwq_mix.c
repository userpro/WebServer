#include <stddef.h>

#include "qwq_mix.h"

qwq_hashmap_key* qwq_string_make_hmap_key(qwq_string* qwq) {
    return qwq_hashmap_make_key(QWQ_STRING,
                                (void*) qwq,
                                qwq_string_hmap_hash,
                                qwq_string_hmap_cmp,
                                qwq_string_hmap_free);
}

qwq_hashmap_val* qwq_string_make_hmap_val(qwq_string* qwq) {
    return qwq_hashmap_make_val(QWQ_STRING, (void*) qwq, qwq_string_hmap_free);
}

qwq_string* qwq_hashmap_to_qwq_string(qwq_hashmap* qmap,
                                      const char* kv_deli,
                                      int kv_deli_len,
                                      const char* entry_deli,
                                      int entry_deli_len) {
    qwq_string* out = NULL;
    out = qwq_string_new1();
    if (!out) {
        return NULL;
    }
    qwq_hashmap_kv *kv_list = NULL, *tmp_kv = NULL;
    kv_list = qmap->head;
    while (kv_list) {
        tmp_kv = kv_list;
        if (qwq_string_cat2(out, (qwq_string*) tmp_kv->key->data) < 0) {
            qwq_string_destroy(out);
            return NULL;
        }
        if (qwq_string_cat1(out, kv_deli, kv_deli_len) < 0) {
            qwq_string_destroy(out);
            return NULL;
        }
        if (qwq_string_cat2(out, (qwq_string*) tmp_kv->val->data) < 0) {
            qwq_string_destroy(out);
            return NULL;
        }
        if (qwq_string_cat1(out, entry_deli, entry_deli_len) < 0) {
            qwq_string_destroy(out);
            return NULL;
        }
        kv_list = kv_list->next;
    }
    return out;
}