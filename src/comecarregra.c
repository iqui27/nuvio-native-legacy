#include "comecarregra.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int comecarregra_nome_limpo(const char *in, char *out, size_t tam) {
  size_t n = 0;
  if (!out || tam == 0) return 0;
  if (in) {
    size_t lim = tam - 1 < COMECAR_NOME_MAX ? tam - 1 : COMECAR_NOME_MAX;
    for (; *in; in++) {
      unsigned char c = (unsigned char)*in;
      if (c == '\r' || c == '\n') continue;
      if (n >= lim) {
        // Passou do teto: se o ultimo byte copiado abriu um caractere
        // multibyte, ele ainda nao esta inteiro. Recuar ate a fronteira.
        break;
      }
      out[n++] = (char)c;
    }
    // Cortou no meio de um UTF-8? Os bytes de continuacao sao 10xxxxxx.
    if (*in && n > 0) {
      if (((unsigned char)*in & 0xC0) == 0x80) {
        while (n > 0 && ((unsigned char)out[n - 1] & 0xC0) == 0x80) n--;
        if (n > 0) n--;   // o byte inicial do caractere partido
      }
    }
  }
  out[n] = 0;
  return (int)n;
}

void comecarregra_ler(const char *txt, ComecarSalvo *s) {
  const char *p = txt;
  memset(s, 0, sizeof *s);
  while (p && *p) {
    const char *fim = strchr(p, '\n');
    size_t len = fim ? (size_t)(fim - p) : strlen(p);
    char linha[160];
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len);
    linha[len] = 0;
    if (len && linha[len - 1] == '\r') linha[len - 1] = 0;
    if (!strncmp(linha, "estado=", 7)) {
      if (!strcmp(linha + 7, "feito")) s->estado = COMECAR_ESTADO_FEITO;
      else if (!strcmp(linha + 7, "andamento")) s->estado = COMECAR_ESTADO_ANDAMENTO;
    } else if (!strncmp(linha, "nome=", 5)) {
      comecarregra_nome_limpo(linha + 5, s->nome, sizeof s->nome);
    } else if (!strncmp(linha, "cor=", 4)) {
      int c = atoi(linha + 4);
      s->cor = (c >= 0 && c < COMECAR_CORES) ? c : 0;
    } else if (!strncmp(linha, "infantil=", 9)) {
      s->infantil = linha[9] == '1';
    }
    if (!fim) break;
    p = fim + 1;
  }
}

int comecarregra_serializar(const ComecarSalvo *s, char *dst, size_t tam) {
  char nome[COMECAR_NOME_MAX + 1];
  const char *est = s->estado == COMECAR_ESTADO_FEITO ? "feito"
                  : s->estado == COMECAR_ESTADO_ANDAMENTO ? "andamento" : "nenhum";
  int n;
  comecarregra_nome_limpo(s->nome, nome, sizeof nome);
  n = snprintf(dst, tam, "estado=%s\nnome=%s\ncor=%d\ninfantil=%d\n", est, nome,
               (s->cor >= 0 && s->cor < COMECAR_CORES) ? s->cor : 0, s->infantil ? 1 : 0);
  return (n > 0 && (size_t)n < tam) ? n : 0;
}

int comecarregra_decidir(const ComecarSalvo *s, int loginNovo) {
  if (s->estado == COMECAR_ESTADO_FEITO) return COMECAR_NADA;
  if (s->estado == COMECAR_ESTADO_ANDAMENTO) return COMECAR_COMPLETO;
  return loginNovo ? COMECAR_COMPLETO : COMECAR_SO_TOUR;
}
