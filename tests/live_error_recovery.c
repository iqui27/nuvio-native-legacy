// Exercise the real app recovery action, including Android's unknown buffer end.
#define SDL_MAIN_HANDLED
#include "../src/app.c"
#include <assert.h>
static int aberto=1, mini, erro=1, comVideo=1, falha, pronto=1, tocando=1, limpezas;
static unsigned buffering;
static double fimBuffer;
static const char *canal="nvm3u:@00000001:1234567890123456";
int player_aberto(void){return aberto;}
int player_mini_ativo(void){return mini;}
int player_fonte_falhou(void){return erro;}
int player_tem_video(void){return comVideo;}
const char *player_id_canal(void){return canal;}
int video_falhou(void){return falha;}
int video_pronto(void){return pronto;}
int video_tocando(void){return tocando;}
unsigned video_bufferando_ms(void){return buffering;}
double video_buffer_fim(void){return fimBuffer;}
void player_limpar_erro_fonte(void){erro=0;limpezas++;}
Uint32 SDL_GetTicks(void){return 1234;}
static void verificar(int esperado){
  erro=1;int antes=limpezas;recuperarErroFonte();
  assert(limpezas==antes+esperado && erro==!esperado);
}
int main(void){
  verificar(1);assert(canalFonteDesde==1234); // live resumed, buffer end unavailable
  tocando=0;verificar(0);tocando=1;
  buffering=1000;verificar(0);buffering=0;
  pronto=0;verificar(0);pronto=1; // audio without READY video cannot dismiss it
  falha=1;verificar(0);falha=0;
  comVideo=0;verificar(0);comVideo=1; // unrelated trailer cannot dismiss this session
  aberto=0;verificar(0);mini=1;verificar(1);mini=0;aberto=1;
  canal="";verificar(0); // VOD keeps the existing measured-buffer rule
  fimBuffer=2;verificar(1);
  puts("PASS: playing live video clears stale error with unknown buffer; pause, buffering, failure, audio-only and unrelated sessions stay protected");
}
