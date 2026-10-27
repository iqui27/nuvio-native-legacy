// Exercise actual UI ownership with a fake shared platform backend.
#include <assert.h>
#include <string.h>
#include "../src/musicaui.c"
static int retained, mainOpen, active, trailer, blocked, starts, stops, vol=100;
static int failed, ended, ready, startOk=1;
static int refreshes, sameUrl, apiFails;
char *rede_baixar(const char *url, int timeout) {
 assert(!strcmp(url,"https://api.deezer.com/album/42") && timeout==10);
 refreshes++;
 if(apiFails) return NULL;
 char json[300];
 snprintf(json,sizeof json,"{\"tracks\":{\"data\":[{\"title\":\"Track\",\"preview\":\"https://cdnt-preview.dzcdn.net/fresh.mp3?hdnea=exp=4102444800~hmac=%d\"}]}}",sameUrl ? 1 : refreshes);
 return strdup(json);
}
static double seconds;
static Uint32 tick=100;
Uint32 SDL_GetTicks(void){return tick;}
void SDL_Delay(Uint32 ms){struct timespec t={0,ms*1000000L};nanosleep(&t,NULL);}
int player_aberto(void){return mainOpen;}
int player_retido(void){return retained;}
int player_mini_ativo(void){return 0;}
int trailer_aberto(void){return trailer;}
int trailer_com_som(void){return 0;}
void trailer_fechar(void){trailer=0;active=0;}
void trailer_bloquear_musica(int sim){blocked=sim;}
int video_ativo(void){return active;}
void video_parar(void){active=0;stops++;}
void video_volume(int v){vol=v;}
void video_definir_cabecalhos(const char *s){assert(!s[0] || (strstr(s,"User-Agent: Mozilla/5.0") && strstr(s,"Referer: https://www.deezer.com/")));}
void video_definir_mp4(int v){assert(v==1);}
void video_definir_modo_live(int v){assert(v==2);}
int video_tocar(const char *s){assert(!strncmp(s,"https://",8));starts++;active=startOk;return startOk;}
double video_pos(void){return seconds;}
int video_pronto(void){return ready;}
int video_tocando(void){return ready;}
int video_falhou(void){return failed;}
int video_terminou(void){return ended;}
int tex_cores(const char *s,unsigned char *out){(void)s;(void)out;return 0;}
int ajustes_animacoes_reduzidas(void){return 0;}
void gfx_tex_esquecer(GLuint t){(void)t;}
int qr_gerar(Qr *q,const char *s){(void)q;(void)s;return 0;}
int mus_guardado(const char *s,int n){(void)s;(void)n;return 0;}
int mus_alternar_guardado(const char *s,int n,const char *a,const char *b){(void)s;(void)n;(void)a;(void)b;return 1;}
void mus_url_celular(char *s,unsigned n,const char *id,const char *t,int y,int season){(void)n;(void)id;(void)t;(void)y;(void)season;s[0]=0;}
static void key(SDL_Keycode k){SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;musicaui_evento(&e);}
int main(void){
 static MusAlbum a; a.n=14;for(int i=0;i<a.n;i++)strcpy(a.faixa[i].previa,"https://preview/audio.m4a");
 musicaui_abrir(&a,"tt1","Film",2008,0);assert(blocked&&ui.suporta);
 retained=1;key(SDLK_RETURN);assert(!ui.p.ativa&&starts==0&&stops==0);
 retained=0;trailer=active=1;key(SDLK_RETURN);assert(ui.p.ativa&&!trailer&&starts==0);
 musicaui_atualizar(++tick);assert(starts==1&&active&&vol==100);
 ready=1;seconds=1;musicaui_atualizar(++tick);
 key(SDLK_ESCAPE);assert(ui.aberta&&!ui.p.ativa&&!active&&vol==0&&blocked);
 key(SDLK_RETURN);musicaui_atualizar(++tick);assert(active);
 key(SDLK_RETURN);assert(!active&&ui.aberta&&vol==0);
 ui.foco=1;key(SDLK_RETURN);musicaui_atualizar(++tick);ready=1;seconds=30;
 musicaui_atualizar(++tick);assert(ui.p.ativa&&ui.p.faixa==1&&!active);
 seconds=0;musicaui_atualizar(++tick);assert(active);
 ui.foco=4;failed=1;musicaui_atualizar(++tick);assert(!active&&!ui.p.ativa&&ui.foco==1);failed=0;
 ui.foco=5;key(SDLK_RETURN);musicaui_atualizar(++tick);ended=1;
 musicaui_atualizar(++tick);assert(!active&&!ui.p.ativa);ended=0;
 ui.foco=1;startOk=0;key(SDLK_RETURN);musicaui_atualizar(++tick);assert(!active&&!ui.p.ativa);startOk=1;
 ui.foco=1;key(SDLK_RETURN);musicaui_atualizar(++tick);musicaui_fechar();assert(!active&&!blocked);
 musicaui_abrir(&a,"tt1","Film",2008,0);ui.foco=1;key(SDLK_RETURN);musicaui_atualizar(++tick);
 musicaui_abrir(&a,"tt2","Other",2009,0);assert(!active&&!ui.p.ativa);
 key(SDLK_DOWN);for(int i=0;i<7;i++)key(SDLK_DOWN);assert(pagina()==1);
 musicaui_fechar();
 strcpy(a.deezer,"https://www.deezer.com/album/42");
 strcpy(a.faixa[0].titulo,"Track");
 strcpy(a.faixa[0].previa,"https://cdnt-preview.dzcdn.net/dead.mp3?hdnea=exp=1");
 a.n=1; musicaui_abrir(&a,"tt1","Film",2008,0);
 key(SDLK_RETURN);
 for(int i=0;i<1000&&!active;i++){ musicaui_atualizar(++tick); SDL_Delay(1); }
 assert(active && refreshes==1 && strstr(ui.a.faixa[0].previa,"/fresh.mp3"));
 failed=1; musicaui_atualizar(++tick); failed=0;
 for(int i=0;i<1000&&!active;i++){ musicaui_atualizar(++tick); SDL_Delay(1); }
 assert(active && refreshes==2);
 failed=1; musicaui_atualizar(++tick);
 assert(!active&&!ui.p.ativa&&ui.indisponivel[0]&&refreshes==2);
 musicaui_fechar(); failed=0; refreshes=0; sameUrl=1;
 musicaui_abrir(&a,"tt1","Film",2008,0);key(SDLK_RETURN);
 for(int i=0;i<1000&&!active;i++){ musicaui_atualizar(++tick); SDL_Delay(1); }
 assert(active&&refreshes==1);
 failed=1;musicaui_atualizar(++tick);failed=0;
 for(int i=0;i<1000&&ui.p.ativa;i++){ musicaui_atualizar(++tick); SDL_Delay(1); }
 assert(!active&&!ui.p.ativa&&ui.indisponivel[0]&&refreshes==2);
 musicaui_fechar();apiFails=1;
 musicaui_abrir(&a,"tt1","Film",2008,0);key(SDLK_RETURN);
 int before=starts;
 for(int i=0;i<1000&&ui.p.ativa;i++){ musicaui_atualizar(++tick); SDL_Delay(1); }
 assert(starts==before&&!ui.p.ativa&&ui.indisponivel[0]);
 musicaui_fechar();
 puts("music preview ownership and Deezer refresh: ok");return 0;
}
