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
  dados_iniciar(getenv("NUVIO_DADOS"));
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

  // #672: Home applies the chosen order and unwatched filter itself.
  {
    Fileira b;
    static CatItem dados[15];
    for (int i = 0; i < 15; i++) {
      char id[16]; snprintf(id, sizeof id, "tt%d", 500 + i);
      item(&dados[i], id, 1);
      snprintf(dados[i].titulo, sizeof dados[i].titulo, "%c", 'Z' - i);
    }
    snprintf(dados[14].tipo, sizeof dados[14].tipo, "series");
    cat_definir_tudo(dados, 15, fs, 2);
    cat_historico_definir_id("tt514", "series", 1);
    cat_historico_definir_id("tt513", "movie", 1);
    assert(fil_definir_ordem("library", FIL_ORD_AZ));
    fil_recarregar();
    assert(fil_ordem("library") == FIL_ORD_AZ);
    montarBiblioteca(&b);
    assert(b.n == ajustes_itens_fileira());
    assert(!strcmp(cat_item(b.itens[0])->imdb, "tt514"));
    ov_alternar("library");
    ov_definir_perfil(2); ov_definir_perfil(1);
    assert(ov_ativa("library"));
    montarBiblioteca(&b);
    assert(b.n == ajustes_itens_fileira() && b.verTudo);
    assert(!strcmp(cat_item(b.itens[0])->imdb, "tt512"));
    for (int i = 0; i < b.n; i++) assert(!cat_visto(cat_item(b.itens[i])));
    for (int i = 0; i < 13; i++) cat_historico_definir_id(dados[i].imdb, dados[i].tipo, 1);
    cat_definir_tudo(dados, 15, fs, 2);
    quadro();
    { const Fileira *v = naHome("library");
      assert(v && v->n == 0 && v->verTudo);
      focoHero = 0; foco.fileira = (int)(v - fileiras); foco.coluna = 0;
      assert(foco_pode_pressao_longa());
      abrirMenuCartaz(); assert(ctx_aberto());
      SDL_Event e = {0};
      e.type = SDL_KEYUP; e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN; ctx_evento(&e); ctx_evento(&e);
      e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
      assert(!ov_ativa("library"));
      e.key.keysym.sym = SDLK_DOWN; ctx_evento(&e);
      e.key.keysym.sym = SDLK_RETURN; ctx_evento(&e);
      assert(fil_ordem("library") == FIL_ORD_LISTA);
      ctx_abrir_sessao(); }
  }

  // Ordering must handle duration-only metadata, case and Latin accents even
  // when the best matches occur after the row limit.
  {
    static CatItem dados[15]; Fileira b;
    for (int i = 0; i < 15; i++) {
      char id[16]; snprintf(id, sizeof id, "tt%d", 700 + i);
      item(&dados[i], id, 1);
      snprintf(dados[i].titulo, sizeof dados[i].titulo, "Zulu %d", i);
      if (i < 11) snprintf(dados[i].meta, sizeof dados[i].meta, "%d", 2000 + i);
      else snprintf(dados[i].meta, sizeof dados[i].meta, "90 min");
    }
    snprintf(dados[12].titulo, sizeof dados[12].titulo, "banana");
    snprintf(dados[13].titulo, sizeof dados[13].titulo, "Água");
    snprintf(dados[14].titulo, sizeof dados[14].titulo, "abacate");
    snprintf(dados[13].meta, sizeof dados[13].meta, "2022 · 90 min");
    snprintf(dados[14].meta, sizeof dados[14].meta, "2024 · 90 min");
    cat_definir_tudo(dados, 15, fs, 2);
    assert(fil_definir_ordem("library", FIL_ORD_ANO));
    montarBiblioteca(&b);
    assert(!strcmp(cat_item(b.itens[0])->imdb, "tt714"));
    assert(!strcmp(cat_item(b.itens[1])->imdb, "tt713"));
    for (int i = 0; i < b.n; i++) assert(nv_ano_meta(cat_item(b.itens[i])->meta));
    assert(fil_definir_ordem("library", FIL_ORD_AZ));
    montarBiblioteca(&b);
    assert(!strcmp(cat_item(b.itens[0])->imdb, "tt714"));
    assert(!strcmp(cat_item(b.itens[1])->imdb, "tt713"));
    assert(!strcmp(cat_item(b.itens[2])->imdb, "tt712"));
  }

  assert(ajustes_shot_valor("libraryRowLocal", 1) && !ajustes_bib_home());
  quadro();
  assert(!naHome("library"));
  puts("ok  off again: row gone");
  return 0;
}
