// Exercise the real webOS guard with isolated disk state and Settings writes.
#include "video.h"
#include "ajustes.h"
#include "dados.h"
#include "ilha.h"
#include "dts/dts_playback.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
static int notices;
static int enabled(void) { return 1; }
static void notice(const char *key, int type, const char *icon, const char *text,
                   unsigned ms, int button) {
  (void)key; (void)type; (void)icon; (void)ms; (void)button;
  assert(strstr(text, "Dolby Vision"));
  notices++;
}
#define dts_playback_enabled enabled
#define ilha_avisar notice
#undef __APPLE__
#include "../src/video.c"
void dvguard_toggle(int on);

int main(int argc, char **argv) {
  (void)argv;
  const char *dir = getenv("NUVIO_DADOS");
  dados_iniciar(dir);
  ajustes_dir(dir);
  dvguard_toggle(1);
  if (argc > 1) {
    // Unknown firmware with an export window follows the existing webOS 5 fallback.
    snprintf(expWin, sizeof expWin, "host-window");
    assert(dados_gravar(DTS_MARCA_ARQ, "2\ndev\n"));
    assert(!dtsLiberadoNestaTv());
    assert(dados_gravar(DTS_MARCA_ARQ, "2\nold-build\n"));
    assert(dtsLiberadoNestaTv());
    dtsMarcarAbertas(2);
    assert(!dtsLiberadoNestaTv());
    dvguard_toggle(0);
    dvguard_toggle(1);
    assert(dtsLiberadoNestaTv());
    puts("dvguard: DTS also retries after version change and Settings toggle");
    return 0;
  }
  assert(dados_gravar(DTS_MARCA_ARQ, "2\ndev\n"));
  assert(!dvLiberadoNestaTv());
  assert(!dvLiberadoNestaTv());
  assert(notices == 1);

  assert(dados_gravar(DTS_MARCA_ARQ, "2\nold-build\n"));
  assert(dvLiberadoNestaTv());
  assert(dtsAbertasSemFechar() == 0);
  // Reading the migrated record again must not discard this build's deaths.
  dtsMarcarAbertas(2);
  assert(!dvLiberadoNestaTv());
  dvguard_toggle(0);
  assert(dtsAbertasSemFechar() == 2);
  dvguard_toggle(1);
  assert(dvLiberadoNestaTv());
  assert(dtsAbertasSemFechar() == 0);
  assert(notices == 1);

  assert(dados_gravar(DTS_MARCA_ARQ, "2\n"));
  assert(dvLiberadoNestaTv());
  dtsMarcarAbertas(1);
  assert(dvLiberadoNestaTv());
  dtsMarcarAbertas(2);
  assert(!dvLiberadoNestaTv());
  dtsMarcarAbertas(0);
  assert(dvLiberadoNestaTv());
  puts("dvguard: same build blocks; new/legacy build and Settings retry unlock; notice once");
  return 0;
}
