// Teste integrado da issue #31 (ditado em "Achar o que ver" + FAROL30-r1 R1/R2/R3/R4/R8).
//
// Modo padrao: logica completa + HTTP local + R1/R2/R3/R4/R8 + D-pad sem beco +
// ponte real de src/app.c (FarolAchaPedido / abrirFarolPedido).
// Modo NV_VOZ_TELA_SHOT: captura PNG 1920x1080 da Tela 1 (com microfone),
// Ouvindo, Revisao (texto antes de enviar) e Tela 7 (resultados da busca).
//
// Nenhum titulo real, URL real ou nome de perfil entra neste teste.

#ifndef NV_VOZ_TELA_SHOT

#define FAROL_RESOLVER_SEM_MAIN
#define FAROL_TESTE_CAT_PROPRIO

#include "farolvozui.h"
#include "farolachaui.h"
#include "farolbusca.h"
#include "farolbuscaparse.h"
#include "farolacha.h"
#include "faroltexto.h"
#include "sistexto.h"
#include "rede.h"
#include "layout.h"
#include "catalogo.h"
#include "home.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

const CatItem *cat_item(int i);
int cat_n(void);
unsigned cat_revisao(void);
int cat_acrescentar(const CatItem *item);
int cacavisto_filme(const char *imdb, long tmdb, long long *quando);
static void player_descartar_retido(void);
static void detail_abrir(const HomeItem *it);
static int  detail_aberto(void);
static int  detail_indice(void);
static void detail_fechar_seco(void);

#define perfil_infantil_ativo farol_resolver_stub_infantil
#define seguro_perfil_ativo   farol_resolver_stub_seguro
#define farolsinal_recomendar farol_resolver_stub_recomendar
#include "farol_resolver.c"
#undef perfil_infantil_ativo
#undef seguro_perfil_ativo
#undef farolsinal_recomendar

// Overrides fortes das preferencias e da autenticacao para testar R1/R2/R3/R4.
static int V_infantil;
static int V_seguro;
static int V_recomendar;
static int V_consent;
static int V_authDelayMs;
static int V_authChamadas;
static int V_authCancelado;

int perfil_infantil_ativo(void) { return V_infantil; }
int seguro_perfil_ativo(void)   { return V_seguro; }
int farolsinal_recomendar(void) { return V_recomendar; }
int farolconsent_estado(void)   { return V_consent; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int ajustes_teclado_android(void) { return 0; }

int recomenda_cabecalhos(const char **cab, char *aut, size_t na,
                         char *via, size_t nv, char *perfil, size_t np) {
  int restante = V_authDelayMs;
  V_authChamadas++;
  while (restante > 0) {
    RedePararEstado cur = rede_parar_trocar(NULL, NULL);
    rede_parar_trocar(cur.parar, cur.usuario);
    if (cur.parar && cur.parar(cur.usuario)) {
      V_authCancelado = 1;
      return 0;
    }
    usleep(20000);
    restante -= 20;
  }
  snprintf(aut, na, "Authorization: Bearer sint_tok_voz");
  snprintf(via, nv, "X-Nuvio-Auth: nuvio");
  if (perfil && np) perfil[0] = '\0';
  if (cab) {
    cab[0] = aut;
    cab[1] = via;
    cab[2] = NULL;
    cab[3] = NULL;
  }
  return 1;
}

#define CAT_CAP 64
static CatItem  A_itens[CAT_CAP];
static int      A_n;
static unsigned A_rev = 1u;
static int      A_acrescentados;
static int      A_repubOnAcrescentar;
static int      A_abriuN;
static int      A_detailAberto;
static int      A_abriuIdx = -1;
static char     A_abriuImdb[64];
static char     A_abriuTipo[8];

const CatItem *cat_item(int i) {
  return (i >= 0 && i < A_n) ? &A_itens[i] : NULL;
}
int cat_n(void) { return A_n; }
unsigned cat_revisao(void) { return A_rev; }

int cat_acrescentar(const CatItem *item) {
  int idx;
  if (!item || A_n >= CAT_CAP) return -1;
  idx = A_n;
  A_itens[A_n] = *item;
  A_rev++;
  A_acrescentados++;
  A_n++;
  if (A_repubOnAcrescentar == 1) {
    // Simula republicacao na janela pos-cat_acrescentar substituindo o item.
    memset(&A_itens[idx], 0, sizeof A_itens[idx]);
    snprintf(A_itens[idx].imdb, sizeof A_itens[idx].imdb, "tt8888888");
    snprintf(A_itens[idx].tipo, sizeof A_itens[idx].tipo, "movie");
    snprintf(A_itens[idx].titulo, sizeof A_itens[idx].titulo, "obra_outra");
    A_rev++;
  } else if (A_repubOnAcrescentar == 2 && A_n < CAT_CAP) {
    // Simula republicacao deslocando o item recem-acrescentado para idx + 1.
    A_itens[idx + 1] = A_itens[idx];
    memset(&A_itens[idx], 0, sizeof A_itens[idx]);
    snprintf(A_itens[idx].imdb, sizeof A_itens[idx].imdb, "tt8888888");
    snprintf(A_itens[idx].tipo, sizeof A_itens[idx].tipo, "movie");
    snprintf(A_itens[idx].titulo, sizeof A_itens[idx].titulo, "obra_outra");
    A_n++;
    A_rev++;
  }
  return idx;
}

int cacavisto_filme(const char *imdb, long tmdb, long long *quando) {
  (void)imdb; (void)tmdb;
  if (quando) *quando = 0;
  return 0;
}

static void player_descartar_retido(void) {}

static void detail_abrir(const HomeItem *it) {
  const CatItem *c = it ? cat_item(it->indice) : NULL;
  A_abriuN++;
  A_detailAberto = 1;
  A_abriuIdx = it ? it->indice : -1;
  A_abriuImdb[0] = '\0';
  A_abriuTipo[0] = '\0';
  if (c) {
    snprintf(A_abriuImdb, sizeof A_abriuImdb, "%s", c->imdb);
    snprintf(A_abriuTipo, sizeof A_abriuTipo, "%s", c->tipo);
  }
}
static int  detail_aberto(void) { return A_detailAberto; }
static int  detail_indice(void) { return A_detailAberto ? A_abriuIdx : -1; }
static void detail_fechar_seco(void) { A_detailAberto = 0; }

static int falhas;
#define CONFERE(c) do { \
  if (!(c)) { \
    fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); \
    falhas++; \
  } \
} while (0)

static long agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000L + t.tv_nsec / 1000000L;
}

static void zera_tudo(void) {
  farolbusca_encerrar();
  zerar();
  memset(A_itens, 0, sizeof A_itens);
  A_n = 0;
  A_rev++;
  A_acrescentados = 0;
  A_repubOnAcrescentar = 0;
  A_abriuN = 0;
  A_detailAberto = 0;
  A_abriuIdx = -1;
  A_abriuImdb[0] = '\0';
  A_abriuTipo[0] = '\0';
  V_infantil = 0;
  V_seguro = 0;
  V_recomendar = 0;
  V_consent = 0;
  V_authDelayMs = 0;
  V_authChamadas = 0;
  V_authCancelado = 0;
  c_reduzidas = 1;
  farolbusca_definir_privado(-1);
  farolbusca_definir_consentimento(-1);
  farolbusca_definir_auth(NULL, NULL);
  farolbusca_definir_semente(42u);
  resolver_teste_esquecer();
}

static int add_cat(const char *imdb, long tmdb, const char *tipo,
                   const char *titulo, const char *genero, const char *meta,
                   const char *classif, const char *poster) {
  int i = A_n++;
  CONFERE(i < CAT_CAP);
  memset(&A_itens[i], 0, sizeof A_itens[i]);
  snprintf(A_itens[i].imdb, sizeof A_itens[i].imdb, "%s", imdb ? imdb : "");
  A_itens[i].tmdb = tmdb;
  snprintf(A_itens[i].tipo, sizeof A_itens[i].tipo, "%s", tipo ? tipo : "movie");
  snprintf(A_itens[i].titulo, sizeof A_itens[i].titulo, "%s", titulo ? titulo : "");
  snprintf(A_itens[i].genero, sizeof A_itens[i].genero, "%s", genero ? genero : "");
  snprintf(A_itens[i].meta, sizeof A_itens[i].meta, "%s", meta ? meta : "");
  snprintf(A_itens[i].classificacao, sizeof A_itens[i].classificacao, "%s", classif ? classif : "");
  snprintf(A_itens[i].poster, sizeof A_itens[i].poster, "%s", poster ? poster : "");
  A_itens[i].nota = 84;
  A_rev++;
  return i;
}

static void seed_catalogo(void) {
  add_cat("tt0000101", 550,  "movie",  "obra_adulta",   "Filme · Suspense",       "2024 · 92 min", "16", "https://img.invalid/p101.jpg");
  add_cat("tt0000102", 1396, "series", "obra_sem_class","Programa de TV · Drama", "2023 · 48 min", "",   "https://img.invalid/p102.jpg");
  add_cat("tt0000105", 778,  "movie",  "obra_livre",    "Filme · Animação",       "2025 · 86 min", "L",  "https://img.invalid/p105.jpg");
  add_cat("tt0000103", 777,  "movie",  "obra_curta_s",  "Filme · Suspense",       "2024 · 84 min", "12", "https://img.invalid/p103.jpg");
}

static void tecla_rep(SDL_Keycode k, Uint8 rep) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.repeat = rep;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}
static void tecla(SDL_Keycode k) { tecla_rep(k, 0); }

static void tecla_app_rep(SDL_Keycode k, Uint8 rep) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.repeat = rep;
  e.key.keysym.sym = k;
  app_resolver_despachar(&e);
}
static void tecla_app(SDL_Keycode k) { tecla_app_rep(k, 0); }

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

static void varredura_fase(int esperados, int faseEsperada, int vozAtiva) {
  enum { MAXF = 64, PASSOS = 1600 };
  static const SDL_Keycode DIR[4] = { SDLK_RIGHT, SDLK_LEFT, SDLK_DOWN, SDLK_UP };
  int vis[MAXF], nvis = 0, saida[MAXF] = { 0 };
  static unsigned char aresta[MAXF][MAXF];
  int i, j, k;
  unsigned seed = 7331u;
  memset(aresta, 0, sizeof aresta);
  vis[nvis++] = resolver_foco();
  for (int p = 0; p < PASSOS; p++) {
    int antes = resolver_foco(), depois, a, b;
    seed = seed * 1103515245u + 12345u;
    tecla(DIR[(seed >> 16) & 3]);
    if (!resolver_aberto() || (!vozAtiva && resolver_fase() != faseEsperada) ||
        (vozAtiva && !farolvozui_ativo())) {
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

static void ir_foco(int alvo) {
  int i;
  if (resolver_foco() == alvo) return;
  if (resolver_fase() == RESOLVER_FASE_ACHAR) {
    if (alvo == RESOLVER_B_ACHAR_VOZ) {
      tecla(SDLK_DOWN);
      tecla(SDLK_RIGHT);
    } else if (alvo == RESOLVER_B_ACHAR_VOLTAR) {
      tecla(SDLK_DOWN);
      tecla(SDLK_LEFT);
    } else {
      tecla(SDLK_UP);
      for (i = 0; i < 4 && resolver_foco() != alvo; i++) {
        if (resolver_foco() < alvo) tecla(SDLK_RIGHT);
        else tecla(SDLK_LEFT);
      }
    }
    CONFERE(resolver_foco() == alvo);
    return;
  }
  for (i = 0; i < 8 && resolver_foco() != alvo; i++) tecla(SDLK_RIGHT);
  for (i = 0; i < 6 && resolver_foco() != alvo; i++) tecla(SDLK_UP);
  for (i = 0; i < 6 && resolver_foco() != alvo; i++) tecla(SDLK_DOWN);
  for (i = 0; i < 4 && resolver_foco() != alvo; i++) tecla(SDLK_LEFT);
  CONFERE(resolver_foco() == alvo);
}

static FarolBuscaInfo esperar_busca(long maxMs) {
  FarolBuscaInfo info;
  long ini = agoraMs();
  for (;;) {
    FarolBuscaEstado st = farolbusca_estado(&info);
    if (st != FB_BUSCANDO) return info;
    CONFERE(agoraMs() - ini < maxMs);
    if (agoraMs() - ini >= maxMs) return info;
    usleep(5000);
  }
}

static void esperar_voz_ir_para_achados(long maxMs) {
  long ini = agoraMs();
  while (farolvozui_ativo() && agoraMs() - ini < maxMs) {
    resolver_atualizar(0.016f, 0);
    usleep(5000);
  }
}

int main(int argc, char **argv) {
  char rota[512];
  const char *base;
  FILE *f;
  long reqAntes;
  (void)argc;
  CONFERE(argc == 3);
  base = argv[1];

  // ========================================================= R1: auth lenta 1s
  zera_tudo();
  seed_catalogo();
  snprintf(rota, sizeof rota, "%s/ok", base);
  farolbusca_definir_url(rota);
  V_authDelayMs = 1000;
  {
    long ini = agoraMs(), dt;
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("suspense rapido r1") == 1);
    dt = agoraMs() - ini;
    CONFERE(dt < 100); // NUNCA bloqueia fio da UI!
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
    CONFERE(V_authChamadas == 1);
  }

  // R1 cancelamento durante auth lenta: encerra em < 300 ms como FB_CANCELADO.
  zera_tudo();
  seed_catalogo();
  snprintf(rota, sizeof rota, "%s/ok", base);
  farolbusca_definir_url(rota);
  V_authDelayMs = 2000;
  {
    long ini;
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("cancelar auth") == 1);
    usleep(80000);
    ini = agoraMs();
    farolbusca_cancelar();
    info = esperar_busca(1000);
    CONFERE(agoraMs() - ini < 300);
    CONFERE(info.estado == FB_CANCELADO && info.motivo == FB_MOTIVO_CANCELADO);
    CONFERE(V_authCancelado == 1);
  }

  // ========================================================= R2: filtro infantil
  zera_tudo();
  seed_catalogo();
  V_infantil = 1;
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  farolbusca_definir_auth("Authorization: Bearer sint_tok_voz", "X-Nuvio-Auth: nuvio");
  {
    FarolBuscaInfo info;
    FarolBuscaItem itens[FAROL_BUSCA_IDS_MAX], fora;
    unsigned rev = 0;
    int n;
    CONFERE(farolbusca_pedir("desenho livre") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK);
    n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
    // /misto devolve: tt0000101 (16), tt0000102 (sem classif), tt0000105 (L), tt9990001 (fora).
    // No perfil infantil SO tt0000105 pode sair!
    CONFERE(n == 1);
    CONFERE(!strcmp(itens[0].imdb, "tt0000105") && itens[0].noCatalogo == 1);
    // Nunca abre item fora do catalogo no perfil infantil.
    memset(&fora, 0, sizeof fora);
    snprintf(fora.imdb, sizeof fora.imdb, "tt9990001");
    snprintf(fora.tipo, sizeof fora.tipo, "movie");
    fora.tmdb = 999001;
    CONFERE(farolbusca_selecionar(&fora) == 0);
    CONFERE(farolbusca_abrir_item(&fora) == -1);
    CONFERE(A_acrescentados == 0);
    // Nem item adulto do catalogo.
    memset(&fora, 0, sizeof fora);
    snprintf(fora.imdb, sizeof fora.imdb, "tt0000101");
    snprintf(fora.tipo, sizeof fora.tipo, "movie");
    fora.tmdb = 550;
    CONFERE(farolbusca_abrir_item(&fora) == -1);
  }
  // Fallback local infantil tambem filtra classificacao.
  c_offline = 1;
  {
    FarolBuscaInfo info;
    FarolBuscaItem itens[FAROL_BUSCA_IDS_MAX];
    unsigned rev = 0;
    int n;
    CONFERE(farolbusca_pedir("um suspense curto") == 1);
    info = esperar_busca(1000);
    CONFERE(info.estado == FB_PRONTO && info.usouLocal == 1);
    n = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
    CONFERE(n == 1 && !strcmp(itens[0].imdb, "tt0000105"));
  }
  c_offline = 0;

  // ========================================================= R3 + R4: preferencias reais
  // R3: seguro_perfil_ativo() + perfil_infantil_ativo() envia infantil:1 e consentiu:0!
  zera_tudo();
  seed_catalogo();
  V_seguro = 1;
  V_infantil = 1;
  V_recomendar = 1;
  V_consent = 1;
  snprintf(rota, sizeof rota, "%s/privado-infantil", base);
  farolbusca_definir_url(rota);
  {
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("aventura livre") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }

  // R4 + P2-3: adulto nao privado -> consentiu:1 SO quando farolsinal_recomendar() && farolconsent_estado() == 2 (SIM).
  // "So nesta TV" (1) NUNCA vira consentiu:1!
  zera_tudo();
  seed_catalogo();
  V_seguro = 0;
  V_infantil = 0;
  V_recomendar = 0;
  V_consent = 2;
  CONFERE(farolbusca_consentimento() == 0);
  snprintf(rota, sizeof rota, "%s/sem-consent", base);
  farolbusca_definir_url(rota);
  {
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("drama r4 desligado") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }

  V_recomendar = 1;
  V_consent = 0;
  CONFERE(farolbusca_consentimento() == 0);
  {
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("drama r4 sem consent") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }

  V_recomendar = 1;
  V_consent = 1; // SO_TV (1) -> consentiu:0!
  CONFERE(farolbusca_consentimento() == 0);
  {
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("drama r4 so tv") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }

  V_recomendar = 1;
  V_consent = 2; // SIM (2) -> consentiu:1!
  CONFERE(farolbusca_consentimento() == 1);
  snprintf(rota, sizeof rota, "%s/so-consent1", base);
  farolbusca_definir_url(rota);
  {
    FarolBuscaInfo info;
    CONFERE(farolbusca_pedir("drama r4 com consent") == 1);
    info = esperar_busca(3000);
    CONFERE(info.estado == FB_PRONTO && info.motivo == FB_MOTIVO_OK && info.http == 200);
  }

  // ========================================================= Fluxo de voz + D-pad
  // Sem st_voz_disponivel(): botao de microfone oculto (13 focos na Tela 1).
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(0);
  CONFERE(st_voz_disponivel() == 0);
  CONFERE(farolvozui_disponivel() == 0);
  resolver_abrir();
  varredura_fase(13, RESOLVER_FASE_INICIO, 0);

  // Com st_voz_disponivel() == 1: botao de microfone aparece na Tela 1 (14 focos).
  st_teste_ligar(1);
  CONFERE(st_voz_disponivel() == 1);
  CONFERE(farolvozui_disponivel() == 1);
  resolver_abrir();
  desenhar();
  CONFERE(escreveu(FT_ACAO_FALAR));
  varredura_fase(14, RESOLVER_FASE_INICIO, 0);

  // OK com repeat=1 no microfone e ignorado; repeat=0 inicia ditado ("Ouvindo…").
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla_rep(SDLK_RETURN, 1);
  CONFERE(!farolvozui_ativo());
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_OUVINDO);
  CONFERE(st_estado() == ST_OUVINDO);
  desenhar();
  CONFERE(escreveu(FT_OUVINDO));
  varredura_fase(1, RESOLVER_FASE_SINTOMA, 1);

  // Texto parcial via st_ler() sem disparar HTTP; Voltar cancela (st_fechar) com 0 POSTs.
  reqAntes = 0;
  f = fopen(argv[2], "r");
  if (f) { CONFERE(fscanf(f, "%ld", &reqAntes) == 1); fclose(f); }

  st_teste_evento("R75");
  st_teste_evento("Psuspense parcial");
  desenhar();
  CONFERE(!strcmp(farolvozui_parcial(), "suspense parcial"));
  CONFERE(escreveu("suspense parcial"));
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && st_estado() == ST_PARADO);
  CONFERE(resolver_fase() == RESOLVER_FASE_SINTOMA && resolver_foco() == RESOLVER_B_ACHAR_VOZ);

  // Gancho #32 (farolvozui_abrir_texto): exibe texto ANTES de enviar; Voltar descarta com 0 POSTs.
  CONFERE(farolvozui_abrir_texto("  comedia   leve ", RESOLVER_FASE_SINTOMA) == 1);
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(!strcmp(farolvozui_frase(), "comedia leve"));
  desenhar();
  CONFERE(escreveu(FT_VOCE_DISSE));
  CONFERE(escreveu("comedia leve"));
  varredura_fase(3, RESOLVER_FASE_SINTOMA, 1); // Confirmar, Falar, Voltar
  tecla(SDLK_AC_BACK);
  CONFERE(!farolvozui_ativo());

  // Ditado real -> revisao antes do envio -> Voltar descarta com 0 POSTs.
  tecla(SDLK_RETURN); // abre voz de novo (foco ainda em RESOLVER_B_ACHAR_VOZ)
  CONFERE(farolvozui_estado() == FV_OUVINDO);
  st_teste_evento("Vum suspense curto");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(!strcmp(farolvozui_frase(), "um suspense curto"));
  CONFERE(escreveu(FT_VOCE_DISSE));
  CONFERE(escreveu(FT_CONFIRMA_DESCARTA));
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo());
  {
    long reqDepois = -1;
    f = fopen(argv[2], "r");
    CONFERE(f != NULL);
    if (f) { CONFERE(fscanf(f, "%ld", &reqDepois) == 1); fclose(f); }
    CONFERE(reqDepois == reqAntes); // 0 pedidos HTTP!
  }

  // Cancelamento em voo (FV_BUSCANDO) contra /slow em < 300 ms.
  snprintf(rota, sizeof rota, "%s/slow", base);
  farolbusca_definir_url(rota);
  tecla(SDLK_RETURN);
  st_teste_evento("Vfilme demorado");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO && resolver_foco() == FV_FOCO_CONFIRMAR);
  // key.repeat=1 NAO confirma envio!
  tecla_rep(SDLK_RETURN, 1);
  CONFERE(farolvozui_estado() == FV_REVISAO);
  // key.repeat=0 confirma e entra em FV_BUSCANDO sem travar UI.
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  desenhar();
  CONFERE(escreveu(FT_BUSCA_FRASE_PROCURANDO));
  usleep(80000);
  {
    long ini = agoraMs();
    tecla(SDLK_ESCAPE);
    CONFERE(!farolvozui_ativo());
    CONFERE(farolbusca_estado(NULL) == FB_BUSCANDO || farolbusca_estado(NULL) == FB_CANCELADO);
    CONFERE(esperar_busca(1000).estado == FB_CANCELADO);
    CONFERE(agoraMs() - ini < 300);
  }

  // ========================================================= Fluxo completo -> Tela 7 -> Abrir titulo
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  esperar_voz_ir_para_achados(3000);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3); // ate 3 cartazes
  desenhar();
  CONFERE(escreveu("um suspense curto"));
  // Na Tela 7 com voz disponivel: 3 cartazes + Voltar + Falar = 5 focos sem beco.
  varredura_fase(5, RESOLVER_FASE_ACHAR, 0);

  // Abrir microfone a partir da Tela 7 e cancelar volta para Tela 7 intacta.
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_OUVINDO);
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);

  // Mudar cat_revisao() com Tela 7 aberta re-resolve os achados e abre pelo contrato FarolAchaPedido.
  memmove(&A_itens[1], &A_itens[0], sizeof(CatItem) * (size_t)A_n);
  A_n++;
  memset(&A_itens[0], 0, sizeof A_itens[0]);
  snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9999000");
  snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
  snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "desloca");
  A_rev++;
  desenhar();
  CONFERE(farolachaui_revisao() == A_rev);

  // Primeiro cartaz (tt0000101, agora no indice 1): OK passa pelo bloco real de app.c!
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuN == 1);
  CONFERE(A_abriuIdx == 1 && !strcmp(A_abriuImdb, "tt0000101"));

  // Terceiro cartaz (tt9990001, fora do catalogo): acrescenta pelo id e abre via FarolAchaPedido.
  A_detailAberto = 0; A_abriuN = 0; A_abriuIdx = -1; A_abriuImdb[0] = '\0';
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  ir_foco(RESOLVER_B_ACHAR_RETOMAR); // terceiro cartaz (indice 2)
  {
    int antesAcr = A_acrescentados;
    tecla_app(SDLK_RETURN);
    CONFERE(!resolver_aberto());
    CONFERE(A_acrescentados == antesAcr + 1);
    CONFERE(A_detailAberto == 1 && A_abriuN == 1);
    CONFERE(!strcmp(A_abriuImdb, "tt9990001"));
  }

  // R8: geracao coerente — pedido 1 lento substituido por pedido 2 (429) nao mistura cabecalho e resultados.
  {
    FarolBuscaInfo i1, i2;
    uint64_t g1;
    zera_tudo();
    seed_catalogo();
    st_teste_ligar(1);
    snprintf(rota, sizeof rota, "%s/slow", base);
    farolbusca_definir_url(rota);
    CONFERE(farolbusca_pedir("pedido lento um") == 1);
    farolbusca_estado(&i1);
    g1 = i1.geracao;
    usleep(50000);
    snprintf(rota, sizeof rota, "%s/429", base);
    farolbusca_definir_url(rota);
    resolver_abrir();
    CONFERE(farolvozui_abrir_texto("um suspense curto", RESOLVER_FASE_SINTOMA) == 1);
    tecla(SDLK_RETURN);
    esperar_voz_ir_para_achados(3000);
    farolbusca_estado(&i2);
    CONFERE(i2.geracao > g1 && i2.motivo == FB_MOTIVO_LIMITE);
    desenhar();
    CONFERE(escreveu(FT_BUSCA_FRASE_LIMITE));
    CONFERE(farolachaui_n() >= 1);
  }

  // ========================================================= Fallback 429 e Offline na Tela 7
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/429", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() >= 1);
  desenhar();
  CONFERE(escreveu(FT_BUSCA_FRASE_LIMITE));

  // Offline: cae imediato no fallback local com FT_BUSCA_FRASE_OFFLINE.
  c_offline = 1;
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  desenhar();
  CONFERE(escreveu(FT_BUSCA_FRASE_OFFLINE));
  c_offline = 0;

  // ========================================================= S9 (FAROL31B): re-ditado B -> 429 + Voltar antes de adotar B
  // Tela 7 mostra consulta A ("um suspense curto", 3 titulos). Usuario aciona
  // microfone na Tela 7, dita B ("comedia rapida") e confirma; B responde 429
  // (motivo=5), mas usuario aperta Voltar antes da Tela 7 adotar B.
  // garantirRevisao() NAO pode adotar B nem misturar titulo A com aviso de B!
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  desenhar();
  CONFERE(escreveu("um suspense curto"));

  // Aciona microfone da Tela 7, dita B, confirma contra /429, espera B chegar
  // no backend (FB_PRONTO motivo=5) ainda sem rodar farolvozui_atualizar, e
  // aperta Voltar para cancelar B antes de Tela 7 adotar B.
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_OUVINDO);
  snprintf(rota, sizeof rota, "%s/429", base);
  farolbusca_definir_url(rota);
  st_teste_evento("Vcomedia rapida");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  {
    FarolBuscaInfo infoB = esperar_busca(3000);
    CONFERE(infoB.estado == FB_PRONTO && infoB.motivo == FB_MOTIVO_LIMITE);
  }
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  desenhar();
  CONFERE(farolachaui_n() == 3);
  CONFERE(escreveu("um suspense curto"));
  CONFERE(!escreveu("comedia rapida"));
  CONFERE(!escreveu(FT_BUSCA_FRASE_LIMITE));
  // OK no 1o cartaz de A continua abrindo a obra de A (tt0000101).
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuN == 1);
  CONFERE(!strcmp(A_abriuImdb, "tt0000101"));

  // Quando B (429) E adotada pela Tela 7, titulo B e aviso de limite de B
  // atualizam juntos (nunca titulo A com resultados/aviso de B).
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  snprintf(rota, sizeof rota, "%s/429", base);
  farolbusca_definir_url(rota);
  st_teste_evento("Vcomedia rapida");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  desenhar();
  CONFERE(escreveu("comedia rapida"));
  CONFERE(!escreveu("um suspense curto"));
  CONFERE(escreveu(FT_BUSCA_FRASE_LIMITE));

  // ========================================================= S4 (FAROL31B): busca lenta em voo cancelada na Tela 7
  // B e lenta (/slow); usuario cancela ainda em FV_BUSCANDO e volta para A.
  // Tela 7 preserva os 3 cartazes e titulo de A; OK no 1o cartaz de A abre A
  // (inclusive mesmo se cat_revisao() mudar apos o cancelamento!).
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);

  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  snprintf(rota, sizeof rota, "%s/slow", base);
  farolbusca_definir_url(rota);
  st_teste_evento("Vfilme lento b");
  desenhar();
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  usleep(80000);
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(esperar_busca(1000).estado == FB_CANCELADO);
  desenhar();
  CONFERE(farolachaui_n() == 3);
  CONFERE(escreveu("um suspense curto"));
  CONFERE(!escreveu(FT_BUSCA_FRASE_PROCURANDO));
  // OK no 1o cartaz de A logo apos cancelar B (mesma revisao) abre tt0000101!
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuN == 1);
  CONFERE(A_abriuIdx == 0 && !strcmp(A_abriuImdb, "tt0000101"));

  // Tambem com cat_revisao() mudando apos o cancelamento de B: A continua valida!
  A_detailAberto = 0; A_abriuN = 0; A_abriuIdx = -1; A_abriuImdb[0] = '\0';
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  snprintf(rota, sizeof rota, "%s/slow", base);
  farolbusca_definir_url(rota);
  st_teste_evento("Vfilme lento c");
  desenhar();
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  usleep(80000);
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(esperar_busca(1000).estado == FB_CANCELADO);
  // Desloca catalogo mesmo com farolbusca em FB_CANCELADO de B: A continua valida!
  memmove(&A_itens[1], &A_itens[0], sizeof(CatItem) * (size_t)A_n);
  A_n++;
  memset(&A_itens[0], 0, sizeof A_itens[0]);
  snprintf(A_itens[0].imdb, sizeof A_itens[0].imdb, "tt9999001");
  snprintf(A_itens[0].tipo, sizeof A_itens[0].tipo, "movie");
  snprintf(A_itens[0].titulo, sizeof A_itens[0].titulo, "desloca_s4");
  A_rev++;
  desenhar();
  CONFERE(farolachaui_revisao() == A_rev);
  CONFERE(farolachaui_n() == 3);
  CONFERE(escreveu("um suspense curto"));
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && A_abriuN == 1);
  CONFERE(A_abriuIdx == 1 && !strcmp(A_abriuImdb, "tt0000101"));

  // ========================================================= P2-1 (FAROL31B): janela pos-cat_acrescentar
  // Caso 1: catalogo republicado substitui o item recem-acrescentado por outra obra -> aborta sem abrir outra obra!
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  A_repubOnAcrescentar = 1;
  ir_foco(RESOLVER_B_ACHAR_RETOMAR); // 3o cartaz (tt9990001, fora do catalogo)
  tecla_app(SDLK_RETURN);
  A_repubOnAcrescentar = 0;
  CONFERE(resolver_aberto());
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);

  // Caso 2: catalogo republicado desloca o item recem-acrescentado para idx+1 -> rebusca pela identidade e abre tt9990001!
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  A_repubOnAcrescentar = 2;
  ir_foco(RESOLVER_B_ACHAR_RETOMAR); // 3o cartaz (tt9990001)
  tecla_app(SDLK_RETURN);
  A_repubOnAcrescentar = 0;
  CONFERE(!resolver_aberto());
  CONFERE(A_abriuN == 1 && A_detailAberto == 1);
  CONFERE(!strcmp(A_abriuImdb, "tt9990001"));

  // ========================================================= P2-4 (FAROL31B): revalidacao infantil no OK
  // Liga perfil infantil DEPOIS de listar em adulto e ANTES do OK no cartaz "16" catalogado.
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  st_teste_evento("Vum suspense curto");
  desenhar();
  tecla(SDLK_RETURN);
  esperar_voz_ir_para_achados(3000);
  CONFERE(farolachaui_n() == 3);
  {
    FarolBuscaItem itens[FAROL_BUSCA_IDS_MAX];
    unsigned rev = 0;
    int nr = farolbusca_resolver(itens, FAROL_BUSCA_IDS_MAX, &rev);
    int idxAbrir = -1;
    CONFERE(nr >= 2 && !strcmp(itens[0].imdb, "tt0000101") && itens[0].noCatalogo == 1);
    CONFERE(!strcmp(itens[1].imdb, "tt0000105") && itens[1].noCatalogo == 1);
    V_infantil = 1;
    // farolbusca_selecionar bloqueia cartaz "16" catalogado, mas libera cartaz "L".
    CONFERE(farolbusca_selecionar(&itens[0]) == 0);
    CONFERE(farolbusca_selecionar(&itens[1]) == 1);
    CONFERE(farolbusca_pediu_abrir(&idxAbrir) == 1 && idxAbrir == 2);
  }
  // selecionarFocado (via OK na Tela 7) tambem bloqueia o cartaz "16" (tt0000101).
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(resolver_aberto());
  CONFERE(A_abriuN == 0 && A_detailAberto == 0);
  V_infantil = 0;

  // Escala e animacoes reduzidas desligadas (exercicio sem crashes).
  c_reduzidas = 0;
  CONFERE(resolver_escala() > 0.0f);
  CONFERE(farolvozui_abrir_texto("teste animacao", RESOLVER_FASE_SINTOMA) == 1);
  desenhar();
  farolvozui_fechar();
  resolver_fechar();
  farolbusca_encerrar();

  if (falhas) {
    fprintf(stderr, "farol_voz: %d falha(s)\n", falhas);
    return 1;
  }
  puts("farol_voz: R1, R2, R3, R4, R8, fluxo de voz e D-pad ok");
  return 0;
}

#else // NV_VOZ_TELA_SHOT

#ifndef RESOLVER_TESTE
#define RESOLVER_TESTE 1
#endif
#include "resolver.h"
#include "resolverregra.h"
#include "farol.h"
#include "farolachaui.h"
#include "farolvozui.h"
#include "farolbusca.h"
#include "faroltexto.h"
#include "sistexto.h"
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

static int captura(const char *nome, int quadros) {
  unsigned char *pix = malloc(1920 * 1080 * 4);
  SDL_Surface *s;
  int i, y, variou = 0;
  assert(pix);
  for (i = 0; i < quadros; i++) {
    quadro(1.0f / 60.0f);
    SDL_Delay(2);
  }
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

static void criar_poster(const char *path, Uint8 r0, Uint8 g0, Uint8 b0) {
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
  const char *saida = argc > 1 ? argv[1] : "/tmp/farol-voz-shot";
  const char *baseUrl = argc > 2 ? argv[2] : NULL;
  const char *dir = getenv("NUVIO_DADOS");
  char p1[600], p2[600], p3[600], n1[700], n2[700], n3[700], n4[700];
  CatItem itens[3];
  SDL_Window *w;
  SDL_GLContext gl;
  ResolverFatos rf;
  FarolFatos ff;
  int i, lisas = 0;

  if (!dir || !dir[0] || !baseUrl) return 2;
  dados_iniciar(dir);
  ajustes_dir(dir);
  st_teste_ligar(1);

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
  w = SDL_CreateWindow("farol-voz-shot", 0, 0, 64, 64,
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
  criar_poster(p1, 40, 80, 140);
  criar_poster(p2, 130, 55, 70);
  criar_poster(p3, 45, 115, 95);

  memset(itens, 0, sizeof itens);
  snprintf(itens[0].imdb, sizeof itens[0].imdb, "tt0000101");
  snprintf(itens[0].tipo, sizeof itens[0].tipo, "movie");
  snprintf(itens[0].titulo, sizeof itens[0].titulo, "Amostra Suspense");
  snprintf(itens[0].genero, sizeof itens[0].genero, "Filme · Suspense");
  snprintf(itens[0].meta, sizeof itens[0].meta, "2024 · 84 min");
  snprintf(itens[0].poster, sizeof itens[0].poster, "%s", p1);
  itens[0].nota = 86;

  snprintf(itens[1].imdb, sizeof itens[1].imdb, "tt0000102");
  itens[1].tmdb = 1396;
  snprintf(itens[1].tipo, sizeof itens[1].tipo, "series");
  snprintf(itens[1].titulo, sizeof itens[1].titulo, "Amostra Serie");
  snprintf(itens[1].genero, sizeof itens[1].genero, "Programa de TV · Suspense");
  snprintf(itens[1].meta, sizeof itens[1].meta, "2025 · 48 min");
  snprintf(itens[1].poster, sizeof itens[1].poster, "%s", p2);
  itens[1].nota = 88;

  snprintf(itens[2].imdb, sizeof itens[2].imdb, "tt0000103");
  snprintf(itens[2].tipo, sizeof itens[2].tipo, "movie");
  snprintf(itens[2].titulo, sizeof itens[2].titulo, "Amostra Curta");
  snprintf(itens[2].genero, sizeof itens[2].genero, "Filme · Suspense");
  snprintf(itens[2].meta, sizeof itens[2].meta, "2023 · 96 min");
  snprintf(itens[2].poster, sizeof itens[2].poster, "%s", p3);
  itens[2].nota = 82;
  cat_definir(itens, 3);

  farolbusca_definir_url(baseUrl);
  farolbusca_definir_auth("Authorization: Bearer sint_tok_voz", "X-Nuvio-Auth: nuvio");

  memset(&rf, 0, sizeof rf);
  rf.addonsQtd = 3;
  resolver_teste_fatos(&rf);
  farolregra_fatos_iniciar(&ff);
  ff.plat = FP_ANDROID;
  ff.addonsQtd = 3;
  resolver_teste_fatos_farol(&ff);

  resolver_abrir();
  for (i = 0; i < 6 && resolver_foco() != RESOLVER_B_ACHAR_VOZ; i++)
    tecla(SDLK_RIGHT);
  assert(resolver_foco() == RESOLVER_B_ACHAR_VOZ);

  snprintf(n1, sizeof n1, "%s-tela1.png", saida);
  lisas += !captura(n1, 25);

  tecla(SDLK_RETURN);
  assert(farolvozui_estado() == FV_OUVINDO);
  st_teste_evento("R80");
  st_teste_evento("Pum suspense curto");
  snprintf(n2, sizeof n2, "%s-ouvindo.png", saida);
  lisas += !captura(n2, 20);

  st_teste_evento("Vum suspense curto");
  quadro(1.0f / 60.0f);
  assert(farolvozui_estado() == FV_REVISAO);
  snprintf(n3, sizeof n3, "%s-revisao.png", saida);
  lisas += !captura(n3, 20);

  tecla(SDLK_RETURN);
  for (i = 0; i < 200 && farolvozui_ativo(); i++) {
    quadro(1.0f / 60.0f);
    SDL_Delay(10);
  }
  assert(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  assert(farolachaui_n() >= 1);
  snprintf(n4, sizeof n4, "%s-tela7.png", saida);
  lisas += !captura(n4, 25);

  farolbusca_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  if (lisas) {
    fprintf(stderr, "FALHA: farol_voz shot gerou captura lisa\n");
    return 1;
  }
  puts("farol_voz: capturas 1920x1080 ok");
  return 0;
}

#endif
