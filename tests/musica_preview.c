// Exercise actual UI ownership with a fake shared platform backend.
#include <assert.h>
#include <string.h>
#include "../src/musicaui.c"
static int retained, mainOpen, active, trailer, blocked, starts, stops, vol=100;
static int failed, ended, ready, startOk=1;
static double seconds;
static Uint32 tick=100;
Uint32 SDL_GetTicks(void){return tick;}
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
void video_definir_cabecalhos(const char *s){assert(!s[0]);}
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
 puts("music preview ownership: ok");return 0;
}
