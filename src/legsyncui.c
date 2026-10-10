// Apresentacao do AutoSync (F05): textos traduzidos, rotulos das acoes e o
// PROVEDOR da linha de sincronizacao do seletor de legendas do F04
// (legendasui.h, LegendasSyncProvider). Fica separado de legsync.c para o
// nucleo ser testado sem GL.
#include "legsync.h"
#include "idioma.h"
#include "plrui.h"
#include "player.h"
#include "legendasui.h"
#include <stdio.h>
#include <string.h>

const char *legsync_acao_rotulo(int a) {
  switch (a) {
    case LEGSYNC_ACAO_RAPIDA:   return i18n("R\xc3\xa1pida");
    case LEGSYNC_ACAO_COMPLETA: return i18n("Completa");
    case LEGSYNC_ACAO_DESFAZER: return i18n("Desfazer");
    case LEGSYNC_ACAO_OUTRA:    return i18n("Outra refer\xc3\xaancia");
    case LEGSYNC_ACAO_PARAR:    return i18n("Parar");
    case LEGSYNC_ACAO_AUDIO:    return i18n("Por \xc3\xa1udio");
  }
  return "";
}

static void segundos(int ms, char *dst, unsigned tam) {
  snprintf(dst, tam, "%+.2f s", ms / 1000.0);
  plrui_decimal(dst);
}

// F06: por que "Por audio" nao aparece entre as acoes (o ajuste esta ligado).
static const char *motivoAudioTexto(LegSyncMotivo m) {
  switch (m) {
    case LEGSYNC_M_AUD_PLATAFORMA:  return i18n("Por \xc3\xa1udio: indispon\xc3\xadvel nesta plataforma");
    case LEGSYNC_M_AUD_PASSTHROUGH: return i18n("Por \xc3\xa1udio: indispon\xc3\xadvel com passthrough ligado");
    case LEGSYNC_M_AUD_SEM_AUDIO:   return i18n("Por \xc3\xa1udio: sem \xc3\xa1udio decodificado");
    case LEGSYNC_M_AUD_SEM_FALA:    return i18n("Por \xc3\xa1udio: sem falas claras; nada foi alterado");
    default: return NULL;
  }
}

static void textoBase(const LegSyncVisao *v, char *dst, unsigned tam);

void legsync_texto(const LegSyncVisao *v, char *dst, unsigned tam) {
  const char *a;
  if (!dst || !tam) return;
  textoBase(v, dst, tam);
  // O motivo do audio entra junto do estado, uma vez, quando ele ja nao e o
  // proprio estado (passthrough/plataforma da escuta pedida).
  a = motivoAudioTexto(v->motivoAudio);
  // Em repouso (pronta/desfeita) o motivo do audio SUBSTITUI o estado: as
  // acoes da embutida continuam visiveis entre < >, e o motivo cabe na linha.
  if (a && (v->fase == LEGSYNC_PRONTA || v->fase == LEGSYNC_DESFEITA)) { snprintf(dst, tam, "%s", a); return; }
  if (a && !motivoAudioTexto(v->motivo) && v->fase != LEGSYNC_DEPOIS) {
    size_t n = strlen(dst);
    if (n) snprintf(dst + n, tam > n ? tam - n : 0, " \xc2\xb7 %s", a);
    else snprintf(dst, tam, "%s", a);
  }
}

static void textoBase(const LegSyncVisao *v, char *dst, unsigned tam) {
  char s[32];
  const char *a;
  dst[0] = 0;
  switch (v->fase) {
    case LEGSYNC_DEPOIS:     snprintf(dst, tam, "%s", i18n("Segundo idioma: dispon\xc3\xadvel depois")); return;
    case LEGSYNC_AGUARDANDO: snprintf(dst, tam, "%s", i18n("Aguardando a legenda baixar")); return;
    case LEGSYNC_PRONTA:
      if (!(v->acoes & LEGSYNC_ACAO_RAPIDA) && (v->acoes & LEGSYNC_ACAO_AUDIO)) {
        snprintf(dst, tam, "%s", i18n("Pronta: compara com as falas do \xc3\xa1udio")); return;
      }
      snprintf(dst, tam, "%s", i18n("Pronta: compara com a legenda incorporada do arquivo")); return;
    case LEGSYNC_LENDO:      snprintf(dst, tam, i18n("Lendo a legenda incorporada\xe2\x80\xa6 %d%%"), v->progresso); return;
    case LEGSYNC_ANALISANDO: snprintf(dst, tam, "%s", i18n("Analisando\xe2\x80\xa6")); return;
    case LEGSYNC_ACEITA:     segundos(v->offsetAutoMs, s, sizeof s);
                             snprintf(dst, tam, i18n("Sincronizada: %s"), s); return;
    case LEGSYNC_OUVINDO:    snprintf(dst, tam, i18n("Ouvindo as falas\xe2\x80\xa6 %d%%"), v->progresso); return;
    case LEGSYNC_RECUSADA:
      if ((a = motivoAudioTexto(v->motivo)) != NULL) { snprintf(dst, tam, "%s", a); return; }
      snprintf(dst, tam, "%s", i18n("Sem confian\xc3\xa7" "a suficiente; nada foi alterado")); return;
    case LEGSYNC_PAUSADA:    snprintf(dst, tam, "%s", i18n("Pausada pela busca no v\xc3\xad" "deo; retoma sozinha")); return;
    case LEGSYNC_DESFEITA:   snprintf(dst, tam, "%s", i18n("Corre\xc3\xa7\xc3\xa3o desfeita; o atraso manual continua")); return;
    case LEGSYNC_INDISPONIVEL: break;
  }
  if ((a = motivoAudioTexto(v->motivo)) != NULL) { snprintf(dst, tam, "%s", a); return; }
  switch (v->motivo) {
    case LEGSYNC_M_EMBUTIDA:   snprintf(dst, tam, "%s", i18n("A legenda incorporada j\xc3\xa1 acompanha o v\xc3\xad" "deo")); break;
    case LEGSYNC_M_PLATAFORMA: snprintf(dst, tam, "%s", i18n("Indispon\xc3\xadvel nesta plataforma")); break;
    case LEGSYNC_M_EXTERNA_INCOMPLETA:
      snprintf(dst, tam, "%s", i18n("Legenda externa incompleta; n\xc3\xa3o d\xc3\xa1 para sincronizar")); break;
    case LEGSYNC_M_SEM_REFERENCIA:
      snprintf(dst, tam, "%s", i18n("Indispon\xc3\xadvel: sem legenda de texto indexada no arquivo")); break;
    case LEGSYNC_M_SEM_RANGE:  snprintf(dst, tam, "%s", i18n("Indispon\xc3\xadvel: o servidor n\xc3\xa3o aceita leitura parcial")); break;
    case LEGSYNC_M_REDE:       snprintf(dst, tam, "%s", i18n("Indispon\xc3\xadvel: falha de rede ao ler a refer\xc3\xaancia")); break;
    case LEGSYNC_M_ORCAMENTO:  snprintf(dst, tam, "%s", i18n("Indispon\xc3\xadvel: limite de dados atingido")); break;
    case LEGSYNC_M_SEM_OUTRA:  snprintf(dst, tam, "%s", i18n("Sem outra refer\xc3\xaancia completa neste arquivo")); break;
    default:                   snprintf(dst, tam, "%s", i18n("Escolha uma legenda externa para sincronizar")); break;
  }
}

// --- provedor do seletor (legendasui.c) ----------------------------------------
// R4: UMA linha so, sem menu. A sincronia e automatica (legsync.c); aqui ela
// aparece como estado ("Sincronizando…", "Sincronizada", "Não deu para
// sincronizar", ou "…usando <outra>") e, se houver algo corrigido, "Desfazer".
// Principal e segunda legenda (2.1: o slot 1 tem so o estado, sem acoes), so
// com legenda EXTERNA ativa; nunca em canal ao vivo.
// Onde nao ha como sincronizar nunca (plataforma sem referencia), a linha some.
void legsync_texto_simples(const LegSyncVisao *v, char *dst, unsigned tam) {
  if (!dst || !tam) return;
  dst[0] = 0;
  if (v->fase == LEGSYNC_DESFEITA) {
    snprintf(dst, tam, "%s", i18n("Corre\xc3\xa7\xc3\xa3o desfeita; o atraso manual continua")); return;
  }
  if (v->autoFase == 3) { snprintf(dst, tam, "%s", i18n("N\xc3\xa3o deu para sincronizar")); return; }
  if (v->fase == LEGSYNC_ACEITA || v->autoFase == 2) {
    if (v->autoTrocou && v->autoNome[0]) snprintf(dst, tam, i18n("N\xc3\xa3o deu para sincronizar \xe2\x80\x94 usando %s"), v->autoNome);
    else snprintf(dst, tam, "%s", i18n("Sincronizada"));
    return;
  }
  switch (v->fase) {
    case LEGSYNC_AGUARDANDO: case LEGSYNC_PRONTA: case LEGSYNC_LENDO: case LEGSYNC_ANALISANDO:
    case LEGSYNC_OUVINDO: case LEGSYNC_PAUSADA:
      snprintf(dst, tam, "%s", i18n("Sincronizando\xe2\x80\xa6")); return;
    case LEGSYNC_RECUSADA:
      snprintf(dst, tam, "%s", i18n("N\xc3\xa3o deu para sincronizar")); return;
    default: break;
  }
  if (v->autoFase == 1) snprintf(dst, tam, "%s", i18n("Sincronizando\xe2\x80\xa6"));
}

// The pill used to show the green check for every outcome: "Legenda aplicada ·
// X", plus a small "sem ajuste de tempo" when the plan gave up. On the owner's
// TV the plan gave up on every film (the 45 s cap fired mid reference read,
// legsync.c), so every film read as "ok" while nothing had been corrected.
// Synced is only what the
// renderer is actually using: an ACCEPTED result in force (legsync_offset_ms
// adds the automatic offset only in that state).
int legsync_pilula_final(const LegSyncVisao *v, const char *provedor, char *dst, unsigned tam) {
  char s[32];
  const char *p = provedor && *provedor ? provedor : "";
  if (!dst || !tam) return 0;
  if (v && v->fase == LEGSYNC_ACEITA) {
    segundos(v->offsetAutoMs, s, sizeof s);
    snprintf(dst, tam, i18n("Legenda sincronizada \xc2\xb7 %s \xc2\xb7 %s"), p, s);
    return 1;
  }
  snprintf(dst, tam, i18n("Legenda aplicada \xc2\xb7 %s \xc2\xb7 n\xc3\xa3o sincronizada"), p);
  return 0;
}

int legsync_pil_passo(LegSyncPil *p, const LegSyncVisao *v, unsigned agora, const char *provedor, char *texto, unsigned tam) {
  int quer, r = 0;
  if (v->autoDesligado) { p->espera = 0; return LEGSYNC_PIL_ZERAR; }
  if (v->falhaDownload && v->autoFase != 1) { p->espera = 0; return LEGSYNC_PIL_BAIXAR; }
  // Fechada, esperando o fim do plano: so o fim reabre. 2/3 = terminou (aceito,
  // desistiu/recusou/sem referencia); 1 segue; 0 = cancelado (sem plano): nada.
  if (p->espera && p->estado == LEGSYNC_PIL_OFF) {
    if (v->autoFase == 2 || v->autoFase == 3) {
      p->espera = 0; p->rastreia = 0;
      p->final = legsync_pilula_final(v, provedor, texto, tam);
      p->estado = LEGSYNC_PIL_APLICADA; p->desde = agora | 1u;
      r |= LEGSYNC_PIL_FINAL_TARDE | (v->autoFase == 2 ? LEGSYNC_PIL_LEMBRAR : 0);
    } else if (v->autoFase != 1) p->espera = 0;
    return r;
  }
  if (p->estado == LEGSYNC_PIL_OFF) return 0;
  if (p->rastreia) {
    if (v->fase == LEGSYNC_AGUARDANDO) quer = LEGSYNC_PIL_PROCURANDO;
    else if (v->autoFase == 1) quer = LEGSYNC_PIL_SINCRONIZANDO;
    else quer = LEGSYNC_PIL_APLICADA;
    // Cada troca baixa de novo. Só o callback pode declarar falha de download.
    if (quer == LEGSYNC_PIL_APLICADA && v->autoFase == 2) r |= LEGSYNC_PIL_LEMBRAR;
    // Leitura longa: a ilha encolhe de volta no relogio e o plano segue; o
    // aviso final reabre quando a sessao terminar.
    if (p->estado == LEGSYNC_PIL_SINCRONIZANDO && quer == LEGSYNC_PIL_SINCRONIZANDO && agora - p->desde >= LEGSYNC_PIL_SINC_MS) {
      p->espera = 1; p->troca = 0; p->estado = LEGSYNC_PIL_OFF;
      return r | LEGSYNC_PIL_ESCONDEU;
    }
    // so avanca (nunca volta a "Procurando" por oscilacao) e respeita o tempo minimo
    if (quer != p->estado && (p->estado == LEGSYNC_PIL_PROCURANDO || (p->estado == LEGSYNC_PIL_SINCRONIZANDO && quer == LEGSYNC_PIL_APLICADA) ||
                              (p->estado == LEGSYNC_PIL_APLICADA && quer == LEGSYNC_PIL_SINCRONIZANDO && p->troca)) &&
        agora - p->desde >= LEGSYNC_PIL_MIN_MS) {
      p->estado = quer; p->desde = agora | 1u;
    }
    // O texto final sai do estado REAL ("sincronizada" so com o offset aceito).
    if (p->estado == LEGSYNC_PIL_APLICADA) {
      int antes = p->final;
      p->final = legsync_pilula_final(v, provedor, texto, tam);
      if (p->final && !antes) p->desde = agora | 1u;
    }
  }
  if (p->estado == LEGSYNC_PIL_APLICADA && agora - p->desde >= (p->final == 0 ? LEGSYNC_PIL_SEMSYNC_MS : LEGSYNC_PIL_APLICADA_MS))
    r |= LEGSYNC_PIL_ZERAR;
  return r;
}

static int acoesAgora(int slot, int *lista, int max) {
  LegSyncVisao v;
  int n = 0;
  if (slot != 0 || max < 1) return 0;
  v = legsync_visao(0);
  if (v.autoDesligado) {
    static const int a[] = { LEGSYNC_ACAO_RAPIDA, LEGSYNC_ACAO_COMPLETA, LEGSYNC_ACAO_AUDIO,
                            LEGSYNC_ACAO_DESFAZER, LEGSYNC_ACAO_OUTRA, LEGSYNC_ACAO_PARAR };
    for (unsigned i = 0; i < sizeof a / sizeof *a && n < max; i++)
      if (v.acoes & a[i]) lista[n++] = a[i];
  } else if (v.acoes & LEGSYNC_ACAO_DESFAZER) lista[n++] = LEGSYNC_ACAO_DESFAZER;
  return n;
}

static const char *pEstado(int slot, void *u) {
  static char b[200];
  LegSyncVisao v;
  (void)u;
  if (slot < 0 || slot > 1 || player_id_canal()[0]) return NULL;
  v = legsync_visao(slot);
  if (v.fase == LEGSYNC_INDISPONIVEL && v.autoFase == 0) return NULL;
  if (v.autoDesligado) legsync_texto(&v, b, sizeof b);
  else legsync_texto_simples(&v, b, sizeof b);
  return b[0] ? b : NULL;
}

static int pAcoes(int slot, const char **rot, int max, void *u) {
  int l[6], n = acoesAgora(slot, l, max < 6 ? max : 6), i;
  (void)u;
  for (i = 0; i < n; i++) rot[i] = legsync_acao_rotulo(l[i]);
  return n;
}

static void pExecutar(int slot, int acao, void *u) {
  int l[6], n = acoesAgora(slot, l, 6);
  (void)u;
  if (acao >= 0 && acao < n) legsync_acao(l[acao]);
}

void legsync_ui_ligar(void) {
  static const LegendasSyncProvider p = { pEstado, pAcoes, pExecutar, NULL };
  legendasui_definir_sync(&p);
}
