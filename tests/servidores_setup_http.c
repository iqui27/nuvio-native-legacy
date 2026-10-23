// Real request builders against deterministic in-process HTTP fixtures.
#include "srvsetup.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static int step;
unsigned rede_pedido_capacidades(void){return REDE_CAP_JOB;}
int rede_pedir(const RedePedido *p,RedeResposta *r){
  assert(p->prazo_ms && p->max_bytes && p->mesmo_host);
  const char *body="";
  if(step==0){
    assert(!strcmp(p->url,"https://connect.emby.media/service/user/authenticate"));
    assert(!strcmp(p->metodo,"POST") && strstr(p->corpo,"fixture-password"));
    assert(!p->tls_pessoal);
    body="{\"AccessToken\":\"fixture-cloud\",\"User\":{\"Id\":\"123\"}}";
  }else if(step==1){
    assert(!strcmp(p->url,"https://connect.emby.media/service/servers?userId=123"));
    assert(!strcmp(p->cabecalhos[3],"X-Connect-UserToken: fixture-cloud"));
    body="[{\"Name\":\"Studio\",\"SystemId\":\"12345678\",\"LocalAddress\":\"http://192.168.1.2:8096\",\"AccessKey\":\"fixture-exchange\"}]";
  }else if(step==2){
    assert(!strcmp(p->url,"http://192.168.1.2:8096/emby/System/Info/Public"));
    body="{\"Id\":\"12345678\",\"ServerName\":\"Studio\",\"Version\":\"4.9\",\"ProductName\":\"Emby Server\"}";
  }else if(step==3){
    assert(!strcmp(p->url,"http://192.168.1.2:8096/emby/Connect/Exchange?format=json&ConnectUserId=123"));
    assert(!strcmp(p->cabecalhos[2],"X-Emby-Token: fixture-exchange"));
    body="{\"AccessToken\":\"fixture-local\",\"LocalUserId\":\"987\"}";
  }else if(step==4){
    assert(!strcmp(p->url,"http://192.168.1.2:8096/emby/Users/Public"));
    body="[{\"Name\":\"Visible\",\"Id\":\"987\"},{\"Name\":\"Invalid\",\"Id\":\"../secret\"}]";
  }else assert(0);
  step++;memset(r,0,sizeof *r);r->status=200;r->corpo=strdup(body);r->n_corpo=strlen(body);return 1;
}
void rede_resposta_limpar(RedeResposta *r){free(r->corpo);free(r->cabecalhos);memset(r,0,sizeof *r);}
int main(void){
  RedeResposta r; char token[128],user[64];
  assert(!em_connect_pedir("user/authenticate","{\"rawpw\":\"fixture-password\"}",NULL,NULL,&r));
  assert(em_connect_ler_conta(r.corpo,token,sizeof token,user,sizeof user));rede_resposta_limpar(&r);
  EmConnectServidor server[2];assert(em_connect_servidores(token,user,NULL,server,2)==1);
  JfConta c={0};c.tipo=JF_TIPO_EMBY;strcpy(c.dispositivoId,"fixture-device");
  assert(!em_connect_trocar(&c,server,user,NULL));assert(!strcmp(c.token,"fixture-local") && !strcmp(c.usuarioId,"987"));
  assert(!strcmp(c.connectKey,"fixture-exchange") && !strcmp(c.connectUser,"123"));
  SrvUsuario users[2];assert(srv_usuarios(&c,NULL,users,2)==1);assert(!strcmp(users[0].nome,"Visible"));
  assert(step==5);puts("Server HTTP: Connect password, server list, identity, exchange and public users: PASS");return 0;
}
