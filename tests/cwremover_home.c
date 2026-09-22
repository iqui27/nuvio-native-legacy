// A HOME REMONTA NO MESMO QUADRO em que "Tirar de Continuar assistindo" tira o
// card. Metade de tests/cwremover.sh; a outra (fileira publicada e refacao)
// esta em tests/cwremover.c.
//
// O DEFEITO MEDIDO AQUI (22/09): sincronizarFileiras ganhou na 1.4 um guarda
// curto por contadores (cat_revisao, fil_revisao, col_revisao, numero de
// fileiras). A remocao encolhia a janela sem subir nenhum deles, e a home
// seguia com n=3 numa fileira que ja era n=2: o card tirado era coberto pelo
// vizinho, o ultimo aparecia duas vezes (o slot orfao) e so a proxima
// republicacao acertava. Sem janela, rede ou TV: inclui src/home.c direto,
// como tests/fimfileira.c.
#include <assert.h>
#include "../src/home.c"

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
void  dados_marcar_sujo(int leve) { (void)leve; }
// ajustes_aplicar_blob (abaixo) fala com linguas.c; nada disso entra na regra.
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

static Uint32 relogio = 1000;
static void quadro(void) { relogio += 16; home_atualizar(0.016f, relogio); }

static const Fileira *naHome(const char *chave) {
  int r;
  for (r = 0; r < nFileiras; r++) if (!strcmp(fileiras[r].chave, chave)) return &fileiras[r];
  return NULL;
}
static const char *card(const Fileira *f, int c) {
  const CatItem *it = cat_item(fileiraItemIndice(f, c));
  return it ? it->imdb : "";
}

int main(void) {
  static CatItem it[5];
  static CatFileira fs[2];
  const Fileira *cw, *lista;
  int i;
  fil_definir_limite(FIL_LIMITE_MAX);
  // A fileira de retomada vem LIGADA do perfil; sem arquivo de ajustes o
  // padrao deste binario a desliga, e a home nem a montaria.
  ajustes_aplicar_blob("{\"continueWatchingEnabled\":true}");
  for (i = 0; i < 5; i++) {
    snprintf(it[i].imdb, sizeof it[i].imdb, "tt%d", i);
    snprintf(it[i].titulo, sizeof it[i].titulo, "T%d", i);
    snprintf(it[i].tipo, sizeof it[i].tipo, "movie");
    it[i].progresso = i < 3 ? 40 : 0;
  }
  snprintf(fs[0].chave, sizeof fs[0].chave, "continue_watching");
  snprintf(fs[0].titulo, sizeof fs[0].titulo, "Continuar assistindo");
  fs[0].ini = 0; fs[0].n = 3;
  snprintf(fs[1].chave, sizeof fs[1].chave, "lista");
  snprintf(fs[1].titulo, sizeof fs[1].titulo, "Lista");
  snprintf(fs[1].base, sizeof fs[1].base, "https://addon.invalid");
  snprintf(fs[1].catId, sizeof fs[1].catId, "top");
  fs[1].ini = 3; fs[1].n = 2;
  cat_definir_tudo(it, 5, fs, 2);
  quadro(); quadro();
  cw = naHome("continue_watching");
  assert(cw && cw->n == 3);

  // O gesto: a mesma chamada que desc_tirar_continuar faz (ctxmenu.c), e UM
  // quadro de home_atualizar depois — o quadro em que o menu fecha.
  assert(cat_tirar_continuar("tt1") == 1);
  quadro();
  cw = naHome("continue_watching");
  assert(cw);
  if (cw->n != 2) {
    fprintf(stderr, "home ainda com %d cards em Continuar assistindo (esperado 2): "
            "%s %s %s\n", cw->n, card(cw, 0), card(cw, 1), cw->n > 2 ? card(cw, 2) : "");
    return 1;
  }
  assert(!strcmp(card(cw, 0), "tt0") && !strcmp(card(cw, 1), "tt2"));
  lista = naHome("lista");
  assert(lista && lista->n == 2);
  assert(!strcmp(card(lista, 0), "tt3") && !strcmp(card(lista, 1), "tt4"));
  puts("ok  home remonta no mesmo quadro: tt1 fora, sem card repetido, lista intacta");

  // O ULTIMO card: a fileira sai da home, nao fica um cabecalho vazio.
  assert(cat_tirar_continuar("tt0") == 1 && cat_tirar_continuar("tt2") == 1);
  quadro();
  assert(!naHome("continue_watching"));
  lista = naHome("lista");
  assert(lista && !strcmp(card(lista, 0), "tt3") && !strcmp(card(lista, 1), "tt4"));
  puts("ok  tirar o ultimo card tira a fileira da home");
  puts("cwremover_home: tudo ok");
  return 0;
}
