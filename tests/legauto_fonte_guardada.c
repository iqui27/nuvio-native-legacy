// Retomar real de app.c: fonte fora da lista conserva o release e os extras.
#define fontevolta_conferir teste_conferir
#include "../src/app.c"
#include "stub_m3u.h"
#undef fontevolta_conferir
#include "../src/legauto.c"
#include <assert.h>

static Stream guardada, normal;
static CatItem titulo;
static int abertoTeste = 1, comVideo, indiceFonte = 0, chamadasExtras;
static const char *canal = "";
static char urlAberta[4096], nomeExtras[512];
Uint32 SDL_GetTicks(void) { return 1000; }
int player_aberto(void) { return abertoTeste; }
int player_com_video(void) { return comVideo; }
const char *player_id_canal(void) { return canal; }
int player_indice(void) { return 0; }
const CatItem *cat_item(int i) { (void)i; return &titulo; }
void player_episodio_atual(int *t, int *e) { *t = *e = 0; }
int cat_id_stream(int i, int t, int e, char *out, unsigned n) {
  (void)i; (void)t; (void)e; snprintf(out, n, "%s", titulo.imdb); return 1;
}
const char *sessao_usuario(void) { return "fixture"; }
int perfis_ativo(void) { return 1; }
int fontevolta_pegar(const char *alvo, const char *conta, int perfil, Uint32 agora, Stream *s) {
  assert(!strcmp(alvo, titulo.imdb) && !strcmp(conta, "fixture") && perfil == 1 && agora == 1000);
  *s = guardada; return 1;
}
void teste_conferir(const char *u, const char *c) { (void)u; (void)c; }
void marco(const char *s) { (void)s; }
void stream_definir_atual(int i) { indiceFonte = i; }
int stream_atual(void) { return indiceFonte; }
const Stream *stream_item(int i) { return i == 0 ? &normal : NULL; }
int stream_e_mp4(const Stream *s) { (void)s; return 0; }
void video_definir_dv(int dv) { (void)dv; }
void video_definir_cabecalhos(const char *c) { (void)c; }
void video_definir_mp4(int mp4) { (void)mp4; }
void player_definir_fonte(const char *u) { snprintf(urlAberta, sizeof urlAberta, "%s", u); }
void player_definir_tentativa(int n, int max) { (void)n; (void)max; }
void addons_legendas_fonte(const char *id, const char *tipo, const char *arq,
                          unsigned long long tam, const char *hash, const char *url, int cab) {
  assert(!strcmp(id, titulo.imdb) && !strcmp(tipo, "movie"));
  assert(tam == guardada.tamanhoBytes && !strcmp(hash, guardada.videoHash));
  assert(!strcmp(url, urlAberta) && !cab);
  snprintf(nomeExtras, sizeof nomeExtras, "%s", arq); chamadasExtras++;
}
uint64_t legsync_hash_url(const char *u) { uint64_t h = 5381; while (*u) h = h * 33 + (unsigned char)*u++; return h; }
const char *i18n(const char *s) { return s; }

int main(void) {
  snprintf(titulo.imdb, sizeof titulo.imdb, "tt205");
  snprintf(titulo.tipo, sizeof titulo.tipo, "movie");
  snprintf(guardada.url, sizeof guardada.url, "https://cdn.invalid/opaque");
  snprintf(guardada.arquivo, sizeof guardada.arquivo, "Show.WEB-DL.NTb.mkv");
  guardada.tamanhoBytes = 1000000;
  snprintf(guardada.videoHash, sizeof guardada.videoHash, "0123456789abcdef");
  assert(tocarFonteGuardada() && indiceFonte == -1);
  legendasDaFonte(); // antes do primeiro quadro
  assert(!strcmp(legauto_midia(urlAberta), guardada.arquivo));
  Legenda v[2] = {0};
  for (int i = 0; i < 2; i++) {
    snprintf(v[i].idioma, sizeof v[i].idioma, "pt");
    snprintf(v[i].url, sizeof v[i].url, "https://subs.invalid/%d.srt", i);
  }
  snprintf(v[0].arquivo, sizeof v[0].arquivo, "Show.HDTV.LOL.srt");
  snprintf(v[1].arquivo, sizeof v[1].arquivo, "Show.WEB-DL.NTb.srt");
  assert(legauto_escolher(v, 2, "pt", legauto_midia(urlAberta), NULL, 0, 0) == 1);
  voltaAtiva = 0; comVideo = 1; legendasDaFonte(); // watchdog ja terminou
  assert(!strcmp(legauto_midia(urlAberta), guardada.arquivo));
  assert(chamadasExtras == 1 && !strcmp(nomeExtras, guardada.arquivo));
  legendasDaFonte(); assert(chamadasExtras == 1);
  snprintf(normal.arquivo, sizeof normal.arquivo, "Other.BluRay.mkv");
  indiceFonte = 0; comVideo = 0; legendasDaFonte();
  assert(!strcmp(legauto_midia(urlAberta), normal.arquivo));
  indiceFonte = -1; limparFonteVOD(); legendasDaFonte();
  assert(!strcmp(legauto_midia(urlAberta), urlAberta));
  indiceFonte = 0; abertoTeste = 0; legendasDaFonte();
  assert(!strcmp(legauto_midia(urlAberta), urlAberta));
  abertoTeste = 1; canal = "channel"; legendasDaFonte();
  assert(!strcmp(legauto_midia(urlAberta), urlAberta));
  puts("legauto: Retomar conserva release/extras, troca e fechamento limpam a fonte");
}
