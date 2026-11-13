// Actual webOS settings implementation; host playback stays simulated.
#define NV_WEBOS
#include "../src/ajustes.c"
#include "nvtmp.h"
#include <assert.h>
#include <unistd.h>
#include "ram-controls.c"

// This platform probe would consult the TV on webOS. Fixture has no TV.
void avisos_tv_linha(char *line, size_t cap, char *model, size_t modelCap) {
  if(cap) line[0]=0;
  if(modelCap) model[0]=0;
}
static void key(SDL_Keycode k) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;ajustes_evento(&e);
}
static void enable(int op, int enabled) {
  focarOpcao(op);
  if (lig(op)!=enabled) key(SDLK_RETURN);
  assert(!uxEditor && lig(op)==enabled);
}
static void reload(const char *dir) { ajustes_dir(dir);ajustes_iniciar(); }
int main(void) {
  char dir[1024], savedDir[1024];
  snprintf(dir,sizeof dir,"%s/nuvio-ram-controls-XXXXXX",nvt_dir());
  assert(mkdtemp(dir));assert(SDL_Init(SDL_INIT_TIMER)==0);
  assert(setenv("NUVIO_DADOS",dir,1)==0);dados_iniciar(dir);player_dir(dir);
  reload(dir);assert(gravar());
  assert(!ajustes_buffer_dados() && !ajustes_ram_overlay() && !ajustes_ram_verde());
  // The conservative reduction policy keeps its existing ON default. Its
  // saved choice is independent of the three opt-in display/shortcut controls.
  assert(ajustes_buffer_ram_reduz());
  enable(AJ_BUFFER_RAM_REDUZ,0);reload(dir);assert(!ajustes_buffer_ram_reduz());
  const int ops[]={AJ_BUFFER_DADOS,AJ_RAM_OVERLAY,AJ_RAM_VERDE};
  for(int i=0;i<3;i++) {
    assert(ajOpcaoDisponivel(ops[i]));assert(ajustes_escopo(ops[i])==ESC_TV);
    enable(ops[i],1);reload(dir);
    for(int j=0;j<3;j++) assert(lig(ops[j])==(j<=i));
  }
  enable(AJ_RAM_OVERLAY,0);reload(dir);
  assert(ajustes_ram_verde() && ajustes_buffer_dados() && !ajustes_ram_overlay());
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=NV_SCANCODE_GREEN;
  assert(bufferRamTecla(&e));reload(dir);assert(ajustes_ram_overlay());
  e.key.repeat=1;assert(!bufferRamTecla(&e) && ajustes_ram_overlay());
  e.key.repeat=0;e.type=SDL_KEYUP;assert(!bufferRamTecla(&e));
  e.type=SDL_KEYDOWN;assert(bufferRamTecla(&e));reload(dir);assert(!ajustes_ram_overlay());
  e.key.keysym.scancode=0;e.key.keysym.sym=SDLK_F2;
  assert(bufferRamTecla(&e));reload(dir);assert(ajustes_ram_overlay());
  enable(AJ_RAM_VERDE,0);reload(dir);
  assert(!bufferRamTecla(&e) && ajustes_ram_overlay() && ajustes_buffer_dados());
  enable(AJ_RAM_VERDE,1);
  snprintf(savedDir,sizeof savedDir,"%s",dirAjustes);
  snprintf(dirAjustes,sizeof dirAjustes,"%s/missing/child",dir);
  assert(bufferRamTecla(&e) && ajustes_ram_overlay()); // Save failure rolls back.
  snprintf(dirAjustes,sizeof dirAjustes,"%s",savedDir);reload(dir);
  assert(ajustes_ram_overlay() && ajustes_ram_verde() && ajustes_buffer_dados());
  enable(AJ_BUFFER_DADOS,0);reload(dir);
  assert(!ajustes_buffer_dados() && ajustes_ram_overlay() && ajustes_ram_verde());
  assert(!ajustes_buffer_ram_reduz());
  enable(AJ_BUFFER_RAM_REDUZ,1);reload(dir);assert(ajustes_buffer_ram_reduz());
  assert(!ajustes_buffer_dados() && ajustes_ram_overlay() && ajustes_ram_verde());
  char path[1400];snprintf(path,sizeof path,"rm -rf '%s'",dir);
  // Fixture files only: created above, no configured data directory is used.
  assert(system(path)==0);SDL_Quit();
  puts("PASS release RAM controls: real callbacks, independent OFF defaults, disk reload, GREEN/F2, repeat/key-up, save rollback");
}
