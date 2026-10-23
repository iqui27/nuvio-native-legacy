// Regressao: sinopse pronta nao pode impedir logo/ano/nota da ficha.
#define main detalheanime_main
#include "detalheanime.c"
#include "nvtmp.h"
#undef main

#ifndef LOGORAPIDO_BASELINE
static int viuPrimario;
static void antesComplemento(const char *url) {
  if (!strstr(url, "catalog.nuvio.tv")) return;
  CatItem c;
  assert(cat_copiar_item(0, &c));
  assert(c.logo[0] && c.nota == 81 && c.nElenco == 1 && c.sinopse[0]);
  viuPrimario++;
}
#endif
int main(void) {
  CatItem c = {0};
  strcpy(c.imdb, "tt0000176"); strcpy(c.tipo, "movie");
  strcpy(c.titulo, "Filme Salvo"); strcpy(c.sinopse, "Sinopse ja na Home.");
  strcpy(c.poster, "https://p.test/poster.jpg");
  completarRaso(&c, META_COMPLETO, "movie");
  assert(!strcmp(c.sinopse, "Sinopse ja na Home."));
  assert(!strcmp(c.logo, "https://p.test/logo.png"));
  assert(c.nota == 81 && strstr(c.meta, "2019"));
  puts("ok: logo/ano/nota entram mesmo com sinopse pronta, sem trocar texto");
#ifndef LOGORAPIDO_BASELINE
  // Texto pronto sem logo tambem deve disparar pre-busca e preservar o texto.
  limparCacheMeta();
  rota("/meta/movie/tt0000176.json", META_COMPLETO);
  c.logo[0] = c.meta[0] = 0; c.nota = 0;
  nPedidos = 0;
  assert(desc_sinopse_completar_item(&c, NULL, NULL) == 1);
  assert(nPedidos == 1 && c.logo[0] && c.nota == 81);
  assert(!strcmp(c.sinopse, "Sinopse ja na Home."));
  assert(desc_sinopse_completar_item(&c, NULL, NULL) == -1 && nPedidos == 1);
  puts("ok: sinopse sem logo pre-busca uma vez e preserva texto existente");
  // Hero e detalhe devem compartilhar o corpo: nao pedir /meta outra vez.
  c.sinopse[0] = c.logo[0] = c.meta[0] = 0; c.nota = 0;
  nPedidos = 0;
  assert(desc_sinopse_completar_item(&c, NULL, NULL) == 1);
  assert(nPedidos == 0);
  char chave[96]; desc_meta_chave(chave, sizeof chave, c.tipo, c.imdb);
  char *corpo = metaCacheObter(chave);
  assert(corpo && strstr(corpo, "logo.png")); free(corpo);
  assert(c.logo[0] && c.nota == 81 && c.meta[0]);
  puts("ok: pre-busca do hero conserva logo/ano/nota e corpo no cache compartilhado");
  // Simula reinicio: esvazia somente LRU; a ficha tem de voltar do disco.
  char dir[NVT_MAX]; nvt_set(dir, sizeof dir, "/nuvio-logorapido-meta-XXXXXX");
  assert(mkdtemp(dir)); fakeDados = dir;
  metaCacheGuardar(chave, META_COMPLETO);
  for (int i = 0; i < META_CACHE_N; i++) {
    free(metaCache[i].corpo); memset(&metaCache[i], 0, sizeof metaCache[i]);
  }
  corpo = metaCacheObter(chave);
  assert(corpo && strstr(corpo, "logo.png")); free(corpo);
  fakePerfil = 2;
  assert(!metaCacheObter(chave));
  fakePerfil = 1; fakeIdioma = "en-US";
  assert(!metaCacheObter(chave));
  fakeIdioma = "pt-BR";
  c.logo[0] = c.sinopse[0] = 0;
  nPedidos = 0;
  assert(desc_sinopse_completar_item(&c, NULL, NULL) == 1);
  assert(nPedidos == 0 && c.logo[0] && c.sinopse[0]);
  char contexto[256], nome[700];
  metaChaveContexto(chave, contexto, sizeof contexto);
  assert(metaDiscoNome(contexto, nome, sizeof nome));
  FILE *f = fopen(nome, "wb"); assert(f);
  fprintf(f, "%ld\n%s\n%s", (long)time(NULL) - META_TTL_S - 1, contexto, META_COMPLETO);
  fclose(f); assert(!metaCacheObter(chave));
  remove(nome); rmdir(dir); fakeDados = "";
  puts("ok: disco conserva ficha/logo sem rede, recusa expirado e isola perfil/idioma");
  // Publicacao parcial nao reverte favoritos e nao toca outro id/tipo.
  cat_definir_tudo(&c, 1, NULL, 0);
  CatItem novo = c;
  strcpy(novo.logo, "https://p.test/nova.png");
  novo.nota = 99;
  cat_definir_na_lista(0, 1);
  assert(cat_completar_basico(0, &novo) == 0);
  assert(cat_item(0)->naLista && cat_item(0)->nota == 81);
  strcpy(novo.imdb, "tt9999999");
  assert(cat_completar_basico(0, &novo) == 0);
  strcpy(novo.imdb, c.imdb); strcpy(novo.tipo, "series");
  assert(cat_completar_basico(0, &novo) == 0);
  puts("ok: completar basico preserva campos existentes e revalida id+tipo");
  // Complemento de outra fonte comecou: logo/info/elenco ja estao publicados.
  limparCacheMeta(); nRotas = 0;
  nFake = 1; fake[0] = (FakeAddon){"Fonte", "https://fonte.test", "fonte", "tt", 0};
  catalogoDe("tt0000176", "movie", "Filme Salvo", "fonte");
  rota("fonte.test/meta/movie/tt0000176.json", META_COMPLETO);
  rota("/meta/movie/tt0000176.json", META_COMPLETO);
  pedidoMetaHook = antesComplemento;
  abrir();
  pedidoMetaHook = NULL;
  assert(viuPrimario == 1);
  puts("ok: logo/info/elenco publicados antes da rede do complemento");
  CatItem parcial = {0};
  completarRaso(&parcial, "{\"meta\":{\"name\":\"Sem poster\",\"logo\":\"https://p.test/l.png\",\"imdbRating\":81}}", "movie");
  assert(parcial.logo[0] && parcial.nota == 81);
  parcial.logo[0] = 0;
  completarRaso(&parcial, "{\"meta\":{\"name\":\"Svg\",\"logo\":\"https://p.test/l.svg\"}}", "movie");
  assert(!parcial.logo[0]);
  puts("ok: ficha sem poster tambem oferece logo; SVG nao entra no cache de textura");
#endif
  puts("logorapido: PASS");
  return 0;
}
