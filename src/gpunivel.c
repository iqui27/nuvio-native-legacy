// Nivel de GPU do .tpk. O porque, os niveis e a regra estao em gpunivel.h.
//
// SEM _Thread_local (a .so do Tizen 4/5 recusa TLS, tests/tpk40_tls.sh): tudo
// aqui roda no fio de desenho, e os estaticos sao so dele.
#include "gpunivel.h"
#ifdef NV_ANDROID
#include "android.h"
#endif
#include "gfx.h"
#include "dados.h"
#include "perfiltv.h"
#include "layout.h"
#include "gl_compat.h"
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

#define GPUN_NIVEL_AUTO_MAX 2   // efeitos minimos; o 3 (720p) so forcado, ver gpun_medir
#define GPUN_ARQ "gpu-nivel.txt"
// Regra do adaptativo (gpunivel.h). Os numeros:
//  - 45 fps: abaixo disso o registro 8825 (22-29) e o jank visivel; a Tizen 6
//    que roda a 60 fica longe dele, entao ela nunca desce.
//  - espera >= 6 ms e maior que a CPU: o quadro e decidido pelo que a GPU
//    devolve, e nao pelo que a CPU submete (no 8825: espera 62, CPU 5).
//  - janela de 4 s, aquecimento de 3 s na primeira entrada na home (a subida
//    das primeiras artes e trabalho de CPU/upload, nao do regime) e 2 s depois
//    de cada degrau; teto de 24 s de home medida por arranque.
#define GPUN_FPS_BOM     45.0
// Abaixo disto, JA com efeitos leves, a tela esta travada: tira mais efeitos
// (registro 9859: Mali-400, Tizen 4.0, 10-21 fps no 1).
#define GPUN_FPS_CRITICO 25.0
#define GPUN_ESPERA_MIN   6.0
#define GPUN_JANELA_MS 4000.0
#define GPUN_AQUECE_MS 3000.0
#define GPUN_ASSENTA_MS 2000.0
#define GPUN_TETO_MS  24000.0

static int nivel = 0, adaptativo = 0, decidido = 0;
static const char *origem = "padrao";
static char renderer[160] = "?", versaoGl[160] = "?", modelo[96] = "?", tizen[32] = "?";
static unsigned long chave;
static int telaW = 1920, telaH = 1080;
// Alvo interno do nivel 3 (720p) ou do recuo de 4K (1920x1080, gpun_alvo_1080).
static GLuint intFbo, intTex;
static int alvo1080, prefFixa;
static int intW, intH, intFalhou, intLigado;
// Descarte (glInvalidateFramebuffer ou glDiscardFramebufferEXT).
typedef void (*PfnDescarte)(GLenum, GLsizei, const GLenum *);
static PfnDescarte descarte;
#ifdef NV_TPK
static const char *descarteNome = "nenhum";
#endif
static int profStencil;   // a janela tem profundidade/stencil para descartar
// Medida.
static double aquece = GPUN_AQUECE_MS, janMs, janEsp, janCpu, totalMs;
static int janN, estavaNaHome, descartes;
// #410B: so vale perder efeitos com >= 15% E >= 5 fps. Os dois pisos
// rejeitam ruido perto de 45 fps e ganhos absolutos pequenos em TV muito lenta.
// ponytail: janelas de Home cheia, nao cena fixa; usar A/B/A se a navegacao
// variar a carga o bastante para falsear esta comparacao.
static double fpsNivel[3];
static int anterior = -1, reavaliar, bloqueado;

static void reiniciarMedida(void) {
  decidido = bloqueado = reavaliar = 0; anterior = -1;
  memset(fpsNivel, 0, sizeof fpsNivel);
  aquece = GPUN_AQUECE_MS; janMs = janEsp = janCpu = totalMs = 0;
  janN = estavaNaHome = descartes = 0;
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
      snprintf(tizen, sizeof tizen, "%s", v); free(v); } }
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
           "avaliacao=1\nbloqueado=%d\nfps0=%.3f\nfps1=%.3f\nfps2=%.3f\n",
           chave, nivel, renderer, versaoGl, modelo, tizen,
           bloqueado, fpsNivel[0], fpsNivel[1], fpsNivel[2]);
  if (!dados_gravar(GPUN_ARQ, buf)) printf("[gpu-nivel] nao gravou %s\n", GPUN_ARQ);
}

#if (defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_GPUN_TESTE)) && !defined(NV_TPK_NIVEL_FORCADO)
static void ler(void) {
  char *t = dados_ler(GPUN_ARQ);
  unsigned long c = 0;
  int v = 0, n = -1, avaliado = 0, trava = 0;
  double f[3] = {0};
  const char *p;
  if (!t) { printf("[gpu-nivel] sem %s: comeca do 0\n", GPUN_ARQ); return; }
  if ((p = strstr(t, "versao=")) != NULL) v = atoi(p + 7);
  if ((p = strstr(t, "chave=")) != NULL) c = strtoul(p + 6, NULL, 16);
  if ((p = strstr(t, "nivel=")) != NULL) n = atoi(p + 6);
  if ((p = strstr(t, "avaliacao=")) != NULL) avaliado = atoi(p + 10) == 1;
  if ((p = strstr(t, "bloqueado=")) != NULL) trava = atoi(p + 10) == 1;
  if ((p = strstr(t, "fps0=")) != NULL) f[0] = atof(p + 5);
  if ((p = strstr(t, "fps1=")) != NULL) f[1] = atof(p + 5);
  if ((p = strstr(t, "fps2=")) != NULL) f[2] = atof(p + 5);
  free(t);
  if (v != 1 || n < 0 || n > 3) { printf("[gpu-nivel] %s invalido: comeca do 0\n", GPUN_ARQ); return; }
  if (c != chave) {
    printf("[gpu-nivel] %s de outra GPU/driver/firmware (chave %lx, agora %lx): mede de novo do 0\n",
           GPUN_ARQ, c, chave);
    return;
  }
  nivel = n > GPUN_NIVEL_AUTO_MAX ? GPUN_NIVEL_AUTO_MAX : n;
  origem = "salvo";
  if (avaliado) {
    memcpy(fpsNivel, f, sizeof f);
    bloqueado = decidido = trava;
  } else if (nivel > 0) {
    reavaliar = nivel;
    nivel = 0;
    origem = "reavaliando salvo";
    printf("[gpu-nivel] nivel salvo %d sem comparacao: reavalia uma vez a partir do 0\n", reavaliar);
  }
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
  gfx_definir_efeitos_leves(nivel >= 1);
  gfx_definir_efeitos_minimos(nivel >= 2);
  fflush(stdout);
}

void gpun_iniciar(int w, int h) {
  const char *ext = "";
  GLint db = 0, sb = 0;
  reiniciarMedida();
  nivel = adaptativo = prefFixa = 0; origem = "padrao";
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

// Ajuste "Efeitos visuais" (so .tpk): 0 automatico, 1 completos, 2 leves.
// Completos/Leves fixam o nivel e desligam a medida; Automatico volta a medir
// a partir do que esta gravado (ou do 0). Build com NV_TPK_NIVEL_FORCADO
// ignora: o canario de teste manda.
static int forca720;
void gpun_forcar_720(void) {
  forca720 = 1; adaptativo = 0; decidido = 1;
  aplicar(3, "ajuste: interface 720p");
}

void gpun_preferencia(int p) {
  if (forca720) return;
#if (defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS) || defined(NV_GPUN_TESTE)) && !defined(NV_TPK_NIVEL_FORCADO)
  prefFixa = p == 1 || p == 2;
  if (p == 1) { adaptativo = 0; aplicar(0, "ajuste: efeitos completos"); return; }
  if (p == 2) {
    // Utgard (Mali-400/450/470, registro 1V5YJY, UA40N5300): level 1 still
    // ran 9-17 fps with swap 60-230 ms and a 4-6x fill. "Leves" there means
    // the minimal level, otherwise the setting pins the TV below what the
    // adaptive measure had already learned (saved level 2).
    adaptativo = 0;
    if (strstr(renderer, "Mali-4")) aplicar(2, "ajuste: efeitos leves (Mali-4xx: minimos)");
    else aplicar(1, "ajuste: efeitos leves");
    return;
  }
  reiniciarMedida();
  adaptativo = 1; origem = "adaptativo"; nivel = 0;
  ler();
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
    aquece = GPUN_ASSENTA_MS;
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
int gpun_efeitos_automaticos(void) { return adaptativo && nivel > 0; }
void gpun_definir_nivel(int n) { aplicar(n, "definido por gpun_definir_nivel"); }

void gpun_log_perfil(long memMB, int texMb, int fios, int heroi) {
#ifdef NV_TPK
  printf("[perfil] tpk mem=%ldMB gpu=\"%s\"%s tizen=%s modelo=%s -> tex=%dMB fios=%d heroi=%d"
         " escala=%s efeitos=%s nivel=%d (%s)\n",
         memMB, renderer, ptv_gpu_fraca_atual() ? " (fraca)" : "", tizen, modelo, texMb, fios, heroi,
         nivel >= 3 ? "1280x720->1920x1080" : alvo1080 ? "1920x1080->4K" : "1920x1080",
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

static int intPreparar(void) {
  GLint ant = 0;
  GLenum st;
  if (intFbo) return 1;
  if (intFalhou) return 0;
  if (alvo1080) { intW = (int)NV_TELA_W; intH = (int)NV_TELA_H; }
  else { intW = (telaW * 2 + 1) / 3; intH = (telaH * 2 + 1) / 3; }
  glGenTextures(1, &intTex);
  glBindTexture(GL_TEXTURE_2D, intTex);
  // RGBA: o alpha e o canal do furo do video (gfx_furo) e tem de chegar a janela.
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, intW, intH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &ant);
  glGenFramebuffers(1, &intFbo);
  glBindFramebuffer(GL_FRAMEBUFFER, intFbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, intTex, 0);
  st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)ant);
  if (st != GL_FRAMEBUFFER_COMPLETE) {
    printf("[gpu-nivel] alvo interno %dx%d incompleto (0x%x)%s\n", intW, intH, (unsigned)st,
           alvo1080 ? ": segue na superficie inteira" : ": fica no nivel 1");
    glDeleteFramebuffers(1, &intFbo); glDeleteTextures(1, &intTex);
    intFbo = intTex = 0; intFalhou = 1;
    if (!alvo1080) aplicar(1, "sem alvo interno");
    return 0;
  }
  printf("[gpu-nivel] alvo interno %dx%d RGBA -> janela %dx%d\n", intW, intH, telaW, telaH);
  fflush(stdout);
  return 1;
}

void gpun_quadro_inicio(void) {
  intLigado = 0;
  if ((nivel < 3 && !alvo1080) || !intPreparar()) return;
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
    gfx_rect(tela, intTex, GFX_COPIA, 0, 0.0f, 1.0f, 0.0f, 1, 1, 1, 1);
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
#endif

static void decidir(const char *porque) {
  decidido = 1;
  printf("[gpu-nivel] decidido: fica no nivel %d (%s)\n", nivel, porque);
  fflush(stdout);
  gravar();
}

void gpun_medir(double dtms, double espera, double cpu, int naHome, int cheia) {
  double fps, e, c;
  if (!adaptativo || decidido || nivel > GPUN_NIVEL_AUTO_MAX) return;
  // GPU fraca pode nao terminar nem a referencia no 0. Tres descartes na
  // tentativa, mesmo intercalados, ativam protecao so nesta sessao: sem
  // janela comparavel nao ha avaliacao para gravar.
  if (naHome && cheia && ptv_gpu_fraca_atual() &&
      espera >= GPUN_ESPERA_MIN && espera > cpu) {
    if (dtms > 1000.0 && ++descartes >= 3) {
      aplicar(GPUN_NIVEL_AUTO_MAX, "GPU fraca: referencia nao termina");
      decidido = 1;
      return;
    }
  } else descartes = 0;
  if (!naHome || !cheia || dtms > 1000.0) {
    // Outra cena invalida tambem a referencia do candidato. Volta ao ultimo
    // nivel confirmado, sem gravar/bloquear; ao voltar mede a referencia nova.
    if (anterior >= 0) {
      fpsNivel[nivel] = fpsNivel[anterior] = 0;
      aplicar(anterior, "comparacao interrompida");
      anterior = -1;
    }
    if (estavaNaHome) { janN = 0; janMs = janEsp = janCpu = 0; if (aquece < GPUN_ASSENTA_MS) aquece = GPUN_ASSENTA_MS; }
    estavaNaHome = 0;
    return;
  }
  estavaNaHome = 1;
  if (aquece > 0) { aquece -= dtms; return; }
  janN++; janMs += dtms; janEsp += espera; janCpu += cpu; totalMs += dtms;
  if (janMs < GPUN_JANELA_MS) return;
  descartes = 0; // janela completa encerra a tentativa
  fps = janN * 1000.0 / janMs;
  e = janEsp / janN;
  c = janCpu / janN;
  printf("[gpu-nivel] janela na home: nivel=%d fps=%.1f espera=%.1fms cpu=%.1fms (%d quadros)\n",
         nivel, fps, e, c, janN);
  janN = 0; janMs = janEsp = janCpu = 0;
  fpsNivel[nivel] = fps;
  if (anterior >= 0) {
    double antes = fpsNivel[anterior];
    reavaliar = 0; // legado so deixa de estar pendente com a comparacao completa
    if (fps < antes * 1.15 || fps - antes < 5.0) {
      printf("[gpu-nivel] nivel %d nao ajudou (%.1f -> %.1f fps): volta ao %d\n",
             nivel, antes, fps, anterior);
      aplicar(anterior, "sem ganho significativo");
      bloqueado = 1;
      decidir("nao desce de novo nesta TV");
      return;
    }
    anterior = -1;
  }
  // Um nivel legado nao tinha prova do ganho. Compara-o diretamente com o
  // 0 na mesma sessao, inclusive se o 0 ja der 45 fps (migracao uma vez).
  if (reavaliar) {
    anterior = 0;
    aplicar(reavaliar, "compara nivel salvo com efeitos completos");
    aquece = GPUN_ASSENTA_MS;
    return;
  }
  if (fps >= GPUN_FPS_BOM) { decidir("fps bom"); return; }
  if (e >= GPUN_ESPERA_MIN && e > c) {
    // O adaptativo PARA no nivel 1 quando ele ja resolve. Teste nas duas
    // Tizen 5.0 (#180, 29/09): "efeitos" ficou liso e bonito; o 720p ficou
    // mais liso mas com o texto borrado demais — ninguem gostou, e por isso o
    // 720p (nivel 3) so existe forcado. Quando o 1 AINDA fica abaixo de
    // GPUN_FPS_CRITICO (a Mali-400 do registro 9859, UA40N5300, Tizen 4.0,
    // 10-21 fps com efeitos leves), desce ao 2: efeitos MINIMOS, em 1080p.
    if (nivel == 0 || (nivel == 1 && fps < GPUN_FPS_CRITICO)) {
      if (totalMs >= GPUN_TETO_MS) { decidir("teto de tempo de medida"); return; }
      anterior = nivel;
      aplicar(nivel + 1, nivel == 0 ? "GPU presa: efeitos leves"
                                    : "GPU presa mesmo com efeitos leves: efeitos minimos");
      // So grava depois de medir o candidato: interrupcao nao o confirma.
      aquece = GPUN_ASSENTA_MS;
      return;
    }
    decidir("GPU presa, mas ja no ultimo nivel");
    return;
  }
  decidir("lento pela CPU ou pelo resto, nao pela GPU: menos pixel nao ajuda");
}
