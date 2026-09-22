// Fim de fileira: o foco chega a ultima coluna e continua recebendo seta.
//
// Issue #65: "When reaching the end of a row, the application freezes and
// crashes." Este arquivo leva o foco ao fim de CADA fileira e alem — direita
// repetida no fim, baixo ate a ultima fileira e alem, cima ate o destaque e
// alem — chamando home_evento e home_atualizar como o loop principal faz, com
// pausas maiores que o repouso da gravacao de posicao e o da expansao.
//
// Corre sob ASAN/UBSan (SANITIZE=1). O que ele cobra:
//   (a) a coluna nunca passa de n (ou de n + 1 quando ha "Ver tudo");
//   (b) o indice de catalogo do card focado esta dentro de cat_n();
//   (c) nenhuma leitura fora de faixa em animFoco/fileiras/foco (o sanitizer);
//   (d) o mesmo percurso com a descoberta republicando o catalogo NUM OUTRO
//       FIO, encolhendo e crescendo fileiras — que e o cenario do comentario
//       de sincronizarFileiras.
//
// Sem janela, rede ou TV, como tests/homepos.c: inclui src/home.c direto.
#include <assert.h>
#include <pthread.h>
#include "../src/home.c"

char *dados_ler(const char *nome) { (void)nome; return NULL; }
int   dados_gravar(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
int   dados_gravar_leve(const char *nome, const char *c) { (void)nome; (void)c; return 1; }
char *dados_caminho(char *dst, unsigned tam, const char *nome) {
  (void)dst; (void)tam; (void)nome; return NULL;
}
int   dados_apagar(const char *nome) { (void)nome; return 1; }
int   perfis_ativo(void) { return 1; }
const char *addons_base_por_id(const char *id) { (void)id; return ""; }
const char *addons_nome_por_id(const char *id) { (void)id; return ""; }

// Dubles do que home_evento/home_atualizar alcancam e este teste nao quer:
// textura, menu contextual, "Ver tudo". Eles registram a chamada e nada mais.
static int nCtx, nVerTudo, nVerTudoCol;
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
// A home passou a esconder o heroi tambem com o player aberto (home.c:1726);
// aqui nao ha player nenhum, entao responde sempre fechado.
int  player_aberto(void) { return 0; }
int  detail_aberto(void) { return 0; }
int  trailer_aberto(void) { return 0; }
int  trailer_tocando(void) { return 0; }
void ctx_abrir(int indice) { (void)indice; nCtx++; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti; nVerTudo++;
}
void vertudo_colecao(const ColFolder *f) { (void)f; nVerTudoCol++; }
const char *i18n(const char *s) { return s; }

static Uint32 relogio = 1000;
static void quadro(int quantos) {
  for (int i = 0; i < quantos; i++) { relogio += 16; home_atualizar(0.016f, relogio); }
}
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
  quadro(1);
}

// O que tem de valer em QUALQUER estado do foco.
static void invariantes(const char *onde) {
  if (focoHero) return;
  if (!(foco.fileira >= 0 && foco.fileira < nFileiras)) {
    fprintf(stderr, "%s: fileira %d fora de [0,%d)\n", onde, foco.fileira, nFileiras);
    abort();
  }
  const Fileira *s = &fileiras[foco.fileira];
  int max = s->n + (s->verTudo ? 1 : 0);
  if (!(foco.coluna >= 0 && foco.coluna < max)) {
    fprintf(stderr, "%s: coluna %d fora de [0,%d) na fileira %d '%s'\n",
            onde, foco.coluna, max, foco.fileira, s->chave);
    abort();
  }
  if (foco.nColunas[foco.fileira] != max) {
    fprintf(stderr, "%s: nColunas %d != %d na fileira %d\n", onde,
            foco.nColunas[foco.fileira], max, foco.fileira);
    abort();
  }
  if (max > MAX_CARDS) { fprintf(stderr, "%s: %d colunas > MAX_CARDS\n", onde, max); abort(); }
  if (s->tipo != FILEIRA_CATALOGOS && s->tipo != FILEIRA_SOCIAL && foco.coluna < s->n) {
    int idx = s->ini + foco.coluna;
    // A fileira pode ser mais velha que o catalogo (cenario d): o indice pode
    // estar fora, e o que se cobra e que o app NAO o use as cegas.
    if (!(idx >= 0 && idx < cat_n()))
      fprintf(stderr, "%s: aviso: indice %d fora do catalogo (%d) na fileira '%s'\n",
              onde, idx, cat_n(), s->chave);
  }
}

// Percorre a home inteira: cada fileira ate o fim e alem.
static void percorrer(const char *onde) {
  // Comeca no destaque (UP na fileira 0), e sai dele para a fileira 0.
  tecla(SDLK_UP); tecla(SDLK_UP); tecla(SDLK_RIGHT);
  for (int i = 0; i < 40; i++) tecla(SDLK_RIGHT);   // fim do carrossel e alem
  tecla(SDLK_DOWN);
  invariantes(onde);
  for (int passo = 0; passo < FOCUS_MAX_FILEIRAS + 4; passo++) {
    int fAntes = foco.fileira;
    for (int i = 0; i < MAX_CARDS + 8; i++) { tecla(SDLK_RIGHT); invariantes(onde); }
    // Repouso: gravacao de posicao (1,2 s), expansao e troca do destaque.
    quadro(120);
    invariantes(onde);
    // OK no ultimo card / no "Ver tudo".
    { SDL_Event e; memset(&e, 0, sizeof e);
      e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN; home_evento(&e);
      quadro(2);
      e.type = SDL_KEYUP; home_evento(&e); quadro(1);
      (void)home_pediu_abrir(); (void)home_pediu_menu(); }
    invariantes(onde);
    for (int i = 0; i < 3; i++) { tecla(SDLK_LEFT); invariantes(onde); }
    tecla(SDLK_DOWN);
    invariantes(onde);
    if (foco.fileira == fAntes) break;   // ultima fileira: baixo nao move
  }
  for (int i = 0; i < 4; i++) { tecla(SDLK_DOWN); invariantes(onde); }
  for (int i = 0; i < FOCUS_MAX_FILEIRAS + 4; i++) { tecla(SDLK_UP); invariantes(onde); }
  for (int i = 0; i < 4; i++) { tecla(SDLK_UP); invariantes(onde); }
  for (int i = 0; i < MAX_CARDS + 4; i++) { tecla(SDLK_LEFT); (void)home_pediu_menu(); invariantes(onde); }
}

// Catalogo de teste: fileiras de tamanhos variados, inclusive uma cheia (12,
// o teto que da o card "Ver tudo" na coluna 12) e uma de um item.
#define N_ITENS 200
static CatItem itensA[N_ITENS];
static CatFileira filsA[16];
static void montarFils(CatFileira *f, int nf, int base, int variacao) {
  int ini = 0;
  for (int i = 0; i < nf; i++) {
    memset(&f[i], 0, sizeof f[i]);
    snprintf(f[i].chave, sizeof f[i].chave, "catalogo_%d", i);
    snprintf(f[i].titulo, sizeof f[i].titulo, "Lista %d", i);
    snprintf(f[i].base, sizeof f[i].base, "https://example.invalid/addon");
    snprintf(f[i].tipo, sizeof f[i].tipo, "movie");
    snprintf(f[i].catId, sizeof f[i].catId, "id%d", i);
    f[i].ini = ini;
    f[i].n = base + ((i * 7 + variacao) % 13);   // 1..13+
    ini += f[i].n;
  }
  snprintf(f[0].chave, sizeof f[0].chave, "continue_watching");
  f[0].base[0] = f[0].catId[0] = 0;
  snprintf(f[1].titulo, sizeof f[1].titulo, "Top 10 - Filme");
}

// Cenario (d): a descoberta republica enquanto o fio de desenho navega.
static volatile int republicando = 1;
static void *fioRepublicar(void *u) {
  static CatItem itensB[N_ITENS];
  static CatFileira filsB[16];
  int v = 0;
  (void)u;
  for (int i = 0; i < N_ITENS; i++) snprintf(itensB[i].imdb, sizeof itensB[i].imdb, "tt%05d", 5000 + i);
  while (republicando) {
    int nf = 3 + (v % 14);
    int qtd = 20 + (v * 37) % (N_ITENS - 20);
    montarFils(filsB, nf, 1 + (v % 3), v);
    cat_definir_tudo(itensB, qtd, filsB, nf);
    v++;
    struct timespec ts = { 0, 200000 };   // 0,2 ms entre publicacoes
    nanosleep(&ts, NULL);
  }
  return NULL;
}

int main(void) {
  fil_definir_limite(FIL_LIMITE_MAX);
  assert(MAX_FIL <= FOCUS_MAX_FILEIRAS);
  for (int i = 0; i < N_ITENS; i++) {
    snprintf(itensA[i].imdb, sizeof itensA[i].imdb, "tt%05d", i);
    snprintf(itensA[i].titulo, sizeof itensA[i].titulo, "Titulo %d", i);
    snprintf(itensA[i].tipo, sizeof itensA[i].tipo, "movie");
  }
  montarFils(filsA, 16, 1, 0);
  filsA[3].n = 12;   // fileira CHEIA: 12 cartazes + "Ver tudo" na coluna 12
  filsA[5].n = 1;
  filsA[7].n = 40;   // mais do que a home aceita: cortada em 12
  { int ini = 0; for (int i = 0; i < 16; i++) { filsA[i].ini = ini; ini += filsA[i].n; } }

  cat_definir_tudo(itensA, N_ITENS, filsA, 16);
  quadro(1);
  assert(nFileiras > 3);
  int temCheia = 0;
  for (int r = 0; r < nFileiras; r++)
    if (fileiras[r].n == 12 && fileiras[r].verTudo) temCheia = 1;
  assert(temCheia);

  // (a)(b)(c): catalogo parado.
  percorrer("parado");
  printf("fim de fileira: percurso com catalogo parado OK (%d fileiras)\n", nFileiras);

  // Fileiras que ENCOLHEM com o foco no fim: republica menor, e o foco tem de
  // cair dentro da fileira nova, nunca alem dela.
  { CatFileira filsC[16];
    montarFils(filsC, 16, 1, 0);
    for (int i = 0; i < 16; i++) filsC[i].n = 2;
    { int ini = 0; for (int i = 0; i < 16; i++) { filsC[i].ini = ini; ini += 2; } }
    tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
    for (int i = 0; i < 20; i++) tecla(SDLK_RIGHT);
    invariantes("antes de encolher");
    cat_definir_tudo(itensA, 40, filsC, 16);
    quadro(1);
    invariantes("depois de encolher");
    for (int i = 0; i < 20; i++) { tecla(SDLK_RIGHT); invariantes("encolhida"); }
    printf("fim de fileira: encolher com o foco no fim OK\n");
  }

  // (d): republicacao concorrente.
  pthread_t t;
  assert(pthread_create(&t, NULL, fioRepublicar, NULL) == 0);
  for (int volta = 0; volta < 6; volta++) percorrer("concorrente");
  republicando = 0;
  pthread_join(t, NULL);
  percorrer("depois do concorrente");
  printf("fim de fileira: percurso com republicacao concorrente OK\n");
  printf("fim de fileira: PASS (ctx=%d vertudo=%d colecao=%d)\n", nCtx, nVerTudo, nVerTudoCol);
  return 0;
}
