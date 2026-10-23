// Focused persistence test using ajustes.c and the real JSON parser.
// The argument is an existing, disposable directory. Presentation, media
// languages and unrelated settings stores are inert; profile files use stdio.
// Build with function/data sections and linker garbage collection.
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED

#ifdef _WIN32
static int replaceFile(const char *from, const char *to);
#define rename replaceFile
#endif
#include "../src/ajustes.c"
#ifdef _WIN32
#undef rename
// Windows CRT rename cannot replace a file. Model the POSIX operation used
// by the TV targets, only inside the disposable test directory.
static int replaceFile(const char *from, const char *to) {
  assert(!strncmp(from, dirAjustes, strlen(dirAjustes)));
  assert(!strncmp(to, dirAjustes, strlen(dirAjustes)));
  if (remove(to) && errno != ENOENT) return -1;
  return rename(from, to);
}
#endif

int main(int argc, char **argv) {
  assert(argc == 2 && strlen(argv[1]) < sizeof dirAjustes);
  setenv("NUVIO_DADOS", argv[1], 1);
  dados_iniciar(argv[1]);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", argv[1]);
  assert(!ajustes_pular_creditos() && valorPadrao[AJ_PULAR_CREDITOS] == 1);
  assert(somenteDesteAparelho(AJ_PULAR_CREDITOS) && !dePerfil(AJ_PULAR_CREDITOS));
  assert(!strcmp(uxEscopo(AJ_PULAR_CREDITOS), "Só nesta TV"));
  for (int i = 0; i < AJ_N; i++) assert(strcmp(CHAVE[i], "autoCreditosLocal"));

  valor[AJ_PULAR_CREDITOS] = 0;
  valor[AJ_HERO] = 1;
  ajustes_perfil_guardar(1);
  char *snapshot = dados_ler("ajustes-p1.txt");
  assert(snapshot && !strstr(snapshot, "pularCreditosLocal") && !strstr(snapshot, "autoCreditosLocal"));
  free(snapshot);
  valor[AJ_HERO] = 0;
  ajustes_perfil_guardar(2);
  assert(ajustes_perfil_restaurar(1) && ajustes_pular_creditos());
  assert(valor[AJ_HERO] == 1);
  assert(ajustes_perfil_restaurar(2) && ajustes_pular_creditos());
  assert(valor[AJ_HERO] == 0);
  assert(dados_gravar("ajustes-p4.txt", "heroSectionEnabled 1\npularCreditosLocal 1\nautoCreditosLocal 1\n"));
  assert(ajustes_perfil_restaurar(4) && ajustes_pular_creditos());
  assert(!ajustes_perfil_restaurar(5) && ajustes_pular_creditos());
  assert(gravar());
  snapshot = dados_ler("ajustes.txt");
  assert(snapshot && strstr(snapshot, "pularCreditosLocal 0") && !strstr(snapshot, "autoCreditosLocal"));
  free(snapshot);

  valor[AJ_IDIOMA] = IDIOMA_EN + 1;
  valor[AJ_PAUSA_OVERLAY] = 0;
  valor[AJ_PULAR_CREDITOS] = 1;
  assert(ajustes_aplicar_blob("{\"pularCreditosLocal\":true,\"autoCreditosLocal\":true}") == 0);
  assert(!ajustes_pular_creditos());
  valor[AJ_PULAR_CREDITOS] = 0;
  assert(ajustes_aplicar_blob("{\"pularCreditosLocal\":false,\"autoCreditosLocal\":false}") == 0);
  assert(ajustes_pular_creditos());
  char *merged = NULL;
  assert(ajustes_mesclar_blob("{\"pauseOverlayEnabled\":false}", &merged) > 0);
  assert(merged && strstr(merged, "c_devices_v1") && !strstr(merged, "autoCreditos"));
  assert(somenteDesteAparelho(AJ_PULAR_CREDITOS));
  assert(strstr(merged, "c_settings_v1")); // Versioned C preferences now back up portable settings.
  free(merged);

  ajustes_perfil_esquecer();
  assert(ajustes_pular_creditos() && !dados_ler("ajustes-p1.txt"));
  puts("ajustes_creditos: one device-local preference, default off, persistence and account/profile isolation passed");
  return 0;
}
