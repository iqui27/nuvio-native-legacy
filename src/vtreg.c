// Ver vtreg.h.
#include "vtreg.h"
#include <stdio.h>
#include <string.h>

static int minusc(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }
static int ehHex(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
static int ehDig(int c) { return c >= '0' && c <= '9'; }

// `agulha` minuscula dentro de `s`, sem diferenciar caixa.
static int contem(const char *s, const char *agulha) {
  size_t n = strlen(agulha);
  if (!s) return 0;
  for (; *s; s++) {
    size_t i = 0;
    while (i < n && s[i] && minusc((unsigned char)s[i]) == agulha[i]) i++;
    if (i == n) return 1;
  }
  return 0;
}

typedef struct { const char *agulha, *token; } Par;
// A ORDEM importa: o mais especifico antes ("eac3" antes de "ac3").
static const Par CONTAINER[] = {
  { "matroska", "mkv" }, { "mkv", "mkv" }, { "webm", "webm" }, { "mp4", "mp4" }, { "m4v", "mp4" },
  { "mov", "mp4" }, { "mpegts", "ts" }, { "m2ts", "ts" }, { "mpeg-ts", "ts" }, { "hls", "hls" },
  { "m3u8", "hls" }, { "dash", "dash" }, { "avi", "avi" }, { "flv", "flv" }, { "ts", "ts" }, { NULL, NULL }
};
static const Par VIDEO[] = {
  { "hevc", "hevc" }, { "h265", "hevc" }, { "h.265", "hevc" }, { "hev1", "hevc" }, { "hvc1", "hevc" },
  { "dvhe", "hevc" }, { "dvh1", "hevc" }, { "av01", "av1" }, { "av1", "av1" }, { "vp9", "vp9" },
  { "vp09", "vp9" }, { "vp8", "vp8" }, { "avc", "h264" }, { "h264", "h264" }, { "h.264", "h264" },
  { "mpeg4", "mpeg4" }, { "xvid", "mpeg4" }, { "divx", "mpeg4" }, { "mpeg2", "mpeg2" }, { "vc1", "vc1" },
  { "vc-1", "vc1" }, { NULL, NULL }
};
static const Par AUDIO[] = {
  { "truehd", "truehd" }, { "true-hd", "truehd" }, { "mlp", "truehd" }, { "eac3", "eac3" }, { "e-ac-3", "eac3" }, { "ec-3", "eac3" },
  { "ec3", "eac3" }, { "ddp", "eac3" }, { "ac3", "ac3" }, { "ac-3", "ac3" }, { "dts", "dts" }, { "aac", "aac" },
  { "mp4a", "aac" }, { "opus", "opus" }, { "vorbis", "vorbis" }, { "flac", "flac" }, { "mp3", "mp3" },
  { "mpeg", "mp3" }, { "pcm", "pcm" }, { "audio/raw", "pcm" }, { NULL, NULL }
};
static const Par HDR[] = {
  { "dolby", "dv" }, { "dovi", "dv" }, { "hdr10+", "hdr10p" }, { "hdr10plus", "hdr10p" }, { "hdr10", "hdr10" },
  { "hlg", "hlg" }, { "pq", "hdr10" }, { "hdr", "hdr10" }, { "none", "sdr" }, { "sdr", "sdr" }, { NULL, NULL }
};
static const char *const RESULTADO[VTR_N] = {
  "tocou", "nao_abriu", "caiu", "prazo", "travou", "lenta", "decoder", "tela_preta", "sem_audio"
};

static void vocab(const char *s, const Par *t, char *out, size_t cap) {
  const char *tok = "?";
  if (s && *s) {
    tok = "outro";
    for (; t->agulha; t++) if (contem(s, t->agulha)) { tok = t->token; break; }
  }
  snprintf(out, cap, "%s", tok);
}

int vtreg_texto_seguro(const char *s, char *out, size_t cap) {
  size_t n = 0, i;
  int hex = 0, maxHex = 0, pontos = 0, soDigPonto = 1;
  if (!cap) return 0;
  if (!s || !*s) goto nao;
  if (contem(s, "://") || contem(s, "www.") || contem(s, "magnet") || contem(s, "http")) goto nao;
  for (i = 0; s[i]; i++) {
    unsigned char c = (unsigned char)s[i];
    if (c == '/' || c == '\\' || c == '@' || c == '?' || c == '=' || c == '&' || c == '%' || c == ':') goto nao;
    if (ehHex(c)) { if (++hex > maxHex) maxHex = hex; } else hex = 0;
    if (c == '.') pontos++;
    else if (!ehDig(c)) soDigPonto = 0;
  }
  // 16+ hex seguidos: pedaco de hash ou de identificador de aparelho.
  if (maxHex >= 16) goto nao;
  // Quatro grupos de digitos e nada mais: IPv4.
  if (soDigPonto && pontos == 3) {
    int a, b, c, d;
    if (sscanf(s, "%d.%d.%d.%d", &a, &b, &c, &d) == 4 && a <= 255 && b <= 255 && c <= 255 && d <= 255 &&
        strlen(s) >= 7 && (a > 99 || b > 99 || c > 99 || d > 99)) goto nao;
  }
  for (i = 0; s[i] && n + 1 < cap; i++) {
    unsigned char c = (unsigned char)s[i];
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || ehDig(c) || c == '.' || c == '-' || c == '_' || c == '+')
      out[n++] = (char)c;
    else if (c == ' ' || c == '(' || c == ')' || c == ',') {
      if (n && out[n - 1] != ' ') out[n++] = ' ';
    }
    // qualquer outro byte (acento, controle, aspas) some
  }
  while (n && out[n - 1] == ' ') n--;
  out[n] = 0;
  if (!n) goto nao;
  return 1;
nao:
  snprintf(out, cap, "?");
  return 0;
}

static int faixa(int v, const int *tetos, int n) {
  int i;
  if (v <= 0) return 0;
  for (i = 0; i < n; i++) if (v <= tetos[i]) return tetos[i];
  return tetos[n - 1];
}

const char *vtreg_resultado_nome(int resultado) {
  return resultado >= 0 && resultado < VTR_N ? RESULTADO[resultado] : "outro";
}

int vtreg_classificar(const VtRegSinais *s) {
  if (!s) return -1;
  switch (s->motivo) {
    case 1: return s->erroDecoder ? VTR_DECODER : s->abriu ? VTR_CAIU : VTR_NAO_ABRIU;
    case 2: return VTR_PRAZO;
    case 3: return VTR_TRAVOU;
    case 4: return VTR_LENTA;
  }
  return -1;
}

int vtreg_normalizar(const VtRegEntrada *e, VtRegRegistro *r) {
  static const int MBPS[] = { 2, 5, 10, 20, 40, 80, 999 };
  static const int ALT[] = { 480, 720, 1080, 1440, 2160, 4320 };
  static const char *const PLAT[] = { "webos", "tizen", "android", "linux", "macos", NULL };
  int i, semFormato;
  if (!e || !r) return 0;
  memset(r, 0, sizeof *r);
  if (e->resultado < 0 || e->resultado >= VTR_N) return 0;
  snprintf(r->plataforma, sizeof r->plataforma, "outro");
  for (i = 0; PLAT[i]; i++)
    if (e->plataforma && !strcmp(e->plataforma, PLAT[i])) snprintf(r->plataforma, sizeof r->plataforma, "%s", PLAT[i]);
  vtreg_texto_seguro(e->modelo, r->modelo, sizeof r->modelo);
  vtreg_texto_seguro(e->firmware, r->firmware, sizeof r->firmware);
  vocab(e->container, CONTAINER, r->container, sizeof r->container);
  vocab(e->video, VIDEO, r->video, sizeof r->video);
  vocab(e->audio, AUDIO, r->audio, sizeof r->audio);
  vocab(e->hdr, HDR, r->hdr, sizeof r->hdr);
  r->mbps = faixa((e->kbps + 999) / 1000, MBPS, 7);
  if (e->kbps <= 0) r->mbps = 0;
  r->altura = faixa(e->altura, ALT, 6);
  r->resultado = e->resultado;
  r->trocou = e->trocou ? 1 : 0;
  semFormato = r->container[0] == '?' && r->video[0] == '?';
  if (semFormato && (r->resultado == VTR_PRAZO || r->resultado == VTR_LENTA || r->resultado == VTR_TRAVOU)) return 0;
  return 1;
}

size_t vtreg_json(const VtRegRegistro *r, char *out, size_t cap) {
  int n;
  if (!r || !out || !cap) return 0;
  // Todos os campos de texto sao [A-Za-z0-9 ._+-?]: nada a escapar.
  n = snprintf(out, cap,
    "{\"v\":1,\"plataforma\":\"%s\",\"modelo\":\"%s\",\"firmware\":\"%s\",\"container\":\"%s\","
    "\"video\":\"%s\",\"audio\":\"%s\",\"hdr\":\"%s\",\"mbps\":%d,\"altura\":%d,\"resultado\":\"%s\",\"trocou\":%d}",
    r->plataforma, r->modelo, r->firmware, r->container, r->video, r->audio, r->hdr,
    r->mbps, r->altura, vtreg_resultado_nome(r->resultado), r->trocou);
  if (n <= 0 || (size_t)n >= cap) { out[0] = 0; return 0; }
  return (size_t)n;
}

unsigned vtreg_chave(const VtRegRegistro *r) {
  char b[400];
  unsigned h = 2166136261u;
  size_t i, n = vtreg_json(r, b, sizeof b);
  for (i = 0; i < n; i++) { h ^= (unsigned char)b[i]; h *= 16777619u; }
  return h ? h : 1;
}

int vtreg_novo(VtRegVistos *v, unsigned chave) {
  int i;
  if (!v) return 0;
  for (i = 0; i < v->n; i++) if (v->chave[i] == chave) return 0;
  v->chave[v->prox] = chave;
  v->prox = (v->prox + 1) % VTR_VISTOS;
  if (v->n < VTR_VISTOS) v->n++;
  return 1;
}

int vtreg_pode_enviar(int consentiu, int ajusteLigado, int temServidor) {
  return consentiu && ajusteLigado && temServidor;
}
