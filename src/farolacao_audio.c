// FAROL #16 — executores de audio e memoria. Ver farolacao_audio.h.
#include "farolacao_audio.h"
#include "farol.h"
#include "faroltexto.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "memlog.h"
#include <string.h>

// Implementados em ajustes.c e faixas.c (o app todo os linka; o teste os troca).
int  ajustes_dts_valor(void);
void ajustes_dts_definir(int valor);
int  faixas_audio_compativel(void);
int  faixas_audio_trocar_compativel(void);

// --- ops ---------------------------------------------------------------------
static FarolAcaoAudioOps ops;
static int opsIniciadas;

void farolacao_audio_ops_padrao(FarolAcaoAudioOps *o) {
  if (!o) return;
  o->dts_valor = ajustes_dts_valor;
  o->dts_definir = ajustes_dts_definir;
  o->faixa_compativel = faixas_audio_compativel;
  o->faixa_trocar = faixas_audio_trocar_compativel;
  o->mem_livre_mb = memlog_livre_mb;
  o->mem_aliviar = memlog_aliviar;
}

void farolacao_audio_teste_ops(const FarolAcaoAudioOps *o) {
  if (o) { ops = *o; opsIniciadas = 1; }
  else opsIniciadas = 0;
}

static const FarolAcaoAudioOps *operar(void) {
  if (!opsIniciadas) { farolacao_audio_ops_padrao(&ops); opsIniciadas = 1; }
  return &ops;
}

// --- reabertura ----------------------------------------------------------------
static void (*quandoReabrir)(void);
static int reaberturaPendente;

void farolacao_audio_quando_reabrir(void (*cb)(void)) { quandoReabrir = cb; }

int farolacao_audio_reabertura_consumir(void) {
  int v = reaberturaPendente;
  reaberturaPendente = 0;
  return v;
}

static void pedirReabertura(void) {
  if (quandoReabrir) quandoReabrir();
  else reaberturaPendente = 1;
}

// --- ilha -----------------------------------------------------------------------
static void ilhaFeita(const char *frase, IlhaDesfazer desfazer, const void *ctx, size_t ctxN) {
  IlhaAcao a;
  memset(&a, 0, sizeof a);
  a.icone = "check";
  a.frase = frase;
  a.titulo = "";
  a.onde = "";
  a.tipo = ILHA_ACENTO;
  a.desfazer = desfazer;
  a.ctx = ctx;
  a.ctxN = ctxN;
  ilhaacao_feita(&a);
}

// --- converter -------------------------------------------------------------------
typedef struct { int antes, alvo; } CtxConverter;

// Desfazer: devolve o ajuste ao valor de antes e reabre para o som voltar ao
// que era. So desfaz se o ajuste ainda esta onde o Farol o deixou — se a
// pessoa mexeu depois, a escolha dela vale.
static void desfazerConverter(const void *ctx) {
  const CtxConverter *c = ctx;
  const FarolAcaoAudioOps *o = operar();
  if (!c || o->dts_valor() != c->alvo) return;
  o->dts_definir(c->antes);
  // So reabre se a restauracao pegou de fato (o ajuste pode recusar valores
  // fora do intervalo da build); caso contrario nada mudou.
  if (o->dts_valor() == c->antes) pedirReabertura();
}

static int converter(const FarolFatos *f) {
  const FarolAcaoAudioOps *o = operar();
  CtxConverter c;
  if (!f || f->faixaDts <= 0 || f->conversaoPossivel <= 0 ||
      !farolacao_audio_plataforma_converte(f->plat, f->osMaior))
    return 0;
  c.antes = o->dts_valor();
  c.alvo = farolacao_audio_dts_alvo(f->plat, c.antes);
  if (c.alvo < 0) return 0;
  if (c.alvo != c.antes) {
    o->dts_definir(c.alvo);
    if (o->dts_valor() != c.alvo) return 0;   // o ajuste recusou: nada mudou
  }
  pedirReabertura();
  // Sem mudanca de ajuste nao ha o que desfazer (so reabrimos).
  if (c.alvo != c.antes) ilhaFeita(i18n(FT_ACAO_CONVERTER), desfazerConverter, &c, sizeof c);
  else ilhaFeita(i18n(FT_ACAO_CONVERTER), NULL, NULL, 0);
  return 1;
}

// --- trocar faixa ----------------------------------------------------------------
static int trocarFaixa(const FarolFatos *f) {
  const FarolAcaoAudioOps *o = operar();
  if (!f || f->outraFaixa <= 0) return 0;
  if (o->faixa_compativel() < 0) return 0;      // a consulta ao vivo manda
  if (!o->faixa_trocar()) return 0;
  ilhaFeita(i18n(FT_AUDIO_OUTRA_FAIXA_L2), NULL, NULL, 0);
  return 1;
}

// --- aliviar e reabrir -----------------------------------------------------------
static int aliviarReabrir(void) {
  const FarolAcaoAudioOps *o = operar();
  // A acao SEMPRE alivia (o mesmo callback do aviso do memlog). Depois,
  // memlog_pode_reabrir le de novo — e alivia outra vez se ainda estiver abaixo
  // do piso (150 MB): reabrir o pipeline assim e o que faz o webOS fechar o app.
  // So pede reabertura se autorizar.
  o->mem_aliviar();
  if (!memlog_pode_reabrir(o->mem_livre_mb, o->mem_aliviar)) return 0;
  pedirReabertura();
  ilhaFeita(i18n(FT_MEMORIA_LIBERADA), NULL, NULL, 0);
  return 1;
}

int farolacao_audio_executar(int acao, const FarolFatos *fatos) {
  int ok;
  if (acao != FAROL_A_CONVERTER_AUDIO && acao != FAROL_A_TROCAR_FAIXA &&
      acao != FAROL_A_ALIVIAR_REABRIR)
    return 0;
  farol_registrar_acao(acao);
  if (acao == FAROL_A_CONVERTER_AUDIO) ok = converter(fatos);
  else if (acao == FAROL_A_TROCAR_FAIXA) ok = trocarFaixa(fatos);
  else ok = aliviarReabrir();
  farol_registrar_resultado(acao, ok);
  return ok;
}
