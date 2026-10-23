// Library grid must fill the usable width at every text scale and UI size, and
// the source badge must sit on the right edge (owner: other scales leave rows
// half empty). Includes the .c to read the static layout helpers.
#include "../src/biblioteca.c"
#include <assert.h>

int main(void) {
  static const float TXT[] = { 0.85f, 0.9f, 1.0f, 1.1f, 1.2f, 1.3f };
  static const float UI[] = { 0.8f, 0.85f, 0.9f, 1.0f, 1.15f, 1.2f, 1.3f, 1.5f };
  int fails = 0;
  dados_iniciar(getenv("NUVIO_DADOS"));
  for (int t = 0; t < 6; t++)
    for (int u = 0; u < 8; u++) {
      float txt = TXT[t], realW = 1920.0f;
      gfx_escala_txt_definir(txt); gfx_escala_ui_definir(UI[u]);
      { TXT_INI();
        int n = colunasCartaz();
        float ultimo = bibX() + (n - 1) * passoColuna() + BIB_CARD_W;   // right edge of last poster
        float vazio = bibDireita() - ultimo;
        float margemReal = realW - bibDireita() * txt;                  // right margin in real px
        float margemEsq = bibX() * txt;
        { ESCALA_MIN_INI(BIB_TOPO_ESCALA_MIN);
          float selo = hdrDir() * gfx_escala();                         // badge right edge, real px
          int ok = vazio >= 0.0f && vazio < BIB_CARD_W && fabsf(selo - bibDireita() * txt) < 0.5f
                   && fabsf(margemReal - margemEsq) < 0.5f;
          printf("txt=%.2f ui=%.2f cols=%d vazio=%.1f selo=%.1f dir=%.1f margem=%.1f/%.1f %s\n",
                 txt, UI[u], n, vazio, selo, bibDireita() * txt, margemEsq, margemReal, ok ? "ok" : "FAIL");
          fails += !ok;
          ESCALA_MIN_FIM(); }
        TXT_FIM(); }
    }
  if (fails) { printf("FAIL: %d combos\n", fails); return 1; }
  printf("biblioteca_grade_escala: PASS\n");
  return 0;
}
