#include "playinfo.h"
#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#ifndef PLAYINFO_MODEL_ONLY
#define NV_ESCALA_TELA
#include "ajustes.h"
#include "addons.h"
#include "anim.h"
#include "escala.h"
#include "faixas.h"
#include "idioma.h"
#include "player.h"
#include "plrui.h"
#include "ponteiro.h"
#include "teclavoltar.h"
#else
#define i18n(s) (s)
#endif

// No URL is accepted as display metadata. File paths retain only the basename.
static void safe(char *dst, size_t n, const char *s) {
  if (!s || !*s || strstr(s, "://") || strstr(s, "magnet:") ||
      strpbrk(s, "?&=@") || strstr(s, "token=") || strstr(s, "password=")) return;
  snprintf(dst, n, "%s", s);
  for (char *p = dst; *p; p++) if ((unsigned char)*p < 32) *p = ' ';
}

void playinfo_model(PlayInfo *m, const Stream *s, const VideoStats *stats,
                    const VideoInfo *info, int width, int height, const char *hdr,
                    const VideoFaixa *audio, const VideoFaixa *subtitle,
                    int externalSubtitle, const P2pmEstado *p2p, int declaredKbps) {
  int server = s && (!strncmp(s->bingeGroup, "jellyfin|", 9) ||
      !strncmp(s->bingeGroup, "emby|", 5) || !strncmp(s->bingeGroup, "plex|", 5));
  const char *method = server ? strchr(s->bingeGroup, '|') + 1 : "";
  int original = !server || !strcmp(method, "DirectPlay");
  memset(m, 0, sizeof *m);
  for (int i = 0; i < PI_N; i++) snprintf(m->value[i], sizeof m->value[i], "—");
#define V(i) m->value[i], sizeof m->value[i]
  if (!server) snprintf(m->value[PI_MODE], sizeof m->value[PI_MODE], "Direct");
  else if (!strcmp(method, "DirectPlay")) snprintf(m->value[PI_MODE], sizeof m->value[PI_MODE], "Direct Play");
  else if (!strcmp(method, "DirectStream")) snprintf(m->value[PI_MODE], sizeof m->value[PI_MODE], "Direct Stream");
  else if (!strcmp(method, "Transcode")) snprintf(m->value[PI_MODE], sizeof m->value[PI_MODE], "Transcoding");
  if (s) {
    const char *file = s->arquivo;
    safe(V(PI_SOURCE), s->provedor);
    // Reject URLs before stripping their path or query.
    if (!strstr(file, "://") && !strstr(file, "magnet:")) {
      const char *p = file;
      for (const char *q = file; *q; q++) if (*q == '/' || *q == '\\') p = q + 1;
      safe(V(PI_FILE), p);
    }
    if (original) {
      safe(V(PI_CONTAINER), s->container);
      const char *ext = strrchr(m->value[PI_FILE], '.');
      if (ext && (!strcasecmp(ext, ".mkv") || !strcasecmp(ext, ".mp4") ||
                  !strcasecmp(ext, ".webm") || !strcasecmp(ext, ".ts") || !strcasecmp(ext, ".m4v"))) {
        snprintf(m->value[PI_CONTAINER], sizeof m->value[PI_CONTAINER], "%s", ext + 1);
        for (char *p = m->value[PI_CONTAINER]; *p; p++) *p = (char)toupper((unsigned char)*p);
      }
    }
  }
  if (info) {
    safe(V(PI_CODEC), info->codec); safe(V(PI_CONTAINER), info->container);
    if (isfinite(info->fps) && info->fps > 0) snprintf(m->value[PI_FPS], sizeof m->value[PI_FPS], "%.3f fps", info->fps);
  }
  if (width > 0 && height > 0) snprintf(m->value[PI_RESOLUTION], sizeof m->value[PI_RESOLUTION], "%d × %d", width, height);
  if (hdr && *hdr && strcmp(hdr, "desconhecido") && strcmp(hdr, "unknown"))
    safe(V(PI_HDR), !strcmp(hdr, "none") ? "SDR" : !strcmp(hdr, "DolbyVision") ? "Dolby Vision" : hdr);
  if (info && info->kbps > 0) snprintf(m->value[PI_BITRATE], sizeof m->value[PI_BITRATE], "%.2f Mbps · %s", info->kbps / 1000.0, i18n("Medido"));
  else if (original && declaredKbps > 0)
    snprintf(m->value[PI_BITRATE], sizeof m->value[PI_BITRATE], "%.2f Mbps · %s", declaredKbps / 1000.0, i18n("Do add-on"));
  if (audio) {
    char codec[64] = "—", lang[16] = "—", channels[24] = "—";
    safe(codec, sizeof codec, audio->codec); safe(lang, sizeof lang, audio->idioma);
    if (audio->canais > 0) snprintf(channels, sizeof channels, "%d ch", audio->canais);
    snprintf(m->value[PI_AUDIO], sizeof m->value[PI_AUDIO], "%s · %s · %s", codec, channels, lang);
  }
  if (externalSubtitle) snprintf(m->value[PI_SUBTITLE], sizeof m->value[PI_SUBTITLE], "%s", i18n("Legenda externa"));
  else if (subtitle) safe(V(PI_SUBTITLE), subtitle->rotulo);
  if (stats) {
    if (isfinite(stats->bufferSeg) && stats->bufferSeg >= 0) snprintf(m->value[PI_BUFFER], sizeof m->value[PI_BUFFER], "%.1f s", stats->bufferSeg);
    else if (stats->bufferPct >= 0 && stats->bufferPct <= 100) snprintf(m->value[PI_BUFFER], sizeof m->value[PI_BUFFER], "%d%%", stats->bufferPct);
    if (stats->quadrosPerdidos >= 0) snprintf(m->value[PI_DROPPED], sizeof m->value[PI_DROPPED], "%lld", stats->quadrosPerdidos);
  }
  m->p2p = (s && s->urlP2P) || (p2p && p2p->ativo);
  if (m->p2p && p2p && p2p->ativo) {
    snprintf(m->value[PI_PEERS], sizeof m->value[PI_PEERS], "%u", p2p->pares);
    snprintf(m->value[PI_DOWNLOAD], sizeof m->value[PI_DOWNLOAD], "%.2f MB/s", p2p->baixando_bps / 1000000.0);
  }
#undef V
}

#ifndef PLAYINFO_MODEL_ONLY
static int opened, sampled;
static Uint32 lastSample, lastFrame;
static float alpha;
static PlayInfo model;
#ifdef NV_SHOT_HOOKS
const PlayInfo *playinfo_sample(void) { return &model; }
float playinfo_alpha(void) { return alpha; }
#endif
void playinfo_open(void) { opened = 1; sampled = 0; }
void playinfo_close(void) { opened = 0; }
int playinfo_event(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN) return 0;
  if (e->key.keysym.sym == SDLK_i && player_aberto()) {
    if (!e->key.repeat) { if (opened) playinfo_close(); else playinfo_open(); }
    return 1;
  }
  if (opened && nv_tecla_voltar(e)) { playinfo_close(); return 1; }
  return 0;
}
static void closeClick(int x, int y) { (void)x; (void)y; playinfo_close(); }
void playinfo_draw(Uint32 now) {
  if (!player_aberto()) opened = 0;
  if (opened && (!sampled || now - lastSample >= 1000u)) {
    VideoStats stats; VideoInfo info; P2pmEstado p;
    video_estatisticas(&stats); video_informacoes(&info); p2pmotor_estado(&p);
    playinfo_model(&model, stream_item(stream_atual()), &stats, &info,
        video_largura(), video_altura(), video_pronto() ? video_hdr() : "", video_audio(video_audio_atual()),
        video_legenda(faixas_legenda_ativa()), faixas_legenda_externa_id()[0] != 0, &p, stream_kbps_atual());
    if (faixas_legenda_externa_id()[0]) {
      const Legenda *sub = addons_legenda(faixas_legenda_ativa() - video_n_legenda());
      if (sub) safe(model.value[PI_SUBTITLE], sizeof model.value[PI_SUBTITLE], sub->rotulo);
    }
    if (video_pronto() && faixas_legenda_ativa() < 0 && !faixas_legenda_externa_id()[0])
      snprintf(model.value[PI_SUBTITLE], sizeof model.value[PI_SUBTITLE], "%s", i18n("Desligada"));
    lastSample = now; sampled = 1;
  }
  float dt = lastFrame ? fminf((now - lastFrame) / 1000.0f, .05f) : 1.0f / 60;
  lastFrame = now;
  alpha = anim_mola(alpha, opened ? 1 : 0, dt, opened ? 11 : 14);
  if (alpha < .002f) return;
  ESCALA_INI();
  float w = fminf(560, NV_TELA_W - 48), row = 32, pad = 24;
  TxtEstilo body = TXT_ILHA_SUB, heading = TXT_ILHA_NOME;
#ifdef NV_TOUCH_UI
  // Native mobile uses a 1080-unit short edge: keep the card readable on phones.
  if (layout_modo_mobile()) {
    w = fminf(900, NV_TELA_W - 48); row = 54; pad = 30;
    body = TXT_V2_ROT; heading = TXT_V2_36B;
  }
#endif
  int n = model.p2p ? PI_N : PI_PEERS;
  GfxRect r = {NV_TELA_W - nv_inset_d / gfx_escala_ui() - w - 24,
               24 + nv_inset_t / gfx_escala_ui() + (1 - alpha) * -12,
               w, pad * 2 + 52 + n * row};
  int rim = gfx_vidro_sem_aro; gfx_vidro_sem_aro = 1;
  plrui_material(r, 28, 0, alpha);
  gfx_vidro_sem_aro = rim;
  if (opened) ponteiro_alvo(r.x, r.y, r.w, r.h, NULL, NULL, 0, 0);
  TxtLinha title = txt_linha_corta(heading, "Informações de reprodução", 243, 242, 239, 255, w - pad * 2 - 42);
  txt_desenhar_alpha(title, r.x + pad, r.y + pad, alpha);
  GfxRect close = {r.x + w - pad - 32, r.y + pad, 32, 32};
  gfx_icone(close, "aj_x", .95f, .95f, .94f, alpha);
  if (opened) ponteiro_alvo(close.x - 12, close.y - 12, 56, 56, NULL, closeClick, 0, 0);
  static const char *const labels[PI_N] = {"Modo", "Fonte", "Arquivo", "Contêiner", "Codec de vídeo", "Resolução",
      "Taxa de quadros", "HDR", "Taxa de vídeo", "Áudio", "Legenda", "Buffer à frente", "Quadros perdidos", "Pares P2P", "Baixando"};
  float labelW = w * .42f;
  for (int i = 0; i < n; i++) {
    float y = r.y + pad + 52 + i * row;
    TxtLinha l = txt_linha_corta(body, labels[i], 243, 242, 239, 150, labelW - 8);
    TxtLinha v = txt_linha_corta(body, model.value[i], 243, 242, 239, 255, w - 2 * pad - labelW);
    txt_desenhar_alpha(l, r.x + pad, y, alpha);
    txt_desenhar_alpha(v, r.x + pad + labelW, y, alpha);
  }
  ESCALA_FIM();
}
#endif
