// Ver estatvivoui.h. Geometria do mockup stats-velocidade (telas 2 e 3), em
// pixels de layout 1920x1080.
#include "estatvivoui.h"
#include "ajustes.h"
#include "audioinfo.h"
#include "badges.h"
#include "linguas.h"
#include "catalogo.h"
#include "gfx.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "perfiltv.h"
#include "player.h"
#include "ponteiro.h"
#include "streams.h"
#include "text.h"
#include "video.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define EVU_X 800.0f
#define EVU_Y 76.0f
#define EVU_W 1036.0f
#define EVU_H 928.0f
#define EVU_TILE_W 472.0f
#define EVU_GAP 18.0f
#define EVU_TX 243, 242, 239, 255

typedef struct {
  char titulo[160], sub[160], addon[64], resolucao[32], codecHdr[64], taxa[24], audio[96], legenda[64];
  int plataforma;
} EvTela;

static int aberto, fixado, temSessao;
static Uint32 abriuEm, proxAmostra;
static unsigned long hashUrl;
static EvEstado est;
static EvTela tela;
#ifdef ESTATVIVO_TESTE
static int teste;
#endif

static char sepDec(void) { return idioma_ponto_decimal(ajustes_idioma()) ? '.' : ','; }

void estatvivo_abrir(void) {
  if (!aberto) abriuEm = SDL_GetTicks();
  aberto = 1;
  fixado = 0;
  printf("[estatisticas] aberto\n");
  fflush(stdout);
}
void estatvivo_fechar(void) { aberto = 0; fixado = 0; }
int  estatvivo_aberto(void) { return aberto; }
int  estatvivo_fixado(void) { return fixado; }
int  estatvivo_kbps(int *estimada) {
  if (estimada) *estimada = est.velOrigem != EVO_MEDIDO;
  return temSessao ? est.kbps : -1;
}

int estatvivo_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !e || e->type != SDL_KEYDOWN) return aberto && e && e->type == SDL_KEYUP;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE) { estatvivo_fechar(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER) { aberto = 0; fixado = 1; return 1; }
  return 1;   // modal: as setas nao vazam para o player por baixo
}

// --- A AMOSTRA -----------------------------------------------------------------
static unsigned long fnv(const char *s) {
  unsigned long h = 2166136261UL;
  for (; s && *s; s++) { h ^= (unsigned char)*s; h *= 16777619UL; }
  return h;
}

static const char *codecDe(uint64_t b) {
  if (b & badges_bit("co-x265")) return "HEVC";
  if (b & badges_bit("co-av1")) return "AV1";
  if (b & badges_bit("co-x264")) return "H.264";
  return "";
}

// As linhas do cartao: o que o add-on declara da fonte e o que o pipeline diz.
static void montarTela(int taxaKbps) {
  const CatItem *c = cat_item(player_indice());
  const Stream *s = stream_item(stream_atual());
  const char *hdr = video_hdr(), *ep = player_linha_episodio();
  int w = video_largura(), h = video_altura(), samsung;
  size_t u;
  memset(&tela, 0, sizeof tela);
  tela.plataforma = (int)ptv_plataforma();
  samsung = tela.plataforma == PTV_TIZEN || tela.plataforma == PTV_TPK;
  if (c) snprintf(tela.titulo, sizeof tela.titulo, "%s", c->titulo);
  u = 0;
  if (ep && ep[0]) u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s", ep);
  else if (c && c->meta[0]) u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s", c->meta);
  if (h > 0 && u < sizeof tela.sub)
    u += (size_t)snprintf(tela.sub + u, sizeof tela.sub - u, "%s%s", u ? " · " : "",
                          h >= 2000 || w >= 3200 ? "4K" : h >= 1000 ? "1080p" : h >= 700 ? "720p" : "SD");
  if (s) snprintf(tela.addon, sizeof tela.addon, "%s", s->provedor);
  if (w > 0 && h > 0) snprintf(tela.resolucao, sizeof tela.resolucao, "%d × %d", w, h);
  else if (s && s->altura > 0) snprintf(tela.resolucao, sizeof tela.resolucao, "%dp", s->altura);
  { const char *cod = s ? codecDe(s->badges) : "";
    char hd[24] = "";
    int doAddon = 0;
    if (!samsung && hdr && hdr[0] && strcmp(hdr, "none") && strcmp(hdr, "SDR"))
      snprintf(hd, sizeof hd, "%s", !strcmp(hdr, "DolbyVision") ? "Dolby Vision" : hdr);
    else if (s) {
      int m = badges_fonte_hdr_marca(s->badges);
      if (s->dolbyVision && !samsung) snprintf(hd, sizeof hd, "Dolby Vision");
      else if (m >= 0) snprintf(hd, sizeof hd, "%s", marca_formato_nome((FormatoMarca)m));
      doAddon = hd[0] != 0;
    }
    if (!hd[0]) snprintf(hd, sizeof hd, "SDR");
    snprintf(tela.codecHdr, sizeof tela.codecHdr, "%s%s%s%s", cod, cod[0] ? " · " : "", hd,
             doAddon && samsung ? " (add-on)" : "");
    if (h > 0 && u < sizeof tela.sub && strcmp(hd, "SDR"))
      snprintf(tela.sub + u, sizeof tela.sub - u, " %s", hd); }
  // #293: o audio e a legenda EM USO, do que o pipeline listou. Codec/canais so
  // quando a plataforma os da (audioinfo vazio = fica so o idioma); sem faixa, vazio.
  { const VideoFaixa *fa = video_audio(video_audio_atual());
    const VideoFaixa *fl = video_legenda(video_legenda_atual());
    if (fa) {
      char cc[40] = "";
      audioinfo_texto(fa->codec, fa->canais, strstr(fa->rotulo, "Atmos") != NULL, cc, sizeof cc);
      { const char *lg = fa->idioma[0] ? i18n(ling_nome(fa->idioma)) : "";
        snprintf(tela.audio, sizeof tela.audio, "%s%s%s", cc, cc[0] && lg[0] ? " · " : "", lg); }
    }
    if (fl) snprintf(tela.legenda, sizeof tela.legenda, "%s", fl->rotulo);
  }
  if (taxaKbps > 0) {
    char m[16];
    ev_fmt_mbps(m, sizeof m, taxaKbps, sepDec());
    snprintf(tela.taxa, sizeof tela.taxa, "%s Mbps", m);
  }
}

void estatvivo_atualizar(Uint32 agora) {
  EvEntrada in;
  const Stream *s;
  time_t t;
  struct tm *lt;
  unsigned long hu;
#ifdef ESTATVIVO_TESTE
  if (teste) return;
#endif
  if (!player_aberto() || !player_com_video()) {
    if (temSessao) { ev_zerar(&est); temSessao = 0; }
    aberto = fixado = 0;
    return;
  }
  hu = fnv(video_url_atual());
  // Outra URL = outro video (ou outra fonte): tudo recomeca. A reconexao
  // reabre a MESMA URL e segue contando.
  if (!temSessao || hu != hashUrl) { ev_zerar(&est); hashUrl = hu; temSessao = 1; proxAmostra = 0; }
  if (proxAmostra && (Sint32)(agora - proxAmostra) < 0) return;
  proxAmostra = agora + 1000u;
  memset(&in, 0, sizeof in);
  video_estatisticas(&in.vs);
  in.pos = video_pos();
  in.bufferando = video_bufferando_ms() > 0;
  in.tocando = video_tocando() && !in.bufferando;
  in.reconectando = video_reconectando() != 0;
  in.falhou = video_falhou();
  s = stream_item(stream_atual());
  in.taxaKbps = s ? ev_taxa_arquivo(s->tamanhoBytes, s->tamanhoMB, video_duracao()) : 0;
  t = time(NULL);
  lt = localtime(&t);
  ev_amostra(&est, agora, &in, lt ? lt->tm_hour : 0, lt ? lt->tm_min : 0);
  if (aberto || fixado) montarTela(in.taxaKbps);
}

// --- PECAS DO DESENHO ------------------------------------------------------------
static float caps(const char *s, float x, float y, float a) {
  char up[160];
  idioma_maiusc_em(ajustes_idioma(), up, sizeof up, i18n(s));
  return txt_tracking(TXT_V2_KICK, up, 154, 154, 156, x, y, a, 3.4f);
}

// Chip neutro: fundo branco a 8%, fio de 8%, icone e texto 20/600.
static float chip(const char *icone, const char *s, float x, float y, float h, float a, int desenha) {
  TxtLinha t = txt_linha(TXT_AJ_SEG, s, 216, 216, 216, 255);
  float w = 18.0f + (icone ? 20.0f + 10.0f : 0.0f) + (float)t.w + 18.0f;
  if (!desenha) return w;
  gfx_cor((GfxRect){ x, y, w, h }, 0.5f, 1, 1, 1, 0.08f * a);
  gfx_anel((GfxRect){ x, y, w, h }, 0.5f, 1.0f, 1, 1, 1, 0.08f * a);
  if (icone) gfx_icone((GfxRect){ x + 18.0f, y + (h - 20.0f) * 0.5f, 20, 20 }, icone, 0.85f, 0.85f, 0.85f, a);
  txt_desenhar_alpha(t, x + 18.0f + (icone ? 30.0f : 0.0f), y + (h - (float)t.h) * 0.5f, a);
  return w;
}

static const char *iconeOrigem(int o) {
  switch (o) {
    case EVO_ADDON: return "aj_tag";
    case EVO_LINK: return "aj_link";
    case EVO_SEM: return "aj_info";
    default: return "aj_activity";
  }
}

// O ladrilho: superficie, icone num quadrado, rotulo e o selo da origem.
static void ladrilho(GfxRect r, const char *icone, const char *rotulo, int origem, float a) {
  float cw;
  TxtLinha l;
  gfx_cor(r, 18.0f / r.h, 0.118f, 0.118f, 0.125f, a);
  gfx_cor((GfxRect){ r.x + 24, r.y + 24, 44, 44 }, 12.0f / 44.0f, 0.165f, 0.165f, 0.176f, a);
  gfx_icone((GfxRect){ r.x + 24 + 9, r.y + 24 + 9, 26, 26 }, icone, 0.953f, 0.949f, 0.937f, a);
  cw = chip(iconeOrigem(origem), i18n(ev_origem_rotulo((EvOrigem)origem)), 0, 0, 40, a, 0);
  chip(iconeOrigem(origem), i18n(ev_origem_rotulo((EvOrigem)origem)), r.x + r.w - 20 - cw, r.y + 26, 40, a, 1);
  l = txt_linha_corta(TXT_G22M, rotulo, 180, 180, 182, 255, r.w - 24 - 58 - cw - 30);
  txt_desenhar_alpha(l, r.x + 24 + 58, r.y + 24 + (44 - (float)l.h) * 0.5f, a);
}

// Numero grande e unidade na mesma linha de base. Devolve a largura.
static float numero(const char *n, const char *unidade, float x, float y, float a, int apagado) {
  TxtLinha t = apagado ? txt_linha(TXT_ILHA_TITULO, n, 111, 111, 114, 255) : txt_linha(TXT_AJ_NUM58, n, EVU_TX);
  float w = (float)t.w;
  txt_desenhar_alpha(t, x, y, a);
  if (unidade && unidade[0]) {
    TxtLinha u = txt_linha(TXT_V2_LN, unidade, 180, 180, 182, 255);
    txt_desenhar_alpha(u, x + w + 8, y + (float)t.h * 0.80f - (float)u.h, a);
    w += 8 + (float)u.w;
  }
  return w;
}

// Linha de apoio com trechos fortes: `p` alterna normal, forte, normal...
static void apoio(const char *const *p, int n, float x, float y, float maxW, float a) {
  int i;
  for (i = 0; i < n && maxW > 20; i++) {
    TxtLinha t = (i & 1) ? txt_linha_corta(TXT_G21B, p[i], 255, 255, 255, 255, maxW)
                         : txt_linha_corta(TXT_AJ_TEXTO, p[i], 180, 180, 182, 255, maxW);
    if (!p[i][0]) continue;
    txt_desenhar_alpha(t, x, y + ((i & 1) ? 1.0f : 0.0f), a);
    // O espaco da borda do trecho nao entra na largura medida: o vao e daqui.
    x += (float)t.w + 7.0f;
    maxW -= (float)t.w + 7.0f;
  }
}

static void segmento(float x0, float y0, float x1, float y1, float esp, float r, float g, float b, float a) {
  float dx = x1 - x0, dy = y1 - y0, halo = 2.0f;
  GfxRect q = { fminf(x0, x1) - halo, fminf(y0, y1) - halo, fabsf(dx) + 2 * halo, fabsf(dy) + 2 * halo };
  if (dx * dx + dy * dy < 1.0f) return;
  gfx_rect(q, 0, GFX_LINHA, esp / q.h, (dx * dy < 0.0f) ? 1.0f : 0.0f, 0, halo / q.h, r, g, b, a);
}

// A linha de velocidade com a area embaixo: um retangulo por amostra.
static void faisca(const int *v, int n, GfxRect r, float a) {
  int i, mx = 1;
  float passo, px = 0, py = 0;
  if (n < 2) return;
  for (i = 0; i < n; i++) if (v[i] > mx) mx = v[i];
  passo = r.w / (float)(n - 1);
  for (i = 0; i < n; i++) {
    float x = r.x + r.w - (float)(n - 1 - i) * passo;
    float y = r.y + r.h - r.h * 0.85f * (float)v[i] / (float)mx;
    if (i) {
      gfx_cor((GfxRect){ px, fminf(py, y), x - px, r.y + r.h - fminf(py, y) }, 0, 1, 1, 1, 0.07f * a);
      segmento(px, py, x, y, 2.0f, 0.95f, 0.95f, 0.94f, 0.9f * a);
    }
    px = x; py = y;
  }
}

static void barra(float x, float y, float w, float frac, float a) {
  if (frac < 0) frac = 0;
  if (frac > 1) frac = 1;
  gfx_cor((GfxRect){ x, y, w, 10 }, 0.5f, 0.18f, 0.18f, 0.192f, a);
  if (w * frac > 2) gfx_cor((GfxRect){ x, y, w * frac, 10 }, 0.5f, 0.902f, 0.902f, 0.894f, a);
}

static void kv(const char *k, const char *v, float x, float y, float w, int fio, float a) {
  TxtLinha tv = txt_linha_corta(TXT_G23B, v[0] ? v : "—", 236, 236, 236, 255, w * 0.76f);
  TxtLinha tk = txt_linha_corta(TXT_AJ_TEXTO, k, 154, 154, 156, 255, w - (float)tv.w - 16);
  if (fio) gfx_cor((GfxRect){ x, y, w, 1 }, 0, 0.227f, 0.227f, 0.235f, a);
  txt_desenhar_alpha(tk, x, y + (40 - (float)tk.h) * 0.5f, a);
  txt_desenhar_alpha(tv, x + w - (float)tv.w, y + (40 - (float)tv.h) * 0.5f, a);
}

// --- OS SEIS LADRILHOS -------------------------------------------------------------
static void tileVelocidade(GfxRect r, float a) {
  char n[24], p[24], m[24], t[64];
  float x = r.x + 24, y = r.y + 86, w = r.w - 48;
  int medido = est.velOrigem == EVO_MEDIDO;
  ladrilho(r, "aj_gauge", medido ? i18n("Velocidade agora") : i18n("Velocidade"), est.velOrigem, a);
  ev_fmt_mbps(n, sizeof n, est.kbps, sepDec());
  // Estimado nao tem casa decimal: a precisao seria inventada.
  if (est.kbps >= 0 && !medido) snprintf(n, sizeof n, "%d", (est.kbps + 500) / 1000);
  if (est.kbps >= 0 && !medido) {
    TxtLinha q = txt_linha(TXT_AJ_NUM58, "≈", EVU_TX);
    txt_desenhar_alpha(q, x, y, a);
    x += (float)q.w + 14;
  }
  numero(n, "Mbps", x, y, a, 0);
  x = r.x + 24;
  if (medido && est.picoKbps > 0) {
    const char *partes[4];
    ev_fmt_mbps(p, sizeof p, est.picoKbps, sepDec());
    ev_fmt_mbps(m, sizeof m, est.mediaKbps, sepDec());
    snprintf(t, sizeof t, "%s Mbps", p);
    partes[0] = i18n("Pico"); partes[1] = t; partes[2] = i18n("· média"); partes[3] = m;
    apoio(partes, 4, x, r.y + 160, w, a);
  } else {
    const char *s = est.velOrigem == EVO_SEM ? "Esta TV não informa a velocidade"
                  : est.kbps < 0 && !est.nHist ? "Medindo…"
                  : est.bufOrigem == EVO_MEDIDO ? "Pelo ritmo em que o buffer enche"
                  : "Pelo ritmo em que o vídeo toca";
    TxtLinha l = txt_linha_corta(TXT_AJ_TEXTO, s, 180, 180, 182, 255, w);
    txt_desenhar_alpha(l, x, r.y + 160, a);
  }
  faisca(est.hist, est.nHist, (GfxRect){ x, r.y + 188, w, 38 }, a);
}

static void tileBuffer(GfxRect r, float a) {
  char n[24], t[96];
  float x = r.x + 24, y = r.y + 86, w = r.w - 48;
  ladrilho(r, "aj_database", i18n("Buffer à frente"), est.bufOrigem, a);
  if (est.bufOrigem == EVO_MEDIDO) {
    float nw;
    snprintf(n, sizeof n, "%d", (int)(est.bufSeg + 0.5));
    nw = numero(n, "s", x, y, a, 0);
    if (est.bufMb >= 0) {
      TxtLinha l;
      snprintf(t, sizeof t, "≈ %d MB", (int)(est.bufMb + 0.5));
      l = txt_linha(TXT_V2_LN, t, 180, 180, 182, 255);
      txt_desenhar_alpha(l, x + nw + 32, y + (float)txt_linha(TXT_AJ_NUM58, "0", EVU_TX).h * 0.80f - (float)l.h, a);
    }
    barra(x, r.y + 166, w, (float)(est.bufSeg / (est.bufAlvo > 0 ? est.bufAlvo : 60.0)), a);
    if (est.bufAlvo > 0) snprintf(t, sizeof t, i18n("de %d s que o player tenta manter"), (int)(est.bufAlvo + 0.5));
    else snprintf(t, sizeof t, "%s", i18n("segundos de vídeo já baixados"));
    { TxtLinha l = txt_linha_corta(TXT_AJ_TEXTO, t, 180, 180, 182, 255, w); txt_desenhar_alpha(l, x, r.y + 190, a); }
  } else if (est.bufOrigem == EVO_SO_PCT) {
    snprintf(n, sizeof n, "%d", est.bufPct);
    numero(n, "%", x, y, a, 0);
    barra(x, r.y + 166, w, (float)est.bufPct / 100.0f, a);
    txt_bloco(TXT_AJ_TEXTO, i18n("A Samsung informa a porcentagem, não os segundos"), 180, 180, 182,
              x, r.y + 186, w, 28, a, 2);
  } else {
    numero(i18n("Indisponível"), NULL, x, y + 8, a, 1);
    txt_bloco(TXT_AJ_TEXTO, i18n("Esta TV não informa o buffer"), 180, 180, 182, x, r.y + 160, w, 28, a, 2);
  }
}

static void tileFonte(GfxRect r, float a) {
  float x = r.x + 24, y = r.y + 78, w = r.w - 48;
  ladrilho(r, "aj_film", i18n("Fonte"), tela.addon[0] ? EVO_ADDON : EVO_SEM, a);
  kv(i18n("Add-on"), tela.addon, x, y, w, 0, a);
  kv(i18n("Resolução"), tela.resolucao, x, y + 34, w, 1, a);
  kv(i18n("Codec · HDR"), tela.codecHdr, x, y + 68, w, 1, a);
  kv(i18n("Taxa do arquivo"), tela.taxa, x, y + 102, w, 1, a);
  kv(i18n("Áudio"), tela.audio, x, y + 136, w, 1, a);
  kv(i18n("Legenda"), tela.legenda, x, y + 170, w, 1, a);
}

static void tileServidor(GfxRect r, float a) {
  char n[16], t[64];
  float x = r.x + 24, y = r.y + 86, w = r.w - 48;
  if (est.servOrigem == EVO_MEDIDO) {
    const char *p[2];
    ladrilho(r, "aj_server", i18n("Conexões e servidor"), EVO_MEDIDO, a);
    snprintf(n, sizeof n, "%d", est.conexoes);
    numero(n, i18n(est.conexoes == 1 ? "conexão" : "conexões"), x, y, a, 0);
    p[0] = i18n("Servidor"); p[1] = est.host[0] ? est.host : "—";
    apoio(p, 2, x, r.y + 160, w, a);
    if (est.primeiroByteMs >= 0) {
      snprintf(t, sizeof t, "%d ms", est.primeiroByteMs);
      p[0] = i18n("Resposta do 1º byte"); p[1] = t;
      apoio(p, 2, x, r.y + 192, w, a);
    }
  } else if (est.servOrigem == EVO_LINK) {
    TxtLinha h = txt_linha_corta(TXT_ILHA_PERGUNTA, est.host, EVU_TX, w);
    ladrilho(r, "aj_server", i18n("Servidor"), EVO_LINK, a);
    txt_desenhar_alpha(h, x, y, a);
    txt_bloco(TXT_AJ_TEXTO, i18n("A TV não diz quantas conexões o player abre"), 180, 180, 182,
              x, y + (float)h.h + 14, w, 28, a, 2);
  } else {
    ladrilho(r, "aj_server", i18n("Servidor"), EVO_SEM, a);
    numero(i18n("Indisponível"), NULL, x, y + 8, a, 1);
  }
}

static void tileErros(GfxRect r, float a) {
  char n[16], t[128];
  float x = r.x + 24, y = r.y + 86, w = r.w - 48;
  ladrilho(r, "aj_rotate-cw", i18n("Erros e reconexões"), EVO_MEDIDO, a);
  snprintf(n, sizeof n, "%d", est.erros);
  numero(n, i18n(est.erros == 1 ? "erro" : "erros"), x, y, a, 0);
  if (!est.reconexoes) snprintf(t, sizeof t, "%s", i18n("nenhuma reconexão"));
  else if (est.reconexoes == 1) snprintf(t, sizeof t, i18n("1 reconexão às %02d:%02d"), est.reconHora, est.reconMin);
  else snprintf(t, sizeof t, i18n("%d reconexões, a última às %02d:%02d"), est.reconexoes, est.reconHora, est.reconMin);
  if (est.reconexoes && !est.erros) {
    size_t u = strlen(t);
    snprintf(t + u, sizeof t - u, " · %s", i18n("sem falha"));
  }
  { TxtLinha l = txt_linha_corta(TXT_AJ_TEXTO, t, 180, 180, 182, 255, w); txt_desenhar_alpha(l, x, r.y + 160, a); }
}

static void tileQuadros(GfxRect r, float a) {
  char n[24], q[32], p[24], t[64];
  float x = r.x + 24, y = r.y + 86, w = r.w - 48;
  ladrilho(r, "aj_monitor-play", i18n("Quadros perdidos"), est.quadrosOrigem, a);
  if (est.quadrosOrigem == EVO_MEDIDO) {
    const char *partes[2];
    ev_fmt_milhar(n, sizeof n, est.perdidos);
    ev_fmt_milhar(q, sizeof q, est.quadros);
    snprintf(t, sizeof t, i18n("de %s"), q);
    numero(n, t, x, y, a, 0);
    ev_fmt_pct2(p, sizeof p, est.perdidos, est.quadros, sepDec());
    snprintf(t, sizeof t, "%s %%", p);
    (void)partes;
    { TxtLinha l = txt_linha(TXT_G21B, t, 255, 255, 255, 255);
      txt_desenhar_alpha(l, x, r.y + 161, a);
      TxtLinha d = txt_linha_corta(TXT_AJ_TEXTO, i18n("desde que o filme abriu"), 180, 180, 182, 255, w - (float)l.w - 7);
      txt_desenhar_alpha(d, x + (float)l.w + 7, r.y + 160, a); }
  } else {
    int samsung = tela.plataforma == PTV_TIZEN || tela.plataforma == PTV_TPK;
    numero(i18n("Indisponível"), NULL, x, y + 8, a, 1);
    txt_bloco(TXT_AJ_TEXTO, i18n(samsung ? "O decodificador da Samsung não conta quadros"
                                         : "O player desta TV não conta quadros"),
              180, 180, 182, x, r.y + 154, w, 28, a, 2);
  }
}

static const char *nomePlataforma(int p) {
  switch (p) {
    case PTV_TIZEN: return "Samsung · Tizen";
#ifdef NV_TPK40
    case PTV_TPK: return "Samsung · Tizen 4/5";
#else
    case PTV_TPK: return "Samsung · Tizen 6+";
#endif
    case PTV_ANDROID: return "Android TV";
    default: return "LG webOS";
  }
}

static void ptFora(int x, int y) { (void)x; (void)y; estatvivo_fechar(); }

static void desenharCartao(float a) {
  GfxRect c = { EVU_X, EVU_Y, EVU_W, EVU_H };
  float x = c.x + 36, cw, xr;
  if (ponteiro_ativo() && a > 0.5f) {
    ponteiro_camada();
    ponteiro_alvo(0, 0, NV_TELA_W, NV_TELA_H, NULL, ptFora, 0, 0);
    ponteiro_alvo(c.x, c.y, c.w, c.h, NULL, NULL, 0, 0);
  }
  gfx_cor(c, 26.0f / c.h, 0.078f, 0.078f, 0.086f, 0.97f * a);
  gfx_anel(c, 26.0f / c.h, 1.0f, 1, 1, 1, 0.05f * a);
  caps("Estatísticas ao vivo", x, c.y + 30, a);
  { TxtLinha t = txt_linha_corta(TXT_ILHA_PERGUNTA, tela.titulo[0] ? tela.titulo : i18n("Tocando agora"), EVU_TX, 470);
    txt_desenhar_alpha(t, x, c.y + 58, a); }
  if (tela.sub[0]) {
    TxtLinha s = txt_linha_corta(TXT_AJ_TEXTO, tela.sub, 180, 180, 182, 255, 470);
    txt_desenhar_alpha(s, x, c.y + 108, a);
  }
  // Chips a direita: plataforma e cadencia.
  xr = c.x + c.w - 36;
  cw = chip("aj_clock", i18n("atualiza a cada 1 s"), 0, 0, 40, a, 0);
  xr -= cw;
  chip("aj_clock", i18n("atualiza a cada 1 s"), xr, c.y + 34, 40, a, 1);
  cw = chip("aj_tv-minimal-play", nomePlataforma(tela.plataforma), 0, 0, 40, a, 0);
  xr -= 12 + cw;
  chip("aj_tv-minimal-play", nomePlataforma(tela.plataforma), xr, c.y + 34, 40, a, 1);
  { float x0 = x, x1 = x + EVU_TILE_W + 24, y0 = c.y + 140;
    tileVelocidade((GfxRect){ x0, y0, EVU_TILE_W, 244 }, a);
    tileBuffer((GfxRect){ x1, y0, EVU_TILE_W, 244 }, a);
    tileFonte((GfxRect){ x0, y0 + 244 + EVU_GAP, EVU_TILE_W, 284 }, a);
    tileServidor((GfxRect){ x1, y0 + 244 + EVU_GAP, EVU_TILE_W, 284 }, a);
    tileErros((GfxRect){ x0, y0 + 284 + 244 + 2 * EVU_GAP, EVU_TILE_W, 214 }, a);
    tileQuadros((GfxRect){ x1, y0 + 284 + 244 + 2 * EVU_GAP, EVU_TILE_W, 214 }, a); }
  // Dicas embaixo do cartao.
  { static const char *const K[2] = { "OK", "Voltar" };
    static const char *const L[2] = { "Fixar no canto", "Fechar" };
    float hx = x, hy = c.y + c.h + 18;
    int i;
    for (i = 0; i < 2; i++) {
      TxtLinha k = txt_linha(TXT_V2_KBD18, K[i], EVU_TX), l = txt_linha(TXT_AJ_TEXTO, L[i], 196, 196, 196, 255);
      float kw = (float)k.w + 20 < 48 ? 48 : (float)k.w + 20;
      gfx_cor((GfxRect){ hx, hy, kw, 30 }, 0.5f, 0.165f, 0.165f, 0.176f, a);
      txt_desenhar_alpha(k, hx + (kw - (float)k.w) * 0.5f, hy + (30 - (float)k.h) * 0.5f, a);
      txt_desenhar_alpha(l, hx + kw + 10, hy + (30 - (float)l.h) * 0.5f, a);
      hx += kw + 10 + (float)l.w + 24;
    } }
}

// FIXADO NO CANTO: a velocidade e o buffer numa pilula no alto a esquerda,
// fora do caminho da ilha do relogio (direita) e das legendas (embaixo).
static void desenharPilula(void) {
  char n[24], b[48];
  GfxRect r = { 48, 40, 0, 64 };
  TxtLinha tn, tb;
  ev_fmt_mbps(n, sizeof n, est.kbps, sepDec());
  if (est.kbps >= 0 && est.velOrigem != EVO_MEDIDO) snprintf(n, sizeof n, "≈ %d", (est.kbps + 500) / 1000);
  tn = txt_linha(TXT_G26B, n, EVU_TX);
  if (est.bufOrigem == EVO_MEDIDO) snprintf(b, sizeof b, i18n("Mbps · buffer %d s"), (int)(est.bufSeg + 0.5));
  else if (est.bufOrigem == EVO_SO_PCT) snprintf(b, sizeof b, i18n("Mbps · buffer %d%%"), est.bufPct);
  else snprintf(b, sizeof b, "Mbps");
  tb = txt_linha(TXT_AJ_TEXTO, b, 196, 196, 196, 255);
  r.w = 20 + 26 + 12 + (float)tn.w + 10 + (float)tb.w + 22 + 120 + 20;
  gfx_cor(r, 0.5f, 0.078f, 0.078f, 0.086f, 0.92f);
  gfx_anel(r, 0.5f, 1.0f, 1, 1, 1, 0.06f);
  gfx_icone((GfxRect){ r.x + 20, r.y + 19, 26, 26 }, "aj_gauge", 0.953f, 0.949f, 0.937f, 1);
  txt_desenhar(tn, r.x + 58, r.y + (64 - (float)tn.h) * 0.5f);
  txt_desenhar(tb, r.x + 58 + (float)tn.w + 10, r.y + (64 - (float)tb.h) * 0.5f);
  faisca(est.hist, est.nHist, (GfxRect){ r.x + r.w - 20 - 120, r.y + 16, 120, 32 }, 1);
}

void estatvivo_desenhar(Uint32 agora) {
  float a = 1.0f;
  if (!aberto && !fixado) return;
#ifndef ESTATVIVO_TESTE
  if (!player_aberto()) return;
#endif
  if (fixado) { desenharPilula(); return; }
  if (!ajustes_animacoes_reduzidas()) {
    float t = (float)(agora - abriuEm) / 180.0f;
    a = t >= 1.0f ? 1.0f : t < 0 ? 0 : t;
  }
  if (!tela.titulo[0] && !tela.addon[0] && temSessao) montarTela(0);
  desenharCartao(a);
}

#ifdef ESTATVIVO_TESTE
void estatvivo_teste(const EvEstado *e, const EvTelaTeste *t) {
  teste = 1;
  temSessao = 1;
  est = *e;
  memset(&tela, 0, sizeof tela);
  snprintf(tela.titulo, sizeof tela.titulo, "%s", t->titulo);
  snprintf(tela.sub, sizeof tela.sub, "%s", t->sub);
  snprintf(tela.addon, sizeof tela.addon, "%s", t->addon);
  snprintf(tela.resolucao, sizeof tela.resolucao, "%s", t->resolucao);
  snprintf(tela.codecHdr, sizeof tela.codecHdr, "%s", t->codecHdr);
  snprintf(tela.taxa, sizeof tela.taxa, "%s", t->taxa);
  snprintf(tela.audio, sizeof tela.audio, "%s", t->audio);
  snprintf(tela.legenda, sizeof tela.legenda, "%s", t->legenda);
  tela.plataforma = t->plataforma;
  abriuEm = 0;
}
#endif
