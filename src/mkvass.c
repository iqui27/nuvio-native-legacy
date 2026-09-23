#include "mkvass.h"

#include "rede.h"
#include "legenda.h"
#include "assrender.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <math.h>
#include <limits.h>

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
#define MKVASS_CORPO_MAX   (16L * 1024 * 1024)
// Falhas de Range seguidas antes de desistir (NOGO_REDE).
#define MKVASS_FALHAS_MAX  5
// Duracao quando o bloco nao traz BlockDuration (SimpleBlock). Raro em
// legenda — ffmpeg e mkvmerge escrevem BlockGroup — mas um valor e melhor que
// um evento de zero segundos que nunca aparece.
#define MKVASS_DUR_PADRAO  3.0
// Fala que comeca ate isto a frente do playhead e URGENTE: o lote vai ao
// overlay assim que chega, sem esperar a passada (que custa 1-2 s pelo teto
// de Ranges). Com a entrega sem pisca (legenda_atualizar_corpo) isto e barato.
#define MKVASS_URGENTE_SEG 20.0
// Fora do urgente, uma entrega a cada isto no maximo: cada entrega reparseia
// o corpo inteiro (legenda.c e libass).
#define MKVASS_ENTREGA_MS  2000L

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
#define ID_TRACKTYPE   0x83UL
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
#define ID_ATTACHMENTS 0x1941A469UL
#define ID_ATTACHEDFILE 0x61A7UL
#define ID_FILENAME     0x466EUL
#define ID_FILEMIMETYPE 0x4660UL
#define ID_FILEDATA     0x465CUL

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

typedef struct {
  char *nome;
  unsigned char *dados;
  long tam;
} FonteMkv;

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
  long     posAttachments;
  int      seekHeadVisto;
  Ponto   *pontos;
  int      nPontos, nColhidos;
  char    *corpo;          // cabecalho ASS + linhas Dialogue: colhidas
  size_t   corpoTam, corpoCap;
  ClCache  cl[CL_CACHE];
  int      clProx;
  char     sidecar[64];
  char     sidecarFontes[80];
  int      falhas;         // Ranges falhados seguidos
  int      sujo;           // corpo mudou desde a ultima entrega ao overlay
  int      entregas;       // 0 = a proxima e a primeira (fontes + carga cheia)
  long     t0, ultEntrega; // ms monotonicos: pedido, ultima entrega
  int      eventosEntregues, primeiraFala;
  double   posRef;         // playhead quando o grupo foi pedido (medida)
  int      atrasados, perdidos, palpites, palpitesFalhos;
  FonteMkv *fontes;
  int      nFontes;
  int      fontesCompletas;
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

static int extensaoFonte(const char *nome, const char *mime) {
  const char *p = nome ? strrchr(nome, '.') : NULL;
  if (mime && (!strncasecmp(mime, "font/", 5) || !strncasecmp(mime, "application/x-font", 18))) return 1;
  if (!p) return 0;
  return !strcasecmp(p, ".ttf") || !strcasecmp(p, ".otf") ||
         !strcasecmp(p, ".ttc") || !strcasecmp(p, ".otc");
}

static void liberarFontes(Fio *f) {
  int i;
  for (i = 0; i < f->nFontes; i++) { free(f->fontes[i].nome); free(f->fontes[i].dados); }
  free(f->fontes); f->fontes = NULL; f->nFontes = 0;
}

static int lerAttachments(Fio *f, const unsigned char *p, long n) {
  Iter it = { p, n, 0 }; unsigned long id; const unsigned char *d; long t;
  while (proximo(&it, &id, &d, &t)) {
    Iter j; unsigned long fid; const unsigned char *fd; long ft;
    const unsigned char *dados = NULL; long dadosN = 0;
    char nome[256] = {0}, mime[128] = {0};
    if (id != ID_ATTACHEDFILE) continue;
    j.p = d; j.n = t; j.o = 0;
    while (proximo(&j, &fid, &fd, &ft)) {
      if (fid == ID_FILENAME) {
        size_t z = (size_t)ft < sizeof nome - 1 ? (size_t)ft : sizeof nome - 1;
        memcpy(nome, fd, z); nome[z] = 0;
      } else if (fid == ID_FILEMIMETYPE) {
        size_t z = (size_t)ft < sizeof mime - 1 ? (size_t)ft : sizeof mime - 1;
        memcpy(mime, fd, z); mime[z] = 0;
      } else if (fid == ID_FILEDATA) { dados = fd; dadosN = ft; }
    }
    if (!dados || dadosN <= 0 || !extensaoFonte(nome, mime)) continue;
    if (f->nFontes >= 32) continue;
    {
      FonteMkv *nv = realloc(f->fontes, (size_t)(f->nFontes + 1) * sizeof *nv);
      if (!nv) return 0;
      f->fontes = nv;
      f->fontes[f->nFontes].nome = strdup(nome[0] ? nome : "attachment.ttf");
      f->fontes[f->nFontes].dados = malloc((size_t)dadosN);
      if (!f->fontes[f->nFontes].nome || !f->fontes[f->nFontes].dados) {
        free(f->fontes[f->nFontes].nome); free(f->fontes[f->nFontes].dados); return 0;
      }
      memcpy(f->fontes[f->nFontes].dados, dados, (size_t)dadosN);
      f->fontes[f->nFontes].tam = dadosN;
      f->nFontes++;
    }
  }
  return 1;
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
  char a[24], b[24];
  char *linha;
  const char *p = (const char *)dados;
  long i = 0, layerIni, layerFim, k;
  size_t w, capacidade;
  // Pula ReadOrder; guarda Layer.
  while (i < n && p[i] != ',') i++;
  if (i >= n) return 0;
  layerIni = ++i;
  while (i < n && p[i] != ',') i++;
  if (i >= n) return 0;
  layerFim = i++;
  tempoAss(ini, a, sizeof a); tempoAss(fim, b, sizeof b);
  /* O texto ASS e um campo sem limite pratico (desenhos vetoriais e algumas
   * falas de karaoke passam facilmente de 1 KiB). A versao antiga usava uma
   * linha[1024] e silenciosamente cortava o evento no meio de uma tag. */
  capacidade = (size_t)n + 64u;
  linha = malloc(capacidade);
  if (!linha) return 0;
  w = (size_t)snprintf(linha, capacidade, "Dialogue: %.*s,%s,%s,",
                       (int)(layerFim - layerIni), p + layerIni, a, b);
  // O resto (Style ate Text) vai como esta. Quebra de linha crua dentro do
  // bloco viraria fim de linha do corpo e partiria o evento em dois; a quebra
  // do ASS e \N e essa passa intacta.
  for (k = i; k < n && w + 2 < capacidade; k++) {
    char c = p[k];
    if (c == '\r' || c == '\n') c = ' ';
    linha[w++] = c;
  }
  linha[w++] = '\n'; linha[w] = 0;
  k = corpoAnexar(f, linha, w);
  free(linha);
  return (int)k;
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

#define MARCA_COMPLETO "; mkvass-estado: completo-v2\n"
#define MARCA_PARCIAL  "; mkvass-estado: parcial "
#define MARCA_FONTES   "NVASS-FONTES-1\n"

static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static size_t b64_tamanho(size_t n) { return ((n + 2u) / 3u) * 4u; }

static void b64_codificar(char *dst, const unsigned char *src, size_t n) {
  size_t i = 0, o = 0;
  while (i < n) {
    size_t resto = n - i;
    unsigned a = src[i++], b = resto > 1u ? src[i++] : 0, c = resto > 2u ? src[i++] : 0;
    dst[o++] = B64[a >> 2]; dst[o++] = B64[((a & 3u) << 4) | (b >> 4)];
    dst[o++] = resto > 1u ? B64[((b & 15u) << 2) | (c >> 6)] : '=';
    dst[o++] = resto > 2u ? B64[c & 63u] : '=';
  }
  dst[o] = 0;
}

static int b64_valor(unsigned char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= 'a' && c <= 'z') return c - 'a' + 26;
  if (c >= '0' && c <= '9') return c - '0' + 52;
  if (c == '+') return 62;
  if (c == '/') return 63;
  return -1;
}

static unsigned char *b64_decodificar(const char *src, size_t n, size_t *tam) {
  unsigned char *dst; size_t i, o = 0;
  if (!src || !tam || n % 4u || n > 24u * 1024u * 1024u) return NULL;
  dst = malloc(n / 4u * 3u + 1u);
  if (!dst) return NULL;
  for (i = 0; i < n; i += 4u) {
    int a = b64_valor((unsigned char)src[i]), b = b64_valor((unsigned char)src[i + 1]);
    int c = src[i + 2] == '=' ? 0 : b64_valor((unsigned char)src[i + 2]);
    int d = src[i + 3] == '=' ? 0 : b64_valor((unsigned char)src[i + 3]);
    if (a < 0 || b < 0 || c < 0 || d < 0 ||
        (src[i + 2] == '=' && src[i + 3] != '=') ||
        ((src[i + 2] == '=' || src[i + 3] == '=') && i + 4u != n)) {
      free(dst); return NULL;
    }
    dst[o++] = (unsigned char)((a << 2) | (b >> 4));
    if (src[i + 2] != '=') dst[o++] = (unsigned char)((b << 4) | (c >> 2));
    if (src[i + 3] != '=') dst[o++] = (unsigned char)((c << 6) | d);
  }
  *tam = o;
  return dst;
}

static const char *linhaCache(const char **p, size_t *n) {
  const char *ini = *p, *fim = strchr(ini, '\n');
  if (!fim) return NULL;
  *n = (size_t)(fim - ini); *p = fim + 1;
  return ini;
}

static int lerFontesSidecar(Fio *f) {
  char *cache = dados_ler(f->sidecarFontes);
  const char *p; size_t linhaN, cacheN; long count, i, total = 0;
  if (!cache) return 0;
  cacheN = strlen(cache);
  if (cacheN > 24u * 1024u * 1024u || strncmp(cache, MARCA_FONTES, strlen(MARCA_FONTES))) {
    free(cache); return 0;
  }
  p = cache + strlen(MARCA_FONTES);
  { const char *l = linhaCache(&p, &linhaN); char tmp[16];
    if (!l || !linhaN || linhaN >= sizeof tmp) { free(cache); return 0; }
    memcpy(tmp, l, linhaN); tmp[linhaN] = 0; count = strtol(tmp, NULL, 10);
    if (count < 0 || count > 32) { free(cache); return 0; } }
  liberarFontes(f);
  for (i = 0; i < count; i++) {
    size_t nome64N = 0, nomeN = 0;
    const char *nome64 = linhaCache(&p, &nome64N);
    unsigned char *nome = nome64 ? b64_decodificar(nome64, nome64N, &nomeN) : NULL;
    const char *dados64; size_t dados64N, dadosN = 0;
    unsigned char *dados;
    FonteMkv *nv;
    if (!nome || nomeN == 0 || nomeN >= 256 || memchr(nome, 0, nomeN)) { free(nome); goto falha; }
    dados64 = linhaCache(&p, &dados64N);
    dados = dados64 ? b64_decodificar(dados64, dados64N, &dadosN) : NULL;
    if (!dados || dadosN == 0 || dadosN > MKVASS_CORPO_MAX || total > MKVASS_CORPO_MAX - (long)dadosN) {
      free(nome); free(dados); goto falha;
    }
    nv = realloc(f->fontes, (size_t)(f->nFontes + 1) * sizeof *nv);
    if (!nv) { free(nome); free(dados); goto falha; }
    f->fontes = nv;
    f->fontes[f->nFontes].nome = malloc(nomeN + 1u);
    if (!f->fontes[f->nFontes].nome) { free(nome); free(dados); goto falha; }
    memcpy(f->fontes[f->nFontes].nome, nome, nomeN); f->fontes[f->nFontes].nome[nomeN] = 0;
    f->fontes[f->nFontes].dados = dados; f->fontes[f->nFontes].tam = (long)dadosN;
    f->nFontes++; total += (long)dadosN;
    free(nome);
  }
  if (p != cache + cacheN) goto falha;
  f->fontesCompletas = 1;
  free(cache); return 1;
falha:
  liberarFontes(f); free(cache); return 0;
}

static int gravarFontesSidecar(Fio *f) {
  size_t cap = strlen(MARCA_FONTES) + 8u, usado;
  char *cache, *p;
  int i, ok;
  if (!f->fontesCompletas || f->nFontes < 0 || f->nFontes > 32) return 0;
  for (i = 0; i < f->nFontes; i++) {
    size_t nn = strlen(f->fontes[i].nome), dn = (size_t)f->fontes[i].tam;
    if (nn > 255 || dn == 0 || dn > MKVASS_CORPO_MAX) return 0;
    cap += b64_tamanho(nn) + b64_tamanho(dn) + 2u;
  }
  if (cap > 24u * 1024u * 1024u) return 0;
  cache = malloc(cap + 1u); if (!cache) return 0;
  p = cache; usado = strlen(MARCA_FONTES); memcpy(p, MARCA_FONTES, usado); p += usado;
  p += sprintf(p, "%d\n", f->nFontes);
  for (i = 0; i < f->nFontes; i++) {
    size_t nn = strlen(f->fontes[i].nome), dn = (size_t)f->fontes[i].tam;
    b64_codificar(p, (const unsigned char *)f->fontes[i].nome, nn); p += b64_tamanho(nn); *p++ = '\n';
    b64_codificar(p, f->fontes[i].dados, dn); p += b64_tamanho(dn); *p++ = '\n';
  }
  *p = 0;
  ok = dados_gravar_leve(f->sidecarFontes, cache);
  free(cache); return ok;
}

// Grava o corpo com a marca de estado na PRIMEIRA linha (';' e comentario em
// ASS, e antes de qualquer secao o parser de legenda.c ignora a linha). No
// parcial a marca leva um bit por CuePoint, na ordem do indice: e o que deixa
// a proxima abertura continuar de onde parou em vez de recomecar.
static void gravarSidecar(Fio *f, int completo, int saindo) {
  char *tudo; size_t n, i; int gravou = 0;
  if (!f->corpo || !f->corpoTam || !f->sidecar[0]) return;
  if (completo && !gravarFontesSidecar(f)) {
    fprintf(stderr, "[mkvass] cache completo adiado: fontes Matroska nao foram confirmadas\n");
    return;
  }
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
  // SAINDO (parar ou troca de faixa) pode gravar com a geracao ja nova: o fio
  // seguinte so le o sidecar depois que este morre (espera S.vivos), entao nao
  // ha contaminacao — e o parcial deixava de ser salvo justo na troca.
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao || saindo) {
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
      if (alvo == ID_TRACKS)      f->posTracks = f->segIni + pos;
      if (alvo == ID_CUES)        f->posCues = f->segIni + pos;
      if (alvo == ID_INFO)        f->posInfo = f->segIni + pos;
      if (alvo == ID_ATTACHMENTS) f->posAttachments = f->segIni + pos; }
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
  int ordinalLeg = 0;
  while (proximo(&it, &id, &d, &t)) {
    Iter j; unsigned long fid; const unsigned char *fd; long ft;
    int numero = 0, tipo = 0, ehAss = 0; const unsigned char *priv = NULL; long privN = 0;
    if (id != ID_TRACKENTRY) continue;
    j.p = d; j.n = t; j.o = 0;
    while (proximo(&j, &fid, &fd, &ft)) {
      if (fid == ID_TRACKNUMBER) numero = (int)lerUint(fd, ft);
      else if (fid == ID_TRACKTYPE) tipo = (int)lerUint(fd, ft);
      else if (fid == ID_CODECID)
        ehAss = ft >= 10 && (!strncmp((const char *)fd, "S_TEXT/ASS", 10) ||
                             !strncmp((const char *)fd, "S_TEXT/SSA", 10));
      else if (fid == ID_CODECPRIV) { priv = fd; privN = ft; }
    }
    if (f->faixa < 0) {
      if (tipo != 17 || ordinalLeg++ != -f->faixa - 1) continue;
      f->faixa = numero;
    } else if (numero != f->faixa) continue;
    if (!ehAss || !priv) return -1;
    return montarCabecalho(f, priv, privN) ? 1 : -1;
  }
  return 0;
}

// Le o cabecalho: EBML, Segment, e os elementos de nivel 1 que cabem na
// primeira janela. O que o SeekHead apontar para fora e buscado depois.
static int lerCabecalho(Fio *f) {
  long n = 0, o = 0; int ui = 0, ut = 0; unsigned long id; long tam;
  int achouTracks = 0, achouInfo = 0, achouAttachments = 0, tracksVisto = 0;
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
  f->posTracks = f->posCues = f->posInfo = f->posAttachments = -1;
  while (o < n) {
    id = lerId(p + o, n - o, &ui);
    if (!id) break;
    tam = lerTam(p + o + ui, n - o - ui, &ut);
    if (tam < 0) break;
    o += ui + ut;
    if (o + tam > n) break;        // elemento passa da janela: o SeekHead resolve
    if (id == ID_SEEKHEAD) { f->seekHeadVisto = 1; lerSeekHead(f, p + o, tam); }
    else if (id == ID_INFO) { lerInfo(f, p + o, tam); achouInfo = 1; }
    else if (id == ID_TRACKS) {
      tracksVisto = 1;
      achouTracks = lerTracks(f, p + o, tam);
      if (achouTracks < 0) { free(p); return MKVASS_NOGO_FAIXA; }
    }
    else if (id == ID_ATTACHMENTS) {
      achouAttachments = lerAttachments(f, p + o, tam);
      if (!achouAttachments) { free(p); return MKVASS_NOGO_REDE; }
      f->fontesCompletas = 1;
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
  /* Attachments costumam ficar depois de Tracks e fora da primeira janela.
   * O SeekHead da maioria dos muxers aponta para eles; lemos o tamanho do
   * elemento primeiro e so entao buscamos os bytes, sem baixar o video. */
  if (!achouAttachments && !f->fontesCompletas && f->posAttachments >= 0) {
    long cabTam = 0, dadosN = 0; unsigned char *cab = range(f, f->posAttachments, 64, &cabTam);
    if (!cab) return MKVASS_NOGO_REDE;
    if (cabTam > 0) {
      int ai = larguraDe(cab[0]), at = 0; long an;
      an = ai > 0 && ai < cabTam ? lerTam(cab + ai, cabTam - ai, &at) : -1;
      if (an > 0 && an <= MKVASS_CORPO_MAX) {
        long inicio = ai + at;
        unsigned char *dados = NULL;
        int dadosAlocados = 0;
        if (inicio + an <= cabTam) { dados = cab + inicio; dadosN = an; }
        else { dados = range(f, f->posAttachments + inicio, an, &dadosN); dadosAlocados = 1; }
        if (dados && dadosN >= an) achouAttachments = lerAttachments(f, dados, an);
        if (dadosAlocados) free(dados);
      }
    }
    free(cab);
    if (!achouAttachments) return MKVASS_NOGO_REDE;
    f->fontesCompletas = 1;
  }
  if (achouAttachments) f->fontesCompletas = 1;
  else if (f->posAttachments < 0 && f->seekHeadVisto) f->fontesCompletas = 1;
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
  if (n < tam) { free(p); return MKVASS_NOGO_REDE; }
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

/* Tamanhos dos frames de um Block laced. O primeiro byte do payload e o
 * numero de frames menos um; os formatos Xiph, fixed e EBML diferem apenas
 * na forma de escrever os N-1 primeiros tamanhos. */
static int tamanhosLace(const unsigned char *p, long n, unsigned flags,
                        long **saida, int *nFrames, long *cab) {
  long *tam = NULL, pos = 0, soma = 0;
  int i, nf;
  if (!(flags & 0x06)) {
    tam = calloc(1, sizeof *tam);
    if (!tam) return 0;
    tam[0] = n; *saida = tam; *nFrames = 1; *cab = 0; return 1;
  }
  if (n < 1) return 0;
  nf = (int)p[pos++] + 1;
  if (nf < 1 || nf > 256) return 0;
  tam = calloc((size_t)nf, sizeof *tam);
  if (!tam) return 0;
  if ((flags & 0x06) == 0x02) {             /* Xiph lacing */
    for (i = 0; i < nf - 1; i++) {
      long v = 0;
      do {
        if (pos >= n) { free(tam); return 0; }
        v += p[pos++];
      } while (p[pos - 1] == 255);
      tam[i] = v; soma += v;
    }
  } else if ((flags & 0x06) == 0x04) {      /* fixed-size lacing */
    long resto = n - pos;
    if (resto < 0 || resto % nf) { free(tam); return 0; }
    for (i = 0; i < nf - 1; i++) tam[i] = resto / nf;
    soma = tam[0] * (nf - 1);
  } else {                                  /* EBML lacing */
    int w = 0; long v;
    v = lerVint(p + pos, n - pos, &w);
    if (v < 0) { free(tam); return 0; }
    tam[0] = v; soma = v; pos += w;
    for (i = 1; i < nf - 1; i++) {
      unsigned long raw; long delta, bias;
      raw = (unsigned long)lerVint(p + pos, n - pos, &w);
      if (!w) { free(tam); return 0; }
      /* EBML signed integer: bias = 2^(7*w-1)-1. */
      if (w >= 8) bias = LONG_MAX;
      else bias = (1L << (7 * w - 1)) - 1L;
      delta = (long)raw - bias;
      tam[i] = tam[i - 1] + delta;
      if (tam[i] < 0) { free(tam); return 0; }
      soma += tam[i]; pos += w;
    }
  }
  if (pos > n || soma > n - pos) { free(tam); return 0; }
  tam[nf - 1] = n - pos - soma;
  *saida = tam; *nFrames = nf; *cab = pos;
  return 1;
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
    // Start = Timestamp do Cluster + relativo do bloco. Sem Timestamp no
    // Cluster (nao deveria acontecer), o CueTime e a mesma coisa vista do
    // indice.
    ini = cl->temTs ? segundosDe(f, cl->ts) + (double)rel * (double)f->escala / 1e9
                    : segundosDe(f, cueTempo);
    fim = ini + (temDur ? segundosDe(f, dur) : MKVASS_DUR_PADRAO);
    if (fim <= ini) fim = ini + 0.5;
    {
      const unsigned char *frames = bl + vt + 3;
      long framesN = blN - vt - 3, cab, *tams = NULL, off = 0;
      int nf = 0, fi;
      if (!tamanhosLace(frames, framesN, flags, &tams, &nf, &cab)) return total;
      for (fi = 0; fi < nf; fi++) {
        if (tams[fi] > 0 && anexarEvento(f, frames + cab + off, tams[fi], ini, fim)) {
          f->sujo = 1;
          // A fala chegou quando ja devia estar na tela (ou ja tinha saido).
          // Antes do seek/escolha e esperado; no meio do filme e o sintoma.
          if (f->posRef > 0 && fim <= f->posRef) f->perdidos++;
          else if (f->posRef > 0 && ini < f->posRef) {
            f->atrasados++;
            if (f->atrasados <= 20)
              printf("[mkvass] fala %.3f-%.3f colhida TARDE: playhead %.3f (%+.0f ms)\n",
                     ini, fim, f->posRef, (f->posRef - ini) * 1000.0);
          }
        }
        off += tams[fi];
      }
      free(tams);
    }
    return total;
  }
}

// Interpreta os pontos [i..j] a partir de um buffer que comeca no byte
// `bufIni` do arquivo. Mesmo contrato de colherGrupo.
static int colherDoBuffer(Fio *f, int i, int j, unsigned char *p, long n, long bufIni,
                          const ClCache *cl) {
  int k;
  for (k = i; k <= j; k++) {
    long o = f->pontos[k].cluster + cl->hdr + f->pontos[k].rel - bufIni;
    long r = (o >= 0 && o < n) ? lerBloco(f, p + o, n - o, cl, f->pontos[k].tempo) : 0;
    if (r < 0) {
      // Uma fala maior que a janela: completa com um Range so para ela.
      long falta = -r, m = 0;
      unsigned char *q = malloc((size_t)(n - o + falta));
      unsigned char *resto = q ? range(f, bufIni + n, falta, &m) : NULL;
      if (q && resto && m >= falta) {
        memcpy(q, p + o, (size_t)(n - o)); memcpy(q + (n - o), resto, (size_t)falta);
        r = lerBloco(f, q, n - o + falta, cl, f->pontos[k].tempo);
      }
      if (q && (!resto || m < falta)) { free(q); free(resto); return -2; }
      free(q); free(resto);
    }
    f->pontos[k].colhido = r > 0 ? 1 : 2;
    if (r > 0) f->nColhidos++;
  }
  return 1;
}

// 1 quando p[0..n) comeca com um bloco (BlockGroup com Block primeiro, ou
// SimpleBlock) desta faixa. So o cabecalho: o bloco pode passar da janela.
static int blocoDaFaixa(const Fio *f, const unsigned char *p, long n) {
  int ui = 0, ut = 0, vt = 0; unsigned long id; long tam, blN;
  const unsigned char *bl;
  id = lerId(p, n, &ui);
  if (id != ID_BLOCKGROUP && id != ID_SIMPLEBLOCK) return 0;
  tam = lerTam(p + ui, n - ui, &ut);
  if (tam < 4) return 0;
  bl = p + ui + ut; blN = n - ui - ut; if (blN > tam) blN = tam;
  if (id == ID_BLOCKGROUP) {
    int bi = 0, bt = 0; long bn;
    if (lerId(bl, blN, &bi) != ID_BLOCK) return 0;
    bn = lerTam(bl + bi, blN - bi, &bt);
    if (bn < 4 || bi + bt + bn > tam) return 0;
    bl += bi + bt; blN -= bi + bt;
  }
  return lerVint(bl, blN, &vt) == f->faixa && vt > 0;
}

// PALPITE do cabecalho do Cluster: id (4 bytes) + tamanho (1 a 8). O
// CueRelativePosition conta a partir dos DADOS do Cluster, entao o bloco esta
// em cluster + 5..12 + rel. Um Range cobrindo os oito lugares possiveis acha o
// bloco sem o Range de 24 bytes do cabecalho — METADE dos pedidos, porque na
// legenda de anime quase toda fala cai num Cluster diferente. O Start vem do
// CueTime (e o timestamp absoluto do bloco, pela especificacao). So vale com
// UM lugar que bate; ambiguo ou nenhum, volta ao caminho do cabecalho.
// Devolve: 1 colheu, 0 rede, -2 resposta curta, 2 palpite nao serviu.
static int colherPorPalpite(Fio *f, int i, int j) {
  long cl0 = f->pontos[i].cluster;
  long base = cl0 + 5 + f->pontos[i].rel;
  long fim = cl0 + 12 + f->pontos[j].rel + MKVASS_BLOCO;
  long n = 0; unsigned char *p; int h, achou = 0, bate = 0, r;
  ClCache *c;
  p = range(f, base, fim - base, &n);
  if (!p) return 0;
  if (n < fim - base) { free(p); return -2; }
  for (h = 5; h <= 12; h++)
    if (blocoDaFaixa(f, p + (h - 5), n - (h - 5))) { achou = h; bate++; }
  if (bate != 1) { free(p); f->palpitesFalhos++; return 2; }
  c = &f->cl[f->clProx]; f->clProx = (f->clProx + 1) % CL_CACHE;
  c->pos = cl0; c->hdr = achou; c->temTs = 0; c->ts = 0;
  f->palpites++;
  r = colherDoBuffer(f, i, j, p, n, base, c);
  free(p);
  return r;
}

// Colhe o grupo de pontos [i..j] (mesmo Cluster, proximos) num Range so.
// Marca cada um como colhido (1) ou desistido (2). Devolve 1 se a rede
// respondeu.
static int colherGrupo(Fio *f, int i, int j) {
  const ClCache *cl = NULL;
  long ini, n = 0, fim; unsigned char *p; int k, r;
  for (k = 0; k < CL_CACHE; k++)
    if (f->cl[k].hdr && f->cl[k].pos == f->pontos[i].cluster) { cl = &f->cl[k]; break; }
  if (!cl) {
    r = colherPorPalpite(f, i, j);
    if (r != 2) return r;
    cl = cluster(f, f->pontos[i].cluster);
  }
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
  r = colherDoBuffer(f, i, j, p, n, ini, cl);
  free(p);
  return r;
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
// SO A PRIMEIRA entrega passa fontes e carrega do zero. As seguintes sao o
// mesmo documento com mais eventos: legenda_atualizar_corpo troca a faixa do
// libass sem apagar o quadro em tela. Antes, TODA entrega (uma por passada, a
// cada 1-2 s enquanto colhia) apagava a legenda, desligava o libass e
// reenviava as fontes — o pisca "por lote".
static int entregarCorpoSeAtual(Fio *f, const char *corpo) {
  int ok = 0;
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && !S.parar) {
    int i;
    if (!f->entregas) {
      assrender_limpar_fontes();
      for (i = 0; i < f->nFontes; i++)
        assrender_adicionar_fonte(f->fontes[i].nome, f->fontes[i].dados,
                                  (size_t)f->fontes[i].tam);
      legenda_definir_corpo(corpo);
    } else legenda_atualizar_corpo(corpo);
    f->entregas++;
    ok = 1;
  }
  pthread_mutex_unlock(&S.trava);
  return ok;
}

static int contarEventos(const Fio *f) {
  int n = 0; const char *p = f->corpo;
  while (p && (p = strstr(p, "\nDialogue: ")) != NULL) { n++; p += 11; }
  return n;
}

static void entregar(Fio *f) {
  int n;
  if (!f->sujo || !f->corpo) return;
  if (!entregarCorpoSeAtual(f, f->corpo)) return;
  f->sujo = 0; f->ultEntrega = agoraMs();
  n = contarEventos(f);
  if (n > 0 && !f->primeiraFala) {
    f->primeiraFala = 1;
    printf("[mkvass] primeira entrega com fala: %d eventos, %ld ms desde a escolha (playhead %.1f s, %ld Ranges)\n",
           n, f->ultEntrega - f->t0, f->posRef, S.pedidos);
    fflush(stdout);
  }
  f->eventosEntregues = n;
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

// Proximo ponto pendente pela prioridade do laco (ver trabalhar). -1 quando
// nao ha nenhum. *noJanela diz se ele esta em [ini, fim].
static int proximoPendente(const Fio *f, double ini, double fim, int *noJanela) {
  int i, frente = -1, atras = -1;
  *noJanela = 0;
  for (i = 0; i < f->nPontos; i++) {
    double t;
    if (f->pontos[i].colhido) continue;
    t = segundosDe(f, f->pontos[i].tempo);
    if (t >= ini && t <= fim) { *noJanela = 1; return i; }
    if (t > fim) { if (frente < 0) frente = i; }
    else atras = i;               // o ultimo antes de ini: o mais perto
  }
  return frente >= 0 ? frente : atras;
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
  snprintf(f->sidecarFontes, sizeof f->sidecarFontes, "%s.fonts", f->sidecar);
  sc = dados_ler(f->sidecar);
  if (sc && !strncmp(sc, MARCA_COMPLETO, strlen(MARCA_COMPLETO))) {
    // Os eventos e anexos usam versoes/provas separadas. Um marcador de corpo
    // sem cache de fontes correspondente nao pode virar cache completo.
    if (lerFontesSidecar(f)) {
      if (!entregarCorpoSeAtual(f, sc + strlen(MARCA_COMPLETO))) { free(sc); goto fim; }
      if (!definirEstadoSeAtual(f, MKVASS_COMPLETO)) { free(sc); goto fim; }
      printf("[mkvass] sidecar completo %s: sem rede\n", f->sidecar);
      fflush(stdout);
      free(sc);
      goto fim;
    }
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

  // O laco. Cada escolha pega o proximo ponto pendente nesta ordem: a janela
  // [pos - atras, pos + janela] em ordem de tempo; depois o resto A FRENTE; por
  // ultimo o que ficou atras, do mais perto para o mais longe. Assim a faixa
  // inteira chega em segundo plano (e o sidecar fecha completo), mas a fala
  // da proxima cena e sempre a primeira da fila. Antes so a janela era
  // colhida, e o fio dormia quando ela acabava.
  { int janelaCheia = 0; double janelaDe = -1;
  while (minhaVez(f)) {
    double pos, ini, fim; int i, feitos = 0, colheuAlgo = 0;
    pthread_mutex_lock(&S.trava);
    if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); goto sair; }
    pos = S.pos; S.nColhidos = f->nColhidos;
    pthread_mutex_unlock(&S.trava);
    while (feitos < MKVASS_RANGES_POR_SEG && minhaVez(f)) {
      double t; int j, noJanela;
      ini = pos - MKVASS_ATRAS_SEG; fim = pos + MKVASS_JANELA_SEG;
      i = proximoPendente(f, ini, fim, &noJanela);
      if (i < 0) break;
      if (!noJanela && (!janelaCheia || janelaDe != pos)) {
        janelaCheia = 1; janelaDe = pos;
        printf("[mkvass] janela %.0f-%.0f s colhida: %ld ms desde a escolha, %d/%d blocos, %ld Ranges, "
               "palpites %d (falhos %d), tarde %d, antes do playhead %d\n",
               ini, fim, agoraMs() - f->t0, f->nColhidos, f->nPontos, S.pedidos,
               f->palpites, f->palpitesFalhos, f->atrasados, f->perdidos);
        fflush(stdout);
      }
      t = segundosDe(f, f->pontos[i].tempo);
      // Junta os vizinhos do mesmo Cluster que cabem em MKVASS_JUNTAR.
      j = i;
      while (j + 1 < f->nPontos && !f->pontos[j + 1].colhido &&
             f->pontos[j + 1].cluster == f->pontos[i].cluster &&
             f->pontos[j + 1].rel >= f->pontos[i].rel &&
             f->pontos[j + 1].rel - f->pontos[i].rel < MKVASS_JUNTAR) j++;
      f->posRef = pos;
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
      } else colheuAlgo = 1;
      }
      feitos++;
      // Fala da cena de agora: ao overlay ja, sem esperar a passada.
      if (t < pos + MKVASS_URGENTE_SEG || agoraMs() - f->ultEntrega >= MKVASS_ENTREGA_MS)
        entregar(f);
      // O playhead pode ter saltado (seek) durante o Range: reprioriza.
      pthread_mutex_lock(&S.trava);
      pos = S.pos;
      pthread_mutex_unlock(&S.trava);
    }
    entregar(f);
    pthread_mutex_lock(&S.trava);
    if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); goto sair; }
    S.nColhidos = f->nColhidos;
    pthread_mutex_unlock(&S.trava);
    if (f->nColhidos + contarDesistidos(f) >= f->nPontos) {
      // Tudo o que o indice tinha. Os desistidos (2) contam como feitos: o
      // indice apontava para algo que nao era um bloco desta faixa.
      gravarSidecar(f, 1, 0);
      if (!definirEstadoSeAtual(f, MKVASS_COMPLETO)) goto fim;
      printf("[mkvass] completo: %d/%d blocos, %ld Ranges, %ld ms desde a escolha "
             "(palpites %d, falhos %d; tarde %d, antes do playhead %d)\n",
             f->nColhidos, f->nPontos, S.pedidos, agoraMs() - f->t0,
             f->palpites, f->palpitesFalhos, f->atrasados, f->perdidos);
      fflush(stdout);
      goto fim;
    }
    if (!feitos || !colheuAlgo) {
      // Nada pendente que a rede devolvesse: espera a posicao andar.
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
  } }
sair:
  // Saiu antes do fim (parar, troca de faixa, rede): guarda o que ha para a
  // proxima abertura nao repetir os Ranges ja pagos.
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao) S.nColhidos = f->nColhidos;
  pthread_mutex_unlock(&S.trava);
  if (f->nColhidos > 0 && mkvass_estado() != MKVASS_COMPLETO) gravarSidecar(f, 0, 1);

fim:
  pthread_mutex_lock(&S.trava);
  S.vivos--;
  pthread_cond_broadcast(&S.sinal);
  pthread_mutex_unlock(&S.trava);
  free(f->pontos); free(f->corpo); liberarFontes(f); free(f);
  return NULL;
}

// --- API ---------------------------------------------------------------------------

static void iniciarFaixa(const char *url, int numeroFaixa) {
  Fio *f; pthread_t t;
  if (!url || !*url || numeroFaixa == 0) return;
  f = calloc(1, sizeof *f);
  if (!f) return;
  snprintf(f->url, sizeof f->url, "%s", url);
  f->faixa = numeroFaixa;
  f->t0 = agoraMs();
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

void mkvass_iniciar(const char *url, int numeroFaixa) {
  if (numeroFaixa <= 0) return;
  iniciarFaixa(url, numeroFaixa);
}

void mkvass_iniciar_ordinal(const char *url, int ordinalFaixa) {
  if (ordinalFaixa < 0 || ordinalFaixa >= 64) return;
  iniciarFaixa(url, -ordinalFaixa - 1);
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
