// Exercise the real Hunt state, queue and sender without a network thread.
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
static int no_thread(pthread_t *t, const pthread_attr_t *a, void *(*fn)(void *), void *u) {
  (void)t; (void)a; (void)fn; (void)u; return 1;
}
#define pthread_create no_thread
#include "../src/caca.c"
#undef pthread_create

static const char *account = "A", *provider = "nuvio", *token = "token-A";
static int profile = 1, captures, posts;
static char sent_auth[3100], sent_via[64], sent_profile[64], sent_body[256];
const char *sessao_usuario(void) { return account; }
int perfis_ativo(void) { return profile; }
int recomenda_alcance(void) { return 1; }
int recomenda_cabecalhos(const char **c, char *a, size_t na, char *v, size_t nv, char *p, size_t np) {
  captures++;
  snprintf(a, na, "Authorization: Bearer %s", token);
  snprintf(v, nv, "X-Nuvio-Auth: %s", provider);
  snprintf(p, np, "X-Nuvio-Perfil: %d", profile);
  c[0] = a; c[1] = v; c[2] = p; c[3] = NULL; return 1;
}
int recomenda_cabecalhos_local(const char **c, char *a, size_t na, char *v, size_t nv, char *p, size_t np) {
  return recomenda_cabecalhos(c, a, na, v, nv, p, np);
}
char *rede_postar_st(const char *u, int t, const char *const *c, const char *b, int *s) {
  (void)u; (void)t; posts++; *s = 200;
  snprintf(sent_auth, sizeof sent_auth, "%s", c[0]);
  snprintf(sent_via, sizeof sent_via, "%s", c[1]);
  snprintf(sent_profile, sizeof sent_profile, "%s", c[2] ? c[2] : "");
  snprintf(sent_body, sizeof sent_body, "%s", b); return NULL;
}
char *dados_ler(const char *name) {
  FILE *f = fopen(name, "rb");
  if (!f) return NULL;
  char *b = calloc(1, 8192); assert(b);
  fread(b, 1, 8191, f); fclose(f); return b;
}
int dados_gravar(const char *name, const char *b) {
  FILE *f = fopen(name, "wb"); assert(f); fputs(b, f); fclose(f); return 1;
}
// Unused network/list/UI paths retained by the production queue and step.
const char *desc_tmdb_idioma(void) { return "en"; }
const char *nuvem_tmdb_chave(void) { return ""; }
const char *i18n(const char *s) { return s; }
unsigned long long cacavisto_geracao(void) { return 0; }
int cacavisto_filme(const char *id, long tmdb, long long *quando) {
  (void)id; (void)tmdb; *quando = 0; return 0;
}
int col_extra_definir_grupo(const char *id, const ColFolder *v, int n) {
  (void)id; (void)v; (void)n; return 1;
}
void ilhaacao_feita(const IlhaAcao *a) { (void)a; }
int main(void) {
  // Old unqualified data must not be assigned to whichever account logs in.
  dados_gravar("cacas-p1.txt", "v1\t1\t99\t-\t0\t15\t1\t8\t1\tLegacy\t-\n");
  caca_iniciar(); assert(caca_n() == 0);
  nSlots = 1;
  Slot *s = &slots[0];
  s->i.tipo = CACA_DIRECAO; s->i.alvo = 42;
  s->i.flags = CACA_F_SEMEADA | CACA_F_FILEIRA;
  s->i.ganhos = CONQ_COMPLETA;
  s->i.vistos = s->i.total = 2;
  strcpy(s->chave, "dir:42"); strcpy(s->i.nome, "A Hunt");
  gravarCacas();
  // A -> logout -> B, same profile number, without an explicit profile hook.
  account = ""; caca_passo(1, 0); assert(caca_n() == 0);
  account = "B"; caca_passo(2, 0); assert(caca_n() == 0);
  caca_iniciar(); assert(caca_n() == 0); // restart must not read A's file
  nSlots = 1;
  slots[0].i.tipo = CACA_DIRECAO; slots[0].i.alvo = 7;
  slots[0].i.flags = CACA_F_PRIVADA;
  strcpy(slots[0].chave, "dir:7");
  gravarCacas(); caca_iniciar();
  assert(caca_n() == 1 && slots[0].i.alvo == 7);
  assert(slots[0].i.ganhos == 0 && slots[0].i.flags == CACA_F_PRIVADA);
  account = "A"; caca_passo(3, 0);
  assert(caca_n() == 1 && slots[0].i.ganhos == CONQ_COMPLETA);
  assert(!(slots[0].i.flags & CACA_F_PRIVADA));
  nFila = 0;
  postarConquista(&slots[0], CONQ_COMPLETA);
  assert(nFila == 1);
  Job queued = fila[0];
  // Switch after enqueue/before dispatch: sender must never read B's headers.
  account = "B"; token = "token-B"; profile = 2;
  int before = captures;
  fazerPostar(&queued);
  assert(posts == 1 && captures == before);
  assert(!strcmp(sent_auth, "Authorization: Bearer token-A"));
  assert(!strcmp(sent_profile, "X-Nuvio-Perfil: 1"));
  // Same Nuvio account/profile, different Trakt account.
  account = "A"; profile = 1; provider = "trakt"; token = "trakt-A";
  nFila = 0; postarConquista(&slots[0], CONQ_COMPLETA);
  queued = fila[0]; token = "trakt-B";
  before = captures; fazerPostar(&queued);
  assert(captures == before && !strcmp(sent_auth, "Authorization: Bearer trakt-A"));
  assert(!strcmp(sent_via, "X-Nuvio-Auth: trakt"));
  // Completed Hunt never unlocks again: Only me must send visivel=0 now.
  nFila = 0;
  assert(caca_alternar(0, CACA_F_PRIVADA));
  assert(nFila == 1); fazerPostar(&fila[0]);
  assert(strstr(sent_body, "\"visivel\":0") && strstr(sent_body, "\"nivel\":4"));
  caca_iniciar();
  assert(slots[0].i.flags & CACA_F_PRIVADA);
  puts("PASS: Hunt account persistence, privacy update and captured credentials");
}
