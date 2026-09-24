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
#define MKVASS_CUES_1      (256L * 1024)
// Teto de blocos e de corpo. O mesmo teto de legenda.c (LEG_MAX_CUES): mais
// do que isto o overlay nao aceita de qualquer forma.
#define MKVASS_MAX_PONTOS  8000
#define MKVASS_CORPO_MAX   (16L * 1024 * 1024)
// Falhas de Range seguidas antes de desistir (NOGO_REDE). Entre uma e outra
// o fio RECUA (0,5, 1, 2, 4 s): antes eram 500 ms fixos, e cinco pedidos em
// 2,5 s contra um CDN que acabou de recusar uma conexao so repetiam a recusa.
// NOGO_REDE nao e o fim: faixas.c tenta de novo com recuo, sem limite, enquanto
// a faixa estiver escolhida (ver mkvass_recuo_ms).
#define MKVASS_FALHAS_MAX  5
// Janela menor da varredura, depois de estouros de prazo: cada estouro corta a
// janela pela metade ate aqui. 64 KB ainda cobre o cabecalho de um Cluster e
// dezenas de blocos de legenda.
#define MKVASS_VARRE_CH_MIN (64L * 1024)
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
// Ranges EM PARALELO. Medido na C9 com a fonte Debridio (23/09): ~1,2 s por
// Range, qualquer tamanho — um por vez colhia uma fala a cada 1,2 s, e a
// janela de 90 s levou 49 s. Tres conexoes a mais (o pipeline ja usa uma ou
// duas) triplicam o ritmo sem disputar banda: cada pedido e de ~1 KB.
#ifndef MKVASS_PARALELOS
#define MKVASS_PARALELOS   3
#endif
#define MKVASS_PREBUSCA    8
// VARREDURA (#92, webOS 25): quando o indice NAO tem CuePoint da faixa de
// legenda (mkvmerge --cues none, remux que so indexa o video) ou os tem sem
// CueRelativePosition, o bloco so se acha lendo o Cluster. Em vez de
// desistir e entregar a faixa ao renderizador da TV (o "pisca e corta metade
// da frase"), o modulo VARRE os Clusters: janelas de MKVASS_VARRE_CH bytes,
// pulando por cima do payload de video/audio que passa da janela (o proximo
// pedido comeca no fim do bloco, sem baixa-lo). Custa banda — baixa boa parte
// do arquivo, nao 0,07 % — por isso so entra quando o indice nao serve, e
// avisa no log. 512 KB e o compromisso entre round-trips (~1,2 s cada na C9)
// e o teto de MKVASS_RANGES_POR_SEG: no maximo 4 MB/s de leitura extra.
#define MKVASS_VARRE_CH    (512L * 1024)
// Teto de um bloco da faixa lido inteiro na varredura. Uma fala de ASS tem
// dezenas de bytes; um desenho vetorial, alguns KB. Mais que isto e video
// com o numero da faixa por coincidencia.
#define MKVASS_VARRE_BLOCO (1L * 1024 * 1024)
// JANELA da varredura, em segundos de midia a frente do playhead. E o que
// limita o trafego: sem indice, cada segundo de janela custa um segundo de
// VIDEO em bytes, nao algumas falas — num MKV de 1,4 GB varrer tudo dobraria
// o trafego da reproducao. A varredura NUNCA sai da janela: quando ela acaba
// o fio dorme ate o playhead andar, e um seek recomeca na nova posicao.
// (Os testes compilam com um valor menor para provar a proporcao.)
#ifndef MKVASS_VARRE_JANELA_SEG
#define MKVASS_VARRE_JANELA_SEG 120.0
#endif
// Atras do playhead: o Cluster que contem a posicao entra sempre; alem dele,
// so o que ficou para tras ha pouco (seek curto para tras).
#define MKVASS_VARRE_ATRAS_SEG 30.0
// Folga minima do buffer de VIDEO para a varredura pedir bytes. Abaixo disto
// ela pausa: o video tem prioridade sobre a legenda na mesma conexao.
// Quem informa e o player (mkvass_folga); sem informacao (Tizen, que so da
// porcentagem) nao ha pausa.
#define MKVASS_VARRE_FOLGA_SEG 20.0
// Trechos ja varridos guardados no modo SEM Cues nenhum (para o salto por
// bitrate nao reler o que ja veio).
#define MKVASS_COB 32

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
  long          cursor;    // varredura: onde retomar dentro do trecho (0 = do inicio)
  double        cursorTs;  // tempo do Cluster no cursor (-1 = desconhecido). Enquanto
                           // ele passa da janela, o fio dorme SEM tocar a rede.
} Ponto;

// Varredura sem Cues nenhum: trecho ja lido, em bytes e em tempo de midia.
typedef struct { long b0, b1; double t0, t1; } Cob;
// Janela de bytes da varredura: [ini, ini+n) do arquivo, num buffer so.
// `eof`: o servidor devolveu menos do que o pedido sem fim conhecido.
typedef struct { unsigned char *p; long ini, n; int eof; } Janela;

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
  char     url[4096];       // mesmo limite de Stream.url e video_url_atual
  int      faixa;
  unsigned geracao;
  int      estado;
  double   pos;
  int      vivos;          // fios de colheita vivos (o novo espera o velho)
  int      parar;
  long     pedidos, bytes;
  int      nPontos, nColhidos;
  int      varredura;      // 0 pelo indice; 1 varrendo Clusters (ver MKVASS_VARRE_CH)
  double   folga;          // buffer de video a frente (s); < 0 = desconhecido
  unsigned fontesLegG;     // geracao da legenda em que as fontes do MKV entraram (0 = nenhuma)
  // O QUE A URL ENSINOU, guardado entre as tentativas (mkvass_retomar) da
  // MESMA url e faixa; um pedido novo zera. `urlFinal`: o endereco depois dos
  // redirecionamentos, resolvido UMA vez ("" = nao resolvida ou igual).
  // `paralelos`: conexoes extras que o servidor aceita (cai para 1 no primeiro
  // freio). `varreCh`: janela da varredura (cai pela metade a cada prazo
  // estourado). `ultHttp`/`ultCurl`: a ultima falha, para o log e o aviso.
  char     urlFinal[4096];
  int      paralelos;
  long     varreCh;
  int      ultHttp, ultCurl;
} S = { PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, "", 0, 0,
        MKVASS_OCIOSO, 0.0, 0, 0, 0, 0, 0, 0, 0, -1.0, 0, "", 0, 0, 0, 0 };

// Tudo abaixo e DO FIO: so o fio de colheita toca, sem trava.
typedef struct {
  unsigned g;
  char     url[sizeof S.url];
  int      faixa;
  long     segIni;         // onde comecam os dados do Segment
  unsigned long escala;    // TimestampScale (ns por unidade)
  long     posTracks, posCues, posInfo;   // absolutas; -1 = SeekHead nao disse
  long     posAttachments;
  int      seekHeadVisto;
  long     segFim;         // fim do Segment (absoluto); -1 = tamanho desconhecido
  long     primeiroCluster;// posicao do primeiro Cluster; -1 = ainda nao visto
  // VARREDURA (ver MKVASS_VARRE_CH): os `pontos` deixam de ser blocos e passam
  // a ser Clusters a varrer (rel = -1). `varreEncadeia`: a lista veio do
  // indice do VIDEO e pode faltar Cluster entre dois pontos — a varredura
  // segue de um ponto ate o Cluster do seguinte; 0 = a lista veio da propria
  // faixa (sem CueRelativePosition) e cada Cluster listado basta.
  int      varredura, varreEncadeia;
  // Modo SEM Cues nenhum (varreInteira): um trecho so, lido por um cursor
  // dentro da janela; seek longe salta por bitrate (varreDur vem do Info) e
  // ressincroniza no proximo Cluster; `cob` guarda o que ja foi lido.
  int      varreInteira, varreSaltou;
  double   varreDur;       // Duration do Segment, em segundos (0 = nao veio)
  double   varreTs;        // tempo do ultimo Cluster visto pela varredura (-1 = nenhum)
  Cob      cob[MKVASS_COB];
  int      nCob;
  Janela   jan;            // janela da varredura, viva entre trechos
  Ponto   *pontos;
  int      nPontos, nColhidos;
  char    *corpo;          // cabecalho ASS + linhas Dialogue: colhidas
  size_t   corpoTam, corpoCap;
  ClCache  cl[CL_CACHE];
  int      clProx;
  char     sidecar[64];
  char     sidecarFontes[80];
  int      falhas;         // Ranges falhados seguidos
  // POR QUE FALHOU (#92). `urlOrig`: a url pedida (a do player); `url` pode
  // ser o endereco final depois dos redirecionamentos. `ultSt`/`ultErro`: HTTP
  // e libcurl do ultimo Range falho. `definitivo`: o HTTP de uma recusa que
  // nao passa tentando de novo (404, 410, 401, 400, 416 no byte 0, 403 com uma
  // conexao so) — vira MKVASS_NOGO_HTTP. `freios`: recusas de CDN (429, 503,
  // 403) vistas. `reresolveu`: a url final ja foi trocada de volta pela
  // original uma vez (link vencido).
  char     urlOrig[sizeof S.url];
  int      ultSt, ultErro, definitivo, freios, reresolveu, paralelos;
  int      finalVisto;     // o endereco final ja foi lido de uma resposta
  long     varreCh;
  // CuePoint da faixa cujo bloco nao se acha pela posicao: sem
  // CueRelativePosition (rel = -1 desde o indice) ou com uma que nao cai num
  // bloco da faixa (vira -1 na colheita). Esses pontos sao colhidos lendo o
  // Cluster INTEIRO (colherCluster) — so dentro da janela de midia e com
  // folga de buffer, como a varredura, porque cada um custa um Cluster.
  int      semRel, relRuins;
  int      trechoSemCluster;   // colherCluster: o indice apontava para outra coisa
  double   folgaJan;           // S.folga copiada pelo laco (ver proximoPendente)
  int      sujo;           // corpo mudou desde a ultima entrega ao overlay
  // GERACAO DA LEGENDA a que este fio entrega (legenda_geracao na escolha da
  // faixa; a nova depois da primeira carga). Lote com geracao que nao e mais a
  // da legenda e de uma faixa que saiu: descartado. `herdar`: nova tentativa
  // da MESMA faixa (mkvass_retomar) — continua o documento em tela sem
  // recarregar, se ele ainda for desta geracao.
  unsigned legG;
  int      herdar;
  // `segurar` (mkvass_retomar_segurando): nao entrega ate nColhidos passar de
  // `colhidosIni` (o que o sidecar parcial trouxe) ou a faixa fechar.
  int      segurar, colhidosIni;
  int      entregas;       // 0 = a proxima e a primeira (fontes + carga cheia)
  long     t0, ultEntrega; // ms monotonicos: pedido, ultima entrega
  int      eventosEntregues, primeiraFala;
  double   posRef;         // playhead quando o grupo foi pedido (medida)
  int      atrasados, perdidos, palpites, palpitesFalhos;
  long     redeMs, redeMaxMs, redeN;   // latencia dos Ranges (medida)
  struct Job *pre[MKVASS_PREBUSCA];     // pre-busca: Ranges ja pedidos ao pool
  struct Job *jobFontes;               // Attachments, lidos em segundo plano
  int      fontesPasso;                // 0 nada; 1 cabecalho pedido; 2 dados pedidos; 3 feito
  long     fontesCabN;
  int      fontesRepetidas;             // pedidos de fontes refeitos apos falha de rede
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

// Teto de Ranges por segundo: SEGURA o fio quando o segundo corrente
// estourou. Todo pedido passa por aqui (range e o pool), entao o teto vale
// para tudo (cabecalho, Cues e blocos).
// Chamado por quem VAI pedir (o fio no range, o pool antes de cada pedido):
// o teto vale para o instante em que o servidor recebe o pedido.
static pthread_mutex_t vezTrava = PTHREAD_MUTEX_INITIALIZER;
static void esperarVez(void) {
  // Janela DESLIZANTE: no maximo MKVASS_RANGES_POR_SEG inicios em qualquer
  // 1 s (+50 ms de folga para o pedido chegar ao servidor). O corte por
  // segundo do relogio deixava 8 no fim de um segundo e 8 no comeco do
  // seguinte — 16 num segundo visto do servidor, com os fios do pool.
  static long inicios[MKVASS_RANGES_POR_SEG]; static int prox;
  long agora, espera;
  pthread_mutex_lock(&vezTrava);
  agora = agoraMs();
  espera = inicios[prox] ? inicios[prox] + 1050L - agora : 0;
  if (espera > 0) { usleep((useconds_t)(espera * 1000L)); agora = agoraMs(); }
  inicios[prox] = agora ? agora : 1;
  prox = (prox + 1) % MKVASS_RANGES_POR_SEG;
  pthread_mutex_unlock(&vezTrava);
}

// --- pool de Ranges ----------------------------------------------------------
//
// Fios fixos, vivos a sessao inteira: cada um guarda o seu handle da libcurl
// (rede.c: um handle por fio), e a conexao TLS fica aberta entre pedidos. Um
// fio novo por pedido pagaria o handshake de novo a cada fala.
typedef struct Job {
  char url[sizeof S.url];
  long ini, n;
  unsigned char *r; long tam, ms;
  int st, erro;               // HTTP e libcurl da resposta (ver rede_baixar_trecho_st)
  int estado;                 // 0 na fila, 1 baixando, 2 pronto
  unsigned g;                 // geracao de quem pediu (contabilidade)
  struct Job *prox;
} Job;
static pthread_mutex_t PT = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  PTem = PTHREAD_COND_INITIALIZER, PFeito = PTHREAD_COND_INITIALIZER;
static Job *filaIni, *filaFim;
static int poolFios;

// Prazo de um Range: 15 s como sempre, mais 1 s por 64 KB. O Cues de 256 KB e
// a janela de 512 KB da varredura disputam a conexao com o video; um prazo
// fixo de 15 s para 512 KB numa TV que baixa o video a 9 Mbit/s estourava sem
// a rede estar caida.
static int prazoDe(long n) {
  long s = 15 + n / (64L * 1024);
  return s > 45 ? 45 : (int)s;
}

static void *poolFio(void *u) {
  (void)u;
  for (;;) {
    Job *j; long t;
    pthread_mutex_lock(&PT);
    while (!filaIni) pthread_cond_wait(&PTem, &PT);
    j = filaIni; filaIni = j->prox; if (!filaIni) filaFim = NULL;
    j->estado = 1;
    pthread_mutex_unlock(&PT);
    esperarVez();
    t = agoraMs();
    j->tam = 0; j->st = j->erro = 0;
    j->r = (unsigned char *)rede_baixar_trecho_st(j->url, prazoDe(j->n), j->ini, j->ini + j->n - 1,
                                                  &j->tam, &j->st, &j->erro, NULL, 0);
    j->ms = agoraMs() - t;
    // Contado AQUI, quando o servidor respondeu, e nao quando o fio consome:
    // a pre-busca consumida em rajada parecia 10 pedidos num segundo.
    pthread_mutex_lock(&S.trava);
    if (j->g == S.geracao && !S.parar) { S.pedidos++; if (j->r) S.bytes += j->tam; }
    pthread_mutex_unlock(&S.trava);
    pthread_mutex_lock(&PT);
    j->estado = 2;
    pthread_cond_broadcast(&PFeito);
    pthread_mutex_unlock(&PT);
  }
  return NULL;
}

static Job *submeter(Fio *f, long ini, long n) {
  Job *j = calloc(1, sizeof *j);
  if (!j) return NULL;
  snprintf(j->url, sizeof j->url, "%s", f->url);
  j->ini = ini; j->n = n; j->g = f->g;
  pthread_mutex_lock(&PT);
  while (poolFios < MKVASS_PARALELOS) {
    pthread_t t;
    if (pthread_create(&t, NULL, poolFio, NULL) != 0) break;
    pthread_detach(t); poolFios++;
  }
  if (!poolFios) { pthread_mutex_unlock(&PT); free(j); return NULL; }
  if (filaFim) filaFim->prox = j; else filaIni = j;
  filaFim = j;
  pthread_cond_signal(&PTem);
  pthread_mutex_unlock(&PT);
  return j;
}

static int jobPronto(Job *j) {
  int e;
  pthread_mutex_lock(&PT); e = j->estado; pthread_mutex_unlock(&PT);
  return e == 2;
}

static void contarRede(Fio *f, long ms) {
  f->redeN++; f->redeMs += ms; if (ms > f->redeMaxMs) f->redeMaxMs = ms;
}

// Recusa que NAO passa tentando de novo a mesma url: arquivo que sumiu (404,
// 410), link sem permissao (401), pedido mal formado (400) e 416 no byte 0 (o
// servidor nao serve Range nenhum). 416 mais adiante e so "passou do fim".
static int httpDefinitivo(int st, long ini) {
  return st == 400 || st == 401 || st == 404 || st == 410 || (st == 416 && ini == 0);
}
// FREIO do CDN: 429 (pedidos demais), 503 (ocupado) e 403 — que os CDNs de
// debrid tambem usam para "conexoes demais neste link". A resposta e ter UMA
// conexao extra, nao cinco pedidos seguidos (ver falhou).
static int httpFreio(int st) { return st == 403 || st == 429 || st == 503; }

static void publicarAprendido(Fio *f) {
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && !S.parar) {
    S.ultHttp = f->ultSt; S.ultCurl = f->ultErro;
    S.paralelos = f->paralelos; S.varreCh = f->varreCh;
    if (strcmp(f->url, f->urlOrig)) snprintf(S.urlFinal, sizeof S.urlFinal, "%s", f->url);
    else S.urlFinal[0] = 0;
  }
  pthread_mutex_unlock(&S.trava);
}

// UM Range falhou. Conta a falha e decide O QUE ELA ENSINA (#92):
//   - recusa definitiva: na url final, volta UMA vez a original (o link do
//     debrid pode ter vencido e o redirecionador emite outro); na original, a
//     faixa desiste com o codigo (MKVASS_NOGO_HTTP) em vez de "falha de rede";
//   - freio do CDN: passa a UMA conexao extra (a do video e a outra);
//   - prazo estourado (curl 28): janela da varredura pela metade, e a partir
//     do segundo seguido tambem uma conexao so.
// Uma linha de log por falha, com HTTP e libcurl: era o que faltava para ler
// um "falha de rede" no registro de quem relatou.
static void falhou(Fio *f, long ini, long n, int st, int erro) {
  int def;
  f->falhas++;
  f->ultSt = st; f->ultErro = erro;
  printf("[mkvass] Range %ld+%ld falhou: HTTP %d, curl %d (%d seguida(s), %d conexao(oes) extra(s), url %s)\n",
         ini, n, st, erro, f->falhas, f->paralelos, strcmp(f->url, f->urlOrig) ? "final" : "original");
  def = httpDefinitivo(st, ini) || (st == 403 && f->paralelos <= 1 && f->freios > 0);
  if (def) {
    if (!f->reresolveu && strcmp(f->url, f->urlOrig)) {
      f->reresolveu = 1;
      snprintf(f->url, sizeof f->url, "%s", f->urlOrig);
      f->falhas = 0;
      f->finalVisto = 0;     // a proxima resposta traz o link novo
      printf("[mkvass] HTTP %d na url final: de volta a url original (o link pode ter vencido)\n", st);
    } else {
      f->definitivo = st;
      f->falhas = MKVASS_FALHAS_MAX;
      printf("[mkvass] HTTP %d: recusa definitiva, nao adianta tentar de novo\n", st);
    }
  } else {
    if (httpFreio(st)) f->freios++;
    if ((httpFreio(st) || (erro == 28 && f->falhas >= 2)) && f->paralelos > 1) {
      f->paralelos = 1;
      printf("[mkvass] freio do servidor (HTTP %d, curl %d): 1 conexao extra daqui em diante\n", st, erro);
    }
    if (erro == 28 && f->varreCh > MKVASS_VARRE_CH_MIN) {
      f->varreCh /= 2;
      if (f->varreCh < MKVASS_VARRE_CH_MIN) f->varreCh = MKVASS_VARRE_CH_MIN;
      printf("[mkvass] prazo estourado: janela da varredura agora %ld KB\n", f->varreCh / 1024);
    }
  }
  fflush(stdout);
  publicarAprendido(f);
}

// Recuo entre falhas seguidas DENTRO do fio: 0,5, 1, 2, 4, 8 s. Acorda antes
// so para parar ou trocar de faixa — o playhead andando nao encurta o recuo.
static void recuar(Fio *f) {
  long ms = 500L, ate;
  int k;
  for (k = 1; k < f->falhas && ms < 8000L; k++) ms *= 2;
  if (ms > 8000L) ms = 8000L;
  ate = agoraMs() + ms;
  while (minhaVez(f)) {
    long falta = ate - agoraMs();
    struct timespec rt;
    if (falta <= 0) break;
    if (falta > 250) falta = 250;
    pthread_mutex_lock(&S.trava);
    clock_gettime(CLOCK_REALTIME, &rt);
    rt.tv_nsec += falta * 1000000L;
    while (rt.tv_nsec >= 1000000000L) { rt.tv_sec++; rt.tv_nsec -= 1000000000L; }
    if (f->g == S.geracao && !S.parar) pthread_cond_timedwait(&S.sinal, &S.trava, &rt);
    pthread_mutex_unlock(&S.trava);
  }
}

// Espera o job, faz a MESMA contabilidade de range() e devolve o corpo (que
// passa a ser de quem chamou). O job e liberado.
static unsigned char *colherJob(Fio *f, Job *j, long *tam) {
  unsigned char *r; int atual, st, erro; long ini, n;
  pthread_mutex_lock(&PT);
  while (j->estado != 2) pthread_cond_wait(&PFeito, &PT);
  pthread_mutex_unlock(&PT);
  r = j->r; *tam = j->tam;
  st = j->st; erro = j->erro; ini = j->ini; n = j->n;
  contarRede(f, j->ms);
  free(j);
  pthread_mutex_lock(&S.trava);
  atual = f->g == S.geracao && !S.parar;
  pthread_mutex_unlock(&S.trava);
  if (!atual) { free(r); *tam = 0; return NULL; }
  if (!r) { falhou(f, ini, n, st, erro); return NULL; }
  f->falhas = 0;
  return r;
}

// Um Range. Conta pedidos e bytes e passa pelo teto. Se o mesmo trecho ja foi
// pedido ao pool (pre-busca do laco), espera aquele em vez de pedir de novo.
static int avancarFontes(Fio *f, int esperar);

static unsigned char *range(Fio *f, long ini, long n, long *tam) {
  char *r; int atual, k, st = 0, erro = 0, pedirFinal = !f->finalVisto; long t;
  char fin[sizeof f->url];
  for (k = 0; k < MKVASS_PREBUSCA; k++)
    if (f->pre[k] && f->pre[k]->ini == ini && f->pre[k]->n == n) {
      Job *j = f->pre[k]; f->pre[k] = NULL;
      return colherJob(f, j, tam);
    }
  // Recusa definitiva ja vista: nao bate de novo no servidor.
  if (f->definitivo) { *tam = 0; return NULL; }
  // Uma conexao so: o pedido das fontes (no pool) termina antes deste sair.
  if (f->paralelos <= 1 && f->jobFontes) avancarFontes(f, 1);
  esperarVez();
  *tam = 0;
  t = agoraMs();
  r = rede_baixar_trecho_st(f->url, prazoDe(n), ini, ini + n - 1, tam, &st, &erro,
                            pedirFinal ? fin : NULL, sizeof fin);
  contarRede(f, agoraMs() - t);
  pthread_mutex_lock(&S.trava);
  atual = f->g == S.geracao && !S.parar;
  if (atual) {
    S.pedidos++;
    if (r) S.bytes += *tam;
  }
  pthread_mutex_unlock(&S.trava);
  if (!atual) { free(r); *tam = 0; return NULL; }
  if (!r) { falhou(f, ini, n, st, erro); return NULL; }
  f->falhas = 0;
  // URL FINAL UMA VEZ (#92), lida da PRIMEIRA resposta (sem pedido a mais):
  // link de addon (AIOStreams, Comet...) e um redirecionador, e cada Range
  // pagava o 307 de novo. Num registro de 24/09 (webOS, anime pelo
  // AIOStreams) o redirecionador estourava o prazo (curl 28 com HTTP 307)
  // enquanto o CDN do TorBox respondia ao video. Guardada para as tentativas
  // seguintes; se o endereco final recusar (link vencido), falhou() volta a
  // original uma vez. O sidecar continua pelo nome da url PEDIDA.
  if (pedirFinal) {
    f->finalVisto = 1;
    if (fin[0] && strcmp(fin, f->url)) {
      char a[120], b[120];
      snprintf(f->url, sizeof f->url, "%s", fin);
      printf("[mkvass] url final resolvida: %s -> %s\n",
             rede_url_publica(f->urlOrig, a, sizeof a), rede_url_publica(f->url, b, sizeof b));
      fflush(stdout);
      publicarAprendido(f);
    }
  }
  return (unsigned char *)r;
}

// Os pedidos de UMA VEZ SO (cabecalho, Tracks, Cues): antes um unico Range
// falho ali virava NOGO_REDE na hora, e tres desses — um por tentativa de
// faixas.c — devolviam a faixa a TV com "falha de rede" em poucos segundos.
// Agora cada um insiste com recuo, como os blocos, ate MKVASS_FALHAS_MAX ou
// uma recusa definitiva.
static unsigned char *rangeInsistir(Fio *f, long ini, long n, long *tam) {
  for (;;) {
    unsigned char *p = range(f, ini, n, tam);
    if (p) return p;
    if (f->definitivo || f->falhas >= MKVASS_FALHAS_MAX || !minhaVez(f)) return NULL;
    recuar(f);
    if (!minhaVez(f)) return NULL;
  }
}

// Descarta a pre-busca que sobrou (contada como pedido: o servidor a recebeu).
static void soltarPrebusca(Fio *f) {
  int k;
  for (k = 0; k < MKVASS_PREBUSCA; k++)
    if (f->pre[k]) { long t; Job *j = f->pre[k]; f->pre[k] = NULL; free(colherJob(f, j, &t)); }
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
  long cs; int h, m, sec;
  if (s < 0) s = 0;
  /* CENTESIMOS, como o formato ASS manda. Antes saiam milesimos ("31.660"), e o
   * libass le a fracao como centesimos SEMPRE: string2timecode faz
   * ms * 10. "0:14:31.660" virava 871 s + 6,60 s = 877,6 s (conferido com o
   * libass 0.17.5: Start 877600). Resultado: cada fala entrava 0 a 9,9 s
   * atrasada, conforme os milesimos, e as que ficavam com duracao negativa
   * nunca apareciam. Era o "fora de sincronia" e parte das "falas faltando".
   * O parser de legenda.c le "%lf" e aceita os dois formatos. O erro de
   * arredondamento fica abaixo de 5 ms, menos de um quadro. */
  cs = (long)llround(s * 100.0);
  h = (int)(cs / 360000L); cs -= (long)h * 360000L;
  m = (int)(cs / 6000L);   cs -= (long)m * 6000L;
  sec = (int)(cs / 100L);  cs -= (long)sec * 100L;
  snprintf(dst, tam, "%d:%02d:%02d.%02ld", h, m, sec, cs);
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

// v3: os tempos passaram a centesimos (ver tempoAss). Sidecars v2 guardam
// milesimos, que o libass le errado — sao descartados e colhidos de novo.
#define MARCA_COMPLETO "; mkvass-estado: completo-v3\n"
#define MARCA_PARCIAL  "; mkvass-estado: parcial-v3 "
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

// Float EBML (4 ou 8 bytes, big-endian). Leitura propria de 64 bits: lerUint
// devolve unsigned long, que no ARM de 32 bits do webOS nao guarda 8 bytes.
static double lerFloat(const unsigned char *p, long n) {
  if (n == 4) { union { unsigned int u; float f; } v; v.u = (unsigned int)lerUint(p, 4); return v.f; }
  if (n == 8) { union { unsigned long long u; double d; } v; long i; v.u = 0;
                for (i = 0; i < 8; i++) v.u = (v.u << 8) | p[i]; return v.d; }
  return 0.0;
}

#define ID_DURATION 0x4489UL

static void lerInfo(Fio *f, const unsigned char *p, long n) {
  Iter it = { p, n, 0 }; unsigned long id; const unsigned char *d; long t;
  double dur = 0.0;
  while (proximo(&it, &id, &d, &t)) {
    if (id == ID_TSSCALE) { unsigned long v = lerUint(d, t); if (v) f->escala = v; }
    else if (id == ID_DURATION) dur = lerFloat(d, t);
  }
  // Duration e em unidades de TimestampScale; a escala pode vir depois dela
  // no mesmo Info, por isso a conversao e feita ao fim.
  if (dur > 0.0) f->varreDur = dur * (double)f->escala / 1e9;
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
  unsigned char *p = rangeInsistir(f, 0, MKVASS_CAB, &n);
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
  // O tamanho do Segment e o fim do arquivo para a varredura: pedir alem dele
  // e um 416 que rede.c devolve como falha. -2 = desconhecido (transmissao).
  f->segFim = tam >= 0 ? o + tam : -1;
  f->primeiroCluster = -1;
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
    else if (id == ID_CLUSTER) { f->primeiroCluster = o - ui - ut; break; }
    o += tam;
  }
  free(p);
  // Fora da janela: uma viagem a mais, so quando o SeekHead sabe onde.
  if (!achouInfo && f->posInfo >= 0) {
    p = rangeInsistir(f, f->posInfo, 4096, &n);
    if (p) { ui = 0; if (lerId(p, n, &ui) == ID_INFO) { tam = lerTam(p + ui, n - ui, &ut);
               if (tam > 0 && ui + ut + tam <= n) lerInfo(f, p + ui + ut, tam); }
             free(p); }
  }
  /* Attachments costumam ficar depois de Tracks e fora da primeira janela.
   * O SeekHead da maioria dos muxers aponta para eles; lemos o tamanho do
   * elemento primeiro e so entao buscamos os bytes, sem baixar o video. */
  //
  // EM SEGUNDO PLANO (#92): na C9 cada Range custa ~1,2 s e as fontes vinham
  // antes de tudo, em dois pedidos — a primeira fala esperava por elas. Agora
  // o pedido vai ao pool e o laco segue colhendo; quando as fontes chegam, a
  // proxima entrega recarrega com elas (avancarFontes).
  if (!achouAttachments && !f->fontesCompletas && f->posAttachments >= 0) {
    f->jobFontes = submeter(f, f->posAttachments, 64);
    f->fontesPasso = f->jobFontes ? 1 : 3;
  }
  if (achouAttachments) f->fontesCompletas = 1;
  else if (f->posAttachments < 0 && f->seekHeadVisto) f->fontesCompletas = 1;
  if (!achouTracks) {
    // Tracks inteiro na janela e a faixa nao esta la: nao ha o que buscar.
    if (tracksVisto || f->posTracks < 0) return MKVASS_NOGO_FAIXA;
    p = rangeInsistir(f, f->posTracks, 64L * 1024, &n);
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

// Anda o pedido das fontes. `esperar` bloqueia ate o fim (antes do sidecar
// completo, que exige as fontes). Devolve 1 quando as fontes acabaram de
// entrar.
static int avancarFontes(Fio *f, int esperar) {
  long n = 0; unsigned char *p;
  if (f->fontesPasso != 1 && f->fontesPasso != 2) return 0;
  if (!esperar && !jobPronto(f->jobFontes)) return 0;
  { long ji = f->jobFontes->ini, jn = f->jobFontes->n;
    p = colherJob(f, f->jobFontes, &n);
    f->jobFontes = NULL;
    // Falha de rede no pedido das fontes (o 429 de um CDN que aceita uma
    // conexao, que este pedido em paralelo com o Cues provoca): pede de novo,
    // ate tres vezes. Antes a legenda seguia sem as fontes do fansub.
    if (!p && !f->definitivo && f->fontesRepetidas < 3) {
      f->fontesRepetidas++;
      f->jobFontes = submeter(f, ji, jn);
      if (f->jobFontes) {
        printf("[mkvass] fontes anexadas: pedido de novo (%d/3)\n", f->fontesRepetidas);
        fflush(stdout);
        if (esperar) return avancarFontes(f, 1);
        return 0;
      }
    } }
  if (f->fontesPasso == 1) {
    int ai, at = 0; long an;
    if (!p || n < 2) { free(p); f->fontesPasso = 3; goto falhou; }
    ai = larguraDe(p[0]);
    an = ai > 0 && ai < n ? lerTam(p + ai, n - ai, &at) : -1;
    if (an <= 0 || an > MKVASS_CORPO_MAX) { free(p); f->fontesPasso = 3; goto falhou; }
    if (ai + at + an <= n) {
      int ok = lerAttachments(f, p + ai + at, an);
      free(p); f->fontesPasso = 3;
      if (!ok) goto falhou;
    } else {
      free(p);
      f->jobFontes = submeter(f, f->posAttachments + ai + at, an);
      f->fontesCabN = an;
      f->fontesPasso = f->jobFontes ? 2 : 3;
      if (!f->jobFontes) goto falhou;
      if (esperar) return avancarFontes(f, 1);
      return 0;
    }
  } else {
    int ok = p && n >= f->fontesCabN && lerAttachments(f, p, f->fontesCabN);
    free(p); f->fontesPasso = 3;
    if (!ok) goto falhou;
  }
  f->fontesCompletas = 1;
  // A proxima entrega e "primeira" de novo: limpa, passa as fontes, recarrega.
  // Tambem numa retomada (herdar): o fio anterior pode ter caido antes das
  // fontes. Orfao (entregas < 0) continua orfao.
  if (f->entregas >= 0) f->entregas = 0;
  // Retomada so recarrega se as fontes ainda nao estao no libass desta geracao.
  { int temFontes;
    pthread_mutex_lock(&S.trava); temFontes = S.fontesLegG && S.fontesLegG == f->legG; pthread_mutex_unlock(&S.trava);
    if (f->nFontes && !temFontes) f->herdar = 0; }
  f->sujo = 1;
  printf("[mkvass] fontes anexadas: %d, %ld ms desde a escolha\n", f->nFontes, agoraMs() - f->t0);
  fflush(stdout);
  return 1;
falhou:
  printf("[mkvass] fontes anexadas: falha ao ler (a legenda segue com as da TV)\n");
  fflush(stdout);
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

// --- varredura: quando o indice nao aponta os blocos da faixa ------------------

// Um Cluster candidato a varredura: posicao absoluta e o CueTime do indice.
typedef struct { long cluster; unsigned long tempo; } ClVarre;

static int cmpClVarre(const void *a, const void *b) {
  const ClVarre *x = a, *y = b;
  return x->cluster < y->cluster ? -1 : x->cluster > y->cluster;
}

static int anexarClVarre(ClVarre **v, int *n, int *cap, long cluster, unsigned long tempo) {
  if (*n >= MKVASS_MAX_PONTOS) return 1;
  if (*n >= *cap) {
    int nc = *cap ? *cap * 2 : 64;
    ClVarre *nv = realloc(*v, (size_t)nc * sizeof *nv);
    if (!nv) return 0;
    *v = nv; *cap = nc;
  }
  (*v)[*n].cluster = cluster; (*v)[*n].tempo = tempo; (*n)++;
  return 1;
}

// Anda pelos elementos de nivel 1 a partir do Segment ate o primeiro Cluster:
// um Range de 16 bytes por elemento (SeekHead, Info, Tracks, Attachments,
// Tags — meia duzia de idas). So e chamado quando nao ha Cues para dizer onde
// os Clusters estao e o primeiro nao coube na janela do cabecalho (fontes
// anexadas de varios MB antes do video, o caso dos fansubs).
static int acharPrimeiroCluster(Fio *f) {
  long o = f->segIni; int passos = 0;
  while (passos++ < 32 && (f->segFim < 0 || o < f->segFim)) {
    long n = 0, tam; int ui = 0, ut = 0; unsigned long id;
    unsigned char *p = rangeInsistir(f, o, 16, &n);
    if (!p) return 0;
    id = lerId(p, n, &ui);
    tam = id ? lerTam(p + ui, n - ui, &ut) : -1;
    free(p);
    if (!id || tam == -1) return 0;
    if (id == ID_CLUSTER) { f->primeiroCluster = o; return 1; }
    if (tam == -2) return 0;
    o += ui + ut + tam;
  }
  return 0;
}

// Troca o indice por blocos pela lista de Clusters a varrer (ordenada, sem
// repeticao). Ver `varreEncadeia` na struct.
static int armarVarredura(Fio *f, ClVarre *v, int n, int encadeia) {
  int i, k = 0;
  qsort(v, (size_t)n, sizeof *v, cmpClVarre);
  free(f->pontos);
  f->pontos = calloc((size_t)n, sizeof *f->pontos);
  if (!f->pontos) { f->nPontos = 0; return 0; }
  for (i = 0; i < n; i++) {
    if (k && f->pontos[k - 1].cluster == v[i].cluster) continue;
    f->pontos[k].cluster = v[i].cluster; f->pontos[k].rel = -1;
    f->pontos[k].tempo = v[i].tempo; f->pontos[k].colhido = 0;
    f->pontos[k].cursor = 0; f->pontos[k].cursorTs = -1.0; k++;
  }
  f->nPontos = k;
  f->varredura = 1; f->varreEncadeia = encadeia;
  return k > 0;
}

// Sem Cues nenhum: um trecho so, do primeiro Cluster ao fim do Segment —
// lido pelo cursor, dentro da janela, com salto por bitrate no seek.
static int armarVarreduraInteira(Fio *f) {
  ClVarre v;
  if (f->primeiroCluster < 0 && !acharPrimeiroCluster(f)) return 0;
  v.cluster = f->primeiroCluster; v.tempo = 0;
  if (!armarVarredura(f, &v, 1, 1)) return 0;
  f->varreInteira = 1; f->varreTs = -1.0;
  return 1;
}

static int lerCues(Fio *f) {
  long n = 0, tam; int ui = 0, ut = 0; unsigned char *p;
  Iter it; unsigned long id; const unsigned char *d; long t;
  int semRel = 0, cap = 256, r = 0;
  // Clusters citados pelo indice: os da PROPRIA faixa (CuePoint sem
  // CueRelativePosition) e os das OUTRAS (o video). Servem a varredura quando
  // o indice por bloco nao existe.
  ClVarre *daFaixa = NULL, *doVideo = NULL;
  int nFaixa = 0, capFaixa = 0, nVideo = 0, capVideo = 0;
  // Sem Cues no SeekHead: nao ha indice nenhum. Varre do primeiro Cluster.
  if (f->posCues < 0) return armarVarreduraInteira(f) ? 0 : MKVASS_NOGO_SEM_INDICE;
  // UM Range com folga (MKVASS_CUES_1) em vez de "16 bytes para ler o tamanho
  // e depois o corpo": na C9 cada ida custa ~1,5 s. O Cues de um episodio de
  // 24 min cabe folgado; maior que isso, o resto vem num segundo pedido.
  p = rangeInsistir(f, f->posCues, MKVASS_CUES_1, &n);
  if (!p) return MKVASS_NOGO_REDE;
  id = lerId(p, n, &ui);
  if (id != ID_CUES) {
    // O inicio do arquivo onde pedimos o fim: o servidor ignorou o Range.
    // Outra coisa ali: o SeekHead mente sobre o Cues — vale a varredura.
    if (n >= 4 && lerId(p, n, &ui) == ID_EBML) { free(p); return MKVASS_NOGO_SEM_RANGE; }
    free(p);
    return armarVarreduraInteira(f) ? 0 : MKVASS_NOGO_SEM_INDICE;
  }
  tam = lerTam(p + ui, n - ui, &ut);
  if (tam <= 0 || tam > MKVASS_CUES_MAX) {
    free(p);
    return armarVarreduraInteira(f) ? 0 : MKVASS_NOGO_SEM_INDICE;
  }
  if (ui + ut + tam <= n) {
    memmove(p, p + ui + ut, (size_t)tam);
  } else {
    long falta = ui + ut + tam - n, m = 0;
    unsigned char *q = realloc(p, (size_t)(ui + ut + tam)), *resto;
    if (!q) { free(p); return MKVASS_NOGO_REDE; }
    p = q;
    resto = rangeInsistir(f, f->posCues + n, falta, &m);
    if (!resto || m < falta) { free(resto); free(p); return MKVASS_NOGO_REDE; }
    memcpy(p + n, resto, (size_t)falta); free(resto);
    memmove(p, p + ui + ut, (size_t)tam);
  }
  n = tam;
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
        if (cl < 0) continue;
        if (trk != f->faixa) {
          if (!anexarClVarre(&doVideo, &nVideo, &capVideo, f->segIni + cl, tempo)) r = MKVASS_NOGO_REDE;
          continue;
        }
        if (!anexarClVarre(&daFaixa, &nFaixa, &capFaixa, f->segIni + cl, tempo)) r = MKVASS_NOGO_REDE;
        // Sem CueRelativePosition o ponto fica com rel = -1: o Cluster e
        // conhecido e sera lido inteiro (colherCluster). Antes isto mandava a
        // faixa TODA para a varredura.
        if (rel < 0) semRel++;
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
  if (r) { free(daFaixa); free(doVideo); return r; }
  if (f->nPontos) {
    free(daFaixa); free(doVideo);
    qsort(f->pontos, (size_t)f->nPontos, sizeof *f->pontos, cmpPonto);
    f->semRel = semRel;
    if (semRel)
      printf("[mkvass] faixa %d: %d de %d CuePoints sem CueRelativePosition — "
             "o Cluster deles e lido inteiro, na janela do playhead\n", f->faixa, semRel, f->nPontos);
    return 0;
  }
  // O indice nao tem CuePoint da faixa. Antes isto era no-go e a faixa
  // voltava ao renderizador da TV. Agora VARRE: pela lista da propria faixa
  // (so sobra quando o teto de pontos cortou tudo), senao pela do video (pode
  // faltar Cluster entre dois pontos, por isso encadeia), senao o arquivo
  // inteiro.
  if (nFaixa)      r = armarVarredura(f, daFaixa, nFaixa, 0) ? 0 : MKVASS_NOGO_SEM_REL;
  else if (nVideo) r = armarVarredura(f, doVideo, nVideo, 1) ? 0 : MKVASS_NOGO_SEM_INDICE;
  else             r = armarVarreduraInteira(f) ? 0 : MKVASS_NOGO_SEM_INDICE;
  free(daFaixa); free(doVideo);
  // "Sem indice" por um Range que falhou no caminho (acharPrimeiroCluster) e
  // falha de REDE, passageira — nao o arquivo (ver mkvass_recuo_ms).
  if (r == MKVASS_NOGO_SEM_INDICE && f->falhas) r = MKVASS_NOGO_REDE;
  return r;
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

static int blocoDaFaixa(const Fio *f, const unsigned char *p, long n);

// Interpreta os pontos [i..j] a partir de um buffer que comeca no byte
// `bufIni` do arquivo. Mesmo contrato de colherGrupo.
static int colherDoBuffer(Fio *f, int i, int j, unsigned char *p, long n, long bufIni,
                          const ClCache *cl) {
  int k;
  for (k = i; k <= j; k++) {
    long o = f->pontos[k].cluster + cl->hdr + f->pontos[k].rel - bufIni;
    // So um bloco DESTA faixa vale: lerBloco tambem "aceita" um bloco de video
    // (devolve o tamanho para andar), e o ponto sairia colhido sem fala.
    long r = (o >= 0 && o < n && blocoDaFaixa(f, p + o, n - o))
           ? lerBloco(f, p + o, n - o, cl, f->pontos[k].tempo) : 0;
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
    if (r == 0) {
      // A posicao do indice nao cai num bloco da faixa. Antes o ponto era
      // dado como desistido e a fala sumia; agora volta a fila para o Cluster
      // ser lido inteiro (colherCluster).
      f->pontos[k].rel = -1; f->pontos[k].colhido = 0;
      if (++f->relRuins <= 3)
        printf("[mkvass] CueRelativePosition nao aponta um bloco da faixa (%.3f s): "
               "o Cluster sera lido inteiro\n", segundosDe(f, f->pontos[k].tempo));
      continue;
    }
    f->pontos[k].colhido = 1;
    f->nColhidos++;
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

static const ClCache *clusterVisto(const Fio *f, long pos) {
  int k;
  for (k = 0; k < CL_CACHE; k++) if (f->cl[k].hdr && f->cl[k].pos == pos) return &f->cl[k];
  return NULL;
}

static int temPre(const Fio *f, long ini, long n) {
  int k;
  for (k = 0; k < MKVASS_PREBUSCA; k++)
    if (f->pre[k] && f->pre[k]->ini == ini && f->pre[k]->n == n) return 1;
  return 0;
}

// Da varredura (definidas adiante): a primeira janela de um trecho.
static long varreTam(const Fio *f, long o, long fim, long precisa);
static long varreFim(const Fio *f, int i);

// O Range que colherGrupo vai pedir para [i..j]: pelo cabecalho conhecido,
// ou o do palpite.
static void rangeDoGrupo(const Fio *f, int i, int j, long *ini, long *n) {
  const ClCache *cl = clusterVisto(f, f->pontos[i].cluster);
  long cl0 = f->pontos[i].cluster;
  // Varredura: a primeira janela do trecho (o mesmo calculo de garantir).
  if (f->varredura || f->pontos[i].rel < 0) { *ini = cl0; *n = varreTam(f, cl0, varreFim(f, i), 16); return; }
  // Palpite ja no ar para este grupo: e ele que colherGrupo vai usar.
  if (cl && temPre(f, cl0 + 5 + f->pontos[i].rel,
                   7 + f->pontos[j].rel - f->pontos[i].rel + MKVASS_BLOCO)) cl = NULL;
  if (cl) { *ini = cl0 + cl->hdr + f->pontos[i].rel;
            *n = f->pontos[j].rel - f->pontos[i].rel + MKVASS_BLOCO; }
  else    { *ini = cl0 + 5 + f->pontos[i].rel;
            *n = 7 + f->pontos[j].rel - f->pontos[i].rel + MKVASS_BLOCO; }
}

// --- varredura: ler os Clusters quando o indice nao aponta os blocos ---------

static void entregar(Fio *f);

// Janela de bytes da varredura: [ini, ini+n) do arquivo, num buffer so.
// `eof`: o servidor devolveu menos do que o pedido sem fim conhecido — nao ha
// mais arquivo depois da janela.

// Tamanho do proximo pedido da varredura a partir de `o`, sem passar de `fim`
// (0 = desconhecido). O MESMO calculo na pre-busca (rangeDoGrupo), senao o
// Range pedido de antemao nao casa com o que a varredura pede.
static long varreTam(const Fio *f, long o, long fim, long precisa) {
  long ch = f->varreCh > 0 ? f->varreCh : MKVASS_VARRE_CH;
  long len = precisa > ch ? precisa : ch;
  if (fim > 0 && o + len > fim) len = fim - o;
  return len;
}

// Fim do trecho do ponto i: o Cluster do ponto seguinte, ou o fim do Segment
// (0 = desconhecido: le ate o servidor devolver menos do que o pedido).
static long varreFim(const Fio *f, int i) {
  // Pelo indice da faixa (colherCluster) o proximo ponto pode ser do MESMO
  // Cluster: o trecho vai ate o fim do Segment e para no fim deste Cluster
  // (varreEncadeia = 0).
  if (f->varredura && i + 1 < f->nPontos) return f->pontos[i + 1].cluster;
  return f->segFim > 0 ? f->segFim : 0;
}

// Garante `precisa` bytes a partir de `o` na janela, pedindo outro Range
// quando e o caso. *out aponta para o byte `o`; *disp diz quantos ha dali
// ate o fim da janela. Devolve 1 ok; 2 acabou o trecho/arquivo; 0 a rede
// falhou; -2 resposta curta dentro de um fim conhecido (truncada).
static int garantir(Fio *f, Janela *w, long o, long precisa, long fim,
                    const unsigned char **out, long *disp) {
  long n = 0, len; unsigned char *p;
  if (fim > 0 && o + precisa > fim) precisa = fim - o;
  if (precisa <= 0) return 2;
  if (w->p && o >= w->ini && o + precisa <= w->ini + w->n) {
    *out = w->p + (o - w->ini); *disp = w->ini + w->n - o; return 1;
  }
  if (w->p && w->eof && o >= w->ini) {
    if (o >= w->ini + w->n) return 2;
    *out = w->p + (o - w->ini); *disp = w->ini + w->n - o; return 1;
  }
  len = varreTam(f, o, fim, precisa);
  if (len <= 0) return 2;
  p = range(f, o, len, &n);
  if (!p) return 0;
  free(w->p); w->p = p; w->ini = o; w->n = n; w->eof = n < len;
  if (n < precisa) return fim > 0 ? -2 : 2;
  *out = p; *disp = n;
  return 1;
}

// --- cobertura (modo sem Cues nenhum) ------------------------------------------

static const Cob *cobQueContem(const Fio *f, long b) {
  int k;
  for (k = 0; k < f->nCob; k++) if (b >= f->cob[k].b0 && b < f->cob[k].b1) return &f->cob[k];
  return NULL;
}

static const Cob *cobNoTempo(const Fio *f, double t) {
  int k;
  for (k = 0; k < f->nCob; k++) if (t >= f->cob[k].t0 && t <= f->cob[k].t1) return &f->cob[k];
  return NULL;
}

// Registra [b0,b1) como lido, fundindo com vizinhos que encostam.
static void cobAdicionar(Fio *f, long b0, long b1, double t0, double t1) {
  int k;
  if (b1 <= b0) return;
  for (k = 0; k < f->nCob; k++) {
    Cob *c = &f->cob[k];
    if (b0 <= c->b1 && b1 >= c->b0) {
      if (b0 < c->b0) { c->b0 = b0; c->t0 = t0; }
      if (b1 > c->b1) { c->b1 = b1; c->t1 = t1; }
      // Pode ter encostado no seguinte: funde uma vez.
      { int m; for (m = 0; m < f->nCob; m++) {
          Cob *d = &f->cob[m];
          if (d == c || d->b0 > c->b1 || d->b1 < c->b0) continue;
          if (d->b0 < c->b0) { c->b0 = d->b0; c->t0 = d->t0; }
          if (d->b1 > c->b1) { c->b1 = d->b1; c->t1 = d->t1; }
          f->cob[m] = f->cob[--f->nCob]; break; } }
      return;
    }
  }
  if (f->nCob < MKVASS_COB) { Cob c = { b0, b1, t0, t1 }; f->cob[f->nCob++] = c; }
}

// 1 quando um so trecho lido vai do primeiro Cluster ao fim do Segment.
static int cobCompleta(const Fio *f) {
  const Cob *c = cobQueContem(f, f->primeiroCluster);
  return c && f->segFim > 0 && c->b1 >= f->segFim;
}

// Acha o proximo Cluster a partir de `byte` procurando o id 1F 43 B6 75 e
// conferindo que o que segue e um tamanho valido e um Timestamp (E7). E a
// ressincronizacao classica do Matroska. Devolve a posicao e o tempo do
// Cluster em *ts; -1 se nao achou em ate 4 MB.
static long ressincronizar(Fio *f, long byte, double *ts) {
  int tent;
  for (tent = 0; tent < 8; tent++) {
    long n = 0, k; unsigned char *p;
    long len = f->varreCh > 0 ? f->varreCh : MKVASS_VARRE_CH;
    if (f->segFim > 0 && byte + len > f->segFim) len = f->segFim - byte;
    if (len < 16) return -1;
    p = range(f, byte, len, &n);
    if (!p) return -1;
    for (k = 0; k + 16 <= n; k++) {
      int ut = 0, ti = 0, tt = 0; long tam, tn; unsigned long v;
      if (p[k] != 0x1F || p[k + 1] != 0x43 || p[k + 2] != 0xB6 || p[k + 3] != 0x75) continue;
      tam = lerTam(p + k + 4, n - k - 4, &ut);
      if (tam == -1 || (tam > 0 && f->segFim > 0 && byte + k + 4 + ut + tam > f->segFim + 1)) continue;
      if (lerId(p + k + 4 + ut, n - k - 4 - ut, &ti) != ID_TIMESTAMP) continue;
      tn = lerTam(p + k + 4 + ut + ti, n - k - 4 - ut - ti, &tt);
      if (tn < 1 || tn > 8 || k + 4 + ut + ti + tt + tn > n) continue;
      v = lerUint(p + k + 4 + ut + ti + tt, tn);
      *ts = segundosDe(f, v);
      free(p);
      return byte + k;
    }
    free(p);
    // Sem Cluster na janela: segue (a janela e menor que um Cluster de 4K).
    byte += n > 8 ? n - 8 : n;
    if (n < len) return -1;
  }
  return -1;
}

// Poe o cursor do modo sem Cues onde a janela [pos - atras, pos + janela]
// pede. Fica como esta se o cursor ja serve; usa o que ja foi lido quando o
// playhead voltou para dentro de um trecho coberto; senao SALTA: estima o
// byte por bitrate (Duration do Info) e ressincroniza no proximo Cluster.
// Sem Duration nao ha salto: a varredura segue sequencial (limitada pela
// janela, entao um seek longo espera o playhead... e a rede).
static void posicionarVarredura(Fio *f, double pos) {
  Ponto *p = &f->pontos[0];
  double alvo = pos - MKVASS_VARRE_ATRAS_SEG / 3.0, ts = -1.0;
  const Cob *c;
  long byte, b; int tent;
  if (f->varreTs >= 0.0 && f->varreTs >= pos - MKVASS_VARRE_ATRAS_SEG &&
      f->varreTs <= pos + MKVASS_VARRE_JANELA_SEG) return;
  // Ja lemos o tempo do playhead: continua do fim daquele trecho.
  c = cobNoTempo(f, pos);
  if (c) { p->cursor = c->b1; p->cursorTs = -1.0; f->varreTs = c->t1; return; }
  if (alvo <= 0.0) { p->cursor = f->primeiroCluster; p->cursorTs = -1.0; f->varreTs = 0.0; return; }
  if (f->varreDur <= 1.0 || f->segFim <= f->primeiroCluster) return;
  byte = f->primeiroCluster + (long)((double)(f->segFim - f->primeiroCluster) * (alvo / f->varreDur));
  for (tent = 0; tent < 4; tent++) {
    if (byte < f->primeiroCluster) byte = f->primeiroCluster;
    if (byte >= f->segFim) byte = f->segFim - MKVASS_VARRE_CH;
    b = ressincronizar(f, byte, &ts);
    if (b < 0) return;
    // Aceita ate 60 s antes do alvo (custa varrer isso) e nunca depois dele.
    if (ts <= alvo + 2.0 && ts >= alvo - 60.0) break;
    byte = b + (long)((alvo - ts) * (double)(f->segFim - f->primeiroCluster) / f->varreDur);
    if (ts > alvo) byte -= MKVASS_VARRE_CH;   // errou para a frente: recua um pouco mais
  }
  if (b < 0) return;
  c = cobQueContem(f, b);
  if (c) { p->cursor = c->b1; f->varreTs = c->t1; }
  else   { p->cursor = b; f->varreTs = ts; }
  p->cursorTs = -1.0;
  f->varreSaltou = 1;
  printf("[mkvass] varredura saltou para %.0f s (byte %ld, alvo %.0f s, playhead %.0f s)\n",
         f->varreTs, p->cursor, alvo, pos);
  fflush(stdout);
}

// Varre o trecho do ponto i: do seu Cluster ate o Cluster do ponto seguinte
// (ou so este Cluster, quando a lista veio da propria faixa). Le em janelas
// de MKVASS_VARRE_CH e PULA os blocos de outras faixas que passam da janela:
// o proximo pedido comeca no fim deles, sem baixar o payload de video. Cada
// bloco da faixa vai para o corpo pelo mesmo lerBloco do caminho indexado.
// PARA quando um Cluster comeca depois de `tLim` (fim da janela de midia),
// guardando o cursor para retomar dali. Devolve 1 quando terminou o trecho,
// 3 quando parou na janela (ou a troca de faixa interrompeu), 0 se a rede
// falhou, -2 numa resposta curta.
// Pelo indice: o bloco no offset `rel` dos dados do Cluster `pos` ja veio pelo
// CueRelativePosition de outro ponto. Ler o Cluster inteiro nao o repete.
static int relJaColhido(const Fio *f, long pos, long rel) {
  int k;
  if (f->varredura) return 0;
  for (k = 0; k < f->nPontos; k++)
    if (f->pontos[k].cluster == pos && f->pontos[k].rel == rel && f->pontos[k].colhido == 1) return 1;
  return 0;
}

static int varrerTrecho(Fio *f, int i, double tLim) {
  // A janela VIVE no Fio: o trecho seguinte costuma comecar dentro dela
  // (cauda do Range anterior), e uma janela local por trecho rebaixava isso.
  Janela *wp = &f->jan;
  long ini = f->pontos[i].cursor > 0 ? f->pontos[i].cursor : f->pontos[i].cluster;
  long o = ini, fim = varreFim(f, i), t0 = agoraMs();
  unsigned long cueTempo = f->pontos[i].tempo;
  double tsIni = -1.0, tsFim = -1.0;
  int r = 3, clusters = 0, blocos = 0, terminou = 0;
  // O Cluster do cursor ja e conhecido e continua fora da janela: nada a
  // fazer, e sem rede — cada poll ocioso custava uma janela de 512 KB.
  if (f->pontos[i].cursor > 0 && f->pontos[i].cursorTs >= 0.0 && tLim > 0.0 &&
      f->pontos[i].cursorTs > tLim) return 3;
  while (fim <= 0 || o < fim) {
    const unsigned char *p; long disp, tam, dados, clFim; int ui = 0, ut = 0, g;
    unsigned long id; ClCache cl;
    if (!minhaVez(f)) goto sair;
    // Modo sem Cues: o que ja foi lido (antes de um salto) e pulado.
    if (f->varreInteira && f->nCob) {
      const Cob *c = cobQueContem(f, o);
      if (c) {
        if (tsIni >= 0.0) cobAdicionar(f, ini, o, tsIni, tsFim);
        ini = o = c->b1; tsIni = tsFim = c->t1; f->varreTs = c->t1;
        if (c->t1 > tLim) goto sair;
        continue;
      }
    }
    g = garantir(f, wp, o, 16, fim, &p, &disp);
    if (g == 2) break;
    if (g != 1) { r = g; goto sair; }
    id = lerId(p, disp, &ui);
    if (!id) break;
    tam = lerTam(p + ui, disp - ui, &ut);
    if (tam == -1) break;
    if (id != ID_CLUSTER) {
      // Pelo indice (colherCluster) o CueClusterPosition TEM de ser um
      // Cluster; outra coisa ali e indice errado, e andar dali ate achar um
      // leria o arquivo.
      if (!f->varredura) { f->trechoSemCluster = 1; break; }
      if (tam == -2) break;             // tamanho desconhecido fora de Cluster: sem como pular
      o += ui + ut + tam; continue;
    }
    dados = o + ui + ut;
    clFim = tam == -2 ? fim : dados + tam;
    if (fim > 0 && (clFim <= 0 || clFim > fim)) clFim = fim;
    cl.pos = o; cl.hdr = ui + ut; cl.ts = 0; cl.temTs = 0;
    // O Timestamp e o primeiro filho: decide AQUI se o Cluster ainda esta na
    // janela. Fora dela, o cursor fica no comeco do Cluster e o fio dorme.
    { long ctam2, cab2; int cui2 = 0, cut2 = 0;
      g = garantir(f, wp, dados, 16, clFim > 0 ? clFim : fim, &p, &disp);
      if (g == 1 && lerId(p, disp, &cui2) == ID_TIMESTAMP) {
        ctam2 = lerTam(p + cui2, disp - cui2, &cut2); cab2 = cui2 + cut2;
        if (ctam2 > 0 && ctam2 <= 8 && garantir(f, wp, dados, cab2 + ctam2, clFim > 0 ? clFim : fim, &p, &disp) == 1) {
          double ts = segundosDe(f, lerUint(p + cab2, ctam2));
          if (tLim > 0.0 && ts > tLim) {
            f->pontos[i].cursor = cl.pos; f->pontos[i].cursorTs = ts;
            goto sair;
          }
          f->varreTs = ts;
          if (tsIni < 0.0) tsIni = ts;
          tsFim = ts;
        }
      } else if (g != 1 && g != 2) { r = g; goto sair; } }
    clusters++;
    o = dados;
    while (clFim <= 0 || o < clFim) {
      long ctam, cfim, cab; int cui = 0, cut = 0; unsigned long cid;
      long lim = clFim > 0 ? clFim : fim;
      if (!minhaVez(f)) goto sair;
      g = garantir(f, wp, o, 16, lim, &p, &disp);
      if (g == 2) { o = clFim > 0 ? clFim : o; if (clFim <= 0) fim = o; break; }
      if (g != 1) { r = g; goto sair; }
      cid = lerId(p, disp, &cui);
      if (!cid) { o = clFim > 0 ? clFim : o; if (clFim <= 0) fim = o; break; }
      ctam = lerTam(p + cui, disp - cui, &cut);
      if (ctam < 0) { o = clFim > 0 ? clFim : o; if (clFim <= 0) fim = o; break; }
      cab = cui + cut; cfim = o + cab + ctam;
      if (cid == ID_TIMESTAMP && ctam > 0 && ctam <= 8) {
        g = garantir(f, wp, o, cab + ctam, lim, &p, &disp);
        if (g == 1) { cl.ts = lerUint(p + cab, ctam); cl.temTs = 1; }
        else if (g != 2) { r = g; goto sair; }
      } else if ((cid == ID_SIMPLEBLOCK || cid == ID_BLOCKGROUP) && ctam > 0) {
        // So o cabecalho do bloco decide se e da faixa; o payload de video
        // fica onde esta.
        long peek = cab + ctam; if (peek > 40) peek = 40;
        g = garantir(f, wp, o, peek, lim, &p, &disp);
        if (g == 1 && ctam <= MKVASS_VARRE_BLOCO && blocoDaFaixa(f, p, disp) &&
            !relJaColhido(f, cl.pos, o - dados)) {
          g = garantir(f, wp, o, cab + ctam, lim, &p, &disp);
          if (g == 1) {
            if (lerBloco(f, p, cab + ctam, &cl, cueTempo) > 0) blocos++;
            // A primeira fala vai ja; as seguintes no ritmo de MKVASS_ENTREGA_MS.
            if (f->sujo && (!f->primeiraFala || agoraMs() - f->ultEntrega >= MKVASS_ENTREGA_MS))
              entregar(f);
          } else if (g != 2) { r = g; goto sair; }
        } else if (g != 1 && g != 2) { r = g; goto sair; }
      }
      o = cfim;
    }
    // Cluster inteiro lido: o cursor avanca (o ponto retoma daqui se parar).
    f->pontos[i].cursor = o; f->pontos[i].cursorTs = -1.0;
    if (!f->varreEncadeia) break;
  }
  // Fim do trecho. No modo sem Cues so conta como terminado se a cobertura
  // for continua do primeiro Cluster ao fim do Segment (sem buracos de salto).
  if (f->varreInteira) {
    if (tsIni >= 0.0) cobAdicionar(f, ini, o, tsIni, tsFim);
    tsIni = -1.0;
    f->pontos[i].cursor = fim > 0 ? fim : o;
    terminou = cobCompleta(f);
  } else terminou = 1;
  r = terminou ? 1 : 3;
sair:
    if (f->varreInteira && tsIni >= 0.0) cobAdicionar(f, ini, o, tsIni, tsFim);
  if (terminou) {
    f->pontos[i].colhido = 1; f->nColhidos++;
    if (f->nColhidos <= 3 || blocos)
      printf("[mkvass] trecho %d/%d varrido: %d Cluster(s), %d bloco(s) da faixa, %ld ms\n",
             i + 1, f->nPontos, clusters, blocos, agoraMs() - t0);
  }
  return r;
}

// Proximo trecho da varredura DENTRO da janela: o Cluster que contem o
// playhead entra sempre; depois os que comecam ate `janela` s a frente; atras,
// so ate MKVASS_VARRE_ATRAS_SEG. Fora disso: -1, e o fio dorme. E isto que
// impede a varredura de baixar o arquivo inteiro.
static int proximoVarre(const Fio *f, double pos) {
  int i, ult = -1;
  for (i = 0; i < f->nPontos; i++) {
    if (segundosDe(f, f->pontos[i].tempo) <= pos) ult = i; else break;
  }
  for (i = 0; i < f->nPontos; i++) {
    double t = segundosDe(f, f->pontos[i].tempo);
    if (f->pontos[i].colhido) continue;
    if (t > pos + MKVASS_VARRE_JANELA_SEG) break;
    if (i != ult && t < pos - MKVASS_VARRE_ATRAS_SEG) continue;
    return i;
  }
  return -1;
}

static void dormir(Fio *f, long ms) {
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && !S.parar) {
    struct timespec rt; clock_gettime(CLOCK_REALTIME, &rt);
    rt.tv_nsec += ms * 1000000L;
    while (rt.tv_nsec >= 1000000000L) { rt.tv_sec++; rt.tv_nsec -= 1000000000L; }
    pthread_cond_timedwait(&S.sinal, &S.trava, &rt);
  }
  pthread_mutex_unlock(&S.trava);
}

// O laco da VARREDURA (separado do laco indexado de proposito: aquele colhe
// a faixa inteira em segundo plano porque cada fala custa 1 KB; este NUNCA
// sai da janela porque cada segundo custa um segundo de video).
//   - so trechos em [pos - atras, pos + janela]; seek -> a janela muda e o
//     proximo trecho e o da nova posicao (no modo sem Cues, salto por bitrate);
//   - PAUSA enquanto o buffer de video a frente < MKVASS_VARRE_FOLGA_SEG
//     (o player informa por mkvass_folga; sem informacao nao pausa);
//   - uma linha por minuto com os bytes varridos.
// Devolve 1 quando fechou COMPLETO; 0 quando saiu (parar, troca, rede).
static int varrerLaco(Fio *f) {
  long ultLog = agoraMs(), bytesMarca, pausas = 0;
  int pausado = 0;
  pthread_mutex_lock(&S.trava); bytesMarca = S.bytes; pthread_mutex_unlock(&S.trava);
  while (minhaVez(f)) {
    double pos, folga; int i, ocioso = 0;
    pthread_mutex_lock(&S.trava);
    pos = S.pos; folga = S.folga; S.nColhidos = f->nColhidos;
    pthread_mutex_unlock(&S.trava);
    avancarFontes(f, 0);
    if (folga >= 0.0 && folga < MKVASS_VARRE_FOLGA_SEG) {
      if (!pausado) {
        pausado = 1; pausas++;
        printf("[mkvass] varredura PAUSADA: buffer de video %.0f s a frente (< %.0f s)\n",
               folga, MKVASS_VARRE_FOLGA_SEG);
        fflush(stdout);
      }
      ocioso = 1;
    } else {
      if (pausado) {
        pausado = 0;
        printf("[mkvass] varredura retomada: buffer de video %.0f s a frente\n", folga);
        fflush(stdout);
      }
      if (f->varreInteira) posicionarVarredura(f, pos);
      i = proximoVarre(f, pos);
      if (i < 0) ocioso = 1;
      else if (f->pontos[i].cursor > 0 && f->pontos[i].cursorTs >= 0.0 &&
               f->pontos[i].cursorTs > pos + MKVASS_VARRE_JANELA_SEG) ocioso = 1;
      else {
        int r;
        f->posRef = pos;
        r = varrerTrecho(f, i, pos + MKVASS_VARRE_JANELA_SEG);
        if (r == -2) {
          if (definirEstadoSeAtual(f, MKVASS_NOGO_REDE)) {
            printf("[mkvass] Range curto na varredura: desistindo\n"); fflush(stdout);
          }
          return 0;
        }
        if (r == 0) {
          if (f->falhas >= MKVASS_FALHAS_MAX) {
            if (definirEstadoSeAtual(f, f->definitivo ? MKVASS_NOGO_HTTP : MKVASS_NOGO_REDE)) {
              printf("[mkvass] %d Ranges falhados seguidos na varredura: parando esta tentativa "
                     "(HTTP %d, curl %d)\n", f->falhas, f->ultSt, f->ultErro);
              fflush(stdout);
            }
            return 0;
          }
          recuar(f);
        }
        if (r == 3) ocioso = 1;
      }
    }
    if (f->sujo && (!f->primeiraFala || ocioso || agoraMs() - f->ultEntrega >= MKVASS_ENTREGA_MS))
      entregar(f);
    if (agoraMs() - ultLog >= 60000L) {
      long bytes;
      pthread_mutex_lock(&S.trava); bytes = S.bytes; pthread_mutex_unlock(&S.trava);
      printf("[mkvass] varredura: %ld KB no ultimo minuto, cursor %.0f s, playhead %.0f s, "
             "buffer %.0f s, %d/%d trechos, pausas %ld\n",
             (bytes - bytesMarca) / 1024, f->varreTs, pos, folga, f->nColhidos, f->nPontos, pausas);
      fflush(stdout);
      bytesMarca = bytes; ultLog = agoraMs();
    }
    if (f->nColhidos >= f->nPontos) {
      if (avancarFontes(f, 1)) entregar(f);
      entregar(f);
      gravarSidecar(f, 1, 0);
      pthread_mutex_lock(&S.trava);
      if (f->g == S.geracao) S.nColhidos = f->nColhidos;
      pthread_mutex_unlock(&S.trava);
      if (!definirEstadoSeAtual(f, MKVASS_COMPLETO)) return 0;
      printf("[mkvass] varredura completa: %d trecho(s), %ld Ranges, %ld ms desde a escolha (pausas %ld)\n",
             f->nPontos, S.pedidos, agoraMs() - f->t0, pausas);
      fflush(stdout);
      return 1;
    }
    if (ocioso) dormir(f, 250);
  }
  return 0;
}

// Ponto sem posicao util (rel = -1): le o Cluster do CueClusterPosition
// INTEIRO — pelo mesmo leitor da varredura, que pula o payload de video maior
// que a janela sem baixa-lo — e tira de la todo bloco da faixa. Um Cluster so,
// e nao a cadeia de Clusters da varredura. Todo ponto pendente do mesmo
// Cluster sai junto. Devolve como colherGrupo.
static int colherCluster(Fio *f, int i) {
  int k, r;
  f->trechoSemCluster = 0;
  r = varrerTrecho(f, i, 0.0);
  if (r == 3) return 1;                 // troca de faixa no meio: o laco ve
  if (r != 1) return r;
  if (f->trechoSemCluster) {
    // O indice nao apontava um Cluster: insistir nao ajuda.
    f->pontos[i].colhido = 2; f->nColhidos--;
    return 1;
  }
  for (k = 0; k < f->nPontos; k++)
    if (k != i && f->pontos[k].cluster == f->pontos[i].cluster && f->pontos[k].colhido != 1) {
      f->pontos[k].colhido = 1; f->nColhidos++;
    }
  return 1;
}

// Colhe o grupo de pontos [i..j] (mesmo Cluster, proximos) num Range so.
// Marca cada um como colhido (1) ou desistido (2). Devolve 1 se a rede
// respondeu.
static int colherGrupo(Fio *f, int i, int j) {
  const ClCache *cl = NULL;
  long ini, n = 0, fim; unsigned char *p; int k, r;
  // Na varredura quem colhe e varrerLaco; este caminho e o indexado.
  if (f->pontos[i].rel < 0) return colherCluster(f, i);
  cl = clusterVisto(f, f->pontos[i].cluster);
  // A pre-busca pediu o palpite antes de outro grupo ensinar o cabecalho
  // deste Cluster: usa o que ja veio em vez de pedir outro Range.
  if (cl && temPre(f, f->pontos[i].cluster + 5 + f->pontos[i].rel,
                   7 + f->pontos[j].rel - f->pontos[i].rel + MKVASS_BLOCO)) cl = NULL;
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
    if (!f->entregas && !(f->herdar && legenda_ligada_em(f->legG))) {
      // Primeira carga: so se a legenda ainda e da geracao que a escolha
      // deixou. Outro dono no meio (externa, outra faixa, desligada) = este
      // corpo e de uma faixa que saiu.
      if (legenda_geracao() == f->legG) {
        unsigned g;
        assrender_limpar_fontes();
        for (i = 0; i < f->nFontes; i++)
          assrender_adicionar_fonte(f->fontes[i].nome, f->fontes[i].dados,
                                    (size_t)f->fontes[i].tam);
        g = legenda_definir_corpo_se(corpo, f->legG);
        if (g) { f->legG = g; ok = 1; S.fontesLegG = f->nFontes ? g : 0; }
      }
    } else ok = legenda_atualizar_corpo_se(corpo, f->legG);
    if (ok) f->entregas++;
    else if (f->entregas >= 0) {
      printf("[mkvass] lote da faixa %d DESCARTADO: a legenda trocou de dono (geracao %u)\n",
             f->faixa, f->legG);
      fflush(stdout);
      f->entregas = -1000000;     // loga uma vez; nunca volta a ser a primeira
    }
  }
  pthread_mutex_unlock(&S.trava);
  return ok;
}

static int contarEventos(const Fio *f) {
  int n = 0; const char *p = f->corpo;
  while (p && (p = strstr(p, "\nDialogue: ")) != NULL) { n++; p += 11; }
  return n;
}

static int contarDesistidos(const Fio *f);

static void entregar(Fio *f) {
  int n;
  if (!f->sujo || !f->corpo) return;
  // TV desenhando por enquanto: so religa o overlay com fala NOVA (ver
  // mkvass_retomar_segurando). A faixa completa solta sempre.
  if (f->segurar) {
    if (f->nColhidos <= f->colhidosIni && f->nColhidos + contarDesistidos(f) < f->nPontos) return;
    f->segurar = 0;
  }
  // Orfao (a legenda trocou de dono): nem reparseia. A colheita segue para o
  // sidecar, que a proxima escolha desta faixa aproveita.
  if (f->entregas < 0) { f->sujo = 0; return; }
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
  double pos = ini + MKVASS_ATRAS_SEG;
  int pausa = f->folgaJan >= 0.0 && f->folgaJan < MKVASS_VARRE_FOLGA_SEG;
  *noJanela = 0;
  for (i = 0; i < f->nPontos; i++) {
    double t;
    if (f->pontos[i].colhido) continue;
    t = segundosDe(f, f->pontos[i].tempo);
    // Ponto de Cluster inteiro (rel = -1): custa um Cluster de VIDEO em
    // bytes, entao segue as regras da varredura — so na janela de midia e
    // com folga de buffer. O resto da faixa continua em segundo plano.
    if (f->pontos[i].rel < 0 &&
        (pausa || t > pos + MKVASS_VARRE_JANELA_SEG || t < pos - MKVASS_VARRE_ATRAS_SEG)) continue;
    if (t >= ini && t <= fim) { *noJanela = 1; return i; }
    if (t > fim) { if (frente < 0) frente = i; }
    else atras = i;               // o ultimo antes de ini: o mais perto
  }
  return frente >= 0 ? frente : atras;
}

// Fim do grupo que comeca em i: vizinhos do mesmo Cluster a menos de
// MKVASS_JUNTAR. O MESMO calculo no laco e na pre-busca, senao o Range
// pedido de antemao nao casa com o que colherGrupo pede.
static int grupoFim(const Fio *f, int i) {
  int j = i;
  while (j + 1 < f->nPontos && !f->pontos[j + 1].colhido &&
         f->pontos[j + 1].cluster == f->pontos[i].cluster &&
         f->pontos[j + 1].rel >= f->pontos[i].rel &&
         f->pontos[j + 1].rel - f->pontos[i].rel < MKVASS_JUNTAR) j++;
  return j;
}

// Mantem ate MKVASS_PARALELOS Ranges no ar: os proximos grupos pela mesma
// prioridade do laco (reservados com colhido = 3 so durante a escolha).
static void preBuscar(Fio *f, double pos) {
  int ii[MKVASS_PARALELOS], jj[MKVASS_PARALELOS], n, k, m, noAr = 0;
  for (k = 0; k < MKVASS_PREBUSCA; k++) if (f->pre[k]) noAr++;
  for (n = 0; n < MKVASS_PARALELOS; n++) {
    int noJ, i = proximoPendente(f, pos - MKVASS_ATRAS_SEG, pos + MKVASS_JANELA_SEG, &noJ);
    if (i < 0) break;
    ii[n] = i; jj[n] = grupoFim(f, i);
    for (m = ii[n]; m <= jj[n]; m++) f->pontos[m].colhido = 3;
  }
  for (k = 0; k < n; k++) for (m = ii[k]; m <= jj[k]; m++) f->pontos[m].colhido = 0;
  // Pre-busca que nao serve a nenhum dos proximos grupos (o playhead saltou,
  // ou o grupo foi colhido por outro caminho) sai, para nao ocupar lugar.
  for (m = 0; m < MKVASS_PREBUSCA; m++) {
    int serve = 0;
    if (!f->pre[m]) continue;
    for (k = 0; k < n && !serve; k++) {
      long ini, len; rangeDoGrupo(f, ii[k], jj[k], &ini, &len);
      serve = f->pre[m]->ini == ini && f->pre[m]->n == len;
    }
    if (!serve && jobPronto(f->pre[m])) { long t; Job *j = f->pre[m]; f->pre[m] = NULL; free(colherJob(f, j, &t)); noAr--; }
  }
  // `paralelos` cai para 1 no primeiro freio do CDN (ver falhou): sem
  // pre-busca nenhuma, o fio pede um Range por vez e o servidor ve UMA
  // conexao alem da do video. (Com a pre-busca de 1, o cabecalho de Cluster e
  // o palpite que o fio pede por conta propria ainda saiam em paralelo.)
  for (k = 0; k < n && f->paralelos > 1 && noAr < f->paralelos; k++) {
    long ini, len; int slot;
    rangeDoGrupo(f, ii[k], jj[k], &ini, &len);
    if (temPre(f, ini, len)) continue;
    for (slot = 0; slot < MKVASS_PREBUSCA && f->pre[slot]; slot++) {}
    if (slot >= MKVASS_PREBUSCA) break;
    f->pre[slot] = submeter(f, ini, len);
    if (f->pre[slot]) noAr++;
  }
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

  // Tentativa nova da mesma url: o endereco final que a anterior leu.
  pthread_mutex_lock(&S.trava);
  if (f->g == S.geracao && S.urlFinal[0] && strcmp(S.urlFinal, f->url)) {
    snprintf(f->url, sizeof f->url, "%s", S.urlFinal);
    f->finalVisto = 1;
  }
  pthread_mutex_unlock(&S.trava);
  if (f->finalVisto) {
    char a[120], b[120];
    printf("[mkvass] url final reaproveitada: %s -> %s\n",
           rede_url_publica(f->urlOrig, a, sizeof a), rede_url_publica(f->url, b, sizeof b));
    fflush(stdout);
  }

  r = lerCabecalho(f);
  if (!r) r = lerCues(f);
  // Rede que falhou com uma recusa definitiva: e o codigo que vai a folha.
  if (r == MKVASS_NOGO_REDE && f->definitivo) r = MKVASS_NOGO_HTTP;
  if (r) {
    if (!definirEstadoSeAtual(f, r)) { free(sc); goto fim; }
    printf("[mkvass] no-go %d (faixa %d): %s (HTTP %d, curl %d)\n", r, f->faixa,
           r == MKVASS_NOGO_NAO_MKV ? "nao e MKV" :
           r == MKVASS_NOGO_SEM_RANGE ? "servidor sem Range" :
           r == MKVASS_NOGO_FAIXA ? "faixa nao e ASS" :
           r == MKVASS_NOGO_SEM_INDICE ? "sem CuePoint da faixa" :
           r == MKVASS_NOGO_SEM_REL ? "sem CueRelativePosition" :
           r == MKVASS_NOGO_HTTP ? "servidor recusou" : "rede", f->ultSt, f->ultErro);
    fflush(stdout);
    free(sc);
    goto fim;
  }
  pthread_mutex_lock(&S.trava);
  if (f->g != S.geracao || S.parar) { pthread_mutex_unlock(&S.trava); free(sc); goto fim; }
  S.nPontos = f->nPontos; S.varredura = f->varredura;
  pthread_mutex_unlock(&S.trava);
  if (f->varredura) {
    // A linha que separa as hipoteses do #92 no log de quem nao tem indice:
    // o app NAO entregou a faixa a TV, esta varrendo.
    // Sem ';' dentro do formato: a varredura-i18n anda para tras ate o ';'
    // anterior para achar o printf, e um ';' no texto a deixava sem contexto.
    printf("[mkvass] faixa %d: o indice nao aponta os blocos desta faixa — VARREDURA de %d trecho(s) "
           "(lista=%s, baixa os Clusters e nao so as falas, Segment ate %ld)\n",
           f->faixa, f->nPontos, f->varreEncadeia ? "cues-do-video" : "cues-da-faixa", f->segFim);
    fflush(stdout);
  }

  if (sc && !strncmp(sc, MARCA_PARCIAL, strlen(MARCA_PARCIAL)) && restaurarParcial(f, sc))
    printf("[mkvass] sidecar parcial: %d/%d blocos ja colhidos\n", f->nColhidos, f->nPontos);
  f->colhidosIni = f->nColhidos;
  free(sc);
  printf("[mkvass] faixa %d: %d blocos indexados, escala %lu ns, cabecalho %zu bytes "
         "(%ld ms desde a escolha, %ld Ranges, medio %ld ms, max %ld; fontes=%d)\n",
         f->faixa, f->nPontos, f->escala, f->corpoTam, agoraMs() - f->t0, f->redeN,
         f->redeN ? f->redeMs / f->redeN : 0, f->redeMaxMs,
         // fontes: 1 pedidas ao pool (segundo plano), 3 lidas/sem anexo, 0 nada
         f->fontesPasso ? f->fontesPasso : f->fontesCompletas ? 3 : 0);
  fflush(stdout);
  entregar(f);
  if (!definirEstadoSeAtual(f, MKVASS_COLHENDO)) goto fim;

  // VARREDURA: laco proprio, preso a janela (ver varrerLaco).
  if (f->varredura) { if (varrerLaco(f)) goto fim; goto sair; }

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
    pos = S.pos; S.nColhidos = f->nColhidos; f->folgaJan = S.folga;
    pthread_mutex_unlock(&S.trava);
    while (feitos < MKVASS_RANGES_POR_SEG && minhaVez(f)) {
      double t; int j, noJanela;
      ini = pos - MKVASS_ATRAS_SEG; fim = pos + MKVASS_JANELA_SEG;
      i = proximoPendente(f, ini, fim, &noJanela);
      if (i < 0) break;
      if (!noJanela && (!janelaCheia || janelaDe != pos)) {
        janelaCheia = 1; janelaDe = pos;
        printf("[mkvass] janela %.0f-%.0f s colhida: %ld ms desde a escolha, %d/%d blocos, %ld Ranges "
               "(medio %ld ms, max %ld), palpites %d (falhos %d), tarde %d, antes do playhead %d\n",
               ini, fim, agoraMs() - f->t0, f->nColhidos, f->nPontos, S.pedidos,
               f->redeN ? f->redeMs / f->redeN : 0, f->redeMaxMs,
               f->palpites, f->palpitesFalhos, f->atrasados, f->perdidos);
        fflush(stdout);
      }
      t = segundosDe(f, f->pontos[i].tempo);
      // Junta os vizinhos do mesmo Cluster que cabem em MKVASS_JUNTAR.
      j = grupoFim(f, i);
      // Os proximos grupos ja vao para o pool enquanto este e lido.
      preBuscar(f, pos);
      avancarFontes(f, 0);
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
          if (definirEstadoSeAtual(f, f->definitivo ? MKVASS_NOGO_HTTP : MKVASS_NOGO_REDE)) {
            printf("[mkvass] %d Ranges falhados seguidos: parando esta tentativa (HTTP %d, curl %d)\n",
                   f->falhas, f->ultSt, f->ultErro);
            fflush(stdout);
          }
          goto sair;
        }
        recuar(f);
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
      soltarPrebusca(f);
      // O sidecar completo exige as fontes: espera o pedido delas.
      if (avancarFontes(f, 1)) entregar(f);
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
  soltarPrebusca(f);
  free(f->jan.p); f->jan.p = NULL;
  if (f->jobFontes) { long t; free(colherJob(f, f->jobFontes, &t)); f->jobFontes = NULL; }
  pthread_mutex_lock(&S.trava);
  S.vivos--;
  pthread_cond_broadcast(&S.sinal);
  pthread_mutex_unlock(&S.trava);
  free(f->pontos); free(f->corpo); liberarFontes(f); free(f);
  return NULL;
}

// --- API ---------------------------------------------------------------------------

static void iniciarFaixa(const char *url, int numeroFaixa, int herdar, int segurar) {
  Fio *f; pthread_t t;
  if (!url || !*url || numeroFaixa == 0) return;
  f = calloc(1, sizeof *f);
  if (!f) return;
  snprintf(f->url, sizeof f->url, "%s", url);
  snprintf(f->urlOrig, sizeof f->urlOrig, "%s", url);
  f->faixa = numeroFaixa;
  f->legG = legenda_geracao();
  f->herdar = herdar;
  f->segurar = segurar;
  f->t0 = agoraMs();
  assrender_preaquecer();
  pthread_mutex_lock(&S.trava);
  // Tentativa nova da MESMA url (mkvass_retomar): herda o que ela ensinou —
  // url final, uma conexao so, janela menor. Escolha nova: comeca do zero.
  if (!herdar || strcmp(S.url, url)) {
    S.urlFinal[0] = 0; S.paralelos = MKVASS_PARALELOS; S.varreCh = MKVASS_VARRE_CH;
    S.ultHttp = S.ultCurl = 0;
  }
  f->paralelos = S.paralelos > 0 ? S.paralelos : MKVASS_PARALELOS;
  f->varreCh = S.varreCh > 0 ? S.varreCh : MKVASS_VARRE_CH;
  S.geracao++;
  f->g = S.geracao;
  snprintf(S.url, sizeof S.url, "%s", url);
  S.faixa = numeroFaixa;
  S.estado = MKVASS_PREPARANDO;
  S.parar = 0;
  S.pedidos = S.bytes = 0;
  S.nPontos = S.nColhidos = 0; S.varredura = 0; S.folga = -1.0;
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
  iniciarFaixa(url, numeroFaixa, 0, 0);
}

void mkvass_iniciar_ordinal(const char *url, int ordinalFaixa) {
  if (ordinalFaixa < 0 || ordinalFaixa >= 64) return;
  iniciarFaixa(url, -ordinalFaixa - 1, 0, 0);
}

static void retomar(int segurar) {
  char url[sizeof S.url]; int faixa;
  pthread_mutex_lock(&S.trava);
  snprintf(url, sizeof url, "%s", S.url); faixa = S.faixa;
  pthread_mutex_unlock(&S.trava);
  iniciarFaixa(url, faixa, 1, segurar);
}

void mkvass_retomar(void) { retomar(0); }
void mkvass_retomar_segurando(void) { retomar(1); }

// PASSAGEIRA x DEFINITIVA. Rede (timeout, 5xx, freio do CDN, Range falhado
// seguido) e o servidor que devolveu o arquivo inteiro UMA vez podem passar:
// recuo de 2, 5, 15, 30 e depois 60 s, sem limite — a rede da TV que caiu por
// um minuto volta, e a legenda do app volta com ela. O resto e do arquivo
// (nao e MKV, faixa nao e ASS, sem indice) ou do servidor (recusa HTTP
// definitiva, Range recusado de novo) e nao muda tentando.
long mkvass_recuo_ms(int estado, int falhas, int recusasRange) {
  static const long recuo[] = { 2000L, 5000L, 15000L, 30000L, 60000L };
  const int n = (int)(sizeof recuo / sizeof recuo[0]);
  if (falhas < 0) falhas = 0;
  if (falhas >= n) falhas = n - 1;
  if (estado == MKVASS_NOGO_REDE) return recuo[falhas];
  if (estado == MKVASS_NOGO_SEM_RANGE && recusasRange == 0) return recuo[falhas];
  return 0;
}

void mkvass_ultima_falha(int *http, int *curl) {
  pthread_mutex_lock(&S.trava);
  if (http) *http = S.ultHttp;
  if (curl) *curl = S.ultCurl;
  pthread_mutex_unlock(&S.trava);
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

void mkvass_folga(double segundosAFrente) {
  pthread_mutex_lock(&S.trava);
  // Acorda o fio so quando cruza o limiar da pausa, nos dois sentidos.
  { int antes = S.folga >= 0.0 && S.folga < MKVASS_VARRE_FOLGA_SEG;
    int depois = segundosAFrente >= 0.0 && segundosAFrente < MKVASS_VARRE_FOLGA_SEG;
    S.folga = segundosAFrente;
    if (antes != depois) pthread_cond_broadcast(&S.sinal); }
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

int mkvass_varredura(void) {
  int v;
  pthread_mutex_lock(&S.trava); v = S.varredura; pthread_mutex_unlock(&S.trava);
  return v;
}

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
