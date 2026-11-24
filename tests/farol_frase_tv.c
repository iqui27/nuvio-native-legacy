// Teste integrado da issue Farol #32 ("Demais TVs: entrada de frase no Achar o que ver"):
//   - Samsung .tpk/.wgt e WASM (voz=0, ime=1, cel=0): teclado/IME, st_evento,
//     ST_FIM -> FV_REVISAO (0 POSTs), repeat=1 ignorado em OK/Voltar, Voltar
//     descarta, OK busca e abre Tela 7, ST_CANCELOU na Tela 7 preserva consulta A.
//   - LG webOS / QR celular (voz=0, ime=0, cel=1): celular_abrir_digitar() real,
//     GET + POST HTTP real em loopback na URL do QR com UTF-8, celular_estado() ==
//     CEL_PARADO apos receber, FV_REVISAO (0 POSTs), Voltar descarta, OK busca e
//     abre Tela 7, Voltar/fechar com QR aberto fecha socket e preserva consulta A,
//     falha/bloqueio (CEL_FALHOU) e expiracao (CEL_EXPIROU) sem beco.
//   - Modo combinado (voz=0, ime=1, cel=1), tudo desligado (0, 0, 0), Android (1, 1, 1).
//   - Modo NV_FRASE_TV_SHOT: captura PNG 1920x1080 do QR celular real e da previa UTF-8.
//
// Nenhum titulo real, URL real ou nome de perfil entra neste teste.

#ifndef NV_FRASE_TV_SHOT

#define FAROL_RESOLVER_SEM_MAIN
#define FAROL_TESTE_CAT_PROPRIO

#include "farolvozui.h"
#include "farolachaui.h"
#include "farolbusca.h"
#include "farolbuscaparse.h"
#include "farolacha.h"
#include "faroltexto.h"
#include "celular.h"
#include "entrada_texto.h"
#include "qr.h"
#include "sistexto.h"
#include "rede.h"
#include "layout.h"
#include "catalogo.h"
#include "home.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
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
#define ajustes_idioma        farol_resolver_stub_idioma
#include "farol_resolver.c"
#undef perfil_infantil_ativo
#undef seguro_perfil_ativo
#undef farolsinal_recomendar
#undef ajustes_idioma

int perfil_infantil_ativo(void) { return 0; }
int seguro_perfil_ativo(void)   { return 0; }
int farolsinal_recomendar(void) { return 0; }
int farolconsent_estado(void)   { return 0; }
const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int ajustes_teclado_android(void)  { return 0; }
int ajustes_idioma(void)           { return 0; }

int recomenda_cabecalhos(const char **cab, char *aut, size_t na,
                         char *via, size_t nv, char *perfil, size_t np) {
  snprintf(aut, na, "Authorization: Bearer sint_tok_tv32");
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
static int      A_abriuN;
static int      A_detailAberto;
static int      A_abriuIdx = -1;
static char     A_abriuImdb[64];

const CatItem *cat_item(int i) {
  return (i >= 0 && i < A_n) ? &A_itens[i] : NULL;
}
int cat_n(void) { return A_n; }
unsigned cat_revisao(void) { return A_rev; }

int cat_acrescentar(const CatItem *item) {
  int idx;
  if (!item || A_n >= CAT_CAP) return -1;
  idx = A_n++;
  A_itens[idx] = *item;
  A_rev++;
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
  if (c) snprintf(A_abriuImdb, sizeof A_abriuImdb, "%s", c->imdb);
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
  farolvozui_teste_modos(-1, -1, -1);
  zerar();
  memset(A_itens, 0, sizeof A_itens);
  A_n = 0;
  A_rev++;
  A_abriuN = 0;
  A_detailAberto = 0;
  A_abriuIdx = -1;
  A_abriuImdb[0] = '\0';
  c_reduzidas = 1;
  farolbusca_definir_privado(-1);
  farolbusca_definir_consentimento(-1);
  farolbusca_definir_auth(NULL, NULL);
  farolbusca_definir_semente(42u);
  resolver_teste_esquecer();
  celular_fechar();
}

static void add_cat(const char *imdb, long tmdb, const char *tipo,
                    const char *titulo, const char *genero, const char *meta) {
  int i = A_n++;
  CONFERE(i < CAT_CAP);
  memset(&A_itens[i], 0, sizeof A_itens[i]);
  snprintf(A_itens[i].imdb, sizeof A_itens[i].imdb, "%s", imdb);
  A_itens[i].tmdb = tmdb;
  snprintf(A_itens[i].tipo, sizeof A_itens[i].tipo, "%s", tipo);
  snprintf(A_itens[i].titulo, sizeof A_itens[i].titulo, "%s", titulo);
  snprintf(A_itens[i].genero, sizeof A_itens[i].genero, "%s", genero);
  snprintf(A_itens[i].meta, sizeof A_itens[i].meta, "%s", meta);
  A_itens[i].nota = 84;
  A_rev++;
}

static void seed_catalogo(void) {
  add_cat("tt0000101", 550,  "movie",  "obra_suspense", "Filme · Suspense", "2024 · 92 min");
  add_cat("tt0000102", 1396, "series", "obra_drama",    "Programa de TV · Drama", "2023 · 48 min");
  add_cat("tt0000103", 777,  "movie",  "obra_curta",    "Filme · Suspense", "2024 · 84 min");
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

static void tecla_app(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  app_resolver_despachar(&e);
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

static long ler_req_count(const char *path) {
  FILE *f = fopen(path, "r");
  long v = -1;
  CONFERE(f != NULL);
  if (f) {
    CONFERE(fscanf(f, "%ld", &v) == 1);
    fclose(f);
  }
  return v;
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
  if (nvis != esperados) {
    fprintf(stderr, "FALHA: esperava %d focos, achou %d\n", esperados, nvis);
    falhas++;
  }
  for (i = 0; i < nvis; i++) CONFERE(saida[i] || nvis == 1);
  for (k = 0; k < nvis; k++)
    for (i = 0; i < nvis; i++)
      for (j = 0; j < nvis; j++)
        if (aresta[i][k] && aresta[k][j]) aresta[i][j] = 1;
  for (i = 0; i < nvis; i++)
    for (j = 0; j < nvis; j++)
      if (i != j && !aresta[i][j]) {
        fprintf(stderr, "FALHA: foco %d nao alcanca %d\n", vis[i], vis[j]);
        falhas++;
      }
}

static void ir_foco(int alvo) {
  int i;
  if (resolver_foco() == alvo) return;
  if (farolvozui_ativo()) {
    for (i = 0; i < 6 && resolver_foco() != alvo; i++) tecla(SDLK_RIGHT);
    for (i = 0; i < 6 && resolver_foco() != alvo; i++) tecla(SDLK_LEFT);
    CONFERE(resolver_foco() == alvo);
    return;
  }
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

static void esperar_voz_ir_para_achados(long maxMs) {
  long ini = agoraMs();
  while (farolvozui_ativo() && agoraMs() - ini < maxMs) {
    resolver_atualizar(0.016f, 0);
    usleep(5000);
  }
}

// Cliente HTTP minimo sobre loopback para falar com celular.c de verdade.
static int http_loopback(int porta, const char *req, char *resp, size_t cap) {
  struct sockaddr_in a;
  struct timeval tv;
  int s = socket(AF_INET, SOCK_STREAM, 0);
  size_t tot = 0, reqLen = strlen(req), env = 0;
  int status = 0;
  if (resp && cap) resp[0] = '\0';
  if (s < 0) return -1;
  tv.tv_sec = 2;
  tv.tv_usec = 0;
  setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
  setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof tv);
  memset(&a, 0, sizeof a);
  a.sin_family = AF_INET;
  a.sin_port = htons((unsigned short)porta);
  inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) {
    close(s);
    return -1;
  }
  while (env < reqLen) {
    ssize_t w = send(s, req + env, reqLen - env, 0);
    if (w <= 0) { close(s); return -1; }
    env += (size_t)w;
  }
  while (resp && tot + 1 < cap) {
    ssize_t r = recv(s, resp + tot, cap - 1 - tot, 0);
    if (r <= 0) break;
    tot += (size_t)r;
  }
  if (resp && cap) resp[tot] = '\0';
  close(s);
  if (resp && sscanf(resp, "HTTP/%*d.%*d %d", &status) == 1) return status;
  return -1;
}

static void url_encode(const char *src, char *dst, size_t cap) {
  static const char HEX[] = "0123456789ABCDEF";
  size_t k = 0;
  const unsigned char *p = (const unsigned char *)src;
  for (; *p && k + 4 < cap; p++) {
    if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') ||
        (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '.') {
      dst[k++] = (char)*p;
    } else if (*p == ' ') {
      dst[k++] = '+';
    } else {
      dst[k++] = '%';
      dst[k++] = HEX[(*p >> 4) & 0xF];
      dst[k++] = HEX[*p & 0xF];
    }
  }
  dst[k] = '\0';
}

static void extrair_host_path(const char *url, char *host, size_t nh,
                              char *path, size_t np) {
  const char *p = !strncmp(url, "http://", 7) ? url + 7 : url;
  const char *slash = strchr(p, '/');
  size_t hl = slash ? (size_t)(slash - p) : strlen(p);
  if (hl >= nh) hl = nh - 1;
  memcpy(host, p, hl);
  host[hl] = '\0';
  snprintf(path, np, "%s", slash ? slash : "/");
}

static int enviar_post_celular(const char *url, int porta, const char *frase) {
  char host[128], path[128], enc[1024], body[1100], req[2048], resp[4096];
  extrair_host_path(url, host, sizeof host, path, sizeof path);
  url_encode(frase, enc, sizeof enc);
  snprintf(body, sizeof body, "t=%s", enc);
  snprintf(req, sizeof req,
           "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Origin: http://%s\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n\r\n%s",
           path, host, host, strlen(body), body);
  return http_loopback(porta, req, resp, sizeof resp);
}

static int fazer_get_celular(const char *url, int porta, char *resp, size_t cap) {
  char host[128], path[128], req[1024];
  extrair_host_path(url, host, sizeof host, path, sizeof path);
  snprintf(req, sizeof req,
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Connection: close\r\n\r\n",
           path, host);
  return http_loopback(porta, req, resp, cap);
}

int main(int argc, char **argv) {
  char rota[512];
  const char *base;
  const char *countFile;
  long reqAntes;
  CONFERE(argc == 3);
  if (argc != 3) return 2;
  base = argv[1];
  countFile = argv[2];

  // ========================================================= 1. Tudo desligado (0, 0, 0)
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(0);
  farolvozui_teste_modos(0, 0, 0);
  CONFERE(farolvozui_disponivel() == 0);
  CONFERE(farolvozui_abrir(RESOLVER_FASE_SINTOMA) == 0);
  resolver_abrir();
  varredura_fase(13, RESOLVER_FASE_INICIO, 0);
  // Entrar na Tela 7 (Curto): 3 cartazes + Voltar = 4 focos (sem botao de frase).
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla(SDLK_RETURN);
  CONFERE(resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  varredura_fase(4, RESOLVER_FASE_ACHAR, 0);

  // ========================================================= 2. Samsung / WASM (voz=0, ime=1, cel=0)
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  farolvozui_teste_modos(0, 1, 0);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);

  CONFERE(farolvozui_disponivel() == 1);
  CONFERE(!strcmp(farolvozui_rotulo_botao(), FT_ACAO_TECLADO_TV));
  resolver_abrir();
  desenhar();
  CONFERE(escreveu(FT_ACAO_TECLADO_TV));
  varredura_fase(14, RESOLVER_FASE_INICIO, 0);

  // OK com repeat=1 ignorado; repeat=0 abre IME (FV_DIGITANDO).
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla_rep(SDLK_RETURN, 1);
  CONFERE(!farolvozui_ativo());
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_DIGITANDO);
  CONFERE(farolvozui_modo_atual() == FV_MODO_IME);
  CONFERE(st_estado() == ST_DIGITANDO);
  desenhar();
  CONFERE(escreveu(FT_BUSCA_FRASE_PROMPT));
  CONFERE(escreveu(FT_BUSCA_FRASE_EXEMPLO));
  varredura_fase(1, RESOLVER_FASE_SINTOMA, 1);

  // Backspace, SDL_TEXTINPUT e Space durante FV_DIGITANDO NAO fecham nem viram Voltar!
  {
    SDL_Event ti;
    memset(&ti, 0, sizeof ti);
    ti.type = SDL_TEXTINPUT;
    snprintf(ti.text.text, sizeof ti.text.text, "a");
    resolver_evento(&ti);
    tecla(SDLK_BACKSPACE);
    tecla(SDLK_SPACE);
    // repeat=1 no Voltar tambem e ignorado em FV_DIGITANDO!
    tecla_rep(SDLK_ESCAPE, 1);
    tecla_rep(SDLK_AC_BACK, 1);
    CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_DIGITANDO);
  }

  // Encaminhamento real de st_evento(e) via texto_sistema_evento() + ST_TEXTO + ST_FIM -> FV_REVISAO (0 POSTs).
  reqAntes = ler_req_count(countFile);
  st_teste_evento("Tparcial digitado");
  desenhar();
  CONFERE(!strcmp(farolvozui_parcial(), "parcial digitado"));
  CONFERE(escreveu("parcial digitado"));

  {
    // Tambem valida st_evento(e) recebendo texto_sistema_evento() diretamente.
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = texto_sistema_evento();
    ev.user.code = TS_EV_TEXTO;
    resolver_evento(&ev);
    CONFERE(farolvozui_estado() == FV_DIGITANDO);
  }

  // ST_FIM com espacos/quebras e UTF-8 entra em FV_REVISAO com 0 POSTs e fecha IME.
  st_teste_evento("D  ação\n\t suspense   curto  ");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(st_estado() == ST_PARADO);
  CONFERE(!strcmp(farolvozui_frase(), "ação suspense curto"));
  CONFERE(ler_req_count(countFile) == reqAntes); // 0 POSTs!
  CONFERE(escreveu(FT_VOCE_DISSE));
  CONFERE(escreveu("ação suspense curto"));
  CONFERE(escreveu(FT_ACAO_TECLADO_TV));
  varredura_fase(3, RESOLVER_FASE_SINTOMA, 1); // Buscar, Teclado da TV, Voltar

  // Voltar na previa descarta com 0 POSTs!
  tecla_rep(SDLK_ESCAPE, 1); // repeat=1 ignorado!
  CONFERE(farolvozui_estado() == FV_REVISAO);
  tecla_rep(SDLK_ESCAPE, 0);
  CONFERE(!farolvozui_ativo());
  CONFERE(ler_req_count(countFile) == reqAntes);

  // Corte UTF-8 <= 200 bytes no ST_FIM (nunca quebra sequencia multibyte!).
  {
    char longa[400];
    size_t pos = 0;
    longa[pos++] = 'D';
    while (pos + 2 < 260) {
      longa[pos++] = (char)0xC3; // 'é' (2 bytes UTF-8)
      longa[pos++] = (char)0xA9;
    }
    longa[pos] = '\0';
    tecla(SDLK_RETURN);
    CONFERE(farolvozui_estado() == FV_DIGITANDO);
    st_teste_evento(longa);
    desenhar();
    CONFERE(farolvozui_estado() == FV_REVISAO);
    CONFERE(strlen(farolvozui_frase()) <= 200);
    CONFERE((strlen(farolvozui_frase()) % 2) == 0); // nao cortou 'é' no meio
    tecla(SDLK_ESCAPE);
    CONFERE(!farolvozui_ativo());
    CONFERE(ler_req_count(countFile) == reqAntes);
  }

  // Fluxo completo Samsung/WASM -> OK (ignora repeat=1) -> busca -> Tela 7 (consulta A).
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_DIGITANDO);
  st_teste_evento("Dfilme curto samsung");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(resolver_foco() == FV_FOCO_CONFIRMAR);
  tecla_rep(SDLK_RETURN, 1); // repeat=1 NAO dispara busca!
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(ler_req_count(countFile) == reqAntes);
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  esperar_voz_ir_para_achados(3000);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  desenhar();
  CONFERE(escreveu("filme curto samsung"));
  CONFERE(escreveu(FT_ACAO_TECLADO_TV));
  varredura_fase(5, RESOLVER_FASE_ACHAR, 0);

  // ST_CANCELOU na Tela 7 fecha IME e preserva consulta A intacta!
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_DIGITANDO);
  st_teste_evento("X");
  desenhar();
  CONFERE(!farolvozui_ativo() && st_estado() == ST_PARADO);
  CONFERE(resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  desenhar();
  CONFERE(escreveu("filme curto samsung"));

  // Falha ao abrir IME (st_teste_ligar(0) com override ime=1): FV_AVISO sem beco.
  st_teste_ligar(0);
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_AVISO);
  varredura_fase(2, RESOLVER_FASE_ACHAR, 1); // Teclado da TV, Voltar
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);

  // ========================================================= 3. LG webOS / QR celular (voz=0, ime=0, cel=1)
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(0);
  farolvozui_teste_modos(0, 0, 1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);

  CONFERE(farolvozui_disponivel() == 1);
  CONFERE(!strcmp(farolvozui_rotulo_botao(), FT_ACAO_DIGITAR_CELULAR));
  resolver_abrir();
  desenhar();
  CONFERE(escreveu(FT_ACAO_DIGITAR_CELULAR));
  varredura_fase(14, RESOLVER_FASE_INICIO, 0);

  // Abrir QR real via celular_abrir_digitar().
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_ativo() && farolvozui_estado() == FV_QR);
  CONFERE(farolvozui_modo_atual() == FV_MODO_QR);
  CONFERE(celular_estado() == CEL_ESPERANDO);
  CONFERE(celular_porta() > 0);
  CONFERE(celular_url()[0] != '\0');
  {
    char qrUrl[128], resp[8192];
    int porta = celular_porta();
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    desenhar();
    CONFERE(escreveu(FT_DIGITE_CELULAR));
    CONFERE(escreveu(qrUrl));
    CONFERE(escreveu(FT_BUSCA_FRASE_EXEMPLO));
    varredura_fase(1, RESOLVER_FASE_SINTOMA, 1);

    // repeat=1 no Voltar em FV_QR e ignorado!
    tecla_rep(SDLK_ESCAPE, 1);
    CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);

    // GET real na URL do QR deve retornar 200 com formulario HTML.
    reqAntes = ler_req_count(countFile);
    CONFERE(fazer_get_celular(qrUrl, porta, resp, sizeof resp) == 200);
    CONFERE(strstr(resp, "<form") != NULL);

    // POST real com frase UTF-8 + espacos/quebras.
    CONFERE(enviar_post_celular(qrUrl, porta, "  ação\n\t suspense   rápido  ") == 200);

    // farolvozui_atualizar() pega o texto, fecha celular (CEL_PARADO!), e vai para FV_REVISAO com 0 POSTs de busca!
    desenhar();
    CONFERE(farolvozui_estado() == FV_REVISAO);
    CONFERE(celular_estado() == CEL_PARADO);
    CONFERE(!strcmp(farolvozui_frase(), "ação suspense rápido"));
    CONFERE(ler_req_count(countFile) == reqAntes); // 0 POSTs de busca!
    CONFERE(escreveu(FT_ACAO_DIGITAR_CELULAR));
    varredura_fase(3, RESOLVER_FASE_SINTOMA, 1); // Buscar, Digitar pelo celular, Voltar

    // Voltar na previa descarta com 0 POSTs de busca.
    tecla(SDLK_ESCAPE);
    CONFERE(!farolvozui_ativo());
    CONFERE(ler_req_count(countFile) == reqAntes);
  }

  // Novo ciclo QR -> GET + POST -> FV_REVISAO -> OK -> busca -> Tela 7 (consulta A).
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CONFERE(fazer_get_celular(qrUrl, porta, resp, sizeof resp) == 200);
    CONFERE(enviar_post_celular(qrUrl, porta, "suspense leve celular") == 200);
  }
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO && celular_estado() == CEL_PARADO);
  CONFERE(!strcmp(farolvozui_frase(), "suspense leve celular"));
  tecla_rep(SDLK_RETURN, 1); // repeat=1 ignorado
  CONFERE(farolvozui_estado() == FV_REVISAO);
  tecla_rep(SDLK_RETURN, 0);
  CONFERE(farolvozui_estado() == FV_BUSCANDO);
  esperar_voz_ir_para_achados(3000);
  CONFERE(!farolvozui_ativo() && resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  desenhar();
  CONFERE(escreveu("suspense leve celular"));
  CONFERE(escreveu(FT_ACAO_DIGITAR_CELULAR));
  varredura_fase(5, RESOLVER_FASE_ACHAR, 0);

  // Abrir QR na Tela 7 e apertar Voltar fecha socket e preserva consulta A!
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo());
  CONFERE(celular_estado() == CEL_PARADO);
  CONFERE(resolver_fase() == RESOLVER_FASE_ACHAR);
  CONFERE(farolachaui_n() == 3);
  desenhar();
  CONFERE(escreveu("suspense leve celular"));

  // OK no 1o cartaz de A ainda abre tt0000101!
  ir_foco(RESOLVER_B_ACHAR_CURTO);
  tecla_app(SDLK_RETURN);
  CONFERE(!resolver_aberto());
  CONFERE(A_detailAberto == 1 && !strcmp(A_abriuImdb, "tt0000101"));

  // Fechar Resolver durante FV_QR fecha socket obrigatoriamente!
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  resolver_fechar();
  CONFERE(!farolvozui_ativo() && celular_estado() == CEL_PARADO);

  // Falha/bloqueio real (CEL_FALHOU via CEL_ERROS_MAX caminhos errados) -> FV_AVISO sem beco.
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  {
    char host[128], path[128], req[512], resp[512];
    int porta = celular_porta(), i;
    extrair_host_path(celular_url(), host, sizeof host, path, sizeof path);
    snprintf(req, sizeof req,
             "GET /caminhoerrado HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Connection: close\r\n\r\n",
             host);
    for (i = 0; i < CEL_ERROS_MAX && celular_estado() == CEL_ESPERANDO; i++) {
      int st = http_loopback(porta, req, resp, sizeof resp);
      if (st < 0) break;
    }
    for (i = 0; i < 40 && celular_estado() == CEL_ESPERANDO; i++) usleep(25000);
    CONFERE(celular_estado() == CEL_FALHOU);
    desenhar();
    CONFERE(farolvozui_estado() == FV_AVISO);
    CONFERE(celular_estado() == CEL_PARADO);
    varredura_fase(2, RESOLVER_FASE_SINTOMA, 1); // Digitar pelo celular, Voltar
    // Tentar de novo reabre QR limpo!
    ir_foco(FV_FOCO_FALAR);
    tecla(SDLK_RETURN);
    CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
    tecla(SDLK_ESCAPE);
    CONFERE(!farolvozui_ativo() && celular_estado() == CEL_PARADO);
  }

  // Expiracao real (CEL_EXPIROU com CEL_VALIDADE_S=2 no build de teste) -> FV_AVISO sem beco.
  tecla(SDLK_RETURN); // foco ainda em RESOLVER_B_ACHAR_VOZ
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  {
    long ini = agoraMs();
    while (farolvozui_estado() == FV_QR && agoraMs() - ini < 5000) {
      resolver_atualizar(0.016f, 0);
      usleep(50000);
    }
    CONFERE(farolvozui_estado() == FV_AVISO);
    CONFERE(celular_estado() == CEL_PARADO);
    varredura_fase(2, RESOLVER_FASE_SINTOMA, 1);
    tecla(SDLK_ESCAPE);
    CONFERE(!farolvozui_ativo());
  }

  // ========================================================= 4. Modo combinado (voz=0, ime=1, cel=1)
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  farolvozui_teste_modos(0, 1, 1);
  snprintf(rota, sizeof rota, "%s/misto", base);
  farolbusca_definir_url(rota);

  CONFERE(farolvozui_disponivel() == 1);
  CONFERE(!strcmp(farolvozui_rotulo_botao(), FT_ACAO_TECLADO_TV));
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_DIGITANDO);
  // No modo combinado, FV_DIGITANDO tem 2 focos: Cancelar e Digitar pelo celular.
  varredura_fase(2, RESOLVER_FASE_SINTOMA, 1);
  ir_foco(FV_FOCO_ALT);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR);
  CONFERE(st_estado() == ST_PARADO);
  CONFERE(celular_estado() == CEL_ESPERANDO);
  // No modo combinado, FV_QR tem 2 focos: Cancelar e Teclado da TV.
  varredura_fase(2, RESOLVER_FASE_SINTOMA, 1);
  ir_foco(FV_FOCO_ALT);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_DIGITANDO);
  CONFERE(celular_estado() == CEL_PARADO);
  CONFERE(st_estado() == ST_DIGITANDO);

  // Conclui pelo IME -> FV_REVISAO tem 4 focos (Buscar, Teclado da TV, Digitar pelo celular, Voltar).
  st_teste_evento("Dfrase combinada");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  CONFERE(escreveu(FT_ACAO_TECLADO_TV));
  CONFERE(escreveu(FT_ACAO_DIGITAR_CELULAR));
  varredura_fase(4, RESOLVER_FASE_SINTOMA, 1);
  // Alternar na revisao para Digitar pelo celular abre FV_QR limpo.
  ir_foco(FV_FOCO_ALT);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_QR && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo() && celular_estado() == CEL_PARADO);

  // ========================================================= 5. Android (voz=1, ime=1, cel=1): mantem 100% #31
  zera_tudo();
  seed_catalogo();
  st_teste_ligar(1);
  farolvozui_teste_modos(1, 1, 1);
  CONFERE(!strcmp(farolvozui_rotulo_botao(), FT_ACAO_FALAR));
  resolver_abrir();
  ir_foco(RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  CONFERE(farolvozui_estado() == FV_OUVINDO);
  tecla_rep(SDLK_ESCAPE, 1); // repeat=1 ignorado em FV_OUVINDO!
  CONFERE(farolvozui_estado() == FV_OUVINDO);
  st_teste_evento("Vteste android 31");
  desenhar();
  CONFERE(farolvozui_estado() == FV_REVISAO);
  varredura_fase(3, RESOLVER_FASE_SINTOMA, 1); // exatamente 3 focos como #31!
  tecla(SDLK_ESCAPE);
  CONFERE(!farolvozui_ativo());

  farolvozui_teste_modos(-1, -1, -1);
  resolver_fechar();
  farolbusca_encerrar();

  if (falhas) {
    fprintf(stderr, "farol_frase_tv: %d falha(s)\n", falhas);
    return 1;
  }
  puts("farol_frase_tv: Samsung/WASM, LG QR, combinado, desligado e Android ok");
  return 0;
}

#else // NV_FRASE_TV_SHOT

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
#include "celular.h"
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

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/farol-frase-tv-shot";
  const char *dir = getenv("NUVIO_DADOS");
  char nQr[700], nRev[700], nIme[700];
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
  w = SDL_CreateWindow("farol-frase-tv-shot", 0, 0, 64, 64,
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

  memset(&rf, 0, sizeof rf);
  rf.addonsQtd = 3;
  resolver_teste_fatos(&rf);
  farolregra_fatos_iniciar(&ff);
  ff.plat = FP_WEBOS;
  ff.addonsQtd = 3;
  resolver_teste_fatos_farol(&ff);

  // 1. LG QR celular real + textura GL real via qrtex.h
  farolvozui_teste_modos(0, 0, 1);
  resolver_abrir();
  for (i = 0; i < 6 && resolver_foco() != RESOLVER_B_ACHAR_VOZ; i++)
    tecla(SDLK_RIGHT);
  assert(resolver_foco() == RESOLVER_B_ACHAR_VOZ);
  tecla(SDLK_RETURN);
  assert(farolvozui_estado() == FV_QR);
  assert(celular_estado() == CEL_ESPERANDO);
  snprintf(nQr, sizeof nQr, "%s-qr.png", saida);
  lisas += !captura(nQr, 20);

  // Fecha QR (destroi textura GL e fecha socket) e abre previa UTF-8.
  tecla(SDLK_ESCAPE);
  assert(!farolvozui_ativo() && celular_estado() == CEL_PARADO);
  assert(farolvozui_abrir_texto("ação e suspense rápido para hoje à noite",
                                RESOLVER_FASE_SINTOMA) == 1);
  assert(farolvozui_estado() == FV_REVISAO);
  snprintf(nRev, sizeof nRev, "%s-revisao-utf8.png", saida);
  lisas += !captura(nRev, 20);

  // 2. Samsung/WASM (IME)
  tecla(SDLK_ESCAPE);
  st_teste_ligar(1);
  farolvozui_teste_modos(0, 1, 0);
  assert(farolvozui_abrir(RESOLVER_FASE_SINTOMA) == 1);
  assert(farolvozui_estado() == FV_DIGITANDO);
  snprintf(nIme, sizeof nIme, "%s-ime.png", saida);
  lisas += !captura(nIme, 20);

  resolver_fechar();
  farolvozui_teste_modos(-1, -1, -1);
  farolbusca_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  IMG_Quit();
  SDL_Quit();
  if (lisas) {
    fprintf(stderr, "FALHA: farol_frase_tv shot gerou captura lisa\n");
    return 1;
  }
  puts("farol_frase_tv: capturas 1920x1080 (QR + previa UTF-8 + IME) ok");
  return 0;
}

#endif
