#include "vaitocar.h"
// Puro: so le. A lista de fontes ja esta em memoria (streams.c) e as marcas no
// cache de episodios do intro.c — os dois abastecidos pelos caminhos que ja
// existiam para o player. Nada de rede, nada de GL.
#include <stdio.h>
#include <string.h>
#include "intro.h"
#include "ajustes.h"
#include "idioma.h"
#include "badges.h"
#include "plrui.h"

// O AUDIO como quem assiste nomeia: o codec que o addon cita e, quando o texto
// diz, os canais. O que nao estiver no texto fica de fora — sem chute.
static void audioDo(const Stream *s, char *dst, size_t n) {
  static const struct { const char *bit, *rot; } T[] = {
    { "a-truehd", "TrueHD" }, { "a-atmos", "Atmos" }, { "a-dtshdma", "DTS-HD MA" },
    { "a-dtshd", "DTS-HD" }, { "a-dtsx", "DTS:X" }, { "a-dts", "DTS" },
    { "a-eac3", "EAC3" }, { "a-ddp", "EAC3" }, { "a-dd", "AC3" },
    { "a-ac3", "AC3" }, { "a-aac", "AAC" },
  };
  dst[0] = 0;
  for (size_t i = 0; i < sizeof T / sizeof T[0]; i++)
    if (s->badges & badges_bit(T[i].bit)) { snprintf(dst, n, "%s", T[i].rot); break; }
  if (!dst[0]) return;
  if (stream_texto_tem(s, "5.1") || s->dolbyAtmos) strncat(dst, " 5.1", n - strlen(dst) - 1);
  else if (stream_texto_tem(s, "7.1")) strncat(dst, " 7.1", n - strlen(dst) - 1);
}

// IDIOMA como a pessoa le: "Português" quando ha faixa em PT (dublado ou dual),
// "Legendado" quando so ha legenda; "· legenda PT" so se o addon cita PT-BR
// alem da faixa. Vazio = sem dado.
static void idiomasDo(const Stream *s, char *dst, size_t n) {
  const char *id = stream_idioma(s);
  dst[0] = 0;
  if (!id) return;
  if (!strcmp(id, "Legendado")) { snprintf(dst, n, "%s", i18n("Legendado")); return; }
  snprintf(dst, n, "%s", i18n("Português"));
  if (stream_texto_tem(s, "legendado") || stream_texto_tem(s, "legenda"))
    strncat(dst, i18n(" · legenda PT"), n - strlen(dst) - 1);
}

static void tamanhoDo(const Stream *s, char *dst, size_t n) {
  unsigned long long mb = s->tamanhoBytes ? s->tamanhoBytes >> 20 : s->tamanhoMB > 0 ? (unsigned long long)s->tamanhoMB : 0;
  dst[0] = 0;
  if (!mb) return;
  if (mb >= 1024) { snprintf(dst, n, "%.1f GB", mb / 1024.0); plrui_decimal(dst); }
  else snprintf(dst, n, "%llu MB", mb);
}

void vt_descrever(const Stream *s, VaiTocar *vt) {
  static const struct { const char *bit, *rot; } TIPO[] = {
    { "q-remux", "Remux" }, { "q-bluray", "BluRay" }, { "q-webdl", "WEB-DL" }, { "q-webrip", "WEB-Rip" },
  };
  vt->tem = 1;
  for (size_t i = 0; i < sizeof TIPO / sizeof TIPO[0]; i++)
    if (s->badges & badges_bit(TIPO[i].bit)) { snprintf(vt->tipo, sizeof vt->tipo, "%s", TIPO[i].rot); break; }
  if (s->altura >= 2000 || (s->badges & badges_bit("r-4k"))) snprintf(vt->res, sizeof vt->res, "4K");
  else if (s->altura >= 1000 || (s->badges & badges_bit("r-1080"))) snprintf(vt->res, sizeof vt->res, "1080p");
  else if (s->altura >= 700 || (s->badges & badges_bit("r-720"))) snprintf(vt->res, sizeof vt->res, "720p");
  // HDR: o Dolby Vision antes (o que a TV liga primeiro). Vazio = SDR.
  if (s->dolbyVision || (s->badges & badges_bit("v-dv"))) snprintf(vt->hdr, sizeof vt->hdr, "Dolby Vision");
  else if (s->badges & badges_bit("v-hdr10plus")) snprintf(vt->hdr, sizeof vt->hdr, "HDR10+");
  else if (s->badges & badges_bit("v-hdr10")) snprintf(vt->hdr, sizeof vt->hdr, "HDR10");
  else if (s->badges & (badges_bit("v-hdr") | badges_bit("v-hlg")))
    snprintf(vt->hdr, sizeof vt->hdr, "HDR");
  audioDo(s, vt->audio, sizeof vt->audio);
  idiomasDo(s, vt->idiomas, sizeof vt->idiomas);
  tamanhoDo(s, vt->tamanho, sizeof vt->tamanho);
  vt->cache = !s->foraCache;
  snprintf(vt->provedor, sizeof vt->provedor, "%s", s->provedor);
}

int vt_montar(VaiTocar *vt, const char *imdb, int temporada, int episodio) {
  memset(vt, 0, sizeof *vt);
  vt->temporada = temporada; vt->episodio = episodio;
  // Sem a lista do MESMO alvo desta pagina, nao ha o que descrever.
  char alvo[64];
  if (!imdb || !imdb[0]) return 0;
  if (temporada > 0 && episodio > 0) snprintf(alvo, sizeof alvo, "%s:%d:%d", imdb, temporada, episodio);
  else snprintf(alvo, sizeof alvo, "%s", imdb);
  if (!stream_lista_do_alvo(alvo)) return 0;
  int i = stream_automatico();
  const Stream *s = i >= 0 ? stream_item(i) : NULL;
  if (!s) return 0;
  vt_descrever(s, vt);
  vt->pularOn = vt_pular_ligado();
  if (vt->pularOn) {
    vt->marcas = vt_marcas_episodio(imdb, temporada, episodio);
    if (vt->marcas) intro_marcas(imdb, temporada, episodio, &vt->origem);
  }
  return 1;
}

int vt_pular_ligado(void) {
  return ajustes_pular_abertura() || ajustes_pular_resumo() || ajustes_pular_creditos();
}

int vt_marcas_episodio(const char *imdb, int temporada, int episodio) {
  int origem;
  if (!imdb || !imdb[0]) return 0;
  return intro_marcas(imdb, temporada, episodio, &origem);
}

void vt_nome_amigavel(const VaiTocar *vt, char *dst, size_t n) {
  char res[40];
  snprintf(res, sizeof res, "%s%s%s", vt->res, vt->res[0] && vt->hdr[0] ? " " : "", vt->hdr);
  snprintf(dst, n, "%s%s%s%s%s", vt->tipo, vt->tipo[0] && res[0] ? " · " : "", res,
           (vt->tipo[0] || res[0]) && vt->audio[0] ? " · " : "", vt->audio);
  if (!dst[0]) snprintf(dst, n, "%s", vt->provedor);
}

const char *vt_origem_rotulo(int origem) {
  switch (origem) {
    case VT_O_THEINTRODB: return i18n("marcas do TheIntroDB");
    case VT_O_ANISKIP:    return i18n("marcas do AniSkip");
    case VT_O_CAPITULOS:  return i18n("capítulos do arquivo");
    case VT_O_MISTA:      return i18n("capítulos do arquivo + TheIntroDB");
    default:              return "";
  }
}
