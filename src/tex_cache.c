#include "tex_cache.h"
#include "rede.h"
#include "gfx.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "webp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ITENS_ABS 512
#define MAX_FILA 128
#define NV_TEX_STALE_FRAMES 8
#define NV_TEX_STALE_MS 200
#define NV_TEX_UPLOAD_BUDGET_MS 4.0

// FALHOU e um estado de verdade, nao a ausencia de um. Sem ele, um caminho que
// nao decodifica volta a VAZIO, o desenho pede de novo no quadro seguinte e o
// ciclo nao termina nunca: o item ocupa uma vaga em voo e um turno na fila a
// frente das imagens boas, para falhar de novo. Com recuo, a segunda tentativa
// so acontece daqui a 2 s, a terceira a 10 s, e depois desiste na sessao.
typedef enum { VAZIO=0, PENDENTE, DECODIFICADO, PRONTO, FALHOU } Estado;

typedef struct {
  char caminho[512];
  unsigned long hash;  // FNV-1a do caminho, para pular o strcmp na busca
  Estado estado;
  SDL_Surface *sup;   // preenchida pela thread; consumida no bombear
  GLuint tex;
  int w, h;
  // Teto de largura PEDIDO para este item. O hero em tela cheia precisa de 1920;
  // um poster de 212 nao. Um teto unico para todos servia mal aos dois: a 960 o
  // hero era decodificado com metade da resolucao e esticado para 1920 na tela,
  // que e o borrao que o dono viu.
  int limite;
  unsigned long uso;  // contador LRU
  // Luminancia media dos pixels OPACOS, 0..255; -1 enquanto nao se sabe.
  // Medida uma vez, na thread de decode. Serve ao logo do titulo: o TMDB nao
  // marca claro/escuro em lugar nenhum (o ranking do proprio app web e so
  // idioma + vote_average), entao a unica forma de saber se um logo e preto e
  // OLHAR os pixels.
  int lum;
  // Quando tentar de novo (ticks) e quantas vezes ja falhou. Ver o enum Estado.
  Uint32 tentarEm;
  int    falhas;
  unsigned long ultimoQuadro;
  Uint32 ultimoPedido;
  // Croma medio: max(R,G,B) - min(R,G,B) dos mesmos pixels opacos. Separa logo
  // PRETO (acromatico, variante errada do TMDB) de logo de MARCA escuro mas
  // colorido (vermelho, vinho), que deve passar intacto.
  int croma;
} Item;

#define NV_TEX_FIOS 2
#define NV_TEX_FIOS_REDE 4
static SDL_Thread *thrs[NV_TEX_FIOS];
static SDL_Thread *thrsRede[NV_TEX_FIOS_REDE];
static Item itens[MAX_ITENS_ABS];
static int nMax = 64;
static unsigned long relogio = 1;

static SDL_mutex *mtx;
static SDL_cond  *cond;
static SDL_Thread *thr;
static int rodando = 0;

// Quanto de memoria as texturas PRONTAS ocupam. Sem esta conta o cache so
// despejava quando FALTAVA SLOT — e com 96 slots de arte 1920x1080 isso da 800
// MB. Com arte de verdade o app chegou a 104 MB e morreu com "double free or
// corruption" dentro do SDL: era falta de memoria, nao bug de ponteiro.
static long bytesUsados = 0;
static long orcamento = 0;
// Pasta do cache de download. Declarada aqui e nao mais abaixo porque
// noCache() precisa dela.
static char dirCache[512];
// SO PODE APAGAR O QUE ESTA NO NOSSO CACHE DE DOWNLOAD.
//
// Esta funcao existe porque eu apaguei arte do PACOTE. As duas remocoes que
// acrescentei nesta rodada (o arquivo que nao decodifica, e o teto do cache em
// MEMFS) rodavam sobre `caminho` sem olhar de onde ele vinha — e icones e
// badges vem do --preload-file, em /app/art/icones, que nao e cache: e o
// pacote. Apagado dali, o icone NAO TEM COMO VOLTAR, porque nao ha URL para
// rebaixar. O sintoma foi exatamente esse: "os icones pararam de carregar".
//
// O ramo de falha que ja existia no arquivo sempre teve esta guarda, com o
// comentario "So apaga o que esta no NOSSO cache". Eu escrevi duas remocoes
// novas ao lado dela e nao a repeti.
static int noCache(const char *caminho) {
  return dirCache[0] && caminho &&
         !strncmp(caminho, dirCache, strlen(dirCache));
}
// Bytes gravados no cache de disco. No alvo Tizen "disco" e MEMFS, ou seja RAM
// (o log mostra idbfs=0/0.0ms), e nada nunca e apagado — cada arte baixada fica
// na memoria pelo resto da sessao. Isso nao aparecia em lugar nenhum: nem no
// [mem], que mede o heap do malloc, nem no total de texturas, que mede a GPU.
// Sem este numero nao da para dizer se a travada depois de muito uso e o cache
// de disco crescendo ou outra coisa.
static long cacheDiscoBytes = 0;
// TETO DO CACHE DE DISCO, so no alvo Tizen. La o "disco" e MEMFS, ou seja RAM:
// medido na TV, chegou a 280 MB so navegando, mais do que o heap inteiro do
// app (256 MiB) e invisivel a todos os outros contadores. No LG o cache e disco
// de verdade e nao precisa de teto nenhum.
//
// 48 MB e o que cabe sem competir com o heap e ainda segura algumas telas de
// arte ja baixada. NAO e numero medido: e o teto que falta ser calibrado com o
// cache-disco= do relatorio, e por isso ele continua no log.
#ifdef __EMSCRIPTEN__
#define NV_CACHE_DISCO_MAX (48L * 1024L * 1024L)
#else
#define NV_CACHE_DISCO_MAX (1L << 60)   /* sem teto: disco de verdade */
#endif
static unsigned long quadroAtual = 1;

// O driver tambem aloca a piramide de mipmaps. Contar apenas o nivel base
// deixava o cache ultrapassar o teto real em cerca de 33% nas artes de card.
// COBRAR A PIRAMIDE SO QUANDO ELA EXISTE.
//
// A regra "w < 1024" era o palpite certo no LG e ERRADO no Tizen. La o driver
// Mali expoe OES_texture_npot e quase toda arte ganha piramide; aqui o WebGL 1
// RECUSA mipmap em textura nao-potencia-de-dois (o painel da TV confirma:
// WebGL/npot = false), entao praticamente nenhum card tem piramide — e mesmo
// assim todos eram cobrados por ela.
//
// O efeito e ~33% de sobrecontagem em cada textura de card: o cache se julgava
// nos 96 MB do orcamento estando em ~72 MB reais, e despejava muito mais do que
// precisava. Medido na TV: despejos=84, depois 66, depois 41 em intervalos de
// 3 s, com texturas=104 — ou seja, girando quase o cache inteiro, e cada volta
// custa baixar, decodificar e subir de novo. E esse giro que aparece como FPS
// caindo para 2 de vez em quando e voltando.
static long bytesTexturaMip(int w, int h, int comPiramide) {
  long total = (long)w * h * 4;
  int mw = w, mh = h;
  if (!comPiramide) return total;
  while (mw > 1 || mh > 1) {
    mw = (mw + 1) / 2;
    mh = (mh + 1) / 2;
    total += (long)mw * mh * 4;
  }
  return total;
}

// Mesma decisao que o envio toma, para a conta e a cobranca nao divergirem.
static int temPiramide(int w, int h) {
  if (w >= 1024) return 0;
#ifdef __EMSCRIPTEN__
  { int lp = (w > 0) && ((w & (w - 1)) == 0);
    int ap = (h > 0) && ((h & (h - 1)) == 0);
    if (!lp || !ap) return 0; }
#endif
  return 1;
}

static long bytesTextura(int w, int h) {
  return bytesTexturaMip(w, h, temPiramide(w, h));
}

// DUAS FILAS, e a separacao e o conserto.
//
// `fila` era a unica, e o download acontecia DENTRO do fio de decodificacao
// (garantirLocal, chamado de threadDecode). Com dois fios de decode em
// prioridade baixa, cada um ficava BLOQUEADO NA REDE por ate 8 s em vez de
// decodificar — com cache frio a arte entrava a conta-gotas mesmo com a rede
// sobrando, que foi exatamente o relato: "a internet ta rapida e as artes
// demoram".
//
// Agora `fila` e a fila de REDE (baixar para o cache de disco) e `filaDec` a de
// DECODIFICACAO. Rede e espera de I/O, nao trabalho de CPU: da para ter varios
// fios sem roubar quadro do desenho. Decodificar continua com dois, em
// prioridade baixa, pelo motivo ja medido.
static int fila[MAX_FILA];
static int filaIni = 0, filaFim = 0;
static int filaDec[MAX_FILA];
static int decIni = 0, decFim = 0;
static SDL_cond *condDec;

// Sinalizado pelos fios de decode quando LIBERAM um lugar na fila.
static SDL_cond *condLivre;

// Enfileira para DECODIFICAR. Chamado com o mutex tomado, e ESPERA quando a
// fila esta cheia.
//
// Descartar em silencio, que era o que estava aqui, deixava o item PENDENTE
// para sempre: ninguem o decodificava e nada o reenfileirava. MEDIDO com cache
// frio: as texturas subiam ate 24 e PARAVAM — quatro fios de rede enchem a fila
// mais rapido do que dois de decode a esvaziam, entao o descarte virava a regra
// e nao a excecao. Esperar aqui e o que faz a rede andar no passo do decode em
// vez de atropela-lo.
static void paraDecode(int idx) {
  for (;;) {
    int prox = (decFim + 1) % MAX_FILA;
    if (prox != decIni) {
      filaDec[decFim] = idx; decFim = prox;
      SDL_CondSignal(condDec);
      return;
    }
    if (!rodando) return;
    SDL_CondWait(condLivre, mtx);
  }
}

// FNV-1a do caminho. A busca abaixo roda para cada card visivel em cada
// quadro, contra ate 96 slots; comparar um inteiro primeiro reduz o strcmp a
// so os candidatos com o mesmo hash (na pratica, o proprio item).
static unsigned long hashCaminho(const char *s) {
  unsigned long h = 2166136261UL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}

int    tex_n_busca = 0;
double tex_ms_busca = 0.0;
static double texFreqMs = 0.0;
static int pedidoObsoleto(const Item *it) {
  Uint32 agora;
  if (!it->ultimoPedido || !it->ultimoQuadro) return 0;
  agora = SDL_GetTicks();
  return quadroAtual > it->ultimoQuadro + NV_TEX_STALE_FRAMES &&
         agora - it->ultimoPedido >= NV_TEX_STALE_MS;
}

void tex_novo_quadro(void) {
  tex_n_busca = 0;
  tex_ms_busca = 0.0;
  (void)texFreqMs;
  if (!mtx) return;
  SDL_LockMutex(mtx);
  quadroAtual++;
  // Um decode concluido mas nunca mais desenhado nao deve ocupar memoria nem
  // bloquear a arte que entrou na tela. Pedidos PENDENTES sao cancelados pelo
  // consumidor da fila, para que o indice do slot nao seja reutilizado antes
  // de a fila o retirar.
  for (int i = 0; i < nMax; i++) {
    if (itens[i].estado == DECODIFICADO && pedidoObsoleto(&itens[i])) {
      SDL_FreeSurface(itens[i].sup);
      itens[i].sup = NULL;
      itens[i].estado = VAZIO;
      itens[i].caminho[0] = 0;
    }
  }
  SDL_UnlockMutex(mtx);
}

// Envolve TRAVA + busca com relogio de CPU. So os chamadores da THREAD DE
// DESENHO passam por aqui; a thread de decode usa acharIndice cru, senao os
// dois fios somariam no mesmo contador e o numero deixaria de descrever o
// quadro.
//
// A TRAVA ENTRA NA CONTA, e nao so a busca. Os fios de decode rodam em
// prioridade BAIXA e pegam este mesmo mutex: um fio de decode preemptado
// SEGURANDO o mutex faz o fio de desenho esperar por ele — inversao de
// prioridade classica. Medir so o acharIndice esconderia exatamente esse custo,
// que e o unico caminho pelo qual o decode pode roubar o quadro.
//
// RESULTADO, para nao refazer a conta: com 192 slots e ~70 buscas por quadro na
// home, trava + busca linear somaram 0,04 a 0,12 ms POR QUADRO — menos de 1% de
// um quadro de 20ms. A busca linear e a inversao de prioridade estavam na lista
// de suspeitos do quadro de 22ms e as duas foram DESCARTADAS POR MEDIDA; nao
// vale trocar isto por tabela de hash. O relogio fica atras de NV_PERF_FINO
// porque custava duas leituras por consulta.
#ifdef NV_PERF_FINO
#define BUSCA_MEDIDA(idx, cam, h) do { \
  if (texFreqMs == 0.0) texFreqMs = 1000.0 / (double)SDL_GetPerformanceFrequency(); \
  Uint64 t0_ = SDL_GetPerformanceCounter(); \
  SDL_LockMutex(mtx); \
  (idx) = acharIndice((cam), (h)); \
  tex_ms_busca += (double)(SDL_GetPerformanceCounter() - t0_) * texFreqMs; \
  tex_n_busca++; \
} while (0)
#else
#define BUSCA_MEDIDA(idx, cam, h) do { \
  SDL_LockMutex(mtx); \
  (idx) = acharIndice((cam), (h)); \
  tex_n_busca++; \
} while (0)
#endif

static int acharIndice(const char *caminho, unsigned long h) {
  for (int i = 0; i < nMax; i++)
    if (itens[i].estado != VAZIO && itens[i].hash == h &&
        strcmp(itens[i].caminho, caminho) == 0) return i;
  return -1;
}

// Escolhe vitima LRU entre os PRONTOS. Nunca descarta o que esta em voo, senao
// a thread escreveria em cima de um slot reaproveitado.
// Quantos pedidos podem estar EM VOO ao mesmo tempo.
//
// Sem teto, uma tela cheia de arte nova pede ~90 imagens de uma vez, os slots
// enchem de PENDENTE e o slotLivre passa a descartar textura PRONTA para dar
// lugar a mais pendente — a tela FICA EM BRANCO e nao volta, porque o que
// termina e jogado fora antes de aparecer.
//
// So aparece com o cache de disco FRIO (primeira execucao, ou logo depois de
// reinstalar o app, que apaga art/cache junto). Foi assim que apareceu aqui: eu
// tinha acabado de apagar o cache a mao e o dono viu a home sem poster nenhum.
//
// Um terco dos slots mantem o fio de decodificacao ocupado e ainda deixa dois
// tercos para o que ja esta na tela.
static int emVoo(void) {
  int n = 0;
  for (int i = 0; i < nMax; i++)
    if (itens[i].estado == PENDENTE || itens[i].estado == DECODIFICADO) n++;
  return n;
}

static int slotLivre(void) {
  if (emVoo() >= nMax / 3) return -1;   // pede de novo no proximo quadro
  for (int i = 0; i < nMax; i++) if (itens[i].estado == VAZIO) return i;
  // Slot que ja desistiu vale mais como vaga que um PRONTO em uso: reaproveita
  // antes de despejar arte que esta na tela.
  for (int i = 0; i < nMax; i++)
    if (itens[i].estado == FALHOU && itens[i].falhas >= 3) {
      memset(&itens[i], 0, sizeof(Item));
      itens[i].lum = -1;
      return i;
    }
  int melhor = -1; unsigned long menor = ~0UL;
  for (int i = 0; i < nMax; i++) {
    if (itens[i].estado != PRONTO) continue;
    if (itens[i].uso < menor) { menor = itens[i].uso; melhor = i; }
  }
  if (melhor >= 0) {
    if (itens[melhor].tex) { gfx_tex_esquecer(itens[melhor].tex); glDeleteTextures(1, &itens[melhor].tex); }
    bytesUsados -= bytesTextura(itens[melhor].w, itens[melhor].h);
    if (bytesUsados < 0) bytesUsados = 0;
    memset(&itens[melhor], 0, sizeof(Item));
    itens[melhor].lum = -1;   // 0 seria "preto"; o desconhecido e -1
  }
  return melhor;
}

// Despeja os menos usados ate caber no orcamento. Chamada com o mutex travado.
static void podar(void) {
  while (bytesUsados > orcamento) {
    int melhor = -1; unsigned long menor = ~0UL;
    for (int i = 0; i < nMax; i++) {
      if (itens[i].estado != PRONTO) continue;
      if (itens[i].uso < menor) { menor = itens[i].uso; melhor = i; }
    }
    if (melhor < 0) break;          // so restou o que esta em voo
    if (itens[melhor].tex) { gfx_tex_esquecer(itens[melhor].tex); glDeleteTextures(1, &itens[melhor].tex); }
    bytesUsados -= bytesTextura(itens[melhor].w, itens[melhor].h);
    if (bytesUsados < 0) bytesUsados = 0;
    memset(&itens[melhor], 0, sizeof(Item));
    itens[melhor].lum = -1;   // 0 seria "preto"; o desconhecido e -1
  }
}

// Diretorio onde as imagens baixadas ficam. Uma vez baixada, a imagem vale
// para sempre: arte de filme nao muda. Sem isto cada volta a home refaria
// dezenas de downloads.
// Teto de largura da textura.
//
// 960 e nao 1280. As contas: o card de arte mede 410px e o cartao grande do
// detalhe 684; so o hero e a arte em tela cheia passam disso, e nesses dois a
// imagem ja aparece desfocada ou coberta de texto. A 1280 o cache vivia
// ENCOSTADO no teto de 72 MB (medido: 71,4 MB com 29 texturas), despejando e
// rebaixando sem parar — o que aparece como 30fps com jank em todo quadro
// depois de alguns minutos de uso. A 960 a mesma cena cabe com folga.
// Teto de decodificacao das artes de CARD.
//
// Era 960 para tudo, e a maior arte de card que a tela desenha e a miniatura de
// episodio, com 640 (NV_DETP_EP_W). Um poster de 212 de largura era decodificado
// a 960x1440 e custava 5 MB de textura — vinte deles ja passam do orcamento
// inteiro de 96 MB.
//
// Foi o que o dono viu: mexendo nas fileiras, e principalmente ao ABRIR UM
// FILME (que pede backdrop de 1920 mais miniaturas, posteres de relacionados e
// fotos de elenco de uma vez), o total estourava e o podar despejava tudo que
// estava na tela — ficava cinza e nao voltava.
//
// 640 cobre a maior arte de card sem sobra e divide o custo por 2,25: o mesmo
// poster passa a custar 2,2 MB. O hero continua com teto proprio de 1920, pela
// promocao.
#define NV_TEX_LARG_MAX 640
// TETO DO HEROI: 1920 no webOS, 1280 no Tizen. Os dois numeros sao medidos, e
// medem coisas diferentes porque as duas TVs se comportam de forma oposta.
//
//   LG C9      despejos=0    espera media 287 ms   estouros 3 em 18
//   Samsung    despejos=31 a 56 por amostra de 3 s, espera media 399 ms,
//   AU7000     estouros 18 em 24 — tres de cada quatro trocas de heroi
//              mostram o marcador em vez da arte
//
// No Tizen o cache vive ENCOSTADO no teto (texturas=94, 94.4MB de 96) e gira
// quase inteiro a cada poucos segundos; cada volta custa baixar, decodificar e
// subir de novo, e e isso que aparece como FPS 22-28 com 35 janks. Um heroi de
// 1920x1080x4 sao 8,3 MB; a 1280 sao 3,7 MB — 55% a menos, e a decodificacao
// cai junto, que e o que a espera de 400 ms mede.
//
// POR QUE NAO PRE-BUSCAR OS VIZINHOS, que era o pedido do #21: pre-busca
// ACRESCENTA duas texturas de heroi num cache que ja despeja. O que sairia sao
// os posteres — e o mesmo relator diz que os posteres tambem estao lentos.
// Seria trocar fundo lento por tudo lento.
//
// O LG fica em 1920 porque la nao ha problema nenhum a resolver, e cortar
// qualidade sem defeito e so perda.
#ifdef __EMSCRIPTEN__
#define NV_TEX_HERO_LARG_MAX 1280
#else
#define NV_TEX_HERO_LARG_MAX 1920
#endif

// TETO POR USO — o 640 acima e o padrao, e ele e GRANDE DEMAIS para a maioria
// das artes. Ele foi dimensionado pela MAIOR arte de card (a miniatura de
// episodio, 640), mas se aplica a todas: um poster desenhado com 212 de largura
// era decodificado a 640x960 e custava 2,4 MB. Com o orcamento de 96 MB isso da
// ~40 texturas — MEDIDO no aparelho: com a home rolando o log mostrava
// `texturas=40 pend=32 92.3MB`, ou seja, a fila entupida e o cache ja
// despejando o que ainda estava na tela para caber o que entrava.
//
// Esse e o "carrega as coisas enquanto passa": nao e rede nem decode lento, e o
// cache batendo no teto e re-decodificando o que acabou de despejar.
//
// Aqui cada chamador pede pela LARGURA COM QUE DESENHA, e a conta vira
// largura * escala do buffer * folga. Na TV a escala e 1 (drawable=1920x1080,
// medido) e um poster passa a custar ~420 KB em vez de 2,4 MB — quase seis
// vezes mais arte no mesmo orcamento. No Mac retina a escala e 2 e a previa
// continua nitida.
//
// A FOLGA de 1,25 cobre o card que cresce ao receber foco (escala ~1,08) e
// evita reamostrar no limite exato, que serrilha.
#define NV_TEX_FOLGA 1.25f
static float escalaBuf = 1.0f;

void tex_escala(float e) {
  if (e > 0.1f && e < 8.0f) escalaBuf = e;
}


void tex_cache_dir(const char *dir) {
  if (!dir || !*dir) return;
  snprintf(dirCache, sizeof dirCache, "%s", dir);
  mkdir(dirCache, 0777);
  // A PASTA PODE EXISTIR E NAO SER GRAVAVEL, e isso ja aconteceu: o tools/arm.sh
  // manda art/ num tar feito no Mac, e o tar extraido como root na TV carimba
  // o dono com o uid do Mac (13888160) e modo 755. O app roda como uid 5152,
  // entao depois de CADA deploy a pasta ficava so-leitura.
  //
  // O efeito era invisivel: garantirLocal baixava a imagem, o fopen do
  // temporario falhava, ela devolvia 0 sem dizer nada, e o unico sintoma era
  // "card sem arte". Medido: 91 "decode falhou" numa navegacao, com ZERO erro
  // de rede — as imagens chegavam e eram jogadas fora. Cada uma volta a ser
  // pedida ate o terceiro recuo, entao os dois fios de decode ficam ocupados
  // baixando o que nunca vai poder ser guardado.
  //
  // O app nao consegue consertar (chmod de quem nao e dono falha), mas TEM de
  // dizer. Sem esta linha o defeito nao tem como ser atribuido a causa.
  if (access(dirCache, W_OK) != 0)
    printf("[tex] PASTA DE CACHE SEM ESCRITA: %s — toda imagem baixada sera"
           " descartada (confira o dono, o deploy carimba o uid do Mac)\n",
           dirCache);
  fflush(stdout);
}

// Nome de arquivo estavel a partir da URL. Hash simples (FNV-1a) e nao o nome
// da URL porque elas trazem barra, query e caracteres que nao cabem em nome de
// arquivo — e porque duas URLs diferentes precisam de arquivos diferentes.
static void nomeDeCache(const char *url, char *dst, size_t tam) {
  unsigned long h = 2166136261UL;
  const char *p = url, *ponto = strrchr(url, '.');
  char ext[8] = ".jpg";
  for (; *p; p++) { h ^= (unsigned char)*p; h *= 16777619UL; }
  if (ponto && strlen(ponto) <= 5 && !strchr(ponto, '/'))
    snprintf(ext, sizeof ext, "%s", ponto);
  snprintf(dst, tam, "%s/%08lx%s", dirCache, h, ext);
}

// Baixa a URL para o cache, se ainda nao estiver la. Devolve 1 se ha arquivo
// utilizavel no fim. Roda no fio de decodificacao, entao bloquear aqui nao
// custa quadro nenhum.
static int garantirLocal(const char *url, char *dst, size_t tam) {
  FILE *f;
  char *corpo;
  long n = 0;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
    snprintf(dst, tam, "%s", url);
    return 1;
  }
  if (!dirCache[0]) return 0;
  nomeDeCache(url, dst, tam);
  f = fopen(dst, "rb");
  if (f) { fseek(f, 0, SEEK_END); n = ftell(f); fclose(f); if (n > 512) return 1; }
  // 8 s e nao 25: isto e uma IMAGEM. Com 25 s, duas URLs mortas seguravam os
  // dois fios de decode por quase um minuto e a tela inteira parava de receber
  // arte — repetidamente, porque nada guarda a falha.
  corpo = rede_baixar_bin(url, 8, &n);
  // ESTE RAMO ERA MUDO. Medido numa navegacao da home: 93 "decode falhou" com
  // ZERO "[rede] falha" no log — todas as falhas passavam por aqui, com o curl
  // dizendo sucesso e um corpo curto demais para ser imagem. Sem a linha nao
  // havia como distinguir "servidor recusou" de "cache sem permissao de
  // escrita" de "resposta vazia". Nao repete o caso do HTTP >= 400, que agora
  // o rede.c nomeia sozinho.
  if (!corpo || n <= 512) {
    // O printf ESTAVA DENTRO DE `if (corpo)`, o que calava justamente o caso
    // mais comum: rede_baixar_bin devolvendo NULL. MEDIDO no alvo Tizen: 151
    // downloads tentados, ZERO linha de log e zero textura — com o comentario
    // logo acima afirmando que este ramo ja nao era mudo. Como nada guarda a
    // falha, cada quadro pedia de novo as mesmas URLs, para sempre.
    if (corpo) printf("[tex] corpo curto (%ld B): %.70s\n", n, url);
    else       printf("[tex] download falhou (sem corpo): %.70s\n", url);
    fflush(stdout);
    free(corpo);
    return 0;
  }
  // ASSINATURA DE IMAGEM. rede.c nao confere status HTTP, entao um 404 com
  // pagina de erro de mais de 512 bytes era gravado como "imagem" e ficava no
  // cache de disco PARA SEMPRE — o item nunca mais teria arte, nem depois de o
  // servidor voltar. Aceita JPEG (FF D8), PNG (89 50 4E 47), GIF e RIFF/WEBP.
  { const unsigned char *b0 = (const unsigned char *)corpo;
    int ok = (n > 4) && (
       (b0[0] == 0xFF && b0[1] == 0xD8) ||
       (b0[0] == 0x89 && b0[1] == 0x50 && b0[2] == 0x4E && b0[3] == 0x47) ||
       (b0[0] == 'G'  && b0[1] == 'I'  && b0[2] == 'F') ||
       (b0[0] == 'R'  && b0[1] == 'I'  && b0[2] == 'F'  && b0[3] == 'F'));
    if (!ok) {
      printf("[tex] resposta nao e imagem (%ld B): %.70s\n", n, url);
      fflush(stdout);
      free(corpo);
      return 0;
    } }
  { char tmp[600];
    // Grava em temporario e renomeia: outro fio pode estar lendo o mesmo
    // arquivo, e um arquivo pela metade decodifica como imagem quebrada e fica
    // em cache assim para sempre.
    snprintf(tmp, sizeof tmp, "%s.parcial", dst);
    f = fopen(tmp, "wb");
    // Falhava em SILENCIO. Ver a nota em tex_cache_dir: pasta sem permissao de
    // escrita joga fora toda imagem baixada e o unico sintoma era card cinza.
    if (!f) { printf("[tex] nao consegui gravar %.80s\n", tmp); fflush(stdout);
              free(corpo); return 0; }
    // O RETORNO DO fwrite IMPORTA, e o de fclose tambem.
    //
    // Sem conferir, uma gravacao PARCIAL virava arquivo de cache "valido": o
    // rename promovia o truncado, o teste de assinatura logo acima continuava
    // passando (o comeco do JPEG esta la, FF D8) e o decode falhava DEPOIS, com
    // "Unsupported image format". Como o arquivo ficava no cache, aquela arte
    // nunca mais carregava — o defeito se perpetuava sozinho.
    //
    // No alvo Tizen isto nao e hipotetico: o cache de disco vive em MEMFS, ou
    // seja, na RAM (o log mostra idbfs=0), e sob pressao de memoria a gravacao
    // e exatamente o que fica pela metade. Foi assim que os
    // "decode falhou (Unsupported image format): /nuvio/cache/*.jpg"
    // apareceram em serie na TV.
    { size_t esc = fwrite(corpo, 1, (size_t)n, f);
      int fim = fclose(f);
      if (esc != (size_t)n || fim != 0) {
        printf("[tex] gravacao incompleta (%zu de %ld B): %.70s\n", esc, n, dst);
        fflush(stdout);
        remove(tmp);              // nao deixa meio arquivo virar cache
        free(corpo);
        return 0;
      } }
    rename(tmp, dst);
    cacheDiscoBytes += n;   // decrementado quando o arquivo e apagado
  }
  free(corpo);
  return 1;
}

// FIO DE REDE: tira da fila, garante o arquivo no cache de disco e passa para a
// decodificacao. Nao toca em pixel nenhum, entao pode rodar em prioridade
// normal e em varios — o que ele faz e ESPERAR.
static int threadRede(void *arg) {
  (void)arg;
  for (;;) {
    int idx;
    char caminho[512], local[600];
    SDL_LockMutex(mtx);
    while (rodando && filaIni == filaFim) SDL_CondWait(cond, mtx);
    if (!rodando) { SDL_UnlockMutex(mtx); return 0; }
    idx = fila[filaIni]; filaIni = (filaIni + 1) % MAX_FILA;
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) {
        itens[idx].estado = VAZIO;
        itens[idx].caminho[0] = 0;
      }
      SDL_UnlockMutex(mtx);
      continue;
    }
    strncpy(caminho, itens[idx].caminho, sizeof caminho - 1);
    caminho[sizeof caminho - 1] = 0;
    SDL_UnlockMutex(mtx);

    // Caminho local devolve na hora; so URL sai para a rede.
    SDL_LockMutex(mtx);
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) {
        itens[idx].estado = VAZIO;
        itens[idx].caminho[0] = 0;
      }
      SDL_UnlockMutex(mtx);
      continue;
    }
    SDL_UnlockMutex(mtx);

    if (!garantirLocal(caminho, local, sizeof local)) {
      // Falhou o download. Marca como falha AQUI para o recuo valer — antes o
      // decode e que marcava, e ate la o item ficava PENDENTE ocupando slot.
      SDL_LockMutex(mtx);
      if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
        if (itens[idx].estado == PENDENTE) {
          itens[idx].estado = VAZIO;
          itens[idx].caminho[0] = 0;
        }
        SDL_UnlockMutex(mtx);
        continue;
      }
      itens[idx].estado = FALHOU;
      itens[idx].falhas++;
      itens[idx].tentarEm = SDL_GetTicks() +
          (itens[idx].falhas == 1 ? 2000 : (itens[idx].falhas == 2 ? 10000 : 60000));
      SDL_UnlockMutex(mtx);
      continue;
    }
    SDL_LockMutex(mtx);
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) {
        itens[idx].estado = VAZIO;
        itens[idx].caminho[0] = 0;
      }
      SDL_UnlockMutex(mtx);
      continue;
    }
    paraDecode(idx);
    SDL_UnlockMutex(mtx);
  }
}

static int threadDecode(void *arg) {
  (void)arg;
  // PRIORIDADE BAIXA, e isto nao e detalhe.
  //
  // Medido: durante a navegacao o pior quadro cravava em 42 ms e os janks
  // batiam EXATAMENTE com `pend>0` — ou seja, com este fio trabalhando. Ele
  // decodifica JPEG e reduz a imagem com SDL_BlitScaled, tudo em CPU, e nesta
  // TV sao quatro nucleos fracos: com prioridade igual, ele rouba o quadro do
  // desenho. Arte que aparece um instante depois ninguem nota; o tranco, sim.
  SDL_SetThreadPriority(SDL_THREAD_PRIORITY_LOW);
  for (;;) {
    SDL_LockMutex(mtx);
    while (rodando && decIni == decFim) SDL_CondWait(condDec, mtx);
    if (!rodando) { SDL_UnlockMutex(mtx); return 0; }
    int idx = filaDec[decIni]; decIni = (decIni + 1) % MAX_FILA;
    SDL_CondSignal(condLivre);   // abriu lugar: solta um fio de rede que espera
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) {
        itens[idx].estado = VAZIO;
        itens[idx].caminho[0] = 0;
      }
      SDL_UnlockMutex(mtx);
      continue;
    }
    char caminho[512];
    int limite;
    strncpy(caminho, itens[idx].caminho, sizeof caminho - 1);
    caminho[sizeof caminho - 1] = 0;
    // Copiado SOB O MUTEX: o item pode ser promovido a hero enquanto este fio
    // decodifica, e ler o campo depois daria uma leitura sem trava.
    limite = itens[idx].limite > 0 ? itens[idx].limite : NV_TEX_LARG_MAX;
    SDL_UnlockMutex(mtx);

    // O download JA ACONTECEU no fio de rede; aqui garantirLocal so traduz a
    // URL para o caminho do cache, sem tocar a rede.
    { char local[600];
      if (garantirLocal(caminho, local, sizeof local))
        snprintf(caminho, sizeof caminho, "%s", local);
    }
    SDL_Surface *bruta = IMG_Load(caminho);
    SDL_Surface *conv = NULL;
    // O SDL2_image desta TV nao le WebP; a libwebp do sistema le (webp.c).
    if (!bruta) bruta = webp_carregar(caminho);
    if (bruta) {
      // REDUZ DIRETO DA BRUTA quando ela e maior que o teto, em vez de
      // converter em tamanho cheio e so depois reduzir.
      //
      // POR QUE ISTO IMPORTA, medido na TV Samsung: o caminho antigo mantinha
      // `bruta` e `conv` VIVAS AO MESMO TEMPO, as duas em tamanho cheio. Um
      // backdrop de 3840x2160 em ABGR8888 custa 33 MB, entao o pico era 66 MB
      // por imagem — e com NV_TEX_FIOS igual a 2, 132 MB de pico so em
      // decodificacao, sobre ~60 MB de uso estavel. Num heap FIXO de 256 MiB
      // (a TV aceita reservar, mas recusa crescer depois) isso terminou em
      // "Cannot enlarge memory arrays to size 295014400 bytes (OOM)" com a home
      // ja desenhada e 38 fps.
      //
      // O SDL_BlitScaled CONVERTE O FORMATO durante a copia, entao a copia
      // intermediaria em tamanho cheio nunca precisou existir: o destino ja
      // nasce com 640 de largura e no formato final. O pico cai de 2x para 1x
      // o tamanho da fonte.
      //
      // BLENDMODE_NONE e obrigatorio: o padrao para arte com alpha e BLEND, e
      // blitar em cima de uma superficie recem-criada (que e transparente)
      // multiplicaria a cor pelo alpha e escureceria a borda dos logos. NONE
      // copia o pixel como esta, que e o que uma conversao faz.
      if (bruta->w > limite) {
        int lw = limite;
        int lh = bruta->h * lw / bruta->w;
        SDL_Surface *menor = SDL_CreateRGBSurfaceWithFormat(
            0, lw, lh > 0 ? lh : 1, 32, SDL_PIXELFORMAT_ABGR8888);
        if (menor) {
          SDL_SetSurfaceBlendMode(bruta, SDL_BLENDMODE_NONE);
          // BlitScaled faz media dos vizinhos; um decimador ingenuo deixaria a
          // arte serrilhada.
          SDL_BlitScaled(bruta, NULL, menor, NULL);
          conv = menor;
        }
      }
      // Fonte ja pequena, ou a superficie reduzida nao pode ser criada: o
      // caminho antigo continua valendo.
      if (!conv) conv = SDL_ConvertSurfaceFormat(bruta, SDL_PIXELFORMAT_ABGR8888, 0);
      SDL_FreeSurface(bruta);
    }
#ifdef __EMSCRIPTEN__
    // O ARQUIVO DE CACHE MORRE AQUI, no alvo Tizen e so nele.
    //
    // MEDIDO NA TV: 99 MB de cache-disco so abrindo listas. E RAM — o "disco"
    // do Emscripten e MEMFS —, nada nunca era apagado, e esse consumo nao
    // aparecia em nenhum contador: o [mem] mede o heap do malloc e o total de
    // texturas mede a GPU. Somado ao heap FIXO de 256 MiB, e o que faz o app
    // engasgar e travar depois de alguma navegacao.
    //
    // O cache de disco existe para nao rebaixar a mesma arte quando a textura e
    // despejada. Aqui ele NAO PRECISA EXISTIR: o download passa pela pilha HTTP
    // do proprio navegador, que ja mantem cache em DISCO DE VERDADE, com cota
    // propria e despejo proprio, fora do nosso orcamento. Guardar o arquivo em
    // MEMFS estava duplicando o cache do Chromium dentro da nossa RAM.
    //
    // O arquivo continua sendo o ponto de encontro entre o fio de rede e o de
    // decode — so deixa de sobreviver a ele. Uma arte pedida de novo depois do
    // despejo volta a ser baixada, e essa volta e barata porque o navegador
    // serve do cache dele.
    //
    // So no Emscripten: no LG o cache e disco de verdade e apagar ali seria
    // trocar leitura local por rede.
    // APAGAR SO QUANDO PASSAR DO TETO, e nao sempre.
    //
    // A versao anterior apagava o arquivo depois de TODO decode, e isso foi
    // REGRESSAO: o cache de disco existe justamente para nao rebaixar a arte
    // quando a textura e despejada, e sem ele cada despejo virava um download
    // novo. Com o recuo de 2 s/10 s/60 s em falha, a arte sumia por muito
    // tempo — o dono viu a home carregando "muito lindo e rapido" no v10 e
    // quebrada depois. Trocar 200 MB de RAM por arte que nao aparece nao e
    // troca boa.
    //
    // O defeito real nunca foi guardar: era guardar SEM TETO. Agora guarda ate
    // NV_CACHE_DISCO_MAX e, passando disso, o mais novo nao fica — o conjunto
    // quente que ja esta em disco continua servindo.
    if (conv && cacheDiscoBytes > NV_CACHE_DISCO_MAX && noCache(caminho)) {
      long tam = 0;
      { FILE *g = fopen(caminho, "rb");
        if (g) { fseek(g, 0, SEEK_END); tam = ftell(g); fclose(g); } }
      if (remove(caminho) == 0) {
        cacheDiscoBytes -= tam;
        if (cacheDiscoBytes < 0) cacheDiscoBytes = 0;
      }
    }
#endif
    // TETO DE LARGURA. Antes a arte vinha do pacote ja reduzida; agora vem da
    // rede no tamanho que o servidor tiver, e um backdrop de 1920 custa 8 MB
    // DECODIFICADO — meia duzia deles estoura o orcamento e o cache passa a
    // despejar e rebaixar em circulo, o que aparece como queda de fps e quadros
    // de 100 ms. Reduzir aqui vale para qualquer fonte, presente ou futura.
    if (conv && conv->w > limite) {
      int lw = limite;
      int lh = conv->h * lw / conv->w;
      SDL_Surface *menor = SDL_CreateRGBSurfaceWithFormat(
          0, lw, lh, 32, SDL_PIXELFORMAT_ABGR8888);
      if (menor) {
        // BlitScaled faz media dos vizinhos; um decimador ingenuo deixaria a
        // arte serrilhada, que foi exatamente o defeito do fundo pixelado.
        SDL_BlitScaled(conv, NULL, menor, NULL);
        SDL_FreeSurface(conv);
        conv = menor;
      }
    }

    // MEDIDA DE LUMINANCIA, aqui e nao no desenho: esta thread ja tem os pixels
    // na mao e roda em prioridade baixa. Amostra de 4 em 4 nos dois eixos —
    // 1/16 dos pixels bastam para dizer se uma arte e escura, e a conta inteira
    // num logo de 700x271 seria trabalho sem retorno.
    int lumMedia = -1, cromaMedia = 0;
    if (conv && conv->format->BytesPerPixel == 4) {
      const unsigned char *px = (const unsigned char *)conv->pixels;
      long soma = 0, somaC = 0, n = 0;
      int yy, xx;
      for (yy = 0; yy < conv->h; yy += 4) {
        const unsigned char *ln = px + (size_t)yy * conv->pitch;
        for (xx = 0; xx < conv->w; xx += 4) {
          const unsigned char *q = ln + (size_t)xx * 4;   // ABGR8888: R,G,B,A
          if (q[3] < 200) continue;                       // so o que e opaco
          soma += (q[0] * 299 + q[1] * 587 + q[2] * 114) / 1000;
          { int mx = q[0] > q[1] ? q[0] : q[1]; if (q[2] > mx) mx = q[2];
            int mn = q[0] < q[1] ? q[0] : q[1]; if (q[2] < mn) mn = q[2];
            somaC += mx - mn; }
          n++;
        }
      }
      if (n > 0) { lumMedia = (int)(soma / n); cromaMedia = (int)(somaC / n); }
    }

    // A FALHA PRECISA APARECER. Sem log, uma imagem que nunca decodifica vira
    // um laco silencioso: o desenho pede todo quadro, o fio tenta todo quadro,
    // e o unico sintoma e "esse card nao tem arte". Foi assim que o WEBP do
    // metahub passou despercebido.
    int falhou = 0;
    SDL_LockMutex(mtx);
    if (itens[idx].estado == PENDENTE && pedidoObsoleto(&itens[idx])) {
      // A imagem terminou depois de o card sair da tela. Nao a publique e nao
      // a transforme em falha: outro card pode reutilizar o slot frio.
      itens[idx].estado = VAZIO;
      itens[idx].caminho[0] = 0;
      if (conv) { SDL_FreeSurface(conv); conv = NULL; }
    } else if (itens[idx].estado == PENDENTE) {
      itens[idx].lum = lumMedia;
      itens[idx].croma = cromaMedia;
      itens[idx].sup = conv;
      if (conv) {
        itens[idx].estado = DECODIFICADO;
        itens[idx].falhas = 0;
      } else {
        // MANTEM o caminho: e ele que identifica o slot na proxima consulta e
        // permite responder "ainda nao, tente depois" em vez de reenfileirar.
        // Zerar o caminho, como estava, apagava a memoria da falha junto.
        static const Uint32 RECUO[3] = { 2000, 10000, 60000 };
        int k = itens[idx].falhas;
        itens[idx].estado = FALHOU;
        itens[idx].falhas = k + 1;
        itens[idx].tentarEm = SDL_GetTicks() + RECUO[k < 3 ? k : 2];
        falhou = 1;
      }
    } else if (conv) {
      SDL_FreeSurface(conv);  // slot foi reaproveitado no meio do caminho
    }
    SDL_UnlockMutex(mtx);

    if (falhou) {
      // O TAMANHO E OS PRIMEIROS BYTES, e nao so a mensagem do SDL_image.
      //
      // "Unsupported image format" e o que ele responde para qualquer coisa que
      // nenhum leitor reconhece, e os casos possiveis pedem consertos
      // diferentes: arquivo de 0 byte (a gravacao nao aconteceu), arquivo curto
      // com assinatura boa (truncado — o caso conhecido do MEMFS sob pressao,
      // ver o comentario em garantirLocal), ou assinatura de HTML/JSON (o
      // servidor respondeu pagina de erro com status 200). Sem estes dois
      // numeros as tres coisas chegam ao log identicas, e foi por falta deles
      // que este defeito ja consumiu duas hipoteses erradas — webp (o metahub
      // entrega jpeg) e jpeg progressivo (o SDL_image do emcc decodifica).
      long tam = -1; unsigned char mag[4] = {0,0,0,0};
      { FILE *g = fopen(caminho, "rb");
        if (g) { fseek(g, 0, SEEK_END); tam = ftell(g); rewind(g);
                 if (fread(mag, 1, 4, g) != 4) { }
                 fclose(g); } }
      printf("[tex] decode falhou (%s) tam=%ld magica=%02x%02x%02x%02x: %.70s\n",
             IMG_GetError(), tam, mag[0], mag[1], mag[2], mag[3], caminho);
      // APAGA o arquivo que nao decodifica. Ele so pode ter chegado ao cache
      // corrompido — a assinatura foi conferida no download —, e mante-lo
      // significa que esta arte NUNCA mais carrega, nem depois de o problema
      // que a truncou passar. Apagando, o proximo pedido baixa de novo.
      if (noCache(caminho)) remove(caminho);
      fflush(stdout);
      // Arquivo LOCAL que nao decodifica esta envenenado: garantirLocal o
      // aceita para sempre por ter mais de 512 bytes, entao sem apagar aqui o
      // item nunca mais teria arte. Só apaga o que esta no NOSSO cache.
      if (dirCache[0] && !strncmp(caminho, dirCache, strlen(dirCache)))
        remove(caminho);
    }
  }
}

int tex_iniciar(int max_itens) {
  nMax = max_itens > 0 && max_itens <= MAX_ITENS_ABS ? max_itens : 64;
  // ORCAMENTO ESCALA COM O BUFFER. Os 96 MB sao a conta da TV, onde a escala e
  // 1. No Mac retina a escala e 2, e a MESMA cena precisa de 4x os pixels — com
  // o teto fixo a previa vivia encostada no limite, despejando arte visivel e
  // mostrando um defeito que o aparelho nao tem. Uma previa que mente e pior
  // que nao ter previa.
  //
  // Na TV o fator e 1 e nada muda; e exatamente por isso que a conta pode ser
  // esta e nao um numero maior cravado.
  { float e = escalaBuf > 0.1f ? escalaBuf : 1.0f;
    orcamento = (long)(NV_TEX_ORCAMENTO_MB * e * e) * 1024L * 1024L; }
  bytesUsados = 0;
  memset(itens, 0, sizeof itens);
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  rodando = 1;
  // DOIS fios de decode, nao um. A fila e retirada sob o mutex e cada fio leva
  // um indice proprio, entao mais consumidores e seguro sem outra mudanca.
  //
  // Um fio so era o limite real do "carrega enquanto passa": com 32 itens em
  // voo (o teto de slotLivre) e ~30 ms por imagem nesta TV, a fila levava um
  // segundo para escoar. Dois fios cortam isso pela metade.
  //
  // Nao mais que dois: sao quatro nucleos fracos, e os dois rodam em prioridade
  // BAIXA justamente para nao roubar o quadro do desenho — a nota acima, no
  // threadDecode, registra que com prioridade igual o tranco batia exatamente
  // com `pend>0`. Quatro fios competiriam com o desenho mesmo em prioridade
  // baixa.
  { int k;
    for (k = 0; k < NV_TEX_FIOS; k++)
      thrs[k] = SDL_CreateThread(threadDecode, "nv-decode", NULL);
    // QUATRO fios de REDE, e eles NAO sao como os de decode: nao tocam pixel,
    // so esperam I/O. Podem rodar em prioridade normal e em maior numero sem
    // competir com o desenho — o custo de um fio parado num socket e zero de
    // CPU. Quatro cobre os quatro cartazes que entram na tela de uma vez.
    for (k = 0; k < NV_TEX_FIOS_REDE; k++)
      thrsRede[k] = SDL_CreateThread(threadRede, "nv-rede", NULL);
    thr = thrs[0]; }
  return thr != NULL;
}

void tex_encerrar(void) {
  SDL_LockMutex(mtx); rodando = 0;
  SDL_CondBroadcast(cond); SDL_CondBroadcast(condDec);
  SDL_CondBroadcast(condLivre);
  SDL_UnlockMutex(mtx);
  // Espera os DOIS fios. Esperar so o primeiro deixava o outro decodificando
  // para dentro de itens[] enquanto o laco abaixo ja liberava as superficies.
  { int k;
    for (k = 0; k < NV_TEX_FIOS; k++)
      if (thrs[k]) { SDL_WaitThread(thrs[k], NULL); thrs[k] = NULL; }
    for (k = 0; k < NV_TEX_FIOS_REDE; k++)
      if (thrsRede[k]) { SDL_WaitThread(thrsRede[k], NULL); thrsRede[k] = NULL; }
    thr = NULL; }
  for (int i = 0; i < nMax; i++) {
    if (itens[i].tex) glDeleteTextures(1, &itens[i].tex);
    if (itens[i].sup) SDL_FreeSurface(itens[i].sup);
  }
  SDL_DestroyCond(cond); SDL_DestroyMutex(mtx);
}

static GLuint tex_obter_limite(const char *caminho, int limite) {
  if (!caminho || !*caminho) return 0;
  GLuint saida = 0;
  unsigned long h = hashCaminho(caminho);
  int i; BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].estado == FALHOU) {
    itens[i].ultimoQuadro = quadroAtual;
    itens[i].ultimoPedido = SDL_GetTicks();
    // Ja falhou: so volta para a fila quando o RECUO vencer, e no maximo
    // QUATRO vezes. Sem a espera o pedido voltava a cada quadro e a arte
    // quebrada tomava a frente da boa; sem o teto acontece coisa pior, e ela
    // foi MEDIDA na TV Samsung.
    //
    // A conta estava errada por um: a guarda era `falhas < 3` e o recuo e
    // RECUO[] = {2 s, 10 s, 60 s} indexado por `falhas` ANTES do incremento,
    // entao a terceira falha gravava tentarEm para 60 s adiante e a guarda ja
    // nao deixava aquele prazo ser usado — os 60 s eram codigo morto.
    //
    // EU CONSERTEI ISSO TIRANDO O TETO, E FOI PIOR. O teto nao existe para
    // limitar pedidos: e ele que devolve o slot. slotLivre() reaproveita, ANTES
    // de despejar arte que esta na tela, justamente o slot que ja desistiu
    // (`FALHOU && falhas >= 3`). Sem teto, um item que nunca decodifica volta
    // para PENDENTE para sempre, nunca satisfaz aquela condicao, e slotLivre cai
    // no despejo por LRU — passa a jogar fora textura BOA para dar lugar a arte
    // que nao vai decodificar nunca. Medido no painel da TV: pend=99,
    // despejos=218 numa janela de 3 s, FPS 54-57 com janks, e os MESMOS cinco
    // arquivos reaparecendo no log a cada 2 s (o recuo de 2 s de um item
    // recriado do zero, porque o despejo apaga a memoria da falha junto).
    //
    // `< 4` e nao `< 3`: quatro tentativas no total, que e o que a tabela de
    // recuo sempre descreveu. O prazo de 60 s deixa de ser codigo morto e o
    // slot volta a ser reciclavel.
    if (itens[i].falhas < 4 && SDL_GetTicks() >= itens[i].tentarEm) {
      int prox = (filaFim + 1) % MAX_FILA;
      if (prox != filaIni) {
        itens[i].estado = PENDENTE;
        itens[i].uso = ++relogio;
        fila[filaFim] = i; filaFim = prox; SDL_CondSignal(cond);
      }
    }
    SDL_UnlockMutex(mtx);
    return 0;
  }
  if (i >= 0) {
    itens[i].ultimoQuadro = quadroAtual;
    itens[i].ultimoPedido = SDL_GetTicks();
    itens[i].uso = ++relogio;
    // PROMOCAO: a mesma arte pode ser pedida como poster (960) e depois como
    // hero (1920). Se o teto novo e maior e a textura pronta ficou menor que
    // ele, refaz — senao o hero herda para sempre a versao pequena que o card
    // pediu primeiro, e o borrao volta sem explicacao aparente.
    if (limite > itens[i].limite) {
      int fonteMenor = (itens[i].estado == PRONTO && itens[i].w < itens[i].limite);
      itens[i].limite = limite;
      // `w < limite` NAO basta: um poster da Cinemeta tem 250px de origem, e
      // pedi-lo a 320 refaz o decode para devolver os mesmos 250 — trabalho
      // puro, mais o cinza enquanto refaz. Se a textura pronta ja e MENOR que o
      // teto que ela mesma tinha, a fonte acabou; nao ha o que ganhar.
      if (itens[i].estado == PRONTO && itens[i].w < limite && !fonteMenor) {
        int prox = (filaFim + 1) % MAX_FILA;
        if (prox != filaIni) {
          itens[i].estado = PENDENTE;
          fila[filaFim] = i; filaFim = prox; SDL_CondSignal(cond);
        }
      }
    }
    saida = itens[i].estado == PRONTO ? itens[i].tex : 0;
  } else {
    int novo = slotLivre();
    if (novo >= 0) {
      strncpy(itens[novo].caminho, caminho, sizeof itens[novo].caminho - 1);
      itens[novo].hash = h;
      itens[novo].limite = limite;
      itens[novo].estado = PENDENTE;
      itens[novo].uso = ++relogio;
      itens[novo].ultimoQuadro = quadroAtual;
      itens[novo].ultimoPedido = SDL_GetTicks();
      int prox = (filaFim + 1) % MAX_FILA;
      if (prox != filaIni) { fila[filaFim] = novo; filaFim = prox; SDL_CondSignal(cond); }
      else { itens[novo].estado = VAZIO; itens[novo].caminho[0] = 0; } // fila cheia
    }
  }
  SDL_UnlockMutex(mtx);
  return saida;
}

GLuint tex_obter(const char *caminho) {
  return tex_obter_limite(caminho, NV_TEX_LARG_MAX);
}

// Arte que ocupa a tela inteira: hero da home, backdrop do detalhe e a arte do
// player. 1920 e a largura do painel — pedir mais so gastaria memoria, pedir
// menos e ampliar depois.
GLuint tex_obter_larg(const char *caminho, float largLayout) {
  int cap;
  if (largLayout <= 1.0f) return tex_obter(caminho);
  cap = (int)(largLayout * escalaBuf * NV_TEX_FOLGA + 0.5f);
  // Arredonda para multiplo de 32: sem isso cada largura de desenho vira um
  // teto proprio, e a mesma arte pedida por dois lugares com poucos pixels de
  // diferenca era promovida e RE-DECODIFICADA sem ganho visivel.
  cap = ((cap + 31) / 32) * 32;
  if (cap < 128) cap = 128;
  if (cap > NV_TEX_HERO_LARG_MAX) cap = NV_TEX_HERO_LARG_MAX;
  return tex_obter_limite(caminho, cap);
}

GLuint tex_obter_hero(const char *caminho) {
  return tex_obter_limite(caminho, NV_TEX_HERO_LARG_MAX);
}

// O ARQUIVO, e nao a textura. Ver a nota em tex_cache.h.
//
// TUDO AQUI E REAPROVEITAMENTO, de proposito. O download, o nome estavel da
// URL, a gravacao atomica em .parcial, a conferencia de assinatura, o recuo em
// falha e a deduplicacao ja existem em garantirLocal e no fio de rede; escrever
// um segundo download ao lado deles seria repetir seis defeitos ja consertados.
// O unico gasto e a textura de UM quadro que o decode produz de brinde, pedida
// aqui com o menor teto possivel — o LRU a despeja como qualquer outra.
//
// LIMITE CONHECIDO, no alvo Tizen: passando de NV_CACHE_DISCO_MAX o fio de
// decode APAGA o arquivo do cache logo depois de decodificar (ver a nota longa
// la embaixo, sobre MEMFS). Quando isso acontece esta funcao nunca acha o
// arquivo e devolve NULL para sempre — o cartaz fica parado, que e o
// comportamento de hoje. Nao vira laco de download: o item ja esta PRONTO e o
// pedido abaixo so encosta no LRU.
const char *tex_arquivo(const char *url) {
  static char local[600];
  FILE *f;
  long n = 0;
  if (!url || !*url) return NULL;
  // Ja e arquivo: e o caso das pastas do pacote, cujo caminho vem do disco.
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) return url;
  if (!dirCache[0]) return NULL;
  nomeDeCache(url, local, sizeof local);
  f = fopen(local, "rb");
  if (f) { fseek(f, 0, SEEK_END); n = ftell(f); fclose(f); }
  // Mesmo piso de garantirLocal: abaixo disso e pagina de erro, nao arquivo.
  if (n > 512) return local;
  // 128 e o teto MINIMO que tex_obter_limite aceita pelo caminho normal; o que
  // interessa e o efeito colateral, que e o arquivo no disco.
  tex_obter_limite(url, 128);
  return NULL;
}

float tex_aspecto(const char *caminho) {
  if (!caminho || !*caminho) return 0.0f;
  float a = 0.0f;
  unsigned long h = hashCaminho(caminho);
  int i; BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].estado == PRONTO && itens[i].h > 0)
    a = (float)itens[i].w / (float)itens[i].h;
  SDL_UnlockMutex(mtx);
  return a;
}

int tex_marca_escura(const char *caminho) {
  int r = 0;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  // lum < 0 e "ainda nao medi": responde NAO, para nao tingir arte que ainda
  // vai chegar. Errar para o lado de nao mexer.
  if (i >= 0 && itens[i].estado == PRONTO && itens[i].lum >= 0)
    r = (itens[i].lum < NV_LOGO_LUM_MIN && itens[i].croma < NV_LOGO_CROMA_MAX);
  SDL_UnlockMutex(mtx);
  return r;
}

int tex_bombear(int max_por_quadro) {
  int subiu = 0;
  Uint64 inicio = SDL_GetPerformanceCounter();
  double freq = (double)SDL_GetPerformanceFrequency();
  for (int passo = 0; passo < max_por_quadro; passo++) {
    if (subiu > 0 &&
        (double)(SDL_GetPerformanceCounter() - inicio) * 1000.0 / freq >=
            NV_TEX_UPLOAD_BUDGET_MS)
      break;
    SDL_Surface *sup = NULL; int alvo = -1;
    SDL_LockMutex(mtx);
    for (int i = 0; i < nMax; i++) {
      if (itens[i].estado == DECODIFICADO && itens[i].sup) {
        sup = itens[i].sup; itens[i].sup = NULL; alvo = i; break;
      }
    }
    SDL_UnlockMutex(mtx);
    if (!sup) break;

    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sup->w, sup->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sup->pixels);
    // Mipmaps servem a duas coisas: reduzir o serrilhado quando a arte aparece
  // menor que o original, e — a razao de terem entrado agora — permitir o fundo
  // desfocado da pagina de detalhe. Em GLES2 nao ha blur barato; amostrar um
  // nivel pequeno da piramide com bias e o blur.
  // MIPMAP SO NO QUE ENCOLHE NA TELA. A piramide custa +33% de memoria de GPU
  // sobre a textura, e `bytesUsados` NAO a conta — com o orcamento em 96 MB, o
  // uso real chegava perto de 128 MB e o driver e que decidia o que despejar.
  //
  // Arte de tela cheia (heroi, backdrop) e desenhada 1:1 ou AMPLIADA: nivel
  // menor da piramide nunca e amostrado, entao ali a piramide e custo puro. Os
  // cards, sim, aparecem menores que o decodificado e precisam dela.
  //
  // Sem mipmap o filtro TEM de ser GL_LINEAR: com MIPMAP_NEAREST numa textura
  // sem piramide a amostragem e indefinida e a textura sai PRETA.
  // temPiramide() e a fonte unica desta decisao: a contabilidade de bytes usa a
  // mesma funcao, entao cobranca e geracao nunca divergem.
  int comMip = temPiramide(sup->w, sup->h);
#ifdef __EMSCRIPTEN__
  // WEBGL 1 RECUSA MIPMAP EM TEXTURA NAO-POTENCIA-DE-DOIS, e o preco e o card
  // PRETO. glGenerateMipmap numa NPOT devolve GL_INVALID_OPERATION e nao gera
  // piramide nenhuma; o MIN_FILTER logo abaixo continua pedindo
  // GL_LINEAR_MIPMAP_NEAREST, a textura fica INCOMPLETA, e textura incompleta
  // amostra preto. Sem erro em C, sem erro no log do app: a unica pista fica no
  // console do navegador, "GL_INVALID_OPERATION: glGenerateMipmap: The texture
  // is a non-power-of-two texture", que foi o dono quem abriu e viu.
  //
  // No webOS isto FUNCIONA porque o driver Mali expoe OES_texture_npot, que
  // levanta a restricao. O WebGL 1 nao expoe essa extensao de jeito nenhum.
  // Por isso a guarda e so do alvo Emscripten: mudar o comportamento no LG
  // custaria a piramide da arte NPOT de la, que hoje serve o desfoque da pagina
  // de detalhe e nao tem defeito nenhum.
  //
  // Coerente com o que o comentario acima ja avisava: "sem mipmap o filtro TEM
  // de ser GL_LINEAR". Consequencia aceita: no Tizen a arte NPOT nao tem
  // piramide, entao o desfoque da pagina de detalhe fica mais fraco nela.
  { int lp = (sup->w > 0) && ((sup->w & (sup->w - 1)) == 0);
    int ap = (sup->h > 0) && ((sup->h & (sup->h - 1)) == 0);
    if (!lp || !ap) comMip = 0; }
#endif
  if (comMip) glGenerateMipmap(GL_TEXTURE_2D);
  // MIPMAP_NEAREST e nao _LINEAR: o trilinear le DOIS niveis da piramide por
  // amostra, e nesta GPU isso e o dobro do custo de textura em cada pixel de
  // cada card. A diferenca visual e um degrau na transicao entre niveis, que
  // so apareceria numa animacao de zoom continuo — que o app nao faz.
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                  comMip ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gfx_tex_esquecer(0);  // o bind do upload passou por fora do gfx_rect

    SDL_LockMutex(mtx);
    // PROMOCAO VAZAVA. Quando a mesma arte e pedida com um teto maior (poster a
    // 288 e depois heroi a 1920), o item volta a PENDENTE e passa por aqui de
    // novo — e esta linha sobrescrevia `tex` sem apagar a textura antiga e
    // SOMAVA os bytes novos sem subtrair os velhos.
    //
    // Duas consequencias, as duas silenciosas: uma textura orfa ficava na GPU a
    // cada promocao, e `bytesUsados` so crescia com bytes-fantasma. Com o
    // orcamento inflado, podar() passava a despejar cada vez mais cedo — ate
    // despejar arte que estava na tela, um quadro depois de ela subir. Era mais
    // uma fonte de "poster que some".
    if (itens[alvo].tex) {
      gfx_tex_esquecer(itens[alvo].tex);
      glDeleteTextures(1, &itens[alvo].tex);
    bytesUsados -= bytesTextura(itens[alvo].w, itens[alvo].h);
      if (bytesUsados < 0) bytesUsados = 0;
    }
    itens[alvo].tex = t; itens[alvo].w = sup->w; itens[alvo].h = sup->h;
    itens[alvo].estado = PRONTO;
    bytesUsados += bytesTextura(sup->w, sup->h);
    podar();
    SDL_UnlockMutex(mtx);
    SDL_FreeSurface(sup);
    subiu++;
  }
  return subiu;
}

void tex_estatisticas(int *nItens, int *nPend, long *bytes) {
  int a=0, p=0; long b=0;
  SDL_LockMutex(mtx);
  for (int i = 0; i < nMax; i++) {
    if (itens[i].estado == PRONTO) { a++; b += bytesTextura(itens[i].w, itens[i].h); }
    else if (itens[i].estado != VAZIO) p++;
  }
  SDL_UnlockMutex(mtx);
  if (nItens) *nItens = a;
  if (nPend) *nPend = p;
  if (bytes) *bytes = b;
}

long tex_cache_disco_bytes(void) { return cacheDiscoBytes; }
