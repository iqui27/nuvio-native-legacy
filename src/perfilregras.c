#include "perfilregras.h"
#include "perfilinfantil.h"
#include "perfis.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define PR_MAX 32
static int loaded,values[PR_MAX+1][PR_N],days[PR_MAX+1],used[PR_MAX+1];
static float fractions[PR_MAX+1];
static const char FILE_NAME[]="parental-rules.txt";
static void load(void) {
  if(loaded)return;
  loaded=1;
  for(int p=1;p<=PR_MAX;p++) { values[p][PR_IDADE]=10;values[p][PR_COMPRAS]=1; }
  char *data=dados_ler(FILE_NAME);if(!data)return;
  char *save=NULL;
  for(char *line=strtok_r(data,"\n",&save);line;line=strtok_r(NULL,"\n",&save)) {
    int p,minutes,age,search,purchases,day,seconds;
    if(sscanf(line,"%d %d %d %d %d %d %d",&p,&minutes,&age,&search,&purchases,&day,&seconds)==7 && p>=1 && p<=PR_MAX && minutes>=0 && minutes<=240 && age>=0 && age<=18 && (search==0 || search==1) && (purchases==0 || purchases==1) && seconds>=0) {
      values[p][0]=minutes;values[p][1]=age;values[p][2]=search;values[p][3]=purchases;days[p]=day;used[p]=seconds;
    }
  }
  free(data);
}
static int writeRules(void) {
  char data[4096];size_t pos=0;
  for(int p=1;p<=PR_MAX;p++)pos+=snprintf(data+pos,sizeof data-pos,"%d %d %d %d %d %d %d\n",p,values[p][0],values[p][1],values[p][2],values[p][3],days[p],used[p]);
  return dados_gravar(FILE_NAME,data);
}
int perfil_regra(int profile,int rule) { load();return profile>=1 && profile<=PR_MAX && rule>=0 && rule<PR_N?values[profile][rule]:0; }
int perfil_regra_definir(int profile,int rule,int value) {
  load();if(profile<1 || profile>PR_MAX || rule<0 || rule>=PR_N || value<0 || value>(rule==PR_TEMPO?240:rule==PR_IDADE?18:1))return 0;
  int old=values[profile][rule];values[profile][rule]=value;
  if(writeRules())return 1;
  values[profile][rule]=old;return 0;
}
int perfil_regra_classif(const char *certificate) {
  if(!perfil_infantil_ativo())return 1;
  if(!certificate || !*certificate)return 0;
  int max=perfil_regra(perfis_ativo(),PR_IDADE);
  if(*certificate>='0' && *certificate<='9')return atoi(certificate)<=max;
  if(perfil_infantil_classif_ok(certificate))return 1;
  if(!strcasecmp(certificate,"PG-13") || !strcasecmp(certificate,"TV-14"))return max>=14;
  if(!strcasecmp(certificate,"R") || !strcasecmp(certificate,"NC-17") || !strcasecmp(certificate,"TV-MA"))return max>=18;
  return 0;
}
int perfil_regra_busca(const char *certificate) { return !perfil_infantil_ativo() || perfil_regra(perfis_ativo(),PR_BUSCA_LIVRE) || perfil_regra_classif(certificate); }
int perfil_regra_compras(void) { return perfil_infantil_ativo() && perfil_regra(perfis_ativo(),PR_COMPRAS); }
int perfil_regra_tempo(float seconds) {
  load();int p=perfis_ativo();if(p<1 || p>PR_MAX || !perfil_infantil_ativo())return 0;
  time_t now=time(NULL);struct tm day;localtime_r(&now,&day);int stamp=(day.tm_year+1900)*1000+day.tm_yday;
  if(days[p]!=stamp) { days[p]=stamp;used[p]=0;fractions[p]=0; }
  if(seconds>0 && seconds<10)fractions[p]+=seconds;
  if(fractions[p]>=1) {
    int add=(int)fractions[p],before=used[p];fractions[p]-=add;used[p]+=add;
    if(before/30!=used[p]/30)writeRules();
  }
  return values[p][PR_TEMPO]>0 && used[p]>=values[p][PR_TEMPO]*60;
}
void perfil_regras_esquecer(void) { loaded=0;memset(values,0,sizeof values);memset(days,0,sizeof days);memset(used,0,sizeof used);memset(fractions,0,sizeof fractions);dados_apagar(FILE_NAME); }
