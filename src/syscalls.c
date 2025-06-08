#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "stm32l152xc.h"
#include "core_cm3.h"
#include "syscalls.h"
#undef errno
extern int errno;

void _exit(int status) {
    while (1) {}    // Hang here
}

int _write(int file, char *ptr, int len) {
    return len;
}

caddr_t _sbrk(int incr) {
    extern char _end; // Defined by the linker
    static char *heap_end;
    char *prev_heap_end;

    if (heap_end == 0) {
        heap_end = &_end;
    }
    prev_heap_end = heap_end;

    char *stack = (char *)__get_MSP();
    if (heap_end + incr > stack) {
        // Heap and stack collision
        errno = ENOMEM;
        return (caddr_t) -1;
    }

    heap_end += incr;
    return (caddr_t) prev_heap_end;
}

int _close(int file) {
    return -1;
}

int _fstat(int file, struct stat *st) {
    st->st_mode = S_IFCHR;
    return 0;
}

int _isatty(int file) {
    return 1;
}

int _lseek(int file, int ptr, int dir) {
    return 0;
}

int _read(int file, char *ptr, int len) {
    return 0;
}
