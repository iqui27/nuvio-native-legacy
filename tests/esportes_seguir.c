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
static int profile=1;
static uint64_t generation=1;
uint64_t perfis_sessao_geracao(void){return generation;}
int perfis_sessao_atual(uint64_t s){return s==generation;}
int perfis_ativo(void){return profile;}
const char *sessao_usuario(void){return "follow-proof";}
int ajustes_idioma(void){return 1;}
char *rede_baixar_medido_controle(const char *u,int s,const char *const *h,const RedeControle *c,RedeMedida *m){(void)u;(void)s;(void)h;(void)c;m->status=503;return NULL;}
void esportes_seguir_lembretes(void);
static EspEvento game(void){
  EspEvento e={0};strcpy(e.id,"game-1");strcpy(e.modalidade,"soccer");strcpy(e.ligaId,"bra.1");
  strcpy(e.nome,"Flamengo × Palmeiras");e.inicio=time(NULL)+3600;e.fim=e.inicio+7200;
  e.nParticipantes=2;strcpy(e.participantes[0].id,"fla");strcpy(e.participantes[0].nome,"Flamengo");strcpy(e.participantes[1].id,"pal");return e;
}
int main(int argc,char **argv){
  assert(argc==3);assert(!setenv("NUVIO_DADOS",argv[1],1));dados_iniciar(argv[1]);esportes_abrir_sessao();EspEvento e=game();
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
