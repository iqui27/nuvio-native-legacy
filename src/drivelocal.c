#include "drivelocal.h"
#include "js.h"
#include "jsw.h"
#include "backupcrypto.h"
#include "backupfonts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define DRIVE_API "https://www.googleapis.com/drive/v3/files"
#define DRIVE_UPLOAD "https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id"
#define DRIVE_MIME "application/vnd.nuvio.local-settings"
#define DRIVE_BYTES (BACKUP_LOCAL_MAX + BACKUP_CRYPTO_OVERHEAD)
static int identificador(const char *s,size_t max) {
  if(!s || !*s)return 0;
  size_t n=0;
  for(;s[n];n++) {
    unsigned char c=(unsigned char)s[n];
    if(n>=max || !((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-'))return 0;
  }
  return 1;
}
static int valido(const DriveLocal *c) {
  if(!c || !c->token || !*c->token || !identificador(c->dono,80) || c->perfil<1)return 0;
  for(size_t i=0;c->token[i];i++)if(i>=4096 || (unsigned char)c->token[i]<=32 || (unsigned char)c->token[i]>=127)return 0;
  return 1;
}
static char *escapar(const char *s,size_t limite) {
  if(!s || strnlen(s,limite+1)>limite)return NULL;
  size_t n=strlen(s);char *out=malloc(n*3+1);if(!out)return NULL;
  static const char hex[]="0123456789ABCDEF";size_t k=0;
  for(size_t i=0;i<n;i++) {
    unsigned char c=(unsigned char)s[i];
    if((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='-' || c=='_' || c=='.' || c=='~')out[k++]=(char)c;
    else {out[k++]='%';out[k++]=hex[c>>4];out[k++]=hex[c&15];}
  }
  out[k]=0;return out;
}
static int executarDados(const DriveLocal *c,const char *metodo,const char *url,const char *tipo,
                    const char *corpo,size_t n,size_t max,int binario,char **saida,size_t *tamanho) {
  *saida=NULL;*tamanho=0;
  if(c->job && rede_job_estado(c->job)!=REDE_OK)return DRIVE_LOCAL_CANCELADO;
  char autorizacao[4120];snprintf(autorizacao,sizeof autorizacao,"Authorization: Bearer %s",c->token);
  const char *cab[]={autorizacao,tipo,NULL};
  RedePedido p={.metodo=metodo,.url=url,.cabecalhos=cab,.corpo=corpo,.n_corpo=n,
    .prazo_ms=30000,.max_bytes=max,.max_cabecalhos=16384,.seguir=0,.job=c->job};
  RedeResposta r={0};int recebeu=c->pedir?c->pedir(c->ctx,&p,&r):rede_pedir(&p,&r);
  int resultado=DRIVE_LOCAL_REDE;
  if(r.erro==REDE_CANCELADO || r.erro==REDE_GERACAO || (c->job && rede_job_estado(c->job)!=REDE_OK))resultado=DRIVE_LOCAL_CANCELADO;
  else if(recebeu && r.erro==REDE_OK) {
    if(r.status==401)resultado=DRIVE_LOCAL_AUTORIZAR;
    else if(r.status==403) {
      char motivo[80];
      resultado=DRIVE_LOCAL_NEGADO;
      if(r.corpo && r.n_corpo<=max && !memchr(r.corpo,0,r.n_corpo) &&
         js_texto(r.corpo,r.corpo+r.n_corpo,"reason",motivo,sizeof motivo) &&
         (!strcmp(motivo,"rateLimitExceeded") || !strcmp(motivo,"userRateLimitExceeded")))resultado=DRIVE_LOCAL_REPETIR;
    }
    else if(r.status==404)resultado=DRIVE_LOCAL_AUSENTE;
    else if(r.status==429 || r.status>=500)resultado=DRIVE_LOCAL_REPETIR;
    else if(r.status>=200 && r.status<300) {
      resultado=DRIVE_LOCAL_RESPOSTA;
      if(r.corpo && r.n_corpo && r.n_corpo<=max && (binario || !memchr(r.corpo,0,r.n_corpo))) {
        char *out=malloc(r.n_corpo+1);
        if(out){memcpy(out,r.corpo,r.n_corpo);out[r.n_corpo]=0;*saida=out;*tamanho=r.n_corpo;resultado=DRIVE_LOCAL_OK;}
      }
    } else resultado=DRIVE_LOCAL_RESPOSTA;
  }
  // The network layer never logs caller headers. No token/response logging here.
  memset(autorizacao,0,sizeof autorizacao);rede_resposta_limpar(&r);return resultado;
}
static int executar(const DriveLocal *c,const char *metodo,const char *url,const char *tipo,
                    const char *corpo,size_t n,size_t max,char **saida,size_t *tamanho) {
  return executarDados(c,metodo,url,tipo,corpo,n,max,0,saida,tamanho);
}
static void contexto(const DriveLocal *c,char out[128]) {
  snprintf(out,128,"%s:p%d:settings",c->dono,c->perfil);
}
static const char *pular(const char *p){while(*p && (unsigned char)*p<=32)p++;return p;}
static int objeto(const char *s) {
  s=pular(s);if(*s!='{')return 0;
  const char *fim=js_fim(s);return fim>s && fim[-1]=='}' && !*pular(fim);
}
static int arquivo(const DriveLocal *c,const char *s,const char *fim,DriveBackup *a) {
  char propriedades[512],dono[81],perfil[16],atual[16],versao[8],mime[80],tamanho[24],data[64];
  if(!js_bruto(s,fim,"appProperties",propriedades,sizeof propriedades) || !objeto(propriedades) ||
     !js_texto_raiz(propriedades,"nuvioBackupVersion",versao,sizeof versao) || (strcmp(versao,"1") && strcmp(versao,"2")) ||
     !js_texto_raiz(propriedades,"nuvioOwner",dono,sizeof dono) || strcmp(dono,c->dono) ||
     !js_texto_raiz(propriedades,"nuvioProfile",perfil,sizeof perfil))return 0;
  snprintf(atual,sizeof atual,"%d",c->perfil);if(strcmp(perfil,atual))return 0;
  memset(a,0,sizeof *a);
  a->criptografado=!strcmp(versao,"2");
  if(!js_texto_raiz_em(s,fim,"id",a->id,sizeof a->id) || !identificador(a->id,sizeof a->id-1) ||
     !js_texto_raiz_em(s,fim,"name",a->nome,sizeof a->nome) ||
     !js_texto_raiz_em(s,fim,"mimeType",mime,sizeof mime) || strcmp(mime,DRIVE_MIME) ||
     !js_texto_raiz_em(s,fim,"size",tamanho,sizeof tamanho) ||
     !js_texto_raiz_em(s,fim,"createdTime",data,sizeof data))return -1;
  size_t bytes=0;
  if(!*tamanho)return -1;
  for(const char *p=tamanho;*p;p++) {
    if(*p<'0' || *p>'9' || bytes>(DRIVE_BYTES-(unsigned)(*p-'0'))/10u)return -1;
    bytes=bytes*10u+(unsigned)(*p-'0');
  }
  a->criadoMs=js_ms_iso(data);if(!bytes || a->criadoMs<=0)return -1;
  a->bytes=bytes;return 1;
}
int drivelocal_pasta(const DriveLocal *c,int criar,char id[200]) {
  if(id)*id=0;
  if(!id || !valido(c))return DRIVE_LOCAL_INVALIDO;
  const char consulta[]="trashed=false and mimeType='application/vnd.google-apps.folder' and 'me' in owners and appProperties has { key='nuvioBackupFolder' and value='1' }";
  char *q=escapar(consulta,sizeof consulta-1);if(!q)return DRIVE_LOCAL_INVALIDO;
  char url[1024];int tamanho=snprintf(url,sizeof url,DRIVE_API
    "?spaces=drive&pageSize=1&orderBy=createdTime&q=%s&fields=files(id,mimeType,appProperties)",q);free(q);
  if(tamanho<0 || (size_t)tamanho>=sizeof url)return DRIVE_LOCAL_INVALIDO;
  char *s=NULL;size_t n=0;int resultado=executar(c,"GET",url,NULL,NULL,0,16384,&s,&n);
  if(resultado!=DRIVE_LOCAL_OK)return resultado;
  char lista[16384],mime[80],props[512],marca[8],arquivoId[200];
  resultado=DRIVE_LOCAL_RESPOSTA;
  if(!objeto(s) || !js_bruto(s,NULL,"files",lista,sizeof lista) || *lista!='[')goto fim;
  const char *p=pular(lista+1);
  if(*p!=']') {
    const char *f=js_fim(p);
    if(*p!='{' || f<=p || f[-1]!='}' || *pular(f)!=']' || *pular(pular(f)+1) ||
       !js_texto_raiz_em(p,f,"id",arquivoId,sizeof arquivoId) || !identificador(arquivoId,199) ||
       !js_texto_raiz_em(p,f,"mimeType",mime,sizeof mime) || strcmp(mime,"application/vnd.google-apps.folder") ||
       !js_bruto(p,f,"appProperties",props,sizeof props) || !objeto(props) ||
       !js_texto_raiz(props,"nuvioBackupFolder",marca,sizeof marca) || strcmp(marca,"1"))goto fim;
    strcpy(id,arquivoId);resultado=DRIVE_LOCAL_OK;goto fim;
  }
  if(*pular(p+1))goto fim;
  resultado=DRIVE_LOCAL_AUSENTE;
  if(criar) {
    free(s);s=NULL;
    const char corpo[]="{\"name\":\"Nuvio Backups\",\"mimeType\":\"application/vnd.google-apps.folder\",\"appProperties\":{\"nuvioBackupFolder\":\"1\"}}";
    resultado=executar(c,"POST",DRIVE_API "?fields=id","Content-Type: application/json; charset=UTF-8",
                       corpo,sizeof corpo-1,16384,&s,&n);
    if(resultado==DRIVE_LOCAL_OK) {
      if(!objeto(s) || !js_texto_raiz(s,"id",arquivoId,sizeof arquivoId) || !identificador(arquivoId,199))resultado=DRIVE_LOCAL_RESPOSTA;
      else strcpy(id,arquivoId);
    }
  }
fim:
  free(s);return resultado;
}
int drivelocal_listar(const DriveLocal *c,const char *pagina,DrivePagina *saida) {
  if(saida)memset(saida,0,sizeof *saida);
  if(!saida || !valido(c))return DRIVE_LOCAL_INVALIDO;
  char consulta[512];snprintf(consulta,sizeof consulta,
    "trashed=false and (appProperties has { key='nuvioBackupVersion' and value='1' } or appProperties has { key='nuvioBackupVersion' and value='2' }) and appProperties has { key='nuvioOwner' and value='%s' } and appProperties has { key='nuvioProfile' and value='%d' }",c->dono,c->perfil);
  char *q=escapar(consulta,511),*pg=escapar(pagina?pagina:"",4095);if(!q || !pg){free(q);free(pg);return DRIVE_LOCAL_INVALIDO;}
  size_t cap=strlen(q)+strlen(pg)+512;char *url=malloc(cap);if(!url){free(q);free(pg);return DRIVE_LOCAL_INVALIDO;}
  snprintf(url,cap,DRIVE_API "?spaces=drive&pageSize=50&orderBy=createdTime%%20desc&q=%s&fields=nextPageToken,files(id,name,mimeType,size,createdTime,appProperties)%s%s",q,*pg?"&pageToken=":"",pg);
  free(q);free(pg);char *corpo=NULL;size_t n=0;int resultado=executar(c,"GET",url,NULL,NULL,0,128u*1024u,&corpo,&n);free(url);
  if(resultado!=DRIVE_LOCAL_OK)return resultado;
  DrivePagina *out=calloc(1,sizeof *out);char *lista=malloc(n+1);
  resultado=DRIVE_LOCAL_RESPOSTA;
  if(!out || !lista || !objeto(corpo) || !js_bruto(corpo,NULL,"files",lista,n+1) || *lista!='[')goto fim;
  if(js_tem(corpo,NULL,"nextPageToken") && !js_texto_raiz(corpo,"nextPageToken",out->proxima,sizeof out->proxima))goto fim;
  const char *p=pular(lista+1);int vistos=0;
  while(*p!=']') {
    if(*p!='{' || vistos++>=DRIVE_BACKUPS_PAGINA)goto fim;
    const char *f=js_fim(p);if(f<=p || f[-1]!='}')goto fim;
    DriveBackup a;int estado=arquivo(c,p,f,&a);if(estado<0)goto fim;
    if(estado)out->arquivo[out->n++]=a;
    p=pular(f);if(*p==']')break;
    if(*p++!=',')goto fim;
    p=pular(p);if(*p==']')goto fim;
  }
  if(*pular(p+1))goto fim;
  *saida=*out;resultado=DRIVE_LOCAL_OK;
fim:
  free(lista);free(out);free(corpo);return resultado;
}
/* Search exact spans: encrypted bodies contain NUL and are not C strings. */
static int contem(const void *dados,size_t n,const char *s) {
  size_t k=strlen(s);const unsigned char *p=dados;
  if(k>n)return 0;
  for(size_t i=0;i<=n-k;i++)if(!memcmp(p+i,s,k))return 1;
  return 0;
}
static int enviarCifrado(const DriveLocal *c,unsigned char *cifrado,size_t nc,const char *nome,const char *propriedade,const char *versao,char id[200]) {
  char pasta[200];int resultado=drivelocal_pasta(c,1,pasta);
  if(resultado!=DRIVE_LOCAL_OK){free(cifrado);return resultado;}
  char perfil[16],fronteira[80];
  snprintf(perfil,sizeof perfil,"%d",c->perfil);
  unsigned k=0;
  do{snprintf(fronteira,sizeof fronteira,"nuvio-local-settings-%u",k++);}while(contem(cifrado,nc,fronteira) && k<1000);
  if(contem(cifrado,nc,fronteira)){free(cifrado);return DRIVE_LOCAL_INVALIDO;}
  Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);jsw_cs(&w,"name",nome);jsw_cs(&w,"mimeType",DRIVE_MIME);
  jsw_chave(&w,"parents");jsw_arr_ini(&w);jsw_str(&w,pasta);jsw_arr_fim(&w);
  jsw_chave(&w,"appProperties");jsw_obj_ini(&w);jsw_cs(&w,propriedade,versao);
  jsw_cs(&w,"nuvioOwner",c->dono);jsw_cs(&w,"nuvioProfile",perfil);jsw_obj_fim(&w);jsw_obj_fim(&w);
  const char *meta=jsw_texto_final(&w);char *corpo=NULL;
  size_t cap=meta?strlen(meta)+nc+512:0;
  if(meta)corpo=malloc(cap);
  if(!corpo){jsw_livre(&w);free(cifrado);return DRIVE_LOCAL_INVALIDO;}
  int inicio=snprintf(corpo,cap,"--%s\r\nContent-Type: application/json; charset=UTF-8\r\n\r\n%s\r\n--%s\r\nContent-Type: " DRIVE_MIME "\r\n\r\n",fronteira,meta,fronteira);
  if(inicio<0 || (size_t)inicio+nc>=cap){free(corpo);jsw_livre(&w);free(cifrado);return DRIVE_LOCAL_INVALIDO;}
  jsw_livre(&w);memcpy(corpo+inicio,cifrado,nc);free(cifrado);
  size_t pos=(size_t)inicio+nc;
  int fim=snprintf(corpo+pos,cap-pos,"\r\n--%s--\r\n",fronteira);
  if(fim<0 || (size_t)fim>=cap-pos){free(corpo);return DRIVE_LOCAL_INVALIDO;}
  char tipo[128];snprintf(tipo,sizeof tipo,"Content-Type: multipart/related; boundary=%s",fronteira);
  char *resposta=NULL;size_t nr=0;
  resultado=executar(c,"POST",DRIVE_UPLOAD,tipo,corpo,(size_t)inicio+nc+(size_t)fim,16384,&resposta,&nr);free(corpo);
  if(resultado==DRIVE_LOCAL_OK) {
    char arquivo[200];
    if(!objeto(resposta) || !js_texto_raiz(resposta,"id",arquivo,sizeof arquivo) || !identificador(arquivo,sizeof arquivo-1))resultado=DRIVE_LOCAL_RESPOSTA;
    else strcpy(id,arquivo);
  }
  free(resposta);return resultado;
}
int drivelocal_enviar(const DriveLocal *c,const char *snapshot,size_t n,char id[200]) {
  if(id)*id=0;
  if(!id || !valido(c))return DRIVE_LOCAL_INVALIDO;
  if(!c->senha || !*c->senha || !backupcrypto_disponivel())return DRIVE_LOCAL_SENHA;
  BackupLocal *b=NULL;if(!backuplocal_ler(snapshot,n,c->dono,c->perfil,&b))return DRIVE_LOCAL_INVALIDO;
  char ctx[128];contexto(c,ctx);unsigned char *cifrado=NULL;size_t nc=0;
  long long criado=backuplocal_criado(b);backuplocal_liberar(b);
  if(!backupcrypto_selar(snapshot,n,c->senha,strlen(c->senha),ctx,&cifrado,&nc))return DRIVE_LOCAL_SENHA;
  char nome[128];snprintf(nome,sizeof nome,"Nuvio-settings-p%d-%lld.nvbackup",c->perfil,criado);
  return enviarCifrado(c,cifrado,nc,nome,"nuvioBackupVersion","2",id);
}
static int assetContexto(const DriveLocal *c,const char *font,int chunk,char out[160]) {
  if(!valido(c) || !backupfonts_id(font) || chunk<0 || chunk>=BACKUP_FONT_CHUNKS)return 0;
  snprintf(out,160,"%s:p%d:font:%s:%d",c->dono,c->perfil,font,chunk);return 1;
}
int drivelocal_asset_enviar(const DriveLocal *c,const char *font,int chunk,const void *dados,size_t n,char id[200]) {
  if(id)*id=0;
  char ctx[160];if(!id || !assetContexto(c,font,chunk,ctx) || !dados || !n || n>BACKUP_FONT_CHUNK)return DRIVE_LOCAL_INVALIDO;
  if(!c->senha || !*c->senha)return DRIVE_LOCAL_SENHA;
  unsigned char *cifrado=NULL;size_t nc;
  if(!backupcrypto_selar(dados,n,c->senha,strlen(c->senha),ctx,&cifrado,&nc))return DRIVE_LOCAL_SENHA;
  char nome[80];snprintf(nome,sizeof nome,"Nuvio-font-%s-%d.nvbackup",font,chunk);
  return enviarCifrado(c,cifrado,nc,nome,"nuvioBackupAsset","1",id);
}
int drivelocal_asset_baixar(const DriveLocal *c,const char *font,int chunk,const char *id,unsigned char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  char ctx[160];if(!out || !n || !assetContexto(c,font,chunk,ctx) || !identificador(id,199))return DRIVE_LOCAL_INVALIDO;
  char url[512];snprintf(url,sizeof url,DRIVE_API "/%s?alt=media",id);
  char *dados=NULL;size_t nd=0;int r=executarDados(c,"GET",url,NULL,NULL,0,BACKUP_FONT_CHUNK+BACKUP_CRYPTO_OVERHEAD,1,&dados,&nd);
  if(r==DRIVE_LOCAL_OK && (!c->senha || !backupcrypto_abrir(dados,nd,c->senha,strlen(c->senha),ctx,out,n)))r=DRIVE_LOCAL_SENHA;
  free(dados);return r;
}
int drivelocal_baixar(const DriveLocal *c,const char *id,BackupLocal **saida) {
  if(saida)*saida=NULL;
  if(!saida || !valido(c) || !identificador(id,199))return DRIVE_LOCAL_INVALIDO;
  char url[512];snprintf(url,sizeof url,DRIVE_API "/%s?alt=media",id);
  char *corpo=NULL;size_t n=0;int resultado=executarDados(c,"GET",url,NULL,NULL,0,DRIVE_BYTES,1,&corpo,&n);
  if(resultado==DRIVE_LOCAL_OK) {
    char ctx[128];contexto(c,ctx);
    if(c->exigirCriptografia || (n>=16 && !memcmp(corpo,"NUVIO-AES256GCM1",16))) {
      unsigned char *aberto=NULL;size_t na=0;
      if(!c->senha || !backupcrypto_abrir(corpo,n,c->senha,strlen(c->senha),ctx,&aberto,&na))resultado=DRIVE_LOCAL_SENHA;
      else {
        if(!backuplocal_ler((const char *)aberto,na,c->dono,c->perfil,saida))resultado=DRIVE_LOCAL_RESPOSTA;
        backupcrypto_limpar(aberto,na);free(aberto);
      }
    } else {
      if(!backuplocal_ler(corpo,n,c->dono,c->perfil,saida))resultado=DRIVE_LOCAL_RESPOSTA;
      // Only the original preference-only preview used plaintext. Expanded
      // local connections/hardware may never be admitted through that path.
      else if(backuplocal_parte(*saida,BACKUP_LOCAIS,NULL) || backuplocal_parte(*saida,BACKUP_AJUSTES_HARDWARE,NULL) || backuplocal_parte(*saida,BACKUP_FONTES_IMPORTADAS,NULL)) {
        backuplocal_liberar(*saida);*saida=NULL;resultado=DRIVE_LOCAL_RESPOSTA;
      }
    }
  }
  backupcrypto_limpar(corpo,n);free(corpo);return resultado;
}
