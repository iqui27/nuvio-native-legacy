// Legenda temporaria, a cola de faixas.c: liga a legenda completa do idioma
// quando nao ha nenhuma, nao grava na memoria de escolhas, e so desliga a que
// ela mesma ligou (uma escolha a mao no meio toma a posse).
#include "../src/faixas.c"
#include <assert.h>

static VideoFaixa emb[4];
static int nEmb, ativa = -1, canal, trocas;
static const char *ultima = "";
static CatItem titulo;
static VideoFaixa audioEng = { .idioma = "eng" };

Uint32 SDL_GetTicks(void) { return 1000; }
const char *i18n(const char *s) { return s; }
int addons_n_legendas(void) { return 0; }
const Legenda *addons_legenda(int i) { (void)i; return NULL; }
const CatItem *cat_item(int i) { (void)i; return &titulo; }
int faixasmkv_overlay(const char *c, int t) { (void)c; (void)t; return 0; }
void legenda_desligar(void) {}
const char *legenda_idioma(const char *i, const char *a) { (void)a; return i; }
void legendasui_id_addon(const Legenda *l, char out[24]) { (void)l; out[0] = 0; }
const LegMem *legmem_do_titulo(const char *t) { (void)t; return NULL; }
void legmem_id_titulo(const char *id, char *dst, unsigned tam) { snprintf(dst, tam, "%s", id); }
const char *legmem_ultima(void) { return ultima; }
const char *legmem_preferencia(const char *aj, const LegMem *m, const char *ult, int *origem) {
  (void)m; if (origem) *origem = 0;
  return aj && aj[0] ? aj : ult;
}
uint64_t legsync_hash_url(const char *u) { (void)u; return 0; }
void legsync_primaria_externa(const char *u, const char *i, const char *o) { (void)u; (void)i; (void)o; }
void legsync_primaria_outra(int e) { (void)e; }
void mkvass_iniciar_ordinal(const char *u, int o) { (void)u; (void)o; }
void mkvass_parar(void) {}
int player_aberto(void) { return 1; }
int player_com_video(void) { return 1; }
const char *player_id_canal(void) { return canal ? "canal" : ""; }
int player_indice(void) { return 0; }
const VideoFaixa *video_audio(int i) { (void)i; return &audioEng; }
int video_audio_atual(void) { return 0; }
void video_escolher_legenda(int i) { ativa = i; trocas++; }
int video_n_legenda(void) { return nEmb; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < nEmb ? &emb[i] : NULL; }
int video_legenda_atual(void) { return ativa; }
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
int video_mkv_sondado(void) { return 1; }
void video_sondar_mkv_agora(void) {}
const char *video_url_atual(void) { return ""; }
int xtream_url_limitada(const char *u) { (void)u; return 0; }

static void faixa(int i, const char *idioma, int tipo) {
  memset(&emb[i], 0, sizeof emb[i]);
  snprintf(emb[i].idioma, sizeof emb[i].idioma, "%s", idioma);
  emb[i].tipoLeg = tipo;
}

int main(void) {
  snprintf(titulo.imdb, sizeof titulo.imdb, "tt0000001");
  faixa(0, "eng", LING_LEG_COMUM); faixa(1, "por", LING_LEG_FORCADA); faixa(2, "por", LING_LEG_COMUM);
  nEmb = 3;

  // A escolha automatica do comeco ainda roda: espera, sem mexer em nada.
  legAuto = 1;
  assert(faixas_legenda_temporaria(1, "pt") == 0 && ativa == -1);
  legAuto = 0;

  // Sem preferencia: o idioma do app. A FORCADA em portugues nao serve, a
  // completa sim. Desligar devolve ao que era (sem legenda).
  assert(faixas_legenda_temporaria(1, "pt") == 1 && ativa == 2);
  assert(faixas_legenda_temporaria(1, "pt") == -1 && ativa == 2);   // ja ligada: nada
  assert(faixas_legenda_temporaria(0, "") == 1 && ativa == -1);
  assert(faixas_legenda_temporaria(0, "") == -1);                   // nada dela para desligar

  // A pessoa desligou a mao por ultimo ("none"): a temporaria liga mesmo assim.
  ultima = "none";
  assert(faixas_legenda_temporaria(1, "pt") == 1 && ativa == 2);
  assert(faixas_legenda_temporaria(0, "") == 1 && ativa == -1);
  ultima = "";

  // Preferencia de Ajustes vence o idioma do app.
  ling_local_legenda("eng");
  assert(faixas_legenda_temporaria(1, "pt") == 1 && ativa == 0);
  // Escolha a mao no meio (outra faixa): a posse passa para a pessoa e o
  // "desligar" da temporaria nao apaga a legenda dela.
  escolherLegenda(2);
  assert(ativa == 2);
  assert(faixas_legenda_temporaria(0, "") == -1 && ativa == 2);
  escolherLegenda(-1);
  ling_local_legenda("");

  // Ja havia legenda ligada (quem assiste com legenda): nada muda, nem ao desligar.
  escolherLegenda(0);
  trocas = 0;
  assert(faixas_legenda_temporaria(1, "pt") == -1);
  assert(faixas_legenda_temporaria(0, "") == -1 && ativa == 0 && trocas == 0);
  escolherLegenda(-1);

  // Nenhum idioma serve (app em japones, audio em ingles so com forcada): cai
  // no idioma do audio; sem faixa completa nele, nada.
  assert(faixas_legenda_temporaria(1, "ja") == 1 && ativa == 0);    // audio eng
  assert(faixas_legenda_temporaria(0, "") == 1);
  faixa(0, "eng", LING_LEG_FORCADA);
  assert(faixas_legenda_temporaria(1, "ja") == -1 && ativa == -1);

  // Canal ao vivo: fora.
  canal = 1;
  assert(faixas_legenda_temporaria(1, "pt") == -1 && ativa == -1);
  canal = 0;

  // Titulo novo (faixas_reiniciar) esquece a posse.
  faixa(0, "eng", LING_LEG_COMUM);
  legAuto = 0;
  assert(faixas_legenda_temporaria(1, "en") == 1 && legTempDona);
  legTempDona = 0; ativa = -1;

  puts("legtemp_faixas: ok");
  return 0;
}
