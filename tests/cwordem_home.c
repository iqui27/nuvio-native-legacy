// "SEPARAR FUTUROS" DA UMA FILEIRA PROPRIA NA HOME (issue #127). Parte de
// tests/cwordem.sh; a regra pura esta em tests/cwordem.c e a montagem em
// tests/cwordem_desc.c. Inclui src/home.c direto, como tests/cwremover_home.c.
//
// O que se cobra: com a ordenacao em "Separar futuros", os cards que a
// montagem publicou como futuros (cwo_publicar_futuros) saem de "Continuar
// assistindo" e aparecem numa fileira "Proximos episodios" LOGO ABAIXO dela;
// nos outros modos a fileira fica inteira e a de futuros nao existe.
#include <assert.h>
// Duples do cache de arte do Codex (mesmos de tests/home_layout.c).
void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso; }
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso; }
#include "../src/home.c"
#include "../src/cwordem.h"

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }
void ling_conta_legenda(const char *v) { (void)v; }
void ling_conta_legenda2(const char *v) { (void)v; }
void ling_conta_audio(const char *v) { (void)v; }
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti;
}
void vertudo_colecao(const ColFolder *f) { (void)f; }
const char *i18n(const char *s) { return s; }
// A troca de ordenacao em Ajustes refaz a fileira pela descoberta; aqui a
// montagem nao existe, entao o pedido so e contado.
void desc_refazer_continuar(void) {}

static Uint32 relogio = 1000;
static void quadro(void) { relogio += 16; home_atualizar(0.016f, relogio); }

static const Fileira *naHome(const char *chave) {
  int r;
  for (r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) return &fileiras[r];
  return NULL;
}
static int contar(const char *chave) {
  int r, q = 0;
  for (r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) q++;
  return q;
}
static int posicao(const char *chave) {
  int r;
  for (r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) return r;
  return -1;
}
static const char *card(const Fileira *f, int c) {
  const CatItem *it = cat_item(fileiraItemIndice(f, c));
  return it ? it->imdb : "";
}

int main(void) {
  static CatItem it[5];
  static CatFileira fs[2];
  const Fileira *cw, *prox;
  const char *fut1[1] = { "tt1:1:2" };
  const char *futTodos[3] = { "tt0", "tt1:1:2", "tt2:1:5" };
  int i;
  fil_definir_limite(FIL_LIMITE_MAX);
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  for (i = 0; i < 5; i++) {
    snprintf(it[i].titulo, sizeof it[i].titulo, "T%d", i);
    snprintf(it[i].tipo, sizeof it[i].tipo, "%s", (i == 0 || i > 2) ? "movie" : "series");
  }
  snprintf(it[0].imdb, sizeof it[0].imdb, "tt0");     it[0].progresso = 40;
  snprintf(it[1].imdb, sizeof it[1].imdb, "tt1:1:2"); // a seguir, estreia futura
  snprintf(it[2].imdb, sizeof it[2].imdb, "tt2:1:5"); // a seguir, ja exibido
  snprintf(it[3].imdb, sizeof it[3].imdb, "tt3");
  snprintf(it[4].imdb, sizeof it[4].imdb, "tt4");
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar assistindo");
  fs[0].ini = 0; fs[0].n = 3;
  snprintf(fs[1].chave, sizeof fs[1].chave, "lista");
  snprintf(fs[1].titulo, sizeof fs[1].titulo, "Lista");
  snprintf(fs[1].base, sizeof fs[1].base, "https://addon.invalid");
  snprintf(fs[1].catId, sizeof fs[1].catId, "top");
  fs[1].ini = 3; fs[1].n = 2;
  cwo_publicar_futuros(fut1, 1);
  cat_definir_tudo(it, 5, fs, 2);
  quadro(); quadro();

  // PADRAO: a fileira inteira, sem a de futuros.
  assert(ajustes_cw_ordem() == CWO_PADRAO);
  cw = naHome("continue_watching");
  assert(cw && cw->n == 3 && !naHome("upcoming_section"));
  puts("ok  padrao: futuro continua em Continuar assistindo");

  ajustes_aplicar_blob("{\"continueWatchingSortMode\":\"streaming_style\"}");
  assert(ajustes_cw_ordem() == CWO_STREAMING);
  quadro();
  cw = naHome("continue_watching");
  assert(cw && cw->n == 3 && !naHome("upcoming_section"));
  puts("ok  streaming: ainda uma fileira so");

  // SEPARAR: o futuro sai e ganha fileira logo abaixo.
  ajustes_aplicar_blob("{\"continueWatchingSortMode\":\"split_upcoming\"}");
  assert(ajustes_cw_ordem() == CWO_SEPARAR);
  quadro();
  cw = naHome("continue_watching");
  prox = naHome("upcoming_section");
  assert(cw && prox);
  assert(cw->n == 2 && !strcmp(card(cw, 0), "tt0") && !strcmp(card(cw, 1), "tt2:1:5"));
  assert(prox->n == 1 && !strcmp(card(prox, 0), "tt1:1:2"));
  assert(posicao("upcoming_section") == posicao("continue_watching") + 1);
  assert(!strcmp(prox->titulo, "Próximos episódios"));
  assert(prox->tipo == cw->tipo && !prox->verTudo);
  assert(naHome("lista") && naHome("lista")->n == 2);
  assert(!strcmp(card(naHome("lista"), 0), "tt3"));
  puts("ok  separar: futuro numa fileira propria, logo abaixo, lista intacta");

  // A HOME REMONTADA N VEZES DA A MESMA HOME. Na C9 do dono (24/09) o log
  // subia "19 fileiras na tela" -> 20 -> 21 com o catalogo parado em 16, e ele
  // viu "Proximos episodios" duplicada. Cada caminho que republica passa aqui
  // varias vezes: quadro sem mudanca, refacao de Continuar assistindo (a
  // mesma e a do Trakt cedo), remontagem sem rede, o mesmo conjunto de
  // futuros de novo, ida e volta da Ordenacao, catalogo inteiro de novo.
  { int base = nFileiras, pos = posicao("upcoming_section"), k;
    assert(contar("upcoming_section") == 1);
    for (k = 0; k < 18; k++) {
      switch (k % 6) {
        case 0: quadro(); break;
        case 1: cat_trocar_continuar(it, 3); quadro(); break;
        case 2: cat_republicar_fileiras(fs, 2); quadro(); break;
        case 3: cwo_publicar_futuros(fut1, 1); cat_trocar_continuar(it, 3); quadro(); break;
        case 4: ajustes_aplicar_blob("{\"continueWatchingSortMode\":\"default\"}"); quadro();
                assert(contar("upcoming_section") == 0);
                ajustes_aplicar_blob("{\"continueWatchingSortMode\":\"split_upcoming\"}"); quadro(); break;
        case 5: cat_definir_tudo(it, 5, fs, 2); quadro(); break;
      }
      assert(contar("upcoming_section") == 1);
      assert(contar("continue_watching") == 1);
      assert(posicao("upcoming_section") == pos);
      assert(nFileiras == base);
    }
    printf("ok  separar: 18 remontagens, %d fileiras e uma so de futuros\n", base);
  }

  // Chave repetida vinda de baixo (catalogo publicado com a mesma fileira duas
  // vezes) sai com uma so: a home e idempotente sobre o que chega.
  { static CatFileira dup[3];
    int base = nFileiras;
    dup[0] = fs[0]; dup[1] = fs[1]; dup[2] = fs[1];
    cat_definir_tudo(it, 5, dup, 3);
    quadro(); quadro();
    assert(contar("lista") == 1 && contar("upcoming_section") == 1);
    assert(nFileiras == base);
    cat_definir_tudo(it, 5, fs, 2);
    quadro();
    assert(nFileiras == base);
    puts("ok  chave repetida no catalogo: uma fileira so");
  }

  // TUDO FUTURO: nao sobra cabecalho vazio de Continuar assistindo.
  cwo_publicar_futuros(futTodos, 3);
  cat_trocar_continuar(it, 3);
  quadro();
  prox = naHome("upcoming_section");
  assert(!naHome("continue_watching") && prox && prox->n == 3);
  puts("ok  separar: so futuros -> so a fileira de futuros");

  // Continuar assistindo DESLIGADO leva a de futuros junto.
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":false}");
  quadro();
  assert(!naHome("continue_watching") && !naHome("upcoming_section"));
  puts("ok  retomada desligada: nenhuma das duas");
  puts("cwordem_home: tudo ok");
  return 0;
}
