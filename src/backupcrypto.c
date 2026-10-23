#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "backupcrypto.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(NV_ANDROID) || defined(NV_BACKUP_MBEDTLS)
#define BC_IMPLEMENTADO 1
#include <mbedtls/gcm.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/version.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

static const unsigned char magic[] = "NUVIO-AES256GCM1";
#define BC_RODADAS 600000u
#define BC_FIXO 54u
void backupcrypto_limpar(void *p, size_t n) {
  volatile unsigned char *v = p;
  if (v) while (n--) *v++ = 0;
}
int backupcrypto_disponivel(void) {
#ifdef BC_IMPLEMENTADO
  return 1;
#else
  return 0;
#endif
}
static uint32_t u32(const unsigned char *p) {
  return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static int contextoValido(const char *s, size_t *n) {
  if (!s) return 0;
  *n = strnlen(s, BACKUP_CRYPTO_CONTEXT_MAX + 1u);
  if (!*n || *n > BACKUP_CRYPTO_CONTEXT_MAX) return 0;
  for (size_t i = 0; i < *n; i++) if ((unsigned char)s[i] < 33 || (unsigned char)s[i] > 126) return 0;
  return 1;
}
int backupcrypto_envelope(const void *dados, size_t n, const char *contexto) {
  const unsigned char *p = dados; size_t nc;
  if (!p || !contextoValido(contexto, &nc) || n < BC_FIXO + nc + 16u ||
      n > BACKUP_CRYPTO_MAX + BACKUP_CRYPTO_OVERHEAD ||
      memcmp(p, magic, sizeof magic - 1u) || u32(p + 16) != BC_RODADAS) return 0;
  size_t ctx = (size_t)p[48]<<8 | p[49], bytes = u32(p + 50);
  return ctx == nc && bytes && bytes <= BACKUP_CRYPTO_MAX &&
         BC_FIXO + ctx + bytes + 16u == n && !memcmp(p + BC_FIXO, contexto, nc);
}
#ifdef BC_IMPLEMENTADO
static void por32(unsigned char *p, uint32_t v) {
  p[0]=(unsigned char)(v>>24);p[1]=(unsigned char)(v>>16);
  p[2]=(unsigned char)(v>>8);p[3]=(unsigned char)v;
}
static int aleatorio(unsigned char *out, size_t n) {
  int fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC);
  if (fd < 0) return 0;
  size_t pos = 0; int ok = 1;
  while (pos < n) {
    ssize_t k = read(fd, out + pos, n - pos);
    if (k < 0 && errno == EINTR) continue;
    if (k <= 0) { ok = 0; break; }
    pos += (size_t)k;
  }
  if (close(fd) != 0) ok = 0;
  return ok;
}
static int chave(const void *senha, size_t n, const unsigned char salt[16], unsigned char out[32]) {
  if (!senha || !n || n > 1024) return 0;
#if MBEDTLS_VERSION_NUMBER >= 0x03000000
  return mbedtls_pkcs5_pbkdf2_hmac_ext(MBEDTLS_MD_SHA256, senha, n, salt, 16, BC_RODADAS, 32, out) == 0;
#else
  mbedtls_md_context_t md; mbedtls_md_init(&md);
  int r = mbedtls_md_setup(&md, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
  if (!r) r = mbedtls_pkcs5_pbkdf2_hmac(&md, senha, n, salt, 16, BC_RODADAS, 32, out);
  mbedtls_md_free(&md); return r == 0;
#endif
}
#endif
int backupcrypto_selar(const void *dados, size_t n, const void *senha, size_t ns,
                       const char *contexto, unsigned char **saida, size_t *tamanho) {
  if (saida) *saida = NULL;
  if (tamanho) *tamanho = 0;
  size_t nc;
  if (!saida || !tamanho || !dados || !n || n > BACKUP_CRYPTO_MAX ||
      !senha || !ns || ns > 1024 || !contextoValido(contexto, &nc)) return 0;
#ifdef BC_IMPLEMENTADO
  size_t cab = BC_FIXO + nc, total = cab + n + 16u;
  unsigned char *p = malloc(total), key[32] = {0};
  if (!p) return 0;
  memcpy(p, magic, sizeof magic - 1u); por32(p + 16, BC_RODADAS);
  p[48]=(unsigned char)(nc>>8);p[49]=(unsigned char)nc;por32(p+50,(uint32_t)n);
  memcpy(p + BC_FIXO, contexto, nc);
  int ok = aleatorio(p + 20, 28) && chave(senha, ns, p + 20, key);
  mbedtls_gcm_context gcm; mbedtls_gcm_init(&gcm);
  if (ok) ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, n, p+36, 12, p, cab, dados, p+cab, 16, p+cab+n) == 0;
  mbedtls_gcm_free(&gcm); backupcrypto_limpar(key, sizeof key);
  if (!ok) { backupcrypto_limpar(p, total); free(p); return 0; }
  *saida=p;*tamanho=total;return 1;
#else
  return 0;
#endif
}
int backupcrypto_abrir(const void *dados, size_t n, const void *senha, size_t ns,
                       const char *contexto, unsigned char **saida, size_t *tamanho) {
  if (saida) *saida = NULL;
  if (tamanho) *tamanho = 0;
  if (!saida || !tamanho || !senha || !ns || ns > 1024 || !backupcrypto_envelope(dados, n, contexto)) return 0;
#ifdef BC_IMPLEMENTADO
  const unsigned char *p = dados;
  size_t bytes = u32(p + 50), cab = n - bytes - 16u;
  unsigned char *out = malloc(bytes + 1u), key[32] = {0};
  if (!out) return 0;
  int ok = chave(senha, ns, p + 20, key);
  mbedtls_gcm_context gcm; mbedtls_gcm_init(&gcm);
  if (ok) ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256) == 0 &&
    mbedtls_gcm_auth_decrypt(&gcm, bytes, p+36, 12, p, cab, p+cab+bytes, 16, p+cab, out) == 0;
  mbedtls_gcm_free(&gcm); backupcrypto_limpar(key, sizeof key);
  if (!ok) { backupcrypto_limpar(out, bytes+1u); free(out); return 0; }
  out[bytes]=0;*saida=out;*tamanho=bytes;return 1;
#else
  return 0;
#endif
}
