// Server navigation uses the existing grid; no network or renderer needed.
#include "../src/vertudo.c"
#include <assert.h>

static CatItem items[3];
static int count, requested, added;
static char requestedKey[96];

int servidores_bibliotecas(CatFileira *out, int max) {
  const char *keys[] = {"jellyfin_movies", "jellyfin_collections", "emby_playlists", "plex_collections", "plex_playlists"};
  assert(max >= 5);
  for (int i = 0; i < 5; i++) {
    memset(out + i, 0, sizeof *out);
    snprintf(out[i].chave, sizeof out[i].chave, "%s", keys[i]);
    snprintf(out[i].titulo, sizeof out[i].titulo, "Fixture %d", i);
    snprintf(out[i].tipo, sizeof out[i].tipo, "%s", i ? "group" : "movie");
  }
  return 5;
}
int desc_vertudo_n(void) { return count; }
int desc_vertudo_item(int i, CatItem *out) {
  if (i < 0 || i >= count) return 0;
  *out = items[i]; return 1;
}
int desc_vertudo_fim(void) { return 1; }
int desc_vertudo_erro(void) { return 0; }
void desc_vertudo_mais(void) {}
void desc_vertudo_filtro(const char *base, const char *type, const char *key, const char *genre) {
  (void)type; (void)genre;
  assert(!strcmp(base, "servidores"));
  snprintf(requestedKey, sizeof requestedKey, "%s", key);
  requested++;
}
void desc_vertudo_fonte(const ColSource *s) { (void)s; assert(0); }
const char *desc_chave_tmdb(void) { return ""; }
const char *nuvem_trakt_cliente(void) { return ""; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *i18n(const char *s) { return s; }
int servidores_info(const char *id, const char *type, SrvAchado *out) {
  (void)type;
  memset(out, 0, sizeof *out);
  out->altura = !strcmp(id, "tt1") ? 2160 : 1080;
  out->hdr = !strcmp(id, "tt1");
  out->visto = !strcmp(id, "tt2");
  return !jfid_e(id);
}
void desc_pedir_titulo_tmdb(long id, const char *type) { (void)id; (void)type; assert(0); }
int cat_indice_por_imdb(const char *id) { (void)id; return -1; }
int cat_acrescentar(const CatItem *it) { (void)it; added++; return 7; }

static void key(SDL_Keycode code) {
  SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = code;
  vertudo_evento(&e);
}
static void seed(const char *id, const char *type) {
  memset(items, 0, sizeof items);
  snprintf(items[0].imdb, sizeof items[0].imdb, "%s", id);
  snprintf(items[0].tipo, sizeof items[0].tipo, "%s", type);
  count = 1; servidorLidos = -1; servidorOrdenar();
}
int main(void) {
  vertudo_servidores("jellyfin_movies");
  assert(servidor.nSources == 5 && !strcmp(requestedKey, "jellyfin_movies"));
  seed("tt1", "movie"); tabFocus = 2;
  for (int i = 0; i < 5; i++) key(SDLK_RETURN);
  assert(servidorFiltro == 5 && servidorN == 1);
  key(SDLK_UP); key(SDLK_RIGHT); key(SDLK_RETURN);
  seed("jf:12345678.abcdef", "group");
  if (servidorN != 1) {
    fprintf(stderr, "FAIL: 4K library filter hides collection folders after tab switch\n");
    return 1;
  }
  key(SDLK_RETURN);
  assert(!strcmp(requestedKey, "jellyfin_abcdef") && !added);
  key(SDLK_ESCAPE);
  assert(vertudo_aberta() && !strcmp(requestedKey, "jellyfin_collections"));
  key(SDLK_ESCAPE); assert(!vertudo_aberta());

  const char *roots[] = {"emby_playlists", "plex_collections", "plex_playlists"};
  const char *ids[] = {"em:12345678.abcdef", "px:12345678.123", "px:12345678.456"};
  const char *children[] = {"emby_list_abcdef", "plex_group_123", "plex_list_456"};
  for (int i = 0; i < 3; i++) {
    vertudo_servidores(roots[i]); tabFocus = 0;
    seed(ids[i], i == 1 ? "group" : "list"); key(SDLK_RETURN);
    assert(!strcmp(requestedKey, children[i]) && !added);
    key(SDLK_BACKSPACE);
    assert(vertudo_aberta() && !strcmp(requestedKey, roots[i]));
    seed(i ? "px:12345678.bad:1:2" : "em:12345678.bad:1:2", "list");
    int before = requested; key(SDLK_RETURN); assert(requested == before && !added);
  }
  vertudo_servidores("jellyfin_movies"); tabFocus = 0;
  seed("tt1", "movie"); key(SDLK_RETURN);
  assert(added == 1 && vertudo_pediu_abrir() == 7);
  puts("servers view: filters, collections, playlists, Back, invalid IDs, canonical detail: OK");
  return 0;
}
