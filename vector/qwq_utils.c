#include "qwq_utils.h"

inline int qwq_utils_fls(int x) {
    unsigned int position;
    unsigned int i;
    if (x != 0) {
        for (i = (x >> 1), position = 0; i != 0; ++position) i >>= 1;
    } else {
        position = -1;
    }
    return position + 1;
}

inline unsigned int qwq_utils_roundup_pow_of_two(unsigned int x) {
    return 1UL << qwq_utils_fls(x);
}
