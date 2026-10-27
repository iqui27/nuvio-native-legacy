// JNI falsa sobre o backend real: aceita/rejeita o novo prepare e entrega
// acknowledgements atrasados. Nao depende da JVM, SDL dinamico ou TV.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"
#include "../src/cacheboost.c"   // F07: video_android reads the session volume
#include <assert.h>
#include "../src/livestall.h"
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

int xtream_url_limitada(const char *u) { (void)u; return 0; }
int xtream_conexoes_limitadas(void) { return 0; }
int xtream_conta_ler(XtreamConta *c) { memset(c, 0, sizeof *c); return 0; }
int stream_kbps_atual(void) { return 0; }
static int cap4k[4];
void stream_definir_decoder4k(int h, int a, int v, int av) {
  cap4k[0] = h; cap4k[1] = a; cap4k[2] = v; cap4k[3] = av;
}
#ifdef NV_NDK_JNI
static struct JNINativeInterface jni;
#else
static struct JNINativeInterface_ jni;
#endif
static JNIEnv env = &jni;
static int chamadasNormais, chamadasPosicao, recebidoMs, recebidoGeracao, excecao, lancar;
static int ausente, ausenteFracao, chamadasFracao, recebidoFracao;
static int chamadasPausa, ultimoPausar, chamadasBusca, ultimoBuscar;
static int lobbyPreload, chamadasPausadas;
int juntos_precarregando(void) { return lobbyPreload; }
static unsigned relogio = 100;
void *SDL_AndroidGetJNIEnv(void) { return &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
Uint64 SDL_GetTicks64(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
const char *ling_audio(void) { return ""; }
const char *ling_audio_preferido(void) { return ""; }
int ajustes_audio_normalizar(void) { return 0; }
void marco(const char *s) { (void)s; }
void tex_pressao_reproducao(int l) { (void)l; }
void vel_log(int cent, const char *estado) { (void)cent; (void)estado; }

static jobject JNICALL global(JNIEnv *e, jobject o) { (void)e; return o; }
static jmethodID JNICALL metodo(JNIEnv *e, jclass c, const char *nome, const char *sig) {
  (void)e; (void)c;
  if (!strcmp(nome, "abrirPosicao")) {
    assert(!strcmp(sig, "(Ljava/lang/String;Ljava/lang/String;II)V"));
    if (ausente) { excecao = 1; return NULL; }
    return (jmethodID)(uintptr_t)2;
  }
  if (!strcmp(nome, "abrirRetomada")) {
    assert(!strcmp(sig, "(Ljava/lang/String;Ljava/lang/String;III)V"));
    if (ausenteFracao) { excecao = 1; return NULL; }
    return (jmethodID)(uintptr_t)5;
  }
  if (!strcmp(nome, "abrirPausada")) {
    assert(!strcmp(sig, "(Ljava/lang/String;Ljava/lang/String;IIIILjava/lang/String;)V"));
    return (jmethodID)(uintptr_t)9;
  }
  if (!strcmp(nome, "kbps")) return (jmethodID)(uintptr_t)8;
  if (!strcmp(nome, "pausar")) return (jmethodID)(uintptr_t)6;
  if (!strcmp(nome, "buscar")) return (jmethodID)(uintptr_t)7;
  return (jmethodID)(uintptr_t)1;
}
static jstring JNICALL texto(JNIEnv *e, const jchar *u, jsize n) {
  (void)e; (void)u; (void)n; return (jstring)malloc(1);
}
static void JNICALL apagar(JNIEnv *e, jobject o) { (void)e; free(o); }
static jboolean JNICALL temExcecao(JNIEnv *e) { (void)e; return excecao; }
static void JNICALL limparExcecao(JNIEnv *e) { (void)e; excecao = 0; }
static void JNICALL chamar(JNIEnv *e, jclass c, jmethodID m, ...) {
  (void)e; (void)c;
  if (m == mParar || (uintptr_t)m == 8) return;
  if (m == mPausar) {
    va_list ap; va_start(ap, m); ultimoPausar = va_arg(ap, jint); va_end(ap);
    chamadasPausa++; return;
  }
  if (m == mBuscar) {
    va_list ap; va_start(ap, m); ultimoBuscar = va_arg(ap, jint); va_end(ap);
    chamadasBusca++; return;
  }
  va_list ap; va_start(ap, m);
  (void)va_arg(ap, jstring); (void)va_arg(ap, jstring);
  if (m == mAbrirPausada) {
    chamadasPausadas++;
    recebidoMs = va_arg(ap, jint); recebidoFracao = va_arg(ap, jint);
    recebidoGeracao = va_arg(ap, jint);
    (void)va_arg(ap, jint); (void)va_arg(ap, jstring);
  } else if (m == mAbrirRetomada) {
    chamadasFracao++;
    recebidoMs = va_arg(ap, jint); recebidoFracao = va_arg(ap, jint);
    recebidoGeracao = va_arg(ap, jint);
    if (lancar) excecao = 1;
  } else if (m == mAbrirPosicao) {
    chamadasPosicao++;
    recebidoMs = va_arg(ap, jint); recebidoGeracao = va_arg(ap, jint);
    if (lancar) excecao = 1;
  } else chamadasNormais++;
  va_end(ap);
}
int main(int argc, char **argv) {
  assert(argc == 2); // eventos produzidos pelo abrirMain real no teste JVM
  Java_space_nuvio_nativelegacy_NvPlayer_nativeDecoder4k(NULL, NULL, 1, 0, -1, 1);
  assert(cap4k[0] == 1 && cap4k[1] == 0 && cap4k[2] == -1 && cap4k[3] == 1);

  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = chamar;
  // IDs distintos so para o falso separar parar de abrir.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);
  mParar = (jmethodID)(uintptr_t)4;
  lobbyPreload = 1;
  assert(video_tocar_retomada("https://example.invalid/lobby.mp4", 12, 0));
  assert(chamadasPausadas == 1 && pausaPedida && recebidoMs == 12000);
  video_pausar(0);
  assert(!pausaPedida); // Room Go releases the prepared player.
  lobbyPreload = 0;
  assert(video_tocar_posicao("https://example.invalid/um.mp4", 612.345));
  assert(chamadasPosicao == 1 && recebidoMs == 612345 && chamadasNormais == 0);
  assert(video_retomada_inicial_estado() == 0);
  int antigo = recebidoGeracao;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, antigo, 1);
  assert(video_retomada_inicial_estado() == 1);

  assert(video_tocar_posicao("https://example.invalid/dois.mp4", 120));
  assert(recebidoGeracao != antigo && video_retomada_inicial_estado() == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, antigo, 1);
  assert(video_retomada_inicial_estado() == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, recebidoGeracao, 0);
  assert(video_retomada_inicial_estado() == -1);
  int atual = recebidoGeracao;
  video_parar();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, atual, 1);
  assert(video_retomada_inicial_estado() == -1);
  puts("ok JNI posicao imutavel, ack da geracao e cancelamento");

  lancar = 1;
  int normais = chamadasNormais;
  assert(video_tocar_posicao("https://example.invalid/fallback.mp4", 50));
  assert(chamadasNormais == normais + 1 && video_retomada_inicial_estado() == -1 && !excecao);
  lancar = 0; ausente = 1;
  assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  normais = chamadasNormais;
  assert(video_tocar_posicao("https://example.invalid/legado.mp4", 50));
  assert(chamadasNormais == normais + 1 && video_retomada_inicial_estado() == -1);
  puts("ok excecao e ponte anterior recuam ao abrir normal");

  int posicoes = chamadasPosicao;
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", NAN));
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", INFINITY));
  assert(video_tocar_posicao("https://example.invalid/invalido.mp4", 2147484));
  assert(video_tocar("https://example.invalid/inicio.mp4"));
  assert(chamadasPosicao == posicoes);
  // StreamFit (F03): with the current shell, a start at 0 also goes through
  // abrirPosicao so Kotlin learns the generation; resume state stays -1.
  ausente = 0; assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  normais = chamadasNormais;
  assert(video_tocar("https://example.invalid/zero.mp4"));
  assert(chamadasPosicao == posicoes + 1 && chamadasNormais == normais && recebidoMs == 0);
  assert((unsigned)recebidoGeracao == video_android_sessao() && video_retomada_inicial_estado() == -1);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, recebidoGeracao, 0);
  assert(video_retomada_inicial_estado() == -1);
  puts("ok inicio em 0 entrega a geracao sem mudar a retomada");

  // 2.0.3: so o percentual (retomada da conta, sem registro local). Vai na
  // abertura em centesimos de ponto; o Kotlin aplica com a duracao do
  // container antes do primeiro quadro e confirma pela geracao.
  int fracoes = chamadasFracao; posicoes = chamadasPosicao;
  assert(video_tocar_retomada("https://example.invalid/conta.mkv", 0, 2));
  assert(chamadasFracao == fracoes + 1 && chamadasPosicao == posicoes);
  assert(recebidoMs == 0 && recebidoFracao == 200 && video_retomada_inicial_estado() == 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeRetomada(&env, NULL, recebidoGeracao, 1);
  assert(video_retomada_inicial_estado() == 1);
  // Segundos exatos vencem o percentual: abrirPosicao de sempre.
  fracoes = chamadasFracao;
  assert(video_tocar_retomada("https://example.invalid/local.mkv", 612.345, 34));
  assert(chamadasFracao == fracoes && recebidoMs == 612345 && video_retomada_inicial_estado() == 0);
  // Percentual invalido nao vira posicao.
  assert(video_tocar_retomada("https://example.invalid/x.mkv", 0, NAN));
  assert(video_tocar_retomada("https://example.invalid/x.mkv", 0, 100));
  assert(video_tocar_retomada("https://example.invalid/x.mkv", 0, -3));
  assert(chamadasFracao == fracoes && video_retomada_inicial_estado() == -1);
  // Excecao no Kotlin: abre do jeito de sempre e o C recua ao seek (-1).
  lancar = 1; normais = chamadasNormais;
  assert(video_tocar_retomada("https://example.invalid/excecao.mkv", 0, 34));
  assert(chamadasNormais == normais + 1 && video_retomada_inicial_estado() == -1 && !excecao);
  lancar = 0;
  // Casca sem abrirRetomada: abrirPosicao em 0, estado -1, seek tardio.
  ausenteFracao = 1; assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  fracoes = chamadasFracao; posicoes = chamadasPosicao;
  assert(video_tocar_retomada("https://example.invalid/casca.mkv", 0, 34));
  assert(chamadasFracao == fracoes && chamadasPosicao == posicoes + 1 && recebidoMs == 0);
  assert(video_retomada_inicial_estado() == -1);
  ausenteFracao = 0; assert(resolverMetodos(&env)); mParar = (jmethodID)(uintptr_t)4;
  puts("ok percentual na abertura, segundos vencem, invalido e casca antiga recuam");
  // #409 R2: mesmos codigos Media3 em audio/video/desconhecido. So video
  // pode alimentar o bloqueio por codec nos dois caminhos do automatico.
  const int codigos[] = {4001, 4003, 4004, 4005};
  for (unsigned i = 0; i < sizeof codigos / sizeof *codigos; i++) {
    for (int renderer = -1; renderer <= 2; renderer++) {
      Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, codigos[i], renderer);
      video_bombear();
      assert(video_falhou());
      assert(video_erro_decoder_codigo() == (renderer == 2 ? codigos[i] : 0));
    }
  }
  video_parar(); assert(video_erro_decoder_codigo() == 0);
  assert(video_tocar("https://example.invalid/novo.mkv"));
  assert(video_erro_decoder_codigo() == 0 && !video_falhou());
  puts("ok #409 JNI: audio/desconhecido nao bloqueiam video; reset ao parar/abrir");

  // Queda provocada por seek enquanto pausado: a reabertura deve continuar
  // pausada e aplicar o alvo mesmo sem receber EV_TOCANDO.
  video_parar(); video_definir_reconexao(1);
  assert(video_tocar("https://example.invalid/pausado.mkv"));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 7200000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRIMEIRO_QUADRO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2451000, -1, JNI_FALSE, -1, (jint)video_android_sessao());
  video_bombear();
  video_pausar(1);
  int pausas = chamadasPausa, buscas = chamadasBusca;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, -1);
  video_bombear();
  relogio += 3000; video_bombear();
  assert(chamadasPausa == pausas + 1 && ultimoPausar == 1);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 7200000, 0);
  video_bombear();
  assert(chamadasBusca == buscas + 1 && ultimoBuscar == 2451000);
  assert(video_pausa_confirmada() == 0); // espera o ack do player novo
  FILE *eventos = fopen(argv[1], "r"); assert(eventos);
  int evento, total = 0;
  while (fscanf(eventos, "%d", &evento) == 1) {
    Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, evento, 0, 0);
    total++;
  }
  fclose(eventos); assert(total > 0);
  assert(video_pausa_confirmada() == 0); // falta o primeiro quadro
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRIMEIRO_QUADRO, 0, 0);
  assert(video_pausa_confirmada());
  puts("ok reconexao de seek pausado conserva pausa e posicao");

  // A completed paused reconnect must not override a later playing reload.
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2461000, -1, JNI_FALSE, -1, (jint)video_android_sessao());
  video_bombear();
  assert(!nv_recon_ativa(&recon));
  video_pausar(0);
  assert(video_recarregar());
  pausas = chamadasPausa;
  posicoes = chamadasPosicao;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, -1);
  video_bombear(); relogio += 3000; video_bombear();
  assert(chamadasPosicao == posicoes + 1 && recebidoMs == 2461000);
  assert(chamadasPausa == pausas && !pausaPedida);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 7200000, 0);
  video_bombear();
  assert(chamadasPausa == pausas && !pausaPedida);
  puts("ok playing reload retry ignores completed reconnect pause");

  // A intencao de pausar vale mesmo se a queda chegar antes do ack.
  video_parar(); video_definir_reconexao(1);
  assert(video_tocar("https://example.invalid/pausa-sem-ack.mkv"));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 7200000, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2451000, -1, JNI_FALSE, -1, (jint)video_android_sessao());
  video_bombear(); video_pausar(1);
  pausas = chamadasPausa;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, -1);
  video_bombear(); relogio += 3000; video_bombear();
  assert(chamadasPausa == pausas + 1 && ultimoPausar == 1);
  puts("ok reconexao conserva pausa pedida antes do ack");
  int livePosicoes = chamadasPosicao, liveNormais = chamadasNormais;
  assert(video_tocar_posicao("https://example.invalid/live-window-test.m3u8", 612.345));
  // The live-window grace period belongs only to this open, is clamped,
  // expires without a Kotlin callback, and is cleared by READY/stop messages.
  unsigned liveSessao = video_android_sessao();
  unsigned liveInicio = relogio;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao);
  assert(video_recuperando_live() && chamadasPosicao == livePosicoes + 1 && chamadasNormais == liveNormais);
  relogio = liveInicio + 7999; assert(video_recuperando_live());
  relogio++; assert(!video_recuperando_live());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, INT_MAX, (int)liveSessao);
  assert(video_recuperando_live());
  relogio += 59999; assert(video_recuperando_live());
  relogio++; assert(!video_recuperando_live());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 0, (int)liveSessao);
  assert(!video_recuperando_live());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao - 1);
  assert(!video_recuperando_live());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao);
  video_parar(); assert(!video_recuperando_live());
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao);
  assert(!video_recuperando_live());
  assert(video_tocar_posicao("https://example.invalid/live-window-test.m3u8", 612.345));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_LIVE_RECOVERY, 8000, (int)liveSessao);
  assert(!video_recuperando_live());
  puts("ok bounded live recovery, native expiry, stale sessions and stop; no automatic reopen");
  unsigned segmentSession = video_android_sessao();
  assert(video_live_segmento_ms()==0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,12000,(int)segmentSession);
  assert(video_live_segmento_ms()==12000);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,6000,(int)segmentSession-1);
  assert(video_live_segmento_ms()==12000);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,INT_MAX,(int)segmentSession);
  assert(video_live_segmento_ms()==60000);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,-1,(int)segmentSession);
  assert(video_live_segmento_ms()==0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,12000,(int)segmentSession);
  video_parar(); assert(video_live_segmento_ms()==0);
  assert(video_tocar_posicao("https://example.invalid/next.m3u8",0));
  assert(video_live_segmento_ms()==0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_LIVE_SEGMENT,12000,(int)segmentSession);
  assert(video_live_segmento_ms()==0);
  puts("ok HLS target duration bounds, stale sessions, stop and next open");
  // Real JNI: an advancing negative HLS position must not look frozen at zero.
  unsigned progressSession = video_android_sessao();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_PRONTO,0,0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL,NULL,EV_PRIMEIRO_QUADRO,0,0);
  NvLiveStall moving; nv_ls_zerar(&moving);
  double raw = 99, edge = 99;
  for (unsigned elapsed=0; elapsed<=12000; elapsed+=250) {
    jlong position=-20000+(jlong)elapsed;
    Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,position,position+30000,
      JNI_TRUE, (jint)(64*1024*1024-elapsed), (jint)progressSession);
    assert(video_pos()==0); // keep the UI/resume clock non-negative
    assert(video_live_progresso(&raw,&edge));
    assert(raw==position/1000.0 && edge-raw==30.0);
    assert(video_buffer_fim()==edge);
    assert(nv_ls_passo_buf_prazo(&moving,1000+elapsed,raw,1,0,0,0,edge,36000)==NV_LS_NADA);
  }
  assert(sampleBytes==64*1024*1024-12000 && samplePeakBytes==64*1024*1024);
  // A true freeze at a negative position still retries after eight seconds.
  assert(nv_ls_passo_buf_prazo(&moving,21000,raw,1,0,0,0,edge,36000)==NV_LS_AGENDOU);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,999000,1000000,JNI_TRUE,
    INT_MAX,(jint)progressSession-1);
  assert(video_pos()==0 && samplePeakBytes==64*1024*1024);
  assert(video_live_progresso(&raw,&edge) && raw==-8.0 && edge==22.0);
  // A sliding window moves both coordinates together, preserving the reserve.
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,-18000,12000,JNI_TRUE,1024,(jint)progressSession);
  assert(video_live_progresso(&raw,&edge) && raw==-18.0 && edge==12.0);
  assert(edge-raw==30.0 && video_buffer_fim()==12.0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,1000,500,JNI_TRUE,0,(jint)progressSession);
  assert(video_live_progresso(&raw,&edge) && edge==0.0 && sampleBytes==0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,1000,INT64_MIN+1,JNI_TRUE,-1,(jint)progressSession);
  assert(video_buffer_fim()==0 && sampleBytes==-1);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,INT64_MIN+1,30000,JNI_TRUE,-1,(jint)progressSession);
  assert(!video_live_progresso(&raw,&edge) && video_pos()==0);
  // VOD keeps its existing UI/seek position and buffer reporting policy.
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,2451000,2461000,JNI_FALSE,2048,(jint)progressSession);
  assert(video_pos()==2451 && video_buffer_fim()==0);
  assert(!video_live_progresso(&raw,&edge));
  video_parar();
  assert(sampleBytes==-1 && samplePeakBytes==-1 && video_buffer_fim()==0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,1000,31000,JNI_TRUE,4096,(jint)progressSession);
  assert(!video_live_progresso(&raw,&edge) && sampleBytes==-1);
  assert(video_tocar("https://example.invalid/next-progress.m3u8"));
  assert(!video_live_progresso(&raw,&edge) && samplePeakBytes==-1);
  // The reported Android buffer must not activate the webOS dry-source rule.
  progressSession = video_android_sessao();
  NvLiveStall draining; nv_ls_zerar(&draining);
  for (unsigned elapsed=0;elapsed<=29000;elapsed+=250) {
    jlong position=28591+(jlong)elapsed;
    Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL,NULL,position,58591,
      JNI_TRUE,1024,(jint)progressSession);
    assert(video_live_progresso(&raw,&edge) && edge==58.591);
    assert(video_buffer_fim()==edge);
    assert(nv_ls_passo_buf_prazo(&draining,1000+elapsed,raw,1,0,0,0,edge,36000)==NV_LS_NADA);
  }
  assert(!draining.pendente && draining.tentativa==0);
  assert(nv_ls_passo_buf_prazo(&draining,38000,raw,1,0,0,0,edge,36000)==NV_LS_AGENDOU);
  puts("ok signed live progress, same-window buffer, actual allocator bytes, freeze and stale-session guards");
  puts("video Android retomada: PASS (JNI, geracoes, fallback e limites)");
}
