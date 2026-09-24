#include "streams.h"
#include "idioma.h"
#include "badges.h"
#include <pthread.h>
#include "rede.h"
#include "gfx.h"
#include "text.h"
#include "anim.h"
#include "layout.h"
#include "ajustes.h"   /* ajustes_qualidade: o teto de "Qualidade maxima" */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "addons.h"
#include "marco.h"
#include "debrid.h"
#include "fonteauto.h"
#include "video.h"
#include "botoes.h"
#include "ponteiro.h"

#define FOLHA_W       720.0f
#define FOLHA_LINHA   228.0f
#define FOLHA_TOPO    272.0f
#define FOLHA_AUDIO_W  88.0f
#define FOLHA_AUDIO_N   8
#define FOLHA_AUDIO_BAR 6.0f
#define FOLHA_AUDIO_GAP 5.0f
// Canal 0..255 saturado: as tintas secundarias somam um degrau ao canal, e
// sobre realce escuro a principal ja e 255.
#define C8(v) ((v)>255?255:(v))

static Stream *lista;
static int n = 0;
#define AUTO_EXCL_MAX 32
static int automaticasExcluidas[AUTO_EXCL_MAX];
static int nAutomaticasExcluidas;
static pthread_mutex_t autoExclTrava = PTHREAD_MUTEX_INITIALIZER;
// Trava entre a lista e os fios de verificacao (ver Lote, abaixo): a troca de
// lista e as leituras/escritas dos fios em `lista[]` passam por ela.
static pthread_mutex_t verTrava = PTHREAD_MUTEX_INITIALIZER;
// Sobe a cada lista nova, sob verTrava. A verificacao compara antes de gravar a
// url resolvida: indice de uma lista nao vale na seguinte (ver verificarUma).
static unsigned listaGeracao;
static int atual = -1, recarregar;
static char contexto[320];
// O ALVO DA LISTA — issue #101. Ver a nota longa em streams.h: `alvoPedido` e
// o carimbo do proximo pedido e `alvoLista` o da lista que esta em memoria.
// 64 e o mesmo tamanho que app.c usa para montar "tt1234567:99:99" e para os
// ids de canal do stalker/xtream, que sao os maiores que passam por aqui.
static char alvoPedido[64], alvoLista[64];
void stream_definir_alvo(const char *id) {
  snprintf(alvoPedido, sizeof alvoPedido, "%s", id ? id : "");
}
int stream_lista_do_alvo(const char *id) {
  if (!id || !*id || !alvoLista[0] || n < 1) return 0;
  return !strcmp(alvoLista, id);
}
void stream_definir_atual(int i) { atual = i >= 0 && i < n ? i : -1; }
int stream_atual(void) { return atual; }

// A FONTE QUE A PESSOA JA TINHA ESCOLHIDO neste titulo, quando ela existe
// nesta lista. Quem a encontra e fontepref.c (a regra de igualdade mora la);
// aqui ela e so um indice que entra NA FRENTE da fila de verificacao.
//
// Por que um indice guardado e nao um parametro de stream_primeira_boa: a
// verificacao roda em fio proprio, disparada por app.c com a lista ja pronta,
// e acrescentar parametro obrigaria o fio a carregar o id do titulo — que ele
// nao tem e nao deveria precisar ter.
static int preferida = -1;
void stream_preferir(int i) { preferida = (i >= 0 && i < n) ? i : -1; }
int stream_preferida(void) { return preferida; }
static int automaticaExcluidaSemTrava(int indice) {
  int i;
  for (i = 0; i < nAutomaticasExcluidas; i++)
    if (automaticasExcluidas[i] == indice) return 1;
  return 0;
}
static int automaticaExcluida(int indice) {
  int resultado;
  pthread_mutex_lock(&autoExclTrava);
  resultado = automaticaExcluidaSemTrava(indice);
  pthread_mutex_unlock(&autoExclTrava);
  return resultado;
}
int stream_automatico_excluir(int indice) {
  int resultado = 0;
  pthread_mutex_lock(&autoExclTrava);
  if (indice >= 0 && indice < n && !automaticaExcluidaSemTrava(indice) &&
      nAutomaticasExcluidas < AUTO_EXCL_MAX) {
    automaticasExcluidas[nAutomaticasExcluidas++] = indice;
    resultado = 1;
  }
  pthread_mutex_unlock(&autoExclTrava);
  return resultado;
}
void stream_folha_contexto(const char *s) { snprintf(contexto, sizeof contexto, "%s", s ? s : ""); }
int stream_folha_recarregar(void) { int r = recarregar; recarregar = 0; return r; }

static int aberta = 0, foco = 0, escolha = -1;
static float anim = 0.0f, rolagem = 0.0f;
// Velocidade da mola de 2a ordem da rolagem (anim_mola2): partida macia e
// cauda exponencial, a MESMA curva que a home mede. A de 1a ordem que estava
// aqui partia na velocidade maxima e o primeiro quadro ja saltava 12%.
static float velRol = 0.0f;
// Linha do realce, em unidades de ITEM (2.4 = entre o terceiro e o quarto). O
// realce escorrega entre as linhas em vez de saltar: com o salto seco a folha
// parecia trocar de conteudo a cada tecla, e num D-pad e a continuidade do
// realce que diz "ainda e a mesma lista, voce so andou".


static const char *containerDa(const Stream *s) {
  if (s->mp4 || strstr(s->url, ".mp4") || strstr(s->rotulo, ".mp4")) return "MP4";
  if (strstr(s->url, ".mkv") || strstr(s->arquivo, ".mkv") || strstr(s->descricao, ".mkv")) return "MKV";
  if (strstr(s->url, ".m3u8") || strstr(s->rotulo, "HLS")) return "HLS";
  // A CHAVE PASSA POR i18n AQUI, e nao no chamador. As outras tres devolucoes
  // sao siglas iguais nas duas linguas (MP4, MKV, HLS) e nao tem o que
  // traduzir; so esta e palavra. Ficava crua porque o chamador monta a linha
  // com snprintf e a varredura de i18n nao cobre valor de retorno de funcao —
  // apareceu na foto do album em ingles, com "ARQUIVO" no meio de "Sources",
  // "Reload" e "Automatic pick".
  return i18n("ARQUIVO");
}

static Uint32 recebidaEm;
// Torrents que a ultima lista jogou fora por falta de debrid: com 0 na lista e
// isto > 0, a causa da folha vazia e "falta conta de debrid", nao "os addons
// nao tem" (1.3.12: "145 torrents sem debrid descartados" e folha vazia).
static int descartadosSemDebrid;

Uint32 stream_idade_ms(void) {
  return recebidaEm ? SDL_GetTicks() - recebidaEm : 0xFFFFFFFFu;
}

void stream_definir_lista(const Stream *l, int qtd) {
  int i, k = 0;
  recebidaEm = SDL_GetTicks();
  Stream *nova = l && qtd > 0 ? malloc(sizeof(Stream) * (size_t)qtd) : NULL;
  if (l && qtd > 0 && !nova) return;
  // Torrent sem url so fica se ha debrid para resolve-lo; senao seria uma linha
  // que nunca toca (shouldListStream do web).
  for (i = 0; i < qtd && nova; i++)
    if (l[i].url[0] || debrid_ativo()) nova[k++] = l[i];
  if (nova && qtd - k) printf("[fonte] %d torrents sem debrid descartados\n", qtd - k);
  descartadosSemDebrid = nova ? qtd - k : 0;
  pthread_mutex_lock(&verTrava);
  free(lista); lista = nova; n = nova ? k : 0; atual = -1;
  listaGeracao++;
  pthread_mutex_unlock(&verTrava);
  pthread_mutex_lock(&autoExclTrava);
  nAutomaticasExcluidas = 0;
  pthread_mutex_unlock(&autoExclTrava);
  // LISTA NOVA, INDICE VELHO NAO VALE. A preferida e uma posicao na lista
  // ANTERIOR; mantida, ela apontaria para outra fonte do episodio seguinte —
  // o tipo de defeito que toca a coisa errada sem nenhum erro no log.
  preferida = -1;
  foco = 0;
  // A LISTA HERDA O CARIMBO DO PEDIDO (issue #101). Quem publica e addons.c,
  // que so conhece o alvo DELE — e o dele pode ja estar obsoleto quando a
  // resposta chega. O carimbo vem de quem pediu, em app.c, e e ele que permite
  // a qualquer consumidor perguntar "esta lista e do episodio que eu quero?".
  snprintf(alvoLista, sizeof alvoLista, "%s", alvoPedido);
}

void stream_invalidar(const char *porque) {
  if (n > 0) {
    printf("[fonte] %d fontes de %s descartadas: %s\n", n,
           alvoLista[0] ? alvoLista : "(sem alvo)", porque ? porque : "");
    fflush(stdout);
  }
  stream_definir_lista(NULL, 0);
  alvoLista[0] = 0;
}

int stream_n(void) {
  return n;
}

const Stream *stream_item(int i) {
  return i >= 0 && i < n ? &lista[i] : NULL;
}

// Pontuacao da regra do dono, do mais forte para o mais fraco:
//   MP4 4K Dolby Vision  >  4K Dolby Vision (qualquer container)
//   >  4K  >  Dolby Vision  >  resolucao  >  ordem de chegada
//
// Somar pesos em vez de comparar campo a campo deixa a regra num lugar so e
// legivel: mudar a preferencia e mexer num numero, nao reescrever um encadeado
// de ifs onde a ordem das comparacoes vira a regra escondida.
// TETO DE QUALIDADE ESCOLHIDO EM AJUSTES, que ate agora nao valia nada.
//
// "Qualidade maxima" (Automatica / 4K / 1080p / 720p) era gravada, sincronizada
// com a conta e lida por NINGUEM: `ajustes_qualidade()` nao tinha consumidor na
// escolha de fonte. Quem punha 1080p continuava abrindo a fonte 4K — inclusive
// em canal ao vivo, onde a 4K e a primeira da lista do addon e, numa conexao
// que nao a sustenta, ela e justamente a que demora ou nem abre.
//
// Devolve 0 para "sem teto".
static int alturaMax(void) {
  const char *q = ajustes_qualidade();
  if (!q || !q[0]) return 0;
  if (!strcmp(q, "4K"))    return 2160;
  if (!strcmp(q, "1080p")) return 1080;
  if (!strcmp(q, "720p"))  return 720;
  return 0;                                  // "Automatica"
}

// 1 quando a fonte cabe no teto. Fonte SEM altura declarada cabe: a maioria dos
// canais nao diz resolucao nenhuma, e recusar o que nao se sabe deixaria a
// pessoa sem fonte por causa de um campo que o addon nao preencheu.
static int cabeNoTeto(const Stream *s) {
  int teto = alturaMax();
  return !teto || !s->altura || s->altura <= teto;
}

static long pontos(const Stream *s) {
  long p = 0;
  // DOLBY VISION SO VALE PONTO EM MP4 — e isto e medida, nao teoria.
  //
  // Marcado no aparelho do dono (LG C9, webOS 4.10) tocando um MKV que o addon
  // anunciava como DV:
  //   hdr do pipeline: HDR10 (fonte DV=1)
  // A TV REBAIXOU para HDR10. E o comportamento ja relatado para Matroska —
  // webOS aciona DV nativo em MP4 e cai para HDR10 em MKV — agora confirmado
  // aqui em vez de citado.
  //
  // O que isso significa na pratica: num perfil 5 a camada base NAO e
  // compativel com HDR10 (e IPT-PQ), entao decodifica-la como HDR10 produz
  // exatamente as cores lavadas que o dono relatou. Preferir a versao DV em
  // MKV era escolher, de proposito, o arquivo que fica PIOR nesta TV.
  //
  // Nao ha como consertar a decodificacao pelo caminho da URI: o Kodi so
  // resolve descartando a camada de realce e reescrevendo o RPU, o que exige
  // demuxar e alimentar o pipeline por buffer — outro projeto, ja registrado em
  // video.c. O que ESTA ao alcance e parar de premiar a fonte que nao serve.
  if (s->mp4 && s->altura >= 2160 && s->dolbyVision) p += 100000;
  if (s->altura >= 2160)                             p +=  20000;
  if (s->mp4 && s->dolbyVision)                      p +=  10000;
  // MP4 NA FRENTE DENTRO DA MESMA FAIXA DE RESOLUCAO, pedido do dono (19/09):
  // na LG o MP4 e o container que toca Dolby Vision de verdade e o que menos
  // engasga no pipeline; entre um MP4 e um MKV da mesma altura, o MP4. Fica
  // ABAIXO da faixa de 4K (20000) de proposito: um MP4 1080p nao passa na
  // frente de um MKV 4K — trocar resolucao por container e outra decisao.
  // Nao vale no Tizen: la o AVPlay le MKV sem esse rebaixamento.
#ifndef __EMSCRIPTEN__
  if (s->mp4)                                        p +=   5000;
#endif
  if (s->dolbyAtmos)                                 p +=   2000;
  p += s->altura;
  // ACIMA DO TETO vai para o fim da fila, e nao para fora dela: o teto e
  // preferencia, nao filtro. Uma lista em que so ha 4K e com teto de 1080p tem
  // de continuar tocando — em 4K, com uma linha no log dizendo por que.
  if (!cabeNoTeto(s)) p -= 1000000;
  // FORA DE CACHE NO DEBRID vai para depois das cacheadas, e tambem nao sai
  // da fila: o automatico prefere o que TOCA AGORA. Registro 1163 (1.3.12,
  // AIOStreams+TorBox): a verificacao aceitou "⏳ FHD", o link do AIOStreams
  // manda o TorBox baixar e devolve um clipe de aviso de 8 s — que a
  // verificacao nao tem como distinguir de filme. Com a marca do proprio
  // addon, a cacheada da mesma lista vem antes. Menor que o teto (1000000):
  // uma cacheada acima do teto ainda perde para uma fora de cache dentro dele,
  // como ja perdia para qualquer fonte dentro dele.
  if (s->foraCache) p -= 500000;
  return p;
}

// Endereco de aviso e nao de conteudo. Estes dois foram MEDIDOS no aparelho:
// o AIOStreams manda para slate.m3u8/slate.mp4 ("This playback link couldn't be
// verified") quando o link expirou, e o Debridio para downloading.mp4 quando o
// arquivo ainda nao esta em cache no Real-Debrid. Os dois sao MP4 validos de
// ~120s que TOCAM NORMALMENTE — nao ha erro para detectar, so o endereco.
static int enderecoDeAviso(const char *u) {
  return strstr(u, "downloading.mp4") || strstr(u, "/slate") ||
         strstr(u, "slate.mp4") || strstr(u, "slate.m3u8") ? 1 : 0;
}

// PLAYLIST HLS SEM UM SEGMENTO SEQUER e fonte MORTA, e ela passava na
// verificacao.
//
// MEDIDO na LG, canal da FrostView que o dono relatou como "nao toca mais": o
// proxy devolve HTTP 200, `application/vnd.apple.mpegurl`, 98 bytes:
//     #EXTM3U
//     #EXT-X-VERSION:3
//     #EXT-X-TARGETDURATION:2
//     #EXT-X-MEDIA-SEQUENCE:0
//     #EXT-X-PLAYLIST-TYPE:LIVE
// Cabecalho e mais nada — nenhum #EXTINF, nenhum .ts, nenhuma variante. A
// origem (praia13.com) responde 522, ou seja o canal caiu e o proxy passou a
// servir um esqueleto. rede_url_final so pergunta "a URL resolve?", e resolve:
// a fonte era marcada OK, ia para o pipeline, e o webOS respondia
// `errorCode 100 "Playing error"` — que e o sintoma sem nenhuma pista.
// Outro canal do mesmo addon devolve 1585 bytes e toca, entao isto separa
// fonte morta de fonte viva no mesmo servidor.
//
// So para .m3u8: o custo e um GET de poucos KB na fonte que ja ia ser usada, e
// so acontece na candidata que chegou ate aqui. MP4 continua julgado pelo
// endereco, como antes.
static int playlistVazia(const char *url, const char *cabecalhos) {
  char *corpo;
  const char *vetor[8];
  char copia[512];
  long n = 0;
  int vazia, status = 0, nc = 0;
  if (!strstr(url, ".m3u8") && !strstr(url, "m3u8")) return 0;
  // Os cabecalhos que o addon exigiu (behaviorHints.proxyHeaders). Sem eles um
  // CDN que confere Referer devolve 403 — medido: 403 sem, 200 com.
  if (cabecalhos && *cabecalhos) {
    char *l, *ctx = NULL;
    snprintf(copia, sizeof copia, "%s", cabecalhos);
    for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx))
      vetor[nc++] = l;
  }
  vetor[nc] = NULL;
  corpo = rede_baixar_st(url, 8, nc ? vetor : NULL, &status);
  if (corpo) n = (long)strlen(corpo);
  if (!corpo) return 0;    // nao baixou: nao e prova de vazia, deixa passar
  // STATUS FORA DE 2xx NAO E PLAYLIST VAZIA — E RECUSA, E O CORPO E A PAGINA DE
  // ERRO.
  //
  // Este ramo e o defeito do relato de 17/09 no alvo Samsung. La a requisicao
  // sai por XHR, e o navegador PROIBE definir Referer, Origin e User-Agent:
  // os cabecalhos acima sao ignorados em silencio e o CDN responde 403. O corpo
  // do 403 (4,5 KB de HTML) voltava como se fosse a playlist, nao tinha
  // #EXTINF, e o canal era marcado "fora do ar" ANTES de o AVPlay tentar —
  // que e quem realmente sabe mandar cabecalho na TV. A tela dizia "nao foi
  // possivel abrir a fonte" sem nunca ter tentado abrir.
  //
  // Deixar passar e o certo: esta funcao existe para descartar canal
  // comprovadamente morto, e 403 nao prova isso.
  if (status && (status < 200 || status >= 300)) {
    printf("[fonte] playlist respondeu HTTP %d, nao julgo (pode ser cabecalho que o player manda)\n",
           status);
    free(corpo);
    return 0;
  }
  // Um segmento (#EXTINF) ou uma variante (#EXT-X-STREAM-INF) bastam. A lista
  // mestre so tem variantes; a de midia so tem segmentos.
  vazia = !strstr(corpo, "#EXTINF") && !strstr(corpo, "#EXT-X-STREAM-INF");
  if (vazia)
    printf("[fonte] playlist sem segmento (%ld B)\n", n);
  free(corpo);
  return vazia;
}

// VERIFICACAO DAS CANDIDATAS: EM SERIE, E SO ATE A PRIMEIRA QUE SERVE — #130.
//
// HISTORICO, para ninguem desfazer por velocidade. Ate a 1.4.3 isto era um
// lote de 4 fios sobre ate 8 candidatas (+ a preferida). A regra de escolha era
// a mesma ("a de maior pontuacao que resolve"), so que os 4 fios saiam JUNTOS
// e cada um, ao terminar, pegava a proxima da fila enquanto a primeira ainda
// nao tinha resposta. Com a primeira sendo um torrent lento no TorBox (cache,
// createtorrent, ate 3 olhadas no mylist, requestdl), os outros tres fios
// passavam por 4, 5, 6... Resultado no painel do relator do #130: SEIS
// arquivos da Lioness carregados para UMA reproducao — e cada um conta na cota
// do mes do servico.
//
// Conferir uma candidata de debrid nao e so perguntar. O GET com Range que
// rede_url_final faz no link de reproducao do AIOStreams e o que manda o
// AIOStreams adicionar o torrent na conta; o infoHash sem url passa por
// debrid_resolver, que faz createtorrent/addMagnet. Nos dois casos a conta da
// pessoa ganha um arquivo.
//
// O QUE A SERIE CUSTA EM TEMPO: nada, quando a primeira serve — o lote tambem
// esperava o veredito da PRIMEIRA DA ORDEM antes de decidir (issue #61). So
// quando ha falhas na frente elas passam a somar em vez de se sobrepor, e a
// falha tipica (fora de cache, aviso, 4xx) responde em segundos.
//
// A fila e montada por fonteauto.c: no modo "Primeira da lista" ela tem UMA
// candidata, e nenhuma outra URL e tocada.
#define VER_MAX  16

// A lista pode ser trocada por stream_definir_lista enquanto uma candidata e
// conferida fora da trava. A geracao diz se o indice ainda e da mesma lista;
// sem ela, a url resolvida de um episodio ia parar na linha de mesmo numero
// do episodio seguinte.
typedef struct { unsigned geracao; int abortou; } Conferencia;

static int verificarUma(int i, Conferencia *c) {
  char fim[900], url[4096], cab[512];
  int fileIdx, ok = 0;
  char infoHash[48];
  pthread_mutex_lock(&verTrava);
  // Copia do que precisa, sob a trava: fora dela `lista[]` pode ser trocada.
  // Lista trocada por baixo: a candidata nao existe mais, e nada adiante dela
  // vale ser conferido — parar aqui e o que nao toca uma URL a toa.
  if (listaGeracao != c->geracao || i >= n) {
    c->abortou = 1;
    pthread_mutex_unlock(&verTrava);
    return 0;
  }
  snprintf(url, sizeof url, "%s", lista[i].url);
  snprintf(cab, sizeof cab, "%s", lista[i].cabecalhos);
  snprintf(infoHash, sizeof infoHash, "%s", lista[i].infoHash);
  fileIdx = lista[i].fileIdx;
  pthread_mutex_unlock(&verTrava);

  if (!url[0] && infoHash[0]) {
    // Link recem-saido do unrestrict: nao precisa da segunda viagem abaixo.
    if (debrid_resolver(infoHash, fileIdx, url, sizeof url)) {
      pthread_mutex_lock(&verTrava);
      if (listaGeracao == c->geracao && i < n) {
        snprintf(lista[i].url, sizeof lista[i].url, "%s", url);
        ok = 1;
      } else c->abortou = 1;
      pthread_mutex_unlock(&verTrava);
    } else printf("[fonte] %d torrent nao resolveu no debrid\n", i);
  } else if (!url[0]) {
    ok = 0;
  } else if (!rede_url_final(url, 10, fim, sizeof fim)) {
    printf("[fonte] %d nao resolveu\n", i);
  } else if (enderecoDeAviso(fim)) {
    printf("[fonte] %d e aviso (%.60s)\n", i, fim);
  } else if (playlistVazia(fim, cab)) {
    printf("[fonte] %d tem playlist vazia (canal fora do ar)\n", i);
  } else ok = 1;
  return ok;
}

// fonteauto_primeira nao sabe de lista trocada: depois de uma conferencia
// abortada, as candidatas seguintes "falham" sem tocar a rede.
static int verificarOuParar(int i, void *u) {
  Conferencia *c = u;
  return c->abortou ? 0 : verificarUma(i, c);
}
// Candidata que nao serviu sai da fila DESTA lista: a proxima escolha (o
// reenvio de tentarProximaFonteVOD em app.c) nao a confere de novo — seria
// mais um arquivo no painel do debrid por uma fonte que ja se sabe ruim.
static void falhouUma(int i, void *u) {
  Conferencia *c = u;
  if (!c->abortou) stream_automatico_excluir(i);
}

unsigned stream_lista_geracao(void) {
  unsigned g;
  pthread_mutex_lock(&verTrava);
  g = listaGeracao;
  pthread_mutex_unlock(&verTrava);
  return g;
}

int stream_resolver_escolhida(int i, unsigned geracao, char *url, unsigned nu,
                              char *servico, unsigned ns, int *pct) {
  char infoHash[48];
  int fileIdx, r;
  if (url && nu) url[0] = 0;
  if (!url || !nu) return 0;
  pthread_mutex_lock(&verTrava);
  if (listaGeracao != geracao || i < 0 || i >= n) {
    pthread_mutex_unlock(&verTrava);
    return -1;
  }
  // Ja resolvida (pelo automatico, ou por uma escolha anterior desta lista):
  // e a url pronta, sem ir a rede.
  if (lista[i].url[0]) {
    snprintf(url, nu, "%s", lista[i].url);
    pthread_mutex_unlock(&verTrava);
    return 1;
  }
  snprintf(infoHash, sizeof infoHash, "%s", lista[i].infoHash);
  fileIdx = lista[i].fileIdx;
  pthread_mutex_unlock(&verTrava);
  if (!infoHash[0]) return 0;

  r = debrid_resolver_escolhido(infoHash, fileIdx, url, nu, servico, ns, pct);
  if (r == 1) {
    pthread_mutex_lock(&verTrava);
    if (listaGeracao == geracao && i < n)
      snprintf(lista[i].url, sizeof lista[i].url, "%s", url);
    else r = -1;
    pthread_mutex_unlock(&verTrava);
    if (r < 0) url[0] = 0;
  } else if (r == DEBRID_BAIXANDO) {
    printf("[fonte] %d torrent escolhido esta baixando no %s (%d%%)\n", i,
           servico && servico[0] ? servico : "debrid", pct ? *pct : -1);
  } else {
    printf("[fonte] %d torrent escolhido nao resolveu no debrid\n", i);
  }
  return r;
}

int stream_primeira_boa(int tentativas) {
  int fila[VER_MAX], nf, q, tocadas = 0, escolhida, total, pref;
  int modo = ajustes_fonte_primeira() ? FONTEAUTO_PRIMEIRA : FONTEAUTO_MELHOR;
  long *pts;
  unsigned char *acima, *excl;
  Conferencia c = { 0, 0 };
  tentativas = fonteauto_tentativas(modo, tentativas);
  pthread_mutex_lock(&verTrava);
  total = n;
  pref = preferida;
  c.geracao = listaGeracao;
  if (total < 1) { pthread_mutex_unlock(&verTrava); return -1; }
  if (tentativas > total) tentativas = total;
  if (tentativas > VER_MAX) tentativas = VER_MAX;
  pts = malloc(sizeof *pts * (size_t)total);
  acima = calloc((size_t)total, 1);
  excl = calloc((size_t)total, 1);
  if (!pts || !acima || !excl) {
    pthread_mutex_unlock(&verTrava);
    free(pts); free(acima); free(excl);
    return -1;
  }
  for (q = 0; q < total; q++) {
    pts[q] = pontos(&lista[q]);
    acima[q] = (unsigned char)!cabeNoTeto(&lista[q]);
    excl[q] = (unsigned char)automaticaExcluida(q);
  }
  pthread_mutex_unlock(&verTrava);
  nf = fonteauto_fila(modo, total, pref, pts, acima, excl, tentativas, fila);
  free(pts); free(acima); free(excl);
  if (nf < 1) return -1;

  marco("fonte: verificacao inicio");
  escolhida = fonteauto_primeira(fila, nf, verificarOuParar, falhouUma, &c, &tocadas);
  if (c.abortou) escolhida = -1;
  marco(escolhida >= 0 ? "fonte: verificacao ok" : "fonte: verificacao sem resultado");
  printf("[fonte] verificacao (%s): %d de %d candidata(s) conferida(s)%s\n",
         modo == FONTEAUTO_PRIMEIRA ? "primeira da lista" : "melhor fonte",
         tocadas, nf, c.abortou ? ", lista trocada no meio" : "");
  if (escolhida >= 0) printf("[fonte] %d ok\n", escolhida);
  return escolhida;
}

// A PRIMEIRA FONTE DE CANAL QUE ESTA VIVA, conferida em paralelo e por
// PLAYLIST, nao por pipeline.
//
// MEDIDO na LG, canal da FrostView com seis candidatas mortas: o canal ia
// DIRETO para a primeira da lista e o watchdog de app.c dava 12 s a cada uma
// antes de passar para a proxima — 12, 24, 36, 48, 60 s no log, quase dois
// minutos ate tocar. E o prazo nao pode ser curto: 12 s e o que um canal VIVO
// leva para abrir num 4K pesado.
//
// O barato aqui e que uma fonte morta se denuncia em MEIO SEGUNDO: o proxy
// responde 200 com uma playlist de 98 bytes, cabecalho e nenhum segmento (ver
// playlistVazia). Entao, em vez de esperar o pipeline falhar, pergunta-se a
// playlist — as candidatas em paralelo, e a primeira viva vai para o player.
//
// ORDEM DA LISTA, e nao pontuacao: para canal ao vivo o addon ja manda
// FHD/HD/SD ordenado, e essa ordem e o ranking dele. (O caminho de filme, em
// stream_primeira_boa, continua por pontuacao.)
//
// NAO chama rede_url_final: o link de canal nao passa por debrid nem por
// redirecionamento de expiracao, e aquela chamada custa outro pedido com
// timeout de 10 s. Aqui o unico pedido e o da propria playlist, com 5 s.
// 8 fios e nao 4: a conferencia inteira precisa caber numa rodada so, senao a
// segunda leva outro CANAL_PRAZO_S. Sao pedidos de poucos KB.
#define CANAL_FIOS   8
// 3 s. MEDIDO na LG: canal vivo devolve a playlist em 0,5 s; o proxy que
// pendurou nao devolve em 5, 12 nem 30 (curl do proprio aparelho: `http=000`,
// zero byte). Esperar mais so adia o inevitavel — e este prazo entra INTEIRO no
// tempo que a pessoa fica olhando para a tela preta quando o canal esta fora.
#define CANAL_PRAZO_S 3
static int canalProx, canalN;
// Classe de cada candidata: 0 = nao conferida ainda, 1 = VIVA (playlist com
// segmento), 2 = MORTA (respondeu sem segmento), 3 = MUDA (nao respondeu).
static unsigned char *canalClasse;
static int classeEscolhida;
static pthread_mutex_t canalTrava = PTHREAD_MUTEX_INITIALIZER;

static void *fioCanal(void *u) {
  (void)u;
  for (;;) {
    int meu;
    char *corpo;
    long n = 0;
    pthread_mutex_lock(&canalTrava);
    if (canalProx >= canalN) { pthread_mutex_unlock(&canalTrava); return NULL; }
    meu = canalProx++;
    pthread_mutex_unlock(&canalTrava);
    if (!lista[meu].url[0]) { canalClasse[meu] = 2; continue; }
    { const char *vetor[8];
      char copia[512];
      int nc = 0, status = 0;
      if (lista[meu].cabecalhos[0]) {
        char *l, *ctx = NULL;
        snprintf(copia, sizeof copia, "%s", lista[meu].cabecalhos);
        for (l = strtok_r(copia, "\n", &ctx); l && nc < 7; l = strtok_r(NULL, "\n", &ctx))
          vetor[nc++] = l;
      }
      vetor[nc] = NULL;
      corpo = rede_baixar_st(lista[meu].url, CANAL_PRAZO_S, nc ? vetor : NULL, &status);
      if (corpo) n = (long)strlen(corpo);
      // RECUSA NAO E MORTE, QUANDO O ADDON PEDIU CABECALHO.
      //
      // Este e o caminho do relato de 17/09: canal aparece no guia e some ao
      // abrir, com "nao foi possivel abrir a fonte". Medido contra o addon do
      // relator: o CDN responde 403 sem Referer/Origin/User-Agent e 200 com
      // eles. O corpo do 403 — 4,5 KB de HTML — chegava aqui, nao tinha
      // #EXTINF, e o canal virava classe 2 (MORTA). Todas mortas, nenhuma para
      // tocar, erro na tela.
      //
      // No alvo Samsung isto nao se resolve mandando o cabecalho daqui: a
      // requisicao sai por XHR e o navegador PROIBE definir Referer, Origin e
      // User-Agent — sao cabecalhos controlados pelo agente. Quem sabe manda-los
      // na TV e o AVPlay, que e nativo. Ou seja, esta sonda NAO E AUTORIDADE
      // sobre esta fonte: ela responde 403 para nos e 200 para o player.
      //
      // Entao, quando o addon DECLAROU cabecalhos e a resposta foi recusa, a
      // fonte entra como VIVA e quem decide e o player. Sem cabecalho
      // declarado, 403 continua valendo como morta — ali a sonda e o player
      // veem a mesma coisa.
      if (corpo && status >= 400 && lista[meu].cabecalhos[0]) {
        canalClasse[meu] = 1;
        printf("[fonte] %d respondeu HTTP %d, mas o addon exige cabecalho: quem decide e o player\n",
               meu, status);
        free(corpo);
        continue;
      }
    }
    if (!corpo) {
      // MUDA. MEDIDO na LG com o curl do proprio aparelho: estas URLs do proxy
      // do FrostView nao devolvem NADA — `http=000`, 12 s de espera, zero byte.
      // Nao e a rede da casa: outra URL do mesmo host responde 200 em 0,5 s.
      // Fonte que nao entrega a playlist em CANAL_PRAZO_S tambem nao vai
      // entregar segmento ao pipeline, entao ela cai para ultimo recurso — mas
      // NAO e descartada, porque uma rede ruim de verdade se pareceria com isto.
      canalClasse[meu] = 3;
      printf("[fonte] %d muda: playlist nao respondeu em %ds\n", meu, CANAL_PRAZO_S);
    } else if (strstr(corpo, "#EXTINF") || strstr(corpo, "#EXT-X-STREAM-INF")) {
      canalClasse[meu] = 1;
    } else {
      canalClasse[meu] = 2;
      printf("[fonte] %d morta: playlist com %ld B e nenhum segmento\n", meu, n);
    }
    free(corpo);
  }
}

int stream_canal_primeira_viva(int tentativas) {
  int total = stream_n(), q, criados = 0, escolhida = -1;
  pthread_t fios[CANAL_FIOS];
  if (total < 1) return -1;
  if (tentativas < 1 || tentativas > total) tentativas = total;
  canalClasse = calloc((size_t)tentativas, 1);
  if (!canalClasse) return -1;
  marco("canal: conferindo playlists");
  canalProx = 0; canalN = tentativas;
  for (q = 0; q < CANAL_FIOS && q < tentativas; q++)
    if (pthread_create(&fios[criados], NULL, fioCanal, NULL) == 0) criados++;
  if (!criados) fioCanal(NULL);
  for (q = 0; q < criados; q++) pthread_join(fios[q], NULL);

  // PREFERENCIA POR CLASSE, e dentro da classe pela ORDEM DO ADDON — que para
  // canal ao vivo e o ranking dele (FHD/HD/SD). Viva ganha de muda; muda ganha
  // de nada. Morta nunca entra: ela JA respondeu dizendo que nao tem segmento.
  // Ordem de preferencia: viva dentro do teto, viva acima do teto, muda dentro
  // do teto, muda. O teto nunca tira a ultima fonte da mesa.
  for (q = 0; q < tentativas && escolhida < 0; q++)
    if (canalClasse[q] == 1 && cabeNoTeto(&lista[q])) escolhida = q;
  for (q = 0; q < tentativas && escolhida < 0; q++)
    if (canalClasse[q] == 1) escolhida = q;
  for (q = 0; q < tentativas && escolhida < 0; q++)
    if (canalClasse[q] == 3 && cabeNoTeto(&lista[q])) escolhida = q;
  for (q = 0; q < tentativas && escolhida < 0; q++)
    if (canalClasse[q] == 3) escolhida = q;
  if (escolhida >= 0 && !cabeNoTeto(&lista[escolhida]))
    printf("[fonte] canal: nenhuma fonte dentro do teto de %dp; usando %dp\n",
           alturaMax(), lista[escolhida].altura);
  { int vivas = 0, mortas = 0, mudas = 0;
    for (q = 0; q < tentativas; q++) {
      if (canalClasse[q] == 1) vivas++;
      else if (canalClasse[q] == 2) mortas++;
      else if (canalClasse[q] == 3) mudas++;
    }
    printf("[fonte] canal: %d viva(s), %d morta(s), %d muda(s) de %d; escolhida %d\n",
           vivas, mortas, mudas, tentativas, escolhida); }
  marco(escolhida >= 0 ? "canal: fonte escolhida por playlist"
                       : "canal: nenhuma playlist utilizavel");
  // A CLASSE DA ESCOLHIDA fica disponivel para quem chamou: uma fonte MUDA que
  // entrou por falta de opcao nao merece o mesmo prazo de uma viva. Ver
  // stream_canal_classe_escolhida e o watchdog em app.c.
  classeEscolhida = (escolhida >= 0) ? canalClasse[escolhida] : 0;
  free(canalClasse); canalClasse = NULL;
  return escolhida;
}

int stream_canal_classe_escolhida(void) { return classeEscolhida; }

int stream_automatico(void) {
  if (!stream_n()) return -1;
  int melhor = -1;
  long maior = 0;
  for (int i = 1; i < n; i++) {
    if (automaticaExcluida(i)) continue;
    long p = pontos(&lista[i]);
    // `>` e nao `>=`: em empate fica o PRIMEIRO da lista, que e a ordem em que
    // o addon devolveu — e ele costuma saber algo que a pontuacao nao ve.
    if (melhor < 0 || p > maior) { maior = p; melhor = i; }
  }
  if (!automaticaExcluida(0) && (melhor < 0 || pontos(&lista[0]) > maior))
    melhor = 0;
  return melhor;
}


static int grupo, filtro, soMp4;

// BOTOES DO CABECALHO. "Sem HDR" so existe onde ha o que renegociar (webOS);
// ver o bloco "TELA PRETA COM AUDIO TOCANDO" em video.h. Oferecer um botao que
// nao faz nada seria pior que nao oferecer: a pessoa aperta, nada muda, e passa
// a duvidar dos outros dois. "Só MP4" (#91) filtra a lista — permanece na folha.
enum { BT_RECARREGAR, BT_SEM_HDR, BT_SO_MP4, BT_FECHAR };
static int botaoDe(int i) {
  // Ordem visivel: Recarregar, [Sem HDR], Só MP4, Fechar.
  if (video_pode_forcar_sdr()) {
    if (i == 0) return BT_RECARREGAR;
    if (i == 1) return BT_SEM_HDR;
    if (i == 2) return BT_SO_MP4;
    return BT_FECHAR;
  }
  if (i == 0) return BT_RECARREGAR;
  if (i == 1) return BT_SO_MP4;
  return BT_FECHAR;
}
static int nBotoes(void) { return video_pode_forcar_sdr() ? 4 : 3; }
static const char *rotuloBotao(int b) {
  if (b == BT_RECARREGAR) return "Recarregar";
  if (b == BT_SEM_HDR)    return "Sem HDR";
  if (b == BT_SO_MP4)     return soMp4 ? "MP4 ✓" : "MP4";
  return "Fechar";
}
static char provedores[13][96];
static int nProvedores;

static int passaFiltro(int i) {
  if (soMp4 && !lista[i].mp4) return 0;  if (filtro && strcmp(lista[i].provedor, provedores[filtro])) return 0;
  return 1;
}

static void atualizarProvedores(void) {
  nProvedores = 1;
  snprintf(provedores[0],sizeof provedores[0],"Todos");
  for (int i=0;i<n;i++) {
    int j;
    for(j=1;j<nProvedores;j++) if(!strcmp(provedores[j],lista[i].provedor)) break;
    if(j==nProvedores && nProvedores<13)
      snprintf(provedores[nProvedores++],96,"%s",lista[i].provedor);
  }
  if(filtro>=nProvedores) filtro=0;
}
static int filtrado(int linha) {
  for(int i=0,j=0;i<n;i++)
    if(passaFiltro(i))
      if(j++==linha) return i;
  return -1;
}
static int nFiltrados(void) {
  int k=0;
  for(int i=0;i<n;i++) if(passaFiltro(i)) k++;
  return k;
}

// O foco da fonte segue o mesmo botao primario do menu: fill accent limpo e
// halo macio atras do alvo, sem degradê, translucidez ou reflexo de vidro.
static void corFocoFonte(float *r, float *g, float *b) {
  ajustes_acento(r, g, b);
}

static void focoFonte(GfxRect r, float raio, float alfa) {
  float sr, sg, sb;
  if (alfa <= 0.01f) return;
  corFocoFonte(&sr, &sg, &sb);
  // Retangulos de linha sao altos; metade da intensidade da pilula mantem a
  // luz visivel sem espalhar uma mancha por varios cartoes vizinhos.
  botao_luz(r, 0.55f, alfa);
  gfx_cor(r, raio, sr, sg, sb, alfa);
}

// EQUALIZADOR DO "REPRODUZINDO AGORA". O player nativo nao expoe amplitude
// de audio por quadro, entao isto NAO finge ser medidor: e uma assinatura visual
// discreta de que a fonte esta ativa. O movimento usa so primitivas ja existentes
// e o relogio do desenho; sem alocacao, textura ou fio novo.
static void desenharAudioBars(float x, float y, float alfa, int focado,
                              Uint32 agora) {
  static const float parado[FOLHA_AUDIO_N] = { .35f, .58f, .82f, .52f, .72f, .44f, .64f, .48f };
  float cr, cg, cb;
  int i;
  ajustes_acento_tinta(&cr, &cg, &cb);
  if (focado) cr = cg = cb = ajustes_acento_tinta(NULL, NULL, NULL);
  for (i = 0; i < FOLHA_AUDIO_N; i++) {
    float nivel = parado[i];
    float h;
    if (!ajustes_animacoes_reduzidas())
      nivel = .22f + .78f * (.5f + .5f * sinf((float)agora * .0042f + i * .82f));
    h = 10.0f + nivel * 32.0f;
    gfx_cor((GfxRect){ x + i * (FOLHA_AUDIO_BAR + FOLHA_AUDIO_GAP),
                       y + 42.0f - h, FOLHA_AUDIO_BAR, h },
            .5f, cr, cg, cb, alfa * .92f);
  }
}

void stream_folha_abrir(void) {
  int excl;
  aberta=1; escolha=-1; foco=0; grupo=1; filtro=0; soMp4=0; recarregar=0;
  atualizarProvedores();
  if(atual>=0) foco=atual;
  rolagem=0;velRol=0;
  // A CONTAGEM DA FOLHA NO LOG (#132). O log tinha "[addons] X: N fontes" e
  // "[addons] total N" de um lado e nada do que a folha mostrou do outro: um
  // relato de "so 1 fonte listada" nao tinha como dizer se a queda foi no
  // addon, no descarte de torrent sem debrid ou na folha.
  pthread_mutex_lock(&autoExclTrava);
  excl = nAutomaticasExcluidas;
  pthread_mutex_unlock(&autoExclTrava);
  printf("[fonte] folha: %d de %d na lista (%d addon(s); %d torrent(s) sem debrid "
         "descartado(s); %d ja recusada(s) pelo automatico, continuam na folha)\n",
         nFiltrados(), n, nProvedores - 1, descartadosSemDebrid, excl);
  fflush(stdout);
}
int stream_folha_aberta(void) { return aberta; }
int stream_folha_n(void) { return nFiltrados(); }
void stream_folha_evento(const SDL_Event *e) {
  if(!aberta || e->type!=SDL_KEYDOWN) return;
  SDL_Keycode k=e->key.keysym.sym;
  if(k==SDLK_ESCAPE || k==SDLK_AC_BACK || k==SDLK_BACKSPACE || k==SDLK_DELETE) {aberta=0;return;}
  if(k==SDLK_r) {recarregar=1;return;}
  int nf=nFiltrados();
  if(k==SDLK_UP) {if(grupo==1 && foco>0) foco--; else if(grupo>-1) grupo--;}
  if(k==SDLK_DOWN) {if(grupo<1) grupo++; else if(foco<nf-1) foco++;}
  if(grupo==0 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    filtro+=k==SDLK_RIGHT?1:-1;
    if(filtro<0) filtro=0;
    if(filtro>=nProvedores) filtro=nProvedores-1;
    foco=0;rolagem=0;velRol=0;
  }
  if(grupo==-1 && (k==SDLK_LEFT || k==SDLK_RIGHT)) {
    foco+=k==SDLK_RIGHT?1:-1;
    if(foco<0) foco=0;
    if(foco>=nBotoes()) foco=nBotoes()-1;
  }
  if(k==SDLK_RETURN || k==SDLK_KP_ENTER) {
    if(grupo==-1) {
      switch(botaoDe(foco)) {
        case BT_RECARREGAR: recarregar=1; break;
        // Fecha a folha junto: a imagem volta (ou nao) na propria tela do
        // player, e deixar a folha aberta em cima esconderia o resultado.
        case BT_SEM_HDR:    video_forcar_sdr(); aberta=0; break;
        case BT_SO_MP4:     soMp4 = !soMp4; foco=0; rolagem=0;velRol=0; break;
        default:            aberta=0; break;
      }
    }
    else if(grupo==0) {grupo=1;foco=0;}
    else {escolha=filtrado(foco);if(escolha>=0) aberta=0;}
  }
}
void stream_folha_atualizar(float dt, Uint32 agora) {
  (void)agora;
  anim=anim_mola(anim,aberta?1:0,dt,NV_MOLA_TELA);
  atualizarProvedores();
  int nf=nFiltrados();
  if(grupo==1 && foco>=nf) foco=nf>0?nf-1:0;
  float area=NV_TELA_H-FOLHA_TOPO-32;
  float max=nf*FOLHA_LINHA-area;
  float alvo=foco*FOLHA_LINHA-(area-FOLHA_LINHA)*.5f;
  if(alvo>max) alvo=max;
  if(alvo<0) alvo=0;
  rolagem=anim_mola2(&velRol,rolagem,alvo,dt,NV_MOLA2_SCROLL);
}
int stream_folha_escolheu(int *out) {
  if(escolha<0) return 0;
  if(out) *out=escolha;
  escolha=-1;return 1;
}
// PONTEIRO (#99): as mesmas variaveis das setas (grupo, foco, filtro). O
// provedor so troca no CLIQUE — trocar ao passar por cima mudaria a lista
// debaixo da mao. Clicar fora do painel fecha, como o Voltar.
static void ponteiroFolhaBotao(int i, int b) { (void)b; grupo = -1; foco = i; }
static void ponteiroFolhaLinha(int row, int b) { (void)b; grupo = 1; foco = row; }
static void ponteiroFolhaFiltro(int i, int b) {
  (void)b;
  if (i < 0 || i >= nProvedores) return;
  filtro = i; foco = 0; rolagem = 0; grupo = 1;
}
static void ponteiroFolhaFora(int a, int b) { (void)a; (void)b; aberta = 0; }

void stream_folha_desenhar(Uint32 agora) {
  (void)agora;
  if(anim<.005f) return;
  float x=NV_TELA_W-FOLHA_W+(1-anim)*FOLHA_W;
  // O foco tem fill solido; o painel permanece neutro e so o alvo recebe halo.
  gfx_cor((GfxRect){0,0,NV_TELA_W,NV_TELA_H},0,.02f,.02f,.025f,.35f*anim);
  // Painel flutuante com raio amplo e material neutro. A separacao vem do
  // veu e da superficie, nao de uma luz decorativa presa ao canto.
  gfx_cor((GfxRect){x,24,FOLHA_W,NV_TELA_H-48},28.0f/FOLHA_W,.055f,.058f,.068f,.965f*anim);
  txt_desenhar_alpha(txt_linha(TXT_PAINEL_TITULO,"Fontes",240,241,243,255),x+40,44,anim);
  int ptr = aberta && anim > .5f && ponteiro_ativo();
  if (ptr) {
    ponteiro_alvo(0, 0, x, NV_TELA_H, NULL, ponteiroFolhaFora, 0, 0);
    // O painel em si absorve o clique no vazio (nao fecha, nao da OK).
    ponteiro_alvo(x, 0, FOLHA_W, NV_TELA_H, NULL, NULL, 0, 0);
  }
  int nbt=nBotoes();
  for(int i=0;i<nbt;i++) {
    // Ancorado a DIREITA: com dois ou tres botoes a fileira termina sempre no
    // mesmo ponto, 36 px antes da borda do painel.
    float bx=x+FOLHA_W-36-(nbt-i)*128+8;
    int sel=grupo==-1 && foco==i;
    // Acoes seguem o accent solido e a tinta calculada pelo tema.
    if (ptr) ponteiro_alvo(bx, 44, 120, 50, ponteiroFolhaBotao, NULL, i, 0);
    if(sel) focoFonte((GfxRect){bx,44,120,50},.3f,anim);
    else    gfx_cor((GfxRect){bx,44,120,50},.3f,.075f,.079f,.092f,anim);
    int c=sel?ajustes_tinta_foco():224;
    TxtLinha l=txt_linha(TXT_PG_FIM,rotuloBotao(botaoDe(i)),c,c,c,255);
    txt_desenhar_alpha(l,bx+(120-l.w)*.5f,58,anim);
  }
  // A LINHA DE CONTEXTO EXPLICA O BOTAO EM FOCO. "Sem HDR" nao se explica pelo
  // rotulo, e o rotulo nao pode crescer sem estourar a pilula de 120 px.
  { const char *ajuda=contexto;
    if(grupo==-1 && botaoDe(foco)==BT_SEM_HDR)
      ajuda="Imagem preta com o áudio tocando? Recarrega esta fonte sem HDR nem Dolby Vision.";
    else if(grupo==-1 && botaoDe(foco)==BT_RECARREGAR)
      ajuda="Pergunta as fontes de novo a todos os addons.";
    else if(grupo==-1 && botaoDe(foco)==BT_SO_MP4)
      ajuda=soMp4
        ? "Mostrando só containers MP4 (útil para achar Dolby Vision em MP4). OK tira o filtro."
        : "Filtra a lista para fontes em MP4. OK liga o filtro.";
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,ajuda,184,187,193,255,FOLHA_W-80),x+40,126,anim); }
  gfx_recorte(x+40,180,FOLHA_W-80,62);
  int ini=filtro>1?filtro-1:0;
  float tx=x+40;
  for(int i=ini;i<nProvedores && i<ini+3;i++) {
    float w=i?232:108;int sel=i==filtro;
    int c=sel&&grupo==0?ajustes_tinta_foco():sel?245:190;
    if (ptr) ponteiro_alvo(tx, 182, w, 50, NULL, ponteiroFolhaFiltro, i, 0);
    if(sel && grupo==0) focoFonte((GfxRect){tx,182,w,50},.5f,anim);
    else gfx_cor((GfxRect){tx,182,w,50},.5f,
                 sel?.092f:.075f,sel?.096f:.079f,sel?.110f:.092f,anim);
    TxtLinha l=txt_linha_corta(TXT_PG_FIM,provedores[i],c,c,c,255,w-24);
    txt_desenhar_alpha(l,tx+(w-l.w)*.5f,196,anim);
    if(sel && grupo!=0) {
      float cr,cg,cb; ajustes_acento(&cr,&cg,&cb);
      gfx_cor((GfxRect){tx+16,237,w-32,2},1,cr,cg,cb,anim);
    }
    tx+=w+12;
  }
  gfx_sem_recorte();
  gfx_recorte(x+36,FOLHA_TOPO,FOLHA_W-72,NV_TELA_H-FOLHA_TOPO-32);
  int nf=nFiltrados();
  // A FONTE QUE O AUTOMATICO ESCOLHERIA, marcada. E a resposta a "se eu nao
  // escolher nada, o que toca?" — que ate aqui a folha nao dava: a pessoa via
  // trinta linhas e a pontuacao do dono (MP4 4K DV primeiro) decidia em
  // silencio. A ordem e a mesma de stream_primeira_boa: a lembrada vai na
  // frente quando existe; senao, a de maior pontuacao. Pedido do dono, 16/09.
  //
  // Calculado UMA vez por quadro, fora do laco: stream_automatico percorre a
  // lista inteira, e chama-lo por linha seria n^2 a cada quadro.
  int automatica = preferida >= 0 && !automaticaExcluida(preferida)
                     ? preferida : stream_automatico();
  for(int row=0;row<nf;row++) {
    float y=FOLHA_TOPO+row*FOLHA_LINHA-rolagem;
    if(y+FOLHA_LINHA<FOLHA_TOPO || y>NV_TELA_H-32) continue;
    int i=filtrado(row),sel=grupo==1 && foco==row;
    int corTitulo,corProv,corDesc,corMeta;
    const Stream *s=&lista[i];
    // Cartao cheio e silencioso; o foco solido usa tinta calculada no accent.
    GfxRect r={x+40,y,FOLHA_W-80,FOLHA_LINHA-14};
    if (ptr) {
      // So o que o recorte da lista deixa ver.
      float t = y < FOLHA_TOPO ? FOLHA_TOPO : y;
      float b = y + r.h > NV_TELA_H - 32 ? NV_TELA_H - 32 : y + r.h;
      if (b > t) ponteiro_alvo(r.x, t, r.w, b - t, ponteiroFolhaLinha, NULL, row, 0);
    }
    if(sel) focoFonte(r,.10f,anim);
    else gfx_cor(r,.10f,.062f,.066f,.079f,.92f*anim);
    // O proprio material colorido identifica o foco; nao sobrepor outro ponto.
    { int tinta=ajustes_tinta_foco(), tinta2=ajustes_tinta_foco2();
      int c1=sel?tinta:240, c2=sel?tinta2:175;
      int c3=sel?tinta2:194, c4=sel?tinta2:224;
      corTitulo=c1; corProv=c2; corDesc=c3; corMeta=c4; }
    float lx=x+62,w=FOLHA_W-124;
    char nome[sizeof s->rotulo],descricao[sizeof s->descricao];
    snprintf(nome,sizeof nome,"%s",s->rotulo);snprintf(descricao,sizeof descricao,"%s",s->descricao);
    // SDL_ttf nao interpreta quebras de linha; nao renderizar glifos .notdef.
    for(char *p=nome;*p;p++)if((unsigned char)*p<32)*p=' ';
    for(char *p=descricao;*p;p++)if((unsigned char)*p<32)*p=' ';
    txt_desenhar_alpha(txt_linha_corta(TXT_PAINEL_ITEM,nome,corTitulo,C8(corTitulo+1),C8(corTitulo+3),255,w),lx,y+16,anim);
    // A FONTE LEMBRADA, MARCADA. Sem a marca, quem abre a folha para conferir
    // continua procurando a propria fonte entre dezenas de linhas — que e a
    // queixa literal do issue #56 ("search through many links to find the same
    // source again"). Ancorada a DIREITA da mesma linha do provedor: e o unico
    // espaco vazio da linha, e alinhada a direita ela nao empurra nada.
    //
    // Nao aparece na que esta tocando: ali "Reproduzindo agora" ja ocupa a
    // linha e dizer as duas coisas seria ruido.
    //
    // A LINHA DO PROVEDOR PERDE A LARGURA DA MARCA, e por isso ela e desenhada
    // ANTES. Cortar as duas pela largura inteira faria um nome de addon longo
    // passar por baixo do texto da marca — e em portugues a marca e mais larga
    // que em ingles, entao o defeito apareceria so num dos dois idiomas.
    //
    // A MARCA E UMA PILULA na cor de realce com texto escuro — o mesmo
    // desenho do foco no resto do app desde 16/09 — e nao texto solto: a
    // tres metros, texto colorido de 20 px some no meio de quatro linhas de
    // texto; a pilula e a unica forma cheia da linha e o olho vai nela.
    //
    // NA LINHA SELECIONADA ELA INVERTE, como todo o resto da linha. O acento
    // e branco por padrao: pilula de acento sobre linha clara e uma pilula
    // invisivel com texto escuro solto — pior do que nao ter marca. Continua
    // CHEIA, so troca figura e fundo (fundo escuro, texto claro); um contorno
    // escuro traria de volta justamente o contorno que o dono tirou do app no
    // dia 16/09, e a 3 m um traco de 2 px perde para uma forma cheia.
    float wProv = w;
    if (i != atual && (i == automatica || i == preferida)) {
      float ar, ag, ab;
      TxtLinha m;
      GfxRect pil;
      const char *rot = (i == preferida && i == automatica) ? "Sua escolha anterior · automática"
                      : (i == preferida) ? "Sua escolha anterior"
                      : "Escolha automática";
      ajustes_acento(&ar, &ag, &ab);
      // Sobre linha clara a pilula veste a superficie de repouso da linha
      // (.135,.135,.14) com o texto claro das demais linhas nao selecionadas.
      if (sel) { ar = .135f; ag = .135f; ab = .14f; }
      m = txt_linha(TXT_MINI, rot, sel ? 234 : ajustes_tinta_foco(), sel ? 236 : ajustes_tinta_foco(), sel ? 242 : ajustes_tinta_foco(), 255);
      pil = (GfxRect){ lx + w - (float)m.w - 24.0f, y + 44.0f, (float)m.w + 24.0f, (float)m.h + 10.0f };
      gfx_cor(pil, NV_RAIO_PILL, ar, ag, ab, anim);
      txt_desenhar_alpha(m, pil.x + 12.0f, pil.y + 5.0f, anim);
      // 40 px E NAO 24 DE FOLGA. Com 24 o nome de um addon longo era cortado a
      // 23 px da pilula — dois blocos de texto encostados que o olho le como
      // um so. Relato do dono (16/09): "deixa a badge menos colado no texto".
      wProv = w - pil.w - 40.0f;
      if (wProv < 120.0f) wProv = 120.0f;
    }
    // A fonte ativa ganha um respiro para o equalizador. O rotulo continua
    // sendo texto, entao a traducao de "Reproduzindo agora" permanece na
    // camada de idioma e nao vira uma badge diferente em cada tela.
    if (i == atual) wProv -= FOLHA_AUDIO_W + 14.0f;
    if (wProv < 120.0f) wProv = 120.0f;
    txt_desenhar_alpha(txt_linha_corta(TXT_PG_FIM,i==atual?"Reproduzindo agora":s->provedor,corProv,C8(corProv+3),C8(corProv+10),255,wProv),lx,y+46,anim);
    if (i == atual)
      desenharAudioBars(lx + w - FOLHA_AUDIO_W, y + 40.0f, anim, sel, agora);
    // AS TRES LINHAS DE BAIXO DESCEM 10 px, EM BLOCO. A pilula acaba em y+69 e
    // a descricao comecava em y+76: 8 px de tinta a tinta, que a 3 m viram
    // zero. Os 10 px saem da sobra do RODAPE da linha (as badges acabavam em
    // y+197 numa linha de 214), entao nenhum vao entre as linhas de baixo
    // muda — so entra ar debaixo da pilula. Mexer na pilula em vez disso a
    // tiraria do centro da linha do provedor, que e onde ela esta ancorada.
    txt_bloco(TXT_PG_FIM,descricao,corDesc,C8(corDesc+3),C8(corDesc+8),lx,y+86,w,25,anim,2);
    char meta[192],qual[24]="";
    float mx = lx;
    const char *cont = containerDa(s);
    int ehMp4 = !strcmp(cont, "MP4");
    if(s->altura) snprintf(qual,sizeof qual," · %dp",s->altura);
    // MP4 EM DESTAQUE: pilula cheia na cor de acento no lugar da sigla solta,
    // porque na LG e o container que vale escolher (ver pontos()). O resto da
    // linha de meta segue depois dela.
    if (ehMp4) {
      float ar, ag, ab;
      TxtLinha m;
      GfxRect pil;
      ajustes_acento(&ar, &ag, &ab);
      if (sel) { ar = .135f; ag = .135f; ab = .14f; }
      m = txt_linha(TXT_MINI, "MP4", sel ? 234 : ajustes_tinta_foco(), sel ? 236 : ajustes_tinta_foco(), sel ? 242 : ajustes_tinta_foco(), 255);
      pil = (GfxRect){ lx, y + 146.0f, (float)m.w + 20.0f, (float)m.h + 8.0f };
      gfx_cor(pil, NV_RAIO_PILL, ar, ag, ab, anim);
      txt_desenhar_alpha(m, pil.x + 10.0f, pil.y + 4.0f, anim);
      mx = lx + pil.w + 10.0f;
    }
    snprintf(meta,sizeof meta,"%s%s%s%s",cont,qual,s->dolbyVision?" · Dolby Vision":"",s->dolbyAtmos?" · Atmos":"");
    if(s->tamanhoMB) {size_t p=strlen(meta);snprintf(meta+p,sizeof meta-p," · %.1f GB",s->tamanhoMB/1024.0);}
    { const char *texto = meta;
      // A sigla ja esta na pilula: o texto comeca depois dela e do " · " (4
      // bytes: espaco, U+00B7 em dois bytes, espaco).
      if (ehMp4) { texto += 3; if (!strncmp(texto, " \xc2\xb7 ", 4)) texto += 4; }
      txt_desenhar_alpha(txt_linha_corta(TXT_MINI,texto,corMeta,C8(corMeta+2),C8(corMeta+8),255,w-(mx-lx)),mx,y+150,anim); }
    // O foco conserva cartao escuro em qualquer tema; as logos claras ficam
    // no tratamento padrao e nao trocam para tinta escura no acento branco.
    badges_desenhar(s->badges,lx,y+181,w,26,anim);
  }
  if(!nf) {
    // A FOLHA VAZIA DIZ A CAUSA (B6/#107, D5). So quando a lista esta vazia
    // de verdade (n == 0): lista cheia com filtro de provedor que nao casa
    // nada fica na frase generica, porque ali a causa e o filtro na tela.
    // Ordem: torrent descartado por falta de debrid primeiro (se havia fonte,
    // "os addons nao tem" seria falso), depois o resumo da consulta.
    char causa[160], frase[320];
    const char *s=addons_estado()==ADD_BUSCANDO?"Buscando fontes nos addons…":"Nenhuma fonte direta disponível. Use Recarregar para tentar novamente.";
    if (addons_estado()!=ADD_BUSCANDO && n==0) {
      int tem = 0;
      if (descartadosSemDebrid > 0) {
        snprintf(causa,sizeof causa,i18n("%d fontes precisam de uma conta de debrid, e nenhuma está ligada"),descartadosSemDebrid);
        tem = 1;
      } else tem = addons_motivo_vazio(causa,sizeof causa);
      if (tem) { snprintf(frase,sizeof frase,"%s. %s",causa,i18n("Use Recarregar para tentar novamente.")); s=frase; }
    }
    txt_bloco(TXT_PG_FIM,s,196,199,204,x+56,FOLHA_TOPO+40,FOLHA_W-112,28,anim,3);
  }
  gfx_sem_recorte();
}
