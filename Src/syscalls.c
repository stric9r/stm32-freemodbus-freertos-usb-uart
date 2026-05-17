/**
  ******************************************************************************
  * @file      syscalls.c
  * @brief     Minimal syscall stubs — satisfies newlib's reentrant wrappers
  *            (_close_r, _lseek_r, _read_r, _write_r) so the linker does not
  *            pull in the nosys.specs stubs, which emit "not implemented"
  *            warnings at link time.
  *
  *            Each stub asserts false: none of these should ever be reached
  *            in this project. If one fires, something is calling into the
  *            C file I/O layer unexpectedly.
  ******************************************************************************
  * Stric Roberts, 2026. MIT License — see LICENSE in the project root.
  ******************************************************************************
  */
#include <assert.h>
#include <stdbool.h>
#include <errno.h>

int _close(int file)
{
    (void)file;
    bool const bNotImplemented = false;
    assert(bNotImplemented);
    errno = EBADF;
    return -1;
}

int _lseek(int file, int ptr, int dir)
{
    (void)file;
    (void)ptr;
    (void)dir;
    bool const bNotImplemented = false;
    assert(bNotImplemented);
    errno = ESPIPE;
    return -1;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    bool const bNotImplemented = false;
    assert(bNotImplemented);
    errno = EBADF;
    return -1;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    (void)ptr;
    (void)len;
    bool const bNotImplemented = false;
    assert(bNotImplemented);
    errno = EBADF;
    return -1;
}
