// Retorno de fim de fileira: a seta que bate na borda desloca a fileira (ou a
// pagina) na direcao da tecla e a mola devolve a zero, sem mexer no foco.
//
// Duas partes: (1) a mola AnimBorda sozinha (anim.h) — pico, volta, sinal,
// repouso e animacoes reduzidas; (2) a home de verdade (inclui src/home.c,
// como tests/fimfileira.c): Direita no ultimo cartao, Esquerda na coluna 0
// (continua abrindo o menu, sem batida) e Cima no destaque.
#include <assert.h>
int ctx_aberto(void) { return 0; }   // home.c asks whether the context menu is open
#include "../src/home.c"
// #204: o "Ocultar filmes assistidos" e o cartao do QR da home sem add-ons,
// parados neste teste (descoberta.c e homevazio.c ficam fora do link).
void desc_ocultar_vistos_atualizar(void) {}
int  homevazio_ativo(int fileiras) { (void)fileiras; return 0; }
int  homevazio_evento(const SDL_Event *e) { (void)e; return 0; }
// heroAquecerVizinhos (home_atualizar) pede a arte dos vizinhos: sem textura aqui.
unsigned long tex_hash_public(const char *c) { (void)c; return 0; }
GLuint tex_obter_hero_quente(const char *c) { (void)c; return 0; }
GLuint tex_obter_larg(const char *c, float w) { (void)c; (void)w; return 0; }
GLuint tex_obter_logo_larg_qualquer(const char *c, float w) { (void)c; (void)w; return 0; }
void marco(const char *m) { (void)m; }
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }


void cachearte_marcar_grupo(int grupo, const char *url, int variante, int essencial, int emUso) {
  (void)grupo; (void)url; (void)variante; (void)essencial; (void)emUso;
}
void cachearte_limpar_referencias_grupo(int grupo) { (void)grupo; }
void cachearte_estatisticas_pedir(void) {}
void tex_cache_marcar_larg(int grupo, const char *url, float larg, int essencial, int emUso) {
  (void)grupo; (void)url; (void)larg; (void)essencial; (void)emUso;
}

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
void ctx_fileira(const char *c, const char *t) { (void)c; (void)t; }
void ctx_dispensar_retomar(int on) { (void)on; }
void ctx_abrir_fileira(const char *c, const char *t) { (void)c; (void)t; nCtx++; }
void vertudo_abrir(const char *b, const char *t, const char *c, const char *ti) {
  (void)b; (void)t; (void)c; (void)ti; nVerTudo++;
}
void vertudo_colecao(const ColFolder *f) { (void)f; nVerTudoCol++; }
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }
float tex_aspecto(const char *caminho) { (void)caminho; return 0.0f; }

static Uint32 relogio = 1000;
static void quadro(int quantos) {
  for (int i = 0; i < quantos; i++) { relogio += 16; home_atualizar(0.016f, relogio); }
}
static void teclaSo(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
}

static void parteMola(void) {
  AnimBorda b = {0};
  assert(!anim_borda_passo(&b, 0.016f, 0));          // parado: nada a fazer
  assert(b.x == 0.0f);
  anim_borda_bater(&b, 20.0f);
  float pico = 0.0f, tPico = 0.0f, t = 0.0f, ultimo = 0.0f;
  int ativo = 1;
  while (ativo && t < 2.0f) {
    ativo = anim_borda_passo(&b, 0.016f, 0);
    t += 0.016f;
    assert(b.x >= 0.0f);                             // nunca passa para o outro lado
    if (b.x > pico) { pico = b.x; tPico = t; }
    ultimo = b.x;
  }
  printf("mola: pico %.2f px aos %.0f ms, repouso aos %.0f ms\n", pico, tPico * 1000, t * 1000);
  assert(pico >= 17.5f && pico <= 20.0f);
  assert(tPico < 0.12f);
  assert(!ativo && ultimo == 0.0f && t < 0.9f);
  // Sinal: esquerda / cima.
  anim_borda_bater(&b, -20.0f);
  for (int i = 0; i < 4; i++) anim_borda_passo(&b, 0.016f, 0);
  assert(b.x < -10.0f);
  // Reduzida no meio do movimento: zera na hora.
  assert(!anim_borda_passo(&b, 0.016f, 1));
  assert(b.x == 0.0f && b.alvo == 0.0f);
  // Politica reduzida: bater nem arma.
  anim_politica_reduzida = 1;
  anim_borda_bater(&b, 20.0f);
  assert(b.alvo == 0.0f && !anim_borda_passo(&b, 0.016f, 0) && b.x == 0.0f);
  anim_politica_reduzida = 0;
  printf("mola: OK\n");
}

static float maxAbs(AnimBorda *b, int quadros) {
  float m = 0.0f;
  for (int i = 0; i < quadros; i++) { quadro(1); if (fabsf(b->x) > m) m = fabsf(b->x); }
  return m;
}

static CatItem itens[40];
static CatFileira fils[3];
static void parteHome(void) {
  fil_definir_limite(FIL_LIMITE_MAX);
  for (int i = 0; i < 40; i++) {
    snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt%05d", i);
    snprintf(itens[i].titulo, sizeof itens[i].titulo, "Titulo %d", i);
    snprintf(itens[i].tipo, sizeof itens[i].tipo, "movie");
  }
  int ini = 0;
  for (int i = 0; i < 3; i++) {
    memset(&fils[i], 0, sizeof fils[i]);
    snprintf(fils[i].chave, sizeof fils[i].chave, "catalogo_%d", i);
    snprintf(fils[i].titulo, sizeof fils[i].titulo, "Lista %d", i);
    snprintf(fils[i].base, sizeof fils[i].base, "https://example.invalid/addon");
    snprintf(fils[i].tipo, sizeof fils[i].tipo, "movie");
    snprintf(fils[i].catId, sizeof fils[i].catId, "id%d", i);
    fils[i].ini = ini; fils[i].n = 4; ini += 4;
  }
  cat_definir_tudo(itens, 40, fils, 3);
  quadro(1);
  assert(nFileiras >= 1);
  focoHero = 0; foco.fileira = 0; foco.coluna = 0;
  quadro(60);
  int r = foco.fileira;
  int nCol = foco.nColunas[r];
  assert(nCol >= 2);

  // Direita ate o ultimo cartao: enquanto move, nenhuma batida.
  for (int i = 0; i < nCol - 1; i++) { teclaSo(SDLK_RIGHT); quadro(1); }
  assert(foco.coluna == nCol - 1);
  quadro(60);
  assert(bordaFil[r].x == 0.0f);
  // Mais uma: foco fica, a fileira desloca para a DIREITA e volta a zero.
  teclaSo(SDLK_RIGHT);
  assert(foco.coluna == nCol - 1 && foco.fileira == r && !focoHero);
  quadro(4);
  assert(bordaFil[r].x > 10.0f);
  for (int o = 0; o < nFileiras; o++) if (o != r) assert(bordaFil[o].x == 0.0f);
  assert(bordaPag.x == 0.0f);
  quadro(60);
  assert(bordaFil[r].x == 0.0f && bordaFil[r].alvo == 0.0f);
  assert(foco.coluna == nCol - 1);
  printf("home: direita no ultimo cartao bate e volta, foco parado OK\n");

  // Esquerda ate a coluna 0 e mais uma: abre o menu, sem batida.
  for (int i = 0; i < nCol - 1; i++) { teclaSo(SDLK_LEFT); quadro(1); }
  assert(foco.coluna == 0);
  (void)home_pediu_menu();
  quadro(60);
  teclaSo(SDLK_LEFT);
  assert(home_pediu_menu() == 1);
  assert(maxAbs(&bordaFil[r], 30) == 0.0f);
  printf("home: esquerda na coluna 0 continua abrindo o menu OK\n");

  // Cima na fileira 0 vai ao destaque (sem batida); Cima no destaque bate.
  teclaSo(SDLK_UP);
  assert(focoHero);
  assert(maxAbs(&bordaPag, 60) == 0.0f);
  (void)home_pediu_ilha();
  teclaSo(SDLK_UP);
  assert(focoHero);
  assert(home_pediu_ilha() == 1);   // #495: CIMA no destaque pede a ilha
  quadro(4);
  assert(bordaPag.x < -10.0f);      // a pagina sobe
  quadro(60);
  assert(bordaPag.x == 0.0f);
  printf("home: cima no destaque bate e volta OK\n");

  // Politica reduzida: nenhuma batida.
  anim_politica_reduzida = 1;
  teclaSo(SDLK_UP);
  assert(maxAbs(&bordaPag, 30) == 0.0f);
  teclaSo(SDLK_DOWN); quadro(2);
  for (int i = 0; i < nCol + 2; i++) { teclaSo(SDLK_RIGHT); quadro(1); }
  assert(maxAbs(&bordaFil[foco.fileira], 30) == 0.0f);
  anim_politica_reduzida = 0;
  printf("home: animacoes reduzidas sem movimento OK\n");
}

int main(void) {
  parteMola();
  parteHome();
  printf("bordafileira: PASS\n");
  return 0;
}
