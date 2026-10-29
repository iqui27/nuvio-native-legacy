// Exercise legacy defaults and subsequent explicit choices through real storage.
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>

int main(void) {
  char dir[1024], path[1200];
  snprintf(dir, sizeof dir, "%s/nuvio-guiasom-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  setenv("NUVIO_DADOS", dir, 1); dados_iniciar(dir);
  snprintf(path, sizeof path, "%s/ajustes.txt", dir);
  FILE *f = fopen(path, "w"); assert(f);
  fputs("guiaPreviaSomLocal 0\n", f); fclose(f);
  ajustes_dir(dir);
  assert(!ajustes_guia_previa_sem_som());
  ajustes_dir(dir); assert(!ajustes_guia_previa_sem_som());
  for (int mute = 0; mute <= 1; mute++) {
    ajustes_definir_guia_previa_sem_som(mute);
    ajustes_dir(dir); assert(ajustes_guia_previa_sem_som() == mute);
    ajustes_dir(dir); assert(ajustes_guia_previa_sem_som() == mute);
  }
  // Each old profile migrates independently, even after another profile mutes.
  assert(dados_gravar("ajustes-p41.txt", "guiaPreviaSomLocal 0\n"));
  int v[AJ_N]; char tem[AJ_N];
  memcpy(v, valor, sizeof v);
  assert(perfilLerVetor(41, v, tem));
  assert(tem[AJ_GUIA_PREVIA_SOM] && v[AJ_GUIA_PREVIA_SOM] == 1);
  assert(perfilGravarVetor(41, v));
  v[AJ_GUIA_PREVIA_SOM] = 0;
  assert(perfilGravarVetor(42, v));
  assert(perfilLerVetor(42, v, tem) && v[AJ_GUIA_PREVIA_SOM] == 0);
  assert(perfilLerVetor(41, v, tem) && v[AJ_GUIA_PREVIA_SOM] == 1);
  // Old records cannot override a new explicit choice, in either order.
  for (int order = 0; order < 2; order++) {
    char text[256];
    snprintf(text, sizeof text, "%s 0\n%s 1\n",
      order ? CHAVE[AJ_GUIA_PREVIA_SOM] : "guiaPreviaSomLocal",
      order ? "guiaPreviaSomLocal" : CHAVE[AJ_GUIA_PREVIA_SOM]);
    assert(dados_gravar("ajustes-p43.txt", text));
    assert(perfilLerVetor(43, v, tem));
    assert(v[AJ_GUIA_PREVIA_SOM] == !order);
  }
  puts("PASS: legacy global/profile preview mute migration; explicit choices survive reload");
  return 0;
}
