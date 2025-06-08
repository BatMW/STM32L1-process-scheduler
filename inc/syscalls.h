#ifndef DUMMY_H
#define DUMMY_H
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "core_cm3.h"
#undef errno
extern int errno;

void _exit(int status);

int _write(int file, char *ptr, int len);

caddr_t _sbrk(int incr);

int _close(int file);

int _fstat(int file, struct stat *st);

int _isatty(int file);

int _lseek(int file, int ptr, int dir);

int _read(int file, char *ptr, int len);
#endif
