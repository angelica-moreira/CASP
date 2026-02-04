#include <stddef.h>

int buffer_copy(int *dst, const int *src, size_t n) {
    if (dst == NULL || src == NULL) {     
        return -1;
    }

    if (n == 0) {                          
        return 0;
    }

    for (size_t i = 0; i < n; i++) {       
        dst[i] = src[i];
    }

    return 0;                              
}

