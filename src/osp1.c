#include "osp1.h"
#include <string.h>
#include <ctype.h>

static const uint32_t K256[64] = {
  0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
  0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
  0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
  0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
  0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
  0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
  0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
  0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void bloco(uint32_t h[8], const unsigned char *p) {
  uint32_t w[64], a, b, c, d, e, f, g, hh;
  int i;
  for (i = 0; i < 16; i++)
    w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 |
           (uint32_t)p[4 * i + 2] << 8 | (uint32_t)p[4 * i + 3];
  for (i = 16; i < 64; i++) {
    uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
    uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  a = h[0]; b = h[1]; c = h[2]; d = h[3]; e = h[4]; f = h[5]; g = h[6]; hh = h[7];
  for (i = 0; i < 64; i++) {
    uint32_t t1 = hh + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
    uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void osp1_sha256_ini(Osp1Sha *s) {
  static const uint32_t ini[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
  memcpy(s->h, ini, sizeof ini); s->n = 0; s->nb = 0;
}
void osp1_sha256_add(Osp1Sha *s, const void *d, size_t n) {
  const unsigned char *p = d;
  s->n += n;
  while (n) {
    size_t k = 64 - s->nb;
    if (k > n) k = n;
    memcpy(s->b + s->nb, p, k); s->nb += (unsigned)k; p += k; n -= k;
    if (s->nb == 64) { bloco(s->h, s->b); s->nb = 0; }
  }
}
void osp1_sha256_fim(Osp1Sha *s, unsigned char out[32]) {
  uint64_t bits = s->n * 8;
  unsigned char um = 0x80, zero = 0, len[8];
  int i;
  osp1_sha256_add(s, &um, 1);
  while (s->nb != 56) osp1_sha256_add(s, &zero, 1);
  for (i = 0; i < 8; i++) len[i] = (unsigned char)(bits >> (56 - 8 * i));
  osp1_sha256_add(s, len, 8);
  for (i = 0; i < 8; i++) {
    out[4 * i] = (unsigned char)(s->h[i] >> 24); out[4 * i + 1] = (unsigned char)(s->h[i] >> 16);
    out[4 * i + 2] = (unsigned char)(s->h[i] >> 8); out[4 * i + 3] = (unsigned char)s->h[i];
  }
  memset(s, 0, sizeof *s);
}
void osp1_sha256(const void *d, size_t n, unsigned char out[32]) {
  Osp1Sha s; osp1_sha256_ini(&s); osp1_sha256_add(&s, d, n); osp1_sha256_fim(&s, out);
}

// HMAC com os estados interno/externo ja absorvidos: o PBKDF2 reaproveita os
// dois a cada iteracao (600 mil x 2 blocos a menos numa CPU de TV).
typedef struct { Osp1Sha in, out; } Hmac;
static void hmacIni(Hmac *h, const void *k, size_t nk) {
  unsigned char kb[64], pad[64];
  int i;
  memset(kb, 0, sizeof kb);
  if (nk > 64) osp1_sha256(k, nk, kb); else if (nk) memcpy(kb, k, nk);
  for (i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x36;
  osp1_sha256_ini(&h->in); osp1_sha256_add(&h->in, pad, 64);
  for (i = 0; i < 64; i++) pad[i] = kb[i] ^ 0x5c;
  osp1_sha256_ini(&h->out); osp1_sha256_add(&h->out, pad, 64);
  memset(kb, 0, sizeof kb); memset(pad, 0, sizeof pad);
}
static void hmacFim(const Hmac *base, const void *d, size_t nd, const void *d2, size_t nd2,
                    unsigned char out[32]) {
  Osp1Sha s = base->in;
  unsigned char m[32];
  osp1_sha256_add(&s, d, nd);
  if (nd2) osp1_sha256_add(&s, d2, nd2);
  osp1_sha256_fim(&s, m);
  s = base->out; osp1_sha256_add(&s, m, 32); osp1_sha256_fim(&s, out);
  memset(m, 0, sizeof m);
}
void osp1_hmac(const void *k, size_t nk, const void *d, size_t nd, unsigned char out[32]) {
  Hmac h; hmacIni(&h, k, nk); hmacFim(&h, d, nd, NULL, 0, out); memset(&h, 0, sizeof h);
}

int osp1_pbkdf2(const void *senha, size_t ns, const void *sal, size_t nsal,
                long iter, unsigned char out[32], int (*parar)(void *), void *u) {
  Hmac h;
  unsigned char um[4] = { 0, 0, 0, 1 }, t[32], x[32];
  long i;
  int j;
  if (iter < 1) return 0;
  hmacIni(&h, senha, ns);
  hmacFim(&h, sal, nsal, um, 4, x);       // so um bloco: dkLen = 32 = hLen
  memcpy(t, x, 32);
  for (i = 1; i < iter; i++) {
    if (parar && !(i & 4095) && parar(u)) {
      memset(&h, 0, sizeof h); memset(t, 0, 32); memset(x, 0, 32); return 0;
    }
    hmacFim(&h, x, 32, NULL, 0, x);
    for (j = 0; j < 32; j++) t[j] ^= x[j];
  }
  memcpy(out, t, 32);
  memset(&h, 0, sizeof h); memset(t, 0, 32); memset(x, 0, 32);
  return 1;
}

void osp1_hex(const unsigned char *b, size_t n, char *out) {
  static const char H[] = "0123456789abcdef";
  size_t i;
  for (i = 0; i < n; i++) { out[2 * i] = H[b[i] >> 4]; out[2 * i + 1] = H[b[i] & 15]; }
  out[2 * n] = 0;
}

int osp1_normalizar_codigo(const char *dig, char out[OSP1_CODE_LEN + 1]) {
  static const char ALFA[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
  int n = 0;
  out[0] = 0;
  if (!dig) return 0;
  for (; *dig; dig++) {
    char c = (char)toupper((unsigned char)*dig);
    if (c == ' ' || c == '-') continue;
    if (!strchr(ALFA, c) || n >= OSP1_CODE_LEN) { out[0] = 0; return 0; }
    out[n++] = c;
  }
  out[n] = 0;
  if (n != OSP1_CODE_LEN) { out[0] = 0; return 0; }
  return 1;
}

int osp1_normalizar_convite(const char *dig, char tag[OSP1_CONVITE_TAG + 1], char segredo[OSP1_CONVITE_SEGREDO + 1]) {
  static const char ALFA[] = "23456789ABCDEFGHJKMNPQRSTUVWXYZ";
  char todo[OSP1_CONVITE_TAG + OSP1_CONVITE_SEGREDO + 1];
  int n = 0;
  tag[0] = segredo[0] = 0;
  if (!dig) return 0;
  for (; *dig; dig++) {
    char c = (char)toupper((unsigned char)*dig);
    if (c == ' ' || c == '-') continue;
    if (!strchr(ALFA, c) || n >= OSP1_CONVITE_TAG + OSP1_CONVITE_SEGREDO) return 0;
    todo[n++] = c;
  }
  if (n != OSP1_CONVITE_TAG + OSP1_CONVITE_SEGREDO) return 0;
  memcpy(tag, todo, OSP1_CONVITE_TAG); tag[OSP1_CONVITE_TAG] = 0;
  memcpy(segredo, todo + OSP1_CONVITE_TAG, OSP1_CONVITE_SEGREDO); segredo[OSP1_CONVITE_SEGREDO] = 0;
  return 1;
}

int osp1_derivar(const char *code, const char *pairId, const char *salt,
                 const char *pin, long iter, long iterMin,
                 unsigned char k[32], int (*parar)(void *), void *u) {
  char sal[512];
  size_t n;
  if (!code || !*code || !pairId || !*pairId || !salt || !*salt || !pin || !*pin) return 0;
  if (iter < iterMin || iter > OSP1_ITER_MAX || iter < 1) return 0;
  n = strlen("OSP1") + strlen(pairId) + strlen(salt) + strlen(pin);
  if (n >= sizeof sal) return 0;
  strcpy(sal, "OSP1"); strcat(sal, pairId); strcat(sal, salt); strcat(sal, pin);
  return osp1_pbkdf2(code, strlen(code), sal, n, iter, k, parar, u);
}

static void provaDe(const unsigned char k[32], const char *rot, const char *a,
                    const char *b, char hex[65]) {
  Hmac h; Osp1Sha s; unsigned char m[32], out[32];
  hmacIni(&h, k, 32);
  s = h.in;
  osp1_sha256_add(&s, rot, strlen(rot));
  osp1_sha256_add(&s, a, strlen(a));
  osp1_sha256_add(&s, b, strlen(b));
  osp1_sha256_fim(&s, m);
  s = h.out; osp1_sha256_add(&s, m, 32); osp1_sha256_fim(&s, out);
  osp1_hex(out, 32, hex);
  memset(&h, 0, sizeof h); memset(m, 0, 32); memset(out, 0, 32);
}
void osp1_prova_cliente(const unsigned char k[32], const char *pairId,
                        const char *device, char hex[65]) {
  provaDe(k, "client", pairId ? pairId : "", device ? device : "", hex);
}
void osp1_prova_servidor(const unsigned char k[32], const char *pairId,
                         const char *token, char hex[65]) {
  unsigned char th[32]; char thHex[65];
  osp1_sha256(token ? token : "", token ? strlen(token) : 0, th);
  osp1_hex(th, 32, thHex);
  provaDe(k, "server", pairId ? pairId : "", thHex, hex);
}
int osp1_iguais(const char *a, const char *b) {
  unsigned d = 0;
  int i;
  if (!a || !b || strlen(a) != 64 || strlen(b) != 64) return 0;
  for (i = 0; i < 64; i++) d |= (unsigned)(tolower((unsigned char)a[i]) ^ tolower((unsigned char)b[i]));
  return d == 0;
}
