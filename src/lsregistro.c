// Ver lsregistro.h.
#include "lsregistro.h"
#include <stddef.h>

const char *lsreg_nome_codigo(int c) {
  switch (c) {
    case LSR_UNKNOWN_ERROR:   return "UNKNOWN_ERROR";
    case LSR_OOM:             return "OOM";
    case LSR_PERMISSION:      return "PERMISSION";
    case LSR_DUPLICATE_NAME:  return "DUPLICATE_NAME";
    case LSR_CONNECT_FAILURE: return "CONNECT_FAILURE";
    case LSR_DEPRECATED:      return "DEPRECATED";
    case LSR_NOT_PRIVILEGED:  return "NOT_PRIVILEGED";
    case LSR_NOT_PROXY_PRIV:  return "NOT_PROXY_PRIVILEGED";
    case LSR_PROTOCOL:        return "PROTOCOL_VERSION";
    case LSR_EAGAIN:          return "EAGAIN";
    default:                  return NULL;
  }
}

int lsreg_recusa_da_sessao(int c) {
  return c == LSR_PERMISSION || c == LSR_NOT_PRIVILEGED || c == LSR_PROTOCOL
      || c == LSR_DEPRECATED;
}

static int limite(const LsRegEstado *e) {
  return lsreg_recusa_da_sessao(e->ultimoCodigo) ? 2 : 5;
}

int lsreg_desistiu(const LsRegEstado *e) {
  return e && e->falhas >= limite(e);
}

int lsreg_pode_tentar(const LsRegEstado *e, unsigned agora, int automatico) {
  if (!e || !automatico) return 1;
  if (lsreg_desistiu(e)) return 0;
  // Comparacao por diferenca com sinal: SDL_GetTicks da a volta em 49 dias.
  return e->falhas == 0 || (int)(agora - e->proximaMs) >= 0;
}

void lsreg_falhou(LsRegEstado *e, int codigo, unsigned agora) {
  static const unsigned recuo[] = { 3000u, 8000u, 15000u, 30000u, 60000u };
  int i;
  if (!e) return;
  e->falhas++;
  e->ultimoCodigo = codigo;
  i = e->falhas - 1;
  if (i > 4) i = 4;
  e->proximaMs = agora + recuo[i];
}

void lsreg_deu_certo(LsRegEstado *e) {
  if (!e) return;
  e->falhas = 0; e->ultimoCodigo = 0; e->proximaMs = 0;
}
