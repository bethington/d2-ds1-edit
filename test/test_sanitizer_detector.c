/*
 * Negative control for the sanitizer lane (release-core:
 * control.sanitizer.detector).
 *
 * This program has a seeded heap-buffer-overflow. It is built only when
 * DS1EDIT_ENABLE_SANITIZERS is on (test/cmake/sanitizers.cmake), and its CTest
 * passes only when AddressSanitizer reports that overflow. A sanitizer job
 * that silently stopped instrumenting would let this program exit normally,
 * the CTest would fail, and "sanitizer.core passed" could no longer be
 * mistaken for "the sanitizers were looking".
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static volatile char sink;

int main(void)
{
    volatile int index = 8;          /* one past the end of an 8-byte block */
    char *block = (char *)malloc(8);

    if (block == NULL) {
        return 2;
    }
    memset(block, 1, 8);
    sink = block[index];             /* the seeded bug */
    free(block);
    puts("the seeded overflow was not detected: the sanitizers are not active");
    return 0;
}
