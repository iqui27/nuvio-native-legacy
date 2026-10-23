// CNCVerse Bridge ao vivo (comunidade 17/10): o id do canal e "cnc:" +
// base64url("<plugin>::<dataUrl>"), de 72 a 140 bytes nesta amostra. A aba
// descartava o que passava de 79 e cortava em 63 o que passava de 63, e o
// /stream/ saia com o id cortado ("0 fontes"). Fixtures montadas do protocolo
// Stremio e do codigo aberto do addon (o addon e local, sem endpoint publico).
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include "../src/idcanal.h"
#include "../src/streams.h"
#include <assert.h>

static char *ler(const char *caminho) {
  FILE *f = fopen(caminho, "rb");
  char *b;
  long n;
  assert(f);
  fseek(f, 0, SEEK_END); n = ftell(f); rewind(f);
  b = calloc(1, (size_t)n + 1);
  assert(fread(b, 1, (size_t)n, f) == (size_t)n);
  fclose(f);
  return b;
}
static char *catalogo;
char *rede_baixar(const char *url, int segundos) {
  (void)url; (void)segundos;
  return catalogo ? strdup(catalogo) : ler("tests/fixtures/cncverse_live_catalog.json");
}
char *rede_baixar_medido_controle(const char *url, int segundos,
                                  const char *const *cabecalhos,
                                  const RedeControle *controle,
                                  RedeMedida *medida) {
  (void)cabecalhos; (void)medida;
  if (controle && controle->cancelado && *controle->cancelado) return NULL;
  return rede_baixar(url, segundos);
}
uint64_t perfis_sessao_geracao(void) { return 0; }
int perfis_sessao_atual(uint64_t geracao) { return geracao == 0; }
int perfis_sessao_travar(uint64_t geracao) { (void)geracao; return 1; }
void perfis_sessao_destravar(void) {}
int addons_base_desligada(const char *base) { (void)base; return 0; }
char *dados_ler(const char *n) { (void)n; return NULL; }
int dados_gravar(const char *n, const char *v) { (void)n; (void)v; return 1; }
const char *i18n(const char *s) { return s; }
int xtream_e_id(const char *id) { return id && !strncmp(id, "xtream:", 7); }

int ajustes_livetv_sugerir(void) { return 0; }
const char *livecat_secao(const char *n, int *r) { (void)n; *r = 0; return NULL; }
int main(void) {
  GFonte f = {0};
  int novos;
  sTeto = 30000;
  char *cat = ler("tests/fixtures/cncverse_live_catalog.json");
  char idsOriginais[3][IDCANAL_LONGO_MAX + 1];
  const char *p = js_array(cat, NULL, "metas");
  strcpy(f.base, "https://bridge.example");
  strcpy(f.tipo, "tv"); strcpy(f.id, "live"); strcpy(f.nome, "CNCVerse Live");
  for (int k = 0; k < 3; k++, p = js_prox(js_fim(p)))
    assert(js_texto(p, js_fim(p), "id", idsOriginais[k], sizeof idsOriginais[k]));
  free(cat);

  // ---- aba ao vivo: os 3 canais entram, nenhum some por tamanho de id
  GAddonCarga carga = {0};
  strcpy(carga.base, f.base); carga.ativo = 1; addonsCarga = &carga; nAddonsCarga = 1;
  int fl = sFonte(LTV_ADDON, f.base, f.nome, f.base);
  assert(fl >= 0);
  assert(lerPagina(&f, fl, 0, &novos) == 3);
  assert(novos == 3 && sNCanais == 3);
  for (int k = 0; k < 3; k++) {
    CatItem it;
    char longo[IDCANAL_LONGO_MAX + 1];
    cats = sCats; canalParaItem(&sCanais[k], &it);
    assert(strlen(it.imdb) < sizeof it.imdb);                       // cabe no CatItem sem corte
    assert(!strcmp(it.imdb, sCanais[k].id));
    assert(!strcmp(idcanal_longo(it.imdb, longo, sizeof longo), idsOriginais[k]));  // o /stream/ leva o id inteiro
    for (int j = 0; j < k; j++) assert(strcmp(sCanais[j].id, sCanais[k].id));      // canais distintos
  }
  // id curto de sempre passa intacto
  { char d[80], l[IDCANAL_LONGO_MAX + 1];
    assert(idcanal_curto("m3u:canaisabertos:0", d, sizeof d) && !strcmp(d, "m3u:canaisabertos:0"));
    assert(!strcmp(idcanal_longo("tt123:1:2", l, sizeof l), "tt123:1:2")); }

  // ---- fontes: o stream do CNC entra; o convite de doacao (so externalUrl) nao e midia
  { char *json = ler("tests/fixtures/cncverse_live_streams.json");
    Stream *v = NULL;
    int n = stream_extrair(json, "CNCVerse Bridge", &v);
    assert(n == 1);
    assert(!strcmp(v[0].url, "https://example.invalid/proxy/hls/sun-tv-hd/master.m3u8"));
    assert(v[0].notWebReady);
    free(v); free(json); }
  if (!getenv("CNC_FIXTURE_DIR")) return 0;
  // One generated tv catalog: 1321 distinct long IDs, eleven groups.
  stagingZerar(); sTeto = ltv_teto_canais(2, 2048);
  fl = sFonte(LTV_ADDON, f.base, f.nome, f.base);
  char path[2048];
  const char *dir = getenv("CNC_FIXTURE_DIR");
  assert(dir);
  snprintf(path, sizeof path, "%s/catalog.json", dir);
  catalogo = ler(path);
  assert(lerPagina(&f, fl, 0, &novos) == 1321);
  printf("B: retained %d/1321, arena %zu bytes\n", sNCanais, ltv_arena_bytes(&sArena));
  assert(sNCanais == 1321 && novos == 1321 && !sLtvF[fl].cortada);
  selecionarSecoes(); assert(sNCanais == 1321);
  assert(ltv_arena_bytes(&sArena) < 2 * 1024 * 1024);
  free(catalogo); stagingZerar();
  puts("PASS: canal CNCVerse de id longo entra na aba e pede /stream/ com o id inteiro");
}
