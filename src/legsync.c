// Ver legsync.h. Nucleo sem SDL/GL: o desenho e os textos ficam em legsyncui.c.
#include "legsync.h"
#include "autosync.h"
#include "audsync.h"
#include "legenda.h"
#include "legenda2.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LS_SESSAO_BYTES  (24LL * 1024 * 1024)  // todas as referencias de uma midia
#define LS_LEITURA_BYTES (12LL * 1024 * 1024)  // uma faixa
#define LS_PEDIDOS       6000
#ifndef LS_RITMO
#define LS_RITMO         8                     // Ranges por segundo, como o mkvass
#endif
#define LS_CALMO_MS      2000u
#define LS_FOLGA_MIN     20.0                  // buffer de video abaixo disto pausa a leitura
#define LS_AUTO_MAX      4                     // R4: a escolhida + ate 3 trocas
#define LS_AUTO_REFS     2                     // R4: faixas de referencia incompletas que ainda vale pular

static pthread_mutex_t M = PTHREAD_MUTEX_INITIALIZER;
static struct {
  int criado;
  AutoSync *sync;
  LegRef *ref;
  LegRefLer lerTeste; void *lerTesteU;
  uint64_t sessao, urlHash;
  char url[4096];                 // privado: nunca vai a log
  // principal
  int primTipo;                   // 0 nenhuma, 1 externa, 2 embutida/nativa
  uint64_t primToken;
  unsigned primGer;
  int falhaDownload;
  int primFase;                   // 0 baixando, 1 pronta e completa, 2 falhou/incompleta
  LegendaDocumento *primDoc;
  char primIdioma[24];
  // referencia
  uint64_t refPedido;
  LegendaDocumento *refDoc;
  int refFaixa, ultimaFaixa;
  int excl[16], nExcl;
  long long bytesSessao;
  LegRefMotivo refMotivo;
  char idiomaRef[24];
  // analise
  int querModo;                   // -1 nada; senao AutoSyncModo a pedir quando houver referencia
  int ultimoModo, solicitou, desfeita, reenviar, semOutra;
  unsigned calmoDesde;
  // F06: referencia de AUDIO (audsync.c). audUltimo = a ultima analise pedida
  // foi por audio (desfazer/aceite/recusa falam dela; "Outra referencia" nao).
  int audLigado, audUltimo;
  uint64_t audPedido;
  AudSyncMotivo audFalha;
  LegendaDocumento *audRef, *audRec;
  // R4: plano automatico. autoEtapa: 0 a iniciar, 1 contra a faixa embutida,
  // 2 contra o audio, 9 encerrado. autoFase: 0 sem plano, 1 trabalhando,
  // 2 sincronizou, 3 nao deu.
  int autoEtapa, autoFase, autoTrocou, autoRefTent, autoPend;
  int autoTroca, autoVolta;       // o proximo primaria_externa vem do trocador
  char autoNome[64];
  uint64_t autoTent[LS_AUTO_MAX + 2]; int nAutoTent;
  unsigned autoDesde;             // ultimo PROGRESSO do plano (teto LS_AUTO_TETO_MS sem progresso)
  long long autoMarca;            // assinatura do progresso visto em autoDesde
  int autoLog;                    // ultima fase que foi ao log
  // Slot 1 (segunda legenda, legenda2.c): a mesma engine contra a mesma
  // referencia embutida, sozinha. segOrig = o documento publicado por
  // legenda2 (identidade); segDoc = a copia na geracao desta sessao.
  LegendaDocumento *segOrig, *segDoc;
  int segPedido;
} L = { .querModo = -1 };
#define LS_AUTO_TETO_MS 45000u
static LegSyncTrocador trocador;
static int autoLigado = 1;
void legsync_teste_auto(int ligado) { legsync_auto_habilitar(ligado); }

static uint64_t fnv(const char *s) {
  uint64_t h = UINT64_C(14695981039346656037);
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= UINT64_C(1099511628211); }
  return h;
}
uint64_t legsync_hash_url(const char *url) { return fnv(url); }
void legsync_definir_trocador(LegSyncTrocador t) {
  pthread_mutex_lock(&M); trocador = t; pthread_mutex_unlock(&M);
}

static void soltarRef(void) {
  legenda_documento_liberar(L.refDoc); L.refDoc = NULL; L.refFaixa = 0;
}

static void zerarAnalise(void) {
  L.querModo = -1; L.solicitou = L.desfeita = L.reenviar = L.semOutra = 0;
}

// F06: para a escuta e solta a referencia de audio. Com M. audsync tem lock
// proprio e nunca chama de volta para ca.
static void pararAudio(void) {
  if (L.audPedido) audsync_cancelar();
  L.audPedido = 0; L.audUltimo = 0; L.audFalha = AUDSYNC_M_OK;
  legenda_documento_liberar(L.audRef); legenda_documento_liberar(L.audRec);
  L.audRef = L.audRec = NULL;
}

// Geracao nova para a MESMA escolha de legenda (troca de fonte no meio da
// sessao) ou para um titulo novo (manterPrimaria = 0).
static void soltarSegunda(void) {
  legenda_documento_liberar(L.segOrig); legenda_documento_liberar(L.segDoc);
  L.segOrig = L.segDoc = NULL; L.segPedido = 0;
  legenda2_definir_offset_auto(0);
}

void legsync_auto_habilitar(int ligado) {
  pthread_mutex_lock(&M);
  if (autoLigado && !ligado && L.criado) {
    if (L.autoFase == 1 || (L.segOrig && !L.solicitou && L.querModo < 0)) {
      legref_cancelar(L.ref); L.refPedido = 0;
      autosync_cancelar(L.sync, 0);
      zerarAnalise(); pararAudio();
    }
    autosync_cancelar(L.sync, 1); soltarSegunda();
    L.autoFase = 0; L.autoEtapa = 9; L.autoPend = 0;
    L.autoTroca = L.autoVolta = 0; L.autoTrocou = 0; L.autoNome[0] = 0;
  }
  autoLigado = ligado != 0;
  pthread_mutex_unlock(&M);
}

static void novaSessao(const char *url, int manterPrimaria) {
  L.sessao++;
  soltarSegunda();
  autosync_iniciar(L.sync, L.sessao);
  legref_cancelar(L.ref);
  L.refPedido = 0; soltarRef();
  L.nExcl = 0; L.bytesSessao = 0; L.refMotivo = LEGREF_OK; L.ultimaFaixa = 0;
  L.idiomaRef[0] = 0;
  zerarAnalise();
  pararAudio();
  // R4: outra midia recomeca o plano; a mesma escolha de legenda tenta de novo.
  L.autoEtapa = 0; L.autoRefTent = 0; L.autoPend = 0; L.autoTroca = L.autoVolta = 0;
  L.autoFase = autoLigado && manterPrimaria && L.primTipo == 1 ? 1 : 0;
  if (!manterPrimaria) { L.autoTrocou = 0; L.autoNome[0] = 0; L.nAutoTent = 0; }
  snprintf(L.url, sizeof L.url, "%s", url ? url : "");
  L.urlHash = fnv(L.url);
  if (!manterPrimaria) {
    L.primToken++; L.primTipo = 0; L.primFase = 0; L.falhaDownload = 0; L.primGer = 0;
    legenda_documento_liberar(L.primDoc); L.primDoc = NULL; L.primIdioma[0] = 0;
  }
}

void legsync_teste_leitor(LegRefLer ler, void *u) {
  pthread_mutex_lock(&M); L.lerTeste = ler; L.lerTesteU = u; pthread_mutex_unlock(&M);
}

static double posicaoSegunda(double pos);
void legsync_iniciar(const char *url) {
  legenda2_definir_tempo(posicaoSegunda);
  pthread_mutex_lock(&M);
  if (!L.criado) {
    // Um fio da engine e um do coletor, ociosos ate haver pedido. Criar nao
    // faz rede nem espera nada: pode rodar no player_abrir.
    L.sync = autosync_criar();
    L.ref = (L.lerTeste || legref_disponivel()) ? legref_criar(L.lerTeste, L.lerTesteU) : NULL;
    L.criado = L.sync != NULL;
    if (!L.criado) { legref_destruir(L.ref); L.ref = NULL; pthread_mutex_unlock(&M); return; }
  }
  novaSessao(url, 0);
  pthread_mutex_unlock(&M);
}

void legsync_encerrar(void) {
  pthread_mutex_lock(&M);
  if (L.criado) novaSessao("", 0);
  pthread_mutex_unlock(&M);
}

void legsync_destruir(void) {
  AutoSync *s; LegRef *r; LegendaDocumento *a, *b;
  pthread_mutex_lock(&M);
  s = L.sync; r = L.ref; a = L.primDoc; b = L.refDoc;
  soltarSegunda();
  L.sync = NULL; L.ref = NULL; L.primDoc = L.refDoc = NULL; L.criado = 0;
  L.primToken++; L.refPedido = 0; L.primTipo = 0;
  pthread_mutex_unlock(&M);
  // Join fora do lock: o fio do download pode estar esperando M em aoBaixar.
  autosync_destruir(s);
  legref_destruir(r);
  legenda_documento_liberar(a); legenda_documento_liberar(b);
  pthread_mutex_lock(&M); pararAudio(); pthread_mutex_unlock(&M);
  audsync_destruir();
}

// --- PRINCIPAL ------------------------------------------------------------------
typedef struct {
  uint64_t token, sessao;
  LegendaDocumentoInfo info;
} Meta;

// Fio do download (legenda.c). Monta o documento FORA do lock e publica so se
// esta escolha ainda e a vigente.
static void aoBaixar(const char *corpo, unsigned g, int vigente, void *u) {
  Meta *m = u; LegendaDocumento *doc = NULL, *velho = NULL;
  if (corpo && vigente) doc = legenda_documento_criar(corpo, &m->info);
  pthread_mutex_lock(&M);
  if (L.criado && m->token == L.primToken) {
    velho = L.primDoc; L.primDoc = NULL;
    L.falhaDownload = !corpo; // recusa do parser de análise não é falha da rede
    if (doc && (legenda_documento_info(doc)->flags & LEGENDA_DOC_COMPLETO)) {
      L.primDoc = legenda_documento_reter(doc); L.primGer = g; L.primFase = 1;
      if (m->sessao == L.sessao) autosync_selecionar(L.sync, 0, doc);
    } else L.primFase = 2;
  }
  pthread_mutex_unlock(&M);
  legenda_documento_liberar(velho);
  legenda_documento_liberar(doc);
  free(m);
}

void legsync_primaria_externa(const char *url, const char *idioma, const char *origem) {
  Meta *m = calloc(1, sizeof *m);
  LegendaDocumento *velho = NULL;
  if (!url || !*url) { free(m); return; }
  pthread_mutex_lock(&M);
  if (m && L.criado) {
    uint64_t h = fnv(url);
    m->token = ++L.primToken; m->sessao = L.sessao;
    m->info.sessao = L.sessao; m->info.flags = LEGENDA_DOC_COMPLETO;
    snprintf(m->info.idioma, sizeof m->info.idioma, "%s", idioma ? idioma : "");
    snprintf(m->info.origem, sizeof m->info.origem, "%s", origem && *origem ? origem : "Addon");
    // Identidade opaca: hash da URL, nunca a URL (pode ser assinada).
    snprintf(m->info.identidade, sizeof m->info.identidade, "addon:%016llx", (unsigned long long)h);
    L.primTipo = 1; L.primFase = 0; L.falhaDownload = 0; L.primGer = 0;
    velho = L.primDoc; L.primDoc = NULL;
    snprintf(L.primIdioma, sizeof L.primIdioma, "%s", m->info.idioma);
    autosync_selecionar(L.sync, 0, NULL);
    // A referencia embutida continua valendo para outra externa da mesma
    // midia; as exclusoes eram do par anterior.
    L.nExcl = 0; zerarAnalise(); pararAudio();
    // R4: a pessoa (ou a automatica de idioma) escolheu: plano novo. Vinda do
    // trocador, o plano continua e so anota a legenda tentada.
    L.autoEtapa = 0; L.autoRefTent = 0; L.autoPend = 0;
    if (L.autoTroca && L.autoVolta) { L.autoFase = 3; L.autoEtapa = 9; L.autoTrocou = 0; L.autoNome[0] = 0; }
    else if (L.autoTroca) {
      L.autoFase = 1; L.autoTrocou = 1; L.autoNome[0] = 0;
      if (L.nAutoTent < LS_AUTO_MAX + 2) L.autoTent[L.nAutoTent++] = h;
    } else {
      L.autoFase = autoLigado ? 1 : 0; L.autoTrocou = 0; L.autoNome[0] = 0;
      L.nAutoTent = 1; L.autoTent[0] = h;
    }
    L.autoTroca = L.autoVolta = 0;
    if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }   // pedido com exclusoes do par antigo
    if (!L.refDoc) L.refMotivo = LEGREF_OK;
  } else { free(m); m = NULL; }
  pthread_mutex_unlock(&M);
  legenda_documento_liberar(velho);
  if (m) legenda_carregar_com(url, aoBaixar, m);
  else legenda_carregar(url);
}

void legsync_primaria_outra(int embutida) {
  LegendaDocumento *velho;
  pthread_mutex_lock(&M);
  L.primToken++; L.primTipo = embutida ? 2 : 0; L.primFase = 0; L.falhaDownload = 0; L.primGer = 0;
  velho = L.primDoc; L.primDoc = NULL;
  if (L.criado) {
    autosync_selecionar(L.sync, 0, NULL);
    legref_cancelar(L.ref); L.refPedido = 0;
  }
  zerarAnalise(); pararAudio();
  L.autoFase = 0; L.autoEtapa = 9; L.autoTrocou = 0; L.autoNome[0] = 0; L.nAutoTent = 0;
  L.autoPend = 0; L.autoTroca = L.autoVolta = 0;
  pthread_mutex_unlock(&M);
  legenda_documento_liberar(velho);
}

// Documento da principal na geracao ATUAL (a fonte pode ter trocado depois
// do download). Com M.
static int primariaAtual(void) {
  const LegendaDocumentoInfo *i = legenda_documento_info(L.primDoc);
  if (!L.primDoc || L.primFase != 1) return 0;
  if (i->sessao != L.sessao) {
    int n = 0; const LegendaCue *v = legenda_documento_dados(L.primDoc, &n);
    LegendaDocumentoInfo ni = *i; LegendaDocumento *d;
    ni.sessao = L.sessao;
    d = legenda_documento_de_cues(v, n, &ni);
    if (!d) return 0;
    legenda_documento_liberar(L.primDoc); L.primDoc = d;
  }
  return 1;
}

static int garantirPrimaria(void) {
  return primariaAtual() && autosync_selecionar(L.sync, 0, L.primDoc);
}

static int dona(void) {
  // O overlay mudou de dono por outro caminho (mkvass, desligar): o
  // documento antigo nao descreve o que esta na tela.
  return L.primTipo == 1 && L.primFase == 1 && L.primDoc && legenda_geracao() == L.primGer;
}

int legsync_offset_ms(int manual) {
  int total = manual;
  unsigned g = legenda_geracao();      // fora de M: legenda.c tem lock proprio
  pthread_mutex_lock(&M);
  if (L.criado && L.primTipo == 1 && L.primFase == 1 && L.primDoc && g == L.primGer &&
      legenda_documento_info(L.primDoc)->sessao == L.sessao) {
    autosync_manual(L.sync, 0, manual);
    total = autosync_offset_ms(L.sync, 0);
  }
  pthread_mutex_unlock(&M);
  return total;
}

// Escala (quadros por segundo) e trechos (corte) nao cabem num offset: o
// overlay pergunta a posicao NA LEGENDA para o instante do video, nas mesmas
// condicoes do offset (so o documento dono do overlay). Sem mapa, `pos`.
double legsync_posicao(double pos) {
  double t = pos;
  unsigned g = legenda_geracao();
  pthread_mutex_lock(&M);
  if (L.criado && L.primTipo == 1 && L.primFase == 1 && L.primDoc && g == L.primGer &&
      legenda_documento_info(L.primDoc)->sessao == L.sessao)
    t = autosync_posicao(L.sync, 0, pos);
  pthread_mutex_unlock(&M);
  return t;
}

// Slot 1: o mapa so vale para o documento que a engine julgou; outra escolha
// de segunda legenda ainda nao vista por segundaPasso fica sem mapa.
static double posicaoSegunda(double pos) {
  double t = pos;
  LegendaDocumento *d = legenda2_documento();
  pthread_mutex_lock(&M);
  if (L.criado && L.segDoc && d && d == L.segOrig) t = autosync_posicao(L.sync, 1, pos);
  pthread_mutex_unlock(&M);
  legenda_documento_liberar(d);
  return t;
}

// --- REFERENCIA ------------------------------------------------------------------
static void iniciarColeta(void) {
  LegRefOrcamento o;
  long long resta = LS_SESSAO_BYTES - L.bytesSessao;
  if (!L.ref) { L.refMotivo = LEGREF_PLATAFORMA; L.querModo = -1; return; }
  if (resta < 512 * 1024) { L.refMotivo = LEGREF_ORCAMENTO; L.querModo = -1; return; }
  o.maxBytes = resta < LS_LEITURA_BYTES ? resta : LS_LEITURA_BYTES;
  o.maxPedidos = LS_PEDIDOS; o.pedidosPorSeg = LS_RITMO;
  L.refMotivo = LEGREF_OK;
  L.refPedido = legref_pedir(L.ref, L.url, L.sessao, L.primIdioma, L.excl, L.nExcl, &o);
  if (!L.refPedido) { L.refMotivo = LEGREF_SEM_FAIXA; L.querModo = -1; }
}

static int solicitar(int modo) {
  AutoSyncConfig c = autosync_config(modo == AUTOSYNC_THOROUGH ? AUTOSYNC_THOROUGH : AUTOSYNC_QUICK);
  // Tolerancia: nao ha ajuste em Ajustes; vale o padrao da engine (250 ms).
  if (!L.refDoc || !garantirPrimaria()) return 0;
  if (!autosync_referencia_permitida(L.sync, 0, L.refDoc)) { L.semOutra = 1; return 0; }
  if (!autosync_solicitar(L.sync, 0, L.refDoc, &c)) return 0;
  L.solicitou = 1; L.querModo = -1; L.reenviar = 0; L.desfeita = 0;
  return 1;
}

// F06: o par da MESMA janela ouvida (fala detectada x externa recortada) vai a
// engine sem relaxar nenhum limiar. So um resultado ACEITO muda o offset.
static int solicitarAudio(void) {
  AutoSyncConfig c = autosync_config(AUTOSYNC_QUICK);
  c.raioBuscaMs = AUDSYNC_RAIO_MS;
  if (!L.audRef || !L.audRec) return 0;
  if (!autosync_selecionar(L.sync, 0, L.audRec)) return 0;
  if (!autosync_solicitar(L.sync, 0, L.audRef, &c)) return 0;
  L.solicitou = 1; L.querModo = -1; L.reenviar = 0; L.desfeita = 0;
  return 1;
}

static LegSyncMotivo motivoAudio(void) {
  switch (audsync_capacidade()) {
    case AUDSYNC_CAP_OK: return LEGSYNC_M_NENHUM;
    case AUDSYNC_CAP_PASSTHROUGH: return LEGSYNC_M_AUD_PASSTHROUGH;
    case AUDSYNC_CAP_SEM_AUDIO: return LEGSYNC_M_AUD_SEM_AUDIO;
    default: return LEGSYNC_M_AUD_PLATAFORMA;
  }
}

void legsync_audio_habilitar(int ligado) {
  pthread_mutex_lock(&M);
  if (L.audLigado && !ligado && (L.audPedido || L.audUltimo)) {
    // Desligado no meio: para a escuta; um offset ja aceito fica (desfazer e
    // explicito), mas nada novo e pedido.
    if (L.audPedido) { audsync_cancelar(); L.audPedido = 0; }
    L.audFalha = AUDSYNC_M_OK;
  }
  L.audLigado = ligado != 0;
  pthread_mutex_unlock(&M);
}

void legsync_audio_trocou(void) {
  pthread_mutex_lock(&M);
  if (L.audPedido) { audsync_cancelar(); L.audPedido = 0; L.audUltimo = 0; }
  if (L.audUltimo && autosync_estado(L.sync, 0).estado == AUTOSYNC_ANALYSING) {
    autosync_cancelar(L.sync, 0); L.audUltimo = 0; L.reenviar = 0;
  }
  pthread_mutex_unlock(&M);
}

// Com M. A embutida em curso para: uma referencia por vez.
static int iniciarAudio(void) {
  uint64_t pedido; int ok = 0;
  if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }
  pararAudio();
  zerarAnalise();
  L.audUltimo = 1;
  pedido = audsync_pedir(L.sessao, L.primDoc, AUDSYNC_ALVO_SEG, AUDSYNC_RAIO_MS);
  {
    AudSyncStatus st = audsync_status();
    if (st.pedido == pedido && st.fase == AUDSYNC_FALHOU) L.audFalha = st.motivo;
    else { L.audPedido = pedido; ok = 1; }
  }
  return ok;
}

static int audioPossivel(void) {
  return L.audLigado && motivoAudio() == LEGSYNC_M_NENHUM && primariaAtual();
}

// R4: a legenda atual nao sincroniza. Pede outra do mesmo idioma ao trocador
// (executado fora do lock, no fim de legsync_passo); acabadas as trocas, volta
// a escolha original. Com M.
static void autoFalhou(int semReferencia) {
  if (semReferencia) { L.autoFase = 3; L.autoEtapa = 9; return; }   // sem referencia nao ha como julgar outra
  if (L.nAutoTent < LS_AUTO_MAX && L.primIdioma[0]) L.autoPend = 1;
  else if (L.nAutoTent > 1) L.autoPend = 2;
  else { L.autoFase = 3; L.autoEtapa = 9; }
}

// R4: o motor do plano. Um passo por quadro, com M. Nunca bloqueia.
static void autoPasso(void) {
  AutoSyncResultado e;
  if (L.autoFase != 1 || L.autoPend) return;
  if (L.primTipo != 1) { L.autoFase = 0; return; }
  if (L.primFase == 2) { autoFalhou(0); return; }          // baixou incompleta: tenta outra
  if (L.primFase == 0) return;                              // ainda baixando
  if (!dona()) { L.autoFase = 0; return; }                  // o overlay mudou de dono
  if (L.autoEtapa == 0) {
    L.desfeita = 0;
    if (L.ref) {
      L.autoEtapa = 1; L.ultimoModo = AUTOSYNC_QUICK;
      if (L.refDoc) { if (!solicitar(AUTOSYNC_QUICK)) autoFalhou(0); }
      else {
        L.querModo = AUTOSYNC_QUICK; iniciarColeta();
        if (!L.refPedido) { L.querModo = -1; if (audioPossivel()) { L.autoEtapa = 2; iniciarAudio(); } else autoFalhou(1); }
      }
    } else if (audioPossivel()) { L.autoEtapa = 2; iniciarAudio(); }
    else { L.autoFase = 3; L.autoEtapa = 9; }
    return;
  }
  e = autosync_estado(L.sync, 0);
  if (L.autoEtapa == 1) {
    if (L.solicitou && e.estado == AUTOSYNC_ACCEPTED) { L.autoFase = 2; L.autoEtapa = 9; return; }
    if (L.solicitou && e.estado == AUTOSYNC_REJECTED) { autoFalhou(e.referenciaInvalida); return; }
    if (!L.refDoc && !L.refPedido && L.refMotivo != LEGREF_OK && L.refMotivo != LEGREF_PARADO) {
      // Faixa de referencia ruim (sem duracao/incompleta): outra faixa do arquivo pode servir.
      if ((L.refMotivo == LEGREF_SEM_DURACAO || L.refMotivo == LEGREF_INCOMPLETO) &&
          !L.semOutra && L.autoRefTent < LS_AUTO_REFS) {
        int f = L.ultimaFaixa, k, ja = 0;
        L.autoRefTent++;
        for (k = 0; k < L.nExcl; k++) if (L.excl[k] == f) ja = 1;
        if (f > 0 && !ja && L.nExcl < 16) L.excl[L.nExcl++] = f;
        L.querModo = AUTOSYNC_QUICK; iniciarColeta();
        return;
      }
      // Arquivo sem faixa de texto, sem Range ou fora do ar: sem referencia. Resta o audio.
      if (audioPossivel()) { L.autoEtapa = 2; iniciarAudio(); }
      else autoFalhou(1);
    }
  } else if (L.autoEtapa == 2) {
    if (L.audPedido) return;
    if (L.solicitou && e.estado == AUTOSYNC_ACCEPTED) { L.autoFase = 2; L.autoEtapa = 9; return; }
    if ((L.solicitou && e.estado == AUTOSYNC_REJECTED) || L.audFalha != AUDSYNC_M_OK) {
      L.autoFase = 3; L.autoEtapa = 9;
    }
  }
}

int legsync_acao(int acao) {
  int ok = 0;
  pthread_mutex_lock(&M);
  if (!L.criado) { pthread_mutex_unlock(&M); return 0; }
  switch (acao) {
    case LEGSYNC_ACAO_RAPIDA: case LEGSYNC_ACAO_COMPLETA: {
      int modo = acao == LEGSYNC_ACAO_COMPLETA ? AUTOSYNC_THOROUGH : AUTOSYNC_QUICK;
      if (!dona() || !L.ref) break;
      pararAudio();
      L.ultimoModo = modo; L.desfeita = 0;
      if (L.refDoc) ok = solicitar(modo);
      else {
        L.querModo = modo; ok = 1;
        if (!L.refPedido) iniciarColeta();
        if (!L.refPedido) ok = 0;
      }
      break; }
    case LEGSYNC_ACAO_AUDIO:
      if (!dona() || !L.audLigado || motivoAudio() != LEGSYNC_M_NENHUM || !primariaAtual()) break;
      ok = iniciarAudio();
      break;
    case LEGSYNC_ACAO_DESFAZER:
      autosync_desfazer(L.sync, 0);       // mantem o manual
      L.desfeita = 1; L.querModo = -1; L.reenviar = 0; ok = 1;
      break;
    case LEGSYNC_ACAO_OUTRA: {
      int f = L.refFaixa ? L.refFaixa : L.ultimaFaixa, k, ja = 0;
      if (!dona() || !L.ref || L.semOutra || L.audUltimo) break;
      if (L.refDoc && L.solicitou) autosync_tentar_outra(L.sync, 0);
      for (k = 0; k < L.nExcl; k++) if (L.excl[k] == f) ja = 1;
      if (f > 0 && !ja && L.nExcl < 16) L.excl[L.nExcl++] = f;
      soltarRef();
      L.solicitou = 0; L.desfeita = 0;
      L.querModo = L.ultimoModo; iniciarColeta();
      ok = L.refPedido != 0;
      break; }
    case LEGSYNC_ACAO_PARAR:
      if (L.audPedido) { audsync_cancelar(); L.audPedido = 0; }
      legref_cancelar(L.ref); L.refPedido = 0;
      autosync_cancelar(L.sync, 0);
      L.querModo = -1; L.reenviar = 0; ok = 1;
      break;
  }
  pthread_mutex_unlock(&M);
  return ok;
}

// SEGUNDA LEGENDA (slot 1). Um passo por quadro, com M. A segunda e externa
// por construcao (legenda2.c); corrige-se contra a mesma faixa embutida da
// principal, lida uma vez por midia. So um ACEITO mexe nela: offset puro vai
// para legenda2_definir_offset_auto, escala/trechos pelo mapa (posicaoSegunda).
static void segundaPasso(int sensivel) {
  LegendaDocumento *d = legenda2_sessao() && legenda2_estado() == LEG2_ATIVA ? legenda2_documento() : NULL;
  if (d != L.segOrig) {
    soltarSegunda();
    L.segOrig = d; d = NULL;
    if (L.segOrig) {
      int n = 0; const LegendaCue *v = legenda_documento_dados(L.segOrig, &n);
      LegendaDocumentoInfo ni = *legenda_documento_info(L.segOrig);
      ni.sessao = L.sessao;
      L.segDoc = n > 0 ? legenda_documento_de_cues(v, n, &ni) : NULL;
      if (!L.segDoc || !autosync_selecionar(L.sync, 1, L.segDoc)) {
        legenda_documento_liberar(L.segDoc); L.segDoc = NULL;
      }
    } else autosync_selecionar(L.sync, 1, NULL);
  }
  legenda_documento_liberar(d);
  if (!L.segDoc) return;
  if (!L.segPedido) {
    if (sensivel) return;
    if (L.refDoc) {
      if (autosync_referencia_permitida(L.sync, 1, L.refDoc) &&
          autosync_solicitar(L.sync, 1, L.refDoc, NULL)) L.segPedido = 1;
      else L.segPedido = 2;   // sem como: fica como esta
    } else if (!L.refPedido && L.ref && L.refMotivo == LEGREF_OK) iniciarColeta();
    return;
  }
  {
    AutoSyncResultado e = autosync_estado(L.sync, 1);
    if (sensivel && e.estado == AUTOSYNC_ANALYSING) { autosync_cancelar(L.sync, 1); L.segPedido = 0; return; }
    legenda2_definir_offset_auto(e.estado == AUTOSYNC_ACCEPTED && e.tipo == AUTOSYNC_T_OFFSET ? autosync_offset_ms(L.sync, 1) : 0);
  }
}

void legsync_passo(const char *url, double pos, double folga, int sensivel, unsigned agora) {
  (void)pos;
  pthread_mutex_lock(&M);
  if (!L.criado) { pthread_mutex_unlock(&M); return; }
  // Troca de fonte no meio da sessao: outra midia, outra geracao. A escolha
  // de legenda da pessoa continua; a referencia e o offset automatico nao.
  if (url && *url && fnv(url) != L.urlHash) novaSessao(url, 1);
  if (L.refPedido) {
    LegRefStatus st = legref_status(L.ref);
    if (st.pedido == L.refPedido) {
      if (st.faixa) { L.ultimaFaixa = st.faixa; snprintf(L.idiomaRef, sizeof L.idiomaRef, "%s", st.idioma); }
      if (st.fase == LEGREF_PRONTO) {
        L.refDoc = legref_tomar(L.ref, L.refPedido);
        L.refFaixa = st.faixa; L.bytesSessao += st.bytes; L.refPedido = 0;
        if (!L.refDoc) { L.refMotivo = LEGREF_INCOMPLETO; L.querModo = -1; }
      } else if (st.fase == LEGREF_INDISPONIVEL || st.fase == LEGREF_CANCELADO) {
        L.refMotivo = st.motivo; L.bytesSessao += st.bytes; L.refPedido = 0; L.querModo = -1;
        if (st.motivo == LEGREF_SEM_FAIXA && L.nExcl > 0) L.semOutra = 1;
      }
    }
  }
  // F06: escuta do audio. Seek/buffer pausa (a janela recomeca depois); a
  // janela pronta vira o par de documentos que vai a engine.
  audsync_passo();
  if (L.audPedido) {
    AudSyncStatus st = audsync_status();
    if (st.pedido != L.audPedido) L.audPedido = 0;
    else if (st.fase == AUDSYNC_PRONTO) {
      LegendaDocumento *r = NULL, *c = NULL;
      if (audsync_tomar(L.audPedido, &r, &c)) {
        legenda_documento_liberar(L.audRef); legenda_documento_liberar(L.audRec);
        L.audRef = r; L.audRec = c;
        L.audPedido = 0;
        if (!sensivel && dona()) solicitarAudio(); else L.reenviar = 1;
      }
    } else if (st.fase == AUDSYNC_FALHOU) {
      L.audFalha = st.motivo; L.audPedido = 0;
      fprintf(stderr, "[legsync] audio_sync rejected reason=%s restarts=%d dropped=%ld\n",
              audsync_motivo(st.motivo), st.reinicios, st.descartados);
    } else audsync_pausar(sensivel);
  }
  // TETO DO PLANO AUTOMATICO, SEM PROGRESSO. Na TV do dono a linha
  // "Sincronizacao automatica" ficava em "Sincronizando…" para sempre quando a
  // referencia nunca chegava (leitura parcial pendurada, audio sem fala): o
  // plano desiste com a legenda aplicada como esta ("Nao deu para
  // sincronizar"). O teto contava 45 s DESDE O INICIO, e a referencia de um
  // longa custa um Range por fala (~1300) a no maximo LS_RITMO por segundo:
  // mais de 2 minutos no melhor caso. O teto vencia SEMPRE no meio da leitura,
  // cancelava o que ja tinha vindo e nenhum filme sincronizava (dono, 04/10:
  // "em todos os filmes ela fala que ta ok e ta fora de sincronia";
  // tests/legsync.c 12g). Agora o relogio recomeca a cada passo real (Range
  // lido, faixa nova, escuta, analise, download): so uma etapa PARADA por
  // LS_AUTO_TETO_MS desiste.
  if (L.autoFase == 1) {
    long long marca = (long long)L.autoEtapa * 1000003LL + L.primFase * 7919LL + (L.refDoc != NULL) * 104729LL +
                      (long long)L.refPedido * 15485863LL + L.solicitou * 31LL + L.nAutoTent * 524287LL;
    if (L.refPedido) { LegRefStatus st = legref_status(L.ref); marca += st.pedidos * 3LL + st.feitos * 131071LL; }
    if (L.audPedido) { AudSyncStatus st = audsync_status(); marca += (long long)st.progresso * 8191LL + st.fase; }
    if (!L.autoDesde || marca != L.autoMarca) { L.autoDesde = agora | 1u; L.autoMarca = marca; }
    else if (agora - L.autoDesde > LS_AUTO_TETO_MS) {
      printf("[legsync] automatico: %u s sem progresso (etapa %d, fase %d, ref=%d, pedido=%d); a legenda fica como esta\n",
             LS_AUTO_TETO_MS / 1000u, L.autoEtapa, L.primFase, L.refDoc != NULL, (int)L.refPedido);
      fflush(stdout);
      L.autoFase = 3; L.autoEtapa = 9; L.autoPend = 0; L.autoDesde = 0;
      if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }
      if (L.audPedido) pararAudio();
      autosync_cancelar(L.sync, 0);
    }
  } else L.autoDesde = 0;
  // Competicao: seek/buffer pausa a leitura; buffer de video curto tambem.
  autoPasso();
  if (autoLigado) segundaPasso(sensivel);
  if (L.autoFase * 3 + L.primFase != L.autoLog) {
    L.autoLog = L.autoFase * 3 + L.primFase;
    printf("[legsync] automatico: %s (etapa %d, legenda %s, referencia %s)\n",
           L.autoFase == 1 ? "sincronizando" : L.autoFase == 2 ? "sincronizada" : L.autoFase == 3 ? "nao deu" : "sem plano",
           L.autoEtapa, L.primFase == 0 ? "baixando" : L.primFase == 2 ? "incompleta" : "baixada",
           L.refDoc ? "lida" : L.refPedido ? "lendo" : L.ref ? "nenhuma" : "indisponivel nesta plataforma");
    fflush(stdout);
  }
  legref_pausar(L.ref, sensivel || (folga >= 0.0 && folga < LS_FOLGA_MIN));
  if (sensivel) {
    AutoSyncResultado e = autosync_estado(L.sync, 0);
    L.calmoDesde = 0;
    if (e.estado == AUTOSYNC_ANALYSING) { autosync_cancelar(L.sync, 0); L.reenviar = 1; }
  } else {
    if (!L.calmoDesde) L.calmoDesde = agora | 1u;
    if (L.audUltimo) {
      if (L.reenviar && L.audRef && agora - L.calmoDesde >= LS_CALMO_MS && dona()) solicitarAudio();
    } else if ((L.reenviar || L.querModo >= 0) && L.refDoc && agora - L.calmoDesde >= LS_CALMO_MS && dona())
      solicitar(L.querModo >= 0 ? L.querModo : L.ultimoModo);
  }
  {
    int pend = L.autoPend, n = L.nAutoTent;
    LegSyncTrocador t = trocador;
    uint64_t tent[LS_AUTO_MAX + 2];
    char idioma[24];
    memcpy(tent, L.autoTent, sizeof tent);
    snprintf(idioma, sizeof idioma, "%s", L.primIdioma);
    L.autoPend = 0;
    if (pend && !t) { L.autoFase = 3; L.autoEtapa = 9; pend = 0; }
    if (pend) L.autoTroca = 1, L.autoVolta = pend == 2;
    pthread_mutex_unlock(&M);
    if (pend) {
      // Fora do lock: o trocador liga a legenda por legsync_primaria_externa.
      char nome[64] = "";
      int ok = t(idioma, tent, n, pend == 2, nome, sizeof nome);
      pthread_mutex_lock(&M);
      if (!ok && L.autoTroca) {
        L.autoTroca = L.autoVolta = 0;
        if (pend == 1 && n > 1) L.autoPend = 2;           // sem outra candidata: volta a original no proximo passo
        else { L.autoFase = 3; L.autoEtapa = 9; }
      } else if (ok && pend == 1 && L.autoTrocou) snprintf(L.autoNome, sizeof L.autoNome, "%s", nome);
      pthread_mutex_unlock(&M);
    }
  }
}

static LegSyncMotivo motivoRef(LegRefMotivo m) {
  switch (m) {
    case LEGREF_PLATAFORMA: return LEGSYNC_M_PLATAFORMA;
    case LEGREF_SEM_RANGE: return LEGSYNC_M_SEM_RANGE;
    case LEGREF_REDE: return LEGSYNC_M_REDE;
    case LEGREF_ORCAMENTO: return LEGSYNC_M_ORCAMENTO;
    default: return LEGSYNC_M_SEM_REFERENCIA;
  }
}

static LegSyncMotivo motivoFalhaAudio(AudSyncMotivo m) {
  switch (m) {
    case AUDSYNC_M_PLATAFORMA: return LEGSYNC_M_AUD_PLATAFORMA;
    case AUDSYNC_M_PASSTHROUGH: return LEGSYNC_M_AUD_PASSTHROUGH;
    case AUDSYNC_M_SEM_FALA: case AUDSYNC_M_CONTINUA: return LEGSYNC_M_AUD_SEM_FALA;
    default: return LEGSYNC_M_CONFIANCA;
  }
}

LegSyncVisao legsync_visao(int slot) {
  LegSyncVisao v; AutoSyncResultado e;
  int audOk, base;
  memset(&v, 0, sizeof v);
  if (slot == 1) {   // segunda legenda: so o estado da engine, sem acoes
    pthread_mutex_lock(&M);
    if (L.criado && L.segDoc) {
      e = autosync_estado(L.sync, 1);
      if (L.segPedido == 1 && e.estado == AUTOSYNC_ACCEPTED) {
        v.fase = LEGSYNC_ACEITA; v.autoFase = 2; v.offsetAutoMs = e.offsetMs;
      } else if (L.segPedido == 1 && e.estado == AUTOSYNC_REJECTED) {
        v.fase = LEGSYNC_RECUSADA; v.motivo = LEGSYNC_M_CONFIANCA; v.autoFase = 3;
      } else if (L.segPedido == 2 || (!L.refDoc && !L.refPedido && (!L.ref || L.refMotivo != LEGREF_OK))) {
        v.fase = LEGSYNC_INDISPONIVEL; v.motivo = L.ref ? motivoRef(L.refMotivo) : LEGSYNC_M_PLATAFORMA;
      } else { v.fase = L.refPedido ? LEGSYNC_LENDO : LEGSYNC_ANALISANDO; v.autoFase = 1; }
      v.offsetTotalMs = legenda2_offset_total();
    } else { v.fase = LEGSYNC_INDISPONIVEL; v.motivo = LEGSYNC_M_SEM_EXTERNA; }
    pthread_mutex_unlock(&M);
    return v;
  }
  if (slot != 0) { v.fase = LEGSYNC_DEPOIS; return v; }
  pthread_mutex_lock(&M);
  if (!L.criado) { v.motivo = LEGSYNC_M_SEM_EXTERNA; goto fim; }
  snprintf(v.idiomaRef, sizeof v.idiomaRef, "%s", L.idiomaRef);
  v.autoFase = L.autoFase; v.autoTrocou = L.autoTrocou;
  v.autoDesligado = !autoLigado; v.falhaDownload = L.falhaDownload;
  snprintf(v.autoNome, sizeof v.autoNome, "%s", L.autoNome);
  // F06: so com o ajuste ligado o audio aparece (oferecido ou com o motivo).
  v.motivoAudio = L.audLigado ? motivoAudio() : LEGSYNC_M_NENHUM;
  audOk = L.audLigado && v.motivoAudio == LEGSYNC_M_NENHUM;
  base = (L.ref ? LEGSYNC_ACAO_RAPIDA | LEGSYNC_ACAO_COMPLETA : 0) | (audOk ? LEGSYNC_ACAO_AUDIO : 0);
  v.audio = L.audUltimo;
  if (L.primTipo == 2) { v.motivo = LEGSYNC_M_EMBUTIDA; goto fim; }
  if (L.primTipo != 1) { v.motivo = LEGSYNC_M_SEM_EXTERNA; goto fim; }
  if (!L.ref && !audOk && !L.audUltimo) { v.motivo = LEGSYNC_M_PLATAFORMA; goto fim; }
  if (L.primFase == 0) { v.fase = LEGSYNC_AGUARDANDO; goto fim; }
  if (L.primFase == 2) { v.motivo = LEGSYNC_M_EXTERNA_INCOMPLETA; goto fim; }
  if (!dona()) { v.motivo = LEGSYNC_M_SEM_EXTERNA; goto fim; }
  e = autosync_estado(L.sync, 0);
  v.offsetTotalMs = autosync_offset_ms(L.sync, 0);
  if (L.audPedido) {
    AudSyncStatus st = audsync_status();
    v.acoes = LEGSYNC_ACAO_PARAR;
    if (st.fase == AUDSYNC_OUVINDO) {
      v.fase = st.pausado ? LEGSYNC_PAUSADA : LEGSYNC_OUVINDO; v.progresso = st.progresso;
    } else v.fase = LEGSYNC_ANALISANDO;   // alinhando / entregando a engine
    goto fim;
  }
  if (L.reenviar) { v.fase = LEGSYNC_PAUSADA; v.acoes = LEGSYNC_ACAO_PARAR; goto fim; }
  if (L.refPedido) {
    LegRefStatus st = legref_status(L.ref);
    v.fase = LEGSYNC_LENDO; v.acoes = LEGSYNC_ACAO_PARAR;
    v.progresso = st.total > 0 ? st.feitos * 100 / st.total : 0;
    goto fim;
  }
  if (e.estado == AUTOSYNC_ANALYSING || (L.querModo >= 0 && L.refDoc)) {
    v.fase = LEGSYNC_ANALISANDO; v.acoes = LEGSYNC_ACAO_PARAR; goto fim;
  }
  if (e.estado == AUTOSYNC_ACCEPTED && !L.desfeita) {
    v.fase = LEGSYNC_ACEITA; v.offsetAutoMs = e.offsetMs;
    v.acoes = LEGSYNC_ACAO_DESFAZER | (L.semOutra || L.audUltimo || !L.ref ? 0 : LEGSYNC_ACAO_OUTRA);
    goto fim;
  }
  v.acoes = base;
  if (L.desfeita) {
    v.fase = LEGSYNC_DESFEITA; v.acoes |= L.semOutra || L.audUltimo || !L.ref ? 0 : LEGSYNC_ACAO_OUTRA; goto fim;
  }
  if (L.audUltimo && L.audFalha != AUDSYNC_M_OK) {
    v.motivo = motivoFalhaAudio(L.audFalha);
    v.fase = v.motivo == LEGSYNC_M_AUD_PASSTHROUGH || v.motivo == LEGSYNC_M_AUD_PLATAFORMA
           ? LEGSYNC_INDISPONIVEL : LEGSYNC_RECUSADA;
    goto fim;
  }
  if (L.semOutra) { v.fase = LEGSYNC_INDISPONIVEL; v.motivo = LEGSYNC_M_SEM_OUTRA; v.acoes = audOk ? LEGSYNC_ACAO_AUDIO : 0; goto fim; }
  if (L.solicitou && e.estado == AUTOSYNC_REJECTED) {
    v.fase = LEGSYNC_RECUSADA; v.motivo = LEGSYNC_M_CONFIANCA;
    if (!L.audUltimo && L.ref) v.acoes |= LEGSYNC_ACAO_OUTRA;
    goto fim;
  }
  if (!L.refDoc && L.refMotivo != LEGREF_OK && L.refMotivo != LEGREF_PARADO) {
    v.fase = LEGSYNC_INDISPONIVEL; v.motivo = motivoRef(L.refMotivo);
    // Falha da FAIXA (sem duracao, incompleta): outra faixa pode servir. Falha
    // do ARQUIVO/servidor: so tentar de novo (rede) ou nada.
    v.acoes = (L.refMotivo == LEGREF_SEM_DURACAO || L.refMotivo == LEGREF_INCOMPLETO) ? LEGSYNC_ACAO_OUTRA
            : L.refMotivo == LEGREF_REDE ? LEGSYNC_ACAO_RAPIDA : 0;
    if (audOk) v.acoes |= LEGSYNC_ACAO_AUDIO;
    goto fim;
  }
  v.fase = LEGSYNC_PRONTA;
fim:
  pthread_mutex_unlock(&M);
  return v;
}
