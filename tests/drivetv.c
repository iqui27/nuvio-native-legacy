#define _POSIX_C_SOURCE 200809L
#include "drivetv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CLIENT "test-client.apps.googleusercontent.com"
#define CODE "{\"device_code\":\"device/+&=\",\"user_code\":\"ABCD-EFGH\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":1800,\"interval\":5}"
#define TOKEN "{\"access_token\":\"access-token\",\"refresh_token\":\"refresh-token\",\"expires_in\":3600,\"token_type\":\"Bearer\",\"scope\":\"" DRIVE_APPLICACAO_SCOPE "\"}"
typedef struct {const char *body;size_t n;int status,calls,transport,cancel;char sent[40000],url[128];} Mock;
static int pedir(void *ctx,const RedePedido *p,RedeResposta *r) {
  Mock *m=ctx;m->calls++;snprintf(m->sent,sizeof m->sent,"%s",(const char *)p->corpo);snprintf(m->url,sizeof m->url,"%s",p->url);
  assert(!strcmp(p->metodo,"POST") && !p->seguir && !p->tls_pessoal);
  assert(p->prazo_ms==15000 && p->max_bytes==32768 && p->max_cabecalhos==16384);
  assert(!strcmp(p->cabecalhos[0],"Content-Type: application/x-www-form-urlencoded") && !p->cabecalhos[1]);
  assert(p->n_corpo==strlen(p->corpo) && !strstr(p->url,"secret") && !strstr(p->url,"device_code"));
  if(m->cancel){assert(p->job);rede_job_cancelar(p->job);}
  r->status=m->status;
  if(m->body) {
    r->n_corpo=m->n?m->n:strlen(m->body);
    // Deliberately no trailing NUL: HTTP callers own explicit byte spans.
    r->corpo=malloc(r->n_corpo);assert(r->corpo);memcpy(r->corpo,m->body,r->n_corpo);
  }
  if(!m->transport)r->erro=REDE_PRAZO;
  return m->transport;
}
static void response(Mock *m,int status,const char *body){memset(m,0,sizeof *m);m->status=status;m->body=body;m->transport=1;}
int main(void) {
  Mock m;response(&m,200,CODE);
  DriveTv c={.cliente=CLIENT,.segredo="client+secret&value=",.pedir=pedir,.ctx=&m};
  DriveTvCodigo code;DriveTvToken token;
  assert(drivetv_iniciar(&c,1000,&code)==DRIVE_TV_OK);
  assert(!strcmp(m.url,"https://oauth2.googleapis.com/device/code"));
  assert(!strcmp(m.sent,"client_id=" CLIENT "&scope=https%3A%2F%2Fwww.googleapis.com%2Fauth%2Fdrive.file"));
  assert(!strcmp(code.codigo,"ABCD-EFGH") && code.expiraMs==1801000 && code.proximaMs==6000 && code.intervalo==5);
  response(&m,428,"{\"error\":\"authorization_pending\"}");
  assert(drivetv_consultar(&c,&code,5999,&token)==DRIVE_TV_AGUARDAR && !m.calls && !*token.acesso);
  assert(drivetv_consultar(&c,&code,6000,&token)==DRIVE_TV_AGUARDAR && m.calls==1 && code.proximaMs==11000);
  assert(!strcmp(m.url,"https://oauth2.googleapis.com/token"));
  assert(strstr(m.sent,"client_secret=client%2Bsecret%26value%3D") && strstr(m.sent,"device_code=device%2F%2B%26%3D"));
  assert(strstr(m.sent,"grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code"));
  response(&m,403,"{\"error\":\"slow_down\"}");
  assert(drivetv_consultar(&c,&code,11000,&token)==DRIVE_TV_AGUARDAR && code.intervalo==10 && code.proximaMs==21000);
  assert(drivetv_consultar(&c,&code,20999,&token)==DRIVE_TV_AGUARDAR && m.calls==1);
  response(&m,200,TOKEN);
  assert(drivetv_consultar(&c,&code,21000,&token)==DRIVE_TV_OK);
  assert(!strcmp(token.acesso,"access-token") && !strcmp(token.renovacao,"refresh-token") && token.expiraMs==3621000);
  response(&m,200,"{\"access_token\":\"new-access\",\"token_type\":\"Bearer\",\"expires_in\":3600}");
  assert(drivetv_renovar(&c,"old/+&=refresh",30000,&token)==DRIVE_TV_OK && !strcmp(token.renovacao,"old/+&=refresh"));
  assert(strstr(m.sent,"refresh_token=old%2F%2B%26%3Drefresh") && strstr(m.sent,"grant_type=refresh_token"));
  response(&m,200,TOKEN);
  assert(drivetv_renovar(&c,"old-refresh",30000,&token)==DRIVE_TV_OK && !strcmp(token.renovacao,"refresh-token"));
  const char *badCodes[]={"{}","{\"expires_in\":1800}",
    "{\"device_code\":\"d\",\"user_code\":\"u\",\"verification_url\":\"https://evil.example/device\",\"expires_in\":1800}",
    "{\"device_code\":\"d\",\"user_code\":\"u\",\"verification_url\":\"https://www.google.com/device\",\"nested\":{\"expires_in\":1800}}",
    "{\"device_code\":\"d\",\"device_code\":\"other\",\"user_code\":\"u\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":1800}",
    CODE " trailing", "{\"device_code\":\"bad\\x\"}"};
  for(size_t i=0;i<sizeof badCodes/sizeof *badCodes;i++) {
    response(&m,200,badCodes[i]);memset(&code,0xff,sizeof code);
    assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_RESPOSTA && !*code.dispositivo);
  }
  response(&m,200,"{\"device_code\":\"d\",\"user_code\":\"u\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":1800}");
  assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_OK && code.intervalo==5);
  response(&m,200,CODE);assert(drivetv_iniciar(&c,UINT64_MAX,&code)==DRIVE_TV_RESPOSTA);
  response(&m,200,CODE);assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_OK);
  response(&m,200,TOKEN);
  assert(drivetv_consultar(&c,&code,code.expiraMs,&token)==DRIVE_TV_EXPIRADO && !m.calls);
  const char *errors[]={"access_denied","admin_policy_enforced","expired_token","invalid_grant","invalid_client","unauthorized_client","invalid_scope","org_internal","unrecognized"};
  int expected[]={DRIVE_TV_NEGADO,DRIVE_TV_NEGADO,DRIVE_TV_EXPIRADO,DRIVE_TV_EXPIRADO,DRIVE_TV_CONFIGURACAO,DRIVE_TV_CONFIGURACAO,DRIVE_TV_CONFIGURACAO,DRIVE_TV_CONFIGURACAO,DRIVE_TV_RESPOSTA};
  for(size_t i=0;i<sizeof errors/sizeof *errors;i++) {
    char body[128];snprintf(body,sizeof body,"{\"error\":\"%s\"}",errors[i]);response(&m,400,body);
    assert(drivetv_renovar(&c,"refresh",0,&token)==expected[i] && !*token.acesso && !*token.renovacao);
  }
  response(&m,200,CODE);assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_OK);
  response(&m,503,"unavailable");
  assert(drivetv_consultar(&c,&code,5000,&token)==DRIVE_TV_REPETIR && code.intervalo==10 && code.proximaMs==15000);
  response(&m,0,NULL);m.transport=0;
  assert(drivetv_consultar(&c,&code,15000,&token)==DRIVE_TV_REDE && code.intervalo==20 && code.proximaMs==35000);
  assert(drivetv_consultar(&c,&code,34999,&token)==DRIVE_TV_AGUARDAR && m.calls==1);
  const char *badTokens[]={"{}","{\"nested\":" TOKEN "}",
    "{\"access_token\":\"a\",\"refresh_token\":\"r\",\"expires_in\":3600,\"token_type\":\"Bearer\",\"scope\":\"wrong-scope\"}",
    "{\"access_token\":\"a\",\"refresh_token\":\"r\",\"expires_in\":3600,\"token_type\":\"Bearer\",\"scope\":\"" DRIVE_APPLICACAO_SCOPE "-other\"}",
    "{\"access_token\":\"a\",\"refresh_token\":\"r\",\"expires_in\":1.5,\"token_type\":\"Bearer\"}",
    "{\"access_token\":\"a\\nheader\",\"refresh_token\":\"r\",\"expires_in\":3600,\"token_type\":\"Bearer\"}",
    "{\"access_token\":\"a\",\"refresh_token\":\"r\",\"expires_in\":3600,\"token_type\":\"Basic\"}"};
  for(size_t i=0;i<sizeof badTokens/sizeof *badTokens;i++) {
    response(&m,200,badTokens[i]);memset(&token,0xff,sizeof token);
    assert(drivetv_renovar(&c,"r",0,&token)==DRIVE_TV_RESPOSTA && !*token.acesso && !*token.renovacao);
  }
  response(&m,302,TOKEN);assert(drivetv_renovar(&c,"r",0,&token)==DRIVE_TV_RESPOSTA);
  response(&m,429,"{}");assert(drivetv_renovar(&c,"r",0,&token)==DRIVE_TV_REPETIR);
  char nul[]="{\"error\":\"invalid_client\"}\0hidden";response(&m,400,nul);m.n=sizeof nul-1;
  assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_RESPOSTA);
  DriveTv invalid=c;invalid.cliente="wrong-client";response(&m,200,CODE);
  assert(drivetv_iniciar(&invalid,0,&code)==DRIVE_TV_INVALIDO && !m.calls);
  invalid=c;invalid.segredo="secret\nheader";assert(drivetv_renovar(&invalid,"r",0,&token)==DRIVE_TV_INVALIDO && !m.calls);
  RedeGrupo *g=rede_grupo_criar();assert(g);RedeJob *j=rede_job_criar(g);assert(j);c.job=j;
  response(&m,200,CODE);assert(drivetv_iniciar(&c,0,&code)==DRIVE_TV_OK);
  response(&m,200,TOKEN);m.cancel=1;
  assert(drivetv_consultar(&c,&code,5000,&token)==DRIVE_TV_CANCELADO && !*token.acesso);
  int before=m.calls;assert(drivetv_consultar(&c,&code,10000,&token)==DRIVE_TV_CANCELADO && m.calls==before);
  assert(drivetv_renovar(&c,"refresh",0,&token)==DRIVE_TV_CANCELADO && m.calls==before);
  rede_job_soltar(j);rede_grupo_soltar(g);
  puts("drivetv: polling, refresh, expiry, scopes, private forms and cancellation passed (mock HTTP)");
  return 0;
}
