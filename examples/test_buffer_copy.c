#include <assert.h>
#include <stdio.h>
#include "buffer_copy.c"


int buffer_copy(int *dst, const int *src, size_t n);

int main() {
    int src[3] = {1, 2, 3};
    int dst[3] = {0};

    int ret = buffer_copy(dst, src, 3);

    assert(ret == 0);
    assert(dst[0] == 1);
    assert(dst[1] == 2);
    assert(dst[2] == 3);

    printf("Test passed\n");
    return 0;
}

