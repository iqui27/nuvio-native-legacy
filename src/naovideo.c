#include "naovideo.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>

static int terminaCom(const char *s, size_t n, const char *suf) {
  size_t k = strlen(suf);
  return n >= k && !strncasecmp(s + n - k, suf, k);
}

// Caminho da URL sem query nem fragmento, para olhar a extensao.
static int urlDeManifesto(const char *url) {
  size_t n;
  if (!url) return 0;
  n = strcspn(url, "?#");
  return terminaCom(url, n, ".m3u8") || terminaCom(url, n, ".mpd") || terminaCom(url, n, ".m3u") ||
         strstr(url, "m3u8") != NULL || strstr(url, "M3U8") != NULL;
}

int naovideo_mime(const char *mime, const char *url, long corpo) {
  static const char *const ruins[] = { "text/html", "application/xhtml", "application/json",
    "text/json", "text/javascript", "application/javascript" };
  char m[96];
  size_t k = 0;
  int manifesto = urlDeManifesto(url);
  if (mime) for (; *mime && *mime != ';' && k + 1 < sizeof m; mime++)
    if (*mime != ' ') m[k++] = (char)tolower((unsigned char)*mime);
  m[k] = 0;
  // Corpo inteiro e minusculo: nenhum video cabe em 63 bytes. Manifesto fica de
  // fora porque a playlist sem segmento tem o proprio veredito (playlistVazia).
  if (corpo >= 0 && corpo < 64 && !manifesto) return 1;
  if (!m[0]) return 0;
  for (size_t i = 0; i < sizeof ruins / sizeof *ruins; i++)
    if (!strncmp(m, ruins[i], strlen(ruins[i]))) return 1;
  // text/plain so e video quando e manifesto servido com tipo errado.
  if (!strcmp(m, "text/plain")) return !manifesto;
  return 0;
}

// Palavra inteira (nao "terror" para "error").
static int palavra(const char *t, const char *p) {
  size_t k = strlen(p);
  for (const char *a = t; (a = strstr(a, p)); a += k) {
    int ini = a == t || !isalnum((unsigned char)a[-1]);
    int fim = !isalnum((unsigned char)a[k]);
    if (ini && fim) return 1;
  }
  return 0;
}

// Tokens que so um arquivo de video traz. Presente qualquer um, e filme.
static int temDadoDeVideo(const char *t) {
  static const char *const fr[] = { "2160p", "1080p", "720p", "480p", "576p", "1440p", "4k",
    "uhd", "hdr", "hdr10", "dolby", "hevc", "x265", "x264", "h264", "h.264", "h265", "h.265",
    "av1", "remux", "bluray", "blu-ray", "web-dl", "webrip", "webdl", "hdtv", "bdrip", "dvdrip",
    "atmos", "ddp", "dts", "aac", "ac3", ".mkv", ".mp4", ".avi" };
  for (size_t i = 0; i < sizeof fr / sizeof *fr; i++)
    if (strstr(t, fr[i])) return 1;
  // "12.4 GB", "800 MB", "1.2GB".
  for (const char *a = t; *a; a++)
    if (isdigit((unsigned char)*a)) {
      const char *b = a;
      while (isdigit((unsigned char)*b) || *b == '.' || *b == ',') b++;
      while (*b == ' ') b++;
      if ((b[0] == 'g' || b[0] == 'm') && b[1] == 'b' && !isalnum((unsigned char)b[2])) return 1;
      a = b > a ? b - 1 : a;
    }
  return 0;
}

int naovideo_nome(const char *rotulo, const char *descricao, int altura, long tamanhoMB) {
  // Frases (casam em qualquer ponto), em ingles, portugues, espanhol, frances e
  // alemao: os idiomas do app em que os addons mais escrevem.
  static const char *const frases[] = {
    "support the project", "support this project", "support the addon", "support the add-on",
    "support the development", "support us", "support me", "support our", "donate", "donation",
    "patreon", "ko-fi", "kofi", "buymeacoffee", "buy me a coffee", "paypal.me", "become a patron",
    "join the discord", "join our discord", "join us on discord", "join discord", "discord.gg",
    "join the telegram", "join our telegram", "join telegram", "t.me/",
    "not available", "currently unavailable", "stream unavailable", "no streams available",
    "nao disponivel", "indisponivel", "não disponível", "indisponível", "sem fontes disponiveis",
    "no disponible", "non disponible", "nicht verfugbar", "nicht verfügbar",
    "please configure", "configure the addon", "configure your", "configure addon",
    "not configured", "needs configuration", "configuration required", "configuracao necessaria",
    "configuração necessária", "configure o addon", "please install", "install the addon",
    "install the add-on", "install our", "install addon", "instale o addon",
    "apoie o projeto", "apoie o addon", "faca uma doacao", "faça uma doação", "doacao", "doação",
    "donacion", "donación", "apoya el proyecto", "faire un don", "soutenez le projet",
    "spenden", "unterstutze das projekt", "unterstütze das projekt",
    "note:", "notice:", "warning:", "aviso:", "nota:", "link expired", "token expired",
    "subscription expired", "trial expired", "api key", "invalid key", "invalid api",
    "{}", "[object object]", "missing config", "no config"
  };
  static const char *const palavras[] = { "unavailable", "error", "erro", "expired", "expirado",
    "indisponivel", "fehler", "configure", "unconfigured" };
  char t[2048];
  size_t k = 0;
  if (altura > 0 || tamanhoMB > 0) return 0;
  for (const char *a = rotulo; a && *a && k + 1 < sizeof t; a++) t[k++] = (char)tolower((unsigned char)*a);
  if (k + 1 < sizeof t) t[k++] = ' ';
  for (const char *a = descricao; a && *a && k + 1 < sizeof t; a++) t[k++] = (char)tolower((unsigned char)*a);
  t[k] = 0;
  if (!t[0] || temDadoDeVideo(t)) return 0;
  for (size_t i = 0; i < sizeof frases / sizeof *frases; i++)
    if (strstr(t, frases[i])) return 1;
  for (size_t i = 0; i < sizeof palavras / sizeof *palavras; i++)
    if (palavra(t, palavras[i])) return 1;
  return 0;
}
