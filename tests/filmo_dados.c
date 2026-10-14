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
unsigned long long cat_historico_geracao(void){return 1;}
const char *desc_chave_tmdb(void){return "test";}
const char *desc_tmdb_idioma(void){return "en";}
char *rede_baixar(const char *url,int timeout){(void)timeout;char b[500];int id=0;sscanf(strstr(url,"/movie/")+7,"%d",&id);snprintf(b,sizeof b,"{\"imdb_id\":\"tt%d\",\"credits\":{\"cast\":[{\"name\":\"Actor %d\"},{\"name\":\"Other %d\"},{\"name\":\"Third %d\"}]}}",id,id,id,id);return strdup(b);}
int main(int argc,char **argv){
 if(argc>1&&!strcmp(argv[1],"quiz")){pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"title\":\"M1\",\"character\":\"C1\"},{\"id\":2,\"title\":\"M2\",\"character\":\"C2\"},{\"id\":3,\"title\":\"M3\",\"character\":\"C3\"},{\"id\":4,\"title\":\"M4\",\"character\":\"C4\"}]}","2026-01-01");fd_abrir();worker(novo(1,0));fd_atualizar();FmQuiz q;assert(fd_quiz(0,&q));puts("fresh quiz: PASS");return 0;}
 dados_iniciar(NULL);
 vivo=1;
 pessoa_ler(123,"{\"name\":\"A\",\"cast\":[{\"id\":1,\"name\":\"S1\",\"media_type\":\"tv\",\"episode_count\":1},{\"id\":2,\"name\":\"S2\",\"media_type\":\"tv\",\"episode_count\":1}]}","2026-01-01");
 fd_abrir();
 for(int k=0;k<2;k++){snprintf(ficha.c[k].imdb,16,"tt%d",k+1);remota=ficha;remota.c[k].eps=1;paresN=1;pares[0]=(VistoPar){1,1};alvo=k;operacao=1;resultado=1;fd_atualizar();vistoep_definir(ficha.c[k].imdb,1,1,1);fd_atualizar();assert(ficha.st[k].estado==FM_VISTO);}
 vivo=1;fd_atualizar();assert(ficha.st[0].estado==FM_VISTO&&ficha.st[1].estado==FM_VISTO);
 puts("per-title episodes: PASS");return 0;
}
