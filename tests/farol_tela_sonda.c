// Prova dirigida real ligando src/resolver.c + src/farol.c + src/farolsonda.c
// com transporte simulado (rede_pedir):
//   - OK na tela de Inicio dispara farolsonda_iniciar() (pedidos == 1, texto_testando == 1)
//   - selo e 2 linhas do veredito passivo ja aparecem durante Sondando
//   - Voltar chama farolsonda_cancelar()
//   - fechar+reabrir enquanto o fio cancelado termina nao herda "testando…"
#define FAROL_RESOLVER_SEM_MAIN
#define FAROL_SONDA_REAL
#include "farol_resolver.c"
#include "rede.h"
#include <pthread.h>
#include <unistd.h>

static int falhas;
#define CONFERE(c) do { if (!(c)) { fprintf(stderr, "FALHA %s:%d: %s\n", __FILE__, __LINE__, #c); falhas++; } } while (0)

static pthread_mutex_t t_trava = PTHREAD_MUTEX_INITIALIZER;
static int t_pedidos, t_emVoo, t_liberar, t_pararVisto, t_status = 206, t_temRange, t_temRef;
static char t_ultimaUrl[256];

int player_mini_ativo(void) { return mini; }

int rede_pedir(const RedePedido *req, RedeResposta *res) {
  memset(res, 0, sizeof *res);
  if (!req || !req->sonda || !req->url) return 0;
  pthread_mutex_lock(&t_trava);
  t_pedidos++;
  t_emVoo++;
  snprintf(t_ultimaUrl, sizeof t_ultimaUrl, "%s", req->url);
  for (int i = 0; req->cabecalhos && req->cabecalhos[i]; i++) {
    if (!strcmp(req->cabecalhos[i], "Range: bytes=0-1")) t_temRange = 1;
    if (!strcmp(req->cabecalhos[i], "Referer: https://addon.example")) t_temRef = 1;
  }
  pthread_mutex_unlock(&t_trava);

  for (;;) {
    int parar = req->parar ? req->parar(req->parar_usuario) : 0;
    pthread_mutex_lock(&t_trava);
    if (parar) t_pararVisto = 1;
    int sair = t_liberar;
    int st = t_status;
    pthread_mutex_unlock(&t_trava);
    if (sair) {
      res->status = st;
      res->primeiro_byte_ms = 25;
      break;
    }
    usleep(2000);
  }

  pthread_mutex_lock(&t_trava);
  t_emVoo--;
  pthread_mutex_unlock(&t_trava);
  return res->status > 0;
}

void rede_resposta_limpar(RedeResposta *res) {
  if (res) { free(res->corpo); res->corpo = NULL; }
}

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  resolver_evento(&e);
}

static void quadros(int n, float dt) {
  for (int i = 0; i < n; i++) resolver_atualizar(dt, 0);
}

static void desenhar(void) {
  c_txtN = 0;
  resolver_atualizar(16.0f, 0);
  resolver_desenhar(0);
}

static int escreveu(const char *s) {
  for (int i = 0; i < c_txtN; i++) if (strstr(c_txt[i], s)) return 1;
  return 0;
}

static int esperar_pedidos(int alvo) {
  for (int i = 0; i < 400; i++) {
    pthread_mutex_lock(&t_trava);
    int p = t_pedidos;
    pthread_mutex_unlock(&t_trava);
    if (p >= alvo) return 1;
    usleep(2000);
  }
  return 0;
}

static int esperar_parar_visto(void) {
  for (int i = 0; i < 400; i++) {
    pthread_mutex_lock(&t_trava);
    int v = t_pararVisto;
    pthread_mutex_unlock(&t_trava);
    if (v) return 1;
    usleep(2000);
  }
  return 0;
}

static int esperar_sonda_pronta(FarolSonda *out) {
  for (int i = 0; i < 400; i++) {
    if (farolsonda_estado(out)) return 1;
    usleep(2000);
  }
  return 0;
}

int main(void) {
  FarolSonda s;

  // 1. OK no Inicio dispara farolsonda_iniciar(), mostra "testando…" E o
  //    selo + 2 linhas do veredito passivo durante Sondando, e avanca ao
  //    Veredito quando a sonda termina.
  zerar();
  c_reduzidas = 0;
  c_streamAtual = 0;
  snprintf(c_stream.url, sizeof c_stream.url, "https://cdn.example/sonda.mkv");
  snprintf(c_stream.cabecalhos, sizeof c_stream.cabecalhos, "Referer: https://addon.example\n");
  ultimoFarolDebrid = 1;
  falhar_fonte("O servidor recusou o link (HTTP 403).", "", FC_HTTP, 403, "http");
  resolver_abrir();
  quadros(5, 16.0f);
  tecla(SDLK_RETURN);

  CONFERE(esperar_pedidos(1));
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA);
  CONFERE(resolver_testando());
  desenhar();
  {
    int texto_testando = escreveu(FT_TESTANDO);
    CONFERE(texto_testando == 1);
    CONFERE(escreveu(FT_SONDANDO_FONTE));
    CONFERE(escreveu(FT_A_CAUSA_ENCONTRADA));
    CONFERE(escreveu(FT_DEBRID_DESCONECTOU_L1));
    CONFERE(escreveu(FT_DEBRID_DESCONECTOU_L2));
  }
  pthread_mutex_lock(&t_trava);
  CONFERE(t_pedidos == 1);
  CONFERE(t_temRange == 1 && t_temRef == 1);
  CONFERE(!strcmp(t_ultimaUrl, "https://cdn.example/sonda.mkv"));
  t_liberar = 1;
  pthread_mutex_unlock(&t_trava);

  CONFERE(esperar_sonda_pronta(&s));
  CONFERE(!s.cancelada && s.http == 206);
  quadros(80, 16.0f);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);

  // 2. Voltar cancela farolsonda_iniciar; fechar e reabrir enquanto o fio
  //    cancelado ainda esta terminando NAO herda "testando…".
  pthread_mutex_lock(&t_trava);
  t_liberar = 0;
  t_pararVisto = 0;
  pthread_mutex_unlock(&t_trava);

  tecla(SDLK_ESCAPE);
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO);
  tecla(SDLK_RETURN);
  CONFERE(esperar_pedidos(2));
  CONFERE(resolver_fase() == RESOLVER_FASE_CADEIA && resolver_testando());

  tecla(SDLK_ESCAPE);                                          // Voltar cancela
  CONFERE(esperar_parar_visto());
  CONFERE(resolver_fase() == RESOLVER_FASE_INICIO);
  tecla(SDLK_ESCAPE);                                          // fecha a tela
  CONFERE(!resolver_aberto());

  // Reabre enquanto o fio cancelado ainda nao retornou de rede_pedir().
  resolver_abrir();
  CONFERE(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  tecla(SDLK_RETURN);
  CONFERE(!resolver_testando());
  desenhar();
  CONFERE(!escreveu(FT_TESTANDO));
  quadros(80, 16.0f);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);

  // Libera o fio cancelado e confirma que uma nova abertura volta a sondar.
  pthread_mutex_lock(&t_trava);
  t_liberar = 1;
  pthread_mutex_unlock(&t_trava);
  CONFERE(esperar_sonda_pronta(&s));
  CONFERE(s.cancelada == 1);
  quadros(2, 16.0f);

  tecla(SDLK_ESCAPE);
  pthread_mutex_lock(&t_trava);
  t_liberar = 0;
  t_pararVisto = 0;
  pthread_mutex_unlock(&t_trava);
  tecla(SDLK_RETURN);
  CONFERE(esperar_pedidos(3));
  CONFERE(resolver_testando());
  pthread_mutex_lock(&t_trava);
  t_liberar = 1;
  pthread_mutex_unlock(&t_trava);
  CONFERE(esperar_sonda_pronta(&s));
  quadros(80, 16.0f);
  CONFERE(resolver_fase() == RESOLVER_FASE_VEREDITO);

  if (falhas) { fprintf(stderr, "farol_tela_sonda: %d falha(s)\n", falhas); return 1; }
  puts("farol_tela_sonda: sonda real na cadeia ok");
  return 0;
}
