#include "admin.h"
#include "dados.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define ARQ "admin-pin.txt"   // "v1 <sal hex 32> <hash hex 64>\n"
#define VOLTAS 4096

// --- SHA-256 (FIPS 180-4), a minima ---------------------------------------
typedef struct { unsigned int h[8]; unsigned char b[64]; unsigned long long n; unsigned int k; } Sha;
static const unsigned int K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void bloco(Sha *s, const unsigned char *p) {
  unsigned int w[64], a, b, c, d, e, f, g, h, t1, t2;
  int i;
  for (i = 0; i < 16; i++) w[i] = (unsigned)p[i*4] << 24 | (unsigned)p[i*4+1] << 16 | (unsigned)p[i*4+2] << 8 | p[i*4+3];
  for (; i < 64; i++) {
    unsigned int s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
    unsigned int s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
    w[i] = w[i-16] + s0 + w[i-7] + s1;
  }
  a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
  for (i = 0; i < 64; i++) {
    t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K[i] + w[i];
    t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}
static void sha_ini(Sha *s) {
  static const unsigned int I[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
  memcpy(s->h, I, sizeof I); s->n = 0; s->k = 0;
}
static void sha_add(Sha *s, const unsigned char *p, unsigned long n) {
  while (n--) { s->b[s->k++] = *p++; s->n++; if (s->k == 64) { bloco(s, s->b); s->k = 0; } }
}
static void sha_fim(Sha *s, unsigned char out[32]) {
  unsigned long long bits = s->n * 8;
  int i;
  s->b[s->k++] = 0x80;
  if (s->k > 56) { while (s->k < 64) s->b[s->k++] = 0; bloco(s, s->b); s->k = 0; }
  while (s->k < 56) s->b[s->k++] = 0;
  for (i = 7; i >= 0; i--) s->b[s->k++] = (unsigned char)(bits >> (i * 8));
  bloco(s, s->b);
  for (i = 0; i < 8; i++) { out[i*4] = (unsigned char)(s->h[i] >> 24); out[i*4+1] = (unsigned char)(s->h[i] >> 16); out[i*4+2] = (unsigned char)(s->h[i] >> 8); out[i*4+3] = (unsigned char)s->h[i]; }
}
static void hex(const unsigned char *b, int n, char *o) {
  static const char H[] = "0123456789abcdef";
  int i;
  for (i = 0; i < n; i++) { o[i*2] = H[b[i] >> 4]; o[i*2+1] = H[b[i] & 15]; }
  o[n*2] = 0;
}
void admin_sha256_hex(const unsigned char *buf, unsigned long n, char *hex65) {
  Sha s; unsigned char d[32];
  sha_ini(&s); sha_add(&s, buf, n); sha_fim(&s, d); hex(d, 32, hex65);
}

// hash = SHA-256 iterado VOLTAS vezes sobre (sal || pin).
static void derivar(const unsigned char sal[16], const char *pin, unsigned char out[32]) {
  Sha s; int i;
  sha_ini(&s); sha_add(&s, sal, 16); sha_add(&s, (const unsigned char *)pin, strlen(pin)); sha_fim(&s, out);
  for (i = 1; i < VOLTAS; i++) { sha_ini(&s); sha_add(&s, out, 32); sha_add(&s, sal, 16); sha_fim(&s, out); }
}

static int pinValido(const char *pin) {
  size_t n = pin ? strlen(pin) : 0, i;
  if (n < ADMIN_PIN_MIN || n > ADMIN_PIN_MAX) return 0;
  for (i = 0; i < n; i++) if (pin[i] < '0' || pin[i] > '9') return 0;
  return 1;
}
static int salAleatorio(unsigned char sal[16]) {
  int f = open("/dev/urandom", O_RDONLY), ok = 0;
  if (f >= 0) { ok = read(f, sal, 16) == 16; close(f); }
  if (!ok) {   // sem urandom: relogio + enderecos misturados (so o sal, nunca o PIN)
    unsigned long long t = (unsigned long long)time(NULL) * 2654435761u ^ (unsigned long long)(size_t)sal;
    int i;
    for (i = 0; i < 16; i++) { t = t * 6364136223846793005ULL + 1442695040888963407ULL; sal[i] = (unsigned char)(t >> 33); }
  }
  return 1;
}
static int ler(unsigned char sal[16], unsigned char hash[32]) {
  char *t = dados_ler(ARQ), sh[33], hh[65];
  int i, ok = 0;
  if (!t) return 0;
  if (sscanf(t, "v1 %32s %64s", sh, hh) == 2 && strlen(sh) == 32 && strlen(hh) == 64) {
    ok = 1;
    for (i = 0; i < 16 && ok; i++) { unsigned v; if (sscanf(sh + i*2, "%2x", &v) != 1) ok = 0; else sal[i] = (unsigned char)v; }
    for (i = 0; i < 32 && ok; i++) { unsigned v; if (sscanf(hh + i*2, "%2x", &v) != 1) ok = 0; else hash[i] = (unsigned char)v; }
  }
  memset(t, 0, strlen(t)); free(t);
  return ok;
}

int admin_pin_definido(void) { unsigned char s[16], h[32]; return ler(s, h); }
int admin_pin_definir(const char *pin) {
  unsigned char sal[16], h[32];
  char sh[33], hh[65], linha[128];
  if (!pinValido(pin)) return 0;
  salAleatorio(sal);
  derivar(sal, pin, h);
  hex(sal, 16, sh); hex(h, 32, hh);
  snprintf(linha, sizeof linha, "v1 %s %s\n", sh, hh);
  return dados_gravar(ARQ, linha);
}
int admin_pin_conferir(const char *pin) {
  unsigned char sal[16], h[32], t[32];
  int i, d = 0;
  if (!pinValido(pin) || !ler(sal, h)) return 0;
  derivar(sal, pin, t);
  for (i = 0; i < 32; i++) d |= h[i] ^ t[i];
  return d == 0;
}
void admin_pin_esquecer(void) { dados_apagar(ARQ); }
