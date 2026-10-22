// Icone do app para apoiadores. Ver iconeapp.h.
#include "iconeapp.h"
#include "iconeapp_arte.h"
#include "ajustes.h"
#include "dados.h"
#include "tex_cache.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NV_ANDROID
#include <SDL2/SDL.h>
#include <jni.h>
#endif
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// A ORDEM E A DA GALERIA e o INDICE gravado em ajustes.txt (iconeAppLocal):
// so acrescentar no fim. Bate com V_ICONE_APP (ajustes.c), com LISTA em
// tools/icones-app.sh e com os aliases do AndroidManifest.xml.
static const char *ID[ICONEAPP_N] = {
  "original", "fenix", "nverde", "tvlaranja", "npixel",
  "tricolor", "arco", "tvviva", "cluberetro", "arcaden",
};

static char dirArteIc[512] = ".";
static char caminhos[ICONEAPP_N][600];
static int  apoiadorLido = -1;

int apoiador_ativo(void) {
  const char *e;
  char *s;
  if (apoiadorLido >= 0) return apoiadorLido;
  // O PORTAO DE VERDADE (Patreon, conta) entra AQUI e so aqui. Hoje: ninguem,
  // a nao ser forcado pelo dono para ver a galeria.
  e = getenv("NUVIO_APOIADOR");
  if (e && e[0] == '1') return apoiadorLido = 1;
  // Sem pasta de dados ainda, nao guarda a resposta: perguntar de novo depois.
  if (!dados_dir() || !dados_dir()[0]) return 0;
  s = dados_ler("apoiador.txt");
  apoiadorLido = (s && s[0] == '1');
  free(s);
  if (apoiadorLido) { printf("[app-icon] supporter enabled through local override\n"); fflush(stdout); }
  return apoiadorLido;
}
void apoiador_reler(void) { apoiadorLido = -1; }

void iconeapp_iniciar(const char *dirArte) {
  snprintf(dirArteIc, sizeof dirArteIc, "%s", dirArte && dirArte[0] ? dirArte : ".");
  memset(caminhos, 0, sizeof caminhos);
}

int iconeapp_atual(void) {
  int v = ajustes_icone_app();
  if (!apoiador_ativo() || v < 0 || v >= ICONEAPP_N) return 0;
  return v;
}

const char *iconeapp_id(int i) {
  return (i >= 0 && i < ICONEAPP_N) ? ID[i] : "";
}

const char *iconeapp_caminho(int i) {
  // Um buffer por icone: a galeria pede os dez no mesmo quadro e o tex_cache
  // guarda o ponteiro so durante a chamada, mas assim ninguem depende disso.
  if (i < 0 || i >= ICONEAPP_N) return "";
  if (!caminhos[i][0]) snprintf(caminhos[i], sizeof caminhos[i], "%s/icones-app/%s.png", dirArteIc, ID[i]);
  return caminhos[i];
}

int iconeapp_desenhar(int i, GfxRect r, int ladrilho, float alpha) {
  GLuint t;
  if (i < 0 || i >= ICONEAPP_N || alpha <= 0.004f) return 0;
  // O mesmo fundo do icone original (13,16,30) e o mesmo raio de ladrilho que a
  // TV da a um icone quadrado na grade de apps.
  if (ladrilho) gfx_cor(r, 0.22f, 13.0f/255.0f, 16.0f/255.0f, 30.0f/255.0f, alpha);
  t = tex_obter_larg(iconeapp_caminho(i), r.w);
  if (!t) return 0;
  // GFX_TEXTO: RGB e ALPHA da textura (a marca e recortada); o GFX_SNAP
  // ignoraria o alpha e pintaria o quadrado inteiro.
  gfx_rect(r, t, GFX_TEXTO, 0, 0, 0, 0.0f, 1, 1, 1, alpha);
  return 1;
}

int iconeapp_marca(GfxRect r, float alpha) {
  int i = iconeapp_atual();
  if (i == 0) return 0;
  return iconeapp_desenhar(i, r, 0, alpha);
}

// Re-apply when the bundled art of the icon in force differs from the one last
// applied ("id:hash" in iconeapp-arte.txt). A new APK/package ships the new art
// by itself; this covers installs that only copy files and the wasm splash.
const char *iconeapp_arte_atual(int i) {
  static char b[48];
  snprintf(b, sizeof b, "%s:%s", ID[i], ARTE_HASH[i]);
  return b;
}
int iconeapp_arte_mudou(int i, const char *gravado) {
  if (i < 0 || i >= ICONEAPP_N) return 0;
  return !gravado || strcmp(gravado, iconeapp_arte_atual(i)) != 0;
}

#ifdef NV_ANDROID
// NuvioActivity.trocarIcone(id): agenda a troca do <activity-alias> do launcher
// para quando o app sair da frente (ver a nota la: trocar na hora fecha a
// tarefa, medido no emulador). 0 = ja estava, 2 = agendada, -1 = falhou.
static void androidTrocar(const char *id) {
  JNIEnv *env = (JNIEnv *)SDL_AndroidGetJNIEnv();
  jobject act = (jobject)SDL_AndroidGetActivity();
  jclass cls;
  jmethodID m;
  jstring js;
  int r = -1;
  if (!env || !act) return;
  cls = (*env)->GetObjectClass(env, act);
  m = cls ? (*env)->GetMethodID(env, cls, "trocarIcone", "(Ljava/lang/String;)I") : NULL;
  js = (*env)->NewStringUTF(env, id);
  if (m && js) r = (int)(*env)->CallIntMethod(env, act, m, js);
  if ((*env)->ExceptionCheck(env)) { (*env)->ExceptionClear(env); r = -1; }
  if (js) (*env)->DeleteLocalRef(env, js);
  if (cls) (*env)->DeleteLocalRef(env, cls);
  (*env)->DeleteLocalRef(env, act);
  if (r != 0)
    printf("[app-icon] launcher: %s -> %s\n", id, r == 2 ? "scheduled for app exit" : "failed");
  fflush(stdout);
}
#endif

void iconeapp_aplicar_plataforma(void) {
  int i = iconeapp_atual();
  const char *id = iconeapp_id(i);
#ifdef NV_ANDROID
  androidTrocar(id);
#elif defined(__EMSCRIPTEN__)
  // A abertura do .wgt e HTML e roda ANTES do wasm: so le o que ficou gravado.
  MAIN_THREAD_ASYNC_EM_ASM({
    try {
      var v = UTF8ToString($0);
      if (v === 'original') localStorage.removeItem('nuvio-icone');
      else localStorage.setItem('nuvio-icone', v);
    } catch (e) {}
  }, id);
#else
  (void)id;
#endif
  if (dados_dir() && dados_dir()[0]) dados_gravar("iconeapp-arte.txt", iconeapp_arte_atual(i));
}
