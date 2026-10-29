#ifndef NV_WIN_STATVFS_H
#define NV_WIN_STATVFS_H
#include <windows.h>
struct statvfs { unsigned long long f_bsize, f_frsize, f_blocks, f_bfree, f_bavail; };
static inline int statvfs(const char *p, struct statvfs *s) {
  ULARGE_INTEGER available, total, freebytes;
  if (!GetDiskFreeSpaceExA(p, &available, &total, &freebytes)) return -1;
  s->f_bsize = s->f_frsize = 4096; s->f_blocks = total.QuadPart / 4096;
  s->f_bfree = freebytes.QuadPart / 4096; s->f_bavail = available.QuadPart / 4096; return 0;
}
#endif
