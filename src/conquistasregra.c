#include "conquistasregra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cqregra_chave_valida(const char *chave) {
  static const char *const pref[] = { "dir:", "ator:", "saga:", "top:", "trakt:", "trilha:" };
  if (!chave) return 0;
  for (size_t i = 0; i < sizeof pref / sizeof *pref; i++) {
    size_t lp = strlen(pref[i]);
    if (strncmp(chave, pref[i], lp)) continue;
    size_t n = 0;
    for (const char *c = chave + lp; *c; c++, n++) {
      if (!((*c >= '0' && *c <= '9') || (*c >= 'A' && *c <= 'Z') || (*c >= 'a' && *c <= 'z') ||
            *c == '.' || *c == '_' || *c == '/' || *c == '-'))
        return 0;
    }
    return n >= 1 && n <= 80;
  }
  return 0;
}

int cqregra_nivel(int vistos, int total, int emOrdem) {
  if (total <= 0 || vistos <= 0) return 0;
  if (vistos > total) vistos = total;
  if (vistos >= total) return emOrdem ? 5 : 4;
  if (vistos * 4 >= total * 3) return 3;
  if (vistos * 2 >= total) return 2;
  return 1;
}

int cqregra_nivel_top(int vistos) {
  return vistos >= 250 ? 4 : vistos >= 100 ? 3 : vistos >= 50 ? 2 : vistos >= 25 ? 1 : 0;
}

int cqregra_faixa(int permil) {
  return permil > 300 ? 0 : permil >= 100 ? 1 : permil >= 10 ? 2 : 3;
}

static int limitar(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

// Nome sem o que quebraria a linha ou o separador. Corta sem partir UTF-8.
static void nome_limpo(const char *in, char *out, size_t tam) {
  size_t n = 0, lim = tam - 1;
  for (; in && *in; in++) {
    unsigned char c = (unsigned char)*in;
    if (c == '\r' || c == '\n' || c == '|') continue;
    if (n >= lim) {
      if ((c & 0xC0) == 0x80) {
        while (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0x80) n--;
        if (n > 0) n--;
      }
      break;
    }
    out[n++] = (char)c;
  }
  out[n] = 0;
}

int cqregra_ler(const char *txt, CqEstado *s) {
  const char *p = txt;
  memset(s, 0, sizeof *s);
  while (p && *p && s->n < CQ_MAX) {
    const char *fim = strchr(p, '\n');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[CQ_CHAVE_MAX + CQ_NOME_MAX + 80];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len);
    linha[len] = 0;
    if (len && linha[len - 1] == '\r') linha[len - 1] = 0;

    char *campo[8];
    int nc = 0;
    campo[nc++] = linha;
    for (char *c = linha; *c && nc < 8; c++)
      if (*c == '|') { *c = 0; campo[nc++] = c + 1; }
    if (nc == 8 && cqregra_chave_valida(campo[0])) {
      CqEntrada *e = &s->e[s->n];
      memset(e, 0, sizeof *e);
      snprintf(e->chave, sizeof e->chave, "%s", campo[0]);
      e->familia = limitar(atoi(campo[1]), 0, CQ_FAM_N - 1);
      e->nivel = limitar(atoi(campo[2]), 0, CQ_NIVEIS);
      e->avisado = limitar(atoi(campo[3]), 0, e->nivel);
      e->vistos = limitar(atoi(campo[4]), 0, 100000);
      e->total = limitar(atoi(campo[5]), 0, 100000);
      e->ganho = atol(campo[6]);
      if (e->ganho < 0) e->ganho = 0;
      nome_limpo(campo[7], e->nome, sizeof e->nome);
      if (!cqregra_buscar(s, e->chave) || cqregra_buscar(s, e->chave) == e) s->n++;
    }
    if (!fim) break;
    p = fim + 1;
  }
  return s->n;
}

int cqregra_serializar(const CqEstado *s, char *dst, size_t tam) {
  size_t off = 0;
  if (!tam) return 0;
  dst[0] = 0;
  for (int i = 0; i < s->n; i++) {
    const CqEntrada *e = &s->e[i];
    char nome[CQ_NOME_MAX + 1];
    nome_limpo(e->nome, nome, sizeof nome);
    int n = snprintf(dst + off, tam - off, "%s|%d|%d|%d|%d|%d|%ld|%s\n", e->chave, e->familia,
                     e->nivel, e->avisado, e->vistos, e->total, e->ganho, nome);
    if (n < 0 || (size_t)n >= tam - off) { dst[0] = 0; return 0; }
    off += (size_t)n;
  }
  return (int)off;
}

const CqEntrada *cqregra_buscar(const CqEstado *s, const char *chave) {
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->e[i].chave, chave)) return &s->e[i];
  return NULL;
}

int cqregra_registrar(CqEstado *s, const char *chave, int familia, const char *nome,
                      int nivel, int vistos, int total, long agora) {
  CqEntrada *e = NULL;
  if (!cqregra_chave_valida(chave)) return -1;
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->e[i].chave, chave)) { e = &s->e[i]; break; }
  if (!e) {
    if (s->n >= CQ_MAX) return -1;
    e = &s->e[s->n++];
    memset(e, 0, sizeof *e);
    snprintf(e->chave, sizeof e->chave, "%s", chave);
    e->familia = limitar(familia, 0, CQ_FAM_N - 1);
  }
  if (nome && *nome) nome_limpo(nome, e->nome, sizeof e->nome);
  e->vistos = limitar(vistos, 0, 100000);
  e->total = limitar(total, 0, 100000);
  nivel = limitar(nivel, 0, CQ_NIVEIS);
  if (nivel <= e->nivel) return 0;
  e->nivel = nivel;
  e->ganho = agora;
  return nivel;
}

int cqregra_pendentes(const CqEstado *s) {
  int n = 0;
  for (int i = 0; i < s->n; i++)
    if (s->e[i].nivel > s->e[i].avisado) n++;
  return n;
}

int cqregra_proximo_aviso(const CqEstado *s) {
  int melhor = -1;
  for (int i = 0; i < s->n; i++)
    if (s->e[i].nivel > s->e[i].avisado && (melhor < 0 || s->e[i].ganho >= s->e[melhor].ganho)) melhor = i;
  return melhor;
}

void cqregra_marcar_avisados(CqEstado *s) {
  for (int i = 0; i < s->n; i++) s->e[i].avisado = s->e[i].nivel;
}
