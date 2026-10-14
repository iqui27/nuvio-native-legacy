#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/filmodados.c"
static int watched[3];
void dados_iniciar(const char *s){(void)s;}
char *dados_ler(const char *s){(void)s;return NULL;}
const char *sessao_usuario(void){return "test";}
int perfis_ativo(void){return 1;}
int ajustes_conquistas(void){return 0;}
int cat_n(void){return 0;}
int cat_indice_por_imdb(const char *s){(void)s;return -1;}
int cat_historico_estado_tempo(const char *s,const char *t,long long *ms){(void)s;(void)t;*ms=0;return -1;}
int vistoep_estado(const char *s,int t,int e){(void)t;(void)e;return watched[atoi(s+2)];}
void vistoep_definir(const char *s,int t,int e,int v){(void)t;(void)e;watched[atoi(s+2)]=v;}
const char *i18n(const char *s){return s;}
int main(void){
 dados_iniciar(NULL);
 pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"name\":\"S1\",\"media_type\":\"tv\",\"episode_count\":1},{\"id\":2,\"name\":\"S2\",\"media_type\":\"tv\",\"episode_count\":1}]}","2026-01-01");
 fd_abrir();
 for(int k=0;k<2;k++){snprintf(ficha.c[k].imdb,16,"tt%d",k+1);remota=ficha;remota.c[k].eps=1;paresN=1;pares[0]=(VistoPar){1,1};alvo=k;operacao=1;resultado=1;fd_atualizar();vistoep_definir(ficha.c[k].imdb,1,1,1);fd_atualizar();assert(ficha.st[k].estado==FM_VISTO);}
 fd_atualizar();assert(ficha.st[0].estado==FM_VISTO&&ficha.st[1].estado==FM_VISTO);
 puts("per-title episodes: PASS");return 0;
}
