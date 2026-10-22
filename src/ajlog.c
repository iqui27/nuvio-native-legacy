#include "ajlog.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#define AJLOG_PEND 8
typedef struct { char chave[64]; char ant[24], novo[24]; int origem; unsigned t; int em_uso; } Pend;
static Pend pend[AJLOG_PEND];
static void (*saida)(const char *) = NULL;

void ajlog_saida(void (*fn)(const char *)) { saida = fn; }

static void emite(const char *l) {
  if (saida) { saida(l); return; }
  printf("%s\n", l);
  fflush(stdout);
}

// "pin" so conta como palavra (pin, pinLocal, perfilPin), nao dentro de "mapping".
static int palavra_pin(const char *b, const char *orig) {
  const char *p = b;
  while ((p = strstr(p, "pin"))) {
    size_t i = (size_t)(p - b);
    int ini = i == 0 || !isalpha((unsigned char)b[i - 1]) || isupper((unsigned char)orig[i]);
    int fim = !orig[i + 3] || !islower((unsigned char)orig[i + 3]);
    if (ini && fim) return 1;
    p += 3;
  }
  return 0;
}

int ajlog_chave_sensivel(const char *chave) {
  static const char *const S[] = { "chave", "key", "token", "senha", "password", "url", "regex",
    "email", "servidor", "server", "endereco", "usuario", "portal", "secret" };
  char b[64];
  size_t i, j;
  if (!chave) return 1;
  for (i = 0; chave[i] && i < sizeof b - 1; i++) b[i] = (char)tolower((unsigned char)chave[i]);
  b[i] = 0;
  for (j = 0; j < sizeof S / sizeof *S; j++) if (strstr(b, S[j])) return 1;
  return palavra_pin(b, chave);
}

static const char *nome_origem(int o) {
  return o == AJLOG_AJUSTES ? "ajustes" : o == AJLOG_CENTRAL ? "central" :
         o == AJLOG_PRIMEIRA ? "primeira" : "outro";
}

static void imprime(Pend *p) {
  char l[220];
  snprintf(l, sizeof l, "[ajustes] mudou %s: %s -> %s (origem=%s)", p->chave, p->ant, p->novo,
           nome_origem(p->origem));
  emite(l);
  p->em_uso = 0;
}

static void registra(const char *chave, const char *ant, const char *novo, AjlogOrigem origem,
                     unsigned agora) {
  int i, livre = -1, velho = 0;
  for (i = 0; i < AJLOG_PEND; i++) {
    if (pend[i].em_uso && !strcmp(pend[i].chave, chave)) {
      if (agora - pend[i].t < AJLOG_RAJADA_MS) {   // mesma rajada: so o valor final muda
        snprintf(pend[i].novo, sizeof pend[i].novo, "%s", novo);
        pend[i].t = agora; pend[i].origem = origem;
        if (!strcmp(pend[i].ant, pend[i].novo)) pend[i].em_uso = 0;   // voltou ao inicio: nada mudou
        return;
      }
      imprime(&pend[i]);   // rajada velha: fecha e comeca outra
    }
  }
  for (i = 0; i < AJLOG_PEND; i++) {
    if (!pend[i].em_uso) { livre = i; break; }
    if (pend[i].t < pend[velho].t) velho = i;
  }
  if (livre < 0) { imprime(&pend[velho]); livre = velho; }
  snprintf(pend[livre].chave, sizeof pend[livre].chave, "%s", chave);
  snprintf(pend[livre].ant, sizeof pend[livre].ant, "%s", ant);
  snprintf(pend[livre].novo, sizeof pend[livre].novo, "%s", novo);
  pend[livre].origem = origem; pend[livre].t = agora; pend[livre].em_uso = 1;
}

void ajlog_mudou(const char *chave, int antigo, int novo, AjlogOrigem origem, unsigned agora) {
  char a[24], n[24];
  if (!chave || !*chave || chave[0] == '-' || antigo == novo) return;
  if (ajlog_chave_sensivel(chave)) { snprintf(a, sizeof a, "<oculto>"); snprintf(n, sizeof n, "<oculto>"); }
  else { snprintf(a, sizeof a, "%d", antigo); snprintf(n, sizeof n, "%d", novo); }
  registra(chave, a, n, origem, agora);
}

void ajlog_mudou_texto(const char *chave, const char *antigo, const char *novo,
                       AjlogOrigem origem, unsigned agora) {
  const char *a = antigo && *antigo ? "<texto>" : "<vazio>";
  const char *n = novo && *novo ? "<texto>" : "<vazio>";
  if (!chave || !*chave) return;
  if (!strcmp(a, n) && antigo && novo && !strcmp(antigo, novo)) return;
  registra(chave, a, n, origem, agora);
}

void ajlog_vazar(unsigned agora) {
  int i;
  for (i = 0; i < AJLOG_PEND; i++)
    if (pend[i].em_uso && agora - pend[i].t >= AJLOG_RAJADA_MS) imprime(&pend[i]);
}

void ajlog_vazar_tudo(void) {
  int i;
  for (i = 0; i < AJLOG_PEND; i++) if (pend[i].em_uso) imprime(&pend[i]);
}
