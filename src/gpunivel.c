// Nivel de GPU do .tpk. O porque, os niveis e a regra estao em gpunivel.h.
//
// SEM _Thread_local (a .so do Tizen 4/5 recusa TLS, tests/tpk40_tls.sh): tudo
// aqui roda no fio de desenho, e os estaticos sao so dele.
#include "gpunivel.h"
#include "ajustes.h"
#ifdef NV_TPK
#include "video.h"
#endif
#ifdef NV_ANDROID
#include "android.h"
#endif
#include "gfx.h"
#include "dados.h"
#include "perfiltv.h"
#include "layout.h"
#include "gl_compat.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NV_TPK
#include <dlfcn.h>
#include "tpk_egl.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#endif

// Match android.c's logcat tag; also keep the session file via stdout.
#ifdef NV_ANDROID
#include <android/log.h>
#define printf(...) (__android_log_print(ANDROID_LOG_INFO, "nuvio", __VA_ARGS__), printf(__VA_ARGS__))
#endif

#define GPUN_NIVEL_AUTO_MAX 2   // Effects only; resolution is independent.
#define GPUN_ARQ "gpu-nivel.txt"
static int nivel = 0, adaptativo = 0, decidido = 0;
static const char *origem = "padrao";
static char renderer[160] = "?", versaoGl[160] = "?", modelo[96] = "?", tizen[32] = "?";
static unsigned long chave;
static int telaW = 1920, telaH = 1080;
// Explicit 720p target or 4K fallback (1920x1080, gpun_alvo_1080).
static GLuint intFbo, intTex;
// RCAS input: the upscaled frame at drawable size (only SGSR sharp / FSR 1).
static GLuint rcasFbo, rcasTex;
static int rcasFalhou, upAlgo, upNitidez;
static int alvo1080, prefFixa, upscaleSgsr;
static int forca720, ampliarApp;
static int intW, intH, intFalhou, intLigado;
// Descarte (glInvalidateFramebuffer ou glDiscardFramebufferEXT).
typedef void (*PfnDescarte)(GLenum, GLsizei, const GLenum *);
static PfnDescarte descarte;
#ifdef NV_TPK
static const char *descarteNome = "nenhum";
#endif
static int profStencil;   // a janela tem profundidade/stencil para descartar
// Three-second live FPS reports; see gpunivel.h for the ladder contract.
static int bloqueado;
static double fpsNivel[3]; // Retained in the version-1 disk format.
static int relatoCena, relatosLentos, relatosRapidos, relatosValidos;
static int relatoTentativa = -1;
static int degrau, foscoVetado, recuperacaoEspera;
static void aplicar(int n, const char *motivo);
static unsigned efeitos, semGanho;
static int testePasso, testeAntes;
static double testeFps, medidaMs, medidaRuim;
static int medidaN, medidaTransiente, testeGanhos;

int gpun_efeito_ativo(int passo) { return !!(efeitos & (1u << (passo - 1))); }
static void aplicarEscada(void) {
  aplicar(gpun_efeito_ativo(4) ? 2 : gpun_efeito_ativo(3) ? 1 : 0,
          "automatic effects ladder");
}

static void reiniciarMedida(void) {
  decidido = bloqueado = 0;
  relatoCena = relatosLentos = relatosRapidos = relatosValidos = 0;
  degrau = foscoVetado = recuperacaoEspera = 0;
  relatoTentativa = -1;
  efeitos = semGanho = 0; testePasso = 0;
  medidaMs = medidaRuim = 0; medidaN = medidaTransiente = testeGanhos = 0;
  memset(fpsNivel, 0, sizeof fpsNivel);
}

#ifndef GL_FRAMEBUFFER_BINDING
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif
#define NV_GL_COLOR   0x1800   // GL_COLOR (ES3) == GL_COLOR_EXT
#define NV_GL_DEPTH   0x1801
#define NV_GL_STENCIL 0x1802

static unsigned long djb2(const char *s, unsigned long h) {
  while (s && *s) h = h * 33u + (unsigned char)*s++;
  return h;
}

static const char *glTxt(GLenum e) {
  const char *s = (const char *)glGetString(e);
  return s ? s : "?";
}

#ifdef NV_TPK
// Modelo e versao da Tizen pela API nativa (a mesma que o host .NET le em
// Tizen.System.Information), por dlopen: o link da .so nao ganha dependencia
// nova, e sem a biblioteca fica "?".
static void infoPlataforma(void) {
  static const char *const libs[] = { "libcapi-system-info.so.0", "libcapi-system-info.so", NULL };
  int (*get)(const char *, char **) = NULL;
  void *h = NULL;
  int i;
  for (i = 0; libs[i] && !h; i++) h = dlopen(libs[i], RTLD_NOW);
  if (h) *(void **)&get = dlsym(h, "system_info_get_platform_string");
  if (!get) return;
  { char *v = NULL;
    if (get("http://tizen.org/feature/platform.version", &v) == 0 && v) {
      snprintf(tizen, sizeof tizen, "%s", v); free(v); video_tpk_tizen_definir(tizen); } }
  { char *v = NULL;
    if (get("http://tizen.org/system/model_name", &v) == 0 && v) {
      snprintf(modelo, sizeof modelo, "%s", v); free(v); } }
}

// "OpenGL ES 3.2 ..." -> 3. So um contexto que SE DIZ 3.x ganha as funcoes de 3.x.
static int versaoEs(const char *v) {
  int ma = 0, mi = 0;
  if (v && sscanf(v, "OpenGL ES %d.%d", &ma, &mi) >= 1) return ma;
  return 0;
}

// Extensao inteira na lista (sem casar prefixo de outra).
static int temExt(const char *lista, const char *nome) {
  size_t n = strlen(nome);
  const char *p = lista;
  while (p && (p = strstr(p, nome)) != NULL) {
    if ((p == lista || p[-1] == ' ') && (p[n] == ' ' || p[n] == 0)) return 1;
    p += n;
  }
  return 0;
}

// A lista em linhas de ~480 caracteres, sem o prefixo "GL_" (compacta).
static void logExtensoes(const char *ext) {
  char linha[560];
  int n = 0, total = 0;
  const char *p = ext;
  linha[0] = 0;
  while (p && *p) {
    const char *f;
    size_t t;
    while (*p == ' ') p++;
    if (!*p) break;
    f = strchr(p, ' ');
    t = f ? (size_t)(f - p) : strlen(p);
    if (t > 3 && !strncmp(p, "GL_", 3)) { p += 3; t -= 3; }
    if (n + (int)t + 1 > 480) { printf("[gl] ext: %s\n", linha); n = 0; linha[0] = 0; }
    if (t < sizeof linha - 2) { memcpy(linha + n, p, t); n += (int)t; linha[n++] = ' '; linha[n] = 0; }
    total++;
    p += t;
  }
  if (n) printf("[gl] ext: %s\n", linha);
  printf("[gl] %d extensoes\n", total);
}
#endif

static void gravar(void) {
  char buf[1024];
  if (!adaptativo) return;
  snprintf(buf, sizeof buf, "versao=1\nchave=%lx\nnivel=%d\ngpu=%s\ngl=%s\nmodelo=%s\ntizen=%s\n"
           "avaliacao=1\nbloqueado=%d\nfps0=%.3f\nfps1=%.3f\nfps2=%.3f\nfoscoVetado=%d\n",
           chave, nivel, renderer, versaoGl, modelo, tizen,
           bloqueado, fpsNivel[0], fpsNivel[1], fpsNivel[2], foscoVetado);
  if (!dados_gravar(GPUN_ARQ, buf)) printf("[gpu-nivel] nao gravou %s\n", GPUN_ARQ);
}

#if (defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_GPUN_TESTE)) && !defined(NV_TPK_NIVEL_FORCADO)
static void ler(void) {
  char *t = dados_ler(GPUN_ARQ);
  unsigned long c = 0;
  int v = 0, n = -1, veto = 0;
  const char *p;
  if (!t) { printf("[gpu-nivel] sem %s: comeca do 0\n", GPUN_ARQ); return; }
  if ((p = strstr(t, "versao=")) != NULL) v = atoi(p + 7);
  if ((p = strstr(t, "chave=")) != NULL) c = strtoul(p + 6, NULL, 16);
  if ((p = strstr(t, "nivel=")) != NULL) n = atoi(p + 6);
  if ((p = strstr(t, "foscoVetado=")) != NULL) veto = atoi(p + 12) == 1;
  free(t);
  if (v != 1 || n < 0 || n > 3) { printf("[gpu-nivel] %s invalido: comeca do 0\n", GPUN_ARQ); return; }
  if (c != chave) {
    printf("[gpu-nivel] %s de outra GPU/driver/firmware (chave %lx, agora %lx): mede de novo do 0\n",
           GPUN_ARQ, c, chave);
    return;
  }
  // Reprobe recoverable effects from full quality on every launch. Only the
  // frost veto is hardware evidence worth retaining across sessions.
  foscoVetado = veto;
  nivel = degrau = 0;
}
#endif

static void aplicar(int n, const char *porque) {
  if (n < 0) n = 0;
  if (n > 3) n = 3;
  // No arranque (porque vazio) so fala quando ha o que dizer: na LG e no .wgt
  // o nivel e sempre 0 e o log deles nao ganha linha nova.
  if (n != nivel || (!porque[0] && strcmp(origem, "padrao")))
    printf("[gpu-nivel] nivel %d -> %d (%s)\n", nivel, n, porque[0] ? porque : origem);
  nivel = n;
  // Minimal's global flat-glass shortcut would override forced frost/outline.
  // Automatic uses only the light budget after its animation step.
  gfx_definir_efeitos_leves(adaptativo ? gpun_efeito_ativo(3) &&
                           (ajustes_efeitos_automaticos() & 4) : nivel >= 1);
  gfx_definir_efeitos_minimos(!adaptativo && nivel >= 2);
  fflush(stdout);
}

int gpun_upscale_suportado(void) {
#ifdef NV_TPK
  if (!tpkEgl.GetCurrentContext || eglGetCurrentContext() == EGL_NO_CONTEXT) return 0;
#else
  if (!SDL_GL_GetCurrentContext()) return 0;
#endif
  const char *gpu = (const char *)glGetString(GL_RENDERER);
  if (!gpu || strstr(gpu, "Mali-4")) return 0;
#if !defined(__APPLE__) || defined(NV_GLES_NO_MAC)
  GLint range[2], precision = 0;
  glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
  if (!precision) return 0;
#endif
  return 1;
}

void gpun_ampliar_app(int app) {
  // Owner scope (#205): app scaling covers 720->1080, 720->4K and 1080->4K.
  // Automatic/4K falling back to a 1080 target is the same opt-in 1080->4K path.
  // The caller supplies panel eligibility via ajustes_ampliar_app(). A 4K panel
  // may grant only 1080p (C9); drawable size must not veto its 720p upscaler.
  int novo = app && upscaleSgsr;
  if (novo == ampliarApp) return;
  ampliarApp = novo;
  if ((!forca720 && !alvo1080) || (forca720 && telaW <= 1280 && telaH <= 720))
    printf("[gpu-nivel] ampliar=tv upscale=bilinear %dx%d -> %dx%d\n",
           telaW, telaH, telaW, telaH);
}

static void alvoApagar(GLuint *fbo, GLuint *tex) {
  if (*fbo) glDeleteFramebuffers(1, fbo);
  if (*tex) glDeleteTextures(1, tex);
  *fbo = *tex = 0;
}

// RGBA target; returns the FBO status (0 when both are already deleted).
static GLenum alvoCriar(GLuint *fbo, GLuint *tex, int w, int h) {
  GLint ant = 0;
  GLenum st;
  glGenTextures(1, tex);
  glBindTexture(GL_TEXTURE_2D, *tex);
  // RGBA: o alpha e o canal do furo do video (gfx_furo) e tem de chegar a janela.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &ant);
  glGenFramebuffers(1, fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
  st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  if (st != GL_FRAMEBUFFER_COMPLETE) alvoApagar(fbo, tex);
  return st;
}

void gpun_iniciar(int w, int h) {
  const char *ext = "";
  GLint db = 0, sb = 0;
  reiniciarMedida();
  degrau = foscoVetado = 0;
  nivel = adaptativo = prefFixa = 0; origem = "padrao";
  alvoApagar(&intFbo, &intTex);
  alvoApagar(&rcasFbo, &rcasTex);
  rcasFalhou = 0;
  intW = intH = intFalhou = intLigado = alvo1080 = forca720 = ampliarApp = 0;
  telaW = w > 0 ? w : 1920;
  telaH = h > 0 ? h : 1080;
#ifdef NV_ANDROID
  // #266: marca onde a Shield trava dentro de "rede_preparar" (as consultas GL
  // logo apos o primeiro SwapWindow sao a outra suspeita).
  android_etapa("gpun_iniciar: glGetString");
  printf("[gl] consultando o driver\n"); fflush(stdout);
#endif
  snprintf(renderer, sizeof renderer, "%s", glTxt(GL_RENDERER));
  snprintf(versaoGl, sizeof versaoGl, "%s", glTxt(GL_VERSION));
  upscaleSgsr = gpun_upscale_suportado();
  ext = glTxt(GL_EXTENSIONS);
#ifdef NV_ANDROID
  android_etapa("gpun_iniciar: GL_DEPTH_BITS");
#endif
  glGetIntegerv(GL_DEPTH_BITS, &db);
  glGetIntegerv(GL_STENCIL_BITS, &sb);
#ifdef NV_TPK
  infoPlataforma();
  printf("[gl] GL_VERSION=%s\n", versaoGl);
  printf("[gl] GL_RENDERER=%s | GL_VENDOR=%s\n", renderer, glTxt(GL_VENDOR));
  printf("[gl] GL_SHADING_LANGUAGE_VERSION=%s\n", glTxt(GL_SHADING_LANGUAGE_VERSION));
  printf("[gl] janela: profundidade=%d stencil=%d | TV %s / Tizen %s\n", (int)db, (int)sb, modelo, tizen);
  logExtensoes(ext);
  // ES3 so se o contexto SE DIZ 3.x e o ponteiro existe; senao a extensao.
  if (tpkEgl.GetProcAddress) {
    if (versaoEs(versaoGl) >= 3) {
      *(void **)&descarte = tpkEgl.GetProcAddress("glInvalidateFramebuffer");
      if (descarte) descarteNome = "glInvalidateFramebuffer (ES3)";
    }
    if (!descarte && temExt(ext, "GL_EXT_discard_framebuffer")) {
      *(void **)&descarte = tpkEgl.GetProcAddress("glDiscardFramebufferEXT");
      if (descarte) descarteNome = "glDiscardFramebufferEXT";
    }
  }
  profStencil = db > 0 || sb > 0;
  printf("[gl] descarte de alvo: %s%s\n", descarteNome,
         descarte && profStencil ? " (+ profundidade/stencil da janela no fim do quadro)" : "");
#elif defined(NV_ANDROID)
  // Same facts as the .tpk log, for the Android field logs: which GPU, which
  // buffers the window got (a depth/stencil the app never asked for is
  // bandwidth written back every frame unless discarded below).
  printf("[gl] GL_RENDERER=%s | %s\n", renderer, versaoGl);
  printf("[gl] janela: profundidade=%d stencil=%d\n", (int)db, (int)sb);
  // SEM DESCARTE NO ANDROID (#318). A 2.0.1 carregou o glDiscardFramebufferEXT
  // e, com ele, gpun_descartar_cor passou a valer nos FBOs do desfoque, que
  // rodam justo quando a tela nova traz arte nova. No Xiaomi MiTV-AFKR0
  // (Mali-G31) a tela piscava e congelava depois do OK; a teste-318.1, sem
  // ele (e sem o gputempo), consertou. O ganho nunca foi medido no Android: os
  // alvos do desfoque sao pequenos e so rodam na troca de arte, e a
  // profundidade da janela o EGL ja nao preserva depois do swap (os drivers
  // Mali/Adreno nem a gravam de volta). Pouco a poupar, risco provado. Fica so
  // a linha do log, para o relato de campo.
  (void)ext;
  profStencil = 0;
  printf("[gl] descarte de alvo: nenhum (Android, #318)\n");
#else
  (void)ext; (void)db; (void)sb;
#ifdef __APPLE__
  snprintf(modelo, sizeof modelo, "mac");
#elif defined(NV_LINUX_DESKTOP)
  snprintf(modelo, sizeof modelo, "linux-desktop");
#endif
#endif
  ptv_definir_gpu_fraca(ptv_gpu_fraca(renderer));
#ifdef NV_TPK
  ptv_definir_gpu_utgard(ptv_gpu_utgard(renderer));   // #286: mip na CPU e heroi de 960
#endif
  chave = djb2(tizen, djb2(modelo, djb2(versaoGl, djb2(renderer, 5381))));
  // The level learned on a 4K surface says nothing about 1080p (4x the pixels):
  // a separate key, so a 4K session that dropped effects does not carry them
  // to the 1080p one. 1080p keeps the old key (no re-measure after updating).
  if (telaW > (int)NV_TELA_W) chave = djb2("4k", chave);

#if defined(NV_TPK_NIVEL_FORCADO)
  nivel = NV_TPK_NIVEL_FORCADO;
  origem = "forcado na build (NV_TPK_NIVEL_FORCADO)";
#elif defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS)
  // ANDROID tambem mede: TV box e Google TV vao de Mali-G52 a GPUs bem mais
  // fortes. Na TCL Smart TV Pro o vidro + cor viva dava 29 fps sustentado.
  // LG TAMBEM (2.0.1): log de uma LG webOS 5+ com Mali-G52 r23 na 2.0.0, 668
  // amostras de FPS, mediana 25 e 504 abaixo de 40, em todas as telas (perfil,
  // menu, ajustes, home) e com vidro desligado. A LG ficava no nivel 0 para
  // sempre. Quem roda a 60 (C9, Mali-G510, Mali-G52 r46) nao desce: a regra
  // so mexe com FPS < 45 e a espera da GPU dominando o quadro.
  adaptativo = 1;
  origem = "adaptativo";
  ler();
  // Mesmo GPU fraca precisa da referencia no 0 antes de reduzir efeitos.
#else
  { const char *e = getenv("NUVIO_GPU_NIVEL");
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
    if (e && *e) { nivel = atoi(e); origem = "NUVIO_GPU_NIVEL"; }
#else
    (void)e;
#endif
  }
#endif
  aplicar(nivel, "");
}

// Effects preference on every platform: 0 automatic, 1 full, 2 light, 3 minimal.
// Completos/Leves fixam o nivel e desligam a medida; Automatico volta a medir
// a partir do que esta gravado (ou do 0). Build com NV_TPK_NIVEL_FORCADO
// ignora: o canario de teste manda.
void gpun_forcar_720(void) {
  forca720 = 1;
  printf("[gpu-nivel] ajuste: interface 720p, efeitos independentes\n");
}

void gpun_preferencia(int p) {
#if !defined(NV_TPK_NIVEL_FORCADO)
  prefFixa = p >= 1 && p <= 3;
  if (prefFixa) {
    efeitos = semGanho = 0;
    testePasso = degrau = 0;
  }
  if (p == 3) { adaptativo = 0; aplicar(2, "ajuste: efeitos minimos"); return; }
  if (p == 1) { adaptativo = 0; aplicar(0, "ajuste: efeitos completos"); return; }
  if (p == 2) {
    adaptativo = 0;
    // Mali-4xx stalls at level 1; preserve its established Light safeguard.
    aplicar(ptv_gpu_utgard(renderer) ? 2 : 1, "ajuste: efeitos leves");
    return;
  }
  reiniciarMedida();
  adaptativo = 1; origem = "adaptativo"; nivel = 0;
#if defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_GPUN_TESTE)
  ler();
#endif
  aplicar(nivel, "ajuste: automatico");
#else
  (void)p;
#endif
}

void gpun_alvo_1080(void) {
  if (alvo1080 || forca720 || telaW <= (int)NV_TELA_W) return;
  alvo1080 = 1;
  printf("[gpu-nivel] interface 4K -> alvo interno %dx%d ampliado para %dx%d\n",
         (int)NV_TELA_W, (int)NV_TELA_H, telaW, telaH);
  // The level measured at 4K was paid for 4x the pixels: measure again at
  // 1080p, from what this GPU saved for 1080p (the old key), unless the person
  // fixed "Efeitos visuais".
  chave = djb2(tizen, djb2(modelo, djb2(versaoGl, djb2(renderer, 5381))));
#if (defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_GPUN_TESTE)) && !defined(NV_TPK_NIVEL_FORCADO)
  if (!prefFixa) {
    reiniciarMedida();
    adaptativo = 1; origem = "adaptativo"; nivel = 0;
    ler();
    aplicar(nivel, "interface em 1080p: mede de novo");
  }
#else
  (void)prefFixa;
#endif
  fflush(stdout);
}
int gpun_alvo_1080_ativo(void) { return alvo1080; }

int gpun_nivel(void) { return nivel; }
int gpun_degrau(void) { return degrau; }
int gpun_fosco_vetado(void) { return foscoVetado || gpun_efeito_ativo(2); }
void gpun_fosco_reiniciar(void) {
  foscoVetado = 0;
  efeitos &= ~2u; semGanho &= ~2u;
  if (testePasso) efeitos &= ~(1u << (testePasso - 1));
  testePasso = 0; relatosLentos = relatosRapidos = 0;
  degrau = 4;
  while (degrau && !gpun_efeito_ativo(degrau)) degrau--;
  if (adaptativo) aplicarEscada();
  gravar();
}
int gpun_efeitos_automaticos(void) { return adaptativo && nivel > 0; }
void gpun_definir_nivel(int n) { aplicar(n, "definido por gpun_definir_nivel"); }

void gpun_log_perfil(long memMB, int texMb, int fios, int heroi) {
#ifdef NV_TPK
  printf("[perfil] tpk mem=%ldMB gpu=\"%s\"%s tizen=%s modelo=%s -> tex=%dMB fios=%d heroi=%d"
         " escala=%s efeitos=%s nivel=%d (%s)\n",
         memMB, renderer, ptv_gpu_fraca_atual() ? " (fraca)" : "", tizen, modelo, texMb, fios, heroi,
         forca720 ? "1280x720->1920x1080" : alvo1080 ? "1920x1080->4K" : "1920x1080",
         nivel >= 2 ? "minimos" : nivel >= 1 ? "leves" : "cheios", nivel, origem);
  fflush(stdout);
#else
  (void)memMB; (void)texMb; (void)fios; (void)heroi;
#endif
}

void gpun_descartar_cor(int padrao) {
  GLenum a = padrao ? NV_GL_COLOR : GL_COLOR_ATTACHMENT0;
  if (descarte) descarte(GL_FRAMEBUFFER, 1, &a);
}

void gpun_redimensionar(int w, int h) {
#ifdef NV_TOUCH_UI
  if (w <= 0 || h <= 0) return;
  // Switching TV/Mobile can change the logical canvas without resizing the
  // drawable. A fixed logical target must be rebuilt in that case too.
  if (w == telaW && h == telaH &&
      (!alvo1080 || (intW == (int)NV_TELA_W && intH == (int)NV_TELA_H))) return;
  if (intLigado) glBindFramebuffer(GL_FRAMEBUFFER, 0);
  alvoApagar(&intFbo, &intTex);
  alvoApagar(&rcasFbo, &rcasTex);
  rcasFalhou = 0;
  intW = intH = intFalhou = intLigado = 0;
  telaW = w; telaH = h;
#else
  (void)w; (void)h;
#endif
}

int gpun_ampliacao_ativa(void) {
  return ampliarApp && !intFalhou && intW > 0 && intH > 0 &&
         telaW > intW && telaH > intH;
}

// Settings > This TV > Upscaling algorithm, applied only while the app upscales.
// 0 Automatic: fractional SGSR, integer Sharp-Bilinear (the original rule).
// 1 SGSR, 2 SGSR sharp (+RCAS), 3 FSR 1 (EASU+RCAS), 4 Lanczos, 5 Bilinear.
enum { UP_BILINEAR, UP_NITIDA, UP_SGSR, UP_EASU, UP_LANCZOS };
static int upEtapa(void) {
  if (!gpun_ampliacao_ativa()) return UP_BILINEAR;
  switch (upAlgo) {
    case 1: case 2: return UP_SGSR;
    case 3: return UP_EASU;
    case 4: return UP_LANCZOS;
    case 5: return UP_BILINEAR;
  }
  return telaW % intW || telaH % intH ? UP_SGSR : UP_NITIDA;
}
static int upRcas(void) {
  return gpun_ampliacao_ativa() && (upAlgo == 2 || upAlgo == 3) && upNitidez > 0 && !rcasFalhou;
}
static const char *upscaleAlgoNome(void) {
  static const char *N[] = { "bilinear", "sharp-bilinear", "sgsr", "easu", "lanczos" };
  static const char *R[] = { "", "", "sgsr+rcas", "easu+rcas", "" };
  return upRcas() ? R[upEtapa()] : N[upEtapa()];
}

void gpun_ampliacao(int algo, int nitidez) {
  if (algo == upAlgo && nitidez == upNitidez) return;
  upAlgo = algo; upNitidez = nitidez;
  printf("[gpu-nivel] choice=%d upscale=%s nitidez=%d app=%d target=%dx%d drawable=%dx%d active=%d\n",
         upAlgo, upscaleAlgoNome(), upNitidez, ampliarApp, intW, intH, telaW, telaH,
         gpun_ampliacao_ativa());
  fflush(stdout);
}

static int intPreparar(void) {
  GLenum st;
  if (intFbo) return 1;
  if (intFalhou) return 0;
  if (alvo1080) { intW = (int)NV_TELA_W; intH = (int)NV_TELA_H; }
  else if (gfx_hdr_output && !forca720) { intW = telaW; intH = telaH; }
  else { intW = 1280; intH = 720; }
  st = alvoCriar(&intFbo, &intTex, intW, intH);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    printf("[gpu-nivel] alvo interno %dx%d incompleto (0x%x)%s\n", intW, intH, (unsigned)st,
           ": segue na superficie inteira");
    intFalhou = 1;
    // Target failure must not override the person's effects choice.
    return 0;
  }
  printf("[gpu-nivel] ampliar=%s upscale=%s %dx%d -> %dx%d\n",
         ampliarApp ? "app" : "tv", upscaleAlgoNome(),
         intW, intH, telaW, telaH);
  fflush(stdout);
  return 1;
}

void gpun_quadro_inicio(void) {
  intLigado = 0;
  // A granted 720p drawable is already scaled by the platform compositor.
  if (!gfx_hdr_output && forca720 && telaW <= 1280 && telaH <= 720) return;
  if ((!gfx_hdr_output && !forca720 && !alvo1080) || !intPreparar()) return;
  glBindFramebuffer(GL_FRAMEBUFFER, intFbo);
  glViewport(0, 0, intW, intH);
  gfx_tamanho_alvo(intW, intH);
  intLigado = 1;
  // O glClear de main.c vem logo depois e ja diz a GPU que nada do quadro
  // anterior precisa ser lido neste alvo.
}

void gpun_quadro_fim(void) {
  if (intLigado) {
    GfxRect tela = { 0, 0, NV_TELA_W, NV_TELA_H };
    float asp = gfx_tex_aspect_atual, op = gfx_opacidade_grupo;
    intLigado = 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, telaW, telaH);
    gfx_tamanho_alvo(telaW, telaH);
    glDisable(GL_SCISSOR_TEST);
    // A janela inteira vai ser coberta pela ampliacao, opaca: nada dela
    // precisa ser lido. Sem descarte, um clear diz o mesmo a GPU.
    if (descarte) gpun_descartar_cor(1);
    else { glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT); }
    glDisable(GL_BLEND);   // copia RGB E alpha exatos (o furo do video)
    gfx_tex_aspect_atual = 0.0f;
    gfx_opacidade_grupo = 1.0f;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    // RCAS reads the upscaled frame back from a drawable-sized target.
    int rcas = !gfx_hdr_output && upRcas();
    if (rcas && !rcasFbo && alvoCriar(&rcasFbo, &rcasTex, telaW, telaH) != GL_FRAMEBUFFER_COMPLETE) {
      printf("[gpu-nivel] alvo RCAS %dx%d incompleto: sem nitidez\n", telaW, telaH);
      rcasFalhou = 1; rcas = 0;
    }
    if (rcas) glBindFramebuffer(GL_FRAMEBUFFER, rcasFbo);
    if (gfx_hdr_output)
      gfx_rect(tela, intTex, GFX_HDR_OUTPUT, (float)gfx_hdr_output, 0, 0, 0, 1, 1, 1, 1);
    else switch (upEtapa()) {
      case UP_SGSR:
        gfx_rect(tela, intTex, GFX_COPIA, 0, (float)intW, (float)intH, 0.0f, 1, 1, 1, 1);
        break;
      case UP_NITIDA:
        gfx_rect(tela, intTex, GFX_COPIA_NITIDA, (float)telaW / intW,
                 1.0f / intW, 1.0f / intH, 0.0f, 1, 1, 1, 1);
        break;
      case UP_EASU:
        gfx_rect(tela, intTex, GFX_COPIA_EASU, 0, (float)intW, (float)intH, 0.0f, 1, 1, 1, 1);
        break;
      case UP_LANCZOS:
        gfx_rect(tela, intTex, GFX_COPIA_LANCZOS, 0, (float)intW, (float)intH, 0.0f, 1, 1, 1, 1);
        break;
      default:
        gfx_rect(tela, intTex, GFX_COPIA, 0, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1);
    }
    if (rcas) {
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      gfx_rect(tela, rcasTex, GFX_RCAS, upNitidez / 4.0f,
               1.0f / telaW, 1.0f / telaH, 0.0f, 1, 1, 1, 1);
    }
    // Solta a textura do alvo: no quadro seguinte ela volta a ser o alvo de
    // desenho, e ficar ligada para leitura ao mesmo tempo e o laco de
    // realimentacao que o GLES deixa indefinido.
    glBindTexture(GL_TEXTURE_2D, 0);
    gfx_tex_esquecer(0);
    gfx_tex_aspect_atual = asp;
    gfx_opacidade_grupo = op;
    glEnable(GL_BLEND);
  }
  // Profundidade/stencil da janela nao sao lidos por ninguem depois do swap.
  if (descarte && profStencil) {
    static const GLenum ps[2] = { NV_GL_DEPTH, NV_GL_STENCIL };
    descarte(GL_FRAMEBUFFER, 2, ps);
  }
}

#ifdef NV_GPUN_TESTE
// tests/gpunivel.sh: a regra do adaptativo sem TV nem GL.
void gpun_teste_reiniciar(void) {
  nivel = 0; adaptativo = 1; decidido = 0; origem = "teste";
  reiniciarMedida();
  forca720 = prefFixa = 0;
  aplicar(0, "teste");
}
int gpun_teste_decidido(void) { return decidido; }
const char *gpun_teste_upscale(int sw, int sh, int iw, int ih, int app) {
  telaW = sw; telaH = sh; intW = iw; intH = ih; ampliarApp = app;
  return upscaleAlgoNome();
}
#endif

void gpun_relatar(double fps, double pior, double periodo, int cena, int cheia) {
  double alvo = periodo > 0 ? 1000.0 / periodo : 60.0;
  int automaticos = ajustes_efeitos_automaticos();
  if (medidaTransiente) cheia = 0;
  medidaTransiente = 0;
  if (medidaN) {
    if (medidaRuim > (medidaMs + medidaRuim) * 0.2 || medidaN < 10) cheia = 0;
    if (medidaMs > 0) fps = medidaN * 1000.0 / medidaMs;
  } else if (medidaRuim > 0) cheia = 0;
  medidaMs = medidaRuim = 0; medidaN = 0;
  if (!adaptativo || nivel > GPUN_NIVEL_AUTO_MAX) return;
  if (alvo > 60.0) alvo = 60.0;
  if (cena != relatoCena) {
    if (testePasso) {
      efeitos &= ~(1u << (testePasso - 1)); degrau = testeAntes;
      printf("[gpu-escada] step %d -> %d fps %.1f/%.1f, reverted, scene changed\n",
             testeAntes, testePasso, testeFps, fps);
      testePasso = 0; aplicarEscada();
    }
    relatoCena = cena;
    relatosLentos = relatosRapidos = relatosValidos = 0;
    return; // Discard the first window after a scene/player transition.
  }
  if (!cena || !cheia || !(fps > 0) || pior > 1000.0) {
    relatosLentos = relatosRapidos = 0;
    if (testePasso) {
      efeitos &= ~(1u << (testePasso - 1)); degrau = testeAntes;
      testePasso = testeGanhos = 0; aplicarEscada();
    }
    if (pior > 1000.0)
      printf("[gpu-nivel] report ignored: fps=%.1f worst=%.1fms (stall)\n", fps, pior);
    return;
  }
  if (testePasso) {
    int passo = testePasso;
    int ganho = fps >= testeFps * 1.10 && fps >= testeFps + 3.0;
    if (ganho && ++testeGanhos < 3) return;
    testeGanhos = 0;
    if (!ganho) {
      efeitos &= ~(1u << (passo - 1)); semGanho |= 1u << (passo - 1);
      degrau = testeAntes; aplicarEscada();
    } else if (passo == 2) { foscoVetado = 1; gravar(); }
    printf("[gpu-escada] step %d -> %d fps %.1f/%.1f, %s, %s\n",
           testeAntes, passo, testeFps, fps, ganho ? "kept" : "reverted",
           ganho ? "gain >=10% and >=3 fps" : "no gain this boot");
    testePasso = 0; relatosLentos = relatosRapidos = 0;
    return;
  }
  relatosValidos++;
  // 55/58 FPS on 60 Hz; preserve healthy 50 Hz panels (45/48 FPS).
  relatosLentos = fps < alvo - 5.0 ? relatosLentos + 1 : 0;
  relatosRapidos = fps >= alvo - 2.0 ? relatosRapidos + 1 : 0;
  if (recuperacaoEspera > 0) recuperacaoEspera--;
  int proximo = degrau;
  if (relatosLentos >= 3) {
    // Visit every rung, including forced/vetoed effects, in separate windows.
    // A disabled effect never makes the next effect an immediate jump.
    if (degrau < 4) proximo = degrau + 1;
    if (proximo != degrau && relatoTentativa >= 0) {
      // Retry after five minutes of eligible reports, never a permanent floor.
      recuperacaoEspera = 100;
      relatoTentativa = -1;
    }
  } else if (relatosRapidos >= 10 && degrau > 0 && !recuperacaoEspera) {
    relatoTentativa = degrau;
    proximo--;
    while (proximo > 0 && (!gpun_efeito_ativo(proximo) || !(automaticos & (1 << (proximo - 1))) ||
                          (proximo == 2 && foscoVetado))) proximo--;
  }
  if (proximo != degrau) {
    if (proximo > degrau) {
      unsigned bit = 1u << (proximo - 1);
      if ((automaticos & bit) && !(semGanho & bit) &&
          !(proximo == 2 && foscoVetado)) {
        testeAntes = degrau; testePasso = proximo; testeFps = fps;
        testeGanhos = 0;
        efeitos |= bit;
      }
    } else {
      efeitos &= ~(1u << (degrau - 1));
      printf("[gpu-escada] step %d -> %d fps %.1f/%.1f, kept, recovery trial\n",
             degrau, proximo, fps, fps);
    }
    degrau = proximo;
    proximo = degrau >= 4 ? 2 : degrau >= 3 ? 1 : 0;
    printf("[gpu-nivel] live decision: level %d -> %d fps=%.1f target=%.1f slow=%d fast=%d step=%d\n",
           nivel, proximo, fps, alvo, relatosLentos, relatosRapidos, degrau);
    aplicarEscada();
    relatosLentos = relatosRapidos = 0;
    decidido = 1; bloqueado = 0;
    fpsNivel[nivel] = fps;
    gravar();
  } else if (relatosValidos == 3) {
    printf("[gpu-nivel] live validation: level=%d fps=%.1f target=%.1f reports=%d\n",
           nivel, fps, alvo, relatosValidos);
  }
}

void gpun_medir(double dtms, double espera, double cpu, int cena, int cheia) {
  (void)espera;
  if (!cena || !cheia) { medidaTransiente = 1; return; }
  if (dtms <= 0) return;
  // Reject decode/upload, event/list work and isolated scroll-start stalls.
  if (cpu > 8.0 || dtms > 100.0) medidaRuim += dtms;
  else { medidaMs += dtms; medidaN++; }
}
