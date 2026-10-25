#include "tex_cache.h"
#include "marco.h"
#include "gif.h"
#include <dirent.h>
#include <sys/stat.h>
#include <utime.h>
#include <errno.h>
#include <time.h>
#include <math.h>
#ifndef __EMSCRIPTEN__
#include <pthread.h>
#include <sys/statvfs.h>
#include <sys/resource.h>
#include "cachedisco.h"
#ifdef __linux__
#include <sys/syscall.h>
#endif
static void podarSePreciso(long entrada, int forcar);
static void iniciarGravador(void);
#endif
#include "sdlcompat.h"
#ifdef NV_TEX_TEST_AFTER_POP
extern void NV_TEX_TEST_AFTER_POP(void);
#endif
#ifdef NV_TEX_TEST_BEFORE_DISK_LOCK
extern void NV_TEX_TEST_BEFORE_DISK_LOCK(void);
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <pthread.h>
#include "dados.h"
static void arqDiscoRegistrar(const char *dst);
static int arqDiscoTem(const char *dst);
#endif
#include "rede.h"
#include "gfx.h"
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include "webp.h"
#include "jpegrapido.h"
#include "corviva.h"
#include "artereserva.h"
#include "artetamanho.h"
#include "perfiltv.h"
#include "posterprov.h"
#include <stdint.h>
#include "cachearte.h"
#include "artefalta.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ITENS_ABS 512
#define MAX_FILA 128
#define NV_TEX_STALE_FRAMES 8
#define NV_TEX_STALE_MS 200
#ifndef NV_TEX_UPLOAD_BUDGET_MS
#define NV_TEX_UPLOAD_BUDGET_MS (cachePerfil().uploadMs)
#endif
// Telemetria curta de uma sessao: so registra esperas que ja sao visiveis para
// quem navega. O identificador e FNV do caminho; nenhuma URL entra no trace.
#define NV_TEX_TRACE_MS 250
#define NV_TEX_TRACE_UPLOAD_MS 16
typedef struct {
  Uint32 netMs;       // somente rede_baixar_bin, incluindo fallback(s)
  Uint32 resolveMs;   // arte_reserva_url (/find) antes de um fallback
  Uint32 cacheMs;     // leitura/lock do cache antes de baixar
  Uint32 persistMs;   // LG: so enfileirar a gravacao (fila de fundo); Tizen: escrita/rename
  unsigned int netCalls;
} TexFetchTrace;
// Teto de decodificacao das artes de CARD; a nota que justifica o 640 esta
// mais abaixo, junto do teto do heroi.
#ifdef NV_TIZEN3_LITE
#define NV_TEX_LARG_MAX 320
#define NV_TEX_HERO_LARG_MAX 640
#else
#define NV_TEX_LARG_MAX 640
// Teto do heroi; a nota com as medidas (LG 1920, Tizen 1280) esta mais abaixo.
#ifdef __EMSCRIPTEN__
#define NV_TEX_HERO_LARG_MAX 1280
#else
#define NV_TEX_HERO_LARG_MAX 1920
#endif
#endif

// FALHOU e um estado de verdade, nao a ausencia de um. Sem ele, um caminho que
// nao decodifica volta a VAZIO, o desenho pede de novo no quadro seguinte e o
// ciclo nao termina nunca: o item ocupa uma vaga em voo e um turno na fila a
// frente das imagens boas, para falhar de novo. Com recuo, a segunda tentativa
// so acontece daqui a 2 s, a terceira a 10 s, e depois desiste na sessao.
typedef enum { VAZIO=0, PENDENTE, DECODIFICADO, PRONTO, FALHOU } Estado;

typedef struct {
  char caminho[NV_TEX_URL_MAX];
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
  // Ja foi pedido abaixo de NV_TEX_MIP_LARG, isto e, desenhado como CARD. A
  // promocao a heroi refaz a textura a 1280/1920; sem piramide, o card que
  // divide o arquivo com o heroi trocava de cara ao foco (o "pisca nas artes"
  // da C9, tests/pisca3_shot.sh arte-promo).
  int card;
  // A piramide da textura ATUAL, decidida no envio: a devolucao dos bytes usa
  // ela, e nao uma decisao refeita depois que `card` mudou.
  int mip;
  // O teto com que a TEXTURA ATUAL foi de fato decodificada, e a largura da
  // FONTE que a produziu.
  //
  // `limite` sozinho nao servia para decidir promocao: ele e escrito no PEDIDO
  // e a re-decodificacao pode nao acontecer (a fila de decode pode estar cheia
  // no instante do pedido, ou o item ja estar sendo decodificado com o teto
  // antigo). Quando isso acontecia, `limite` ficava em 1920 com a textura ainda
  // em 544 e a condicao `limite > itens[i].limite` NUNCA mais era verdadeira:
  // o heroi herdava a miniatura do cartaz pelo resto da sessao, esticada 3,5x.
  // Com o teto REALMENTE USADO guardado a parte, o proximo pedido tenta de
  // novo sozinho, sem ninguem precisar notar.
  int tetoUsado;
  int fonteW;
  // TETO COM QUE A VARIANTE MENOR FOI ESCOLHIDA (artetamanho.h), 0 quando o
  // download foi da URL do item. Escrito pelo fio de rede, lido pelo decode:
  // com ele o decode acha o arquivo da variante no disco, grava `tetoUsado`
  // com o teto da variante e `fonteW` desconhecido — o w780 de um card nao e
  // "a fonte acabou", e a promocao a heroi precisa continuar possivel.
  int limiteTamanho;
  // TETO DA VARIANTE A BAIXAR quando o pedido e de ARQUIVO (tex_arquivo, o
  // aquecimento do destaque), 0 nos outros. O aquecimento pede textura de 128
  // px — e o fio de rede escolhia a variante pelo `limite` do item NAQUELE
  // instante: w300 do TMDB, background/small do metahub, w780 do still. O
  // pedido do destaque, no mesmo quadro, subia o limite para 1920 tarde
  // demais: os bytes pequenos chegavam (~500 ms), o decode publicava a 128,
  // a promocao ia a rede DE NOVO pela variante grande (~1 s) e o prazo de
  // 600 ms estourava — `pedido` duas vezes para o mesmo hash no log da TCL
  // (07/10). Com isto o arquivo aquecido ja e o do destaque; o decode segue
  // no teto do pedido (128, ou 1920 se o destaque chegou antes).
  int limiteArquivo;
  unsigned long uso;  // contador LRU
  // Luminancia media dos pixels OPACOS, 0..255; -1 enquanto nao se sabe.
  // Medida uma vez, na thread de decode. Serve ao logo do titulo: o TMDB nao
  // marca claro/escuro em lugar nenhum (o ranking do proprio app web e so
  // idioma + vote_average), entao a unica forma de saber se um logo e preto e
  // OLHAR os pixels.
  int lum;
  // 1 = TODO pixel tem alfa 255 (conferido inteiro na thread de decode); 0 =
  // ha transparencia ou nao se sabe. Ver tex_opaca.
  int opaca;
  unsigned char cores[16*16*4]; // Spatial cover samples, 1 KiB per cache slot.
  CorvivaPaleta paleta; // Ja extraida no decode; retratos publicam sob demanda.
  // Quando tentar de novo (ticks) e quantas vezes ja falhou. Ver o enum Estado.
  Uint32 tentarEm;
  int    falhas;
  unsigned long ultimoQuadro;
  Uint32 ultimoPedido;
  // Marcos de fila para separar espera de rede/decode/upload do tempo de CPU.
  // So sao consumidos pela telemetria; a politica do cache nao depende deles.
  Uint32 filaRedeEm, filaDecEm, filaUploadEm;
  // Croma medio: max(R,G,B) - min(R,G,B) dos mesmos pixels opacos. Separa logo
  // PRETO (acromatico, variante errada do TMDB) de logo de MARCA escuro mas
  // colorido (vermelho, vinho), que deve passar intacto.
  int croma;
  // DESVIO da luminancia dos pixels opacos, MAIS UM (0 = ainda nao medido; o
  // memset do slot zera tudo e zero nao pode ser "tom unico"). Separa a MARCA
  // de uma cor so (silhueta branca, preta, vermelha) do logo em AZULEJO — um
  // quadrado arredondado claro com a marca escura dentro, com a borda
  // transparente. Os dois tem borda transparente; so este numero os distingue,
  // e tingir o segundo como marca pinta um QUADRADO CHAPADO (o "logo branco"
  // do guia na C9, 25/09/2026). Ver tex_logo_tom_unico.
  int desvio1;
  // COR DE FUNDO da arte: a media da BORDA quando ela e opaca (logo com
  // fundo proprio), -2 quando a borda e transparente (logo recortado), -1
  // enquanto nao se sabe. Ver tex_cor_fundo.
  int corR, corG, corB;
  // FURA A FILA. Arte que OCUPA A TELA — o hero da home, o fundo do detalhe —
  // é uma só, é a que a pessoa está olhando, e ela entrava na mesma fila FIFO
  // dos pôsteres da fileira. MEDIDO no aparelho do @rawldon (#55): `pend=17`
  // com o cache no teto, e `[hero] arte atrasada chegou em 12249 ms` contra um
  // prazo de 400. Doze segundos de fundo vazio porque dezessete miniaturas
  // estavam na frente.
  //
  // Vira 0 de novo quando o item conclui (PRONTO ou FALHOU): assim "urgente"
  // quer dizer "pedido como tela cheia e ainda devendo", e não "já foi hero um
  // dia" — senão, depois de algumas telas, metade da fila seria urgente e a
  // preferência voltaria a ser FIFO entre elas.
  int urgente;
  // Caminho do pacote ou absoluto local: nao passa pelo fio de rede. Quando a
  // fila de decode esta cheia, o pedido fica PENDENTE e o proximo quadro tenta
  // de novo; a thread de desenho nunca espera condLivre.
  int localDireto;
  int naFilaDec;
  // ARTE DE PASSAGEM: quadro de sequencia animada, que vale por 67 ms e nunca
  // mais. Ver tex_obter_passageira e a nota em despejar().
  int passageiro;
  int lookahead; // pedido especulativo limitado; nao expira antes do decode
  int fileiraDistancia;
  unsigned long fileiraQuadro;
  // OS BYTES BAIXADOS, no lugar do arquivo de cache (20/09/2026, #72). No
  // Emscripten toda chamada de arquivo feita por um pthread e PROXIADA ao fio
  // principal — fopen/fwrite/rename/fread/remove de cada arte eram viagens
  // sincronas pelo fio que desenha, e o log do AU7000 mostrava esse fio preso
  // fora do nosso codigo (FPS=2 com pior=0) enquanto 15 fios trabalhavam. O
  // fio de rede deixa os bytes aqui; o de decode os consome e libera. Nada
  // sobrevive ao decode: quem serve a re-decodificacao e o cache HTTP do
  // proprio navegador, que ja existia. So o GIF continua indo a arquivo,
  // porque gif.c le por caminho.
  //
  // NO LG TAMBEM, desde 22/09/2026: o fio de rede deixa o corpo aqui e o decode
  // o consome da memoria; o arquivo de cache e escrito depois, pela fila de
  // gravacao (filaGrav). Antes o decode so via o arquivo, entao a entrega
  // esperava fopen/fwrite/rename e as varreduras da pasta sob discoMtx —
  // medido na C9: net_ms=397 e persist_ms=4759 no mesmo fetch.
  unsigned char *bruto;
  long nBruto;
  int varianteCache;
  char urlCache[NV_TEX_URL_MAX];
  // OS 4 PRIMEIROS BYTES do que a rede entregou (#141), e se ja se sabe.
  // Quem pergunta e o cartaz de colecao: "a capa e um GIF?" (para animar a
  // capa quando a conta nao mandou focusGifUrl) e "o GIF de foco veio GIF
  // mesmo?". No Tizen so GIF vai a arquivo — o resto vive em `bruto` e some
  // no decode —, entao sem guardar isto aqui nao ha arquivo onde conferir.
  unsigned char magica[4];
  int temMagica;
  // tex_arquivo ja pediu este GIF DE NOVO uma vez porque o arquivo sumiu do
  // disco (poda). Uma vez por item: e a guarda contra laco de download.
  int rebaixouGif;
} Item;

// Teto pelo qual o fio de rede (e discoDireto) escolhe a VARIANTE: o maior
// entre o do decode e o do arquivo pedido. Chamado com o mutex tomado.
static int limiteDaVariante(const Item *it) {
  return it->limiteArquivo > it->limite ? it->limiteArquivo : it->limite;
}

#define NV_TEX_FIOS 2
// Fios de rede: 4 no LG; 2 no Tizen, onde o fetch de cada um passa pelo fio
// principal (ver Item.bruto e a nota de ADD_FIOS em addons.c).
#ifdef __EMSCRIPTEN__
#define NV_TEX_FIOS_REDE 2
#else
#define NV_TEX_FIOS_REDE 4
#endif
static SDL_Thread *thrs[NV_TEX_FIOS];
static SDL_Thread *thrsRede[NV_TEX_FIOS_REDE];
// FIOS DE REDE ATIVOS, ajustaveis em execucao (tex_definir_fios_rede). Os
// NV_TEX_FIOS_REDE sao criados no arranque e o excedente ESPERA na condicao:
// criar e destruir fio ao vivo exigiria juntar um fio que pode estar no meio
// de um download de 6 s. Parado na condicao ele custa zero de CPU. O numero
// inicial sai de ptv_padrao (perfiltv.c): 2 na LG de 1 GB, o maximo nas outras.
static int fiosRedeAtivos = NV_TEX_FIOS_REDE;
static int fiosRedeCriados = 0;
static SDL_cond *cond;
// Com fios parados na condicao, um Signal pode acordar justamente um deles, que
// volta a dormir e o pedido fica na fila sem ninguem. Broadcast so nesse caso.
static void acordarRede(void) {
  if (fiosRedeAtivos < fiosRedeCriados) SDL_CondBroadcast(cond);
  else SDL_CondSignal(cond);
}
static Item itens[MAX_ITENS_ABS];
// Envio em faixas em curso (ver tex_bombear): o slot, a textura reservada, a
// superficie (que continua em itens[uplIdx].sup) e a proxima linha.
static int uplIdx = -1;
static GLuint uplTex;
static SDL_Surface *uplSup;
static int uplY;
// 1 = os niveis da piramide da CPU ja tiveram o passo deles (ver tex_bombear).
static int uplNiveis;
// Libera a superficie de um item com os niveis da piramide da CPU (abaixo).
static void liberarSup(SDL_Surface *s);
// Bytes baixados que ainda nao foram consumidos pelo decode (LG e Tizen).
static void soltarBruto(Item *it) {
  free(it->bruto); it->bruto = NULL; it->nBruto = 0; it->urlCache[0] = 0;
}
static int nMax = 64;
static unsigned long relogio = 1;

static SDL_mutex *mtx;
#ifndef __EMSCRIPTEN__
// O gravador de disco vive ate o processo sair. Seu callback de poda precisa
// terminar antes de mtx/itens serem destruidos e observar a proxima
// inicializacao completa. Ordem: vidaMtx, depois mtx; nunca o inverso.
static pthread_mutex_t vidaMtx = PTHREAD_MUTEX_INITIALIZER;
#endif
static SDL_Thread *thr;
static int rodando = 0;

// Quanto de memoria as texturas PRONTAS ocupam. Sem esta conta o cache so
// despejava quando FALTAVA SLOT — e com 96 slots de arte 1920x1080 isso da 800
// MB. Com arte de verdade o app chegou a 104 MB e morreu com "double free or
// corruption" dentro do SDL: era falta de memoria, nao bug de ponteiro.
static long bytesUsados = 0;
static long orcamento = 0;
static long decodeReservado;
static SDL_threadID uploadThread;
static SDL_GLContext uploadContext;
static long orcMemTotal;
static PtvCache cachePerfil(void) { return ptv_cache(ptv_plataforma(), orcMemTotal); }

/* Reserve before decode, including codec output, conversion and CPU mips.
 * Completed surfaces remain charged until the frame pump releases them. */
static int decodeCabe(void) {
  long bytes = decodeReservado;
  for (int i = 0; i < nMax; i++)
    for (SDL_Surface *s = itens[i].sup; s; s = s->userdata)
      bytes += (long)s->pitch * s->h;
  PtvCache c = cachePerfil();
  return bytes + (long)c.reservaDecodeMb * 1048576 <= (long)c.decodeMb * 1048576;
}
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
  size_t n = strlen(dirCache);
  // /cache-pacote nao pertence a /cache: a limpeza de decode nao pode
  // apagar arte local so porque o nome da pasta compartilha o prefixo.
  return n && caminho && !strncmp(caminho, dirCache, n) &&
         (dirCache[n - 1] == '/' || caminho[n] == '/');
}
// Bytes gravados no cache de disco. No alvo Tizen "disco" e MEMFS, ou seja RAM
// (o log mostra idbfs=0/0.0ms), e nada nunca e apagado — cada arte baixada fica
// na memoria pelo resto da sessao. Isso nao aparecia em lugar nenhum: nem no
// [mem], que mede o heap do malloc, nem no total de texturas, que mede a GPU.
// Sem este numero nao da para dizer se a travada depois de muito uso e o cache
// de disco crescendo ou outra coisa.
static long cacheDiscoBytes = 0;
// O relatorio de FPS roda na thread de desenho. O caminho de rede pode estar
// varrendo/podando centenas de arquivos sob discoMtx; o relatorio recebe a
// ultima amostra publicada em vez de parar esperando I/O de outra thread.
static long cacheDiscoSnapshot = 0;
/* No LG quem publica e publicarDiscoNativo (bytes do indice); esta fica para
 * o Tizen e para tests/texdisco.c. */
__attribute__((unused)) static void publicarCacheDisco(void) {
  __atomic_store_n(&cacheDiscoSnapshot, cacheDiscoBytes, __ATOMIC_RELEASE);
}
// TETO DO CACHE DE DISCO, so no alvo Tizen.
//
// CORRECAO DE 17/09, e ela importa porque este comentario me levou a um erro:
// a pasta do cache NAO e MEMFS. Medido na QN85Q70AAGXZD pelo inspector,
// `FS.lookupPath('/nuvio/cache').node.mount` responde **IDBFS** — IndexedDB,
// armazenamento de verdade, com cota propria, FORA do heap de 256 MiB. Os
// "280 MB de RAM" que justificavam este teto eram de outra pasta, ou de antes
// de /nuvio existir; a arte nunca competiu com o heap. O proprio texto antigo
// admitia que 48 MB "NAO e numero medido".
//
// O teto continua existindo porque IndexedDB tambem tem cota, e estourar a dela
// faz a gravacao FALHAR — o sintoma seria "card sem arte", que e pior que arte
// que demora. Mas o numero segue sem medicao, e por isso o que foi consertado
// aqui foi a POLITICA, que estava errada independente do numero: ver a nota em
// podarCacheDisco.
#define NV_CACHE_DISCO_MAX ((long)cachePerfil().discoMb * 1048576)
#ifndef __EMSCRIPTEN__
#define NV_CACHE_DISCO_RESERVA (128UL * 1024UL * 1024UL)
// TETO PROPORCIONAL AO ESPACO, no LG (22/09/2026). 512 MB fixos era mais da
// metade do que a C9 tinha: 497 MB de arte em 3309 arquivos numa particao de
// 4,2 GB 82% cheia, com 772 MB livres. O teto efetivo e o menor entre os 512 MB
// e 15% do espaco que o cache PODERIA ocupar (livre + o que ele ja ocupa) —
// somar o proprio cache evita o teto encolher conforme ele cresce e oscilar.
// Na C9 daquele log: 15% de (772 + 497) = ~190 MB. Piso de 32 MB para uma TV
// quase cheia ainda guardar a home; a reserva de 128 MB livres continua valendo
// por cima disso e manda podar antes de o sistema ficar sem espaco.
#define NV_CACHE_DISCO_FRACAO 15
#define NV_CACHE_DISCO_PISO (32L * 1024L * 1024L)
#endif
static unsigned long quadroAtual = 1;

// ITEM 4 DA PESQUISA PLX: recusa no ponto do pedido.
//
// O pedido e o ULTIMO ponto onde ainda da para recusar de graca: rate-limit
// depois do decode e "um em voo" foram medidos por eles e ficaram PIORES que
// nada ("pacing nao torna o trabalho acessivel"). A grade declara aqui, uma vez
// por quadro, se esta em movimento, e cada desenho de cartao marca se aquela
// arte esta FORA da janela visivel. O cache entao recusa, antes de enfileirar
// rede ou decode, o pedido NOVO de uma arte fora da janela.
//
// DOIS NIVEIS, porque o risco de tile escuro nao e o mesmo nos dois casos:
//   1 = cartao NA BORDA (fileira cortada, ainda desenhada no fade): so recusa
//       em MOVIMENTO. Parado, o vizinho da borda e atendido — se recusasse,
//       ele ficaria escuro para sempre, porque parado nao volta a rolar.
//   2 = cartao TOTALMENTE fora da tela: recusa SEMPRE (em movimento ou parado),
//       porque nao ha tile nenhum na tela para ficar escuro — e pedi-lo parado
//       e justamente o trabalho especulativo que o lookahead do item 1 limita.
// Arte ja conhecida (pronta, em voo ou aquecida) NUNCA e recusada: e o que
// evita o tile escuro que eles mediram com a recusa fixa.
static int gradeMovendo;
static float gradeRolDir;
static int pedidoFora;
int tex_n_recusa = 0;

void tex_grade_movendo(int movendo) { gradeMovendo = movendo ? 1 : 0; }
// RolDir em PESSOAS POR QUADRO (card ou fileira), com sinal: qual lado da
// janela recebe a arte nova. O chamador decide quem entra; o cache so usa o
// sinal para marcar a arte que entra como passageira quando ela e despejada.
void tex_grade_velocidade(float passos) { gradeRolDir = passos; }
float tex_grade_direcao(void) { return gradeRolDir; }
void tex_pedido_fora(int nivel) { pedidoFora = nivel < 0 ? 0 : nivel; }

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
#define NV_TEX_MIP_LARG 1024
static int temPiramide(int w, int h, int card) {
  if (w >= NV_TEX_MIP_LARG && !card) return 0;
#ifdef __EMSCRIPTEN__
  { int lp = (w > 0) && ((w & (w - 1)) == 0);
    int ap = (h > 0) && ((h & (h - 1)) == 0);
    if (!lp || !ap) return 0; }
#endif
  return 1;
}

static long bytesTextura(const Item *it) {
  return bytesTexturaMip(it->w, it->h, it->mip);
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

// Tira o proximo da fila circular, com o URGENTE furando a ordem. Chamado com
// o mutex tomado.
//
// A varredura e linear sobre no maximo MAX_FILA (128) inteiros e acontece uma
// vez por item retirado, nao por quadro. Reordenar a fila inteira custaria o
// mesmo e perderia a ordem de chegada dos demais, que e o que faz a fileira
// aparecer da esquerda para a direita em vez de embaralhada.
static int quente(const Item *it);
static int tirarFila(int *f, int *ini, int fim) {
  int p = *ini, achou = -1, idx, nivel = -1;
  // NIVEL 2 = LOGO DE TITULO, 1 = tela cheia, 0 = o resto. O primeiro de maior
  // nivel vence; empate fica na ordem de chegada.
  while (p != fim) {
    int prioridade = quente(&itens[f[p]]) * 3 + itens[f[p]].urgente;
    if (prioridade > nivel) { achou = p; nivel = prioridade; }
    p = (p + 1) % MAX_FILA;
  }
  if (achou < 0) {
    idx = f[*ini]; *ini = (*ini + 1) % MAX_FILA; return idx;
  }
  idx = f[achou];
  // Fecha o buraco puxando quem estava ANTES dele para a frente: os outros
  // mantem a ordem relativa e so o urgente muda de lugar.
  while (achou != *ini) {
    int ant = (achou - 1 + MAX_FILA) % MAX_FILA;
    f[achou] = f[ant];
    achou = ant;
  }
  *ini = (*ini + 1) % MAX_FILA;
  return idx;
}

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
      itens[idx].naFilaDec = 1;
      itens[idx].filaDecEm = SDL_GetTicks();
      SDL_CondSignal(condDec);
      return;
    }
    if (!rodando) return;
    SDL_CondWait(condLivre, mtx);
  }
}

// Chamado com mtx tomado pela thread de desenho. Caminhos locais nao podem
// esperar a fila de decode: se ela estiver cheia, ficam PENDENTE sem item na
// fila e a proxima chamada tex_obter_* tenta novamente.
static int enfileirarDecodeSemEspera(int idx) {
  int prox = (decFim + 1) % MAX_FILA;
  if (!rodando || prox == decIni) return 0;
  filaDec[decFim] = idx;
  decFim = prox;
  itens[idx].naFilaDec = 1;
  itens[idx].filaDecEm = SDL_GetTicks();
  SDL_CondSignal(condDec);
  return 1;
}

static int caminhoLocal(const char *caminho) {
  return strncmp(caminho, "http://", 7) && strncmp(caminho, "https://", 8);
}

// FNV-1a do caminho. A busca abaixo roda para cada card visivel em cada
// quadro, contra ate 96 slots; comparar um inteiro primeiro reduz o strcmp a
// so os candidatos com o mesmo hash (na pratica, o proprio item).
static unsigned long hashCaminho(const char *s) {
  unsigned long h = 2166136261UL;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}

unsigned long tex_hash_public(const char *caminho) {
  return caminho && *caminho ? hashCaminho(caminho) : 0;
}

int    tex_n_busca = 0;
unsigned tex_n_falta = 0;
double tex_ms_busca = 0.0;
int    tex_despejos = 0;
int    tex_despejos_quentes = 0;
static double texFreqMs = 0.0;

// QUENTE = pedido neste quadro ou no anterior, ou seja, ESTA NA TELA. E a
// unica arte que despejar produz sintoma visivel: ela some, o desenho pede de
// novo no quadro seguinte, o decode refaz, ela volta — e o despejo escolhe
// outra da mesma tela. Esse ciclo e o "pisca" — cada volta custa 2 quadros de
// placeholder cinza e ~30 ms de CPU, e nunca termina enquanto o conjunto na
// tela nao couber.
//
// Chamado com o mutex tomado. quadroAtual avanca em tex_novo_quadro, que roda
// DEPOIS de tex_bombear e ANTES de app_desenhar: dentro do bombear o quadro
// que acabou de ser desenhado ainda e `quadroAtual`, dentro do desenho ele e
// `quadroAtual - 1`. Aceitar os dois cobre as duas fases.
static int quente(const Item *it) {
  return it->ultimoQuadro && quadroAtual - it->ultimoQuadro <= 1;
}

// DESFAZ UM PEDIDO EM VOO sem perder o que ja havia.
//
// Um item PENDENTE pode ter `tex` valida: e a PROMOCAO (poster pedido depois
// como hero, ou card que abriu e pediu largura maior), em que o slot volta
// para a fila mantendo a textura pequena. Os fios de rede e decode, ao
// desistirem de um pedido obsoleto, escreviam VAZIO e zeravam o caminho — e a
// textura antiga ficava orfa na GPU, com os bytes dela ainda somados em
// `bytesUsados`. Nem glDeleteTextures resolveria: nao ha contexto GL nos fios.
//
// Se ha textura, o item VOLTA A SER PRONTO com ela — a promocao so nao
// aconteceu, e a versao pequena continua servindo. `limite` fica no valor
// promovido de proposito: com `w < limite` o proximo pedido cai em
// `fonteMenor` e nao refaz a promocao a cada quadro.
//
// OS BYTES BAIXADOS SAEM JUNTO (24/09/2026, cards com a arte de OUTRO titulo
// na C9: "Resident Evil" com o logo de "Searching", "Hokum" com o de
// "Missing"). O fio de rede deixa o corpo em Item.bruto e o decode o consome;
// se o decode desiste antes (pedido velho: o painel rolou), o slot ficava
// VAZIO com o logo ainda em `bruto`. O proximo caminho a ocupar o slot era um
// cartaz que ja estava no disco — o acerto de disco devolve sem tocar em
// `bruto` —, e o decode, que prefere os bytes ao arquivo, publicava o logo com
// o nome do cartaz. A guarda de caminho do decode nao pega: o caminho do item
// e mesmo o do cartaz. tests/texbruto.c refaz a sequencia.
static void desistir(int idx) {
  soltarBruto(&itens[idx]);
  itens[idx].filaRedeEm = 0;
  itens[idx].filaDecEm = 0;
  itens[idx].filaUploadEm = 0;
  if (itens[idx].tex) {
    itens[idx].estado = PRONTO;
    itens[idx].urgente = 0;
    // A PROMOCAO FOI TENTADA E NAO DEU: nao se tenta de novo a cada quadro.
    //
    // `tetoUsado` normalmente so muda quando um decode termina — e e isso que
    // faz o pedido do proximo quadro insistir quando a fila estava cheia. Aqui
    // e o caso oposto: a re-decodificacao ACONTECEU e nao pode ser publicada
    // (pedido obsoleto, sem slot, sem memoria). Insistir seria decodificar em
    // laco para jogar fora em laco. A versao pequena continua servindo, e o
    // proximo pedido com teto AINDA MAIOR volta a tentar.
    itens[idx].tetoUsado = itens[idx].limite;
    return;
  }
  itens[idx].estado = VAZIO;
  itens[idx].caminho[0] = 0;
}

static void amostrar(void);
// PEDIDO VELHO: ninguem desenhou este item por 8 quadros E 200 ms. Serve para o
// cartaz que saiu da tela enquanto o decode acontecia — publicar aquilo seria
// gastar vaga e memoria com arte que ninguem vai ver.
//
// A ARTE JA DECODIFICADA NAO CAI NESTA REGRA, e isto foi medido na C9: um still
// de episodio em 3840x2160 leva 1,6 a 2,0 s para decodificar, e a espera do
// destaque e de 400 ms (NV_HERO_ESPERA_MS). Ou seja, o desenho desistia ANTES
// de o decode terminar, o resultado era descartado por "pedido velho", e na
// volta do carrossel a MESMA arte era decodificada de novo — duas vezes dois
// segundos, para nada. O log mostrava toda arte pesada decodificada em dobro.
//
// Quem ja pagou o decode publica. Guardar uma superficie pronta custa uma vaga
// no cache, e o LRU sabe despejar; jogar fora custa o decode inteiro outra vez.
static int pedidoObsoleto(const Item *it) {
  Uint32 agora;
  // Only ultimoQuadro is sentinel: SDL_GetTicks may return 0 at startup, and a
  // real request at tick zero was treated as never requested (flaky abandon).
  if (!it->ultimoQuadro) return 0;
  if (it->sup || it->lookahead) return 0;   /* ja decodificado: nao se joga fora o que custou */
  agora = SDL_GetTicks();
  return quadroAtual > it->ultimoQuadro + NV_TEX_STALE_FRAMES &&
         (it->ultimoPedido > agora ||
          agora - it->ultimoPedido >= NV_TEX_STALE_MS);
}

static void podar(void);
static void soltarCpu(int falhas);
static int pressaoAtiva(void);
static int pressaoSevera(void);
static volatile int pressaoPlay = 100;   // tex_pressao_reproducao (abaixo)
static int pressaoPlayLigada;
// webOS/.tpk: faixa do piso de reproducao, alimentada pelo relatorio de 3 s
// (tex_memoria_livre). 100 = memoria de sobra, sem piso. O Android nao a usa:
// la o piso e fixo por MemTotal (tex_pressao_play_pct).
static volatile int pressaoPlayMin = 100;

void tex_novo_quadro(void) {
  tex_n_busca = 0;
  tex_ms_busca = 0.0;
  (void)texFreqMs;
  if (!mtx) return;
  SDL_LockMutex(mtx);
  quadroAtual++;
  amostrar();
  // Um decode concluido mas nunca mais desenhado nao deve ocupar memoria nem
  // bloquear a arte que entrou na tela. Pedidos PENDENTES sao cancelados pelo
  // consumidor da fila, para que o indice do slot nao seja reutilizado antes
  // de a fila o retirar.
  for (int i = 0; i < nMax; i++) {
    if (itens[i].estado == DECODIFICADO && pedidoObsoleto(&itens[i])) {
      liberarSup(itens[i].sup);
      itens[i].sup = NULL;
      // Promocao decodificada e nunca desenhada: fica a textura pequena, nao
      // um slot VAZIO com textura orfa (ver desistir).
      desistir(i);
    }
  }
  // PRESSAO DE MEMORIA (tex_pressao_memoria): o aviso chega de outro fio, que
  // nao tem contexto GL e nao pode despejar textura. Quem despeja e este quadro.
  // soltarCpu so no patamar DURO (<=25%%, <150 MB livres): na faixa de 50%%
  // (150-300 MB, onde a G4 fica por minutos) matar todo DECODIFICADO por
  // quadro punha a arte do OSD/posplay num laco decode->mata->decode pela
  // janela inteira — a poda a 50%% ja basta ali (rev #415).
  if (pressaoSevera()) soltarCpu(0);
  // O piso de reproducao (pressaoPlay) so despeja via poda: soltarCpu aqui
  // mataria todo DECODIFICADO que a bomba ainda nao pegou — arte do OSD/posplay
  // entraria em decode->mata->decode durante TODA a sessao (rev #415).
  if (pressaoAtiva() || pressaoPlay < 100) podar();
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

static int fileiraFoco;
void tex_residencia_inicio(int foco) { fileiraFoco = foco; }
int tex_residencia_fileiras(void) { return cachePerfil().fileiras; }
void tex_residencia_marcar(const char *url, int fileira) {
  if (!mtx || !url || !*url) return;
  SDL_LockMutex(mtx);
  int i = acharIndice(url, hashCaminho(url));
  if (i >= 0) {
    int d = abs(fileira - fileiraFoco);
    if (itens[i].fileiraQuadro != quadroAtual || d < itens[i].fileiraDistancia)
      itens[i].fileiraDistancia = d;
    itens[i].fileiraQuadro = quadroAtual;
  }
  SDL_UnlockMutex(mtx);
}
static int distancia(const Item *it) {
  if (!it->fileiraQuadro || quadroAtual - it->fileiraQuadro > 1) return 1000000;
  return it->fileiraDistancia;
}
static int residente(const Item *it) {
  return !pressaoAtiva() && pressaoPlay == 100 &&
         distancia(it) <= cachePerfil().fileiras;
}
static int despejar(int forcar);
static int slotLivre(void) {
  if (emVoo() >= (ptv_plataforma() == PTV_ANDROID && nMax / 3 > 8 ? 8 : nMax / 3)) return -1;   // pede de novo no proximo quadro
  for (int i = 0; i < nMax; i++) if (itens[i].estado == VAZIO) return i;
  // Slot que ja desistiu vale mais como vaga que um PRONTO em uso: reaproveita
  // antes de despejar arte que esta na tela.
  for (int i = 0; i < nMax; i++)
    if (itens[i].estado == FALHOU && itens[i].falhas >= 3) {
      soltarBruto(&itens[i]);
      memset(&itens[i], 0, sizeof(Item));
      itens[i].lum = -1; itens[i].corR = -1;
      return i;
    }
  // The picker keeps GLuints from update until draw. A new request must
  // wait for a cold slot rather than invalidate artwork still in use.
  return despejar(0);
}

// VITIMA LRU ENTRE OS PRONTOS, FRIOS PRIMEIRO. Devolve o slot ja limpo, ou -1.
//
// O LRU cru escolhia o menor `uso`, e com o cache no teto isso INCLUI arte que
// esta na tela: os cards sao desenhados em ordem, entao o primeiro card da
// primeira fileira e sempre o de menor `uso` entre os visiveis. MEDIDO na LG
// (log de 16/09): texturas=152 a 95.9 MB de 96 em quase toda amostra de 3 s —
// o cache VIVE no teto, e cada arte que entra despeja uma que esta la.
//
// Both slotLivre() and podar() preserve hot artwork. Requests retry when
// a slot becomes cold; evicting a hot texture can invalidate a GLuint
// already returned to the picker for the current draw.
//
// HEROIS FRIOS ALEM DESTES SAEM ANTES DE QUALQUER CARTAZ.
//
// MEDIDO na LG (16/09): tex-despejos=46 em 3 s andando por UMA fileira, com o
// cache no teto. Cada card que ganha foco pede a arte de fundo em 1920 —
// 8,3 MB — e para ela caber saem doze cartazes de 660 KB. Doze cards de foco
// e o cache inteiro deu a volta: a fileira de cima ja nao esta la, e voltar a
// ela e ve-la carregar de novo. Os herois, por sua vez, so servem enquanto o
// foco esta neles: o de tres cards atras nunca mais vai ser desenhado.
//
// Dois e nao um: o hero atual e o anterior, que o crossfade ainda desenha.
//
// "Heroi" aqui e o que foi pedido com o teto de tela cheia — tex_obter_hero
// e o tex_obter_larg de largura inteira, que e onde o cap satura. Nao e a
// linha do `urgente` (cap > 640): no Mac retina um card de 410 ja passa dela,
// e a previa despejaria cartazes que a TV nao despeja.
//
// O QUADRO DE SEQUENCIA e da mesma familia, pelo motivo oposto: pequeno, mas
// sao NOVENTA por volta. O cartaz de colecao em foco pede um quadro novo a
// cada 67 ms (home.c), e cada um vale por esse instante. MEDIDO na LG com o
// foco parado num deles: tex-despejos=45 a cada 3 s com a tela IGUAL — 15
// texturas por segundo entrando, e o LRU comum jogando fora, para cada uma,
// o cartaz mais antigo das fileiras de cima. Um minuto parado ali e o cache
// inteiro trocado por quadros de animacao; subir uma fileira e ve-la
// carregar do zero. Quem pede um quadro diz que ele e de passagem
// (tex_obter_passageira) e ele sai antes de qualquer cartaz.
//
// Tres e nao dois, por causa do quadro PRE-BUSCADO: home.c pede o proximo
// junto com o atual, e ele fica frio (pedido uma vez, desenhado dali a 4
// quadros) ate a vez dele. Com o hero anterior do crossfade e o quadro que
// acabou de sair, sao tres frios legitimos ao mesmo tempo.
#define NV_TEX_PASSAGEIROS_FRIOS 3
// Heroi e quem foi pedido no teto de tela cheia EM VIGOR. Com o teto do perfil
// em 1280 na LG (modo Desempenho, TV de 1 GB) um heroi tem limite 1280, e a
// comparacao crua com 1920 o trataria como cartaz: deixaria de sair antes deles.
static int tetoDoHeroi(void);
static int ehHero(const Item *it) {
  int t = tetoDoHeroi();
  return it->limite >= (t < NV_TEX_HERO_LARG_MAX ? t : NV_TEX_HERO_LARG_MAX);
}
static int dePassagem(const Item *it) { return it->passageiro || ehHero(it); }

static int despejar(int forcar) {
  int melhor = -1; unsigned long menor = ~0UL;
  int frio = 0;
  { int nPf = 0, lru = -1; unsigned long m = ~0UL;
    for (int i = 0; i < nMax; i++) {
      if (itens[i].estado != PRONTO || quente(&itens[i]) || residente(&itens[i]) || !dePassagem(&itens[i])) continue;
      nPf++;
      if (itens[i].uso < m) { m = itens[i].uso; lru = i; }
    }
    if (nPf > NV_TEX_PASSAGEIROS_FRIOS) { melhor = lru; frio = 1; goto sai; } }
  for (int i = 0; i < nMax; i++) {
    if (itens[i].estado != PRONTO || residente(&itens[i])) continue;
    if (quente(&itens[i])) { if (frio) continue; }
    else if (!frio) { frio = 1; melhor = -1; menor = ~0UL; }
    if (melhor < 0 || distancia(&itens[i]) > distancia(&itens[melhor]) ||
        (distancia(&itens[i]) == distancia(&itens[melhor]) && itens[i].uso < menor)) {
      menor = itens[i].uso; melhor = i;
    }
  }
sai:
  if (melhor < 0) return -1;
  (void)forcar;
  if (!frio) return -1;
  tex_despejos++; tex_despejos_total++;
  if (!frio) { tex_despejos_quentes++; tex_despejos_quentes_total++; }
  if (itens[melhor].tex) { gfx_tex_esquecer(itens[melhor].tex); glDeleteTextures(1, &itens[melhor].tex); }
  bytesUsados -= bytesTextura(&itens[melhor]);
  if (bytesUsados < 0) bytesUsados = 0;
  soltarBruto(&itens[melhor]);
  memset(&itens[melhor], 0, sizeof(Item));
  itens[melhor].lum = -1;   // 0 seria "preto"; o desconhecido e -1
  itens[melhor].corR = -1;
  return melhor;
}

// Despeja os menos usados ate caber no orcamento. Chamada com o mutex travado.
// Para quando so resta arte quente ou em voo: ver despejar().
//
// PRESSAO DE MEMORIA DO SISTEMA (Android onTrimMemory). Um upload de textura
// chegou a levar 16 s com o low-memory killer ativo (HANDOFF-PERFORMANCE, item
// 4): o app seguia pedindo memoria enquanto o sistema tentava tirar dela.
// Sob aviso, o teto efetivo cai por uns segundos e podar() despeja arte FRIA
// ate caber — nunca a quente nem a em voo (ver despejar). Passado o prazo o teto
// volta sozinho: onTrimMemory avisa na transicao, nao enquanto dura.
// Escrito pelo fio da JNI, lido pelo fio de desenho; int de 32 bits, sem trava.
#define NV_TEX_PRESSAO_MS 30000u
static volatile int pressaoPct = 100;
static int pressaoLivrePct = 100; // Sampled MemAvailable pressure lasts until recovery.
static volatile Uint32 pressaoAte;
// REPRODUCAO NO ANDROID: o trim (abaixo) expira em 30 s porque onTrimMemory
// avisa na transicao; o video toca por horas e a arte da home fica residente
// o tempo todo. Com um teto persistente de tela-cheia, gpu-cache + player
// buffer + pedacos paralelos + preload do proximo titulo estouraram uma TV
// de 2,4 GB (PSS ~575 MB, lmkd matou o app). Durante a reproducao o teto
// efetivo fica nesta fracao, ate a sessao de video fechar. No Android liga
// com o video; no webOS/.tpk so com a TV sem memoria (tex_memoria_livre).
// `pressaoPlay` foi declarado junto das forward declarations (a poda a le).
static int pressaoAtiva(void) {
  if (pressaoPct >= 100) return pressaoLivrePct < 100;
  if ((int)(SDL_GetTicks() - pressaoAte) >= 0) {
    pressaoPct = pressaoPct <= 25 ? 50 : pressaoPct <= 50 ? 75 : 100;
    pressaoAte = SDL_GetTicks() + 5000u;
    printf("[tex] timed memory trim recovering: %d%%\n", pressaoPct);
    fflush(stdout);
    return pressaoPct < 100 || pressaoLivrePct < 100;
  }
  return 1;
}
static int pressaoSevera(void) {
  return pressaoAtiva() && (pressaoPct <= 25 || pressaoLivrePct <= 25);
}
static long limiteBytes(void) {
  (void)pressaoAtiva(); // Expire timed trims before combining the caps.
  int pct = pressaoPct < pressaoLivrePct ? pressaoPct : pressaoLivrePct;
  long lim = pct < 100 ? orcamento / 100 * pct : orcamento;
  if (pressaoPlay < 100 && orcamento / 100 * pressaoPlay < lim)
    lim = orcamento / 100 * pressaoPlay;
  // Pressure must never make the current screen exceed its own cap.
  if (lim < orcamento) {
    long tela = 0;
    for (int i = 0; i < nMax; i++)
      if (quente(&itens[i])) tela += bytesTextura(&itens[i]);
    if (tela > lim) lim = tela < orcamento ? tela : orcamento;
  }
  return lim;
}
// % do teto que fica enquanto um video esta aberto (reproduzindo ou nao:
// fechar a sessao, nao pausar, e o que libera). Numero pela RAM medida na
// TV de 2,4 GB: 217 MB de textura + ~160 MB de player/preparo derrubaram o
// app; com <= 96 MB de textura a soma cabe. A home redecodifica ao voltar —
// o cache de disco continua la.
int tex_pressao_play_pct(void) {
#ifdef NV_ANDROID
  if (!orcMemTotal) return 50;
  if (orcMemTotal < 3000) return 40;
  return 75;
#else
  // webOS/.tpk: o piso NAO e incondicional — a regra e "caps menores so com
  // memoria baixa". A faixa vem do relatorio de 3 s (pressaoPlayMin, em
  // tex_memoria_livre): TV com folga (C9 com GBs livres) fica com o cache
  // inteiro durante a reproducao; a G4 apertada cai no piso medido no #415.
  return pressaoPlayMin;
#endif
}
void tex_pressao_reproducao(int ligada) {
  pressaoPlayLigada = ligada;
  int pct = ligada ? tex_pressao_play_pct() : 100;
  if (pct == pressaoPlay) return;
  pressaoPlay = pct;
  printf("[tex] reproducao %s: teto a %d%% enquanto o video durar\n",
         ligada ? "aberta" : "fechada", pct);
  fflush(stdout);
}
// Niveis de ComponentCallbacks2.onTrimMemory -> fracao do teto que fica.
// 5/10/15 sao com o app em primeiro plano (MODERATE/LOW/CRITICAL); 20 e a UI
// escondida; 40+ e o app em segundo plano; 60/80 o processo na ponta da lista
// do killer. Nivel desconhecido ou < 5 nao muda nada.
int tex_pressao_pct(int nivel) {
  if (nivel >= 60) return 25;
  if (nivel >= 20) return 50;
  if (nivel >= 15) return 40;
  if (nivel >= 10) return 60;
  if (nivel >= 5) return 80;
  return 100;
}
void tex_pressao_memoria(int nivel) {
  int pct = tex_pressao_pct(nivel);
  if (pct >= 100) return;
  /* Aviso mais fraco nao afrouxa um mais forte ainda em vigor. */
  if (pressaoAtiva() && pressaoPct < pct) pct = pressaoPct;
  pressaoPct = pct;
  pressaoAte = SDL_GetTicks() + NV_TEX_PRESSAO_MS;
  printf("[tex] pressao de memoria (trim %d): teto a %d%% por %u s\n", nivel, pct, NV_TEX_PRESSAO_MS / 1000u);
  fflush(stdout);
}
#ifdef NV_ANDROID
#include <jni.h>
JNIEXPORT void JNICALL
Java_space_nuvio_nativelegacy_NuvioActivity_nativeTrimMemoria(JNIEnv *env, jobject act, jint nivel) {
  (void)env; (void)act;
  tex_pressao_memoria((int)nivel);
}
#endif

// SOB PRESSAO: a memoria de CPU que podar() nao alcanca. A superficie ja
// decodificada (`sup`) e os bytes baixados (`bruto`) ficam fora de
// bytesUsados; sem este passe um aviso no meio do decode derruba so a GPU e os
// buffers vivos seguem segurando o heap — e malloc_trim nao solta o que esta
// alocado. O pedido morre junto (desistir): com a TV no piso, guardar decode
// pronto para "talvez desenhar" e o que encheu a G4 no #415.
// `falhas` = tambem limpa os slots FALHOU (tex_aliviar, antes de reabrir o
// pipeline); na varredura do quadro o slot falho fica, guardando o recuo.
static void soltarCpu(int falhas) {
  for (int i = 0; i < nMax; i++) {
    if (!falhas && quente(&itens[i])) continue;
    if (itens[i].estado == DECODIFICADO && itens[i].sup) {
      if (uplSup == itens[i].sup) uplSup = NULL;   // envio em curso revalida pelo idx
      liberarSup(itens[i].sup);
      itens[i].sup = NULL;
      desistir(i);
    } else if ((itens[i].estado == PENDENTE && !itens[i].naFilaDec &&
                !itens[i].filaRedeEm) ||
               (falhas && itens[i].estado == FALHOU)) {
      // O idx de um PENDENTE JA NUMA FILA (rede ou decode) nao pode virar
      // VAZIO aqui: a fila e um anel de indices e devolveria o numero para o
      // proximo slot a ocupa-lo. Ele desiste sozinho ao ser tirado; o `bruto`
      // sai mesmo assim.
      desistir(i);
    } else if (itens[i].estado == FALHOU || itens[i].bruto) {
      soltarBruto(&itens[i]);
    }
  }
}

static void podar(void) {
  long lim = limiteBytes();
  while (bytesUsados > lim)
    if (despejar(0) < 0) break;
}

// webOS/.tpk nao tem onTrimMemory: o relatorio de 3 s (main.c) passa o
// MemAvailable. #415 (LG G4, webOS 9, MemTotal 1965 MB): 127 MB de arte
// residente com o video 4K em tela cheia (tela=0), e o webOS fechou o app com
// 92 MB livres.
// The 150/300 MiB thresholds scale down below 1965 MiB RAM; recovery needs
// another 60 MiB (also scaled). Sampled pressure does not use the trim timer.
// MESMO relatorio arma a faixa do piso de reproducao (pressaoPlayMin): abrir
// video com a TV apertada ja comeca com o cache no piso — o pico de alocacao
// do pipeline acontece ANTES de a memoria cair de vez, e esperar era deixar
// o cache inteiro vivo durante o arranque. A faixa tambem aperta um piso ja
// ligado; afrouxar so no fechar (video aberto nao recupera arte sob pressao
// intermitente). A home redecodifica do disco ao voltar.
void tex_memoria_livre(long livreMb) {
  if (livreMb < 0) return;
  long ram = orcMemTotal > 0 && orcMemTotal < 1965 ? orcMemTotal : 1965;
  long duro = 150 * ram / 1965, baixo = 300 * ram / 1965;
  long folga = 60 * ram / 1965;
  int pct = livreMb < duro ? 25 : livreMb < baixo ? 50 : 100;
  // Stay in each band until available memory clears its recovery margin.
  if (pressaoLivrePct == 25 && livreMb < duro + folga) pct = 25;
  else if (pressaoLivrePct < 100 && pct == 100 && livreMb < baixo + folga) pct = 50;
  if (pct != pressaoLivrePct) {
    pressaoLivrePct = pct;
    printf("[tex] MemAvailable=%ld MB: texture cap at %d%%\n", livreMb, pct);
    fflush(stdout);
  }
  pressaoPlayMin = pct == 25 ? 20 : pct == 50 ? (orcMemTotal >= 3500 ? 35 : 20) : 100;
  if (pressaoPlayLigada && pressaoPlayMin < pressaoPlay) pressaoPlay = pressaoPlayMin;
}
void tex_aliviar(void) {
  tex_pressao_memoria(80);
  if (!mtx) return;
  SDL_LockMutex(mtx);
  soltarCpu(1);
  podar();
  SDL_UnlockMutex(mtx);
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
// promocao. (O #define esta no topo do arquivo: despejar() precisa dele antes.)
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
// (O #define de NV_TEX_HERO_LARG_MAX esta no topo do arquivo.)

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
// QUALIDADE DA IMAGEM, escolhida na tela de Ajustes: 0 baixa, 1 padrao, 2 alta.
//
// O QUE ELA MUDA E O TETO DE DECODIFICACAO, e so isso — o desenho e o mesmo em
// qualquer nivel. A folga sobre a largura de desenho existe para o card que
// cresce no foco e para a arte que sobe de tamanho numa tela maior; subir a
// folga e pedir mais pixel de origem para o MESMO desenho, que e literalmente
// a definicao de qualidade de imagem aqui.
//
//   baixa  1,00  a arte chega no tamanho do desenho; e a que carrega antes e a
//                que cabe numa TV de 2016 com 624 MB de RAM
//   padrao 1,25  o que sempre foi
//   alta   1,60  ~64% mais pixel por arte; a memoria sobe junto (area)
//
// O TETO DO HEROI acompanha: na baixa ele cai para 1280, que e o que a maioria
// das fontes entrega, e a arte de tela cheia deixa de custar 8 MB por troca.
//
// Nao ha recarga: vale para a arte que ENTRAR daqui para frente. Re-decodificar
// o cache inteiro na troca seria um engasgo de segundos na tela de Ajustes, e o
// que ja esta decodificado nao fica errado — so nao melhora ate ser despejado.
static int qualidadeImg = 1;
void tex_qualidade(int nivel) {
  if (nivel >= 0 && nivel <= 2) { qualidadeImg = nivel; arte_tamanho_qualidade(nivel); }
}
static float folgaDaQualidade(void) {
  return qualidadeImg == 0 ? 1.00f : qualidadeImg == 2 ? 1.60f : NV_TEX_FOLGA;
}
// RAM total medida no arranque: declarada la em cima (pressaoPlay usa).
// TETO DO HEROI DO PERFIL (tex_definir_teto_heroi): 0 = sem teto proprio, so a
// regra de qualidade abaixo. E um TETO, nunca um piso: 1920 aqui nao sobe o
// Tizen acima de 1280 sem a qualidade Alta, e a qualidade Baixa continua 1280.
static int tetoHeroiPerfil = 0;
static int tetoDoHeroiQualidade(void);
static int tetoDoHeroi(void) {
  int t = tetoDoHeroiQualidade();
  // Utgard (.tpk, #286): 960 vale tambem para o perfil gravado/diagnostico, que
  // ptv_limitar nunca deixa abaixo de 1280, e para a qualidade Alta.
  if (ptv_gpu_utgard_atual() && t > 960) t = 960;
  return tetoHeroiPerfil >= 640 && tetoHeroiPerfil < t ? tetoHeroiPerfil : t;
}
static int tetoDoHeroiQualidade(void) {
#ifdef NV_TIZEN3_LITE
  return NV_TEX_HERO_LARG_MAX;
#endif
  if (qualidadeImg == 0 || (orcMemTotal > 0 && orcMemTotal <= 1300)) return 1280;
#ifdef __EMSCRIPTEN__
  // SAMSUNG: 1280 e o teto de fabrica (heap fixo de 256 MiB, ver a nota das
  // medidas), e numa TV 4K isso e um fundo esticado 1,5x — "o artwork parece
  // meio pixelado" (dono, 21/09/2026, emulador Tizen 10). Quem escolheu
  // qualidade ALTA numa TV com 2 GB ou mais ganha o heroi em 1920, que e o
  // mesmo custo do LG (8 MB por arte); com 1 GB continua 1280, que e onde o
  // heap aperta (registro D1 1193: 5,7 MiB livres no arranque).
  if (qualidadeImg == 2 && orcMemTotal >= 2000) return 1920;
#endif
  return NV_TEX_HERO_LARG_MAX;
}
static float escalaBuf = 1.0f;
// Budget scale. On a TV the MB are physical RAM: a 4K drawable (escala 2) must
// not multiply them by 4 (128 MB became 512 and the TCL hit the LMK at 720 MB).
// Only the desktop preview (Mac retina) scales, so it mirrors the TV scene.
static float escalaOrc(void) {
#if defined(NV_ANDROID) || defined(NV_TPK) || defined(NV_WEBOS)
  return 1.0f;
#else
  return escalaBuf > 0.1f ? escalaBuf : 1.0f;
#endif
}

__attribute__((unused)) static int arquivoCacheImagem(const char *nome) {
  const char *p = nome;
  while (isxdigit((unsigned char)*p)) p++;
  if (p - nome < 8 || p - nome > 16) return 0;
  return !strcmp(p, ".jpg") || !strcmp(p, ".jpeg") || !strcmp(p, ".png") ||
         !strcmp(p, ".webp") || !strcmp(p, ".gif");
}

void tex_escala(float e) {
  if (e > 0.1f && e < 8.0f) escalaBuf = e;
}


void tex_cache_dir(const char *dir) {
  if (!dir || !*dir) return;
  snprintf(dirCache, sizeof dirCache, "%s", dir);
  mkdir(dirCache, 0777);
#ifndef __EMSCRIPTEN__
  cachearte_nativo_configurar_diretorio(dirCache);
#endif
#ifdef __EMSCRIPTEN__
  /* Artwork has its own lazy IndexedDB database. Do not put these bodies in
   * /nuvio IDBFS: syncfs would hydrate the complete image collection at boot. */
  cachearte_iniciar();
  cachearte_limite_bytes(NV_CACHE_DISCO_MAX);
#endif
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

  // O QUE JA ESTA NO DISCO TAMBEM CONTA. cacheDiscoBytes nascia em 0 a cada
  // arranque e so somava o que a sessao acrescentava, entao o teto de
  // NV_CACHE_DISCO_MAX nunca enxergava o acumulado das sessoes anteriores: a
  // pasta crescia sem limite ao longo dos dias e o log dizia "cache-disco=0.0MB"
  // com arte gravada la dentro.
  //
  // MEDIDO em 17/09 na QN85Q70AAGXZD: 147 arquivos e 23,3 MB em /nuvio/cache
  // enquanto o relatorio da sessao inteira imprimia 0.0MB. O teto e de 48 MB —
  // ou seja, faltava pouco para o contador estar errado por mais do que ele
  // mede.
  //
  // A varredura e uma vez por arranque, com a pasta ja aberta, e o que ela
  // custa e um stat por arquivo.
  //
  // NO LG A VARREDURA SAIU DAQUI (22/09/2026): vira o indice em memoria de
  // cachearte.c, lido uma vez pelo fio de gravacao ao nascer (iniciarGravador),
  // sem segurar nada que o desenho ou os fios de rede esperem. Com 3309
  // arquivos no eMMC ela nao e "um stat por arquivo" barato.
#ifdef __EMSCRIPTEN__
  { DIR *d = opendir(dirCache);
    struct dirent *e;
    long total = 0;
    int n = 0;
    if (d) {
      while ((e = readdir(d)) != NULL) {
        char caminho[768];
        struct stat st;
        if (!arquivoCacheImagem(e->d_name)) continue;
        snprintf(caminho, sizeof caminho, "%s/%s", dirCache, e->d_name);
        if (lstat(caminho, &st) == 0 && S_ISREG(st.st_mode)) {
          total += st.st_size; n++;
          arqDiscoRegistrar(caminho);
        }
      }
      closedir(d);
      cacheDiscoBytes = total;
      publicarCacheDisco();
      if (n)
        printf("[tex] cache de disco ja tinha %d arquivo(s), %.1f MB\n",
               n, total / 1048576.0);
    } }
#else
  iniciarGravador();
#endif
  fflush(stdout);
}

#ifdef __EMSCRIPTEN__
// MARCA USO, para que a poda seja mesmo por MENOS USADO.
//
// podarCacheDisco ordena por mtime, e sem esta chamada o mtime e a data da
// ESCRITA. O efeito seria o inverso do pretendido: a arte MAIS lida — a da
// home, gravada ha semanas e servida do disco desde entao — tem o mtime mais
// velho e seria a primeira a sair, enquanto um poster aberto uma vez ontem
// ficaria. Tocar o mtime no acerto de cache transforma "data de escrita" em
// "data de ultimo uso", que e o que a ordenacao precisa.
//
// No IDBFS o mtime e campo em memoria; nao custa viagem a disco.
static void marcarUso(const char *caminho) { utime(caminho, NULL); }

// PODA O MAIS ANTIGO, e nao o recem-chegado.
//
// A versao anterior fazia o contrario: passado o teto, apagava o arquivo que
// ACABOU de ser baixado ("o mais novo nao fica"). Essa e a pior politica
// possivel — garante que justamente a arte que esta na tela agora nunca fique
// em cache, e que ela seja baixada de novo na proxima vez que aparecer. O
// conjunto "quente" que ela pretendia preservar e exatamente o que ela
// descartava.
//
// O defeito so ficou VISIVEL em 17/09, quando cacheDiscoBytes passou a contar o
// que ja estava no disco (antes nascia em 0 a cada arranque, o teto nunca era
// atingido e o ramo nunca rodava). Consertar o contador ligou um caminho que
// estava errado desde que foi escrito.
//
// Agora: ordena por data de modificacao e apaga do mais velho para o mais novo
// ate voltar a caber, com uma folga de 25% para nao podar a cada arquivo novo.
static void podarCacheDisco(void) {
  struct { char nome[64]; long tam; time_t quando; } lista[512];
  int n = 0, i, j;
  long alvo = NV_CACHE_DISCO_MAX - NV_CACHE_DISCO_MAX / 4;
  DIR *d = opendir(dirCache);
  struct dirent *e;
  if (!d) return;
  while ((e = readdir(d)) != NULL && n < (int)(sizeof lista / sizeof *lista)) {
    char caminho[768];
    struct stat st;
    if (e->d_name[0] == '.') continue;
    snprintf(caminho, sizeof caminho, "%s/%s", dirCache, e->d_name);
    if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode)) continue;
    snprintf(lista[n].nome, sizeof lista[n].nome, "%s", e->d_name);
    lista[n].tam = (long)st.st_size;
    lista[n].quando = st.st_mtime;
    n++;
  }
  closedir(d);
  // Insercao: sao centenas de entradas e isto roda quando o teto estoura, nao
  // por quadro. Um qsort aqui so acrescentaria um comparador para ler.
  for (i = 1; i < n; i++) {
    int k = i;
    while (k > 0 && lista[k - 1].quando > lista[k].quando) {
      char nm[64]; long tm; time_t qd;
      snprintf(nm, sizeof nm, "%s", lista[k - 1].nome); tm = lista[k - 1].tam; qd = lista[k - 1].quando;
      lista[k - 1] = lista[k];
      snprintf(lista[k].nome, sizeof lista[k].nome, "%s", nm); lista[k].tam = tm; lista[k].quando = qd;
      k--;
    }
  }
  for (j = 0; j < n && cacheDiscoBytes > alvo; j++) {
    char caminho[768];
    snprintf(caminho, sizeof caminho, "%s/%s", dirCache, lista[j].nome);
    if (remove(caminho) != 0) continue;
    cacheDiscoBytes -= lista[j].tam;
    if (cacheDiscoBytes < 0) cacheDiscoBytes = 0;
  }
  printf("[tex] cache de disco podado: %d arquivo(s) antigos, agora %.1f MB\n",
         j, cacheDiscoBytes / 1048576.0);
  fflush(stdout);
}
#endif

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

#ifdef __EMSCRIPTEN__
// ARQUIVOS QUE JA ESTAO NA PASTA DE CACHE (Tizen, 24/09/2026). No Tizen so GIF
// vai a arquivo (gif.c le por caminho); o resto vive em Item.bruto. Mas o fio
// de rede so descobria que era GIF DEPOIS de baixar o corpo inteiro para a
// memoria — e entao jogava os bytes fora e chamava garantirLocal, que achava o
// arquivo (download perdido) ou baixava tudo DE NOVO. Registros D1 da TV do
// rawldon: o avatar GIF de 1 MB (a1edacf2, 498x448) baixado 4 vezes na
// sessao 2262 (1.4.3: 2929+761+473+377 ms), 3 na 2512 (1.4.4) e 2 na 1839
// (1.4.1), com o arquivo ja no /nuvio/cache desde a sessao anterior ("cache
// de disco ja tinha 2 arquivo(s), 2.1 MB"); na 2565 (1.4.5), sem o arquivo,
// o primeiro pedido baixou DUAS vezes (`image_requests=2 net_ms=3169`). E um
// dos DOIS fios de rede de arte parado 0,4-3 s por pedido, enquanto a home
// ja pede os cartazes; eles esperavam atras (`fila-rede wait=2706`). Com
// este registro o pedido de um arquivo que ja existe vai direto a
// garantirLocal (um fopen, sem rede).
//
// So nomes, em memoria: a varredura do arranque (tex_cache_dir) ja lia a
// pasta, e garantirLocal registra o que grava. Um nome que a poda apagou
// continua aqui sem dano: garantirLocal confere o arquivo e baixa se faltar.
#define NV_ARQ_DISCO_MAX 128
static unsigned long arqDisco[NV_ARQ_DISCO_MAX];
static int nArqDisco, proxArqDisco;
static pthread_mutex_t arqDiscoMtx = PTHREAD_MUTEX_INITIALIZER;
static void arqDiscoRegistrar(const char *dst) {
  unsigned long h = hashCaminho(dst);
  int i;
  pthread_mutex_lock(&arqDiscoMtx);
  for (i = 0; i < nArqDisco && arqDisco[i] != h; i++) {}
  if (i == nArqDisco) {
    if (nArqDisco < NV_ARQ_DISCO_MAX) arqDisco[nArqDisco++] = h;
    else { arqDisco[proxArqDisco] = h; proxArqDisco = (proxArqDisco + 1) % NV_ARQ_DISCO_MAX; }
  }
  pthread_mutex_unlock(&arqDiscoMtx);
}
static int arqDiscoTem(const char *dst) {
  unsigned long h = hashCaminho(dst);
  int i, achou = 0;
  pthread_mutex_lock(&arqDiscoMtx);
  for (i = 0; i < nArqDisco && !achou; i++) achou = arqDisco[i] == h;
  pthread_mutex_unlock(&arqDiscoMtx);
  return achou;
}
#endif

#ifndef __EMSCRIPTEN__
/* Nunca remove o arquivo entre a entrega da rede e a leitura pelo decoder.
 * Ordem de locks: s_native_mtx (cachearte) e mtx sao tomadas separadamente,
 * nunca uma dentro da outra. */
static int discoProtegido(const char *caminho, void *ctx) {
  int i, protegido = 0;
  (void)ctx;
  if (cachearte_nativo_protegido(caminho)) return 1;
  pthread_mutex_lock(&vidaMtx);
  if (!mtx) { pthread_mutex_unlock(&vidaMtx); return 0; }
#ifdef NV_TEX_TEST_BEFORE_DISK_LOCK
  NV_TEX_TEST_BEFORE_DISK_LOCK();
#endif
  SDL_LockMutex(mtx);
  for (i = 0; i < nMax; i++) {
    char local[600];
    if (itens[i].estado != PENDENTE) continue;
    nomeDeCache(itens[i].caminho, local, sizeof local);
    if (!strcmp(local, caminho)) { protegido = 1; break; }
  }
  SDL_UnlockMutex(mtx);
  pthread_mutex_unlock(&vidaMtx);
  return protegido;
}
static void publicarDiscoNativo(void) {
  __atomic_store_n(&cacheDiscoSnapshot, cachearte_nativo_indice_bytes(), __ATOMIC_RELEASE);
}
/* Ver NV_CACHE_DISCO_FRACAO. Um statvfs, microssegundos: roda por gravacao. */
static long tetoDisco(void) {
  struct statvfs fs;
  long teto = NV_CACHE_DISCO_MAX;
  if (dirCache[0] && statvfs(dirCache, &fs) == 0) {
    if ((double)fs.f_bavail * fs.f_frsize < 300.0 * 1048576.0)
      return NV_CACHE_DISCO_PISO;
    double possivel = (double)fs.f_bavail * (double)fs.f_frsize +
                      (double)cachearte_nativo_indice_bytes();
    double prop = possivel * NV_CACHE_DISCO_FRACAO / 100.0;
    if (prop < (double)teto) teto = (long)prop;
  }
  if (teto < NV_CACHE_DISCO_PISO) teto = NV_CACHE_DISCO_PISO;
  return teto;
}
static void podarSePreciso(long entrada, int forcar) {
  cachearte_nativo_podar(entrada, tetoDisco(), NV_CACHE_DISCO_RESERVA, forcar,
                         discoProtegido, NULL);
  publicarDiscoNativo();
}

/* Escreve `dst` por temporario + rename, SEM fsync: e cache, e um arquivo
 * perdido num corte de energia so custa baixar de novo (o rename atomico
 * garante que nunca fica meio arquivo com o nome final). ENOSPC/EDQUOT na
 * primeira tentativa poda forcado e tenta de novo com os MESMOS bytes, sem
 * voltar a rede. Nao segura trava nenhuma durante o I/O. */
static int gravarArquivo(const char *dst, const char *sufixo,
                         const unsigned char *corpo, long n) {
  char tmp[640];
  int tentativa, ok = 0, erro = 0;
  FILE *f;
  snprintf(tmp, sizeof tmp, "%s%s", dst, sufixo);
  podarSePreciso(n, 0);
  for (tentativa = 0; tentativa < 2; tentativa++) {
    size_t esc = 0;
    int fim = 0;
    errno = 0;
    f = fopen(tmp, "wb");
    if (f) {
      esc = fwrite(corpo, 1, (size_t)n, f);
      erro = errno;
      fim = fclose(f);
      if (fim != 0 && !erro) erro = errno;
      if (esc == (size_t)n && fim == 0) {
        if (rename(tmp, dst) == 0) { ok = 1; break; }
        erro = errno;
      }
    } else erro = errno;
    if (!erro) erro = EIO;
    printf("[tex] gravacao incompleta (%zu de %ld B, erro %d: %s): %.70s\n",
           esc, n, erro, strerror(erro), dst);
    fflush(stdout);
    remove(tmp);
    if (tentativa == 0 && (erro == ENOSPC || erro == EDQUOT)) {
      podarSePreciso(n, 1);
      continue;
    }
    break;
  }
  if (ok) cachearte_nativo_indice_registrar(dst, n);
  cachearte_nativo_gravacao(ok);
  publicarDiscoNativo();
  return ok;
}

/* FILA DE GRAVACAO DE FUNDO (22/09/2026).
 *
 * O download entrega os bytes ao decode NA HORA (Item.bruto) e a copia para o
 * disco entra aqui. Um fio so, nice 19 no Linux: o eMMC da TV e um recurso
 * unico e varios escritores so disputariam a mesma fila do controlador.
 * LIMITADA (entradas e bytes): se o disco nao acompanha, a gravacao nova e
 * DESCARTADA — o cache e otimizacao, e a pior consequencia de descartar e
 * baixar de novo numa proxima vez. Bloquear o fio de rede, nunca.
 *
 * Enquanto o arquivo nao existe, o mesmo pedido acha os bytes aqui
 * (gravacaoPendente) e nao volta a rede. */
#define NV_GRAV_MAX 32
#define NV_GRAV_BYTES (24L * 1024L * 1024L)
typedef struct { char dst[600]; unsigned char *b; long n; int cancelada; } Grav;
static pthread_mutex_t gravMtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gravCond = PTHREAD_COND_INITIALIZER;
static pthread_cond_t gravVazia = PTHREAD_COND_INITIALIZER;
static Grav filaGrav[NV_GRAV_MAX];
static Grav gravAtual;            /* a que esta sendo escrita, ainda consultavel */
static int gravIni, gravN, gravIniciado;
static long gravBytes;
static long gravDescartes;

static void limparCacheEnvenenado(void);
static void *fioGravador(void *arg) {
  (void)arg;
#ifdef __linux__
  /* Prioridade por fio: no Linux o nice vale por tid. */
  setpriority(PRIO_PROCESS, (id_t)syscall(SYS_gettid), 19);
#endif
  /* A UNICA leitura da pasta na sessao, aqui e fora de qualquer trava que
   * outro fio espere. A primeira poda (arte acumulada acima do teto novo)
   * tambem roda aqui, em fundo. */
  limparCacheEnvenenado();
  cachearte_nativo_indice_construir();
  publicarDiscoNativo();
  cachearte_estatisticas_pedir();
  podarSePreciso(0, 0);
  for (;;) {
    char descartar[600] = "";
    pthread_mutex_lock(&gravMtx);
    while (!gravN) { pthread_cond_broadcast(&gravVazia); pthread_cond_wait(&gravCond, &gravMtx); }
    gravAtual = filaGrav[gravIni];
    filaGrav[gravIni].b = NULL;
    gravIni = (gravIni + 1) % NV_GRAV_MAX; gravN--;
    pthread_mutex_unlock(&gravMtx);
    if (gravAtual.b) gravarArquivo(gravAtual.dst, ".fila", gravAtual.b, gravAtual.n);
    pthread_mutex_lock(&gravMtx);
    // O decode pode recusar o corpo enquanto fwrite ainda esta em curso.
    // Nao libera o buffer do gravador; remove o arquivo depois do rename.
    if (gravAtual.cancelada) {
      snprintf(descartar, sizeof descartar, "%s", gravAtual.dst);
      pthread_mutex_unlock(&gravMtx);
      if (remove(descartar) == 0) {
        cachearte_nativo_indice_remover(descartar);
        publicarDiscoNativo();
      }
      pthread_mutex_lock(&gravMtx);
    }
    gravBytes -= gravAtual.n;
    free(gravAtual.b); gravAtual.b = NULL; gravAtual.dst[0] = 0; gravAtual.n = 0;
    gravAtual.cancelada = 0;
    pthread_mutex_unlock(&gravMtx);
  }
  return NULL;
}
// LIMPEZA UNICA DA 1.4.3. Com o reuso de conexao da 1.4.2 a TV podia gravar no
// cache os bytes de OUTRA imagem com o nome de um cartaz (ver soltarHandleR em
// rede.c), e o arquivo errado voltava em toda abertura. Na primeira vez, aqui,
// no fio de fundo e antes do indice, o cache de arte e esvaziado; a marca
// impede de repetir. Arte e so cache: volta a baixar sob demanda.
static void limparCacheEnvenenado(void) {
  char marca[600], cam[1100];
  DIR *d;
  struct dirent *e;
  long n = 0;
  FILE *f;
  if (!dirCache[0]) return;
  snprintf(marca, sizeof marca, "%s/.limpo-143", dirCache);
  if (access(marca, F_OK) == 0) return;
  d = opendir(dirCache);
  if (d) {
    while ((e = readdir(d)) != NULL) {
      if (e->d_name[0] == '.') continue;
      snprintf(cam, sizeof cam, "%s/%s", dirCache, e->d_name);
      if (remove(cam) == 0) n++;
    }
    closedir(d);
  }
  f = fopen(marca, "w");
  if (f) { fputs("1\n", f); fclose(f); }
  printf("[tex] cache de arte limpo uma vez (1.4.3): %ld arquivo(s)\n", n);
  fflush(stdout);
}
static void iniciarGravador(void) {
  pthread_t t;
  pthread_mutex_lock(&gravMtx);
  if (!gravIniciado && pthread_create(&t, NULL, fioGravador, NULL) == 0) {
    pthread_detach(t); gravIniciado = 1;
  }
  pthread_mutex_unlock(&gravMtx);
}
/* Copia os bytes: o original vai para o decode, que o libera. 1 se entrou. */
static int enfileirarGravacao(const char *dst, const unsigned char *b, long n) {
  unsigned char *copia;
  int ok = 0;
  if (!dst || !b || n <= 0) return 0;
  pthread_mutex_lock(&gravMtx);
  if (gravIniciado && gravN < NV_GRAV_MAX && gravBytes + n <= NV_GRAV_BYTES &&
      (copia = (unsigned char *)malloc((size_t)n)) != NULL) {
    Grav *g = &filaGrav[(gravIni + gravN) % NV_GRAV_MAX];
    memcpy(copia, b, (size_t)n);
    snprintf(g->dst, sizeof g->dst, "%s", dst);
    g->b = copia; g->n = n; g->cancelada = 0;
    gravN++; gravBytes += n; ok = 1;
    pthread_cond_signal(&gravCond);
  } else {
    gravDescartes++;
    if (gravDescartes == 1 || gravDescartes % 50 == 0)
      printf("[tex] fila de gravacao cheia (%d, %.1f MB): %ld gravacao(oes) descartadas\n",
             gravN, gravBytes / 1048576.0, gravDescartes);
  }
  pthread_mutex_unlock(&gravMtx);
  return ok;
}
/* Copia dos bytes ainda nao gravados de `dst`, se houver. */
static int gravacaoPendente(const char *dst, unsigned char **b, long *n) {
  int i, ok = 0;
  pthread_mutex_lock(&gravMtx);
  for (i = -1; i < gravN && !ok; i++) {
    Grav *g = i < 0 ? &gravAtual : &filaGrav[(gravIni + i) % NV_GRAV_MAX];
    if (g->b && !g->cancelada && !strcmp(g->dst, dst) &&
        (*b = (unsigned char *)malloc((size_t)g->n)) != NULL) {
      memcpy(*b, g->b, (size_t)g->n); *n = g->n; ok = 1;
    }
  }
  pthread_mutex_unlock(&gravMtx);
  return ok;
}
/* O decode recusou os bytes: nao deixa a fila gravar veneno no cache. */
static void cancelarGravacao(const char *dst) {
  int i;
  pthread_mutex_lock(&gravMtx);
  if (gravAtual.b && !strcmp(gravAtual.dst, dst)) gravAtual.cancelada = 1;
  for (i = 0; i < gravN; i++) {
    Grav *g = &filaGrav[(gravIni + i) % NV_GRAV_MAX];
    if (g->b && !strcmp(g->dst, dst)) { gravBytes -= g->n; free(g->b); g->b = NULL; g->n = 0; }
  }
  pthread_mutex_unlock(&gravMtx);
}
/* Testes: espera a fila esvaziar (a gravacao em curso inclusive). */
void tex_cache_esperar_gravacoes(void) {
  pthread_mutex_lock(&gravMtx);
  while (gravIniciado && (gravN || gravAtual.b)) pthread_cond_wait(&gravVazia, &gravMtx);
  pthread_mutex_unlock(&gravMtx);
}
#endif

#ifdef __EMSCRIPTEN__
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif
#endif

// Baixa UMA url e devolve o corpo so se ele for imagem; NULL com a razao no
// log. Separado de garantirLocal para a reserva do TMDB passar pelo mesmo
// crivo (assinatura, tamanho) que a url original.
// URL DE PROVEDOR DE POSTER NUNCA VAI PARA O LOG COM O QUE TEM DEPOIS DO HOST:
// o token do SpatialPosters e a chave do RPDB moram na query/caminho
// (posterprov.h). As DEMAIS tambem passam por redacao (rede_url_log): o cartaz
// que um addon de meta devolve (btttr.cc do BetterPosters/PostersPlus,
// AioMetadata, RPDB, top-posters) leva a config da pessoa no CAMINHO, e ate a
// 1.6.4 ia inteiro para o log em "[tex] corpo curto"/"download falhou".
static const char *urlLog(const char *url, char *buf, size_t n) {
  return posterprov_e_provedor(url) ? posterprov_redigir(url, buf, n)
                                    : rede_url_log(url, buf, (unsigned)n);
}

static void logoOrigem(const char *url, const char *fonte, Uint32 ms) {
  char msg[320];
  const char *host = strstr(url, "://");
  host = host ? host + 3 : "local";
  size_t nHost = strcspn(host, "/?#");
  const char *at = memchr(host, '@', nHost);
  if (at) { nHost -= (size_t)(at + 1 - host); host = at + 1; }
  snprintf(msg, sizeof msg, "logo: origem hash=%08lx fonte=%s host=%.*s total_ms=%u",
           hashCaminho(url), fonte, (int)nHost, host, (unsigned)ms);
  marco(msg);
}

// ARTE QUE NAO EXISTE NAO VOLTA A RODADA (05/10/2026, B3).
//
// MEDIDO nos registros D1 (364 pessoas desde 22/09): `[tex] download falhou`
// em 94 pessoas na LG, 38 no TPK, 33 no WGT — quase tudo 404 do metahub
// (images/episodes/live) e do bingecat para ids sem arte, mais curl 28 de
// ~8,5 s. O recuo por item (2 s/10 s/60 s, FALHOU) so vale enquanto o slot
// existe: despejado o slot, a falha some com ele, e a proxima abertura da tela
// baixa o MESMO 404 de novo, ocupando um dos fios de rede de arte enquanto a
// arte visivel espera na fila (fila-rede p50/p90/p99 0,6/2,4/10 s na LG).
//
// Aqui fica a memoria por URL, fora do slot: 404/410 vale 24 h e sobrevive ao
// reinicio (arquivo .arte-negativa na pasta do cache, so no nativo — o MEMFS
// do Tizen nao persiste, e la o XHR nao da o status aqui); o prazo estourado
// vale 30 s so na RAM, porque e da rede e nao do titulo. A chave e o FNV da URL
// realmente pedida (a variante de tamanho tem a sua).
//
// O PRAZO de 8 s NAO caiu: o download BEM-SUCEDIDO na LG tem p99 de 6,0 s
// (D1, 8.486 pedidos); um prazo de 5 s faria arte que hoje chega virar falha.
// O custo do prazo longo era repetir o mesmo host morto, e e isso que esta
// memoria corta.
#define NV_NEG_N 512
#define NV_NEG_404_S (24 * 3600)
#define NV_NEG_FALHA_S 30
typedef struct { unsigned long h; long ate; int fixa; } NegArte;
static NegArte negArte[NV_NEG_N];
static pthread_mutex_t negMtx = PTHREAD_MUTEX_INITIALIZER;
// pthread_once e nao flag solta (#203, TSAN: negTem x negTem em dois fios de
// download). Com a flag, o segundo fio via 1 antes de o primeiro terminar de
// ler o disco e consultava a tabela vazia; e dois podiam carregar juntos.
static pthread_once_t negCarregada = PTHREAD_ONCE_INIT;
static int negAcertos;   // pedidos que a memoria poupou nesta sessao
static void negCaminho(char *dst, size_t tam) {
  snprintf(dst, tam, "%s/.arte-negativa", dirCache);
}
static void negCarregar(void) {
#ifndef __EMSCRIPTEN__
  char cam[600];
  FILE *f;
  long agora = (long)time(NULL);
  int i = 0;
  if (!dirCache[0]) return;
  negCaminho(cam, sizeof cam);
  f = fopen(cam, "rb");
  if (!f) return;
  pthread_mutex_lock(&negMtx);
  { unsigned long h; long ate;
    while (i < NV_NEG_N && fread(&h, sizeof h, 1, f) == 1 && fread(&ate, sizeof ate, 1, f) == 1)
      if (ate > agora && ate - agora <= NV_NEG_404_S) {
        negArte[i].h = h; negArte[i].ate = ate; negArte[i].fixa = 1; i++;
      } }
  pthread_mutex_unlock(&negMtx);
  fclose(f);
  if (i) { printf("[tex] arte negativa: %d url(s) sem arte lembradas do disco\n", i); fflush(stdout); }
#endif
}
static void negGravar(void) {
#ifndef __EMSCRIPTEN__
  char cam[600], tmp[620];
  FILE *f;
  long agora = (long)time(NULL);
  int i;
  if (!dirCache[0]) return;
  negCaminho(cam, sizeof cam);
  snprintf(tmp, sizeof tmp, "%s.parcial", cam);
  f = fopen(tmp, "wb");
  if (!f) return;
  pthread_mutex_lock(&negMtx);
  for (i = 0; i < NV_NEG_N; i++)
    if (negArte[i].fixa && negArte[i].ate > agora) {
      fwrite(&negArte[i].h, sizeof negArte[i].h, 1, f);
      fwrite(&negArte[i].ate, sizeof negArte[i].ate, 1, f);
    }
  pthread_mutex_unlock(&negMtx);
  if (fclose(f) == 0) rename(tmp, cam); else remove(tmp);
#endif
}
static int negTem(const char *url) {
  unsigned long h = hashCaminho(url);
  long agora = (long)time(NULL);
  int i, r = 0;
  pthread_once(&negCarregada, negCarregar);
  pthread_mutex_lock(&negMtx);
  for (i = 0; i < NV_NEG_N; i++)
    if (negArte[i].h == h && negArte[i].ate > agora) { r = 1; negAcertos++; break; }
  pthread_mutex_unlock(&negMtx);
  return r;
}
__attribute__((unused)) static void negPor(const char *url, int segundos, int fixa) {
  unsigned long h = hashCaminho(url);
  long agora = (long)time(NULL);
  int i, alvo = 0;
  pthread_mutex_lock(&negMtx);
  for (i = 0; i < NV_NEG_N; i++) {
    if (negArte[i].h == h) { alvo = i; break; }
    if (negArte[i].ate < negArte[alvo].ate) alvo = i;   // vencida/livre primeiro
  }
  negArte[alvo].h = h; negArte[alvo].ate = agora + segundos; negArte[alvo].fixa = fixa;
  pthread_mutex_unlock(&negMtx);
  if (fixa) negGravar();
}
int tex_negativas_poupadas(void) { return negAcertos; }

static char *baixarImagem(const char *url, long *n, TexFetchTrace *trace) {
  char urlLogBuf[160];
  char *corpo;
  // URL VIRTUAL DE FONTE (artereserva.h): o fundo do TMDB/Trakt de um item que
  // so trouxe o do catalogo. Resolve aqui, no fio de rede, e baixa a real; o
  // chamador continua gravando sob a virtual, entao a consulta nao se repete.
  // -1 = nao ha essa arte: falha como um 404, e quem desenha cai na seguinte.
  { char real[NV_TEX_URL_MAX];
    int r;
    Uint32 t = SDL_GetTicks();
    r = arte_fonte_resolver(url, real, sizeof real);
    if (trace) trace->resolveMs += SDL_GetTicks() - t;
    if (r < 0) {
      printf("[tex] fonte sem fundo para: %s\n", urlLog(url, urlLogBuf, sizeof urlLogBuf));
      fflush(stdout);
      return NULL;
    }
    if (r > 0) return baixarImagem(real, n, trace);
  }
  // Ja se sabe que esta URL nao tem arte (ver negTem): falha na hora, sem
  // ocupar o fio — quem desenha cai na proxima fonte como num 404.
  if (negTem(url)) return NULL;
  long tetoAnterior = rede_teto;
  rede_teto = (long)cachePerfil().corpoMb * 1048576;
  // 8 s e nao 25: isto e uma IMAGEM. Com 25 s, duas URLs mortas seguravam os
  // dois fios de decode por quase um minuto e a tela inteira parava de receber
  // arte — repetidamente, porque nada guarda a falha.
  { Uint32 t = SDL_GetTicks();
#ifdef __EMSCRIPTEN__
    // SAMSUNG (#112): o Chromium do Tizen barra TODO `http://` que sai do app
    // (log D1 1647: os icones de canal de 24horas.cc, http, nunca chegavam).
    // O servico de recomendacoes (https) busca por nos e so devolve se o
    // servidor disser image/* — ver servidor/recomendacoes/src/xtream.js. A
    // chave do cache continua sendo `url` (quem grava e o chamador): so o
    // caminho da rede muda. Sem NV_REC_URL na build, direto como antes. A
    // url vai no CORPO do POST: ha painel que poe usuario/senha no caminho do
    // icone, e url de entrada o proprio worker registra (ver xtream.js).
    if (NV_REC_URL[0] && !strncmp(url, "http://", 7))
      corpo = rede_postar_bin(NV_REC_URL "/v1/xtream", 8, url, n);
    else
#endif
    if (posterprov_e_provedor(url)) {
      // PROVEDOR DE POSTER (posterprov.h): no maximo PP_SIMULT juntos, e prazo
      // de 20 s porque um cartaz frio e RENDERIZADO no servidor (medido: ~3 s
      // na instancia publica, ate 30 s no limite dela). O portao fica so em
      // volta do download; esperar vaga nao gasta o prazo.
      posterprov_portao_entrar();
      corpo = rede_baixar_bin(url, 20, n);
      posterprov_portao_sair();
    } else {
#if !defined(__EMSCRIPTEN__) && !defined(rede_baixar_bin)
      // (Os testes trocam rede_baixar_bin por um dublê: sem status ali.)
      // Com o STATUS na mao (rede_baixar_bin devolve so NULL): e ele que
      // separa o 404 do titulo sem arte (24 h) da rede que falhou (2 min).
      RedeMedida md;
      memset(&md, 0, sizeof md);
      corpo = rede_baixar_bin_medido_controle(url, 8, NULL, NULL, n, &md);
      if (corpo && (md.status < 200 || md.status >= 300)) {
        char seg[120];
        printf("[rede] HTTP %d em %s\n", md.status, rede_url_publica(url, seg, sizeof seg));
        free(corpo); corpo = NULL; *n = 0;
      }
      if (!corpo && (md.status == 404 || md.status == 410)) negPor(url, NV_NEG_404_S, 1);
      // Prazo estourado (sem resposta, quase o prazo inteiro): o host esta
      // morto AGORA. 30 s, e nao mais: o recuo do item (2/10/60 s) ainda
      // ganha a tentativa de 60 s se a rede voltar.
      else if (!corpo && md.status == 0 && md.ms >= 6000) negPor(url, NV_NEG_FALHA_S, 0);
      // "SUCESSO" SEM CORPO (2xx com 0 B): o servidor respondeu, mas o que
      // veio nao e arte — corpo abortado/conexao cortada que a transferencia
      // conta como ok. Sem recuo, a mesma URL voltava a cada item para sempre.
      else if (!corpo && md.status >= 200 && md.status < 300) negPor(url, NV_NEG_FALHA_S, 0);
#else
      corpo = rede_baixar_bin(url, 8, n);
#endif
    }
    if (trace) {
      trace->netMs += SDL_GetTicks() - t;
      trace->netCalls++;
    } }
  rede_teto = tetoAnterior;
  // ESTE RAMO ERA MUDO. Medido numa navegacao da home: 93 "decode falhou" com
  // ZERO "[rede] falha" no log — todas as falhas passavam por aqui, com o curl
  // dizendo sucesso e um corpo curto demais para ser imagem. Sem a linha nao
  // havia como distinguir "servidor recusou" de "cache sem permissao de
  // escrita" de "resposta vazia". Nao repete o caso do HTTP >= 400, que agora
  // o rede.c nomeia sozinho.
  if (!corpo || *n <= 512) {
    // O printf ESTAVA DENTRO DE `if (corpo)`, o que calava justamente o caso
    // mais comum: rede_baixar_bin devolvendo NULL. MEDIDO no alvo Tizen: 151
    // downloads tentados, ZERO linha de log e zero textura — com o comentario
    // logo acima afirmando que este ramo ja nao era mudo. Como nada guarda a
    // falha, cada quadro pedia de novo as mesmas URLs, para sempre.
    if (corpo) printf("[tex] corpo curto (%ld B): %s\n", *n, urlLog(url, urlLogBuf, sizeof urlLogBuf));
    else       printf("[tex] download falhou (sem corpo): %s\n", urlLog(url, urlLogBuf, sizeof urlLogBuf));
    fflush(stdout);
    // #205: corpo curto demais para ser imagem (0 B incluso) e resposta que
    // "deu certo": falha COM RECUO. Sem isto a URL refetchava a cada item
    // para sempre — no caminho Android o corpo chegava inteiro e media 0 B.
    if (corpo) negPor(url, NV_NEG_FALHA_S, 0);
    free(corpo);
    return NULL;
  }
  // ASSINATURA DE IMAGEM. rede.c nao confere status HTTP, entao um 404 com
  // pagina de erro de mais de 512 bytes era gravado como "imagem" e ficava no
  // cache de disco PARA SEMPRE — o item nunca mais teria arte, nem depois de o
  // servidor voltar. Aceita JPEG (FF D8), PNG (89 50 4E 47), GIF e RIFF/WEBP.
  { const unsigned char *b0 = (const unsigned char *)corpo;
    int ok = (*n > 4) && (
       (b0[0] == 0xFF && b0[1] == 0xD8) ||
       (b0[0] == 0x89 && b0[1] == 0x50 && b0[2] == 0x4E && b0[3] == 0x47) ||
       (b0[0] == 'G'  && b0[1] == 'I'  && b0[2] == 'F') ||
       (b0[0] == 'R'  && b0[1] == 'I'  && b0[2] == 'F'  && b0[3] == 'F' &&
        *n > 12 && b0[8] == 'W' && b0[9] == 'E' && b0[10] == 'B' && b0[11] == 'P'));
    if (!ok) {
      printf("[tex] resposta nao e imagem (%ld B): %s\n", *n, urlLog(url, urlLogBuf, sizeof urlLogBuf));
      fflush(stdout);
      free(corpo);
      return NULL;
    } }
  // Assinatura dos bytes: "o destaque baixou a mesma imagem do card?"
  // (artereserva.h, arte_mesma_imagem). Um FNV do corpo que ja esta na mao.
  arte_bytes_registrar(url, corpo, *n);
  return corpo;
}

static int resolverReserva(const char *url, char *saida, size_t tam,
                           TexFetchTrace *trace) {
  Uint32 t = SDL_GetTicks();
  int ok = arte_reserva_url(url, saida, tam);
  if (trace) trace->resolveMs += SDL_GetTicks() - t;
  return ok;
}

// Baixa a URL para o cache, se ainda nao estiver la. Devolve 1 se ha arquivo
// utilizavel no fim. Roda no fio de decodificacao, entao bloquear aqui nao
// custa quadro nenhum.
#ifdef __EMSCRIPTEN__
// Grava `corpo` em `dst` por temporario + rename. 1 se o arquivo ficou.
static int gravarLocal(const char *dst, const char *corpo, long n,
                       TexFetchTrace *trace) {
  char tmp[600];
  Uint32 persistEm = SDL_GetTicks();
  int ok = 0, erro = 0;
  long anterior = 0;
  struct stat st;
  size_t esc = 0;
  int fim = 0;
  FILE *f;
  snprintf(tmp, sizeof tmp, "%s.parcial", dst);
  if (stat(dst, &st) == 0) anterior = (long)st.st_size;
  errno = 0;
  f = fopen(tmp, "wb");
  if (f) {
    esc = fwrite(corpo, 1, (size_t)n, f);
    erro = errno;
    fim = fclose(f);
    if (fim != 0 && !erro) erro = errno;
    if (esc == (size_t)n && fim == 0) {
      if (rename(tmp, dst) == 0) ok = 1;
      else erro = errno;
    }
  } else erro = errno;
  if (!ok) {
    if (!erro) erro = EIO;
    printf("[tex] gravacao incompleta (%zu de %ld B, erro %d: %s): %.70s\n",
           esc, n, erro, strerror(erro), dst);
    fflush(stdout);
    remove(tmp);
  } else {
    cacheDiscoBytes += n - anterior;
    publicarCacheDisco();
    arqDiscoRegistrar(dst);
  }
  if (trace) trace->persistMs += SDL_GetTicks() - persistEm;
  return ok;
}

static int garantirLocal(const char *url, char *dst, size_t tam, int *foiRede,
                         TexFetchTrace *trace) {
  FILE *f;
  char *corpo;
  long n = 0;
  if (foiRede) *foiRede = 0;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
    snprintf(dst, tam, "%s", url);
    return 1;
  }
  if (!dirCache[0]) return 0;
  nomeDeCache(url, dst, tam);
  Uint32 cacheEm = SDL_GetTicks();
  f = fopen(dst, "rb");
  if (f) { fseek(f, 0, SEEK_END); n = ftell(f); fclose(f);
    if (n > 512) {
      unsigned char sig[12] = {0};
      FILE *check = fopen(dst, "rb");
      size_t got = check ? fread(sig, 1, sizeof sig, check) : 0;
      if (check) fclose(check);
      { int valid = got >= 4 && (
          (sig[0] == 0xFF && sig[1] == 0xD8) ||
          (sig[0] == 0x89 && sig[1] == 0x50 && sig[2] == 0x4E && sig[3] == 0x47) ||
          (sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F') ||
          (sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'F' && sig[3] == 'F' &&
           got >= 12 && sig[8] == 'W' && sig[9] == 'E' && sig[10] == 'B' && sig[11] == 'P'));
        if (valid) {
          marcarUso(dst);   // sem isto a poda vira o contrario de LRU; ver a nota
          arqDiscoRegistrar(dst);
          if (trace) trace->cacheMs += SDL_GetTicks() - cacheEm;
          return 1;
        }
      }
    } }
  if (trace) trace->cacheMs += SDL_GetTicks() - cacheEm;
  if (foiRede) *foiRede = 1;
  corpo = baixarImagem(url, &n, trace);
  // METAHUB FORA DO AR NAO E CARD CINZA: a mesma imagem existe no TMDB pelo id
  // do IMDb. So depois de a original falhar, e gravada sob a URL original —
  // para o resto do app e como se o metahub tivesse respondido.
  if (!corpo) {
    char alt[400];
    if (resolverReserva(url, alt, sizeof alt, trace)) corpo = baixarImagem(alt, &n, trace);
  }
  if (!corpo) return 0;
  if (!gravarLocal(dst, corpo, n, trace)) { free(corpo); return 0; }
  free(corpo);
  return 1;
}

#else
// ACERTO DE DISCO SEM TRAVA GLOBAL (22/09/2026). Um fopen, 12 bytes de
// assinatura e o tamanho. Antes isto pegava discoMtx, a mesma trava que o
// download de outro fio segurava durante a gravacao e as duas varreduras da
// pasta; na C9 um acerto chegou a esperar 11719 ms (`phase=disk-cache
// cache_ms=11719`), e o decode, que chama garantirLocal para traduzir a URL,
// herdava a espera como `ler 4479`. A poda nao apaga arquivo de item PENDENTE
// (discoProtegido), entao o arquivo achado aqui continua la ate o decode le-lo.
static int acertoDisco(const char *dst, TexFetchTrace *trace) {
  Uint32 cacheEm = SDL_GetTicks();
  unsigned char sig[12] = {0};
  size_t got = 0;
  long n = 0;
  int valido = 0;
  FILE *f = fopen(dst, "rb");
  if (f) {
    got = fread(sig, 1, sizeof sig, f);
    if (!fseek(f, 0, SEEK_END)) n = ftell(f);
    fclose(f);
    valido = n > 512 && got >= 4 && (
        (sig[0] == 0xFF && sig[1] == 0xD8) ||
        (sig[0] == 0x89 && sig[1] == 0x50 && sig[2] == 0x4E && sig[3] == 0x47) ||
        (sig[0] == 'G' && sig[1] == 'I' && sig[2] == 'F') ||
        (sig[0] == 'R' && sig[1] == 'I' && sig[2] == 'F' && sig[3] == 'F' &&
         got >= 12 && sig[8] == 'W' && sig[9] == 'E' && sig[10] == 'B' && sig[11] == 'P'));
    if (valido) {
      cachearte_nativo_hit();
      cachearte_nativo_indice_tocar(dst);
      /* Evita uma escrita de metadados a cada card/quadro; o mtime e o LRU
       * entre sessoes, o indice e o LRU desta. */
      { struct stat st;
        if (!stat(dst, &st) && time(NULL) - st.st_mtime >= 60) utime(dst, NULL); }
    } else {
      remove(dst);
      cachearte_nativo_indice_remover(dst);
      publicarDiscoNativo();
    }
  }
  if (!valido) cachearte_nativo_miss();
  if (trace) trace->cacheMs += SDL_GetTicks() - cacheEm;
  return valido;
}

static int garantirLocal(const char *url, char *dst, size_t tam, int *foiRede,
                         TexFetchTrace *trace) {
  char *corpo;
  long n = 0;
  int ok;
  if (foiRede) *foiRede = 0;
  if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
    snprintf(dst, tam, "%s", url);
    return 1;
  }
  if (!dirCache[0]) return 0;
  nomeDeCache(url, dst, tam);
  if (acertoDisco(dst, trace)) return 1;
  if (foiRede) *foiRede = 1;
  corpo = baixarImagem(url, &n, trace);
  // METAHUB FORA DO AR NAO E CARD CINZA: a mesma imagem existe no TMDB pelo id
  // do IMDb. So depois de a original falhar, e gravada sob a URL original —
  // para o resto do app e como se o metahub tivesse respondido.
  if (!corpo) {
    char alt[400];
    if (resolverReserva(url, alt, sizeof alt, trace)) corpo = baixarImagem(alt, &n, trace);
  }
  if (!corpo) return 0;
  // SINCRONO de proposito, e so aqui: este caminho e o do GIF (gif.c le por
  // caminho, o arquivo tem de existir ao voltar) e o recuo do decode. O
  // caminho quente do LG e baixarParaItem, que entrega da memoria.
  { Uint32 persistEm = SDL_GetTicks();
    ok = gravarArquivo(dst, ".parcial", (const unsigned char *)corpo, n);
    if (trace) trace->persistMs += SDL_GetTicks() - persistEm; }
  free(corpo);
  return ok;
}

#endif

// O QUE O FIO DE REDE ENTREGA AO DE DECODE. Arte baixada agora: os bytes no
// proprio item (Item.bruto), nas duas plataformas. No LG, arte que ja estava
// no cache de disco: o arquivo. GIF e caminho local seguem pelo arquivo.
// OS BYTES SO ENTRAM NO ITEM SE ELE AINDA E O MESMO PEDIDO (23/09/2026, #116).
// O download roda sem a trava por segundos; se nesse meio tempo o slot for
// esquecido e reaproveitado por outro caminho (um icone local, por exemplo),
// gravar os bytes antigos nele faria o decode desenhar a imagem ERRADA no lugar
// do icone — e ela ficaria no cache de texturas com o nome do icone. Chamada
// com o mutex travado. `pedido` e o caminho que o fio tirou da fila.
static int mesmoPedido(int idx, const char *pedido) {
  return itens[idx].estado == PENDENTE && pedido &&
         !strcmp(itens[idx].caminho, pedido);
}

// Under mtx: transfer ownership only to an idle consumer of the exact URL.
static int salvarPedidoMovido(const char *pedido, const char *url, int limite,
                              unsigned char *corpo, long n, int variante) {
  for (int i = 0; i < nMax; i++) {
    Item *it = &itens[i];
    if (!mesmoPedido(i, pedido) || it->naFilaDec || !it->filaRedeEm ||
        it->bruto || limiteDaVariante(it) != limite) continue;
    if (!enfileirarDecodeSemEspera(i)) return 0;
    it->bruto = corpo; it->nBruto = n;
    it->limiteTamanho = strcmp(url, pedido) ? limite : 0;
    it->varianteCache = variante;
    snprintf(it->urlCache, sizeof it->urlCache, "%s", url);
    return 1;
  }
  return 0;
}

static int baixarParaItem(int idx, const char *url, char *dst, size_t tam, int *foiRede,
                          TexFetchTrace *trace) {
  const char *pedido = url;   // o caminho do item; `url` pode virar a variante
  char certo[NV_TEX_URL_MAX];
  int limitePedido;
  SDL_LockMutex(mtx); limitePedido = limiteDaVariante(&itens[idx]); SDL_UnlockMutex(mtx);
#ifdef __EMSCRIPTEN__
  if (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8)) {
    long n = 0;
    unsigned char *corpo;
    char menor[600];
    int variante = NV_CACHE_ARTE_MEDIUM;
    // O CARD NAO PRECISA DO FUNDO DE 1920 (20/09/2026, #72). O metahub serve
    // /background/small/ em 480x270 e 12 KB contra 1920x1080 e 600 KB do
    // /medium/ — o cabecalho de artehero.c mediu medium/big/large/original
    // iguais, mas nao mediu small. Um card deitado desenha 416 (768 em
    // foco), e nesta TV cada medium era 600 KB de rede mais um JPEG HD para
    // decodificar em software. So aqui, so no Tizen, so quando o pedido e de
    // card (limite <= 640): o item continua com a URL medium como chave, e a
    // promocao a heroi baixa o medium de novo, que e o que ela ja fazia.
    { int limite = limitePedido;
      if (arte_tamanho_fundo_pequeno(limite, qualidadeImg == 2)) {
        variante = NV_CACHE_ARTE_SMALL;
        const char *m = strstr(url, "images.metahub.space/background/medium/");
        if (m) {
          snprintf(menor, sizeof menor, "%.*simages.metahub.space/background/small/%s",
                   (int)(m - url), url, m + strlen("images.metahub.space/background/medium/"));
          url = menor;
        }
      } }
    // O RESTO DA ESCADA (TMDB, still do metahub): artetamanho.h.
    if (arte_tamanho_url(url, limitePedido, certo, sizeof certo)) url = certo;
    SDL_LockMutex(mtx);
    if (mesmoPedido(idx, pedido)) itens[idx].limiteTamanho = url != pedido ? limitePedido : 0;
    SDL_UnlockMutex(mtx);
    // JA ESTA NA PASTA (um GIF de outra vez ou desta sessao): o arquivo serve,
    // sem rede. Ver arqDiscoRegistrar.
    { char arq[600];
      nomeDeCache(url, arq, sizeof arq);
      if (dirCache[0] && arqDiscoTem(arq)) return garantirLocal(url, dst, tam, foiRede, trace); }
    /* The exact URL, including its query string, remains the key. `variante`
     * is a second dimension so a small response can never satisfy a hero. */
    corpo = NULL;
    if (cachearte_buscar(url, variante, &corpo, &n)) {
      if (foiRede) *foiRede = 0;
    } else {
      if (foiRede) *foiRede = 1;
      corpo = (unsigned char *)baixarImagem(url, &n, trace);
    }
    if (!corpo) {
      char alt[400];
      if (resolverReserva(url, alt, sizeof alt, trace)) corpo = (unsigned char *)baixarImagem(alt, &n, trace);
    }
    if (!corpo) return 0;
    if (n >= 6 && !memcmp(corpo, "GIF8", 4)) {
      // gif.c le por caminho: este continua indo a arquivo. COM OS BYTES QUE
      // JA VIERAM: ate a 1.4.5 eles eram jogados fora e garantirLocal baixava
      // o GIF inteiro de novo (ver arqDiscoRegistrar).
      int ok;
      if (!dirCache[0]) { free(corpo); return 0; }
      nomeDeCache(url, dst, tam);
      ok = gravarLocal(dst, (const char *)corpo, n, trace);
      free(corpo);
      return ok;
    }
    // WEBP ANIMADO (#141): alem da textura parada de sempre (o Worker le o
    // primeiro quadro), o arquivo para gif.c animar — ele le por caminho, como
    // o GIF acima. So quando este build anima WebP; senao nada muda.
    if (gif_webp_suportado() && gif_webp_animado_bytes(corpo, (size_t)n) && dirCache[0]) {
      char arq[600];
      nomeDeCache(url, arq, sizeof arq);
      gravarLocal(arq, (const char *)corpo, n, trace);
    }
    /* JPEG/PNG/WebP bytes are already compressed. Persist the response as-is;
     * decoding remains the existing worker path and no raw RGBA is stored. */
    if (foiRede && *foiRede) cachearte_salvar(url, variante, corpo, n, 0);
    SDL_LockMutex(mtx);
    if (!mesmoPedido(idx, pedido)) {
      int salvo = salvarPedidoMovido(pedido, url, limitePedido, corpo, n, variante);
      SDL_UnlockMutex(mtx);
      if (!salvo) free(corpo);
      return -1;
    }
    free(itens[idx].bruto);
    itens[idx].bruto = corpo;
    itens[idx].nBruto = n;
    itens[idx].varianteCache = variante;
    snprintf(itens[idx].urlCache, sizeof itens[idx].urlCache, "%s", url);
    SDL_UnlockMutex(mtx);
    snprintf(dst, tam, "%s", url);
    return 1;
  }
#else
  // LG: A ARTE NAO ESPERA O DISCO (22/09/2026). Ordem nova:
  //   1. arquivo no cache -> o decode le o arquivo (sem trava global);
  //   2. bytes ainda na fila de gravacao -> copia, sem rede;
  //   3. rede -> os bytes vao para Item.bruto e o decode comeca ja; a copia
  //      para o disco entra na fila de fundo (enfileirarGravacao).
  // Antes o passo 3 gravava, rodava duas varreduras da pasta e so entao
  // devolvia: na C9, `net_ms=962 persist_ms=8786` num mesmo fetch, e o
  // destaque desistia aos 613 ms com a imagem ja na memoria.
  // GIF continua sincrono por garantirLocal: gif.c le por caminho.
  if (dirCache[0] && (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8))) {
    unsigned char *corpo = NULL;
    long n = 0;
    // A VARIANTE DO TAMANHO DO DESENHO (artetamanho.h), antes do nome de
    // cache: o arquivo no disco e o da variante, e a promocao a heroi, com
    // teto maior, procura outro nome e baixa o maior.
    { int usar = arte_tamanho_url(url, limitePedido, certo, sizeof certo);
      if (usar) url = certo;
      SDL_LockMutex(mtx);
      if (mesmoPedido(idx, pedido)) itens[idx].limiteTamanho = usar ? limitePedido : 0;
      SDL_UnlockMutex(mtx); }
    nomeDeCache(url, dst, tam);
    if (foiRede) *foiRede = 0;
    if (acertoDisco(dst, trace)) return 1;
    if (!gravacaoPendente(dst, &corpo, &n)) {
      if (foiRede) *foiRede = 1;
      corpo = (unsigned char *)baixarImagem(url, &n, trace);
      if (!corpo) {
        char alt[400];
        if (resolverReserva(url, alt, sizeof alt, trace))
          corpo = (unsigned char *)baixarImagem(alt, &n, trace);
      }
      if (!corpo) return 0;
      if (n >= 6 && !memcmp(corpo, "GIF8", 4)) {
        Uint32 persistEm = SDL_GetTicks();
        int ok = gravarArquivo(dst, ".parcial", corpo, n);
        if (trace) trace->persistMs += SDL_GetTicks() - persistEm;
        free(corpo);
        return ok;
      }
      { Uint32 persistEm = SDL_GetTicks();
        enfileirarGravacao(dst, corpo, n);
        if (trace) trace->persistMs += SDL_GetTicks() - persistEm; }
    }
    SDL_LockMutex(mtx);
    // Backpressure compressed responses too; at most the network workers keep
    // one bounded response each while waiting for the decode queue to drain.
    while (rodando && mesmoPedido(idx, pedido)) {
      long pending = n;
      for (int i = 0; i < nMax; i++) pending += itens[i].nBruto;
      if (pending <= orcamento / 4) break;
      SDL_CondWaitTimeout(condLivre, mtx, 20);
    }
    if (!rodando) { SDL_UnlockMutex(mtx); free(corpo); return 0; }
    if (!mesmoPedido(idx, pedido)) {
      int salvo = salvarPedidoMovido(pedido, url, limitePedido, corpo, n, NV_CACHE_ARTE_MEDIUM);
      SDL_UnlockMutex(mtx);
      if (!salvo) free(corpo);
      return -1;
    }
    free(itens[idx].bruto);
    itens[idx].bruto = corpo;
    itens[idx].nBruto = n;
    snprintf(itens[idx].urlCache, sizeof itens[idx].urlCache, "%s", url);
    SDL_UnlockMutex(mtx);
    return 1;
  }
#endif
  return garantirLocal(url, dst, tam, foiRede, trace);
}

// FIO DE REDE: tira da fila, garante o arquivo no cache de disco e passa para a
// decodificacao. Nao toca em pixel nenhum, entao pode rodar em prioridade
// normal e em varios — o que ele faz e ESPERAR.
static int threadRede(void *arg) {
  int meu = (int)(intptr_t)arg;
  for (;;) {
    int idx;
    char caminho[NV_TEX_URL_MAX], local[600];
    Uint32 filaEm = 0, filaWait = 0;
    int urgente = 0;
    SDL_LockMutex(mtx);
    while (rodando && (filaIni == filaFim || meu >= fiosRedeAtivos)) SDL_CondWait(cond, mtx);
    if (!rodando) { SDL_UnlockMutex(mtx); return 0; }
    idx = tirarFila(fila, &filaIni, filaFim);
    if (itens[idx].naFilaDec || itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (!itens[idx].naFilaDec && itens[idx].estado == PENDENTE) desistir(idx);
      SDL_UnlockMutex(mtx);
      continue;
    }
    strncpy(caminho, itens[idx].caminho, sizeof caminho - 1);
    caminho[sizeof caminho - 1] = 0;
    filaEm = itens[idx].filaRedeEm;
    itens[idx].filaRedeEm = 0;
    urgente = itens[idx].urgente;
    SDL_UnlockMutex(mtx);

    if (filaEm) {
      filaWait = SDL_GetTicks() - filaEm;
      if (urgente == 2 || filaWait >= NV_TEX_TRACE_MS)
        printf("[tex-trace] fila-rede hash=%08lx wait=%u urgente=%d\n",
               hashCaminho(caminho), (unsigned)filaWait, urgente);
    }

    // Caminho local devolve na hora; so URL sai para a rede.
    SDL_LockMutex(mtx);
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) desistir(idx);
      SDL_UnlockMutex(mtx);
      continue;
    }
    SDL_UnlockMutex(mtx);

    { Uint32 redeEm = SDL_GetTicks();
      int foiRede = 0;
      TexFetchTrace trace = {0, 0, 0, 0, 0};
      int baixou = baixarParaItem(idx, caminho, local, sizeof local, &foiRede, &trace);
      Uint32 redeMs = SDL_GetTicks() - redeEm;
      if (urgente == 2) logoOrigem(caminho, foiRede ? "rede" : "disco", redeMs);
      if (!baixou || urgente == 2 || redeMs >= NV_TEX_TRACE_MS)
        printf("[tex-trace] fetch hash=%08lx phase=%s total_ms=%u net_ms=%u resolve_ms=%u cache_ms=%u persist_ms=%u image_requests=%u ok=%d urgente=%d\n",
               hashCaminho(caminho), foiRede ? "http" : "disk-cache",
               (unsigned)redeMs, (unsigned)trace.netMs, (unsigned)trace.resolveMs,
               (unsigned)trace.cacheMs, (unsigned)trace.persistMs, trace.netCalls,
               baixou, urgente);
      // -1: the original slot changed; bytes were transferred or freed.
      // The replacement slot is never touched.
      if (baixou < 0) continue;
      if (!baixou) {
      // Falhou o download. Marca como falha AQUI para o recuo valer — antes o
      // decode e que marcava, e ate la o item ficava PENDENTE ocupando slot.
      SDL_LockMutex(mtx);
      if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
        if (itens[idx].estado == PENDENTE) desistir(idx);
        SDL_UnlockMutex(mtx);
        continue;
      }
      // Promocao que nao baixou: a textura pequena continua valendo. Marcar
      // FALHOU aqui apagaria da tela uma arte que existe.
      if (itens[idx].tex) { desistir(idx); SDL_UnlockMutex(mtx); continue; }
      itens[idx].estado = FALHOU;
      itens[idx].urgente = 0;
      itens[idx].falhas++;
      itens[idx].tentarEm = SDL_GetTicks() +
          (itens[idx].falhas == 1 ? 2000 : (itens[idx].falhas == 2 ? 10000 : 60000));
      SDL_UnlockMutex(mtx);
      continue;
      }
    }
    // A MAGICA do que chegou (ver Item.magica). Dos bytes quando vieram para a
    // memoria; do arquivo quando o que chegou foi arquivo (GIF, e no LG o
    // acerto de disco). Lido FORA da trava: e um fopen.
    { unsigned char mag[4] = {0, 0, 0, 0};
      int temArq = 0;
      FILE *g;
      SDL_LockMutex(mtx);
      temArq = !(itens[idx].bruto && itens[idx].nBruto >= 4);
      SDL_UnlockMutex(mtx);
      if (temArq && (g = fopen(local, "rb")) != NULL) {
        temArq = fread(mag, 1, 4, g) == 4 ? 2 : 0;
        fclose(g);
      }
      SDL_LockMutex(mtx);
      if (itens[idx].estado == PENDENTE && !pedidoObsoleto(&itens[idx])) {
        if (itens[idx].bruto && itens[idx].nBruto >= 4) {
          memcpy(itens[idx].magica, itens[idx].bruto, 4); itens[idx].temMagica = 1;
        } else if (temArq == 2) {
          memcpy(itens[idx].magica, mag, 4); itens[idx].temMagica = 1;
        }
      }
      SDL_UnlockMutex(mtx); }
    SDL_LockMutex(mtx);
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) desistir(idx);
      SDL_UnlockMutex(mtx);
      continue;
    }
    paraDecode(idx);
    SDL_UnlockMutex(mtx);
  }
}

// REDUZ POR MEDIA DE AREA. Cada pixel de destino e a media de TODOS os pixels
// de origem que caem na sua celula, pesada pelo alpha (logo com borda
// transparente nao ganha franja escura).
//
// Existe porque SDL_BlitScaled NAO faz media de vizinhos, ao contrario do que
// o comentario que estava aqui dizia: SDL_UpperBlitScaled chama o caminho com
// SDL_ScaleModeNearest (SDL_surface.c, conferido no 2.32.10), e o vizinho mais
// proximo DESCARTA colunas e linhas inteiras. Um backdrop de 1920 para o teto
// de 1280 perdia uma coluna em cada tres e saia serrilhado; a GPU o esticava
// de volta com GL_LINEAR, e o resultado era serrilhado E borrado — o "lavado,
// baixa qualidade" do #54. Nos posteres (342 ou 500 para 212) o mesmo.
//
// A conversao de formato e feita POR FAIXA de linhas, e nao na imagem inteira:
// a copia intermediaria em tamanho cheio era o que estourava o heap de 256 MiB
// do Tizen com backdrops de 3840x2160 (33 MB cada). Aqui o pico e a fonte mais
// uma faixa de poucas linhas.
// REDUCAO ATE 2x POR BILINEAR, em ponto fixo, direto sobre ABGR8888.
//
// MEDIDO na C9 em 19/09: a media de area (abaixo) levava 1.212 ms para
// trazer 2560x1440 a 1920x1080 — 330 ns por pixel de origem, com
// acumuladores de 64 bits num ARM de 32 — e 100 a 190 ms para 960x540 -> 544.
// Com a decodificacao escalada (jpegrapido.c) quase toda reducao passou a ser
// de menos de 2x, e nessa faixa quatro amostras por pixel de SAIDA dao o
// mesmo resultado a olho e custam uma fracao: sao 2 milhoes de pixels de
// saida a ~30 ciclos em vez de 3,7 milhoes de origem a 360. A media de area
// continua sendo o caminho para razoes maiores (PNG grande, webp), onde a
// bilinear pularia pixels e serrilharia.
static SDL_Surface *reduzirBilinear(SDL_Surface *src, int lw, int lh) {
  SDL_Surface *conv = NULL, *dst;
  const unsigned char *sp; int spitch, sw = src->w, sh = src->h;
  unsigned fx, fy, ox, oy;
  if (src->format->format != SDL_PIXELFORMAT_ABGR8888) {
    conv = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ABGR8888, 0);
    if (!conv) return NULL;
    src = conv;
  }
  dst = nv_superficie(0, lw, lh, 32, SDL_PIXELFORMAT_ABGR8888);
  if (!dst) { if (conv) SDL_FreeSurface(conv); return NULL; }
  if (SDL_MUSTLOCK(src)) SDL_LockSurface(src);
  sp = src->pixels; spitch = src->pitch;
  // Passo em 16.16; a amostra cai no CENTRO do pixel de saida mapeado na
  // origem (o -0.5 dos dois lados), como manda a bilinear.
  fx = (unsigned)(((unsigned long long)sw << 16) / (unsigned)lw);
  fy = (unsigned)(((unsigned long long)sh << 16) / (unsigned)lh);
  for (oy = 0; oy < (unsigned)lh; oy++) {
    unsigned char *out = (unsigned char *)dst->pixels + (size_t)oy * (size_t)dst->pitch;
    long long syf = (long long)oy * fy + (fy >> 1) - 32768; if (syf < 0) syf = 0;
    unsigned sy = (unsigned)(syf >> 16), wy = (unsigned)(syf & 0xFFFF) >> 8;
    unsigned sy1 = sy + 1 < (unsigned)sh ? sy + 1 : sy;
    const unsigned char *r0 = sp + (size_t)sy * (size_t)spitch, *r1 = sp + (size_t)sy1 * (size_t)spitch;
    for (ox = 0; ox < (unsigned)lw; ox++) {
      long long sxf = (long long)ox * fx + (fx >> 1) - 32768; if (sxf < 0) sxf = 0;
      unsigned sx = (unsigned)(sxf >> 16), wx = (unsigned)(sxf & 0xFFFF) >> 8;
      unsigned sx1 = sx + 1 < (unsigned)sw ? sx + 1 : sx;
      const unsigned char *a = r0 + sx * 4, *b = r0 + sx1 * 4, *c = r1 + sx * 4, *d = r1 + sx1 * 4;
      unsigned w00 = (256 - wx) * (256 - wy), w10 = wx * (256 - wy), w01 = (256 - wx) * wy, w11 = wx * wy;
      unsigned k;
      for (k = 0; k < 4; k++)
        out[ox * 4 + k] = (unsigned char)((a[k] * w00 + b[k] * w10 + c[k] * w01 + d[k] * w11 + 32768) >> 16);
    }
  }
  if (SDL_MUSTLOCK(src)) SDL_UnlockSurface(src);
  if (conv) SDL_FreeSurface(conv);
  return dst;
}

SDL_Surface *tex_reduzir(SDL_Surface *src, int lw, int lh) {
  SDL_Surface *dst;
  int sw = src->w, sh = src->h;
  if (lw <= 0 || lh <= 0 || sw <= 0 || sh <= 0) return NULL;
  if (sw <= 2 * lw && sh <= 2 * lh && !SDL_ISPIXELFORMAT_INDEXED(src->format->format))
    return reduzirBilinear(src, lw, lh);
  if (SDL_ISPIXELFORMAT_INDEXED(src->format->format)) {
    // SDL_ConvertPixels nao carrega a paleta. Imagem indexada e PNG pequeno
    // (logo); converter inteira aqui nao custa.
    SDL_Surface *c = SDL_ConvertSurfaceFormat(src, SDL_PIXELFORMAT_ABGR8888, 0);
    if (!c) return NULL;
    dst = tex_reduzir(c, lw, lh);
    SDL_FreeSurface(c);
    return dst;
  }
  dst = nv_superficie(0, lw, lh, 32, SDL_PIXELFORMAT_ABGR8888);
  if (!dst) return NULL;
  // COBERTURA FRACIONARIA, em pesos de 0 a 256. Com razao 1,5 (1920 -> 1280)
  // uma celula inteira de pixels daria 1 pixel de origem em um quarto das
  // celulas — e esses sairiam sem media nenhuma. O pixel de origem que a
  // celula corta ao meio entra com metade do peso, e ai todo pixel de destino
  // e media de verdade.
  //
  // Passo horizontal em 32 bits por linha de origem (255*255*256 por pixel,
  // vezes a largura da celula — cabe ate celulas de ~65 pixels); passo
  // vertical em 64 bits, porque soma linhas ja somadas vezes outro peso.
  int faixaH = sh / lh + 3;
  size_t faixaPitch = (size_t)sw * 4;
  unsigned char *faixa = (unsigned char *)malloc(faixaPitch * (size_t)faixaH);
  unsigned *lin = (unsigned *)malloc(sizeof(unsigned) * (size_t)lw * 5);
  unsigned long long *acc =
      (unsigned long long *)malloc(sizeof(unsigned long long) * (size_t)lw * 5);
  // Por coluna de destino: x0, x1 (exclusivo), peso do primeiro, peso do ultimo.
  int *cx = (int *)malloc(sizeof(int) * (size_t)lw * 4);
  if (!faixa || !lin || !acc || !cx) {
    free(faixa); free(lin); free(acc); free(cx);
    SDL_FreeSurface(dst);
    return NULL;
  }
  int ox, oy;
  for (ox = 0; ox < lw; ox++) {
    double ini = (double)ox * sw / lw, fim = (double)(ox + 1) * sw / lw;
    int x0 = (int)ini, x1 = (int)(fim + 0.999999);
    if (x1 <= x0) x1 = x0 + 1;
    if (x1 > sw) x1 = sw;
    double p0 = ((x0 + 1 < fim ? x0 + 1 : fim) - ini);
    double p1 = (fim - (x1 - 1 > ini ? x1 - 1 : ini));
    if (x1 - x0 == 1) p0 = p1 = fim - ini;
    cx[ox * 4] = x0; cx[ox * 4 + 1] = x1;
    cx[ox * 4 + 2] = (int)(p0 * 256.0 + 0.5);
    cx[ox * 4 + 3] = (int)(p1 * 256.0 + 0.5);
  }
  int precisaLock = SDL_MUSTLOCK(src);
  if (precisaLock) SDL_LockSurface(src);
  for (oy = 0; oy < lh; oy++) {
    double ini = (double)oy * sh / lh, fim = (double)(oy + 1) * sh / lh;
    int y0 = (int)ini, y1 = (int)(fim + 0.999999);
    if (y1 <= y0) y1 = y0 + 1;
    if (y1 > sh) y1 = sh;
    int n = y1 - y0, yy;
    if (n > faixaH) n = faixaH;
    SDL_ConvertPixels(sw, n, src->format->format,
                      (const unsigned char *)src->pixels + (size_t)y0 * src->pitch,
                      src->pitch, SDL_PIXELFORMAT_ABGR8888, faixa, (int)faixaPitch);
    memset(acc, 0, sizeof(unsigned long long) * (size_t)lw * 5);
    for (yy = 0; yy < n; yy++) {
      const unsigned char *ln = faixa + (size_t)yy * faixaPitch;
      int y = y0 + yy;
      double py = (y + 1 < fim ? y + 1 : fim) - (y > ini ? y : ini);
      unsigned wy = (unsigned)(py * 256.0 + 0.5);
      if (!wy) continue;
      for (ox = 0; ox < lw; ox++) {
        unsigned *a = lin + ox * 5;
        int x, x0 = cx[ox * 4], x1 = cx[ox * 4 + 1];
        a[0] = a[1] = a[2] = a[3] = a[4] = 0;
        for (x = x0; x < x1; x++) {
          const unsigned char *q = ln + (size_t)x * 4;   // ABGR8888: R,G,B,A
          unsigned w = x == x0 ? (unsigned)cx[ox * 4 + 2]
                     : x == x1 - 1 ? (unsigned)cx[ox * 4 + 3] : 256u;
          unsigned al = q[3] * w;
          a[0] += q[0] * al; a[1] += q[1] * al; a[2] += q[2] * al;
          a[3] += al; a[4] += w;
        }
      }
      for (ox = 0; ox < lw * 5; ox++) acc[ox] += (unsigned long long)lin[ox] * wy;
    }
    unsigned char *out = (unsigned char *)dst->pixels + (size_t)oy * dst->pitch;
    for (ox = 0; ox < lw; ox++) {
      const unsigned long long *a = acc + ox * 5;
      unsigned char *q = out + (size_t)ox * 4;
      if (a[3] > 0) {
        q[0] = (unsigned char)((a[0] + a[3] / 2) / a[3]);
        q[1] = (unsigned char)((a[1] + a[3] / 2) / a[3]);
        q[2] = (unsigned char)((a[2] + a[3] / 2) / a[3]);
        q[3] = (unsigned char)((a[3] + a[4] / 2) / a[4]);
      } else {
        q[0] = q[1] = q[2] = q[3] = 0;
      }
    }
  }
  if (precisaLock) SDL_UnlockSurface(src);
  free(faixa); free(lin); free(acc); free(cx);
  return dst;
}

// PIRAMIDE NA CPU (#201, 06/10/2026). Relato: TCL 55P6K, Android 12 (acho que
// Mali-G57, nao confirmado): foto de elenco, logo de estudio, o logo do IMDb e
// os cartazes empilhados da colecao saem com blocos de lixo; cartaz grande,
// desenhado perto do tamanho decodificado, sai perfeito. O que quebra e so o
// que aparece MENOR que a textura, ou seja o que amostra nivel > 0 da piramide
// feita por glGenerateMipmap. Mac, C9 (Mali-G71) e a TCL do dono (Mali-G52)
// nao mostram nada. A suspeita (NAO provada) e o driver gerar lixo no
// glGenerateMipmap de textura NPOT; o envio do nivel 0 foi conferido e nao
// tem defeito aparente (RGBA/RGBA, pitch = w*4, alinhamento irrelevante em 4
// bytes por pixel, um contexto so, fio principal).
//
// Em vez de depender do driver, o fio de decode monta a piramide inteira com
// tex_reduzir (2x por nivel, ate 1x1, no tamanho que o GL espera:
// max(1, floor(n/2))) e o envio sobe cada nivel com glTexImage2D. Os niveis
// ficam encadeados em `userdata` da superficie base — o SDL nao usa esse campo
// e nao o libera, entao TODA liberacao de superficie do item passa por
// liberarSup.
//
// Padrao: CPU no Android (NV_ANDROID), GPU no resto (LG/Tizen/Mac como
// sempre). NUVIO_TEX_MIP=cpu|gpu troca em qualquer alvo, para diagnostico.
static int mipCpu = -1;

static int mipCpuAtivo(void) {
  if (mipCpu < 0) {
    const char *e = getenv("NUVIO_TEX_MIP");
    if (e && !strcmp(e, "cpu")) mipCpu = 1;
    else if (e && !strcmp(e, "gpu")) mipCpu = 0;
    else {
#ifdef NV_ANDROID
      mipCpu = 1;
#else
      mipCpu = 0;
#endif
      // Utgard (.tpk, #286, UA40N5300): o glGenerateMipmap custava ~64 ms por
      // poster no fio principal (gl_ms 55-73 de 0 a 900 KB, contra 20 ms de um
      // heroi de 3,6 MB sem mip), um quadro inteiro de GPU por upload. A
      // piramide sai no fio de decode e sobe so como glTexImage2D.
      if (ptv_gpu_utgard_atual()) {
        mipCpu = 1;
        printf("[tex] mip=cpu (Utgard)\n"); fflush(stdout);
      }
    }
  }
  return mipCpu;
}

static void liberarSup(SDL_Surface *s) {
  while (s) {
    SDL_Surface *prox = (SDL_Surface *)s->userdata;
    s->userdata = NULL;
    SDL_FreeSurface(s);
    s = prox;
  }
}

// Monta os niveis 1..n sob `base` (ABGR8888). Devolve quantos niveis criou; 0
// se nao precisava (1x1) ou se faltou memoria — nesse caso nada fica
// pendurado e o envio cai no glGenerateMipmap de sempre.
int tex_piramide_cpu(SDL_Surface *base) {
  SDL_Surface *ant = base;
  int n = 0;
  if (!base || base->userdata || base->format->format != SDL_PIXELFORMAT_ABGR8888)
    return 0;
  while (ant->w > 1 || ant->h > 1) {
    int lw = ant->w > 1 ? ant->w / 2 : 1, lh = ant->h > 1 ? ant->h / 2 : 1;
    SDL_Surface *nv = tex_reduzir(ant, lw, lh);
    if (!nv || nv->w != lw || nv->h != lh || nv->pitch != lw * 4) {
      if (nv) SDL_FreeSurface(nv);
      liberarSup((SDL_Surface *)base->userdata);
      base->userdata = NULL;
      return 0;
    }
    ant->userdata = nv;
    ant = nv;
    n++;
  }
  return n;
}

void tex_piramide_liberar(SDL_Surface *base) { liberarSup(base); }

// Sobe os niveis 1..n encadeados em `base` na textura ligada. Devolve quantos
// bytes subiu (para tex_upl_bytes).
static long subirNiveis(SDL_Surface *base) {
  long bytes = 0;
  int nivel = 1;
  SDL_Surface *s;
  for (s = (SDL_Surface *)base->userdata; s; s = (SDL_Surface *)s->userdata, nivel++) {
    glTexImage2D(GL_TEXTURE_2D, nivel, GL_RGBA, s->w, s->h, 0, GL_RGBA,
                 GL_UNSIGNED_BYTE, s->pixels);
    bytes += (long)s->w * s->h * 4;
  }
  return bytes;
}

#ifndef __EMSCRIPTEN__
// Algumas SDL_image do webOS nao incluem GIF. A foto usa o decoder C ja
// embarcado: um quadro, limitado, sem iniciar animacao nem fio adicional.
static SDL_Surface *gifFotoMem(const unsigned char *b, size_t n, int limite,
                              int *ow, int *oh) {
  int w, h, y;
  unsigned char *px = gif_primeiro_rgba(b, n, limite, &w, &h, ow, oh);
  SDL_Surface *s;
  if (!px) return NULL;
  s = nv_superficie(0, w, h, 32, SDL_PIXELFORMAT_ABGR8888);
  if (s)
    for (y = 0; y < h; y++)
      memcpy((char *)s->pixels + y * s->pitch, px + (size_t)y * w * 4, (size_t)w * 4);
  free(px);
  return s;
}

static SDL_Surface *gifFotoArquivo(const char *caminho, int limite, int *ow, int *oh) {
  unsigned char magic[4], *b;
  long n;
  SDL_Surface *s = NULL;
  FILE *f = fopen(caminho, "rb");
  if (!f) return NULL;
  if (fread(magic, 1, sizeof magic, f) != sizeof magic || memcmp(magic, "GIF8", 4) ||
      fseek(f, 0, SEEK_END) != 0 || (n = ftell(f)) < 14 || n > 32L * 1024 * 1024 ||
      fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
  b = malloc((size_t)n);
  if (b && fread(b, 1, (size_t)n, f) == (size_t)n)
    s = gifFotoMem(b, (size_t)n, limite, ow, oh);
  free(b);
  fclose(f);
  return s;
}
#endif

/* Android fallbacks allocate the full source before resizing. Only allow
 * sources within the codec's pixel ceiling; unknown bounds fail closed.
 * Both paths run under threadDecode's existing 48 MiB reservation. */
static int fallbackCabe(int w, int h) {
#ifdef NV_ANDROID
  return w > 0 && h > 0 && (uint64_t)w * (uint64_t)h <= 2u * 1024 * 1024;
#else
  (void)w; (void)h;
  return 1;
#endif
}

static SDL_Surface *decodeArteMem(const unsigned char *b, size_t n, int limite,
                                  int *ow, int *oh) {
  SDL_Surface *s = jpeg_rapido_carregar_mem(b, n, limite, ow, oh);
#ifndef __EMSCRIPTEN__
  if (!s && fallbackCabe(*ow, *oh)) {
    s = webp_carregar_larg_mem(b, n, limite, ow, oh);
    if (!s) {
      SDL_RWops *rw = n <= (size_t)SDL_MAX_SINT32 ? SDL_RWFromConstMem(b, (int)n) : NULL;
      s = rw ? IMG_Load_RW(rw, 1) : NULL;
      if (s) { *ow = s->w; *oh = s->h; }
    }
    if (!s) s = gifFotoMem(b, n, limite, ow, oh);
  }
#endif
  return s;
}

static SDL_Surface *decodeArteArquivo(const char *caminho, int limite,
                                      int *ow, int *oh) {
  SDL_Surface *s = jpeg_rapido_carregar(caminho, limite, ow, oh);
  if (!s && fallbackCabe(*ow, *oh)) {
    s = webp_carregar_larg(caminho, limite, ow, oh);
    if (!s) {
      s = IMG_Load(caminho);
      if (s) { *ow = s->w; *oh = s->h; }
    }
#ifndef __EMSCRIPTEN__
    if (!s) s = gifFotoArquivo(caminho, limite, ow, oh);
#endif
  }
  return s;
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
  // NORMAL e nao LOW desde 19/09, EXPERIMENTO MEDIDO: com LOW (nice 19) o fio
  // recebia ~1,5% do peso de CFS contra o fio de desenho e os de rede, e um
  // JPEG de 1920 levava 250 ms para ler numa CPU que o decodifica em ~80. O
  // jank que justificou o LOW era do decode INTEIRO + media de area; com o
  // decode escalado e a bilinear o trabalho por arte caiu 5x, e a TV tem
  // quatro nucleos. Se o pior quadro voltar a 40 ms com pend>0, volta o LOW.
  SDL_SetThreadPriority(SDL_THREAD_PRIORITY_NORMAL);
  for (;;) {
    SDL_LockMutex(mtx);
    while (rodando && (decIni == decFim || !decodeCabe()))
      SDL_CondWaitTimeout(condDec, mtx, 20);
    if (!rodando) { SDL_UnlockMutex(mtx); return 0; }
    int idx = tirarFila(filaDec, &decIni, decFim);
    // O marcador cobre tambem o decode em andamento. Limpa-lo aqui abre uma
    // janela em que o desenho ve PENDENTE e enfileira o mesmo item outra vez,
    // enquanto este fio ainda esta lendo/reduzindo os pixels.
    SDL_CondSignal(condLivre);   // abriu lugar: solta um fio de rede que espera
    if (itens[idx].estado != PENDENTE || pedidoObsoleto(&itens[idx])) {
      if (itens[idx].estado == PENDENTE) desistir(idx);
      itens[idx].naFilaDec = 0;
      SDL_UnlockMutex(mtx);
      continue;
    }
    long reserva = (long)cachePerfil().reservaDecodeMb * 1048576;
    decodeReservado += reserva;
    char caminho[NV_TEX_URL_MAX];
    int limite, card;
    Uint32 filaEm = 0, filaWait = 0;
    int localDireto = 0;
    strncpy(caminho, itens[idx].caminho, sizeof caminho - 1);
    caminho[sizeof caminho - 1] = 0;
    char urlOrig[NV_TEX_URL_MAX];
    strncpy(urlOrig, caminho, sizeof urlOrig - 1);
    urlOrig[sizeof urlOrig - 1] = 0;
    // Copiado SOB O MUTEX: o item pode ser promovido a hero enquanto este fio
    // decodifica, e ler o campo depois daria uma leitura sem trava.
    limite = itens[idx].limite > 0 ? itens[idx].limite : NV_TEX_LARG_MAX;
    card = itens[idx].card;
    // Cor viva (corviva.h) so para a arte de TELA CHEIA: e o que destaque,
    // detalhe e player pedem (tex_obter_hero), e so ela decide a cor. Cartaz
    // de fileira passaria por aqui as centenas e empurraria do anel de 256 as
    // artes que importam.
    int heroiPedido = ehHero(&itens[idx]);
    int logoPedido = itens[idx].urgente == 2;
    filaEm = itens[idx].filaDecEm;
    itens[idx].filaDecEm = 0;
    localDireto = itens[idx].localDireto;
    int limTam = localDireto ? 0 : itens[idx].limiteTamanho;
    // OS BYTES SAEM DO ITEM AQUI, sob o mutex, e passam a ser deste fio.
    unsigned char *bruto = itens[idx].bruto;
    long nBruto = itens[idx].nBruto;
    int daMemoria = bruto != NULL;
#ifdef __EMSCRIPTEN__
    int varianteCache = itens[idx].varianteCache;
#endif
    char urlCache[NV_TEX_URL_MAX];
    snprintf(urlCache, sizeof urlCache, "%s", itens[idx].urlCache);
    itens[idx].bruto = NULL; itens[idx].nBruto = 0;
    itens[idx].urlCache[0] = 0;
    SDL_UnlockMutex(mtx);
    if (filaEm) {
      filaWait = SDL_GetTicks() - filaEm;
      if (logoPedido || filaWait >= NV_TEX_TRACE_MS)
        printf("[tex-trace] fila-decode hash=%08lx wait=%u kind=%s limite=%d\n",
               hashCaminho(urlOrig), (unsigned)filaWait,
               localDireto ? "local" : "remote", limite);
    }
#ifdef NV_TEX_TEST_AFTER_POP
    NV_TEX_TEST_AFTER_POP();
#endif

    Uint32 t0 = SDL_GetTicks(), tLoad;
    unsigned char magDec[4] = {0, 0, 0, 0};
    int temMagDec = 0;
    int srcW = 0, srcH = 0;
    SDL_Surface *bruta = NULL;
    SDL_Surface *conv = NULL;
    if (bruto) {
      bruta = decodeArteMem(bruto, (size_t)nBruto, limite, &srcW, &srcH);
      free(bruto);
    } else
    {
    // O download JA ACONTECEU no fio de rede; aqui garantirLocal so traduz a
    // URL para o caminho do cache, sem tocar a rede.
    // Com variante (limiteTamanho), o arquivo no disco e o dela.
    { char local[600], certo[NV_TEX_URL_MAX];
      const char *fonte = caminho;
      if (limTam > 0 && arte_tamanho_url(caminho, limTam, certo, sizeof certo)) fonte = certo;
      if (garantirLocal(fonte, local, sizeof local, NULL, NULL))
        snprintf(caminho, sizeof caminho, "%s", local);
    }
    // A MAGICA do arquivo (Item.magica): quem veio direto do disco
    // (discoDireto) nao passou pelo fio de rede, que e quem a lia.
#ifndef __EMSCRIPTEN__
    { FILE *g = fopen(caminho, "rb");
      if (g) { temMagDec = fread(magDec, 1, 4, g) == 4; fclose(g); } }
#endif
    // JPEG SAI DO DECODIFICADOR JA REDUZIDO (jpegrapido.h): 1/2, 1/4 ou 1/8
    // dentro da DCT, o que cobre o pedido. srcW/srcH ficam com o tamanho do
    // ARQUIVO — e o que `fonteW` guarda para decidir promocao — e nao com o
    // do que saiu. Fallbacks preserve the Android allocation ceiling.
    bruta = decodeArteArquivo(caminho, limite, &srcW, &srcH);
    }
    tLoad = SDL_GetTicks();
    if (bruta && !srcW) { srcW = bruta->w; srcH = bruta->h; }
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
      // tex_reduzir CONVERTE O FORMATO por faixa de linhas enquanto reduz,
      // entao a copia intermediaria em tamanho cheio nunca precisa existir: o
      // destino ja nasce com 640 de largura e no formato final. O pico cai de
      // 2x para 1x o tamanho da fonte.
      if (bruta->w > limite) {
        int lw = limite;
        int lh = bruta->h * lw / bruta->w;
        // Media de area, nao SDL_BlitScaled: ver tex_reduzir.
        conv = tex_reduzir(bruta, lw, lh > 0 ? lh : 1);
      }
      // Fonte ja pequena, ou a superficie reduzida nao pode ser criada: o
      // caminho antigo continua valendo.
      // A ponte PNG/WebP do Emscripten ja monta a superficie no formato final
      // ABGR8888. Nao converta de novo: alem de ser uma copia desnecessaria,
      // SDL_ConvertSurfaceFormat e uma fronteira de alpha sensivel nos SDL
      // antigos do alvo Tizen. O upload abaixo le exatamente esses quatro
      // bytes (R,G,B,A), e a mascara vem do formato da propria superficie.
      // Para qualquer decoder que entregue outro formato, mantemos a
      // conversao normal.
      if (!conv && bruta && bruta->format &&
          bruta->format->format == SDL_PIXELFORMAT_ABGR8888) {
        conv = bruta;
        bruta = NULL;
      }
      if (!conv && bruta) conv = SDL_ConvertSurfaceFormat(bruta, SDL_PIXELFORMAT_ABGR8888, 0);
      if (bruta) SDL_FreeSurface(bruta);
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
    // NV_CACHE_DISCO_MAX e, passando disso, poda os MAIS ANTIGOS ate voltar a
    // caber — o recem-baixado, que e o que esta na tela, e justamente o que
    // fica. A politica anterior descartava ele; ver a nota em podarCacheDisco.
    if (conv && cacheDiscoBytes > NV_CACHE_DISCO_MAX) podarCacheDisco();
#endif
    // TETO DE LARGURA. Antes a arte vinha do pacote ja reduzida; agora vem da
    // rede no tamanho que o servidor tiver, e um backdrop de 1920 custa 8 MB
    // DECODIFICADO — meia duzia deles estoura o orcamento e o cache passa a
    // despejar e rebaixar em circulo, o que aparece como queda de fps e quadros
    // de 100 ms. Reduzir aqui vale para qualquer fonte, presente ou futura.
    if (conv && conv->w > limite) {
      int lw = limite;
      int lh = conv->h * lw / conv->w;
      SDL_Surface *menor = tex_reduzir(conv, lw, lh > 0 ? lh : 1);
      if (menor) {
        SDL_FreeSurface(conv);
        conv = menor;
      }
    }

    // DE ONDE VEM A NITIDEZ, quando alguem reclama que a arte esta borrada.
    //
    // Tres numeros e uma URL por decode: a fonte que o servidor mandou, o teto
    // que o pedido impos e o tamanho que virou textura. Sem eles, "a arte esta
    // pixelada" tem tres explicacoes possiveis (URL pequena, teto baixo, ou a
    // promocao que nao aconteceu) e nenhuma medicao para separar. Atras de uma
    // variavel de ambiente porque sao centenas de linhas por sessao.
    // DECODE LENTO SEMPRE APARECE, sem variavel de ambiente. 250 ms num fio de
    // prioridade baixa ja e arte que a pessoa espera; e o unico numero que
    // separa "a internet esta lenta" de "esta imagem tem pixels demais para
    // este nucleo". Sem ele, a resposta a "por que a arte demora" e opiniao.
    { Uint32 dt = SDL_GetTicks() - t0;
      if (conv && (logoPedido || dt >= 250)) {
        // Em duas partes: ler o arquivo (IMG_Load) e reduzir (tex_reduzir +
        // conversao). E o que separa "a libjpeg e lenta" de "a media de area
        // e lenta" — duas respostas com consertos opostos.
        { char lb[160];
          printf("[tex] decode %s: %u ms (ler %u, reduzir %u) para %dx%d (saiu %dx%d) %s\n",
                 dt >= 250 ? "lento" : "logo", (unsigned)dt, (unsigned)(tLoad - t0), (unsigned)(SDL_GetTicks() - tLoad),
                 srcW, srcH, conv->w, conv->h, urlLog(urlOrig, lb, sizeof lb)); }
        if (limTam > 0) printf("[tex] (variante do tamanho, teto %d)\n", limTam);
        printf("[tex-trace] decode hash=%08lx kind=%s queue=%u total=%u load=%u reduce=%u src=%dx%d out=%dx%d\n",
               hashCaminho(urlOrig), localDireto ? "local" : "remote",
               (unsigned)filaWait, (unsigned)dt, (unsigned)(tLoad - t0),
               (unsigned)(SDL_GetTicks() - tLoad), srcW, srcH, conv->w, conv->h);
        fflush(stdout);
      } }
    if (getenv("NUVIO_TEX_LOG") && conv)
      { char lb[160];
        printf("[tex-nitidez] fonte=%dx%d teto=%d final=%dx%d %s\n",
               srcW, srcH, limite, conv->w, conv->h, urlLog(urlOrig, lb, sizeof lb)); }

    // MEDIDA DE LUMINANCIA, aqui e nao no desenho: esta thread ja tem os pixels
    // na mao e roda em prioridade baixa. Amostra de 4 em 4 nos dois eixos —
    // 1/16 dos pixels bastam para dizer se uma arte e escura, e a conta inteira
    // num logo de 700x271 seria trabalho sem retorno.
    int lumMedia = -1, cromaMedia = 0, desvio1 = 0;
    int corR = -1, corG = 0, corB = 0, opaca = 0;
    // OPACA DE PONTA A PONTA? Conferido pixel a pixel (para no primeiro alfa <
    // 255): e o que deixa o desenho dispensar a camada de tela cheia que ficaria
    // inteira atras desta arte (gfx_arte_opaca_atual). Uma amostra nao basta —
    // um pixel transparente perdido mostraria o fundo errado. ~2 M leituras num
    // heroi de 1920x1080, nesta thread de baixa prioridade.
    if (conv && conv->format->BytesPerPixel == 4 && conv->w > 0 && conv->h > 0) {
      int yy, xx;
      opaca = 1;
      for (yy = 0; yy < conv->h && opaca; yy++) {
        const unsigned char *ln = (const unsigned char *)conv->pixels + (size_t)yy * conv->pitch;
        for (xx = 0; xx < conv->w; xx++) if (ln[(size_t)xx * 4 + 3] != 255) { opaca = 0; break; }
      }
    }
    if (conv && conv->format->BytesPerPixel == 4) {
      const unsigned char *px = (const unsigned char *)conv->pixels;
      long soma = 0, somaC = 0, n = 0;
      double soma2 = 0.0;
      int yy, xx;
      for (yy = 0; yy < conv->h; yy += 4) {
        const unsigned char *ln = px + (size_t)yy * conv->pitch;
        for (xx = 0; xx < conv->w; xx += 4) {
          const unsigned char *q = ln + (size_t)xx * 4;   // ABGR8888: R,G,B,A
          if (q[3] < 200) continue;                       // so o que e opaco
          { int lu = (q[0] * 299 + q[1] * 587 + q[2] * 114) / 1000;
            soma += lu; soma2 += (double)lu * (double)lu; }
          { int mx = q[0] > q[1] ? q[0] : q[1]; if (q[2] > mx) mx = q[2];
            int mn = q[0] < q[1] ? q[0] : q[1]; if (q[2] < mn) mn = q[2];
            somaC += mx - mn; }
          n++;
        }
      }
      if (n > 0) {
        double m = (double)soma / (double)n, v = soma2 / (double)n - m * m;
        lumMedia = (int)(soma / n); cromaMedia = (int)(somaC / n);
        desvio1 = (int)(v > 0.0 ? sqrt(v) : 0.0) + 1;
      }
      // COR DE FUNDO: a media da BORDA da imagem (uma moldura de 1 px de
      // cada lado). Logo com fundo proprio (o quadrado cinza do Disney+) tem
      // a borda opaca e de uma cor so; logo recortado tem a borda
      // transparente e ai NAO ha fundo a copiar — quem desenha usa o azulejo
      // dele. Existe para o guia pintar o cartao em foco com a cor por tras
      // do logo, e nao com a media da marca (que num logo preto e branco da
      // um cinza que nao e de ninguem).
      { long br = 0, bg = 0, bb = 0, bn = 0, bt = 0;
        int W = conv->w, H = conv->h;
        for (xx = 0; xx < W; xx += 2) {
          const unsigned char *q0 = px + (size_t)xx * 4;
          const unsigned char *q1 = px + (size_t)(H - 1) * conv->pitch + (size_t)xx * 4;
          bt += 2;
          if (q0[3] >= 200) { br += q0[0]; bg += q0[1]; bb += q0[2]; bn++; }
          if (q1[3] >= 200) { br += q1[0]; bg += q1[1]; bb += q1[2]; bn++; }
        }
        for (yy = 0; yy < H; yy += 2) {
          const unsigned char *q0 = px + (size_t)yy * conv->pitch;
          const unsigned char *q1 = q0 + (size_t)(W - 1) * 4;
          bt += 2;
          if (q0[3] >= 200) { br += q0[0]; bg += q0[1]; bb += q0[2]; bn++; }
          if (q1[3] >= 200) { br += q1[0]; bg += q1[1]; bb += q1[2]; bn++; }
        }
        if (bt > 0 && bn * 10 >= bt * 8) { corR = (int)(br / bn); corG = (int)(bg / bn); corB = (int)(bb / bn); }
        else corR = -2; }
    }

    // COR VIVA: a paleta sai DAQUI, dos pixels que este fio ja tem — nenhuma
    // leitura de volta da GPU, nenhum decode a mais. corviva_extrair amostra
    // uma grade de 32x18 (576 pontos) e nao aloca nada. O custo vai ao log UMA
    // vez, na primeira arte, para a medida da C9 existir sem inundar o log.
    // Roda em TODA arte (~15 us), mas so ANOTA a de tela cheia e a que tem
    // transparencia — o LOGO do titulo ("Cor da logo"), que e decodificado na
    // largura do desenho, nunca no teto do destaque. Cartaz de fileira e opaco
    // e pequeno: fica no slot, sem empurrar ninguem do anel. Retratos podem
    // publicar essa paleta sob demanda, sem decodificar de novo.
    CorvivaPaleta pal = {0};
    if (conv && conv->format->BytesPerPixel == 4) {
      static int medido;
      Uint64 c0 = SDL_GetPerformanceCounter();
      corviva_extrair((const unsigned char *)conv->pixels, conv->w, conv->h,
                      conv->pitch, &pal);
      // Troca atomica: dois fios de decodificacao chegavam juntos (TSAN).
      if (!__atomic_exchange_n(&medido, 1, __ATOMIC_RELAXED)) {
        printf("[cor] extracao: %.0f us (%dx%d, %s)\n",
               (double)(SDL_GetPerformanceCounter() - c0) * 1e6 /
                 (double)SDL_GetPerformanceFrequency(),
               conv->w, conv->h, pal.ok ? "com cor" : "sem cor");
        fflush(stdout);
      }
      if (heroiPedido || pal.transparente) corviva_anotar(urlOrig, &pal);
    }

    // A FALHA PRECISA APARECER. Sem log, uma imagem que nunca decodifica vira
    // um laco silencioso: o desenho pede todo quadro, o fio tenta todo quadro,
    // e o unico sintoma e "esse card nao tem arte". Foi assim que o WEBP do
    // metahub passou despercebido.
    // PIRAMIDE NA CPU, aqui e nao no envio: este fio e de prioridade baixa e
    // ja tem os pixels. Mesma decisao (temPiramide) da contabilidade de bytes.
    // Ver tex_piramide_cpu.
    if (conv && mipCpuAtivo() && temPiramide(conv->w, conv->h, card))
      tex_piramide_cpu(conv);

    int falhou = 0;
    SDL_LockMutex(mtx);
    decodeReservado -= reserva;
    // O SLOT AINDA E DESTE PEDIDO? (23/09/2026, cards com a arte de OUTRO
    // titulo.) O decode roda sem a trava; se o slot foi despejado e
    // reaproveitado por outro caminho nesse meio tempo, ele esta PENDENTE de
    // novo, mas por OUTRA imagem — publicar `conv` ali punha o fundo horizontal
    // de um titulo no cartaz de outro ("The Boys" com a cena do Slime), e a
    // textura ficava no cache com o nome errado. Mesma guarda do download
    // (mesmoPedido): nao sendo o mesmo caminho, a superficie vai para o lixo.
    if (strcmp(itens[idx].caminho, urlOrig)) {
      if (conv) liberarSup(conv);
      { char lb[160]; printf("[tex] decode descartado: o slot virou outro pedido (%s)\n", urlLog(urlOrig, lb, sizeof lb)); }
      fflush(stdout);
      SDL_UnlockMutex(mtx);
      continue;
    }
    if (temMagDec && !itens[idx].temMagica) {
      memcpy(itens[idx].magica, magDec, 4); itens[idx].temMagica = 1;
    }
    if (itens[idx].estado == PENDENTE && !conv && pedidoObsoleto(&itens[idx])) {
      // A imagem terminou depois de o card sair da tela E NAO DECODIFICOU: nao
      // a transforme em falha, outro card pode reutilizar o slot frio.
      //
      // ANTES ISTO TAMBEM JOGAVA FORA DECODE PRONTO, e era o defeito caro. Um
      // still de 3840x2160 leva 1,6 a 2,0 s nesta TV (medido) e a espera do
      // destaque e de 400 ms: o desenho desistia, a superficie pronta ia para
      // o lixo e a mesma arte era decodificada de novo na volta do carrossel.
      // O log mostrava cada arte pesada decodificada em dobro. Quem ja pagou o
      // decode publica; o LRU despeja depois se ninguem usar.
      desistir(idx);
    } else if (itens[idx].estado == PENDENTE && !conv && itens[idx].tex) {
      // Promocao que nao decodificou: fica a versao pequena, sem FALHOU — o
      // FALHOU devolveria 0 ao desenho e a arte sumiria da tela.
      desistir(idx);
      falhou = 1;
    } else if (itens[idx].estado == PENDENTE) {
      itens[idx].lum = lumMedia;
      itens[idx].opaca = opaca;
      itens[idx].paleta = pal;
      if (conv && conv->format->BytesPerPixel == 4)
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
          unsigned sum[4] = {0};
          for (int dy = 0; dy < 4; dy++) for (int dx = 0; dx < 4; dx++) {
            int sx = (x * 4 + dx) * conv->w / 64, sy = (y * 4 + dy) * conv->h / 64;
            const unsigned char *q = (unsigned char *)conv->pixels + sy * conv->pitch + sx * 4;
            for (int c = 0; c < 4; c++) sum[c] += q[c];
          }
          for (int c = 0; c < 4; c++) itens[idx].cores[(y*16+x)*4+c] = sum[c] / 16;
        }
      itens[idx].croma = cromaMedia;
      itens[idx].desvio1 = desvio1;
      itens[idx].corR = corR; itens[idx].corG = corG; itens[idx].corB = corB;
      itens[idx].sup = conv;
      if (conv) {
        itens[idx].estado = DECODIFICADO;
        itens[idx].filaUploadEm = SDL_GetTicks();
        // O QUE SAIU, e nao o que foi pedido: e este par que a promocao le.
        itens[idx].tetoUsado = limite;
        itens[idx].fonteW = srcW;
        // VARIANTE MENOR: o teto que ela cobre, e a fonte de verdade nao e
        // conhecida. Sem isto um w1280 decodificado a 1280 gravava fonteW=1280
        // e a promocao a 1920 ficava bloqueada; e um w780 baixado a 704 mas
        // decodificado depois de o teto subir a 1920 gravava tetoUsado=1920.
        if (limTam > 0) {
          if (limTam < limite) itens[idx].tetoUsado = limTam;
          itens[idx].fonteW = 0;
        }
      } else {
        // MANTEM o caminho: e ele que identifica o slot na proxima consulta e
        // permite responder "ainda nao, tente depois" em vez de reenfileirar.
        // Zerar o caminho, como estava, apagava a memoria da falha junto.
        static const Uint32 RECUO[3] = { 2000, 10000, 60000 };
        int k = itens[idx].falhas;
        itens[idx].estado = FALHOU;
        itens[idx].urgente = 0;
        itens[idx].falhas = k + 1;
        itens[idx].tentarEm = SDL_GetTicks() + RECUO[k < 3 ? k : 2];
        falhou = 1;
      }
    } else if (conv) {
      liberarSup(conv);  // slot foi reaproveitado no meio do caminho
    }
    // So agora o item pode ser reenfileirado por um pedido de quadro seguinte.
    itens[idx].naFilaDec = 0;
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
      { char lb[160];
      printf("[tex] decode falhou (%s) tam=%ld magica=%02x%02x%02x%02x: %.70s\n",
             IMG_GetError(), tam, mag[0], mag[1], mag[2], mag[3], urlLog(caminho, lb, sizeof lb)); }
      // GIF INTEIRO NAO E ARQUIVO ENVENENADO — E ARQUIVO DE OUTRO LEITOR.
      //
      // Issue #49, e as duas fotos do log do @rawldon fecham a cadeia: o
      // SDL_image do alvo Tizen nao le GIF ("Unsupported image format",
      // magica 47494638 = "GIF8"), este ramo APAGAVA o arquivo, e quem o usa
      // de verdade e gif.c, que anima a capa pela <img> do navegador. Com o
      // arquivo apagado, tex_arquivo() volta a devolver NULL, home.c cai na
      // capa parada, o cache baixa o GIF DE NOVO (o cache-disco dele subia
      // 1,2 -> 2,2 -> 3,2 MB, um megabyte por volta), a capa volta a animar
      // por um instante, o decode falha de novo e apaga de novo. E o "toca
      // um segundo, pisca e congela" do relato — com o recuo de 2 s e 10 s
      // deste mesmo cache marcando o ritmo.
      //
      // Um GIF com a assinatura certa e tamanho de GIF nao esta corrompido:
      // esta so no leitor errado. Fica no disco (para gif.c) e o item aqui
      // continua FALHOU, sem baixar de novo.
      { int ehGif = (mag[0] == 'G' && mag[1] == 'I' && mag[2] == 'F' && mag[3] == '8');
        if (!ehGif) {
#ifdef __EMSCRIPTEN__
          /* The decoder is authoritative: signature checks cannot detect a
           * truncated JPEG with a valid SOI. Remove that exact persistent
           * variant so the retry can refill it from HTTP. */
          if (daMemoria && urlCache[0]) cachearte_invalidar(urlCache, varianteCache);
#else
          /* Bytes da memoria que o decoder recusou: a copia deles ainda pode
           * estar na fila de gravacao; nao deixa virar arquivo envenenado. */
          if (daMemoria) {
            char local[600];
            // A chave do item pode ser /original/, mas os bytes baixados e
            // a gravacao pertencem a /w780/ ou /w1280/. Invalida essa URL.
            nomeDeCache(urlCache[0] ? urlCache : urlOrig, local, sizeof local);
            cancelarGravacao(local);
            if (remove(local) == 0) { cachearte_nativo_indice_remover(local); publicarDiscoNativo(); }
          }
#endif
          // APAGA o arquivo que nao decodifica. Ele so pode ter chegado ao
          // cache corrompido — a assinatura foi conferida no download —, e
          // mante-lo significa que esta arte NUNCA mais carrega, nem depois de
          // o problema que a truncou passar. Apagando, o proximo pedido baixa
          // de novo. Só apaga o que esta no NOSSO cache.
          // DESCONTA DO CONTADOR. Sem isto cada decode falho deixava bytes
          // fantasma em cacheDiscoBytes ate o proximo arranque, e o log ja
          // registrou series de 91-93 falhas numa navegacao — bastam ~48 MB
          // delas para a poda passar a apagar tudo e nunca se dar por
          // satisfeita, deixando o cache vazio de vez.
          if (noCache(caminho)) {
#ifdef __EMSCRIPTEN__
            long tamAnt = 0;
            FILE *g = fopen(caminho, "rb");
            if (g) { fseek(g, 0, SEEK_END); tamAnt = ftell(g); fclose(g); }
            if (remove(caminho) == 0) {
              cacheDiscoBytes -= tamAnt;
              if (cacheDiscoBytes < 0) cacheDiscoBytes = 0;
              publicarCacheDisco();
            }
#else
            if (remove(caminho) == 0) { cachearte_nativo_indice_remover(caminho); publicarDiscoNativo(); }
#endif
          }
        } else {
          printf("[tex] e um GIF: fica no disco para gif.c, sem baixar de novo\n");
        }
        fflush(stdout); }
    }
  }
}

// RAM TOTAL DO APARELHO, em MB. Na TV LG vem de /proc/meminfo. No Tizen nao
// ha /proc: vem de navigator.deviceMemory, que o navegador arredonda para
// 0,25/0,5/1/2/4/8 GB e que nem todo Chromium expoe — sem ele, 0, e o
// orcamento fica no padrao de layout.h (96 MB), que e o que sempre foi.
static long memTotalMB(void) {
#ifdef AJUSTES_TESTE
  const char *ram = getenv("NUVIO_SHOT_RAM_MB");
  if (ram && atol(ram) > 0) return atol(ram);
#endif
#ifdef __EMSCRIPTEN__
  double gb = EM_ASM_DOUBLE({
    try {
      var m = (typeof navigator !== 'undefined') ? navigator.deviceMemory : 0;
      return (typeof m === 'number' && m > 0) ? m : 0;
    } catch (e) { return 0; }
  });
  return (long)(gb * 1024.0 + 0.5);
#else
  FILE *f = fopen("/proc/meminfo", "r");
  char linha[128];
  long kb = 0;
  if (!f) return 0;
  while (fgets(linha, sizeof linha, f))
    if (sscanf(linha, "MemTotal: %ld kB", &kb) == 1) break;
  fclose(f);
  return kb / 1024;
#endif
}

// O ORCAMENTO E DECIDIDO NO ARRANQUE, PELA RAM DA TV — e nao por uma build
// para cada modelo.
//
// Uma so faixa nao serve a todas: a C9 (2,2 GB) fica folgada em 128 MB e uma
// webOS 3 de 2016 (1 a 1,5 GB) nao tem esse espaco; uma TV de 3 GB ou mais
// segura o dobro sem sentir. Tres IPKs seriam tres pacotes para manter e a
// pessoa tendo de saber quanta RAM a TV dela tem. MemTotal ja diz.
//
//   < 800 MB   48 MB    webOS 3 de 2016: um relato de 624 MB TOTAIS, ~300 MB
//                       livres (README, secao webOS 3). CHUTE pelo tamanho da
//                       RAM, nao medida: nao ha aparelho desses aqui. O `rss=`
//                       e o `tela=` do relatorio de FPS confirmam ou corrigem.
//   < 1,2 GB   64 MB    webOS 3/4 de 1 GB, mesmo chute
//   < 2 GB     96 MB    webOS 4/5 menores
//   < 3 GB    128 MB    C9: medido, home usa 44-50 MB quentes, RSS 255-278
//   >= 3 GB   192 MB    C1/C2/C3 e mais novas
//
// NO TIZEN a tabela e outra e mais curta, porque o numero mede outra coisa:
// navigator.deviceMemory e a RAM do aparelho vista pelo navegador, e as
// texturas vivem no processo da GPU, fora do heap fixo de 256 MiB do wasm.
// Sem Samsung aqui, os degraus sao CHUTE conservador: <= 1 GB -> 64, 2 GB ->
// 96 (o que era), >= 4 GB -> 128. Sem deviceMemory -> 96. O `rss=` nao existe
// la; o [mem] e o painel de log (tecla vermelha) sao o que confirma.
//
// Duas portas por cima da tabela:
//   NV_TEX_MB_FIXO   -D de compilacao: e a build "alto cache" (tools/arm.sh
//                    --alto-cache, 300 MB) para quem tem TV com muita RAM e
//                    quer o cache inteiro de uma sessao residente.
//   NUVIO_TEX_MB     variavel de ambiente, para medir um numero numa TV sem
//                    recompilar. Vale entre 16 e 1024.
//
// O Tizen fica no NV_TEX_ORCAMENTO_MB de layout.h: la o heap e fixo em 256
// MiB e MemTotal do navegador nao diz nada sobre ele.
static int  orcMB = 0;        // o que foi decidido, para tex_orcamento_info
static int  orcFixo = 0;      // 1 = NV_TEX_MB_FIXO, 2 = NUVIO_TEX_MB, 3 = Ajustes
static int  orcAuto = 0;      // o que orcamentoMB decidiu, para voltar a ele
static int orcamentoMB(void) {
  long mem = memTotalMB();
  int mb;
  const char *porque;
#ifdef NV_TEX_MB_FIXO
  mb = NV_TEX_MB_FIXO; porque = "NV_TEX_MB_FIXO"; orcFixo = 1;
#  ifdef __EMSCRIPTEN__
  // O TETO FIXO NAO VALE NO TIZEN, e este limite existe por MEDICAO.
  //
  // 17/09, QN85Q70AAGXZD (2 GB): a variante de cache grande pede 300 MB e ficou
  // MAIS LENTA que a normal de 96 MB. Mesma arte, mesma saida — o fundo do
  // metahub 1920x1080 reduzido para 1280x720 levou 1481 ms na normal e 3575 ms
  // na de 300 MB. O pior quadro foi de 465 ms para 1475 ms.
  //
  // NAO E DESPEJO: com 300 MB o contador de despejos fica em ZERO a sessao
  // inteira, enquanto a normal despeja (180, 68, 48...). E NAO E O HEAP: o
  // [mem] das duas mostra malloc em 22-31 MiB com 32-40 MiB livres dentro dos
  // 256 MiB. O mecanismo eu NAO PROVEI — o que esta provado e que o numero
  // maior piora, e que ninguem mediu um numero maior que ajude nesta TV.
  //
  // Na LG o teto fixo continua valendo: la sao 2,2 GB de RAM nativa e a medicao
  // foi a favor. Aqui a textura vive no processo de GPU do navegador, dentro
  // dos mesmos 2 GB que a TV inteira usa, e 300 MB e um palpite.
  //
  // O limite e o proprio valor que a regra automatica escolheria: quem passar
  // -DNV_TEX_MB_FIXO alto no alvo Tizen cai nele e o log diz por que.
  { int aut = ptv_tex_auto_mb(PTV_TIZEN, mem);
    if (mb > aut) {
      printf("[tex] NV_TEX_MB_FIXO=%d ignorado no Tizen: %d MB e o que a TV "
             "suporta (medido: teto maior decodifica mais devagar)\n", mb, aut);
      mb = aut; porque = "NV_TEX_MB_FIXO > teto Tizen"; orcFixo = 0;
    } }
#  else
  // NA LG O TETO FIXO OBEDECE A RAM (20/09/2026, registro 15 do servico de
  // avisos): uma TV de 1350 MB (Mali-G31) instalou a highcache e recebeu 300
  // MB de textura — o dobro do que a regra automatica da a 2 GB. O nome da
  // variante promete "cache grande para TV com RAM sobrando", nao "300 em
  // qualquer TV". O teto e o mesmo que Ajustes usa (tetoPermitidoMB): 64 abaixo
  // de 1,2 GB, 160 abaixo de 2 GB, 300 abaixo de 3 GB, 512 acima.
  // (ptv_plataforma: na LG e o PTV_LG de sempre; no .tpk, a tabela PTV_TPK.)
  { int teto = ptv_tex_teto_mb(ptv_plataforma(), mem);
    if (mb > teto) {
      printf("[tex] NV_TEX_MB_FIXO=%d acima do que %ld MB de RAM permitem: fica em %d MB\n", mb, mem, teto);
      mb = teto; porque = "NV_TEX_MB_FIXO limitado pela RAM";
    } }
#  endif
#else
  // A escada mora em perfiltv.c (ptv_tex_auto_mb), junto dos outros padroes
  // por aparelho; os degraus e os numeros sao os que estavam aqui.
  mb = ptv_tex_auto_mb(ptv_plataforma(), mem);
  porque = !mem ? "RAM desconhecida: NV_TEX_ORCAMENTO_MB" : "perfiltv.c (RAM)";
#endif
  { const char *env = getenv("NUVIO_TEX_MB");
    if (env && *env) {
      int v = atoi(env);
      if (v >= 16 && v <= 1024) { mb = v; porque = "NUVIO_TEX_MB"; orcFixo = 2; }
    } }
#ifdef NV_ANDROID
  { int cap = ptv_tex_teto_mb(PTV_ANDROID, mem);
    if (mb > cap) mb = cap; }
#endif
  printf("[tex] orcamento de texturas: %d MB (%s; MemTotal=%ld MB)\n", mb, porque, mem);
  fflush(stdout);
  orcMB = mb; orcMemTotal = mem; orcAuto = mb;
  return mb;
}

// TETO ESCOLHIDO EM AJUSTES (ajustes_tex_mb), aplicado ao vivo. Ver a nota do
// V_TEX_MB em ajustes.c. `mb` = 0 volta para o que orcamentoMB decidiu.
//
// A TRAVA E PELA RAM, e nao pela vontade: uma TV de 1 GB com 300 MB de
// texturas troca despejo por OOM, e OOM no webOS e o app sumindo sem cartao
// nenhum. O teto permitido segue a mesma escada de orcamentoMB, um degrau
// acima do automatico: < 1,2 GB -> 96, < 2 GB -> 160, < 3 GB -> 300 (o unico
// valor alto MEDIDO, na C9 de 2,2 GB), >= 3 GB -> 512 (C1/C2/C3: palpite pela
// RAM, sem aparelho aqui — o `rss=` do relatorio de FPS e quem confirma). No
// Tizen o teto e o proprio automatico (medido: mais e mais lento, ver acima).
static int tetoPermitidoMB(void) {
#ifdef NV_TIZEN3_LITE
  return 32;
#endif
  return ptv_tex_teto_mb(ptv_plataforma(), orcMemTotal);
}
int tex_teto_ram_mb(void) { return tetoPermitidoMB(); }
void tex_definir_orcamento_mb(int mb) {
  int teto = tetoPermitidoMB(), aplicado;
  if (!mtx) return;
  if (mb <= 0) { aplicado = orcAuto; }
  else {
    aplicado = mb > teto ? teto : mb;
    if (mb > teto)
      printf("[tex] teto de %d MB pedido em Ajustes; esta TV suporta %d MB (RAM %ld MB)\n", mb, teto, orcMemTotal);
  }
  SDL_LockMutex(mtx);
  { float e = escalaOrc();
    orcamento = (long)(aplicado * e * e) * 1024L * 1024L; }
  // Slots so CRESCEM ao vivo: encolher com itens alem do novo nMax deixaria
  // texturas vivas fora do alcance do LRU. O teto absoluto continua valendo.
  { int porBytes = aplicado * 3 / 2;
    if (porBytes > nMax) nMax = porBytes > MAX_ITENS_ABS ? MAX_ITENS_ABS : porBytes; }
  orcMB = aplicado; orcFixo = mb > 0 ? 3 : (orcFixo == 3 ? 0 : orcFixo);
  SDL_UnlockMutex(mtx);
  printf("[tex] orcamento de texturas: %d MB (%s)\n", aplicado, mb > 0 ? "escolhido em Ajustes" : "automatico de novo");
  fflush(stdout);
}

// O AUTOMATICO PASSA A SER O PERFIL APROVADO pelo diagnostico. Nao e o mesmo
// que escolher em Ajustes: Ajustes "Automatico" volta para ESTE valor, e um
// numero escolhido em Ajustes (orcFixo 3), cravado na build (1, alto-cache) ou
// no ambiente (2) continua mandando — o perfil so troca o padrao por baixo.
void tex_definir_orcamento_auto_mb(int mb) {
  int teto = tetoPermitidoMB();
  if (!mtx) return;
  if (mb <= 0) mb = ptv_tex_auto_mb(ptv_plataforma(), orcMemTotal);
  if (mb > teto) mb = teto;
  if (mb < 16) mb = 16;
  if (orcFixo == 1 || orcFixo == 2) {
    printf("[tex] perfil automatico de %d MB ignorado: orcamento cravado (%s)\n",
           mb, orcFixo == 1 ? "NV_TEX_MB_FIXO" : "NUVIO_TEX_MB");
    return;
  }
  orcAuto = mb;
  if (orcFixo == 0) tex_definir_orcamento_mb(0);
}

int tex_orcamento_auto_mb(void) { return orcAuto; }

void tex_definir_fios_rede(int n) {
  if (!mtx) return;
  SDL_LockMutex(mtx);
  if (n < 1) n = 1;
  if (n > NV_TEX_FIOS_REDE) n = NV_TEX_FIOS_REDE;
  fiosRedeAtivos = n;
  // Quem estava parado e agora vale acorda e pega a fila que esperava.
  SDL_CondBroadcast(cond);
  SDL_UnlockMutex(mtx);
  printf("[tex] fios de rede ativos: %d de %d\n", n, fiosRedeCriados);
  fflush(stdout);
}

int tex_fios_rede(void) {
  return fiosRedeAtivos < fiosRedeCriados ? fiosRedeAtivos : fiosRedeCriados;
}

void tex_definir_teto_heroi(int larg) {
  // O teto nunca passa do que o build decodifica para tela cheia no Tizen com
  // Alta (1920) nem desce abaixo do corte de card (640).
  if (larg > 1920) larg = 1920;
  tetoHeroiPerfil = larg >= 640 ? larg : 0;
  printf("[tex] teto do heroi: %d px (perfil %d)\n", tetoDoHeroi(), tetoHeroiPerfil);
  fflush(stdout);
}

int tex_teto_heroi(void) { return tetoDoHeroi(); }
int tex_teto_heroi_perfil(void) { return tetoHeroiPerfil; }

// Esquece UMA arte pronta: textura, bytes e o registro de falha. So da thread
// de desenho (apaga textura GL). Arte em voo fica: o fio escreveria no slot.
int tex_esquecer(const char *caminho) {
  int i, ok = 0;
  unsigned long h;
  if (!caminho || !*caminho || !mtx) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && (itens[i].estado == PRONTO || itens[i].estado == FALHOU) &&
      !itens[i].naFilaDec) {
    if (itens[i].tex) {
      gfx_tex_esquecer(itens[i].tex);
      glDeleteTextures(1, &itens[i].tex);
      bytesUsados -= bytesTextura(&itens[i]);
      if (bytesUsados < 0) bytesUsados = 0;
    }
    soltarBruto(&itens[i]);
    memset(&itens[i], 0, sizeof(Item));
    itens[i].lum = -1;
    itens[i].corR = -1;
    ok = 1;
  }
  SDL_UnlockMutex(mtx);
  return ok;
}

void tex_orcamento_info(int *mb, long *memTotal, int *fixo, int *slots) {
  if (mb) *mb = orcMB;
  if (memTotal) *memTotal = orcMemTotal;
  if (fixo) *fixo = orcFixo;
  if (slots) *slots = nMax;
}

void tex_threads_info(int *usadas, int *disponiveis) {
  int u = 0;
  int i;
  if (mtx) {
    SDL_LockMutex(mtx);
    for (i = 0; i < NV_TEX_FIOS; i++) if (thrs[i]) u++;
    for (i = 0; i < NV_TEX_FIOS_REDE && i < fiosRedeAtivos; i++) if (thrsRede[i]) u++;
    SDL_UnlockMutex(mtx);
  }
  if (usadas) *usadas = u;
  if (disponiveis) *disponiveis = NV_TEX_FIOS + NV_TEX_FIOS_REDE;
}

// HISTORICO DE OCUPACAO, uma amostra por segundo, para o grafico de Ajustes.
// Anel de NV_TEX_HIST amostras; quem le recebe do mais antigo ao mais novo.
#define NV_TEX_HIST 120
static long   hist[NV_TEX_HIST];
static int    histN = 0, histFim = 0;
static Uint32 histEm = 0;
long tex_despejos_total = 0;
long tex_despejos_quentes_total = 0;

static void amostrar(void) {
  Uint32 agora = SDL_GetTicks();
  if (histEm && agora - histEm < 1000) return;
  histEm = agora;
  hist[histFim] = bytesUsados;
  histFim = (histFim + 1) % NV_TEX_HIST;
  if (histN < NV_TEX_HIST) histN++;
}

int tex_historico(long *saida, int max) {
  int n, i, ini;
  if (!mtx) return 0;
  SDL_LockMutex(mtx);
  n = histN < max ? histN : max;
  ini = (histFim - n + NV_TEX_HIST) % NV_TEX_HIST;
  for (i = 0; i < n; i++) saida[i] = hist[(ini + i) % NV_TEX_HIST];
  SDL_UnlockMutex(mtx);
  return n;
}

// O que o posterprov precisa saber de uma URL de provedor: -1 falhou (HTTP,
// prazo, corpo que nao e imagem, decode), 1 ja tem textura, 0 ainda vai.
static int estadoPoster(const char *url) {
  if (tex_falhou(url)) return -1;
  return tex_aspecto(url) > 0.0f ? 1 : 0;
}

int tex_iniciar(int max_itens) {
  uploadThread = SDL_ThreadID();
  uploadContext = SDL_GL_GetCurrentContext();
  int mb = orcamentoMB();
  mipCpuAtivo();  // decide antes dos fios de decode (NUVIO_TEX_MIP)
  posterprov_hook_estado(estadoPoster);
  int fiosDecode = NV_TEX_FIOS, fiosRede = NV_TEX_FIOS_REDE;
  // Os outros dois padroes do aparelho (fios ativos e teto do heroi) saem da
  // MESMA tabela do orcamento. Ver perfiltv.c.
  { PtvPerfil pf;
    ptv_padrao(ptv_plataforma(), orcMemTotal, &pf);
    fiosRedeAtivos = pf.fiosRede;
    tetoHeroiPerfil = pf.heroiLarg;
    printf("[tex] padrao do aparelho: %d MB, %d fios de rede, heroi %d px (RAM %ld MB)\n",
           mb, pf.fiosRede, pf.heroiLarg, orcMemTotal); }
#ifdef __EMSCRIPTEN__
  /* Recover IDBFS/account data before spending bandwidth on speculative art. */
  if (dados_modo_recuperacao()) fiosDecode = fiosRede = 1;
#endif
  nMax = max_itens > 0 && max_itens <= MAX_ITENS_ABS ? max_itens : 64;
  // OS SLOTS ACOMPANHAM O ORCAMENTO. 192 slots foram dimensionados para 96-128
  // MB (~660 KB por cartaz da C9): com 300 MB o cache encheria de slots muito
  // antes de encher de bytes, e o LRU voltaria a despejar por FALTA DE VAGA
  // com dois tercos do orcamento por usar. Um slot e meio por MB cobre a
  // mistura real de cartaz, logo e heroi; o teto absoluto e MAX_ITENS_ABS.
  { int porBytes = mb * 3 / 2;
    if (porBytes > nMax) nMax = porBytes > MAX_ITENS_ABS ? MAX_ITENS_ABS : porBytes; }
  // ORCAMENTO ESCALA COM O BUFFER. Os MB sao a conta da TV, onde a escala e
  // 1. No Mac retina a escala e 2, e a MESMA cena precisa de 4x os pixels — com
  // o teto fixo a previa vivia encostada no limite, despejando arte visivel e
  // mostrando um defeito que o aparelho nao tem. Uma previa que mente e pior
  // que nao ter previa.
  //
  // Na TV o fator e 1 e nada muda; e exatamente por isso que a conta pode ser
  // esta e nao um numero maior cravado.
  { float e = escalaOrc();
    orcamento = (long)(mb * e * e) * 1024L * 1024L; }
#ifndef __EMSCRIPTEN__
  pthread_mutex_lock(&vidaMtx);
#endif
  bytesUsados = 0;
  memset(itens, 0, sizeof itens);
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
  rodando = 1;
#ifndef __EMSCRIPTEN__
  pthread_mutex_unlock(&vidaMtx);
#endif
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
    for (k = 0; k < fiosDecode; k++)
      thrs[k] = SDL_CreateThread(threadDecode, "nv-decode", NULL);
    // QUATRO fios de REDE, e eles NAO sao como os de decode: nao tocam pixel,
    // so esperam I/O. Podem rodar em prioridade normal e em maior numero sem
    // competir com o desenho — o custo de um fio parado num socket e zero de
    // CPU. Quatro cobre os quatro cartazes que entram na tela de uma vez.
    for (k = 0; k < fiosRede; k++)
      thrsRede[k] = SDL_CreateThread(threadRede, "nv-rede", (void *)(intptr_t)k);
    fiosRedeCriados = fiosRede;
    thr = thrs[0]; }
  return thr != NULL;
}

void tex_encerrar(void) {
  if (!mtx) return;
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
#ifndef __EMSCRIPTEN__
  // Nenhum mutex SDL esta tomado aqui: o callback em curso pode termina-lo,
  // e os seguintes so veem mtx NULL depois deste encerramento completo.
  pthread_mutex_lock(&vidaMtx);
#endif
  if (uplTex) glDeleteTextures(1, &uplTex);
  uplTex = 0; uplIdx = -1; uplSup = NULL; uplY = 0; uplNiveis = 0;
  for (int i = 0; i < nMax; i++) {
    if (itens[i].tex) glDeleteTextures(1, &itens[i].tex);
    if (itens[i].sup) liberarSup(itens[i].sup);
    soltarBruto(&itens[i]);
  }
  memset(itens, 0, sizeof itens);
  filaIni = filaFim = decIni = decFim = 0;
  bytesUsados = 0; fiosRedeCriados = 0;
  SDL_DestroyCond(condLivre); SDL_DestroyCond(condDec);
  SDL_DestroyCond(cond); SDL_DestroyMutex(mtx);
  condLivre = condDec = cond = NULL; mtx = NULL;
#ifndef __EMSCRIPTEN__
  pthread_mutex_unlock(&vidaMtx);
#endif
}

// Ver tex_obter_larg_qualquer: 1 durante essa chamada, e a textura menor que
// ja existe e entregue enquanto a maior e reprocessada.
static int aceitaMenor;
// Pedido de LOGO DE TITULO em curso (ver tex_obter_logo_larg): sobe o item para
// o nivel 2 da fila, a frente ate do fundo de tela cheia.
static int pedidoLogo;
static int pedidoPalco;
// Pedido de ARQUIVO em curso (tex_arquivo): o teto da variante a baixar. Ver
// Item.limiteArquivo.
static int pedidoArquivo;
// CAMINHO QUE NAO E TEXTO NAO VIRA PEDIDO. Guarda, nao conserto: o caso que a
// trouxe (lixo binario como caminho, "[tex] decode falhou (Couldn't open
// ���̑C)") era um ponteiro para bloco do catalogo ja liberado, consertado em
// catalogo.c (ver cat_quadro). Isto so impede que o proximo defeito da mesma
// familia ocupe um slot, entre na fila de decode e grave lixo no log a cada
// tentativa. URL e caminho de arquivo aqui sempre comecam por ASCII visivel
// ("http", "/", "."); byte de controle em qualquer posicao tambem recusa.
// Acento no MEIO (UTF-8) continua passando.
static int caminhoInvalido(const char *c) {
  const unsigned char *s = (const unsigned char *)c;
  if (*s <= 0x20 || *s >= 0x7f) return 1;
  for (; *s; s++) if (*s < 0x20 || *s == 0x7f) return 1;
  return 0;
}

int tex_disco_direto = 0;
// ARTE QUE JA ESTA NO DISCO NAO ENTRA NA FILA DE REDE (05/10/2026, B3).
//
// MEDIDO na C9 (logs de 04 e 05/10, leitura): das 389 esperas `fila-rede`
// acima de 250 ms, 328 sao de pedidos cujo fetch NAO apareceu como lento —
// acertos de disco (um fopen) que esperaram, somados, 150 s atras dos quatro
// fios ocupados com downloads de 0,6-2 s (net_ms p50 650, p90 1,5-2,1 s) e
// com consultas de fonte de 1,5 s. Ou seja: arte que a TV ja tinha esperava a
// internet dos OUTROS cartazes. E o "na C9 a arte demora a aparecer" com o
// cache de disco cheio (1.300+ arquivos).
//
// Aqui o fio de desenho pergunta ao INDICE do cache de disco (nomes em RAM,
// sem I/O) se o arquivo da variante certa existe; existindo, o item vai
// direto para a fila de DECODE, que ja le do arquivo (garantirLocal so
// traduz a URL). Arquivo que a poda apagou no meio do caminho cai no
// garantirLocal do decode, que baixa como antes. Fila de decode cheia ou
// indice ainda nao montado: fila de rede, como sempre foi.
static int discoDireto(int idx) {
#ifndef __EMSCRIPTEN__
  char certo[NV_TEX_URL_MAX], dst[600];
  int usar;
  if (!dirCache[0] || !itens[idx].caminho[0]) return 0;
  { int lim = limiteDaVariante(&itens[idx]);
    usar = arte_tamanho_url(itens[idx].caminho, lim, certo, sizeof certo);
    nomeDeCache(usar ? certo : itens[idx].caminho, dst, sizeof dst);
    if (!cachearte_nativo_indice_tem(dst)) return 0;
    itens[idx].limiteTamanho = usar ? lim : 0; }
  if (!enfileirarDecodeSemEspera(idx)) return 0;
  tex_disco_direto++;
  if (itens[idx].urgente == 2) logoOrigem(itens[idx].caminho, "disco-direto", 0);
  return 1;
#else
  (void)idx;
  return 0;
#endif
}

#ifdef NV_TPK
// ARTE DO PACOTE QUE FALTA (#290): ver artefalta.h. A auto-atualizacao do .tpk
// troca so a .so; o res/art fica o da instalacao. Arquivo de res/art que nao
// existe vira a URL da mesma tag no GitHub e segue pela fila de rede/cache de
// disco como qualquer arte remota. O access() roda UMA vez por caminho: a
// resposta fica numa tabela de hashes (o pedido se repete a cada quadro).
#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
#define ARTE_VISTA_N 512
static unsigned long arteVistaH[ARTE_VISTA_N];
static unsigned char arteVistaE[ARTE_VISTA_N];   // 0 livre, 1 existe, 2 falta
static int arteFaltas;
static const char *artePacoteOuRemota(const char *caminho, char *buf, size_t tam) {
  unsigned long h;
  int k, slot = -1, estado = 0;
  if (!caminhoLocal(caminho) || !strstr(caminho, ARTE_RAIZ_PACOTE)) return caminho;
  h = hashCaminho(caminho);
  for (k = 0; k < 8; k++) {
    int j = (int)((h + (unsigned long)k) % ARTE_VISTA_N);
    if (arteVistaE[j] && arteVistaH[j] == h) { estado = arteVistaE[j]; break; }
    if (!arteVistaE[j]) { slot = j; break; }
  }
  if (!estado) {
    estado = access(caminho, R_OK) == 0 ? 1 : 2;
    if (slot >= 0) { arteVistaH[slot] = h; arteVistaE[slot] = (unsigned char)estado; }
    if (estado == 2 && arteFaltas < 64) {
      // Uma linha por arquivo (a tabela lembra): e o que diz no log quais
      // artes a .so nova pede e o pacote instalado nao tem.
      arteFaltas++;
      printf("[arte] falta no pacote (%d): %s -> tag v%s\n", arteFaltas,
             strstr(caminho, ARTE_RAIZ_PACOTE) + strlen(ARTE_RAIZ_PACOTE), NV_VERSAO);
      fflush(stdout);
    }
  }
  if (estado != 2) return caminho;
  return arte_url_remota(caminho, NV_VERSAO, buf, tam) ? buf : caminho;
}
#endif

static GLuint tex_obter_limite(const char *caminho, int limite, int urgente,
                               int passageiro) {
#ifdef NV_TIZEN3_LITE
  if (limite > NV_TEX_HERO_LARG_MAX) limite = NV_TEX_HERO_LARG_MAX;
#endif
  if (!caminho || !*caminho || strlen(caminho) >= NV_TEX_URL_MAX) return 0;
#ifdef NV_TPK
  char arteRemota[NV_TEX_URL_MAX];
  caminho = artePacoteOuRemota(caminho, arteRemota, sizeof arteRemota);
#endif
  if (pedidoLogo) urgente = 2;
  if (caminhoInvalido(caminho)) {
    static int avisos;
    if (avisos < 3) {
      const unsigned char *b = (const unsigned char *)caminho;
      char hex[3 * 12 + 1];
      int k, p = 0;
      avisos++;
      // Os bytes em hexa e nao a string: e o que diz de onde o lixo veio
      // (ponteiro do alocador, cabecalho de JPEG, texto de outro campo).
      for (k = 0; k < 12 && b[k]; k++)
        p += snprintf(hex + p, sizeof hex - (size_t)p, "%02x ", b[k]);
      hex[p] = 0;
      printf("[tex] caminho recusado: nao e texto (%s)\n", hex);
      fflush(stdout);
    }
    return 0;
  }
  GLuint saida = 0;
  unsigned long h = hashCaminho(caminho);
  int i; BUSCA_MEDIDA(i, caminho, h);
  // ITEM 4: recusa ANTES do fetch. So vale para pedido NOVO (slot ainda nao
  // existe): arte que ja esta no cache, em voo ou aquecida passa, e e ela que
  // serve a janela visivel. Nivel 2 (fora da tela) recusa sempre; nivel 1 (na
  // borda) so em movimento; o lookahead (item 1) aquece a vizinhanca em repouso.
  // BUSCA_MEDIDA tomou o mutex: sair sem solta-lo congelava rede e decode
  // (cartazes e icones sumiam na C9/TCL, tests/plx_recusa_trava.sh).
  //
  // NIVEL 3: FORA DA TELA NO LADO EM QUE A ROLAGEM ENTRA. A faixa de corte do
  // desenho sobra ~1,5 card alem da borda, e a arte desse lado e demanda com
  // alguns quadros de antecedencia — recusa-la e o defeito "o poster carrega
  // na frente". Ela passa como PASSAGEIRA: se a rolagem inverter antes, sai
  // primeiro e nunca despeja cartaz visivel.
  if (i < 0 && pedidoFora && pedidoFora != 3 &&
      (pedidoFora >= 2 || gradeMovendo)) {
    tex_n_recusa++; SDL_UnlockMutex(mtx); return 0;
  }
  if (i >= 0 && pedidoFora == 3) itens[i].passageiro = 1;
  if (i >= 0 && itens[i].estado == FALHOU) {
    itens[i].ultimoQuadro = quadroAtual;
    itens[i].ultimoPedido = SDL_GetTicks();
    if (urgente > itens[i].urgente) itens[i].urgente = urgente;
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
      itens[i].estado = PENDENTE;
      itens[i].uso = ++relogio;
      if (itens[i].localDireto) {
        (void)enfileirarDecodeSemEspera(i);
      } else {
        int prox = (filaFim + 1) % MAX_FILA;
      if (prox != filaIni) {
          fila[filaFim] = i; filaFim = prox;
          itens[i].filaRedeEm = SDL_GetTicks();
          acordarRede();
        } else {
          itens[i].estado = FALHOU;
        }
      }
    }
    // Falhou mas ainda vai tentar de novo: e arte que pode chegar, e o fundo
    // parado do painel de Salvos precisa saber (ver spainel_fundo).
    if (itens[i].falhas < 4) tex_n_falta++;
    SDL_UnlockMutex(mtx);
    return 0;
  }
  if (i >= 0) {
    itens[i].ultimoQuadro = quadroAtual;
    itens[i].lookahead = 0;
    itens[i].ultimoPedido = SDL_GetTicks();
    itens[i].uso = ++relogio;
    if (pedidoArquivo > itens[i].limiteArquivo) itens[i].limiteArquivo = pedidoArquivo;
    if (itens[i].localDireto && itens[i].estado == PENDENTE && !itens[i].naFilaDec)
      (void)enfileirarDecodeSemEspera(i);
    // Marca mesmo quem JA esta na fila: a arte do hero costuma ter sido pedida
    // antes, como poster da fileira, e e exatamente esse item que precisa
    // furar a fila agora.
    if (urgente && itens[i].estado != PRONTO) {
      if (urgente > itens[i].urgente) {
        printf("[tex-trace] pedido hash=%08lx role=%s limite=%d\n", h,
               urgente > 1 ? "logo" : "hero", limite);
        // DEDUP: o pedido urgente pegou carona num item que ja estava a
        // caminho (aquecimento, pre-busca ou o cartaz da fileira) — nao e
        // download novo. `aquecido`=1: a variante ja e a do destaque.
        if (itens[i].estado == PENDENTE || itens[i].estado == DECODIFICADO)
          printf("[tex-trace] dedup hash=%08lx role=%s estado=%s aquecido=%d\n", h,
                 urgente > 1 ? "logo" : "hero",
                 itens[i].estado == DECODIFICADO ? "decodificado"
                   : (itens[i].tex ? "promocao" : "em-voo"),
                 itens[i].limiteArquivo >= limite);
      }
      if (urgente > itens[i].urgente) itens[i].urgente = urgente;
    }
    // PROMOCAO: a mesma arte pode ser pedida como poster (960) e depois como
    // hero (1920). Se o teto novo e maior e a textura pronta ficou menor que
    // ele, refaz — senao o hero herda para sempre a versao pequena que o card
    // pediu primeiro, e o borrao volta sem explicacao aparente.
    if (pedidoPalco || limite > itens[i].limite) itens[i].limite = limite;
    if (limite < NV_TEX_MIP_LARG && !pedidoArquivo) itens[i].card = 1;
    // PROMOCAO PELO TETO REALMENTE USADO, e nao pelo pedido.
    //
    // `w < limite` NAO basta: um poster da Cinemeta tem 250px de origem, e
    // pedi-lo a 320 refaz o decode para devolver os mesmos 250 — trabalho puro,
    // mais o cinza enquanto refaz. `fonteW` responde isso com o numero medido
    // no decode: so vale re-decodificar se a FONTE tem mais pixels do que a
    // textura atual.
    //
    // E a condicao e reavaliada em TODO pedido enquanto a textura estiver
    // abaixo do teto. Antes ela dependia de `limite` subir, e `limite` subia
    // mesmo quando o enfileiramento falhava: bastava a fila de decode estar
    // cheia naquele quadro — o que no arranque, com 32 itens em voo, e o caso
    // comum — para o heroi ficar presturado na miniatura do cartaz ate o item
    // ser despejado. Com o orcamento de 128 MB e a arte da tela protegida do
    // despejo, "ate ser despejado" pode ser a sessao inteira.
    //
    // `fonteW` DESCONHECIDO (0) NAO BLOQUEIA. So se bloqueia a promocao quando
    // se SABE que a fonte acabou — fonte medida menor ou igual a textura que ja
    // esta na mao. Ha textura publicada por caminhos que nao passam pelo decode
    // (o quadro de GIF que sobe GPU->GPU, e qualquer item montado a mao), e
    // tratar "nao sei" como "acabou" congelaria essas na primeira versao
    // pequena que aparecesse.
    //
    // COM FOLGA DE 25%, senao a promocao vira o custo dominante da navegacao.
    // MEDIDO na C9 em 19/09 (log com o dono abrindo um filme): o MESMO jpeg
    // decodificado a 736 e de novo a 768 (o card focado e 4% maior), o mesmo
    // logo a 480, 512 e 640 conforme a animacao de entrada do detalhe o
    // desenhava maior — cada decode de 300 a 500 ms nesta CPU, para uma
    // diferenca que o olho nao ve a 3 m. A pessoa ve a arte "recarregar"
    // duas ou tres vezes (o cinza entre uma versao e outra). Promover so
    // quando o pedido e ao menos um quarto maior: o card (928) que vira hero
    // (1920) continua promovendo; o foco e a animacao, nao.
    if (itens[i].estado == PRONTO &&
        ((pedidoPalco && itens[i].w > limite) ||
         (itens[i].tetoUsado < limite &&
          limite >= itens[i].tetoUsado + itens[i].tetoUsado / 4 &&
          (itens[i].fonteW <= 0 || itens[i].fonteW > itens[i].w)))) {
      if (itens[i].localDireto) {
        itens[i].estado = PENDENTE;
        (void)enfileirarDecodeSemEspera(i);
      } else if ((itens[i].estado = PENDENTE, discoDireto(i))) {
        /* do disco, sem rede */
      } else {
        int prox = (filaFim + 1) % MAX_FILA;
        itens[i].estado = PRONTO;
        if (prox != filaIni) {
          itens[i].estado = PENDENTE;
          fila[filaFim] = i; filaFim = prox;
          itens[i].filaRedeEm = SDL_GetTicks();
          acordarRede();
        }
      }
      // Fila cheia: nada a fazer aqui. `tetoUsado` continua no valor antigo,
      // entao o pedido do quadro seguinte volta a este mesmo ponto.
    }
    // A TEXTURA ANTIGA SERVE DURANTE A PROMOCAO — quando ela tem pelo menos
    // METADE da largura pedida. O card que abre em 16:9 e pede 544 ja tem o
    // poster de 288: devolver 0 aqui trocava arte por cinza por uns quadros a
    // cada foco, e o cinza e o "pisca". Um pedido novo tem tex == 0 e cai no
    // mesmo caminho de sempre.
    //
    // A metade e a guarda contra o caso oposto: home.c aquece o arquivo do
    // hero com tex_arquivo, que e um pedido de 128 px. Sem a guarda, o hero
    // receberia essa miniatura esticada a 1920 — um borrao de tela cheia por
    // ~100 ms a cada troca — em vez de esperar a grande, que e o que o
    // crossfade dele ja sabe fazer.
    saida = (itens[i].estado == PRONTO || itens[i].w * 2 >= limite || aceitaMenor)
              ? itens[i].tex : 0;
    // A larger Current/Detail cache entry must finish resizing before Stage uses it.
    if (pedidoPalco && itens[i].w > limite) saida = 0;
  } else {
    int novo = slotLivre();
    if (novo >= 0) {
      // Slot novo nao herda bytes de ninguem (ver desistir): o que estiver
      // em `bruto` e de outro caminho e o decode o preferiria ao arquivo.
      soltarBruto(&itens[novo]);
      strncpy(itens[novo].caminho, caminho, sizeof itens[novo].caminho - 1);
      itens[novo].hash = h;
      itens[novo].limite = limite;
      itens[novo].card = limite < NV_TEX_MIP_LARG && !pedidoArquivo;
      itens[novo].limiteTamanho = 0;
      itens[novo].limiteArquivo = pedidoArquivo;
      itens[novo].estado = PENDENTE;
      itens[novo].uso = ++relogio;
      itens[novo].ultimoQuadro = quadroAtual;
      itens[novo].ultimoPedido = SDL_GetTicks();
      itens[novo].urgente = urgente;
      itens[novo].passageiro = passageiro || pedidoFora == 3 ? 1 : 0;
      itens[novo].localDireto = caminhoLocal(caminho);
      if (itens[novo].localDireto) {
        // Fila cheia nao transforma caminho local em espera de rede nem trava
        // a UI; o estado PENDENTE sera reenfileirado no proximo pedido.
        (void)enfileirarDecodeSemEspera(novo);
      } else if (discoDireto(novo)) {
        /* do disco, sem rede */
      } else {
        int prox = (filaFim + 1) % MAX_FILA;
        if (prox != filaIni) {
          fila[filaFim] = novo; filaFim = prox;
          itens[novo].filaRedeEm = SDL_GetTicks();
          if (urgente)
            printf("[tex-trace] pedido hash=%08lx role=%s limite=%d\n", h,
                   urgente > 1 ? "logo" : "hero", limite);
          acordarRede();
        }
        else { itens[novo].estado = VAZIO; itens[novo].caminho[0] = 0; } // fila cheia
      }
    }
  }
  if (!saida) tex_n_falta++;
  SDL_UnlockMutex(mtx);
  return saida;
}

GLuint tex_obter(const char *caminho) {
  return tex_obter_limite(caminho, NV_TEX_LARG_MAX, 0, 0);
}

// Teto de decodificacao a partir da largura de DESENHO. Partilhado por
// tex_obter_larg e tex_obter_passageira para que o mesmo arquivo pedido pelos
// dois caminhos caia no mesmo teto e nao seja promovido a toa.
static int capDeLargura(float largLayout) {
  int cap;
  // Convert layer coordinates to output pixels before choosing URL/decode size.
  // Keep layer scale out of escalaBuf: it must not inflate the byte budget.
  cap = (int)(largLayout * gfx_escala() * escalaBuf * folgaDaQualidade() + 0.5f);
  // Arredonda para multiplo de 32: sem isso cada largura de desenho vira um
  // teto proprio, e a mesma arte pedida por dois lugares com poucos pixels de
  // diferenca era promovida e RE-DECODIFICADA sem ganho visivel.
  cap = ((cap + 31) / 32) * 32;
  if (cap < 128) cap = 128;
  if (cap > tetoDoHeroi()) cap = tetoDoHeroi();
  return cap;
}

void tex_cache_marcar_larg(int grupo, const char *url, float largLayout,
                           int essencial, int emUso) {
  char urlReal[NV_TEX_URL_MAX];
  int cap = largLayout <= 1.0f ? NV_TEX_LARG_MAX : capDeLargura(largLayout);
  int variante = NV_CACHE_ARTE_MEDIUM;
  if (!url || !*url) return;
  snprintf(urlReal, sizeof urlReal, "%s", url);
#ifdef __EMSCRIPTEN__
  if (arte_tamanho_fundo_pequeno(cap, qualidadeImg == 2)) {
    variante = NV_CACHE_ARTE_SMALL;
    const char *m = strstr(url, "images.metahub.space/background/medium/");
    if (m) {
      snprintf(urlReal, sizeof urlReal, "%.*simages.metahub.space/background/small/%s",
               (int)(m - url), url,
               m + strlen("images.metahub.space/background/medium/"));
    }
  }
#endif
  cachearte_marcar_grupo(grupo, urlReal, variante, essencial, emUso);
}

GLuint tex_obter_larg(const char *caminho, float largLayout) {
  int cap;
  if (largLayout <= 1.0f) return tex_obter(caminho);
  cap = capDeLargura(largLayout);
  // Tela cheia fura a fila; card de fileira, nao (ver `urgente` no Item).
  return tex_obter_limite(caminho, cap, cap > NV_TEX_LARG_MAX, 0);
}

int tex_lookahead_parado(TexLookahead *s, float scroll) {
  float delta = scroll - s->scroll;
  if (!s->quadro || fabsf(delta) > 0.25f || s->quadro + 1 != quadroAtual) {
    s->parados = 0;
    if (fabsf(delta) > 0.25f) s->direcao = delta > 0 ? 1 : -1;
  } else if (s->parados < 2) s->parados++;
  s->scroll = scroll; s->quadro = quadroAtual;
  return s->parados >= 2;
}

// VERSAO EM MOVIMENTO do lookahead. Parado, a janela nao muda e a vizinhanca
// e coberta pelo caminho acima; rolando, porem, o parado nunca dispara (a
// posicao muda todo quadro) e a borda que ENTRA e quem precisa de arte — se
// ela so pede ao aparecer, a pessoa ve o esqueleto (o relato do poster que
// "carrega na frente"). Devolve o sinal da rolagem (+1/-1) uma vez por
// quadro por chamador; 0 = sem direcao ou ja usado neste quadro. O chamador
// traduz o sinal no proximo cartao da borda e pede (tex_lookahead_pedir,
// que em movimento nao espera fila vazia).
int tex_lookahead_mover(TexLookahead *s) {
  if (s->quadro == quadroAtual) return 0;
  s->quadro = quadroAtual;
  return gradeRolDir > 0.0f ? 1 : gradeRolDir < 0.0f ? -1 : 0;
}

int tex_lookahead_pedir(const char *caminho, float largura) {
  static unsigned long pedidoQuadro;
  // Em movimento vale um pedido a cada DOIS quadros: quem entra na janela e
  // demanda, nao especulacao, e esperar o repouso e o defeito que se corrige.
  if (!mtx || !caminho || !*caminho ||
      (pedidoQuadro &&
       quadroAtual - pedidoQuadro < (gradeMovendo ? 2u : 1u))) return 0;
  SDL_LockMutex(mtx);
  int i = acharIndice(caminho, hashCaminho(caminho));
  // Nao promove nem despeja arte para especular. Em voo inclui trabalho que
  // os consumidores ja retiraram das filas, mas ainda nao publicaram.
  int livre = 0, adiante = 0;
  for (int j = 0; j < nMax; j++) {
    if (itens[j].estado == VAZIO) livre++;
    if (itens[j].lookahead) adiante++;
  }
  // Parado: so especula com o cache ocioso (nunca atrasa demanda). ROLANDO:
  // a demanda existe sempre — as filas nunca esvaziam — entao o criterio e
  // so nao saturar os slots nem a memoria; a arte pedida sera demanda a
  // seguir, e ja entra com a largura certa.
  int pode = i < 0 && livre > 0 && adiante < 30 &&
             !pressaoAtiva() && bytesUsados < orcamento * 3 / 4 &&
             (gradeMovendo ? emVoo() < nMax / 3 - 2
                           : (!emVoo() && filaIni == filaFim && decIni == decFim));
  SDL_UnlockMutex(mtx);
  if (!pode) return 0;
  pedidoQuadro = quadroAtual;
  // O pedido e ESPECULATIVO por definicao: nao vale a marca `pedidoFora` que o
  // desenho deixou no ultimo cartao — senao a propria pre-busca seria recusada.
  int foraAntes = pedidoFora; pedidoFora = 0;
  (void)tex_obter_larg(caminho, largura);
  pedidoFora = foraAntes;
  SDL_LockMutex(mtx);
  i = acharIndice(caminho, hashCaminho(caminho));
  if (i >= 0) {
    itens[i].lookahead = 1;
    itens[i].ultimoQuadro = 0; // nao conta como card visivel/quente
  }
  SDL_UnlockMutex(mtx);
  return 1;
}

// Como tex_obter_larg, mas ENTREGA O QUE JA EXISTE: se o arquivo foi
// decodificado menor (o logo do card aberto, ~370 px) e o pedido e maior, a
// textura pequena volta ja, ampliada, enquanto a grande e reprocessada — em
// vez de um buraco ate ela chegar. Para arte que e a mesma em dois lugares
// (logo do titulo no card e no detalhe).
GLuint tex_obter_larg_qualquer(const char *caminho, float largLayout) {
  GLuint t;
  aceitaMenor = 1;
  t = tex_obter_larg(caminho, largLayout);
  aceitaMenor = 0;
  return t;
}

// LOGO DE TITULO: um PNG de dezenas de KB que a pessoa espera ao lado do nome.
// MEDIDO na C9 (logcat de 04/10): a fila de rede e FIFO e o logo, pedido como
// arte de card, esperava 0,5 a 0,85 s atras de cartazes e fundos de fileira
// (fila-rede wait=..., urgente=0) antes de comecar a baixar; o decode (dois
// fios, fundos de 3840x2160 de 300-500 ms) e a mesma fila. Nivel 2 fura as duas.
GLuint tex_obter_logo_larg(const char *caminho, float largLayout) {
  GLuint t;
  pedidoLogo = 1;
  t = tex_obter_larg(caminho, largLayout);
  pedidoLogo = 0;
  return t;
}

GLuint tex_obter_logo_larg_qualquer(const char *caminho, float largLayout) {
  GLuint t;
  pedidoLogo = 1;
  t = tex_obter_larg_qualquer(caminho, largLayout);
  pedidoLogo = 0;
  return t;
}

GLuint tex_obter_passageira(const char *caminho, float largLayout) {
  int cap = largLayout <= 1.0f ? NV_TEX_LARG_MAX : capDeLargura(largLayout);
  return tex_obter_limite(caminho, cap, 0, 1);
}

// Arte que ocupa a tela inteira: hero da home, backdrop do detalhe e a arte do
// player. 1920 e a largura do painel — pedir mais so gastaria memoria, pedir
// menos e ampliar depois.
GLuint tex_obter_hero(const char *caminho) {
  return tex_obter_limite(caminho, tetoDoHeroi(), 1, 0);
}

GLuint tex_obter_palco(const char *caminho) {
  int cap = tetoDoHeroi();
  pedidoPalco = 1;
  GLuint t = tex_obter_limite(caminho, cap < 1280 ? cap : 1280, 1, 0);
  pedidoPalco = 0;
  return t;
}

// O MESMO HERO, SEM FURAR A FILA: o vizinho do carrossel que ainda nao esta em
// cena. Mesmo teto e mesmo item de tex_obter_hero (a chegada promove o pedido
// a urgente, e tirarFila o puxa para a frente); so nao toma o lugar de quem
// ja esta esperando como tela cheia.
GLuint tex_obter_hero_quente(const char *caminho) {
  return tex_obter_limite(caminho, tetoDoHeroi(), 0, 0);
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
  // O ARQUIVO SUMIU, MAS ERA GIF (#141): o "limite conhecido" da nota acima.
  // A poda (podarCacheDisco) apaga o arquivo e o item continua PRONTO com a
  // textura de um quadro — o pedido la embaixo so encostaria no LRU, e o
  // cartaz ficaria parado para sempre. Pede o arquivo de novo UMA vez por
  // item (rebaixouGif): o que acabou de chegar e o mais novo da pasta, e a
  // poda apaga do mais velho, entao ele fica. A textura antiga segue na tela
  // durante o download (a mesma regra da promocao em tex_obter_limite).
  if (mtx) {
    unsigned long h = hashCaminho(url);
    int i;
    BUSCA_MEDIDA(i, url, h);
    if (i >= 0 && itens[i].estado == PRONTO && itens[i].temMagica &&
        !memcmp(itens[i].magica, "GIF8", 4) && !itens[i].rebaixouGif &&
        !itens[i].naFilaDec && !itens[i].localDireto) {
      int prox = (filaFim + 1) % MAX_FILA;
      if (prox != filaIni) {
        itens[i].rebaixouGif = 1;
        itens[i].estado = PENDENTE;
        fila[filaFim] = i; filaFim = prox;
        itens[i].filaRedeEm = SDL_GetTicks();
        acordarRede();
        printf("[tex] GIF saiu do cache de disco (poda): pedindo o arquivo de novo, uma vez\n");
        fflush(stdout);
      }
    }
    SDL_UnlockMutex(mtx);
  }
  // 128 e o teto MINIMO que tex_obter_limite aceita pelo caminho normal; o que
  // interessa e o efeito colateral, que e o arquivo no disco.
  pedidoArquivo = tetoDoHeroi();
  tex_obter_limite(url, 128, 0, 0);
  pedidoArquivo = 0;
  return NULL;
}

int tex_cores(const char *caminho, unsigned char out[16*16*4]) {
  int tem = 0;
  if (!mtx || !caminho) return 0;
  SDL_LockMutex(mtx);
  for (int i = 0; i < nMax; i++) if (!strcmp(itens[i].caminho, caminho) &&
      (itens[i].estado == PRONTO || itens[i].estado == DECODIFICADO)) {
    memcpy(out, itens[i].cores, sizeof itens[i].cores); tem = 1; break;
  }
  SDL_UnlockMutex(mtx);
  return tem;
}

void tex_publicar_paleta(const char *caminho) {
  CorvivaPaleta pal;
  int i, tem = 0;
  if (!mtx || !caminho || !*caminho || corviva_paleta(caminho, &pal)) return;
  BUSCA_MEDIDA(i, caminho, hashCaminho(caminho));
  if (i >= 0 && (itens[i].estado == DECODIFICADO || itens[i].estado == PRONTO)) {
    pal = itens[i].paleta;
    tem = 1;
  }
  SDL_UnlockMutex(mtx);
  if (tem) corviva_anotar(caminho, &pal);
}

int tex_magica(const char *caminho, unsigned char magica[4]) {
  int tem = 0, i;
  unsigned long h;
  if (!caminho || !*caminho || !mtx) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].temMagica) { memcpy(magica, itens[i].magica, 4); tem = 1; }
  SDL_UnlockMutex(mtx);
  return tem;
}

int tex_falhou(const char *caminho) {
  int falhou = 0;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0) falhou = (itens[i].estado == FALHOU);
  SDL_UnlockMutex(mtx);
  return falhou;
}

int tex_largura_fonte(const char *caminho) {
  int w = 0, i;
  unsigned long h;
  if (!caminho || !*caminho) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex) w = itens[i].fonteW > 0 ? itens[i].fonteW : itens[i].w;
  SDL_UnlockMutex(mtx);
  return w;
}

float tex_aspecto(const char *caminho) {
  if (!caminho || !*caminho) return 0.0f;
  float a = 0.0f;
  unsigned long h = hashCaminho(caminho);
  int i; BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex && itens[i].h > 0)
    a = (float)itens[i].w / (float)itens[i].h;
  SDL_UnlockMutex(mtx);
  return a;
}

int tex_opaca(const char *caminho) {
  int r = 0;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex) r = itens[i].opaca;
  SDL_UnlockMutex(mtx);
  return r;
}

int tex_luminancia(const char *caminho) {
  int r = -1;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return -1;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex) r = itens[i].lum;
  SDL_UnlockMutex(mtx);
  return r;
}

int tex_cor_fundo(const char *caminho, float *r, float *g, float *b) {
  int ok = 0;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return 0;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex) {
    if (itens[i].corR >= 0) {
      if (r) *r = itens[i].corR / 255.0f;
      if (g) *g = itens[i].corG / 255.0f;
      if (b) *b = itens[i].corB / 255.0f;
      ok = 1;
    } else if (itens[i].corR == -2) ok = 2;
  }
  SDL_UnlockMutex(mtx);
  return ok;
}

int tex_logo_tom_unico(const char *caminho) {
  int r = -1;
  unsigned long h;
  int i;
  if (!caminho || !*caminho) return -1;
  h = hashCaminho(caminho);
  BUSCA_MEDIDA(i, caminho, h);
  if (i >= 0 && itens[i].tex && itens[i].desvio1 > 0)
    r = (itens[i].desvio1 - 1) <= NV_LOGO_TOM_UNICO_DESVIO;
  SDL_UnlockMutex(mtx);
  return r;
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
  if (i >= 0 && itens[i].tex && itens[i].lum >= 0)
    r = (itens[i].lum < NV_LOGO_LUM_MIN && itens[i].croma < NV_LOGO_CROMA_MAX);
  SDL_UnlockMutex(mtx);
  return r;
}

int    tex_upl_n;
long   tex_upl_bytes;

// ENVIO EM FAIXAS (05/10/2026, B3). Textura acima de NV_TEX_FAIXA_BYTES sobe
// em pedacos de linhas, um orcamento de quadro por vez, em vez de num
// glTexImage2D so.
//
// MEDIDO na C9 (logs de 04 e 05/10, leitura): o heroi de 1920x1080 (8,3 MB)
// custa gl_ms 14-58 por envio (p50 ~21) — 38 envios de heroi registrados,
// cada um um quadro perdido inteiro (ou dois) no fio que desenha, e o resto
// do custo aparece no glClear do quadro seguinte. O orcamento de 4 ms por
// quadro (NV_TEX_UPLOAD_BUDGET_MS) nunca valeu para ele: o primeiro envio do
// quadro passa sempre, e ele era um envio so.
//
// Com faixas de 2 MB o mesmo heroi vira 4 pedacos de ~270 linhas. A textura
// so e PUBLICADA (PRONTO) quando a ultima faixa sobe: ate la quem desenha ve
// o que ja via (o cinza/crossfade, ou a versao menor na promocao), nunca uma
// textura pela metade. Num aparelho rapido o laco abaixo sobe varias faixas no
// mesmo quadro, enquanto o orcamento deixar, e nada muda. A superficie fica
// no item (`sup`) durante o envio, entao tex_novo_quadro nao a trata como
// pedido velho e nenhum outro caminho mexe num item DECODIFICADO.
#define NV_TEX_FAIXA_BYTES ((long)cachePerfil().uploadKiB * 1024)
// Rastro do envio em faixas: os mesmos numeros do upload-total.
static Uint32 uplEm, uplGlMs, uplFila;
static char uplPath[512];

// A failed allocation/stripe/mipmap is not publishable. Keep the old artwork
// on promotion; first uploads use the existing failure backoff.
static int envioFalhou(int alvo, GLuint t, SDL_Surface *sup) {
  GLenum erro = glGetError();
  if (erro == GL_NO_ERROR) return 0;
  printf("[tex] upload failed hash=%08lx GL=%#x; keeping previous artwork\n",
         hashCaminho(itens[alvo].caminho), (unsigned)erro);
  while (glGetError() != GL_NO_ERROR) { }
  gfx_tex_esquecer(t);
  gfx_tex_esquecer(0);  // upload binds bypass gfx_rect, including failed uploads
  glDeleteTextures(1, &t);
  if (uplIdx == alvo) {
    uplTex = 0; uplIdx = -1; uplSup = NULL; uplY = 0; uplNiveis = 0;
  }
  SDL_LockMutex(mtx);
  if (itens[alvo].sup == sup) itens[alvo].sup = NULL;
  if (itens[alvo].tex) desistir(alvo);
  else {
    itens[alvo].estado = FALHOU;
    itens[alvo].urgente = 0;
    itens[alvo].falhas++;
    itens[alvo].tentarEm = SDL_GetTicks() +
        (itens[alvo].falhas == 1 ? 2000 : (itens[alvo].falhas == 2 ? 10000 : 60000));
  }
  SDL_UnlockMutex(mtx);
  liberarSup(sup);
  return 1;
}

// Publica a textura `t` (ja com todos os pixels) no item `alvo`. Chamada sem
// a trava; `sup` ja saiu do item.
static int publicarEnvio(int alvo, GLuint t, SDL_Surface *sup, Uint32 uploadEm,
                          Uint32 glMs, Uint32 filaWait, const char *uploadPath) {
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
  int comMip = temPiramide(sup->w, sup->h, itens[alvo].card);
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
  // CPU mip allocation failure must not schedule GPU generation on the
  // render thread. A linear base texture remains complete and usable.
  if (comMip && mipCpuAtivo() && !sup->userdata) comMip = 0;
  if (envioFalhou(alvo, t, sup)) return 0;
  Uint32 mipEm = SDL_GetTicks();
  glBindTexture(GL_TEXTURE_2D, t);
  // Niveis montados na CPU (tex_piramide_cpu) ja subiram em tex_bombear: o
  // driver nao gera nada. Sem eles, o glGenerateMipmap de sempre.
  if (comMip && !sup->userdata) glGenerateMipmap(GL_TEXTURE_2D);
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
  if (envioFalhou(alvo, t, sup)) return 0;
  Uint32 mipMs = SDL_GetTicks() - mipEm;
  Uint32 warmEm = SDL_GetTicks();
  gfx_tex_aquecer(t);
  if (envioFalhou(alvo, t, sup)) return 0;
  Uint32 warmMs = SDL_GetTicks() - warmEm;
  if (glMs >= NV_TEX_TRACE_UPLOAD_MS || mipMs >= NV_TEX_TRACE_UPLOAD_MS ||
      warmMs >= NV_TEX_TRACE_UPLOAD_MS)
    printf("[tex-trace] upload-stages hash=%08lx pixels_ms=%u mip_ms=%u warm_ms=%u\n",
           hashCaminho(uploadPath), (unsigned)glMs, (unsigned)mipMs, (unsigned)warmMs);
  glMs += SDL_GetTicks() - mipEm;

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
    bytesUsados -= bytesTextura(&itens[alvo]);
    if (bytesUsados < 0) bytesUsados = 0;
  }
  int logoPedido = itens[alvo].urgente == 2;
  itens[alvo].tex = t; itens[alvo].w = sup->w; itens[alvo].h = sup->h;
  itens[alvo].mip = comMip;
  itens[alvo].estado = PRONTO;
  itens[alvo].urgente = 0;
  itens[alvo].falhas = 0;
  bytesUsados += bytesTextura(&itens[alvo]);
  podar();
  SDL_UnlockMutex(mtx);
  { long uploadBytes = (long)sup->w * sup->h * 4;
    Uint32 uploadMs = SDL_GetTicks() - uploadEm;
    liberarSup(sup);
    if (logoPedido || uploadMs >= NV_TEX_TRACE_UPLOAD_MS || glMs >= NV_TEX_TRACE_UPLOAD_MS)
      printf("[tex-trace] upload-total hash=%08lx total_ms=%u gl_ms=%u bytes=%ld mip=%d queue=%u\n",
             hashCaminho(uploadPath), (unsigned)uploadMs, (unsigned)glMs,
             uploadBytes, comMip, (unsigned)filaWait); }
  return 1;
}

// Custos iniciais conservadores da pesquisa plx (ms); o pior observado neste
// contexto GL, incluindo mip e warm-up, so aumenta durante a sessao.
static double uplPiorMs[3] = { 2.5, 1.0, 6.0 }; // cartaz, logo, residencia SOLO
static void medirEnvio(int classe, Uint64 inicio) {
  double ms = (SDL_GetPerformanceCounter() - inicio) * 1000.0 /
              SDL_GetPerformanceFrequency();
  if (ms > uplPiorMs[classe]) uplPiorMs[classe] = ms;
}

static void uplAbortar(void) {
  if (uplTex) glDeleteTextures(1, &uplTex);
  uplTex = 0; uplIdx = -1; uplSup = NULL; uplY = 0; uplNiveis = 0;
}

int tex_bombear(int max_por_quadro) {
  // Bind once for standalone cache users too; never create a shared GL worker.
  if (!uploadThread) {
    uploadThread = SDL_ThreadID(); uploadContext = SDL_GL_GetCurrentContext();
  }
  if (SDL_ThreadID() != uploadThread || !uploadContext ||
      SDL_GL_GetCurrentContext() != uploadContext) return 0;
  int subiu = 0, trabalhou = 0, porClasse[3] = {0};
  unsigned char blocked[MAX_ITENS_ABS] = {0};
  // UMA LINHA POR SESSAO com o caminho da piramide e a GPU: e o que separa,
  // num log de usuario, "lixo com mip=gpu" de "lixo com mip=cpu".
  { static int logado;
    if (!logado) {
      const char *r = (const char *)glGetString(GL_RENDERER);
      const char *v = (const char *)glGetString(GL_VERSION);
      logado = 1;
      printf("[tex] mip=%s GL_RENDERER=%s GL_VERSION=%s\n",
             mipCpuAtivo() ? "cpu" : "gpu", r ? r : "?", v ? v : "?");
      fflush(stdout);
    } }
  Uint64 inicio = SDL_GetPerformanceCounter();
  double freq = (double)SDL_GetPerformanceFrequency();
  for (int passo = 0; passo < max_por_quadro + nMax + 16; passo++) {
    if (trabalhou &&
        (double)(SDL_GetPerformanceCounter() - inicio) * 1000.0 / freq >=
            NV_TEX_UPLOAD_BUDGET_MS)
      break;
    if (subiu >= max_por_quadro) break;

    // 1. Ha um envio em faixas em curso: a proxima faixa.
    if (uplIdx >= 0) {
      if (trabalhou) break;  // residencia ocupa o quadro sozinha
      Uint64 passoEm = SDL_GetPerformanceCounter();
      int ok, alvo = uplIdx;
      if (SDL_TryLockMutex(mtx) != 0) break;
      ok = itens[alvo].estado == DECODIFICADO && itens[alvo].sup == uplSup &&
           !strcmp(itens[alvo].caminho, uplPath);
      SDL_UnlockMutex(mtx);
      if (!ok) { uplAbortar(); continue; }
      { SDL_Surface *sup = uplSup;
        int linhas = (int)(NV_TEX_FAIXA_BYTES / ((long)sup->w * 4));
        Uint32 em = SDL_GetTicks();
        if (linhas < 16) linhas = 16;
        // Attribute errors to this upload, not the preceding render frame.
        while (glGetError() != GL_NO_ERROR) { }
        glBindTexture(GL_TEXTURE_2D, uplTex);
        if (uplY < sup->h) {
          if (uplY + linhas > sup->h) linhas = sup->h - uplY;
          glTexSubImage2D(GL_TEXTURE_2D, 0, 0, uplY, sup->w, linhas, GL_RGBA, GL_UNSIGNED_BYTE,
                          (const unsigned char *)sup->pixels + (size_t)uplY * sup->pitch);
          tex_upl_bytes += (long)sup->w * linhas * 4;
          uplY += linhas;
        } else {
          // Niveis da piramide da CPU: um passo proprio depois da ultima
          // faixa, para entrar no mesmo orcamento por quadro (somados, um
          // terco do nivel 0).
          tex_upl_bytes += subirNiveis(sup);
        }
        gfx_tex_esquecer(0);
        if (envioFalhou(alvo, uplTex, sup)) { trabalhou = 1; break; }
        uplGlMs += SDL_GetTicks() - em;
        trabalhou = 1;
        if (uplY < sup->h) { medirEnvio(2, passoEm); break; }
        if (sup->userdata && !uplNiveis) { uplNiveis = 1; medirEnvio(2, passoEm); break; }
        // Ultima faixa: a textura esta inteira, publica.
        { GLuint t = uplTex;
          SDL_LockMutex(mtx);
          itens[alvo].sup = NULL;
          SDL_UnlockMutex(mtx);
          uplTex = 0; uplIdx = -1; uplSup = NULL; uplY = 0; uplNiveis = 0;
          tex_upl_n++;
          subiu += publicarEnvio(alvo, t, sup, uplEm, uplGlMs, uplFila, uplPath); }
      }
      medirEnvio(2, passoEm);
      break;
    }

    // 2. O proximo decodificado: LOGO (2) e TELA CHEIA (1) antes do resto,
    //    empate na ordem dos slots (a de sempre). Um cartaz que chega antes
    //    nao segura o heroi, que e o que a pessoa esta olhando.
    SDL_Surface *sup = NULL; int alvo = -1, nivel = -1, classe = 0;
    Uint32 filaEm = 0, filaWait = 0;
    char uploadPath[512] = "";
    // Defer queued work if a network/decode worker holds the cache lock.
    if (SDL_TryLockMutex(mtx) != 0) break;
    for (int i = 0; i < nMax; i++)
      if (!blocked[i] && itens[i].estado == DECODIFICADO && itens[i].sup && itens[i].urgente > nivel) {
        alvo = i; nivel = itens[i].urgente;
        if (nivel >= 2) break;
      }
    if (alvo >= 0) {
      SDL_Surface *s = itens[alvo].sup;
      classe = (long)s->w * s->h * 4 > 1024L * 1024 ? 2 : nivel == 2 ? 1 : 0;
      double gasto = (SDL_GetPerformanceCounter() - inicio) * 1000.0 / freq;
      // Um passo caro pode exceder o teto SOLO (8 ms), mas nunca trava a fila.
      // Depois do primeiro passo, so admite o proximo se seu pior caso cabe.
      if (porClasse[classe] >= 3 ||
          (trabalhou && (classe == 2 || gasto + uplPiorMs[classe] > NV_TEX_UPLOAD_BUDGET_MS))) {
        SDL_UnlockMutex(mtx);
        break;
      }
      // Pruning cannot free current/last-frame artwork. Reserve room before
      // allocating on the GPU; keep the decoded result queued if only hot
      // textures remain, so drawing never triggers another download/decode.
      long extra = bytesTexturaMip(s->w, s->h,
                                  temPiramide(s->w, s->h, itens[alvo].card));
      // The previous texture remains live until publication; charge both.
      // Publication releases the old texture only after allocating the new one.
      // Charge both on every platform, including promotions.
      long lim = limiteBytes();
      while (bytesUsados + extra > lim)
        if (despejar(0) < 0) break;
      if (bytesUsados + extra > lim) {
        blocked[alvo] = 1;  // Retry next frame; smaller uploads may still fit.
        SDL_UnlockMutex(mtx);
        continue;
      }
      sup = s;
      filaEm = itens[alvo].filaUploadEm; itens[alvo].filaUploadEm = 0;
      snprintf(uploadPath, sizeof uploadPath, "%s", itens[alvo].caminho);
      // Grande: a superficie FICA no item durante o envio em faixas.
      if ((long)sup->w * sup->h * 4 <= NV_TEX_FAIXA_BYTES) itens[alvo].sup = NULL;
    }
    SDL_UnlockMutex(mtx);
    if (!sup) break;

    if (filaEm) {
      filaWait = SDL_GetTicks() - filaEm;
      if (filaWait >= NV_TEX_TRACE_MS)
        printf("[tex-trace] fila-upload hash=%08lx wait=%u\n",
               hashCaminho(uploadPath), (unsigned)filaWait);
    }

    Uint64 passoEm = SDL_GetPerformanceCounter();
    Uint32 uploadEm = SDL_GetTicks();
    while (glGetError() != GL_NO_ERROR) { }
    GLuint t = 0; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    if ((long)sup->w * sup->h * 4 > NV_TEX_FAIXA_BYTES) {
      // So reserva; as faixas vem no passo 1 (deste quadro, se sobrar
      // orcamento, ou dos seguintes).
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sup->w, sup->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
      gfx_tex_esquecer(0);
      if (envioFalhou(alvo, t, sup)) { trabalhou = 1; break; }
      uplIdx = alvo; uplTex = t; uplSup = sup; uplY = 0; uplNiveis = 0;
      uplEm = uploadEm; uplGlMs = SDL_GetTicks() - uploadEm; uplFila = filaWait;
      snprintf(uplPath, sizeof uplPath, "%s", uploadPath);
      medirEnvio(2, passoEm);
      if ((SDL_GetPerformanceCounter() - inicio) * 1000.0 / freq >= 8.0) break;
      continue;
    }
    tex_upl_n++;
    tex_upl_bytes += (long)sup->w * sup->h * 4;
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, sup->w, sup->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, sup->pixels);
    if (sup->userdata) tex_upl_bytes += subirNiveis(sup);
    trabalhou = 1;
    subiu += publicarEnvio(alvo, t, sup, uploadEm, SDL_GetTicks() - uploadEm, filaWait, uploadPath);
    medirEnvio(classe, passoEm);
    porClasse[classe]++;
    if (classe == 2) break;
  }
  return subiu;
}

void tex_estatisticas(int *nItens, int *nPend, long *bytes,
                      int *nQuentes, long *bytesQuentes) {
  int a=0, p=0, q=0; long b=0, bq=0;
  SDL_LockMutex(mtx);
  for (int i = 0; i < nMax; i++) {
    if (itens[i].tex) {
      long t = bytesTextura(&itens[i]);
      a++; b += t;
      if (quente(&itens[i])) { q++; bq += t; }
    }
    if (itens[i].estado == PENDENTE || itens[i].estado == DECODIFICADO) p++;
  }
  SDL_UnlockMutex(mtx);
  if (nItens) *nItens = a;
  if (nPend) *nPend = p;
  if (bytes) *bytes = b;
  if (nQuentes) *nQuentes = q;
  if (bytesQuentes) *bytesQuentes = bq;
}

long tex_cache_disco_bytes(void) {
#ifndef __EMSCRIPTEN__
  return __atomic_load_n(&cacheDiscoSnapshot, __ATOMIC_ACQUIRE);
#else
  return cacheDiscoBytes;
#endif
}
long tex_orcamento_bytes(void) { return orcamento; }
