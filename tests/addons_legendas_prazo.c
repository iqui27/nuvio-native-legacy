// Selecionador real, alimentado pelo worker de addons_legendas.c e relogio virtual.
#include "../src/faixas.c"
#include "../src/legmemoria.c"
#include "../src/legauto.c"
#include <assert.h>

static CatItem item;
static VideoFaixa audio;
static char selecionada[1024];
int video_n_audio(void) { return 1; }
const VideoFaixa *video_audio(int i) { return i == 0 ? &audio : NULL; }
int video_audio_atual(void) { return 0; }
int video_n_legenda(void) { return 0; }
const VideoFaixa *video_legenda(int i) { (void)i; return NULL; }
int video_legenda_atual(void) { return -1; }
void video_escolher_legenda(int i) { (void)i; }
void video_escolher_audio(int i) { (void)i; }
int video_mkv_sondado(void) { return 2; }
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
const char *video_url_atual(void) { return "https://cdn.fixture.invalid/opaque"; }
int mkvass_estado(void) { return 0; }
int mkvass_varredura(void) { return 0; }
void mkvass_parar(void) {}
void mkvass_iniciar_ordinal(const char *u, int o) { (void)u; (void)o; }
void legenda_desligar(void) {}
void video_sondar_mkv_agora(void) {}
int faixasmkv_overlay(const char *c, int t) { (void)c; (void)t; return 0; }
int player_aberto(void) { return 1; }
int player_com_video(void) { return 1; }
int player_indice(void) { return 0; }
const char *player_id_canal(void) { return ""; }
const CatItem *cat_item(int i) { (void)i; return &item; }
int ajustes_legenda_forcada_auto(void) { return 0; }
Uint32 SDL_GetTicks(void) { return 0; }
void legsync_primaria_externa(const char *u, const char *i, const char *p) {
  (void)i; (void)p; snprintf(selecionada, sizeof selecionada, "%s", u);
}
void legsync_primaria_outra(int i) { (void)i; }
uint64_t legsync_hash_url(const char *u) { uint64_t h = 5381; while (*u) h = h * 33 + (unsigned char)*u++; return h; }
void legendasui_id_addon(const Legenda *l, char out[24]) { snprintf(out, 24, "a%016llx", (unsigned long long)legsync_hash_url(l->url)); }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *c) { (void)n; (void)c; return 1; }
int dados_apagar(const char *n) { (void)n; return 1; }

void prazo_auto_iniciar(void) {
  snprintf(item.imdb, sizeof item.imdb, "tt123");
  snprintf(audio.idioma, sizeof audio.idioma, "en");
  selecionada[0] = 0; legExterna = -1; legExternaId[0] = 0;
  legAuto = 1; legAutoDesde = 1; aberta = 0;
}
void prazo_auto_avancar(unsigned ms) { legendaAutomatica(ms); }
int prazo_auto_selecionou(void) { return legExterna >= 0 && selecionada[0] && !legAuto; }
void teste_merge_escolher(const Legenda *l) { faixas_escolher_externa(l); }
int teste_merge_selecionada(const Legenda *l, int indice) {
  char id[24]; legendasui_id_addon(l, id);
  return faixas_legenda_ativa() == indice && !strcmp(faixas_legenda_externa_id(), id) &&
         !strcmp(selecionada, l->url) && !legAuto;
}
