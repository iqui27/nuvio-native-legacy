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

#include "sha256.h"
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
static int lerArquivo(const char *file,unsigned char sal[16], unsigned char hash[32]) {
  char *t = dados_ler(file), sh[33], hh[65];
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

static int definidoArquivo(const char *file) { unsigned char s[16], h[32]; return lerArquivo(file,s,h); }
static int definirArquivo(const char *file,const char *pin) {
  unsigned char sal[16], h[32];
  char sh[33], hh[65], linha[128];
  if (!pinValido(pin)) return 0;
  salAleatorio(sal);
  derivar(sal, pin, h);
  hex(sal, 16, sh); hex(h, 32, hh);
  snprintf(linha, sizeof linha, "v1 %s %s\n", sh, hh);
  return dados_gravar(file, linha);
}
static int conferirArquivo(const char *file,const char *pin) {
  unsigned char sal[16], h[32], t[32];
  int i, d = 0;
  if (!pinValido(pin) || !lerArquivo(file,sal,h)) return 0;
  derivar(sal, pin, t);
  for (i = 0; i < 32; i++) d |= h[i] ^ t[i];
  return d == 0;
}
int admin_pin_definido(void) { return definidoArquivo(ARQ); }
int admin_pin_definir(const char *pin) { return definirArquivo(ARQ,pin); }
int admin_pin_conferir(const char *pin) { return conferirArquivo(ARQ,pin); }
static int perfilArquivo(int profile,char file[48]) {
  if(profile<1 || profile>32)return 0;
  snprintf(file,48,"profile-pin-%d.txt",profile);return 1;
}
int admin_perfil_pin_definido(int profile) { char file[48];return perfilArquivo(profile,file) && definidoArquivo(file); }
int admin_perfil_pin_definir(int profile,const char *pin) { char file[48];return perfilArquivo(profile,file) && definirArquivo(file,pin); }
int admin_perfil_pin_conferir(int profile,const char *pin) { char file[48];return perfilArquivo(profile,file) && conferirArquivo(file,pin); }
void admin_perfil_pin_remover(int profile) { char file[48];if(perfilArquivo(profile,file))dados_apagar(file); }
void admin_pin_esquecer(void) {
  dados_apagar(ARQ);
  for(int p=1;p<=32;p++) { char file[48];perfilArquivo(p,file);dados_apagar(file); }
}
