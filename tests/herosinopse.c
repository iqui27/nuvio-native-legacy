// SINOPSE DOS CANDIDATOS DO DESTAQUE (LG C9, 2.0.3, 08/10).
//
// Com "hero *" o destaque sorteia titulos de todo o catalogo, e os que vieram da
// lista do Trakt chegam RASOS (so nome e arte do metahub): apareciam sem
// sinopse porque o /meta so era pedido ao abrir o detalhe. desc_sinopse_hero
// completa em segundo plano, pelo mesmo /meta do detalhe (catalogo do Nuvio,
// Cinemeta de reserva), um pedido por vez.
//
// O QUE ESTE TESTE PROVA (respostas falsas, sem rede):
//   1. item raso ganha a sinopse do /meta e ela fica NO CATALOGO (o detalhe,
//      que le o mesmo item, a reaproveita);
//   2. item que ja tinha sinopse nao e tocado nem consultado;
//   3. item cuja ficha nao responde NAO sai do catalogo e fica sem sinopse
//      (vira linha de log); uma segunda chamada nao repete o pedido (memoria
//      de falha), entao nao ha rajada;
//   4. a arte do item nao muda (so o texto);
//   5. item de canal (id que nao e do IMDb) nao vai a rede.
//
//   bash tests/herosinopse.sh
#define main detalheanime_main
#include "detalheanime.c"
#undef main

#define META_OK(id, desc) "{\"meta\":{\"id\":\"" id "\",\"type\":\"movie\",\"name\":\"Rain Man\"," \
  "\"poster\":\"https://p.test/x.jpg\",\"description\":\"" desc "\",\"genres\":[\"Drama\"]}}"

static void esperar(void) {
  int i;
  for (i = 0; i < 2000 && hsVivo; i++) usleep(5000);
  assert(!hsVivo);
}

int main(void) {
  CatItem it[4];
  int idx[4] = { 0, 1, 2, 3 };
  memset(it, 0, sizeof it);
  metaprov_zerar_pausa();
  // 0: raso (Trakt); 1: completo; 2: raso e a ficha nao responde; 3: canal.
  snprintf(it[0].imdb, sizeof it[0].imdb, "tt0095953");
  snprintf(it[0].tipo, sizeof it[0].tipo, "movie");
  snprintf(it[0].titulo, sizeof it[0].titulo, "Rain Man");
  snprintf(it[0].backdrop, sizeof it[0].backdrop, "https://images.metahub.space/background/medium/tt0095953/img");
  snprintf(it[1].imdb, sizeof it[1].imdb, "tt0000111");
  snprintf(it[1].tipo, sizeof it[1].tipo, "movie");
  snprintf(it[1].titulo, sizeof it[1].titulo, "Completo");
  snprintf(it[1].sinopse, sizeof it[1].sinopse, "Ja tinha.");
  snprintf(it[1].logo, sizeof it[1].logo, "https://p.test/logo.png");
  snprintf(it[2].imdb, sizeof it[2].imdb, "tt38961497");
  snprintf(it[2].tipo, sizeof it[2].tipo, "movie");
  snprintf(it[2].titulo, sizeof it[2].titulo, "Miasma");
  snprintf(it[3].imdb, sizeof it[3].imdb, "cs:channel:abc");
  snprintf(it[3].tipo, sizeof it[3].tipo, "channel");
  snprintf(it[3].titulo, sizeof it[3].titulo, "Canal");
  cat_definir_tudo(it, 4, NULL, 0);

  rota("/meta/movie/tt0095953.json", META_OK("tt0095953", "Charlie Babbitt discovers his late father left everything to a brother he never knew."));
  rota("/meta/movie/tt38961497.json", NULL);   // ficha fora do ar (Nuvio e Cinemeta)

  nPedidos = 0;
  desc_sinopse_hero(idx, 4);
  esperar();
  assert(!strncmp(cat_item(0)->sinopse, "Charlie Babbitt", 15));          // 1
  assert(!strcmp(cat_item(0)->backdrop,                                  // 4
                 "https://images.metahub.space/background/medium/tt0095953/img"));
  assert(!strcmp(cat_item(1)->sinopse, "Ja tinha."));                     // 2
  assert(!pediu("tt0000111"));
  assert(cat_n() == 4 && !cat_item(2)->sinopse[0]);                       // 3
  assert(!strcmp(cat_item(2)->titulo, "Miasma"));
  assert(!pediu("channel"));                                              // 5
  puts("ok  item raso ganha a sinopse do /meta e ela fica no catalogo");
  puts("ok  titulo sem ficha continua no destaque, sem sinopse (falha logada)");

  { int antes = nPedidos;
    desc_sinopse_hero(idx, 4);
    esperar();
    assert(nPedidos == antes);   // nada a refazer: preenchido ou falha recente
  }
  puts("ok  segunda chamada nao repete pedido (sem rajada)");
  puts("herosinopse: PASS");
  return 0;
}
