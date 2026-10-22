// Leitura real do guia: tv, ids de IPTV e landscape nao exigem genero.
#include "../src/guia.c"
#include <assert.h>

static const char *corpo = "{\"metas\":["
  "{\"id\":\"m3u:canaisabertos:0\",\"type\":\"tv\",\"name\":\"Band SP\","
  "\"poster\":\"https://logos.example/band.png\",\"posterShape\":\"landscape\"},"
  "{\"id\":\"ec:aee\",\"type\":\"tv\",\"name\":\"A&E\","
  "\"poster\":\"https://logos.example/aee.png\",\"posterShape\":\"landscape\"},"
  "{\"id\":\"tt123\",\"type\":\"movie\",\"name\":\"Filme\"}]}";
char *rede_baixar(const char *url, int segundos) {
  (void)segundos;
  assert(!strcmp(url, "https://addon.example/catalog/tv/canais.json?v=1.4.0&probe=1"));
  return strdup(corpo);
}
int addons_base_desligada(const char *base) { (void)base; return 0; }
int main(void) {
  GFonte f = {0};
  int novos;
  strcpy(f.base, "https://addon.example?v=1.4.0&probe=1");
  strcpy(f.tipo, "tv"); strcpy(f.id, "canais"); strcpy(f.nome, "Canais Abertos");
  assert(ehCanal(f.tipo));
  assert(lerPagina(&f, 0, 10, &novos) == 3);
  assert(novos == 2 && sNCanais == 2);
  assert(!strcmp(sCanais[0].id, "m3u:canaisabertos:0"));
  assert(!strcmp(sCanais[1].id, "ec:aee"));
  for (int k = 0; k < 2; k++) {
    assert(sCanais[k].cat >= 0 && sCanais[k].logo[0]);
    assert(!strcmp(sCanais[k].base, f.base));
  }
  puts("PASS: guia aceita catalogo tv sem genre, ids m3u:/ec: e poster landscape");
}
