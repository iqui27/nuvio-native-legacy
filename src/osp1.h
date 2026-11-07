// OSP1: o pareamento do OpenSeek (servidor proprio de miniaturas), lado da TV.
// PURO: SHA-256, HMAC-SHA256 e PBKDF2-HMAC-SHA256 (FIPS 180-4 / RFC 2104 /
// RFC 8018) escritos aqui, sem libcrypto (a do aparelho nao e garantida), e a
// derivacao exata do contrato OpenSeek v1 (docs/api/CONTRACT-v1.md):
//
//   code_norm    = maiusculas(code) sem espacos/hifens
//   K            = PBKDF2-HMAC-SHA256(code_norm, "OSP1"||pair_id||salt||spki_pin, it, 32)
//   client_proof = hex(HMAC-SHA256(K, "client"||pair_id||device_name))
//   server_proof = hex(HMAC-SHA256(K, "server"||pair_id||hex(SHA-256(token))))
//
// Tudo como texto UTF-8; hex minusculo. Nada daqui escreve em log: codigo, K,
// token e pino sao segredos ou quase.
#ifndef NV_OSP1_H
#define NV_OSP1_H
#include <stddef.h>
#include <stdint.h>

#define OSP1_ITER_MIN 200000L
#define OSP1_ITER_MAX 5000000L   // teto anti-DoS da TV (PBKDF2 em CPU de TV)
#define OSP1_CODE_LEN 8
#define OSP1_CONVITE_TAG 4        // etiqueta publica do convite (nomeia o convite no servidor)
#define OSP1_CONVITE_SEGREDO 12   // o segredo do convite (entra no PBKDF2 como o codigo de 8 entra)

typedef struct {
  uint32_t h[8];
  uint64_t n;
  unsigned char b[64];
  unsigned nb;
} Osp1Sha;
void osp1_sha256_ini(Osp1Sha *s);
void osp1_sha256_add(Osp1Sha *s, const void *d, size_t n);
void osp1_sha256_fim(Osp1Sha *s, unsigned char out[32]);
void osp1_sha256(const void *d, size_t n, unsigned char out[32]);
void osp1_hmac(const void *k, size_t nk, const void *d, size_t nd, unsigned char out[32]);
// `parar` (opcional) e consultado a cada 4096 iteracoes; 1 aborta (devolve 0).
int  osp1_pbkdf2(const void *senha, size_t ns, const void *sal, size_t nsal,
                 long iter, unsigned char out[32], int (*parar)(void *), void *u);
void osp1_hex(const unsigned char *b, size_t n, char *out); // out: 2n+1

// "k7qf-m2xd" -> "K7QFM2XD". 1 quando sobram exatamente 8 simbolos do alfabeto
// do contrato (23456789ABCDEFGHJKMNPQRSTUVWXYZ).
int  osp1_normalizar_codigo(const char *digitado, char out[OSP1_CODE_LEN + 1]);
// Codigo de convite digitado: etiqueta (4) + segredo (12), alfabeto do contrato, espacos/tracos ignorados.
int  osp1_normalizar_convite(const char *digitado, char tag[OSP1_CONVITE_TAG + 1], char segredo[OSP1_CONVITE_SEGREDO + 1]);
// K. 0 quando iter fora de [OSP1_ITER_MIN, OSP1_ITER_MAX] (recusa de
// rebaixamento), entrada vazia ou cancelado. `iterMin` permite os vetores de
// teste com poucas iteracoes; o app passa OSP1_ITER_MIN.
int  osp1_derivar(const char *codeNorm, const char *pairId, const char *salt,
                  const char *spkiPin, long iter, long iterMin,
                  unsigned char k[32], int (*parar)(void *), void *u);
void osp1_prova_cliente(const unsigned char k[32], const char *pairId,
                        const char *device, char hex[65]);
void osp1_prova_servidor(const unsigned char k[32], const char *pairId,
                         const char *token, char hex[65]);
// Comparacao em tempo constante de duas provas hex (64 chars). 1 iguais.
int  osp1_iguais(const char *a, const char *b);
#endif
