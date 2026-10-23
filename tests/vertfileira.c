// Descer/subir de fileira pousa na MESMA POSICAO x da tela (issue #205).
//
// Dono: "Quando desce de uma fileira pra outra nao ir pro inicio da fileira
// descer na mesma posicao". O defeito: o focus_mover pousa na COLUNA
// LEMBRADA da fileira nova, que nasce em 0 — descer era um pulo para o
// comeco da fileira de baixo.
//
// Regra cobrada aqui, pelas teclas de verdade (home_evento, sem janela):
//   - foco no cartaz ~4/6 da fileira A, DOWN: o cartaz focado na fileira B
//     e o de centro x mais proximo do cartaz que estava em foco (e nao o
//     primeiro da fileira, e nao a coluna de mesmo indice quando as
//     larguras/rolagens diferem);
//   - UP tem a mesma regra, na volta;
//   - fileira mais curta que o alvo: o pouso fica dentro dela.
//
// Sem janela, rede ou TV, como tests/fimfileira.c: inclui src/home.c direto.
#include <assert.h>
#include <stdio.h>
#include <string.h>
int ctx_aberto(void) { return 0; }   // home.c asks whether the context menu is open
#include "../src/home.c"
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

// Dubles do que home_evento alcancam e este teste nao quer: textura, menu
// contextual, "Ver tudo", home vazia. Registram a chamada e nada mais.
static int nCtx, nVerTudo, nVerTudoCol;
int  tex_falhou(const char *u) { (void)u; return 0; }
int  tex_largura_fonte(const char *u) { (void)u; return 0; }
const char *tex_arquivo(const char *u) { (void)u; return NULL; }
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
int  homevazio_ativo(int fileiras) { (void)fileiras; return 0; }
int  homevazio_evento(const SDL_Event *e) { (void)e; return 0; }
void desc_ocultar_vistos_atualizar(void) {}
float tex_aspecto(const char *c) { (void)c; return 0.0f; }

static Uint32 relogio = 1000;
Uint32 SDL_GetTicks(void) { return relogio; }
static void tecla(SDL_Keycode k) {
  SDL_Event e; memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
  home_evento(&e);
}

// Centro x do cartaz `c` da fileira `r`, como o desenho o poe na tela.
static float cxDe(int r, int c) { return homeFileiraX(r, c) + larguraFil(r) * 0.5f; }
// A coluna de `r` cujo centro x esta mais perto de `cx` (a regra pedida).
static int maisProximo(int r, float cx) {
  int melhor = 0;
  for (int c = 1; c < foco.nColunas[r]; c++)
    if (fabsf(cxDe(r, c) - cx) < fabsf(cxDe(r, melhor) - cx)) melhor = c;
  return melhor;
}

int main(void) {
  // Tres fileiras montadas A MAO, vizinhas e de larguras diferentes: o alvo
  // aqui e a REGRA DE POUSO do foco, nao a montagem do catalogo (que insere
  // fileiras fixas no meio e trocaria os vizinhos). Posteres em pe na 0,
  // cards largos de Continuar assistindo na 1, e uma CURTA na 2.
  memset(fileiras, 0, sizeof fileiras);
  nFileiras = 3;
  snprintf(fileiras[0].chave, sizeof fileiras[0].chave, "catalogo_0");
  fileiras[0].tipo = FILEIRA_NORMAL;   fileiras[0].n = 20; fileiras[0].escala = 1.0f;
  snprintf(fileiras[1].chave, sizeof fileiras[1].chave, "continue_watching");
  fileiras[1].tipo = FILEIRA_CONTINUE;  fileiras[1].n = 20; fileiras[1].escala = 1.0f;
  snprintf(fileiras[2].chave, sizeof fileiras[2].chave, "catalogo_2");
  fileiras[2].tipo = FILEIRA_NORMAL;   fileiras[2].n = 6;  fileiras[2].escala = 1.0f;
  { int cols[3] = {20, 20, 6};
    focus_iniciar(&foco, 3, cols); }
  const int rA = 0, rB = 1, rD = 2;
  // Larguras DIFERENTES: e nelas que "mesmo indice" e "mesma posicao x"
  // deixam de dar o mesmo cartaz.
  assert(fabsf(larguraFil(rA) - larguraFil(rB)) > 1.0f);
  assert(foco.nColunas[rA] == 20 && foco.nColunas[rB] == 20 && foco.nColunas[rD] == 6);

  // DOWN: cartaz ~4 da fileira A (posteres) cai na fileira B (cards largos),
  // na posicao x e NAO no inicio dela.
  memset(scrollX, 0, sizeof scrollX);
  focoHero = 0;
  foco.fileira = rA; foco.coluna = 4;
  foco.colunaLembrada[rA] = 4;
  foco.colunaLembrada[rB] = 0;   // o "inicio de fileira" do defeito
  float cx = cxDe(rA, 4);
  tecla(SDLK_DOWN);
  assert(foco.fileira == rB);
  int esperado = maisProximo(rB, cx);
  assert(esperado > 0);   // comecar da fileira era o defeito; a posicao e no meio
  assert(foco.coluna == esperado);
  // Semantico, sem depender da busca: nenhuma coluna da fileira B tem centro
  // x mais perto do que a escolhida (meia casa de tolerancia).
  assert(fabsf(cxDe(rB, foco.coluna) - cx) <= passoFil(rB) * 0.5f + 0.01f);
  assert(foco.colunaLembrada[rB] == foco.coluna);
  int c1 = foco.coluna;   // pouso sem rolagem, para comparar com o rolado

  // Fileira B ROLADA: a mesma regra com rolagens diferentes entre fileiras —
  // dois cartazes de rolagem = duas colunas de diferenca no pouso.
  memset(scrollX, 0, sizeof scrollX);
  scrollX[rB] = passoFil(rB) * 2.0f;
  foco.fileira = rA; foco.coluna = 4;
  cx = cxDe(rA, 4);
  tecla(SDLK_DOWN);
  assert(foco.fileira == rB);
  assert(foco.coluna == c1 + 2);

  // UP tem a mesma regra na volta: da fileira B larga para a A de posteres.
  memset(scrollX, 0, sizeof scrollX);
  foco.fileira = rB; foco.coluna = 6;
  foco.colunaLembrada[rB] = 6;
  cx = cxDe(rB, 6);
  tecla(SDLK_UP);
  assert(foco.fileira == rA);
  esperado = maisProximo(rA, cx);
  assert(esperado > 6);   // largura diferente: mesmo indice NAO e a regra
  assert(foco.coluna == esperado);

  // Fileira CURTA: da fileira larga col 6 para a de 6 cartazes, o pouso
  // nunca passa do fim — e continua sendo o mais proximo que EXISTE.
  memset(scrollX, 0, sizeof scrollX);
  foco.fileira = rB; foco.coluna = 6;
  cx = cxDe(rB, 6);
  tecla(SDLK_DOWN);
  assert(foco.fileira == rD);
  assert(foco.coluna == maisProximo(rD, cx));
  assert(foco.coluna == 5);   // o mais proximo alem do fim e o ULTIMO cartaz

  printf("fileira vertical: desce/sobe na mesma posicao x OK\n");
  return 0;
}
