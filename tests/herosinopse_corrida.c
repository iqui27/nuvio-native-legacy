// SINOPSE DO DESTAQUE x CATALOGO TROCADO (revisao 2.0.3, achado 2).
//
// fioSinopseHero roda fora do desenho. Dois defeitos possiveis:
//   1. USO DEPOIS DE LIBERAR: pegava `o = cat_item(i)` e so depois copiava
//      `*o`. Duas publicacoes + cat_quadro() no meio liberam o bloco de `o`.
//      Barreira: o teste segura hsTrava; o fio para em hsNegativo() logo depois
//      de pegar `o`; o teste publica dois catalogos, roda cat_quadro() e solta.
//      Sob ASAN o codigo antigo aborta com heap-use-after-free.
//   2. ATUALIZACAO PERDIDA: copiava o item inteiro e devolvia o item inteiro
//      por cat_atualizar_item. Uma mudanca feita por outro fio entre a copia e
//      a escrita (aqui: naLista, pelo gancho NV_CAT_TEST_ANTES_TRAVA, que roda
//      no fio da sinopse antes de ele pegar a trava) era revertida.
//
//   bash tests/herosinopse_corrida.sh            (ASAN+UBSAN, o padrao)
//   SANITIZE=0 bash tests/herosinopse_corrida.sh
#define main detalheanime_main
#include "detalheanime.c"
#undef main

#define META_OK(id, desc) "{\"meta\":{\"id\":\"" id "\",\"type\":\"movie\",\"name\":\"Nome\"," \
  "\"poster\":\"https://p.test/x.jpg\",\"description\":\"" desc "\",\"genres\":[\"Drama\"]}}"

static pthread_t fioTeste;
static _Atomic int ganchoArmado, ganchoRodou, ganchoAlvo;
void nv_cat_teste_antes_trava(void) {
  if (!ganchoArmado || pthread_equal(pthread_self(), fioTeste)) return;
  ganchoArmado = 0;                       // cat_definir_na_lista chama o gancho de novo
  cat_definir_na_lista(ganchoAlvo, 1);    // outro escritor muda o item no meio
  ganchoRodou = 1;
}

static void esperar(void) {
  int i;
  for (i = 0; i < 2000 && hsVivo; i++) usleep(5000);
  assert(!hsVivo);
}

static void item(CatItem *c, const char *imdb, const char *titulo, const char *sinopse) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", imdb);
  snprintf(c->tipo, sizeof c->tipo, "movie");
  snprintf(c->titulo, sizeof c->titulo, "%s", titulo);
  snprintf(c->sinopse, sizeof c->sinopse, "%s", sinopse);
}

int main(void) {
  CatItem it[2], outro[2];
  int idx[2] = { 0, 1 };
  fioTeste = pthread_self();
  metaprov_zerar_pausa();
  rota("/meta/movie/tt0000001.json", META_OK("tt0000001", "Primeira sinopse."));
  rota("/meta/movie/tt0000002.json", META_OK("tt0000002", "Segunda sinopse."));
  rota("/meta/movie/tt0000003.json", META_OK("tt0000003", "Terceira sinopse."));

  // 1. Bloco liberado com o fio parado entre cat_item() e a copia.
  item(&it[0], "tt0000001", "Um", "");
  item(&it[1], "tt0000002", "Dois", "");
  cat_definir_tudo(it, 2, NULL, 0);
  cat_quadro();
  desc_sinopse_hero(idx, 2);
  { int i;   // item 0 sai rapido; o fio dorme HSIN_PAUSA_MS antes do item 1
    for (i = 0; i < 400 && !cat_item(0)->sinopse[0]; i++) usleep(1000);
    assert(cat_item(0)->sinopse[0]); }
  pthread_mutex_lock(&hsTrava);         // o item 1 para em hsNegativo()
  usleep(400000);
  item(&outro[0], "tt0000009", "Outro A", "Tem.");
  item(&outro[1], "tt0000008", "Outro B", "Tem.");
  cat_definir_tudo(outro, 2, NULL, 0);
  cat_definir_tudo(outro, 2, NULL, 0);
  cat_quadro();                         // libera o bloco em que o fio lia
  pthread_mutex_unlock(&hsTrava);
  esperar();
  assert(!strcmp(cat_item(1)->imdb, "tt0000008") && !strcmp(cat_item(1)->sinopse, "Tem."));
  assert(!strcmp(cat_item(1)->titulo, "Outro B"));
  puts("ok  catalogo trocado no meio: o fio nao le bloco liberado nem escreve no titulo errado");

  // 2. Outro escritor muda o item entre a leitura e a escrita do fio.
  item(&it[0], "tt0000003", "", "");
  it[0].naLista = 0;
  cat_definir_tudo(it, 1, NULL, 0);
  cat_quadro();
  ganchoAlvo = 0; ganchoRodou = 0; ganchoArmado = 1;
  { int um[1] = { 0 }; desc_sinopse_hero(um, 1); }
  esperar();
  ganchoArmado = 0;
  assert(ganchoRodou);
  assert(!strcmp(cat_item(0)->sinopse, "Terceira sinopse."));
  assert(!strcmp(cat_item(0)->titulo, "Nome"));        // titulo vazio ganha o da ficha
  assert(cat_item(0)->naLista == 1);                   // e a mudanca do outro fio fica
  puts("ok  a sinopse entra sem reverter o que outro fio mudou no item");
  puts("herosinopse_corrida: PASS");
  return 0;
}
