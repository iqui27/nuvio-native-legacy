// AJUSTE "Teclado: Do sistema" no .tpk de release (tizen-tpk/Voz.cs <->
// src/entrada_texto.c <-> sistexto.h). O host fingido aqui faz o que o Voz.cs
// faz, e o teste confere o que a Busca, o Spotlight e a modal veem:
//   - Automatico e "Do app": grade do app + botao de microfone, como sempre;
//   - "Do sistema": teclado da Samsung para digitar, sem grade e sem microfone;
//   - abrir leva o texto atual; o texto volta AO VIVO (nao so no fim, como no
//     microfone); Concluido com o campo vazio confirma; o app fechar esconde;
//   - cursor (Esquerda/Direita): a posicao em caracteres vira o trecho antes
//     dele, UTF-8 inteiro; no fim, ou de outro dono, nao ha trecho;
//   - o teclado nao subir: a grade e o microfone voltam nesta sessao.
//   bash tests/tpk_teclado_sistema.sh
#include "sistexto.h"
#include "entrada_texto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*Fn)(void);
typedef void (*FnTexto)(const char *atual);
void nv_tpk_voz_registrar(Fn iniciar, Fn parar, int microfone);
void nv_tpk_voz_registrar_texto(FnTexto abrir);
void nv_tpk_texto_valor(const char *v);
void nv_tpk_texto_cursor(int pos);
void nv_tpk_texto_fim(int confirmou);

const char *desc_tmdb_idioma(void) { return "pt-BR"; }
static int modo;   // ajuste Teclado: 0 automatico, 1 do app, 2 do sistema
int ajustes_teclado_android(void) { return modo; }

static int iniciou, parou, abriuTexto;
static char textoAberto[256];
static void hIniciar(void) { iniciou++; }
static void hParar(void) { parou++; }
static void hAbrirTexto(const char *atual) { abriuTexto++; snprintf(textoAberto, sizeof textoAberto, "%s", atual); }

// O laco do app: quadro + eventos para quem tem o campo.
static int ler(int dono, char *t, size_t n) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) st_evento(&e);
  return st_ler(dono, t, n);
}

int main(void) {
  char t[256], antes[256];
  int r;
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  st_teste_ligar(0);
  nv_tpk_voz_registrar(hIniciar, hParar, -1);
  nv_tpk_voz_registrar_texto(hAbrirTexto);
  texto_sistema_mic_modo(TS_MIC_AUTO);

  // Automatico e "Do app": nada muda (grade do app + microfone).
  for (modo = 0; modo <= 1; modo++) {
    assert(!st_ime_disponivel() && !st_teclado_so_sistema());
    assert(st_voz_disponivel() && texto_sistema_voz_pelo_teclado());
    assert(!st_ime_abrir(ST_BUSCA, "x", 64) && abriuTexto == 0);
  }

  // "Do sistema": teclado da Samsung para digitar; grade e microfone somem.
  modo = 2;
  assert(st_ime_disponivel() && st_teclado_so_sistema() && !st_voz_disponivel());
  assert(st_ime_abrir(ST_BUSCA, "matrix", 64));
  assert(abriuTexto == 1 && !strcmp(textoAberto, "matrix") && iniciou == 0);
  assert(st_estado() == ST_DIGITANDO && st_dono() == ST_BUSCA && !st_aviso()[0]);
  // O texto vem ao vivo (o campo do app espelha o do teclado).
  nv_tpk_texto_valor("matri");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_TEXTO && !strcmp(t, "matri"));

  // Cursor: no meio vira o trecho antes dele; no fim, ou de outro dono, nada.
  assert(st_cursor_chars(ST_BUSCA) == -1);          // ainda nao informado: no fim
  nv_tpk_texto_cursor(2);
  ler(ST_BUSCA, t, sizeof t);
  assert(st_cursor_chars(ST_BUSCA) == 2 && st_cursor_chars(ST_SPOT) == -1);
  assert(st_cursor_prefixo(ST_BUSCA, "matri", antes, sizeof antes) && !strcmp(antes, "ma"));
  assert(!st_cursor_prefixo(ST_SPOT, "matri", antes, sizeof antes) && !antes[0]);
  nv_tpk_texto_cursor(5);
  ler(ST_BUSCA, t, sizeof t);
  assert(!st_cursor_prefixo(ST_BUSCA, "matri", antes, sizeof antes));
  // Caracteres, nao bytes: "ação" com o cursor em 2 fica "aç".
  nv_tpk_texto_valor("ação");
  nv_tpk_texto_cursor(2);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_TEXTO && st_cursor_prefixo(ST_BUSCA, t, antes, sizeof antes) && !strcmp(antes, "a\xC3\xA7"));
  // Posicao alem do texto (o app cortou): fica no fim.
  nv_tpk_texto_cursor(40);
  ler(ST_BUSCA, t, sizeof t);
  assert(!st_cursor_prefixo(ST_BUSCA, "ação", antes, sizeof antes));

  // Concluido com o campo apagado: confirma vazio; o cursor some com o teclado.
  nv_tpk_texto_valor("");
  nv_tpk_texto_fim(1);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_FIM && !t[0] && st_estado() == ST_PARADO && !texto_sistema_aberto());
  assert(st_cursor_chars(ST_BUSCA) == -1);

  // Voltar no teclado: cancela (a modal fecha; a busca fica no campo).
  assert(st_ime_abrir(ST_TECLADO, "abc", 64) && abriuTexto == 2);
  nv_tpk_texto_fim(0);
  r = ler(ST_TECLADO, t, sizeof t);
  assert(r == ST_CANCELOU && st_estado() == ST_PARADO);

  // O app fecha a tela com o teclado aberto: o host esconde.
  assert(st_ime_abrir(ST_SPOT, "", 64) && abriuTexto == 3);
  st_fechar(ST_SPOT);
  assert(parou == 1 && !texto_sistema_aberto() && st_estado() == ST_PARADO);
  assert(ler(ST_SPOT, t, sizeof t) == ST_NADA);

  // O teclado nao subiu (prazo de 3 s no host): a grade e o microfone voltam.
  assert(st_ime_abrir(ST_BUSCA, "", 64) && abriuTexto == 4);
  nv_tpk_texto_fim(-1);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_CANCELOU && st_estado() == ST_PARADO);
  assert(!st_ime_disponivel() && !st_teclado_so_sistema() && st_voz_disponivel());
  assert(!st_ime_abrir(ST_BUSCA, "", 64) && abriuTexto == 4);
  // O microfone continua abrindo o teclado da Samsung pelo caminho dele.
  assert(st_voz_iniciar(ST_BUSCA) && iniciou == 1 && abriuTexto == 4);
  st_fechar(ST_BUSCA);

  SDL_Quit();
  puts("PASS: tpk_teclado_sistema (Do sistema: teclado da Samsung, sem grade nem microfone; cursor)");
  return 0;
}
