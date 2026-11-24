// Teste integrado da Tela 1 ("Achar o que ver", ACHAR_X) e Tela 7 ("Achados")
// do Farol (PLANO.md §2.9, §3.2 telas 1 e 7, §3.3 "Nada a achar", issue #25).
//
// Modo padrao: logica completa + D-pad + ASan/UBSan sobre catalogo sintetico
// controlado e ponte de producao de src/app.c.
// Modo NV_ACHA_TELA_SHOT: captura PNG 1920x1080 da Tela 1 (4 intencoes) e
// Tela 7 (3 cartazes) num FBO GLES2/GL, validando as imagens antes de sair.
//
// Nenhum titulo real, URL real ou nome de perfil entra neste teste.

#ifndef NV_ACHA_TELA_SHOT

#define FAROL_RESOLVER_SEM_MAIN
#define FAROL_TESTE_CAT_PROPRIO

#include "farolachaui.h"
#include "farolacha.h"
#include "faroltexto.h"
#include "layout.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// Prototipos usados por farol_resolver.c antes da inclusao.
#include "catalogo.h"
#include "home.h"
const CatItem *cat_item(int i);
int cat_n(void);
unsigned cat_revisao(void);
int cacavisto_filme(const char *imdb, long tmdb, long long *quando);
static void player_descartar_retido(void);
static void detail_abrir(const HomeItem *it);
static int  detail_aberto(void);
static int  detail_indice(void);
static void detail_fechar_seco(void);

#include "farol_resolver.c"

#define CAT_CAP 64
static CatItem  A_itens[CAT_CAP];
static int      A_n;
static unsigned A_rev = 1u;
static char     A_vistos[32][32];
static int      A_vistosN;
static int      A_abriuN;
static int      A_detailAberto;
static int      A_fechouSecoN;
static int      A_abriuIdx = -1;
static char     A_abriuImdb[64];
static char     A_abriuTipo[8];
static char     A_abriuTitulo[160];
static char     A_abriuMeta[96];
static char     A_abriuArte[NV_ARTE_URL_MAX];
static char     A_alvoEsperado[64];
static void   (*A_naEntradaDetailAbrir)(const HomeItem *it);
static void   (*A_naLeituraCatItem)(int i);
static int      A_naLeituraSkip;
static int      A_abriuTrocadoN;
static int      A_detailIndiceOverride = -1;

const CatItem *cat_item(int i) {
  if (i >= 0 && i < A_n && A_naLeituraCatItem) {
    if (A_naLeituraSkip > 0) {
      A_naLeituraSkip--;
    } else {
      void (*fn)(int) = A_naLeituraCatItem;
      A_naLeituraCatItem = NULL;
      fn(i);
    }
  }
  return (i >= 0 && i < A_n) ? &A_itens[i] : NULL;
}
int cat_n(void) { return A_n; }
unsigned cat_revisao(void) { return A_rev; }

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  int i;
  (void)tmdb;
  if (quando) *quando = 0;
  if (!imdb || !imdb[0]) return 0;
  for (i = 0; i < A_vistosN; i++)
    if (!strcmp(A_vistos[i], imdb)) return 1;
  return 0;
}

static void player_descartar_retido(void) {}

static void detail_abrir(const HomeItem *it) {
  const CatItem *c;
  if (A_naEntradaDetailAbrir) {
    void (*fn)(const HomeItem *) = A_naEntradaDetailAbrir;
    A_naEntradaDetailAbrir = NULL;
    fn(it);
  }
  c = it ? cat_item(it->indice) : NULL;
  A_abriuN++;
  A_detailAberto = 1;
  A_abriuIdx = it ? it->indice : -1;
  A_abriuImdb[0] = '\0';
  A_abriuTipo[0] = '\0';
  A_abriuTitulo[0] = '\0';
  A_abriuMeta[0] = '\0';
  A_abriuArte[0] = '\0';
  if (it && it->arte)
    snprintf(A_abriuArte, sizeof A_abriuArte, "%s", it->arte);
  if (c) {
    snprintf(A_abriuImdb, sizeof A_abriuImdb, "%s", c->imdb);
    snprintf(A_abriuTipo, sizeof A_abriuTipo, "%s", c->tipo);
    snprintf(A_abriuTitulo, sizeof A_abriuTitulo, "%s", c->titulo);
    snprintf(A_abriuMeta, sizeof A_abriuMeta, "%s", c->meta);
    if (!strcmp(c->imdb, "tt9000022")) A_abriuTrocadoN++;
  }
}

static int detail_aberto(void) {
  return A_detailAberto;
}

static int detail_indice(void) {
  if (!A_detailAberto) return -1;
  return A_detailIndiceOverride >= 0 ? A_detailIndiceOverride : A_abriuIdx;
}

static void detail_fechar_seco(void) {
  A_detailAberto = 0;
  A_detailIndiceOverride = -1;
  A_fechouSecoN++;
}

static void hook_inverter_cat(const HomeItem *it) {
  CatItem tmp = A_itens[0];
  snprintf(A_alvoEsperado, sizeof A_alvoEsperado, "%s",
           (it && cat_item(it->indice)) ? cat_item(it->indice)->imdb : "");
  A_itens[0] = A_itens[1];
  A_itens[1] = tmp;
  A_rev++;
}

static void hook_inverter_cat_sem_rev(const HomeItem *it) {
  CatItem tmp = A_itens[0];
  (void)it;
  A_itens[0] = A_itens[1];
  A_itens[1] = tmp;
}

static void hook_substituir_sem_rev(const HomeItem *it) {
  (void)it;
  A_itens[0] = A_itens[1];
  A_n = 1;
}

static void hook_cat_item_inverter_sem_rev(int i) {
  CatItem tmp;
  (void)i;
  tmp = A_itens[0];
  A_itens[0] = A_itens[1];
  A_itens[1] = tmp;
}

static void hook_cat_item_substituir_sem_rev(int i) {
  (void)i;
  A_itens[0] = A_itens[1];
  A_n = 1;
}

static void hook_desviar_detail_indice(const HomeItem *it) {
  (void)it;
  A_detailIndiceOverride = 1;
}

static void hook_sumir_obra(const HomeItem *it) {
  (void)it;
  A_n = 1;
  memset(&A_itens[0], 0, sizeof A_itens[0]);
  snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9000032");
  A_itens[0].tmdb = 9032;
  snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
  snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "obra_subst");
  snprintf(A_itens[0].genero, sizeof A_itens[0].genero, "Filme · Drama");
  snprintf(A_itens[0].meta, sizeof A_itens[0].meta, "2025 · 82 min");
  A_itens[0].nota = 85;
  A_rev++;
}

static int falhas;
#define CONFERE(c) do { \
  if (!(c)) { \
    fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); \
    falhas++; \
  } \
} while (0)

static void zera_tudo(void) {
  zerar();
  memset(A_itens, 0, sizeof A_itens);
  A_n = 0;
  A_rev++;
  A_vistosN = 0;
  A_abriuN = 0;
  A_detailAberto = 0;
  A_fechouSecoN = 0;
  A_naEntradaDetailAbrir = NULL;
  A_naLeituraCatItem = NULL;
  A_naLeituraSkip = 0;
  A_abriuTrocadoN = 0;
  A_detailIndiceOverride = -1;
  A_abriuIdx = -1;
  A_abriuImdb[0] = '\0';
  A_abriuTipo[0] = '\0';
  A_abriuTitulo[0] = '\0';
  A_abriuMeta[0] = '\0';
  A_abriuArte[0] = '\0';
  A_alvoEsperado[0] = '\0';
  c_reduzidas = 1;
  resolver_teste_esquecer();
}

static int add_cat(const char *imdb, long tmdb, const char *tipo,
                   const char *titulo, const char *genero, const char *meta,
                   int nota, int progresso, long long retomadoMs) {
  int i = A_n++;
  CONFERE(i < CAT_CAP);
  memset(&A_itens[i], 0, sizeof A_itens[i]);
  snprintf(A_itens[i].imdb, sizeof A_itens[i].imdb, "%s", imdb ? imdb : "");
  A_itens[i].tmdb = tmdb;
  snprintf(A_itens[i].tipo, sizeof A_itens[i].tipo, "%s", tipo ? tipo : "movie");
  snprintf(A_itens[i].titulo, sizeof A_itens[i].titulo, "%s", titulo ? titulo : "");
  snprintf(A_itens[i].genero, sizeof A_itens[i].genero, "%s", genero ? genero : "");
  snprintf(A_itens[i].meta, sizeof A_itens[i].meta, "%s", meta ? meta : "");
  A_itens[i].nota = nota;
  A_itens[i].progresso = progresso;
  A_itens[i].retomadoMs = retomadoMs;
  A_rev++;
  return i;
}

static void marcar_visto(const char *imdb) {
  CONFERE(A_vistosN < 32);
  snprintf(A_vistos[A_vistosN++], sizeof A_vistos[0], "%s", imdb);
}

static void tecla_repeat(SDL_Keycode k, Uint8 rep) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.repeat = rep;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void tecla(SDL_Keycode k) {
  tecla_repeat(k, 0);
}

static void tecla_app_repeat(SDL_Keycode k, Uint8 rep) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.repeat = rep;
  e.key.keysym.sym = k;
  app_resolver_despachar(&e);
}

static void tecla_app(SDL_Keycode k) {
  tecla_app_repeat(k, 0);
}

static void desenhar(void) {
  c_txtN = 0;
  resolver_atualizar(0.016f, 0);
  resolver_desenhar(0);
}

static int escreveu(const char *s) {
  int i;
  for (i = 0; i < c_txtN; i++)
    if (strstr(c_txt[i], s)) return 1;
  return 0;
}

static int escreveu_qtd(const char *s) {
  int i, q = 0;
  for (i = 0; i < c_txtN; i++)
    if (strstr(c_txt[i], s)) q++;
  return q;
}

static void varredura_fase(int esperados, int faseEsperada) {
  enum { MAXF = 64, PASSOS = 1600 };
  static const SDL_Keycode DIR[4] = { SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP };
  int vis[MAXF], nvis = 0, saida[MAXF] = { 0 };
  static unsigned char aresta[MAXF][MAXF];
  int i, j, k;
  unsigned seed = 9191u;
  memset(aresta, 0, sizeof aresta);
  vis[nvis++] = resolver_foco();
  for (int p = 0; p < PASSOS; p++) {
    int antes = resolver_foco(), depois, a, b;
    seed = seed * 1103515245u + 12345u;
    tecla(DIR[(seed >> 16) & 3]);
    if (!resolver_aberto() || resolver_fase() != faseEsperada) {
      CONFERE(!"seta mudou de fase ou fechou");
      return;
    }
    depois = resolver_foco();
    CONFERE(depois >= 0);
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (b == nvis && nvis < MAXF) vis[nvis++] = depois;
    for (a = 0; a < nvis && vis[a] != antes; a++) {}
    for (b = 0; b < nvis && vis[b] != depois; b++) {}
    if (antes != depois && a < nvis && b < nvis) {
      saida[a] = 1;
      aresta[a][b] = 1;
    }
  }
  CONFERE(nvis == esperados);
  for (i = 0; i < nvis; i++) CONFERE(saida[i] || nvis == 1);
  for (k = 0; k < nvis; k++)
    for (i = 0; i < nvis; i++)
      for (j = 0; j < nvis; j++)
        if (aresta[i][k] && aresta[k][j]) aresta[i][j] = 1;
  for (i = 0; i < nvis; i++)
    for (j = 0; j < nvis; j++)
      if (i != j && !aresta[i][j]) {
        fprintf(stderr, "FALHA: fase %d: foco %d nao alcanca %d\n",
                faseEsperada, vis[i], vis[j]);
        falhas++;
      }
}

static void ir_intencao(int alvoFoco) {
  int i;
  for (i = 0; i < 6 && resolver_foco() != alvoFoco; i++) tecla(SDLK_RIGHT);
  for (i = 0; i < 4 && resolver_foco() != alvoFoco; i++) {
    if (resolver_foco() < alvoFoco) tecla(SDLK_DOWN);
    else if (resolver_foco() > alvoFoco) tecla(SDLK_UP);
  }
  CONFERE(resolver_foco() == alvoFoco);
}

static void seed_catalogo_completo(void) {
  // 3 curtos nao vistos de Suspense (<= 100 min)
  add_cat("tt1000001", 101, "movie",  "obra_01", "Filme · Suspense", "2024 · 85 min", 84, 0, 0);
  add_cat("tt1000002", 102, "movie",  "obra_02", "Filme · Suspense", "2025 · 92 min", 88, 0, 0);
  add_cat("tt1000003", 103, "movie",  "obra_03", "Filme · Suspense", "2023 · 1h38",   80, 0, 0);
  // 2 itens longos nao vistos (para FA_NAO_VI / FA_SURPRESA)
  add_cat("tt1000004", 104, "movie",  "obra_04", "Filme · Drama",    "2022 · 134 min", 91, 0, 0);
  add_cat("tt1000005", 105, "series", "obra_05", "Programa de TV · Drama", "2024 · 50 min", 86, 0, 0);
  // 2 itens em andamento (para FA_RETOMAR)
  add_cat("tt1000006", 106, "series", "obra_06", "Programa de TV · Crime", "2024 · 48 min", 82, 45, 1700000002000LL);
  add_cat("tt1000007", 107, "movie",  "obra_07", "Filme · Aventura", "2021 · 120 min", 79, 30, 1700000001000LL);
}

int main(void) {
  int idx, i;

  // 1. D-pad completo na Tela 1 (sem banner e com banner), sem "Em breve", sem beco.
  zera_tudo();
  seed_catalogo_completo();
  resolver_abrir();
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  desenhar();
  CONFERE(escreveu(FT_ACHAR_O_QUE_VER));
  CONFERE(escreveu(FT_INTENCAO_CURTO));
  CONFERE(escreveu(FT_INTENCAO_NAO_VI));
  CONFERE(escreveu(FT_INTENCAO_RETOMAR));
  CONFERE(escreveu(FT_INTENCAO_SURPRESA));
  CONFERE(!escreveu("Em breve"));
  varredura_fase(13, RESOLVER_FASE_INICIO);

  // Move esquerda->direita e direita->esquerda explicitamente entre sintomas e intencoes.
  resolver_abrir();
  tecla(SDLK_RIGHT);
  tecla(SDLK_RIGHT);
  CONFERE(resolver_foco() >= RESOLVER_B_ACHAR_CURTO &&
          resolver_foco() <= RESOLVER_B_ACHAR_SURPRESA);
  tecla(SDLK_LEFT);
  CONFERE(resolver_foco() >= 0 && resolver_foco() < RESOLVER_B_BANNER);

  // Com banner recente: 14 focos alcancaveis na Tela 1.
  falhar_fonte("Falha recente", "", FC_NENHUMA, 0, "http");
  resolver_abrir();
  CONFERE(resolver_foco() == RESOLVER_B_BANNER);
  varredura_fase(14, RESOLVER_FASE_INICIO);

  // 2. OK em cada uma das 4 intencoes + Voltar preservando foco na intencao escolhida.
  {
    static const int INT_ID[4] = {
      RESOLVER_B_ACHAR_CURTO, RESOLVER_B_ACHAR_NAO_VI,
      RESOLVER_B_ACHAR_RETOMAR, RESOLVER_B_ACHAR_SURPRESA
    };
    static const FarolAchaTipo INT_T[4] = {
      FA_CURTO, FA_NAO_VI, FA_RETOMAR, FA_SURPRESA
    };
    for (i = 0; i < 4; i++) {
      zera_tudo();
      seed_catalogo_completo();
      resolver_abrir();
      ir_intencao(INT_ID[i]);
      tecla(SDLK_RETURN);
      CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
      CONFERE(farolachaui_tipo() == INT_T[i]);
      CONFERE(farolachaui_n() > 0);
      CONFERE(resolver_foco() == RESOLVER_B_ACHAR_CURTO); // poster 0 em foco
      desenhar();
      CONFERE(escreveu(farolacha_rotulo(INT_T[i])));
      CONFERE(!escreveu("4K"));
      tecla(SDLK_ESCAPE);
      CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
      CONFERE(resolver_foco() == INT_ID[i]); // foco preservado na intencao
    }
  }

  // 3. Navegacao <= / => / Voltar para casos de 3, 2 e 1 resultados, e botao Voltar.
  // Caso 3 resultados (FA_CURTO): 3 cartazes + Voltar = 4 focos.
  zera_tudo();
  seed_catalogo_completo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 3);
  CONFERE(farolachaui_foco() == 0);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 1);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 2);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 2); // trava no ultimo
  tecla(SDLK_LEFT);  CONFERE(farolachaui_foco() == 1);
  tecla(SDLK_LEFT);  CONFERE(farolachaui_foco() == 0);
  varredura_fase(4, RESOLVER_FASE_ACHAR);
  // Desce para Voltar e confirma com OK.
  tecla(SDLK_DOWN);
  CONFERE(farolachaui_foco() == -1 && resolver_foco() == RESOLVER_B_ACHAR_VOLTAR);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && resolver_foco() == RESOLVER_B_ACHAR_CURTO);

  // Caso 2 resultados (FA_RETOMAR): 2 cartazes + Voltar = 3 focos.
  ir_intencao(RESOLVER_B_ACHAR_RETOMAR);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 2);
  CONFERE(farolachaui_foco() == 0);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 1);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 1);
  varredura_fase(3, RESOLVER_FASE_ACHAR);
  tecla(SDLK_ESCAPE);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && resolver_foco() == RESOLVER_B_ACHAR_RETOMAR);

  // Caso 1 resultado: 1 cartaz + Voltar = 2 focos.
  zera_tudo();
  add_cat("tt2000001", 201, "series", "obra_unica", "Programa de TV · Drama", "2024 · 42 min", 85, 25, 1700000005000LL);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_RETOMAR);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  tecla(SDLK_RIGHT); CONFERE(farolachaui_foco() == 0);
  tecla(SDLK_LEFT);  CONFERE(farolachaui_foco() == 0);
  varredura_fase(2, RESOLVER_FASE_ACHAR);

  // 4. OK num cartaz copia identidade completa e despacha via app.c antes de fechar o Farol.
  zera_tudo();
  seed_catalogo_completo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_RETOMAR);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 2);
  tecla(SDLK_RIGHT); // foca segundo item (obra_07, tt1000007, movie)
  tecla(SDLK_RETURN);
  // P1-2: Farol so fecha depois que app.c confirmar o despacho do detalhe.
  CONFERE(resolver_aberto());
  {
    FarolAchaPedido ped;
    unsigned revRes = 0;
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    CONFERE(!strcmp(ped.imdb, "tt1000007"));
    CONFERE(!strcmp(ped.tipo, "movie"));
    CONFERE(!strcmp(ped.titulo, "obra_07"));
    idx = resolver_resolver_pedido(&ped, &revRes);
    CONFERE(idx >= 0 && idx < cat_n() && revRes == A_rev);
    CONFERE(!strcmp(cat_item(idx)->imdb, "tt1000007"));
    CONFERE(!strcmp(cat_item(idx)->tipo, "movie"));
    CONFERE(resolver_pediu_abrir(&ped) == 0); // consome UMA vez
    resolver_fechar();
    CONFERE(!resolver_aberto());
  }

  // Mesmo fluxo passando pelo bloco real de src/app.c (app_resolver_despachar).
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_NAO_VI);
  tecla(SDLK_RETURN);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1);
  CONFERE(A_abriuN == 1);
  CONFERE(A_abriuIdx >= 0 && A_abriuIdx < cat_n());
  CONFERE(!strcmp(cat_item(A_abriuIdx)->imdb, A_abriuImdb));
  CONFERE(!strcmp(cat_item(A_abriuIdx)->tipo, A_abriuTipo));

  // 5. Caso vazio ("Nada a achar"): sem travamento nem beco; fallback de FA_CURTO.
  zera_tudo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 0);
  CONFERE(farolachaui_foco() == -1 && resolver_foco() == RESOLVER_B_ACHAR_VOLTAR);
  desenhar();
  CONFERE(escreveu("Nada com menos de 100 min"));
  CONFERE(!escreveu(FT_ACHA_L2_NAO_VISTO));
  tecla(SDLK_LEFT); tecla(SDLK_RIGHT); tecla(SDLK_UP); tecla(SDLK_DOWN);
  CONFERE(farolachaui_foco() == -1 && resolver_fase() == RESOLVER_FASE_ACHAR);
  tecla(SDLK_RETURN); // OK no Voltar volta ao Inicio com foco em FA_CURTO
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO && resolver_foco() == RESOLVER_B_ACHAR_CURTO);

  // Fallback honesto em FA_CURTO: nada < 100 min, mas 3 curtos parecidos <= 130 min.
  zera_tudo();
  add_cat("tt3000001", 301, "movie", "obra_301", "Filme · Drama", "2024 · 110 min", 82, 0, 0);
  add_cat("tt3000002", 302, "movie", "obra_302", "Filme · Drama", "2024 · 118 min", 84, 0, 0);
  add_cat("tt3000003", 303, "movie", "obra_303", "Filme · Drama", "2024 · 124 min", 80, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 3 && farolachaui_em_fallback() == 1);
  desenhar();
  CONFERE(escreveu("Nada com menos de 100 min"));
  CONFERE(escreveu(FT_NADA_ACHAR_L2));

  // 6. Mudanca de cat_revisao() com a Tela 7 aberta: sem UAF e recarga limpa com mesma semente.
  zera_tudo();
  seed_catalogo_completo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  {
    unsigned revAntes = farolachaui_revisao();
    unsigned semAntes = farolachaui_semente();
    CONFERE(farolachaui_n() == 3);
    // Simula deslocamento completo do catalogo (Continuar inserido no indice 0) e remocao de um curto.
    memmove(&A_itens[1], &A_itens[0], sizeof(CatItem) * (size_t)A_n);
    A_n++;
    memset(&A_itens[0], 0, sizeof A_itens[0]);
    snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9999999");
    snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
    snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "obra_desloca");
    snprintf(A_itens[0].meta, sizeof A_itens[0].meta, "2026 · 150 min");
    A_itens[0].progresso = 50;
    A_rev++;
    desenhar();
    CONFERE(farolachaui_revisao() == A_rev && farolachaui_revisao() != revAntes);
    CONFERE(farolachaui_semente() == semAntes);
    CONFERE(farolachaui_n() == 3);
    // Agora altera cat_revisao() imediatamente antes do OK num cartaz (troca indices de novo).
    memmove(&A_itens[1], &A_itens[0], sizeof(CatItem) * (size_t)A_n);
    A_n++;
    A_rev++;
    tecla(SDLK_RETURN);
    CONFERE(resolver_aberto());
    {
      FarolAchaPedido ped;
      CONFERE(resolver_pediu_abrir(&ped) == 1);
      idx = resolver_resolver_pedido(&ped, NULL);
      CONFERE(idx >= 0 && idx < cat_n());
      CONFERE(farolacha_minutos_meta(cat_item(idx)->meta) <= 100);
      resolver_fechar();
    }
  }

  // 7. Regra de honestidade:
  //    a) Todos nao vistos + genero comum + todos curtos => FT_ACHA_L1 + FT_ACHA_L2_NAO_VISTO.
  zera_tudo();
  add_cat("tt4000001", 401, "movie", "obra_401", "Filme · Suspense", "2024 · 88 min", 85, 0, 0);
  add_cat("tt4000002", 402, "movie", "obra_402", "Filme · Suspense", "2024 · 91 min", 87, 0, 0);
  add_cat("tt4000003", 403, "movie", "obra_403", "Filme · Suspense", "2024 · 96 min", 89, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(strstr(farolachaui_l1(), "Suspense") != NULL);
  CONFERE(strstr(farolachaui_l1(), "1h40") != NULL);
  CONFERE(!strcmp(farolachaui_l2(), FT_ACHA_L2_NAO_VISTO));
  CONFERE(escreveu(FT_ACHA_L2_NAO_VISTO));

  //    b) Um item ja visto no meio dos 3 curtos => AUSENCIA de FT_ACHA_L2_NAO_VISTO.
  marcar_visto("tt4000001");
  A_rev++;
  zera_tudo();
  add_cat("tt4000001", 401, "movie", "obra_401", "Filme · Suspense", "2024 · 88 min", 85, 0, 0);
  add_cat("tt4000002", 402, "movie", "obra_402", "Filme · Suspense", "2024 · 91 min", 87, 0, 0);
  add_cat("tt4000003", 403, "movie", "obra_403", "Filme · Comédia",  "2024 · 96 min", 89, 0, 0);
  marcar_visto("tt4000001");
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 3);
  CONFERE(farolachaui_l2()[0] == '\0');
  CONFERE(!escreveu(FT_ACHA_L2_NAO_VISTO));
  // Sem genero comum => usa FT_ACHA_CURTOS_L1 (nao menciona "Suspense" nem "Comédia").
  CONFERE(strstr(farolachaui_l1(), "Suspense") == NULL);
  CONFERE(strstr(farolachaui_l1(), "100 min") != NULL);

  // 8. Gancho para #31 e #50 (farolachaui_abrir_op com genero e maxMin).
  {
    FarolAchaAbrir op;
    memset(&op, 0, sizeof op);
    op.tipo = FA_CURTO;
    op.maxMin = 92;
    op.genero = "Suspense";
    op.semente = 77u;
    farolachaui_abrir_op(&op);
    CONFERE(farolachaui_n() == 2);
    CONFERE(!strcmp(farolachaui_l2(), "")); // tt4000001 foi visto
  }

  // 9. P1-1 Causa A: troca de catalogo antes de OK sem redesenho NAO abre outra obra silenciosamente.
  zera_tudo();
  add_cat("tt5000001", 501, "movie", "obra_A", "Filme · Drama", "2024 · 80 min", 85, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1 && farolachaui_foco() == 0);
  desenhar();
  // Sem redesenhar, substitui obra_A por obra_X na posicao 0.
  memset(&A_itens[0], 0, sizeof A_itens[0]);
  snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt5000099");
  A_itens[0].tmdb = 599;
  snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
  snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "obra_X");
  snprintf(A_itens[0].genero, sizeof A_itens[0].genero, "Filme · Drama");
  snprintf(A_itens[0].meta, sizeof A_itens[0].meta, "2024 · 82 min");
  A_rev++;
  tecla_app(SDLK_RETURN);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_revisao() == A_rev);
  // Nova confirmacao do usuario na tela recarregada abre obra_X.
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_abriuN == 1 && A_detailAberto == 1);
  CONFERE(!strcmp(A_abriuImdb, "tt5000099"));

  // Tambem quando a lista vira vazia antes de OK sem redesenho.
  zera_tudo();
  add_cat("tt5000002", 502, "movie", "obra_B", "Filme · Drama", "2024 · 80 min", 85, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  A_n = 0;
  A_rev++;
  tecla_app(SDLK_RETURN);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 0 && farolachaui_foco() == -1);

  // 10. P1-1 Causa B: inversao do catalogo apos resolver_pediu_abrir() (antes de abrirFarolPedido).
  zera_tudo();
  add_cat("tt6000001", 601, "movie", "obra_inv_1", "Filme · Drama", "2024 · 85 min", 90, 0, 0);
  add_cat("tt6000002", 602, "movie", "obra_inv_2", "Filme · Drama", "2024 · 86 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 2);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido ped;
    CatItem tmp;
    char imdbEsperado[64];
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    snprintf(imdbEsperado, sizeof imdbEsperado, "%s", ped.imdb);
    // Inverte o catalogo inteiro entre o getter e abrirFarolPedido().
    tmp = A_itens[0];
    A_itens[0] = A_itens[1];
    A_itens[1] = tmp;
    A_rev++;
    CONFERE(abrirFarolPedido(&ped) == 1);
    CONFERE(A_detailAberto == 1 && !strcmp(A_abriuImdb, imdbEsperado));
    CONFERE(!strcmp(cat_item(A_abriuIdx)->imdb, imdbEsperado));
    // Se a obra sumir completamente entre o getter e abrirFarolPedido(), NAO abre outra nem fecha Farol.
    A_abriuN = 0; A_detailAberto = 0;
    A_n = 1;
    snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt6999999");
    A_itens[0].tmdb = 6999;
    A_rev++;
    CONFERE(abrirFarolPedido(&ped) == 0);
    CONFERE(A_abriuN == 0 && A_detailAberto == 0);
    CONFERE(resolver_aberto());
    resolver_fechar();
  }

  // 11. P1-1 Causa C: dois homonimos sem IMDb/TMDB (150 min x 84 min).
  zera_tudo();
  add_cat("", 0, "movie", "Obra Homonima", "Filme · Drama", "2024 · 150 min", 95, 0, 0);
  add_cat("", 0, "movie", "Obra Homonima", "Filme · Drama", "2024 · 84 min",  75, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuIdx == 1);
  CONFERE(!strcmp(A_abriuMeta, "2024 · 84 min"));

  // Homonimo sem ID apos deslocamento do catalogo: resolve unico por titulo+meta+tipo.
  zera_tudo();
  add_cat("", 0, "movie", "Obra Homonima", "Filme · Drama", "2024 · 150 min", 95, 0, 0);
  add_cat("", 0, "movie", "Obra Homonima", "Filme · Drama", "2024 · 84 min",  75, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido ped;
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    memmove(&A_itens[1], &A_itens[0], sizeof(CatItem) * (size_t)A_n);
    A_n++;
    memset(&A_itens[0], 0, sizeof A_itens[0]);
    snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9990001");
    snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
    snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "Desloca");
    snprintf(A_itens[0].meta, sizeof A_itens[0].meta, "2025 · 140 min");
    A_rev++;
    CONFERE(abrirFarolPedido(&ped) == 1);
    CONFERE(A_abriuIdx == 2 && !strcmp(A_abriuMeta, "2024 · 84 min"));
    resolver_fechar();
  }

  // Dois homonimos identicos sem ID (mesmo titulo+meta+tipo): aceita na mesma revisao, recusa se ambigua apos mudanca.
  zera_tudo();
  add_cat("", 0, "movie", "Obra Igual", "Filme · Drama", "2024 · 84 min", 80, 0, 0);
  add_cat("", 0, "movie", "Obra Igual", "Filme · Drama", "2024 · 84 min", 80, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido ped;
    int idxFocado;
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    idxFocado = ped.indice;
    CONFERE(resolver_resolver_pedido(&ped, NULL) == idxFocado);
    A_rev++;
    CONFERE(resolver_resolver_pedido(&ped, NULL) == -1);
    resolver_fechar();
  }

  // 12. P1-2: obra valida sem poster e sem backdrop abre o detalhe de verdade e fecha o Farol.
  zera_tudo();
  add_cat("tt7000001", 701, "movie", "Local Sem Arte", "Filme · Drama", "2024 · 95 min", 80, 42, 1700000009000LL);
  CONFERE(A_itens[0].poster[0] == '\0' && A_itens[0].backdrop[0] == '\0');
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_RETOMAR);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuN == 1);
  CONFERE(!strcmp(A_abriuImdb, "tt7000001"));
  CONFERE(A_abriuArte[0] == '\0');

  // 13. P2-1: filme de exatos 100 min NAO sai sob "com menos de 100 min" / "1h40".
  zera_tudo();
  add_cat("tt8000001", 801, "movie", "Exatos Cem", "Filme · Suspense", "2025 · 100 min", 85, 0, 0);
  add_cat("tt8000002", 802, "movie", "Exatos 1h40", "Filme · Suspense", "2025 · 1h40", 85, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 2 && farolachaui_em_fallback() == 1);
  CONFERE(strstr(farolachaui_l1(), "Nada com menos de 100 min") != NULL);
  CONFERE(farolachaui_l2()[0] == '\0'); // P2-2: fallback parcial (2 itens) sem L2 enganosa

  // Com 99 min + 100 min: so o de 99 min entra na busca < 100 min.
  zera_tudo();
  add_cat("tt8000003", 803, "movie", "Noventa e Nove", "Filme · Suspense", "2025 · 99 min", 85, 0, 0);
  add_cat("tt8000004", 804, "movie", "Cem Min",        "Filme · Suspense", "2025 · 100 min", 90, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 1 && farolachaui_em_fallback() == 0);
  CONFERE(strstr(farolachaui_l1(), "1h40") != NULL);
  tecla_app(SDLK_RETURN);
  CONFERE(!strcmp(A_abriuImdb, "tt8000003"));

  // 14. P2-2: estado vazio sem duplicar texto no cabecalho e no painel; fallback parcial 1 item sem L2.
  zera_tudo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 0);
  CONFERE(escreveu_qtd("Nada com menos de 100 min") == 1);
  tecla(SDLK_ESCAPE);
  ir_intencao(RESOLVER_B_ACHAR_NAO_VI);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 0);
  CONFERE(escreveu_qtd("Nada encontrado") == 1);

  zera_tudo();
  add_cat("tt8000010", 810, "movie", "Parcial Um", "Filme · Drama", "2025 · 115 min", 84, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  CONFERE(farolachaui_n() == 1 && farolachaui_em_fallback() == 1);
  CONFERE(farolachaui_l2()[0] == '\0');
  CONFERE(!escreveu(FT_ACHA_L2_NAO_VISTO));

  // 15. FAROL25-r2 P1-1: inversao do catalogo na entrada de detail_abrir()
  //     re-resolve a obra certa sem fechar o Farol no item errado.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min", 92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 86 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 2 && farolachaui_foco() == 0);
  A_naEntradaDetailAbrir = hook_inverter_cat;
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1);
  CONFERE(A_fechouSecoN == 1);
  CONFERE(A_alvoEsperado[0] != '\0');
  CONFERE(!strcmp(A_abriuImdb, A_alvoEsperado));
  CONFERE(!strcmp(cat_item(A_abriuIdx)->imdb, A_alvoEsperado));

  // Se a obra escolhida sumir na entrada de detail_abrir(), desfaz a abertura errada,
  // mantem o Farol aberto na Tela 7 e recarrega os Achados.
  zera_tudo();
  add_cat("tt9000031", 9031, "movie", "obra_vai_sumir", "Filme · Drama", "2024 · 84 min", 90, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1);
  tela = TELA_AJUSTES;
  A_naEntradaDetailAbrir = hook_sumir_obra;
  tecla_app(SDLK_RETURN);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(A_detailAberto == 0);
  CONFERE(A_fechouSecoN == 1);
  CONFERE(tela == TELA_AJUSTES);
  CONFERE(farolachaui_revisao() == A_rev);
  CONFERE(farolachaui_n() == 1);

  // 16. FAROL25-r2 P1-2: SDL_KEYDOWN de OK com key.repeat=1 e ignorado ao entrar
  //     na Tela 7 e apos troca de cartao, enquanto setas com repeat=1 continuam movendo foco.
  zera_tudo();
  seed_catalogo_completo();
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  // Setas com repeat=1 na Tela 1 continuam movendo foco.
  tecla_app_repeat(SDLK_DOWN, 1);
  CONFERE(resolver_foco() == RESOLVER_B_ACHAR_NAO_VI);
  tecla_app_repeat(SDLK_UP, 1);
  CONFERE(resolver_foco() == RESOLVER_B_ACHAR_CURTO);
  // Repeat em OK na Tela 1 nao abre Tela 7.
  tecla_app_repeat(SDLK_RETURN, 1);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO);
  // Toque novo abre Tela 7, e repeat imediato de OK (segurado) NAO abre o 1o resultado.
  tecla_app_repeat(SDLK_RETURN, 0);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  tecla_app_repeat(SDLK_RETURN, 1);
  tecla_app_repeat(SDLK_KP_ENTER, 1);
  tecla_app_repeat(SDLK_SPACE, 1);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  // Setas com repeat=1 continuam movendo foco normalmente.
  CONFERE(farolachaui_foco() == 0);
  tecla_app_repeat(SDLK_RIGHT, 1); CONFERE(farolachaui_foco() == 1);
  tecla_app_repeat(SDLK_RIGHT, 1); CONFERE(farolachaui_foco() == 2);
  tecla_app_repeat(SDLK_LEFT, 1);  CONFERE(farolachaui_foco() == 1);
  tecla_app_repeat(SDLK_DOWN, 1);  CONFERE(farolachaui_foco() == -1);
  // OK com repeat=1 em Voltar tambem e ignorado.
  tecla_app_repeat(SDLK_RETURN, 1);
  CONFERE(resolver_fase() == RESOLVER_FASE_ACHAR && farolachaui_foco() == -1);
  tecla_app_repeat(SDLK_UP, 1);    CONFERE(farolachaui_foco() == 1);

  // Repeat em OK apos troca de cartao NAO confirma o cartao substituto.
  zera_tudo();
  add_cat("tt9000041", 9041, "movie", "obra_original", "Filme · Drama", "2024 · 80 min", 85, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  desenhar();
  memset(&A_itens[0], 0, sizeof A_itens[0]);
  snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9000042");
  A_itens[0].tmdb = 9042;
  snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
  snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "obra_substituta");
  snprintf(A_itens[0].genero, sizeof A_itens[0].genero, "Filme · Drama");
  snprintf(A_itens[0].meta, sizeof A_itens[0].meta, "2024 · 82 min");
  A_rev++;
  tecla_app_repeat(SDLK_RETURN, 0);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  CONFERE(farolachaui_revisao() == A_rev);
  tecla_app_repeat(SDLK_RETURN, 1);
  CONFERE(A_abriuN == 0 && A_detailAberto == 0 && resolver_aberto());
  tecla_app_repeat(SDLK_RETURN, 0);
  CONFERE(!resolver_aberto() && A_detailAberto == 1 && !strcmp(A_abriuImdb, "tt9000042"));

  // 17. FAROL25-r3 P1: troca do item em cat_item(idx) SEM incrementar cat_revisao()
  //     (janela de cat_definir_tudo() antes de revisaoSobe()), tanto antes quanto durante abrirTitulo.
  //     (tt9000022 tem 120 min para que FA_CURTO escolha deterministicamente tt9000021 no foco 0.)
  //     a) Troca (inversao) antes de abrirTitulo: nunca chama detail_abrir para tt9000022 e re-resolve tt9000021.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min",  92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 120 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(farolachaui_n() == 1 && farolachaui_foco() == 0);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido ped;
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    CONFERE(!strcmp(ped.imdb, "tt9000021"));
    A_naLeituraSkip = 1;
    A_naLeituraCatItem = hook_cat_item_inverter_sem_rev;
    CONFERE(abrirFarolPedido(&ped) == 1);
    CONFERE(A_abriuTrocadoN == 0);
    CONFERE(A_abriuN == 1 && A_fechouSecoN == 0);
    CONFERE(A_detailAberto == 1 && !strcmp(A_abriuImdb, "tt9000021"));
    CONFERE(!strcmp(cat_item(A_abriuIdx)->imdb, "tt9000021"));
    resolver_fechar();
  }

  //     b) Substituicao total antes de abrirTitulo sem incrementar cat_revisao(): nunca abre tt9000022.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min",  92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 120 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);
  {
    FarolAchaPedido ped;
    CONFERE(resolver_pediu_abrir(&ped) == 1);
    CONFERE(!strcmp(ped.imdb, "tt9000021"));
    A_naLeituraSkip = 1;
    A_naLeituraCatItem = hook_cat_item_substituir_sem_rev;
    CONFERE(abrirFarolPedido(&ped) == 0);
    CONFERE(A_abriuTrocadoN == 0 && A_abriuN == 0 && A_detailAberto == 0);
    CONFERE(resolver_aberto());
    resolver_fechar();
  }

  //     c) Troca (inversao) durante abrirTitulo sem incrementar cat_revisao(): fecha seco e re-resolve tt9000021.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min",  92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 120 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  A_naEntradaDetailAbrir = hook_inverter_cat_sem_rev;
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1);
  CONFERE(A_fechouSecoN == 1);
  CONFERE(!strcmp(A_abriuImdb, "tt9000021"));
  CONFERE(!strcmp(cat_item(A_abriuIdx)->imdb, "tt9000021"));

  //     d) Substituicao total durante abrirTitulo sem incrementar cat_revisao(): fecha seco, restaura tela e mantem Farol.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min",  92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 140 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  tela = TELA_AJUSTES;
  A_naEntradaDetailAbrir = hook_substituir_sem_rev;
  tecla_app(SDLK_RETURN);
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(A_detailAberto == 0 && A_fechouSecoN == 1);
  CONFERE(tela == TELA_AJUSTES);
  CONFERE(farolachaui_n() == 0);

  //     e) Divergencia apenas em detail_indice() apos abrirTitulo(): fecha seco e reabre certo.
  zera_tudo();
  add_cat("tt9000021", 9021, "movie", "obra_escolhida", "Filme · Drama", "2024 · 85 min",  92, 0, 0);
  add_cat("tt9000022", 9022, "movie", "obra_trocada",   "Filme · Drama", "2024 · 120 min", 70, 0, 0);
  resolver_abrir();
  ir_intencao(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  A_naEntradaDetailAbrir = hook_desviar_detail_indice;
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_fechouSecoN == 1);
  CONFERE(!strcmp(cat_item(detail_indice())->imdb, "tt9000021"));

  //     f) Bonus P2-2 r3: repeat em OK fora de RESOLVER_B_ACHAR_* (ex.: RESOLVER_FASE_ENVIO) preservado.
  zera_tudo();
  resolver_abrir();
  for (i = 0; i < 6 && resolver_foco() != RESOLVER_B_LISTA; i++) tecla(SDLK_DOWN);
  CONFERE(resolver_foco() == RESOLVER_B_LISTA);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_ENVIO && resolver_foco() == RESOLVER_B_PRONTO);
  tecla_repeat(SDLK_RETURN, 1);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO);

  // Modo vidro e animacoes ligadas (exercicio sem crashes).
  c_reduzidas = 0;
  c_vidro = 1;
  desenhar();
  farolachaui_atualizar(0.016f, 16);
  c_vidro = 0;

  if (falhas) {
    fprintf(stderr, "farol_acha_tela: %d falha(s)\n", falhas);
    return 1;
  }
  puts("farol_acha_tela: regras, D-pad, honestidade e revisao ok");
  return 0;
}

#else // NV_ACHA_TELA_SHOT

#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif
#include "resolver.h"
#include "resolverregra.h"
#include "farol.h"
#include "farolachaui.h"
#include "faroltexto.h"
#include "ajustes.h"
#include "anim.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "gl_compat.h"
#include "layout.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GLuint fbo, fboTex;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void quadro(float dt) {
  SDL_PumpEvents();
  resolver_atualizar(dt, SDL_GetTicks());
  txt_novo_quadro();
  tex_novo_quadro();
  tex_bombear(8);
  gfx_novo_quadro();
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glViewport(0, 0, 1920, 1080);
  glClearColor(0.06f, 0.08f, 0.10f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  resolver_desenhar(SDL_GetTicks());
}

static void adiantar(int n) {
  int i;
  for (i = 0; i < n; i++) {
    quadro(1.0f / 60.0f);
    SDL_Delay(2);
  }
}

static int captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int y, variou = 0;
  assert(pix);
  adiantar(quadros);
  glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
  for (y = 1; y < 1920 * 1080 && !variou; y += 997)
    if (memcmp(pix + (size_t)y * 4, pix, 3)) variou = 1;
  s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
  assert(s);
  for (y = 0; y < 1080; y++)
    memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
  assert(IMG_SavePNG(s, nome) == 0);
  SDL_FreeSurface(s);
  free(pix);
  return variou;
}

static void criar_poster_local(const char *path, Uint8 r0, Uint8 g0, Uint8 b0) {
  SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, 200, 300, 32, SDL_PIXELFORMAT_RGBA32);
  int y, x;
  assert(s);
  for (y = 0; y < 300; y++) {
    Uint32 *row = (Uint32 *)((Uint8 *)s->pixels + y * s->pitch);
    Uint8 r = (Uint8)(r0 + (y * 40) / 300);
    Uint8 g = (Uint8)(g0 + (y * 30) / 300);
    Uint8 b = (Uint8)(b0 + (y * 50) / 300);
    Uint32 c = SDL_MapRGBA(s->format, r, g, b, 255);
    for (x = 0; x < 200; x++) row[x] = c;
  }
  assert(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/farol-acha-shot";
  const char *dir = getenv("NUVIO_DADOS");
  char p1[600], p2[600], p3[600], nome1[700], nome7[700];
  CatItem itens[3];
  SDL_Window *w;
  SDL_GLContext gl;
  ResolverFatos rf;
  FarolFatos ff;
  int i, lisas = 0;

  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);

  SDL_SetHint("SDL_MAC_BACKGROUND_APP", "1");
  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
#if defined(__APPLE__)
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
  w = SDL_CreateWindow("farol-acha-shot", 0, 0, 64, 64,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  glGenTextures(1, &fboTex);
  glBindTexture(GL_TEXTURE_2D, fboTex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1920, 1080, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glGenFramebuffers(1, &fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex, 0);
  assert(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_tex_esquecer(0);
  {
    char ic[4096];
    if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic);
  }

  snprintf(p1, sizeof p1, "%s/p1.png", dir);
  snprintf(p2, sizeof p2, "%s/p2.png", dir);
  snprintf(p3, sizeof p3, "%s/p3.png", dir);
  criar_poster_local(p1, 40, 80, 140);
  criar_poster_local(p2, 130, 55, 70);
  criar_poster_local(p3, 45, 115, 95);

  memset(itens, 0, sizeof itens);
  snprintf(itens[0].imdb, sizeof itens[0].imdb, "tt9000001");
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "movie");
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Amostra Um");
  snprintf(itens[0].genero, sizeof itens[0].genero, "Filme · Suspense");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2024 · 84 min");
  snprintf(itens[0].poster, sizeof itens[0].poster, "%s", p1);
  itens[0].nota = 86;

  snprintf(itens[1].imdb, sizeof itens[1].imdb, "tt9000002");
  snprintf(itens[1].tipo, sizeof itens[1].tipo, "movie");
  snprintf(itens[1].titulo, sizeof itens[1].titulo, "Amostra Dois");
  snprintf(itens[1].genero, sizeof itens[1].genero, "Filme · Suspense");
  snprintf(itens[1].meta, sizeof itens[1].meta, "2025 · 91 min");
  snprintf(itens[1].poster, sizeof itens[1].poster, "%s", p2);
  itens[1].nota = 88;

  snprintf(itens[2].imdb, sizeof itens[2].imdb, "tt9000003");
  snprintf(itens[2].tipo, sizeof itens[2].tipo, "movie");
  snprintf(itens[2].titulo, sizeof itens[2].titulo, "Amostra Tres");
  snprintf(itens[2].genero, sizeof itens[2].genero, "Filme · Suspense");
  snprintf(itens[2].meta, sizeof itens[2].meta, "2023 · 96 min");
  snprintf(itens[2].poster, sizeof itens[2].poster, "%s", p3);
  itens[2].nota = 82;
  cat_definir(itens, 3);

  memset(&rf, 0, sizeof rf);
  rf.addonsQtd = 3;
  resolver_teste_fatos(&rf);
  farolregra_fatos_iniciar(&ff);
  ff.plat = FP_WEBOS;
  ff.addonsQtd = 3;
  resolver_teste_fatos_farol(&ff);

  resolver_abrir();
  for (i = 0; i < 6 && resolver_foco() != RESOLVER_B_ACHAR_CURTO; i++)
    tecla(SDLK_RIGHT);
  assert(resolver_foco() == RESOLVER_B_ACHAR_CURTO);

  snprintf(nome1, sizeof nome1, "%s-tela1.png", saida);
  lisas += !captura(nome1, 35);

  tecla(SDLK_RETURN);
  assert(resolver_fase() == RESOLVER_FASE_ACHAR);
  assert(farolachaui_n() == 3);

  snprintf(nome7, sizeof nome7, "%s-tela7.png", saida);
  lisas += !captura(nome7, 35);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  if (lisas) {
    fprintf(stderr, "FALHA: farol_acha_tela shot gerou captura lisa\n");
    return 1;
  }
  puts("farol_acha_tela: capturas Tela 1 e Tela 7 ok");
  return 0;
}

#endif
