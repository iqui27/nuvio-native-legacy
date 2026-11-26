// Confirmacao do conserto do Farol — ver farolconserto.h (ADENDO 2.1 C, 33).
#include "farolconserto.h"
#include "farol.h"
#include "farolanel.h"
#include "farolregra.h"
#include "faroltexto.h"
#include <stddef.h>

static int acaoValida(int a) {
  return a > FAROL_A_NENHUMA && a < FAROL_A_N && a != FAROL_A_ENVIAR_REGISTRO;
}

int farol_conserto_iniciar(FarolConserto *c, int acao, int acaoAlt,
                           uint32_t agoraMs, uint64_t fronteiraSeq) {
  if (!c || !acaoValida(acao)) return 0;
  if (c->estado == FCS_CONSERTOU || c->estado == FCS_OCIOSO) {
    c->falha = 0;
    c->acoes = 0;
  }
  if (c->falha && c->acoes >= FAROL_CONSERTO_ACOES_MAX) return 0;   // teto duro
  if (!acaoValida(acaoAlt) || acaoAlt == acao) acaoAlt = FAROL_A_NENHUMA;
  // Segunda acao da mesma falha: nao ha mais alternativa a oferecer.
  if (c->falha && c->acoes + 1 >= FAROL_CONSERTO_ACOES_MAX) acaoAlt = FAROL_A_NENHUMA;
  c->falha = 1;
  c->acoes++;
  c->estado = FCS_OBSERVANDO;
  c->acao = acao;
  c->acaoAlt = acaoAlt;
  c->inicioMs = agoraMs;
  c->proxSeq = fronteiraSeq;
  c->pronto = 0;
  c->prontoMs = 0;
  return 1;
}

static int dif(uint32_t a, uint32_t b) { return (int32_t)(a - b); }

// 1 se o video ja andou o tempo todo apos o ultimo FE_PRONTO sem sair do prazo.
static int andouTudo(const FarolConserto *c) {
  return c->pronto &&
         dif(c->prontoMs + FAROL_CONSERTO_ANDANDO_MS, c->inicioMs) <= FAROL_CONSERTO_PRAZO_MS;
}

int farol_conserto_passo(FarolConserto *c, uint32_t agoraMs) {
  FarolEvento ev[FAROL_ANEL_N];
  uint64_t seqFim = 0, base;
  int i, n, falhou = 0, ok = 0;
  if (!c || c->estado != FCS_OBSERVANDO) return FCP_NADA;
  n = farolanel_ler_seq(ev, FAROL_ANEL_N, 0, &seqFim);
  base = seqFim - (uint64_t)n;
  for (i = 0; i < n && !falhou && !ok; i++) {
    const FarolEvento *e = &ev[i];
    uint64_t seq = base + (uint64_t)i;
    if (seq < c->proxSeq) continue;
    c->proxSeq = seq + 1;
    if (e->mod == FM_FAROL) continue;
    // O video ja andava os 5 s antes deste evento? Entao o que vem depois nao
    // desfaz a confirmacao.
    if (andouTudo(c) && dif(e->t_ms, c->prontoMs) >= FAROL_CONSERTO_ANDANDO_MS) { ok = 1; break; }
    if (dif(e->t_ms, c->inicioMs) > FAROL_CONSERTO_PRAZO_MS) { falhou = 1; break; }
    switch (e->ev) {
      case FE_PRONTO:
        c->pronto = 1;
        c->prontoMs = e->t_ms;
        break;
      case FE_ERRO_FONTE:
      case FE_ERRO_NATIVO:
      case FE_PIPELINE_MORREU:
        falhou = 1;
        break;
      case FE_PARADO:
        if (e->a >= FAROL_CONSERTO_PARADO_MS) falhou = 1;
        break;
      default:
        break;
    }
  }
  if (!falhou && !ok) {
    if (andouTudo(c) && dif(agoraMs, c->prontoMs) >= FAROL_CONSERTO_ANDANDO_MS) ok = 1;
    else if (dif(agoraMs, c->inicioMs) >= FAROL_CONSERTO_PRAZO_MS) falhou = 1;
  }
  if (ok) {
    farol_registrar_consertou(c->acao);
    c->estado = FCS_CONSERTOU;
    c->falha = 0;
    c->acoes = 0;
    return FCP_CONSERTOU;
  }
  if (!falhou) return FCP_NADA;
  farol_registrar_resultado(c->acao, 0);
  if (c->acoes < FAROL_CONSERTO_ACOES_MAX && acaoValida(c->acaoAlt)) {
    c->acao = c->acaoAlt;
    c->acaoAlt = FAROL_A_NENHUMA;
    c->acoes++;
    c->inicioMs = agoraMs;
    c->proxSeq = seqFim;      // so o que a alternativa produzir daqui em diante
    c->pronto = 0;
    c->prontoMs = 0;
    return FCP_EXECUTAR_ALT;
  }
  c->estado = FCS_ESCALOU;
  c->acaoAlt = FAROL_A_NENHUMA;
  return FCP_ESCALAR;
}

void farol_conserto_cancelar(FarolConserto *c) {
  if (!c) return;
  c->estado = FCS_OCIOSO;
  c->falha = 0;
  c->acoes = 0;
  c->acao = FAROL_A_NENHUMA;
  c->acaoAlt = FAROL_A_NENHUMA;
  c->inicioMs = 0;
  c->proxSeq = 0;
  c->pronto = 0;
  c->prontoMs = 0;
}

int farol_conserto_estado(const FarolConserto *c) { return c ? c->estado : FCS_OCIOSO; }
int farol_conserto_acao(const FarolConserto *c) { return c ? c->acao : FAROL_A_NENHUMA; }
int farol_conserto_acoes(const FarolConserto *c) { return c ? c->acoes : 0; }

const char *farol_conserto_texto(const FarolConserto *c) {
  if (!c) return NULL;
  if (c->estado == FCS_CONSERTOU) return FT_VOLTOU_A_TOCAR;
  if (c->estado == FCS_OBSERVANDO) return FT_OBSERVANDO_VIDEO;
  return NULL;
}
