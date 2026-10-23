// Leitura real do guia: tv, ids de IPTV e landscape nao exigem genero.
// A segunda parte e o manifesto MISTO (movie+series+canal, o formato do
// M3U Mux e dos addons de IPTV caseiros): cada canal lembra o tipo
// declarado do catalogo dele — e o tipo do item, quando o meta traz um.
#define NV_PERFIL_SESSAO_FORTE
#include "../src/guia.c"
#include <assert.h>

static const char *corpo = "{\"metas\":["
  "{\"id\":\"m3u:canaisabertos:0\",\"type\":\"tv\",\"name\":\"Band SP\","
  "\"poster\":\"https://logos.example/band.png\",\"posterShape\":\"landscape\"},"
  "{\"id\":\"ec:aee\",\"type\":\"tv\",\"name\":\"A&E\","
  "\"poster\":\"https://logos.example/aee.png\",\"posterShape\":\"landscape\"},"
  "{\"id\":\"tt123\",\"type\":\"movie\",\"name\":\"Filme\"}]}";
static const char *corpoMix = "{\"metas\":["
  "{\"id\":\"m3u-live-1\",\"type\":\"channel\",\"name\":\"Canal Um\","
  "\"poster\":\"https://logos.example/um.png\",\"genres\":[\"News\"]},"
  "{\"id\":\"m3u-live-2\",\"name\":\"Canal Dois\","            // sem "type": vale o do catalogo
  "\"poster\":\"https://logos.example/dois.png\"},"
  "{\"id\":\"m3u-live-3\",\"type\":\"tv\",\"name\":\"Canal Tres\"," // tipo proprio vence o do catalogo
  "\"poster\":\"https://logos.example/tres.png\"},"
  "{\"id\":\"tt999\",\"type\":\"movie\",\"name\":\"Filme\","   // filme no catalogo de canal: fora
  "\"poster\":\"https://logos.example/f.png\"}]}";
static int chamadas;
char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  if (chamadas++ == 0)
    assert(!strcmp(url, "https://addon.example/catalog/tv/canais.json?v=1.4.0&probe=1"));
  else {
    assert(!strcmp(url, "https://mux.example/TK/catalog/channel/m3u-channels.json"));
    return strdup(corpoMix);
  }
  return strdup(corpo);
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
const char *livecat_secao(const char *n,int *r) { (void)n;*r=0;return NULL; }
int main(void) {
  GFonte f = {0};
  int novos;
  sTeto = 30000;
  strcpy(f.base, "https://addon.example?v=1.4.0&probe=1");
  strcpy(f.tipo, "tv"); strcpy(f.id, "canais"); strcpy(f.nome, "Canais Abertos");
  assert(ehCanal(f.tipo));
  GAddonCarga carga = {0};
  strcpy(carga.base, f.base); carga.ativo = 1; addonsCarga = &carga; nAddonsCarga = 1;
  int fl = sFonte(LTV_ADDON, f.base, f.nome, f.base);
  assert(fl >= 0);
  assert(lerPagina(&f, fl, 0, &novos) == 3);
  assert(novos == 2 && sNCanais == 2);
  assert(!strcmp(sCanais[0].id, "m3u:canaisabertos:0"));
  assert(!strcmp(sCanais[1].id, "ec:aee"));
  for (int k = 0; k < 2; k++) {
    assert(sCanais[k].cat >= 0 && sCanais[k].logo[0]);
    assert(!strcmp(sCanais[k].base, f.base));
    assert(!strcmp(sCanais[k].tipo, "tv"));   // tipo declarado, do item ou do catalogo
  }
  // Tipo que o item vai pedir ao /stream/: o declarado pelo catalogo.
  { CatItem it;
    cats = sCats; canalParaItem(&sCanais[0], &it);
    assert(!strcmp(it.tipo, "tv")); }        // "tv" declarado vira o tipo do card
  // ---- manifesto MISTO estilo M3U Mux: catalogo "channel" entre movie/series
  memset(&f, 0, sizeof f);
  strcpy(f.base, "https://mux.example/TK");
  strcpy(f.tipo, "channel"); strcpy(f.id, "m3u-channels"); strcpy(f.nome, "Live TV");
  strcpy(carga.base, f.base);
  fl = sFonte(LTV_ADDON, f.base, f.nome, f.base);
  assert(fl >= 0);
  assert(lerPagina(&f, fl, 0, &novos) == 4);
  assert(novos == 3 && sNCanais == 5);
  assert(!strcmp(sCanais[2].id, "m3u-live-1") && !strcmp(sCanais[2].tipo, "channel"));
  assert(!strcmp(sCanais[3].id, "m3u-live-2") && !strcmp(sCanais[3].tipo, "channel")); // do catalogo
  assert(!strcmp(sCanais[4].id, "m3u-live-3") && !strcmp(sCanais[4].tipo, "tv"));      // do item
  { CatItem it;
    cats = sCats; canalParaItem(&sCanais[2], &it);
    assert(!strcmp(it.tipo, "channel")); }   // canal declarado continua canal no card
  puts("PASS: guia aceita catalogo tv sem genre, ids m3u:/ec:, poster landscape");
  puts("PASS: canal de manifesto misto guarda o tipo declarado do catalogo/item");
}
