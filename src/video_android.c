// Player do Android TV. Quem toca e o Media3 ExoPlayer (NvPlayer.kt), numa
// SurfaceView ATRAS da SDLSurface do app; aqui fica so o estado que o resto do
// app le (video.h) e as chamadas ao Kotlin por JNI. Mesmo desenho do
// video_tpk.c (host .NET): o Kotlin avisa o que acontece pelos natives
// Java_space_nuvio_nativelegacy_NvPlayer_* de qualquer fio, o app le o estado
// no fio dele; por isso os campos sao volatile e nada aqui bloqueia.
//
// DIFERENCAS EM RELACAO AO .tpk
//  * Recorte de fonte de verdade (video_recorte_fonte = 1): o Kotlin posiciona
//    a SurfaceView por layout, entao um retangulo MAIOR que a tela e com origem
//    NEGATIVA e valido e o pai recorta o excedente. No Tizen 9 o ROI fora da
//    painel apagava o plano (#188/#195); aqui nao existe esse limite.
//  * O furo so abre com imagem: video_pronto() so fica 1 depois do evento 8
//    (onRenderedFirstFrame). player.c ja abre o furo quando player_com_video()
//    (comVideo && video_pronto()) e nao precisa mudar. Sem quadro nenhum
//    (so audio) o furo abre 3 s depois de tocar, como o .tpk faz.
//  * HDR/Dolby Vision/Atmos vem do Format das faixas (nativeHdr).
//  * Audio sem decoder: evento 9 (extensao do contrato), nao erro.
//
// Faixas: o Kotlin manda a lista depois que o player as conhece (nativeFaixa e
// nativeFaixasFim) e o app escolhe por escolher(tipo, idx). Legenda EMBUTIDA de
// texto: o Kotlin entrega os cues (nativeLegenda) e o app desenha. Legenda
// EXTERNA (OpenSubtitles/addon) nem passa por aqui: legenda.c baixa e desenha.
//
// TODO: ASS com libass (NV_ASS_LIBASS nao esta definido nesta fase, entao a
// sonda MKV segue "nao ha sonda", como no .tpk); capitulos do MKV para
// video_creditos; passthrough fino de AC3/EAC3 por AudioCapabilities.
#ifdef NV_ANDROID
#include "marco.h"
#include "streams.h"
#include "video.h"
#include "capmkv.h"
#include "audioinfo.h"
#include "video_reconexao.h"
#include "idioma.h"
#include "linguas.h"
#include "audioescolha.h"
#include "streamfitpassiva.h"
#include "audsync.h"
#include "cacheboost.h"
#include "tex_cache.h"
#include "velocidade.h"
#include <SDL2/SDL.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#include <pthread.h>

// --- ponte JNI ---------------------------------------------------------------
#define NV_CLASSE "space/nuvio/nativelegacy/NvPlayer"

static jclass    gCls;      // GlobalRef: FindClass de fio do SDL nao acha classe do app
static jmethodID mAbrir, mParar, mPausar, mBuscar, mVolume, mJanela, mEscolher;
static jmethodID mAbrirPosicao;
// 2.0.3 (opcional): abrirRetomada(url, cab, inicioMs, fracao, geracao). So o
// percentual conhecido (retomada da conta, sem registro local): o Kotlin
// aplica fracao/10000 da duracao do CONTAINER antes do primeiro quadro.
static jmethodID mAbrirRetomada;
static jmethodID mPreparar, mPrepararProximo, mPrepararEstado, mCancelarPreparo, mAbrirPreparada;
static jmethodID mPararMantendoPreparo;
static unsigned preparoAdotar;
// F07 (optional, like abrirPosicao): cache(mb) and ganho(pct). A shell without
// them simply has no seek cache and no boost.
static jmethodID mCache, mGanho, mBuffer;
// Warm resume (#492, optional too): titulo(id) and descartarGuardado().
static jmethodID mTitulo, mDescartar, mMorno, mQuadro;
// #202 (opcional, como os de cima): velocidade(centesimos). Casca sem ele =
// sem a linha da velocidade.
static jmethodID mVelocidade;

static int resolverMetodos(JNIEnv *env) {
  mAbrir    = (*env)->GetStaticMethodID(env, gCls, "abrir", "(Ljava/lang/String;Ljava/lang/String;)V");
  mParar    = (*env)->GetStaticMethodID(env, gCls, "parar", "()V");
  mPausar   = (*env)->GetStaticMethodID(env, gCls, "pausar", "(I)V");
  mBuscar   = (*env)->GetStaticMethodID(env, gCls, "buscar", "(I)V");
  mVolume   = (*env)->GetStaticMethodID(env, gCls, "volume", "(I)V");
  mJanela   = (*env)->GetStaticMethodID(env, gCls, "janela", "(IIIII)V");
  mEscolher = (*env)->GetStaticMethodID(env, gCls, "escolher", "(II)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return 0; }
  // Opcional: uma casca anterior ainda abre normalmente e recebe o seek
  // depois da duracao. A ausencia deste metodo nao derruba a ponte inteira.
  mAbrirPosicao = (*env)->GetStaticMethodID(env, gCls, "abrirPosicao", "(Ljava/lang/String;Ljava/lang/String;II)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mAbrirPosicao = NULL; }
  mAbrirRetomada = (*env)->GetStaticMethodID(env, gCls, "abrirRetomada", "(Ljava/lang/String;Ljava/lang/String;III)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mAbrirRetomada = NULL; }
  mCache = (*env)->GetStaticMethodID(env, gCls, "cache", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mCache = NULL; }
  mTitulo = (*env)->GetStaticMethodID(env, gCls, "cacheTitle", "(Ljava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mTitulo = NULL; }
  mDescartar = (*env)->GetStaticMethodID(env, gCls, "descartarGuardado", "()V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mDescartar = NULL; }
  mMorno = (*env)->GetStaticMethodID(env, gCls, "morno", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mMorno = NULL; }
  mQuadro = (*env)->GetStaticMethodID(env, gCls, "quadro", "(I)I");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mQuadro = NULL; }
  mBuffer = (*env)->GetStaticMethodID(env, gCls, "buffer", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mBuffer = NULL; }
  mGanho = (*env)->GetStaticMethodID(env, gCls, "ganho", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mGanho = NULL; }
  mVelocidade = (*env)->GetStaticMethodID(env, gCls, "velocidade", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mVelocidade = NULL; }
  mPreparar = (*env)->GetStaticMethodID(env, gCls, "preparar", "(Ljava/lang/String;Ljava/lang/String;IIIIILjava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mPreparar = NULL; }
  mPrepararProximo = (*env)->GetStaticMethodID(env, gCls, "prepararProximo", "(Ljava/lang/String;Ljava/lang/String;IIIIILjava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mPrepararProximo = NULL; }
  mPrepararEstado = (*env)->GetStaticMethodID(env, gCls, "prepararEstado", "(I)I");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mPrepararEstado = NULL; }
  mCancelarPreparo = (*env)->GetStaticMethodID(env, gCls, "cancelarPreparo", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mCancelarPreparo = NULL; }
  mAbrirPreparada = (*env)->GetStaticMethodID(env, gCls, "abrirPreparada", "(Ljava/lang/String;Ljava/lang/String;IIIILjava/lang/String;)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mAbrirPreparada = NULL; }
  mPararMantendoPreparo = (*env)->GetStaticMethodID(env, gCls, "pararMantendoPreparo", "(I)V");
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); mPararMantendoPreparo = NULL; }
  return mAbrir && mParar && mPausar && mBuscar && mVolume && mJanela && mEscolher;
}

// Chamado pelo Kotlin (NvPlayer.iniciar), no fio principal, que ve a classe do
// app. Guarda a classe e os jmethodID para os fios do SDL usarem depois.
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(JNIEnv *env, jclass cls) {
  if (!gCls) gCls = (*env)->NewGlobalRef(env, cls);
  if (!resolverMetodos(env)) { gCls = NULL; printf("[video] android: metodos do NvPlayer nao achados\n"); }
  else printf("[video] android: ponte do NvPlayer pronta\n");
  fflush(stdout);
}

// --- F06: PCM do audio tocando (AudioSyncTap.kt) -----------------------------
// O tap so existe numa casca que o anuncia: o primeiro nativeAudioEstado
// registra o backend. Casca antiga = plataforma sem PCM (o ajuste diz isso).
static void ligarTap(int on);
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioEstado(JNIEnv *env, jclass cls, jint fmt) {
  static int registrado;
  (void)env; (void)cls;
  if (!registrado) { registrado = 1; audsync_backend(ligarTap); }
  audsync_formato((int)fmt);
  // F07: the same sink-input format decides the boost. AudioSyncTap FMT_*
  // (0 none, 1 PCM, 2 bitstream) maps 1:1 to CB_GANHO_* (unknown, PCM,
  // passthrough): with bitstream the gain processor never sees samples.
  { static int ultimoGanho = -1;
    int g = fmt == 1 ? CB_GANHO_PCM : fmt == 2 ? CB_GANHO_PASSTHROUGH : CB_GANHO_DESCONHECIDO;
    cacheboost_ganho_relato(g);
    if (g != ultimoGanho && g != CB_GANHO_DESCONHECIDO) {
      printf("[video] android audio: %s\n", g == CB_GANHO_PASSTHROUGH ? "passthrough (sem reforco)" : "PCM (reforco disponivel)");
      fflush(stdout);
    }
    ultimoGanho = g; }
}
// Fio de reproducao do ExoPlayer. Copia para a pilha e entrega ao anel
// limitado do audsync.c: nenhuma alocacao, nunca espera o analisador.
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioPcm(JNIEnv *env, jclass cls, jshortArray pcm, jint n, jlong ptsUs) {
  jshort b[2048];
  (void)cls;
  if (!pcm || n <= 0 || n > (jint)(sizeof b / sizeof *b)) return;
  (*env)->GetShortArrayRegion(env, pcm, 0, n, b);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); return; }
  audsync_pcm((const int16_t *)b, (int)n, (int64_t)ptsUs);
}

// Plano B se o Kotlin nao chamou nativeIniciar antes do primeiro uso (lib
// carregada depois do iniciar): o ClassLoader da Activity enxerga a classe.
static int garantirPonte(JNIEnv *env) {
  jobject act;
  jclass cAct, cLoader, achada;
  jmethodID mGet, mLoad;
  jobject loader;
  jstring nome;
  if (gCls) return 1;
  act = (jobject)SDL_AndroidGetActivity();
  if (!act) return 0;
  cAct = (*env)->GetObjectClass(env, act);
  mGet = (*env)->GetMethodID(env, cAct, "getClassLoader", "()Ljava/lang/ClassLoader;");
  loader = mGet ? (*env)->CallObjectMethod(env, act, mGet) : NULL;
  cLoader = loader ? (*env)->GetObjectClass(env, loader) : NULL;
  mLoad = cLoader ? (*env)->GetMethodID(env, cLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;") : NULL;
  nome = (*env)->NewStringUTF(env, "space.nuvio.nativelegacy.NvPlayer");
  achada = (mLoad && nome) ? (jclass)(*env)->CallObjectMethod(env, loader, mLoad, nome) : NULL;
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); achada = NULL; }
  if (achada) { gCls = (*env)->NewGlobalRef(env, achada); if (!resolverMetodos(env)) gCls = NULL; }
  if (nome) (*env)->DeleteLocalRef(env, nome);
  if (achada) (*env)->DeleteLocalRef(env, achada);
  if (cLoader) (*env)->DeleteLocalRef(env, cLoader);
  if (loader) (*env)->DeleteLocalRef(env, loader);
  (*env)->DeleteLocalRef(env, cAct);
  (*env)->DeleteLocalRef(env, act);
  return gCls != NULL;
}

// UTF-8 -> jstring (UTF-16). O NewStringUTF do JNI e UTF-8 MODIFICADO e erra
// em tudo fora do BMP; cabecalhos e URLs raramente passam disso, mas o custo e
// pequeno.
static jstring paraJString(JNIEnv *env, const char *s) {
  jchar buf[4096];
  int n = 0;
  const unsigned char *p = (const unsigned char *)(s ? s : "");
  while (*p && n < 4094) {
    unsigned c = *p++, extra = 0;
    if (c >= 0xF0) { c &= 0x07; extra = 3; }
    else if (c >= 0xE0) { c &= 0x0F; extra = 2; }
    else if (c >= 0xC0) { c &= 0x1F; extra = 1; }
    else if (c >= 0x80) c = 0xFFFD;
    while (extra-- && (*p & 0xC0) == 0x80) c = (c << 6) | (*p++ & 0x3F);
    if (c >= 0x10000) { c -= 0x10000; buf[n++] = (jchar)(0xD800 + (c >> 10)); buf[n++] = (jchar)(0xDC00 + (c & 0x3FF)); }
    else buf[n++] = (jchar)c;
  }
  return (*env)->NewString(env, buf, n);
}

// jstring (UTF-16) -> UTF-8 de verdade em dst (GetStringUTFChars daria CESU-8
// para emoji, que o SDL_ttf nao le).
static void deJString(JNIEnv *env, jstring js, char *dst, size_t tam) {
  const jchar *u;
  jsize n, i;
  size_t o = 0;
  if (tam == 0) return;
  dst[0] = 0;
  if (!js) return;
  u = (*env)->GetStringChars(env, js, NULL);
  if (!u) return;
  n = (*env)->GetStringLength(env, js);
  for (i = 0; i < n && o + 5 < tam; i++) {
    unsigned c = u[i];
    if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) {
      c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00); i++;
    }
    if (c < 0x80) dst[o++] = (char)c;
    else if (c < 0x800) { dst[o++] = (char)(0xC0 | (c >> 6)); dst[o++] = (char)(0x80 | (c & 0x3F)); }
    else if (c < 0x10000) { dst[o++] = (char)(0xE0 | (c >> 12)); dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); dst[o++] = (char)(0x80 | (c & 0x3F)); }
    else { dst[o++] = (char)(0xF0 | (c >> 18)); dst[o++] = (char)(0x80 | ((c >> 12) & 0x3F));
           dst[o++] = (char)(0x80 | ((c >> 6) & 0x3F)); dst[o++] = (char)(0x80 | (c & 0x3F)); }
  }
  dst[o] = 0;
  (*env)->ReleaseStringChars(env, js, u);
}

// Env do fio que chama (o SDL ja anexa o fio do app) ou NULL sem ponte.
static JNIEnv *ambiente(void) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  if (!env || !garantirPonte(env)) return NULL;
  return env;
}
static void fimChamada(JNIEnv *env) {
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); printf("[video] android: excecao no NvPlayer\n"); fflush(stdout); }
}
static void kSemArg(jmethodID m) {
  JNIEnv *env = ambiente();
  if (!env) return;
  (*env)->CallStaticVoidMethod(env, gCls, m);
  fimChamada(env);
}
static void kInt(jmethodID m, int v) {
  JNIEnv *env = ambiente();
  if (!env) return;
  (*env)->CallStaticVoidMethod(env, gCls, m, (jint)v);
  fimChamada(env);
}

static int precarregarVideo(const char *url, const char *cab, double segundos,
                            double pct, int buffer, int cacheMb, unsigned token,
                            const char *idiomaAudio, int proximo) {
  JNIEnv *env;
  jmethodID metodo;
  jstring u, c, a;
  int inicioMs, fracao, ok;
  if (!url || !*url || !token || token > INT_MAX) return 0;
  env = ambiente();
  metodo = proximo ? mPrepararProximo : mPreparar;
  if (!env || !metodo || !mPrepararEstado || !mCancelarPreparo || !mAbrirPreparada) return 0;
  inicioMs = isfinite(segundos) && segundos > 0.0 && segundos <= INT_MAX / 1000.0
               ? (int)(segundos * 1000.0) : 0;
  fracao = !inicioMs && isfinite(pct) && pct > 0.0 && pct < 100.0
               ? (int)(pct * 100.0 + 0.5) : 0;
  u = paraJString(env, url);
  c = u && !(*env)->ExceptionCheck(env) ? paraJString(env, cab ? cab : "") : NULL;
  a = c && !(*env)->ExceptionCheck(env) ? paraJString(env, idiomaAudio ? idiomaAudio : "") : NULL;
  if (!u || !c || !a) {
    if (u) (*env)->DeleteLocalRef(env, u);
    if (c) (*env)->DeleteLocalRef(env, c);
    if (a) (*env)->DeleteLocalRef(env, a);
    fimChamada(env); return 0;
  }
  (*env)->CallStaticVoidMethod(env, gCls, metodo, u, c, (jint)inicioMs,
      (jint)fracao, (jint)token, (jint)buffer, (jint)cacheMb, a);
  ok = !(*env)->ExceptionCheck(env);
  (*env)->DeleteLocalRef(env, u); (*env)->DeleteLocalRef(env, c);
  (*env)->DeleteLocalRef(env, a);
  fimChamada(env);
  return ok;
}
int video_precarregar(const char *url, const char *cab, double segundos,
                     double pct, int buffer, int cacheMb, unsigned token, const char *idiomaAudio) {
  return precarregarVideo(url, cab, segundos, pct, buffer, cacheMb, token, idiomaAudio, 0);
}
int video_precarregar_proximo(const char *url, const char *cab, double segundos,
                             double pct, int buffer, int cacheMb, unsigned token, const char *idiomaAudio) {
  return precarregarVideo(url, cab, segundos, pct, buffer, cacheMb, token, idiomaAudio, 1);
}
int video_precarregar_estado(unsigned token) {
  JNIEnv *env = ambiente();
  int estado;
  if (!env || !mPrepararEstado || !token || token > INT_MAX) return 0;
  estado = (*env)->CallStaticIntMethod(env, gCls, mPrepararEstado, (jint)token);
  if ((*env)->ExceptionCheck(env)) { fimChamada(env); return 0; }
  return estado >= -1 && estado <= 2 ? estado : 0;
}
void video_precarregar_cancelar(unsigned token) {
  JNIEnv *env;
  if (!token || preparoAdotar == token) preparoAdotar = 0;
  env = ambiente();
  if (env && mCancelarPreparo)
    (*env)->CallStaticVoidMethod(env, gCls, mCancelarPreparo, (jint)token);
  if (env) fimChamada(env);
}
void video_precarregar_adotar(unsigned token) { preparoAdotar = token <= INT_MAX ? token : 0; }

// escolher(3, on): liga/desliga o tap no Kotlin (so posta ao fio principal).
static void ligarTap(int on) {
  JNIEnv *env = ambiente();
  if (!env) return;
  (*env)->CallStaticVoidMethod(env, gCls, mEscolher, (jint)3, (jint)(on ? 1 : 0));
  fimChamada(env);
}

// --- estado ------------------------------------------------------------------
#define MAX_FAIXAS NV_FAIXA_MAX
static VideoFaixa faixaAudio[MAX_FAIXAS], faixaLeg[MAX_FAIXAS];
static volatile int nAudio, nLeg, audioAtual, legAtual = -1;
static SDL_mutex *travaLeg;
static char legTexto[1024];
static Uint32 legAte;

static char urlAtual[4096];
static char cabecalhos[2048];
// ULTIMA JANELA ENTREGUE ao Kotlin (dedupe de reenvio; ver janelaSeMudou, la
// embaixo, onde a regra vive junto das funcoes que enviam).
static int envX, envY, envW, envH, envEnc, temEnv;
// prontoLoad = o player preparou (evento 1); primeiroQuadro = evento 8.
// video_pronto() so e 1 com imagem (ver o cabecalho).
static volatile int superficieEstavel;
static volatile int ativo, prontoLoad, primeiroQuadro, falhou, terminou, tocando, largura, altura;
static volatile int conflito, semDecoderAudio;
// PAUSA CONFIRMADA (player_suspender): o pedido daqui e o evento 3 do Kotlin
// DEPOIS dele. O evento chega do fio principal ~3 ms depois (medido na TCL,
// 02/10); um evento 2 (tocando) no meio desfaz a confirmacao.
static volatile int pausaPedida, pausaVista;
static volatile int durMs, bufferando, posMs, posNativoMs;
// VELOCIDADE (#202, video.h): o ExoPlayer novo de cada abertura nasce em 1x
// (velEnviada = 100 no abrirSessao) e o video_bombear reaplica o pedido assim
// que ele esta pronto. setPlaybackSpeed aceita qualquer valor positivo; com o
// audio em PASSTHROUGH (bitstream ao receptor) eu acho que o sink nao consegue
// mudar o tempo e o Media3 segue em 1x, sem erro — nao provado em TV.
static volatile int velPedida = 100, velEnviada = 100, velRecusada;
static volatile Uint32 bufferDesde, tocandoDesde;
static volatile const char *hdrAtual = "none";
static volatile int dvAtual, atmosAtual;
static unsigned sessao;
// F07: the player arms the next video_tocar with its cache limit
// (cacheboost_backend_cache); trailers never arm, so they stay uncached and
// at 100%. `cacheSessao` survives the reconnection reopens of that video.
static int bufferPreset, bufferEnviado;
static int cacheArmado = -1, cacheSessao, cacheEnviado, playerSessao;
// Warm resume (#492): id of the title the player armed for the next open, the
// one of this open (empty for trailers/channels) and the one Kotlin has.
static char tituloArmado[96], tituloSessao[96], tituloEnviado[96];
static int ganhoEnviado = CB_VOL_NORMAL;
static int retomadaInicialEstado = -1;
static pthread_mutex_t travaRetomada = PTHREAD_MUTEX_INITIALIZER;
#define ANDROID_FURO_PRAZO_MS 3000u   // sem quadro (so audio): abre 3 s depois de tocar

// RECONEXAO (video_reconexao.h): igual ao .tpk. O evento 5 so ANOTA; a
// decisao e o recarregar sao do video_bombear.
static NvReconexao recon;
static int reconProxima, reconPermitida, reconIniciou;
static volatile int reconErroPend, reconErroCod, reconErroVideo;
static int reconAudio = -1, reconLeg = -1;
static volatile int reconFaixasPend;
static int reconBuscarMs = -1;
static int reconPausado;
static int recarregarPausa = -1, recarregarPosMs;
static char erroTxt[64];

// Codigos do PlaybackException (Media3) que sao de REDE: IO_UNSPECIFIED 2000,
// IO_NETWORK_CONNECTION_FAILED 2001, IO_NETWORK_CONNECTION_TIMEOUT 2002. NAO
// MEDIDO em aparelho: status HTTP (2004), 403/404 e formato quebrado ficam de
// fora de proposito, recarregar a mesma URL so repete a recusa.
static int erroDeRede(int cod) { return cod >= 2000 && cod <= 2002; }

void video_escolher_audio(int i);

// A ORDEM DO AUDIO (audioescolha.h), como no video_tpk.c; sem regra que
// decida, fica a do arquivo.
static void escolherAudioPreferido(void) {
  AudioCand c[MAX_FAIXAS];
  int i, o, k;
  if (nAudio < 2) return;
  for (i = 0; i < nAudio; i++) {
    c[i].idioma = faixaAudio[i].idioma; c[i].codec = faixaAudio[i].codec; c[i].rotulo = faixaAudio[i].rotulo;
    c[i].canais = faixaAudio[i].canais;
    // O FFmpeg do player decodifica DTS/TrueHD: aqui nao ha o limite da LG e
    // da Samsung, entao DTS/TrueHD concorre igual (dts = 0): no idioma vale a
    // primeira faixa do arquivo, seja qual for o codec (dono, 15/10).
    c[i].dts = 0; c[i].pode = 1; c[i].numero = faixaAudio[i].numero;
  }
  k = audioesc_escolher(c, nAudio, &o);
  if (k < 0 || k == audioAtual) return;
  printf("[video] audio pela ordem (%s): %s (faixa %d de %d)\n", audioesc_origem_nome(o), faixaAudio[k].rotulo, k + 1, nAudio);
  video_escolher_audio(k);
}

// --- natives (Kotlin -> C) ----------------------------------------------------
// Fio principal do Kotlin, depois que o player conhece as faixas: uma chamada
// por faixa (tipo 0 = audio, 1 = legenda) e no fim nativeFaixasFim. O contador
// so sobe no fim, com a lista inteira escrita.
static VideoFaixa novasA[MAX_FAIXAS], novasL[MAX_FAIXAS];
static int nNovasA, nNovasL;

// `flags` (#287): bit 0 = SELECTION_FLAG_FORCED, bit 1 = ROLE_FLAG_DESCRIBES_
// MUSIC_AND_SOUND (SDH); `nome` = Format.label (o Name da faixa do MKV).
// `mime`/`canais` (#293): codec e canais da faixa de audio.
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeFaixa(JNIEnv *env, jclass cls, jint tipo, jint idx, jstring lingua,
                                                                       jint flags, jstring nome, jstring mime, jint canais) {
  VideoFaixa *f;
  char l[16], nm[48], mm[24], ai[40];
  (void)cls;
  deJString(env, lingua, l, sizeof l);
  deJString(env, nome, nm, sizeof nm);
  if (tipo == 0) { if (nNovasA >= MAX_FAIXAS) return; f = &novasA[nNovasA++]; }
  else           { if (nNovasL >= MAX_FAIXAS) return; f = &novasL[nNovasL++]; }
  memset(f, 0, sizeof *f);
  f->numero = idx;
  f->ordinalMkv = tipo ? idx : -1;
  if (strcmp(l, "und") && strcmp(l, "unknown") && strcmp(l, "zxx")) snprintf(f->idioma, sizeof f->idioma, "%s", l);
  if (f->idioma[0]) snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(f->idioma)));
  else snprintf(f->rotulo, sizeof f->rotulo, "%s %d", i18n(tipo ? "Legenda" : "Áudio"),
                tipo ? nNovasL : nNovasA);
  if (tipo) {
    const char *r;
    // O nome de exibicao nao passa pelo buffer curto usado pela classificacao.
    // deJString preserva UTF-8 completo, inclusive emoji, ate o limite do campo.
    deJString(env, nome, f->nome, sizeof f->nome);
    f->tipoLeg = ling_tipo_legenda(nm, flags & 1, (flags & 2) != 0);
    f->letreiro = f->tipoLeg == LING_LEG_FORCADA || f->tipoLeg == LING_LEG_LETREIROS;
    r = ling_tipo_legenda_rotulo(f->tipoLeg);
    if (r) {
      size_t k = strlen(f->rotulo);
      snprintf(f->rotulo + k, sizeof f->rotulo - k, "  \xc2\xb7  %s", i18n(r));
    }
  } else {
    // #293: codec (mime do Format) e canais; a linha de baixo da folha de Audio
    // vira "E-AC-3 5.1 Atmos". Mime desconhecido: rotulo como estava.
    deJString(env, mime, mm, sizeof mm);
    snprintf(f->codec, sizeof f->codec, "%s", mm);
    f->canais = canais;
    if (audioinfo_texto(mm, canais, 0, ai, sizeof ai))
    { char base[sizeof f->rotulo];
      snprintf(base, sizeof base, "%s", f->rotulo);
      snprintf(f->rotulo, sizeof f->rotulo, "%s  \xc2\xb7  %s", base, ai); }
  }
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeFaixasFim(JNIEnv *env, jclass cls, jint selAudio, jint selLeg) {
  // Lista de legendas VAZIA mandada com a de audio ja existente = releitura so
  // do audio (HLS que publica as faixas depois de tocar): a legenda que o app
  // ja escolheu fica.
  int releitura = (nAudio || nLeg) && !nNovasL && nLeg;
  (void)env; (void)cls; (void)selLeg;
  memcpy(faixaAudio, novasA, sizeof novasA);
  audioAtual = selAudio >= 0 ? selAudio : 0;
  if (!releitura) {
    memcpy(faixaLeg, novasL, sizeof novasL);
    legAtual = -1;   // o Kotlin sobe com legenda desligada; quem liga e o app (faixas.c)
    nLeg = nNovasL;
  }
  nAudio = nNovasA;
  nNovasA = nNovasL = 0;
  printf("[video] faixas: %d audio, %d legenda\n", nAudio, nLeg);
  fflush(stdout);
  // Recarregar de reconexao: devolve as faixas que a pessoa tinha.
  if (reconFaixasPend) {
    if (!releitura && reconLeg >= -1 && reconLeg < nLeg) video_escolher_legenda(reconLeg);
    if (nAudio > 0) {
      reconFaixasPend = 0;
      if (reconAudio >= 0 && reconAudio < nAudio) video_escolher_audio(reconAudio);
    }
    printf("[video] reconexao: faixas devolvidas (audio %d, legenda %d)\n", reconAudio, reconLeg);
    fflush(stdout);
    return;
  }
  escolherAudioPreferido();
}

// Texto da legenda embutida em vigor, valido por `durMs` (estimada no Kotlin:
// o Media3 nao diz quando o cue acaba, so quando o proximo grupo chega).
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeLegenda(JNIEnv *env, jclass cls, jstring texto, jint dur) {
  char buf[sizeof legTexto];
  (void)cls;
  if (!travaLeg) return;
  deJString(env, texto, buf, sizeof buf);
  SDL_LockMutex(travaLeg);
  memcpy(legTexto, buf, sizeof legTexto);
  legAte = SDL_GetTicks() + (Uint32)(dur > 0 ? dur : 3000);
  SDL_UnlockMutex(travaLeg);
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativePos(JNIEnv *env, jclass cls, jint ms) {
  (void)env; (void)cls;
  posMs = posNativoMs = ms;
}

// ESTATISTICAS AO VIVO (video.h, VideoStats): o Kotlin manda 1x/s, do fio
// principal, o que ele CONTA — bytes do socket (ParaleloDataSource), buffer
// (bufferedPosition), conexoes abertas, 1o byte, host final e quadros
// (AnalyticsListener). A trava e curta: o app copia no quadro dele.
static pthread_mutex_t travaStats = PTHREAD_MUTEX_INITIALIZER;
static VideoStats statsAndroid;
static int statsValido;
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeStats(JNIEnv *env, jclass cls,
    jlong bytes, jint bufferMs, jint alvoMs, jint conexoes, jint primeiroByteMs,
    jlong perdidos, jlong quadros, jstring host) {
  char h[96];
  (void)cls;
  deJString(env, host, h, sizeof h);
  pthread_mutex_lock(&travaStats);
  video_stats_vazio(&statsAndroid);
  statsAndroid.bytes = bytes >= 0 ? (long long)bytes : -1;
  statsAndroid.bufferSeg = bufferMs >= 0 ? bufferMs / 1000.0 : -1.0;
  statsAndroid.bufferAlvoSeg = alvoMs > 0 ? alvoMs / 1000.0 : 0.0;
  statsAndroid.conexoes = conexoes;
  statsAndroid.primeiroByteMs = primeiroByteMs;
  statsAndroid.quadrosPerdidos = perdidos;
  statsAndroid.quadros = quadros;
  snprintf(statsAndroid.host, sizeof statsAndroid.host, "%s", h);
  statsValido = 1;
  pthread_mutex_unlock(&travaStats);
}
void video_estatisticas(VideoStats *s) {
  pthread_mutex_lock(&travaStats);
  if (statsValido && ativo) *s = statsAndroid;
  else video_stats_vazio(s);
  pthread_mutex_unlock(&travaStats);
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(JNIEnv *env, jclass cls, jint geracao, jint aceita) {
  (void)env; (void)cls;
  pthread_mutex_lock(&travaRetomada);
  if ((unsigned)geracao == sessao) retomadaInicialEstado = aceita ? 1 : -1;
  pthread_mutex_unlock(&travaRetomada);
}
int video_retomada_inicial_estado(void) {
  int estado;
  pthread_mutex_lock(&travaRetomada);
  estado = retomadaInicialEstado;
  pthread_mutex_unlock(&travaRetomada);
  return estado;
}
static unsigned novaRetomada(int estado) {
  unsigned geracao;
  pthread_mutex_lock(&travaRetomada);
  if (++sessao == 0) sessao++;
  geracao = sessao; retomadaInicialEstado = estado;
  pthread_mutex_unlock(&travaRetomada);
  return geracao;
}
// StreamFit (F03): the session the Kotlin player received with its open; the
// passive telemetry gate (streamfitpassiva.c) only accepts this generation.
unsigned video_android_sessao(void) {
  unsigned g;
  pthread_mutex_lock(&travaRetomada);
  g = sessao;
  pthread_mutex_unlock(&travaRetomada);
  return g;
}
static void estadoRetomada(int estado) {
  pthread_mutex_lock(&travaRetomada);
  retomadaInicialEstado = estado;
  pthread_mutex_unlock(&travaRetomada);
}

// Capacidade da TELA (Display.getHdrCapabilities), lida no onCreate: so o
// formato preferido entre resolucoes iguais depende dela (streams.c).
// -1 = o aparelho nao informou (lista vazia): desconhecido nao penaliza.
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeTela(JNIEnv *env, jclass cls, jint hdr, jint dv) {
  (void)env; (void)cls;
  stream_definir_tela(hdr, dv);
  printf("[video] android tela hdr=%d dv=%d\n", (int)hdr, (int)dv);
  fflush(stdout);
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeDecoder4k(JNIEnv *env, jclass cls,
                                                                                  jint hevc, jint avc, jint vp9, jint av1) {
  (void)env; (void)cls;
  stream_definir_decoder4k(hevc, avc, vp9, av1);
  printf("[tv] decoder 4K: hevc=%d avc=%d vp9=%d av1=%d\n", (int)hevc, (int)avc, (int)vp9, (int)av1);
  fflush(stdout);
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeErroTexto(JNIEnv *env, jclass cls, jstring texto) {
  char t[512];
  (void)cls;
  deJString(env, texto, t, sizeof t);
  printf("[video] android erro: %s\n", t);
  fflush(stdout);
}

// #345: keep audio diagnostics in the uploaded app log, not only logcat.
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeAudioLog(JNIEnv *env, jclass cls, jstring texto) {
  char t[512];
  (void)cls;
  deJString(env, texto, t, sizeof t);
  printf("[video] android audio: %s\n", t);
  fflush(stdout);
}

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeHdr(JNIEnv *env, jclass cls, jstring hdr, jint dv, jint atmos) {
  char h[24];
  (void)cls;
  deJString(env, hdr, h, sizeof h);
  // So literais: video_hdr() devolve o ponteiro cru ao fio do app.
  hdrAtual = !strcmp(h, "HDR10") ? "HDR10" : !strcmp(h, "HLG") ? "HLG" :
             !strcmp(h, "DolbyVision") ? "DolbyVision" : "none";
  dvAtual = dv != 0;
  atmosAtual = atmos != 0;
  printf("[video] android hdr=%s dv=%d atmos=%d\n", (const char *)hdrAtual, dvAtual, atmosAtual);
  fflush(stdout);
}

// F07: what the Kotlin player did with the seek cache in this open. Kotlin
// main thread; cacheboost locks. (The audio output state for the boost comes
// from F06's nativeAudioEstado, below: one detection for both features.)
JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeCache(JNIEnv *env, jclass cls, jint estado, jint pedidoMb, jint limiteMb, jint usadoMb) {
  static int ultimo = -1;
  (void)env; (void)cls;
  cacheboost_cache_relato(estado, pedidoMb, limiteMb, usadoMb);
  if (estado != ultimo) {
    ultimo = estado;
    printf("[video] android cache: estado %d, pedido %d MB, limite %d MB\n", (int)estado, (int)pedidoMb, (int)limiteMb);
    fflush(stdout);
  }
}

enum { EV_PRONTO = 1, EV_TOCANDO = 2, EV_PAUSADO = 3, EV_FIM = 4, EV_ERRO = 5,
       EV_TAMANHO = 6, EV_BUFFER = 7, EV_PRIMEIRO_QUADRO = 8, EV_AUDIO_SEM_DECODER = 9,
       EV_SUPERFICIE_ESTAVEL = 10 };

JNIEXPORT void JNICALL Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(JNIEnv *env, jclass cls, jint tipo, jint a, jint b) {
  (void)env; (void)cls;
  switch (tipo) {
    case EV_PRONTO:  if (!prontoLoad) marco("video: pronto (android)"); durMs = a; prontoLoad = 1; break;
    case EV_TOCANDO: tocando = 1; bufferando = 0; pausaVista = 0; if (!tocandoDesde) tocandoDesde = SDL_GetTicks() | 1; break;
    case EV_PAUSADO: tocando = 0; if (pausaPedida) pausaVista = 1; break;
    case EV_FIM:     terminou = 1; tocando = 0; break;
    // Sem `falhou` aqui: o video_bombear decide entre reconectar e desistir.
    case EV_ERRO:    reconErroCod = a; reconErroVideo = b == 2; /* Media3 C.TRACK_TYPE_VIDEO */
                     reconErroPend = 1; tocando = 0;
                     snprintf(erroTxt, sizeof erroTxt, "%d player error:%d", a, b);
                     printf("[video] android: erro do player %d (%d)\n", a, b); break;
    case EV_TAMANHO: largura = a; altura = b; break;
    case EV_BUFFER:
      if (a < 100 && !bufferando) { bufferando = 1; bufferDesde = SDL_GetTicks(); }
      else if (a >= 100) bufferando = 0;
      break;
    case EV_PRIMEIRO_QUADRO: marco("video: primeiro quadro (android)"); primeiroQuadro = 1; break;
    case EV_SUPERFICIE_ESTAVEL: superficieEstavel = 1; break;
    case EV_AUDIO_SEM_DECODER: semDecoderAudio = 1; break;
    default: break;
  }
  if (tipo != EV_BUFFER) { printf("[video] android evento %d (%d, %d)\n", tipo, a, b); fflush(stdout); }
}

// --- video.h (C -> Kotlin) -----------------------------------------------------
int  video_iniciar(void) {
  JNIEnv *env;
  if (!travaLeg) travaLeg = SDL_CreateMutex();
  env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  return env && garantirPonte(env);
}
int  video_iniciar_auto(void) { return video_iniciar(); }
int  video_registro_negado(void) { return 0; }

// Abre urlAtual no Kotlin. Serve a fonte nova e ao recarregar da reconexao.
static int abrirSessao(int inicioMs, int fracao) {
  unsigned preparado = preparoAdotar;
  preparoAdotar = 0;
  JNIEnv *env;
  ativo = 1; superficieEstavel = 0; prontoLoad = primeiroQuadro = falhou = terminou = tocando = 0;
  velEnviada = 100;
  largura = altura = durMs = posMs = posNativoMs = 0; bufferando = 1; bufferDesde = SDL_GetTicks();
  tocandoDesde = 0; semDecoderAudio = 0; erroTxt[0] = 0; reconErroVideo = 0;
  pausaPedida = pausaVista = 0;
  hdrAtual = "none"; dvAtual = atmosAtual = 0;
  pthread_mutex_lock(&travaStats); statsValido = 0; pthread_mutex_unlock(&travaStats);
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; legAte = 0;
  nNovasA = nNovasL = 0;
  temEnv = 0;   // a SurfaceView nova ainda nao recebeu janela nenhuma
  if (!travaLeg) travaLeg = SDL_CreateMutex();
  unsigned geracao = novaRetomada(-1);
  env = ambiente();
  if (!env) { falhou = 1; printf("[video] android: NvPlayer indisponivel\n"); fflush(stdout); return 0; }
  // F07: the cache limit is sticky in Kotlin (read at each open), so it only
  // crosses JNI when it changes. Posted before the open on the same main
  // thread queue, it is in place when the open runs.
  // #338: buffer MiB (0 = Auto), sticky in Kotlin like the cache limit.
  if (mBuffer && bufferPreset != bufferEnviado) {
    (*env)->CallStaticVoidMethod(env, gCls, mBuffer, (jint)bufferPreset);
    fimChamada(env);
    bufferEnviado = bufferPreset;
  }
  if (mCache && cacheSessao != cacheEnviado) {
    (*env)->CallStaticVoidMethod(env, gCls, mCache, (jint)cacheSessao);
    fimChamada(env);
    cacheEnviado = cacheSessao;
  }
  if (mTitulo && strcmp(tituloSessao, tituloEnviado)) {
    jstring t = paraJString(env, tituloSessao);
    if (t) {
      (*env)->CallStaticVoidMethod(env, gCls, mTitulo, t);
      (*env)->DeleteLocalRef(env, t);
      fimChamada(env);
      snprintf(tituloEnviado, sizeof tituloEnviado, "%s", tituloSessao);
    }
  }
  {
    jstring u = paraJString(env, urlAtual);
    jstring c = u && !(*env)->ExceptionCheck(env) ? paraJString(env, cabecalhos) : NULL;
    if (!u || !c) {
      if (u) (*env)->DeleteLocalRef(env, u);
      if (c) (*env)->DeleteLocalRef(env, c);
      fimChamada(env);
      if (preparado) video_precarregar_cancelar(preparado);
      falhou = 1; return 0;
    }
    // abrirPosicao also when starting at 0: it is how the Kotlin side learns
    // this session's generation (StreamFit passive telemetry). At 0 the
    // resume state stays -1, exactly as the plain abrir path left it.
    if (preparado && mAbrirPreparada) {
      jstring a = paraJString(env, ling_audio());
      if (!a) {
        (*env)->DeleteLocalRef(env, u); (*env)->DeleteLocalRef(env, c);
        fimChamada(env);
        video_precarregar_cancelar(preparado);
        falhou = 1; return 0;
      }
      if (inicioMs > 0 || fracao > 0) estadoRetomada(0);
      (*env)->CallStaticVoidMethod(env, gCls, mAbrirPreparada, u, c,
          (jint)inicioMs, (jint)fracao, (jint)geracao, (jint)preparado, a);
      (*env)->DeleteLocalRef(env, a);
      if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        estadoRetomada(-1);
        (*env)->CallStaticVoidMethod(env, gCls, mAbrir, u, c);
      }
    } else if (inicioMs <= 0 && fracao > 0 && mAbrirRetomada) {
      estadoRetomada(0);
      (*env)->CallStaticVoidMethod(env, gCls, mAbrirRetomada, u, c, (jint)0, (jint)fracao, (jint)geracao);
      if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        estadoRetomada(-1);
        printf("[video] retomada: abrirRetomada recusado, abre do inicio e o player busca depois\n");
        fflush(stdout);
        (*env)->CallStaticVoidMethod(env, gCls, mAbrir, u, c);
      }
    } else if (mAbrirPosicao) {
      if (fracao > 0 && inicioMs <= 0) {
        printf("[video] retomada: casca sem abrirRetomada, o player busca depois do pronto\n");
        fflush(stdout);
      }
      if (inicioMs > 0) estadoRetomada(0);
      (*env)->CallStaticVoidMethod(env, gCls, mAbrirPosicao, u, c, (jint)inicioMs, (jint)geracao);
      if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionClear(env);
        estadoRetomada(-1);
        (*env)->CallStaticVoidMethod(env, gCls, mAbrir, u, c);
      }
    } else (*env)->CallStaticVoidMethod(env, gCls, mAbrir, u, c);
    (*env)->DeleteLocalRef(env, u);
    (*env)->DeleteLocalRef(env, c);
  }
  int chamadaFalhou = (*env)->ExceptionCheck(env);
  fimChamada(env);
  if (chamadaFalhou) { falhou = 1; return 0; }
  // Every Kotlin open starts at 100%: a player session re-applies its volume
  // (source change, reconnection) after the open in the same queue.
  ganhoEnviado = CB_VOL_NORMAL;
  // Numero medido na TV TCL de 2,4 GB: 217 MB de textura + 96 MB de buffer +
  // paralelo/preparo levaram o app a ~575 MB de PSS e o lmkd matou. So o
  // player em tela cheia esconde a arte da home; trailers (home/detalhe) nao.
  // Ligado so depois de abrir: falha no meio nao deixa o teto preso.
  tex_pressao_reproducao(playerSessao);
  if (playerSessao && cacheboost_volume() != CB_VOL_NORMAL) cacheboost_backend_ganho(cacheboost_volume());
  return 1;
}

int video_tocar_retomada(const char *u, double segundos, double pct) {
  int inicioMs = isfinite(segundos) && segundos > 0.0 && segundos <= INT_MAX / 1000.0
    ? (int)(segundos * 1000.0) : 0;
  // Centesimos de ponto percentual; segundos exatos vencem (abrirPosicao).
  int fracao = !inicioMs && isfinite(pct) && pct > 0.0 && pct < 100.0
    ? (int)(pct * 100.0 + 0.5) : 0;
  snprintf(urlAtual, sizeof urlAtual, "%s", u ? u : "");
  capmkv_iniciar(urlAtual);
  nv_recon_zerar(&recon);
  reconPermitida = reconProxima; reconProxima = 0;
  reconIniciou = 0; reconErroPend = 0;
  reconAudio = reconLeg = -1; reconFaixasPend = 0; reconBuscarMs = -1; reconPausado = 0;
  recarregarPausa = -1;
  playerSessao = cacheArmado >= 0;
  cacheSessao = cacheArmado > 0 ? cacheArmado : 0;
  cacheArmado = -1;
  snprintf(tituloSessao, sizeof tituloSessao, "%s", playerSessao ? tituloArmado : "");
  tituloArmado[0] = 0;
  return abrirSessao(inicioMs, fracao);
}
int video_tocar_posicao(const char *u, double segundos) { return video_tocar_retomada(u, segundos, 0.0); }

void cacheboost_backend_buffer(int mb) { bufferPreset = mb >= 0 && mb <= 256 ? mb : 0; }
void cacheboost_backend_cache(int mb) { cacheArmado = mb > 0 ? mb : 0; }
void cacheboost_backend_titulo(const char *id) { snprintf(tituloArmado, sizeof tituloArmado, "%s", id ? id : ""); }
void cacheboost_backend_descartar(void) { if (mDescartar) kSemArg(mDescartar); }
void cacheboost_backend_morno(int ligado) { if (mMorno) kInt(mMorno, ligado); }
int cacheboost_backend_quadro(int acao) {
  JNIEnv *env = ambiente(); int r = 0;
  if (env && mQuadro) { r = (*env)->CallStaticIntMethod(env, gCls, mQuadro, acao); fimChamada(env); }
  return r;
}
void cacheboost_backend_ganho(int pct) {
  if (pct < CB_VOL_MIN) pct = CB_VOL_MIN;
  if (pct > CB_VOL_MAX) pct = CB_VOL_MAX;
  if (!mGanho || !ativo || pct == ganhoEnviado) return;
  ganhoEnviado = pct;
  kInt(mGanho, pct);
}
int video_tocar(const char *u) { return video_tocar_posicao(u, 0.0); }

void video_definir_reconexao(int sim) { reconProxima = sim ? 1 : 0; }

// SEEK ATE O FIM DO ARQUIVO. Segurar a direita na barra mandava o seek para o
// ultimo milissegundo (2547127 de 2547128 ms, Shield, 17/10/2026): o Media3
// falha o load ali (IllegalStateException, erro 2000), o erro 2000 conta como
// rede, e a reconexao retomava na MESMA posicao, tres vezes, ate o cartao de
// erro. Nenhum alvo entra nos ultimos VIDEO_FIM_MARGEM_MS: o fim toca e
// termina pelo caminho normal (EV_FIM: visto, proximo episodio).
#define VIDEO_FIM_MARGEM_MS 2000
static int foraDoFim(int ms) {
  int d = durMs;
  if (d > 2 * VIDEO_FIM_MARGEM_MS && ms > d - VIDEO_FIM_MARGEM_MS) return d - VIDEO_FIM_MARGEM_MS;
  return ms;
}

int video_recarregar(void) {
  int alvo = foraDoFim(posNativoMs), pausado = pausaPedida;
  if (!ativo || !prontoLoad || !urlAtual[0] || falhou || terminou ||
      reconErroPend || video_reconectando() || recarregarPausa >= 0 ||
      alvo < 0 || !ambiente()) return 0;
  reconAudio = audioAtual; reconLeg = legAtual; reconFaixasPend = 1;
  reconBuscarMs = -1;
  recarregarPosMs = alvo;
  if (!abrirSessao(alvo, 0)) { recarregarPausa = -1; return 0; }
  recarregarPausa = pausado;
  // As duas chamadas entram na mesma fila do Kotlin: a pausa pertence ao
  // player novo, antes do prepare completar, sem alterar o audio preferido.
  if (pausado) video_pausar(1);
  return 1;
}
int  video_reconectando(void) {
  return nv_recon_ativa(&recon) && (recon.pendente || !prontoLoad) ? recon.tentativa : 0;
}

void video_bombear(void) {
  double pos = video_pos();
  int eraRecarga = recarregarPausa >= 0;
  if (recarregarPausa >= 0) {
    if (falhou || terminou || !ativo) recarregarPausa = -1;
    else if (prontoLoad && !reconErroPend && !recon.pendente) {
      // Cascas antigas podem recusar a posicao de abertura. Nesse caso a
      // mesma sessao recebe o seek, inclusive se a pessoa estava pausada.
      if (recarregarPosMs > 0 && video_retomada_inicial_estado() < 0)
        kInt(mBuscar, recarregarPosMs);
      if (recarregarPausa) video_pausar(1);
      recarregarPausa = -1;
    }
  }
  if (mVelocidade && ativo && prontoLoad && velEnviada != velPedida) {
    velEnviada = velPedida;
    kInt(mVelocidade, velEnviada);
    // So "enviado": o setPlaybackSpeed nao recusa nada, e quem prova que andou
    // e o medidor do player (VelMedidor), pela posicao contra o relogio.
    vel_log(velEnviada, "enviado ao ExoPlayer");
  }
  if (prontoLoad && pos > 0.5) reconIniciou = 1;
  if (prontoLoad && reconBuscarMs < 0) nv_recon_progresso(&recon, pos);
  // O seek do recarregar sai com o player ja tocando.
  if (reconBuscarMs >= 0 && prontoLoad && (tocando || reconPausado)) {
    kInt(mBuscar, reconBuscarMs);
    printf("[video] reconexao: retomado em %ds\n", reconBuscarMs / 1000);
    fflush(stdout);
    reconBuscarMs = -1;
  }
  if (reconErroPend) {
    int antes = recon.tentativa;
    int rede = erroDeRede(reconErroCod);
    reconErroPend = 0;
    if (reconPermitida && urlAtual[0] && (reconIniciou || recon.tentativa) &&
        nv_recon_erro(&recon, rede, SDL_GetTicks(),
                      eraRecarga ? recarregarPosMs / 1000.0 : pos)) {
      if (!antes && !eraRecarga) {
        reconAudio = audioAtual; reconLeg = legAtual;
        reconPausado = pausaPedida;
      }
      if (recon.tentativa != antes) {
        printf("[video] conexao caiu (%d): tentativa %d/%d, espera %us\n",
               reconErroCod, recon.tentativa, NV_RECON_MAX,
               nv_recon_espera_ms(recon.tentativa) / 1000u);
        fflush(stdout);
      }
      bufferando = 0;
    } else {
      if (recon.esgotou) { printf("[video] reconexao: desistiu depois de %d tentativas\n", NV_RECON_MAX); fflush(stdout); }
      falhou = 1;
    }
  }
  if (falhou) recarregarPausa = -1;
  if (nv_recon_vencida(&recon, SDL_GetTicks())) {
    printf("[video] reconectando: alvo %.0fs, tentativa %d\n", recon.alvo, recon.tentativa);
    fflush(stdout);
    reconFaixasPend = 1;
    reconBuscarMs = recarregarPausa < 0 && recon.alvo > 1.0 ? foraDoFim((int)(recon.alvo * 1000.0)) : -1;
    if (!abrirSessao(recarregarPausa >= 0 ? recarregarPosMs : 0, 0))
      { reconErroCod = -1; reconErroPend = 1; }
    else if (recarregarPausa > 0) video_pausar(1);
    else if (recarregarPausa < 0 && reconPausado) {
      // abrirSessao nasce tocando. Enfileirar pause logo depois da abertura
      // conserva o estado de um seek feito pausado; o seek de retomada acima
      // aceita prontoLoad mesmo sem EV_TOCANDO.
      pausaPedida = 1; pausaVista = 0;
      kInt(mPausar, 1);
    }
  }
}
void video_parar(void) {
  novaRetomada(-1);
  capmkv_zerar();   // fio de capitulos em voo nao alimenta o proximo titulo
  nv_recon_zerar(&recon);
  reconErroPend = 0; reconErroVideo = 0; reconFaixasPend = 0; reconBuscarMs = -1; reconPausado = 0;
  recarregarPausa = -1;
  if (ativo) {
    // Troca de episodio: o consumidor ja destacou o proximo buffer. Soltar
    // o decoder atual nao pode cancelar esse preparo nem o token da abertura.
    JNIEnv *env = ambiente();
    if (preparoAdotar && env && mPararMantendoPreparo)
      kInt(mPararMantendoPreparo, (int)preparoAdotar);
    else { preparoAdotar = 0; kSemArg(mParar); }
  }
  ativo = prontoLoad = primeiroQuadro = tocando = 0; superficieEstavel = 0;
  pausaPedida = pausaVista = 0;
  tex_pressao_reproducao(0);
}
void video_pausar(int p) {
  pausaVista = 0; pausaPedida = p ? 1 : 0;
  if (nv_recon_ativa(&recon)) reconPausado = pausaPedida;
  if (recarregarPausa >= 0) recarregarPausa = pausaPedida;
  kInt(mPausar, pausaPedida);
}
int video_pausa_confirmada(void) {
  return pausaPedida && pausaVista && !tocando && ativo && prontoLoad && primeiroQuadro &&
         !falhou && !terminou && !video_reconectando();
}

void video_volume(int pct) { kInt(mVolume, pct); }
void video_buscar(double s) {
  posMs = foraDoFim((int)(s * 1000.0));   // a barra nao pode voltar enquanto o seek corre
  kInt(mBuscar, posMs);
  terminou = 0;
}
// `encaixa` = 1: o quadro ENCAIXA no retangulo mantendo a proporcao (tarja),
// que e o que o plano de video da LG e o LetterBox da Samsung fazem com uma
// janela lisa — o nucleo conta com isso (trailer "Original", janela antes do
// videoInfo). 0: o retangulo e exato, ja calculado com a proporcao (recorte).
// ULTIMA JANELA ENTREGUE (dedupe). O nucleo manda o retangulo de novo quando
// dois modos de aspecto coincidem no mesmo filme — Ajustar largura e Original
// num 2.39:1, por exemplo — e a cada releitura do videoInfo; o log da TCL
// (11/10) mostrou "1920x1037" pedido duas vezes seguidas. O Kotlin nao faria
// nada com o repetido (layout identico) e o dvtrace enchia de "changed" sem
// mudanca. O reaplicar do trailer chama janelaKt DIRETO, fora do dedupe: ele
// existe justamente para reenviar. abrirSessao zera — a SurfaceView de cada
// abertura comeca sem janela nenhuma e precisa receber ate o retangulo igual.
static void janelaKt(int x, int y, int w, int h, int encaixa) {
  JNIEnv *env = ambiente();
  if (!env) return;   // sem JNI nao ha entrega: o dedupe so lembra do que chegou
  envX = x; envY = y; envW = w; envH = h; envEnc = encaixa; temEnv = 1;
  (*env)->CallStaticVoidMethod(env, gCls, mJanela, (jint)x, (jint)y, (jint)w, (jint)h, (jint)encaixa);
  fimChamada(env);
}
static void janelaSeMudou(int x, int y, int w, int h, int encaixa) {
  if (temEnv && x == envX && y == envY && w == envW && h == envH && encaixa == envEnc) return;
  janelaKt(x, y, w, h, encaixa);
}
void video_janela(int x, int y, int w, int h) { janelaSeMudou(x, y, w, h, 1); }

// RECORTE DE FONTE PELO RETANGULO DE DESTINO AMPLIADO. Mesma conta do ROI do
// .tpk (video_tpk.c, #178): desenhar o recorte (sx,sy,sw,sh) dentro de
// (dx,dy,dw,dh) e o MESMO que desenhar o quadro INTEIRO num retangulo maior,
// deslocado para o pedaco desejado cair sobre o destino:
//   escala = dw/sw, dh/sh;  W,H = quadro * escala;  X,Y = d - s * escala.
// Aqui o retangulo pode sair da tela e ter origem negativa: o Kotlin aplica por
// layout na SurfaceView e o pai recorta o excedente, sem a restricao do Tizen.
static int ultX, ultY, ultW, ultH, temUlt;
void video_janela_fonte(int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) {
  double qw = largura, qh = altura, ex, ey;
  int X, Y, W, H;

  // Sem as dimensoes do quadro nao ha recorte a calcular: janela lisa (encaixa).
  if (qw < 2.0 || qh < 2.0 || sw <= 0 || sh <= 0) { temUlt = 0; video_janela(dx, dy, dw, dh); return; }
  // Quadro INTEIRO num destino que o chamador ja calculou (Original encaixado,
  // Esticar, Encaixar altura): o destino e EXATO, nao se encaixa de novo —
  // senao Esticar virava Original.
  if (sx <= 0 && sy <= 0 && sw >= (int)qw && sh >= (int)qh) { temUlt = 0; janelaSeMudou(dx, dy, dw, dh, 0); return; }

  ex = (double)dw / (double)sw;
  ey = (double)dh / (double)sh;
  W  = (int)(qw * ex + 0.5);
  H  = (int)(qh * ey + 0.5);
  X  = (int)(dx - sx * ex + (dx - sx * ex < 0 ? -0.5 : 0.5));
  Y  = (int)(dy - sy * ey + (dy - sy * ey < 0 ? -0.5 : 0.5));

  printf("[video] android recorte %d,%d %dx%d de %.0fx%.0f -> janela %d,%d %dx%d\n",
         sx, sy, sw, sh, qw, qh, X, Y, W, H);
  fflush(stdout);
  ultX = X; ultY = Y; ultW = W; ultH = H; temUlt = 1;
  janelaSeMudou(X, Y, W, H, 0);
}
int  video_recorte_fonte(void) { return 1; }
void video_recorte_reaplicar(void) { if (temUlt) janelaKt(ultX, ultY, ultW, ultH, 0); }
// O Kotlin escala 1920x1080 de layout para a camada real; nada a fazer aqui.
void video_escala_definir(int sw, int sh) { (void)sw; (void)sh; }

const char *video_url_atual(void) { return urlAtual; }
double video_pos(void) { return prontoLoad ? posMs / 1000.0 : 0; }
double video_duracao(void) { return durMs / 1000.0; }
// Capitulos do MKV lidos por um fio lateral (capmkv.c, 203-capitulos).
double video_creditos(void) { return capmkv_creditos(video_duracao()); }
double video_buffer_fim(void) { return 0; }
unsigned video_bufferando_ms(void) {
  // Esperando para reconectar: o watchdog (app.c) nao troca de fonte.
  if (nv_recon_ativa(&recon)) return 0;
  return bufferando ? SDL_GetTicks() - bufferDesde : 0;
}
void video_definir_dv(int dv) { (void)dv; }
void video_definir_cabecalhos(const char *c) { snprintf(cabecalhos, sizeof cabecalhos, "%s", c ? c : ""); }
const char *video_cabecalhos(void) { return cabecalhos; }
void video_definir_mp4(int m) { (void)m; }
int  video_tocando(void) { return tocando; }
// 1 SO COM IMAGEM: e o que abre o furo (player_com_video). Fonte sem quadro
// (so audio) cai no prazo de ANDROID_FURO_PRAZO_MS depois de tocar.
int  video_pronto(void) {
  Uint32 desde;
  if (!prontoLoad) return 0;
  if (primeiroQuadro) return 1;
  desde = tocandoDesde;
  return desde && tocando && SDL_GetTicks() - desde >= ANDROID_FURO_PRAZO_MS;
}
int  video_ativo(void) { return ativo; }
int  video_superficie_estavel(void) { return superficieEstavel; }
int  video_quadro_visto(void) { return primeiroQuadro; }
int  video_falhou(void) { return falhou; }
int  video_audio_nao_suportado(void) { return semDecoderAudio; }
int  video_seek_desistiu(void) { return 0; }
int  video_terminou(void) { return terminou; }
// Foco de audio perdido = pausa (NvPlayer); nao ha "outro app com o video".
int  video_conflito_recurso(void) { return conflito; }
const char *video_erro_texto(void) { return falhou ? erroTxt : ""; }
int video_erro_decoder_codigo(void) { return falhou && reconErroVideo ? reconErroCod : 0; }
// O Media3 nao da o sinal de decoder anunciado: valem os neutros do .tpk.
int  video_decoder_anunciou(void) { return 1; }

int  video_n_audio(void) { return nAudio; }
int  video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_audio(int i) { return (i >= 0 && i < nAudio) ? &faixaAudio[i] : 0; }
const VideoFaixa *video_legenda(int i) { return (i >= 0 && i < nLeg) ? &faixaLeg[i] : 0; }
int  video_legenda_ordinal_mkv(int i) { return (i >= 0 && i < nLeg) ? faixaLeg[i].ordinalMkv : -1; }
// "Nao ha sonda": sem libass (NV_ASS_LIBASS) a sonda de MKV nao serve a nada.
// TODO quando o libass entrar: sonda real, como video.c.
int  video_mkv_sondado(void) { return 2; }
void video_sondar_mkv_agora(void) {}
int  video_audio_atual(void) { return audioAtual; }
int  video_legenda_atual(void) { return legAtual; }
void video_escolher_audio(int i) {
  if (i < 0 || i >= nAudio) return;
  audioAtual = i;
  {
    JNIEnv *env = ambiente();
    if (!env) return;
    (*env)->CallStaticVoidMethod(env, gCls, mEscolher, (jint)0, (jint)faixaAudio[i].numero);
    fimChamada(env);
  }
}
void video_escolher_legenda(int i) {
  if (i >= nLeg) return;
  legAtual = i;
  if (travaLeg) { SDL_LockMutex(travaLeg); legTexto[0] = 0; legAte = 0; SDL_UnlockMutex(travaLeg); }
  {
    JNIEnv *env = ambiente();
    if (!env) return;
    // -1 desliga a legenda no Kotlin.
    (*env)->CallStaticVoidMethod(env, gCls, mEscolher, (jint)1, (jint)(i >= 0 ? faixaLeg[i].numero : -1));
    fimChamada(env);
  }
}
int  video_legenda_nativa(char *d, int t) {
  if (!d || t < 2) return 0;
  d[0] = 0;
  if (!ativo || legAtual < 0 || !travaLeg) return 0;
  SDL_LockMutex(travaLeg);
  if (legTexto[0] && (Sint32)(legAte - SDL_GetTicks()) > 0) snprintf(d, (size_t)t, "%s", legTexto);
  SDL_UnlockMutex(travaLeg);
  return d[0] != 0;
}
void video_legenda_externa(const char *u) { (void)u; }
// O atraso do estilo vale para a embutida: o Kotlin guarda e adia os cues
// (so atraso positivo; adiantar um cue que ainda nao chegou nao da).
void video_legenda_estilo(const VideoLegendaEstilo *e) {
  static int ultAtraso = 0x7fffffff;
  if (!e || e->atrasoMs == ultAtraso) return;
  ultAtraso = e->atrasoMs;
  {
    JNIEnv *env = ambiente();
    if (!env) return;
    (*env)->CallStaticVoidMethod(env, gCls, mEscolher, (jint)2, (jint)e->atrasoMs);
    fimChamada(env);
  }
}
int  video_tem_atmos(void) { return atmosAtual; }
int  video_tem_dolby_vision(void) { return dvAtual; }
const char *video_hdr(void) { return (const char *)hdrAtual; }
int  video_largura(void) { return largura; }
int  video_altura(void) { return altura; }
int  video_pode_forcar_sdr(void) { return 0; }
int  video_velocidade_suportada(void) { return mVelocidade != NULL && !velRecusada; }
void video_velocidade(int c) { if (!velRecusada) velPedida = (c <= 0 || c > 400) ? 100 : c; }
int  video_velocidade_atual(void) { return velRecusada ? 100 : velPedida; }
void video_velocidade_recusada(void) { velRecusada = 1; velPedida = 100; }   // o bombear manda o 100
// PASSTHROUGH (medido na TCL, 06/10: EAC3 ao receptor, 1,5x pedido, video a
// 1x). Trocar o audio para PCM decodificado enquanto a velocidade != 1 exigiria
// reabrir o player com outra cadeia de audio no meio do filme (e o receptor
// perderia o Atmos/5.1 por isso): nao e uma mudanca contida. A linha fica
// esmaecida com o motivo.
int  video_velocidade_bloqueada(void) { return cacheboost_ganho_estado() == CB_GANHO_PASSTHROUGH; }
void video_forcar_sdr(void) {}
void video_encerrar(void) { video_parar(); }
#endif
