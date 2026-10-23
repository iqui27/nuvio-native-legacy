// Actual native owners, local fixture files, no Google/network/playback.
#include "../src/ajustes.c"
#include "backupapp.h"
#include "backupdisco.h"
#include "backupstores.h"
#include <assert.h>
static const char session[]="eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJ1c2VyLTEiLCJleHAiOjQxMDI0NDQ4MDB9.test-signature\nfixture-refresh\n0\n";
static void same(const char *name,const char *expected) {
 char *s=dados_ler(name);assert(s && !strcmp(s,expected));free(s);
}
static void coverage(void) {
 int native=0,account=0;
 for(int i=0;i<AJ_N;i++) {
  if(!CHAVE[i] || CHAVE[i][0]=='-' || OPCOES[i].tipo==OP_ACAO || OPCOES[i].tipo==OP_LEITURA || i==AJ_PROVEDORES_ESCOPO || i==AJ_ENVIO_AUTO)continue;
  int scopes=0;
  for(int k=0;k<=AJ_BACKUP_HARDWARE;k++)scopes+=abLocal(i,k,valor[i]);
  if(ajustePortavel(i)){assert(scopes==0);account++;continue;}
  assert(scopes==1);native++;
  int old=valor[i],low=OPCOES[i].tipo==OP_NUMERO?OPCOES[i].min:0;
  int high=OPCOES[i].tipo==OP_NUMERO?OPCOES[i].max:nValores(i)-1;
  for(int v=low;;v=high) {
   valor[i]=v;int scope=abLocal(i,AJ_BACKUP_HARDWARE,v)?AJ_BACKUP_HARDWARE:abLocal(i,AJ_BACKUP_PERFIL,v)?AJ_BACKUP_PERFIL:AJ_BACKUP_APARELHO;
   int persisted=i==AJ_IDIOMA?ajustes_idioma():v;
   char line[128],*b=NULL,*merged=NULL;snprintf(line,sizeof line,"%s %d\n",CHAVE[i],persisted);
   int captured=ajustes_backup_capturar(scope,&b);
   if(!captured || !strstr(b,line))
    fprintf(stderr,"backup coverage: missing key=%s scope=%d expected=%d\n",CHAVE[i],scope,v);
   assert(captured && strstr(b,line));
   if(i==AJ_IDIOMA) {
    char marker[48];snprintf(marker,sizeof marker,"idiomaAutoLocal %d\n",v==0);
    assert(strstr(b,marker) && strstr(b,"idiomaFonteLocal "));
    const char *sections[3]={b,NULL,NULL};char *selected[3]={0};
    assert(ajustes_backup_selecionar(sections,0,selected));
    assert(strstr(selected[0],line) && strstr(selected[0],marker));
    for(int section=0;section<3;section++)free(selected[section]);
   }
   assert(ajustes_backup_mesclar(scope,"unknownKey untouched\n",b,&merged));
   assert(strstr(merged,line) && strstr(merged,"unknownKey untouched\n"));free(b);free(merged);
   if(v==high)break;
  }
  valor[i]=old;
 }
 printf("PASS: %d local settings round-trip; %d account-sync settings excluded using the actual sync policy\n",native,account);
}
int main(void) {
 dados_iniciar(getenv("NUVIO_DADOS"));assert(dados_dir()[0]);
 assert(dados_gravar("sessao.txt",session));sessao_iniciar();
 assert(dados_gravar("perfil.txt","7\n"));perfis_carregar_ativo();
 ajustes_dir(dados_dir());coverage();
 // Verify that excluded preferences really enter the native account payload,
 // while hardware choices and credentials do not. No account HTTP is used.
 char *cloud=NULL;assert(ajustes_mesclar_blob("{}",&cloud)>0 && cloud);
 assert(strstr(cloud,"c_settings_v1") && strstr(cloud,"c_legendaIdioma") && strstr(cloud,"c_audioIdioma"));
 // Upstream now includes a separate c_devices_v1 snapshot. The hardware
 // choice must remain absent from shared profile values, and logins from both.
 const char *settings,*settingsEnd,*values,*valuesEnd,*ignored,*ignoredEnd;
 assert(acharValor(cloud,cloud+strlen(cloud),"c_settings_v1",&settings,&settingsEnd));
 assert(acharValor(settings,settingsEnd,"values",&values,&valuesEnd));
 assert(!acharValor(values,valuesEnd,"c_interfaceModo",&ignored,&ignoredEnd));
 assert(!strstr(cloud,"fixture-refresh"));free(cloud);
 BackupContexto c;assert(backupapp_contexto(&c));
 assert(dados_gravar("trakt-p7.txt","destination-rotating-token\n"));
 assert(dados_gravar("discord-p7.txt","destination-discord-token\n"));
 assert(dados_gravar("fileirasui-p7.txt","ordem 1\nlimite 7\n"));
 assert(dados_gravar("ltv-urls-p7.txt","m3u\tSource\thttps://source.invalid/list\n"));
 assert(dados_gravar("guia-ltv-fav-p7.txt","m3u:source\tchannel-1\n"));
 assert(dados_gravar("ltv-epgmapa-p7.txt","m3u:source\tchannel-1\txmltv:guide\tprogram-1\n"));
 assert(dados_gravar("ltv-indice-p7.txt","generated-channel-cache-index\n"));
 valor[AJ_QUALIDADE_IMG]=0;valor[AJ_INTERFACE_MODO]=1;valor[AJ_AMPLIAR_UI]=1;
 valor[AJ_TAMANHO_AJUSTES]=0;valor[AJ_LEG_LINGUA]=1;
 assert(gravar());ajustes_perfil_guardar(7);
 BackupLocal *b=NULL;assert(backupapp_capturar(&c,1792330000,&b));
 assert(!backuplocal_parte(b,BACKUP_PLAYER,NULL));
 assert(!backuplocal_parte(b,BACKUP_FONTES_COMPARTILHADAS,NULL));
 assert(!backuplocal_parte(b,BACKUP_FONTES_PERFIL,NULL));
 char *wire=NULL;size_t n=0;assert(backuplocal_exportar(b,&wire,&n));
 assert(!strstr(wire,"fixture-refresh") && !strstr(wire,"destination-rotating-token") && !strstr(wire,"destination-discord-token") && !strstr(wire,"generated-channel-cache-index"));
 assert(!strstr(wire,"legendaIdioma ") && !strstr(wire,"ordem 1\n"));free(wire);
 valor[AJ_INTERFACE_MODO]=0;valor[AJ_AMPLIAR_UI]=2;valor[AJ_QUALIDADE_IMG]=1;valor[AJ_TAMANHO_AJUSTES]=1;valor[AJ_LEG_LINGUA]=2;
 assert(gravar());ajustes_perfil_guardar(7);
 assert(dados_gravar("fileirasui-p7.txt","ordem 0\nlimite 3\n"));
 assert(dados_gravar("ltv-urls-p7.txt","m3u\tDestination\thttps://destination.invalid/list\n"));
 assert(dados_gravar("guia-ltv-fav-p7.txt","changed\n"));
 assert(dados_gravar("ltv-epgmapa-p7.txt","changed\n"));
 BackupLocal *selected=NULL;assert(backupapp_selecionar(&c,b,0,&selected));
 assert(!backuplocal_parte(selected,BACKUP_AJUSTES_HARDWARE,NULL));
 assert(backupapp_validar(&c,selected));assert(backupapp_restaurar(&c,selected)==BACKUP_RESTORE_OK);
 assert(valor[AJ_AMPLIAR_UI]==2);
 assert(valor[AJ_INTERFACE_MODO]==0 && valor[AJ_QUALIDADE_IMG]==1 && valor[AJ_TAMANHO_AJUSTES]==1 && valor[AJ_LEG_LINGUA]==2);
 same("fileirasui-p7.txt","ordem 0\nlimite 3\n");
 same("guia-ltv-fav-p7.txt","m3u:source\tchannel-1\n");
 same("ltv-epgmapa-p7.txt","m3u:source\tchannel-1\txmltv:guide\tprogram-1\n");
 same("ltv-urls-p7.txt","m3u\tDestination\thttps://destination.invalid/list\nm3u\tSource\thttps://source.invalid/list\n");
 assert(backupapp_restaurar(&c,selected)==BACKUP_RESTORE_OK);
 same("ltv-urls-p7.txt","m3u\tDestination\thttps://destination.invalid/list\nm3u\tSource\thttps://source.invalid/list\n");
 // Hardware-only restore must update profile-backed display values too.
 BackupLocal *hardwareOnly=backuplocal_novo(c.usuario,c.perfil,1792330000);assert(hardwareOnly);
 size_t hardwareSize=0;const char *hardwareBody=backuplocal_parte(b,BACKUP_AJUSTES_HARDWARE,&hardwareSize);
 assert(hardwareBody && backuplocal_adicionar(hardwareOnly,BACKUP_AJUSTES_HARDWARE,hardwareBody,hardwareSize));
 assert(backupapp_validar(&c,hardwareOnly) && backupapp_restaurar(&c,hardwareOnly)==BACKUP_RESTORE_OK);
 assert(valor[AJ_AMPLIAR_UI]==1 && valor[AJ_TAMANHO_AJUSTES]==0);
 backuplocal_liberar(hardwareOnly);
 // Hardware opt-in restores display choices, never the account's languages.
 assert(backupapp_restaurar(&c,b)==BACKUP_RESTORE_OK);
 assert(valor[AJ_AMPLIAR_UI]==1);
 assert(valor[AJ_INTERFACE_MODO]==1 && valor[AJ_QUALIDADE_IMG]==0 && valor[AJ_TAMANHO_AJUSTES]==0 && valor[AJ_LEG_LINGUA]==2);
 same("fileirasui-p7.txt","ordem 0\nlimite 7\n");
 same("sessao.txt",session);same("trakt-p7.txt","destination-rotating-token\n");same("discord-p7.txt","destination-discord-token\n");
 // Legacy scalar payloads cannot replay account-managed language preferences.
 char *merged=NULL;assert(ajustes_backup_mesclar(AJ_BACKUP_PERFIL,"legendaIdioma 2\n","NUVIO-LOCAL-SETTINGS 1\nscope profile\nlegendaIdioma 1\n",&merged));
 assert(!strcmp(merged,"legendaIdioma 2\n"));free(merged);
 BackupLocal *bad=backuplocal_novo(c.usuario,c.perfil,1792330001);assert(bad);
 assert(backuplocal_adicionar(bad,BACKUP_AJUSTES_APARELHO,"bad",3));
 assert(!backupapp_validar(&c,bad) && backupapp_restaurar(&c,bad)==BACKUP_RESTORE_INVALIDO);backuplocal_liberar(bad);
 perfis_invalidar_sessao();assert(!backupapp_validar(&c,b));assert(backupapp_restaurar(&c,b)==BACKUP_RESTORE_CONTEXTO);
 backuplocal_liberar(selected);backuplocal_liberar(b);
 puts("PASS: actual local restore, account/settings/token preservation, hardware opt-in, URL merge, repeated restore and stale-session rejection");
 return 0;
}
