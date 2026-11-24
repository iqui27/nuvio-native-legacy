// farol_produtores_tv — issue 9 do lote sense (PLANO.md secao 5, aceite do
// BRIEF). O anel do Farol recebe o erro nativo de cada backend ja normalizado
// pelo farolerro, sem URL, titulo nem texto livre. Cada modo compila o
// BACKEND REAL e dirige o ponto que a TV atinge, com a cadeia de erro real:
//
//   TEST_ANDROID (cc -DTEST_ANDROID -DNV_ANDROID):
//     Java_..._nativeEvento — o JNI que o Kotlin chama com EV_ERRO
//     ("2004 player error:2") e EV_AUDIO_SEM_DECODER ("audio no decoder").
//   TEST_TPK (cc -DTEST_TPK -DNV_TPK [ -DNV_TPK40 -Wl,--wrap=mkvfix_estado ]):
//     video_tpk_log_host com as linhas do host .NET ("erro ConnectionFailed",
//     "erro AudioCodecNotSupported", "interrompido: ResourceConflict"),
//     nv_tpk_video_evento com o EV_ERRO nativo e recuperarPlayer (o ponto
//     real do "relogio congelado", chamado por vigiarPlayer). Com NV_TPK40:
//     o ErroGeracao de preparo num VOD https aciona o relay TLS fora do
//     EV_ERRO, e o caso relay-falha-depois cobre a nao-duplicacao.
//   TEST_TIZEN (cc -DTEST_TIZEN -D__EMSCRIPTEN__ -Itests/farol_wgt_stub):
//     video_tocar + video_bombear com nv_av implementado em C no lugar do
//     EM_JS — o mesmo contrato do webapis.avplay: "disp", "abrir", "estado"
//     (doubles) e "erro" (S.erro). O onerror da TV chega como
//     "PLAYER_ERROR_CONNECTION_FAILED".
//   (sem define):
//     as mesmas chamadas farolprod_* que os tres arquivos fazem, para as
//     cadeias que so nascem dentro do video_bombear ("2002 player error:
//     recovery timeout", "2002 player error: stalled download").
//
// As conferencias sao sobre o ANEL (farolanel_ler): tipo, a, b e o token
// fechado do farolerro — o que prova que nenhum texto cru passou.

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "farolanel.h"

static int falhos;

// Confere um evento do anel: tipo, modulo FM_VIDEO, sev e os dois numeros;
// o token e o cod fechado que o farolerro escolheu — nunca o texto da TV.
static void chk(const FarolEvento *e, int ev, int sev,
                int32_t a, int32_t b, const char *tok, const char *onde) {
  if (e->ev == (uint16_t)ev && e->mod == FM_VIDEO && e->sev == (uint8_t)sev &&
      e->a == a && e->b == b && !strcmp(e->tok, tok)) return;
  fprintf(stderr, "farol_produtores_tv [%s]: ev=%u mod=%u sev=%u a=%d b=%d "
          "tok=\"%s\" (esperava ev=%d sev=%d a=%d b=%d tok=%s)\n",
          onde, e->ev, e->mod, e->sev, (int)e->a, (int)e->b, e->tok,
          ev, sev, (int)a, (int)b, tok);
  falhos++;
}

// ======================================================================
// Android (Media3): o JNI real e o video_bombear real, sobre a fixture de
// tests/video_android_retomada.c (JNI falso completo).
// ======================================================================
#if defined(TEST_ANDROID)

#define main retomada_main
#include "video_android_retomada.c"
#undef main
#include <stdatomic.h>
#include <sched.h>
#include <stdarg.h>
#include <time.h>
#include "farolanel.h"

const Stream *stream_fonte_reproducao(const char *url, const char *cab) {
  (void)url; (void)cab; return NULL;
}
int stream_kbps(const Stream *s) { (void)s; return 0; }

// Intercalacao deterministica JNI x fio do app (link -Wl,--wrap). O fio do
// app para DENTRO do classificador ate o callback do Media3 publicar a falha
// seguinte; o snprintf do JNI escreve erroTxt byte a byte para o TSan ver.
static pthread_t fio_app;
static atomic_int passo, escreveu, parserTerminou, armado;
extern int __real_farolerro_ler(FarolPlat, const char *, int, FarolErro *);

// A barreira tem prazo proprio: se a chamada do classificador da reconexao
// for suprimida (mutacao ou regressao), nenhum dos lados sinaliza e a espera
// morria no timeout externo (rc 124), mascarando a falha real. Estourado o
// prazo, falha com mensagem e rc 1.
static void barreira_esperar(atomic_int *flag, const char *lado) {
  struct timespec t0, t;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  while (!atomic_load_explicit(flag, memory_order_acquire)) {
    sched_yield();
    clock_gettime(CLOCK_MONOTONIC, &t);
    if (t.tv_sec - t0.tv_sec >= 10) {
      fprintf(stderr, "farol_produtores_tv [android]: barreira (%s) sem sinal em 10 s — "
              "a chamada do classificador da reconexao foi suprimida?\n", lado);
      exit(1);
    }
  }
}

int __wrap_snprintf(char *dst, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int rc;
  if (dst == erroTxt) {
    char local[sizeof erroTxt];
    rc = vsnprintf(local, sizeof local, fmt, ap);
    if (rc >= 0) {
      size_t w = (size_t)rc < n ? (size_t)rc + 1 : n;
      if (w > sizeof local) w = sizeof local;
      if (atomic_load_explicit(&armado, memory_order_relaxed) &&
          !pthread_equal(pthread_self(), fio_app)) {
        for (size_t i = 0; i < w; i++) ((volatile char *)dst)[i] = local[i];
        atomic_store_explicit(&escreveu, 1, memory_order_release);
        barreira_esperar(&parserTerminou, "media3 aguarda o classificador");
      } else memcpy(dst, local, w);
    }
  } else {
    rc = vsnprintf(dst, n, fmt, ap);
  }
  va_end(ap);
  return rc;
}

int __wrap_farolerro_ler(FarolPlat plat, const char *texto, int codigo, FarolErro *e) {
  if (atomic_load_explicit(&armado, memory_order_relaxed) &&
      pthread_equal(pthread_self(), fio_app) &&
      atomic_exchange_explicit(&passo, 1, memory_order_acq_rel) == 0) {
    int rc;
    barreira_esperar(&escreveu, "fio do app aguarda a entrega do Media3");
    rc = __real_farolerro_ler(plat, texto, codigo, e);
    printf("reconexao: codigo da decisao=%d, tok=%s\n", codigo, e->cod);
    atomic_store_explicit(&parserTerminou, 1, memory_order_relaxed);
    return rc;
  }
  return __real_farolerro_ler(plat, texto, codigo, e);
}

static void *callbackMedia3(void *unused) {
  (void)unused;
  barreira_esperar(&passo, "callback media3 aguarda o classificador");
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2004, 2);
  return NULL;
}

int main(void) {
  FarolEvento ev[8];
  int n, i;
  unsigned genAtual;
  fio_app = pthread_self();
  jni.NewGlobalRef = global; jni.GetStaticMethodID = metodo;
  jni.NewString = texto; jni.DeleteLocalRef = apagar;
  jni.ExceptionCheck = temExcecao; jni.ExceptionClear = limparExcecao;
  jni.CallStaticVoidMethod = chamar;
  Java_space_nuvio_nativelegacy_NvPlayer_nativeIniciar(&env, (jclass)(uintptr_t)3);
  mParar = (jmethodID)(uintptr_t)4;
  assert(video_tocar("http://example.invalid/init.mp4"));
  farolanel_zerar();

  // "2004 player error:2" — EV_ERRO do Media3 com o renderer de video.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2004, 2);
  // "audio no decoder" — a flag EV_AUDIO_SEM_DECODER vira texto no produtor.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_AUDIO_SEM_DECODER, 0, 0);
  // Erro de rede nativo, desta vez no renderer de audio.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 2000, 1);
  // Erro de formato (3xxx): o cod sai do proprio numero, sem lista nova.
  Java_space_nuvio_nativelegacy_NvPlayer_nativeEvento(NULL, NULL, EV_ERRO, 3001, 2);

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 4) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos, esperava 4\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO,    2, 2004, 2, "m3_2004", "android erro 2004");
  chk(&ev[1], FE_AUDIO_RECUSADO, 2, 0,    0, "m3_audio", "android audio sem decoder");
  chk(&ev[2], FE_ERRO_NATIVO,    2, 2000, 1, "m3_2000", "android erro 2000");
  chk(&ev[3], FE_ERRO_NATIVO,    2, 3001, 2, "m3_3001", "android erro 3001");

  // JNI identificada (nativeErro): aceita geracao atual (m3_4003) e descarta obsoleta.
  farolanel_zerar();
  genAtual = video_android_sessao();
  assert(genAtual != 0);
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genAtual, 4003, 2);
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 1) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos no nativeErro atual, esperava 1\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 4003, 2, "m3_4003", "android nativeErro atual");

  assert(video_tocar("http://example.invalid/proxima.mp4"));
  assert(video_android_sessao() != genAtual);
  farolanel_zerar();
  Java_space_nuvio_nativelegacy_NvPlayer_nativeErro(NULL, NULL, (jint)genAtual, 4003, 2);
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 0) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos no nativeErro obsoleto, esperava 0\n", n); return 1; }

  // Falha 2002 pendente e o JNI entregando 2004 no meio da classificacao:
  // o token da retentativa tem de ser o da falha que disparou a decisao.
  {
    pthread_t media3;
    assert(video_tocar("http://example.invalid/test.mp4"));
    farolanel_zerar();
    nv_recon_zerar(&recon);
    falhou = 0; bufferando = 0; pausaPedida = 0; prontoLoad = 0;
    reconPermitida = 1; reconIniciou = 1;
    assert(erroPublicarSeAtual(video_android_sessao(), 2002, 0, "2002 player error:2"));
    atomic_store(&armado, 1);
    if (pthread_create(&media3, NULL, callbackMedia3, NULL) != 0) return 2;
    video_bombear();
    pthread_join(media3, NULL);
    atomic_store(&armado, 0);
  }
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  {
    int recon = 0, erro2004 = 0;
    for (i = 0; i < n; i++) {
      if (ev[i].ev == FE_RECONEXAO) {
        recon++;
        chk(&ev[i], FE_RECONEXAO, 1, 1, (int32_t)nv_recon_espera_ms(1),
            "m3_2002", "android retentativa parada na falha certa");
      }
      if (ev[i].ev == FE_ERRO_NATIVO && !strcmp(ev[i].tok, "m3_2004")) {
        erro2004++;
        chk(&ev[i], FE_ERRO_NATIVO, 2, 2004, 2, "m3_2004", "android erro 2004 do JNI");
      }
    }
    if (recon != 1 || erro2004 != 1) {
      fprintf(stderr, "farol_produtores_tv [android]: %d recon / %d erro 2004 (esperava 1/1)\n",
              recon, erro2004);
      falhos++;
    }
  }

  // O prazo da retentativa estoura dentro do video_bombear real:
  // "2002 player error: recovery timeout" tem de sair uma vez, no ponto real.
  assert(video_tocar("http://example.invalid/timeout.mp4"));
  farolanel_zerar();
  falhou = 0; prontoLoad = 0; bufferando = 0; pausaPedida = 0;
  reconPermitida = 1; reconIniciou = 1;
  recon.tentativa = 1; recon.pendente = 0; recon.esgotou = 0; recon.abriu = relogio;
  relogio += NV_RECON_PRAZO_MS + 1;
  video_bombear();
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos no recovery timeout, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado", "android recovery timeout");
  chk(&ev[1], FE_RECONEXAO,   1, 2, (int32_t)nv_recon_espera_ms(2), "m3_parado", "android retentativa do prazo");

  // VOD parada em silencio (sem erro do Media3): "stalled download".
  assert(video_tocar("http://example.invalid/stalled.mp4"));
  farolanel_zerar();
  nv_recon_zerar(&recon);
  falhou = 0; pausaPedida = 0;
  reconPermitida = 1; reconIniciou = 1;
  bufferando = 1; bufferDesde = relogio;
  relogio += NV_RECON_PRAZO_MS + 1;
  video_bombear();
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [android]: %d eventos no stalled, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado", "android stalled download");
  chk(&ev[1], FE_RECONEXAO,   1, 1, (int32_t)nv_recon_espera_ms(1), "m3_parado", "android retentativa do stalled");

  if (falhos) return 1;
  puts("farol_produtores_tv: android ok");
  return 0;
}

// ======================================================================
// Samsung .tpk (Tizen.Multimedia): as linhas do log do host e o evento nativo,
// como em tests/tpk-preso.c.
// ======================================================================
#elif defined(TEST_TPK)

#define main tpk_preso_main
#include "tpk-preso.c"
#undef main
#include "farolanel.h"

// A arvore de integracao liga a escolha regional de audio (audioescolha.c).
int ling_prefere_regional(const char *pref) { (void)pref; return 0; }
int ling_afinidade(const char *cod, const char *pref) { (void)cod; (void)pref; return 0; }

static unsigned geracao;
#ifdef NV_TPK40
// O estado da sonda mkvfix e falso nesta perna (--wrap): SONDANDO segura o
// relay TLS "em curso"; FALHOU derruba o relay para o desfecho no EV_ERRO.
static int mkvfixEstadoFake = MKVFIX_NADA;
int __wrap_mkvfix_estado(void) { return mkvfixEstadoFake; }
#endif
static void abrir_geracao(const char *url, const char *cab, unsigned ger) {
#ifdef NV_TPK40
  // O prepare https falha sem o host chegar a tocar: a abertura conta, mas
  // o player nao nasce — e o ErroGeracao de preparo que chega em seguida.
  if (!strcmp(url, "https://example.invalid/movie.mp4")) { aberturas++; geracao = ger; return; }
#endif
  abrir(url, cab);
  geracao = ger;
}

// Um unico ErrorOccurred do Video.cs: a linha de log E o ErroGeracao da
// mesma falha. O anel tem de guardar UM evento de erro — o que chega pelo
// caminho da geracao, com o codigo numerico — nao um por canal.
static int verificar(const char *linha, int codigo, int tipo, const char *token,
                     int aEsperado, const char *onde) {
  FarolEvento ev[8];
  int n, i, erros = 0, recons = 0;
  video_definir_mp4(1);
  video_definir_reconexao(1);
  if (!video_tocar("http://test/movie")) return 2;
  prontoHost(); posicao = 12000; passo(1);
  farolanel_zerar();
  if (!geracao) return 2;
  // A ordem real: Log("erro ...") antes de ErroGeracao(geracao, codigo, 0).
  video_tpk_log_host(linha);
  nv_tpk_video_erro_geracao(geracao, codigo, 0);
  passo(1);
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  for (i = 0; i < n; i++) {
    printf("  ev=%d a=%d b=%d tok=%s\n", ev[i].ev, (int)ev[i].a, (int)ev[i].b, ev[i].tok);
    if (ev[i].ev == tipo && !strcmp(ev[i].tok, token)) {
      erros++;
      chk(&ev[i], tipo, 2, aEsperado, 0, token, onde);
    }
    if (ev[i].ev == FE_RECONEXAO) {
      recons++;
      chk(&ev[i], FE_RECONEXAO, 1, 1, (int32_t)nv_recon_espera_ms(1), token, onde);
    }
  }
  if (erros != 1) {
    fprintf(stderr, "farol_produtores_tv [tpk]: %s deu %d eventos da falha (esperava 1)\n",
            onde, erros);
    falhos++;
  }
  video_parar();
  return recons;
}

int main(void) {
  FarolEvento ev[8];
  int n;
  nv_tpk_video_registrar(abrir, parar, pausar, buscar, NULL, NULL, pos);
  nv_tpk_video_registrar_geracao(abrir_geracao);

  // Rede: "erro ConnectionFailed" + ErroGeracao 0xFE6C0026 — um evento de
  // erro so, com o codigo nativo; a retentativa legitima continua saindo.
  if (verificar("erro ConnectionFailed", (int)0xFE6C0026u,
                FE_ERRO_NATIVO, "tpk_conexao", (int32_t)0xFE6C0026,
                "tpk conexao") != 1) {
    fprintf(stderr, "farol_produtores_tv [tpk]: reconexao legitima ausente\n");
    falhos++;
  }
  // Audio: "erro AudioCodecNotSupported" + ErroGeracao 0xFE6C002E — um
  // FE_AUDIO_RECUSADO so (nao e erro de rede: sem retentativa).
  if (verificar("erro AudioCodecNotSupported", (int)0xFE6C002Eu,
                FE_AUDIO_RECUSADO, "tpk_audio", (int32_t)0xFE6C002E,
                "tpk audio") != 0) {
    fprintf(stderr, "farol_produtores_tv [tpk]: retentativa indevida no audio\n");
    falhos++;
  }

  // Conflito de recurso (so existe a linha de texto, sem geracao) e o
  // relogio congelado seguem entrando uma vez cada.
  video_definir_mp4(1); video_definir_reconexao(1);
  if (!video_tocar("http://test/movie")) return 2;
  prontoHost(); posicao = 12000; passo(1);
  farolanel_zerar();
  video_tpk_log_host("interrompido: ResourceConflict");
  recuperarPlayer(-1);
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [tpk]: %d eventos, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 0,  0, "tpk_conflito", "tpk resource conflict");
  chk(&ev[1], FE_ERRO_NATIVO, 2, -2, 0, "tpk_preso",    "tpk relogio congelado");
  video_parar();

#ifdef NV_TPK40
  // TPK40: o ErroGeracao de PREPARO num VOD https desvia do EV_ERRO para
  // acionar o relay TLS (Video.cs:494 → video_tpk.c, ramo NV_TPK40). A falha
  // conta uma vez mesmo fora do caminho normal.
  video_definir_mp4(1); video_definir_reconexao(1);
  if (!video_tocar("https://example.invalid/movie.mp4") || !geracao) return 2;
  farolanel_zerar();
  mkvfixEstadoFake = MKVFIX_SONDANDO;
  video_tpk_log_host("erro ConnectionFailed");
  nv_tpk_video_erro_geracao(geracao, (int)0xFE6C0026u, 1);
  passo(1);
  if (!mf.relayTentou || !mf.esperando) {
    fprintf(stderr, "farol_produtores_tv [tpk40]: o relay TLS nao engatou "
            "(relayTentou=%d, esperando=%d)\n", mf.relayTentou, mf.esperando);
    return 2;
  }
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 1) { fprintf(stderr, "farol_produtores_tv [tpk40]: %d eventos na falha do preparo, esperava 1\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, (int32_t)0xFE6C0026, 0, "tpk_conexao", "tpk40 preparo https");

  // O relay falha depois: o desfecho cai no EV_ERRO normal (-1) — a falha do
  // preparo nao re-entra; o evento novo e o do relay, um por falha.
  mkvfixEstadoFake = MKVFIX_FALHOU;
  passo(1);
  if (!video_falhou()) {
    fprintf(stderr, "farol_produtores_tv [tpk40]: relay falhou e o video nao marcou falha\n");
    return 2;
  }
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [tpk40]: %d eventos apos o relay falhar, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, (int32_t)0xFE6C0026, 0, "tpk_conexao", "tpk40 preparo https (inalterado)");
  chk(&ev[1], FE_ERRO_NATIVO, 2, -1, 0, "tpk_outro", "tpk40 relay indisponivel");
  video_parar();
#endif

  if (falhos) return 1;
#ifdef NV_TPK40
  puts("farol_produtores_tv: tpk40 ok");
#else
  puts("farol_produtores_tv: tpk ok");
#endif
  return 0;
}

// ======================================================================
// Samsung .wgt (AVPlay): o backend inteiro com nv_av em C no lugar do EM_JS.
// ======================================================================
#elif defined(TEST_TIZEN)

#include "../src/video_tizen.c"

// --- o mundo que o firmware empresta ao EM_JS ------------------------------
static unsigned agora;
static double estadoAv[EST_N];      // o "estado" que a getState relataria
static const char *erroAvplay;      // o S.erro do JS ("" = sem erro)
static int dispAvplay = 1;          // webapis.avplay existe

Uint32 SDL_GetTicks(void) { return agora; }
void marco(const char *nome) { (void)nome; }
const char *i18n(const char *s) { return s; }
const char *ling_nome(const char *codigo) { return codigo; }
const char *ling_audio(void) { return ""; }
const char *audioesc_origem_nome(int origem) { (void)origem; return ""; }
int audioesc_dts_truehd(const char *codec, const char *rotulo) { (void)codec; (void)rotulo; return 0; }
int audioesc_escolher(const AudioCand *c, int n, int *origem) {
  (void)c; (void)n; if (origem) *origem = 0; return -1;
}
int mkv_faixas_e_caps(const char *url, MkvFaixa *saida, int max,
                      MkvCap *caps, int maxCaps, int *nCaps) {
  (void)url; (void)saida; (void)max; (void)caps; (void)maxCaps;
  if (nCaps) *nCaps = 0;
  return 0;
}
double mkv_creditos_nomeados(const MkvCap *caps, int n) { (void)caps; (void)n; return 0.0; }
double mkv_creditos_ultimo(const MkvCap *caps, int n) { (void)caps; (void)n; return 0.0; }
void capmkv_aplicar(const MkvCap *caps, int n) { (void)caps; (void)n; }
// A arvore de integracao liga o painel de informacoes (js.c/vazao.c): o teste
// nao abre o painel, os simbolos so precisam existir.
int js_texto(const char *ini, const char *fim, const char *chave,
             char *dst, size_t tam) {
  (void)ini; (void)fim; (void)chave; (void)dst; (void)tam; return 0;
}
double js_num(const char *ini, const char *fim, const char *chave, double padrao) {
  (void)ini; (void)fim; (void)chave; return padrao;
}
int vazao_host_publico(const char *url, char *dst, size_t n) {
  (void)url; if (dst && n) dst[0] = 0; return 0;
}

// O fio de teste E o fio principal: avChamar executa o pedido na hora.
int emscripten_is_main_browser_thread(void) { return 1; }
int emscripten_main_runtime_thread_id(void) { return 0; }
em_proxying_queue *em_proxying_queue_create(void) { return NULL; }
void emscripten_proxy_sync(em_proxying_queue *q, int thread,
                           void (*func)(void *), void *arg) {
  (void)q; (void)thread; (void)func; (void)arg;
}

// nv_av em C, com o contrato do corpo JS: "disp" diz se ha webapis.avplay,
// "abrir" retorna 1 e limpa S.erro, "estado" enche doubles, "erro" copia
// S.erro para dst, os demais comandos de controle retornam sucesso.
double nv_av(const char *cmd, const char *txt, double a, double b,
             double c, double d, char *dst, int dstTam) {
  (void)txt; (void)a; (void)b; (void)c; (void)d;
  if (!cmd) return 0;
  if (!strcmp(cmd, "disp")) return dispAvplay;
  if (!strcmp(cmd, "abrir")) { erroAvplay = NULL; return 1; }
  if (!strcmp(cmd, "estado")) {
    if (!dst || dstTam < (int)sizeof estadoAv) return 0;
    memcpy(dst, estadoAv, sizeof estadoAv);
    return 1;
  }
  if (!strcmp(cmd, "erro")) {
    if (!erroAvplay || !dst || dstTam < 2) return 0;
    snprintf(dst, (size_t)dstTam, "%s", erroAvplay);
    return 1;
  }
  if (!strcmp(cmd, "faixas") || !strcmp(cmd, "infofluxo") ||
      !strcmp(cmd, "playinfo")) return 0;
  return 1;   // rect, roi, parar, pausar, buscar, encerrar, leg_mudo...
}

int main(void) {
  FarolEvento ev[8];
  int n, i;
  farolanel_zerar();
  memset(estadoAv, 0, sizeof estadoAv);

  assert(video_iniciar() == 1);
  video_definir_reconexao(1);
  video_definir_mp4(1);                       // sem a sonda MKV (evita o fio)
  assert(video_tocar("http://tv.local/f.mp4") == 1);

  // Quadro pronto e andando: ainda nenhum evento no anel.
  agora = 2000;
  estadoAv[EST_ABERTO] = 1; estadoAv[EST_PRONTO] = 1; estadoAv[EST_TOCANDO] = 1;
  estadoAv[EST_POS] = 12.0; estadoAv[EST_DUR] = 3600.0;
  estadoAv[EST_BUFPCT] = 100; estadoAv[EST_LARG] = 1920; estadoAv[EST_ALT] = 1080;
  video_bombear();
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 0) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos antes do erro\n", n); return 1; }

  // A TV acusa a queda: onerror com o nome do enum PlayerError do AVPlay.
  erroAvplay = "PLAYER_ERROR_CONNECTION_FAILED";
  estadoAv[EST_ERRO] = 1; estadoAv[EST_TOCANDO] = 0; estadoAv[EST_BUFPCT] = 0;
  agora = 3000;
  video_bombear();

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos no erro, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 0, 0, "av_conexao", "tizen onerror");
  chk(&ev[1], FE_RECONEXAO,   1, 1, (int32_t)nv_recon_espera_ms(1), "av_conexao", "tizen retentativa 1");

  // A espera vence, a mesma fonte reabre (abrir limpa S.erro), a reproducao
  // volta pronta e a rede cai de novo: segunda retentativa, mesmo problema.
  agora += nv_recon_espera_ms(1) + 1;
  estadoAv[EST_ERRO] = 0; estadoAv[EST_PRONTO] = 0;
  video_bombear();                            // vencida -> reabrirMesmaFonte
  estadoAv[EST_PRONTO] = 1; estadoAv[EST_TOCANDO] = 1; estadoAv[EST_BUFPCT] = 100;
  video_bombear();                            // pronto de novo, seek pos-queda
  erroAvplay = "PLAYER_ERROR_CONNECTION_FAILED";
  estadoAv[EST_ERRO] = 1; estadoAv[EST_TOCANDO] = 0;
  video_bombear();

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 4) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos na 2a queda, esperava 4\n", n); return 1; }
  chk(&ev[2], FE_ERRO_NATIVO, 2, 0, 0, "av_conexao", "tizen onerror 2");
  chk(&ev[3], FE_RECONEXAO,   1, 2, (int32_t)nv_recon_espera_ms(2), "av_conexao", "tizen retentativa 2");

  // O prazo da recuperacao estoura dentro do video_bombear real: a sessao
  // reaberta pela espera nunca fica pronta (a rede continua morta).
  agora += nv_recon_espera_ms(2) + 1;   // vence a espera da 2a tentativa
  estadoAv[EST_ERRO] = 0;               // a reabertura limpa o erro da sessao
  estadoAv[EST_PRONTO] = 0; estadoAv[EST_TOCANDO] = 0; estadoAv[EST_BUFPCT] = 0;
  video_bombear();                      // nv_recon_vencida -> reabrirMesmaFonte
  farolanel_zerar();
  agora += 30001;                       // NV_RECON_PRAZO_MS + 1 sem ficar pronto
  video_bombear();
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 2) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos no timeout, esperava 2\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 0, 0, "av_conexao", "tizen recovery timeout");
  chk(&ev[1], FE_RECONEXAO,   1, 3, (int32_t)nv_recon_espera_ms(3), "av_conexao", "tizen retentativa do timeout");

  // Mensagem livre do AVPlay com PLAYER_ERROR_ dentro de uma URL: o anel
  // nao guarda fragmento de texto — so os 11 nomes do enum AVPlayError.
  farolanel_zerar();
  erroAvplay = "open: invalid source https://example.invalid/PLAYER_ERROR_CANARIO_CHAVE";
  logarErro("abrir");
  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  erroAvplay = NULL;
  if (n != 1) { fprintf(stderr, "farol_produtores_tv [tizen]: %d eventos no canario, esperava 1\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 0, 0, "av_outro", "tizen canario em URL");
  for (i = 0; i < n; i++)
    if (strstr(ev[i].tok, "canario")) {
      fprintf(stderr, "farol_produtores_tv [tizen]: fragmento de texto livre no anel: \"%s\"\n", ev[i].tok);
      falhos++;
    }

  if (falhos) return 1;
  puts("farol_produtores_tv: tizen ok");
  return 0;
}

// ======================================================================
// Nivel do helper: as cadeias que so nascem dentro dos video_bombear
// (os printf do video_android.c em recovery timeout / stalled download) e a
// retentativa, com os mesmos argumentos dos pontos reais.
// ======================================================================
#else

#include "farolprodutor.h"
#include <pthread.h>

// O host .NET e o JNI emitem dos fios de callback; o anel e a unica estrutura
// compartilhada. Sob TSan isto cobre a trava de gravacao sem arrastar o resto
// do estado do backend (que tem fio proprio por desenho).
#define FIOS_N   4
#define EVS_FIO 25   /* 4 x 50 eventos: tudo fica no anel de 256 */
static void *fioProdutor(void *arg) {
  int i, k = (int)(intptr_t)arg;
  for (i = 0; i < EVS_FIO; i++) {
    farolprod_erro(FP_TIZEN_TPK, "erro ConnectionFailed", 0, 0);
    farolprod_recon(FP_TIZEN_TPK, "erro ConnectionFailed", 0, k * EVS_FIO + i, 1000);
  }
  return NULL;
}

int main(void) {
  FarolEvento ev[8];
  int n, i;
  farolanel_zerar();

  // As duas cadeias que video_android.c escreve em erroTxt nos caminhos
  // "recovery timeout" e "stalled download" (video_bombear, fora do JNI).
  farolprod_erro(FP_ANDROID, "2002 player error: recovery timeout", 2002, 0);
  farolprod_erro(FP_ANDROID, "2002 player error: stalled download", 2002, 0);
  // A retentativa seguinte: a=tentativa, b=espera, tok=cod do erro original.
  farolprod_recon(FP_ANDROID, "2004 player error:2", 2004, 1, 4000);
  // O prazo estourado do .wgt, texto fixo do proprio app.
  farolprod_erro(FP_TIZEN_WGT, "network recovery timeout", 0, 0);
  // .tpk: o relogio congelado visto pela retentativa (recuperarPlayer -> -2).
  farolprod_recon(FP_TIZEN_TPK, NULL, -2, 2, 8000);

  n = farolanel_ler(ev, (int)(sizeof ev / sizeof ev[0]), 0);
  if (n != 5) { fprintf(stderr, "farol_produtores_tv [helper]: %d eventos, esperava 5\n", n); return 1; }
  chk(&ev[0], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado",    "android recovery timeout");
  chk(&ev[1], FE_ERRO_NATIVO, 2, 2002, 0, "m3_parado",    "android stalled download");
  chk(&ev[2], FE_RECONEXAO,   1, 1, 4000, "m3_2004",      "android retentativa");
  chk(&ev[3], FE_ERRO_NATIVO, 2, 0,    0, "av_conexao",   "wgt recovery timeout");
  chk(&ev[4], FE_RECONEXAO,   1, 2, 8000, "tpk_preso",    "tpk retentativa preso");

  // Barreira de privacidade do anel: nenhum token carrega URL ou texto livre.
  for (i = 0; i < n; i++) {
    const char *p;
    for (p = ev[i].tok; *p; p++)
      if (!(*p >= 'a' && *p <= 'z') && !(*p >= '0' && *p <= '9') &&
          *p != '_' && *p != '.' && *p != ':' && *p != '+' && *p != '-') {
        fprintf(stderr, "farol_produtores_tv [helper]: tok inseguro \"%s\"\n", ev[i].tok);
        falhos++;
        break;
      }
  }
  // Quatro fios de produtor gravando ao mesmo tempo, como no callback do host.
  { pthread_t t[FIOS_N];
    farolanel_zerar();
    for (i = 0; i < FIOS_N; i++)
      assert(pthread_create(&t[i], NULL, fioProdutor, (void *)(intptr_t)i) == 0);
    for (i = 0; i < FIOS_N; i++) pthread_join(t[i], NULL);
    n = farolanel_contar(FE_ERRO_NATIVO, 0, 0);
    if (n != FIOS_N * EVS_FIO ||
        farolanel_contar(FE_RECONEXAO, 0, 0) != FIOS_N * EVS_FIO) {
      fprintf(stderr, "farol_produtores_tv [helper]: %d erros nativos de %d\n",
              n, FIOS_N * EVS_FIO);
      return 1;
    }
  }

  if (falhos) return 1;
  puts("farol_produtores_tv: helper ok");
  return 0;
}

#endif
