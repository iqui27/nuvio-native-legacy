#include "video.h"
#include "idioma.h"
#include "linguas.h"
#include <SDL2/SDL.h>
#include "marco.h"
#include "mkv.h"
#include "js.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

// Nomes que o uMS aceita em charColor, e os rotulos que a folha mostra.
//
// FORA do #if do aparelho: a folha de faixas desenha os rotulos tambem no Mac,
// onde o resto do modulo e stub. Deixa-los no lado da TV quebrava a ligacao da
// build de desenvolvimento — que e onde a interface e conferida.
const char *const VIDEO_LEG_CORES[VIDEO_LEG_NCORES] = {
  "white", "yellow", "green", "blue", "red", "black"
};
const char *const VIDEO_LEG_CORES_PT[VIDEO_LEG_NCORES] = {
  "Branco", "Amarelo", "Verde", "Azul", "Vermelho", "Preto"
};

static int extensaoLegenda(const char *ini, const char *fim) {
  static const char *const ext[] = { ".srt", ".vtt", ".smi", ".ass", ".ssa", ".sub" };
  size_t i;
  for (i = 0; i < sizeof ext / sizeof *ext; i++) {
    size_t n = strlen(ext[i]);
    const char *p;
    if ((size_t)(fim - ini) < n) continue;
    p = fim - n;
    { size_t k; for (k = 0; k < n; k++)
        if (tolower((unsigned char)p[k]) != ext[i][k]) break;
      if (k == n) return 1; }
  }
  return 0;
}

void video_normalizar_url_legenda(const char *url, char *dst, unsigned tam) {
  const char *q;
  size_t antes, sufixo, cabe;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!url) return;
  q = strchr(url, '?');
  if (!q) q = url + strlen(url);
  if (extensaoLegenda(url, q)) { snprintf(dst, tam, "%s", url); return; }
  antes = (size_t)(q - url); sufixo = strlen(q);
  cabe = antes + 4 + sufixo;
  if (cabe + 1 > tam) { snprintf(dst, tam, "%s", url); return; }
  memcpy(dst, url, antes);
  memcpy(dst + antes, ".srt", 4);
  memcpy(dst + antes + 4, q, sufixo + 1);
}

// ============================================================================
// DAQUI PARA BAIXO, NADA COMPILA NO ALVO TIZEN (Emscripten).
//
// O corpo deste arquivo e LS2 + dlopen + libAcbAPI + libglib, e o navegador da
// TV Samsung nao tem nenhuma dessas coisas. O problema e que TUDO ISSO COMPILA
// sob o emcc — o dlfcn.h do Emscripten traz cotos de dlopen/dlsym que devolvem
// NULL sem erro —, entao o alvo Tizen caia neste ramo por engano, linkava, e
// ficava sem video sem uma unica linha de log dizendo por que. O corpo do alvo
// Tizen mora em src/video_tizen.c, sobre a API AVPlay; e o mesmo padrao que
// src/rede.c ja usa (la a libcurl por dlopen virou XHR).
//
// A GUARDA COMECA AQUI, e nao no topo do arquivo, de proposito: VIDEO_LEG_CORES
// e video_normalizar_url_legenda, logo acima, sao codigo puro que os TRES alvos
// usam — a folha de faixas desenha os rotulos das cores tambem no Mac. Empurrar
// a guarda para o topo obrigaria a duplica-los no video_tizen.c, e duas copias
// de uma tabela e uma copia para divergir da outra.
// ============================================================================
#ifndef __EMSCRIPTEN__

// Declarada aqui porque o loadCompleted a chama muito antes de ela ser
// definida. O clang do Mac aceita a implicita; o gcc do ARM recusa — e o ARM
// que esta certo.
static void aplicarEstilo(void);

// Declarada aqui porque o loadCompleted a chama muito antes de ela ser
// definida. O clang do Mac aceita a implicita e o gcc do ARM recusa — e o ARM
// que esta certo.
static void aplicarEstilo(void);


// Definidos adiante (junto de urlAtual, que e o que o fio consome); declarados
// aqui porque o parse do sourceInfo, bem acima, e quem dispara o fio.
static char  urlAtual[1024];   // URL da reproducao corrente
// Recuperacao de pipeline destruido: pedida pelo fio de resposta do luna e
// executada no fio principal (video_bombear), porque recarregar de dentro do
// tratador de evento reentra no mesmo caminho que acabou de falhar.
static int    recuperando;
static double retomarEm;
// Posicao a aplicar assim que o load terminar. Seek antes do loadCompleted e
// mandado para um pipeline que ainda nao existe e some sem erro.
static double posAoCarregar;
// FAIXAS a restaurar depois de uma queda de pipeline. Sem isto o video voltava
// com OUTRO audio — o pipeline novo comeca sempre na faixa 0, e o dono, que
// tinha escolhido a dele, via a escolha ser desfeita sozinha. `-1` = nao ha o
// que restaurar.
static int   audioAoCarregar = -1, legAoCarregar = -1;
// Definida bem abaixo; usada na leitura do sourceInfo para aplicar o idioma
// de audio preferido assim que as faixas aparecem.
void video_escolher_audio(int i);
static char  legUrlAoCarregar[1024];
// URL da legenda EXTERNA em uso. O legAtual nao a representa: quem escolhe uma
// legenda do OpenSubtitles nao mexe em faixa nenhuma do arquivo, so aponta o
// setSubtitleSource. Sem guardar a URL, a recuperacao trazia de volta a legenda
// embutida de antes, ou nenhuma.
static char  legUrlAtual[1024];
// Avanco pendente: alvo e quando manda-lo. Ver SEEK_REPOUSO_MS.
static int    pausaPedida;   // 1 enquanto a pausa foi pedida por nos
// Sonda de MKV pedida, esperando o buffer. Ver a nota no sourceInfo.
static int    mkvPendente;
// 1 quando a fonte foi anunciada como MP4. Ver video_definir_mp4.
static int    fonteMp4;
static double seekAlvo;
static Uint32 seekEm;
// Declarada aqui porque video_bombear a chama antes da definicao. O clang do
// Mac aceita a implicita; o gcc do ARM recusa — e o ARM que esta certo. Terceira
// vez neste arquivo.
static void seekAgora(double segundos);
static void *lerMkv(void *arg);
static pthread_t fioMkv;
static int       fioMkvVivo;
// Identidade monotonica do pipeline. Callbacks do LS2 podem sobreviver ao
// unload; sem uma geracao, a resposta antiga pode ocupar o estado da proxima
// abertura e fazer o load correto ser ignorado.
static unsigned  sessao;

#ifdef __APPLE__
// No Mac nao existe barramento nem plano de video. Os cotos deixam o resto do
// app compilar e rodar igual, so sem imagem em movimento.
int  video_iniciar(void) { return 0; }
int  video_tocar(const char *u) { (void)u; return 0; }
void video_bombear(void) {}
void video_parar(void) {}
void video_pausar(int p) { (void)p; }
void video_buscar(double s) { (void)s; }
void video_janela(int x,int y,int w,int h) { (void)x;(void)y;(void)w;(void)h; }
// Coto que FALTAVA: a funcao existia so no ramo do aparelho, entao o build do
// Mac quebrava no link com "_video_janela_fonte, referenced from
// _aplicarAspecto". E o espelho da armadilha ja conhecida — o Mac nao compila a
// metade do pipeline, e por isso nao valida `video.c`; aqui ele cobra a
// declaracao que a outra metade nao tem. Toda funcao nova de video precisa
// aparecer NOS DOIS ramos.
void video_janela_fonte(int sx,int sy,int sw,int sh,int dx,int dy,int dw,int dh) {
  (void)sx;(void)sy;(void)sw;(void)sh;(void)dx;(void)dy;(void)dw;(void)dh;
}
double video_pos(void) { return 0; }
double video_duracao(void) { return 0; }
// Sem pipeline nao ha arquivo para ler capitulos: no Mac o pos-reproducao cai
// no plano B dos ultimos minutos, que e o mesmo caminho de um MKV sem
// capitulos. Melhor um stub honesto que um numero inventado.
double video_creditos(void) { return 0.0; }
double video_buffer_fim(void) { return 0; }
void video_definir_dv(int dv) { (void)dv; }
int  video_tocando(void) { return 0; }
int  video_pronto(void) { return 0; }
int  video_ativo(void) { return 0; }
int  video_n_audio(void) { return 0; }
int  video_n_legenda(void) { return 0; }
const VideoFaixa *video_audio(int i) { (void)i; return 0; }
const VideoFaixa *video_legenda(int i) { (void)i; return 0; }
int  video_audio_atual(void) { return 0; }
int  video_legenda_atual(void) { return -1; }
void video_escolher_audio(int i) { (void)i; }
void video_escolher_legenda(int i) { (void)i; }
void video_legenda_externa(const char *u) { (void)u; }
void video_legenda_estilo(const VideoLegendaEstilo *e) { (void)e; }
void video_definir_mp4(int m) { (void)m; }
int  video_tem_atmos(void) { return 0; }
int  video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return "none"; }
int  video_largura(void) { return 0; }
int  video_altura(void) { return 0; }
int  video_pode_forcar_sdr(void) { return 0; }
void video_forcar_sdr(void) {}
void video_encerrar(void) {}
#else
#include <dlfcn.h>

typedef struct LSHandle LSHandle;
typedef struct LSMessage LSMessage;
typedef int (*Filtro)(LSHandle *, LSMessage *, void *);

// LSError e struct por valor e nao ha header C no SDK. Um buffer folgado evita
// corromper a pilha quando a lib escreve o erro dentro dele.
static char ERRO[256];

static int         (*lsRegister)(const char *, LSHandle **, void *);
static int         (*lsAttach)(LSHandle *, void *, void *);
static int         (*lsCall)(LSHandle *, const char *, const char *, Filtro, void *, unsigned long *, void *);
static const char *(*lsPayload)(LSMessage *);
static void *(*loopNovo)(void *, int);
static void  (*loopRodar)(void *);
static void  (*loopParar)(void *);

// libAcbAPI e SEMPRE por dlopen. Linkar cria um DT_NEEDED e, se a lib faltar
// (ela sumiu no webOS 5), o processo morre antes do main e antes do log —
// nao sobra nem uma linha para diagnosticar.
static long (*acbCriar)(void);
static int  (*acbIniciar)(long, int, const char *, void *);
static int  (*acbSink)(long, int);
static int  (*acbMidia)(long, const char *);
static int  (*acbEstado)(long, int, int, long *);
static int  (*acbJanela)(long, long, long, long, long, int, long *);
// Janela CUSTOMIZADA: recorte de fonte + retangulo de destino. E o caminho com
// permissao. Chamar luna://com.webos.service.tv.display/setCustomDisplayWindow
// direto e RECUSADO pelo hub — "Not permitted to send to
// com.webos.service.tv.display" —, porque o app se registra como
// com.webos.media.client.nuvio e esse papel nao alcanca o servico de display.
// A libAcbAPI alcanca: ela expoe AcbAPI_setCustomDisplayWindow e fala com o
// tv.display por dentro, que e como o proprio navegador da TV faz.
static int  (*acbJanelaCustom)(long, long, long, long, long,
                               long, long, long, long, int, long *);
static void (*acbDestruir)(long);
// O navegador da TV chama isto e nos nao chamavamos: sem o connect o plano de
// video existe, decodifica e toca o audio, mas nao e ligado a saida — tela
// preta com som, exatamente o sintoma observado.
static int  (*acbConectar)(long, int, long *);
// Recebe JSON como string (confirmado: a lib chama strlen no argumento antes de
// montar um std::string). Sem esta chamada o servico do ACB nunca repassa nada
// para com.webos.service.tv.display e o plano de video nao liga — o sintoma e
// audio normal com tela preta.
static int  (*acbVideoData)(long, const char *, long *);
static int  (*acbAudioData)(long, const char *, long *);

// ---------------------------------------------------------------- webOS 5+
//
// A LG apagou a libAcbAPI na webOS 5.0. Sem ela o dlopen falhava, video_iniciar
// devolvia 0 e NENHUMA fonte abria — o relato "streams are not playing" de quem
// testou num OLED 2020 (CX, webOS 5). O caminho de la e outro: a SDL da propria
// TV exporta uma janela para o plano de video e o uMS recebe o ID dela em
// `windowId`, no lugar do "window_id_dummy" que o caminho do ACB usa. E o mesmo
// que o Kodi e o moonlight fazem na 5+.
//
// Assinaturas TIRADAS DO HEADER DO SDK (SDL2/SDL_webOS.h do buildroot da
// openlgtv), nao de memoria:
//   const char *SDL_webOSCreateExportedWindow(int type);
//   SDL_bool    SDL_webOSSetExportedWindow(const char *id, SDL_Rect *src, SDL_Rect *dst);
//   SDL_bool    SDL_webOSExportedSetCropRegion(const char *id, SDL_Rect *org,
//                                              SDL_Rect *src, SDL_Rect *dst);
//   void        SDL_webOSDestroyExportedWindow(const char *id);
// type 0 = SDL_WEBOS_EXPORED_WINDOW_TYPE_VIDEO (a grafia truncada e do header).
//
// Por dlsym e nao por chamada direta: o binario e o MESMO nos dois mundos, e na
// webOS 4 esses simbolos nao existem. Referencia direta viraria dependencia de
// link e o app nao subiria mais na C9, trocando um aparelho quebrado por outro.
static const char *(*sdlExpCriar)(int);
static int         (*sdlExpJanela)(const char *, SDL_Rect *, SDL_Rect *);
static int         (*sdlExpRecorte)(const char *, SDL_Rect *, SDL_Rect *, SDL_Rect *);
static void        (*sdlExpDestruir)(const char *);
// ID devolvido pela SDL. Vazio = estamos no caminho do ACB (webOS 4).
static char        expWin[64];

static LSHandle *bus;
static void     *laco;
static pthread_t fio;
static long      acb;
// Retangulo pedido pela UI. Guardado porque o ACB so aceita a janela depois do
// loadCompleted, que chega muito depois de quem pediu.
static int       janX, janY, janW = 1920, janH = 1080;
// Ultimo par fonte/destino aplicado pelo setDisplayWindow do uMS, para nao
// repetir a mesma chamada a cada quadro. fonX = -1 quer dizer "nada aplicado".
static int       fonX = -1, fonY, fonW, fonH, dstX = -1, dstY, dstW, dstH;
// Caracteristicas do fluxo, tiradas do evento videoInfo da assinatura do uMS.
// O ACB precisa delas para descrever o video ao pipeline de exibicao.
static int       vidW = 1920, vidH = 1080, vidTaxa = 30;
static long      vidBits;
static char      vidVarredura[24] = "progressive";
// hdrType real informado pelo uMS para a camada que chegou ao decoder. Isto
// vence o rotulo do addon: um arquivo marcado HDR-DV pode entregar apenas a
// camada HDR10 nesta TV/perfil.
static char      vidHdr[24] = "none";
static long      seiX0, seiX1, seiX2, seiY0, seiY1, seiY2;
static long      seiBrancoX, seiBrancoY, seiMinLum, seiMaxLum;
static long      seiMaxCLL, seiMaxFALL;
static int       vuiPrim = 2, vuiTrans = 2, vuiMatriz = 2;
static int       vidAtmos, vidDV;
// Estado do recuo de Dolby Vision (ver o bloco em video_tocar). Declarados
// AQUI e nao junto da funcao porque o parser do videoInfo, bem acima, marca
// viuVideo — e no C a ordem de declaracao manda.
static int       dvNaCarga, dvRecuado, viuVideo;
// A pessoa pediu SEM HDR nesta sessao (ver video_forcar_sdr). Fica ligado ate a
// proxima fonte: se ela pediu porque a tela estava preta, uma recuperacao
// automatica nao pode devolver o Dolby Vision e a tela preta junto.
static int       semDVForcado;

// Faixas lidas do sourceInfo. Guardadas porque a tela precisa delas a cada
// quadro e reprocessar o JSON no desenho seria desperdicio.
static VideoFaixa faixaAudio[NV_FAIXA_MAX], faixaLeg[NV_FAIXA_MAX];
static int nAudio, nLeg, audioAtual, legAtual = -1;

// Afirmacao de DV da fonte escolhida. Setada por video_definir_dv ANTES do
// tocar, porque o video_tocar zera vidDV ao comecar uma sessao nova.
static int dvPedido;

// O nome legivel do idioma mora em linguas.c: addons.c precisava da mesma
// tabela e mantinha uma propria, com tres idiomas.

static char      midia[64];
static double    posSeg, durSeg;
static int       tocando, pronto, ligado;

// PLAYER_TYPE_MSE. O ACB usa isto para saber que a fonte e um pipeline de
// midia e nao um sintonizador.
// Os enums do ACB nao tem header publico e chutar sai caro: com playerType 10 e
// sink 1 o aparelho registrou "playerType":"mse","vsmSinkType":"sub" — ou seja,
// o video foi para o plano SECUNDARIO (PIP) e a tela ficou preta. Os numeros
// ficam ajustaveis por /tmp/nuvio-acb justamente para conferir contra o que o
// ls-monitor mostra que o ACB resolveu, em vez de adivinhar de novo.
static int tipoJogador = 0, tipoSink = 0, estCarregado = 1, estTocando = 2;
// hdrType do setMediaVideoData. O padrao e "none"; para testar Dolby Vision,
// escreva na SEGUNDA linha de /tmp/nuvio-acb: "dolby_vision" ou "hdr10".
// Afirmar DV sem o pipeline pedir e mentira — por isso NAO existe deteccao
// automatica: o sourceInfo do uMS nao distingue HEVC main-10 HDR10 de DV.
static char hdrTipo[24] = "none";

static void lerAjustesAcb(void) {
  FILE *f = fopen("/tmp/nuvio-acb", "r");
  if (!f) return;
  if (fscanf(f, "%d %d %d %d", &tipoJogador, &tipoSink, &estCarregado, &estTocando) > 0)
    printf("[video] acb ajustes: jogador=%d sink=%d carregado=%d tocando=%d\n",
           tipoJogador, tipoSink, estCarregado, estTocando);
  { // resto da primeira linha descartado; a SEGUNDA linha, se existir, e o hdrType.
    char linha[64];
    if (fgets(linha, sizeof linha, f) && fgets(linha, sizeof linha, f)) {
      char t[24] = "";
      if (sscanf(linha, "%23s", t) == 1 && t[0]) {
        snprintf(hdrTipo, sizeof hdrTipo, "%s", t);
        if (strstr(hdrTipo, "dolby")) vidDV = 1;
        printf("[video] acb ajustes: hdrType=%s\n", hdrTipo);
      }
    }
  }
  fclose(f);
}

#define NV_ACB_FOREGROUND 1

// Procura a chave e exige que o que vem depois seja NUMERO.
//
// O evento e {"currentTime":{"currentTime":8580,...}}: a primeira ocorrencia da
// chave e o objeto externo, e atof("{...") devolve 0. A barra ficava parada em
// 0:00 com a duracao correta ao lado — o tipo de erro que parece "o player nao
// atualiza" e na verdade e leitura do campo errado.
static double numeroDe(const char *p, const char *chave) {
  const char *q = p;
  size_t n = strlen(chave);
  while ((q = strstr(q, chave)) != NULL) {
    const char *v = q + n;
    while (*v == ' ') v++;
    if ((*v >= '0' && *v <= '9') || *v == '-' || *v == '.') return atof(v);
    q += n;
  }
  return -1.0;
}

static pthread_t fioBind;
static volatile int bindVivo = 0;   // existe um bind em andamento?
// Se o load novo termina durante o bind lento da sessao anterior, guarda o
// trabalho. video_bombear inicia o bind assim que o fio anterior liberar.
static volatile int bindPendente;

// Latencia do pipeline: pedido de load -> loadCompleted -> primeiro quadro.
// Sao os numeros que dizem se o comeco e o buffer estao saudaveis; sem eles
// "ta lento" e impressao.
static struct timespec t0Pedido;
static int cronPediu, cronLoad, cronQuadro;
static long msDesdePedido(void) {
  struct timespec a;
  clock_gettime(CLOCK_MONOTONIC, &a);
  return (a.tv_sec - t0Pedido.tv_sec) * 1000L + (a.tv_nsec - t0Pedido.tv_nsec) / 1000000L;
}
// Ate onde o buffer do pipeline ja cobre (segundos), do evento bufferRange.
static double bufferSeg;

static void esperar(int ms) { struct timespec t; t.tv_sec = ms / 1000;
  t.tv_nsec = (long)(ms % 1000) * 1000000L; nanosleep(&t, NULL); }

// O JSON de video do ACB, montado por partes porque os valores de HDR mudam
// com o que se esta afirmando. VUI segue H.273/HEVC: 9=BT.2020, 16=PQ
// (SMPTE 2084) — e o par que HDR10 e DV pedem; SDR fica em 2 (unspecified),
// que e o que sempre foi mandado e toca.
// Versao maior do webOS desta TV. Importa porque o pipeline RENOMEOU campos na
// 5.0 e um campo com nome errado e ignorado em silencio — nada falha, o HDR so
// nao liga. Conferido no Kodi (MediaPipelineWebOS.cpp, SetHDR):
//   hdrData[m_webOSVersion < 5 ? "mediaSei" : "sei"] = sei;
//   hdrData[m_webOSVersion < 5 ? "mediaVui" : "vui"] = vui;
// /etc/starfish-release da a linha "Rockhopper release 4.10.2-31 (...)" — o
// numero depois de "release" e o que vale. Sem o arquivo, a ausencia da
// libAcbAPI ja e prova de 5+, porque foi nela que a LG apagou a lib.
static int webosMaior(void) {
  static int v;
  if (v) return v;
  { FILE *f = fopen("/etc/starfish-release", "r");
    if (f) {
      char linha[256];
      while (fgets(linha, sizeof linha, f)) {
        const char *r = strstr(linha, "release ");
        if (r && sscanf(r + 8, "%d", &v) == 1 && v > 0) break;
        v = 0;
      }
      fclose(f);
    } }
  if (!v) v = expWin[0] ? 5 : 4;
  return v;
}

static void montarVideoData(char *vd, size_t n, const char *ctx,
                            const char *htipo, int prim, int trans, int matriz) {
  const char *varr = strstr(vidVarredura, "inter") ? "VIDEO_INTERLACED"
                   : "VIDEO_PROGRESSIVE";
  const char *kSei = webosMaior() < 5 ? "mediaSei" : "sei";
  const char *kVui = webosMaior() < 5 ? "mediaVui" : "vui";
  char cor[768] = "";
  if (!strcmp(htipo, "HDR10")) {
    snprintf(cor, sizeof cor,
      "\"%s\":{\"displayPrimariesX0\":%ld,\"displayPrimariesX1\":%ld,"
      "\"displayPrimariesX2\":%ld,\"displayPrimariesY0\":%ld,"
      "\"displayPrimariesY1\":%ld,\"displayPrimariesY2\":%ld,"
      "\"maxContentLightLevel\":%ld,\"maxDisplayMasteringLuminance\":%ld,"
      "\"maxPicAverageLightLevel\":%ld,\"minDisplayMasteringLuminance\":%ld,"
      "\"whitePointX\":%ld,\"whitePointY\":%ld},"
      "\"%s\":{\"colorPrimaries\":%d,\"matrixCoeffs\":%d,"
      "\"transferCharacteristics\":%d,\"videoFullRangeFlag\":false},",
      kSei, seiX0, seiX1, seiX2, seiY0, seiY1, seiY2, seiMaxCLL, seiMaxLum,
      seiMaxFALL, seiMinLum, seiBrancoX, seiBrancoY,
      kVui, prim, matriz, trans);
  } else if (!strcmp(htipo, "none")) {
    snprintf(cor, sizeof cor,
      "\"%s\":{\"displayPrimariesX0\":0,\"displayPrimariesX1\":0,"
      "\"displayPrimariesX2\":0,\"displayPrimariesY0\":0,"
      "\"displayPrimariesY1\":0,\"displayPrimariesY2\":0,"
      "\"maxContentLightLevel\":0,\"maxDisplayMasteringLuminance\":0,"
      "\"maxPicAverageLightLevel\":0,\"minDisplayMasteringLuminance\":0,"
      "\"whitePointX\":0,\"whitePointY\":0},"
      "\"%s\":{\"colorPrimaries\":2,\"matrixCoeffs\":2,"
      "\"transferCharacteristics\":2,\"videoFullRangeFlag\":false},",
      kSei, kVui);
  }
  snprintf(vd, n,
    //  - "context" com o mediaId TEM de vir no proprio JSON: o servico do
    //    ACB repassa o payload como veio, nao insere o campo. Sem ele o
    //    tv.display responde ERROR_06 "Invalid argument" com
    //    "context": "" — e o erro nao diz qual argumento e.
    "{\"content\":\"movie\",\"context\":\"%s\",\"video\":{"
    "\"adaptive\":false,\"bitRate\":%ld,"
    "\"data3D\":{\"currentPattern\":\"2d\",\"originalPattern\":\"2d\","
            "\"typeLR\":\"LR\"},"
    "\"frameRate\":%d.0,\"hdrType\":\"%s\","
    "\"height\":%d,\"width\":%d,"
    "%s"
    "\"hfr\":false,"
    "\"path\":\"network\","
    "\"pixelAspectRatio\":{\"height\":1,\"width\":1},"
    "\"rotation\":\"0\",\"scanType\":\"%s\",\"specificRendering\":\"none\""
    "}}", ctx, vidBits, vidTaxa, htipo, vidH, vidW, cor, varr);
}

static void *prenderPlano(void *u) {
  (void)u;
  // O bind e POR SESSAO: cada loadCompleted tem de religar o plano. Foi o bug
  // da "segunda reproducao preta com som" — o ACB continuava apontando para o
  // mediaId da sessao anterior, que o unload matou. A guarda de midia cobre a
  // troca de titulo no MEIO do bind (unload+load em menos de ~1,5s de pausas):
  // continuar descreveria ao tv.display um mediaId que ja morreu.
  char minha[64];
  snprintf(minha, sizeof minha, "%s", midia);
  long tarefa = 0;
  acbMidia(acb, minha);            esperar(200);
  if (strcmp(midia, minha)) goto fora;
  acbEstado(acb, NV_ACB_FOREGROUND, estCarregado, &tarefa); esperar(200);
  if (strcmp(midia, minha)) goto fora;
  printf("[video] connect=%d\n", acbConectar(acb, tipoSink, &tarefa)); esperar(200);
  if (strcmp(midia, minha)) goto fora;
  {
    // Strings e formato copiados de controles positivos na MESMA TV:
    // Apple TV e Nuvio web usam "DolbyVision" sem SEI/VUI; HDR10 usa o SEI/VUI
    // real do videoInfo. A grafia/capitalizacao e semanticamente relevante.
    // Enquanto isso nao existia, "none" ia sempre: o C9 exibia o video
    // mapeado em SDR e o modo HDR/DV da TV nunca ligava — exatamente o
    // sintoma do teste 4K.
    char htipo[24];
    int prim = 2, trans = 2, matriz = 2;
    if (strcmp(hdrTipo, "none")) { snprintf(htipo, sizeof htipo, "%s", hdrTipo);
                                  prim = vuiPrim; trans = vuiTrans; matriz = vuiMatriz; }
    else if (!strcasecmp(vidHdr, "DolbyVision") || vidDV) {
                                  snprintf(htipo, sizeof htipo, "DolbyVision"); }
    else if (!strcasecmp(vidHdr, "HDR10")) {
                                  snprintf(htipo, sizeof htipo, "HDR10");
                                  prim = vuiPrim; trans = vuiTrans; matriz = vuiMatriz; }
    else                           snprintf(htipo, sizeof htipo, "none");
    char vd[2048];
    montarVideoData(vd, sizeof vd, minha, htipo, prim, trans, matriz);
    { char ad[160];
      snprintf(ad, sizeof ad,
               "{\"context\":\"%s\",\"audio\":{\"immersive\":\"none\"}}", minha);
      int rvd = acbVideoData(acb, vd, &tarefa);
      printf("[video] videoData=%d (hdrType=%s)\n", rvd, htipo);
      if (!strcmp(htipo, "DolbyVision") && !strcasecmp(vidHdr, "HDR10")) {
        // O ACB aceita DolbyVision e a TV acende o badge mesmo quando o
        // demuxer do MKV so entregou a camada HDR10 — nesse caso o plano fica
        // sem imagem. O retorno sincrono nao detecta isso. Depois de negociar
        // DV, voltar para o formato REAL do decoder recupera imagem + HDR10.
        esperar(700);
        montarVideoData(vd, sizeof vd, minha, "HDR10", vuiPrim, vuiTrans, vuiMatriz);
        printf("[video] MKV DV entregue como HDR10; fallback real: %d\n",
               acbVideoData(acb, vd, &tarefa));
      } else if (!strcmp(htipo, "DolbyVision") && !strcasecmp(vidHdr, "none")) {
        // Profile DV que o demuxer desta TV nao reconheceu nem como camada
        // HDR10. O badge liga, mas nao ha quadro DV; voltar a SDR garante
        // imagem. MP4 reconhecido vem como DolbyVision e nao entra aqui.
        esperar(700);
        montarVideoData(vd, sizeof vd, minha, "none", 2, 2, 2);
        printf("[video] DV nao reconhecido pelo decoder; fallback SDR: %d\n",
               acbVideoData(acb, vd, &tarefa));
      } else if (rvd != 1 && strcmp(htipo, "none")) {
        montarVideoData(vd, sizeof vd, minha, "none", 2, 2, 2);
        printf("[video] videoData recusou hdrType=%s, repetindo sem HDR: %d\n",
               htipo, acbVideoData(acb, vd, &tarefa));
      }
      printf("[video] audioData=%d\n", acbAudioData(acb, ad, &tarefa));
      fflush(stdout);
    }
    esperar(300);
    if (strcmp(midia, minha)) goto fora;
  }
  acbJanela(acb, janX, janY, janW, janH,
          (janX == 0 && janY == 0 && janW == 1920 && janH == 1080), &tarefa);
  acbEstado(acb, NV_ACB_FOREGROUND, estTocando, &tarefa);
  printf("[video] plano preso em %d,%d %dx%d\n", janX, janY, janW, janH);
  fflush(stdout);
fora:
  if (strcmp(midia, minha)) printf("[video] bind abortado: a midia trocou no meio\n");
  bindVivo = 0;
  return NULL;
}

static int aoEvento(LSHandle *h, LSMessage *m, void *u) {
  const char *p = lsPayload(m);
  unsigned minhaSessao = (unsigned)(uintptr_t)u;
  (void)h;
  if (minhaSessao != sessao) return 1;
  if (!p) return 1;
  printf("[video] ev %s\n", p); fflush(stdout);
  if (strstr(p, "sourceInfo")) {
    const char *q;
    nAudio = nLeg = 0;
    vidAtmos = 0;
    // Percorre audioTrackInfo item a item. O sourceInfo e um objeto so, entao
    // andar pelos "{" depois da chave do vetor e o suficiente aqui.
    q = strstr(p, "\"audioTrackInfo\"");
    if (q) {
      const char *fimVet = strchr(q, ']');
      const char *o = strchr(q, '{');
      while (o && nAudio < NV_FAIXA_MAX && (!fimVet || o < fimVet)) {
        const char *fo = strchr(o, '}');
        VideoFaixa *f = &faixaAudio[nAudio];
        char cod[16] = "", ch[8] = "", imm[16] = "";
        memset(f, 0, sizeof *f);
        f->numero = nAudio;
        { const char *l = strstr(o, "\"language\":\"");
          if (l && (!fo || l < fo)) {
            size_t k = 0; l += 12;
            while (*l && *l != '"' && k + 1 < sizeof f->idioma) f->idioma[k++] = *l++;
            f->idioma[k] = 0;
            if (!strcmp(f->idioma, "(null)")) f->idioma[0] = 0;
          } }
        { const char *c2 = strstr(o, "\"codec\":\"");
          if (c2 && (!fo || c2 < fo)) {
            size_t k = 0; c2 += 9;
            while (*c2 && *c2 != '"' && k + 1 < sizeof cod) cod[k++] = *c2++;
            cod[k] = 0;
          } }
        { const char *m = strstr(o, "\"immersive\":\"");
          if (m && (!fo || m < fo)) {
            size_t k = 0; m += 13;
            while (*m && *m != '"' && k + 1 < sizeof imm) imm[k++] = *m++;
            imm[k] = 0;
            if (!strcasecmp(imm, "ATMOS")) vidAtmos = 1;
          } }
        { double c3 = numeroDe(o, "\"channels\":");
          if (c3 == 6) snprintf(ch, sizeof ch, "5.1");
          else if (c3 == 8) snprintf(ch, sizeof ch, "7.1");
          else if (c3 == 2) snprintf(ch, sizeof ch, "2.0"); }
        // TRADUZ A PARTE ANTES DE MONTAR: "Português  ·  5.1" nunca casa com
        // chave nenhuma da tabela (varredura-i18n.py, porta 2), entao o
        // rotulo ficava em portugues mesmo com o app em ingles sempre que
        // havia canal ou Atmos ao lado do idioma. i18n() aqui, snprintf
        // depois — como as telas ja consertadas fazem.
        snprintf(f->rotulo, sizeof f->rotulo, "%s%s%s%s%s",
                 f->idioma[0] ? i18n(ling_nome(f->idioma)) : i18n("Faixa"),
                 imm[0] ? "  \xc2\xb7  " : (ch[0] ? "  \xc2\xb7  " : ""),
                 imm[0] ? "Atmos" : "",
                 (imm[0] && ch[0]) ? " " : "", ch);
        nAudio++;
        o = fo ? strchr(fo, '{') : NULL;
      }
    }
    // DIAGNOSTICO: despeja o sourceInfo CRU uma vez por titulo. A TV nao
    // devolve idioma de legenda nos arquivos do dono (todas saem como
    // "Legenda N"), e sem ver o JSON de verdade qualquer conserto e chute —
    // pode ser outro nome de campo, pode ser que o pipeline nao etiquete mesmo.
    // Ler com: sshpass ... scp root@TV:/tmp/nuvio-faixas.json .
    { static int despejou;
      if (!despejou) {
        FILE *fd = fopen("/tmp/nuvio-faixas.json", "w");
        if (fd) { fputs(p, fd); fclose(fd); despejou = 1; }
      } }

    q = strstr(p, "\"subtitleTrackInfo\"");
    if (q) {
      const char *fimVet = strchr(q, ']');
      const char *o = strchr(q, '{');
      while (o && nLeg < NV_FAIXA_MAX && (!fimVet || o < fimVet)) {
        const char *fo = strchr(o, '}');
        VideoFaixa *f = &faixaLeg[nLeg];
        memset(f, 0, sizeof *f);
        f->numero = (int)numeroDe(o, "\"trackNum\":");
        { const char *l = strstr(o, "\"language\":\"");
          if (l && (!fo || l < fo)) {
            size_t k = 0; l += 12;
            while (*l && *l != '"' && k + 1 < sizeof f->idioma) f->idioma[k++] = *l++;
            f->idioma[k] = 0;
            if (!strcmp(f->idioma, "(null)")) f->idioma[0] = 0;
          } }
        // Arquivo sem etiqueta de idioma e o caso comum em MKV de release.
        // Numerar e honesto; inventar "Ingles" seria pior.
        if (f->idioma[0])
          snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(f->idioma)));
        else
          snprintf(f->rotulo, sizeof f->rotulo, i18n("Legenda %d"), f->numero + 1);
        nLeg++;
        o = fo ? strchr(fo, '{') : NULL;
      }
    }
    printf("[video] faixas: audio=%d legenda=%d atmos=%d\n", nAudio, nLeg, vidAtmos);
    fflush(stdout);

    // Escolhe o audio no idioma preferido, se houver um e se o arquivo o
    // tiver. Sem preferencia, ou sem faixa correspondente, NAO se mexe: a
    // escolha do pipeline (faixa 0) e melhor que uma trocada por chute — quem
    // quer outra abre a folha de faixas, que continua listando todas.
    { const char *pref = ling_audio();
      if (pref[0] && nAudio > 1) {
        int i;
        for (i = 0; i < nAudio; i++) {
          if (!faixaAudio[i].idioma[0] || !ling_casa(faixaAudio[i].idioma, pref)) continue;
          printf("[video] audio preferido: %s (faixa %d de %d)\n",
                 ling_nome(faixaAudio[i].idioma), i + 1, nAudio);
          fflush(stdout);
          // Direto, e nao por audioAoCarregar: aquele campo e da RECUPERACAO
          // (pipeline morto) e sobrescreve-lo aqui apagaria a faixa que a
          // pessoa tinha escolhido antes da queda. O sourceInfo chega com o
          // pipeline ja carregado, entao o selectTrack vale agora.
          if (audioAoCarregar < 0) video_escolher_audio(i);
          break;
        }
      } }

    // O PIPELINE NAO DA IDIOMA DE LEGENDA. Medido nesta TV, num arquivo com 43
    // legendas: o audioTrackInfo vem com "en"/"es"/"fr"/"it" e TODA entrada do
    // subtitleTrackInfo vem com "language":"(null)". Nao ha outro campo ali —
    // a informacao nao sai do pipeline, e a lista virava "Legenda 1..43", que
    // nao ajuda ninguem a escolher.
    //
    // O jeito de saber e ler o proprio arquivo, que e o que o navegador faz de
    // graca no app web. Dispara um fio que baixa os primeiros 2 MB por Range e
    // le o elemento Tracks do Matroska; quando volta, casa por trackNum e
    // reescreve os rotulos. Nao bloqueia a reproducao: se falhar, ou se o
    // arquivo nao for MKV, fica o que ja estava.
    { int faltando = 0, i;
      for (i = 0; i < nLeg; i++) if (!faixaLeg[i].idioma[0]) faltando = 1;
      // SO ANOTA. Quem dispara e o video_bombear, quando o buffer estiver
      // saudavel — a sonda concorre com a propria reproducao (mesma conexao,
      // mesmo servidor) e o sourceInfo chega justamente no pior instante, com o
      // pipeline ainda enchendo o buffer. MEDIDO na TV: buffer em falta 1,6 s
      // depois da leitura, caindo a 2,8 s e levando 9 s para se recuperar.
      //
      // O idioma da legenda nao tem pressa: so importa quando o dono abre a
      // folha de faixas.
      // MP4 nunca tem Tracks de Matroska: sondar e trafego garantidamente
      // perdido, e ele sai da MESMA conexao do video.
      if (faltando && !fonteMp4) mkvPendente = 1;
      else if (faltando) marco("mkv: fonte e MP4, sonda dispensada"); }
  }

  if (strstr(p, "videoInfo")) {
    viuVideo = 1;   // fecha o prazo do recuo de DV
    double v;
    v = numeroDe(p, "\"width\":");      if (v > 0) vidW = (int)v;
    v = numeroDe(p, "\"height\":");     if (v > 0) vidH = (int)v;
    v = numeroDe(p, "\"frameRate\":");  if (v > 0) vidTaxa = (int)v;
    v = numeroDe(p, "\"bitRate\":");    if (v > 0) vidBits = (long)v;
    { const char *q = strstr(p, "\"scanType\":\"");
      if (q) { const char *f; q += 12; f = strchr(q, '"');
        if (f && f - q < (int)sizeof vidVarredura) {
          memcpy(vidVarredura, q, f - q); vidVarredura[f - q] = 0; } } }
    { const char *q = strstr(p, "\"hdrType\":\"");
      if (q) { const char *f; q += 11; f = strchr(q, '"');
        if (f && f - q < (int)sizeof vidHdr) {
          memcpy(vidHdr, q, f - q); vidHdr[f - q] = 0;
          // Junto com o que a FONTE afirmava. Sozinho, o hdrType nao responde a
          // pergunta que importa em MKV: "pedimos Dolby Vision e a TV entregou
          // Dolby Vision, ou ela rebaixou para HDR10?". Os relatos de fora
          // (Kodi, Plex, UMS) dizem que o webOS aciona DV nativo em MP4 perfis
          // 5 e 8 e cai para HDR10 em Matroska; esta linha e o que permite
          // confirmar ou desmentir isso NESTA TV, com medida em vez de fama.
          printf("[video] HDR do pipeline: %s (fonte afirmava DV=%d)\n",
                 vidHdr, dvPedido);
          // Vai tambem para os MARCOS, que sao legiveis no aparelho: o stdout
          // do app lancado pelo applicationManager nao chega a lugar nenhum, e
          // era por isso que esta medida — a unica que responde se a TV honrou
          // ou rebaixou o Dolby Vision — so existia em teoria.
          { char m[64];
            snprintf(m, sizeof m, "hdr do pipeline: %s (fonte DV=%d)",
                     vidHdr, dvPedido);
            marco(m); } } } }
    // O Nuvio web que toca corretamente repassa estes valores sem alterar.
    // Para DolbyVision ele omite os dois blocos; montarVideoData faz o mesmo.
    { double x;
#define LER_SEI(nome, dst) do { x = numeroDe(p, "\"" nome "\":"); if (x >= 0) dst = (long)x; } while (0)
      LER_SEI("displayPrimariesX0", seiX0); LER_SEI("displayPrimariesX1", seiX1);
      LER_SEI("displayPrimariesX2", seiX2); LER_SEI("displayPrimariesY0", seiY0);
      LER_SEI("displayPrimariesY1", seiY1); LER_SEI("displayPrimariesY2", seiY2);
      LER_SEI("whitePointX", seiBrancoX); LER_SEI("whitePointY", seiBrancoY);
      LER_SEI("minDisplayMasteringLuminance", seiMinLum);
      LER_SEI("maxDisplayMasteringLuminance", seiMaxLum);
      LER_SEI("maxContentLightLevel", seiMaxCLL);
      LER_SEI("maxPicAverageLightLevel", seiMaxFALL);
#undef LER_SEI
      x = numeroDe(p, "\"colorPrimaries\":"); if (x >= 0) vuiPrim = (int)x;
      x = numeroDe(p, "\"transferCharacteristics\":"); if (x >= 0) vuiTrans = (int)x;
      x = numeroDe(p, "\"matrixCoeffs\":"); if (x >= 0) vuiMatriz = (int)x;
    }
  }
  if (strstr(p, "loadCompleted")) {
    marco("video loadCompleted");
    // ORDEM: faixas primeiro, posicao depois. Trocar de faixa reinicia o
    // decode no pipeline; fazer isso DEPOIS do seek jogaria a posicao fora.
    if (audioAoCarregar >= 0) {
      int a2 = audioAoCarregar; audioAoCarregar = -1;
      if (a2 > 0) video_escolher_audio(a2);
    }
    if (legUrlAoCarregar[0]) {
      char u[1024];
      snprintf(u, sizeof u, "%s", legUrlAoCarregar);
      legUrlAoCarregar[0] = 0; legAoCarregar = -1;
      video_legenda_externa(u);
    } else if (legAoCarregar >= 0) {
      int l2 = legAoCarregar; legAoCarregar = -1;
      video_escolher_legenda(l2);
    }
    if (posAoCarregar > 1.0) {
      double alvo = posAoCarregar;
      posAoCarregar = 0.0;
      video_buscar(alvo);
      marco("retomado apos queda do pipeline");
    }
    // O pipeline e novo: o estilo da legenda nao sobrevive ao load anterior.
    aplicarEstilo();
    pronto = 1;
    if (cronPediu && !cronLoad) {
      cronLoad = 1;
      printf("[video] load->loadCompleted %lums\n", msDesdePedido());
    }
    // O bind do ACB vai para um fio proprio COM PAUSAS entre os passos.
    // Motivo medido: cada chamada do AcbAPI e assincrona (o servico responde
    // pelo barramento) e disparando tudo em sequencia o setMediaVideoData
    // chegava antes do register terminar — o servico respondia
    // "piplineID key Error!!", parava a sequencia e nunca mandava o stopMute.
    // Sem o stopMute o video fica mudo: tela preta com audio normal.
    // E POR SESSAO, nao uma vez por processo: sem isto a segunda reproducao
    // herda um ACB apontando para o mediaId morto da anterior.
    if (acb && midia[0]) bindPendente = 1;
  }
  if (strstr(p, "bufferRange")) {
    double e = numeroDe(p, "\"endTime\":");
    if (e >= 0) bufferSeg = e;
  }
  // PAUSA POR FALTA DE DADOS. O dono relatou "fica pausando" e os marcos nao
  // registravam NADA — porque encher e esvaziar o buffer nao gera evento neste
  // lado, e uma pausa dessas nao passa por `paused` nem por erro. Sem isto a
  // unica coisa que sobra e adivinhar.
  //
  // Carimba quanto do buffer havia no instante: e o numero que separa "a fonte
  // nao entrega" de "o decoder engasgou".
  if (strstr(p, "bufferingStart")) {
    char m[64];
    snprintf(m, sizeof m, "buffering INICIO (buffer %+.1fs a frente)",
             bufferSeg - posSeg);
    marco(m);
  }
  if (strstr(p, "bufferingEnd")) {
    char m[64];
    snprintf(m, sizeof m, "buffering FIM (buffer %+.1fs a frente)",
             bufferSeg - posSeg);
    marco(m);
  }
  if (strstr(p, "playing")) {
    tocando = 1;
    if (acb && midia[0]) {
      long tarefa = 0;
      acbJanela(acb, janX, janY, janW, janH,
                (janX == 0 && janY == 0 && janW == 1920 && janH == 1080), &tarefa);
      printf("[video] janela reaplicada com o fluxo ja tocando\n"); fflush(stdout);
    }
  }
  if (strstr(p, "paused")) {
    // So carimba quando NAO fomos nos que pausamos: pausa do dono e esperada,
    // pausa vinda do pipeline e o defeito.
    if (tocando && !pausaPedida) marco("pausado PELO PIPELINE");
    tocando = 0;
  }
  if (strstr(p, "endOfStream")) { tocando = 0; marco("endOfStream"); }

  // ERRO DO PIPELINE. Nao havia tratamento nenhum: quando o uMS recusava um
  // seek ou perdia a fonte, o app simplesmente parava e ninguem sabia por que —
  // "eu passei e ele nao continuou mais" e exatamente o formato desse silencio.
  // Nao ha o que consertar sem saber a causa, e a causa vem no proprio evento.
  // ERRO DE VERDADE, e nao "errorCode: 0".
  //
  // A primeira versao carimbava tudo que tivesse `errorCode`, e o uMS manda
  // esse campo em resposta NORMAL — os marcos encheram de
  // `pipeline erro: errorText":"No Error"`, que e ruido escondendo o sinal.
  if (strstr(p, "errorText") && !strstr(p, "\"No Error\"")) {
    const char *q = strstr(p, "errorText");
    char m[96];
    snprintf(m, sizeof m, "pipeline erro: %.60s", q);
    { char *n2; for (n2 = m; *n2; n2++) if (*n2 == '\n' || *n2 == '\r') *n2 = ' '; }
    marco(m);
    // PIPELINE DESTRUIDO. Medido duas vezes na TV do dono: ~71 s depois de um
    // avanco, o uMS responde "com.webos.pipeline.<id> is not running" e o video
    // simplesmente para — o app nao fazia NADA, e era isso que ele descrevia
    // como "passei e nao continuou mais".
    //
    // Recarrega a mesma fonte e volta para onde estava. Nao e conserto da
    // CAUSA (o pipeline morre por algo entre o seek e a fonte do debrid, que
    // este lado nao enxerga), e sim de nao deixar o dono na tela parada.
    if (strstr(p, "is not running") && urlAtual[0] && !recuperando) {
      recuperando = 1;
      retomarEm = posSeg;
      // GUARDA AS ESCOLHAS. A posicao sozinha nao basta: o pipeline novo nasce
      // com a faixa 0 e a legenda desligada.
      audioAoCarregar = audioAtual;
      legAoCarregar   = legAtual;
      snprintf(legUrlAoCarregar, sizeof legUrlAoCarregar, "%s", legUrlAtual);
      marco("pipeline morreu: recarregando");
    }
  }
  { double v = numeroDe(p, "\"currentTime\":");
    if (v >= 0) {
      posSeg = v / 1000.0;
      // Primeiro quadro com avanco: o numero de inicio de verdade.
      if (cronPediu && !cronQuadro && posSeg > 0.0) {
        cronQuadro = 1;
        printf("[video] load->primeiro quadro %lums\n", msDesdePedido());
        fflush(stdout);
      }
    } }
  { double v = numeroDe(p, "\"duration\":");
    if (v >= 0) durSeg = v / 1000.0; }
  return 1;
}

static int soLog(LSHandle *h, LSMessage *m, void *u) {
  (void)h; (void)u;
  printf("[video] %s\n", lsPayload(m)); fflush(stdout);
  return 1;
}

static void chamar(const char *metodo, const char *carga, Filtro cb) {
  char uri[128]; unsigned long tok = 0;
  snprintf(uri, sizeof uri, "luna://com.webos.media/%s", metodo);
  if (!lsCall(bus, uri, carga, cb, NULL, &tok, ERRO))
    printf("[video] %s falhou\n", metodo);
}

// Variante para callbacks que precisam saber a qual sessao pertencem. O
// contexto e um inteiro convertido em ponteiro; nao ha alocacao para vazar nem
// memoria cujo tempo de vida possa acabar antes da resposta assincrona.
static void chamarCtx(const char *metodo, const char *carga, Filtro cb,
                      void *ctx) {
  char uri[128]; unsigned long tok = 0;
  snprintf(uri, sizeof uri, "luna://com.webos.media/%s", metodo);
  if (!lsCall(bus, uri, carga, cb, ctx, &tok, ERRO))
    printf("[video] %s falhou\n", metodo);
}

// Chamada a OUTRO servico. O recorte de fonte NAO mora no com.webos.media: ele
// respondeu `Unknown method "setDisplayWindow" for category "/"`, e a lista do
// `ls-monitor -i com.webos.media` confirma que nao existe ali. Quem tem os
// metodos de janela e o com.webos.service.tv.display:
//
//   "setDisplayWindow":       {"provides":["tv.management","private","tv.settings","all","public"]}
//   "setCustomDisplayWindow": idem
//
// setCustomDisplayWindow e o que aceita a fonte junto do destino, que e o
// recorte de que o zoom precisa.
static void chamarEm(const char *servico, const char *metodo,
                     const char *carga, Filtro cb) {
  char uri[160]; unsigned long tok = 0;
  snprintf(uri, sizeof uri, "luna://%s/%s", servico, metodo);
  if (!lsCall(bus, uri, carga, cb, NULL, &tok, ERRO))
    printf("[video] %s/%s falhou\n", servico, metodo);
}

static int aoCarregar(LSHandle *h, LSMessage *m, void *u) {
  const char *p = lsPayload(m), *q;
  char b[256];
  unsigned minhaSessao = (unsigned)(uintptr_t)u;
  (void)h;
  printf("[video] load: %s\n", p ? p : "(nulo)"); fflush(stdout);
  if (minhaSessao != sessao) {
    printf("[video] load antigo ignorado (sessao %u, atual %u)\n",
           minhaSessao, sessao);
    // Um load cancelado ainda pode criar um pipeline no uMS. Liberar esse
    // recurso evita deixar o decoder ocupado quando o usuario reabre o filme.
    char antigo[96] = "";
    if (p) js_texto(p, NULL, "mediaId", antigo, sizeof antigo);
    if (antigo[0] && strcmp(antigo, midia)) {
      snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", antigo);
      chamar("unload", b, soLog);
    }
    return 1;
  }
  if (!p || midia[0]) return 1;
  q = strstr(p, "\"mediaId\":\"");
  if (!q) return 1;
  q += 11;
  { const char *f = strchr(q, '"');
    if (!f || f - q >= (int)sizeof midia) return 1;
    memcpy(midia, q, f - q); midia[f - q] = 0; }

  snprintf(b, sizeof b, "{\"connectionId\":\"%s\"}", midia);
  chamar("notifyForeground", b, soLog);
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  chamarCtx("subscribe", b, aoEvento, (void *)(uintptr_t)minhaSessao);
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"type\":\"video\",\"index\":0}", midia);
  chamar("selectTrack", b, soLog);
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  chamar("play", b, soLog);
  return 1;
}

static void *rodarLaco(void *u) { (void)u; loopRodar(laco); return NULL; }

// O ACB EXIGE um callback de verdade. Passar NULL nao e ignorado: no primeiro
// evento ele salta para o endereco 0 e o app morre com SIGSEGV em pc=0x0, longe
// do ponto onde o NULL foi escrito.
static void acbNotificou(long h, long tarefa, long evento,
                         long estApp, long estToca, int resposta) {
  (void)h; (void)tarefa;
  printf("[video] acb evento=%ld app=%ld toca=%ld resp=%d\n",
         evento, estApp, estToca, resposta);
  fflush(stdout);
}

#define SIM(h, v, n) do { \
    *(void **)(&v) = dlsym(h, n); \
    if (!v) { printf("[video] falta %s\n", n); return 0; } \
  } while (0)

int video_iniciar(void) {
  void *L, *G, *A;
  if (ligado) return 1;
  L = dlopen("libluna-service2.so.3", RTLD_NOW);
  if (!L) L = dlopen("libluna-service2.so", RTLD_NOW);
  G = dlopen("libglib-2.0.so.0", RTLD_NOW);
  A = dlopen("libAcbAPI.so.1", RTLD_NOW);
  if (!L || !G) { printf("[video] libs: %s\n", dlerror()); return 0; }
  if (!A) {
    // webOS 5+: sem ACB, a janela exportada da SDL e quem prende o plano.
    // dlopen(NULL) devolve o handle do PROPRIO processo: a libSDL2 ja esta
    // carregada (o binario linka contra ela), entao os simbolos, quando
    // existem, estao ai. dlopen de outra copia da SDL daria dois estados dela
    // no mesmo processo. (RTLD_DEFAULT faria o mesmo, mas exige _GNU_SOURCE.)
    void *eu = dlopen(NULL, RTLD_NOW);
    printf("[video] sem libAcbAPI (webOS 5+): tentando janela exportada da SDL\n");
    *(void **)(&sdlExpCriar)    = dlsym(eu, "SDL_webOSCreateExportedWindow");
    *(void **)(&sdlExpJanela)   = dlsym(eu, "SDL_webOSSetExportedWindow");
    *(void **)(&sdlExpRecorte)  = dlsym(eu, "SDL_webOSExportedSetCropRegion");
    *(void **)(&sdlExpDestruir) = dlsym(eu, "SDL_webOSDestroyExportedWindow");
    // O recorte de fonte e opcional (custa o zoom, nao a imagem), do mesmo jeito
    // que AcbAPI_setCustomDisplayWindow e opcional no caminho do ACB.
    if (!sdlExpRecorte) printf("[video] sem SDL_webOSExportedSetCropRegion; zoom fica indisponivel\n");
    if (!sdlExpCriar || !sdlExpJanela) {
      printf("[video] esta TV nao tem ACB nem janela exportada; sem caminho de video\n");
      return 0;
    }
    { const char *id = sdlExpCriar(0);   /* 0 = ..._TYPE_VIDEO */
      if (!id || !*id) { printf("[video] SDL_webOSCreateExportedWindow nao devolveu id\n"); return 0; }
      snprintf(expWin, sizeof expWin, "%s", id);
      printf("[video] janela exportada: %s\n", expWin); }
  }

  SIM(L, lsRegister, "LSRegister");
  SIM(L, lsAttach,   "LSGmainAttach");
  SIM(L, lsCall,     "LSCall");
  SIM(L, lsPayload,  "LSMessageGetPayload");
  SIM(G, loopNovo,   "g_main_loop_new");
  SIM(G, loopRodar,  "g_main_loop_run");
  SIM(G, loopParar,  "g_main_loop_quit");
  if (A) {
  SIM(A, acbCriar,    "AcbAPI_create");
  SIM(A, acbIniciar,  "AcbAPI_initialize");
  SIM(A, acbSink,     "AcbAPI_setSinkType");
  SIM(A, acbMidia,    "AcbAPI_setMediaId");
  SIM(A, acbEstado,   "AcbAPI_setState");
  SIM(A, acbJanela,   "AcbAPI_setDisplayWindow");
  // NAO usa SIM: se a lib desta TV nao tiver o simbolo, o app segue sem zoom
  // em vez de nao iniciar. O recorte e util, mas nao vale o app inteiro.
  *(void **)(&acbJanelaCustom) = dlsym(A, "AcbAPI_setCustomDisplayWindow");
  if (!acbJanelaCustom) printf("[video] sem AcbAPI_setCustomDisplayWindow; zoom fica indisponivel\n");
  SIM(A, acbDestruir, "AcbAPI_destroy");
  SIM(A, acbConectar, "AcbAPI_connectDass");
  SIM(A, acbVideoData, "AcbAPI_setMediaVideoData");
  SIM(A, acbAudioData, "AcbAPI_setMediaAudioData");
  }

  // O nome PRECISA casar com o padrao do papel LS2 do app
  // (allowedNames: "com.webos.media.client.*"). Qualquer outro nome e recusado
  // pelo hub e nada depois disso acontece.
  if (!lsRegister("com.webos.media.client.nuvio", &bus, ERRO)) {
    printf("[video] LSRegister recusado\n"); return 0;
  }
  laco = loopNovo(NULL, 0);
  if (!lsAttach(bus, laco, ERRO)) { printf("[video] attach falhou\n"); return 0; }
  // Laco proprio: o LS2 exige um GMainLoop girando, e girar isso no laco de
  // desenho custaria quadros. As respostas chegam neste fio e so mexem em
  // variaveis simples, lidas pelo desenho sem trava.
  pthread_create(&fio, NULL, rodarLaco, NULL);

  if (A) {
    lerAjustesAcb();
    acb = acbCriar();
    acbIniciar(acb, tipoJogador, "space.nuvio.native.legacy", (void *)acbNotificou);
    acbSink(acb, tipoSink);
  }
  ligado = 1;
  printf("[video] pronto (webOS %d, acb=%ld, janela=%s)\n",
         webosMaior(), acb, expWin[0] ? expWin : "-");
  fflush(stdout);
  return 1;
}

// --- recuo automatico do Dolby Vision ---------------------------------------
//
// MEDIDO na OLED65C9, dois arquivos DV em MKV:
//   arquivo A: sem DolbyHdrInfo toca em HDR10; COM o bloco engata Dolby Vision.
//   arquivo B: sem o bloco toca normal (HDR10, com imagem); COM o bloco fica
//              SO O AUDIO, e o pipeline nunca reporta videoInfo.
// Testado 8/"single" e 7/"dual" no arquivo B: os dois quebram igual.
//
// Como nao demuxamos, nao ha como saber de antemao em qual dos dois casos a
// fonte cai — declarar as cegas ganha DV num arquivo e perde a IMAGEM no outro,
// que e troca ruim. Entao a declaracao vira uma APOSTA COM PRAZO: se o pipeline
// nao reportar videoInfo em NV_DV_PRAZO_MS, recarrega a mesma URL sem o bloco.
// O custo e alguns segundos no arquivo que nao aceita; o ganho e nunca ficar
// sem imagem por causa de uma afirmacao nossa.
#define NV_DV_PRAZO_MS 7000


// --- idioma das legendas lido do proprio arquivo -----------------------------
// Ver a nota no ponto de disparo, logo abaixo do parse do sourceInfo.
// INICIO DOS CREDITOS, em segundos, ou 0 quando o arquivo nao diz.
//
// Sai do capitulo final do Matroska, lido na MESMA descida de 320 KB que ja
// buscava as faixas. E o "marcador correto" que faltava: sem ele, quando os
// creditos comecam so pode ser chutado, e o chute erra em minutos — cedo demais
// rouba o desfecho, tarde demais aparece com os creditos ja rolando.
// Dois valores, e nao um: o do NOME e conclusivo assim que lido; o POSICIONAL
// depende da duracao, que quase sempre ainda e 0 quando o cabecalho termina de
// ser lido (o fio do MKV corre junto com a abertura da sessao de video). Fixar
// o posicional ali daria 0 sempre, e a regra existiria sem nunca valer.
static double creditosNomeado;   // capitulo que se identifica como creditos
static double creditosUltimo;    // inicio do ultimo capitulo, seja qual for

double video_creditos(void) {
  double dur;
  if (creditosNomeado > 1.0) return creditosNomeado;
  dur = video_duracao();
  // O ultimo capitulo so vale como creditos se comecar no ultimo quarto: em
  // disco com um capitulo a cada cinco minutos, o ultimo e uma cena qualquer.
  if (creditosUltimo > 1.0 && dur > 1.0 && creditosUltimo > dur * 0.75)
    return creditosUltimo;
  return 0.0;
}

static void *lerMkv(void *arg) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  MkvCap   caps[MKV_MAX_CAPS];
  char url[1024];
  int n, i, j, casou = 0, nCaps = 0;
  (void)arg;

  snprintf(url, sizeof url, "%s", urlAtual);

  n = mkv_faixas_e_caps(url, fx, MKV_MAX_FAIXAS, caps, MKV_MAX_CAPS, &nCaps);
  if (nCaps > 0) {
    creditosNomeado = mkv_creditos_nomeados(caps, nCaps);
    creditosUltimo  = nCaps > 1 ? caps[nCaps - 1].inicio : 0.0;
    printf("[mkv] %d capitulos; creditos nomeados em %.0fs, ultimo \"%s\" em %.0fs\n",
           nCaps, creditosNomeado, caps[nCaps - 1].nome, creditosUltimo);
    fflush(stdout);
  }
  if (n < 1) {
    // Sem isto o unico sinal era uma linha de stdout, que na TV nao chega a
    // lugar nenhum — e a lista ficava em "Legenda 1, Legenda 2" sem ninguem
    // saber se o arquivo nao e MKV, se o Range falhou ou se o cabecalho passa
    // dos 2 MB que baixamos.
    marco("mkv: nenhuma faixa lida (nao e MKV, ou Range falhou)");
    fioMkvVivo = 0; return NULL;
  }

  // SEM MUTEX, e de proposito: este arquivo nao tem um. faixaLeg ja e escrito
  // pelo fio de resposta do luna e lido pelo desenho sem trava nenhuma, e
  // introduzir uma trava so aqui daria falsa seguranca — protegeria a escrita
  // e nao a leitura. O dano possivel e um rotulo lido pela metade em UM quadro;
  // por isso cada campo e preenchido de uma vez, com um snprintf so, e o
  // rotulo (que e o que aparece) e escrito por ULTIMO, depois do idioma.
  // O trackNum do sourceInfo da LG e o TrackNumber do Matroska: casar por ele,
  // e nao por ordem. As duas listas nao vem na mesma ordem (o sourceInfo desta
  // TV comecou em 42, 40, 41, 32...), e casar por posicao trocaria os idiomas
  // de lugar — pior que nao ter idioma nenhum.
  for (i = 0; i < nLeg; i++) {
    if (faixaLeg[i].idioma[0]) continue;
    for (j = 0; j < n; j++) {
      if (fx[j].numero != faixaLeg[i].numero) continue;
      if (fx[j].idioma[0] && strcmp(fx[j].idioma, "und")) {
        snprintf(faixaLeg[i].idioma, sizeof faixaLeg[i].idioma, "%s", fx[j].idioma);
        casou++;
      }
      // O NOME da faixa ("Forced", "SDH", "Full") e o que separa duas legendas
      // do MESMO idioma. Sem ele o dono ve "Portugues" tres vezes e escolhe no
      // escuro — e essa e justamente a lista que ele reclamou.
      if (fx[j].nome[0])
        snprintf(faixaLeg[i].rotulo, sizeof faixaLeg[i].rotulo, "%s%s%s",
                 faixaLeg[i].idioma[0] ? i18n(ling_nome(faixaLeg[i].idioma)) : "",
                 faixaLeg[i].idioma[0] ? "  \xc2\xb7  " : "", fx[j].nome);
      else if (faixaLeg[i].idioma[0])
        snprintf(faixaLeg[i].rotulo, sizeof faixaLeg[i].rotulo, "%s",
                 i18n(ling_nome(faixaLeg[i].idioma)));
      break;
    }
  }
  { char m[64];
    snprintf(m, sizeof m, "mkv: %d faixas lidas, %d legendas com idioma", n, casou);
    marco(m); }
  printf("[mkv] %d legendas ganharam idioma\n", casou);
  fflush(stdout);
  fioMkvVivo = 0;
  return NULL;
}

static long  msDoLoad = 0;

static long agoraMs(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static int tocarInterno(const char *url, int comDV);

int video_tocar(const char *url) {
  dvRecuado = 0;
  // FONTE NOVA, decisao nova: o "sem HDR" era sobre o arquivo anterior.
  semDVForcado = 0;
  // Titulo novo: o marcador do anterior nao vale. Sem isto um filme sem
  // capitulos herdaria os creditos do filme de antes — e o painel subiria numa
  // hora sem relacao nenhuma com o que esta tocando.
  creditosNomeado = creditosUltimo = 0.0;
  snprintf(urlAtual, sizeof urlAtual, "%s", url ? url : "");
  return tocarInterno(url, 1);
}

// Chamado uma vez por quadro. So existe para o prazo acima: sem ele o recuo
// dependeria de o usuario perceber que nao ha imagem e sair da tela.
void video_bombear(void) {
  // O ACB demora cerca de 1,5 s para ligar uma sessao. Se o usuario sair e
  // reabrir nesse intervalo, o loadCompleted novo encontra bindVivo=1. Antes
  // ele simplesmente desistia para sempre; agora o pedido fica pendente.
  if (bindPendente && !bindVivo && acb && midia[0]) {
    bindPendente = 0;
    bindVivo = 1;
    if (pthread_create(&fioBind, NULL, prenderPlano, NULL) == 0)
      pthread_detach(fioBind);
    else {
      bindVivo = 0;
      bindPendente = 1;
    }
  }
  // SONDA DE MKV so com folga de buffer. 20 s a frente e o sinal de que a
  // fonte esta entregando mais rapido do que o decoder consome, e portanto de
  // que ha banda sobrando para os 320 KB do cabecalho.
  if (mkvPendente && !fioMkvVivo && urlAtual[0] && bufferSeg - posSeg >= 20.0) {
    mkvPendente = 0;
    fioMkvVivo = 1;
    if (pthread_create(&fioMkv, NULL, lerMkv, NULL) != 0) fioMkvVivo = 0;
    else pthread_detach(fioMkv);
  }
  // Avanco pendente que ja repousou.
  if (seekEm && SDL_GetTicks() >= seekEm) {
    Uint32 q = seekEm; seekEm = 0; (void)q;
    seekAgora(seekAlvo);
  }
  // RECUPERACAO DO PIPELINE, no fio principal. Ver a nota em `recuperando`.
  if (recuperando) {
    double alvo = retomarEm;
    recuperando = 0;
    marco("recarregando a fonte");
    if (tocarInterno(urlAtual, !semDVForcado) && alvo > 1.0) {
      // O seek so vale depois do load; guardar o alvo e deixar o
      // loadCompleted aplica-lo evita mandar posicao para um pipeline que
      // ainda nao existe.
      posAoCarregar = alvo;
    }
  }
  // O recuo por prazo foi REMOVIDO por nao funcionar: o gatilho era "o pipeline
  // nao reportou videoInfo", e o uMS reporta videoInfo, sourceInfo e
  // loadCompleted normalmente mesmo nos arquivos que ficam sem imagem. Medido:
  // videoInfo 3840x1606 hdrType=DolbyVision e loadCompleted em 3212ms, tela
  // preta com audio correndo. Nao ha no uMS sinal de QUADRO EXIBIDO — o
  // currentTime avanca puxado pelo audio.
  //
  // A funcao fica porque a batida por quadro e util assim que existir um sinal
  // melhor (contador de quadros, ou o proprio perfil lido do MKV).
  (void)dvNaCarga; (void)dvRecuado; (void)viuVideo;
  (void)msDoLoad; (void)urlAtual; (void)agoraMs; (void)tocarInterno;
}

static int tocarInterno(const char *url, int comDV) {
  char carga[2048];
  unsigned minhaSessao;
  if (!ligado && !video_iniciar()) return 0;
  video_parar();
  minhaSessao = ++sessao;
  viuVideo = 0;
  // O retangulo aplicado e da SESSAO: sem zerar, uma sessao nova que calcule o
  // mesmo rect cairia no "ja e esse" e nunca chegaria a mandar nada ao plano.
  // (semUms NAO zera: se esta TV nao entende o recorte de fonte, nao passa a
  // entender no titulo seguinte, e insistir so arrisca a imagem de novo.)
  fonX = -1; dstX = dstY = dstW = dstH = -1;
  posSeg = durSeg = bufferSeg = 0; tocando = pronto = 0; midia[0] = 0;
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; vidAtmos = 0;
  legUrlAtual[0] = 0; mkvPendente = 0;
  snprintf(vidHdr, sizeof vidHdr, "none");
  seiX0 = seiX1 = seiX2 = seiY0 = seiY1 = seiY2 = 0;
  seiBrancoX = seiBrancoY = seiMinLum = seiMaxLum = seiMaxCLL = seiMaxFALL = 0;
  vuiPrim = vuiTrans = vuiMatriz = 2;
  // A afirmacao de DV da fonte escolhida sobrevive ao reset: e ela que o bind
  // descreve ao tv.display. Sem ela, toda sessao nasceria "none".
  vidDV = dvPedido;
  cronPediu = 1; cronLoad = 0; cronQuadro = 0;
  clock_gettime(CLOCK_MONOTONIC, &t0Pedido);
  // windowId "window_id_dummy" NAO e enfeite: com string vazia o load responde
  // returnValue:true, aloca mediaId e nunca busca o arquivo. Falha muda.
  // DolbyHdrInfo: e assim que o Kodi anuncia Dolby Vision a este mesmo pipeline
  // (xbmc/cores/VideoPlayer/MediaPipelineWebOS.cpp):
  //   contents["DolbyHdrInfo"]["encryptionType"] = "clear"
  //   contents["DolbyHdrInfo"]["profileId"]      = dovi.dv_profile
  //   contents["DolbyHdrInfo"]["trackType"]      = el_present_flag ? "dual" : "single"
  //
  // DIFERENCA QUE PODE INVALIDAR TUDO ISTO, e por isso e um EXPERIMENTO: o Kodi
  // demuxa com ffmpeg e ENTREGA BUFFERS por option.externalStreamingInfo, onde
  // esse bloco vive. Nos passamos uma URI e a TV faz HTTP, demux e decode.
  // Declarar o bloco no modo URI pode ser ignorado em silencio — e a unica forma
  // de saber e medir o hdrType que volta.
  //
  // profileId 8 / "single" e o que o Kodi declara DEPOIS de converter o perfil 7,
  // nao o que o arquivo tem. Como nao demuxamos, nao sabemos o perfil real; por
  // isso os valores sao ajustaveis por variavel de ambiente para poder testar
  // 7/"dual" contra 8/"single" no mesmo arquivo sem recompilar.
  char dolby[192] = "";
  dvNaCarga = 0;
  if (dvPedido && comDV) {
    // Os valores tambem saem de /tmp/nuvio-dv.conf ("<perfil> <trilha>", ex:
    // "7 dual"), porque o app e lancado pelo SAM e nao da para passar variavel
    // de ambiente por ali. Sem o arquivo, valem o ambiente e depois o padrao.
    static char pArq[16], tArq[16];
    const char *perfil = getenv("NUVIO_DV_PROFILE");
    const char *trilha = getenv("NUVIO_DV_TRACK");
    { FILE *f = fopen("/tmp/nuvio-dv.conf", "r");
      if (f) {
        pArq[0] = tArq[0] = 0;
        if (fscanf(f, "%15s %15s", pArq, tArq) >= 1) {
          if (pArq[0]) perfil = pArq;
          if (tArq[0]) trilha = tArq;
        }
        fclose(f);
      } }
    // DESLIGADO POR PADRAO, e a razao esta medida:
    //
    //   arquivo A: sem o bloco toca em HDR10; COM o bloco engata Dolby Vision.
    //   varios outros: sem o bloco tocam normal; COM o bloco ficam SEM IMAGEM
    //                  (um chegou a mostrar o primeiro quadro e congelar, com o
    //                  audio correndo).
    //
    // Ganhar DV num arquivo e perder a imagem em varios e troca ruim. E nao ha
    // como escolher sozinho: tentei um prazo que recarregaria sem o bloco caso
    // o pipeline nao reportasse video, e ele NAO SERVE — o uMS reporta videoInfo
    // (3840x1606, hdrType DolbyVision) e loadCompleted normalmente mesmo quando
    // nenhum quadro chega ao plano. "Reportou" nao e "exibiu", e nao existe no
    // uMS um sinal de quadro avancando: currentTime anda com o audio.
    //
    // Entao vira OPT-IN, para experimentar arquivo a arquivo:
    //   echo "8 single" > /tmp/nuvio-dv.conf   (ou "7 dual")
    //   rm /tmp/nuvio-dv.conf                  volta ao seguro
    // O caminho definitivo e saber o perfil real do arquivo antes de afirmar
    // qualquer coisa: ler o cabecalho do MKV por HTTP Range e achar o
    // BlockAdditionMapping com dvcC/dvvC, que e onde o Matroska guarda isso.
    if (!perfil || !*perfil || !strcmp(perfil, "off")) {
      /* sem declaracao: comportamento conhecido e seguro */
    } else {
      snprintf(dolby, sizeof dolby,
               "\"externalStreamingInfo\":{\"contents\":{\"DolbyHdrInfo\":{"
               "\"encryptionType\":\"clear\",\"profileId\":%s,\"trackType\":\"%s\"}}},",
               (perfil && *perfil) ? perfil : "8",
               (trilha && *trilha) ? trilha : "single");
      printf("[video] DolbyHdrInfo declarado: %s\n", dolby); fflush(stdout);
      dvNaCarga = 1;
    }
  }
  snprintf(carga, sizeof carga,
      "{\"payload\":{\"option\":{\"useSeekableRanges\":true,"
      "\"appId\":\"space.nuvio.native.legacy\","
      "%s"
      "\"bufferControl\":{\"userBufferCtrl\":false},"
      "\"windowId\":\"%s\"}},"
      "\"uri\":\"%s\",\"type\":\"media\"}",
      dolby,
      // No caminho do ACB o id e um marcador qualquer (so nao pode ser vazio,
      // ver acima); no da webOS 5 ele e o endereco REAL do plano exportado e um
      // valor errado aqui deixa o video sem para onde ir.
      expWin[0] ? expWin : "window_id_dummy",
      url);
  printf("[video] URL: %s\n", url); fflush(stdout);
  msDoLoad = agoraMs();
  chamarCtx("load", carga, aoCarregar, (void *)(uintptr_t)minhaSessao);
  return 1;
}

void video_parar(void) {
  char b[128];
  // Invalida tambem a sessao que ainda esta esperando o retorno de load. Esse
  // era o caso abrir -> sair -> abrir que travava: nao havia mediaId para
  // descarregar, mas o callback antigo continuava vivo e contaminava o novo.
  sessao++;
  bindPendente = 0;
  recuperando = 0; retomarEm = posAoCarregar = 0.0;
  audioAoCarregar = legAoCarregar = -1;
  legUrlAoCarregar[0] = 0;
  pausaPedida = 0; seekEm = 0; mkvPendente = 0;
  if (ligado && midia[0]) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
    chamar("unload", b, soLog);
  }
  midia[0] = 0; tocando = pronto = 0;
}

void video_pausar(int pausado) {
  char b[128];
  if (!ligado || !midia[0]) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  chamar(pausado ? "pause" : "play", b, soLog);
  tocando = !pausado;
  pausaPedida = pausado;
}

// AVANCO COM REPOUSO.
//
// Medido na TV: segurar a seta produzia QUATRO seeks em 0,8 s (16 s, 26 s, 36 s,
// 46 s) — quatro pedidos de faixa seguidos a mesma fonte, e ~71 s depois o
// pipeline morria. Nao esta provado que um causa o outro, mas mandar quatro
// posicoes quando o dono quis UMA e desperdicio de qualquer forma: as tres
// primeiras sao descartadas assim que a quarta chega.
//
// A posicao MOSTRADA muda na hora (senao a barra nao responde ao toque); o que
// espera o repouso e o comando ao pipeline.
#define SEEK_REPOUSO_MS 350

void video_buscar(double segundos) {
  if (!ligado || !midia[0]) return;
  if (segundos < 0) segundos = 0;
  posSeg = segundos;
  seekAlvo = segundos;
  seekEm = SDL_GetTicks() + SEEK_REPOUSO_MS;
}

// Manda de fato. Chamado pelo video_bombear quando o repouso vence.
static void seekAgora(double segundos) {
  char b[192];
  if (!ligado || !midia[0]) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"position\":%d}",
           midia, (int)(segundos * 1000.0));
  chamar("seek", b, soLog);
  { char m[48]; snprintf(m, sizeof m, "seek para %ds", (int)segundos); marco(m); }
}

// O retangulo do plano de hardware. E por AQUI que os modos de zoom acontecem:
// o video nao e um elemento com `transform: scale()` como no app web — e um
// plano atras da superficie GL, e ampliar significa mandar um retangulo MAIOR
// que a tela, com x/y negativos, e deixar o excedente sair pela borda. E o
// mesmo resultado do transform do web: a barra preta embutida no quadro sai da
// area visivel em vez de ser (impossivelmente) recortada por object-fit.
//
// O retangulo NAO pode passar da tela. MEDIDO: mandar ao ACB um retangulo com
// origem negativa ou maior que o painel (que era como eu tentava ampliar) NAO
// recorta nada — o plano simplesmente APAGA, e a tela fica preta em todo modo
// com escala, com imagem so no ORIGINAL, o unico onde a escala e 1. O
// acbJanela recebe UM retangulo, o de DESTINO, e nao existe recorte de fonte
// ali; um plano de hardware nao descarta o excedente como o compositor do
// navegador faz com transform: scale(). Quem amplia e o video_janela_fonte
// abaixo, recortando a FONTE.
void video_janela(int x, int y, int w, int h) {
  long tarefa = 0;
  int cheia = (x == 0 && y == 0 && w == 1920 && h == 1080);
  if (w < 1 || h < 1) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 1920) w = 1920 - x;
  if (y + h > 1080) h = 1080 - y;
  if (w < 1 || h < 1) return;
  if (x == janX && y == janY && w == janW && h == janH) return;  // sem repetir o mesmo rect a cada quadro
  janX = x; janY = y; janW = w; janH = h;
  if (!ligado || !midia[0]) return;   // sem midia presa, aplicar seria no vazio
  if (!acb && !expWin[0]) return;
  printf("[video] janela %d,%d %dx%d cheia=%d\n", x, y, w, h, cheia);
  fflush(stdout);
  if (expWin[0]) {
    // src NULL = o quadro inteiro. Quem recorta a fonte e o video_janela_fonte.
    SDL_Rect dst; dst.x = x; dst.y = y; dst.w = w; dst.h = h;
    printf("[video] janela exportada -> %d\n", sdlExpJanela(expWin, NULL, &dst));
    fflush(stdout);
    return;
  }
  acbJanela(acb, x, y, w, h, cheia, &tarefa);
}

// A resposta do uMS ao setDisplayWindow, LOGADA — e AGIDA.
//
// A assinatura source/destination ainda nao foi confirmada nesta TV (o
// ls-monitor ficou para depois, a TV estava ocupada). Se ela estiver errada, o
// pedido e recusado e o plano fica com o retangulo de antes — ou seja, o
// sintoma seria de novo "tela preta no zoom", que e exatamente o erro que esta
// rodada corrigiu. Entao a recusa nao pode passar calada: ao primeiro
// returnValue:false o modulo DESISTE do caminho do uMS e volta ao acbJanela em
// tela cheia. Perde-se o zoom, que e um recurso; nao se perde a imagem, que e o
// filme. Errar para o lado de continuar mostrando video e a unica escolha
// defensavel enquanto isto nao foi medido.
static int semUms;   // 1 depois que o uMS recusou o setDisplayWindow

static int aoJanela(LSHandle *h, LSMessage *m, void *u) {
  const char *p = lsPayload(m);
  (void)h; (void)u;
  printf("[video] setDisplayWindow -> %s\n", p ? p : "(nulo)");
  fflush(stdout);
  if (p && strstr(p, "\"returnValue\":false")) {
    long tarefa = 0;
    semUms = 1;
    printf("[video] uMS recusou o recorte de fonte; voltando a tela cheia pelo ACB\n");
    fflush(stdout);
    janX = janY = 0; janW = 1920; janH = 1080;
    if (acb) acbJanela(acb, 0, 0, 1920, 1080, 1, &tarefa);
  }
  return 1;
}

// ZOOM DE VERDADE: recorta a FONTE e mantem o destino dentro da tela.
//
// O uMS aceita os dois retangulos na mesma chamada — `source` em coordenadas do
// QUADRO DECODIFICADO e `destination` em coordenadas de tela. Ampliar entao nao
// e inflar o destino (que apaga o plano), e sim pedir um pedaco MENOR da fonte
// para o mesmo destino: e assim que a barra preta embutida no quadro sai da
// area visivel. E a mesma imagem que o web produz com transform: scale(), so
// que calculada do lado certo do escalonador.
//
// Mantem o acbJanela para o caso de tela cheia sem recorte, que ja funcionava.
void video_janela_fonte(int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh) {
  char b[420];
  int cheia;
  if (sw < 2 || sh < 2 || dw < 1 || dh < 1) return;
  // O uMS ja recusou uma vez nesta sessao: nao insistir. Cada tentativa nova
  // seria outra chance de deixar o plano num estado sem imagem.
  if (semUms) { video_janela(dx, dy, dw, dh); return; }
  // Destino preso a tela: o mesmo limite que vale para o acbJanela.
  if (dx < 0) { dw += dx; dx = 0; }
  if (dy < 0) { dh += dy; dy = 0; }
  if (dx + dw > 1920) dw = 1920 - dx;
  if (dy + dh > 1080) dh = 1080 - dy;
  if (dw < 1 || dh < 1) return;
  cheia = (dx == 0 && dy == 0 && dw == 1920 && dh == 1080);
  if (sx == fonX && sy == fonY && sw == fonW && sh == fonH &&
      dx == dstX && dy == dstY && dw == dstW && dh == dstH) return;
  fonX = sx; fonY = sy; fonW = sw; fonH = sh;
  dstX = dx; dstY = dy; dstW = dw; dstH = dh;
  janX = dx; janY = dy; janW = dw; janH = dh;   // o reaplicar do bind usa estes
  if (!ligado || !midia[0]) return;
  // Formato do com.webos.service.tv.display: `sourceInput` e o recorte no
  // quadro decodificado, `displayOutput` o retangulo na tela, `sink` MAIN
  // porque o video vai para o plano principal (o secundario e o PIP).
  snprintf(b, sizeof b,
           "{\"sink\":\"MAIN\",\"fullScreen\":%s,"
           "\"sourceInput\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d},"
           "\"displayOutput\":{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}}",
           cheia ? "true" : "false", sx, sy, sw, sh, dx, dy, dw, dh);
  printf("[video] fonte %d,%d %dx%d -> destino %d,%d %dx%d\n",
         sx, sy, sw, sh, dx, dy, dw, dh);
  fflush(stdout);
  // webOS 5+: o recorte vive na janela exportada. `org` e o quadro inteiro,
  // `src` o pedaco pedido e `dst` o retangulo na tela — os mesmos tres papeis
  // do sourceInput/displayOutput do tv.display, so que ditos a SDL.
  if (expWin[0]) {
    SDL_Rect org, src, dst;
    org.x = 0;  org.y = 0;  org.w = vidW > 0 ? vidW : 1920; org.h = vidH > 0 ? vidH : 1080;
    src.x = sx; src.y = sy; src.w = sw; src.h = sh;
    dst.x = dx; dst.y = dy; dst.w = dw; dst.h = dh;
    if (sdlExpRecorte && sdlExpRecorte(expWin, &org, &src, &dst)) return;
    // Recusou (ou nem existe): cair para tela cheia sem recorte pela mesma
    // regra do ACB — perde-se o zoom, nao a imagem.
    printf("[video] janela exportada recusou o recorte; sem zoom\n"); fflush(stdout);
    semUms = 1;
    video_janela(dx, dy, dw, dh);
    return;
  }
  // O caminho e o ACB, nao o luna direto: o hub recusa o app no tv.display.
  if (acbJanelaCustom && acb) {
    long tarefa = 0;
    int r = acbJanelaCustom(acb, sx, sy, sw, sh, dx, dy, dw, dh, cheia, &tarefa);
    printf("[video] acb janela custom -> %d\n", r); fflush(stdout);
    if (r) return;
    printf("[video] acb recusou o recorte; voltando a tela cheia\n"); fflush(stdout);
  }
  semUms = 1;
  video_janela(dx, dy, dw, dh);
  (void)b; (void)aoJanela;
}

double video_pos(void)      { return posSeg; }
double video_duracao(void)  { return durSeg; }
double video_buffer_fim(void) { return bufferSeg; }
int    video_tocando(void)  { return tocando; }
int    video_pronto(void)   { return pronto; }
// Ha midia carregada. O furo na superficie usa ISTO e nao o loadCompleted:
// abrir o buraco cedo nao custa nada (atras dele so existe o plano de video) e
// esperar o evento deixaria a tela desenhada por cima do video se o evento
// mudar de nome ou nao vier.
int    video_ativo(void)    { return midia[0] != 0; }

int  video_n_audio(void)   { return nAudio; }
int  video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_audio(int i)   { return (i >= 0 && i < nAudio) ? &faixaAudio[i] : NULL; }
const VideoFaixa *video_legenda(int i) { return (i >= 0 && i < nLeg) ? &faixaLeg[i] : NULL; }
int  video_audio_atual(void)   { return audioAtual; }
int  video_legenda_atual(void) { return legAtual; }
int  video_tem_atmos(void)        { return vidAtmos; }
// O SELO agora sai do PIPELINE, nao da afirmacao da fonte.
//
// `vidDV` e o que o addon AFIRMOU sobre a URL, e continua sendo o que o bind
// descreve ao tv.display (montarVideoData le vidDV, nao esta funcao) — la a
// afirmacao e a unica informacao disponivel antes de haver imagem, e sem ela
// nao ha como pedir Dolby Vision. Mas para o SELO ela e a fonte errada: esta
// MEDIDO nesta TV que um MKV anunciado como DV volta com hdrType "HDR10" no
// videoInfo. Ligar o selo na afirmacao fazia a tela anunciar Dolby Vision em
// cima de um fluxo HDR10 — e selo que mente e pior que selo ausente, porque e
// nele que o dono confia para saber se pegou a versao boa.
//
// Antes do videoInfo chegar, vidHdr e "none" e a resposta e 0: nenhum selo por
// alguns segundos e honesto; um selo que aparece e depois se desmente, nao.
int  video_tem_dolby_vision(void) {
  return !strcasecmp(vidHdr, "DolbyVision") || !strcasecmp(vidHdr, "dolby_vision");
}
// hdrType cru do pipeline, para a tela poder dizer "HDR10" quando for HDR10 em
// vez de calar. "none" quando o fluxo e SDR ou ainda nao se sabe.
const char *video_hdr(void)       { return vidHdr; }
int  video_largura(void)          { return vidW; }
int  video_altura(void)           { return vidH; }

// Ver o bloco "TELA PRETA COM AUDIO TOCANDO" em video.h. Reaproveita a
// maquinaria de `recuperando`, que ja sabe recarregar a fonte e voltar para a
// posicao — reimplementar o recarregar aqui seria um segundo caminho para a
// mesma coisa, e o load precisa acontecer no fio principal.
int  video_pode_forcar_sdr(void) { return urlAtual[0] != 0; }
void video_forcar_sdr(void) {
  if (!urlAtual[0]) return;
  semDVForcado = 1;
  if (recuperando) return;            // um recarregar ja esta a caminho
  recuperando = 1;
  retomarEm = posSeg;
  printf("[video] pedido manual: recarregar sem HDR em %.1fs "
         "(hdr do pipeline=%s, fonte afirmava DV=%d)\n",
         posSeg, vidHdr, dvPedido);
  fflush(stdout);
}

void video_definir_dv(int dv) { dvPedido = dv ? 1 : 0; }

void video_escolher_audio(int i) {
  char b[192];
  const VideoFaixa *f = video_audio(i);
  if (!ligado || !midia[0] || !f) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"type\":\"audio\",\"index\":%d}",
           midia, f->numero);
  chamar("selectTrack", b, soLog);
  audioAtual = i;
}

void video_escolher_legenda(int i) {
  char b[192];
  if (!ligado || !midia[0]) return;
  if (i < 0) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"enable\":false}", midia);
    chamar("setSubtitleEnable", b, soLog);
    legAtual = -1;
    return;
  }
  { const VideoFaixa *f = video_legenda(i);
    if (!f) return;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"enable\":true}", midia);
    chamar("setSubtitleEnable", b, soLog);
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"type\":\"text\",\"index\":%d}",
             midia, f->numero);
    chamar("selectTrack", b, soLog);
    legAtual = i;
    legUrlAtual[0] = 0;   // voltou para uma faixa do arquivo
    aplicarEstilo(); }
}

// O estilo escolhido, guardado porque o PIPELINE NASCE A CADA LOAD e nao herda
// nada do video anterior. Reaplicado em loadCompleted e sempre que a legenda e
// (re)selecionada.
static VideoLegendaEstilo estilo = { 120, 0, 0, 3, 1, 0, 0, 0 };
static int temEstilo;

static void aplicarEstilo(void) {
  char b[256];
  if (!ligado || !midia[0] || !temEstilo) return;
  /* Embutida ainda pertence ao uMS: reduz o percentual aos cinco degraus. */
  { int p=estilo.tamanho, t=p<=70?0:p<=100?1:p<=130?2:p<=165?3:4;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"fontSize\":%d}", midia, t);
    chamar("setSubtitleFontSize", b, soLog); }
  { int c = estilo.cor;
    if (c < 0 || c >= VIDEO_LEG_NCORES) c = 0;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"charColor\":\"%s\"}",
             midia, VIDEO_LEG_CORES[c]);
    chamar("setSubtitleCharacterColor", b, soLog); }
  // Opacidade DA LETRA, separada da do fundo. O handler existe no firmware da
  // C9 (`setSubtitleCharacterOpacity`) e recebe 0..255. Tres niveis evitam uma
  // folha interminavel no controle remoto e mantem o texto legivel sobre video.
  { int op = estilo.opacidade == 3 ? 64 : estilo.opacidade == 2 ? 128
           : estilo.opacidade == 1 ? 191 : 255;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"charOpacity\":%d}", midia, op);
    chamar("setSubtitleCharacterOpacity", b, soLog); }
  // O fundo e a dupla cor+opacidade: sem declarar a cor, mudar so a opacidade
  // nao tem o que revelar.
  { int f = estilo.fundo; if (f < 0) f = 0; if (f > 4) f = 4;
    int op = f == 4 ? 255 : f * 64;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"bgColor\":\"black\"}", midia);
    chamar("setSubtitleBackgroundColor", b, soLog);
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"bgOpacity\":%d}", midia, op);
    chamar("setSubtitleBackgroundOpacity", b, soLog); }
  // A folha oferece 0..7; o uMS quer -3..4.
  { int p = estilo.posicao; if (p < 0) p = 0; if (p > 7) p = 7;
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"position\":%d}", midia, p - 3);
    chamar("setSubtitlePosition", b, soLog); }
  // VERIFICADO NA TELA: "uniform" desenha contorno em volta das letras.
  // "none" e o sem-borda. O retorno do uMS nao serve de prova aqui — ele
  // respondeu returnValue:true ate para valores inventados.
  { const char *ed = estilo.borda == 2 ? "dropShadow"
                   : (estilo.borda == 1 ? "uniform" : "none");
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"charEdgeType\":\"%s\"}",
             midia, ed);
    chamar("setSubtitleCharacterEdge", b, soLog); }
  if (estilo.atrasoMs) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"sync\":%d}", midia, estilo.atrasoMs);
    chamar("setSubtitleSync", b, soLog);
  }
}

void video_legenda_estilo(const VideoLegendaEstilo *e) {
  if (!e) return;
  estilo = *e;
  temEstilo = 1;
  aplicarEstilo();
}

void video_definir_mp4(int ehMp4) { fonteMp4 = ehMp4; }

void video_legenda_externa(const char *url) {
  char b[1400], reconhecivel[1024];
  if (!ligado || !midia[0] || !url || !*url) return;
  // O uMS baixa e sincroniza sozinho — o app so aponta. E o que permite usar
  // legenda do OpenSubtitles em arquivo que nao traz nenhuma embutida. Nesta
  // LG, URI /file/123 produziu errorCode 210 "Unknown Subtitle"; o MESMO
  // arquivo servido como /file/123.srt e reconhecido pelo formato.
  video_normalizar_url_legenda(url, reconhecivel, sizeof reconhecivel);
  snprintf(b, sizeof b,
           "{\"mediaId\":\"%s\",\"uri\":\"%s\",\"preferredEncodings\":[\"UTF-8\"]}",
           midia, reconhecivel);
  chamar("setSubtitleSource", b, soLog);
  snprintf(legUrlAtual, sizeof legUrlAtual, "%s", reconhecivel);
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"enable\":true}", midia);
  chamar("setSubtitleEnable", b, soLog);
  aplicarEstilo();
  printf("[video] legenda externa: %.80s\n", reconhecivel);
  fflush(stdout);
}

void video_encerrar(void) {
  if (!ligado) return;
  video_parar();
  if (acb) { acbDestruir(acb); acb = 0; }
  if (expWin[0] && sdlExpDestruir) { sdlExpDestruir(expWin); expWin[0] = 0; }
  if (laco) loopParar(laco);
  ligado = 0;
}
#endif

#endif  /* !__EMSCRIPTEN__ */
