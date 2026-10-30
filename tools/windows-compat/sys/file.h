#ifndef NV_WIN_FILE_H
#define NV_WIN_FILE_H
#include <windows.h>
#include <io.h>
#define LOCK_EX 2
#define LOCK_UN 8
static inline int flock(int fd, int op) {
  HANDLE h = (HANDLE)_get_osfhandle(fd); OVERLAPPED o = {0};
  return (op == LOCK_UN ? UnlockFileEx(h, 0, 1, 0, &o) : LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0, &o)) ? 0 : -1;
}
#endif
