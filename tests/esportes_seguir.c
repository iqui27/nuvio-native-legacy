/* Follow persistence uses real disk storage and a second process on restart. */
#define _POSIX_C_SOURCE 200809L
#include "esportes.h"
#include "lembrete.h"
#include "dados.h"
#include "rede.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int profile=1, providerState;
static time_t providerStart;
static int requests;
static uint64_t generation=1;
uint64_t perfis_sessao_geracao(void){return generation;}
int perfis_sessao_atual(uint64_t s){return s==generation;}
int perfis_ativo(void){return profile;}
const char *sessao_usuario(void){return "follow-proof";}
int ajustes_idioma(void){return 1;}
char *rede_baixar_medido_controle(const char *u,int s,const char *const *h,const RedeControle *c,RedeMedida *m){
  (void)s;(void)h;(void)c;
  if(!providerStart){m->status=503;return NULL;}
  assert(strstr(u,"/soccer/bra.1/scoreboard?event=game-1&"));requests++;
  char date[40],json[1400];struct tm tm;gmtime_r(&providerStart,&tm);strftime(date,sizeof date,"%Y-%m-%dT%H:%M:%SZ",&tm);
  snprintf(json,sizeof json,"{\"events\":[{\"id\":\"game-1\",\"name\":\"Updated game\",\"date\":\"%s\",\"status\":{\"type\":{\"state\":\"pre\",\"name\":\"%s\"}},\"venue\":{\"fullName\":\"Updated stadium\"},\"competitors\":[{\"team\":{\"id\":\"fla\",\"displayName\":\"Flamengo\"}},{\"team\":{\"id\":\"pal\",\"displayName\":\"Palmeiras\"}}]}]}",date,providerState?"STATUS_CANCELED":"STATUS_SCHEDULED");
  m->status=200;return strdup(json);
}
static void drain(void){
  for(int i=0;esportes_favoritos_carregando()&&i<5000;i++){
    esportes_passo();struct timespec t={0,1000000};nanosleep(&t,NULL);
  }
  assert(!esportes_favoritos_carregando());
}
void esportes_seguir_lembretes(void);
static EspEvento game(void){
  EspEvento e={0};strcpy(e.id,"game-1");strcpy(e.modalidade,"soccer");strcpy(e.ligaId,"bra.1");
  strcpy(e.liga,"Brasileirão");strcpy(e.local,"Stadium | Main\nGate");
  strcpy(e.participantes[1].nome,"Palmeiras");e.nEmissoras=1;strcpy(e.emissoras[0],"ESPN");
  strcpy(e.nome,"Flamengo × Palmeiras");e.inicio=time(NULL)+3600;e.fim=e.inicio+7200;
  e.nParticipantes=2;strcpy(e.participantes[0].id,"fla");strcpy(e.participantes[0].nome,"Flamengo");strcpy(e.participantes[1].id,"pal");return e;
}
int main(int argc,char **argv){
  assert(argc==3);assert(!setenv("NUVIO_DADOS",argv[1],1));dados_iniciar(argv[1]);esportes_abrir_sessao();EspEvento e=game();
  if(!strcmp(argv[2],"game-write")){
    assert(esportes_seguir_jogo(&e,1));return 0;
  }
  if(!strncmp(argv[2],"game-",5)){
    assert(!esportes_favoritos_times()&&esportes_segue_jogo(&e));
    const EspEvento *saved=esportes_jogo_seguido(0);assert(saved);
    if(!strcmp(argv[2],"game-restart")){
      assert(saved->nParticipantes==2&&!strcmp(saved->participantes[1].nome,"Palmeiras"));
      assert(!strcmp(saved->liga,e.liga)&&!strcmp(saved->local,e.local));
      assert(saved->nEmissoras==1&&!strcmp(saved->emissoras[0],"ESPN"));
      esportes_seguir_lembretes();assert(lembrete_n()==1&&lembrete_item(0)->ini==saved->inicio);
    }else{
      esportes_seguir_lembretes();assert(lembrete_n()==1);
      providerStart=saved->inicio+86400;
      drain();assert(requests==1);
      saved=esportes_jogo_seguido(0);assert(saved->inicio==providerStart&&!strcmp(saved->local,"Updated stadium"));
      esportes_seguir_lembretes();assert(lembrete_n()==1&&lembrete_item(0)->ini==providerStart);
      providerState=1;esportes_pedir_favoritos(1);drain();
      assert(esportes_jogo_seguido(0)->estado==3);
      esportes_seguir_lembretes();assert(!lembrete_n());
    }
    puts("PASS game-only restart/provider refresh/reminders");return 0;
  }
  if(!strcmp(argv[2],"write")){
    assert(!esportes_segue_evento(&e));
    assert(esportes_seguir_time(&e,0,1));assert(esportes_seguir_time(&e,0,1));
    assert(esportes_favoritos_times()==1&&esportes_segue_time(&e,0));
    assert(esportes_seguir_jogo(&e,1));assert(esportes_seguir_jogo(&e,1));
    assert(esportes_jogos_seguidos_n()==1&&esportes_segue_jogo(&e));
  }else{
    assert(esportes_segue_time(&e,0)&&esportes_segue_jogo(&e));
    assert(esportes_segue_evento(&e));EspEvento other=e;strcpy(other.id,"other");
    assert(esportes_segue_evento(&other));strcpy(other.participantes[0].id,"other");assert(!esportes_segue_evento(&other));
    profile=2;generation++;esportes_abrir_sessao();
    assert(!esportes_segue_evento(&e)&&!esportes_jogos_seguidos_n());
    esportes_seguir_lembretes();assert(!lembrete_n());
    profile=1;generation++;esportes_abrir_sessao();
    esportes_seguir_lembretes();assert(lembrete_n()==1);
    esportes_seguir_lembretes();assert(lembrete_n()==1);
    assert(lembrete_item(0)->ini==esportes_jogo_seguido(0)->inicio);
    lembrete_ajustar(0,e.inicio+600,e.fim+600);esportes_seguir_lembretes();
    assert(lembrete_item(0)->ini==e.inicio+600); /* Stored snapshot must not undo a provider update. */
    char blocked[700];snprintf(blocked,sizeof blocked,"%s/esportes-%08x-p1.txt.tmp",dados_dir(),0u);
    /* Force the real atomic writer to fail by making its temporary path a directory. */
    const char *account="follow-proof";unsigned hash=2166136261u;
    for(const char *p=account;*p;p++){hash^=(unsigned char)*p;hash*=16777619u;}
    snprintf(blocked,sizeof blocked,"%s/esportes-%08x-p1.txt.tmp",dados_dir(),hash);
    assert(!mkdir(blocked,0700));
    assert(!esportes_seguir_jogo(&e,0)&&esportes_segue_jogo(&e));
    assert(!esportes_seguir_time(&e,0,0)&&esportes_segue_time(&e,0));
    assert(!rmdir(blocked));
    assert(esportes_seguir_time(&e,0,0));assert(!esportes_segue_time(&e,0)&&esportes_segue_evento(&e));
    assert(esportes_seguir_jogo(&e,0));assert(!esportes_segue_evento(&e));
    esportes_seguir_lembretes();assert(!lembrete_n());
    generation++;esportes_abrir_sessao();assert(!esportes_segue_evento(&e));
    e.participantes[0].id[0]=0;assert(!esportes_seguir_time(&e,0,1));
    e.id[0]=0;assert(!esportes_seguir_jogo(&e,1));
    EspParticipante teams[4];
    const char *json="{\"sports\":[{\"leagues\":[{\"teams\":[{\"team\":{\"id\":\"fla\",\"displayName\":\"Flamengo\"}},{\"team\":{\"id\":\"pal\",\"displayName\":\"Palmeiras\"}}]}]}]}";
    assert(esportes_times_ler(json,teams,4)==2&&!strcmp(teams[1].nome,"Palmeiras"));
  }
  puts("PASS follow persistence, restart, profile isolation, filter, reminders and roster");return 0;
}
