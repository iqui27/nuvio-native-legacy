// ESTILO TAB CONTROLS (owner, 11/10): OK on Tamanho/Opacidade/Posicao/Atraso opens
// a slider under the item, Cor opens the swatch palette, Fonte opens the font
// list; Back closes the control before the panel; the panel never changes height.
#include "../src/faixas.c"
#include <assert.h>
static VideoFaixa emb[1];
int video_n_legenda(void) { return 0; }
const VideoFaixa *video_legenda(int i) { return i >= 0 && i < 1 ? &emb[i] : NULL; }
int mkvass_estado(void) { return 0; }
int video_n_audio(void) { return 1; }
int mkvass_varredura(void) { return 0; }
int addons_n_legendas(void) { return 0; }
const Legenda *addons_legenda(int i) { (void)i; return NULL; }
const char *player_id_canal(void) { return ""; }
const char *i18n(const char *s) { return s; }
static VideoLegendaEstilo estilo;
static int tocou, mudou, fonteLocal = -1, camposTocados;
VideoLegendaEstilo *player_leg_estilo(void) { return &estilo; }
void player_leg_estilo_mudou(void) { mudou++; }
void player_leg_estilo_tocou(int m) {
  if (m == PLR_LEG_NADA) camposTocados = 0; else camposTocados |= m;
  if (m == PLR_LEG_COR) tocou++;
}
int player_leg_estilo_tocado(int m) { return (camposTocados & m) != 0; }
unsigned ajustes_leg_borda_rgb(int cor) { return video_legenda_borda_rgb(cor); }
int player_leg_estilo_restaurar(void) {
  estilo = (VideoLegendaEstilo){120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER, 0, -1};
  fonteLocal = -1; camposTocados = 0; mudou++;
  return 1;
}
int legfontes_n(void) { return 2; }
const char *legfontes_nome(int i) { return i == 0 ? "Imported One" : i == 1 ? "Imported Two" : ""; }
int legfontes_selecionada(int slot) { return slot == 0 ? fonteLocal : -1; }
int player_leg_fonte_definir(int familia) { estilo.familia = familia; fonteLocal = -1; player_leg_estilo_tocou(PLR_LEG_FONTE); return 1; }
int player_leg_fonte_local_definir(int indice) { if (indice < 0 || indice >= legfontes_n()) return 0; fonteLocal = indice; player_leg_estilo_tocou(PLR_LEG_FONTE); return 1; }
static int ass;
int assrender_ativo(void) { return ass; }
int assrender_texto_simples(void) { return 0; }

static int tecla(SDL_Keycode k, int rep) { return estiloTecla(k, rep); }
static void irPara(int item) { foco[FX_COL_ESTILO] = item; ctl = -1; }
int main(void) {
  float h0 = alturaEstilo();
  estilo = (VideoLegendaEstilo){ 120, 0, 0, 3, 1, 0, 0, TXT_FAMILIA_INTER, 0, -1 };
  coluna = FX_COL_ESTILO;

  // Tamanho: OK opens the slider; the arrows change the value; Back closes it.
  irPara(0);
  assert(tecla(SDLK_RETURN, 0) == 1 && ctl == 0 && estTipo(ctl) == 1);
  assert(tecla(SDLK_RIGHT, 0) && estilo.tamanho == 130);
  assert(tecla(SDLK_LEFT, 0) && tecla(SDLK_LEFT, 0) && estilo.tamanho == 110);
  assert(tecla(SDLK_RIGHT, 1) && estilo.tamanho == 130);             // holding: bigger step
  for (int i = 0; i < 40; i++) tecla(SDLK_RIGHT, 0);
  assert(estilo.tamanho == 300);                                      // clamps, no wrap
  assert(alturaEstilo() == h0);
  assert(tecla(SDLK_ESCAPE, 0) == 1 && ctl == -1);                    // Back closes the slider...
  assert(tecla(SDLK_ESCAPE, 0) == 0);                                 // ...then the panel

  // Atraso: 0,1 s per tap, 0,5 s while held, +-60 s.
  irPara(7);
  assert(tecla(SDLK_RETURN, 0) && ctl == 7);
  tecla(SDLK_RIGHT, 0); assert(estilo.atrasoMs == 100);
  tecla(SDLK_RIGHT, 1); assert(estilo.atrasoMs == 600);
  for (int i = 0; i < 400; i++) tecla(SDLK_RIGHT, 1);
  assert(estilo.atrasoMs == LEGUI_ATRASO_MAX);
  for (int i = 0; i < 400; i++) tecla(SDLK_LEFT, 1);
  assert(estilo.atrasoMs == -LEGUI_ATRASO_MAX);
  assert(tecla(SDLK_RETURN, 0) && ctl == -1);                         // OK closes too

  // Opacidade: four stops; right = more opaque.
  irPara(3); estilo.opacidade = 3;
  tecla(SDLK_RETURN, 0); assert(ctl == 3);
  tecla(SDLK_RIGHT, 0); assert(estilo.opacidade == 2);
  tecla(SDLK_LEFT, 0); tecla(SDLK_LEFT, 0); tecla(SDLK_LEFT, 0); assert(estilo.opacidade == 3);
  ctl = -1;

  // Posicao: eight levels, slider + miniature.
  irPara(5); estilo.posicao = 3;
  tecla(SDLK_RETURN, 0); assert(ctl == 5);
  tecla(SDLK_RIGHT, 0); assert(estilo.posicao == 4);
  for (int i = 0; i < 20; i++) tecla(SDLK_RIGHT, 0);
  assert(estilo.posicao == 7);
  ctl = -1;

  // Pointer: a click on the track puts the marker under the cursor.
  irPara(0); tecla(SDLK_RETURN, 0);
  estDeFrac(0, 0.0f); assert(estilo.tamanho == 50);
  estDeFrac(0, 1.0f); assert(estilo.tamanho == 300);
  estDeFrac(0, 0.5f); assert(estilo.tamanho == 180);
  ctl = -1;

  // Cor: the palette opens; the arrows apply live and mark "touched"; OK closes.
  irPara(2); tocou = 0;
  tecla(SDLK_RETURN, 0); assert(ctl == 2 && estTipo(ctl) == 2 && ctlSub == 0);
  tecla(SDLK_RIGHT, 0); assert(estilo.cor == 1 && tocou == 1);
  tecla(SDLK_LEFT, 0); tecla(SDLK_LEFT, 0); assert(estilo.cor == VIDEO_LEG_NCORES - 1);
  assert(alturaEstilo() == h0);
  tecla(SDLK_RETURN, 0); assert(ctl == -1 && estilo.cor == VIDEO_LEG_NCORES - 1);

  // Fonte: the list opens, the panel keeps its height, the choice is live.
  irPara(1);
  tecla(SDLK_RETURN, 0); assert(ctl == 1 && estTipo(ctl) == 3 && ctlSub == TXT_FAMILIA_INTER);
  tecla(SDLK_RIGHT, 0); assert(estilo.familia == 1);
  tecla(SDLK_DOWN, 0); assert(estilo.familia == 3);
  tecla(SDLK_UP, 0); tecla(SDLK_LEFT, 0); assert(estilo.familia == 0);
  tecla(SDLK_UP, 0); assert(estilo.familia == 0);
  assert(alturaEstilo() > h0); // imported rows remain fully visible
  tecla(SDLK_BACKSPACE, 0); assert(ctl == -1);

  // The same in-player menu reaches imported faces, applies them, and reopens
  // with the correct check/focus. Picking a bundled face clears the override.
  tecla(SDLK_RETURN, 0); ctlSub = TXT_FAMILIA_N - 2;
  tecla(SDLK_DOWN, 0); assert(fonteLocal == 0 && ctlSub == TXT_FAMILIA_N);
  tecla(SDLK_RETURN, 0); assert(ctl == -1);
  tecla(SDLK_RETURN, 0); assert(ctlSub == TXT_FAMILIA_N);
  estEscolher(TXT_FAMILIA_N + 1); assert(fonteLocal == 1);
  estEscolher(2); assert(fonteLocal == -1 && estilo.familia == 2);
  estilo.familia = 0; ctl = -1;

  // Hover only focuses: nothing is applied until the click.
  tecla(SDLK_RETURN, 0); ponteiroSub(4, 0); assert(ctlSub == 4 && estilo.familia == 0);
  ponteiroEscolher(4, 0); assert(estilo.familia == 4 && ctl == -1);

  // Rows without a control still act on OK; Restaurar resets.
  irPara(8); tecla(SDLK_RETURN, 0); assert(ctl == -1 && estilo.negrito == 1);
  irPara(9); tecla(SDLK_RETURN, 0); assert(estilo.tamanho == 120 && estilo.familia == TXT_FAMILIA_INTER);

  // Authored ASS values stay preserved until the user explicitly changes a
  // field. The controls remain editable, as in the upstream player.
  ass = 1;
  irPara(1); assert(estiloPreservadoAss(1));
  tecla(SDLK_RETURN, 0); assert(ctl == 1);
  tecla(SDLK_RIGHT, 0); assert(!estiloPreservadoAss(1));
  irPara(2); assert(estiloPreservadoAss(2));
  tecla(SDLK_RETURN, 0); assert(ctl == 2);
  tecla(SDLK_RIGHT, 0); assert(!estiloPreservadoAss(2));
  irPara(0); tecla(SDLK_RETURN, 0); assert(ctl == 0);                 // size stays a slider
  puts("estilo controls: sliders, palette and font list open in place ok");
  return 0;
}
