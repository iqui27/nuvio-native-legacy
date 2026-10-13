// Regra do Resolver. Ver resolverregra.h.
#include "resolverregra.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

unsigned resolverregra_mascara(int sintoma) {
  switch (sintoma) {
    case RES_S_NAO_COMECA: return (1u << RES_T_N) - 1u;
    case RES_S_TRAVA:      return (1u << RES_T_INTERNET) | (1u << RES_T_DEBRID) | (1u << RES_T_FONTE) | (1u << RES_T_TV);
    case RES_S_SOM:        return (1u << RES_T_TV) | (1u << RES_T_AUDIO);
    case RES_S_CONTAS:     return 1u << RES_T_INTERNET;
    default:               return 0;
  }
}

// 1 se o teste `t` falha com estes fatos.
static int falha(int t, const ResolverFatos *f) {
  switch (t) {
    case RES_T_INTERNET: return f->offline;
    case RES_T_ADDONS:   return f->addonsQtd <= 0 || f->addonsMotivo[0];
    case RES_T_DEBRID:   return f->debridMotivo[0] != 0;
    case RES_T_FONTE:    return f->haFalha;
    case RES_T_TV:       return f->tvMotivo[0] != 0;
    case RES_T_AUDIO:    return f->audioMotivo[0] != 0;
  }
  return 0;
}

static int acao_de(int t) {
  switch (t) {
    case RES_T_INTERNET: return RES_ACAO_REPETIR;
    case RES_T_ADDONS: case RES_T_DEBRID: return RES_ACAO_FONTES;
    default: return RES_ACAO_ENVIAR;
  }
}

void resolverregra_testar(int sintoma, const ResolverFatos *f, ResolverResultado *r) {
  unsigned m = resolverregra_mascara(sintoma);
  int t, parou = 0;
  memset(r, 0, sizeof *r);
  r->primeira = -1;
  r->acao = RES_ACAO_NENHUMA;
  for (t = 0; t < RES_T_N; t++) {
    r->testado[t] = (m >> t) & 1u;
    if (!r->testado[t] || parou) continue;
    if (falha(t, f)) {
      r->estado[t] = RES_FALHOU;
      r->primeira = t;
      r->acao = acao_de(t);
      parou = 1;
    } else r->estado[t] = RES_OK;
  }
}

// ------------------------------------------------------------------- falha
static ResolverFalha ultima;
static int temUltima;

static void copia(char *dst, size_t n, const char *s) { snprintf(dst, n, "%s", s ? s : ""); }

void resolverregra_falha_registrar(int etapa, const char *titulo, const char *dica,
                                   const char *midia, long agora) {
  ultima.agora = agora;
  ultima.etapa = etapa;
  copia(ultima.titulo, sizeof ultima.titulo, titulo);
  copia(ultima.dica, sizeof ultima.dica, dica);
  copia(ultima.midia, sizeof ultima.midia, midia);
  temUltima = 1;
}

int resolverregra_falha_recente(ResolverFalha *out, long agora) {
  if (!temUltima || agora - ultima.agora > RES_REJEITA_S) return 0;
  if (out) *out = ultima;
  return 1;
}

void resolverregra_falha_limpar(void) { temUltima = 0; memset(&ultima, 0, sizeof ultima); }

// --------------------------------------------------------------- enviados
static int codigo_ok(const char *c) {
  size_t i, n = strlen(c);
  if (n != 6) return 0;
  for (i = 0; i < n; i++) if (!isalnum((unsigned char)c[i])) return 0;
  return 1;
}

int resolverregra_enviados_ler(const char *txt, long agora, ResolverEnviado *v, int max) {
  int n = 0, i, j;
  const char *p = txt;
  while (p && *p && n < max) {
    char lin[96], c[16];
    long q;
    int s;
    size_t k = strcspn(p, "\n");
    snprintf(lin, sizeof lin, "%.*s", (int)(k < sizeof lin - 1 ? k : sizeof lin - 1), p);
    p += k;
    if (*p == '\n') p++;
    if (sscanf(lin, "%15s %ld %d", c, &q, &s) != 3 || !codigo_ok(c)) continue;
    if (agora - q >= RES_GUARDA_S) continue;     // passou dos 30 dias
    if (s < 0 || s >= RES_S_N) s = RES_S_NAO_COMECA;
    snprintf(v[n].codigo, sizeof v[n].codigo, "%s", c);
    v[n].quando = q;
    v[n].sintoma = s;
    n++;
  }
  for (i = 1; i < n; i++) {            // insercao: pouco item, ordem estavel
    ResolverEnviado e = v[i];
    for (j = i - 1; j >= 0 && v[j].quando < e.quando; j--) v[j + 1] = v[j];
    v[j + 1] = e;
  }
  return n;
}

int resolverregra_enviados_serializar(const ResolverEnviado *v, int n, char *dst, size_t tam) {
  size_t u = 0;
  int i;
  if (tam) dst[0] = 0;
  for (i = 0; i < n; i++) {
    int w = snprintf(dst + u, tam - u, "%s %ld %d\n", v[i].codigo, v[i].quando, v[i].sintoma);
    if (w < 0 || (size_t)w >= tam - u) { if (tam) dst[u] = 0; return 0; }
    u += (size_t)w;
  }
  return (int)u;
}

int resolverregra_enviados_remover(ResolverEnviado *v, int n, const char *codigo) {
  int i, k = 0;
  for (i = 0; i < n; i++) if (strcmp(v[i].codigo, codigo)) v[k++] = v[i];
  return k;
}

int resolverregra_enviados_add(ResolverEnviado *v, int n, int max, const char *codigo, long quando, int sintoma) {
  int i;
  n = resolverregra_enviados_remover(v, n, codigo);
  if (n >= max) n = max - 1;
  for (i = n; i > 0; i--) v[i] = v[i - 1];
  snprintf(v[0].codigo, sizeof v[0].codigo, "%s", codigo);
  v[0].quando = quando;
  v[0].sintoma = sintoma;
  return n + 1;
}

int resolverregra_codigo_exibir(const char *codigo, char *dst, size_t tam) {
  if (tam) dst[0] = 0;
  if (!codigo || strlen(codigo) != 6 || tam < 12) return 0;
  snprintf(dst, tam, "NV-%.4s-%.2s", codigo, codigo + 4);
  return 1;
}

int resolverregra_ha(long s, int *n) {
  if (s < 60) { *n = 0; return RES_HA_AGORA; }
  if (s < 3600) { *n = (int)(s / 60); return RES_HA_MIN; }
  if (s < 86400) { *n = (int)(s / 3600); return RES_HA_H; }
  *n = (int)(s / 86400);
  return RES_HA_DIAS;
}
