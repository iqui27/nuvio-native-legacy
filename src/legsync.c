// Ver legsync.h. Nucleo sem SDL/GL: o desenho e os textos ficam em legsyncui.c.
#include "legsync.h"
#include "autosync.h"
#include "legenda.h"
#include "legenda2.h"
#include <math.h>
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
// Buffer de video abaixo disto pausa a leitura. O uMS da LG segura ~10 s a
// frente (bufferRange percent 100 = 10 s, C9 12/10): com 20 s a leitura nunca
// andava la. 8 s = so le com o buffer perto de cheio.
#define LS_FOLGA_MIN     8.0
#define LS_AUTO_MAX      2                     // a escolhida + 1 troca (troca demais = legenda piscando)
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
  uint64_t primUrlHash;
  // referencia
  uint64_t refPedido;
  LegendaDocumento *refDoc;
  int refFaixa, ultimaFaixa, refAlt;
  int excl[16], nExcl;
  long long bytesSessao;
  LegRefMotivo refMotivo;
  char idiomaRef[24];
  // analise
  int querModo;                   // -1 nada; senao AutoSyncModo a pedir quando houver referencia
  int ultimoModo, solicitou, desfeita, reenviar, semOutra;
  unsigned calmoDesde;
  char primUrl[1024];             // URL da principal externa (chave da reserva por audio; nunca vai a log)
  // RESERVA POR AUDIO (Android, 06/11; legsync.h). audEtapa: 0 nada,
  // 1 ouvindo junto da embutida (armado), 2 assumiu esta legenda,
  // 3 nao deu. audUltimo = o resultado atual e do audio (pilula, desfazer).
  int audLigado, audUltimo, audEtapa, audDadosMoveis;
  char audChave[1024];            // URL da legenda que o audio sincroniza
  uint64_t audUrlHash;            // primUrlHash dela: o mapa so vale para ela
  LegSyncAudioTrecho audMapa[LEGSYNC_AUDIO_TRECHOS];
  int audN, audEstimado;
  int audTroca;                   // a proxima principal vem da troca pedida pelo audio
  char audTrocaUrl[1024];         // pedido de troca, executado fora do lock
  // Chamadas ao backend pendentes (fim de legsync_passo, fora do lock).
  int pendArmar, pendParar, pendAssumir;
  double posSeg;                  // ultima posicao vista (offset da pilula com mapa de audio)
  // R4: plano automatico. autoEtapa: 0 a iniciar, 1 contra a faixa embutida,
  // 2 contra o audio, 9 encerrado. autoFase: 0 sem plano, 1 trabalhando,
  // 2 sincronizou, 3 nao deu.
  int autoEtapa, autoFase, autoTrocou, autoRefTent, autoPend;
  int autoTroca, autoVolta;       // o proximo primaria_externa vem do trocador
  int autoTrocasEp;               // trocas automaticas neste episodio (no maximo 1)
  int autoOrigN, primN;           // falas da escolha original / da legenda atual
  int manualFase, manualLog;      // LegSyncVisao.manualFase; ultima fase que foi ao log
  char autoNome[64];
  uint64_t autoTent[LS_AUTO_MAX + 2]; int nAutoTent;
  unsigned autoDesde;             // ultimo PROGRESSO do plano (teto LS_AUTO_TETO_MS sem progresso)
  long long autoMarca;            // assinatura do progresso visto em autoDesde
  int autoLog;                    // ultima fase que foi ao log
  AutoSyncMotivo autoFalha;       // ultimo motivo da engine (log/UI)
  // Slot 1 (segunda legenda, legenda2.c): a mesma engine contra a mesma
  // referencia embutida, sozinha. segOrig = o documento publicado por
  // legenda2 (identidade); segDoc = a copia na geracao desta sessao.
  LegendaDocumento *segOrig, *segDoc;
  int segPedido;
} L = { .querModo = -1 };
#define LS_AUTO_TETO_MS 45000u
static LegSyncTrocador trocador;
static LegSyncTrocadorUrl trocadorUrl;
static const LegSyncAudio *audioBackend;
static int autoLigado = 1;
static unsigned manualSeq;   // legsync_manual_seq: lido sem trava pela pilula
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
void legsync_definir_trocador_url(LegSyncTrocadorUrl t) {
  pthread_mutex_lock(&M); trocadorUrl = t; pthread_mutex_unlock(&M);
}
void legsync_audio_backend(const LegSyncAudio *b) {
  pthread_mutex_lock(&M); audioBackend = b; pthread_mutex_unlock(&M);
}
void legsync_audio_dados_moveis(int permitido) {
  pthread_mutex_lock(&M); L.audDadosMoveis = permitido != 0; pthread_mutex_unlock(&M);
}

static void soltarRef(void) {
  legenda_documento_liberar(L.refDoc); L.refDoc = NULL; L.refFaixa = 0; L.refAlt = 0;
}

static void zerarAnalise(void) {
  L.querModo = -1; L.solicitou = L.desfeita = L.reenviar = L.semOutra = 0;
}

// Para a reserva por audio e tira o mapa dela. Com M; o backend ouve o
// pedido no fim de legsync_passo (fora do lock).
static void pararAudio(void) {
  if (L.audEtapa) { L.pendParar = 1; L.pendArmar = L.pendAssumir = 0; }
  L.audEtapa = 0; L.audUltimo = 0; L.audN = 0; L.audEstimado = 0;
  L.audChave[0] = 0; L.audUrlHash = 0; L.audTroca = 0; L.audTrocaUrl[0] = 0;
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
    L.autoFase = 0; L.autoEtapa = 9; L.autoPend = 0; L.autoFalha = AUTOSYNC_OK;
    L.autoTroca = L.autoVolta = 0; L.autoTrocou = 0; L.autoNome[0] = 0;
  }
  autoLigado = ligado != 0;
  pthread_mutex_unlock(&M);
}

static void novaSessao(const char *url, int manterPrimaria) {
  L.sessao++;
  L.manualFase = L.manualLog = 0;
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
  if (!manterPrimaria) { L.autoTrocou = 0; L.autoNome[0] = 0; L.nAutoTent = 0; L.autoTrocasEp = 0; L.autoOrigN = 0; }
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
  pthread_mutex_lock(&M); pararAudio(); L.pendParar = 0; pthread_mutex_unlock(&M);
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
      legenda_documento_dados(doc, &L.primN);
      if (!L.autoTrocou) L.autoOrigN = L.primN;
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
    if (!L.autoTroca && L.primTipo == 1 && L.primUrlHash == h &&
        (L.primFase == 0 || (L.primFase == 1 && legenda_geracao() == L.primGer))) {
      pthread_mutex_unlock(&M); free(m); return;
    }
    L.primUrlHash = h;
    snprintf(L.primUrl, sizeof L.primUrl, "%s", url);
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
    // A troca pedida pelo AUDIO segue com ele (a legenda nova adota o mapa do
    // backend). Qualquer outra escolha para a reserva; o plano novo arma de
    // novo e o audio ja ouvido continua no backend (a fala nao e da legenda).
    L.nExcl = 0; zerarAnalise();
    if (!L.audTroca) pararAudio();
    // R4: a pessoa (ou a automatica de idioma) escolheu: plano novo. Vinda do
    // trocador, o plano continua e so anota a legenda tentada.
    L.autoEtapa = 0; L.autoRefTent = 0; L.autoPend = 0; L.manualFase = L.manualLog = 0;
    L.autoFalha = AUTOSYNC_OK;
    // Volta da original depois da troca recusada: o audio ainda pode assumir
    // (etapa 8: espera a original baixar e decide em autoPasso).
    if (L.audTroca) {
      // Etapa 2 direto: autoPasso entrega a legenda nova ao audio quando baixar.
      L.autoFase = 1; L.autoEtapa = 2; L.autoTrocou = 1; L.autoTrocasEp++;
      if (L.nAutoTent < LS_AUTO_MAX + 2) L.autoTent[L.nAutoTent++] = h;
    } else if (L.autoTroca && L.autoVolta) { L.autoFase = 1; L.autoEtapa = 8; L.autoTrocou = 0; L.autoNome[0] = 0; }
    else if (L.autoTroca) {
      L.autoFase = 1; L.autoTrocou = 1; L.autoNome[0] = 0; L.autoTrocasEp++;
      if (L.nAutoTent < LS_AUTO_MAX + 2) L.autoTent[L.nAutoTent++] = h;
    } else {
      L.autoFase = autoLigado ? 1 : 0; L.autoTrocou = 0; L.autoNome[0] = 0;
      L.nAutoTent = 1; L.autoTent[0] = h;
    }
    L.autoTroca = L.autoVolta = 0; L.audTroca = 0;
    if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }   // pedido com exclusoes do par antigo
    if (!L.refDoc) L.refMotivo = LEGREF_OK;
  } else { free(m); m = NULL; }
  pthread_mutex_unlock(&M);
  legenda_documento_liberar(velho);
  if (m) legenda_carregar_idioma(url, idioma, aoBaixar, m);
  else legenda_carregar_idioma(url, idioma, NULL, NULL);
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
  L.autoPend = 0; L.autoTroca = L.autoVolta = 0; L.manualFase = L.manualLog = 0;
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
  // `manual` is the user's delay (positive = subtitle shown LATER). The lookup
  // convention (legenda_cues, assrender, autosync) is the opposite: positive
  // shifts the cue search earlier. Translate once, here: lookup = autoCorr -
  // manual.
  int total = -manual;
  unsigned g = legenda_geracao();      // fora de M: legenda.c tem lock proprio
  pthread_mutex_lock(&M);
  if (L.criado && L.primTipo == 1 && L.primFase == 1 && L.primDoc && g == L.primGer &&
      legenda_documento_info(L.primDoc)->sessao == L.sessao)
    total += autosync_offset_ms(L.sync, 0);
  pthread_mutex_unlock(&M);
  return total;
}

// Escala (quadros por segundo) e trechos (corte) nao cabem num offset: o
// overlay pergunta a posicao NA LEGENDA para o instante do video, nas mesmas
// condicoes do offset (so o documento dono do overlay). Sem mapa, `pos`.
// Mapa do audio (Reshaped: SubtitleSyncModel.delayUsAt): o trecho e o ultimo
// que comeca antes de `pos` (ou o primeiro); legenda = (video - desloc) / escala.
static double posicaoAudio(double pos) {
  const LegSyncAudioTrecho *t = &L.audMapa[0];
  double ms = pos * 1000.0;
  int i;
  for (i = 1; i < L.audN; i++) if ((double)L.audMapa[i].deMs <= ms) t = &L.audMapa[i];
  return t->escala > 0.0 ? (ms - t->deslocMs) / t->escala / 1000.0 : pos;
}

double legsync_posicao(double pos) {
  double t = pos;
  unsigned g = legenda_geracao();
  pthread_mutex_lock(&M);
  if (L.criado && L.primTipo == 1 && L.primFase == 1 && L.primDoc && g == L.primGer &&
      legenda_documento_info(L.primDoc)->sessao == L.sessao)
    t = L.audN > 0 && L.audUrlHash == L.primUrlHash ? posicaoAudio(pos) : autosync_posicao(L.sync, 0, pos);
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

void legsync_audio_habilitar(int ligado) {
  pthread_mutex_lock(&M);
  if (L.audLigado && !ligado) {
    // Desligado no meio: a reserva para e o mapa dela sai (a legenda volta
    // ao tempo do arquivo). Um plano que dependia do audio acaba.
    if (L.audEtapa == 2 && L.autoFase == 1) { L.autoFase = 3; L.autoEtapa = 9; }
    pararAudio();
  }
  L.audLigado = ligado != 0;
  pthread_mutex_unlock(&M);
}

static int audioPossivel(void) {
  return L.audLigado && audioBackend && primariaAtual();
}

// O plano comecou: o audio ouve junto da embutida. Com M.
static void escutarCedo(void) {
  if (L.audEtapa || !audioPossivel()) return;
  L.audEtapa = 1; L.pendArmar = 1; L.pendParar = 0;
  printf("[legsync] automatico: audio escutando junto da embutida\n"); fflush(stdout);
}

// A embutida decidiu (aceitou ou o plano acabou sem audio): a escuta sai. Com M.
static void soltarCedo(void) {
  if (L.audEtapa == 1) pararAudio();
}

// O AUDIO ASSUME (06/11): sem referencia embutida, ou ela recusou (depois da
// troca unica). O backend recebe a legenda da tela; o que ja ouviu conta. Com M.
static int assumirAudio(const char *porque) {
  if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }
  printf("[legsync] automatico: audio assume (%s)%s\n", porque,
         L.audEtapa == 1 ? ", com a escuta em curso" : ""); fflush(stdout);
  zerarAnalise();
  L.autoEtapa = 2; L.audEtapa = 2; L.audUltimo = 1; L.audN = 0; L.audEstimado = 0;
  snprintf(L.audChave, sizeof L.audChave, "%s", L.primUrl);
  L.audUrlHash = L.primUrlHash;
  L.pendAssumir = 1; L.pendArmar = L.pendParar = 0;
  return 1;
}

static int mesmaChave(const char *chave) {
  return chave && L.audEtapa == 2 && L.audChave[0] && !strcmp(chave, L.audChave);
}

void legsync_audio_modelo(const char *chave, const LegSyncAudioTrecho *t, int n, int estimado) {
  int i, k = 0;
  pthread_mutex_lock(&M);
  if (L.criado && mesmaChave(chave)) {
    int antes = L.audN;
    for (i = 0; t && i < n && k < LEGSYNC_AUDIO_TRECHOS; i++)
      if (t[i].escala > 0.5 && t[i].escala < 2.0) L.audMapa[k++] = t[i];
    L.audN = k; L.audEstimado = k > 0 && estimado;
    if (k) {
      printf("[legsync] audio: %s, %d trecho(s), escala %.5f, desloc %.0f ms\n",
             estimado ? "estimativa" : "sincronizada", k, L.audMapa[0].escala, L.audMapa[0].deslocMs);
      if (L.autoEtapa == 2 && (L.autoFase == 1 || L.autoFase == 2)) { L.autoFase = 2; L.autoFalha = AUTOSYNC_OK; }
    } else if (antes) {
      printf("[legsync] audio: estimativa retirada; a legenda volta ao tempo do arquivo\n");
      if (L.autoEtapa == 2 && L.autoFase == 2) L.autoFase = 1;
    }
    fflush(stdout);
    if (L.manualFase == 1 && k) L.manualFase = 2;
  }
  pthread_mutex_unlock(&M);
}

void legsync_audio_falhou(const char *chave, const char *motivo) {
  pthread_mutex_lock(&M);
  if (L.criado && mesmaChave(chave)) {
    printf("[legsync] audio: nao deu (%s)\n", motivo ? motivo : "?"); fflush(stdout);
    L.audEtapa = 3; L.audN = 0;
    if (L.autoEtapa == 2 && L.autoFase == 1) {
      L.autoFase = 3; L.autoEtapa = 9;
      if (L.autoFalha == AUTOSYNC_OK) L.autoFalha = AUTOSYNC_LOW_CONFIDENCE;
    }
  }
  pthread_mutex_unlock(&M);
}

void legsync_audio_trocar(const char *chave, const char *url) {
  pthread_mutex_lock(&M);
  if (L.criado && mesmaChave(chave) && url && url[0]) {
    // A regra da legenda piscando (12/10): uma troca automatica por episodio.
    if (L.autoTrocasEp == 0 && trocadorUrl) snprintf(L.audTrocaUrl, sizeof L.audTrocaUrl, "%s", url);
    else { printf("[legsync] audio: outra legenda encaixa, mas ja houve troca neste episodio\n"); fflush(stdout); }
  }
  pthread_mutex_unlock(&M);
}

// R4: a legenda atual nao sincroniza. Pede outra do mesmo idioma ao trocador
// (executado fora do lock, no fim de legsync_passo); acabadas as trocas, volta
// a escolha original. Com M.
// Regra (12/10, legenda piscando na C9): a automatica nunca tira da tela a
// legenda que a pessoa ve so porque nao conseguiu sincronizar. So troca depois
// de TENTAR e a referencia RECUSAR, uma vez por episodio; a candidata com menos
// da metade das falas da original devolve a original (autoPasso).
static void autoEncerrar(const char *motivo) {
  soltarCedo();
  L.autoFase = 3; L.autoEtapa = 9;
  if (L.autoFalha == AUTOSYNC_OK) L.autoFalha = AUTOSYNC_LOW_CONFIDENCE;
  printf("[legsync] automatico: encerra sem trocar (%s)\n", motivo); fflush(stdout);
}

static void autoFalhou(int semReferencia) {
  if (semReferencia) {
    // A embutida nao serve de juiz: o audio ainda pode.
    if (audioPossivel()) { assumirAudio("referencia embutida invalida"); return; }
    // Sem referencia nao e "baixa confianca": o log e a pilula dizem o motivo
    // real (a UI de Android/LG segue igual: os dois viram o mesmo texto la).
    if (L.autoFalha == AUTOSYNC_OK) L.autoFalha = AUTOSYNC_NO_REFERENCE;
    autoEncerrar("sem referencia para julgar outra"); return;
  }
  if (L.autoTrocasEp == 0 && L.nAutoTent < LS_AUTO_MAX && L.primIdioma[0]) {
    L.autoPend = 1;
    printf("[legsync] automatico: referencia recusou; troca uma vez\n"); fflush(stdout);
  } else if (audioPossivel()) assumirAudio("embutida recusada");
  else { soltarCedo(); autoEncerrar(L.autoTrocasEp ? "ja trocou neste episodio" : "sem outra do idioma"); }
}

// R4: o motor do plano. Um passo por quadro, com M. Nunca bloqueia.
static void autoPasso(void) {
  AutoSyncResultado e;
  if (L.autoFase != 1 || L.autoPend) return;
  if (L.primTipo != 1) { L.autoFase = 0; return; }
  if (L.primFase == 2) {
    // Nao e "baixa confianca" (06/11): o download falhou (HTTP 429, rede) ou
    // o arquivo nao deu falas. O log diz qual; a pilula segue igual.
    if (L.autoFalha == AUTOSYNC_OK) L.autoFalha = AUTOSYNC_INCOMPLETE;
    autoEncerrar(L.falhaDownload ? "download da legenda falhou; fica a da tela" : "legenda sem falas legiveis; fica a da tela");
    return;
  }
  if (L.primFase == 0) return;                              // ainda baixando
  if (L.autoEtapa == 2) {
    // Troca pedida pelo audio: a legenda nova baixou; o audio a assume e
    // adota o mapa com que decidiu a troca. Fora isso, o audio trabalha (o
    // backend responde por legsync_audio_modelo/falhou).
    if (L.audEtapa == 2 && strcmp(L.audChave, L.primUrl) && dona()) assumirAudio("troca pelo audio");
    return;
  }
  if (L.autoTrocou && L.autoOrigN > 0 && L.primN * 2 < L.autoOrigN) {
    printf("[legsync] automatico: candidata com %d de %d falas; volta a original\n", L.primN, L.autoOrigN);
    fflush(stdout);
    L.autoPend = 2; return;
  }
  if (!dona()) { L.autoFase = 0; return; }                  // o overlay mudou de dono
  if (L.autoEtapa == 8) {
    // A original voltou (a troca nao serviu): o audio assume, ou acaba.
    if (audioPossivel()) assumirAudio("embutida recusou a original e a troca");
    else autoEncerrar("ja trocou neste episodio");
    return;
  }
  if (L.autoEtapa == 0) {
    L.desfeita = 0;
    if (L.ref) {
      L.autoEtapa = 1; L.ultimoModo = AUTOSYNC_QUICK;
      if (L.refDoc) { if (!solicitar(AUTOSYNC_QUICK)) autoEncerrar("pedido nao aceito"); }
      else {
        L.querModo = AUTOSYNC_QUICK; iniciarColeta();
        if (!L.refPedido) { L.querModo = -1; if (audioPossivel()) assumirAudio("sem referencia embutida"); else autoFalhou(1); }
        else escutarCedo();
      }
      if (L.refDoc && L.autoEtapa == 1) escutarCedo();
    } else if (audioPossivel()) assumirAudio("sem referencia embutida");
    else { if (L.autoFalha == AUTOSYNC_OK) L.autoFalha = AUTOSYNC_NO_REFERENCE; autoEncerrar("sem referencia nesta plataforma"); }
    return;
  }
  e = autosync_estado(L.sync, 0);
  if (L.autoEtapa == 1) {
    if (L.solicitou && e.estado == AUTOSYNC_ACCEPTED) { soltarCedo(); L.autoFase = 2; L.autoEtapa = 9; L.autoFalha = AUTOSYNC_OK; return; }
    if (L.solicitou && e.estado == AUTOSYNC_REJECTED) { L.autoFalha = e.motivo; autoFalhou(e.referenciaInvalida); return; }
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
      if (audioPossivel()) assumirAudio("sem referencia embutida");
      else { soltarCedo(); autoFalhou(1); }
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
    case LEGSYNC_ACAO_DESFAZER:
      if (L.audUltimo) pararAudio();      // o mapa do audio sai; o manual fica
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
      if (L.audEtapa == 1 || (L.audEtapa == 2 && !L.audN)) pararAudio();
      legref_cancelar(L.ref); L.refPedido = 0;
      autosync_cancelar(L.sync, 0);
      L.querModo = -1; L.reenviar = 0; ok = 1;
      break;
  }
  if (acao & (LEGSYNC_ACAO_RAPIDA | LEGSYNC_ACAO_COMPLETA | LEGSYNC_ACAO_OUTRA)) {
    // Pedido a mao: a pilula mostra "Sincronizando…" na hora e o resultado no
    // fim; recusado tambem aparece (antes era silencio).
    const char *nome = acao == LEGSYNC_ACAO_RAPIDA ? "rapida" : acao == LEGSYNC_ACAO_COMPLETA ? "completa" : "outra";
    L.manualFase = ok ? 1 : 3; L.manualLog = L.manualFase;
    if (L.autoFase == 1 && ok) { L.autoFase = 0; L.autoEtapa = 9; L.autoPend = 0; }   // a mao assume
    // O menu oferece Completa/Outra DEPOIS que a automatica desistiu
    // (legsyncui.c; todas as plataformas desde 06/11). O resultado desse
    // pedido e o que a linha mostra, nao o "nao deu" antigo da automatica.
    if (L.autoFase == 3 && ok) { L.autoFase = 0; L.autoEtapa = 9; L.autoPend = 0; }
    printf("[legsync] manual: %s -> %s (%s)\n", nome, ok ? "aceita" : "recusada",
           ok ? "trabalhando" : !dona() ? "legenda nao e externa" : !L.ref ? "sem referencia nesta plataforma" :
           L.semOutra ? "sem outra faixa" : "estado atual nao permite");
    fflush(stdout);
    __atomic_add_fetch(&manualSeq, 1u, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&M);
  return ok;
}

unsigned legsync_manual_seq(void) { return __atomic_load_n(&manualSeq, __ATOMIC_ACQUIRE); }

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

// Chamadas ao backend do audio e a troca pedida por ele: fora do lock (o
// backend e a casca Kotlin; o trocador liga a legenda por
// legsync_primaria_externa, que pega M).
static void despacharAudio(void) {
  const LegSyncAudio *b;
  int armar, parar, assumir, moveis;
  LegendaDocumento *doc = NULL;
  LegSyncTrocadorUrl tu;
  char chave[1024], idioma[24], troca[1024];
  pthread_mutex_lock(&M);
  b = audioBackend; tu = trocadorUrl;
  armar = L.pendArmar; parar = L.pendParar; assumir = L.pendAssumir; moveis = L.audDadosMoveis;
  L.pendArmar = L.pendParar = L.pendAssumir = 0;
  if (assumir) {
    if (L.primDoc && L.primFase == 1) doc = legenda_documento_reter(L.primDoc);
    snprintf(chave, sizeof chave, "%s", L.audChave);
    snprintf(idioma, sizeof idioma, "%s", L.primIdioma);
    if (!doc) { assumir = 0; L.pendAssumir = L.audEtapa == 2 && L.primFase == 0; }   // ainda baixando: no proximo passo
  }
  snprintf(troca, sizeof troca, "%s", L.audTrocaUrl);
  L.audTrocaUrl[0] = 0;
  if (troca[0]) L.audTroca = 1;
  pthread_mutex_unlock(&M);
  if (b) {
    if (parar && b->parar) b->parar();
    if (armar && b->armar) b->armar(moveis);
    if (assumir && b->assumir) b->assumir(chave, idioma, doc);
  }
  legenda_documento_liberar(doc);
  if (troca[0]) {
    char nome[64] = "";
    int ok = tu && tu(troca, nome, sizeof nome);
    pthread_mutex_lock(&M);
    if (ok) {
      snprintf(L.autoNome, sizeof L.autoNome, "%s", nome);
      printf("[legsync] audio: trocou para uma legenda que encaixa no audio\n");
    } else {
      L.audTroca = 0;
      printf("[legsync] audio: a legenda que encaixa nao esta mais na lista\n");
    }
    fflush(stdout);
    pthread_mutex_unlock(&M);
  }
}

void legsync_passo(const char *url, double pos, double folga, int sensivel, unsigned agora) {
  pthread_mutex_lock(&M);
  if (!L.criado) { pthread_mutex_unlock(&M); despacharAudio(); return; }
  L.posSeg = pos;
  // Troca de fonte no meio da sessao: outra midia, outra geracao. A escolha
  // de legenda da pessoa continua; a referencia e o offset automatico nao.
  if (url && *url && fnv(url) != L.urlHash) novaSessao(url, 1);
  if (L.refPedido) {
    LegRefStatus st = legref_status(L.ref);
    if (st.pedido == L.refPedido) {
      if (st.faixa) { L.ultimaFaixa = st.faixa; snprintf(L.idiomaRef, sizeof L.idiomaRef, "%s", st.idioma); }
      if (st.fase == LEGREF_PRONTO) {
        L.refDoc = legref_tomar(L.ref, L.refPedido);
        L.refFaixa = st.faixa; L.refAlt = st.alternativas; L.bytesSessao += st.bytes; L.refPedido = 0;
        if (!L.refDoc) { L.refMotivo = LEGREF_INCOMPLETO; L.querModo = -1; }
      } else if (st.fase == LEGREF_INDISPONIVEL || st.fase == LEGREF_CANCELADO) {
        L.refMotivo = st.motivo; L.bytesSessao += st.bytes; L.refPedido = 0; L.querModo = -1;
        if (st.motivo == LEGREF_SEM_FAIXA && L.nExcl > 0) L.semOutra = 1;
      }
    }
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
  // A reserva por audio ouve o filme inteiro (Reshaped): sem teto na etapa 2.
  if (L.autoFase == 1 && L.autoEtapa != 2) {
    long long marca = (long long)L.autoEtapa * 1000003LL + L.primFase * 7919LL + (L.refDoc != NULL) * 104729LL +
                      (long long)L.refPedido * 15485863LL + L.solicitou * 31LL + L.nAutoTent * 524287LL;
    if (L.refPedido) { LegRefStatus st = legref_status(L.ref); marca += st.pedidos * 3LL + st.feitos * 131071LL; }
    // Leitura PAUSADA de proposito (seek/buffer curto, rede lenta: C9 12/10 com
    // 2-12 s de buffer) nao e etapa parada: o teto so corre lendo de fato.
    int pausada = L.refPedido && (sensivel || (folga >= 0.0 && folga < LS_FOLGA_MIN));
    if (!L.autoDesde || marca != L.autoMarca || pausada) { L.autoDesde = agora | 1u; L.autoMarca = marca; }
    else if (agora - L.autoDesde > LS_AUTO_TETO_MS) {
      printf("[legsync] automatico: %u s sem progresso (etapa %d, fase %d, ref=%d, pedido=%d); a legenda fica como esta\n",
             LS_AUTO_TETO_MS / 1000u, L.autoEtapa, L.primFase, L.refDoc != NULL, (int)L.refPedido);
      fflush(stdout);
      autoEncerrar("sem progresso");
      L.autoPend = 0; L.autoDesde = 0;
      if (L.refPedido) { legref_cancelar(L.ref); L.refPedido = 0; }
      autosync_cancelar(L.sync, 0);
    }
  } else L.autoDesde = 0;
  // Competicao: seek/buffer pausa a leitura; buffer de video curto tambem.
  autoPasso();
  if (autoLigado) segundaPasso(sensivel);
  if (L.autoFase * 3 + L.primFase != L.autoLog) {
    L.autoLog = L.autoFase * 3 + L.primFase;
    printf("[legsync] automatico: %s (etapa %d, legenda %s, referencia %s%s%s)\n",
           L.autoFase == 1 ? "sincronizando" : L.autoFase == 2 ? "sincronizada" : L.autoFase == 3 ? "nao deu" : "sem plano",
           L.autoEtapa, L.primFase == 0 ? "baixando" : L.primFase == 2 ? "incompleta" : "baixada",
           L.refDoc ? "lida" : L.refPedido ? "lendo" : L.ref ? "nenhuma" : "indisponivel nesta plataforma",
           L.autoFase == 3 ? ", motivo " : "",
           L.autoFase == 3 ? (L.refMotivo != LEGREF_OK ? legref_motivo(L.refMotivo) : autosync_motivo(L.autoFalha)) : "");
    fflush(stdout);
  }
  legref_pausar(L.ref, sensivel || (folga >= 0.0 && folga < LS_FOLGA_MIN));
  if (sensivel) {
    AutoSyncResultado e = autosync_estado(L.sync, 0);
    L.calmoDesde = 0;
    if (e.estado == AUTOSYNC_ANALYSING) { autosync_cancelar(L.sync, 0); L.reenviar = 1; }
  } else {
    if (!L.calmoDesde) L.calmoDesde = agora | 1u;
    if (!L.audUltimo && (L.reenviar || L.querModo >= 0) && L.refDoc && agora - L.calmoDesde >= LS_CALMO_MS && dona())
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
    if (pend && !t) {
      // Sem trocador (so testes/plataforma sem lista de addons): o audio assume.
      pend = 0;
      if (audioPossivel()) assumirAudio("embutida recusada, sem troca");
      else { soltarCedo(); L.autoFase = 3; L.autoEtapa = 9; }
    }
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
        else if (L.autoFase == 1 && audioPossivel()) assumirAudio("embutida recusada, sem outra legenda do idioma");
        else { soltarCedo(); L.autoFase = 3; L.autoEtapa = 9; }
      } else if (ok && pend == 1 && L.autoTrocou) snprintf(L.autoNome, sizeof L.autoNome, "%s", nome);
      pthread_mutex_unlock(&M);
    }
  }
  despacharAudio();
}

static LegSyncMotivo motivoRef(LegRefMotivo m) {
  switch (m) {
    case LEGREF_PLATAFORMA: return LEGSYNC_M_PLATAFORMA;
    case LEGREF_SEM_RANGE: return LEGSYNC_M_SEM_RANGE;
    case LEGREF_REDE: return LEGSYNC_M_REDE;
    case LEGREF_ORCAMENTO: return LEGSYNC_M_ORCAMENTO;
    case LEGREF_SEM_FAIXA: return LEGSYNC_M_SEM_TEXTO;
    default: return LEGSYNC_M_SEM_REFERENCIA;
  }
}

static LegSyncMotivo motivoFalhaAuto(AutoSyncMotivo m) {
  switch (m) {
    case AUTOSYNC_REGION_DISAGREEMENT: return LEGSYNC_M_REGION_DISAGREEMENT;
    // A pilula diz "sem legenda embutida para comparar" em vez de um
    // generico "nao sincronizada" (todas as plataformas desde 06/11).
    case AUTOSYNC_NO_REFERENCE: return LEGSYNC_M_SEM_REFERENCIA;
    case AUTOSYNC_LOW_CONFIDENCE: case AUTOSYNC_AMBIGUOUS: return LEGSYNC_M_CONFIANCA;
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
        v.fase = LEGSYNC_RECUSADA; v.motivo = motivoFalhaAuto(L.autoFalha); v.autoFase = 3;
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
  v.outrasRef = L.refAlt;
  v.jaCompleta = L.ultimoModo == AUTOSYNC_THOROUGH && L.solicitou;
  audOk = L.audLigado && audioBackend != NULL;
  base = L.ref ? LEGSYNC_ACAO_RAPIDA | LEGSYNC_ACAO_COMPLETA : 0;
  v.audio = L.audUltimo;
  if (L.primTipo == 2) { v.motivo = LEGSYNC_M_EMBUTIDA; goto fim; }
  if (L.primTipo != 1) { v.motivo = LEGSYNC_M_SEM_EXTERNA; goto fim; }
  if (!L.ref && !audOk && !L.audUltimo) { v.motivo = LEGSYNC_M_PLATAFORMA; goto fim; }
  if (L.primFase == 0) { v.fase = LEGSYNC_AGUARDANDO; goto fim; }
  if (L.primFase == 2) { v.motivo = LEGSYNC_M_EXTERNA_INCOMPLETA; goto fim; }
  if (!dona()) { v.motivo = LEGSYNC_M_SEM_EXTERNA; goto fim; }
  e = autosync_estado(L.sync, 0);
  v.offsetTotalMs = autosync_offset_ms(L.sync, 0);
  if (L.audEtapa == 2 && L.audUrlHash == L.primUrlHash) {
    // A reserva por audio cuida desta legenda: sincronizada (o mapa vale; o
    // offset da pilula e o do instante atual), ou ainda ouvindo.
    if (L.audN > 0) {
      v.fase = LEGSYNC_ACEITA; v.acoes = LEGSYNC_ACAO_DESFAZER;
      v.offsetAutoMs = (int)lround((posicaoAudio(L.posSeg) - L.posSeg) * 1000.0);
    } else { v.fase = LEGSYNC_ANALISANDO; v.acoes = LEGSYNC_ACAO_PARAR; }
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
  if (L.audEtapa == 3 && L.audUrlHash == L.primUrlHash) { v.fase = LEGSYNC_RECUSADA; v.motivo = LEGSYNC_M_CONFIANCA; goto fim; }
  if (L.semOutra) { v.fase = LEGSYNC_INDISPONIVEL; v.motivo = LEGSYNC_M_SEM_OUTRA; v.acoes = 0; goto fim; }
  if (L.solicitou && e.estado == AUTOSYNC_REJECTED) {
    v.fase = LEGSYNC_RECUSADA; v.motivo = motivoFalhaAuto(e.motivo);
    if (!L.audUltimo && L.ref) v.acoes |= LEGSYNC_ACAO_OUTRA;
    goto fim;
  }
  if (!L.refDoc && L.refMotivo != LEGREF_OK && L.refMotivo != LEGREF_PARADO) {
    v.fase = LEGSYNC_INDISPONIVEL; v.motivo = motivoRef(L.refMotivo);
    // Falha da FAIXA (sem duracao, incompleta): outra faixa pode servir. Falha
    // do ARQUIVO/servidor: so tentar de novo (rede) ou nada.
    v.acoes = (L.refMotivo == LEGREF_SEM_DURACAO || L.refMotivo == LEGREF_INCOMPLETO) ? LEGSYNC_ACAO_OUTRA
            : L.refMotivo == LEGREF_REDE ? LEGSYNC_ACAO_RAPIDA : 0;
    goto fim;
  }
  v.fase = LEGSYNC_PRONTA;
fim:
  if (v.autoFase == 3 && v.motivo == LEGSYNC_M_NENHUM && L.autoFalha != AUTOSYNC_OK)
    v.motivo = motivoFalhaAuto(L.autoFalha);
  if (L.manualFase == 1) {
    if (v.fase == LEGSYNC_ACEITA) L.manualFase = 2;
    else if (v.fase == LEGSYNC_RECUSADA || v.fase == LEGSYNC_INDISPONIVEL || v.fase == LEGSYNC_DESFEITA) L.manualFase = 3;
  }
  if (L.manualFase != L.manualLog) {
    L.manualLog = L.manualFase;
    printf("[legsync] manual: %s\n", L.manualFase == 2 ? "sincronizada" : "nao deu"); fflush(stdout);
  }
  v.manualFase = L.manualFase;
  pthread_mutex_unlock(&M);
  return v;
}
