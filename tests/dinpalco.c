// Reuse the headless Home fixture; exercise production routing and geometry.
static int testePalcoFundo;
static int teste_palco_fundo(void) { return testePalcoFundo; }
#define ajustes_palco_fundo teste_palco_fundo
#define main home_fixture_main
#include "home_layout.c"
#undef main
#undef ajustes_fundo_din
extern int ajustes_fundo_din(void);

int main(void) {
  assert(ajustes_fundo_din() == 0); // Fresh device: Gray, independent of layout.
  nFileiras = 3;
  fileiras[0] = (Fileira){.tipo = FILEIRA_NORMAL, .n = 6, .escala = 1};
  fileiras[1] = fileiras[0];
  foco.fileira = 2; foco.coluna = 0; focoHero = 0;
  const TipoFileira types[] = {FILEIRA_PALCO, FILEIRA_HEROI_DIVIDIDO};
  for (int t = 0; t < 2; t++) {
    fileiras[2] = (Fileira){.tipo = types[t], .n = 6, .escala = 1};
    for (int hero = 0; hero < 2; hero++) {
      homeCom(HOME_LAYOUT_MODERNA, hero);
      float top = topoFileiras(), cut = corteFileiras(), gap = fileiraGap();
      float width = larguraFil(2), height = alturaTotalFil(2);
      homeCom(HOME_LAYOUT_DINAMICA, hero);
      assert(layoutHome() == HOME_LAYOUT_MODERNA);
      assert(topoFileiras() == top && corteFileiras() == cut && fileiraGap() == gap);
      assert(larguraFil(2) == width && alturaTotalFil(2) == height);
      assert(alfaFileiraHome(1, top - 80, top, cut) == 0);
      for (int mode = 0; mode < 3; mode++) {
        testePalcoFundo = mode;
        assert(palcoFundoDestino() == (types[t] == FILEIRA_PALCO ? mode : 2));
      }
      focoHero = 1;
      assert(layoutHome() == HOME_LAYOUT_DINAMICA && palcoFundoDestino() == 1);
      focoHero = 0;
      homeCom(HOME_LAYOUT_PADRAO, hero);
      assert(layoutHome() == HOME_LAYOUT_PADRAO && palcoFundoDestino() == 1);
    }
  }
  homeCom(HOME_LAYOUT_DINAMICA, 1);
  fileiras[2].tipo = FILEIRA_NORMAL;
  assert(layoutHome() == HOME_LAYOUT_DINAMICA && palcoFundoDestino() == 1);
  puts("dinpalco: Stage/Split use Moderna geometry, hide upper rows; Stage follows setting, Split keeps artwork; Gray default");
  return 0;
}
