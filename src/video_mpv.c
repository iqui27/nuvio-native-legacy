// Desktop release backend. All mpv and GL calls stay on the SDL render thread.
#ifdef NV_DESKTOP_MPV
#include "video.h"
#include "mediaurl.h"
#include "layout.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <mpv/client.h>
#include <mpv/render_gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static mpv_handle *player;
static mpv_render_context *renderer;
static GLuint texture, framebuffer;
static int texW, texH, ready, active, failed, ended, played, paused, awaitingStart;
static int width, height, nAudio, nSub, audio = -1, sub = -1, speed = 100, volume = 100;
static int dx, dy, dw = 1920, dh = 1080, sx, sy, sw, sh;
static double position, duration, bufferEnd, pendingSeek = -1;
static Uint64 sampleAt, bufferingAt;
static char currentUrl[NV_MEDIA_URL_MAX], headers[8192], errorText[256];
static VideoFaixa audios[NV_FAIXA_MAX], subs[NV_FAIXA_MAX];
static VideoLegendaEstilo subtitleStyle;
static int haveSubtitleStyle;
static PFNGLGENFRAMEBUFFERSPROC genFramebuffer;
static PFNGLBINDFRAMEBUFFERPROC bindFramebuffer;
static PFNGLFRAMEBUFFERTEXTURE2DPROC attachTexture;
static PFNGLCHECKFRAMEBUFFERSTATUSPROC checkFramebuffer;
static PFNGLDELETEFRAMEBUFFERSPROC deleteFramebuffer;
static PFNGLUSEPROGRAMPROC useProgram;
static PFNGLACTIVETEXTUREPROC activeTexture;
static PFNGLGETVERTEXATTRIBIVPROC getAttribute;
static PFNGLGETVERTEXATTRIBPOINTERVPROC getAttributePointer;
static PFNGLVERTEXATTRIBPOINTERPROC attributePointer;
static PFNGLENABLEVERTEXATTRIBARRAYPROC enableAttribute;
static PFNGLDISABLEVERTEXATTRIBARRAYPROC disableAttribute;
static PFNGLBINDBUFFERPROC bindBuffer;
typedef struct { GLint enabled, size, type, normalized, stride, buffer; void *pointer; } Attribute;
static Attribute attributes[16];
static GLint arrayBuffer, elementBuffer;
static void saveAttributes(void) {
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&arrayBuffer);
  glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING,&elementBuffer);
  for (int i=0; i<16; ++i) {
    Attribute *a=&attributes[i];
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_ENABLED,&a->enabled);
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_SIZE,&a->size);
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_TYPE,&a->type);
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_NORMALIZED,&a->normalized);
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_STRIDE,&a->stride);
    getAttribute(i,GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING,&a->buffer);
    getAttributePointer(i,GL_VERTEX_ATTRIB_ARRAY_POINTER,&a->pointer);
  }
}
static void restoreAttributes(void) {
  for (int i=0; i<16; ++i) {
    Attribute *a=&attributes[i];
    bindBuffer(GL_ARRAY_BUFFER,a->buffer);
    attributePointer(i,a->size,a->type,a->normalized,a->stride,a->pointer);
    if (a->enabled) enableAttribute(i); else disableAttribute(i);
  }
  bindBuffer(GL_ARRAY_BUFFER,arrayBuffer); bindBuffer(GL_ELEMENT_ARRAY_BUFFER,elementBuffer);
}

const char *const VIDEO_LEG_CORES[VIDEO_LEG_NCORES] = {"white", "yellow", "green", "blue", "red", "black"};
const char *const VIDEO_LEG_CORES_PT[VIDEO_LEG_NCORES] = {"Branco", "Amarelo", "Verde", "Azul", "Vermelho", "Preto"};

static int check(int rc) {
  if (rc < 0) {
    failed = 1;
    snprintf(errorText, sizeof errorText, "mpv: %s", mpv_error_string(rc));
  }
  return rc >= 0;
}
static const char *properties[] = {"track-list", "time-pos", "duration", "demuxer-cache-duration",
  "video-params", "pause", "paused-for-cache"};
static mpv_node cached[7];
static const mpv_node *property(const char *path) {
  char copy[128]; snprintf(copy, sizeof copy, "%s", path);
  char *save = NULL, *key = strtok_r(copy, "/", &save);
  const mpv_node *node = NULL;
  for (int i=0; i<7; ++i) if (key && !strcmp(properties[i], key) && cached[i].format==MPV_FORMAT_NODE_MAP)
    for (int j=0; j<cached[i].u.list->num; ++j)
      if (!strcmp(cached[i].u.list->keys[j],"data")) node=&cached[i].u.list->values[j];
  while (node && (key=strtok_r(NULL, "/", &save))) {
    if (node->format == MPV_FORMAT_NODE_ARRAY) {
      if (!strcmp(key,"count")) return NULL;
      int i=atoi(key);
      node = i>=0 && i<node->u.list->num ? &node->u.list->values[i] : NULL;
    } else if (node->format == MPV_FORMAT_NODE_MAP) {
      const mpv_node *next=NULL;
      for (int i=0; i<node->u.list->num; ++i)
        if (!strcmp(node->u.list->keys[i],key)) next=&node->u.list->values[i];
      node=next;
    } else return NULL;
  }
  return node;
}
static double number(const char *key, double fallback) {
  const mpv_node *n=property(key);
  if (!strcmp(key,"track-list/count")) {
    n=property("track-list"); return n && n->format==MPV_FORMAT_NODE_ARRAY ? n->u.list->num : 0;
  }
  if (!n) return fallback;
  if (n->format==MPV_FORMAT_DOUBLE) return n->u.double_;
  if (n->format==MPV_FORMAT_INT64) return (double)n->u.int64;
  return fallback;
}
static int flag(const char *key) {
  const mpv_node *n=property(key);
  return n && n->format==MPV_FORMAT_FLAG && n->u.flag;
}
static void set(const char *key, const char *value) {
  if (player) mpv_set_property_async(player, 0, key, MPV_FORMAT_STRING, &value);
}
static void setNumber(const char *key, double value) {
  if (player) mpv_set_property_async(player, 0, key, MPV_FORMAT_DOUBLE, &value);
}
static void trackText(int index, const char *key, char *dst, size_t size) {
  char path[96]; snprintf(path, sizeof path, "track-list/%d/%s", index, key);
  const mpv_node *n=property(path);
  snprintf(dst,size,"%s",n && n->format==MPV_FORMAT_STRING ? n->u.string : "");
}
static void tracks(void) {
  nAudio = nSub = 0; audio = sub = -1;
  int count = (int)number("track-list/count", 0);
  for (int i = 0; i < count; ++i) {
    char type[16], path[96];
    trackText(i, "type", type, sizeof type);
    int isAudio = !strcmp(type, "audio");
    if (!isAudio && strcmp(type, "sub")) continue;
    int *n = isAudio ? &nAudio : &nSub;
    if (*n == NV_FAIXA_MAX) continue;
    VideoFaixa *t = &(isAudio ? audios : subs)[*n];
    memset(t, 0, sizeof *t);
    t->ordinalMkv = t->stream_index = t->stream_id = -1;
    trackText(i, "lang", t->idioma, sizeof t->idioma);
    trackText(i, "title", t->nome, sizeof t->nome);
    trackText(i, "codec", t->codec, sizeof t->codec);
    snprintf(path, sizeof path, "track-list/%d/id", i);
    t->numero = (int)number(path, 0);
    snprintf(path, sizeof path, "track-list/%d/demux-channel-count", i);
    t->canais = (int)number(path, 0);
    snprintf(path, sizeof path, "track-list/%d/forced", i);
    t->letreiro = flag(path);
    snprintf(path, sizeof path, "track-list/%d/selected", i);
    if (flag(path)) *(isAudio ? &audio : &sub) = *n;
    snprintf(t->rotulo, sizeof t->rotulo, "%.25s%s%.16s",
             t->nome[0] ? t->nome : t->idioma, t->codec[0] ? " · " : "", t->codec);
    ++*n;
  }
}
static void *getProc(void *ctx, const char *name) {
  (void)ctx;
  return SDL_GL_GetProcAddress(name);
}
int video_iniciar(void) {
  if (player) return 1;
  if (!SDL_GL_GetCurrentContext()) return check(MPV_ERROR_UNINITIALIZED);
#define GL_PROC(variable, type, name) variable = (type)SDL_GL_GetProcAddress(name); if (!variable) return check(MPV_ERROR_UNSUPPORTED)
  GL_PROC(genFramebuffer, PFNGLGENFRAMEBUFFERSPROC, "glGenFramebuffers");
  GL_PROC(bindFramebuffer, PFNGLBINDFRAMEBUFFERPROC, "glBindFramebuffer");
  GL_PROC(attachTexture, PFNGLFRAMEBUFFERTEXTURE2DPROC, "glFramebufferTexture2D");
  GL_PROC(checkFramebuffer, PFNGLCHECKFRAMEBUFFERSTATUSPROC, "glCheckFramebufferStatus");
  GL_PROC(deleteFramebuffer, PFNGLDELETEFRAMEBUFFERSPROC, "glDeleteFramebuffers");
  GL_PROC(useProgram, PFNGLUSEPROGRAMPROC, "glUseProgram");
  GL_PROC(activeTexture, PFNGLACTIVETEXTUREPROC, "glActiveTexture");
  GL_PROC(getAttribute, PFNGLGETVERTEXATTRIBIVPROC, "glGetVertexAttribiv");
  GL_PROC(getAttributePointer, PFNGLGETVERTEXATTRIBPOINTERVPROC, "glGetVertexAttribPointerv");
  GL_PROC(attributePointer, PFNGLVERTEXATTRIBPOINTERPROC, "glVertexAttribPointer");
  GL_PROC(enableAttribute, PFNGLENABLEVERTEXATTRIBARRAYPROC, "glEnableVertexAttribArray");
  GL_PROC(disableAttribute, PFNGLDISABLEVERTEXATTRIBARRAYPROC, "glDisableVertexAttribArray");
  GL_PROC(bindBuffer, PFNGLBINDBUFFERPROC, "glBindBuffer");
#undef GL_PROC
  player = mpv_create();
  if (!player) return check(MPV_ERROR_NOMEM);
  mpv_set_option_string(player, "config", "no");
  mpv_set_option_string(player, "vo", "libmpv");
  mpv_set_option_string(player, "hwdec", "auto-safe");
  mpv_set_option_string(player, "idle", "yes");
  mpv_set_option_string(player, "terminal", "no");
  mpv_set_option_string(player, "input-default-bindings", "no");
  mpv_set_option_string(player, "sub-auto", "no");
  mpv_set_option_string(player, "keepaspect", "no");
  mpv_set_option_string(player, "cache", "yes");
  mpv_set_option_string(player, "demuxer-max-bytes", "64MiB");
  // Test-only audio sink; production always uses mpv's native device selection.
#ifdef NV_MPV_TEST
  mpv_set_option_string(player, "ao", "null");
#endif
  if (!check(mpv_initialize(player))) { video_encerrar(); return 0; }
  mpv_opengl_init_params gl = {.get_proc_address = getProc};
  mpv_render_param params[] = {
    {MPV_RENDER_PARAM_API_TYPE, MPV_RENDER_API_TYPE_OPENGL},
    {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl}, {MPV_RENDER_PARAM_INVALID, NULL}
  };
  GLint oldProgram, oldActive, oldFbo;
  glGetIntegerv(GL_CURRENT_PROGRAM,&oldProgram); glGetIntegerv(GL_ACTIVE_TEXTURE,&oldActive);
  glGetIntegerv(GL_FRAMEBUFFER_BINDING,&oldFbo);
  saveAttributes(); glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
  int rc=mpv_render_context_create(&renderer, player, params);
  glPopClientAttrib(); glPopAttrib(); restoreAttributes();
  useProgram(oldProgram); activeTexture(oldActive); bindFramebuffer(GL_FRAMEBUFFER,oldFbo);
  if (!check(rc)) { video_encerrar(); return 0; }
  for (int i=0; i<7; ++i) mpv_observe_property(player, i+1, properties[i], MPV_FORMAT_NODE);
  return 1;
}
int video_iniciar_auto(void) { return video_iniciar(); }
int video_registro_negado(void) { return 0; }
void video_parar(void) {
  if (player) {
    const char *cmd[] = {"stop", NULL};
    mpv_command_async(player, 0, cmd);
    while (mpv_wait_event(player, 0)->event_id != MPV_EVENT_NONE) {}
  }
  for (int i=0; i<7; ++i) mpv_free_node_contents(&cached[i]);
  memset(cached,0,sizeof cached);
  ready = active = failed = ended = played = paused = awaitingStart = 0;
  width = height = nAudio = nSub = 0; audio = sub = -1;
  position = duration = bufferEnd = 0; sampleAt = bufferingAt = 0;
  pendingSeek = -1; currentUrl[0] = errorText[0] = 0;
}
int video_tocar(const char *url) {
  video_parar();
  if (!url || !*url || strlen(url) >= sizeof currentUrl) return check(MPV_ERROR_INVALID_PARAMETER);
  if (!video_iniciar()) return 0;
  snprintf(currentUrl, sizeof currentUrl, "%s", url);
  // MPV_FORMAT_NODE_ARRAY preserves commas inside HTTP header values.
  char copy[sizeof headers]; snprintf(copy, sizeof copy, "%s", headers);
  mpv_node values[128]; mpv_node_list list = {0, values, NULL};
  char *save = NULL;
  for (char *line = strtok_r(copy, "\r\n", &save); line && list.num < 128; line = strtok_r(NULL, "\r\n", &save)) {
    if (!strchr(line, ':')) continue;
    values[list.num++] = (mpv_node){.u.string = line, .format = MPV_FORMAT_STRING};
  }
  mpv_node node = {.u.list = &list, .format = MPV_FORMAT_NODE_ARRAY};
  if (!check(mpv_set_property_async(player, 0, "http-header-fields", MPV_FORMAT_NODE, &node))) return 0;
  set("pause", "no"); setNumber("volume", volume); setNumber("speed", video_taxa_efetiva() / 100.0);
  const char *cmd[] = {"loadfile", url, "replace", NULL};
  awaitingStart = 1;
  active = check(mpv_command_async(player, 0, cmd));
  return active;
}
void video_bombear(void) {
  if (!player) return;
  for (;;) {
    mpv_event *event = mpv_wait_event(player, 0);
    if (event->event_id == MPV_EVENT_NONE) break;
    if (event->event_id == MPV_EVENT_START_FILE) awaitingStart = 0;
    if (!active || (awaitingStart && event->event_id != MPV_EVENT_COMMAND_REPLY)) continue;
    if (event->event_id == MPV_EVENT_FILE_LOADED) {
      ready = 1; tracks();
      if (haveSubtitleStyle) video_legenda_estilo(&subtitleStyle);
      if (pendingSeek >= 0) { double target = pendingSeek; pendingSeek = -1; video_buscar(target); }
    } else if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
      mpv_event_property *p=event->data;
      int i=(int)event->reply_userdata-1;
      if (i>=0 && i<7) {
        mpv_free_node_contents(&cached[i]); memset(&cached[i],0,sizeof cached[i]);
        if (p->format==MPV_FORMAT_NODE && p->data) {
          // Copy through mpv_event_to_node, which owns an independent snapshot.
          mpv_event_to_node(&cached[i],event);
        }
        if (!i) tracks();
      }
    } else if (event->event_id == MPV_EVENT_COMMAND_REPLY && event->error < 0) {
      check(event->error);
    }
    else if (event->event_id == MPV_EVENT_END_FILE) {
      mpv_event_end_file *end = event->data;
      if (end->reason == MPV_END_FILE_REASON_ERROR) { check(end->error); active = ready = 0; }
      else if (end->reason == MPV_END_FILE_REASON_EOF) { ended = 1; active = 0; }
    }
  }
  if (!active) return;
  double p = number("time-pos", -1);
  if (p >= 0 && (p != position || !sampleAt)) { position = p; sampleAt = SDL_GetTicks64(); }
  duration = number("duration", 0);
  bufferEnd = position + number("demuxer-cache-duration", 0);
  width = (int)number("video-params/w", 0); height = (int)number("video-params/h", 0);
  paused = flag("pause");
  if (flag("paused-for-cache")) { if (!bufferingAt) bufferingAt = SDL_GetTicks64(); }
  else bufferingAt = 0;
  if (ready && !paused && !bufferingAt && p >= 0) played = 1;
  static int lastRate;
  int rate=video_taxa_efetiva();
  if (rate!=lastRate) { setNumber("speed",rate/100.0); lastRate=rate; }
}

// Composite behind the completed, premultiplied UI using destination alpha.
// Requires desktop GL 2.1 compatibility and an RGBA drawable, on the SDL thread.
int video_desktop_render(int drawableW, int drawableH) {
  if (!renderer || !active || !ready || dw <= 0 || dh <= 0 || drawableW <= 0 || drawableH <= 0) return 0;
  GLint oldFbo, oldProgram, oldActive;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFbo);
  glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActive);
  saveAttributes();
  glPushAttrib(GL_ALL_ATTRIB_BITS); glPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
  activeTexture(GL_TEXTURE0);
  glMatrixMode(GL_PROJECTION); glPushMatrix();
  glMatrixMode(GL_MODELVIEW); glPushMatrix();
  glMatrixMode(GL_TEXTURE); glPushMatrix();
  int w = (int)ceil(dw * drawableW / (double)NV_TELA_W), h = (int)ceil(dh * drawableH / (double)NV_TELA_H);
  activeTexture(GL_TEXTURE0);
  if (!texture) glGenTextures(1, &texture);
  glBindTexture(GL_TEXTURE_2D, texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  if (w != texW || h != texH) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    texW = w; texH = h;
  }
  if (!framebuffer) genFramebuffer(1, &framebuffer);
  bindFramebuffer(GL_FRAMEBUFFER, framebuffer);
  attachTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
  int ok = checkFramebuffer(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  if (ok) {
    mpv_opengl_fbo fbo = {(int)framebuffer, w, h, GL_RGBA8};
    int flip = 1, block = 0;
    mpv_render_param params[] = {{MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
      {MPV_RENDER_PARAM_FLIP_Y, &flip}, {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &block},
      {MPV_RENDER_PARAM_INVALID, NULL}};
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    mpv_render_context_update(renderer);
    ok = check(mpv_render_context_render(renderer, params));
  }
  bindFramebuffer(GL_FRAMEBUFFER, (GLuint)oldFbo);
  useProgram(0); activeTexture(GL_TEXTURE0);
  glDisable(GL_DEPTH_TEST); glDisable(GL_CULL_FACE); glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST); glDisable(GL_ALPHA_TEST); glDisable(GL_LIGHTING);
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  glViewport(0, 0, drawableW, drawableH);
  glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0, NV_TELA_W, NV_TELA_H, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW); glLoadIdentity();
  glMatrixMode(GL_TEXTURE); glLoadIdentity();
  glEnable(GL_BLEND); glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_ONE);
  glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texture);
  glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
  float x0 = sw > 0 && width > 0 ? (float)sx / width : 0;
  float y0 = sh > 0 && height > 0 ? (float)sy / height : 0;
  float x1 = sw > 0 && width > 0 ? (float)(sx + sw) / width : 1;
  float y1 = sh > 0 && height > 0 ? (float)(sy + sh) / height : 1;
  if (ok) {
    glBegin(GL_QUADS);
    glTexCoord2f(x0, 1-y0); glVertex2i(dx, dy);
    glTexCoord2f(x1, 1-y0); glVertex2i(dx+dw, dy);
    glTexCoord2f(x1, 1-y1); glVertex2i(dx+dw, dy+dh);
    glTexCoord2f(x0, 1-y1); glVertex2i(dx, dy+dh);
    glEnd();
  }
  glMatrixMode(GL_TEXTURE); glPopMatrix();
  glMatrixMode(GL_MODELVIEW); glPopMatrix();
  glMatrixMode(GL_PROJECTION); glPopMatrix();
  glPopClientAttrib(); glPopAttrib(); restoreAttributes();
  useProgram((GLuint)oldProgram); activeTexture((GLenum)oldActive);
  return ok;
}
void video_desktop_swap(void) { if (renderer) mpv_render_context_report_swap(renderer); }
void video_encerrar(void) {
  if (renderer) mpv_render_context_free(renderer);
  renderer = NULL;
  if (player) mpv_terminate_destroy(player);
  player = NULL;
  if (framebuffer) deleteFramebuffer(1, &framebuffer);
  if (texture) glDeleteTextures(1, &texture);
  texture = framebuffer = 0; texW = texH = 0;
  ready = active = 0;
  for (int i=0; i<7; ++i) mpv_free_node_contents(&cached[i]);
  memset(cached,0,sizeof cached);
}
void video_pausar(int p) { set("pause", p ? "yes" : "no"); }
void video_volume(int pct) { volume = pct < 0 ? 0 : pct > 100 ? 100 : pct; setNumber("volume", volume); }
void video_buscar(double seconds) {
  if (!isfinite(seconds)) return;
  if (seconds < 0) seconds = 0;
  if (!ready) { pendingSeek = seconds; return; }
  char value[48]; snprintf(value, sizeof value, "%.6f", seconds);
  const char *cmd[] = {"seek", value, "absolute+exact", NULL};
  check(mpv_command_async(player, 0, cmd));
}
void video_janela(int x, int y, int w, int h) { dx=x; dy=y; dw=w; dh=h; sx=sy=sw=sh=0; }
void video_janela_fonte(int x, int y, int w, int h, int a, int b, int c, int d) {
  dx=a; dy=b; dw=c; dh=d; sx=x; sy=y; sw=w; sh=h;
}
void video_recorte_reaplicar(void) {}
void video_escala_definir(int w, int h) { (void)w; (void)h; }
int video_recorte_fonte(void) { return 1; }
const char *video_url_atual(void) { return currentUrl; }
double video_pos(void) { return position; }
double video_pos_amostra(Uint64 *ticks) { if (ticks) *ticks=sampleAt; return position; }
double video_duracao(void) { return duration; }
double video_creditos(void) { return 0; }
double video_creditos_nomeado(void) { return 0; }
double video_creditos_ultimo(double dur) { (void)dur; return 0; }
double video_buffer_fim(void) { return bufferEnd; }
unsigned video_bufferando_ms(void) { return bufferingAt ? (unsigned)(SDL_GetTicks64()-bufferingAt) : 0; }
int video_tocando(void) { return active && ready && !paused && !bufferingAt; }
int video_pausa_confirmada(void) { return active && ready && paused; }
int video_pronto(void) { return ready; }
int video_ativo(void) { return active; }
int video_falhou(void) { return failed; }
const char *video_erro_texto(void) { return errorText; }
int video_decoder_anunciou(void) { return width > 0 && height > 0; }
int video_iniciando(void) { return active && !ready; }
int video_fonte_tocou(void) { return played; }
int video_terminou(void) { return ended; }
int video_n_audio(void) { return nAudio; }
int video_n_legenda(void) { return nSub; }
const VideoFaixa *video_audio(int i) { return i >= 0 && i < nAudio ? &audios[i] : NULL; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < nSub ? &subs[i] : NULL; }
int video_audio_atual(void) { return audio; }
int video_legenda_atual(void) { return sub; }
static void selectTrack(const char *key, int id) {
  char value[24]; snprintf(value, sizeof value, "%d", id); set(key, value); tracks();
}
void video_escolher_audio(int i) { if (video_audio(i)) selectTrack("aid", audios[i].numero); }
void video_escolher_legenda(int i) {
  if (i < 0) { set("sid", "no"); tracks(); }
  else if (video_legenda(i)) selectTrack("sid", subs[i].numero);
}
int video_legenda_nativa(char *dst, int size) { if (dst && size > 0) *dst=0; return 0; }
void video_legenda_externa(const char *url) {
  if (!player || !url || !*url) return;
  const char *cmd[] = {"sub-add", url, "select", NULL}; check(mpv_command_async(player, 0, cmd));
}
void video_normalizar_url_legenda(const char *url, char *dst, unsigned size) {
  if (dst && size) snprintf(dst, size, "%s", url ? url : "");
}
void video_legenda_estilo(const VideoLegendaEstilo *e) {
  if (!e) return;
  subtitleStyle=*e; haveSubtitleStyle=1;
  setNumber("sub-font-size", 32.0 * (e->tamanho >= 50 && e->tamanho <= 200 ? e->tamanho : 120) / 120.0);
  setNumber("sub-delay", e->atrasoMs / 1000.0);
  set("sub-color", VIDEO_LEG_CORES[e->cor >= 0 && e->cor < VIDEO_LEG_NCORES ? e->cor : 0]);
  setNumber("sub-pos", 100 - e->posicao * 3);
  setNumber("sub-border-size", e->borda == 1 ? 2 : 0);
  setNumber("sub-shadow-offset", e->borda == 2 ? 2 : 0);
  set("sub-bold", e->negrito ? "yes" : "no");
  char color[24];
  unsigned rgb=video_legenda_borda_rgb(e->corBorda);
  snprintf(color,sizeof color,"#%06X",rgb); set("sub-border-color",color);
  int opacity=leg_opacidade_byte(e->opacidade);
  static const unsigned colors[]={0xffffff,0xffff00,0x00ff00,0x0000ff,0xff0000,0};
  rgb=colors[e->cor>=0 && e->cor<6 ? e->cor : 0];
  snprintf(color,sizeof color,"#%06X%02X",rgb,opacity); set("sub-color",color);
  int background=e->fundo>=1 && e->fundo<=4 ? 255*e->fundo/4 : 0;
  snprintf(color,sizeof color,"#000000%02X",background); set("sub-back-color",color);
}
void video_definir_cabecalhos(const char *value) { snprintf(headers, sizeof headers, "%s", value ? value : ""); }
const char *video_cabecalhos(void) { return headers; }
int video_recarregar(void) {
  if (!active) return 0;
  char url[sizeof currentUrl]; snprintf(url, sizeof url, "%s", currentUrl);
  double pos=position; int pause=paused;
  if (!video_tocar(url)) return 0;
  video_buscar(pos); video_pausar(pause); return 1;
}
void video_velocidade(int value) { speed = value > 0 ? value : 100; setNumber("speed", video_taxa_efetiva()/100.0); }
int video_velocidade_atual(void) { return speed; }
int video_velocidade_suportada(void) { return 1; }
int video_velocidade_bloqueada(void) { return 0; }
void video_velocidade_recusada(void) { video_velocidade(100); }
int video_largura(void) { return width; }
int video_altura(void) { return height; }
void video_estatisticas(VideoStats *s) { video_stats_vazio(s); if (active) s->bufferSeg=bufferEnd-position; }
void video_informacoes(VideoInfo *s) { memset(s, 0, sizeof *s); }
int video_faixa_dts(const VideoFaixa *f) { return f && (!strcmp(f->codec,"dts") || !strcmp(f->codec,"dca")); }
int video_dts_estado(void) { return video_faixa_dts(video_audio(audio)) ? VIDEO_DTS_NATIVO : VIDEO_DTS_NENHUM; }
const char *video_dts_saida(void) { return ""; }
int video_dts_legenda_desenhar(double t,int d,float x,float y,float w,float h,float a) {
  (void)t;(void)d;(void)x;(void)y;(void)w;(void)h;(void)a; return 0;
}
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
int video_mkv_sondado(void) { return 2; }
void video_sondar_mkv_agora(void) {}
int video_audio_nao_suportado(void) { return 0; }
int video_seek_desistiu(void) { return 0; }
int video_conflito_recurso(void) { return 0; }
int video_reconectando(void) { return 0; }
void video_definir_reconexao(int v) { (void)v; }
void video_definir_mp4(int v) { (void)v; }
void video_definir_dv(int v) { (void)v; }
int video_tem_atmos(void) { return 0; }
int video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void) { return "none"; } // SDR output; mpv tone maps HDR sources.
int video_pode_forcar_sdr(void) { return 0; }
void video_forcar_sdr(void) {}
void video_dv_guard_reiniciar(void) {}
int video_dv_ativo(void) { return 0; }
int video_dv_recuo_consumir(void) { return 0; }
void video_dv_fase(VideoDvFase *f) { if (f) memset(f,0,sizeof *f); }
int video_dv_candidato(const char *url) { (void)url; return 0; }
void video_dv_tela(int v) { (void)v; }
void video_dv_recusar(void) {}
void video_dv_segurar(int v) { (void)v; }
int video_luna(const char *u,const char *p,void (*cb)(const char *,void *),void *ctx) {
  (void)u;(void)p;(void)cb;(void)ctx; return 0;
}
#endif
