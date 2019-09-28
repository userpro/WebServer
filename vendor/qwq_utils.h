#ifndef QWQ_UTILS_H_
#define QWQ_UTILS_H_

#define QWQ_DLINKLIST_INS_FRT(head, x) \
    do {                               \
        x->prev = NULL;                \
        x->next = head;                \
        if (head) head->prev = x;      \
        head = x;                      \
    } while (0)

#define QWQ_DLINKLIST_DEL(head, x)                \
    do {                                          \
        if (!x->prev) {                           \
            head = x->next;                       \
            if (head) head->prev = NULL;          \
        } else {                                  \
            x->prev->next = x->next;              \
            if (x->next) x->next->prev = x->prev; \
        }                                         \
    } while (0)

int qwq_utils_fls(int x);
unsigned int qwq_utils_roundup_pow_of_two(unsigned int x);

#endif