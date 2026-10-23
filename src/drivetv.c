#include "drivetv.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GOOGLE_OAUTH "https://oauth2.googleapis.com"
#define RESPOSTA_MAX 32768u
static int ascii(const char *s,size_t max) {
  if(!s || !*s)return 0;
  for(size_t i=0;s[i];i++)if(i>=max || (unsigned char)s[i]<33 || (unsigned char)s[i]>126)return 0;
  return 1;
}
static int valido(const DriveTv *c,int segredo) {
  if(!c || !ascii(c->cliente,512) || (segredo && !ascii(c->segredo,4096)))return 0;
  const char *fim=strstr(c->cliente,".apps.googleusercontent.com");
  return fim && !fim[strlen(".apps.googleusercontent.com")];
}
static char *escapar(const char *s) {
  size_t n=strlen(s);char *out=malloc(n*3+1);if(!out)return NULL;
  const char hex[]="0123456789ABCDEF";size_t k=0;
  for(size_t i=0;i<n;i++) {
    unsigned char c=(unsigned char)s[i];
    if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~')out[k++]=(char)c;
    else{out[k++]='%';out[k++]=hex[c>>4];out[k++]=hex[c&15];}
  }
  out[k]=0;return out;
}
static const char *espaco(const char *p){while(*p==' ' || *p=='\n' || *p=='\r' || *p=='\t')p++;return p;}
static const char *fimTexto(const char *p) {
  if(*p++!='"')return NULL;
  for(;*p;p++) {
    if(*p=='"')return p+1;
    if((unsigned char)*p<32)return NULL;
    if(*p=='\\') {
      p++;if(!*p)return NULL;
      if(*p=='u') {
        for(int i=0;i<4;i++){p++;if(!((*p>='0' && *p<='9') || (*p>='a' && *p<='f') || (*p>='A' && *p<='F')))return NULL;}
      } else if(!strchr("\"\\/bfnrt",*p))return NULL;
    }
  }
  return NULL;
}
// Root-only fields, no nested matches or duplicate-key ambiguity. Google
// responses are small; inspect the whole object before returning a field.
static int campo(const char *s,const char *nome,const char **ini,size_t *n) {
  const char *p=espaco(s);int achou=0;*ini=NULL;*n=0;
  if(*p++!='{')return -1;
  p=espaco(p);
  if(*p=='}')return *espaco(p+1)?-1:0;
  for(;;) {
    const char *k=p,*kf=fimTexto(k);if(!kf)return -1;
    p=espaco(kf);if(*p++!=':')return -1;
    const char *v=espaco(p),*f=v;
    if(*v=='"')f=fimTexto(v);
    else if(*v=='{' || *v=='['){f=js_fim(v);if(f<=v || f[-1]!=(*v=='{'?'}':']'))return -1;}
    else{while(*f && *f!=',' && *f!='}' && *f!=']' && (unsigned char)*f>32)f++;if(f==v)return -1;}
    if(!f)return -1;
    char chave[128];if(!js_cadeia(k,chave,sizeof chave))return -1;
    if(!strcmp(chave,nome)) {
      if(achou++)return -1;
      *ini=v;*n=(size_t)(f-v);
    }
    p=espaco(f);if(*p=='}')return *espaco(p+1)?-1:achou;
    if(*p++!=',')return -1;
    p=espaco(p);if(*p!='"')return -1;
  }
}
static int texto(const char *s,const char *nome,char *out,size_t cap) {
  const char *p;size_t n;
  return campo(s,nome,&p,&n)==1 && n>=2 && *p=='"' && p[n-1]=='"' && js_cadeia(p,out,cap);
}
static int numero(const char *s,const char *nome,unsigned padrao,unsigned max,unsigned *out) {
  const char *p;size_t n;int tem=campo(s,nome,&p,&n);
  if(!tem){*out=padrao;return padrao>0;}
  if(tem<0 || !n || (n>1 && *p=='0'))return 0;
  unsigned v=0;
  for(size_t i=0;i<n;i++){if(p[i]<'0' || p[i]>'9' || v>(max-(unsigned)(p[i]-'0'))/10u)return 0;v=v*10u+(unsigned)(p[i]-'0');}
  if(!v || v>max)return 0;
  *out=v;return 1;
}
static int prazo(uint64_t agora,unsigned segundos,uint64_t *out) {
  uint64_t ms=(uint64_t)segundos*1000u;if(agora>UINT64_MAX-ms)return 0;
  *out=agora+ms;return 1;
}
static int executar(const DriveTv *c,const char *url,const char *nomes[],const char *valores[],int qtd,char **saida,int *status) {
  *saida=NULL;*status=0;
  if(c->job && rede_job_estado(c->job)!=REDE_OK)return DRIVE_TV_CANCELADO;
  size_t cap=1;char *campos[4]={0};int estado=DRIVE_TV_INVALIDO;
  for(int i=0;i<qtd;i++) {
    campos[i]=escapar(valores[i]);if(!campos[i])goto fim;
    cap+=strlen(nomes[i])+strlen(campos[i])+2;
  }
  char *corpo=malloc(cap);if(!corpo)goto fim;
  size_t pos=0;
  for(int i=0;i<qtd;i++) {
    int n=snprintf(corpo+pos,cap-pos,"%s%s=%s",i?"&":"",nomes[i],campos[i]);
    if(n<0 || (size_t)n>=cap-pos){free(corpo);goto fim;}pos+=(size_t)n;
  }
  const char *cab[]={"Content-Type: application/x-www-form-urlencoded",NULL};
  RedePedido p={.metodo="POST",.url=url,.cabecalhos=cab,.corpo=corpo,.n_corpo=pos,
    .max_bytes=RESPOSTA_MAX,.max_cabecalhos=16384,.prazo_ms=15000,.seguir=0,.job=c->job};
  RedeResposta r={0};int recebeu=c->pedir?c->pedir(c->ctx,&p,&r):rede_pedir(&p,&r);
  memset(corpo,0,pos);free(corpo);estado=DRIVE_TV_REDE;
  if(r.erro==REDE_CANCELADO || r.erro==REDE_GERACAO || (c->job && rede_job_estado(c->job)!=REDE_OK))estado=DRIVE_TV_CANCELADO;
  else if(recebeu && r.erro==REDE_OK) {
    *status=r.status;estado=DRIVE_TV_RESPOSTA;
    if(r.status==429 || r.status>=500)estado=DRIVE_TV_REPETIR;
    else if(r.corpo && r.n_corpo && r.n_corpo<=RESPOSTA_MAX && !memchr(r.corpo,0,r.n_corpo)) {
      *saida=malloc(r.n_corpo+1);
      if(*saida) {
        memcpy(*saida,r.corpo,r.n_corpo);(*saida)[r.n_corpo]=0;
        const char *v;size_t n;
        if(campo(*saida,"error",&v,&n)>=0)estado=DRIVE_TV_OK;
        else{memset(*saida,0,r.n_corpo);free(*saida);*saida=NULL;}
      }
    }
  }
  rede_resposta_limpar(&r);
fim:
  for(int i=0;i<4;i++)if(campos[i]){memset(campos[i],0,strlen(campos[i]));free(campos[i]);}
  return estado;
}
static int erro(const char *s) {
  char e[80];if(!texto(s,"error",e,sizeof e))return DRIVE_TV_RESPOSTA;
  if(!strcmp(e,"access_denied") || !strcmp(e,"admin_policy_enforced"))return DRIVE_TV_NEGADO;
  if(!strcmp(e,"expired_token") || !strcmp(e,"invalid_grant"))return DRIVE_TV_EXPIRADO;
  if(!strcmp(e,"invalid_client") || !strcmp(e,"unauthorized_client") || !strcmp(e,"org_internal") || !strcmp(e,"invalid_scope"))return DRIVE_TV_CONFIGURACAO;
  return DRIVE_TV_RESPOSTA;
}
static int temEscopo(const char *s) {
  size_t n=strlen(DRIVE_APPLICACAO_SCOPE);
  for(const char *p=s;*p;) {
    while(*p==' ')p++;
    const char *f=strchr(p,' ');if(!f)f=p+strlen(p);
    if((size_t)(f-p)==n && !memcmp(p,DRIVE_APPLICACAO_SCOPE,n))return 1;
    p=f;
  }
  return 0;
}
static int token(const char *s,uint64_t agora,const char *anterior,DriveTvToken *out) {
  DriveTvToken *t=calloc(1,sizeof *t);if(!t)return DRIVE_TV_RESPOSTA;
  char tipo[32],escopos[2048];unsigned segundos;const char *v;size_t n;int ok=0;
  int tem=campo(s,"scope",&v,&n);
  if(!texto(s,"access_token",t->acesso,sizeof t->acesso) || !ascii(t->acesso,4096) ||
     !texto(s,"token_type",tipo,sizeof tipo) || strcmp(tipo,"Bearer") ||
     !numero(s,"expires_in",0,86400,&segundos) || !prazo(agora,segundos,&t->expiraMs) ||
     tem<0 || ((!anterior || tem) && (!texto(s,"scope",escopos,sizeof escopos) || !temEscopo(escopos))))goto fim;
  tem=campo(s,"refresh_token",&v,&n);
  if(tem==0 && anterior)snprintf(t->renovacao,sizeof t->renovacao,"%s",anterior);
  else if(tem<0 || !texto(s,"refresh_token",t->renovacao,sizeof t->renovacao))goto fim;
  if(!ascii(t->renovacao,4096))goto fim;
  *out=*t;ok=1;
fim:
  memset(t,0,sizeof *t);free(t);return ok?DRIVE_TV_OK:DRIVE_TV_RESPOSTA;
}
int drivetv_iniciar(const DriveTv *c,uint64_t agora,DriveTvCodigo *out) {
  if(out)memset(out,0,sizeof *out);
  if(!out || !valido(c,0))return DRIVE_TV_INVALIDO;
  const char *nomes[]={"client_id","scope"},*valores[]={c->cliente,DRIVE_APPLICACAO_SCOPE};
  char *s=NULL;int status=0,ret=executar(c,GOOGLE_OAUTH "/device/code",nomes,valores,2,&s,&status);
  if(ret!=DRIVE_TV_OK)return ret;
  DriveTvCodigo codigo={0};unsigned segundos;
  if(status!=200)ret=erro(s);
  else if(!texto(s,"device_code",codigo.dispositivo,sizeof codigo.dispositivo) || !ascii(codigo.dispositivo,2048) ||
          !texto(s,"user_code",codigo.codigo,sizeof codigo.codigo) || !ascii(codigo.codigo,128) ||
          !texto(s,"verification_url",codigo.url,sizeof codigo.url) ||
          (strcmp(codigo.url,"https://www.google.com/device") && strcmp(codigo.url,"https://google.com/device")) ||
          !numero(s,"expires_in",0,86400,&segundos) || !numero(s,"interval",5,3600,&codigo.intervalo) ||
          !prazo(agora,segundos,&codigo.expiraMs) || !prazo(agora,codigo.intervalo,&codigo.proximaMs))ret=DRIVE_TV_RESPOSTA;
  else *out=codigo;
  memset(s,0,strlen(s));free(s);return ret;
}
int drivetv_consultar(const DriveTv *c,DriveTvCodigo *codigo,uint64_t agora,DriveTvToken *out) {
  if(out)memset(out,0,sizeof *out);
  if(!out || !valido(c,1) || !codigo || !ascii(codigo->dispositivo,2048) || !codigo->intervalo || codigo->intervalo>3600)return DRIVE_TV_INVALIDO;
  if(c->job && rede_job_estado(c->job)!=REDE_OK)return DRIVE_TV_CANCELADO;
  if(agora>=codigo->expiraMs)return DRIVE_TV_EXPIRADO;
  if(agora<codigo->proximaMs)return DRIVE_TV_AGUARDAR;
  if(!prazo(agora,codigo->intervalo,&codigo->proximaMs))return DRIVE_TV_INVALIDO;
  const char *nomes[]={"client_id","client_secret","device_code","grant_type"};
  const char *valores[]={c->cliente,c->segredo,codigo->dispositivo,"urn:ietf:params:oauth:grant-type:device_code"};
  char *s=NULL;int status=0,ret=executar(c,GOOGLE_OAUTH "/token",nomes,valores,4,&s,&status);
  if(ret==DRIVE_TV_REDE || ret==DRIVE_TV_REPETIR) {
    codigo->intervalo=codigo->intervalo>1800?3600:codigo->intervalo*2;
    prazo(agora,codigo->intervalo,&codigo->proximaMs);
  }
  if(ret!=DRIVE_TV_OK)return ret;
  if(status==200)ret=token(s,agora,NULL,out);
  else {
    char e[80];ret=erro(s);
    if(texto(s,"error",e,sizeof e)) {
      if(!strcmp(e,"authorization_pending"))ret=DRIVE_TV_AGUARDAR;
      else if(!strcmp(e,"slow_down")) {
        codigo->intervalo=codigo->intervalo>3595?3600:codigo->intervalo+5;
        prazo(agora,codigo->intervalo,&codigo->proximaMs);ret=DRIVE_TV_AGUARDAR;
      }
    }
  }
  memset(s,0,strlen(s));free(s);return ret;
}
int drivetv_renovar(const DriveTv *c,const char *anterior,uint64_t agora,DriveTvToken *out) {
  if(out)memset(out,0,sizeof *out);
  if(!out || !valido(c,1) || !ascii(anterior,4096))return DRIVE_TV_INVALIDO;
  const char *nomes[]={"client_id","client_secret","refresh_token","grant_type"};
  const char *valores[]={c->cliente,c->segredo,anterior,"refresh_token"};
  char *s=NULL;int status=0,ret=executar(c,GOOGLE_OAUTH "/token",nomes,valores,4,&s,&status);
  if(ret!=DRIVE_TV_OK)return ret;
  ret=status==200?token(s,agora,anterior,out):erro(s);
  memset(s,0,strlen(s));free(s);return ret;
}
