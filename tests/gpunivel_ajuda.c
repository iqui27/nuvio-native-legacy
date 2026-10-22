// #410B: ajuda real de Ajustes, inclusive dependencias e as 29 traducoes.
#include "../src/ajustes.c"
#include <assert.h>

void gpun_teste_reiniciar(void);

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
  for (int p = 1; p <= 2; p++) {
    gpun_preferencia(p);
    for (unsigned i = 0; i < sizeof ops / sizeof *ops; i++)
      assert(!strcmp(ajudaOpcao(ops[i]), ajudaOpcaoBase(ops[i])));
  }
  gpun_teste_reiniciar();
  assert(!strcmp(ajudaOpcao(AJ_VIDRO), ajudaOpcaoBase(AJ_VIDRO)));
  puts("gpunivel_ajuda: 13 opcoes, 30 idiomas, minimos/leves automaticos e manual ok");
  return 0;
}
