// #507: "Fileira da Biblioteca" on Home. Off by default: no row. On: one row
// with the titles saved in the Library's source, one card per title, and a
// title saved later shows up without a new catalog publication.
//
//   bash tests/bibhome.sh
#include <assert.h>
#include "../src/home.c"

static Uint32 relogio = 1000;
static void quadro(void) { relogio += 16; home_atualizar(0.016f, relogio); }

static const Fileira *naHome(const char *chave) {
  for (int r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) return &fileiras[r];
  return NULL;
}
static void conferir(const char *quando, const char *esperado) {
  const Fileira *b = naHome("library");
  char tem[200] = "";
  for (int c = 0; b && c < b->n; c++) {
    const CatItem *it = cat_item(fileiraItemIndice(b, c));
    if (c) strcat(tem, " ");
    strcat(tem, it ? it->imdb : "?");
  }
  if (strcmp(tem, esperado)) {
    fprintf(stderr, "FAIL %s: library = [%s], expected [%s]\n", quando, tem, esperado);
    exit(1);
  }
  printf("ok  %s: [%s]\n", quando, tem);
}
static void item(CatItem *it, const char *imdb, int salvo) {
  memset(it, 0, sizeof *it);
  snprintf(it->imdb, sizeof it->imdb, "%s", imdb);
  snprintf(it->titulo, sizeof it->titulo, "T %s", imdb);
  snprintf(it->tipo, sizeof it->tipo, "movie");
  if (salvo) { it->naLista = 1; it->listaFontes = 1u << ajustes_fonte_biblioteca(); }
}

int main(void) {
  static CatItem it[5];
  static CatFileira fs[2];
  fil_definir_limite(FIL_LIMITE_MAX);
  item(&it[0], "tt100", 0); item(&it[1], "tt200", 1); item(&it[2], "tt300", 0);
  item(&it[3], "tt200", 1);   // the same saved title in a second row
  item(&it[4], "tt400", 1);
  snprintf(fs[0].chave, sizeof fs[0].chave, "a"); fs[0].ini = 0; fs[0].n = 3;
  snprintf(fs[1].chave, sizeof fs[1].chave, "b"); fs[1].ini = 3; fs[1].n = 2;
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "A"); snprintf(fs[1].titulo, sizeof fs[1].titulo, "B");
  cat_definir_tudo(it, 5, fs, 2);
  // Sem addon nenhum a Home e a tela-vazia (homevazio_ativo), que engole o OK
  // antes de ele virar "Ver tudo". Um addon de catalogo basta para desarma-la.
  { AddonRemoto a = {0};
    snprintf(a.nome, sizeof a.nome, "cat"); snprintf(a.url, sizeof a.url, "https://example.invalid"); a.ativo = 1;
    addons_definir_lista(&a, 1); }

  assert(!ajustes_bib_home());
  quadro(); quadro();
  assert(!naHome("library"));
  puts("ok  off by default: no Library row");

  assert(ajustes_shot_valor("libraryRowLocal", 0) && ajustes_bib_home());
  quadro();
  conferir("on", "tt200 tt400");
  { const Fileira *b = naHome("library");
    assert(b->tipo == FILEIRA_NORMAL && !strcmp(b->titulo, i18n("Biblioteca")));
    // #672: o card "Ver tudo" fecha a fileira, como nas de catalogo.
    assert(b->verTudo && b->catTipo[0]); }

  // #672: OK no "Ver tudo" pede a tela da Biblioteca (o roteador troca de
  // tela), nao uma grade vertudo — a fileira nao vem de catalogo de addon.
  focoHero = 0;
  { int r; for (r = 0; r < nFileiras; r++)
      if (!strcmp(fileiras[r].chave, "library")) break;
    assert(r < nFileiras);
    foco.fileira = r; foco.coluna = fileiras[r].n;
    if (r < FOCUS_MAX_FILEIRAS) foco.nColunas[r] = fileiras[r].n + 1; }
  { SDL_Event e = {0};
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; home_evento(&e);
    e.type = SDL_KEYUP;   e.key.keysym.sym = SDLK_RETURN; home_evento(&e); }
  assert(home_pediu_biblioteca() && !home_pediu_abrir());

  cat_definir_na_lista(0, 1);   // the Detail "+" on tt100
  quadro();
  conferir("saved later", "tt100 tt200 tt400");

  assert(ajustes_shot_valor("libraryRowLocal", 1) && !ajustes_bib_home());
  quadro();
  assert(!naHome("library"));
  puts("ok  off again: row gone");
  return 0;
}
