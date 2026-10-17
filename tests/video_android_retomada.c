// JNI falsa sobre o backend real: aceita/rejeita o novo prepare e entrega
// acknowledgements atrasados. Nao depende da JVM, SDL dinamico ou TV.
#define NV_ANDROID 1
#include <SDL2/SDL.h>
void *SDL_AndroidGetJNIEnv(void);
void *SDL_AndroidGetActivity(void);
#include "../src/video_android.c"
#include "../src/cacheboost.c"   // F07: video_android reads the session volume
#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

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
static unsigned relogio = 100;
void *SDL_AndroidGetJNIEnv(void) { return &env; }
void *SDL_AndroidGetActivity(void) { return NULL; }
Uint32 SDL_GetTicks(void) { return relogio; }
SDL_mutex *SDL_CreateMutex(void) { return (SDL_mutex *)(uintptr_t)1; }
const char *ling_audio(void) { return ""; }
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
  if (m == mParar) return;
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
  if (m == mAbrirRetomada) {
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
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2451000);
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
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2461000);
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
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2451000);
  video_bombear(); video_pausar(1);
  pausas = chamadasPausa;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, -1);
  video_bombear(); relogio += 3000; video_bombear();
  assert(chamadasPausa == pausas + 1 && ultimoPausar == 1);
  puts("ok reconexao conserva pausa pedida antes do ack");

  // Seek ate o fim (Shield, 17/10/2026): episodio de 2547128 ms, barra levada
  // a 2547127. O Media3 falhava o load ali (erro 2000) e a reconexao voltava
  // ao mesmo ponto tres vezes. Nem o seek nem a retomada entram nos ultimos
  // 2 s; o resto do episodio toca e termina pelo EV_FIM.
  video_parar(); video_definir_reconexao(1);
  assert(video_tocar("https://example.invalid/fim.mkv"));
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 2547128, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRIMEIRO_QUADRO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 30000);
  video_bombear();
  buscas = chamadasBusca;
  video_buscar(2547.127);
  assert(chamadasBusca == buscas + 1 && ultimoBuscar == 2545128);
  video_buscar(1200.0);                       // longe do fim: sem mudanca
  assert(ultimoBuscar == 1200000);
  // A posicao do player pode chegar ao fim mesmo assim; a retomada nao.
  Java_space_nuvio_nativelegacy_NvPlayer_nativePos(NULL, NULL, 2547127);
  video_bombear();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, -1);
  video_bombear(); relogio += 3000; video_bombear();
  buscas = chamadasBusca;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_PRONTO, 2547128, 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_TOCANDO, 0, 0);
  video_bombear();
  assert(chamadasBusca == buscas + 1 && ultimoBuscar == 2545128);
  // Duracao desconhecida (0) ou curta: o alvo segue como veio.
  video_parar();
  assert(video_tocar("https://example.invalid/sem-duracao.mkv"));
  video_buscar(99.5);
  assert(ultimoBuscar == 99500);
  puts("ok seek e retomada nunca entram nos ultimos 2 s do arquivo");
  puts("video Android retomada: PASS (JNI, geracoes, fallback e limites)");
}
