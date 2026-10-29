// Pagina real, com catalogo/previa locais; os guardas de dono nao sao simulados.
#define cat_item loading_test_cat_item
#define tex_falhou loading_test_tex_falhou
#define ajustes_hero_fonte loading_test_ajustes_hero_fonte
#define ajustes_hero_arte_diferente loading_test_ajustes_hero_arte_diferente
#define trocaarte_previa_fundo loading_test_preview
#include "../src/detail.c"
#include <assert.h>
static CatItem fixtureTitle;
static char fixturePreview[NV_TEX_URL_MAX];
const CatItem *cat_item(int i) { return i == 0 ? &fixtureTitle : NULL; }
const char *trocaarte_previa_fundo(void) {
  return fixturePreview[0] ? fixturePreview : NULL;
}
void fixture_detail_set(const CatItem *c, const char *url, int poster) {
  idx=0; saindo=0; aberto=url&&*url; idxTemCopia=c!=NULL;
  fixturePreview[0]=0;
  if(c) { fixtureTitle=*c; idxCopia=*c; snprintf(idxImdb,sizeof idxImdb,"%s",c->imdb); }
  else { memset(&fixtureTitle,0,sizeof fixtureTitle); idxImdb[0]=0; }
  snprintf(arteFixa,sizeof arteFixa,"%s",url?url:""); arteFixaPoster=poster;
  if(poster) assert(c&&!strcmp(c->poster,url));
}
void fixture_detail_backdrop(const char *url) {
  snprintf(fixtureTitle.backdrop,sizeof fixtureTitle.backdrop,"%s",url);
  snprintf(fixtureTitle.backdropCatalogo,sizeof fixtureTitle.backdropCatalogo,"%s",url);
}
void fixture_detail_poster(const char *url) {
  snprintf(fixtureTitle.poster,sizeof fixtureTitle.poster,"%s",url);
}
static void fixture_open(const CatItem *c) {
  fixture_detail_set(c,"",0); aberto=1;
}
void fixture_detail_ownership_checks(void (*verify)(int,const char *)) {
  CatItem c={0},other;
  char out[NV_TEX_URL_MAX],saved[NV_TEX_URL_MAX]; int poster;
  snprintf(c.imdb,sizeof c.imdb,"tt1234567");
  snprintf(c.tipo,sizeof c.tipo,"movie");
  snprintf(c.backdrop,sizeof c.backdrop,"https://example.invalid/owned-landscape.jpg");
  snprintf(c.backdropCatalogo,sizeof c.backdropCatalogo,"%s",c.backdrop);
  snprintf(c.poster,sizeof c.poster,"https://example.invalid/owned-poster.jpg");
  fixture_open(&c);
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&!poster&&!strcmp(out,c.backdrop),
         "getter copies the actual selected same-title landscape");
  snprintf(saved,sizeof saved,"%s",out); arteFixa[0]=0;
  fixture_detail_backdrop("https://example.invalid/changed-catalog-art.jpg");
  verify(!strcmp(out,saved),"getter output owns its bytes after catalog/cache mutation");
  fixture_open(&c); memset(out,'x',sizeof out); poster=1;
  verify(!detail_arte_titulo(&c,out,2,&poster)&&!out[0]&&!poster,
         "getter rejects truncation and clears caller output/classification");
  fixture_open(&c); out[0]='x'; poster=1;
  verify(!detail_arte_titulo(NULL,out,sizeof out,&poster)&&!out[0]&&!poster&&
         !detail_arte_titulo(&c,NULL,0,NULL),"getter safely rejects missing title/output");
  fixture_open(&c); other=c; snprintf(other.imdb,sizeof other.imdb,"tt7654321");
  verify(!detail_arte_titulo(&other,out,sizeof out,&poster),"getter rejects another requested title");
  fixture_open(&c); other=c; snprintf(other.tipo,sizeof other.tipo,"series");
  verify(!detail_arte_titulo(&other,out,sizeof out,&poster),"getter rejects another requested type");
  fixture_open(&c); snprintf(fixtureTitle.imdb,sizeof fixtureTitle.imdb,"tt7654321");
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects a catalog slot reused by another title");
  fixture_open(&c); snprintf(fixtureTitle.tipo,sizeof fixtureTitle.tipo,"series");
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects same-ID catalog type replacement");
  fixture_open(&c); idx=1;
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects a missing current catalog slot");
  fixture_open(&c); saindo=1;
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects an exiting title page");
  fixture_open(&c); aberto=0;
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects a closed title page");
  fixture_open(&c); idxTemCopia=0;
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter requires an owned title snapshot");
  fixture_open(&c); idxImdb[0]=0;
  verify(!detail_arte_titulo(&c,out,sizeof out,&poster),"getter rejects an unowned title identity");
  c.backdrop[0]=c.backdropCatalogo[0]=0; fixture_open(&c);
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&!poster&&
         strstr(out,"metahub.space/background/")!=NULL,
         "real Metahub fallback remains landscape despite an available poster");
  snprintf(c.imdb,sizeof c.imdb,"local:owned"); fixture_open(&c);
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&poster&&!strcmp(out,c.poster),
         "actual portrait fallback is recorded at the moment artwork freezes");
  snprintf(saved,sizeof saved,"%s",out);
  fixture_detail_poster("https://example.invalid/replacement-poster.jpg");
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&poster&&!strcmp(out,saved),
         "frozen old poster stays portrait after catalog poster-only enrichment");
  snprintf(fixturePreview,sizeof fixturePreview,"%s",saved);
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&!poster&&!strcmp(out,saved),
         "an explicit artwork preview is always a background even at the poster URL");
  fixturePreview[0]=0;
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&poster&&!strcmp(out,saved),
         "leaving preview restores the frozen portrait classification");
  fixture_detail_backdrop("https://example.invalid/enriched-backdrop.jpg");
  verify(detail_arte_titulo(&c,out,sizeof out,&poster)&&!poster&&
         !strcmp(out,"https://example.invalid/enriched-backdrop.jpg"),
         "same-title real backdrop releases a temporary portrait once");
  fixture_detail_set(NULL,NULL,0);
}
