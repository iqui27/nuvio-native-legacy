// #503: the actual recommendation seed, metadata parser and detail-art helpers.
// The script extracts complete production functions; only their service and
// Settings boundaries are supplied here. No network, texture or renderer runs.
#include "catalogo.h"
#include "artehero.h"
#include "js.h"
#include "idiomacod.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static uint64_t sobSessao;
static uint64_t nv_perfil_sessao_ler(void) { return 1; }
static CatItem probe, sobSemente;
static int idx, arteFixaPoster, sobFioVivo, sobIndice;
static char arteFixa[NV_ARTE_URL_MAX];
static const char *custom, *preview;
static int fundoAddon, logoAddon;
static const char *arteDeViva(int i);
static const char *poster = "https://image.tmdb.org/t/p/w342/recommendation-poster.jpg";
static const char *landscape = "https://image.tmdb.org/t/p/w1280/full-backdrop.jpg";

const CatItem *cat_item(int i) { return i == 0 ? &probe : NULL; }
int cat_indice_por_imdb(const char *id) { (void)id; return -1; }
int cat_indice_por_imdb_tipo(const char *id, const char *tipo) { (void)tipo; return cat_indice_por_imdb(id); }
static int descCatAcrescentar(const CatItem *c) { probe = *c; return 0; }
// These asynchronous service paths must not run for a complete IMDb seed.
void desc_pedir_titulo(const char *id) { (void)id; abort(); }
void desc_pedir_titulo_tmdb(long id, const char *tipo) { (void)id; (void)tipo; abort(); }
static void pedirTmdb(long id, const char *tipo, int semente) {
  (void)id; (void)tipo; (void)semente; abort();
}
const char *i18n(const char *s) { return s; }
int ajustes_idioma_ingles(void) { return 1; }
int ajustes_idioma(void) { return IDIOMA_EN; }
int ajustes_fundo_addon(void) { return fundoAddon; }
int ajustes_logo_addon(void) { return logoAddon; }
int ajustes_hero_fonte(void) { return ARTEHERO_AUTO; }
int ajustes_hero_arte_diferente(void) { return 0; }
const char *trocaarte_previa_fundo(void) { return preview; }
int tex_falhou(const char *url) { (void)url; return 0; }
#define DESC_URL_LIDA (NV_ARTE_URL_MAX + 1)
#include "arte_recomendacao.inc"

static int checks, failures;
static void verify(int ok, const char *what) {
  checks++;
  if (!ok) failures++;
  printf("%s  %s\n", ok ? "ok" : "FAIL", what);
}
static const char *customArt(const char *id) {
  return !strcmp(id, probe.imdb) ? custom : NULL;
}
static void reset(const char *backdrop) {
  memset(&probe, 0, sizeof probe);
  sobFioVivo = 0; sobIndice = -1;
  desc_pedir_titulo_semente("tt9999901", 0, "movie", "Recommended title", "2026", poster);
  assert(sobIndice == 0 && !sobFioVivo && !probe.backdrop[0]);
  assert(!strcmp(probe.poster, poster));
  assert(!strcmp(probe.imdb, "tt9999901") && !strcmp(probe.tipo, "movie"));
  if (backdrop) snprintf(probe.backdrop, sizeof probe.backdrop, "%s", backdrop);
  snprintf(probe.backdropCatalogo, sizeof probe.backdropCatalogo, "%s", probe.backdrop);
  idx = 0; arteFixa[0] = 0; arteFixaPoster = 0;
  custom = preview = NULL; fundoAddon = logoAddon = 0;
  artehero_definir_escolha(NULL, NULL); artehero_definir_falhou(NULL); artehero_qualidade(1);
}
static const char *metadata(const char *backdrop) {
  static char json[4096];
  snprintf(json, sizeof json,
    "{\"meta\":{\"id\":\"tt9999901\",\"name\":\"Recommended title\",\"type\":\"movie\","
    "\"poster\":\"%s\",\"background\":\"%s\",\"description\":\"Remote synopsis\","
    "\"releaseInfo\":\"2030\",\"imdbRating\":8.7,\"moviedb_id\":987,"
    "\"logo\":\"https://example.invalid/remote-logo.png\"}}", poster, backdrop);
  return json;
}
static void existingText(void) {
  snprintf(probe.sinopse, sizeof probe.sinopse, "Existing synopsis");
  snprintf(probe.logo, sizeof probe.logo, "https://example.invalid/existing-logo.png");
  snprintf(probe.classificacao, sizeof probe.classificacao, "14");
  probe.tmdb = 123; probe.nota = 95;
}

int main(void) {
  char initial[NV_ARTE_URL_MAX], after[NV_ARTE_URL_MAX];
  reset(poster);
  existingText();
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  int classified = arteFixaPoster;
  completarRaso(&probe, metadata(landscape), "movie");
  snprintf(after, sizeof after, "%s", arteDe(0));
  verify(classified, "recommendation poster alias is temporary portrait artwork");
  verify(!strcmp(probe.backdrop, landscape), "synopsis-ready seed receives its real metadata backdrop");
  verify(!strcmp(after, landscape), "recommendation seed and late metadata upgrade the visible detail art");
  verify(!arteDetalheEhPoster(0), "upgraded landscape uses backdrop layout");

  reset(poster);
  verify(arteDetalheEhPoster(0), "poster classification works before the first drawing call");
  verify(!strcmp(arteDe(0), poster), "poster alias remains stable while metadata is pending");
  verify(arteDetalheEhPoster(0), "pending poster uses contained layout instead of stretched cover");

  reset(landscape);
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  snprintf(probe.backdrop, sizeof probe.backdrop, "https://example.invalid/different-photo.jpg");
  verify(!strcmp(arteDe(0), initial), "genuine opening backdrop stays frozen after later enrichment");
  verify(!arteDetalheEhPoster(0), "real landscape is never classified as a temporary poster");

  reset(poster);
  custom = "https://example.invalid/chosen-landscape.jpg";
  artehero_definir_escolha(customArt, NULL);
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  verify(!strcmp(initial, custom), "explicit custom artwork wins over the recommendation seed");
  verify(!arteDetalheEhPoster(0), "custom landscape classification follows the selected URL");
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!strcmp(arteDe(0), custom), "late metadata cannot replace user-chosen artwork");

  reset("");
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  verify(strstr(initial, "images.metahub.space/background/") != NULL,
         "empty backdrop retains the existing IMDb fallback policy");
  verify(!arteDetalheEhPoster(0), "fallback landscape is not classified from an unrelated poster field");

  reset(poster);
  existingText();
  CatItem expected = probe;
  completarRaso(&probe, metadata(landscape), "movie");
  snprintf(expected.backdrop, sizeof expected.backdrop, "%s", landscape);
  snprintf(expected.backdropCatalogo, sizeof expected.backdropCatalogo, "%s", landscape);
  snprintf(expected.backdropTmdb, sizeof expected.backdropTmdb, "%s", landscape);
  verify(!memcmp(&probe, &expected, sizeof probe),
         "art-only completion preserves existing synopsis, logo, metadata, identity and poster");

  reset(landscape); existingText(); expected = probe;
  completarRaso(&probe, metadata("https://example.invalid/another-backdrop.jpg"), "movie");
  verify(!memcmp(&probe, &expected, sizeof probe), "metadata cannot replace an existing genuine backdrop");

  reset(poster); existingText();
  snprintf(probe.origem, sizeof probe.origem, "org.addon");
  snprintf(probe.backdropCatalogo, sizeof probe.backdropCatalogo, "https://example.invalid/addon-art.jpg");
  fundoAddon = 1; expected = probe;
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!memcmp(&probe, &expected, sizeof probe), "enabled addon artwork preference protects its existing image");
  fundoAddon = 0;
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!strcmp(probe.backdrop, landscape), "disabled addon artwork preference permits missing-art completion");

  reset(poster); existingText(); expected = probe;
  completarRaso(&probe, metadata(""), "movie");
  verify(!memcmp(&probe, &expected, sizeof probe), "another poster-only metadata response does not replace the seed");
  completarRaso(&probe, "{}", "movie");
  verify(!memcmp(&probe, &expected, sizeof probe), "missing metadata leaves the existing title unchanged");

  reset(""); existingText();
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!strcmp(probe.backdrop, landscape), "text-ready title with no backdrop receives the missing landscape");

  reset(poster);
  completarRaso(&probe, metadata(""), "movie");
  verify(!strcmp(probe.sinopse, "Remote synopsis") && !strcmp(probe.backdrop, poster),
         "first metadata response can supply text while artwork remains a poster seed");
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!strcmp(initial, poster) && !strcmp(arteDe(0), landscape),
         "a later metadata provider completes artwork after synopsis was already filled");
  verify(!strcmp(probe.sinopse, "Remote synopsis") && probe.tmdb == 987 && probe.nota == 87,
         "late artwork preserves metadata filled by the first provider");

  reset(poster);
  snprintf(probe.origem, sizeof probe.origem, "org.addon");
  snprintf(probe.logo, sizeof probe.logo, "https://example.invalid/addon-logo.png");
  logoAddon = 1;
  completarRaso(&probe, metadata(landscape), "movie");
  verify(!strcmp(probe.logo, "https://example.invalid/addon-logo.png") &&
         !strcmp(probe.sinopse, "Remote synopsis"), "existing addon logo remains protected during shallow completion");

  reset(poster);
  snprintf(initial, sizeof initial, "%s", arteDe(0));
  snprintf(probe.poster, sizeof probe.poster, "https://example.invalid/new-poster.jpg");
  verify(!strcmp(arteDe(0), initial) && arteDetalheEhPoster(0),
         "replaced poster metadata cannot reclassify the frozen old portrait as a landscape");
  preview = "https://example.invalid/preview.jpg";
  verify(!strcmp(arteDe(0), preview) && !arteDetalheEhPoster(0), "artwork preview still overrides frozen art as a backdrop");

  printf("arte_recomendacao: %d checks %s\n", checks, failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
