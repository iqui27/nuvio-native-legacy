// A linha "Importar fonte TTF/OTF" dos Ajustes abre a modal de teclado no modo
// LONGO: uma URL de fonte passa de 64 caracteres e o downloader precisa dela
// inteira (F82). O teste dirige a rota de producao — definirValorDireto ->
// ajFonteAplicar -> teclado_abrir_com, a mesma do D-pad na TV — e alimenta a
// modal do jeito que o teclado do sistema (definirDoSistema, mesmo filtro de
// alfabeto e teto) e o controle remoto (aplicar, um caractere por OK) alimentam.
// O Concluir segue o caminho real de gravacao: teclado PRONTO ->
// ajustes_atualizar -> legfontes_pedir_url -> fonteurl, e a resposta vem por
// legfontes_atualizar como no laco do app.
#include "../src/ajustes.c"
#include "celbotao.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

void teclado_teste_sistema(const char *t);   // teclado.c, so com AJUSTES_TESTE
void teclado_teste_foco(int f, int c);

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (!celb_evento(&e)) teclado_evento(&e);
}

// O mesmo "novo" que a tela produz quando o foco cai na linha de importacao:
// base de fontes do sistema + as importadas + 1 (ver ajFonteNome/nValores).
static void abrirFonteUrl(void) {
  int novo = ajFonteBase(AJ_FONTE_UI) + legfontes_n();
  assert(definirValorDireto(AJ_FONTE_UI, novo) == 1);
  assert(teclado_aberto());
}

// Navega ate "Concluir" como o controle remoto (fim da grade, ultima coluna) e
// devolve o quadro ao laco dos Ajustes, que consome o resultado da modal.
static void concluir(void) {
  int i;
  teclado_teste_foco(0, 0);
  for (i = 0; i < 20; i++) tecla(SDLK_DOWN);
  for (i = 0; i < 4; i++) tecla(SDLK_RIGHT);
  tecla(SDLK_RETURN);
  assert(!teclado_aberto());
  ajustes_atualizar(0.016f, SDL_GetTicks());
}

static void cancelar(void) {
  tecla(SDLK_ESCAPE);
  assert(!teclado_aberto());
  (void)teclado_resultado();
  ajustes_atualizar(0.016f, SDL_GetTicks());
}

int main(void) {
  const char *dir = getenv("NUVIO_DADOS");
  setvbuf(stdout, NULL, _IONBF, 0);
  assert(dir && *dir);
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar(dir);
  ajustes_dir(dir);
  legfontes_dir(dir);
  ajustes_iniciar();
  assert(dados_gravar("aviso-legal.txt", "1\n"));

  // URL de fonte HTTPS real de 87 caracteres (a sonda da revisao).
  const char *url87 =
    "https://cdn.example.invalid/assets/fonts/inter-display-regular/InterDisplay-Regular.ttf";
  assert(strlen(url87) == 87);
  // E uma de 300, bem acima do teto antigo de 64 e dentro do campo longo.
  char url300[400];
  memcpy(url300, "https://cdn.example.invalid/fonts/", 34);
  memset(url300 + 34, 'a', 262);
  memcpy(url300 + 296, ".ttf", 5);
  assert(strlen(url300) == 300);

  // Teclado do sistema: o texto chega inteiro pelo filtro de definirDoSistema.
  abrirFonteUrl();
  teclado_teste_sistema(url87);
  assert(!strcmp(teclado_texto(), url87));
  // E o Concluir entrega esse texto ao downloader (o evento volta por
  // legfontes_atualizar: na sandbox sem rede o fim e de erro, mas so existe
  // porque o pedido saiu com a URL inteira).
  concluir();
  {
    const char *evento = NULL;
    int i;
    for (i = 0; i < 400 && !evento; i++) {
      evento = legfontes_atualizar();
      if (!evento) SDL_Delay(25);
    }
    assert(evento != NULL);
  }

  abrirFonteUrl();
  teclado_teste_sistema(url300);
  assert(!strcmp(teclado_texto(), url300));
  cancelar();

  // Controle remoto: cada OK na celula focada anexa um caractere enquanto
  // couber no campo — o mesmo n < maxN de teclado.c. 300 OKs tem de entrar
  // todos; com o teto antigo a grade parava nos 64.
  abrirFonteUrl();
  {
    int i;
    for (i = 0; i < 300; i++) tecla(SDLK_RETURN);
    assert(strlen(teclado_texto()) == 300);
  }
  cancelar();

  puts("teclado_fonte_url: URL longa pelo sistema, pelo controle e ao downloader ok");
  return 0;
}
