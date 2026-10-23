// A RAIL NAS CAPTURAS (26/09, "quando a sidebar ta no modo pinned ela corta a
// interface"). Os *_shot.c desenham SO a tela; no app a rail e desenhada por
// cima dela em app.c, e era exatamente essa sobreposicao que nenhuma captura
// mostrava. Com NUVIO_RAIL definido o teste liga o modo pedido e pinta o menu
// por cima, como o app faz:
//
//   NUVIO_RAIL=fixa       collapseSidebar Fixa, barra moderna desligada
//   NUVIO_RAIL=moderna    barra moderna ligada (desliga o recolhimento)
//   NUVIO_RAIL=recolhida  o padrao de fabrica do perfil: rail nenhuma
//   NUVIO_RAIL=dinamica   layout Dinamica: so a pilula da barra no topo
//
// Sem a variavel, nada muda: o teste continua o de antes. O modo entra pelo
// caminho real (um ajustes.txt lido por ajustes_dir), numa pasta PROPRIA, e
// nao na NUVIO_DADOS do teste — a dele pode ter um ajustes.txt que o teste
// escreveu e que nao e nosso.
#ifndef NV_TESTS_RAIL_SHOT_H
#define NV_TESTS_RAIL_SHOT_H
#include "ajustes.h"
#include "menu.h"
#include "ilha.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "nvtmp.h"

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
    snprintf(dir, sizeof dir, "%s/nuvio-rail-shot-XXXXXX", t && *t ? t : nvt_dir());
    if (!mkdtemp(dir)) { dir[0] = 0; return; }
  }
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);
  f = fopen(caminho, "w");
  if (!f) return;
  // V_RAIL = { Recolhida, Fixa } e V_LIGA = { Ligado, Desligado }: o indice
  // gravado e o da lista, entao "modernSidebar 0" e LIGADA.
  fprintf(f, "collapseSidebar %d\nmodernSidebar %d\n",
          strcmp(m, "recolhida") ? 1 : 0, strcmp(m, "moderna") ? 1 : 0);
  // NUVIO_RAIL_OCULTOS=1 (#162): Guia e Agenda escondidos, para ver a barra
  // com menos itens.
  // NUVIO_RAIL=dinamica: layout Dinamica da home, onde a barra e a pilula da
  // Apple TV no topo esquerdo (menu.c) e nao ha rail fixa.
  if (!strcmp(m, "dinamica")) fputs("homeLayoutLocal 2\n", f);
  // NUVIO_SHOT_EN=1 (English UI) and NUVIO_SHOT_FONTE=3 (Montserrat): the
  // owner's real Android TV. Without them this file would silently reset the
  // language and font the shot itself had written.
  if (getenv("NUVIO_SHOT_EN")) fputs("idioma 1\n", f);
  if (getenv("NUVIO_SHOT_FONTE")) fprintf(f, "fonteInterface %d\n", atoi(getenv("NUVIO_SHOT_FONTE")));
  { const char *o = getenv("NUVIO_RAIL_OCULTOS");
    if (o && *o == '1') fputs("menuGuiaLocal 1\nmenuAgendaLocal 1\n", f); }
  fclose(f);
  ajustes_dir(dir);
}

// Pinta a rail por cima do quadro ja desenhado, com `destino` aceso.
static void rail_shot_relogio(void) {   // NUVIO_SHOT_RELOGIO=1: the clock island, as app.c draws it
  if (getenv("NUVIO_SHOT_RELOGIO")) {
    ilha_relogio_visivel(1);
    ilha_posicionar(1);
    ilha_desenhar(SDL_GetTicks());
  }
}
static void rail_shot_desenhar(int destino) {
  if (!rail_shot_ligado()) { rail_shot_relogio(); return; }
  menu_definir_destino(destino);
  { const char *m = getenv("NUVIO_RAIL");
    if (m && !strcmp(m, "dinamica")) {   // the pill fades in like app.c asks for it
      int k;
      menu_pilula_mostrar(1.0f);
      for (k = 0; k < 30; k++) menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
    } }
  menu_desenhar(SDL_GetTicks());
  rail_shot_relogio();
}
#endif
