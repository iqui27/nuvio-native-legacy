// tests/farol_escolha_tv.c — teste integrado da issue Farol #55
// ("Assistente por frase nas demais TVs" — Samsung .tpk/.wgt, LG QR celular,
// WASM no Assistente P5 + /v1/farol/escolha).
//
// Cobre:
//   - -DNV_TPK (release .tpk com nv_tpk_voz_registrar): FT_ACAO_TECLADO_TV,
//     st_voz_iniciar, FESCUI_DIGITANDO, st_aviso(), FESCUI_PREVIA (0 POSTs),
//     OK -> /v1/farol/escolha pre-responde frase canonica -> P4, ST_CANCELOU /
//     Voltar chama st_fechar(ST_BUSCA), alternancia FESCUI_FOCO_ALT <-> QR,
//     TS_MIC_DESLIGADO, UTF-8 4 bytes (200 codepoints).
//   - Samsung / WASM IME (voz=0, ime=1, cel=0): st_ime_abrir, st_evento(e)
//     protegendo SDLK_BACKSPACE/SDL_TEXTINPUT/SDLK_SPACE, ST_FIM ->
//     FESCUI_PREVIA (0 POSTs) -> OK -> /v1/farol/escolha, falha IME sem beco.
//   - LG webOS QR celular (voz=0, ime=0, cel=1): celular_abrir_digitar() real
//     + GET/POST HTTP loopback UTF-8, celular_estado() == CEL_PARADO,
//     FESCUI_PREVIA (0 POSTs), Voltar descarta, OK -> /v1/farol/escolha,
//     Voltar/fechar/reset fecha socket sem leak GL, CEL_FALHOU / CEL_EXPIROU
//     sem beco, URL rejeitada.
//   - Modo combinado (0, 1, 1), desligado (0, 0, 0), Android (1, 1, 1).
//   - Privacidade/consentimento multi-TV via IME e QR (infantil, sem
//     consentimento, SO_TV, recomendar=0, privado -> 0 POSTs e fallback limpo).
//   - UTF-8 2/3/4 bytes (200 codepoints) + key.repeat=1 ignorado em OK/Voltar.
//   - Capturas PNG 1920x1080 com contexto GLES2 real e checagem de zero leak GL.
#include "ajustes.h"
#include "catalogo.h"
#include "celular.h"
#include "dados.h"
#include "entrada_texto.h"
#include "farolacha.h"
#include "farolcompara.h"
#include "farolconsent.h"
#include "farolescolha.h"
#include "farolescolha_rede.h"
#include "farolescolhaui.h"
#include "farolsinal.h"
#include "faroltexto.h"
#include "gfx.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "qr.h"
#include "resolver.h"
#include "seguro.h"
#include "sistexto.h"
#include "tex_cache.h"
#include "text.h"

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <GLES2/gl2.h>
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

static int gFalhas;
#define CHECK(c) do { if (!(c)) { \
  fprintf(stderr, "%s:%d: FALHA: %s\n", __FILE__, __LINE__, #c); \
  gFalhas++; exit(1); \
} } while (0)

static int gPlayer;
static int playerTeste(void) { return gPlayer; }

static long long monoMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (long long)t.tv_sec * 1000LL + (long long)(t.tv_nsec / 1000000L);
}

static int lerContador(const char *path) {
  FILE *f = fopen(path, "rb");
  int v = 0;
  if (!f) return 0;
  if (fscanf(f, "%d", &v) != 1) v = 0;
  fclose(f);
  return v;
}

static void teclaR(SDL_Keycode k, int repeat) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  e.key.repeat = (Uint8)repeat;
  resolver_evento(&e);
  resolver_atualizar(0.016f, 1000);
}
static void tecla(SDL_Keycode k) { teclaR(k, 0); }

static void esperarUiEspera(int maxMs) {
  long long lim = monoMs() + (long long)maxMs;
  while (monoMs() < lim) {
    resolver_atualizar(0.016f, 1500);
    if (farolescolhaui_modo() != FESCUI_ESPERA) return;
    usleep(5000);
  }
}

static CatItem CAT[48];

static void montarCatCanonico(void) {
  int i;
  static const char *const G_OUTROS[] = {
    "Filme · Suspense · Crime",
    "Filme · Terror",
    "Série · Drama · Mistério",
    "Filme · Ação · Guerra",
    "Série · Comédia",
    "Filme · Documentário",
  };
  for (i = 0; i < 48; i++) {
    CatItem *c = &CAT[i];
    memset(c, 0, sizeof *c);
    snprintf(c->imdb, sizeof c->imdb, "tt55%05d", i + 1);
    snprintf(c->titulo, sizeof c->titulo, "Obra %02d", i + 1);
    snprintf(c->classificacao, sizeof c->classificacao, "%s", i < 6 ? "L" : "14");
    if (i < 4) {
      snprintf(c->tipo, sizeof c->tipo, "movie");
      snprintf(c->genero, sizeof c->genero, "%s",
               (i % 2 == 0) ? "Filme · Animação · Família · Comédia"
                            : "Filme · Família · Aventura · Drama");
      snprintf(c->meta, sizeof c->meta, "%d · %d min", (i < 2) ? 2022 : 1995, 65 + i * 5);
      c->nota = 90 - i * 2;
    } else {
      int serie = (i % 3 == 0);
      snprintf(c->tipo, sizeof c->tipo, "%s", serie ? "series" : "movie");
      snprintf(c->genero, sizeof c->genero, "%s", G_OUTROS[i % 6]);
      snprintf(c->meta, sizeof c->meta, "%d · %d min", (i % 2) ? 2021 : 1994,
               serie ? 42 : 125 + (i % 4) * 10);
      c->nota = 60 + (i % 25);
    }
  }
  cat_definir(CAT, 48);
}

static void irParaEscolha(void) {
  int i;
  for (i = 0; i < 40 && resolver_foco() != RESOLVER_B_ACHAR_ESCOLHA; i++) {
    int f = resolver_foco();
    if ((f >= RESOLVER_B_ACHAR_CURTO && f <= RESOLVER_B_ACHAR_SURPRESA) ||
        f == RESOLVER_B_ACHAR_VOZ)
      tecla(SDLK_DOWN);
    else tecla(SDLK_RIGHT);
  }
  CHECK(resolver_foco() == RESOLVER_B_ACHAR_ESCOLHA);
}

static void abrirP5(void) {
  resolver_abrir();
  resolver_atualizar(0.016f, 1000);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_INICIO);
  irParaEscolha();
  tecla(SDLK_RETURN);
  CHECK(resolver_aberto() && resolver_fase() == RESOLVER_FASE_ESCOLHA);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
}

static void focarEntrada(void) {
  CHECK(farolescolhaui_voz_visivel() == 1);
  tecla(SDLK_DOWN);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOZ);
}

static void configurarAppAutorizado(const char *url) {
  farolescolha_rede_teste_zera();
  farolescolha_rede_definir_url(url);
  farolescolha_rede_definir_auth("Authorization: Bearer tok_segredo_tv55",
                                 "X-Nuvio-Auth: nuvio", NULL);
  farolescolhaui_teste_hora(20);
  farolescolhaui_teste_infantil(0);
  CHECK(perfil_infantil_definir(perfis_ativo(), 0));
  farolsinal_definir_recomendar(1);
  seguro_iniciar(0, 1000, NULL);
  CHECK(seguro_perfil_ativo() == 0);
  CHECK(farolconsent_definir(FAROL_CONSENT_SIM));
  CHECK(farolescolha_rede_autorizado() == 1);
}

// Cliente HTTP minimo sobre loopback para falar com celular.c de verdade.
static int httpLoopback(int porta, const char *req, char *resp, size_t cap) {
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

static void urlEncode(const char *src, char *dst, size_t cap) {
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

static void extrairHostPath(const char *url, char *host, size_t nh,
                            char *path, size_t np) {
  const char *p = !strncmp(url, "http://", 7) ? url + 7 : url;
  const char *slash = strchr(p, '/');
  size_t hl = slash ? (size_t)(slash - p) : strlen(p);
  if (hl >= nh) hl = nh - 1;
  memcpy(host, p, hl);
  host[hl] = '\0';
  snprintf(path, np, "%s", slash ? slash : "/");
}

static int enviarPostCelular(const char *url, int porta, const char *frase) {
  char host[128], path[128], enc[3200], body[3300], req[4096], resp[4096];
  extrairHostPath(url, host, sizeof host, path, sizeof path);
  urlEncode(frase, enc, sizeof enc);
  snprintf(body, sizeof body, "t=%s", enc);
  snprintf(req, sizeof req,
           "POST %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Origin: http://%s\r\n"
           "Content-Type: application/x-www-form-urlencoded\r\n"
           "Content-Length: %zu\r\n"
           "Connection: close\r\n\r\n%s",
           path, host, host, strlen(body), body);
  return httpLoopback(porta, req, resp, sizeof resp);
}

static int fazerGetCelular(const char *url, int porta, char *resp, size_t cap) {
  char host[128], path[128], req[1024];
  extrairHostPath(url, host, sizeof host, path, sizeof path);
  snprintf(req, sizeof req,
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Connection: close\r\n\r\n",
           path, host);
  return httpLoopback(porta, req, resp, cap);
}

static int contarUtf8Valido(const char *s) {
  const unsigned char *p = (const unsigned char *)s;
  int cps = 0;
  if (!s) return -1;
  while (*p) {
    unsigned char c = *p;
    long cp;
    int extra, i;
    if (c < 0x80) { p++; cps++; continue; }
    if (c >= 0xC2 && c <= 0xDF)      { cp = c & 0x1F; extra = 1; }
    else if (c >= 0xE0 && c <= 0xEF) { cp = c & 0x0F; extra = 2; }
    else if (c >= 0xF0 && c <= 0xF4) { cp = c & 0x07; extra = 3; }
    else return -1;
    for (i = 1; i <= extra; i++) {
      if ((p[i] & 0xC0) != 0x80) return -1;
      cp = (cp << 6) | (p[i] & 0x3F);
    }
    if ((extra == 2 && cp < 0x800) || (extra == 3 && cp < 0x10000)) return -1;
    if ((cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) return -1;
    p += extra + 1;
    cps++;
  }
  return cps;
}

static void montarRepUtf8(char *dst, size_t cap, char prefixo,
                          const char *seq, int vezes) {
  size_t pos = 0, sl = strlen(seq);
  int i;
  if (prefixo && pos + 1 < cap) dst[pos++] = prefixo;
  for (i = 0; i < vezes && pos + sl < cap; i++) {
    memcpy(dst + pos, seq, sl);
    pos += sl;
  }
  dst[pos] = '\0';
}

static void irFocoFluxo(int alvo) {
  int i;
  if (farolescolhaui_foco() == alvo) return;
  for (i = 0; i < 6 && farolescolhaui_foco() != alvo; i++) tecla(SDLK_RIGHT);
  for (i = 0; i < 6 && farolescolhaui_foco() != alvo; i++) tecla(SDLK_LEFT);
  CHECK(farolescolhaui_foco() == alvo);
}

static void limparTudo(void) {
  farolescolhaui_teste_modos(-1, -1, -1);
  farolescolha_rede_encerrar();
  farolcompara_fechar();
  farolcompara_teste_reset();
  resolver_fechar();
  resolver_atualizar(1.0f, 9000);
  farolescolhaui_reset();
  celular_fechar();
  st_fechar(ST_BUSCA);
}

#ifdef NV_TPK
#include "tpk_egl.h"
TpkEgl tpkEgl;
typedef void (*FnTpkVoz)(void);
void nv_tpk_voz_registrar(FnTpkVoz iniciar, FnTpkVoz parar, int microfone);
void nv_tpk_texto_valor(const char *v);
void nv_tpk_texto_fim(int confirmou);

static int T_iniciou;
static int T_parou;
static void hTpkIniciar(void) { T_iniciou++; }
static void hTpkParar(void) { T_parou++; }

static void bombearTpk(void) {
  SDL_Event ev;
  texto_sistema_quadro();
  while (SDL_PollEvent(&ev)) resolver_evento(&ev);
  resolver_atualizar(0.016f, 1200);
}

static int rodarTpkRelease(const char *base, const char *countFile) {
  static const char *HINT = "Aperte o microfone do controle e fale.";
  char url[512];
  int antes;

  limparTudo();
  montarCatCanonico();
  st_teste_ligar(0);
  farolescolhaui_teste_modos(-1, -1, -1);
  T_iniciou = 0;
  T_parou = 0;

  nv_tpk_voz_registrar(hTpkIniciar, hTpkParar, -1);
  texto_sistema_mic_modo(TS_MIC_AUTO);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);

  CHECK(st_ime_disponivel() == 0);
  CHECK(st_voz_disponivel() == 1);
  CHECK(texto_sistema_voz_pelo_teclado() == 1);
  CHECK(celular_disponivel() == 1);
  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_TECLADO_TV));
  CHECK(!strcmp(farolescolhaui_icone_botao(), "aj_keyboard"));

  // 1. Entrada por Teclado da TV -> st_voz_iniciar -> FESCUI_DIGITANDO + st_aviso() ->
  //    ST_FIM -> FESCUI_PREVIA (0 POSTs) -> OK -> /v1/farol/escolha pre-responde -> P4!
  abrirP5();
  focarEntrada();
  antes = lerContador(countFile);
  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && T_iniciou == 0);
  teclaR(SDLK_RETURN, 0);
  CHECK(T_iniciou == 1);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  CHECK(st_estado() == ST_DIGITANDO && st_dono() == ST_BUSCA);
  CHECK(!strcmp(st_aviso(), HINT));

  // Parcial enquanto teclado Samsung cobre a tela: 0 POSTs e mantem FESCUI_DIGITANDO.
  nv_tpk_texto_valor("parcial tpk");
  bombearTpk();
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  CHECK(!strcmp(st_aviso(), HINT));
  CHECK(lerContador(countFile) == antes);

  // ST_FIM -> FESCUI_PREVIA (0 POSTs!) -> repeat=1 ignorado -> OK -> /v1/farol/escolha -> P4.
  nv_tpk_texto_valor("  algo\n\t leve de uma hora para ver com meu filho  ");
  nv_tpk_texto_fim(1);
  bombearTpk();
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(st_estado() == ST_PARADO);
  CHECK(!st_aviso()[0]);
  CHECK(!strcmp(farolescolhaui_frase(), "algo leve de uma hora para ver com meu filho"));
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_CONFIRMAR);
  CHECK(lerContador(countFile) == antes);

  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA && lerContador(countFile) == antes);
  teclaR(SDLK_ESCAPE, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA && lerContador(countFile) == antes);

  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_ESPERA || farolescolhaui_modo() == FESCUI_COMPARAR);
  esperarUiEspera(3000);
  CHECK(lerContador(countFile) == antes + 1);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_TEMPO) == FESC_T100);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_PESO) == FESC_LEVE);
  CHECK(farolescolha_resposta(farolescolhaui_motor(), FESC_COMPANHIA) == FESC_CRIANCA);
  CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
  CHECK(!resolver_aberto() && farolcompara_aberto());
  farolcompara_fechar();
  farolcompara_teste_reset();
  resolver_atualizar(1.0f, 2000);

  // 2. ST_CANCELOU e Voltar em FESCUI_DIGITANDO chamam st_fechar(ST_BUSCA) e voltam intactos!
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(T_iniciou == 2 && farolescolhaui_modo() == FESCUI_DIGITANDO);
  nv_tpk_texto_fim(0);
  bombearTpk();
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(st_estado() == ST_PARADO && !st_aviso()[0]);
  CHECK(farolescolha_feitas(farolescolhaui_motor()) == 0);

  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(T_iniciou == 3 && farolescolhaui_modo() == FESCUI_DIGITANDO);
  tecla(SDLK_ESCAPE);
  bombearTpk();
  CHECK(T_parou == 1);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(st_estado() == ST_PARADO && !st_aviso()[0]);

  // 3. Alternancia FESCUI_FOCO_ALT: Teclado da TV <-> QR celular real -> /v1/farol/escolha!
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(T_iniciou == 4 && farolescolhaui_modo() == FESCUI_DIGITANDO);
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  bombearTpk();
  CHECK(T_parou == 2 && st_estado() == ST_PARADO);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);

  // Volta para Teclado da TV via FESCUI_FOCO_ALT -> fecha QR e reabre Samsung IME.
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  CHECK(celular_estado() == CEL_PARADO);
  CHECK(T_iniciou == 5 && farolescolhaui_modo() == FESCUI_DIGITANDO);
  CHECK(st_estado() == ST_DIGITANDO && !strcmp(st_aviso(), HINT));

  // Volta para QR via FESCUI_FOCO_ALT e envia frase por loopback HTTP -> PREVIA (0 POSTs) -> OK -> P4.
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  bombearTpk();
  CHECK(T_parou == 3 && farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    antes = lerContador(countFile);
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, "algo leve de uma hora para ver com meu filho") == 200);
    resolver_atualizar(0.016f, 1300);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA && celular_estado() == CEL_PARADO);
    CHECK(!strcmp(farolescolhaui_frase(), "algo leve de uma hora para ver com meu filho"));
    CHECK(lerContador(countFile) == antes);
    tecla(SDLK_RETURN);
    esperarUiEspera(3000);
    CHECK(lerContador(countFile) == antes + 1);
    CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
    CHECK(!resolver_aberto() && farolcompara_aberto());
    farolcompara_fechar();
    farolcompara_teste_reset();
    resolver_atualizar(1.0f, 2200);
  }

  // 4. Falha de abertura do host (nv_tpk_texto_fim(-1)) -> FESCUI_AVISO sem beco (com QR alt).
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(T_iniciou == 6 && farolescolhaui_modo() == FESCUI_DIGITANDO);
  nv_tpk_texto_fim(-1);
  bombearTpk();
  CHECK(farolescolhaui_modo() == FESCUI_AVISO);
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);

  // 5. Corte UTF-8 de 4 bytes (220 emojis -> 200 codepoints = 800 bytes) via ponte real TPK.
  {
    char emojis[1024];
    antes = lerContador(countFile);
    montarRepUtf8(emojis, sizeof emojis, '\0', "\xF0\x9F\x8E\xAC", 220);
    focarEntrada();
    tecla(SDLK_RETURN);
    CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
    nv_tpk_texto_valor(emojis);
    nv_tpk_texto_fim(1);
    bombearTpk();
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
    CHECK(contarUtf8Valido(farolescolhaui_frase()) == FAROL_ESCOLHA_FRASE_MAX);
    CHECK(strlen(farolescolhaui_frase()) == 800);
    CHECK(lerContador(countFile) == antes);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  }

  // 6. Microfone desligado no .tpk cai graciosamente para QR celular.
  texto_sistema_mic_modo(TS_MIC_DESLIGADO);
  CHECK(st_voz_disponivel() == 0 && st_ime_disponivel() == 0 && celular_disponivel() == 1);
  CHECK(farolescolhaui_voz_visivel() == 1);
  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_DIGITAR_CELULAR));
  CHECK(!strcmp(farolescolhaui_icone_botao(), "aj_smartphone"));
  texto_sistema_mic_modo(TS_MIC_AUTO);

  limparTudo();
  CHECK(!gFalhas);
  puts("ok  farol_escolha_tv (NV_TPK): teclado/mic Samsung + st_aviso + QR alt + /v1/farol/escolha + UTF-8");
  return 0;
}
#else

static void provaModosDesligados(const char *base) {
  char url[512];
  limparTudo();
  montarCatCanonico();
  st_teste_ligar(0);
  farolescolhaui_teste_modos(0, 0, 0);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  abrirP5();
  CHECK(farolescolhaui_voz_visivel() == 0);
  tecla(SDLK_DOWN);
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  limparTudo();
  puts("ok  modos desligados (0,0,0): botao de frase oculto");
}

static void provaSamsungWasmIme(const char *base, const char *countFile) {
  char url[512];
  int antes;
  SDL_Event ti, ev;

  limparTudo();
  montarCatCanonico();
  st_teste_ligar(1);
  farolescolhaui_teste_modos(0, 1, 0);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);

  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_TECLADO_TV));
  CHECK(!strcmp(farolescolhaui_icone_botao(), "aj_keyboard"));

  abrirP5();
  focarEntrada();
  antes = lerContador(countFile);

  // OK com repeat=1 ignorado; repeat=0 abre IME (FESCUI_DIGITANDO).
  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  CHECK(st_estado() == ST_DIGITANDO && st_dono() == ST_BUSCA);

  // SDL_TEXTINPUT, SDL_TEXTEDITING, Backspace, Space e repeat=1 em Voltar NAO fecham!
  memset(&ti, 0, sizeof ti);
  ti.type = SDL_TEXTINPUT;
  snprintf(ti.text.text, sizeof ti.text.text, "a");
  resolver_evento(&ti);
  memset(&ti, 0, sizeof ti);
  ti.type = SDL_TEXTEDITING;
  resolver_evento(&ti);
  tecla(SDLK_BACKSPACE);
  tecla(SDLK_SPACE);
  teclaR(SDLK_ESCAPE, 1);
  teclaR(SDLK_AC_BACK, 1);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);

  // st_evento(e) encaminha texto_sistema_evento() + ST_TEXTO + ST_FIM -> FESCUI_PREVIA (0 POSTs).
  st_teste_evento("Tparcial ime");
  resolver_atualizar(0.016f, 1100);
  CHECK(!strcmp(farolescolhaui_parcial(), "parcial ime"));

  memset(&ev, 0, sizeof ev);
  ev.type = texto_sistema_evento();
  ev.user.code = TS_EV_TEXTO;
  resolver_evento(&ev);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);

  st_teste_evento("D  algo\n\t leve de uma hora para ver com meu filho  ");
  resolver_atualizar(0.016f, 1200);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(st_estado() == ST_PARADO);
  CHECK(!strcmp(farolescolhaui_frase(), "algo leve de uma hora para ver com meu filho"));
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_CONFIRMAR);
  CHECK(lerContador(countFile) == antes);

  // repeat=1 ignorado em OK/Voltar na previa; Voltar descarta com 0 POSTs.
  teclaR(SDLK_RETURN, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA && lerContador(countFile) == antes);
  teclaR(SDLK_ESCAPE, 1);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA && lerContador(countFile) == antes);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(farolescolha_feitas(farolescolhaui_motor()) == 0);
  CHECK(lerContador(countFile) == antes);

  // Fluxo completo IME -> OK -> POST /v1/farol/escolha -> P4.
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  st_teste_evento("Dalgo leve de uma hora para ver com meu filho");
  resolver_atualizar(0.016f, 1300);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  tecla(SDLK_RETURN);
  esperarUiEspera(3000);
  CHECK(lerContador(countFile) == antes + 1);
  CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
  CHECK(!resolver_aberto() && farolcompara_aberto());
  farolcompara_fechar();
  farolcompara_teste_reset();
  resolver_atualizar(1.0f, 2000);

  // Falha ao abrir IME (st_teste_ligar(0) com override ime=1): FESCUI_AVISO sem beco.
  st_teste_ligar(0);
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_AVISO);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);

  limparTudo();
  puts("ok  Samsung/WASM IME (0,1,0): st_ime_abrir, st_evento, Space/Backspace, previa 0 POSTs, /v1/farol/escolha e aviso");
}

static void provaLgWebOsQr(const char *base, const char *countFile) {
  char url[512];
  int antes;

  limparTudo();
  montarCatCanonico();
  st_teste_ligar(0);
  farolescolhaui_teste_modos(0, 0, 1);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);

  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_DIGITAR_CELULAR));
  CHECK(!strcmp(farolescolhaui_icone_botao(), "aj_smartphone"));

  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR);
  CHECK(celular_estado() == CEL_ESPERANDO);
  CHECK(celular_porta() > 0 && celular_url()[0] != '\0');

  // repeat=1 no Voltar em FESCUI_QR e ignorado!
  teclaR(SDLK_ESCAPE, 1);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);

  // GET real + POST UTF-8 -> FESCUI_PREVIA (0 POSTs) + celular_estado() == CEL_PARADO -> Voltar descarta.
  {
    char qrUrl[128], resp[8192];
    int porta = celular_porta();
    antes = lerContador(countFile);
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(strstr(resp, "<form") != NULL);
    CHECK(enviarPostCelular(qrUrl, porta, "  algo\n\t leve de uma hora para ver com meu filho  ") == 200);
    resolver_atualizar(0.016f, 1100);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
    CHECK(celular_estado() == CEL_PARADO);
    CHECK(!strcmp(farolescolhaui_frase(), "algo leve de uma hora para ver com meu filho"));
    CHECK(lerContador(countFile) == antes);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
    CHECK(lerContador(countFile) == antes);
  }

  // Novo ciclo QR -> GET + POST -> FESCUI_PREVIA -> OK -> /v1/farol/escolha -> P4.
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    antes = lerContador(countFile);
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, "algo leve de uma hora para ver com meu filho") == 200);
    resolver_atualizar(0.016f, 1200);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA && celular_estado() == CEL_PARADO);
    teclaR(SDLK_RETURN, 1);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA && lerContador(countFile) == antes);
    tecla(SDLK_RETURN);
    esperarUiEspera(3000);
    CHECK(lerContador(countFile) == antes + 1);
    CHECK(farolescolhaui_modo() == FESCUI_COMPARAR);
    CHECK(!resolver_aberto() && farolcompara_aberto());
    farolcompara_fechar();
    farolcompara_teste_reset();
    resolver_atualizar(1.0f, 2000);
  }

  // Voltar / fechar / reset com QR aberto sempre fecha socket!
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);

  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  farolescolhaui_fechar();
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);

  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  farolescolhaui_reset();
  CHECK(farolescolhaui_modo() == FESCUI_FECHADO && celular_estado() == CEL_PARADO);

  // URL enviada pelo celular -> FESCUI_AVISO (0 POSTs, socket fechado).
  resolver_fechar();
  resolver_atualizar(1.0f, 2500);
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    antes = lerContador(countFile);
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, "veja http://mal.example") == 200);
    resolver_atualizar(0.016f, 2600);
    CHECK(farolescolhaui_modo() == FESCUI_AVISO && celular_estado() == CEL_PARADO);
    CHECK(lerContador(countFile) == antes);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  }

  // CEL_FALHOU via CEL_ERROS_MAX caminhos errados -> FESCUI_AVISO sem beco -> OK reabre QR!
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    char host[128], path[128], req[512], resp[512];
    int porta = celular_porta(), i;
    extrairHostPath(celular_url(), host, sizeof host, path, sizeof path);
    snprintf(req, sizeof req,
             "GET /caminhoerrado HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", host);
    for (i = 0; i < CEL_ERROS_MAX && celular_estado() == CEL_ESPERANDO; i++) {
      if (httpLoopback(porta, req, resp, sizeof resp) < 0) break;
    }
    for (i = 0; i < 40 && celular_estado() == CEL_ESPERANDO; i++) usleep(25000);
    CHECK(celular_estado() == CEL_FALHOU);
    resolver_atualizar(0.016f, 2700);
    CHECK(farolescolhaui_modo() == FESCUI_AVISO && celular_estado() == CEL_PARADO);
    CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOZ);
    tecla(SDLK_RETURN);
    CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);
  }

  // CEL_EXPIROU real (CEL_VALIDADE_S=2) -> FESCUI_AVISO sem beco.
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    long long ini = monoMs();
    while (farolescolhaui_modo() == FESCUI_QR && monoMs() - ini < 5000) {
      resolver_atualizar(0.016f, 2800);
      usleep(50000);
    }
    CHECK(farolescolhaui_modo() == FESCUI_AVISO && celular_estado() == CEL_PARADO);
    tecla(SDLK_RETURN);
    CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);
  }

  limparTudo();
  puts("ok  LG webOS QR (0,0,1): GET/POST loopback, CEL_PARADO, previa 0 POSTs, Voltar/fechar/reset, FALHOU/EXPIROU");
}

static void provaCombinadoEAndroid(const char *base) {
  char url[512];
  limparTudo();
  montarCatCanonico();
  st_teste_ligar(1);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);

  // Combinado (0, 1, 1): alternancia FESCUI_FOCO_ALT entre DIGITANDO e QR, e na PREVIA.
  farolescolhaui_teste_modos(0, 1, 1);
  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_TECLADO_TV));
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && st_estado() == ST_PARADO &&
        celular_estado() == CEL_ESPERANDO);
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO && celular_estado() == CEL_PARADO &&
        st_estado() == ST_DIGITANDO);
  st_teste_evento("Dfrase combinada");
  resolver_atualizar(0.016f, 1100);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  irFocoFluxo(FESCUI_FOCO_ALT);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);

  // Android (1, 1, 1): preserva #54 100%.
  limparTudo();
  st_teste_ligar(1);
  farolescolhaui_teste_modos(1, 1, 1);
  CHECK(!strcmp(farolescolhaui_rotulo_botao(), FT_ACAO_FALAR));
  CHECK(!strcmp(farolescolhaui_icone_botao(), "aj_mic"));
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_OUVINDO);
  st_teste_evento("Valgo leve android");
  resolver_atualizar(0.016f, 1200);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  // Em Android nao ha FESCUI_FOCO_ALT (so CONFIRMAR, VOZ, VOLTAR).
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOZ);
  tecla(SDLK_RIGHT);
  CHECK(farolescolhaui_foco() == FESCUI_FOCO_VOLTAR);
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);

  limparTudo();
  puts("ok  modo combinado (0,1,1) e Android (1,1,1) preservado");
}

static void provaUtf8MultiTv(const char *base, const char *countFile) {
  char url[512], longa[1024];
  int antes;

  limparTudo();
  montarCatCanonico();
  st_teste_ligar(1);
  farolescolhaui_teste_modos(0, 1, 0);
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  abrirP5();
  antes = lerContador(countFile);

  // IME: 150 x 'é' (300 B, 150 cp)
  montarRepUtf8(longa, sizeof longa, 'D', "\xC3\xA9", 150);
  focarEntrada();
  tecla(SDLK_RETURN);
  st_teste_evento(longa);
  resolver_atualizar(0.016f, 1100);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(contarUtf8Valido(farolescolhaui_frase()) == 150 && strlen(farolescolhaui_frase()) == 300);
  tecla(SDLK_ESCAPE);

  // IME: 220 x 'é' (2B) -> 200 cp (400 B)
  montarRepUtf8(longa, sizeof longa, 'D', "\xC3\xA9", 220);
  focarEntrada();
  tecla(SDLK_RETURN);
  st_teste_evento(longa);
  resolver_atualizar(0.016f, 1200);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(contarUtf8Valido(farolescolhaui_frase()) == FAROL_ESCOLHA_FRASE_MAX &&
        strlen(farolescolhaui_frase()) == 400);
  tecla(SDLK_ESCAPE);

  // IME: 220 x '漢' (3B) -> 200 cp (600 B)
  montarRepUtf8(longa, sizeof longa, 'D', "\xE6\xBC\xA2", 220);
  focarEntrada();
  tecla(SDLK_RETURN);
  st_teste_evento(longa);
  resolver_atualizar(0.016f, 1300);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(contarUtf8Valido(farolescolhaui_frase()) == FAROL_ESCOLHA_FRASE_MAX &&
        strlen(farolescolhaui_frase()) == 600);
  tecla(SDLK_ESCAPE);

  // IME: 220 x '🎬' (4B) -> 200 cp (800 B)
  montarRepUtf8(longa, sizeof longa, 'D', "\xF0\x9F\x8E\xAC", 220);
  focarEntrada();
  tecla(SDLK_RETURN);
  st_teste_evento(longa);
  resolver_atualizar(0.016f, 1400);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(contarUtf8Valido(farolescolhaui_frase()) == FAROL_ESCOLHA_FRASE_MAX &&
        strlen(farolescolhaui_frase()) == 800);
  tecla(SDLK_ESCAPE);

  // IME: fronteira 200 com espaco: 199 x 'é' + "   " + '漢' -> 199 cp (398 B)
  montarRepUtf8(longa, sizeof longa, 'D', "\xC3\xA9", 199);
  strncat(longa, "   \xE6\xBC\xA2", sizeof longa - strlen(longa) - 1);
  focarEntrada();
  tecla(SDLK_RETURN);
  st_teste_evento(longa);
  resolver_atualizar(0.016f, 1500);
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(contarUtf8Valido(farolescolhaui_frase()) == 199 && strlen(farolescolhaui_frase()) == 398);
  tecla(SDLK_ESCAPE);

  // QR: mistura UTF-8 2+3+4 bytes (70 triplas = 210 cp -> 200 cp = 599 B)
  farolescolhaui_teste_modos(0, 0, 1);
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  {
    char qrUrl[128], resp[4096], mista[1024];
    int porta = celular_porta(), i;
    size_t pos = 0;
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    mista[pos++] = ' ';
    for (i = 0; i < 70; i++) {
      memcpy(mista + pos, "\xC3\xA9\xE6\xBC\xA2\xF0\x9F\x8E\xAC", 9);
      pos += 9;
    }
    mista[pos] = '\0';
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, mista) == 200);
    resolver_atualizar(0.016f, 1600);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA && celular_estado() == CEL_PARADO);
    CHECK(contarUtf8Valido(farolescolhaui_frase()) == FAROL_ESCOLHA_FRASE_MAX);
    CHECK(strlen(farolescolhaui_frase()) == 599);
    tecla(SDLK_ESCAPE);
    CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  }
  CHECK(lerContador(countFile) == antes);

  limparTudo();
  puts("ok  UTF-8 2/3/4 bytes (200 codepoints sem corte) em IME e QR");
}

static void rodarCasoPrivacidadeMultiTv(const char *url, const char *countFile,
                                       int modoImeOn, int caso) {
  int antes = lerContador(countFile);
  configurarAppAutorizado(url);
  if (caso == 0) farolescolhaui_teste_infantil(1);
  else if (caso == 2) CHECK(farolconsent_definir(FAROL_CONSENT_SO_TV));
  else if (caso == 3) farolsinal_definir_recomendar(0);
  else if (caso == 4) {
    seguro_iniciar(1, 1100, NULL);
    seguro_iniciar(1, 1200, NULL);
    CHECK(seguro_perfil_ativo() == 1);
  }

  if (modoImeOn) {
    st_teste_ligar(1);
    farolescolhaui_teste_modos(0, 1, 0);
  } else {
    st_teste_ligar(0);
    farolescolhaui_teste_modos(0, 0, 1);
  }

  abrirP5();
  if (caso == 1) CHECK(farolconsent_definir(FAROL_CONSENT_NAO_PERGUNTADO));
  focarEntrada();
  tecla(SDLK_RETURN);
  if (modoImeOn) {
    CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
    st_teste_evento("Dalgo leve");
    resolver_atualizar(0.016f, 1100);
  } else {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, "algo leve") == 200);
    resolver_atualizar(0.016f, 1100);
  }
  CHECK(farolescolhaui_modo() == FESCUI_PREVIA);
  CHECK(lerContador(countFile) == antes);
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA);
  CHECK(lerContador(countFile) == antes);
  limparTudo();
}

static void provaPrivacidadeMultiTv(const char *base, const char *countFile) {
  char url[512];
  int m, c;
  montarCatCanonico();
  snprintf(url, sizeof url, "%s/canonico", base);
  for (m = 0; m < 2; m++) {
    for (c = 0; c < 5; c++) rodarCasoPrivacidadeMultiTv(url, countFile, m, c);
  }
  puts("ok  privacidade/consentimento multi-TV (IME + QR): infantil/sem-consent/SO_TV/recomendar=0/privado = 0 POSTs e fallback");
}
#endif

#ifdef NV_LINUX_DESKTOP
static void salvarPng(const char *path, const unsigned char *rgba, int w, int h) {
  int y, pitch = w * 4;
  unsigned char *flip = malloc((size_t)pitch * (size_t)h);
  SDL_Surface *s;
  CHECK(flip != NULL);
  for (y = 0; y < h; y++)
    memcpy(flip + (size_t)y * (size_t)pitch, rgba + (size_t)(h - 1 - y) * (size_t)pitch,
           (size_t)pitch);
  s = SDL_CreateRGBSurfaceFrom(flip, w, h, 32, pitch, 0x000000ffu, 0x0000ff00u,
                               0x00ff0000u, 0xff000000u);
  CHECK(s != NULL);
  CHECK(IMG_SavePNG(s, path) == 0);
  SDL_FreeSurface(s);
  free(flip);
}

static void capturar(const char *dir, const char *nome, unsigned char *px, int W, int H) {
  char path[1024];
  SDL_Surface *lida;
  int i, claros = 0;
  size_t b;
  for (i = 0; i < 20; i++) {
    resolver_atualizar(0.016f, 3000u + (Uint32)i * 16u);
    txt_novo_quadro();
    gfx_novo_quadro();
    glViewport(0, 0, W, H);
    glClearColor(0.04f, 0.05f, 0.07f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    resolver_desenhar(3000u + (Uint32)i * 16u);
    glFinish();
  }
  glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);
  for (b = 0; b < (size_t)W * (size_t)H * 4u; b += 4)
    if (px[b] > 200 && px[b + 1] > 200 && px[b + 2] > 200) claros++;
  CHECK(claros > 2000);
  snprintf(path, sizeof path, "%s/%s.png", dir, nome);
  salvarPng(path, px, W, H);
  lida = IMG_Load(path);
  CHECK(lida && lida->w == W && lida->h == H);
  SDL_FreeSurface(lida);
  printf("ok  captura %s: %dx%d\n", nome, W, H);
}

static void provaGl(const char *dir, const char *base) {
  const int W = 1920, H = 1080;
  char url[512];
  GLuint qtex;
  SDL_Window *win;
  SDL_GLContext gl;
  unsigned char *px = malloc((size_t)W * (size_t)H * 4u);
  CHECK(px != NULL);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
  win = SDL_CreateWindow("farol-55", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, W, H,
                         SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  CHECK(win != NULL);
  gl = SDL_GL_CreateContext(win);
  CHECK(gl != NULL);
  glViewport(0, 0, W, H);
  gfx_tamanho_alvo(W, H);
  CHECK(gfx_iniciar());
  { char ic[4096]; if (realpath("deploy/app/art", ic)) gfx_icones_dir(ic); }
  CHECK(txt_iniciar("deploy/app", 1));
  tex_iniciar(16);

  montarCatCanonico();
  snprintf(url, sizeof url, "%s/canonico", base);
  configurarAppAutorizado(url);
  st_teste_ligar(0);
  farolescolhaui_teste_modos(0, 0, 1);

  abrirP5();
  capturar(dir, "esc55-pergunta", px, W, H);

  // 1. Abrir QR cria textura GL real; Voltar destroi textura GL + fecha socket (0 leak).
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR && celular_estado() == CEL_ESPERANDO);
  capturar(dir, "esc55-qr", px, W, H);
  qtex = (GLuint)farolescolhaui_teste_qr_tex();
  CHECK(qtex != 0 && glIsTexture(qtex));
  tecla(SDLK_ESCAPE);
  CHECK(farolescolhaui_modo() == FESCUI_PERGUNTA && celular_estado() == CEL_PARADO);
  CHECK(farolescolhaui_teste_qr_tex() == 0 && !glIsTexture(qtex));

  // 2. Recebimento via celular_pegar() destroi textura GL + fecha socket; captura previa UTF-8.
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_QR);
  capturar(dir, "esc55-qr", px, W, H);
  qtex = (GLuint)farolescolhaui_teste_qr_tex();
  CHECK(qtex != 0 && glIsTexture(qtex));
  {
    char qrUrl[128], resp[4096];
    int porta = celular_porta();
    snprintf(qrUrl, sizeof qrUrl, "%s", celular_url());
    CHECK(fazerGetCelular(qrUrl, porta, resp, sizeof resp) == 200);
    CHECK(enviarPostCelular(qrUrl, porta, "ação e suspense rápido para hoje") == 200);
    resolver_atualizar(0.016f, 4000);
    CHECK(farolescolhaui_modo() == FESCUI_PREVIA && celular_estado() == CEL_PARADO);
    CHECK(farolescolhaui_teste_qr_tex() == 0 && !glIsTexture(qtex));
  }
  capturar(dir, "esc55-previa-qr", px, W, H);
  tecla(SDLK_ESCAPE);

  // 3. farolescolhaui_fechar() e farolescolhaui_reset() com QR aberto destroem textura GL (0 leak).
  focarEntrada();
  tecla(SDLK_RETURN);
  capturar(dir, "esc55-qr", px, W, H);
  qtex = (GLuint)farolescolhaui_teste_qr_tex();
  CHECK(qtex != 0 && glIsTexture(qtex));
  farolescolhaui_fechar();
  CHECK(celular_estado() == CEL_PARADO &&
        farolescolhaui_teste_qr_tex() == 0 && !glIsTexture(qtex));

  focarEntrada();
  tecla(SDLK_RETURN);
  capturar(dir, "esc55-qr", px, W, H);
  qtex = (GLuint)farolescolhaui_teste_qr_tex();
  CHECK(qtex != 0 && glIsTexture(qtex));
  farolescolhaui_reset();
  CHECK(celular_estado() == CEL_PARADO &&
        farolescolhaui_teste_qr_tex() == 0 && !glIsTexture(qtex));

  // 4. Samsung/WASM IME + Aviso.
  resolver_fechar();
  resolver_atualizar(1.0f, 5000);
  st_teste_ligar(1);
  farolescolhaui_teste_modos(0, 1, 1);
  abrirP5();
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_DIGITANDO);
  capturar(dir, "esc55-ime", px, W, H);
  tecla(SDLK_ESCAPE);

  st_teste_ligar(0);
  farolescolhaui_teste_modos(0, 1, 1);
  focarEntrada();
  tecla(SDLK_RETURN);
  CHECK(farolescolhaui_modo() == FESCUI_AVISO);
  capturar(dir, "esc55-aviso", px, W, H);

  limparTudo();
  free(px);
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(win);
  puts("ok  GL 1920x1080 + ZERO leak de textura QR em Voltar/recebimento/fechar/reset");
}
#endif

int main(int argc, char **argv) {
  const char *base = argc > 1 ? argv[1] : "";
  const char *countFile = argc > 2 ? argv[2] : "";
  const char *shotDir = argc > 3 ? argv[3] : NULL;
  CHECK(base[0] && countFile[0]);
  CHECK(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG);
  ajustes_iniciar();
  dados_iniciar("deploy/app/art");
  farolcompara_teste_player(playerTeste);

#ifdef NV_TPK
  (void)shotDir;
  if (rodarTpkRelease(base, countFile) != 0) return 1;
#else
  provaModosDesligados(base);
  provaSamsungWasmIme(base, countFile);
  provaLgWebOsQr(base, countFile);
  provaCombinadoEAndroid(base);
  provaUtf8MultiTv(base, countFile);
  provaPrivacidadeMultiTv(base, countFile);
#ifdef NV_LINUX_DESKTOP
  if (shotDir) provaGl(shotDir, base);
#else
  (void)shotDir;
#endif
#endif

  farolescolha_rede_encerrar();
  CHECK(!gFalhas);
  CHECK(farolcompara_teste_esperar(3000));
  SDL_Quit();
  puts("farol_escolha_tv: tudo ok");
  return 0;
}
