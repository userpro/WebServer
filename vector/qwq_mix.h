#ifndef QWQ_MIX_H
#define QWQ_MIX_H

#include "qwq_hashmap.h"
#include "qwq_string.h"

qwq_hashmap_key* qwq_string_make_hmap_key(qwq_string* qwq);

qwq_hashmap_val* qwq_string_make_hmap_val(qwq_string* qwq);

qwq_string* qwq_hashmap_to_qwq_string(qwq_hashmap* qmap,
                                      const char* kv_deli,
                                      int kv_deli_len,
                                      const char* entry_deli,
                                      int entry_deli_len);

#endif