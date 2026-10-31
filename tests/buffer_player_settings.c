#include "ajustes.h"
#define NV_WEBOS 1
#include "../src/ajustes.c"
#include <assert.h>
#include <unistd.h>
// Platform notification boundary; the host fixture does not query a TV.
void avisos_tv_linha(char *line,size_t cap,char *model,size_t model_cap) {
  if(cap)line[0]=0;if(model_cap)model[0]=0;
}
static void key(SDL_Keycode k) { assert(uxEvento(k)); }
static void open_buffer(void) { uxAbrirEditor(AJ_BUFFER);assert(uxEditor==1); }
static void switch_row(void) {
  for(int i=uxPendente;i<nValores(AJ_BUFFER)-1;i++)key(SDLK_DOWN);
  key(SDLK_DOWN);assert(uxBufferDadosFoco);
}
int main(void) {
  char dir[]="/tmp/nuvio-buffer-settings-XXXXXX";assert(mkdtemp(dir));
  assert(SDL_Init(SDL_INIT_TIMER)==0);setenv("NUVIO_DADOS",dir,1);
  dados_iniciar(dir);snprintf(dirAjustes,sizeof dirAjustes,"%s",dir);ajustes_iniciar();
  assert(gravar());assert(!ajustes_buffer_dados());
  assert(ajustes_buffer_native_connections()==1 && ajustes_buffer_conversion_connections()==4);
  assert(ajustes_escopo(AJ_BUFFER_NATIVE_CONNECTIONS)==ESC_TV && ajustes_escopo(AJ_BUFFER_CONVERSION_CONNECTIONS)==ESC_TV);
  assert(!ajustePortavel(AJ_BUFFER_NATIVE_CONNECTIONS) && !ajusteAparelho(AJ_BUFFER_NATIVE_CONNECTIONS));
  for(int native=1;native<=4;native++)for(int conversion=1;conversion<=4;conversion++) {
    assert(definirValorDireto(AJ_BUFFER_NATIVE_CONNECTIONS,native-1));
    assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,conversion-1));
    ajustes_dir(dir);
    assert(ajustes_buffer_native_connections()==native && ajustes_buffer_conversion_connections()==conversion);
  }
  assert(definirValorDireto(AJ_BUFFER_NATIVE_CONNECTIONS,0));
  assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,3));

  assert(AJ_BUFFER_DADOS==AJ_MP4_BD_TEST-3 && ajustes_escopo(AJ_BUFFER_DADOS)==ESC_TV);
  assert(!ajustePortavel(AJ_BUFFER_DADOS) && !ajusteAparelho(AJ_BUFFER_DADOS));
  assert(!ajustes_ram_overlay() && !ajustes_ram_verde());
  assert(ajustes_escopo(AJ_RAM_OVERLAY)==ESC_TV && ajustes_escopo(AJ_RAM_VERDE)==ESC_TV);
  assert(!ajustePortavel(AJ_RAM_OVERLAY) && !ajustePortavel(AJ_RAM_VERDE));
  assert(!ajusteAparelho(AJ_RAM_OVERLAY) && !ajusteAparelho(AJ_RAM_VERDE));
  // Independent persistent switches on the second row, both initially OFF.
  open_buffer();switch_row();key(SDLK_DOWN);assert(uxBufferDadosFoco==2);
  key(SDLK_RETURN);assert(ajustes_ram_overlay() && !ajustes_ram_verde());
  key(SDLK_LEFT);assert(uxBufferDadosFoco==3);key(SDLK_RETURN);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  key(SDLK_ESCAPE);valor[AJ_RAM_OVERLAY]=valor[AJ_RAM_VERDE]=1;ajustes_dir(dir);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  open_buffer();ajPontRamAlternar(3,0);assert(!ajustes_ram_verde() && ajustes_ram_overlay());
  ajPontRamAlternar(2,0);assert(!ajustes_ram_overlay());key(SDLK_ESCAPE);
  int before=ajustes_buffer_preset();
  open_buffer();switch_row();key(SDLK_RETURN);
  assert(uxEditor && uxPendente==before && uxBufferDados==0 && ajustes_buffer_dados());
  // ON is already persisted before leaving the editor, without Confirm.
  key(SDLK_ESCAPE);assert(!uxEditor && ajustes_buffer_dados() && ajustes_buffer_preset()==before);
  valor[AJ_BUFFER_DADOS]=1;ajustes_dir(dir);assert(ajustes_buffer_dados());
  // Mouse OFF also saves immediately; canceling a size change keeps OFF.
  open_buffer();ajPontBufferAlternar(0,0);ajPontEdValor(2,0);
  assert(!uxBufferDadosFoco && uxPendente==2 && !ajustes_buffer_dados());
  switch_row();key(SDLK_DOWN);assert(uxBufferDadosFoco==2);
  key(SDLK_DOWN);assert(uxRodape==1 && !uxBufferDadosFoco);
  key(SDLK_UP);assert(uxBufferDadosFoco==2 && !uxRodape);
  key(SDLK_ESCAPE);
  assert(!uxEditor && !ajustes_buffer_dados() && ajustes_buffer_preset()==before);
  valor[AJ_BUFFER_DADOS]=0;ajustes_dir(dir);assert(!ajustes_buffer_dados());
  // Left/right work immediately, and confirming a size retains the toggle.
  open_buffer();switch_row();key(SDLK_LEFT);assert(ajustes_buffer_dados());
  key(SDLK_RIGHT);assert(!ajustes_buffer_dados());
  key(SDLK_RETURN);assert(ajustes_buffer_dados());
  ajPontEdValor(2,0);key(SDLK_RETURN);
  assert(!uxEditor && ajustes_buffer_dados() && ajustes_buffer_preset()==2);
  // One confirmation restores both the size and the diagnostic default.
  open_buffer();ajPontRamAlternar(2,0);ajPontRamAlternar(3,0);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  uxRestaurar=1;uxAplicar();
  assert(!uxEditor && !ajustes_buffer_dados() && !ajustes_ram_overlay() && !ajustes_ram_verde() && ajustes_buffer_preset()==0);
  // Fixed audio preference uses the actual UI save/reload path, not just a getter.
  uxAbrirEditor(AJ_DV_AUDIO_FIXO);assert(uxEditor==1);
  uxPendente=1;uxAplicar();assert(ajustes_dv_audio_fixo()==1);
  valor[AJ_DV_AUDIO_FIXO]=0;ajustes_dir(dir);assert(ajustes_dv_audio_fixo()==1);
#ifdef NV_MP4_BD_TEST
  assert(!ajustes_mp4_bd_test());
  uxAbrirEditor(AJ_MP4_BD_TEST);assert(uxEditor==1);
  uxPendente=0;uxAplicar();assert(ajustes_mp4_bd_test());
  valor[AJ_MP4_BD_TEST]=1;ajustes_dir(dir);assert(ajustes_mp4_bd_test());
#endif
  // A failed immediate save restores the visible and live toggle state.
  open_buffer();switch_row();
  char saved[sizeof dirAjustes];memcpy(saved,dirAjustes,sizeof saved);
  snprintf(dirAjustes,sizeof dirAjustes,"/dev/null/not-a-directory");key(SDLK_RETURN);
  assert(uxEditor && uxBufferDados==1 && !ajustes_buffer_dados() && ajustes_buffer_preset()==0);
  ajPontRamAlternar(2,0);ajPontRamAlternar(3,0);
  assert(!ajustes_ram_overlay() && !ajustes_ram_verde());
  assert(!ajustes_ram_overlay_alternar() && !ajustes_ram_overlay());
  ajPontBufferDados(0,0);
  // Failing to save a size cannot affect an already saved ON toggle.
  memcpy(dirAjustes,saved,sizeof saved);key(SDLK_RETURN);assert(ajustes_buffer_dados());
  snprintf(dirAjustes,sizeof dirAjustes,"/dev/null/not-a-directory");uxPendente=3;uxAplicar();
  assert(uxEditor && ajustes_buffer_dados() && ajustes_buffer_preset()==0);
  memcpy(dirAjustes,saved,sizeof saved);uxCancelar();
  valor[AJ_BUFFER_DADOS]=1;ajustes_dir(dir);assert(ajustes_buffer_dados());
  SDL_Quit();
  puts("PASS buffer data chooser: immediate remote/mouse ON/OFF, persistence without Confirm, independent size/cancel, restore and write failure rollback");
  return 0;
}
