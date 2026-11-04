// #410B: ajuda real de Ajustes, inclusive dependencias e as 29 traducoes.
#include "../src/ajustes.c"
#include <assert.h>

void gpun_teste_reiniciar(void);
const char *gpun_teste_upscale(int sw, int sh, int iw, int ih, int app);

int main(void) {
  static const int ops[] = {AJ_VIDRO, AJ_VIDRO_CONTORNO, AJ_VIDRO_OPAC,
    AJ_VIDRO_FOSCO, AJ_PROF, AJ_PROF_BORDA, AJ_PROF_BRILHO, AJ_PROF_COBERTURA,
    AJ_PROF_POSTERS, AJ_PROF_CW, AJ_PROF_EPS, AJ_PROF_ELENCO, AJ_PROF_TRAILERS};
  memcpy(valor, valorPadrao, sizeof valor);
  gpun_teste_reiniciar();
  for (int idioma = 0; idioma < IDIOMA_N; idioma++) {
    valor[AJ_IDIOMA] = idioma + 1; // 0 e Automatico
    assert(ajustes_idioma() == idioma);
    for (int n = 1; n <= 2; n++) {
      gpun_definir_nivel(n);
      const char *aviso = i18n(n == 2
        ? "Efeitos visuais está em mínimos nesta TV; a opção tem pouco efeito. Ajuste em Esta TV › Efeitos visuais."
        : "Efeitos visuais está em leves nesta TV; a opção tem pouco efeito. Ajuste em Esta TV › Efeitos visuais.");
      if (idioma != IDIOMA_PT) assert(strncmp(aviso, "Efeitos visuais está", 19));
      for (int off = 0; off <= 1; off++) {
        valor[AJ_VIDRO] = valor[AJ_PROF] = off;
        for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++) {
          const char *s = ajudaOpcao(ops[i]);
          assert(!strncmp(s, aviso, strlen(aviso)));
          assert(strstr(s, i18n(ajudaOpcaoBase(ops[i]))));
        }
      }
      assert(!strcmp(ajudaOpcao(AJ_IDIOMA), ajudaOpcaoBase(AJ_IDIOMA)));
    }
  }
  for (int p = 1; p <= 3; p++) {
    gpun_preferencia(p);
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++)
      assert(!strcmp(ajudaOpcao(ops[i]), ajudaOpcaoBase(ops[i])));
  }
  gpun_teste_reiniciar();
  assert(!strcmp(ajudaOpcao(AJ_VIDRO), ajudaOpcaoBase(AJ_VIDRO)));
  // Rungs 1-2 keep level 0 but are real reductions: Settings must say so.
  memcpy(valor, valorPadrao, sizeof valor);
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  gpun_teste_reiniciar();
  gpun_relatar(60, 40, 1000.0 / 60, 1, 1);
  for (int rung = 1; rung <= 2; rung++) {
    for (int i = 0; i < 3; i++) gpun_relatar(30, 40, 1000.0 / 60, 1, 1);
    if (rung == 1) {
      assert(gpun_nivel() == 0 && gpun_efeito_ativo(1) && ajustes_vidro_qualidade() == 3);
      assert(strcmp(ajudaOpcao(AJ_VIDRO), ajudaOpcaoBase(AJ_VIDRO)));
    }
    for (int i = 0; i < 3; i++) gpun_relatar(34, 40, 1000.0 / 60, 1, 1);
  }
  // Recovery leaves only the frost mark: reduced under Frost Automatic, not Frost On.
  for (int i = 0; i < 20; i++) gpun_relatar(60, 40, 1000.0 / 60, 1, 1);
  assert(gpun_fosco_vetado() && gpun_degrau() == 0 && gpun_nivel() == 0);
  assert(strcmp(ajudaOpcao(AJ_VIDRO), ajudaOpcaoBase(AJ_VIDRO)));
  valor[AJ_VIDRO_FOSCO] = 1;
  assert(!strcmp(ajudaOpcao(AJ_VIDRO), ajudaOpcaoBase(AJ_VIDRO)));
  // Automatic resolution says what it really draws, in every language.
  memcpy(valor, valorPadrao, sizeof valor);
  assert(!strstr(uxBeneficio(AJ_RESOLUCAO), "1080p")); // It may start at 4K.
  ajustes_ui_saida(1920, 1080);
  for (int idioma = 0; idioma < IDIOMA_N; idioma++) {
    char atual[64];
    valor[AJ_IDIOMA] = idioma + 1;
    snprintf(atual, sizeof atual, i18n("Atual: %s"), "1080p");
    assert(strstr(i18n("Atual: %s"), "%s"));
    assert(strstr(ajudaOpcao(AJ_RESOLUCAO), atual));
    assert(strstr(ajudaOpcao(AJ_RESOLUCAO), i18n(ajudaOpcaoBase(AJ_RESOLUCAO))));
  }
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;
  assert(strstr(ajudaOpcao(AJ_RESOLUCAO), "Atual: 1080p"));
  ajustes_ui_saida(1280, 720);
  assert(strstr(ajudaOpcao(AJ_RESOLUCAO), "Atual: 720p"));
  gpun_teste_upscale(3840, 2160, 3840, 2160, 0); // The 4K surface, no GL.
  ajustes_ui_saida(3840, 2160);
  assert(strstr(ajudaOpcao(AJ_RESOLUCAO), "Atual: 4K"));
  gpun_alvo_1080(); // Mid-session fallback keeps the 4K surface but draws 1080p.
  assert(strstr(ajudaOpcao(AJ_RESOLUCAO), "Atual: 1080p"));
  valor[AJ_RESOLUCAO] = RES_1080; // Manual: the row already says it.
  assert(!strcmp(ajudaOpcao(AJ_RESOLUCAO), ajudaOpcaoBase(AJ_RESOLUCAO)));
  puts("gpunivel_ajuda: 13 opcoes, 30 idiomas, minimos/leves automaticos e manual ok");
  return 0;
}
