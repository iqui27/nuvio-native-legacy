// Ajuste "Teclado" (Android): 0 automatico (o de sempre), 1 so a grade do app,
// 2 o teclado do sistema abre direto em todo campo. st_teste_ligar(1) faz o
// teste fazer o papel do Android.
#include "ajustes.h"
#include "dados.h"
#include "sistexto.h"
#include "teclado.h"
#include "spotlight.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "nvtmp.h"

static char dir[NVT_MAX], caminho[700];
static void modo(int v) {
  FILE *f = fopen(caminho, "w"); assert(f);
  fprintf(f, "tecladoAndroidLocal %d\n", v); fclose(f);
  ajustes_dir(dir);
  assert(ajustes_teclado_android() == v);
}
static int imeAberto(void) { return st_estado() == ST_DIGITANDO && st_dono() == ST_TECLADO; }
static void abrir(int tipo) {
  st_fechar(ST_TECLADO);
  teclado_abrir("Titulo", "dica", 40);
  if (tipo >= 0) teclado_tipo(tipo);
  // O modo "Do Android" adia a abertura um quadro (teclado_tipo pode trocar
  // o tipo logo depois do abrir).
  teclado_atualizar(0, 0);
}

int main(void) {
  assert(SDL_Init(SDL_INIT_EVENTS | SDL_INIT_TIMER) == 0);
  nvt_set(dir, sizeof dir, "/nuvio-teclado-ime-XXXXXX"); assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  st_teste_ligar(1);

  // Automatico: campo de texto abre a grade; e-mail e senha abrem o sistema.
  modo(0);
  abrir(TECLADO_TIPO_TEXTO); assert(!imeAberto() && teclado_foco_campo() == 0);
  abrir(TECLADO_TIPO_EMAIL); assert(imeAberto());
  abrir(TECLADO_TIPO_SENHA); assert(imeAberto());

  // Do app: o sistema nunca abre sozinho, nem em e-mail/senha; a grade tem foco.
  modo(1);
  abrir(TECLADO_TIPO_TEXTO); assert(!imeAberto() && teclado_foco_campo() == 0);
  abrir(TECLADO_TIPO_EMAIL); assert(!imeAberto() && teclado_foco_campo() == 0);
  abrir(TECLADO_TIPO_SENHA); assert(!imeAberto() && teclado_foco_campo() == 0);

  // Do Android: abre direto em todo campo, com o botao Teclado da TV em foco
  // (o caminho de volta para a grade, que Voltar tambem mostra).
  modo(2);
  abrir(-1); assert(imeAberto() && teclado_foco_campo() != 0);
  abrir(TECLADO_TIPO_TEXTO); assert(imeAberto());
  abrir(TECLADO_TIPO_EMAIL); assert(imeAberto());

  st_fechar(ST_TECLADO);
  // Busca: "Do app" deixa OK abrir e usar a grade; os outros modos usam IME.
  for (int v = 0; v <= 2; v++) {
    SDL_Event e;
    modo(v);
    spot_abrir(0);
    assert(spot_foco_campo() == 1);
    assert((st_estado() == ST_DIGITANDO && st_dono() == ST_SPOT) == (v != 1));
    memset(&e, 0, sizeof e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
    spot_evento(&e);
    assert(spot_teclado_app_aberto() == (v == 1));
    assert((st_estado() == ST_DIGITANDO && st_dono() == ST_SPOT) == (v != 1));
    if (v == 1) {
      assert(spot_foco_campo() == 0);
      spot_evento(&e); // OK na primeira tecla realmente escreve na busca.
      assert(spot_consulta()[0]);
      e.key.keysym.sym = SDLK_ESCAPE;
      spot_evento(&e);
      assert(!spot_teclado_app_aberto() && spot_foco_campo() == 1);
    }
    spot_fechar();
  }

  // Fora do Android o ajuste nao muda nada. st_teste_ligar(0) desliga o
  // "abre sozinho", e teclado_teste_modos(1, ...) faz o papel do teclado de
  // sistema DISPONIVEL (TV Samsung .tpk, web): e-mail/senha tem que continuar
  // abrindo a GRADE em foco, nao o IME por cima da modal.
  st_teste_ligar(0);
  teclado_teste_modos(1, -1, -1);
  abrir(TECLADO_TIPO_TEXTO); assert(!imeAberto() && teclado_foco_campo() == 0);
  abrir(TECLADO_TIPO_EMAIL); assert(!imeAberto() && teclado_foco_campo() == 0);
  abrir(TECLADO_TIPO_SENHA); assert(!imeAberto() && teclado_foco_campo() == 0);
  teclado_teste_modos(-1, -1, -1);
  puts("teclado_ime_modo: ok");
  return 0;
}
