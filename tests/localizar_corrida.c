// TEXTO LOCALIZADO x CATALOGO TROCADO (revisao 2.0.3, mesmo padrao do achado 2,
// tests/herosinopse_corrida.c).
//
// fioLocalizar (descoberta.c) roda fora do desenho. Dois defeitos possiveis:
//   1. USO DEPOIS DE LIBERAR: pegava `o = cat_item(i)` sem trava e lia/copiava
//      `*o` (locChaveDoItem, o->imdb, `*e = *o`). Quem publica dois catalogos
//      e roda cat_quadro() no meio libera o bloco de `o`. Aqui o fio principal
//      faz o papel do desenho: publica e roda cat_quadro() sem parar enquanto o
//      fio localiza lotes de 32 (sem rede: o texto ja esta no cache). Sob ASAN o
//      codigo antigo aborta com heap-use-after-free em fioLocalizar na maioria
//      das rodadas (e corrida, nao barreira; a parte 2 e deterministica).
//   2. ATUALIZACAO PERDIDA: copiava o item inteiro e devolvia o item inteiro
//      por cat_atualizar_item. Uma mudanca feita por outro fio entre a copia e
//      a escrita (aqui: naLista, pelo gancho NV_CAT_TEST_ANTES_TRAVA, que roda
//      no fio da localizacao antes de ele pegar a trava) era revertida.
//
//   bash tests/localizar_corrida.sh            (ASAN+UBSAN, o padrao)
//   SANITIZE=0 bash tests/localizar_corrida.sh
#define main detalheanime_main
#include "detalheanime.c"
#undef main

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
  for (i = 0; i < 2000 && locVivo; i++) usleep(5000);
  assert(!locVivo);
}

static void item(CatItem *c, const char *imdb, const char *titulo) {
  memset(c, 0, sizeof *c);
  snprintf(c->imdb, sizeof c->imdb, "%s", imdb);
  snprintf(c->tipo, sizeof c->tipo, "movie");
  snprintf(c->titulo, sizeof c->titulo, "%s", titulo);
  c->poster[0] = 'x';
}

// Texto localizado ja no cache (sem rede): o fio so le, copia e escreve.
static void semear(const char *imdb, const char *tit) {
  char chave[64];
  locChave(chave, sizeof chave, "movie", imdb);
  locGuardar(chave, tit, "Sinopse no idioma.", "https://l.test/logo.png",
             "https://l.test/fundo.jpg", 1);
}

#define NLOTE 32
int main(void) {
  static CatItem a[NLOTE], b[NLOTE];
  int idx[NLOTE], i, volta;
  fioTeste = pthread_self();
  fakeMetaExterno = 0;
  fakeIdioma = "pt-BR";
  assert(idiomaNaoIngles());
  memset(locCache, 0, sizeof locCache);
  for (i = 0; i < NLOTE; i++) {
    char id[24];
    snprintf(id, sizeof id, "tt%07d", 100 + i);
    semear(id, "Titulo traduzido");
    item(&a[i], id, "English title");
    item(&b[i], id, "English title B");
    idx[i] = i;
  }

  // 1. Catalogo trocado e liberado (cat_quadro) enquanto o fio localiza.
  cat_definir_tudo(a, NLOTE, NULL, 0);
  cat_quadro();
  // O fio fica no ar o tempo todo: cada pedido com ele vivo vale uma volta a
  // mais (locDeNovo). O fio principal publica e libera sem parar, ~3 s.
  { double t0 = cat_relogio_ms();
    for (volta = 0; cat_relogio_ms() - t0 < 3000.0; volta++) {
      desc_localizar_indices(idx, NLOTE);
      cat_definir_tudo((volta & 1) ? a : b, NLOTE, NULL, 0);
      cat_definir_tudo((volta & 1) ? b : a, NLOTE, NULL, 0);
      cat_quadro();                       // libera o bloco em que o fio podia ler
    } }
  esperar();
  cat_quadro();
  puts("ok  catalogo trocado no meio: o fio nao le bloco liberado");

  // 2. Outro escritor muda o item entre a leitura e a escrita do fio.
  memset(locCache, 0, sizeof locCache);
  semear("tt0000003", "Titulo traduzido");
  item(&a[0], "tt0000003", "English title");
  a[0].naLista = 0;
  cat_definir_tudo(a, 1, NULL, 0);
  cat_quadro();
  ganchoAlvo = 0; ganchoRodou = 0; ganchoArmado = 1;
  { int um[1] = { 0 }; desc_localizar_indices(um, 1); }
  esperar();
  ganchoArmado = 0;
  assert(ganchoRodou);
  assert(!strcmp(cat_item(0)->titulo, "Titulo traduzido"));
  assert(!strcmp(cat_item(0)->sinopse, "Sinopse no idioma."));
  assert(!strcmp(cat_item(0)->logo, "https://l.test/logo.png"));
  assert(!strcmp(cat_item(0)->backdrop, "https://l.test/fundo.jpg"));
  assert(!strcmp(cat_item(0)->backdropCatalogo, "https://l.test/fundo.jpg"));
  assert(cat_item(0)->naLista == 1);                   // a mudanca do outro fio fica
  puts("ok  titulo/sinopse/logo/fundo entram sem reverter o que outro fio mudou no item");

  puts("localizar_corrida: PASS");
  return 0;
}
