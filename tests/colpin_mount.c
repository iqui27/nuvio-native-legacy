// #700 root: pinned collection folders on the SAME base/type/catalog with
// different genres must reach the Home with their OWN genre window.
// Real montar() (descoberta.c via montagem_cedo.c), real catalogo.c,
// colecoes.c and homefil.c pin state. Only transport/account are fixtures.
//
//   bash tests/colpin_mount.sh
#define main montagem_cedo_original_main
#include "montagem_cedo.c"
#undef main
#include "homefil.h"

// fileiras.c row-order edits homefil.c can reach; montagem_cedo's fil_oculta
// is always 0, so pinning never moves/unhides rows here.
void fil_alternar(int i) { (void)i; assert(!"fil_alternar"); }
void fil_remover(int i) { (void)i; assert(!"fil_remover"); }
int fil_mover(int i, int d) { (void)i; (void)d; assert(!"fil_mover"); return 0; }

// Same addon/catalog for every folder; the genre extra is the only difference.
static volatile int pedAction, pedComedy, pedDrama, pedPlainA;
static volatile int comedyMorta;
static char *redeGenero(const char *url) {
  char buf[600];
  const char *p, *g;
  if (strstr(url, "/manifest.json")) return strdup(MANIFESTO);
  p = strstr(url, "/catalog/movie/");
  if (!p) return NULL;
  g = strstr(p, "genre=");
  if (g && !strncmp(g + 6, "Action", 6)) {
    __atomic_add_fetch(&pedAction, 1, __ATOMIC_SEQ_CST); g = "act";
  } else if (g && !strncmp(g + 6, "Comedy", 6)) {
    __atomic_add_fetch(&pedComedy, 1, __ATOMIC_SEQ_CST);
    if (comedyMorta) return NULL;   // addon silent: lerCatalogo respondeu=0
    g = "com";
  } else if (g && !strncmp(g + 6, "Drama", 5)) {
    __atomic_add_fetch(&pedDrama, 1, __ATOMIC_SEQ_CST); g = "dra";
  } else if (g) {
    return NULL;
  } else {
    char id[8];
    snprintf(id, sizeof id, "%.5s", p + 15);
    if (!strcmp(id, "cat_a")) __atomic_add_fetch(&pedPlainA, 1, __ATOMIC_SEQ_CST);
    snprintf(buf, sizeof buf,
             "{\"metas\":[{\"id\":\"tt%s1\",\"type\":\"movie\",\"name\":\"%s 1\",\"poster\":\"p.jpg\"},"
             "{\"id\":\"tt%s2\",\"type\":\"movie\",\"name\":\"%s 2\",\"poster\":\"p.jpg\"}]}",
             id + 4, id, id + 4, id);
    return strdup(buf);
  }
  snprintf(buf, sizeof buf,
           "{\"metas\":[{\"id\":\"tt%s1\",\"type\":\"movie\",\"name\":\"%s 1\",\"poster\":\"p.jpg\"},"
           "{\"id\":\"tt%s2\",\"type\":\"movie\",\"name\":\"%s 2\",\"poster\":\"p.jpg\"}]}",
           g, g, g, g);
  return strdup(buf);
}

static const CatFileira *rowKey(const char *key) {
  for (int r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->chave, key)) return cat_fileira(r);
  return NULL;
}
static const CatFileira *rowCat(const char *catId) {
  for (int r = 0; r < cat_n_fileiras(); r++)
    if (!strcmp(cat_fileira(r)->catId, catId)) return cat_fileira(r);
  return NULL;
}
static int rowHas(const CatFileira *f, const char *imdb) {
  for (int j = 0; f && j < f->n; j++) {
    const CatItem *it = cat_item(f->ini + j);
    if (it && !strcmp(it->imdb, imdb)) return 1;
  }
  return 0;
}
static void dump(const char *label) {
  printf("    %s:", label);
  for (int r = 0; r < cat_n_fileiras(); r++) {
    const CatFileira *f = cat_fileira(r);
    printf(" %s[", f->chave);
    for (int j = 0; j < f->n; j++) { const CatItem *it = cat_item(f->ini + j); printf("%s%s", j ? "," : "", it ? it->imdb : "?"); }
    printf("]");
  }
  printf("\n");
}
static const ColFolder *folder(const char *id) {
  for (int i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    if (f && !strcmp(f->id, id)) return f;
  }
  return NULL;
}
static void zeroPed(void) { pedAction = pedComedy = pedDrama = pedPlainA = 0; }

int main(void) {
  snprintf(dirDados, sizeof dirDados, "%snuvio-colpin-mount-%d", nvt("/"), (int)getpid());
  assert(mkdir(dirDados, 0700) == 0);
  desc_tmdb(dirDados);
  homeestado_iniciar();
  fixtureRede = redeGenero;
  // ac/co/dr: same base+type+catalog cat_a, three distinct genres. cat_a is
  // swallowed by the collection, so the Home has no plain cat_a row at all.
  assert(col_definir_json("{\"collections\":[{\"id\":\"g\",\"title\":\"G\",\"folders\":["
    "{\"id\":\"ac\",\"title\":\"Action\",\"tileShape\":\"POSTER\",\"sources\":[{\"addonBaseUrl\":\"" BASE "\",\"type\":\"movie\",\"catalogId\":\"cat_a\",\"genre\":\"Action\"}]},"
    "{\"id\":\"co\",\"title\":\"Comedy\",\"sources\":[{\"addonBaseUrl\":\"" BASE "\",\"type\":\"movie\",\"catalogId\":\"cat_a\",\"genre\":\"Comedy\"}]},"
    "{\"id\":\"dr\",\"title\":\"Drama\",\"sources\":[{\"addonBaseUrl\":\"" BASE "\",\"type\":\"movie\",\"catalogId\":\"cat_a\",\"genre\":\"Drama\"}]}]}]}"));
  assert(folder("ac") && folder("co") && folder("dr"));
  assert(cf_alternar(folder("ac")) == 1 && cf_alternar(folder("co")) == 1);
  assert(cf_n() == 2);

  printf("==> two pinned genre folders, same base/type/catalog, real mount\n");
  zeroPed();
  desc_iniciar();
  esperarQuieto();
  dump("published");
  const CatFileira *ra = rowKey("colpin_ac"), *rc = rowKey("colpin_co");
  assert(!rowCat("cat_a") && pedPlainA == 0);          // swallowed catalog: no plain row
  assert(ra && rc && ra != rc);                        // BASE: no colpin rows at all
  assert(pedAction == 1 && pedComedy == 1);            // genre extra reached the addon
  assert(ra->n == 2 && rowHas(ra, "ttact1") && rowHas(ra, "ttact2"));
  assert(!rowHas(ra, "tta1") && !rowHas(ra, "ttcom1"));  // not the unfiltered window, not co's
  assert(rc->n == 2 && rowHas(rc, "ttcom1") && rowHas(rc, "ttcom2"));
  assert(!rowHas(rc, "tta1") && !rowHas(rc, "ttact1"));
  assert(!ra->base[0] && !ra->catId[0] && !rc->base[0]); // no quota, no see-all
  assert(!rowKey("colpin_dr") && pedDrama == 0);          // unpinned: no row, no request
  puts("ok  each pinned folder publishes its own genre window");

  printf("\n==> unpin + rebuild drops only that row\n");
  assert(cf_alternar(folder("co")) == 0);
  desc_remontar_fileiras();
  dump("rebuilt");
  assert(rowKey("colpin_ac") && !rowKey("colpin_co"));
  puts("ok  unpinned folder's row gone without a new mount");

  printf("\n==> silent addon: one timeout per base per mount, folder keeps card\n");
  assert(cf_alternar(folder("co")) == 1 && cf_alternar(folder("dr")) == 1);
  comedyMorta = 1;
  zeroPed();
  desc_repetir();
  esperarQuieto();
  dump("published");
  assert(pedAction == 1 && pedComedy == 1 && pedDrama == 0);  // dr skipped: base went silent
  assert(rowKey("colpin_ac") && !rowKey("colpin_co") && !rowKey("colpin_dr"));
  puts("ok  dead base costs one timeout; later genre folders on it stay card");

  printf("\n==> profile without pins: no genre rows, no genre requests\n");
  comedyMorta = 0;
  cf_definir_perfil(2);
  assert(cf_n() == 0);
  zeroPed();
  desc_repetir();
  esperarQuieto();
  dump("published");
  assert(!rowKey("colpin_ac") && !rowKey("colpin_co") && pedAction == 0 && pedComedy == 0);
  puts("ok  profile switch leaves no pinned genre rows");
  printf("colpin_mount: PASS\n");
  return 0;
}
