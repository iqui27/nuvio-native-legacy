// A RAIL NAS CAPTURAS (26/09, "quando a sidebar ta no modo pinned ela corta a
// interface"). Os *_shot.c desenham SO a tela; no app a rail e desenhada por
// cima dela em app.c, e era exatamente essa sobreposicao que nenhuma captura
// mostrava. Com NUVIO_RAIL definido o teste liga o modo pedido e pinta o menu
// por cima, como o app faz:
//
//   NUVIO_RAIL=fixa       collapseSidebar Fixa, barra moderna desligada
//   NUVIO_RAIL=moderna    barra moderna ligada (desliga o recolhimento)
//   NUVIO_RAIL=recolhida  o padrao de fabrica do perfil: rail nenhuma
//
// Sem a variavel, nada muda: o teste continua o de antes. O modo entra pelo
// caminho real (um ajustes.txt lido por ajustes_dir), numa pasta PROPRIA, e
// nao na NUVIO_DADOS do teste — a dele pode ter um ajustes.txt que o teste
// escreveu e que nao e nosso.
#ifndef NV_TESTS_RAIL_SHOT_H
#define NV_TESTS_RAIL_SHOT_H
#include "ajustes.h"
#include "menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int rail_shot_ligado(void) {
  const char *m = getenv("NUVIO_RAIL");
  return m && *m;
}

// Reaplica o modo. Barato (um arquivo de duas linhas) e idempotente: o teste
// chama antes de cada captura, porque alguns releem o proprio ajustes.txt no
// meio do roteiro e isso devolveria a rail ao padrao.
static void rail_shot_aplicar(void) {
  static char dir[512];
  const char *m = getenv("NUVIO_RAIL");
  char caminho[600];
  FILE *f;
  if (!m || !*m) return;
  if (!dir[0]) {
    const char *t = getenv("TMPDIR");
    snprintf(dir, sizeof dir, "%s/nuvio-rail-shot-XXXXXX", t && *t ? t : "/tmp");
    if (!mkdtemp(dir)) { dir[0] = 0; return; }
  }
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  if (!f) return;
  // V_RAIL = { Recolhida, Fixa } e V_LIGA = { Ligado, Desligado }: o indice
  // gravado e o da lista, entao "modernSidebar 0" e LIGADA.
  fprintf(f, "collapseSidebar %d\nmodernSidebar %d\n",
          strcmp(m, "recolhida") ? 1 : 0, strcmp(m, "moderna") ? 1 : 0);
  fclose(f);
  ajustes_dir(dir);
}

// Pinta a rail por cima do quadro ja desenhado, com `destino` aceso.
static void rail_shot_desenhar(int destino) {
  if (!rail_shot_ligado()) return;
  menu_definir_destino(destino);
  menu_desenhar(SDL_GetTicks());
}
#endif
