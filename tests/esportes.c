#include "esportes.h"
#include "js.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const EspLiga liga = {"nba", "NBA", ""};
static const char *jogo = "{\"events\":[{\"id\":\"10\",\"date\":\"2026-10-19T20:00Z\",\"name\":\"Boston Celtics at Orlando Magic\",\"competitions\":[{\"id\":\"10\",\"date\":\"2026-10-19T20:00Z\",\"status\":{\"type\":{\"state\":\"in\",\"shortDetail\":\"Q2\"}},\"venue\":{\"fullName\":\"Arena\"},\"competitors\":[{\"score\":\"80\",\"team\":{\"id\":\"1\",\"displayName\":\"Boston Celtics\",\"shortDisplayName\":\"Celtics\",\"abbreviation\":\"BOS\"}},{\"score\":\"70\",\"team\":{\"id\":\"2\",\"displayName\":\"Orlando Magic\",\"shortDisplayName\":\"Magic\",\"abbreviation\":\"ORL\"}}],\"broadcasts\":[{\"names\":[\"ESPN\",\"ABC\"]}]}]}]}";
static void variosEsportes(void) {
  static const struct {const char *mod,*liga,*a,*b,*curtoA,*curtoB,*sim,*nao;} casos[]={
    {"football","NFL","Jacksonville Jaguars","New England Patriots","Jaguars","Patriots","NFL: Jacksonville Jaguars vs New England Patriots","NFL: Dallas Cowboys vs Green Bay Packers"},
    {"football","NCAA Football","Oklahoma Sooners","North Carolina Tar Heels","Sooners","Tar Heels","NCAA Football: Oklahoma Sooners at North Carolina Tar Heels","NFL: Dallas Cowboys vs Green Bay Packers"},
    {"soccer","Premier League","Manchester United","Liverpool","Man United","Liverpool","Premier League: Man United v Liverpool","Manchester United v Manchester City"},
    {"basketball","NBA","Boston Celtics","Orlando Magic","Celtics","Magic","NBA: Celtics vs Magic","NBA: Celtics vs Cavaliers"},
    {"hockey","NHL","Toronto Maple Leafs","Boston Bruins","Maple Leafs","Bruins","NHL: Boston Bruins at Toronto Maple Leafs","NHL: Toronto Maple Leafs vs Montreal Canadiens"},
    {"tennis","ATP","Carlos Alcaraz","Jannik Sinner","Alcaraz","Sinner","ATP Tennis: Alcaraz v Sinner","ATP: Alcaraz v Novak Djokovic"},
    {"mma","UFC","Islam Makhachev","Charles Oliveira","Makhachev","Oliveira","UFC: Makhachev vs Oliveira","UFC: Makhachev vs Dustin Poirier"}
  };
  time_t inicio=1800000000;
  for(size_t i=0;i<sizeof casos/sizeof *casos;i++) {
    EspEvento e={0};e.inicio=inicio;e.nParticipantes=2;
    snprintf(e.modalidade,sizeof e.modalidade,"%s",casos[i].mod);snprintf(e.liga,sizeof e.liga,"%s",casos[i].liga);
    snprintf(e.participantes[0].nome,sizeof e.participantes[0].nome,"%s",casos[i].a);
    snprintf(e.participantes[1].nome,sizeof e.participantes[1].nome,"%s",casos[i].b);
    snprintf(e.participantes[0].curto,sizeof e.participantes[0].curto,"%s",casos[i].curtoA);
    snprintf(e.participantes[1].curto,sizeof e.participantes[1].curto,"%s",casos[i].curtoB);
    assert(esportes_corresponde(&e,casos[i].sim,inicio-1800,inicio+7200));
    assert(!esportes_corresponde(&e,casos[i].nao,inicio,inicio+7200));
    assert(!esportes_corresponde(&e,"ESPN HD",inicio,inicio+7200));
    assert(!esportes_corresponde(&e,casos[i].a,inicio,inicio+7200));
    assert(!esportes_corresponde(&e,casos[i].sim,inicio-86400,inicio-80000));
    assert(!esportes_corresponde(&e,casos[i].sim,inicio+86400,inicio+90000));
    char reprise[600];snprintf(reprise,sizeof reprise,"%s Highlights",casos[i].sim);
    assert(!esportes_corresponde(&e,reprise,inicio,inicio+7200));
    snprintf(reprise,sizeof reprise,"%s Replay",casos[i].sim);
    assert(!esportes_corresponde(&e,reprise,inicio,inicio+7200));
  }
  EspEvento nfl={0};nfl.inicio=inicio;nfl.nParticipantes=2;
  strcpy(nfl.modalidade,"football");strcpy(nfl.liga,"NFL");
  strcpy(nfl.participantes[0].nome,"Kansas City Chiefs");strcpy(nfl.participantes[0].sigla,"KC");
  strcpy(nfl.participantes[1].nome,"San Francisco 49ers");strcpy(nfl.participantes[1].sigla,"SF");
  assert(esportes_corresponde(&nfl,"NFL: KC @ SF",inicio,inicio+7200));
  assert(esportes_corresponde(&nfl,"NFL: SF vs KC",inicio,inicio+7200));
  assert(esportes_corresponde(&nfl,"SF at KC",inicio,inicio+7200));
  assert(!esportes_corresponde(&nfl,"NFL: KC vs SEA",inicio,inicio+7200));
  assert(!esportes_corresponde(&nfl,"KC news and SF weather",inicio,inicio+7200));
  assert(!esportes_corresponde(&nfl,"NHL: KC @ SF",inicio,inicio+7200));
  assert(!esportes_corresponde(&nfl,"KC @ SF Replay",inicio,inicio+7200));
  assert(!esportes_corresponde(&nfl,"KC @ SF",inicio-86400,inicio-80000));
  printf("sports matching: %zu sports/matchups, 56 positive/negative cases passed\n",sizeof casos/sizeof *casos);
}
int main(void) {
  variosEsportes();
  EspEvento e[5]; EspLiga ls[3];
  assert(esportes_modalidades_n() == 16);
  assert(!esportes_modalidade(-1)); assert(!esportes_modalidade(16));
  assert(esportes_ligas_ler("{\"leagues\":[{\"slug\":\"eng.1\",\"name\":\"Premier League\"},{\"slug\":\"../bad\",\"name\":\"Bad\"},{\"slug\":\"nba\",\"name\":\"NBA\"}]}",ls,3)==2);
  assert(!strcmp(ls[1].id,"nba"));
  memset(e,0,sizeof e); assert(esportes_eventos_ler(jogo,"basketball",&liga,e,5)==1);
  assert(e[0].nParticipantes==2 && e[0].estado==1);
  assert(!strcmp(e[0].participantes[0].pontos,"80"));
  assert(!strcmp(e[0].local,"Arena")); assert(e[0].nEmissoras==2);
  time_t t=(time_t)(js_ms_iso("2026-10-19T20:00Z")/1000);
  assert(esportes_corresponde(e,"NBA: Celtics vs Magic",t-1800,t+10800)==100);
  assert(esportes_corresponde(e,"BOS @ ORL",t,t+10800)==100);
  assert(!esportes_corresponde(e,"Celtics vs Cavaliers",t,t+10800));
  assert(!esportes_corresponde(e,"NBA Basketball",t,t+10800));
  assert(!esportes_corresponde(e,"NHL Celtics vs Magic",t,t+10800));
  assert(!esportes_corresponde(e,"Women: Celtics vs Magic",t,t+10800));
  assert(!esportes_corresponde(e,"Celtics vs Magic",t-86400,t-80000));
  assert(!esportes_corresponde(e,"Celtics vs Magic Highlights",t,t+10800));
  assert(!esportes_corresponde(e,"BOSTON ORLANDO",t,t+10800));
  assert(!esportes_corresponde(e,"BOSON ORLANDO",t,t+10800));
  assert(esportes_emissora(e,"US | ESPN HD"));
  assert(!esportes_emissora(e,"ESPNExtra"));
  assert(!esportes_corresponde(e,"ESPN NBA",t,t+10800));
  e[0].estado=3; assert(!esportes_corresponde(e,"Celtics vs Magic",t,t+10800));
  /* Do not inherit a participant ID/name/date as missing event identity. */
  assert(!esportes_eventos_ler("{\"events\":[{\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"wrong\",\"name\":\"wrong\",\"date\":\"2026-10-19T20:00Z\"}}]}]}]}","basketball",&liga,e,5));
  const char *tennis="{\"events\":[{\"id\":\"tour\",\"name\":\"Tournament\",\"date\":\"2026-10-19T10:00Z\",\"groupings\":[{\"competitions\":[{\"id\":\"match\",\"date\":\"2026-10-19T20:00Z\",\"competitors\":[{\"id\":\"7\",\"linescores\":[{\"value\":6},{\"value\":7}],\"athlete\":{\"displayName\":\"Player One\"}},{\"athlete\":{\"id\":\"8\",\"displayName\":\"Player Two\"}}]}]}]}]}";
  assert(esportes_eventos_ler(tennis,"tennis",&liga,e,5)==1);
  assert(!strcmp(e[0].id,"tour:match") && e[0].inicio==t);
  assert(e[0].nParticipantes==2 && strstr(e[0].nome,"Player One"));
  assert(!strcmp(e[0].participantes[0].id,"7"));
  assert(!strcmp(e[0].participantes[0].pontos,"6 7"));
  const char *race="{\"events\":[{\"id\":\"race\",\"name\":\"United States Grand Prix\",\"date\":\"2026-10-19T20:00Z\",\"competitions\":[{\"id\":\"p1\",\"date\":\"2026-10-19T20:00Z\",\"type\":{\"abbreviation\":\"FP1\"}},{\"id\":\"p2\",\"date\":\"2026-10-20T20:00Z\",\"type\":{\"abbreviation\":\"FP2\"}}]}]}";
  assert(esportes_eventos_ler(race,"racing",&liga,e,5)==2);
  assert(!strcmp(e[1].id,"race:p2") && !strcmp(e[1].sessao,"FP2"));
  assert(esportes_corresponde(e,"F1 United States Grand Prix FP1",t,t+7200));
  assert(!esportes_corresponde(e,"F1 United States Grand Prix FP2",t,t+7200));
  assert(!esportes_corresponde(e,"F1 Italian Grand Prix",t,t+7200));
  assert(esportes_eventos_ler(race,"racing",&liga,e,1)==1);
  assert(esportes_eventos_ler("{\"events\":[]}","soccer",&liga,e,5)==0);
  /* Real-shaped artwork variants: dark league mark, team mark and athlete portrait. */
  assert(esportes_ligas_ler("{\"leagues\":[{\"slug\":\"nfl\",\"name\":\"NFL\",\"logos\":[{\"href\":\"light.png\",\"rel\":[\"default\"]},{\"href\":\"dark.png\",\"rel\":[\"dark\"]}]}]}",ls,3)==1);
  assert(!strcmp(ls[0].logo,"dark.png"));
  const char *art="{\"events\":[{\"id\":\"art\",\"name\":\"A at B\",\"date\":\"2026-10-19T20:00Z\",\"competitions\":[{\"competitors\":[{\"team\":{\"id\":\"a\",\"displayName\":\"A\",\"logo\":\"team.png\",\"logoDark\":\"team-dark.png\"}},{\"athlete\":{\"id\":\"b\",\"displayName\":\"B\",\"headshot\":{\"href\":\"portrait.png\"}}}]}]}]}";
  assert(esportes_eventos_ler(art,"basketball",&liga,e,5)==1);
  assert(!strcmp(e[0].participantes[0].logo,"team-dark.png"));
  assert(!strcmp(e[0].participantes[1].logo,"portrait.png"));
  puts("sports parsing and event matching: passed"); return 0;
}
