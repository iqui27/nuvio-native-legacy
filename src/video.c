#include "app_id.h"
#include "video.h"
#include "webosver.h"
#include "audioinfo.h"
#include "esmaecer.h"
#include "video_escala.h"
#include "video_reconexao.h"
#include "video_seekretry.h"
#include "video_dvretry.h"
#include "video_dvaudio.h"
#include "idioma.h"
#include "linguas.h"
#include <SDL2/SDL.h>
#include "marco.h"
#include "mkv.h"
#include "capmkv.h"
#include "mkvass.h"
#include "js.h"
#include "lsregistro.h"
#include "rede.h"
#include "velocidade.h"
#include "cacheboost.h"
#include "dts/dts_playback.h"
#include "dts_tv.h"
#include "dts/dts_engine.h"
#include "ajustes.h"
#include "dts/dts_overlay.h"
#include "legenda.h"
#include "dados.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>
#include <unistd.h>

static DtsPlayback *dtsSessao;
static char dtsSaida[64];
// DOLBY VISION IN MKV (Settings > Playback, experimental). The TV's own URI
// player turns a Dolby Vision MKV into HDR10 (measured on an LG C9). The same
// Starfish BUFFERSTREAM path used for DTS conversion, fed by our demux with the
// file's DolbyHdrInfo, engages Dolby Vision there (DV badge, picture and E-AC-3
// 5.1 sound confirmed by the owner on the C9, webOS 4.10). The trigger is the
// file's own dvcC (MKV header probe), never the add-on's label.
// dvMkv*: what the header says, written by the probe thread and read here;
// plain ints, the same tolerance faixaLeg already has.
static volatile int dvSondado, dvMkvPerfil, dvMkvEl, dvMkvBl, dvMkvRpu;
// A sonda leu o comeco do arquivo e ele nao e Matroska (mkv_contentor_definitivo).
static volatile int mkvNaoMkv;
static char dvAudioCodec[16][16];   // Matroska CodecID of each audio track, file order
static char dvAudioIdioma[16][8];   // idioma/nome de cada faixa DO MKV (a lista da TV pode trazer so uma)
static char dvAudioNome[16][48];
static int  dvAudioCanais[16];       // Audio > Channels de cada faixa DO MKV (0 = nao informado)
static volatile int dvAudios;
// Faixa do MKV (ordinal entre as de audio) escolhida pelo dvPronto no lugar de uma
// que o caminho nao alimenta; -1 = nenhuma. O iniciarDts a entrega ao motor.
static int dvAudioMkvOrd = -1;
static int dvHabilitado, dtsModoDv, dvRecuoAviso;
// O player ainda nao pediu o ponto salvo ao player da TV (video_dv_segurar):
// a troca para o caminho espera, para nascer na posicao certa.
static int dvSegurar;
// A TELA DO DV (dvtela.h, video_dv_fase): por que o DV desta fonte nao vai
// acontecer (VIDEO_DV_NAO_*), a fonte nova, o demux ja abriu o arquivo, a faixa
// da TV que foi trocada, e o mudo do player da TV enquanto a tela cobre.
static int dvNao, dvSessaoN, dvFonteAberta, dvSilencio, dvSilenciado;
// O PONTO DE PARTIDA: onde a pessoa apertou Play (0, ou a retomada pedida com a
// tela do DV de pe). A tela so silencia o HDR10 da TV, que segue andando por
// baixo dela durante as novas tentativas da sonda (3/8/20 s): esse trecho
// ninguem viu. O caminho do DV parte daqui, e o HDR10 que fica volta para ca.
static double dvEntradaSeg;
// Menos que isto andado nao paga o seek (e o re-buffer) de volta.
#define DV_ENTRADA_FOLGA_S 2.0
static char dvAudioDe[16];
// The audio track a Dolby Vision session of this URL last played (file
// ordinal + language). Leaving the film and resuming the same source reopens
// it natively with the TV's default track; the path starts on this one.
static char dvMemUrl[1024], dvMemIdioma[8];
static int dvMemOrd = -1;
// Island notice for app.c: 1 = the source could not keep up, 2 = the path did
// not start. Consumed once.
int video_dv_recuo_consumir(void) { int v = dvRecuoAviso; dvRecuoAviso = 0; return v; }
int video_dv_ativo(void) { return dtsSessao && dtsModoDv; }
// This source already played (webOS): a later reload of the SAME source
// (Dolby Vision/DTS path handing back to the TV player, audio switch) is not
// a source that failed to open. Other targets keep 0.
static int fonteTocou;
int video_fonte_tocou(void) { return fonteTocou; }
static int dvLiberadoNestaTv(void);
static int dvPronto(void);
const char *video_dts_saida(void) { return dtsSaida; }

int video_faixa_dts(const VideoFaixa *f) {
  char s[96];
  size_t i;
  if (!f) return 0;
  snprintf(s, sizeof s, " %s %s ", f->codec, f->rotulo);
  for (i = 0; s[i]; i++) s[i] = (char)tolower((unsigned char)s[i]);
  if (!strstr(s, "dts") && !strstr(s, " dca")) return 0;
  if (strstr(s, "dts:x") || strstr(s, "dts-x") || strstr(s, "dtsx")) return 3;
  if (strstr(s, "dts-hd") || strstr(s, "dts hd") || strstr(s, "a_dts/") ||
      strstr(s, "dts-ma") || strstr(s, "dts ma")) return 2;
  return 1;
}
// So o ramo webOS escreve aqui (PREPARANDO, CONVERTIDO, FALHOU): o resto sai
// da faixa e do pipeline, igual em todos os alvos. No Mac, video_simular.
static int dtsEstado;
int video_dts_estado(void) {
  if (dtsEstado) return dtsEstado;
  if (!video_pronto() || !video_faixa_dts(video_audio(video_audio_atual()))) return VIDEO_DTS_NENHUM;
  return video_audio_nao_suportado() ? VIDEO_DTS_SEM_SOM : VIDEO_DTS_NATIVO;
}
int video_dts_legenda_desenhar(double seconds, int delay, float x, float y,
                              float w, float h, float alpha) {
  return dts_overlay_draw(dtsSessao, seconds - delay / 1000.0, x, y, w, h,
                          video_largura(), video_altura(), alpha);
}

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
// Mesmo limite de Stream.url: o pipeline recebe a URL original, e cortar a
// copia faria somente a sonda MKV/ASS falhar (inclusive apos tentar de novo).
static char  urlAtual[4096];
#if !defined(NV_TPK) && !defined(NV_ANDROID)   // .tpk e Android: video_url_atual vem do video_*.c do alvo
const char *video_url_atual(void) { return urlAtual; }
#endif
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
static int    pausaConfirmada;
// Sonda de MKV pedida, esperando o buffer. Ver a nota no sourceInfo.
static int    mkvPendente;
// 1 quando a fonte foi anunciada como MP4. Ver video_definir_mp4.
static int    fonteMp4;
static double seekAlvo;
static Uint32 seekEm;
// Seek diagnostics (#246): when the last "seek" went out, whether seekDone has
// come back, and the seekable/trickable flags the uMS reported. Log only.
static Uint32 seekEnvEm;
static NvSeekRetry seekRetry;
// Sonda do MKV que falhou: nova tentativa agendada (relogio) para a MESMA
// fonte. mkvRetrySessao/mkvRetryUrl amarram o agendamento a quem o pediu.
static NvDvSonda mkvRetry;
static Uint32 mkvRetryEm;
static unsigned mkvFioSessao;   // sessao em que o fio lerMkv atual nasceu
static unsigned mkvRetrySessao;
static char   mkvRetryUrl[4096];   // do tamanho de urlAtual: o strcmp nao pode falhar por corte
static Uint32 seekRetryEm;   // != 0: reenviar seekAlvo quando o relogio passar disto
static int    seekEnvAlvo, seekEnvAviso;
static int    srcSeekable = -1, srcTrickable = -1;
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

#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
// No Mac nao existe barramento nem plano de video. Os cotos deixam o resto do
// app compilar e rodar igual, so sem imagem em movimento. As capturas podem
// simular um video (video_simular, video.h); zerado e o coto mudo.
static VideoSimulacao SIM;
void video_simular(const VideoSimulacao *s) {
  if (s) SIM = *s; else memset(&SIM, 0, sizeof SIM);
  dtsEstado = SIM.dts;
}
int  video_iniciar(void) { return 0; }
int  video_iniciar_auto(void) { return 0; }
int  video_registro_negado(void) { return 0; }
int video_luna(const char *uri, const char *carga, void (*cb)(const char *, void *), void *ctx) {
  (void)uri; (void)carga; (void)cb; (void)ctx; return 0;
}
int  video_tocar(const char *u) { snprintf(urlAtual, sizeof urlAtual, "%s", u ? u : ""); return SIM.aceita; }
void video_bombear(void) {}
void video_parar(void) {}
void video_pausar(int p) { (void)p; }
int  video_pausa_confirmada(void) { return 0; }
void video_volume(int pct) { (void)pct; }
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
void video_recorte_reaplicar(void) {}
void video_escala_definir(int sw, int sh) { (void)sw; (void)sh; }
double video_pos(void) { return SIM.pos; }
double video_duracao(void) { return SIM.duracao; }
// Sem pipeline nao ha arquivo para ler capitulos: no Mac o pos-reproducao cai
// no plano B dos ultimos minutos, que e o mesmo caminho de um MKV sem
// capitulos. Melhor um stub honesto que um numero inventado.
double video_creditos(void) { return 0.0; }
double video_buffer_fim(void) { return SIM.bufferFim; }
void video_definir_dv(int dv) { (void)dv; }
int  video_tocando(void) { return SIM.tocando; }
int  video_iniciando(void) { return SIM.iniciando; }
int  video_pronto(void) { return SIM.pronto; }
// A tela do DV no Mac: o que a simulacao disser, e o registro das chamadas para
// os testes provarem o mudo (dv_tela) e a escolha do HDR10 (recusar).
static int simDvTela = -1, simDvRecusas;
void video_dv_fase(VideoDvFase *f) { if (f) *f = SIM.dvFase; }
int  video_dv_candidato(const char *url) { (void)url; return SIM.dvCandidato; }
void video_dv_tela(int cobrindo) { simDvTela = cobrindo ? 1 : 0; }
void video_dv_recusar(void) { simDvRecusas++; SIM.dvFase.recusa = VIDEO_DV_NAO_PESSOA; }
void video_dv_segurar(int segurar) { (void)segurar; }
int  video_simulado_dv_tela(void) { return simDvTela; }
int  video_simulado_dv_recusas(void) { return simDvRecusas; }
int  video_ativo(void) { return SIM.aceita; }
int  video_falhou(void) { return 0; }
const char *video_erro_texto(void) { return ""; }
int  video_decoder_anunciou(void) { return 1; }
int  video_audio_nao_suportado(void) { return 0; }
int  video_seek_desistiu(void) { return 0; }
int  video_terminou(void) { return 0; }
int  video_conflito_recurso(void) { return 0; }
unsigned video_bufferando_ms(void) { return SIM.bufferandoMs; }
int  video_n_audio(void) { return SIM.nAudio; }
int  video_n_legenda(void) { return SIM.nLeg; }
const VideoFaixa *video_audio(int i) { return i >= 0 && i < SIM.nAudio ? &SIM.audio[i] : 0; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < SIM.nLeg ? &SIM.leg[i] : 0; }
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
int  video_mkv_sondado(void) { return 2; }
void video_sondar_mkv_agora(void) {}
int  video_audio_atual(void) { return SIM.audioAtual; }
int  video_legenda_atual(void) { return SIM.nLeg ? SIM.legAtual : -1; }
void video_escolher_audio(int i) { (void)i; }
void video_escolher_legenda(int i) { (void)i; }
int  video_legenda_nativa(char *d, int t) { (void)t; if (d) d[0] = 0; return 0; }
void video_legenda_externa(const char *u) { (void)u; }
void video_legenda_estilo(const VideoLegendaEstilo *e) { (void)e; }
void video_definir_mp4(int m) { (void)m; }
void video_definir_reconexao(int sim) { (void)sim; }
int  video_reconectando(void) { return SIM.reconectando; }
// No Mac quem toca e o pipeline do sistema por outro caminho; os cabecalhos do
// addon so tem efeito no payload do load da webOS. Stub para o alvo linkar.
void video_definir_cabecalhos(const char *cabs) { (void)cabs; }
int  video_tem_atmos(void) { return SIM.atmos; }
int  video_tem_dolby_vision(void) { return SIM.dv; }
const char *video_hdr(void) { return SIM.hdr[0] ? SIM.hdr : "none"; }
int  video_largura(void) { return SIM.largura; }
int  video_altura(void) { return SIM.altura; }
#ifdef NV_SHOT_HOOKS
// Capturas: a folha de Fontes com o "Sem HDR" da LG (6 botoes no cabecalho).
static int shotSdr;
void video_shot_pode_forcar_sdr(int sim) { shotSdr = sim; }
int  video_pode_forcar_sdr(void) { return shotSdr; }
#else
int  video_pode_forcar_sdr(void) { return 0; }
#endif
// No Mac nao ha plano de video: 1 para que a tela de aspecto ofereca todos os
// modos ao desenvolver, que e o mesmo que a LG faz.
int  video_recorte_fonte(void) { return 1; }
void video_forcar_sdr(void) {}
void video_encerrar(void) {}
// Sem pipeline no Mac: a linha da velocidade aparece (para as capturas e para
// desenvolver a folha) e o pedido so fica guardado.
static int velMac = VEL_NORMAL;
int  video_velocidade_suportada(void) { return 1; }
void video_velocidade(int c) { velMac = c > 0 ? c : VEL_NORMAL; }
int  video_velocidade_atual(void) { return velMac; }
void video_velocidade_recusada(void) { velMac = VEL_NORMAL; }
// Capturas: o passthrough simulado (cacheboost_ganho_relato) esmaece a linha.
int  video_velocidade_bloqueada(void) { return cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH; }
// .tpk da Samsung: o player e o do host .NET, em video_tpk.c.
#elif !defined(NV_TPK) && !defined(NV_ANDROID)   // ramo luna (webOS): Android usa video_android.c
#include <dlfcn.h>

typedef struct LSHandle LSHandle;
typedef struct LSMessage LSMessage;
typedef int (*Filtro)(LSHandle *, LSMessage *, void *);

// LSError, na forma do luna-service2 (include/public/luna-service2/
// lunaservice.h). Nao ha header C no SDK, entao a struct e repetida aqui — e a
// folga de 256 bytes continua, para a lib nunca escrever alem do que alocamos.
//
// O CAMPO `message` E UM PONTEIRO. Ate a 1.4.1 o log imprimia ERRO+4 como
// texto, ou seja, os BYTES DO PONTEIRO: era o `msg=<lixo>` dos registros
// 1720-1774, e o lixo mudava a cada tentativa porque era o endereco da
// mensagem nova. E como ninguem chamava LSErrorFree, cada recusa deixava essa
// mensagem alocada.
typedef struct {
  int         code;
  char       *message;
  const char *file;
  int         line;
  const char *func;
  void       *padding;
  unsigned long magic;
} NvLsErro;
static union { NvLsErro e; char folga[256]; } ERRO_U;
#define ERRO ((void *)&ERRO_U)

static int  (*lsErroIniciar)(void *);   // LSErrorInit: poe o `magic`
static void (*lsErroLiberar)(void *);   // LSErrorFree: solta a `message`

// Antes de CADA chamada que recebe o LSError: solta a mensagem da anterior e
// reinicia a struct como a lib espera (LSErrorInit), em vez do memset puro.
static void erroLimpar(void) {
  if (ERRO_U.e.message && lsErroLiberar) lsErroLiberar(ERRO);
  memset(&ERRO_U, 0, sizeof ERRO_U);
  if (lsErroIniciar) lsErroIniciar(ERRO);
}

static void logErroLs(const char *onde) {
  const char *nome = lsreg_nome_codigo(ERRO_U.e.code);
  printf("[video] %s: lsError code=%d (%s) msg=%.200s\n", onde, ERRO_U.e.code,
         nome ? nome : "?", ERRO_U.e.message ? ERRO_U.e.message : "(vazio)");
  fflush(stdout);
}

// Contexto do processo, UMA vez, na primeira recusa. O papel LS2 do app casa
// pelo exeName (/var/palm/ls2-dev/roles/*/space.nuvio.native.legacy.json), e
// nas sessoes recusadas dos registros 1720-1774 faltava a linha
// "[HLunaServiceBridge::proc]" do inicio e o HOME era /tmp — sinal de que o app
// nao foi aberto pelo caminho de sempre. Hipotese, nao prova: esta linha e o
// que vai separar "o papel sumiu" de "o processo nao e quem o papel descreve".
static void logContextoLs(void) {
  char exe[256]; ssize_t n;
  const char *home = getenv("HOME"), *app = getenv("APPID");
  n = readlink("/proc/self/exe", exe, sizeof exe - 1);
  exe[n > 0 ? n : 0] = 0;
  printf("[video] contexto do registro: uid=%d exe=%s HOME=%s APPID=%s papel=%s\n",
         (int)getuid(), exe[0] ? exe : "?", home ? home : "-", app ? app : "-",
         access("/var/palm/ls2-dev/roles/pub/" NV_APP_ID ".json", F_OK) == 0
           ? "visivel" : "nao visivel daqui");
  fflush(stdout);
}

static LsRegEstado regEstado;
static int regNegado;          // PERMISSION nesta sessao
static int regAvisouDesistir;

static int         (*lsRegister)(const char *, LSHandle **, void *);
static int         (*lsUnregister)(LSHandle *, void *);
static int         (*lsAttach)(LSHandle *, void *, void *);
static int         (*lsCall)(LSHandle *, const char *, const char *, Filtro, void *, unsigned long *, void *);
static int (*lsCallUma)(LSHandle *, const char *, const char *, Filtro, void *, unsigned long *, void *);
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
static int  (*acbFinalizar)(long);
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
// Tamanho da superficie (drawable) em que o retangulo de DESTINO do plano de
// video e entendido; 1920x1080 ate o main dizer outra coisa. Ver video_escala.h.
static int       escW = 1920, escH = 1080;
// Destino em unidades de layout -> pixels da superficie. So o DESTINO escala: a
// fonte (recorte) e o quadro `org` sao coordenadas do quadro decodificado.
static SDL_Rect escDst(int x, int y, int w, int h) {
  NvRetInt r = { x, y, w, h };
  SDL_Rect o;
  r = nv_video_escalar(r, 1920, 1080, escW, escH);
  o.x = r.x; o.y = r.y; o.w = r.w; o.h = r.h;
  return o;
}
// Ultimo par fonte/destino aplicado pelo setDisplayWindow do uMS, para nao
// repetir a mesma chamada a cada quadro. fonX = -1 quer dizer "nada aplicado".
static int       fonX = -1, fonY, fonW, fonH, dstX = -1, dstY, dstW, dstH;
// Caracteristicas do fluxo, tiradas do evento videoInfo da assinatura do uMS.
// O ACB precisa delas para descrever o video ao pipeline de exibicao.
static int       vidW = 1920, vidH = 1080, vidTaxa = 30;
// Tamanho do quadro usado na ultima SDL_webOSSetExportedWindow (#158). Quando o
// videoInfo chega depois com outro tamanho, a janela e reaplicada com ele.
static int       expSrcW, expSrcH;
static void      expJanelaAplicar(void);
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
static int       tocando, pronto, ligado, falhou, terminou;
// VELOCIDADE (#202, video.h). velPedida e o que a pessoa quer; velEnviada o
// que este pipeline ja recebeu (nasce 100 a cada load). O setPlayRate do uMS e
// o mesmo que o app web do Nuvio manda ({mediaId, playRate, audioOutput:true});
// a resposta diz se a TV aceitou. Recusou uma vez, a linha some do app inteiro
// (a mesma TV nao passa a aceitar no titulo seguinte).
static volatile int velPedida = 100, velEnviada = 100, velRecusada;
// Ver video_erro_texto. Escrito no fio do LS2, lido pelo de desenho: e so
// texto curto e o pior caso de corrida e ler meia mensagem num quadro.
static char      erroTexto[96];
// errorCode 200 "Audio Codec Not Supported": o VIDEO segue tocando e so o
// audio morre. Ver o tratamento em lerEvento.
static int       audioNaoSup;
static int dtsHabilitado, dtsTentou, dtsRevisao, dtsFalhaLogada;
static int dtsLegAntes = -1, dtsLegCount;
static VideoFaixa dtsLegFaixa;
static int dtsNativePending;
static VideoFaixa dtsNativeTarget;
static char dtsLegUrlAntes[1024];
static char cabsHttp[512];
static int iniciarDts(int stream);
static void bombearDts(void);
static int dtsLiberadoNestaTv(void);
static void dtsMarcarAbertas(int n);
// RECONEXAO (video_reconexao.h). O erro chega no fio do LS2 e so ANOTA
// (reconErroPend); a decisao e o recarregar sao do video_bombear, no fio
// principal, como o `recuperando`.
static NvReconexao recon;
static int reconProxima, reconPermitida, reconIniciou;
static volatile int reconErroPend, reconErroRede;
// Escolhas da pessoa no instante da queda. Guardadas a parte porque cada
// tentativa passa por tocarInterno, que zera audioAtual/legAtual/legUrlAtual:
// uma segunda tentativa leria o estado do recarregar que nao abriu.
static int  reconAudio = -1, reconLeg = -1;
static char reconLegUrl[1024];

// PLAYER_TYPE_MSE. O ACB usa isto para saber que a fonte e um pipeline de
// midia e nao um sintonizador.
// Os enums do ACB nao tem header publico e chutar sai caro: com playerType 10 e
// sink 1 o aparelho registrou "playerType":"mse","vsmSinkType":"sub" — ou seja,
// o video foi para o plano SECUNDARIO (PIP) e a tela ficou preta. Os numeros
// ficam ajustaveis por /tmp/nuvio-acb justamente para conferir contra o que o
// ls-monitor mostra que o ACB resolveu, em vez de adivinhar de novo.
static int tipoJogador = 0, tipoSink = 0, estCarregado = 1, estTocando = 2;
static int tipoJogadorManual, acbTipoAtual = -1, acbTipoFalhou = -1;
#define NV_ACB_PLAYER_MSE 10
// hdrType do setMediaVideoData. O padrao e "none"; para testar Dolby Vision,
// escreva na SEGUNDA linha de /tmp/nuvio-acb: "dolby_vision" ou "hdr10".
// Afirmar DV sem o pipeline pedir e mentira — por isso NAO existe deteccao
// automatica: o sourceInfo do uMS nao distingue HEVC main-10 HDR10 de DV.
static char hdrTipo[24] = "none";

static void lerAjustesAcb(void) {
  FILE *f = fopen("/tmp/nuvio-acb", "r");
  if (!f) return;
  if (fscanf(f, "%d %d %d %d", &tipoJogador, &tipoSink, &estCarregado, &estTocando) > 0) {
    tipoJogadorManual = 1;
    printf("[video] acb ajustes: jogador=%d sink=%d carregado=%d tocando=%d\n",
           tipoJogador, tipoSink, estCarregado, estTocando);
  }
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
static int bindJoinable;
typedef struct { unsigned sessao; long acb; int tipo; char midia[64]; } AcbBind;
static int bindAtivo(void) { return __atomic_load_n(&bindVivo, __ATOMIC_ACQUIRE); }
static int acbConfigurarTipo(int bufferstream);
static void acbBindRecolher(void) {
  if (bindJoinable && !bindAtivo()) { pthread_join(fioBind, NULL); bindJoinable = 0; }
}
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
// Instante do bufferingStart que AINDA nao teve bufferingEnd; 0 = nao esta
// bufferizando. Escrito no fio do LS2 e lido no de desenho: um Uint32 que so
// alterna entre 0 e um carimbo, e o mesmo grau de descuido que posSeg e
// bufferSeg ja tem aqui — a leitura errada custa um quadro de decisao, nunca
// um estado invalido.
static Uint32 bufferandoDesde;

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
// A LINHA INTEIRA vai ao log uma vez (#158). O "pronto (webOS 5, ...)" do
// registro 6311 (LG C4 atualizada para webOS 11.2) era o chute "sem ACB => 5"
// e nao a versao: a TV do relato e as que tocam saiam iguais no log, e a
// unica diferenca conhecida — o firmware — nao aparecia em lugar nenhum.
// Quem decide a versao e o webosver.c (webos_release do nyx, depois o
// starfish-release): a TV de 2017 (webOS 3.9) so tem o nyx e antes caia no 4
// daqui. Desconhecida continua valendo o chute de sempre (5 com a libAcbAPI
// ausente, senao 4), guardado no primeiro uso.
static int webosMaior(void) {
  static int v, lido;
  if (v) return v;
  v = nv_webos_major();
  if (!lido) {
    lido = 1;
    char sf[128];
    nv_webos_starfish_linha(sf, sizeof sf);
    printf("[video] starfish-release: %s\n", sf[0] ? sf : "(sem arquivo)");
    printf("[video] webos major=%d fonte=%s\n", v, nv_webos_fonte());
    fflush(stdout);
  }
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

static int acbBindValido(const AcbBind *bind) {
  return bind->sessao == __atomic_load_n(&sessao, __ATOMIC_ACQUIRE) &&
    bind->acb == acb && !strcmp(bind->midia, midia);
}

static void *prenderPlano(void *u) {
  AcbBind *bind = u;
  const char *minha = bind->midia;
  const long meuAcb = bind->acb;
  // O bind e POR SESSAO: cada loadCompleted tem de religar o plano. Foi o bug
  // da "segunda reproducao preta com som" — o ACB continuava apontando para o
  // mediaId da sessao anterior, que o unload matou. A guarda de midia cobre a
  // troca de titulo no MEIO do bind (unload+load em menos de ~1,5s de pausas):
  // continuar descreveria ao tv.display um mediaId que ja morreu.
  printf("[video] bind inicio sessao=%u tipo=%d acb=%ld mediaId=%.63s\n",
         bind->sessao, bind->tipo, bind->acb, bind->midia);
  fflush(stdout);
  if (!acbBindValido(bind)) goto fora;
  long tarefa = 0;
  printf("[video] bind mediaId=%d\n", acbMidia(meuAcb, minha)); esperar(200);
  if (!acbBindValido(bind)) goto fora;
  printf("[video] bind loaded=%d\n", acbEstado(meuAcb, NV_ACB_FOREGROUND, estCarregado, &tarefa)); esperar(200);
  if (!acbBindValido(bind)) goto fora;
  printf("[video] connect=%d\n", acbConectar(meuAcb, tipoSink, &tarefa)); esperar(200);
  if (!acbBindValido(bind)) goto fora;
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
      if (!acbBindValido(bind)) goto fora;
      int rvd = acbVideoData(meuAcb, vd, &tarefa);
      // DOLBY VISION IN MKV: the file's own dvcC proved the layer and our
      // demux feeds it with DolbyHdrInfo, so the "decoder did not see DV"
      // guesses below do not apply. They read vidHdr, which this path resets
      // and the TV re-reports only after the bind; on the C9 the 700 ms
      // re-send as SDR/HDR10 won that race and the TV never entered its
      // Dolby Vision mode (no DV popup) although the pipeline said DolbyVision.
      const int dvNosso = dtsSessao && dtsModoDv && !strcmp(htipo, "DolbyVision");
      printf("[video] videoData=%d (hdrType=%s%s)\n", rvd, htipo, dvNosso ? ", Dolby Vision from the file" : "");
      if (dvNosso) { /* keep DolbyVision */ }
      else if (!strcmp(htipo, "DolbyVision") && !strcasecmp(vidHdr, "HDR10")) {
        // O ACB aceita DolbyVision e a TV acende o badge mesmo quando o
        // demuxer do MKV so entregou a camada HDR10 — nesse caso o plano fica
        // sem imagem. O retorno sincrono nao detecta isso. Depois de negociar
        // DV, voltar para o formato REAL do decoder recupera imagem + HDR10.
        esperar(700);
        if (!acbBindValido(bind)) goto fora;
        montarVideoData(vd, sizeof vd, minha, "HDR10", vuiPrim, vuiTrans, vuiMatriz);
        printf("[video] MKV DV entregue como HDR10; fallback real: %d\n",
               acbVideoData(meuAcb, vd, &tarefa));
      } else if (!strcmp(htipo, "DolbyVision") && !strcasecmp(vidHdr, "none")) {
        // Profile DV que o demuxer desta TV nao reconheceu nem como camada
        // HDR10. O badge liga, mas nao ha quadro DV; voltar a SDR garante
        // imagem. MP4 reconhecido vem como DolbyVision e nao entra aqui.
        esperar(700);
        if (!acbBindValido(bind)) goto fora;
        montarVideoData(vd, sizeof vd, minha, "none", 2, 2, 2);
        printf("[video] DV nao reconhecido pelo decoder; fallback SDR: %d\n",
               acbVideoData(meuAcb, vd, &tarefa));
      } else if (rvd != 1 && strcmp(htipo, "none")) {
        montarVideoData(vd, sizeof vd, minha, "none", 2, 2, 2);
        printf("[video] videoData recusou hdrType=%s, repetindo sem HDR: %d\n",
               htipo, acbVideoData(meuAcb, vd, &tarefa));
      }
      if (acbAudioData) {
        if (!acbBindValido(bind)) goto fora;
        printf("[video] audioData=%d\n", acbAudioData(meuAcb, ad, &tarefa));
      }
      fflush(stdout);
    }
    esperar(300);
    if (!acbBindValido(bind)) goto fora;
  }
  // COM RECORTE DE FONTE JA PEDIDO, prende o plano com o recorte — a janela
  // lisa aqui era o que desfazia o zoom do trailer (trailer.c pede o recorte
  // assim que o videoInfo chega, e este bind termina depois disso).
  if (!acbBindValido(bind)) goto fora;
  { SDL_Rect d = escDst(fonX >= 0 ? dstX : janX, fonX >= 0 ? dstY : janY,
                        fonX >= 0 ? dstW : janW, fonX >= 0 ? dstH : janH);
    if (fonX >= 0 && acbJanelaCustom)
      printf("[video] bind customWindow=%d\n", acbJanelaCustom(meuAcb, fonX, fonY, fonW, fonH, d.x, d.y, d.w, d.h,
                      (dstX == 0 && dstY == 0 && dstW == 1920 && dstH == 1080), &tarefa));
    else {
      d = escDst(janX, janY, janW, janH);
      printf("[video] bind window=%d\n", acbJanela(meuAcb, d.x, d.y, d.w, d.h,
                (janX == 0 && janY == 0 && janW == 1920 && janH == 1080), &tarefa));
    } }
  if (!acbBindValido(bind)) goto fora;
  printf("[video] bind playing=%d\n", acbEstado(meuAcb, NV_ACB_FOREGROUND, pausaPedida ? 3 : estTocando, &tarefa));
  printf("[video] plano preso em %d,%d %dx%d%s\n", janX, janY, janW, janH,
         fonX >= 0 ? " (com recorte)" : "");
  fflush(stdout);
fora:
  if (!acbBindValido(bind)) printf("[video] bind abortado: sessao ou midia mudou\n");
  fflush(stdout);
  free(bind);
  __atomic_store_n(&bindVivo, 0, __ATOMIC_RELEASE);
  return NULL;
}

// Ver o comentario em lerEvento (#111). So faz algo quando JA ha um recorte
// aplicado nesta midia; o caminho e o mesmo que o video_janela_fonte usaria.
static void recorteNoPrimeiroQuadro(void) {
  if (fonX < 0 || !ligado || !midia[0]) return;
  if (expWin[0] && sdlExpRecorte) {
    SDL_Rect org, src, dst;
    int ok;
    org.x = 0; org.y = 0; org.w = vidW > 0 ? vidW : 1920; org.h = vidH > 0 ? vidH : 1080;
    src.x = fonX; src.y = fonY; src.w = fonW; src.h = fonH;
    dst = escDst(dstX, dstY, dstW, dstH);
    ok = sdlExpRecorte(expWin, &org, &src, &dst);
    printf("[video] recorte reaplicado no primeiro quadro (janela exportada) -> %d\n", ok);
    fflush(stdout);
  } else if (acb && acbJanelaCustom) {
    printf("[video] recorte reaplicado no primeiro quadro (acb)\n");
    video_recorte_reaplicar();
  }
}

// Definida junto da tela do DV (video_dv_tela), chamada no load e no playing.
static void dvSilenciarTv(void);
static int eventoPayload(const char *p, unsigned minhaSessao) {
  if (minhaSessao != sessao) return 1;
  if (!p) return 1;
  // O payload do uMS pode vir com '\n' no fim (medido na C9 em 23/09: 4370
  // das 4391 linhas "[video] ev" seguidas de uma linha vazia). Corta so no log.
  // OS EVENTOS DE RELOGIO SAO AMOSTRADOS NO LOG. currentTime chega a cada
  // ~200 ms e bufferRange/streamingInfo/subtitlePosition quase tanto: nos logs
  // de campo da 1.4.3 eram 84% do registro, e os 200 KB do envio automatico
  // cobriam so ~4 minutos de filme — o que aconteceu antes do problema ja
  // tinha saido. Um de cada tipo a cada 30 s basta para saber que o video
  // andava; estado, erro, sourceInfo e o resto continuam inteiros.
  { static Uint32 ultimoRel[4];
    static const char *const RELOGIO[4] = {
      "\"currentTime\"", "\"bufferRange\"", "\"streamingInfo\"", "subtitlePosition"
    };
    size_t n = strlen(p);
    int k, logar = 1;
    for (k = 0; k < 4; k++)
      if (strstr(p, RELOGIO[k])) {
        Uint32 agora = SDL_GetTicks();
        if (ultimoRel[k] && agora - ultimoRel[k] < 30000) logar = 0;
        else ultimoRel[k] = agora;
        break;
      }
    if (strstr(p, "\"error") || strstr(p, "rror\"")) logar = 1;   // erro sempre sai
    while (n && (p[n - 1] == '\n' || p[n - 1] == '\r' || p[n - 1] == ' ')) n--;
    if (logar) { printf("[video] ev %.*s\n", (int)n, p); fflush(stdout); } }
  if (strstr(p, "seekDone") && seekEnvEm) {
    printf("[video] seek to %ds done in %ums\n", seekEnvAlvo, (unsigned)(SDL_GetTicks() - seekEnvEm));
    fflush(stdout);
    seekEnvEm = 0;
    nv_seek_ok(&seekRetry);
  }
  if (strstr(p, "sourceInfo")) {
    { const char *q = strstr(p, "\"seekable\":");
      srcSeekable = q ? (strncmp(q + 11, "true", 4) == 0) : -1;
      q = strstr(p, "\"trickable\":");
      srcTrickable = q ? (strncmp(q + 12, "true", 4) == 0) : -1; }
    const char *q;
    nAudio = nLeg = 0;
    vidAtmos = 0;
    // Empty strings/arrays mean no tracks. Stay inside each track array so
    // the following videoTrackInfo cannot be mistaken for audio.
    q = js_array(p, NULL, "audioTrackInfo");
    if (q) {
      const char *o = q;
      while (o && *o == '{' && nAudio < NV_FAIXA_MAX) {
        const char *fo = js_fim(o);
        if (!fo) break;
        VideoFaixa *f = &faixaAudio[nAudio];
        char cod[16] = "", ch[8] = "", imm[16] = "";
        memset(f, 0, sizeof *f);
        f->numero = nAudio;
        f->stream_index = -1; f->stream_id = -1;
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
        snprintf(f->codec, sizeof f->codec, "%s", cod);
        f->canais = (int)js_num(o, fo, "channels", 0);
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
        // #293: com o codec conhecido a linha de baixo e "E-AC-3 5.1 Atmos";
        // sem ele, o que sempre foi ("Atmos 5.1" / "5.1").
        { char ai[40];
          if (audioinfo_texto(cod, f->canais, !strcasecmp(imm, "ATMOS"), ai, sizeof ai))
            snprintf(f->rotulo, sizeof f->rotulo, "%s  \xc2\xb7  %s",
                     f->idioma[0] ? i18n(ling_nome(f->idioma)) : i18n("Faixa"), ai);
          else
            snprintf(f->rotulo, sizeof f->rotulo, "%s%s%s%s%s",
                     f->idioma[0] ? i18n(ling_nome(f->idioma)) : i18n("Faixa"),
                     imm[0] ? "  \xc2\xb7  " : (ch[0] ? "  \xc2\xb7  " : ""),
                     imm[0] ? "Atmos" : "",
                     (imm[0] && ch[0]) ? " " : "", ch); }
        nAudio++;
        o = js_prox(fo);
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

    q = js_array(p, NULL, "subtitleTrackInfo");
    if (q) {
      const char *o = q;
      while (o && *o == '{' && nLeg < NV_FAIXA_MAX) {
        const char *fo = js_fim(o);
        if (!fo) break;
        VideoFaixa *f = &faixaLeg[nLeg];
        memset(f, 0, sizeof *f);
        f->numero = (int)numeroDe(o, "\"trackNum\":");
        // Resolvido so quando o cabecalho do MKV chega (lerMkv). O memset
        // acima deixaria 0, que e um ordinal VALIDO — a primeira legenda.
        f->ordinalMkv = -1;
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
        o = js_prox(fo);
      }
    }
    printf("[video] faixas: audio=%d legenda=%d atmos=%d\n", nAudio, nLeg, vidAtmos);
    fflush(stdout);

    if (dtsNativePending) {
      int match = -1, matches = 0;
      for (int i = 0; i < nAudio; i++) {
        const VideoFaixa *f = &faixaAudio[i];
        if (!strcasecmp(f->codec, dtsNativeTarget.codec) &&
            (!dtsNativeTarget.idioma[0] || ling_casa(f->idioma, dtsNativeTarget.idioma)) &&
            (!dtsNativeTarget.canais || f->canais == dtsNativeTarget.canais)) {
          match = i; matches++;
        }
      }
      dtsNativePending = 0;
      if (matches == 1) { audioAoCarregar = match; video_escolher_audio(match); }
      else {
        falhou = 1;
        snprintf(erroTexto, sizeof erroTexto, "Cannot safely identify the selected native audio track");
        return 1;
      }
    }

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
    // le o elemento Tracks do Matroska; quando volta, casa pelo ordinal (mkv_casar_legendas) e
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
      // A MESMA descida traz os CAPITULOS, e e deles que sai o marcador de
      // creditos (creditosNomeado -> video_creditos). Prender a sonda ao idioma
      // da legenda deixava sem marcador todo MKV SEM faixa de legenda (nLeg==0,
      // `faltando` fica 0): sem marcador, o cartao de proximo episodio cai na
      // regra dos 2 min finais e sobe antes dos creditos — o #34, de volta como
      // #73. O Tizen sempre sondou todo MKV (video_tizen.c: mkvPendente =
      // !fonteMp4), e e por isso que la o cartao acerta.
      if (!fonteMp4 && !nv_url_e_mp4(urlAtual)) mkvPendente = 1;
      else if (faltando) marco("mkv: fonte e MP4, sonda dispensada"); }
  }

  if (strstr(p, "videoInfo")) {
    viuVideo = 1;   // fecha o prazo do recuo de DV
    double v;
    v = numeroDe(p, "\"width\":");      if (v > 0) vidW = (int)v;
    v = numeroDe(p, "\"height\":");     if (v > 0) vidH = (int)v;
    // O quadro real chegou diferente do que a janela exportada recebeu como
    // origem (#158): reaplica, sem recorte de fonte (esse tem caminho proprio).
    if (expWin[0] && fonX < 0 && expSrcW > 0 && (vidW != expSrcW || vidH != expSrcH))
      expJanelaAplicar();
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
      double alvo = posAoCarregar, entrada = dvEntradaSeg;
      posAoCarregar = 0.0;
      video_buscar(alvo);
      dvEntradaSeg = entrada;   // recarga interna: o ponto de partida nao muda
      marco("retomado apos queda do pipeline");
    }
    // O pipeline e novo: o estilo da legenda nao sobrevive ao load anterior.
    aplicarEstilo();
    pronto = 1;
    dvSilenciarTv();   // a tela do DV cobre este HDR10: sem som (dvtela.h)
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
    // So o PRIMEIRO de uma sequencia carimba. O uMS repete o bufferingStart
    // enquanto nao enche; reiniciar o relogio a cada repeticao daria um "faz
    // 0 ms que travou" eterno, que e exatamente o caso que se quer detectar.
    if (!bufferandoDesde) bufferandoDesde = SDL_GetTicks();
    snprintf(m, sizeof m, "buffering INICIO (buffer %+.1fs a frente)",
             bufferSeg - posSeg);
    marco(m);
  }
  if (strstr(p, "bufferingEnd")) {
    char m[64];
    bufferandoDesde = 0;
    snprintf(m, sizeof m, "buffering FIM (buffer %+.1fs a frente)",
             bufferSeg - posSeg);
    marco(m);
  }
  if (strstr(p, "playing")) {
    pausaConfirmada = 0;
    tocando = 1;
    dvSilenciarTv();
    if (acb && midia[0]) {
      long tarefa = 0;
      // COM RECORTE DE FONTE, reaplicar o recorte — e nao a janela lisa. A
      // janela lisa por cima do recorte devolvia o quadro inteiro: o trailer
      // (trailer.c) pedia o zoom antes do `playing`, esta linha desfazia, e a
      // tarja preta voltava (dono, 20/09/2026: "mas ta com a barra").
      if (fonX >= 0 && acbJanelaCustom) {
        SDL_Rect d = escDst(dstX, dstY, dstW, dstH);
        acbJanelaCustom(acb, fonX, fonY, fonW, fonH, d.x, d.y, d.w, d.h,
                        (dstX == 0 && dstY == 0 && dstW == 1920 && dstH == 1080), &tarefa);
        printf("[video] recorte reaplicado com o fluxo ja tocando\n");
      } else {
        SDL_Rect d = escDst(janX, janY, janW, janH);
        acbJanela(acb, d.x, d.y, d.w, d.h,
                  (janX == 0 && janY == 0 && janW == 1920 && janH == 1080), &tarefa);
        printf("[video] janela reaplicada com o fluxo ja tocando\n");
      }
      fflush(stdout);
    }
  }
  if (strstr(p, "paused")) {
    // So carimba quando NAO fomos nos que pausamos: pausa do dono e esperada,
    // pausa vinda do pipeline e o defeito.
    if (tocando && !pausaPedida) marco("pausado PELO PIPELINE");
    tocando = 0;
    pausaConfirmada = pausaPedida;
  }
  if (strstr(p, "endOfStream")) { tocando = 0; terminou = 1; marco("endOfStream"); }

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
    // Guardado para a tela (video_erro_texto): codigo e texto, sem o resto do
    // JSON. "40403 server error:40403" e o que o registro 4958 (#158) trouxe
    // no canal que o provedor dava como "Media Not Found".
    { double cod = numeroDe(p, "\"errorCode\":");
      const char *t = strstr(p, "errorText\":\"");
      char txt[72] = "";
      if (t) {
        const char *f;
        t += 12;
        f = strchr(t, '"');
        if (f && f - t < (int)sizeof txt) { memcpy(txt, t, (size_t)(f - t)); txt[f - t] = 0; }
      }
      snprintf(erroTexto, sizeof erroTexto, "%.0f %s", cod >= 0 ? cod : 0.0, txt); }
    // PIPELINE DESTRUIDO. Medido duas vezes na TV do dono: ~71 s depois de um
    // avanco, o uMS responde "com.webos.pipeline.<id> is not running" e o video
    // simplesmente para — o app nao fazia NADA, e era isso que ele descrevia
    // como "passei e nao continuou mais".
    //
    // Recarrega a mesma fonte e volta para onde estava. Nao e conserto da
    // CAUSA (o pipeline morre por algo entre o seek e a fonte do debrid, que
    // este lado nao enxerga), e sim de nao deixar o dono na tela parada.
    // AUDIO NAO SUPORTADO NAO E FONTE MORTA. MEDIDO no registro 1545 (webOS
    // 5, 1.4.0, MKV 2160p HDR10 com EAC3 6ch): o uMS manda errorCode 200
    // "Audio Codec Not Supported" e segue — loadCompleted, currentTime
    // andando ate 1946 s. Marcar `falhou` aqui fazia o watchdog de canal
    // pular a fonte e o trailer se dar por morto com a imagem tocando; e a
    // pessoa, sem som, nao recebia nenhuma explicacao. Agora e um aviso: o
    // player mostra que o audio desta fonte nao toca nesta TV.
    if (js_num(p, NULL, "errorCode", -1) == 200) {
      audioNaoSup = 1;
      marco("audio nao suportado pela TV (errorCode 200): video segue");
    } else
    if (strstr(p, "is not running") && urlAtual[0] && !recuperando) {
      recuperando = 1;
      retomarEm = posSeg;
      // GUARDA AS ESCOLHAS. A posicao sozinha nao basta: o pipeline novo nasce
      // com a faixa 0 e a legenda desligada.
      audioAoCarregar = audioAtual;
      legAoCarregar   = legAtual;
      snprintf(legUrlAoCarregar, sizeof legUrlAoCarregar, "%s", legUrlAtual);
      marco("pipeline morreu: recarregando");
    } else if (nv_seek_e_recusa(js_num(p, NULL, "errorCode", -1), strstr(p, "seek Failure") != NULL)) {
      // SEEK RECUSADO NAO E FONTE MORTA (#246, registro 54650): tenta de novo
      // com espera e, esgotado, segue tocando de onde esta.
      unsigned w = nv_seek_recusado(&seekRetry);
      if (w) {
        char m2[64];
        seekRetryEm = (SDL_GetTicks() + w) | 1;
        snprintf(m2, sizeof m2, "seek recusado: nova tentativa %d/%d em %u ms",
                 seekRetry.falhas, NV_SEEK_MAX, w);
        marco(m2);
      } else {
        seekRetryEm = 0; seekEnvEm = 0;
        marco("seek recusado: desistiu, segue tocando de onde esta");
      }
    } else if (!recuperando) {
      // Qualquer outro erro: pode ser a rede caindo com o episodio andando.
      // Quem decide entre reconectar e `falhou` e o video_bombear.
      reconErroRede = nv_recon_rede_ums(numeroDe(p, "\"errorCode\":"));
      reconErroPend = 1;
    } else falhou = 1;
  }
  { double v = numeroDe(p, "\"currentTime\":");
    if (v >= 0) {
      posSeg = v / 1000.0;
      // Primeiro quadro com avanco: o numero de inicio de verdade.
      if (cronPediu && !cronQuadro && posSeg > 0.0) {
        cronQuadro = 1;
        printf("[video] load->primeiro quadro %lums\n", msDesdePedido());
        // #111 (LG 50UA73006LA, webOS 25): "comeca com tela preta; trocar a
        // proporcao faz a imagem aparecer". Trocar a proporcao e a UNICA coisa
        // que reenvia o recorte, porque video_janela_fonte descarta o pedido
        // igual ao anterior. COMPATIVEL COM o firmware ignorar o recorte que
        // chegou antes do primeiro quadro — NAO PROVADO, nao ha log do relator
        // nem webOS 25 aqui. Reaplicar o ultimo recorte agora e barato (uma
        // chamada) e a linha de log diz, no proximo registro, se ele foi aceito.
        recorteNoPrimeiroQuadro();
        fflush(stdout);
      }
    } }
  { double v = numeroDe(p, "\"duration\":");
    if (v >= 0) durSeg = v / 1000.0; }
  return 1;
}

static int aoEvento(LSHandle *h, LSMessage *m, void *u) {
  (void)h;
  return eventoPayload(lsPayload(m), (unsigned)(uintptr_t)u);
}

static int soLog(LSHandle *h, LSMessage *m, void *u) {
  (void)h; (void)u;
  printf("[video] %s\n", lsPayload(m)); fflush(stdout);
  return 1;
}

// LSCall com LSError PROPRIO, na pilha: assim nenhuma chamada divide o ERRO
// global com o LSRegister nem com outra chamada em outro fio. A falha agora diz
// o motivo, e a mensagem e liberada (antes ficava alocada a cada recusa).
static int lsChamar(const char *uri, const char *carga, Filtro cb, void *ctx,
                    const char *rotulo) {
  union { NvLsErro e; char folga[256]; } er;
  unsigned long tok = 0;
  int ok;
  memset(&er, 0, sizeof er);
  if (lsErroIniciar) lsErroIniciar(&er);
  ok = lsCall(bus, uri, carga, cb, ctx, &tok, &er);
  if (!ok) {
    const char *nome = lsreg_nome_codigo(er.e.code);
    printf("[video] %s falhou: code=%d (%s) msg=%.200s\n", rotulo, er.e.code,
           nome ? nome : "?", er.e.message ? er.e.message : "(vazio)");
  }
  if (er.e.message && lsErroLiberar) lsErroLiberar(&er);
  return ok;
}

// CHAMADA LUNA PARA QUEM NAO E O PLAYER (ondever.c: listar apps, abrir a
// Netflix, abrir a loja). Mesmo barramento: o app roda como usuario comum no
// jail e nao pode executar luna-send (medido na C9, 02/10: "sh: luna-send:
// Permission denied"); o LS2 por dlopen e a porta que funciona. Uma resposta
// so (LSCallOneReply), entregue no fio do laco do glib.
static int iniciar(int automatico);
typedef struct { void (*cb)(const char *, void *); void *ctx; } LunaPedido;
static int lunaResposta(LSHandle *h, LSMessage *m, void *u) {
  LunaPedido *p = u;
  (void)h;
  if (p) { if (p->cb) p->cb(lsPayload(m), p->ctx); free(p); }
  return 1;
}
int video_luna(const char *uri, const char *carga, void (*cb)(const char *, void *), void *ctx) {
  union { NvLsErro e; char folga[256]; } er;
  unsigned long tok = 0;
  LunaPedido *p;
  int ok;
  if (!bus) iniciar(1);
  if (!bus || !lsCallUma) { printf("[video] luna %s: bus unavailable\n", uri); fflush(stdout); return 0; }
  p = malloc(sizeof *p);
  if (!p) return 0;
  p->cb = cb; p->ctx = ctx;
  memset(&er, 0, sizeof er);
  if (lsErroIniciar) lsErroIniciar(&er);
  ok = lsCallUma(bus, uri, carga, lunaResposta, p, &tok, &er);
  if (!ok) {
    printf("[video] luna %s failed: code=%d msg=%.200s\n", uri, er.e.code,
           er.e.message ? er.e.message : "(vazio)");
    fflush(stdout);
    free(p);
  }
  if (er.e.message && lsErroLiberar) lsErroLiberar(&er);
  return ok;
}

static void chamar(const char *metodo, const char *carga, Filtro cb) {
  char uri[128];
  snprintf(uri, sizeof uri, "luna://com.webos.media/%s", metodo);
  lsChamar(uri, carga, cb, NULL, metodo);
}

// Variante para callbacks que precisam saber a qual sessao pertencem. O
// contexto e um inteiro convertido em ponteiro; nao ha alocacao para vazar nem
// memoria cujo tempo de vida possa acabar antes da resposta assincrona.
static void chamarCtx(const char *metodo, const char *carga, Filtro cb,
                      void *ctx) {
  char uri[128];
  snprintf(uri, sizeof uri, "luna://com.webos.media/%s", metodo);
  lsChamar(uri, carga, cb, ctx, metodo);
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
  char uri[160], rot[160];
  snprintf(uri, sizeof uri, "luna://%s/%s", servico, metodo);
  snprintf(rot, sizeof rot, "%s/%s", servico, metodo);
  lsChamar(uri, carga, cb, NULL, rot);
}

static void protegerScreensaver(void);
static int modoLoad;   // video_modo_live_consumir do load em curso
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

  protegerScreensaver();
  snprintf(b, sizeof b, "{\"connectionId\":\"%s\"}", midia);
  chamar("notifyForeground", b, soLog);
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  chamarCtx("subscribe", b, aoEvento, (void *)(uintptr_t)minhaSessao);
  // MODO 1/2 (#158): sem selectTrack antes do sourceInfo — o pipeline de live
  // ainda nao sabe que faixas tem, e e a unica chamada que este app manda que
  // o navegador e o Kodi nao mandam no comeco.
  if (!modoLoad) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"type\":\"video\",\"index\":0}", midia);
    chamar("selectTrack", b, soLog);
  }
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  chamar("play", b, soLog);
  return 1;
}

static void *rodarLaco(void *u) { (void)u; loopRodar(laco); return NULL; }

// SCREENSAVER DA LG DURANTE O FILME. O buraco de video composto por GL nao conta
// como "video em tela cheia" para o tvpower, entao o timer de inatividade do
// remoto dispara o screensaver no meio do filme (relato C1, 1.7.0: a cada ~20
// min). API nao documentada: assina registerScreenSaverRequest; quando o
// screensaver vai ativar chega state "Active" + timestamp, e responder ack:false
// com o MESMO timestamp o cancela. Sem filme tocando respondemos ack:true para
// nao segurar o screensaver normal da TV.
static int protetorLigado;
static int aoPedidoScreensaver(LSHandle *h, LSMessage *m, void *u) {
  const char *p = lsPayload(m);
  char estado[24], ts[64], b[192];
  int segurar;
  (void)h; (void)u;
  if (!ligado || !bus || !p ||
      !js_texto_raiz(p, "state", estado, sizeof estado) || strcmp(estado, "Active")) return 1;
  // Preserva o token recebido, inclusive quando string, sem arredondar numeros
  // grandes ou responder com timestamp truncado.
  if (!js_bruto(p, NULL, "timestamp", ts, sizeof ts)) return 1;
  if (ts[0] != '"') {
    char *fim;
    if (!isdigit((unsigned char)ts[0]) && ts[0] != '-') return 1;
    strtod(ts, &fim);
    if (*fim) return 1;
  }
  segurar = midia[0] && tocando && !pausaPedida && !terminou && !falhou;
  // A tela de descanso do Nuvio (vitrine/relogio) e quem cuida da TV parada:
  // o screensaver da LG entraria por cima dela no mesmo minuto.
  if (!segurar && esmaecer_segura_protetor_tv()) segurar = 2;
  snprintf(b, sizeof b, "{\"clientName\":\"" NV_APP_ID "\",\"ack\":%s,\"timestamp\":%s}",
           segurar ? "false" : "true", ts);
  printf("[video] screensaver pedido: %s\n", segurar == 2 ? "seguro (tela de descanso do Nuvio)"
                                           : segurar ? "seguro (filme tocando)" : "liberado");
  fflush(stdout);
  lsChamar("luna://com.webos.service.tvpower/power/responseScreenSaverRequest",
           b, NULL, NULL, "responseScreenSaverRequest");
  return 1;
}
static void protegerScreensaver(void) {
  if (protetorLigado || !ligado || !bus) return;
  protetorLigado = lsChamar("luna://com.webos.service.tvpower/power/registerScreenSaverRequest",
           "{\"subscribe\":true,\"clientName\":\"" NV_APP_ID "\"}",
           aoPedidoScreensaver, NULL, "registerScreenSaverRequest");
}

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

/* URI sources use VIDEO (0); BUFFERSTREAM uses MSE (10), per the inspected
 * SDK and both native reference players. An explicit acb override wins.
 * Rebuild only on the main thread after the old bind has stopped, so no bind
 * can call a freed handle. Failed initialization is not retried every frame. */
static int acbConfigurarTipo(int bufferstream) {
  int alvo = tipoJogadorManual ? tipoJogador : (bufferstream ? NV_ACB_PLAYER_MSE : tipoJogador);
  if (!acbCriar || !acbIniciar) return 0;
  if (acb && acbTipoAtual == alvo) return 1;
  if (bindAtivo() || acbTipoFalhou == alvo) return 0;
  acbBindRecolher();
  if (acb) {
    if (acbFinalizar) printf("[video] acb finalize=%d\n", acbFinalizar(acb));
    acbDestruir(acb); acb = 0;
  }
  long novo = acbCriar();
  int ok = novo && acbIniciar(novo, alvo, NV_APP_ID, (void *)acbNotificou);
  printf("[video] acb tipo=%d init=%d\n", alvo, ok); fflush(stdout);
  if (!ok) {
    if (novo) { if (acbFinalizar) acbFinalizar(novo); acbDestruir(novo); }
    acbTipoAtual = -1; acbTipoFalhou = alvo; return 0;
  }
  acb = novo; acbTipoAtual = alvo; acbTipoFalhou = -1;
  printf("[video] acb sink=%d\n", acbSink(acb, tipoSink)); fflush(stdout);
  if (pronto && midia[0]) bindPendente = 1;
  return 1;
}

#define SIM(h, v, n) do { \
    *(void **)(&v) = dlsym(h, n); \
    if (!v) { printf("[video] falta %s\n", n); return 0; } \
  } while (0)

static int iniciar(int automatico);
int video_iniciar(void)      { return iniciar(0); }
int video_iniciar_auto(void) { return iniciar(1); }
int video_registro_negado(void) { return regNegado; }

static int iniciar(int automatico) {
  void *L, *G, *A;
  if (ligado) return 1;
  if (!lsreg_pode_tentar(&regEstado, SDL_GetTicks(), automatico)) {
    if (lsreg_desistiu(&regEstado) && !regAvisouDesistir) {
      regAvisouDesistir = 1;
      printf("[video] registro recusado %d vez(es) (%s): o trailer para de tentar "
             "nesta sessao; play ainda tenta\n", regEstado.falhas,
             lsreg_nome_codigo(regEstado.ultimoCodigo)
               ? lsreg_nome_codigo(regEstado.ultimoCodigo) : "?");
      fflush(stdout);
    }
    return 0;
  }
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
  // Soft: sem eles o erro so fica sem `magic` e sem liberar, como era antes.
  *(void **)(&lsErroIniciar) = dlsym(L, "LSErrorInit");
  *(void **)(&lsErroLiberar) = dlsym(L, "LSErrorFree");
  // Soft como acbJanelaCustom: so e usado na limpeza de um registro a meio
  // caminho; faltar numa lib nao pode custar o video inteiro.
  *(void **)(&lsUnregister) = dlsym(L, "LSUnregister");
  *(void **)(&lsCallUma) = dlsym(L, "LSCallOneReply");
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
  *(void **)(&acbFinalizar) = dlsym(A, "AcbAPI_finalize");
  SIM(A, acbConectar, "AcbAPI_connectDass");
  SIM(A, acbVideoData, "AcbAPI_setMediaVideoData");
  // NAO usa SIM, pela mesma razao de AcbAPI_setCustomDisplayWindow logo acima.
  // Os dumps de firmware da webosbrew mostram AcbAPI_setMediaAudioData ausente
  // na webOS 3.4.0 (W16N, 2016) e presente na 3.9.2 e na 4.10. Com SIM o
  // simbolo faltando derrubava video_iniciar() inteiro e a TV de 2016 ficava
  // sem NENHUM caminho de video por causa de um printf de diagnostico.
  *(void **)(&acbAudioData) = dlsym(A, "AcbAPI_setMediaAudioData");
  if (!acbAudioData) printf("[video] sem AcbAPI_setMediaAudioData; seguindo sem ele\n");
  }

  // O nome PRECISA casar com o padrao do papel LS2 do app
  // (allowedNames: "com.webos.media.client.*"). Qualquer outro nome e recusado
  // pelo hub e nada depois disso acontece.
  //
  // O nome fixo e o que o uMS espera no dia a dia. Depois de um deploy que mata
  // o processo sem video_encerrar(), o hub ainda segura esse nome por um tempo
  // e o novo arranque cai em "LSRegister recusado" para sempre no nome fixo.
  // O sufixo com o PID e o mesmo padrao permitido e libera o trailer/player sem
  // esperar o hub soltar o cadastro fantasma.
  //
  // O nome alternativo continua sendo tentado em qualquer recusa: nao se sabe
  // com que codigo o caso do deploy (o que motivou o sufixo) recusava, porque o
  // log nao lia o codigo direito. Quem limita o custo e a politica acima.
  bus = NULL;
  erroLimpar();
  if (!lsRegister(NV_LS_MEDIA_CLIENT, &bus, ERRO)) {
    char alt[160];
    logErroLs("LSRegister nome fixo recusado");
    snprintf(alt, sizeof alt, "%s.%d", NV_LS_MEDIA_CLIENT, (int)getpid());
    printf("[video] tentando %s\n", alt);
    bus = NULL;
    erroLimpar();
    if (!lsRegister(alt, &bus, ERRO)) {
      int cod = ERRO_U.e.code;
      logErroLs("LSRegister recusado");
      if (!regEstado.falhas) logContextoLs();
      lsreg_falhou(&regEstado, cod, SDL_GetTicks());
      if (cod == LSR_PERMISSION) regNegado = 1;
      erroLimpar();
      bus = NULL;
      return 0;
    }
  }
  if (!laco) laco = loopNovo(NULL, 0);
  erroLimpar();
  if (!lsAttach(bus, laco, ERRO)) {
    logErroLs("attach falhou");
    // Devolve o nome ao hub: sem isto o registro fica preso e a PROXIMA
    // tentativa de video_iniciar cai no "LSRegister recusado" para sempre.
    erroLimpar();
    if (lsUnregister) lsUnregister(bus, ERRO);
    erroLimpar();
    bus = NULL;
    lsreg_falhou(&regEstado, 0, SDL_GetTicks());
    return 0;
  }
  erroLimpar();
  lsreg_deu_certo(&regEstado);
  regNegado = 0;
  // Laco proprio: o LS2 exige um GMainLoop girando, e girar isso no laco de
  // desenho custaria quadros. As respostas chegam neste fio e so mexem em
  // variaveis simples, lidas pelo desenho sem trava.
  pthread_create(&fio, NULL, rodarLaco, NULL);

  if (A) {
    lerAjustesAcb();
    acbConfigurarTipo(0);
  }
  ligado = 1;
  printf("[video] pronto (webOS %d, acb=%ld, janela=%s)\n",
         webosMaior(), acb, expWin[0] ? expWin : "-");
  fflush(stdout);
  // TELA DE DESCANSO (esmaecer.h): o pedido do screensaver da TV passa a ser
  // respondido desde ja, e nao so no primeiro filme. Sem tela de descanso do
  // Nuvio a resposta continua ack:true (o da TV entra como sempre).
  protegerScreensaver();
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

/* LG can omit an unsupported audio track entirely from sourceInfo. Recover
 * its identity only when the file has a single audio track; a filtered list
 * with several tracks does not tell us which one native playback selected. */
static void completarAudioMkv(const MkvFaixa *fx, int n) {
  const MkvFaixa *audio = NULL;
  int count = 0;
  for (int i = 0; i < n; i++) {
    if (fx[i].tipo == 2) { audio = &fx[i]; count++; }
  }
  if (count != 1 || nAudio > 1 || (nAudio && faixaAudio[0].codec[0])) return;
  VideoFaixa *f = &faixaAudio[0];
  if (!nAudio) {
    memset(f, 0, sizeof *f);
    f->stream_index = f->stream_id = -1;
    f->ordinalMkv = 0;
    audioAtual = 0;
  }
  snprintf(f->codec, sizeof f->codec, "%s", !strncmp(audio->codec, "A_DTS", 5) ? "dts" : audio->codec);
  if (!f->idioma[0] && strcmp(audio->idioma, "und"))
    snprintf(f->idioma, sizeof f->idioma, "%s", audio->idioma);
  if (!f->canais) f->canais = audio->canais;
  snprintf(f->rotulo, sizeof f->rotulo, "%s", audio->nome[0] ? audio->nome : audio->codec);
  nAudio = 1;
  printf("[dts] MKV recovered single audio track: codec=%s channels=%d\n", f->codec, f->canais);
  fflush(stdout);
}

static void *lerMkv(void *arg) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  MkvCap   caps[MKV_MAX_CAPS];
  char url[sizeof urlAtual];
  int n, i, j, casou = 0, nCaps = 0;
  (void)arg;

  snprintf(url, sizeof url, "%s", urlAtual);

  // PRE-BUSCA (#92, v1.4.7): o inicio do arquivo ja foi lido ANTES do video
  // pelo mkvass. Serve aqui sem um pedido a mais pela rede — com o video
  // tocando e no mesmo CDN, que e justamente quando o do relato recusava.
  n = 0;
  { unsigned char *cab = NULL; long cabN = 0;
    if (mkvass_cabecalho(url, &cab, &cabN)) {
      n = mkv_faixas_do_trecho(cab, cabN, fx, MKV_MAX_FAIXAS, caps, MKV_MAX_CAPS, &nCaps);
      printf("[mkv] sonda pelo trecho da pre-busca (%ld bytes, sem rede): %d faixa(s)%s\n", cabN, n,
             n > 0 ? "" : " — Tracks nao coube, vai a rede");
      fflush(stdout);
      // Faixas na janela mas Chapters fora dela: UM Range pelo SeekHead.
      if (n > 0 && nCaps == 0) nCaps = mkv_capitulos_alem(url, cab, cabN, caps, MKV_MAX_CAPS, NULL);
      free(cab);
    } }
  if (n < 1) n = mkv_faixas_e_caps(url, fx, MKV_MAX_FAIXAS, caps, MKV_MAX_CAPS, &nCaps);
  if (nCaps > 0) {
    creditosNomeado = mkv_creditos_nomeados(caps, nCaps);
    creditosUltimo  = mkv_creditos_ultimo(caps, nCaps);
    printf("[mkv] %d capitulos; creditos nomeados em %.0fs, ultimo \"%s\" em %.0fs\n",
           nCaps, creditosNomeado, caps[nCaps - 1].nome, creditosUltimo);
    fflush(stdout);
    capmkv_aplicar(caps, nCaps);   // abertura/creditos/previa do arquivo -> modulo de intro
  }
  if (n < 1) {
    // Sem isto o unico sinal era uma linha de stdout, que na TV nao chega a
    // lugar nenhum — e a lista ficava em "Legenda 1, Legenda 2" sem ninguem
    // saber se o arquivo nao e MKV, se o Range falhou ou se o cabecalho passa
    // dos 2 MB que baixamos.
    marco("mkv: nenhuma faixa lida (nao e MKV, ou Range falhou)");
    // CHEGARAM BYTES E NAO SAO MATROSKA (MP4 "ftyp", MPEG-TS 0x47...): resposta
    // definitiva, nao falha de rede. C9, 2.0.3: um MP4 com URL sem extensao
    // levou as 3 novas tentativas (3/8/20 s) com a tela do DV de pe ~50 s.
    // Sem DV para esta fonte; dvPronto() devolve o HDR10 ao ponto de partida e
    // a tela sai no proximo quadro.
    { int cont = mkv_ultimo_contentor();
      if (mkv_contentor_definitivo(cont) && mkvFioSessao == __atomic_load_n(&sessao, __ATOMIC_ACQUIRE) &&
          !strcmp(url, urlAtual)) {
        char m[96];
        printf("[mkv] nao e Matroska (%s no comeco do arquivo): sem nova tentativa\n",
               mkv_contentor_nome(cont));
        fflush(stdout);
        snprintf(m, sizeof m, "mkv: nao e Matroska (%s), sem nova tentativa", mkv_contentor_nome(cont));
        marco(m);
        mkvNaoMkv = 1; dvSondado = 1;
        fioMkvVivo = 0; return NULL;
      } }
    // Falha de sonda nao e "nao e MKV" (C9: a rede da TV ficou ~10 s fora logo
    // apos o disparo). Se o DV ainda pode valer para ESTA fonte, agenda nova
    // tentativa em vez de dar dvSondado=1 e perder o DV pelo resto da sessao.
    if (dvHabilitado && !fonteMp4 && !nv_url_e_mp4(url) && !dtsSessao && !dtsTentou && !dvSondado &&
        mkvFioSessao == __atomic_load_n(&sessao, __ATOMIC_ACQUIRE) && !strcmp(url, urlAtual)) {
      int tent = 0;
      unsigned espera = nv_dvsonda_falhou(&mkvRetry, &tent);
      if (espera) {
        char m[96];
        snprintf(m, sizeof m, "mkv: sonda falhou: nova tentativa %d/%d em %u ms", tent, NV_DVSONDA_MAX, espera);
        printf("[mkv] sonda falhou: nova tentativa %d/%d em %u ms\n", tent, NV_DVSONDA_MAX, espera);
        fflush(stdout);
        marco(m);
        snprintf(mkvRetryUrl, sizeof mkvRetryUrl, "%s", url);
        mkvRetrySessao = mkvFioSessao;
        mkvRetryEm = SDL_GetTicks() + espera;
        if (!mkvRetryEm) mkvRetryEm = 1;
        fioMkvVivo = 0; return NULL;
      }
      printf("[mkv] sonda desistiu\n"); fflush(stdout);
      marco("mkv: sonda desistiu");
    }
    dvSondado = 1;
    fioMkvVivo = 0; return NULL;
  }

  completarAudioMkv(fx, n);
  { int na = 0, achouVideo = 0;
    for (j = 0; j < n; j++) {
      if (fx[j].tipo == 1 && !achouVideo) {
        achouVideo = 1;
        dvMkvEl = fx[j].dvEl; dvMkvBl = fx[j].dvBl; dvMkvRpu = fx[j].dvRpu;
        dvMkvPerfil = fx[j].dvPerfil;
        if (fx[j].dvPerfil)
          printf("[dv] file: profile=%d level=%d rpu=%d el=%d bl=%d compat=%d\n", fx[j].dvPerfil,
                 fx[j].dvNivel, fx[j].dvRpu, fx[j].dvEl, fx[j].dvBl, fx[j].dvCompat);
      } else if (fx[j].tipo == 2 && na < 16) {
        snprintf(dvAudioIdioma[na], sizeof dvAudioIdioma[0], "%s", fx[j].idioma);
        snprintf(dvAudioNome[na], sizeof dvAudioNome[0], "%s", fx[j].nome);
        dvAudioCanais[na] = fx[j].canais;
        snprintf(dvAudioCodec[na++], sizeof dvAudioCodec[0], "%s", fx[j].codec);
      }
    }
    dvAudios = na; dvSondado = 1; }

  // DIAGNOSTICO DO DV (203-dvrpu). "Sem [dv] file:" tinha tres causas possiveis
  // e o log nao separava: o arquivo nao tem dvcC; o Tracks nao coube; ou o DV
  // esta so EM BANDA (NAL 62 nos quadros, sem dvcC). Esta linha decide na TV.
  for (j = 0; j < n; j++) if (fx[j].tipo == 1) {
    const MkvDiag *d = mkv_diag();
    char bt[5] = "-";
    if (fx[j].bamN) { int k; for (k = 0; k < 4; k++) { char c = (char)(fx[j].bamTipo >> (24 - 8 * k)); bt[k] = c >= 32 && c < 127 ? c : '?'; } bt[4] = 0; }
    printf("[dv] sonda: trecho=%ld tracks=%s(%ld B) hvcC=%s(%ld B, nal62=%d) bam=%d(%s) dvcC=%d entrada=%s rpu_banda=%s quadros=%d rpu_tipo=%d vdr_rpu_profile=%d\n",
           d->lidos, !d->tracksAchado ? "ausente" : d->tracksInteiro ? "inteiro" : "cortado", d->tracksTam,
           fx[j].nalTam ? "sim" : "nao", fx[j].cpN, fx[j].hvccNal62, fx[j].bamN, bt, fx[j].dvPerfil != 0,
           fx[j].entradaInteira ? "inteira" : "parcial",
           d->rpu == 1 ? "sim" : d->rpu == 0 ? "nao" : d->blocoIni >= 0 ? "indeciso(quadro-cortado)" : "indeciso(sem-quadro)",
           d->quadros, d->rpuTipo, d->rpuPerfil);
    // A fonte AFIRMOU DV, o cabecalho nao tem dvcC e o 1o quadro passou do
    // trecho: UM Range com esse quadro (teto 4 MB), so neste caso raro — a nota
    // do topo de mkv.c mede o custo de ler mais com o video tocando.
    if (dvPedido && !fx[j].dvPerfil && fx[j].nalTam && d->rpu == -1 && d->blocoIni >= 0) {
      long long q = d->blocoFim - d->blocoIni;
      int r = mkv_rpu_alem(url, fx[j].nalTam, 4L * 1024 * 1024);
      printf("[dv] sonda: rpu_banda pelo 1o quadro (%lld bytes em %lld): %s rpu_tipo=%d vdr_rpu_profile=%d\n",
             q, d->blocoIni, r == 1 ? "sim" : r == 0 ? "nao" : "indeciso", d->rpuTipo, d->rpuPerfil);
    }
    fflush(stdout);
    break;
  }

  // SEM MUTEX, e de proposito: este arquivo nao tem um. faixaLeg ja e escrito
  // pelo fio de resposta do luna e lido pelo desenho sem trava nenhuma, e
  // introduzir uma trava so aqui daria falsa seguranca — protegeria a escrita
  // e nao a leitura. O dano possivel e um rotulo lido pela metade em UM quadro;
  // por isso cada campo e preenchido de uma vez, com um snprintf so, e o
  // rotulo (que e o que aparece) e escrito por ULTIMO, depois do idioma.
  //
  // O trackNum do sourceInfo da LG e o ORDINAL entre as legendas do arquivo,
  // NAO o TrackNumber do Matroska (#92, medido: ver mkv_casar_legendas). Esta
  // nota dizia o contrario, e o casamento por TrackNumber deslocava tudo: a
  // legenda N da TV recebia o codec e o idioma da TrackEntry de numero N — a
  // primeira nao casava com nada (sem selo ASS, desenhada pela TV), a segunda
  // virava o video, a terceira o audio, e dai em diante cada uma levava o que
  // era da legenda tres posicoes antes. O mkvass colhia essa mesma faixa
  // errada: o "Italian" do relato tocava as falas em ingles.
  { int tv[NV_FAIXA_MAX], idx[NV_FAIXA_MAX], modo, nt = nLeg;
    if (nt > NV_FAIXA_MAX) nt = NV_FAIXA_MAX;
    for (i = 0; i < nt; i++) tv[i] = faixaLeg[i].numero;
    modo = mkv_casar_legendas(fx, n, tv, nt, idx);
    // DIAGNOSTICO que faltou no #92: sem as duas listas lado a lado no log, o
    // deslocamento parecia "a TV desenha mal" e ninguem via que era o app.
    { int nArq = 0;
      for (j = 0; j < n; j++) {
        if (fx[j].tipo == 17) nArq++;
        printf("[mkv] faixa num=%d tipo=%d codec=%s idioma=%s nome=%s\n", fx[j].numero,
               fx[j].tipo, fx[j].codec, fx[j].idioma[0] ? fx[j].idioma : "-",
               fx[j].nome[0] ? fx[j].nome : "-");
      }
      // As CONTAGENS entram na linha: "nenhum" com tv != arquivo e a TV
      // escondendo (ou o trecho de 320 KB cortando) faixas — e nesse caso
      // NENHUMA faixa ASS vai ao overlay do app, todas ficam com a TV.
      printf("[mkv] legendas da TV x arquivo: modo=%s (tv=%d, arquivo=%d, lista da TV %s)\n",
             modo == MKV_CASA_ORDINAL ? "ordinal" : modo == MKV_CASA_NUMERO ? "trackNumber" : "nenhum",
             nt, nArq, nLeg > NV_FAIXA_MAX ? "CORTADA em NV_FAIXA_MAX" : "inteira");
      if (modo == MKV_CASA_NADA && nt > 0) {
        char m[96];
        snprintf(m, sizeof m, "mkv: legendas nao casaram (tv=%d, arquivo=%d): ASS fica com a TV", nt, nArq);
        marco(m);
      } }
    for (i = 0; i < nt; i++) {
      VideoFaixa *f = &faixaLeg[i];
      int jaTemIdioma = f->idioma[0] != 0, ordinal = -1, k;
      const MkvFaixa *m;
      j = idx[i];
      printf("[mkv]   tv[%d] trackNum=%d idiomaTV=%s -> %s%d %s %s\n", i, f->numero,
             f->idioma[0] ? f->idioma : "-", j >= 0 ? "num=" : "sem par", j >= 0 ? fx[j].numero : 0,
             j >= 0 ? fx[j].codec : "", j >= 0 && fx[j].idioma[0] ? fx[j].idioma : "");
      if (j < 0) continue;
      m = &fx[j];
      for (k = 0; k < j; k++) if (fx[k].tipo == 17) ordinal++;
      ordinal++;
      // O codec e o ordinal entram SEMPRE (a folha marca a faixa ASS pelo codec
      // e o mkvass colhe pelo ordinal); idioma e nome so quando o sourceInfo da
      // TV nao trouxe idioma.
      snprintf(f->codec, sizeof f->codec, "%s", m->codec);
      f->ordinalMkv = ordinal;
      { const char *peloNome = ling_do_nome(m->nome);
        int letreiro = ling_letreiro(m->nome, m->forcado), corrigiu = 0;
        char id[8];
        snprintf(id, sizeof id, "%s", jaTemIdioma ? f->idioma
                 : (m->idioma[0] && strcmp(m->idioma, "und")) ? m->idioma : "");
        // O NOME DIZ OUTRO IDIOMA: "Português" etiquetado eng e comum em
        // release remontado, e quem escolhe pela lista le o nome. So quando o
        // nome cita UM idioma com todas as letras (ling_do_nome); fora disso
        // fica a etiqueta do arquivo.
        if (peloNome && (!id[0] || !ling_casa(peloNome, id))) {
          printf("[mkv]   tv[%d]: nome \"%s\" diz %s, etiqueta diz %s: vale o nome\n", i,
                 m->nome, peloNome, id[0] ? id : "-");
          snprintf(id, sizeof id, "%s", peloNome);
          corrigiu = 1;
        }
        f->letreiro = letreiro;
        f->tipoLeg = ling_tipo_legenda(m->nome, m->forcado, m->sdh);
        if (jaTemIdioma && !corrigiu && !letreiro) continue;
        if (!jaTemIdioma && id[0]) casou++;
        snprintf(f->idioma, sizeof f->idioma, "%s", id);
        // LETREIROS: "Português  ·  Letreiros", seja o nome "Signs & Songs",
        // "Forced" ou a flag do arquivo — a pessoa precisa saber que essa nao
        // traduz o dialogo.
        if (letreiro)
          snprintf(f->rotulo, sizeof f->rotulo, "%s%s%s",
                   f->idioma[0] ? i18n(ling_nome(f->idioma)) : "",
                   f->idioma[0] ? "  \xc2\xb7  " : "", i18n(ling_tipo_legenda_rotulo(f->tipoLeg)));
        // O NOME da faixa ("SDH", "Full") e o que separa duas legendas do
        // MESMO idioma. Sem ele o dono ve "Portugues" tres vezes e escolhe no
        // escuro — e essa e justamente a lista que ele reclamou. Nome que e so
        // o idioma ("Português", uma palavra) nao repete o que ja esta ali.
        else if (m->nome[0] && !(peloNome && !strchr(m->nome, ' ')))
          snprintf(f->rotulo, sizeof f->rotulo, "%s%s%s",
                   f->idioma[0] ? i18n(ling_nome(f->idioma)) : "",
                   f->idioma[0] ? "  \xc2\xb7  " : "", m->nome);
        else if (f->idioma[0])
          snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(f->idioma)));
      }
    }
    fflush(stdout); }
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
static void pararSessao(void);

// Recarrega a fonte corrente e, quando o load terminar, devolve faixas e
// posicao (o loadCompleted aplica: faixas primeiro, posicao depois). Serve a
// morte de pipeline e a reconexao.
//
// As escolhas vao para os campos *AoCarregar DEPOIS do tocarInterno: ele passa
// por pararSessao, que os zera. Antes o `recuperando` gravava faixas e legenda
// externa no fio do LS2 e o tocarInterno as apagava em seguida, entao o video
// voltava sempre com a faixa 0 e sem legenda.
static int recarregarMesmaFonte(double alvo, int aud, int leg, const char *legUrl) {
  char lu[sizeof legUrlAoCarregar];
  snprintf(lu, sizeof lu, "%s", legUrl ? legUrl : "");
  if (!tocarInterno(urlAtual, !semDVForcado)) return 0;
  audioAoCarregar = aud;
  legAoCarregar   = leg;
  snprintf(legUrlAoCarregar, sizeof legUrlAoCarregar, "%s", lu);
  // O seek so vale depois do load; guardar o alvo e deixar o loadCompleted
  // aplica-lo evita mandar posicao para um pipeline que ainda nao existe.
  if (alvo > 1.0) posAoCarregar = alvo;
  return 1;
}

void video_definir_reconexao(int sim) { reconProxima = sim ? 1 : 0; }
int  video_reconectando(void) {
  return nv_recon_ativa(&recon) && (recon.pendente || !pronto) ? recon.tentativa : 0;
}

int video_tocar(const char *url) {
  dvRecuado = 0;
  dtsTentou = 0; dtsRevisao = 0; dtsSaida[0] = 0; dtsNativePending = 0; dtsEstado = 0;
  dtsHabilitado = dts_playback_enabled() && dtsLiberadoNestaTv();
  printf("[dts] fallback %s\n", dtsHabilitado ? "available" : "unavailable");
  dvSondado = dvMkvPerfil = dvAudios = 0; dtsModoDv = 0; dvAudioMkvOrd = -1; dvSegurar = 0;
  mkvNaoMkv = 0;
  dvNao = 0; dvSessaoN++; dvFonteAberta = 0; dvAudioDe[0] = 0;
  nv_dvsonda_zerar(&mkvRetry); mkvRetryEm = 0; dvEntradaSeg = 0.0;
  dvHabilitado = dvLiberadoNestaTv();
  fflush(stdout);
  // O modo vale para esta fonte e para os recarregar dela (tocarInterno).
  modoLoad = video_modo_live_consumir();
  nv_recon_zerar(&recon);
  reconPermitida = reconProxima; reconProxima = 0;
  reconIniciou = 0; reconErroPend = 0; fonteTocou = 0;
  reconAudio = reconLeg = -1; reconLegUrl[0] = 0;
  falhou = 0; terminou = 0; audioNaoSup = 0;
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
static void velBombear(void);
// Nova tentativa da sonda do MKV (ver mkvRetry). Roda no fio de desenho; so
// dispara com o prazo vencido, FORA de buffering, e se a fonte e a sessao ainda
// sao as de quem agendou — trocar de fonte ou sair cancela em silencio.
static void mkvRetryBombear(void) {
  if (!mkvRetryEm) return;
  if (mkvRetrySessao != __atomic_load_n(&sessao, __ATOMIC_ACQUIRE) || strcmp(mkvRetryUrl, urlAtual) ||
      dtsSessao || fonteMp4 || nv_url_e_mp4(urlAtual) || !dvHabilitado || dtsTentou) {
    mkvRetryEm = 0; return;
  }
  if (fioMkvVivo || !nv_dvsonda_pronta((int)(SDL_GetTicks() - mkvRetryEm) >= 0, bufferandoDesde != 0)) return;
  mkvRetryEm = 0;
  mkvPendente = 1;
  video_sondar_mkv_agora();
}
void video_bombear(void) {
  velBombear();
  mkvRetryBombear();
  acbBindRecolher();
  if (ligado) acbConfigurarTipo(dtsSessao != NULL);
  if (dtsSessao) bombearDts();
  else if (dvHabilitado && !dtsNativePending && !dtsTentou && !modoLoad && urlAtual[0] && !dvSegurar && dvPronto()) {
    dtsModoDv = 1;
    iniciarDts(-1);
  }
  else if (dtsHabilitado && !dtsNativePending && !dtsTentou && !modoLoad && urlAtual[0]) {
    // SO COM FAIXA DTS CONHECIDA (teste na C9, 06/10): o erro 200 tambem vem
    // de TrueHD e de faixa ainda sem codec; o fallback derrubava a reproducao
    // nativa, que na 2.0.0 seguia. Sem a lista de faixas, espera ela chegar.
    int knownDts = video_faixa_dts(video_audio(audioAtual)) > 0;
    int forcar = 0;
#ifdef NV_DTS_DEBUG
    // A C9 decodifica DTS sozinha: sem isto o caminho de conversao nunca roda
    // nela. `touch /tmp/nuvio-dts-forcar` converte toda faixa DTS escolhida.
    forcar = knownDts && access("/tmp/nuvio-dts-forcar", F_OK) == 0;
#endif
    // Toda LG de 2020 em diante (release >= 5, dts_tv.h) converte o DTS antes
    // de tocar, inclusive os modelos 2023/24 que tem DTS: nao da para detectar
    // o suporte com seguranca (o webOS 26 toca MUDO sem errorCode 200, #285).
    // So a C9 e anteriores (webOS < 5) usam o caminho nativo + errorCode 200.
    if ((audioNaoSup && knownDts) || forcar ||
        nv_dts_converter_ja(webosMaior(), knownDts, 1, dtsTentou)) {
      dtsEstado = VIDEO_DTS_PREPARANDO;
      iniciarDts(-1);
    }
  }
  // Sem conversao nesta TV (adapter ausente, app morreu com ela aberta, webOS
  // 3/4): se a TV nao tem DTS, o silencio e certo — avisa na ilha em vez de
  // deixar parecer que toca.
  else if (!dtsHabilitado && pronto && (!dtsEstado || dtsEstado == VIDEO_DTS_SEM_SOM))
    dtsEstado = nv_dts_sem_som(webosMaior(), video_faixa_dts(video_audio(audioAtual)) > 0, 0)
                ? VIDEO_DTS_SEM_SOM : 0;
  // O ACB demora cerca de 1,5 s para ligar uma sessao. Se o usuario sair e
  // reabrir nesse intervalo, o loadCompleted novo encontra bindVivo=1. Antes
  // ele simplesmente desistia para sempre; agora o pedido fica pendente.
  if (bindPendente && !bindAtivo() && acb && midia[0] && pronto && acbConfigurarTipo(dtsSessao != NULL)) {
    AcbBind *bind = malloc(sizeof *bind);
    if (bind) {
      bind->sessao = __atomic_load_n(&sessao, __ATOMIC_ACQUIRE);
      bind->acb = acb; bind->tipo = acbTipoAtual; snprintf(bind->midia, sizeof bind->midia, "%s", midia);
      bindPendente = 0;
      __atomic_store_n(&bindVivo, 1, __ATOMIC_RELEASE);
      if (pthread_create(&fioBind, NULL, prenderPlano, bind) == 0) bindJoinable = 1;
      else { free(bind); __atomic_store_n(&bindVivo, 0, __ATOMIC_RELEASE); bindPendente = 1; }
    }
  }
  // SONDA DE MKV so com folga de buffer. 20 s a frente e o sinal de que a
  // fonte esta entregando mais rapido do que o decoder consome, e portanto de
  // que ha banda sobrando para os 320 KB do cabecalho.
  //
  // Com o inicio do arquivo JA LIDO pela pre-busca do mkvass (#92, v1.4.7) a
  // sonda nao custa rede: dispara logo, e a legenda automatica decide no
  // sourceInfo em vez de esperar os 20 s de buffer.
  if (mkvPendente && !fioMkvVivo && urlAtual[0] &&
      ((audioNaoSup && !nAudio) || bufferSeg - posSeg >= 20.0 || mkvass_cabecalho(urlAtual, NULL, NULL)))
    video_sondar_mkv_agora();
  // Avanco pendente que ja repousou.
  if (seekEm && SDL_GetTicks() >= seekEm) {
    Uint32 q = seekEm; seekEm = 0; (void)q;
    seekAgora(seekAlvo);
  }
  if (seekRetryEm && (int)(SDL_GetTicks() - seekRetryEm) >= 0) {
    seekRetryEm = 0;
    if (!seekEm && pronto) seekAgora(seekAlvo);   // novo seek do dono tem prioridade
  }
  if (seekEnvEm && !seekEnvAviso && SDL_GetTicks() - seekEnvEm >= 3000) {
    seekEnvAviso = 1;
    printf("[video] seek to %ds: no seekDone after 3000 ms; pipeline at %dms, seekable=%d trickable=%d\n",
           seekEnvAlvo, (int)(posSeg * 1000.0), srcSeekable, srcTrickable);
    fflush(stdout);
  }
  // RECUPERACAO DO PIPELINE, no fio principal. Ver a nota em `recuperando`.
  if (recuperando) {
    double alvo = retomarEm;
    recuperando = 0;
    marco("recarregando a fonte");
    recarregarMesmaFonte(alvo, audioAoCarregar, legAoCarregar, legUrlAoCarregar);
  }
  // RECONEXAO: ver video_reconexao.h.
  if (pronto && posSeg > 0.5) reconIniciou = fonteTocou = 1;
  if (pronto) nv_recon_progresso(&recon, posSeg);
  if (reconErroPend) {
    int antes = recon.tentativa;
    reconErroPend = 0;
    if (reconPermitida && urlAtual[0] && (reconIniciou || recon.tentativa) &&
        nv_recon_erro(&recon, reconErroRede, SDL_GetTicks(), posSeg)) {
      if (!antes) {
        reconAudio = audioAtual; reconLeg = legAtual;
        snprintf(reconLegUrl, sizeof reconLegUrl, "%s", legUrlAtual);
      }
      if (recon.tentativa != antes) {
        char m[96];
        snprintf(m, sizeof m, "conexao caiu (%.40s): tentativa %d/%d, espera %us",
                 erroTexto, recon.tentativa, NV_RECON_MAX,
                 nv_recon_espera_ms(recon.tentativa) / 1000u);
        marco(m);
      }
      bufferandoDesde = 0;
    } else {
      if (recon.esgotou) marco("reconexao: desistiu depois de 3 tentativas");
      falhou = 1;
    }
  }
  if (nv_recon_vencida(&recon, SDL_GetTicks())) {
    char m[64];
    snprintf(m, sizeof m, "reconectando: alvo %.0fs, tentativa %d", recon.alvo, recon.tentativa);
    marco(m);
    // Load que nem sai daqui conta como a proxima tentativa.
    if (!recarregarMesmaFonte(recon.alvo, reconAudio, reconLeg, reconLegUrl)) {
      reconErroRede = 1; reconErroPend = 1;
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

// CABECALHOS EXIGIDOS PELO ADDON, ver video.h.
void video_definir_cabecalhos(const char *cabs) {
  snprintf(cabsHttp, sizeof cabsHttp, "%s", cabs ? cabs : "");
}

// Monta o "httpHeader" do payload do load a partir das linhas "Nome: valor".
//
// DE ONDE SAI ESTA CHAVE, porque ela nao esta em documentacao publica: foi
// lida no proprio aparelho. Em /usr/lib/libcbe.so, a tabela de opcoes de
// media_option de umediaclient_impl.cc traz "httpHeader" entre
// "usePipelinePreload" e "bufferControl" — irma de "useSeekableRanges" e
// "bufferControl", que sao exatamente as que este payload ja usa e que
// funcionam. E /usr/lib/libpf-1.0.so consome PF_HTTP_HEADER_REFERRER,
// PF_HTTP_HEADER_USER_AGENT e PF_HTTP_HEADER_COOKIES, que sao os tres campos
// abaixo.
//
// METODO QUE NAO SERVIU, registrado para ninguem repetir: procurar as chaves no
// binario do umediaserver da ZERO — e da zero tambem para "bufferControl" e
// "useSeekableRanges", que comprovadamente funcionam. Quem parseia e libcbe, e
// nao o umediaserver. E `luna-send` por ssh nao imprime resposta NENHUMA nesta
// TV, nem de getSystemTime, entao tambem nao serve de sonda.
static void montarHttpHeader(char *dst, unsigned tam) {
  char copia[512], ref[320], ua[320], ck[320];
  char *l, *ctx = NULL;
  int algum = 0;
  dst[0] = 0; ref[0] = 0; ua[0] = 0; ck[0] = 0;
  if (!cabsHttp[0]) return;
  snprintf(copia, sizeof copia, "%s", cabsHttp);
  for (l = strtok_r(copia, "\n", &ctx); l; l = strtok_r(NULL, "\n", &ctx)) {
    char *d = strchr(l, ':');
    char *v;
    if (!d) continue;
    *d = 0;
    v = d + 1;
    while (*v == ' ') v++;
    if (!strcasecmp(l, "referer") || !strcasecmp(l, "referrer"))
      snprintf(ref, sizeof ref, "%s", v);
    else if (!strcasecmp(l, "user-agent"))
      snprintf(ua, sizeof ua, "%s", v);
    else if (!strcasecmp(l, "cookie") || !strcasecmp(l, "cookies"))
      snprintf(ck, sizeof ck, "%s", v);
    // Origin e qualquer outro ficam de fora: libpf so consome estes tres, e
    // inventar campo no payload e o tipo de coisa que o uMS ignora calado.
  }
  if (!ref[0] && !ua[0] && !ck[0]) return;
  // A FORMA CERTA E O ANINHAMENTO, e ela foi MEDIDA na C9 em 18/09 — nao ha
  // documentacao publica disto.
  //
  // O que funciona e  option.transmission.httpHeader.referer . Testei seis
  // formas no aparelho, todas com o mesmo canal e o mesmo Referer, e so esta
  // faz o pipeline tocar:
  //
  //   {"httpHeader":{"referer":...}}                  -> errorCode 203
  //   {"httpHeader":"Referer: ..."}                   -> errorCode 203
  //   {"httpHeader":{"Referer":...}}  (maiusculo)     -> errorCode 203
  //   {"referer":...} solto em option                 -> errorCode 203
  //   {"option":{"httpHeader":{...}}}                 -> errorCode 203
  //   {"transmission":{"httpHeader":{"referer":...}}} -> errorCode 0, TOCA
  //
  // "errorCode 203" e "AV Type Not Founded": o pipeline buscou a pagina de 403
  // em vez do video. Com a forma certa o log mostra o buffer subindo
  // (endTime 0 -> 5 -> 11 -> 17 -> 23 s) e nenhuma linha de erro depois do load.
  //
  // O nome "httpHeader" saiu da tabela de media_option de libcbe.so e os campos
  // de PF_HTTP_HEADER_REFERRER/_USER_AGENT/_COOKIES de libpf-1.0.so. O que
  // faltava era o pai: "transmission", que esta na mesma tabela.
  //
  // MEDIDO: so o "referer" foi provado tocando. "userAgent" e "cookies" vao
  // junto porque libpf os consome com o mesmo prefixo, mas nao ha aqui um caso
  // que dependa deles — se um dia um addon exigir, confira antes de assumir.
  { int u = snprintf(dst, tam, "\"transmission\":{\"httpHeader\":{");
    if (ref[0]) { u += snprintf(dst + u, tam - u, "%s\"referer\":\"%s\"", algum++ ? "," : "", ref); }
    if (ua[0])  { u += snprintf(dst + u, tam - u, "%s\"userAgent\":\"%s\"", algum++ ? "," : "", ua); }
    if (ck[0])  { u += snprintf(dst + u, tam - u, "%s\"cookies\":\"%s\"", algum++ ? "," : "", ck); }
    snprintf(dst + u, tam - u, "}},");
  }
  // O cookie do stream e credencial de terceiro (CloudFront assinado): o log
  // vai para o D1, entao mostra so quais campos foram, nunca o valor.
  printf("[video] httpHeader no load: referer=%s userAgent=%s cookies=%s\n",
         ref[0] ? "sim" : "nao", ua[0] ? "sim" : "nao",
         ck[0] ? "sim (omitido)" : "nao");
  fflush(stdout);
}

static int tocarInterno(const char *url, int comDV) {
  // 8 KB E NAO 2: a URL de fonte vai ate 4096 (Stream.url) e a do Pluto TV
  // leva ~1,7 KB de query com JWT. Com 2048 o snprintf CORTAVA o JSON no meio
  // e o uMS respondia `Method "load" for category "/" was not handled` — que
  // se le como "o servico sumiu", nao como "mandei JSON pela metade". MEDIDO
  // na C9 em 18/09 com dois canais do Pluto. A conferencia de tamanho logo
  // abaixo transforma o proximo estouro em erro com nome.
  char carga[8192];
  unsigned minhaSessao;
  if (!ligado && !video_iniciar()) return 0;
  pararSessao();
  minhaSessao = __atomic_add_fetch(&sessao, 1, __ATOMIC_ACQ_REL);
  viuVideo = 0;
  // O retangulo aplicado e da SESSAO: sem zerar, uma sessao nova que calcule o
  // mesmo rect cairia no "ja e esse" e nunca chegaria a mandar nada ao plano.
  // (semUms NAO zera: se esta TV nao entende o recorte de fonte, nao passa a
  // entender no titulo seguinte, e insistir so arrisca a imagem de novo.)
  fonX = -1; dstX = dstY = dstW = dstH = -1;
  posSeg = durSeg = bufferSeg = 0; tocando = pronto = 0; midia[0] = 0;
  velEnviada = 100;   // pipeline novo nasce em 1x; video_bombear reaplica
  bufferandoDesde = 0;
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
  { char hh[768];
    montarHttpHeader(hh, sizeof hh);
  if (modoLoad == 2) {
    // PAYLOAD ENXUTO DE LIVE (#158, modo 2): o que o load de um <video> HLS
    // costuma mandar — transporte dito pelo nome, sem useSeekableRanges nem
    // bufferControl (que sao de VOD). Experimental, atras de Ajustes.
    int hls = strstr(url, ".m3u8") != NULL;
    snprintf(carga, sizeof carga,
        "{\"payload\":{\"option\":{"
        "\"appId\":\"" NV_APP_ID "\","
        "%s%s"
        "\"mediaTransportType\":\"%s\","
        "\"windowId\":\"%s\"}},"
        "\"uri\":\"%s\",\"type\":\"media\"}",
        dolby, hh, hls ? "HLS" : "URI",
        expWin[0] ? expWin : "window_id_dummy", url);
  } else
  snprintf(carga, sizeof carga,
      "{\"payload\":{\"option\":{\"useSeekableRanges\":true,"
      "\"appId\":\"" NV_APP_ID "\","
      "%s%s"
      "\"bufferControl\":{\"userBufferCtrl\":false},"
      "\"windowId\":\"%s\"}},"
      "\"uri\":\"%s\",\"type\":\"media\"}",
      dolby, hh,
      // No caminho do ACB o id e um marcador qualquer (so nao pode ser vazio,
      // ver acima); no da webOS 5 ele e o endereco REAL do plano exportado e um
      // valor errado aqui deixa o video sem para onde ir.
      expWin[0] ? expWin : "window_id_dummy",
      url);
    if (strlen(carga) >= sizeof carga - 1) {
      printf("[video] carga do load nao coube em %zu bytes (URL de %zu): recusando\n",
             sizeof carga, strlen(url));
      fflush(stdout);
      falhou = 1;
      return 0;
    } }
  // A URL INTEIRA NAO VAI AO LOG (#158). Ate a 1.5.3 esta linha imprimia a
  // URL crua, e a do Xtream leva usuario e senha no caminho
  // (<servidor>/live/U/P/<id>.m3u8): os registros 4958 e 6311 chegaram ao D1
  // com a credencial do provedor da pessoa dentro. O host e a extensao bastam
  // para a triagem ("qual servidor", "HLS ou TS").
  { char pub[160], ext[12] = "";
    const char *q = strchr(url, '?'), *b, *d;
    size_t n = q ? (size_t)(q - url) : strlen(url);
    for (b = url + n; b > url && b[-1] != '/'; b--) {}
    for (d = url + n; d > b && d[-1] != '.'; d--) {}
    if (d > b && (size_t)(url + n - d) < sizeof ext - 1) {
      size_t k = (size_t)(url + n - d), i;
      int ok = 1;
      for (i = 0; i < k; i++) if (!isalnum((unsigned char)d[i])) ok = 0;
      if (ok && k) { ext[0] = '.'; memcpy(ext + 1, d, k); ext[k + 1] = 0; }
    }
    printf("[video] URL: %s%s%s\n", rede_url_publica(url, pub, sizeof pub),
           ext[0] ? " " : "", ext);
    fflush(stdout); }
  // JANELA EXPORTADA ANTES DO LOAD (#158) — DEFENSIVO, NAO PROVADO.
  //
  // O que o registro mostra (6311/6314, LG C4 em webOS 11.2, e 6362/6372,
  // outra TV com PowerVR, outro provedor): o load volta errorCode 0, o
  // resourceInfo reserva VDEC e ADEC, o bufferRange sobe (9 s numa, 36 s na
  // outra) — e NUNCA chega videoInfo, sourceInfo nem loadCompleted, em
  // NENHUMA fonte (Xtream, flixnest, sslip), ate o "Playing error" (100). Nas
  // outras TVs com o mesmo caminho (c7ca4398, 7005) o videoInfo chega em ~3 s.
  // Os dados chegam; o decoder nao se anuncia.
  //
  // O que muda aqui: ate agora o SetExportedWindow so era chamado DEPOIS do
  // load (video_janela desiste sem mediaId, e o preview ja tinha gravado o
  // retangulo antes — o "sem repetir" engolia a chamada seguinte). Na
  // primeira reproducao de uma sessao a janela exportada ia ao pipeline sem
  // nunca ter recebido quadro nem destino. A ordem do guia de midia do
  // webosbrew e: cria a janela, SetExportedWindow, e so entao o load com o
  // windowId. Se isto e o que o webOS 11 passou a exigir, nao sei: e a unica
  // diferenca de protocolo que este lado controla, e custa uma chamada que o
  // fluxo ja fazia (depois). A prova e o proximo registro dessa TV mostrar
  // videoInfo.
  if (expWin[0]) expJanelaAplicar();
  if (modoLoad) { printf("[video] modo do load: %d\n", modoLoad); fflush(stdout); }
  msDoLoad = agoraMs();
  chamarCtx("load", carga, aoCarregar, (void *)(uintptr_t)minhaSessao);
  return 1;
}

// Sair do video (ou trocar de fonte) cancela a queda em curso: sem isto o
// video_bombear recarregaria a fonte velha depois de a tela ja ter fechado.
void video_parar(void) {
  nv_recon_zerar(&recon);
  reconErroPend = 0;
  pararSessao();
}

static void pararSessao(void) {
  char b[128];
  int tinhaDts = dtsSessao != NULL;
  DtsPlayback *antigoDts = dtsSessao;
  dtsSessao = NULL;
  if (antigoDts) dts_playback_close(antigoDts);
  dtsSaida[0] = 0;

  // Invalida tambem a sessao que ainda esta esperando o retorno de load. Esse
  // era o caso abrir -> sair -> abrir que travava: nao havia mediaId para
  // descarregar, mas o callback antigo continuava vivo e contaminava o novo.
  __atomic_add_fetch(&sessao, 1, __ATOMIC_ACQ_REL);
  bindPendente = 0;
  recuperando = 0; retomarEm = posAoCarregar = 0.0;
  audioAoCarregar = legAoCarregar = -1;
  legUrlAoCarregar[0] = 0;
  pausaPedida = 0; seekEm = 0; mkvPendente = 0;
  nv_seek_zerar(&seekRetry); seekRetryEm = 0;
  seekEnvEm = 0;   // seek sem seekDone morre com a sessao (iniciarDts le os tres)
  dvSilenciado = 0;   // o mudo e por mediaId: o pipeline novo nasce com som
  mkvRetryEm = 0;   // sonda reagendada morre com a sessao (o contador zera em video_tocar)
  pausaConfirmada = 0;
  if (ligado && midia[0] && !tinhaDts) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
    chamar("unload", b, soLog);
  }
  if (tinhaDts) {
    dts_overlay_draw(NULL, 0, 0, 0, 0, 0, 0, 0, 0);
    dtsMarcarAbertas(0);   // fechou sem o app morrer
    if (dtsEstado != VIDEO_DTS_FALHOU) dtsEstado = 0;
  }
  midia[0] = 0; tocando = pronto = 0; falhou = 0; audioNaoSup = 0;
  erroTexto[0] = 0;
}


// CONVERSAO DE DTS SO NO webOS 5+ (PR #259). Na LG C9 (webOS 4.10.2), com a
// conversao forcada, o app morreu as duas vezes no mesmo ponto, logo depois de
// "native-play-accepted": SIGSEGV em pthread_setname_np com pthread_t nulo,
// chamado por KADP_OSA_CreateThread <- KADP_AUDIO_OpenMaster <- lxao_open_renderer
// <- gst_lx_audio_render_event, numa thread do GStreamer que o pipeline
// BUFFERSTREAM da Starfish roda DENTRO do nosso processo (o caminho normal do
// uMS roda o audio no processo da LG). Ou seja, o pthread_create da LG falhou
// e ela nao conferiu. Nosso codigo nao esta na pilha e a causa do pthread_create
// falhar NAO esta provada (palpite: thread de tempo real negada ao app na
// jaula). Ate haver prova numa TV webOS 3/4 a conversao fica desligada nelas,
// e a TV faz o que a 2.0.0 fazia: video sem o audio que ela recusa.
// `touch /tmp/nuvio-dts-liberar` religa para teste.
//
// E EM QUALQUER VERSAO: dts-conversao.txt conta as conversoes abertas que nao
// fecharam. Duas seguidas = o app morreu duas vezes com a conversao aberta, e
// ela se desliga nesta TV. Uma so pode ser a TV desligada no meio do filme.
#define DTS_MARCA_ARQ "dts-conversao.txt"
static int dtsAbertasSemFechar(void) {
  char *t = dados_ler(DTS_MARCA_ARQ);
  int n = t ? atoi(t) : 0;
  free(t);
  return n;
}
static void dtsMarcarAbertas(int n) {
  char b[16];
  if (!n && !dtsAbertasSemFechar()) return;
  snprintf(b, sizeof b, "%d\n", n);
  dados_gravar(DTS_MARCA_ARQ, b);
}
static int dtsLiberadoNestaTv(void) {
  static int v = -1;
  int maior, abertas;
  if (v >= 0) return v;
  if (access("/tmp/nuvio-dts-liberar", F_OK) == 0) {
    printf("[dts] conversao liberada por /tmp/nuvio-dts-liberar\n"); fflush(stdout);
    return v = 1;
  }
  maior = webosMaior();
  abertas = dtsAbertasSemFechar();
  if (maior < 5) {
    printf("[dts] conversao desligada no webOS %d (crash no audio da LG na C9 4.10)\n", maior);
    v = 0;
  } else if (abertas >= 2) {
    printf("[dts] conversao desligada: o app morreu %d vezes com ela aberta\n", abertas);
    v = 0;
  } else v = 1;
  fflush(stdout);
  return v;
}
// Dolby Vision in MKV: the Settings option, then what this TV can do. webOS 3
// is out (no proof of the BUFFERSTREAM audio path there); two app deaths with
// the path open turn it off on this TV, the same counter as DTS conversion.
// webOS 3 fica fora; desconhecida chega aqui ja como o chute do webosMaior().
static int dvVersaoLiberada(int maior) { return maior >= 4; }
static int dvLiberadoNestaTv(void) {
  int forcado = 0;
#ifdef NV_DTS_DEBUG
  forcado = access("/tmp/nuvio-dv-forcar", F_OK) == 0;
#endif
  if (!dts_playback_enabled() || (!ajustes_dv_mkv() && !forcado)) return 0;
  if (!dvVersaoLiberada(webosMaior())) {
    printf("[dv] Dolby Vision in MKV unavailable on webOS %d\n", webosMaior()); fflush(stdout);
    return 0;
  }
  if (dtsAbertasSemFechar() >= 2) {
    printf("[dv] Dolby Vision in MKV off: the app died twice with it open\n"); fflush(stdout);
    return 0;
  }
  return 1;
}
// O HDR10 fica (recusa com a tela de pe): volta ao ponto de partida antes de a
// tela sair. Sem a tela a pessoa viu o HDR10 andar, e ele segue de onde esta.
static void dvVoltarEntrada(void) {
  char m[80];
  if (!dvSilencio || dtsSessao || !ligado || !midia[0] || posSeg - dvEntradaSeg <= DV_ENTRADA_FOLGA_S) return;
  printf("[dv] HDR10 andou %.1f s coberto pela tela: volta ao ponto de partida %.1fs\n",
         posSeg - dvEntradaSeg, dvEntradaSeg);
  fflush(stdout);
  snprintf(m, sizeof m, "dv: HDR10 volta ao ponto de partida %ds", (int)dvEntradaSeg);
  marco(m);
  video_buscar(dvEntradaSeg);
}
// The file decides: single-layer profile 5 or 8 (base layer + RPU, no
// enhancement layer) and a selected audio track the path can feed (E-AC-3 and
// AC-3 pass through, DTS is converted). Anything else stays with the TV player.
static int dvPronto(void) {
  if (fonteMp4) { dvHabilitado = 0; dvNao = VIDEO_DV_NAO_SEM_DV; return 0; }
  if (!dvSondado) {
    if (mkvPendente && !fioMkvVivo) video_sondar_mkv_agora();
    return 0;
  }
  if (!(dvMkvPerfil == 5 || dvMkvPerfil == 8) || dvMkvEl || !dvMkvBl || !dvMkvRpu) {
    if (dvMkvPerfil) {
      printf("[dv] profile %d (el=%d): stays on the TV player\n", dvMkvPerfil, dvMkvEl);
      fflush(stdout);
    }
    // Sem perfil: o arquivo nao tem dvcC, ou a sonda desistiu de ler o
    // cabecalho (tres falhas) — sao notas diferentes para a pessoa.
    dvNao = dvMkvPerfil ? VIDEO_DV_NAO_PERFIL : mkvNaoMkv ? VIDEO_DV_NAO_CONTENTOR
          : mkvRetry.desistiu ? VIDEO_DV_NAO_SONDA : VIDEO_DV_NAO_SEM_DV;
    dvHabilitado = 0; dvVoltarEntrada(); return 0;
  }
  if (dvMemOrd >= 0 && dvMemOrd < nAudio && dvMemOrd != audioAtual && !strcmp(urlAtual, dvMemUrl) &&
      (!dvMemIdioma[0] || !faixaAudio[dvMemOrd].idioma[0] || ling_casa(faixaAudio[dvMemOrd].idioma, dvMemIdioma))) {
    printf("[dv] audio track %d restored for this source\n", dvMemOrd + 1); fflush(stdout);
    video_escolher_audio(dvMemOrd);
  }
  if (dvAudioMkvOrd >= 0) return 1;   // ja decidido; iniciarDts pode ter adiado (bind ativo)
  // audioAtual e o indice da lista da TV; dvAudio* sao as faixas do MKV. A TV
  // pode filtrar a lista: localiza a faixa dela no MKV antes de olhar o codec.
  const VideoFaixa *tv = audioAtual >= 0 && audioAtual < nAudio ? &faixaAudio[audioAtual] : NULL;
  int m = nv_dvaudio_localizar(nAudio, dvAudios, audioAtual, tv ? tv->idioma : "", tv ? tv->codec : "",
                               tv ? tv->canais : 0, (const char (*)[16])dvAudioCodec,
                               (const char (*)[8])dvAudioIdioma, dvAudioCanais, ling_casa, audioinfo_codec);
  if (m < 0 && audioAtual >= 0 && dvAudios > 0) {
    // Sem par seguro: nao troca. O motor confere idioma e codec por conta
    // propria (dts_playback.c) e, sem faixa que alimente, volta ao HDR10.
    printf("[dv] audio da TV %d (%s %s) sem par seguro nas %d faixas do MKV: nao troca\n", audioAtual + 1,
           tv ? tv->idioma : "-", tv ? tv->codec : "-", dvAudios);
    fflush(stdout);
  }
  if (m >= 0) {
    const char *c = dvAudioCodec[m];
    if (!nv_dvaudio_alimentavel(c)) {
      // TrueHD (e afins) nao entra no caminho, mas o remux costuma trazer um
      // E-AC-3 do mesmo idioma. A escolha sai da lista do PROPRIO MKV (a da TV
      // tem uma faixa so) e vai direto ao motor: iniciarDts a passa como ordinal
      // do arquivo. NAO ha selectTrack no player da TV, que nem conhece a faixa.
      // O dvMem acima ja foi aplicado, entao uma escolha lembrada nao e
      // sobrescrita; a escolha da pessoa DEPOIS (video_escolher_audio com a
      // sessao DV ativa) continua entregando o resto ao player da TV.
      int k = nv_dvaudio_decidir(nAudio, dvAudios, m, (const char (*)[16])dvAudioCodec,
                                 (const char (*)[8])dvAudioIdioma, (const char (*)[48])dvAudioNome,
                                 ling_casa);
      if (k >= 0) {
        printf("[dv] audio %s -> faixa %d (%s %s) para manter Dolby Vision\n", c, k + 1,
               dvAudioCodec[k], dvAudioNome[k]);
        fflush(stdout);
        dvAudioMkvOrd = k;
        snprintf(dvAudioDe, sizeof dvAudioDe, "%s", c);
        return 1;
      }
      printf("[dv] audio %s cannot be fed: stays on the TV player\n", c); fflush(stdout);
      dvNao = VIDEO_DV_NAO_AUDIO;
      dvHabilitado = 0; dvVoltarEntrada(); return 0;
    }
  }
  return 1;
}
// A CONVERSAO FALHOU: volta ao player nativo na mesma posicao, como se o DTS
// nunca tivesse sido tentado (video sem o audio recusado, o comportamento da
// 2.0.0). dtsTentou continua 1, entao nao ha segunda tentativa nesta fonte.
// Antes o player fechava (log da C9: "playback failed" e logo "saida").
static void dtsVoltarNativo(const char *motivo) {
  char lu[sizeof legUrlAtual];
  dtsEstado = VIDEO_DTS_FALHOU;
  printf("[dts] conversao falhou (%s): volta ao player nativo em %.1fs\n", motivo, posSeg);
  fflush(stdout);
  snprintf(lu, sizeof lu, "%s", legUrlAtual);
  if (!recarregarMesmaFonte(posSeg, -1, -1, lu)) falhou = 1;
}
// O caminho do DV nem chegou a abrir (plano MSE, conversor ausente): a mesma
// nota do caminho que nao abriu, e a tela sai em vez de esperar para sempre.
static void dvCaminhoNaoAbriu(void) {
  if (!dtsModoDv) return;
  dvRecuoAviso = 2; dvNao = VIDEO_DV_NAO_FALHOU;
  dtsModoDv = 0; dvHabilitado = 0;
}
static int iniciarDts(int stream) {
  /* Retry from the next pump before unloading or spawning a native worker.
   * The old bind must finish before its ACB handle can become MSE. */
  if (bindAtivo()) return 0;
  DtsTrack selected;
  const VideoFaixa *a = video_audio(audioAtual);
  // A POSICAO DE PARTIDA. Um seek pedido ao player da TV e ainda nao feito (o
  // ponto salvo da retomada, mandado 350 ms depois do pedido, ou ja enviado sem
  // seekDone) e a posicao certa; posSeg nao, porque o currentTime da TV volta a
  // escreve-lo com a posicao antiga ate o seek acontecer. O pararSessao abaixo
  // zera o seekEm, e o caminho nascia no inicio do arquivo em vez do ponto salvo.
  // Com a tela do DV de pe, o HDR10 andou sem ninguem ver (novas tentativas da
  // sonda): o caminho parte do ponto de partida, nao de onde a TV chegou.
  double alvo = dtsModoDv && dvSilencio ? dvEntradaSeg
              : (seekEm || seekEnvEm || seekRetryEm) ? seekAlvo : posSeg;
  int paused = pausaPedida, ordinal = audioAtual, count = nAudio, trocada = 0;
  memset(&selected, 0, sizeof selected);
  if (a) {
    snprintf(selected.language, sizeof selected.language, "%s", a->idioma);
    snprintf(selected.codec, sizeof selected.codec, "%s", a->codec);
    selected.channels = a->canais;
  }
  // DV com a faixa trocada pelo dvPronto: ordinal, contagem e idioma sao os do
  // MKV (dts_playback.c prepare(): o k-esimo audio do arquivo, conferido por
  // idioma e por codec que o caminho alimenta). Canais ficam 0 = nao conferir.
  if (dtsModoDv && dvAudioMkvOrd >= 0 && dvAudioMkvOrd < dvAudios) {
    memset(&selected, 0, sizeof selected);
    snprintf(selected.language, sizeof selected.language, "%s", dvAudioIdioma[dvAudioMkvOrd]);
    snprintf(selected.codec, sizeof selected.codec, "%s",
             !strcmp(dvAudioCodec[dvAudioMkvOrd], "A_EAC3") ? "eac3" : "ac3");
    ordinal = dvAudioMkvOrd; count = dvAudios; trocada = 1;
  }
  dtsLegAntes = legAtual; dtsLegCount = nLeg;
  memset(&dtsLegFaixa, 0, sizeof dtsLegFaixa);
  if (video_legenda(legAtual)) dtsLegFaixa = *video_legenda(legAtual);
  snprintf(dtsLegUrlAntes, sizeof dtsLegUrlAntes, "%s", legUrlAtual);
  dtsTentou = 1;
  dvFonteAberta = 0;
  pararSessao();
  posSeg = alvo; pausaPedida = paused;
  dtsRevisao = 0; dtsFalhaLogada = 0;
  if (acbCriar && !acbConfigurarTipo(1)) {
    marco("DTS startup failed: MSE video plane initialization");
    dvCaminhoNaoAbriu();
    dtsVoltarNativo("video plane");
    return 0;
  }
  dtsMarcarAbertas(dtsAbertasSemFechar() + 1);
  dts_engine_set_ac3(ajustes_dts_ac3());
  dts_playback_next_dv(dtsModoDv);
  dtsSessao = dts_playback_start(urlAtual, cabsHttp, stream, alvo,
                               paused, expWin, 0, (a || trocada) ? &selected : NULL, ordinal, count);
  if (!dtsSessao) {
    dtsMarcarAbertas(0);
    marco("DTS startup failed: software playback unavailable");
    dvCaminhoNaoAbriu();
    dtsVoltarNativo("converter unavailable");
    return 0;
  }
  if (dtsModoDv) {
    marco("Dolby Vision: MKV path requested");
    return 1;
  }
  dtsEstado = VIDEO_DTS_PREPARANDO;
  snprintf(dtsSaida, sizeof dtsSaida, "DTS: preparing audio");
  marco("DTS: local software conversion requested");
  return 1;
}
/* Adapter diagnostics contain only stage names and bounded technical details;
 * they must not carry the source URL, authentication headers or packet addresses. */
static int logDtsStage(const char *event) {
  char name[48] = "", detail[160] = "";
  if (!js_tem(event, NULL, "dtsStage")) return 0;
  js_texto(event, NULL, "name", name, sizeof name);
  js_texto(event, NULL, "detail", detail, sizeof detail);
  for (char *p = name; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
  for (char *p = detail; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
  printf("[dts] stage=%s %s\n", name, detail);
  fflush(stdout);
  if (!strcmp(name, "source-opened")) dvFonteAberta = 1;
  return 1;
}
/* Native load callbacks can precede publication of the prepared metadata.
 * Restore readiness from the worker snapshot after each metadata reset: an
 * already consumed loadCompleted event will not be emitted a second time. */
static void sincronizarPlanoDts(const DtsPlaybackStatus *st) {
  int novaMidia = st->media_id[0] && strcmp(midia, st->media_id);
  int estavaPronto = pronto;
  pronto = st->prepared && st->native_loaded;
  if (!pronto) bindPendente = 0;
  if (st->prepared && novaMidia) {
    snprintf(midia, sizeof midia, "%s", st->media_id);
    printf("[dts] native media ID ready\n"); fflush(stdout);
    if (expWin[0]) expJanelaAplicar();
  }
  if (pronto && acb && midia[0] && (novaMidia || !estavaPronto)) {
    bindPendente = 1;
    printf("[dts] native video plane bind pending\n"); fflush(stdout);
  }
}
static void bombearDts(void) {
  DtsPlaybackStatus st;
  char event[2048];
  int i;
  memset(&st, 0, sizeof st);
  dts_playback_status(dtsSessao, &st);
  if (st.failed) {
    /* A fast startup failure can happen before the main thread sees its stages. */
    while (dts_playback_event(dtsSessao, event, sizeof event)) logDtsStage(event);
    if (!dtsFalhaLogada) {
      printf("[dts] playback failed: %.159s\n", st.error); fflush(stdout);
      dtsFalhaLogada = 1;
    }
    dtsSaida[0] = 0;
    if (dtsModoDv) {
      dvRecuoAviso = !strcmp(st.error, DTS_PLAYBACK_STARVED) ? 1 : 2;
      dvNao = dvRecuoAviso == 1 ? VIDEO_DV_NAO_LENTO : VIDEO_DV_NAO_FALHOU;
      dtsModoDv = 0; dvHabilitado = 0;
      dtsVoltarNativo("dolby vision");
      dtsEstado = 0;
      return;
    }
    dtsVoltarNativo("converter");
    return;
  }
  /* Load may synchronously queue videoInfo/playing before the worker publishes
   * its source metadata. Process metadata first so it cannot erase those
   * native observations on the following pump. Startup failures drain above. */
  if (!st.prepared) return;
  if (st.prepared && st.revision != dtsRevisao) {
    int initial = !dtsRevisao;
    int selectedSub = initial ? -1 : (video_legenda(legAtual) ? video_legenda(legAtual)->stream_index : -1);
    dtsRevisao = st.revision;
    pronto = 0; tocando = 0; bindPendente = 0;
    if (!st.media_id[0]) midia[0] = 0;
    printf("[dts] prepared revision=%d video=%s %dx%d audio=%s channels=%d stream=%d\n",
           st.revision, st.info.video_codec, st.info.width, st.info.height,
           st.info.audio_codec, st.info.channels, st.info.audio_stream);
    fflush(stdout);
    durSeg = st.info.duration_ns / 1e9;
    vidW = st.info.width; vidH = st.info.height;
    vidTaxa = st.info.fps_den ? st.info.fps_num / st.info.fps_den : 30;
    vuiPrim = st.info.color_primaries; vuiTrans = st.info.color_transfer;
    vuiMatriz = st.info.color_matrix; vidDV = st.info.dovi_profile != 0;
    /* Source metadata configures playback; only native videoInfo proves HDR.
     * A reload of the same session (seek, track switch) keeps what the TV
     * already reported: paused after a seek, videoInfo only comes back on
     * play, and the Dolby Vision badge used to vanish until then. */
    if (initial) { snprintf(vidHdr, sizeof vidHdr, "%s", "none"); vidAtmos = 0; }
    viuVideo = 0;
    nAudio = nLeg = 0;
    for (i = 0; i < st.info.n_tracks; i++) {
      const DtsTrack *t = &st.info.tracks[i];
      VideoFaixa *f;
      if (t->kind == DTS_AUDIO && nAudio < NV_FAIXA_MAX) {
        f = &faixaAudio[nAudio];
        if (t->stream_index == st.info.audio_stream) audioAtual = nAudio;
        memset(f, 0, sizeof *f); f->numero = nAudio++;
      } else if (t->kind == DTS_SUBTITLE && nLeg < NV_FAIXA_MAX) {
        f = &faixaLeg[nLeg];
        memset(f, 0, sizeof *f); f->numero = nLeg; f->ordinalMkv = nLeg++;
      } else continue;
      f->stream_index = t->stream_index; f->stream_id = t->stream_id;
      f->canais = t->channels; f->letreiro = t->forced;
      f->tipoLeg = t->forced ? LING_LEG_FORCADA : LING_LEG_COMUM;
      snprintf(f->codec, sizeof f->codec, "%s", t->codec);
      /* ASS consumers expect the Matroska CodecID, not an FFmpeg name. */
      if (!strcmp(t->codec, "ass") || !strcmp(t->codec, "ssa"))
        snprintf(f->codec, sizeof f->codec, "S_TEXT/ASS");
      snprintf(f->idioma, sizeof f->idioma, "%.7s", t->language);
      snprintf(f->rotulo, sizeof f->rotulo, "%.26s · %.12s", t->language[0] ?
               i18n(ling_nome(t->language)) : i18n("Faixa"), t->codec);
    }
    legAtual = -1;
    if (initial) {
      int matches = 0, match = -1;
      /* Native subtitle trackNum is a file ordinal. When a list is filtered,
       * restore only a uniquely identifiable language/codec combination. */
      if (dtsLegAntes >= 0 && dtsLegCount == nLeg) {
        int ordinal = dtsLegFaixa.ordinalMkv >= 0 ? dtsLegFaixa.ordinalMkv : dtsLegFaixa.numero;
        if (ordinal >= 0 && ordinal < nLeg) match = ordinal;
        if (match >= 0 && dtsLegFaixa.idioma[0] &&
            !ling_casa(dtsLegFaixa.idioma, faixaLeg[match].idioma)) match = -1;
      }
      if (match < 0 && dtsLegAntes >= 0 && dtsLegFaixa.idioma[0]) {
        for (i = 0; i < nLeg; i++)
          if (ling_casa(dtsLegFaixa.idioma, faixaLeg[i].idioma) &&
              (!dtsLegFaixa.codec[0] || !strcmp(dtsLegFaixa.codec, faixaLeg[i].codec))) {
            match = i; matches++;
          }
        if (matches != 1) match = -1;
      }
      legAtual = match;
      snprintf(legUrlAtual, sizeof legUrlAtual, "%s", dtsLegUrlAntes);
      if (match >= 0 && !legUrlAtual[0])
        dts_playback_subtitle(dtsSessao, faixaLeg[match].stream_index);
    } else {
      for (i = 0; i < nLeg; i++) if (faixaLeg[i].stream_index == selectedSub) legAtual = i;
    }
    if (dtsModoDv && audioAtual >= 0 && audioAtual < nAudio) {
      snprintf(dvMemUrl, sizeof dvMemUrl, "%s", urlAtual);
      snprintf(dvMemIdioma, sizeof dvMemIdioma, "%s", faixaAudio[audioAtual].idioma);
      dvMemOrd = audioAtual;
    }
    /* Converted only when the source track is DTS; on the Dolby Vision path
     * AC-3/E-AC-3 pass through untouched and there is nothing to label. */
    if (!dtsModoDv || (audioAtual >= 0 && audioAtual < nAudio && !strcmp(faixaAudio[audioAtual].codec, "dts"))) {
      snprintf(dtsSaida, sizeof dtsSaida, "%s", !strcmp(st.info.audio_codec, "ac3") ? "DTS → AC3 5.1" : "DTS → AAC Stereo");
      dtsEstado = VIDEO_DTS_CONVERTIDO;
    } else { dtsSaida[0] = 0; dtsEstado = 0; }
    if (expWin[0]) expJanelaAplicar();
    protegerScreensaver();
  }
  /* Also handles media IDs assigned only after native preroll. */
  sincronizarPlanoDts(&st);
  while (dts_playback_event(dtsSessao, event, sizeof event))
    if (!logDtsStage(event)) eventoPayload(event, sessao);
}

void video_pausar(int pausado) {
  char b[128];
  if (dtsSessao) {
    pausaConfirmada = 0; pausaPedida = !!pausado;
    dts_playback_pause(dtsSessao, pausado); return;
  }

  if (!ligado || !midia[0]) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\"}", midia);
  pausaConfirmada = 0;
  pausaPedida = pausado;
  chamar(pausado ? "pause" : "play", b, soLog);
  tocando = !pausado;
}

int video_pausa_confirmada(void) {
  return pausaPedida && pausaConfirmada && pronto && midia[0] &&
         !falhou && !terminou && !video_reconectando();
}

void video_volume(int pct) {
  char b[128];
  if (dtsSessao) { dts_playback_volume(dtsSessao, pct); return; }
  if (!ligado || !midia[0]) return;
  if (pct < 0) pct = 0; else if (pct > 100) pct = 100;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"volume\":%d}", midia, pct);
  chamar("setVolume", b, soLog);
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
  if (dtsSessao) {
    if (segundos < 0) segundos = 0;
    if (dvSilencio) dvEntradaSeg = segundos;
    posSeg = segundos; seekAlvo = segundos;
    seekEm = SDL_GetTicks() + SEEK_REPOUSO_MS; return;
  }
  if (!ligado || !midia[0]) return;
  if (segundos < 0) segundos = 0;
  // Com a tela do DV de pe ninguem busca com o controle (ela come as teclas):
  // o seek e a retomada, o ponto onde o filme deve comecar.
  if (dvSilencio) dvEntradaSeg = segundos;
  posSeg = segundos;
  seekAlvo = segundos;
  seekEm = SDL_GetTicks() + SEEK_REPOUSO_MS;
}

// Manda de fato. Chamado pelo video_bombear quando o repouso vence.
static void seekAgora(double segundos) {
  char b[192];
  if (dtsSessao) { dts_playback_seek(dtsSessao, segundos); return; }

  if (!ligado || !midia[0]) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"position\":%d}",
           midia, (int)(segundos * 1000.0));
  chamar("seek", b, soLog);
  seekEnvEm = SDL_GetTicks() | 1; seekEnvAlvo = (int)segundos; seekEnvAviso = 0;
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
// A JANELA EXPORTADA COM O QUADRO INTEIRO COMO ORIGEM (#158). Aqui ia `src`
// NULL ("o quadro inteiro"), e o SDL de parte das TVs repassa o nulo direto ao
// protocolo: "error marshalling arguments for set_exported_window (signature
// oo): null value passed for arg 0". Em umas TVs isso so fica no log (1.4.2:
// o video seguia tocando); em outras a conexao com o Wayland cai, o SDL manda
// SDL_QUIT e o app fecha — o "aperto play no canal e o app sai" do #158, com
// "tipo=0x100" logo depois do erro nos registros 4000 e 4012 (LG C4).
// `src` e o quadro que o decoder entrega, como no guia de midia do webosbrew
// e no ss4s: {0, 0, largura, altura}. Quem recorta a fonte e o
// video_janela_fonte. Antes do videoInfo vidW/vidH ainda sao os da midia
// anterior (ou 1920x1080); quando ele chega com outro tamanho, a janela e
// reaplicada (ver o bloco do videoInfo).
static void expJanelaAplicar(void) {
  SDL_Rect src, dst;
  if (!expWin[0] || !sdlExpJanela || janW < 1 || janH < 1) return;
  src.x = 0; src.y = 0; src.w = vidW > 0 ? vidW : 1920; src.h = vidH > 0 ? vidH : 1080;
  dst = escDst(janX, janY, janW, janH);
  expSrcW = src.w; expSrcH = src.h;
  printf("[video] janela exportada (quadro %dx%d) -> %d\n", src.w, src.h,
         sdlExpJanela(expWin, &src, &dst));
  fflush(stdout);
}

// Chamada UMA vez pelo main, com o drawable que o SDL entregou. Fica em 1920x1080
// (escala 1, o caminho da C9) se vier algo invalido.
void video_escala_definir(int sw, int sh) {
  if (sw < 1 || sh < 1) return;
  escW = sw; escH = sh;
  printf("[video] escala da janela %.3fx%.3f (superficie %dx%d, layout 1920x1080)%s\n",
         (double)sw / 1920.0, (double)sh / 1080.0, sw, sh,
         (sw == 1920 && sh == 1080) ? "" : " — HIPOTESE: destino lido no espaco da superficie (#176)");
  fflush(stdout);
  // Um retangulo ja guardado em pixels antigos nao existe: o cache de dedup e
  // por unidades de layout, que nao mudaram, entao nada a invalidar.
}

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
  // A janela simples muda o plano sem passar pelo par fonte/destino — o cache
  // deixaria de refletir o aplicado e engoliria a proxima chamada identica
  // (era o "Azul restaura a UI mas o video fica no tamanho do PiP": o par de
  // tela cheia era igual ao ultimo mandado, e o dedup o descartava).
  fonX = -1; dstX = -1;
  if (!ligado || !midia[0]) return;   // sem midia presa, aplicar seria no vazio
  if (!acb && !expWin[0]) return;
  printf("[video] janela %d,%d %dx%d cheia=%d\n", x, y, w, h, cheia);
  fflush(stdout);
  if (expWin[0]) {
    expJanelaAplicar();
    return;
  }
  { SDL_Rect d = escDst(x, y, w, h);
    acbJanela(acb, d.x, d.y, d.w, d.h, cheia, &tarefa); }
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
    if (acb) acbJanela(acb, 0, 0, escW, escH, 1, &tarefa);
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
void video_recorte_reaplicar(void) {
  long tarefa = 0;
  SDL_Rect d;
  if (fonX < 0 || !ligado || !midia[0] || !acb || !acbJanelaCustom) return;
  d = escDst(dstX, dstY, dstW, dstH);
  printf("[video] recorte repetido: fonte %d,%d %dx%d -> destino %d,%d %dx%d -> %d\n",
         fonX, fonY, fonW, fonH, dstX, dstY, dstW, dstH,
         acbJanelaCustom(acb, fonX, fonY, fonW, fonH, d.x, d.y, d.w, d.h,
                         (dstX == 0 && dstY == 0 && dstW == 1920 && dstH == 1080), &tarefa));
  fflush(stdout);
}

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
    dst = escDst(dx, dy, dw, dh);
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
    SDL_Rect d = escDst(dx, dy, dw, dh);
    int r = acbJanelaCustom(acb, sx, sy, sw, sh, d.x, d.y, d.w, d.h, cheia, &tarefa);
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
// O "COMECA PAUSADO" DO DOLBY VISION EM MKV (#203, C9 do dono, 08/10). No
// caminho proprio o `pronto` sobe no loadCompleted (sincronizarPlanoDts:
// prepared && native_loaded), mas o primeiro quadro so vem depois de o worker
// encher o pipeline: 3,4 s na C9 em 07/10 (loadCompleted 204769, `playing`
// 208141) e 10 s na fonte lenta de 08/10 (516911 -> 526691). O worker NAO pausa
// nada ali (worker-rate pause=0, native-play-accepted count=1 logo depois do
// loadCompleted). Quem lia "pronto e nao tocando" como pausa era o player:
// player_carregando() = !video_pronto() ja era 0, e o painel de pausa subia aos
// 5 s (PAUSAO_ESPERA_MS) com a ficha do titulo — e a retomada "de quem abre
// pausado" (2,5 s depois de pronto) mandava um seek que recarrega o caminho.
// Isto separa o preroll da pausa: carregado, sem `playing`, sem pausa nossa.
int    video_iniciando(void) {
  return dtsSessao && pronto && !tocando && !pausaPedida && !falhou && !terminou;
}
void   video_dv_segurar(int segurar) { dvSegurar = segurar ? 1 : 0; }

// --- A TELA DO DV (dvtela.h) -------------------------------------------------
void video_dv_fase(VideoDvFase *f) {
  int nosso = dtsSessao && dtsModoDv;
  if (!f) return;
  memset(f, 0, sizeof *f);
  f->sessao = dvSessaoN;
  f->sondado = dvSondado; f->perfil = dvMkvPerfil; f->el = dvMkvEl;
  if (dvAudioMkvOrd >= 0 && dvAudioMkvOrd < dvAudios && dvAudioDe[0]) {
    f->audioTrocado = 1;
    snprintf(f->audioDe, sizeof f->audioDe, "%s", dvAudioDe);
    snprintf(f->audioPara, sizeof f->audioPara, "%s", dvAudioCodec[dvAudioMkvOrd]);
  }
  f->caminho = dtsModoDv && dtsTentou;
  f->fonteAberta = nosso && (dvFonteAberta || dtsRevisao > 0);
  f->carregado = nosso && pronto;
  f->dvConfirmado = nosso && !strcmp(vidHdr, "DolbyVision");
  f->tocando = nosso && tocando;
  f->recusa = dvNao;
  f->falhou = falhou;
}
int video_dv_candidato(const char *url) {
  return dvPedido && !fonteMp4 && url && !nv_url_e_mp4(url) && dvLiberadoNestaTv();
}
// O HDR10 do player da TV, coberto pela tela, fica MUDO. setVolume e o mesmo
// do trailer mudo (uMS, por mediaId); a reproducao normal nunca mexe no volume,
// entao devolver e voltar a 100. O caminho do DV e outro mediaId, com o volume
// dele (dts_playback: 100 a cada load).
static void dvSilenciarTv(void) {
  char b[128];
  if (!dvSilencio || dvSilenciado || dtsSessao || !ligado || !midia[0]) return;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"volume\":0}", midia);
  chamar("setVolume", b, soLog);
  dvSilenciado = 1;
  printf("[dv] tela cobrindo: player da TV mudo\n"); fflush(stdout);
}
void video_dv_tela(int cobrindo) {
  char b[128];
  cobrindo = cobrindo ? 1 : 0;
  dvSilencio = cobrindo;
  if (cobrindo) { dvSilenciarTv(); return; }
  if (dvSilenciado && !dtsSessao && ligado && midia[0]) {
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"volume\":100}", midia);
    chamar("setVolume", b, soLog);
    printf("[dv] tela saiu: volume do player da TV de volta\n"); fflush(stdout);
  }
  dvSilenciado = 0;
}
// "Assistir agora em HDR10": esta fonte nao tenta mais DV. Com o caminho ja
// aberto, volta ao player da TV na posicao de partida dele (o preroll ainda nao
// andou); antes disso, so desliga e o player da TV segue.
void video_dv_recusar(void) {
  dvNao = VIDEO_DV_NAO_PESSOA;
  dvHabilitado = 0; mkvRetryEm = 0;
  if (dtsSessao && dtsModoDv) {
    char lu[sizeof legUrlAtual];
    double alvo = posSeg;
    dtsModoDv = 0;
    printf("[dv] a pessoa escolheu HDR10: volta ao player da TV em %.1fs\n", alvo); fflush(stdout);
    snprintf(lu, sizeof lu, "%s", legUrlAtual);
    if (!recarregarMesmaFonte(alvo, -1, -1, lu)) falhou = 1;
  } else { printf("[dv] a pessoa escolheu HDR10\n"); fflush(stdout); dvVoltarEntrada(); }
}
// Ha midia carregada. O furo na superficie usa ISTO e nao o loadCompleted:
// abrir o buraco cedo nao custa nada (atras dele so existe o plano de video) e
// esperar o evento deixaria a tela desenhada por cima do video se o evento
// mudar de nome ou nao vier.
int    video_ativo(void)    { return midia[0] != 0; }
// Erro real do pipeline na fonte atual ("Playing error" e afins). Zera no
// proximo video_tocar. O watchdog de canal usa isto para trocar de fonte —
// sem a flag, um pipeline que carrega e morre em seguida nunca dispara a
// proxima da lista.
int    video_falhou(void)   { return falhou; }
const char *video_erro_texto(void) { return erroTexto; }
int    video_decoder_anunciou(void) { return viuVideo; }
int    video_audio_nao_suportado(void) { return audioNaoSup; }
int    video_seek_desistiu(void) { return seekRetry.desistiu; }
int    video_terminou(void) { return terminou; }
int    video_conflito_recurso(void) { return 0; }
unsigned video_bufferando_ms(void) {
  Uint32 d = bufferandoDesde;
  // Esperando para reconectar nao e "fonte que morreu sem dizer": o watchdog
  // (app.c) nao pode trocar de fonte enquanto a maquina de reconexao decide.
  if (!d || nv_recon_ativa(&recon)) return 0;
  // 1 e nao 0 quando o carimbo acabou de sair: 0 e a resposta reservada para
  // "nao esta bufferizando", e devolve-lo no primeiro milissegundo diria o
  // contrario do que aconteceu.
  { Uint32 v = SDL_GetTicks() - d; return v ? (unsigned)v : 1u; }
}

int  video_n_audio(void)   { return nAudio; }
int  video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_audio(int i)   { return (i >= 0 && i < nAudio) ? &faixaAudio[i] : NULL; }
const VideoFaixa *video_legenda(int i) { return (i >= 0 && i < nLeg) ? &faixaLeg[i] : NULL; }
int video_legenda_ordinal_mkv(int i) { return (i >= 0 && i < nLeg) ? faixaLeg[i].ordinalMkv : -1; }

// Ver video.h. Derivado dos dois sinais que ja existem, sem estado novo: a
// sonda "voltou" quando nao esta pendente nem rodando. Antes do sourceInfo os
// dois sao 0 e isto diria "voltou" — mas ai nLeg tambem e 0 e nao ha faixa
// para escolher, entao ninguem pergunta.
int video_mkv_sondado(void) {
  if (dtsSessao) return dtsRevisao ? 1 : 0;
  if (!urlAtual[0] || fonteMp4) return 2;
  return (mkvPendente || fioMkvVivo) ? 0 : 1;
}

// Dispara a sonda AGORA, fora do gatilho de buffer de video_bombear. Chamado
// pela folha de legendas (#92, webOS 25): a pessoa escolheu uma faixa e o
// ordinal so existe depois da sonda — esperar 20 s de buffer que numa fonte
// lenta nunca chegam deixava a faixa ASS com a TV em silencio, sem uma linha
// de log que dissesse por que. Os 320 KB competem com o buffer (medido), mas
// a colheita do mkvass que vem a seguir pede mais que isso.
void video_sondar_mkv_agora(void) {
  if (dtsSessao || !mkvPendente || fioMkvVivo || !urlAtual[0]) return;
  mkvPendente = 0;
  if (nv_url_e_mp4(urlAtual)) return;   // .mp4 nunca tem Tracks de Matroska
  fioMkvVivo = 1;
  printf("[mkv] sonda do cabecalho disparada (buffer %.0f s a frente)\n", bufferSeg - posSeg);
  fflush(stdout);
  mkvFioSessao = __atomic_load_n(&sessao, __ATOMIC_ACQUIRE);
  if (pthread_create(&fioMkv, NULL, lerMkv, NULL) != 0) fioMkvVivo = 0;
  else pthread_detach(fioMkv);
}
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
// O tv.display do webOS tem o par sourceInput/displayOutput: pedir um pedaco
// MENOR do quadro para o mesmo destino e recorte de verdade.
int  video_recorte_fonte(void) { return 1; }

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
  if (dtsSessao && f) {
    if (f->stream_index < 0) return;
    if (!strcmp(f->codec, "dts") ||
        (dtsModoDv && (!strcmp(f->codec, "eac3") || !strcmp(f->codec, "ac3")))) {
      dts_playback_seek(dtsSessao, posSeg);
      dts_playback_audio(dtsSessao, f->stream_index);
      audioAtual = i;
    } else {
      VideoFaixa target = *f;
      double alvo = posSeg;
      char lu[sizeof legUrlAtual];
      snprintf(lu, sizeof lu, "%s", legUrlAtual);
      /* Match the native list after it arrives; demux ordinals need not match. */
      dtsNativeTarget = target; dtsNativePending = 1;
      /* A track the Dolby Vision path cannot feed: the TV player takes the
       * rest of this source (HDR10), and the path does not come back. */
      if (dtsModoDv) { dtsModoDv = 0; dvHabilitado = 0; }
      if (recarregarMesmaFonte(alvo, -1, -1, lu)) dtsTentou = 0;
      else dtsNativePending = 0;
    }
    return;
  }

  if (!ligado || !midia[0] || !f) return;
  /* Unsupported audio belongs to the previous selection. Give the new native
   * track a chance before allowing its own codec error to trigger conversion. */
  if (i != audioAtual) audioNaoSup = 0;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"type\":\"audio\",\"index\":%d}",
           midia, f->numero);
  chamar("selectTrack", b, soLog);
  audioAtual = i;
}

void video_escolher_legenda(int i) {
  char b[192];
  if (dtsSessao) {
    const VideoFaixa *f = video_legenda(i);
    if (i >= 0 && (!f || f->stream_index < 0)) return;
    legAtual = i < 0 ? -1 : i; legUrlAtual[0] = 0;
    dts_playback_subtitle(dtsSessao, f ? f->stream_index : -1);
    if (f) dts_playback_seek(dtsSessao, posSeg);
    return;
  }
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
  if (dtsSessao) return;
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
  if (dtsSessao && url && *url) {
    dts_playback_subtitle(dtsSessao, -1); legAtual = -1;
    snprintf(legUrlAtual, sizeof legUrlAtual, "%s", url);
    legenda_carregar(url); return;
  }
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

// Resposta do setPlayRate. O contexto leva a sessao e o valor pedido, para
// uma resposta atrasada de um pipeline que ja saiu nao desligar a linha.
static int aoVelocidade(LSHandle *h, LSMessage *m, void *u) {
  const char *p = lsPayload(m);
  unsigned ctx = (unsigned)(uintptr_t)u, minhaSessao = ctx >> 10;
  int cent = (int)(ctx & 0x3ffu), ok = p && strstr(p, "\"returnValue\":true") != NULL;
  (void)h;
  if (!ok) printf("[video] setPlayRate: %.200s\n", p ? p : "(nulo)");
  if (minhaSessao != (sessao & 0x3fffffu)) return 1;
  vel_log(cent, ok ? "plataforma ok" : "plataforma recusou");
  if (!ok) { velRecusada = 1; velPedida = 100; }
  return 1;
}

// Manda so com o pipeline pronto E tocando: nao ha prova de que o setPlayRate
// numa midia pausada nao a retome, e a pausa e da pessoa. Pausado, o pedido
// espera o play.
static void velBombear(void) {
  char b[160];
  int v = velPedida;
  if (velRecusada || !ligado || !midia[0] || !pronto || !tocando || v == velEnviada) return;
  velEnviada = v;
  snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"playRate\":%d.%02d,\"audioOutput\":true}",
           midia, v / 100, v % 100);
  chamarCtx("setPlayRate", b, aoVelocidade,
            (void *)(uintptr_t)(((sessao & 0x3fffffu) << 10) | ((unsigned)v & 0x3ffu)));
}

int  video_velocidade_suportada(void) { return !velRecusada; }
void video_velocidade(int c) {
  if (c <= 0 || c > 400) c = 100;
  if (velRecusada) return;
  velPedida = c;
}
int  video_velocidade_atual(void) { return velRecusada ? 100 : velPedida; }
void video_velocidade_recusada(void) {
  velRecusada = 1; velPedida = 100;
  // Desfaz no pipeline tambem (o setPlayRate pode ter valido pela metade).
  if (ligado && midia[0]) {
    char b[160];
    snprintf(b, sizeof b, "{\"mediaId\":\"%s\",\"playRate\":1.00,\"audioOutput\":true}", midia);
    chamar("setPlayRate", b, soLog);
  }
  velEnviada = 100;
}
int  video_velocidade_bloqueada(void) { return 0; }

void video_encerrar(void) {
  if (!ligado) return;
  video_parar();
  if (bindJoinable) { pthread_join(fioBind, NULL); bindJoinable = 0; }
  if (acb) { if (acbFinalizar) acbFinalizar(acb); acbDestruir(acb); acb = 0; }
  acbTipoAtual = acbTipoFalhou = -1;
  if (expWin[0] && sdlExpDestruir) { sdlExpDestruir(expWin); expWin[0] = 0; }
  if (laco) loopParar(laco);
  if (bus && lsUnregister) {
    erroLimpar();
    lsUnregister(bus, ERRO);
    erroLimpar();
    bus = NULL;
  }
  protetorLigado = 0;
  ligado = 0;
}
// Converted playback uses the same GL text overlay as external subtitles.
int video_legenda_nativa(char *d, int t) {
  if (dtsSessao && t > 0)
    return dts_playback_subtitle_text(dtsSessao, posSeg - estilo.atrasoMs / 1000.0, d, (size_t)t);
  if (d && t > 0) d[0] = 0;
  return 0;
}

#endif

#endif  /* !__EMSCRIPTEN__ */
