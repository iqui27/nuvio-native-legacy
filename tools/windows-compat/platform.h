/* Forced include for the Windows target only. Keep TV translation units intact. */
#ifndef NV_WINDOWS_PLATFORM_H
#define NV_WINDOWS_PLATFORM_H
#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <io.h>
#include <direct.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#ifdef IN
#undef IN
#endif
#include <limits.h>
#include <string.h>
#include <strings.h>
#ifndef O_DIRECTORY
#define O_DIRECTORY 0
#endif
#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef SHUT_RDWR
#define SHUT_RDWR SD_BOTH
#endif
static inline int nv_setenv(const char *k, const char *v, int overwrite) { return !overwrite && getenv(k) ? 0 : _putenv_s(k, v); }
static inline int nv_mkdir(const char *p, int mode) { (void)mode; return _mkdir(p); }
#define setenv nv_setenv
#define mkdir nv_mkdir
#ifndef S_ISLNK
#define S_ISLNK(mode) 0
#endif
#ifndef EDQUOT
#define EDQUOT ENOSPC
#endif
#define fsync _commit
#define lstat stat
#define timegm _mkgmtime
static inline char *realpath(const char *p, char *out) { return _fullpath(out, p, PATH_MAX); }
static inline int rand_r(unsigned *seed) { *seed = *seed * 1103515245u + 12345u; return (int)((*seed >> 1) & RAND_MAX); }
static inline char *strcasestr(const char *s, const char *part) {
  size_t n = strlen(part); for (; *s; s++) if (!strncasecmp(s, part, n)) return (char *)s;
  return n ? NULL : (char *)s;
}
static inline int fchmod(int fd, int mode) {
  char path[32768]; HANDLE h = (HANDLE)_get_osfhandle(fd);
  if (!GetFinalPathNameByHandleA(h, path, sizeof path, FILE_NAME_NORMALIZED)) return -1;
  return _chmod(path, mode);
}
static inline char *strndup(const char *s, size_t n) {
  size_t len = strnlen(s, n); char *p = malloc(len + 1);
  if (p) { memcpy(p, s, len); p[len] = 0; } return p;
}
static inline struct tm *localtime_r(const time_t *t, struct tm *out) { return localtime_s(out, t) == 0 ? out : NULL; }
static inline struct tm *gmtime_r(const time_t *t, struct tm *out) { return gmtime_s(out, t) == 0 ? out : NULL; }
/* ReplaceFile semantics: POSIX rename replaces an existing destination. */
static inline int nv_rename(const char *a, const char *b) { return MoveFileExA(a, b, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) ? 0 : -1; }
#define rename nv_rename
static inline FILE *nv_fopen(const char *p, const char *mode) {
  if (strcmp(p, "/dev/urandom")) return fopen(p, mode);
  FILE *f = tmpfile(); unsigned char bytes[256];
  if (!f || BCryptGenRandom(NULL, bytes, sizeof bytes, BCRYPT_USE_SYSTEM_PREFERRED_RNG)) { if (f) fclose(f); return NULL; }
  if (fwrite(bytes, 1, sizeof bytes, f) != sizeof bytes) { fclose(f); return NULL; }
  rewind(f); return f;
}
#define fopen nv_fopen
static inline int nv_open(const char *p, int flags, ...) {
  if (!strcmp(p, "/dev/urandom")) {
    FILE *f = nv_fopen(p, "rb"); if (!f) return -1;
    int fd = _dup(_fileno(f)); fclose(f); return fd;
  }
  return _open(p, flags | O_BINARY, _S_IREAD | _S_IWRITE);
}
#define open nv_open
#endif
