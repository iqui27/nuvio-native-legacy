// OSP1 (pareamento do OpenSeek): SHA-256/HMAC/PBKDF2 e a derivacao do contrato
// contra vetores gerados SO do contrato com a stdlib do Python
// (tests/openseek_vectors.py -> tests/openseek_vectors.json). Com
// OPENSEEK_VECTORS=<arquivo> confere tambem os vetores do backend.
//
//   bash tests/openseek_osp1.sh
#include "../src/osp1.h"
#include "../src/js.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *ler(const char *nome) {
  FILE *f = fopen(nome, "rb");
  long n; char *b;
  if (!f) return NULL;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  b = malloc((size_t)n + 1);
  if (!b || fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
  b[n] = 0; fclose(f);
  return b;
}
static void hexDe(const unsigned char *d, size_t n, char *out) { osp1_hex(d, n, out); }
static int cancelar(void *u) { return ++*(int *)u > 3; }

// Um caso no formato de openseek_vectors.json. Nomes alternativos aceitos para
// o arquivo do backend (que pode usar outro nome por campo).
static int campo(const char *o, const char *f, const char *const *nomes, char *dst, size_t n) {
  for (int i = 0; nomes[i]; i++) if (js_texto(o, f, nomes[i], dst, n)) return 1;
  return 0;
}
static int conferirCaso(const char *o, const char *f, int *pulou) {
  static const char *const N_CODE[] = { "code", "code_typed", "pairing_code", NULL };
  static const char *const N_PAIR[] = { "pair_id", NULL };
  static const char *const N_SALT[] = { "salt", NULL };
  static const char *const N_PIN[] = { "spki_pin", "pin", NULL };
  static const char *const N_DEV[] = { "device_name", "device", NULL };
  static const char *const N_TOK[] = { "token", NULL };
  static const char *const N_K[] = { "K_hex", "k_hex", "K", "key_hex", NULL };
  static const char *const N_CP[] = { "client_proof", NULL };
  static const char *const N_SP[] = { "server_proof", NULL };
  char code[64], norm[16], pair[80], salt[80], pin[80], dev[80], tok[128], k[80], cp[80], sp[80];
  char kHex[65], cpHex[65], spHex[65];
  unsigned char K[32];
  long it = (long)js_num(o, f, "iterations", -1);
  if (it < 0) it = (long)js_num(o, f, "iter", -1);
  if (!campo(o, f, N_CODE, code, sizeof code) || !campo(o, f, N_PAIR, pair, sizeof pair) ||
      !campo(o, f, N_SALT, salt, sizeof salt) || !campo(o, f, N_PIN, pin, sizeof pin) ||
      !campo(o, f, N_DEV, dev, sizeof dev) || !campo(o, f, N_CP, cp, sizeof cp) || it < 1) {
    (*pulou)++; return 0;
  }
  assert(osp1_normalizar_codigo(code, norm));
  { char esperado[16]; if (js_texto(o, f, "code_norm", esperado, sizeof esperado)) assert(!strcmp(norm, esperado)); }
  assert(osp1_derivar(norm, pair, salt, pin, it, 1, K, NULL, NULL));
  hexDe(K, 32, kHex);
  if (campo(o, f, N_K, k, sizeof k)) assert(!strcmp(kHex, k));
  osp1_prova_cliente(K, pair, dev, cpHex);
  assert(!strcmp(cpHex, cp));
  if (campo(o, f, N_TOK, tok, sizeof tok) && campo(o, f, N_SP, sp, sizeof sp)) {
    osp1_prova_servidor(K, pair, tok, spHex);
    assert(!strcmp(spHex, sp));
    assert(osp1_iguais(spHex, sp));
  }
  // O aplicativo recusa < 200 000 (rebaixamento); o vetor so passa com iterMin 1.
  if (it < OSP1_ITER_MIN) assert(!osp1_derivar(norm, pair, salt, pin, it, OSP1_ITER_MIN, K, NULL, NULL));
  return 1;
}
static int conferirArquivo(const char *nome, int *pulou) {
  char *j = ler(nome);
  int n = 0;
  if (!j) return -1;
  for (const char *e = js_array(j, NULL, "osp1"); e; e = js_prox(js_fim(e))) {
    if (*e != '{') break;
    n += conferirCaso(e, js_fim(e) + 1, pulou);
  }
  if (!n) {   // outro formato: array na raiz, ou "vectors"/"cases"
    const char *e = js_raiz_array(j);
    if (!e) e = js_array(j, NULL, "vectors");
    if (!e) e = js_array(j, NULL, "cases");
    for (; e && *e == '{'; e = js_prox(js_fim(e))) n += conferirCaso(e, js_fim(e) + 1, pulou);
  }
  free(j);
  return n;
}

int main(void) {
  unsigned char d[32];
  char h[65], prims[1 << 12], *j;
  int pulou = 0, n, cont = 0;
  const char *back = getenv("OPENSEEK_VECTORS");

  // Primitivas (FIPS 180-4, RFC 4231, RFC 7914 §11) contra o json.
  j = ler("tests/openseek_vectors.json");
  assert(j);
  {
    const char *p = strstr(j, "\"primitives\"");
    assert(p);
    snprintf(prims, sizeof prims, "%.*s", (int)(js_fim(strchr(p, '{')) + 1 - p), p);
  }
  free(j);
#define CONF(nome, expr) do { expr; hexDe(d, 32, h); char e_[80]; \
    assert(js_texto(prims, NULL, nome, e_, sizeof e_)); assert(!strcmp(h, e_)); } while (0)
  CONF("sha256_empty", osp1_sha256("", 0, d));
  CONF("sha256_abc", osp1_sha256("abc", 3, d));
  CONF("sha256_448", osp1_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, d));
  {
    Osp1Sha s; static char a[1000];
    memset(a, 'a', sizeof a);
    osp1_sha256_ini(&s);
    for (int i = 0; i < 1000; i++) osp1_sha256_add(&s, a, sizeof a);
    CONF("sha256_million_a", osp1_sha256_fim(&s, d));
  }
  { unsigned char k[20]; memset(k, 0x0b, 20); CONF("hmac_rfc4231_1", osp1_hmac(k, 20, "Hi There", 8, d)); }
  { unsigned char k[131]; memset(k, 0xaa, 131);
    const char *m = "Test Using Larger Than Block-Size Key - Hash Key First";
    CONF("hmac_long_key", osp1_hmac(k, 131, m, strlen(m), d)); }
  CONF("pbkdf2_password_salt_1", assert(osp1_pbkdf2("password", 8, "salt", 4, 1, d, NULL, NULL)));
  CONF("pbkdf2_password_salt_4096", assert(osp1_pbkdf2("password", 8, "salt", 4, 4096, d, NULL, NULL)));

  // Codigo: alfabeto do contrato, 8 simbolos, espacos/hifens fora.
  { char c[16];
    assert(osp1_normalizar_codigo("k7qf-m2xd", c) && !strcmp(c, "K7QFM2XD"));
    assert(osp1_normalizar_codigo(" K7QF M2XD ", c) && !strcmp(c, "K7QFM2XD"));
    assert(!osp1_normalizar_codigo("K7QF-M2X", c) && !c[0]);       // 7
    assert(!osp1_normalizar_codigo("K7QF-M2XDD", c));               // 9
    assert(!osp1_normalizar_codigo("K7QF-M2X0", c));                // 0 nao existe
    assert(!osp1_normalizar_codigo("K7QF-M2XI", c));                // I nao existe
    assert(!osp1_normalizar_codigo("K7QF_M2XD", c));
    assert(!osp1_normalizar_codigo(NULL, c)); }
  // Rebaixamento e teto.
  assert(!osp1_derivar("K7QFM2XD", "00", "11", "sha256//x", 199999, OSP1_ITER_MIN, d, NULL, NULL));
  assert(!osp1_derivar("K7QFM2XD", "00", "11", "sha256//x", OSP1_ITER_MAX + 1, OSP1_ITER_MIN, d, NULL, NULL));
  assert(!osp1_derivar("", "00", "11", "sha256//x", 1000, 1, d, NULL, NULL));
  // Cancelamento no meio (sair do Ajustes durante o PBKDF2).
  assert(!osp1_pbkdf2("p", 1, "s", 1, 600000, d, cancelar, &cont) && cont == 4);
  // Comparacao das provas.
  assert(osp1_iguais("ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01",
                     "AB01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01"));
  assert(!osp1_iguais("ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01",
                      "ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab01ab02"));
  assert(!osp1_iguais("ab", "ab"));

  n = conferirArquivo("tests/openseek_vectors.json", &pulou);
  assert(n == 3 && pulou == 0);
  printf("openseek_osp1: contract vectors %d/3 ok\n", n);
  if (back && *back) {
    pulou = 0;
    n = conferirArquivo(back, &pulou);
    if (n < 0) printf("openseek_osp1: backend vectors not found (%s)\n", back);
    else {
      assert(n > 0);
      printf("openseek_osp1: backend vectors %d ok, %d skipped (unknown shape)\n", n, pulou);
    }
  }
  printf("openseek_osp1: ok\n");
  return 0;
}
