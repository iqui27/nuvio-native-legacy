// Real MKV proxy, fake upstream: hidden Cues plus expiring direct-file URLs.
#include "mkvfix.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
static int hiddenMkv, expiredStatus;
static unsigned char media[512];
static void hiddenHeader(void) {
  const unsigned char header[] = {
    0x1a,0x45,0xdf,0xa3,0x80, 0x18,0x53,0x80,0x67,0xff,
    0x11,0x4d,0x9b,0x74,0x8e, 0x4d,0xbb,0x8b,
    0x53,0xab,0x84,0x11,0x4d,0x9b,0x74, 0x53,0xac,0x81,0xf6,
    0xec,0xb1
  };
  const unsigned char second[] = {
    0x11,0x4d,0x9b,0x74,0x8f, 0x4d,0xbb,0x8c,
    0x53,0xab,0x84,0x1c,0x53,0xbb,0x6b, 0x53,0xac,0x82,0x01,0x76
  };
  memset(media, 0, sizeof media);
  memcpy(media, header, sizeof header);
  memcpy(media + 78, "\x15\x49\xa9\x66\x80", 5);
  memcpy(media + 256, second, sizeof second);
  memcpy(media + 384, "\x1c\x53\xbb\x6b\x80", 5);
}
char *rede_baixar_trecho64_final(const char *u, const char *h, int64_t a, int64_t z,
    long *n, int64_t *total, int *st, volatile int *cancel, char *fin, int *cross) {
  (void)h; (void)cancel;
  *n = 0; *st = 404; if (fin) fin[0] = 0; if (cross) *cross = 0;
  if (!hiddenMkv || a < 0 || a >= sizeof media) return NULL;
  if (z >= sizeof media) z = sizeof media - 1;
  *n = (long)(z - a + 1); *st = 206;
  if (total) *total = sizeof media;
  if (fin) snprintf(fin, 8192, "%s", u);
  char *p = malloc((size_t)*n); memcpy(p, media + a, (size_t)*n); return p;
}
int rede_fluxo64(const char *u, const char *h, int64_t a,
    int (*sink)(void *, const char *, size_t), void *opaque, int64_t *total,
    int *st, volatile int *cancel, char *fin, int *cross, long stall, int64_t *bytes) {
  (void)cancel; (void)fin; (void)cross; (void)stall;
  *bytes = 0; *total = sizeof media;
  if (strstr(u, "/direct")) { *st = expiredStatus; return -1; }
  ok(strstr(u, "/fresh") != NULL, "proxy retries a fresh selected-file URL");
  ok(!h || !*h, "refreshed direct URL receives no source headers");
  *st = 206; *bytes = sizeof media - a;
  return sink(opaque, (const char *)media + a, (size_t)*bytes) ? 0 : 1;
}
static void proxyRead(const char *url) {
  int port = 0; char path[256], req[512], reply[2048]; size_t n = 0;
  ok(sscanf(url, "http://127.0.0.1:%d/%255s", &port, path) == 2, "seek activates local MKV proxy");
  int fd = socket(AF_INET, SOCK_STREAM, 0);
  struct sockaddr_in addr = {0}; addr.sin_family = AF_INET;
  addr.sin_port = htons(port); addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  struct timeval tv = {3, 0}; setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  ok(connect(fd, (struct sockaddr *)&addr, sizeof addr) == 0, "connect to MKV proxy");
  int len = snprintf(req, sizeof req, "GET /%s HTTP/1.1\r\nRange: bytes=0-511\r\n\r\n", path);
  send(fd, req, (size_t)len, 0);
  ssize_t r; while (n < sizeof reply - 1 && (r = recv(fd, reply + n, sizeof reply - 1 - n, 0)) > 0) n += (size_t)r;
  reply[n] = 0; close(fd);
  ok(strstr(reply, "206 Partial Content") != NULL, "expired direct file recovers through MKV proxy");
  char *body = strstr(reply, "\r\n\r\n");
  ok(body && n - (size_t)(body + 4 - reply) == sizeof media, "proxy delivers full requested range after refresh");
  if (body && n - (size_t)(body + 4 - reply) == sizeof media) {
    MkvfixCab c;
    ok(mkvfix_ler_inicio((unsigned char *)body + 4, sizeof media, &c) &&
       mkvfix_entrada(c.id, c.pos, c.n, 0x1c53bb6b) == 374,
       "refreshed proxy preserves repaired Cues");
  }
}
