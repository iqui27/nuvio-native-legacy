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
  key(SDLK_DOWN);assert(uxBufferDadosFoco==4);
  key(SDLK_DOWN);assert(uxBufferDadosFoco==5);
  key(SDLK_DOWN);assert(uxBufferDadosFoco==1);
}
int main(void) {
  char dir[]="/tmp/nuvio-buffer-settings-XXXXXX";assert(mkdtemp(dir));
  assert(SDL_Init(SDL_INIT_TIMER)==0);setenv("NUVIO_DADOS",dir,1);
  dados_iniciar(dir);snprintf(dirAjustes,sizeof dirAjustes,"%s",dir);ajustes_iniciar();
  assert(gravar());assert(!ajustes_buffer_dados());
  assert(ajustes_buffer_native_connections()==1 && ajustes_buffer_conversion_connections()==4);
  assert(!uxDiferente(AJ_BUFFER));
  assert(ajustes_escopo(AJ_BUFFER_NATIVE_CONNECTIONS)==ESC_TV && ajustes_escopo(AJ_BUFFER_CONVERSION_CONNECTIONS)==ESC_TV);
  assert(!ajustePortavel(AJ_BUFFER_NATIVE_CONNECTIONS) && !ajusteAparelho(AJ_BUFFER_NATIVE_CONNECTIONS));
  for(int native=1;native<=4;native++)for(int conversion=1;conversion<=4;conversion++) {
    assert(definirValorDireto(AJ_BUFFER_NATIVE_CONNECTIONS,native-1));
    assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,conversion-1));
    ajustes_dir(dir);
    assert(ajustes_buffer_native_connections()==native && ajustes_buffer_conversion_connections()==conversion);
  }
  /* "Standard (1) - Uses method from Tizen": one continuous request. */
  assert(!ajustes_buffer_conversion_stream());
  assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,0));
  ajustes_dir(dir);
  assert(ajustes_buffer_conversion_stream() && ajustes_buffer_conversion_connections()==1);
  assert(definirValorDireto(AJ_BUFFER_NATIVE_CONNECTIONS,0));
  assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,1));
  assert(!ajustes_buffer_conversion_stream());
  // Both selectors belong to Buffer size, with no duplicate category rows.
  for(unsigned i=0;i<sizeof TELA/sizeof *TELA;i++)
    assert(TELA[i].tipo!=IT_OPC || !uxBufferConexao(TELA[i].op));
  for(int row=4;row<=5;row++)for(int choice=0;choice<4;choice++) {
    int op=row==4?AJ_BUFFER_NATIVE_CONNECTIONS:AJ_BUFFER_CONVERSION_CONNECTIONS;
    open_buffer();ajPontEdValor(2,0);ajPontBufferConexaoAbrir(row,0);
    assert(uxBufferPai.ativo && uxOp==op && uxBufferPai.pendente==2);
    while(uxPendente<choice)key(SDLK_DOWN);
    while(uxPendente>choice)key(SDLK_UP);
    key(SDLK_RETURN);
    assert(uxEditor==1 && uxOp==AJ_BUFFER && uxBufferDadosFoco==row && uxPendente==2 && !uxBufferPai.ativo);
    assert(valor[op]==choice && ajustes_buffer_preset()==0);
    assert(uxDiferente(AJ_BUFFER)==(ajustes_buffer_native_connections()!=1 || ajustes_buffer_conversion_connections()!=2));
    key(SDLK_ESCAPE);ajustes_dir(dir);assert(valor[op]==choice);
  }
  // Canceling a child preserves its saved value and the parent's size draft.
  open_buffer();ajPontEdValor(1,0);ajPontBufferConexaoAbrir(4,0);
  key(SDLK_UP);key(SDLK_ESCAPE);
  assert(uxOp==AJ_BUFFER && uxPendente==1 && uxBufferDadosFoco==4 && ajustes_buffer_native_connections()==4);
  key(SDLK_UP);assert(!uxBufferDadosFoco && uxPendente==1);
  key(SDLK_ESCAPE);assert(!uxEditor && !uxBufferPai.ativo && ajustes_buffer_preset()==0);
  // A failed child save remains on the child. Back returns without saving it.
  open_buffer();ajPontEdValor(2,0);ajPontBufferConexaoAbrir(5,0);key(SDLK_UP);
  char goodDir[sizeof dirAjustes];memcpy(goodDir,dirAjustes,sizeof goodDir);
  snprintf(dirAjustes,sizeof dirAjustes,"/dev/null/not-a-directory");key(SDLK_RETURN);
  assert(uxBufferPai.ativo && uxOp==AJ_BUFFER_CONVERSION_CONNECTIONS && ajustes_buffer_conversion_connections()==4);
  memcpy(dirAjustes,goodDir,sizeof goodDir);key(SDLK_ESCAPE);
  assert(uxOp==AJ_BUFFER && uxPendente==2 && uxBufferDadosFoco==5);
  // Restore on a child restores only its own connections, then returns.
  ajPontBufferConexaoAbrir(4,0);key(SDLK_RIGHT);key(SDLK_RETURN);key(SDLK_LEFT);key(SDLK_RETURN);
  assert(uxOp==AJ_BUFFER && uxBufferDadosFoco==4 && uxPendente==2);
  assert(ajustes_buffer_native_connections()==1 && ajustes_buffer_conversion_connections()==4);
  ajPontBufferConexaoAbrir(5,0);key(SDLK_RIGHT);key(SDLK_RETURN);key(SDLK_LEFT);key(SDLK_RETURN);
  assert(uxOp==AJ_BUFFER && uxBufferDadosFoco==5 && uxPendente==2);
  assert(ajustes_buffer_native_connections()==1 && ajustes_buffer_conversion_connections()==2);
  key(SDLK_ESCAPE);
  assert(!uxDiferente(AJ_BUFFER));

  assert(AJ_BUFFER_DADOS==AJ_MP4_BD_TEST-3 && ajustes_escopo(AJ_BUFFER_DADOS)==ESC_TV);
  assert(!ajustePortavel(AJ_BUFFER_DADOS) && !ajusteAparelho(AJ_BUFFER_DADOS));
  assert(!ajustes_ram_overlay() && !ajustes_ram_verde());
  assert(ajustes_escopo(AJ_RAM_OVERLAY)==ESC_TV && ajustes_escopo(AJ_RAM_VERDE)==ESC_TV);
  assert(!ajustePortavel(AJ_RAM_OVERLAY) && !ajustePortavel(AJ_RAM_VERDE));
  assert(!ajusteAparelho(AJ_RAM_OVERLAY) && !ajusteAparelho(AJ_RAM_VERDE));
  // Independent persistent switches share one row, navigated left/right.
  open_buffer();switch_row();key(SDLK_RIGHT);assert(uxBufferDadosFoco==2);
  key(SDLK_RETURN);assert(ajustes_ram_overlay() && !ajustes_ram_verde());
  key(SDLK_RIGHT);assert(uxBufferDadosFoco==3);key(SDLK_RETURN);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  key(SDLK_ESCAPE);valor[AJ_RAM_OVERLAY]=valor[AJ_RAM_VERDE]=1;ajustes_dir(dir);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  open_buffer();ajPontRamAlternar(3,0);assert(!ajustes_ram_verde() && ajustes_ram_overlay());
  ajPontRamAlternar(2,0);assert(!ajustes_ram_overlay());key(SDLK_ESCAPE);
  int before=ajustes_buffer_preset();
  open_buffer();switch_row();key(SDLK_RETURN);
  assert(uxEditor && uxPendente==3 && uxBufferDados==0 && ajustes_buffer_dados());
  // ON is already persisted before leaving the editor, without Confirm.
  key(SDLK_ESCAPE);assert(!uxEditor && ajustes_buffer_dados() && ajustes_buffer_preset()==before);
  valor[AJ_BUFFER_DADOS]=1;ajustes_dir(dir);assert(ajustes_buffer_dados());
  // Mouse OFF also saves immediately; canceling a size change keeps OFF.
  open_buffer();ajPontBufferAlternar(0,0);ajPontEdValor(2,0);
  assert(!uxBufferDadosFoco && uxPendente==2 && !ajustes_buffer_dados());
  switch_row();key(SDLK_RIGHT);assert(uxBufferDadosFoco==2);
  key(SDLK_DOWN);assert(uxRodape==1 && !uxBufferDadosFoco);
  key(SDLK_UP);assert(uxBufferDadosFoco==1 && !uxRodape);
  key(SDLK_ESCAPE);
  assert(!uxEditor && !ajustes_buffer_dados() && ajustes_buffer_preset()==before);
  valor[AJ_BUFFER_DADOS]=0;ajustes_dir(dir);assert(!ajustes_buffer_dados());
  // OK toggles immediately; left/right moves between the three switches.
  open_buffer();switch_row();key(SDLK_RETURN);assert(ajustes_buffer_dados());
  key(SDLK_RETURN);assert(!ajustes_buffer_dados());
  key(SDLK_RETURN);assert(ajustes_buffer_dados());
  ajPontEdValor(2,0);key(SDLK_RETURN);
  assert(!uxEditor && ajustes_buffer_dados() && ajustes_buffer_preset()==2);
  // One confirmation restores size, connections and diagnostic defaults.
  assert(definirValorDireto(AJ_BUFFER_NATIVE_CONNECTIONS,2));
  assert(definirValorDireto(AJ_BUFFER_CONVERSION_CONNECTIONS,0));
  open_buffer();ajPontRamAlternar(2,0);ajPontRamAlternar(3,0);
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  memcpy(goodDir,dirAjustes,sizeof goodDir);
  snprintf(dirAjustes,sizeof dirAjustes,"/dev/null/not-a-directory");
  uxRestaurar=1;uxAplicar();
  assert(uxEditor && ajustes_buffer_preset()==2 && ajustes_buffer_dados());
  assert(ajustes_ram_overlay() && ajustes_ram_verde());
  assert(ajustes_buffer_native_connections()==3 && ajustes_buffer_conversion_connections()==1);
  memcpy(dirAjustes,goodDir,sizeof goodDir);
  uxRestaurar=1;uxAplicar();
  assert(!uxEditor && !ajustes_buffer_dados() && !ajustes_ram_overlay() && !ajustes_ram_verde() && ajustes_buffer_preset()==0);
  assert(ajustes_buffer_native_connections()==1 && ajustes_buffer_conversion_connections()==2);
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
