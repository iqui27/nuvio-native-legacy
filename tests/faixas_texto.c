// #421: a escolha real mantem a TV durante a colheita; ASS nao muda.
#include "../src/faixas.c"
#include <assert.h>
int xtream_url_limitada(const char *url) { (void)url; return 0; }
static VideoFaixa faixa;
static int nativa, ordinal;
const VideoFaixa *video_legenda(int i) { assert(i == 2); return &faixa; }
void video_escolher_legenda(int i) { nativa = i; }
const char *video_url_atual(void) { return "https://example.test/texto.mkv"; }
void mkvass_iniciar_ordinal(const char *url, int ord) { assert(*url); ordinal = ord; }

// Handoff do 1o cue (#8 revisao): faixa de TEXTO nao pode tostar "o app
// voltou a desenhar" — o app nunca tinha deixado de desenhar. O toast so
// existe quando houve falha de verdade antes (a TV estava no fallback).
static int toasts, colhidos;
static VideoLegendaEstilo estilo;
VideoLegendaEstilo *player_leg_estilo(void) { return &estilo; }
void player_toast(const char *texto, unsigned ms) { (void)texto; (void)ms; toasts++; }
unsigned legenda_geracao(void) { return 0; }
int legenda_ligada_em(unsigned geracao) { (void)geracao; return 1; }
int mkvass_nogo(void) { return 0; }
int mkvass_estado(void) { return MKVASS_COMPLETO; }
void mkvass_estatisticas(long *pedidos, long *bytes, int *col, int *total) {
  if (pedidos) *pedidos = 0; if (bytes) *bytes = 0;
  if (col) *col = colhidos; if (total) *total = colhidos;
}
// O resto do grafo de faixas_atualizar: player_aberto()==0 faz
// legendaAutomatica/pilAtualizar sairem no primeiro if, e o no-go/retomar
// nunca abrem com estes stubs. So precisam existir para o link.
Uint32 SDL_GetTicks(void) { return 1000; }
const char *i18n(const char *s) { return s; }
int player_aberto(void) { return 0; }
int player_com_video(void) { return 0; }
const char *player_id_canal(void) { return ""; }
int player_indice(void) { return 0; }
void player_toast_ex(const char *t, unsigned ms, const char *ic, int am) { (void)t; (void)ms; (void)ic; (void)am; }
const CatItem *cat_item(int i) { (void)i; return NULL; }
int addons_n_legendas(void) { return 0; }
const Legenda *addons_legenda(int i) { (void)i; return NULL; }
int addons_legendas_prontas(void) { return 0; }
int addons_legendas_copiar(Legenda *dst, int max, unsigned *g, int *p) { (void)dst; (void)max; (void)g; (void)p; return 0; }
int ajustes_legenda_forcada_auto(void) { return 0; }
void legenda_desligar(void) {}
void legenda_pre_descartar(const char *motivo) { (void)motivo; }
void legendasui_atualizar(float dt) { (void)dt; }
void legendasui_id_addon(const Legenda *l, char out[24]) { (void)l; out[0] = 0; }
const char *ling_legenda(void) { return ""; }
int ling_casa(const char *codigo, const char *pref) { (void)codigo; (void)pref; return 0; }
int ling_legenda_auto_tipo(const char *pref, const char *audio, int forcadaAuto,
                           const char *const *emb, const int *tipoEmb, int nEmb, int embFechado,
                           const char *const *add, int nAdd, int addFechado) {
  (void)pref; (void)audio; (void)forcadaAuto; (void)emb; (void)tipoEmb; (void)nEmb;
  (void)embFechado; (void)add; (void)nAdd; (void)addFechado; return LING_AUTO_NADA;
}
int legauto_escolher(const Legenda *v, int n, const char *pref, const char *midia,
                     const uint64_t *excl, int nExcl, uint64_t lembrada) {
  (void)v; (void)n; (void)pref; (void)midia; (void)excl; (void)nExcl; (void)lembrada; return -1;
}
void legauto_lembrar(const char *titulo, const char *idioma, uint64_t hash) { (void)titulo; (void)idioma; (void)hash; }
uint64_t legauto_lembrada(const char *titulo, const char *idioma) { (void)titulo; (void)idioma; return 0; }
const LegMem *legmem_do_titulo(const char *titulo) { (void)titulo; return NULL; }
void legmem_id_titulo(const char *id, char *dst, unsigned tam) { (void)id; (void)dst; (void)tam; }
const char *legmem_ultima(void) { return ""; }
const char *legmem_preferencia(const char *ajuste, const LegMem *doTitulo, const char *ultima, int *origem) {
  (void)doTitulo; (void)ultima; *origem = 0; return ajuste;
}
int legmem_exata(const LegMem *m, const int *numeros, const char *const *idiomas, const char *const *nomes, int nEmb,
                 const char *const *idsAdd, int nAdd) {
  (void)m; (void)numeros; (void)idiomas; (void)nomes; (void)nEmb; (void)idsAdd; (void)nAdd; return -1;
}
unsigned legsync_manual_seq(void) { return 0; }
uint64_t legsync_hash_url(const char *url) { (void)url; return 0; }
void legsync_primaria_externa(const char *url, const char *idioma, const char *origem) { (void)url; (void)idioma; (void)origem; }
void legsync_primaria_outra(int embutida) { (void)embutida; }
LegSyncVisao legsync_visao(int slot) { (void)slot; LegSyncVisao v; memset(&v, 0, sizeof v); return v; }
int legsync_pil_passo(LegSyncPil *p, const LegSyncVisao *v, unsigned agora, const char *provedor, char *texto, unsigned tam) {
  (void)p; (void)v; (void)agora; (void)provedor; (void)texto; (void)tam; return 0;
}
void mkvass_parar(void) {}
const VideoFaixa *video_audio(int i) { (void)i; return NULL; }
int video_audio_atual(void) { return 0; }
int video_legenda_atual(void) { return -1; }
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
int video_mkv_sondado(void) { return 0; }
int video_n_audio(void) { return 0; }
int video_n_legenda(void) { return 0; }
void video_sondar_mkv_agora(void) {}
int video_velocidade_bloqueada(void) { return 0; }
int video_velocidade_atual(void) { return VEL_NORMAL; }
int video_velocidade_suportada(void) { return 1; }

int main(void) {
  strcpy(faixa.codec, "S_TEXT/UTF8");
#ifdef NV_ANDROID
  assert(!vaiOverlay(&faixa)); // Media3 ja fornece o texto.
#else
  assert(vaiOverlay(&faixa));
  overlayAssumir(2, 7);
  assert(nativa == 2 && ordinal == 7 && legOverlayTV && legOverlay == 2);
  assert(legOverlayFalhas == 0 && legOverlayRetomar == 0);
  strcpy(faixa.codec, "S_TEXT/WEBVTT"); assert(vaiOverlay(&faixa));
#endif
  strcpy(faixa.codec, "S_TEXT/ASS");
  assert(vaiOverlay(&faixa)); overlayAssumir(2, 7);
  assert(nativa == -1 && !legOverlayTV);
  strcpy(faixa.codec, "S_HDMV/PGS");
#ifdef NV_TPK
  // .tpk decodifica PGS em software: vai para o overlay, sem segurar a TV.
  assert(vaiOverlay(&faixa)); overlayAssumir(2, 7);
  assert(nativa == -1 && !legOverlayTV);
#else
  assert(!vaiOverlay(&faixa));
#endif
#ifndef NV_ANDROID
  // 1o cue da faixa de texto: nativa desligada (handoff), SEM toast.
  strcpy(faixa.codec, "S_TEXT/UTF8"); overlayAssumir(2, 7); toasts = 0;
  colhidos = 3; faixas_atualizar(1.f / 60, 1000);
  assert(nativa == -1 && !legOverlayTV && legOverlayFalhas == 0);
  assert(toasts == 0);
  // Fallback por falha e retomada: AGORA o toast "voltou a desenhar".
  legOverlayTV = 1; legOverlayFalhas = 2; legOverlayColhidos = 3;
  colhidos = 5; faixas_atualizar(1.f / 60, 2000);
  assert(nativa == -1 && !legOverlayTV && toasts == 1);
#endif
  puts("faixas_texto: escolha, handoff sem toast e toast so apos falha ok");
}
