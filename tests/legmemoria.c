// A LEGENDA ESCOLHIDA A MAO VOLTA (2.0.3). Dono, log da TCL (The Grand Tour,
// 29 legendas embutidas): escolheu Portugues a mao, saiu, retomou — e voltou
// sem legenda, sem nem uma linha "[legenda] automatica". A automatica (#129)
// so olhava ling_legenda() (Ajustes/conta), vazia nessa TV.
//
// Roda a decisao REAL de faixas.c (legendaAutomatica) com o video e os addons
// falsos, e prova:
//   1. pt escolhida a mao -> reabrir o mesmo titulo liga a MESMA faixa
//      (numero 22, nao a primeira pt, numero 21);
//   2. outro titulo com pt -> pt;
//   3. Ajustes com idioma -> Ajustes vence a ultima escolha;
//   4. desligada a mao -> reabrir nao tem nenhuma (nem a faixa padrao do arquivo);
//   5. a mesma faixa sumiu (outra fonte) -> o mesmo idioma;
//   6. legenda de addon escolhida a mao -> volta a mesma de addon;
//   7. #287: audio no idioma da ultima escolha -> so a forcada;
//   8. a memoria e do PERFIL e sobrevive a releitura do arquivo.
// Na e5b54bc4 (antes do conserto) o passo 1 ja falha: nada e ligado.
#include "../src/faixas.c"
#if __has_include("../src/legmemoria.c")
#include "../src/legmemoria.c"
#define TEM_LEGMEM 1
#else
#define TEM_LEGMEM 0
#endif
#include "../src/linguas.c"
#include "../src/legauto.c"
#include <assert.h>

// --- video falso ---------------------------------------------------------------
static VideoFaixa leg[4], aud[1];
static int nLeg, ativa = -1, nAud = 1;
int video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < nLeg ? &leg[i] : NULL; }
int video_n_audio(void) { return nAud; }
const VideoFaixa *video_audio(int i) { return i == 0 ? &aud[0] : NULL; }
int video_audio_atual(void) { return 0; }
int video_legenda_atual(void) { return ativa; }
void video_escolher_legenda(int i) { ativa = i; }
int video_mkv_sondado(void) { return 2; }      // Android: "nao ha sonda"
int video_legenda_ordinal_mkv(int i) { (void)i; return -1; }
const char *video_url_atual(void) { return "http://x/The.Grand.Tour.mkv"; }
void video_escolher_audio(int i) { (void)i; }
int mkvass_estado(void) { return 0; }
int mkvass_varredura(void) { return 0; }
void mkvass_parar(void) {}
void mkvass_iniciar_ordinal(const char *u, int o) { (void)u; (void)o; }
void legenda_desligar(void) {}
const char *i18n(const char *s) { return s; }

Uint32 SDL_GetTicks(void) { return 0; }
int faixasmkv_overlay(const char *c, int t) { (void)c; (void)t; return 0; }
void video_sondar_mkv_agora(void) {}
uint64_t legsync_hash_url(const char *u) { uint64_t h = 1469598103934665603ull; while (*u) h = (h ^ (unsigned char)*u++) * 1099511628211ull; return h; }
// Mesma identidade de legendasui.c: provedor + url.
void legendasui_id_addon(const Legenda *l, char out[24]) {
  snprintf(out, 24, "a%016llx", l ? (unsigned long long)(legsync_hash_url(l->provedor) ^ legsync_hash_url(l->url)) : 0ull);
}

// --- player / catalogo falsos ---------------------------------------------------
static CatItem item;
const CatItem *cat_item(int i) { (void)i; return &item; }
int player_indice(void) { return 0; }
int player_aberto(void) { return 1; }
int player_com_video(void) { return 1; }
const char *player_id_canal(void) { return ""; }
static int forcadaAuto;
int ajustes_legenda_forcada_auto(void) { return forcadaAuto; }

// --- addons falsos ----------------------------------------------------------------
static Legenda adds[2];
static int nAdds;
int addons_n_legendas(void) { return nAdds; }
const Legenda *addons_legenda(int i) { return i >= 0 && i < nAdds ? &adds[i] : NULL; }
int addons_legendas_prontas(void) { return 1; }
int addons_legendas_copiar(Legenda *v, int max, unsigned *a, int *b) {
  int i; (void)a; (void)b;
  for (i = 0; i < nAdds && i < max; i++) v[i] = adds[i];
  return i;
}
static char urlExterna[256];
void legsync_primaria_externa(const char *url, const char *idi, const char *prov) {
  (void)idi; (void)prov; snprintf(urlExterna, sizeof urlExterna, "%s", url);
}
void legsync_primaria_outra(int x) { (void)x; if (!x) urlExterna[0] = 0; }

// --- disco falso (dados.c) ---------------------------------------------------------
static char discoNome[4][48], *discoDado[4];
static int achar(const char *n, int criar) {
  int i;
  for (i = 0; i < 4; i++) if (discoDado[i] && !strcmp(discoNome[i], n)) return i;
  if (!criar) return -1;
  for (i = 0; i < 4; i++) if (!discoDado[i]) { snprintf(discoNome[i], sizeof discoNome[i], "%s", n); return i; }
  return -1;
}
int dados_gravar(const char *n, const char *c) {
  int i = achar(n, 1);
  if (i < 0) return 0;
  free(discoDado[i]); discoDado[i] = strdup(c);
  return 1;
}
char *dados_ler(const char *n) { int i = achar(n, 0); return i < 0 ? NULL : strdup(discoDado[i]); }
int dados_apagar(const char *n) { int i = achar(n, 0); if (i >= 0) { free(discoDado[i]); discoDado[i] = NULL; } return 1; }

// --- roteiro -------------------------------------------------------------------------
static void faixa(int i, const char *idi, int numero, int tipo) {
  memset(&leg[i], 0, sizeof leg[i]);
  snprintf(leg[i].idioma, sizeof leg[i].idioma, "%s", idi);
  snprintf(leg[i].rotulo, sizeof leg[i].rotulo, "%s %d", idi, numero);
  leg[i].numero = numero;
  leg[i].tipoLeg = tipo;
}
static void titulo(const char *imdb) {
  memset(&item, 0, sizeof item);
  snprintf(item.imdb, sizeof item.imdb, "%s", imdb);
  snprintf(item.titulo, sizeof item.titulo, "Titulo %s", imdb);
}
// Abre o titulo de novo: a TV comeca na faixa `padrao` do arquivo e a
// automatica roda alguns quadros.
static int abrir(int padrao) {
  Uint32 t;
  ativa = padrao; legExterna = -1; legExternaId[0] = 0; urlExterna[0] = 0;
  legAuto = 1; legAutoDesde = 0; aberta = 0;
  for (t = 1000; t < 1000 + 40000 && legAuto; t += 500) legendaAutomatica(t);
  return legExterna >= 0 ? legExterna : ativa;
}
static void grandTour(void) {
  nLeg = 3; nAdds = 0;
  faixa(0, "en", 3, LING_LEG_COMUM);
  faixa(1, "por", 21, LING_LEG_COMUM);   // "Portuguese" na TCL
  faixa(2, "pob", 22, LING_LEG_COMUM);
  snprintf(aud[0].idioma, sizeof aud[0].idioma, "en");
}

int main(void) {
  int r;
  forcadaAuto = 1;
#if TEM_LEGMEM
  legmem_definir_perfil(1);   // perfis.c faz isto no arranque
#endif
  grandTour();
  titulo("tt5712554:5:1");
  // Sem idioma em Ajustes nem na conta, como na TCL.
  assert(!ling_legenda()[0]);
  r = abrir(-1);
  assert(r == -1);   // nunca escolheu nada: comportamento de antes, nenhuma

  // 1. escolhe a pt-BR (faixa 2, numero 22) A MAO, sai, volta no mesmo titulo.
  aberta = 1; faixas_escolher_embutida(2); aberta = 0;
  r = abrir(-1);
  printf("mesmo titulo depois de escolher a faixa 2: %d\n", r);
  assert(r == 2);
  // o episodio seguinte da mesma serie: a mesma faixa.
  titulo("tt5712554:5:2");
  assert(abrir(-1) == 2);

  // 5. a mesma faixa sumiu (outra fonte, numeros diferentes): o mesmo idioma.
  faixa(1, "pob", 7, LING_LEG_COMUM); faixa(2, "en", 8, LING_LEG_COMUM);
  assert(abrir(-1) == 1);
  grandTour();

  // 2. outro titulo com pt: pt (a ultima escolha a mao).
  titulo("tt0111161");
  nLeg = 2; faixa(0, "en", 1, LING_LEG_COMUM); faixa(1, "por", 2, LING_LEG_COMUM);
  assert(abrir(-1) == 1);

  // 7. #287: audio em portugues -> so a forcada pt, nunca a inteira.
  titulo("tt0068646");
  nLeg = 3; faixa(0, "por", 1, LING_LEG_COMUM); faixa(1, "en", 2, LING_LEG_COMUM);
  faixa(2, "por", 3, LING_LEG_FORCADA);
  snprintf(aud[0].idioma, sizeof aud[0].idioma, "pt");
  assert(abrir(-1) == 2);
  nLeg = 2;   // sem forcada: nenhuma
  assert(abrir(-1) == -1);
  snprintf(aud[0].idioma, sizeof aud[0].idioma, "en");

  // 3. Ajustes com idioma vence a ultima escolha a mao (em outro titulo).
  ling_local_legenda("es");
  titulo("tt0109830");
  nLeg = 2; faixa(0, "por", 1, LING_LEG_COMUM); faixa(1, "spa", 2, LING_LEG_COMUM);
  assert(abrir(-1) == 1);
  ling_local_legenda("");

  // 6. legenda de ADDON escolhida a mao volta, a mesma (nao a primeira pt).
  titulo("tt0137523");
  nLeg = 0; nAdds = 2;
  snprintf(adds[0].idioma, sizeof adds[0].idioma, "pt"); snprintf(adds[0].url, sizeof adds[0].url, "http://s/a.srt");
  snprintf(adds[0].provedor, sizeof adds[0].provedor, "OpenSubtitles");
  snprintf(adds[1].idioma, sizeof adds[1].idioma, "pt"); snprintf(adds[1].url, sizeof adds[1].url, "http://s/b.srt");
  snprintf(adds[1].provedor, sizeof adds[1].provedor, "Subdl");
  aberta = 1; faixas_escolher_externa(&adds[1]); aberta = 0;
  r = abrir(-1);
  printf("addon escolhido a mao volta: %d (%s)\n", r, urlExterna);
  assert(r == 1 && !strcmp(urlExterna, "http://s/b.srt"));
  nAdds = 0;

  // 4. desligada a mao: reabrir nao tem nenhuma, nem a padrao do arquivo.
  grandTour();
  titulo("tt5712554:5:3");
  aberta = 1; faixas_escolher_embutida(-1); aberta = 0;
  r = abrir(0);
  printf("desligada a mao, arquivo abre na faixa 0: %d\n", r);
  assert(r == -1);
  // e em outro titulo tambem (a ultima escolha foi "nenhuma").
  titulo("tt0068646");
  nLeg = 2; faixa(0, "en", 1, LING_LEG_COMUM); faixa(1, "por", 2, LING_LEG_COMUM);
  assert(abrir(-1) == -1);

#if TEM_LEGMEM
  // 8. POR PERFIL, e relida do arquivo.
  titulo("tt0111161");
  aberta = 1; faixas_escolher_embutida(1); aberta = 0;   // pt no perfil 1
  legmem_definir_perfil(2);
  assert(!legmem_ultima()[0] && !legmem_do_titulo("tt0111161"));
  assert(abrir(-1) == -1);                               // o perfil 2 nunca escolheu
  legmem_definir_perfil(1);                              // relido do "disco"
  assert(!strcmp(legmem_ultima(), "por"));
  assert(abrir(-1) == 1);
  // A ordem da preferencia, pura.
  { int o; LegMem m; memset(&m, 0, sizeof m); snprintf(m.idioma, sizeof m.idioma, "es");
    assert(!strcmp(legmem_preferencia("en", &m, "pt", &o), "es") && o == LEGMEM_DE_TITULO);
    assert(!strcmp(legmem_preferencia("en", NULL, "pt", &o), "en") && o == LEGMEM_DE_AJUSTE);
    assert(!strcmp(legmem_preferencia("", NULL, "pt", &o), "pt") && o == LEGMEM_DE_ULTIMA);
    assert(!strcmp(legmem_preferencia("none", NULL, "pt", &o), "pt") && o == LEGMEM_DE_ULTIMA);
    assert(!strcmp(legmem_preferencia("none", NULL, "", &o), "none") && o == LEGMEM_DE_AJUSTE);
    assert(!legmem_preferencia("", NULL, "", &o)[0] && o == LEGMEM_DE_NADA); }
  legmem_esquecer();
  assert(!legmem_ultima()[0] && !dados_ler("legmemoria-p1.txt"));
#endif
  puts("legenda escolhida a mao volta: mesma faixa, mesmo idioma, nenhuma, Ajustes, addon, #287, perfil ok");
  return 0;
}
