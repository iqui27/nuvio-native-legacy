#include "mkvass.h"

#ifdef __EMSCRIPTEN__
// Coto do alvo Tizen: o AVPlay desenha a legenda embutida e nao entrega o
// texto; nada a colher. Ver a nota no cabecalho. Manter o coto aqui (e nao
// #ifdef em cada chamador) e o que deixa faixas.c e player.c iguais nos dois
// alvos.
void mkvass_iniciar(const char *url, int numeroFaixa) { (void)url; (void)numeroFaixa; }
void mkvass_passo(double posSeg) { (void)posSeg; }
void mkvass_parar(void) {}
int  mkvass_estado(void) { return MKVASS_OCIOSO; }
int  mkvass_nogo(void) { return 0; }
int  mkvass_ocupado(void) { return 0; }
void mkvass_estatisticas(long *p, long *b, int *c, int *t) {
  if (p) *p = 0; if (b) *b = 0; if (c) *c = 0; if (t) *t = 0;
}
#else

#include "rede.h"
#include "legenda.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <math.h>

// --- parametros --------------------------------------------------------------

// Quanto A FRENTE do playhead colher. 90 s e o alto da faixa que o spike
// sugeriu (60-90): cobre um seek curto para a frente sem esperar, e ainda
// mantem a fila de pedidos pequena.
#ifndef MKVASS_JANELA_SEG
#define MKVASS_JANELA_SEG 90.0
#endif
// Blocos que COMECARAM ate isto atras do playhead ainda entram: uma fala longa
// ou um letreiro de 8 s que comecou antes do seek precisa aparecer.
#define MKVASS_ATRAS_SEG 8.0
// Teto de Ranges por segundo de relogio. O fio de rede compete com o pipeline
// pela MESMA conexao/servidor (ver a nota de 320 KB em mkv.c); 8 pedidos
// pequenos por segundo e uma fracao do que um segmento de video custa.
#ifndef MKVASS_RANGES_POR_SEG
#define MKVASS_RANGES_POR_SEG 8
#endif
// Dois blocos do MESMO Cluster a menos de isto um do outro saem num Range so:
// baixar 64 KB de video no meio custa menos que um round-trip a mais na TV.
#define MKVASS_JUNTAR      (64L * 1024)
// Primeira leitura do arquivo: SeekHead, Info e Tracks moram nos primeiros KB
// em tudo que ffmpeg e mkvmerge produzem. Se o SeekHead apontar para fora, o
// elemento e buscado onde ele diz.
#define MKVASS_CAB         (16L * 1024)
// Janela por bloco: cabecalho do BlockGroup + a linha. Uma fala de fansub tem
// 60-200 bytes; 512 sobra e evita o segundo Range para completar.
#define MKVASS_BLOCO       512L
// Teto do elemento Cues. Um filme de 3 h com cue por bloco de legenda e por
// quadro-chave de video fica em algumas centenas de KB.
#define MKVASS_CUES_MAX    (4L * 1024 * 1024)
// Teto de blocos e de corpo. O mesmo teto de legenda.c (LEG_MAX_CUES): mais
// do que isto o overlay nao aceita de qualquer forma.
#define MKVASS_MAX_PONTOS  8000
#define MKVASS_CORPO_MAX   (2L * 1024 * 1024)
// Falhas de Range seguidas antes de desistir (NOGO_REDE).
#define MKVASS_FALHAS_MAX  5
// Duracao quando o bloco nao traz BlockDuration (SimpleBlock). Raro em
// legenda — ffmpeg e mkvmerge escrevem BlockGroup — mas um valor e melhor que
// um evento de zero segundos que nunca aparece.
#define MKVASS_DUR_PADRAO  3.0

// --- EBML --------------------------------------------------------------------
//
// Os mesmos leitores de mkv.c, repetidos aqui de proposito: mkv.c e "so
// cabecalho" por contrato, e incluir o .c dele para partilhar tres funcoes
// estaticas amarraria os dois modulos pelo pior lado. Sao vinte linhas.

static int larguraDe(unsigned char b) {
  int i;
  for (i = 0; i < 8; i++) if (b & (0x80 >> i)) return i + 1;
  return 0;
}

static unsigned long lerId(const unsigned char *p, long resta, int *usou) {
  int w, i; unsigned long v = 0;
  if (resta < 1) return 0;
  w = larguraDe(p[0]);
  if (w < 1 || w > 4 || resta < w) return 0;
  for (i = 0; i < w; i++) v = (v << 8) | p[i];
  *usou = w;
  return v;
}

// -1 invalido, -2 tamanho desconhecido.
static long lerTam(const unsigned char *p, long resta, int *usou) {
  int w, i, todosUm = 1; unsigned long v;
  if (resta < 1) return -1;
  w = larguraDe(p[0]);
  if (w < 1 || w > 8 || resta < w) return -1;
  v = p[0] & (0xFF >> w);
  if ((unsigned char)(p[0] & (0xFF >> w)) != (unsigned char)(0xFF >> w)) todosUm = 0;
  for (i = 1; i < w; i++) { if (p[i] != 0xFF) todosUm = 0; v = (v << 8) | p[i]; }
  *usou = w;
  if (todosUm) return -2;
  return (long)v;
}

// Vint de DADO (o numero da faixa no Block): mascara removida, sem o caso
// "desconhecido".
static long lerVint(const unsigned char *p, long resta, int *usou) {
  int w, i; unsigned long v;
  if (resta < 1) return -1;
  w = larguraDe(p[0]);
  if (w < 1 || w > 8 || resta < w) return -1;
  v = p[0] & (0xFF >> w);
  for (i = 1; i < w; i++) v = (v << 8) | p[i];
  *usou = w;
  return (long)v;
}

static unsigned long lerUint(const unsigned char *p, long n) {
  unsigned long v = 0; long i;
  if (n < 1 || n > 8) return 0;
  for (i = 0; i < n; i++) v = (v << 8) | p[i];
  return v;
}

// Itera os filhos de um elemento: a cada chamada devolve id, ponteiro para os
// dados e tamanho; 0 quando acabou ou quando algo nao fecha.
typedef struct { const unsigned char *p; long n, o; } Iter;
static int proximo(Iter *it, unsigned long *id, const unsigned char **dados, long *tam) {
  int ui = 0, ut = 0; long t;
  if (it->o >= it->n) return 0;
  *id = lerId(it->p + it->o, it->n - it->o, &ui);
  if (!*id) return 0;
  t = lerTam(it->p + it->o + ui, it->n - it->o - ui, &ut);
  if (t < 0) return 0;
  it->o += ui + ut;
  if (it->o + t > it->n) return 0;
  *dados = it->p + it->o; *tam = t;
  it->o += t;
  return 1;
}

#define ID_EBML        0x1A45DFA3UL
#define ID_SEGMENT     0x18538067UL
#define ID_SEEKHEAD    0x114D9B74UL
#define ID_SEEK        0x4DBBUL
#define ID_SEEKID      0x53ABUL
#define ID_SEEKPOS     0x53ACUL
#define ID_INFO        0x1549A966UL
#define ID_TSSCALE     0x2AD7B1UL
#define ID_TRACKS      0x1654AE6BUL
#define ID_TRACKENTRY  0xAEUL
#define ID_TRACKNUMBER 0xD7UL
#define ID_CODECID     0x86UL
#define ID_CODECPRIV   0x63A2UL
#define ID_CUES        0x1C53BB6BUL
#define ID_CUEPOINT    0xBBUL
#define ID_CUETIME     0xB3UL
#define ID_CUETRACKPOS 0xB7UL
#define ID_CUETRACK    0xF7UL
#define ID_CUECLUSTER  0xF1UL
#define ID_CUERELPOS   0xF0UL
#define ID_CLUSTER     0x1F43B675UL
#define ID_TIMESTAMP   0xE7UL
#define ID_BLOCKGROUP  0xA0UL
#define ID_BLOCK       0xA1UL
#define ID_BLOCKDUR    0x9BUL
#define ID_SIMPLEBLOCK 0xA3UL

// --- estado ------------------------------------------------------------------

typedef struct {
  long          cluster;   // posicao ABSOLUTA do elemento Cluster no arquivo
  long          rel;       // CueRelativePosition: a partir dos DADOS do Cluster
  unsigned long tempo;     // CueTime, em unidades de TimestampScale
  unsigned char colhido;   // 0 nao; 1 sim; 2 desistiu (bloco nao bate)
} Ponto;

// Cache do cabecalho dos ultimos Clusters visitados: largura do cabecalho
// (id + tamanho) e Timestamp. Os blocos vem em ordem de tempo, entao os
// Clusters repetem-se em sequencia — um anel pequeno resolve.
#define CL_CACHE 16
typedef struct { long pos; int hdr; unsigned long ts; int temTs; } ClCache;

static struct {
  pthread_mutex_t trava;
  pthread_cond_t  sinal;
  char     url[1400];
  int      faixa;
  unsigned geracao;
  int      estado;
  double   pos;
  int      vivos;          // fios de colheita vivos (o novo espera o velho)
  int      parar;
  long     pedidos, bytes;
  int      nPontos, nColhidos;
} S = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, "", 0, 0,
        MKVASS_OCIOSO, 0.0, 0, 0, 0, 0, 0, 0 };

// Tudo abaixo e DO FIO: so o fio de colheita toca, sem trava.
typedef struct {
  unsigned g;
  char     url[1400];
  int      faixa;
  long     segIni;         // onde comecam os dados do Segment
  unsigned long escala;    // TimestampScale (ns por unidade)
  long     posTracks, posCues, posInfo;   // absolutas; -1 = SeekHead nao disse
  Ponto   *pontos;
  int      nPontos, nColhidos;
  char    *corpo;          // cabecalho ASS + linhas Dialogue: colhidas
  size_t   corpoTam, corpoCap;
  ClCache  cl[CL_CACHE];
  int      clProx;
  char     sidecar[64];
  int      falhas;         // Ranges falhados seguidos
  int      sujo;           // corpo mudou desde a ultima entrega ao overlay
} Fio;

static int minhaVez(const Fio *f) {
  int ok;
  pthread_mutex_lock(&S.trava);
  ok = (f->g == S.geracao) && !S.parar;
  pthread_mutex_unlock(&S.trava);
  return ok;
}

// Publicacoes do fio precisam conferir a geracao DENTRO do mesmo lock que a
// troca de faixa. Conferir com minhaVez() e gravar depois deixa uma janela em
// que o pedido novo pode nascer e o fio velho ainda sobrescrever seu estado.
static int definirEstadoSeAtual(Fio *f, int e) {
  int ok = 0;
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && !S.parar) { S.estado = e; ok = 1; }
  pthread_mutex_unlock(&S.trava);
  return ok;
}

// --- rede, com contagem e teto -----------------------------------------------

static long agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
}

// Um Range. Conta pedidos e bytes, e SEGURA o fio quando o teto do segundo
// corrente estourou — e o unico lugar por onde a rede passa, entao o teto
// vale para tudo (cabecalho, Cues e blocos).
static unsigned char *range(Fio *f, long ini, long n, long *tam) {
  static long segundo = -1; static int noSegundo;
  char *r; int atual;
  long s = agoraMs() / 1000L;
  if (s != segundo) { segundo = s; noSegundo = 0; }
  if (noSegundo >= MKVASS_RANGES_POR_SEG) {
    long espera = (segundo + 1) * 1000L - agoraMs();
    if (espera > 0) usleep((useconds_t)(espera * 1000L));
    segundo = agoraMs() / 1000L; noSegundo = 0;
  }
  noSegundo++;
  *tam = 0;
  r = rede_baixar_trecho(f->url, 15, ini, ini + n - 1, tam);
  pthread_mutex_lock(&S.trava);
  atual = f->g == S.geracao && !S.parar;
  if (atual) {
    S.pedidos++;
    if (r) S.bytes += *tam;
  }
  pthread_mutex_unlock(&S.trava);
  if (!atual) { free(r); *tam = 0; return NULL; }
  if (!r) { f->falhas++; return NULL; }
  f->falhas = 0;
  return (unsigned char *)r;
}

// --- corpo ASS ---------------------------------------------------------------

static int corpoAnexar(Fio *f, const char *s, size_t n) {
  if (f->corpoTam + n + 1 > f->corpoCap) {
    size_t nc = f->corpoCap ? f->corpoCap : 8192;
    char *nv;
    while (nc < f->corpoTam + n + 1) nc *= 2;
    if (nc > MKVASS_CORPO_MAX) return 0;
    nv = realloc(f->corpo, nc);
    if (!nv) return 0;
    f->corpo = nv; f->corpoCap = nc;
  }
  memcpy(f->corpo + f->corpoTam, s, n);
  f->corpoTam += n;
  f->corpo[f->corpoTam] = 0;
  return 1;
}

// O CodecPrivate da faixa e o cabecalho ASS inteiro ([Script Info], estilos e
// a linha Format: dos eventos). Fica tudo ate [Events]; dali em diante o
// modulo escreve a PROPRIA linha Format:, porque as linhas Dialogue: que ele
// monta seguem a ordem classica (Layer, Start, End, Style, Name, margens,
// Effect, Text) e nao a ordem que o arquivo original declarou — o bloco do
// Matroska ja vem SEM Start/End e com ReadOrder na frente, entao a ordem do
// arquivo nao serve para nada aqui.
static int montarCabecalho(Fio *f, const unsigned char *priv, long n) {
  const char *ev;
  size_t ate = (size_t)n;
  char *tmp = malloc((size_t)n + 1);
  if (!tmp) return 0;
  memcpy(tmp, priv, (size_t)n); tmp[n] = 0;
  ev = strstr(tmp, "[Events]");
  if (ev) ate = (size_t)(ev - tmp);
  f->corpoTam = 0;
  if (!corpoAnexar(f, tmp, ate)) { free(tmp); return 0; }
  free(tmp);
  if (f->corpoTam && f->corpo[f->corpoTam - 1] != '\n') corpoAnexar(f, "\n", 1);
  { static const char EV[] = "\n[Events]\nFormat: Layer, Start, End, Style, Name, "
                             "MarginL, MarginR, MarginV, Effect, Text\n";
    return corpoAnexar(f, EV, sizeof EV - 1); }
}

static void tempoAss(double s, char *dst, size_t tam) {
  int h, m; double r;
  if (s < 0) s = 0;
  h = (int)(s / 3600.0); s -= h * 3600.0;
  m = (int)(s / 60.0);   r = s - m * 60.0;
  // Milissegundos e nao centesimos: o parser de legenda.c le "%lf" e o bloco
  // do Matroska tem precisao de ms — arredondar a cs jogaria fora 5 ms a toa.
  snprintf(dst, tam, "%d:%02d:%06.3f", h, m, r);
}

// Bloco "ReadOrder,Layer,Style,Name,MarginL,MarginR,MarginV,Effect,Text" ->
// "Dialogue: Layer,Start,End,Style,Name,MarginL,MarginR,MarginV,Effect,Text".
static int anexarEvento(Fio *f, const unsigned char *dados, long n,
                        double ini, double fim) {
  char linha[1024], a[24], b[24];
  const char *p = (const char *)dados;
  long i = 0, layerIni, layerFim, k;
  size_t w;
  // Pula ReadOrder; guarda Layer.
  while (i < n && p[i] != ',') i++;
  if (i >= n) return 0;
  layerIni = ++i;
  while (i < n && p[i] != ',') i++;
  if (i >= n) return 0;
  layerFim = i++;
  tempoAss(ini, a, sizeof a); tempoAss(fim, b, sizeof b);
  w = (size_t)snprintf(linha, sizeof linha, "Dialogue: %.*s,%s,%s,",
                       (int)(layerFim - layerIni), p + layerIni, a, b);
  // O resto (Style ate Text) vai como esta. Quebra de linha crua dentro do
  // bloco viraria fim de linha do corpo e partiria o evento em dois; a quebra
  // do ASS e \N e essa passa intacta.
  for (k = i; k < n && w < sizeof linha - 2; k++) {
    char c = p[k];
    if (c == '\r' || c == '\n') c = ' ';
    linha[w++] = c;
  }
  linha[w++] = '\n'; linha[w] = 0;
  return corpoAnexar(f, linha, w);
}

// --- sidecar -----------------------------------------------------------------

// FNV-1a de 64 bits da URL: o nome do sidecar. Nao e criptografico e nao
// precisa ser — colisao aqui custa uma legenda errada num arquivo que a pessoa
// ainda pode trocar na folha.
static void nomeSidecar(const char *url, int faixa, char *dst, size_t tam) {
  unsigned long long h = 1469598103934665603ULL;
  const unsigned char *p = (const unsigned char *)url;
  while (*p) { h ^= *p++; h *= 1099511628211ULL; }
  // A faixa entra no nome: um MKV com duas legendas ASS (normal e "signs")
  // tem dois sidecars, e o da faixa errada nao pode responder pela outra.
  snprintf(dst, tam, "mkvass-%016llx-%d.ass", h, faixa);
}

#define MARCA_COMPLETO "; mkvass-estado: completo\n"
#define MARCA_PARCIAL  "; mkvass-estado: parcial "

// Grava o corpo com a marca de estado na PRIMEIRA linha (';' e comentario em
// ASS, e antes de qualquer secao o parser de legenda.c ignora a linha). No
// parcial a marca leva um bit por CuePoint, na ordem do indice: e o que deixa
// a proxima abertura continuar de onde parou em vez de recomecar.
static void gravarSidecar(Fio *f, int completo) {
  char *tudo; size_t n, i; int gravou = 0;
  if (!f->corpo || !f->corpoTam || !f->sidecar[0]) return;
  n = f->corpoTam + 64 + (completo ? 0 : (size_t)f->nPontos);
  tudo = malloc(n + 1);
  if (!tudo) return;
  if (completo) strcpy(tudo, MARCA_COMPLETO);
  else {
    strcpy(tudo, MARCA_PARCIAL);
    for (i = 0; i < (size_t)f->nPontos; i++)
      strcat(tudo, f->pontos[i].colhido == 1 ? "1" : "0");
    strcat(tudo, "\n");
  }
  strcat(tudo, f->corpo);
  // Segura a troca de geracao durante a escrita. O parcial do pedido que esta
  // sendo parado e valido; um fio velho depois de uma troca nao pode tocar o
  // mesmo sidecar e contaminar a nova faixa.
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao) {
    dados_gravar_leve(f->sidecar, tudo);
    gravou = 1;
  }
  pthread_mutex_unlock(&S.trava);
  free(tudo);
  if (!gravou) return;
  printf("[mkvass] sidecar %s gravado (%s, %d/%d blocos, %zu bytes)\n",
         f->sidecar, completo ? "completo" : "parcial", f->nColhidos, f->nPontos,
         f->corpoTam);
  fflush(stdout);
}

// --- cabecalho: SeekHead, Info, Tracks ---------------------------------------

static void lerSeekHead(Fio *f, const unsigned char *p, long n) {
  Iter it = { p, n, 0 }; unsigned long id; const unsigned char *d; long t;
  while (proximo(&it, &id, &d, &t)) {
    if (id != ID_SEEK) continue;
    { Iter j = { d, t, 0 }; unsigned long sid; const unsigned char *sd; long st;
      unsigned long alvo = 0; long pos = -1;
      while (proximo(&j, &sid, &sd, &st)) {
        if (sid == ID_SEEKID)  alvo = lerUint(sd, st);
        if (sid == ID_SEEKPOS) pos  = (long)lerUint(sd, st);
      }
      if (pos < 0) continue;
      if (alvo == ID_TRACKS) f->posTracks = f->segIni + pos;
      if (alvo == ID_CUES)   f->posCues   = f->segIni + pos;
      if (alvo == ID_INFO)   f->posInfo   = f->segIni + pos; }
  }
}

static void lerInfo(Fio *f, const unsigned char *p, long n) {
  Iter it = { p, n, 0 }; unsigned long id; const unsigned char *d; long t;
  while (proximo(&it, &id, &d, &t))
    if (id == ID_TSSCALE) { unsigned long v = lerUint(d, t); if (v) f->escala = v; }
}

// Devolve: 1 achou a faixa e e ASS; 0 nao achou; -1 achou e NAO e ASS.
static int lerTracks(Fio *f, const unsigned char *p, long n) {
  Iter it = { p, n, 0 }; unsigned long id; const unsigned char *d; long t;
  while (proximo(&it, &id, &d, &t)) {
    Iter j; unsigned long fid; const unsigned char *fd; long ft;
    int numero = 0, ehAss = 0; const unsigned char *priv = NULL; long privN = 0;
    if (id != ID_TRACKENTRY) continue;
    j.p = d; j.n = t; j.o = 0;
    while (proximo(&j, &fid, &fd, &ft)) {
      if (fid == ID_TRACKNUMBER) numero = (int)lerUint(fd, ft);
      else if (fid == ID_CODECID)
        ehAss = ft >= 10 && (!strncmp((const char *)fd, "S_TEXT/ASS", 10) ||
                             !strncmp((const char *)fd, "S_TEXT/SSA", 10));
      else if (fid == ID_CODECPRIV) { priv = fd; privN = ft; }
    }
    if (numero != f->faixa) continue;
    if (!ehAss || !priv) return -1;
    return montarCabecalho(f, priv, privN) ? 1 : -1;
  }
  return 0;
}

// Le o cabecalho: EBML, Segment, e os elementos de nivel 1 que cabem na
// primeira janela. O que o SeekHead apontar para fora e buscado depois.
static int lerCabecalho(Fio *f) {
  long n = 0, o = 0; int ui = 0, ut = 0; unsigned long id; long tam;
  int achouTracks = 0, achouInfo = 0, tracksVisto = 0;
  unsigned char *p = range(f, 0, MKVASS_CAB, &n);
  if (!p) return MKVASS_NOGO_REDE;
  if (n < 64 || lerId(p, n, &ui) != ID_EBML) { free(p); return MKVASS_NOGO_NAO_MKV; }
  tam = lerTam(p + ui, n - ui, &ut);
  if (tam < 0) { free(p); return MKVASS_NOGO_NAO_MKV; }
  o = ui + ut + tam;
  if (lerId(p + o, n - o, &ui) != ID_SEGMENT) { free(p); return MKVASS_NOGO_NAO_MKV; }
  tam = lerTam(p + o + ui, n - o - ui, &ut);
  if (tam == -1) { free(p); return MKVASS_NOGO_NAO_MKV; }
  o += ui + ut;
  f->segIni = o;
  f->escala = 1000000UL;
  f->posTracks = f->posCues = f->posInfo = -1;
  while (o < n) {
    id = lerId(p + o, n - o, &ui);
    if (!id) break;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) break;
    o += ui + ut;
    if (o + tam > n) break;        // elemento passa da janela: o SeekHead resolve
    if (id == ID_SEEKHEAD) lerSeekHead(f, p + o, tam);
    else if (id == ID_INFO) { lerInfo(f, p + o, tam); achouInfo = 1; }
    else if (id == ID_TRACKS) {
      tracksVisto = 1;
      achouTracks = lerTracks(f, p + o, tam);
      if (achouTracks < 0) { free(p); return MKVASS_NOGO_FAIXA; }
    }
    else if (id == ID_CLUSTER) break;
    o += tam;
  }
  free(p);
  // Fora da janela: uma viagem a mais, so quando o SeekHead sabe onde.
  if (!achouInfo && f->posInfo >= 0) {
    p = range(f, f->posInfo, 4096, &n);
    if (p) { ui = 0; if (lerId(p, n, &ui) == ID_INFO) { tam = lerTam(p + ui, n - ui, &ut);
               if (tam > 0 && ui + ut + tam <= n) lerInfo(f, p + ui + ut, tam); }
             free(p); }
  }
  if (!achouTracks) {
    // Tracks inteiro na janela e a faixa nao esta la: nao ha o que buscar.
    if (tracksVisto || f->posTracks < 0) return MKVASS_NOGO_FAIXA;
    p = range(f, f->posTracks, 64L * 1024, &n);
    if (!p) return MKVASS_NOGO_REDE;
    ui = 0;
    if (lerId(p, n, &ui) != ID_TRACKS) { free(p); return MKVASS_NOGO_SEM_RANGE; }
    tam = lerTam(p + ui, n - ui, &ut);
    if (tam <= 0 || ui + ut + tam > n) { free(p); return MKVASS_NOGO_FAIXA; }
    achouTracks = lerTracks(f, p + ui + ut, tam);
    free(p);
    if (achouTracks <= 0) return MKVASS_NOGO_FAIXA;
  }
  return 0;
}

// --- Cues ----------------------------------------------------------------------

static int cmpPonto(const void *a, const void *b) {
  const Ponto *x = a, *y = b;
  if (x->tempo < y->tempo) return -1;
  if (x->tempo > y->tempo) return 1;
  if (x->cluster != y->cluster) return x->cluster < y->cluster ? -1 : 1;
  return x->rel < y->rel ? -1 : x->rel > y->rel;
}

static int lerCues(Fio *f) {
  long n = 0, tam; int ui = 0, ut = 0; unsigned char *p;
  Iter it; unsigned long id; const unsigned char *d; long t;
  int semRel = 0, cap = 256;
  if (f->posCues < 0) return MKVASS_NOGO_SEM_INDICE;
  p = range(f, f->posCues, 16, &n);
  if (!p) return MKVASS_NOGO_REDE;
  id = lerId(p, n, &ui);
  if (id != ID_CUES) {
    // O inicio do arquivo onde pedimos o fim: o servidor ignorou o Range.
    int r = (n >= 4 && lerId(p, n, &ui) == ID_EBML) ? MKVASS_NOGO_SEM_RANGE
                                                    : MKVASS_NOGO_SEM_INDICE;
    free(p); return r;
  }
  tam = lerTam(p + ui, n - ui, &ut);
  free(p);
  if (tam <= 0 || tam > MKVASS_CUES_MAX) return MKVASS_NOGO_SEM_INDICE;
  p = range(f, f->posCues + ui + ut, tam, &n);
  if (!p) return MKVASS_NOGO_REDE;
  if (n < tam) { free(p); return MKVASS_NOGO_SEM_INDICE; }
  f->pontos = calloc((size_t)cap, sizeof *f->pontos);
  if (!f->pontos) { free(p); return MKVASS_NOGO_REDE; }
  it.p = p; it.n = tam; it.o = 0;
  while (proximo(&it, &id, &d, &t)) {
    Iter j; unsigned long cid; const unsigned char *cd; long ct;
    unsigned long tempo = 0;
    if (id != ID_CUEPOINT) continue;
    j.p = d; j.n = t; j.o = 0;
    while (proximo(&j, &cid, &cd, &ct)) {
      if (cid == ID_CUETIME) tempo = lerUint(cd, ct);
      else if (cid == ID_CUETRACKPOS) {
        Iter k = { cd, ct, 0 }; unsigned long kid; const unsigned char *kd; long kt;
        long trk = -1, cl = -1, rel = -1;
        while (proximo(&k, &kid, &kd, &kt)) {
          if (kid == ID_CUETRACK)   trk = (long)lerUint(kd, kt);
          if (kid == ID_CUECLUSTER) cl  = (long)lerUint(kd, kt);
          if (kid == ID_CUERELPOS)  rel = (long)lerUint(kd, kt);
        }
        if (trk != f->faixa || cl < 0) continue;
        if (rel < 0) { semRel = 1; continue; }
        if (f->nPontos >= MKVASS_MAX_PONTOS) continue;
        if (f->nPontos >= cap) {
          Ponto *nv = realloc(f->pontos, (size_t)cap * 2 * sizeof *nv);
          if (!nv) break;
          f->pontos = nv; cap *= 2;
        }
        f->pontos[f->nPontos].cluster = f->segIni + cl;
        f->pontos[f->nPontos].rel = rel;
        f->pontos[f->nPontos].tempo = tempo;
        f->pontos[f->nPontos].colhido = 0;
        f->nPontos++;
      }
    }
  }
  free(p);
  if (!f->nPontos) return semRel ? MKVASS_NOGO_SEM_REL : MKVASS_NOGO_SEM_INDICE;
  // Um CuePoint da faixa sem posicao relativa ja basta para desistir: o bloco
  // dele so se acharia varrendo o Cluster inteiro.
  if (semRel) return MKVASS_NOGO_SEM_REL;
  qsort(f->pontos, (size_t)f->nPontos, sizeof *f->pontos, cmpPonto);
  return 0;
}

// --- blocos --------------------------------------------------------------------

static double segundosDe(const Fio *f, unsigned long unidades) {
  return (double)unidades * (double)f->escala / 1e9;
}

// Cabecalho do Cluster: largura (id + tamanho) e o Timestamp, que e o primeiro
// filho em tudo que os muxers produzem. Um Range de 24 bytes por Cluster
// novo; repetidos saem do anel.
static const ClCache *cluster(Fio *f, long pos) {
  int i; long n = 0; int ui = 0, ut = 0; long tam; unsigned char *p; ClCache *c;
  for (i = 0; i < CL_CACHE; i++) if (f->cl[i].hdr && f->cl[i].pos == pos) return &f->cl[i];
  p = range(f, pos, 24, &n);
  if (!p) return NULL;
  if (lerId(p, n, &ui) != ID_CLUSTER) { free(p); return NULL; }
  tam = lerTam(p + ui, n - ui, &ut);
  if (tam == -1) { free(p); return NULL; }
  c = &f->cl[f->clProx]; f->clProx = (f->clProx + 1) % CL_CACHE;
  c->pos = pos; c->hdr = ui + ut; c->temTs = 0; c->ts = 0;
  { long o = ui + ut; int vi = 0, vt = 0;
    if (o < n && lerId(p + o, n - o, &vi) == ID_TIMESTAMP) {
      long vt2 = lerTam(p + o + vi, n - o - vi, &vt);
      if (vt2 > 0 && o + vi + vt + vt2 <= n) { c->ts = lerUint(p + o + vi + vt, vt2); c->temTs = 1; }
    } }
  free(p);
  return c;
}

// Interpreta UM bloco (BlockGroup ou SimpleBlock) em p[0..n). Devolve os bytes
// que ele ocupa (para andar ate o proximo), 0 quando nao e da faixa ou nao
// fecha, e -k quando faltam k bytes para o bloco caber na janela.
static long lerBloco(Fio *f, const unsigned char *p, long n, const ClCache *cl,
                     unsigned long cueTempo) {
  int ui = 0, ut = 0; unsigned long id; long tam, total;
  const unsigned char *bl = NULL; long blN = 0; unsigned long dur = 0; int temDur = 0;
  id = lerId(p, n, &ui);
  if (id != ID_BLOCKGROUP && id != ID_SIMPLEBLOCK) return 0;
  tam = lerTam(p + ui, n - ui, &ut);
  if (tam <= 0) return 0;
  total = ui + ut + tam;
  if (total > n) return -(total - n);
  if (id == ID_BLOCKGROUP) {
    Iter it = { p + ui + ut, tam, 0 }; unsigned long gid; const unsigned char *gd; long gt;
    while (proximo(&it, &gid, &gd, &gt)) {
      if (gid == ID_BLOCK) { bl = gd; blN = gt; }
      else if (gid == ID_BLOCKDUR) { dur = lerUint(gd, gt); temDur = 1; }
    }
  } else { bl = p + ui + ut; blN = tam; }
  if (!bl || blN < 4) return total;
  { int vt = 0; long trk = lerVint(bl, blN, &vt); int rel; unsigned flags;
    double ini, fim;
    if (trk != f->faixa || vt + 3 > blN) return total;
    rel = (int)(short)((bl[vt] << 8) | bl[vt + 1]);
    flags = bl[vt + 2];
    // LACING (bits 0x06): varios quadros num bloco. Em legenda nao acontece
    // na pratica (ffmpeg e mkvmerge nunca lacam texto) e desfazer o lacing
    // Xiph/EBML e codigo que nunca rodaria; o bloco e pulado, e o log diz.
    if (flags & 0x06) {
      printf("[mkvass] bloco com lacing (flags 0x%02x) pulado\n", flags);
      return total;
    }
    // Start = Timestamp do Cluster + relativo do bloco. Sem Timestamp no
    // Cluster (nao deveria acontecer), o CueTime e a mesma coisa vista do
    // indice.
    ini = cl->temTs ? segundosDe(f, cl->ts) + (double)rel * (double)f->escala / 1e9
                    : segundosDe(f, cueTempo);
    fim = ini + (temDur ? segundosDe(f, dur) : MKVASS_DUR_PADRAO);
    if (fim <= ini) fim = ini + 0.5;
    anexarEvento(f, bl + vt + 3, blN - vt - 3, ini, fim);
    f->sujo = 1;
    return total;
  }
}

// Colhe o grupo de pontos [i..j] (mesmo Cluster, proximos) num Range so.
// Marca cada um como colhido (1) ou desistido (2). Devolve 1 se a rede
// respondeu.
static int colherGrupo(Fio *f, int i, int j) {
  const ClCache *cl = cluster(f, f->pontos[i].cluster);
  long ini, n = 0, fim; unsigned char *p; int k;
  if (!cl) {
    // Range falhou (NULL) ou o cue nao aponta para um Cluster: no segundo
    // caso o indice mente e insistir nao ajuda.
    if (f->falhas) return 0;
    for (k = i; k <= j; k++) f->pontos[k].colhido = 2;
    return 1;
  }
  ini = f->pontos[i].cluster + cl->hdr + f->pontos[i].rel;
  fim = f->pontos[j].cluster + cl->hdr + f->pontos[j].rel + MKVASS_BLOCO;
  p = range(f, ini, fim - ini, &n);
  if (!p) return 0;
  // Um 206 curto nao e um bloco invalido: e uma resposta truncada. Nao
  // transforme isso em "desistido", pois os desistidos contam como completos
  // e poderiam fechar um sidecar sem o texto. O worker converte o caso em
  // NOGO_REDE apos a politica de falhas, preservando o fallback nativo.
  if (n < fim - ini) { free(p); return -2; }
  for (k = i; k <= j; k++) {
    long o = f->pontos[k].rel - f->pontos[i].rel;
    long r = (o < n) ? lerBloco(f, p + o, n - o, cl, f->pontos[k].tempo) : 0;
    if (r < 0) {
      // Uma fala maior que a janela: completa com um Range so para ela.
      long falta = -r, m = 0;
      unsigned char *q = malloc((size_t)(n - o + falta));
      unsigned char *resto = q ? range(f, ini + n, falta, &m) : NULL;
      if (q && resto && m >= falta) {
        memcpy(q, p + o, (size_t)(n - o)); memcpy(q + (n - o), resto, (size_t)falta);
        r = lerBloco(f, q, n - o + falta, cl, f->pontos[k].tempo);
      }
      if (q && (!resto || m < falta)) { free(q); free(resto); free(p); return -2; }
      free(q); free(resto);
    }
    f->pontos[k].colhido = r > 0 ? 1 : 2;
    if (r > 0) f->nColhidos++;
  }
  free(p);
  return 1;
}

// Entrega o corpo ao overlay. legenda_definir_corpo REFAZ o vetor de cues do
// zero a partir do corpo inteiro, em vez de um "anexar" incremental — e por
// escolha: o vetor precisa continuar ordenado e com a maior duracao
// recalculada, e reparsear algumas centenas de linhas custa menos de um
// quadro; um anexar teria de reimplementar isso dentro de legenda.c so para
// este chamador. A entrega e por LOTE (uma vez por passada do laco), nao por
// bloco.
//
// O mutex de S fica preso DURANTE a publicacao. Conferir a geracao, soltar o
// mutex e so entao chamar legenda_definir_corpo deixava a troca de faixa entrar
// no meio: o fio velho publicava cues antigos depois que o novo ja era atual.
// legenda_definir_corpo so toma a trava interna de legenda e nao chama mkvass,
// portanto esta ordem nao forma ciclo com os chamadores.
static int entregarCorpoSeAtual(Fio *f, const char *corpo) {
  int ok = 0;
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && !S.parar) {
    legenda_definir_corpo(corpo);
    ok = 1;
  }
  pthread_mutex_unlock(&S.trava);
  return ok;
}

static void entregar(Fio *f) {
  if (!f->sujo || !f->corpo) return;
  if (entregarCorpoSeAtual(f, f->corpo)) f->sujo = 0;
}

// Restaura um sidecar parcial: corpo e bits. Devolve 1 se serviu.
static int restaurarParcial(Fio *f, const char *sc) {
  const char *bits = sc + strlen(MARCA_PARCIAL), *nl = strchr(bits, '\n');
  int i;
  if (!nl || (int)(nl - bits) != f->nPontos) return 0;   // indice mudou: ignora
  free(f->corpo); f->corpo = NULL; f->corpoTam = f->corpoCap = 0;
  if (!corpoAnexar(f, nl + 1, strlen(nl + 1))) return 0;
  f->nColhidos = 0;
  for (i = 0; i < f->nPontos; i++) {
    f->pontos[i].colhido = bits[i] == '1' ? 1 : 0;
    if (bits[i] == '1') f->nColhidos++;
  }
  f->sujo = 1;
  return 1;
}

// Pontos em que o modulo desistiu (o indice apontava para algo que nao era um
// bloco desta faixa). Contam como feitos para o "completo".
static int contarDesistidos(const Fio *f) {
  int i, n = 0;
  for (i = 0; i < f->nPontos; i++) if (f->pontos[i].colhido == 2) n++;
  return n;
}

// --- o fio -----------------------------------------------------------------------

static void *trabalhar(void *arg) {
  Fio *f = arg;
  char *sc;
  int r;

  // Espera o fio anterior morrer: os dois escreveriam o mesmo sidecar e
  // entregariam corpos ao overlay fora de ordem.
  pthread_mutex_lock(&S.trava);
  while (S.vivos > 1 && f->g == S.geracao) pthread_cond_wait(&S.sinal, &S.trava);
  pthread_mutex_unlock(&S.trava);
  if (!minhaVez(f)) goto fim;

  nomeSidecar(f->url, f->faixa, f->sidecar, sizeof f->sidecar);
  sc = dados_ler(f->sidecar);
  if (sc && !strncmp(sc, MARCA_COMPLETO, strlen(MARCA_COMPLETO))) {
    // De graca: nenhum Range.
    if (!entregarCorpoSeAtual(f, sc + strlen(MARCA_COMPLETO))) { free(sc); goto fim; }
    if (!definirEstadoSeAtual(f, MKVASS_COMPLETO)) { free(sc); goto fim; }
    printf("[mkvass] sidecar completo %s: sem rede\n", f->sidecar);
    fflush(stdout);
    free(sc);
    goto fim;
  }

  r = lerCabecalho(f);
  if (!r) r = lerCues(f);
  if (r) {
    if (!definirEstadoSeAtual(f, r)) { free(sc); goto fim; }
    printf("[mkvass] no-go %d (faixa %d): %s\n", r, f->faixa,
           r == MKVASS_NOGO_NAO_MKV ? "nao e MKV" :
           r == MKVASS_NOGO_SEM_RANGE ? "servidor sem Range" :
           r == MKVASS_NOGO_FAIXA ? "faixa nao e ASS" :
           r == MKVASS_NOGO_SEM_INDICE ? "sem CuePoint da faixa" :
           r == MKVASS_NOGO_SEM_REL ? "sem CueRelativePosition" : "rede");
    fflush(stdout);
    free(sc);
    goto fim;
  }
  pthread_mutex_lock(&S.trava);
  if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); free(sc); goto fim; }
  S.nPontos = f->nPontos;
  pthread_mutex_unlock(&S.trava);

  if (sc && !strncmp(sc, MARCA_PARCIAL, strlen(MARCA_PARCIAL)) && restaurarParcial(f, sc))
    printf("[mkvass] sidecar parcial: %d/%d blocos ja colhidos\n", f->nColhidos, f->nPontos);
  free(sc);
  printf("[mkvass] faixa %d: %d blocos indexados, escala %lu ns, cabecalho %zu bytes\n",
         f->faixa, f->nPontos, f->escala, f->corpoTam);
  fflush(stdout);
  entregar(f);
  if (!definirEstadoSeAtual(f, MKVASS_COLHENDO)) goto fim;

  // O laco: a cada passada colhe o que esta na janela [pos - atras, pos +
  // janela] em ordem de tempo, ate MKVASS_RANGES_POR_SEG grupos, entrega ao
  // overlay, e dorme ate a posicao andar (ou 250 ms).
  while (minhaVez(f)) {
    double pos, ini, fim; int i, feitos = 0, pendentes = 0;
    pthread_mutex_lock(&S.trava);
    if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); goto sair; }
    pos = S.pos; S.nColhidos = f->nColhidos;
    pthread_mutex_unlock(&S.trava);
    ini = pos - MKVASS_ATRAS_SEG; fim = pos + MKVASS_JANELA_SEG;
    for (i = 0; i < f->nPontos && feitos < MKVASS_RANGES_POR_SEG && minhaVez(f); i++) {
      double t = segundosDe(f, f->pontos[i].tempo); int j;
      if (f->pontos[i].colhido) continue;
      if (t > fim) break;
      if (t < ini) continue;
      pendentes++;
      // Junta os vizinhos do mesmo Cluster que cabem em MKVASS_JUNTAR.
      j = i;
      while (j + 1 < f->nPontos && !f->pontos[j + 1].colhido &&
             f->pontos[j + 1].cluster == f->pontos[i].cluster &&
             f->pontos[j + 1].rel >= f->pontos[i].rel &&
             f->pontos[j + 1].rel - f->pontos[i].rel < MKVASS_JUNTAR) j++;
      { int colheu = colherGrupo(f, i, j);
      if (colheu == -2) {
        if (definirEstadoSeAtual(f, MKVASS_NOGO_REDE)) {
          printf("[mkvass] Range curto: desistindo sem completar a faixa\n");
          fflush(stdout);
        }
        goto sair;
      }
      if (!colheu) {
        if (f->falhas >= MKVASS_FALHAS_MAX) {
          if (definirEstadoSeAtual(f, MKVASS_NOGO_REDE)) {
            printf("[mkvass] %d Ranges falhados seguidos: desistindo\n", f->falhas);
            fflush(stdout);
          }
          goto sair;
        }
        usleep(500 * 1000);
      }
      }
      feitos++;
      i = j;
    }
    entregar(f);
    pthread_mutex_lock(&S.trava);
    if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); goto sair; }
    S.nColhidos = f->nColhidos;
    pthread_mutex_unlock(&S.trava);
    if (f->nColhidos + contarDesistidos(f) >= f->nPontos) {
      // Tudo o que o indice tinha. Os desistidos (2) contam como feitos: o
      // indice apontava para algo que nao era um bloco desta faixa.
      gravarSidecar(f, 1);
      if (!definirEstadoSeAtual(f, MKVASS_COMPLETO)) goto fim;
      printf("[mkvass] completo: %d/%d blocos, %ld Ranges\n", f->nColhidos, f->nPontos, S.pedidos);
      fflush(stdout);
      goto fim;
    }
    if (!pendentes || feitos < MKVASS_RANGES_POR_SEG) {
      // Nada na janela (ou a janela esvaziou): espera a posicao andar.
      pthread_mutex_lock(&S.trava);
      if (f->g == S.geracao && !S.parar) {
        // CLOCK_MONOTONIC no cond exigiria pthread_condattr_setclock, que o
        // sysroot do webOS 4 nao garante; um timeout de 250 ms por REALTIME
        // erra pouco e o laco tolera.
        struct timespec rt; clock_gettime(CLOCK_REALTIME, &rt);
        rt.tv_nsec += 250L * 1000000L;
        if (rt.tv_nsec >= 1000000000L) { rt.tv_sec++; rt.tv_nsec -= 1000000000L; }
        pthread_cond_timedwait(&S.sinal, &S.trava, &rt);
      }
      pthread_mutex_unlock(&S.trava);
    }
  }
sair:
  // Saiu antes do fim (parar, troca de faixa, rede): guarda o que ha para a
  // proxima abertura nao repetir os Ranges ja pagos.
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao) S.nColhidos = f->nColhidos;
  pthread_mutex_unlock(&S.trava);
  if (f->nColhidos > 0 && mkvass_estado() != MKVASS_COMPLETO) gravarSidecar(f, 0);

fim:
  pthread_mutex_lock(&S.trava);
  S.vivos--;
  pthread_cond_broadcast(&S.sinal);
  pthread_mutex_unlock(&S.trava);
  free(f->pontos); free(f->corpo); free(f);
  return NULL;
}

// --- API ---------------------------------------------------------------------------

void mkvass_iniciar(const char *url, int numeroFaixa) {
  Fio *f; pthread_t t;
  if (!url || !*url || numeroFaixa <= 0) return;
  f = calloc(1, sizeof *f);
  if (!f) return;
  snprintf(f->url, sizeof f->url, "%s", url);
  f->faixa = numeroFaixa;
  pthread_mutex_lock(&S.trava);
  S.geracao++;
  f->g = S.geracao;
  snprintf(S.url, sizeof S.url, "%s", url);
  S.faixa = numeroFaixa;
  S.estado = MKVASS_PREPARANDO;
  S.parar = 0;
  S.pedidos = S.bytes = 0;
  S.nPontos = S.nColhidos = 0;
  S.vivos++;
  pthread_cond_broadcast(&S.sinal);   // acorda o fio antigo para ele ver a geracao nova
  pthread_mutex_unlock(&S.trava);
  if (pthread_create(&t, NULL, trabalhar, f) == 0) pthread_detach(t);
  else {
    pthread_mutex_lock(&S.trava);
    S.vivos--; S.estado = MKVASS_NOGO_REDE;
    pthread_mutex_unlock(&S.trava);
    free(f);
  }
}

void mkvass_passo(double posSeg) {
  pthread_mutex_lock(&S.trava);
  if (S.estado == MKVASS_COLHENDO || S.estado == MKVASS_PREPARANDO) {
    // Meio segundo de histerese: acordar o fio a cada quadro por 16 ms de
    // avanco nao muda a janela e so custa trocas de contexto.
    if (fabs(posSeg - S.pos) >= 0.5) { S.pos = posSeg; pthread_cond_broadcast(&S.sinal); }
  }
  pthread_mutex_unlock(&S.trava);
}

void mkvass_parar(void) {
  pthread_mutex_lock(&S.trava);
  if (S.vivos > 0) S.parar = 1;
  if (S.estado != MKVASS_COMPLETO && S.estado < MKVASS_NOGO) S.estado = MKVASS_OCIOSO;
  S.pos = 0.0;
  pthread_cond_broadcast(&S.sinal);
  pthread_mutex_unlock(&S.trava);
}

int mkvass_estado(void) {
  int e;
  pthread_mutex_lock(&S.trava); e = S.estado; pthread_mutex_unlock(&S.trava);
  return e;
}

int mkvass_nogo(void) { return mkvass_estado() >= MKVASS_NOGO; }

int mkvass_ocupado(void) {
  int v;
  pthread_mutex_lock(&S.trava); v = S.vivos; pthread_mutex_unlock(&S.trava);
  return v > 0;
}

void mkvass_estatisticas(long *pedidos, long *bytes, int *colhidos, int *total) {
  pthread_mutex_lock(&S.trava);
  if (pedidos)  *pedidos  = S.pedidos;
  if (bytes)    *bytes    = S.bytes;
  if (colhidos) *colhidos = S.nColhidos;
  if (total)    *total    = S.nPontos;
  pthread_mutex_unlock(&S.trava);
}

#endif  /* __EMSCRIPTEN__ */
