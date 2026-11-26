// Ver vtregenvio.h.
#include "vtregenvio.h"
#include "ajustes.h"
#include "perfilinfantil.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

static VtRegVistos vistos;
#ifdef VTREG_TESTE
static int (*postarTeste)(const char *url, const char *corpo);
void vtregenvio_teste_postar(int (*f)(const char *url, const char *corpo)) { postarTeste = f; }
void vtregenvio_teste_zerar(void) { memset(&vistos, 0, sizeof vistos); }
#else
#include "rede.h"
static void *fioEnvio(void *u) {
  static const char *const cab[] = { "Content-Type: application/json", NULL };
  char url[200], *corpo = u;
  int st = 0;
  snprintf(url, sizeof url, "%s/v1/vaitocar", NV_REC_URL);
  free(rede_postar_st(url, 10, cab, corpo, &st));
  // So o status: o corpo ja e anonimo, mas o log nao precisa dele.
  printf("[vaitocar] registro enviado: http %d\n", st);
  fflush(stdout);
  free(corpo);
  return NULL;
}
#endif

int vtregenvio_enviar(const VtRegEntrada *e) {
  VtRegRegistro r;
  char json[400];
  int servidor = NV_REC_URL[0] != 0;
#ifdef VTREG_TESTE
  servidor = postarTeste != NULL;
#endif
  if (!vtreg_pode_enviar(ajustes_vaitocar(), !perfil_infantil_ativo(), servidor)) return 0;
  if (!vtreg_normalizar(e, &r) || !vtreg_json(&r, json, sizeof json)) return 0;
  if (!vtreg_novo(&vistos, vtreg_chave(&r))) return 0;
#ifdef VTREG_TESTE
  { char url[200];
    snprintf(url, sizeof url, "%s/v1/vaitocar", NV_REC_URL);
    return postarTeste(url, json); }
#elif defined(__EMSCRIPTEN__)
  return 0;   // .wgt: sem fio para o POST ainda; ver docs/issues/vai-tocar-registro.md
#else
  { pthread_t t;
    char *c = strdup(json);
    if (!c) return 0;
    if (pthread_create(&t, NULL, fioEnvio, c) != 0) { free(c); return 0; }
    pthread_detach(t);
    return 1; }
#endif
}

#ifndef VTREG_TESTE
#include "video.h"
#if defined(NV_WEBOS)
#include "avisos.h"
#elif !defined(NV_ANDROID) && !defined(__EMSCRIPTEN__) && !defined(_WIN32)
#include <sys/utsname.h>   // Windows (MinGW) nao tem: la modelo e firmware saem "?"
#endif

void vtregenvio_aparelho(const char **plataforma, const char **modelo, const char **firmware) {
  static char mod[64], fw[48];
  static int lido;
  const char *plat;
#if defined(NV_ANDROID)
  plat = "android";
  if (!lido) {   // "<MANUFACTURER> <MODEL>|<SDK_INT>|<RELEASE>|<versionName>" (android.c)
    const char *e = getenv("NUVIO_TV_INFO"), *b = e ? strchr(e, '|') : NULL, *c = b ? strchr(b + 1, '|') : NULL;
    if (e && b) snprintf(mod, sizeof mod, "%.*s", (int)(b - e), e);
    if (c) { const char *d = strchr(c + 1, '|');
      snprintf(fw, sizeof fw, "%.*s", d ? (int)(d - c - 1) : (int)strlen(c + 1), c + 1); }
  }
#elif defined(NV_WEBOS)
  plat = "webos";
  if (!lido) {   // "[tv] modelo=.. host=webos-<versao> .." (avisos.c)
    char linha[256];
    const char *h;
    avisos_tv_linha(linha, sizeof linha, mod, sizeof mod);
    if (!strcmp(mod, "webos")) mod[0] = 0;
    h = strstr(linha, "host=webos-");
    if (h) { size_t n = strcspn(h + 11, " "); snprintf(fw, sizeof fw, "%.*s", (int)n, h + 11); }
  }
#elif defined(NV_TPK) || defined(__EMSCRIPTEN__)
  plat = "tizen";   // modelo e firmware ainda nao chegam aqui: saem "?"
#elif defined(__APPLE__)
  plat = "macos";
#elif defined(_WIN32)
  plat = "outro";   // fora do vocabulario do Worker; modelo e firmware saem "?"
#else
  plat = "linux";
#endif
#if !defined(NV_ANDROID) && !defined(NV_WEBOS) && !defined(NV_TPK) && !defined(__EMSCRIPTEN__) && !defined(_WIN32)
  if (!lido) {
    struct utsname u;
    if (uname(&u) == 0) { snprintf(mod, sizeof mod, "%s", u.machine); snprintf(fw, sizeof fw, "%s", u.release); }
  }
#endif
  lido = 1;
  if (plataforma) *plataforma = plat;
  if (modelo) *modelo = mod;
  if (firmware) *firmware = fw;
}

void vtregenvio_resultado(int resultado, int trocou) {
  VtRegEntrada e;
  VideoInfo vi;
  const VideoFaixa *f = NULL;
  int atual, n;
  // Barato quando desligado: nem le o player.
  if (!ajustes_vaitocar() || resultado < 0 || resultado >= VTR_N) return;
  memset(&e, 0, sizeof e);
  memset(&vi, 0, sizeof vi);
  vtregenvio_aparelho(&e.plataforma, &e.modelo, &e.firmware);
  video_informacoes(&vi);
  vi.codec[sizeof vi.codec - 1] = 0; vi.container[sizeof vi.container - 1] = 0;
  e.container = vi.container; e.video = vi.codec; e.kbps = vi.kbps;
  e.hdr = video_hdr();
  e.altura = video_altura();
  // Faixa de audio em uso. So o codec vai adiante, e por vocabulario fechado:
  // o rotulo (que pode trazer o titulo da faixa do arquivo) nunca sai cru.
  atual = video_audio_atual(); n = video_n_audio();   // indice da lista, como em faixas.c
  if (n > 0) f = video_audio(atual >= 0 && atual < n ? atual : 0);
  if (f) e.audio = f->codec[0] ? f->codec : f->rotulo;
  e.resultado = resultado; e.trocou = trocou;
  vtregenvio_enviar(&e);
}

void vtregenvio_falha(int motivo, int abriu, int erroDecoder, int trocou) {
  VtRegSinais s = { motivo, abriu, erroDecoder };
  vtregenvio_resultado(vtreg_classificar(&s), trocou);
}
#endif
