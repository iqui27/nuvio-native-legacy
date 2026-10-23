// BOTAO DE MICROFONE no .tpk (tizen-tpk/Voz.cs <-> src/entrada_texto.c): o
// botao abre o teclado da Samsung so para usar o microfone dele; o teclado do
// app continua sendo o das buscas. O host fingido aqui faz o que o Voz.cs faz,
// e o teste confere o que a Busca e o Spotlight veem por sistexto.h:
//   - sem host registrado (TV sem microfone): sem botao;
//   - registrado: botao SIM, teclado do sistema para digitar NAO;
//   - OK no botao abre o teclado da Samsung; o campo diz "Aperte o microfone
//     do controle e fale." (nao "Ouvindo…": o teclado ainda nao ouve);
//   - o teclado cobre a tela: o texto do meio do caminho NAO chega ao campo
//     (nada e buscado por baixo dele); so o final, inteiro, com Concluido
//     (ST_FIM); Voltar cancela sem mexer no campo; nos dois o aviso some.
//   bash tests/tpk_voz.sh
#include "sistexto.h"
#include "entrada_texto.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef void (*Fn)(void);
void nv_tpk_voz_registrar(Fn iniciar, Fn parar, int microfone);
void nv_tpk_texto_valor(const char *v);
void nv_tpk_texto_fim(int confirmou);

const char *desc_tmdb_idioma(void) { return "pt-BR"; }
int ajustes_teclado_android(void) { return 0; }

static int iniciou, parou, valorRecebido;
static void hIniciar(void) { iniciou++; }
static void hParar(void) { parou++; }

// O laco do app: quadro + eventos para quem tem o campo.
static int ler(int dono, char *t, size_t n) {
  SDL_Event e;
  texto_sistema_quadro();
  while (SDL_PollEvent(&e)) st_evento(&e);
  return st_ler_valor(dono, t, n, &valorRecebido);
}

static const char *HINT = "Aperte o microfone do controle e fale.";

int main(void) {
  char t[256];
  int r;
  assert(SDL_Init(SDL_INIT_EVENTS) == 0);
  st_teste_ligar(0);

  // TV sem microfone: o host nao registra, o botao nao existe.
  assert(!st_voz_disponivel() && !st_ime_disponivel());
  assert(!st_voz_iniciar(ST_BUSCA));

  // AJUSTE "Botao de microfone na busca" contra o que a TV respondeu.
  // #585: TV microphone capability does not describe the remote's mic.
  // Automatic must keep the registered Samsung keyboard reachable.
  nv_tpk_voz_registrar(hIniciar, hParar, 0);
  texto_sistema_mic_modo(TS_MIC_AUTO);
  assert(st_voz_disponivel() && texto_sistema_voz_pelo_teclado());
  texto_sistema_mic_modo(TS_MIC_LIGADO);   assert(st_voz_disponivel());
  texto_sistema_mic_modo(TS_MIC_DESLIGADO); assert(!st_voz_disponivel());
  // TV sem resposta: Automatico mostra (o teclado continua util).
  nv_tpk_voz_registrar(hIniciar, hParar, -1);
  texto_sistema_mic_modo(TS_MIC_AUTO);     assert(st_voz_disponivel());
  // TV com microfone: Automatico mostra, Desligado esconde.
  nv_tpk_voz_registrar(hIniciar, hParar, 1);
  texto_sistema_mic_modo(TS_MIC_DESLIGADO); assert(!st_voz_disponivel() && !st_voz_iniciar(ST_BUSCA));
  texto_sistema_mic_modo(TS_MIC_AUTO);

  // Registrado: o botao existe; digitar continua no teclado do app, e o
  // teclado modal (chaves, nomes) fica sem o "Falar".
  assert(st_voz_disponivel() && texto_sistema_voz_pelo_teclado());
  assert(!st_ime_disponivel());
  assert(!st_ime_abrir(ST_BUSCA, "", 64));

  // OK no botao: teclado da Samsung aberto, esperando o microfone do controle.
  assert(st_voz_iniciar(ST_BUSCA) && iniciou == 1);
  assert(texto_sistema_voz_pelo_teclado());
  assert(st_estado() == ST_DIGITANDO && st_dono() == ST_BUSCA);
  assert(!strcmp(st_aviso(), HINT));
  // A fala chega pelo teclado, mas fica com ele enquanto ele cobre a tela.
  nv_tpk_texto_valor("o poderoso");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_NADA && !t[0] && !valorRecebido);
  assert(!strcmp(st_aviso(), HINT));
  nv_tpk_texto_valor("o poderoso chefão");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_NADA);
  // Concluido: so agora o texto inteiro entra (e a busca roda).
  nv_tpk_texto_fim(1);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_FIM && !strcmp(t, "o poderoso chefão") && valorRecebido);
  assert(st_estado() == ST_PARADO && !texto_sistema_aberto());
  assert(!st_aviso()[0]);                 // o aviso some com o teclado

  // Voltar no teclado da Samsung depois de falar: cancelado, nada chega ao
  // campo, aviso some.
  assert(st_voz_iniciar(ST_SPOT) && iniciou == 2 && !strcmp(st_aviso(), HINT));
  nv_tpk_texto_valor("matrix");
  nv_tpk_texto_fim(0);
  r = ler(ST_SPOT, t, sizeof t);
  assert(r == ST_CANCELOU && !t[0] && !valorRecebido && st_estado() == ST_PARADO && !st_aviso()[0]);

  // O teclado do app depois do botao: o texto volta a chegar normalmente
  // (a trava do "so o final" era so da sessao do microfone).
  st_teste_ligar(1);
  assert(st_ime_abrir(ST_BUSCA, "", 64));
  st_teste_evento("Tabc");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_TEXTO && !strcmp(t, "abc") && valorRecebido);
  // Integration: an empty edit followed by Back still counts as received text.
  st_teste_evento("T");
  st_teste_evento("X");
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_CANCELOU && !t[0] && valorRecebido);
  st_fechar(ST_BUSCA);
  st_teste_ligar(0);

  // Voltar na tela enquanto o teclado esta aberto: o app fecha, o host esconde.
  assert(st_voz_iniciar(ST_BUSCA) && iniciou == 3);
  st_fechar(ST_BUSCA);
  assert(parou == 1 && !texto_sistema_aberto() && st_estado() == ST_PARADO && !st_aviso()[0]);
  r = ler(ST_SPOT, t, sizeof t);          // o fim do fechamento nao vaza
  assert(r == ST_NADA);

  // Host startup failure must survive the bridge as visible feedback.
  assert(st_voz_iniciar(ST_BUSCA));
  nv_tpk_texto_fim(-1);
  r = ler(ST_BUSCA, t, sizeof t);
  assert(r == ST_CANCELOU && st_estado() == ST_PARADO);
  assert(st_aviso()[0] && !valorRecebido && !texto_sistema_aberto());

  SDL_Quit();
  puts("PASS: tpk_voz (botao abre o teclado da Samsung; teclado do app intacto)");
  return 0;
}
