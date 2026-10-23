// Exercise the actual preference reader, Settings controls and overlay RGB resolver.
#include "../src/ajustes.c"
#include <assert.h>
void sync_proteger_ajustes_locais(void) {}

static char dirPrefs[512];
static int prefsLidas, aspecto;
const char *const VIDEO_LEG_CORES[VIDEO_LEG_NCORES] = {"White", "Yellow", "Green", "Blue", "Red", "Grey"};
const char *const VIDEO_LEG_CORES_PT[VIDEO_LEG_NCORES] = {"White", "Yellow", "Green", "Blue", "Red", "Grey"};
char *SDL_GetBasePath(void) { return NULL; }
void SDL_free(void *p) { free(p); }
void dados_marcar_sujo(int imediato) { (void)imediato; }
void video_legenda_estilo(const VideoLegendaEstilo *e) { (void)e; }
#include "prefs.inc"

// Compile the real player picker and key handler with the real preference writer.
#define FX_COL_ESTILO 2
#define FX_N_ESTILO 11
#define FX_BARRA_COLS 3
#include "legendasui.h"
enum { EST_FONTE = 1, EST_COR = 2, EST_RESTAURAR = 9, EST_COR_BORDA = 10 };
static int ctl = -1, foco[3], coluna;
static int estiloPreservadoAss(int linha) { (void)linha; return 0; }
#include "picker.inc"

int main(int argc, char **argv) {
  assert(argc == 2);
  valor[AJ_LEG_CONTORNO_COR] = 3; // Yellow saved by 2.0.5.
  char path[600];
  snprintf(path, sizeof path, "%s/player.txt", argv[1]);
  FILE *f = fopen(path, "w");
  assert(f);
  fputs("leg_cor_borda 0\nleg_tocado 0\n", f); // Old player saved default Black.
  assert(fclose(f) == 0);
  player_dir(argv[1]);
  player_leg_estilo_carregar();
  assert(ajustes_leg_borda_rgb(player_leg_estilo()->corBorda) == 0xffde30u);
  foco[FX_COL_ESTILO] = EST_COR_BORDA;
  assert(estiloTecla(SDLK_RETURN, 0));
  assert(ctlSub == 2); // Focus the displayed legacy Yellow, never the unset sentinel.
  assert(player_leg_estilo()->corBorda == -1);
  assert(estiloTecla(SDLK_ESCAPE, 0));
  assert(player_leg_estilo()->corBorda == -1);
  // Unrelated edits must preserve the legacy colour, including after reload.
  assert(ajLegendaDefinir(AJ_LEG1_TAMANHO, 140));
  player_dir(argv[1]);
  player_leg_estilo_carregar();
  assert(ajustes_leg_borda_rgb(player_leg_estilo()->corBorda) == 0xffde30u);
  VideoLegendaEstilo secondary;
  valor[AJ_LEG2_COR_BORDA] = 1; // Explicit Black while primary remains legacy Yellow.
  ajustes_leg2_estilo(player_leg_estilo(), &secondary);
  assert(ajustes_leg_borda_rgb(secondary.corBorda) == 0);
  assert(ajLegendaDefinir(AJ_LEG1_COR_BORDA, 0));
  assert(ajustes_leg_borda_rgb(player_leg_estilo()->corBorda) == 0);
  player_dir(argv[1]);
  player_leg_estilo_carregar();
  assert(ajustes_leg_borda_rgb(player_leg_estilo()->corBorda) == 0);
  valor[AJ_LEG2_COR_BORDA] = 0; // Follow the explicit primary Black.
  ajustes_leg2_estilo(player_leg_estilo(), &secondary);
  assert(ajustes_leg_borda_rgb(secondary.corBorda) == 0);
  // Player movement and OK explicitly persist Black over legacy Yellow.
  player_leg_estilo()->corBorda = -1;
  assert(estiloTecla(SDLK_RETURN, 0) && ctlSub == 2);
  assert(estiloTecla(SDLK_LEFT, 0) && ctlSub == 1);
  assert(estiloTecla(SDLK_LEFT, 0) && ctlSub == 0);
  assert(estiloTecla(SDLK_RETURN, 0) && ctl == -1);
  player_dir(argv[1]); player_leg_estilo_carregar();
  assert(player_leg_estilo()->corBorda == 0);
  // Legacy Grey has no matching border swatch: focus Black without saving.
  valor[AJ_LEG_CONTORNO_COR] = 2;
  player_leg_estilo()->corBorda = -1;
  assert(estiloTecla(SDLK_RETURN, 0) && ctlSub == 0);
  assert(player_leg_estilo()->corBorda == -1);
  assert(estiloTecla(SDLK_RETURN, 0) && ctl == -1);
  player_dir(argv[1]); player_leg_estilo_carregar();
  assert(player_leg_estilo()->corBorda == 0);
  // OK on the inherited Yellow explicitly stores Yellow too.
  valor[AJ_LEG_CONTORNO_COR] = 3;
  player_leg_estilo()->corBorda = -1;
  assert(estiloTecla(SDLK_RETURN, 0) && ctlSub == 2);
  assert(estiloTecla(SDLK_RETURN, 0) && ctl == -1);
  player_dir(argv[1]); player_leg_estilo_carregar();
  assert(player_leg_estilo()->corBorda == 2);
  // Existing explicit player colours also survive upgrade.
  f = fopen(path, "w"); assert(f);
  fprintf(f, "leg_cor_borda 0\nleg_tocado %d\n", PLR_LEG_COR_BORDA);
  assert(fclose(f) == 0);
  player_dir(argv[1]); player_leg_estilo_carregar();
  assert(ajustes_leg_borda_rgb(player_leg_estilo()->corBorda) == 0);
  puts("legenda_borda_preta: PASS");
  return 0;
}
