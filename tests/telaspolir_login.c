#include "nav21.h"
#include "nuvem.h"
#define nuvem_pronta() 1
#include "../src/login.c"
#undef nuvem_pronta
void telaspolir_login(void){
  modo=LG_EMAIL;foco=LE_EMAIL;email[0]=senha[0]=0;campoAberto=-1;
}
void telaspolir_login_nav(void){
  const SDL_Keycode keys[]={SDLK_DOWN,SDLK_RIGHT,SDLK_LEFT,SDLK_DOWN,SDLK_DOWN,SDLK_UP,SDLK_UP,SDLK_UP};
  const int expected[]={LE_SENHA,LE_MOSTRAR,LE_SENHA,LE_ENTRAR,LE_QR,LE_ENTRAR,LE_SENHA,LE_EMAIL};
  telaspolir_login();
  for(int i=0;i<8;i++){
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=keys[i];login_evento(&e);
    char path[80];snprintf(path,sizeof path,"Step %d, %s",i+1,SDL_GetKeyName(keys[i]));
    nav21_expect("Login/email",path,foco,expected[i]);
  }
}
