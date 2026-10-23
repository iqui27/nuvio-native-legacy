// Drive protocol checks use a mock transport and the real network job,
// envelope and JSON modules. No Google API/account sign-in occurs here.
#define _GNU_SOURCE 1
#define _POSIX_C_SOURCE 200809L
#include "drivelocal.h"
#include "backupcrypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define META "{\"id\":\"Backup_1\",\"name\":\"設定 Türkçe\",\"mimeType\":\"application/vnd.nuvio.local-settings\",\"size\":\"256\",\"createdTime\":\"2026-10-18T10:30:00Z\",\"appProperties\":{\"nuvioBackupVersion\":\"1\",\"nuvioOwner\":\"user-1\",\"nuvioProfile\":\"7\"}}"
#define PASTA "{\"files\":[{\"id\":\"Folder_1\",\"mimeType\":\"application/vnd.google-apps.folder\",\"appProperties\":{\"nuvioBackupFolder\":\"1\"}}]}"
typedef struct {
  const char *resposta,*snapshot;size_t n,ns;int status,chamadas,operacao,transporte,cancelar;
  RedeErro erro;
  const char *pasta,*criada;int pastaChamadas,criarChamadas,pastaStatus,criarStatus;
  char url[20000];
} Mock;
static int pedir(void *ctx,const RedePedido *p,RedeResposta *r) {
  Mock *m=ctx;snprintf(m->url,sizeof m->url,"%s",p->url);
  assert(!strncmp(p->url,"https://www.googleapis.com/",27));
  assert(!p->seguir && !p->tls_pessoal && p->prazo_ms==30000 && p->max_cabecalhos==16384);
  assert(p->cabecalhos && !strcmp(p->cabecalhos[0],"Authorization: Bearer test-token"));
  assert(!strstr(p->url,"test-token"));
  if(strstr(p->url,"pageSize=1&") || !strcmp(p->url,"https://www.googleapis.com/drive/v3/files?fields=id")) {
    const char *body;
    if(!strcmp(p->metodo,"GET")) {
      m->pastaChamadas++;assert(strstr(p->url,"spaces=drive") && strstr(p->url,"nuvioBackupFolder"));
      assert(!p->corpo && !p->cabecalhos[1]);body=m->pasta?m->pasta:PASTA;r->status=m->pastaStatus?m->pastaStatus:200;
    } else {
      m->criarChamadas++;assert(!strcmp(p->metodo,"POST") && p->corpo);
      assert(!strcmp(p->cabecalhos[1],"Content-Type: application/json; charset=UTF-8"));
      assert(strstr(p->corpo,"\"name\":\"Nuvio Backups\"") && strstr(p->corpo,"nuvioBackupFolder"));
      assert(!strstr(p->corpo,"permissions") && !strstr(p->corpo,"anyone"));
      body=m->criada?m->criada:"{\"id\":\"Folder_1\"}";r->status=m->criarStatus?m->criarStatus:200;
    }
    r->corpo=strdup(body);assert(r->corpo);r->n_corpo=strlen(body);return 1;
  }
  m->chamadas++;
  if(m->operacao==1) {
    assert(!strcmp(p->metodo,"POST"));
    assert(!strcmp(p->url,"https://www.googleapis.com/upload/drive/v3/files?uploadType=multipart&fields=id"));
    assert(p->max_bytes==16384 && p->corpo && p->n_corpo>m->ns);
    assert(p->cabecalhos[1] && strstr(p->cabecalhos[1],"Content-Type: multipart/related; boundary="));
    const char *s=p->corpo;assert(!strstr(s,"test-token") && !strstr(s,"backup-password"));
    assert(strstr(s,"\"parents\":[\"Folder_1\"]") && strstr(s,"\"nuvioOwner\":\"user-1\""));
    assert(strstr(s,"\"nuvioProfile\":\"7\"") && strstr(s,"Nuvio-settings-p7-1792319400.nvbackup"));
    assert(strstr(s,"\"nuvioBackupVersion\":\"2\""));
    const char *payload=strstr(s,"NUVIO-AES256GCM1");assert(payload);
    size_t span=p->n_corpo-(size_t)(payload-s);
    char tail[160];snprintf(tail,sizeof tail,"\r\n--%s--\r\n",strstr(p->cabecalhos[1],"boundary=")+9);
    assert(span>strlen(tail) && !memcmp(s+p->n_corpo-strlen(tail),tail,strlen(tail)));
    span-=strlen(tail);
    assert(!strstr(s,m->snapshot));
    unsigned char *decoded=NULL;size_t nd=0;
    assert(backupcrypto_abrir(payload,span,"backup-password",15,"user-1:p7:settings",&decoded,&nd));
    assert(nd==m->ns && !memcmp(decoded,m->snapshot,nd));backupcrypto_limpar(decoded,nd);free(decoded);
  } else {
    assert(!strcmp(p->metodo,"GET") && !p->corpo && !p->n_corpo && !p->cabecalhos[1]);
    assert(p->max_bytes==(m->operacao==2?BACKUP_LOCAL_MAX+BACKUP_CRYPTO_OVERHEAD:128u*1024u));
  }
  if(m->cancelar){assert(p->job);rede_job_cancelar(p->job);}
  memset(r,0,sizeof *r);r->status=m->status;r->erro=m->erro;
  if(m->resposta) {
    r->n_corpo=m->n?m->n:strlen(m->resposta);r->corpo=malloc(r->n_corpo+1);assert(r->corpo);
    memcpy(r->corpo,m->resposta,r->n_corpo);r->corpo[r->n_corpo]=0;
  }
  return m->transporte;
}
static void configurar(Mock *m,const char *resposta,int operacao) {
  const char *s=m->snapshot;size_t n=m->ns;
  memset(m,0,sizeof *m);m->snapshot=s;m->ns=n;m->resposta=resposta;m->status=200;m->operacao=operacao;m->transporte=1;
}
int main(void) {
  BackupLocal *b=backuplocal_novo("user-1",7,1792319400);assert(b);
  const char texto[]="NUVIO-LOCAL-SETTINGS 1\nscope profile\n";
  assert(backuplocal_adicionar(b,BACKUP_AJUSTES_PERFIL,texto,sizeof texto-1));
  char *snapshot=NULL;size_t ns=0;assert(backuplocal_exportar(b,&snapshot,&ns));backuplocal_liberar(b);
  Mock m={.snapshot=snapshot,.ns=ns};DriveLocal c={.token="test-token",.dono="user-1",.perfil=7,.pedir=pedir,.ctx=&m,.senha="backup-password"};
  char id[200];DrivePagina *pagina=malloc(sizeof *pagina);assert(pagina);
  configurar(&m,NULL,0);
  assert(drivelocal_pasta(&c,0,id)==DRIVE_LOCAL_OK && !strcmp(id,"Folder_1") && m.pastaChamadas==1 && !m.criarChamadas);
  configurar(&m,NULL,0);m.pasta="{\"files\":[]}";
  assert(drivelocal_pasta(&c,0,id)==DRIVE_LOCAL_AUSENTE && !*id && !m.criarChamadas);
  assert(drivelocal_pasta(&c,1,id)==DRIVE_LOCAL_OK && !strcmp(id,"Folder_1") && m.criarChamadas==1);
  configurar(&m,NULL,0);m.pasta="{\"files\":[]}";m.criarStatus=503;
  assert(drivelocal_pasta(&c,1,id)==DRIVE_LOCAL_REPETIR && !*id && m.criarChamadas==1);
  configurar(&m,NULL,0);m.pasta="{\"files\":[{\"id\":\"WrongFolder\",\"mimeType\":\"application/vnd.google-apps.folder\",\"appProperties\":{\"nuvioBackupFolder\":\"0\"}}]}";
  assert(drivelocal_pasta(&c,1,id)==DRIVE_LOCAL_RESPOSTA && !*id && !m.criarChamadas);
  configurar(&m,NULL,0);m.pastaStatus=403;
  assert(drivelocal_pasta(&c,1,id)==DRIVE_LOCAL_NEGADO && !*id && !m.criarChamadas);
  configurar(&m,"{\"nested\":{\"id\":\"wrong\"},\"id\":\"Backup_1\"}",1);
  assert(drivelocal_enviar(&c,snapshot,ns,id)==DRIVE_LOCAL_OK && !strcmp(id,"Backup_1") && m.chamadas==1);
  assert(m.pastaChamadas==1 && !m.criarChamadas);
  char *semNul=malloc(ns);assert(semNul);memcpy(semNul,snapshot,ns);
  assert(drivelocal_enviar(&c,semNul,ns,id)==DRIVE_LOCAL_OK && !strcmp(id,"Backup_1"));free(semNul);
  configurar(&m,"{\"id\":\"../bad\"}",1);
  assert(drivelocal_enviar(&c,snapshot,ns,id)==DRIVE_LOCAL_RESPOSTA && !*id);
  configurar(&m,NULL,1);m.transporte=0;m.erro=REDE_PRAZO;
  assert(drivelocal_enviar(&c,snapshot,ns,id)==DRIVE_LOCAL_REDE && m.chamadas==1 && !*id);
  configurar(&m,"{\"files\":[" META "],\"nextPageToken\":\"a+/= &?\"}",0);
  assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_OK && pagina->n==1);
  assert(!strcmp(pagina->arquivo[0].id,"Backup_1") && !strcmp(pagina->arquivo[0].nome,"設定 Türkçe"));
  assert(pagina->arquivo[0].bytes==256 && pagina->arquivo[0].criadoMs>0 && !strcmp(pagina->proxima,"a+/= &?"));
  assert(strstr(m.url,"spaces=drive") && strstr(m.url,"orderBy=createdTime%20desc") && !strstr(m.url,"pageToken="));
  assert(strstr(m.url,"nuvioOwner") && strstr(m.url,"user-1") && strstr(m.url,"nuvioProfile"));
  configurar(&m,"{\"files\":[]}",0);
  assert(drivelocal_listar(&c,"a+/= &?",pagina)==DRIVE_LOCAL_OK && !pagina->n && !*pagina->proxima);
  assert(strstr(m.url,"pageToken=a%2B%2F%3D%20%26%3F"));
  configurar(&m,"{\"files\":[{\"id\":\"foreign\"}," META "]}",0);
  assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_OK && pagina->n==1);
  const char *invalidos[]={"{}","{\"files\":null}","{\"files\":[}","{\"files\":[" META ",]}","{\"files\":[" META ",false]}","{\"files\":[]} garbage","{\"files\":[],\"nextPageToken\":7}"};
  for(size_t i=0;i<sizeof invalidos/sizeof *invalidos;i++) {
    configurar(&m,invalidos[i],0);memset(pagina,0xff,sizeof *pagina);
    assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_RESPOSTA && !pagina->n && !*pagina->proxima);
  }
  // An invalid matching entry must not expose a partially parsed page.
  char ruim[2048];snprintf(ruim,sizeof ruim,"{\"files\":[%s,%s]}",META,META);
  char *segundo=strstr(strstr(ruim,"\"size\":\"256\"")+1,"\"size\":\"256\"");assert(segundo);
  segundo[8]='x';configurar(&m,ruim,0);
  assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_RESPOSTA && !pagina->n);
  configurar(&m,"{\"files\":[" META "]}",0);DriveLocal outra=c;outra.dono="user-2";
  assert(drivelocal_listar(&outra,NULL,pagina)==DRIVE_LOCAL_OK && !pagina->n);
  outra=c;outra.perfil=8;assert(drivelocal_listar(&outra,NULL,pagina)==DRIVE_LOCAL_OK && !pagina->n);
  int status[]={401,403,404,429,500,302};
  int esperado[]={DRIVE_LOCAL_AUTORIZAR,DRIVE_LOCAL_NEGADO,DRIVE_LOCAL_AUSENTE,DRIVE_LOCAL_REPETIR,DRIVE_LOCAL_REPETIR,DRIVE_LOCAL_RESPOSTA};
  for(size_t i=0;i<sizeof status/sizeof *status;i++) {
    configurar(&m,"{}",0);m.status=status[i];
    assert(drivelocal_listar(&c,NULL,pagina)==esperado[i] && m.chamadas==1 && !pagina->n);
  }
  configurar(&m,"{\"error\":{\"errors\":[{\"reason\":\"userRateLimitExceeded\"}]}}",0);m.status=403;
  assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_REPETIR);
  configurar(&m,snapshot,2);BackupLocal *baixado=NULL;
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_OK && baixado);
  assert(!strcmp(m.url,"https://www.googleapis.com/drive/v3/files/Backup_1?alt=media"));backuplocal_liberar(baixado);baixado=NULL;
  assert(drivelocal_baixar(&outra,"Backup_1",&baixado)==DRIVE_LOCAL_RESPOSTA && !baixado);
  snapshot[ns/2]^=1;assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_RESPOSTA && !baixado);snapshot[ns/2]^=1;
  // Encrypted wire data must be decrypted before native parsing. Wrong
  // passwords, truncated tags and a downgraded v2 file never return a snapshot.
  unsigned char *sealed=NULL;size_t sealedN=0;
  assert(backupcrypto_selar(snapshot,ns,c.senha,strlen(c.senha),"user-1:p7:settings",&sealed,&sealedN));
  configurar(&m,(const char *)sealed,2);m.n=sealedN;c.exigirCriptografia=1;
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_OK && baixado);
  backuplocal_liberar(baixado);baixado=NULL;
  c.senha="wrong-password";
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_SENHA && !baixado);
  c.senha="backup-password";sealed[sealedN-1]^=1;
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_SENHA && !baixado);
  sealed[sealedN-1]^=1;m.n--;
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_SENHA && !baixado);
  configurar(&m,snapshot,2);
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_SENHA && !baixado);
  c.exigirCriptografia=0;free(sealed);
  configurar(&m,NULL,1);c.senha=NULL;
  assert(drivelocal_enviar(&c,snapshot,ns,id)==DRIVE_LOCAL_SENHA && !m.chamadas && !m.pastaChamadas);
  c.senha="backup-password";
  configurar(&m,snapshot,2);int antes=m.chamadas;
  assert(drivelocal_baixar(&c,"../escape?alt=media",&baixado)==DRIVE_LOCAL_INVALIDO && !baixado && m.chamadas==antes);
  outra=c;outra.token="token\r\nX-Injected: yes";
  assert(drivelocal_listar(&outra,NULL,pagina)==DRIVE_LOCAL_INVALIDO && !m.chamadas);
  outra=c;outra.dono="bad'query";assert(drivelocal_listar(&outra,NULL,pagina)==DRIVE_LOCAL_INVALIDO && !m.chamadas);
  outra=c;outra.dono="user-2";assert(drivelocal_enviar(&outra,snapshot,ns,id)==DRIVE_LOCAL_INVALIDO && !*id && !m.chamadas);
  char longa[4097];memset(longa,'x',sizeof longa-1);longa[sizeof longa-1]=0;
  assert(drivelocal_listar(&c,longa,pagina)==DRIVE_LOCAL_INVALIDO && !m.chamadas);
  char *grande=malloc(BACKUP_LOCAL_MAX+2u);assert(grande);memset(grande,'x',BACKUP_LOCAL_MAX+1u);grande[BACKUP_LOCAL_MAX+1u]=0;
  configurar(&m,grande,2);
  assert(drivelocal_baixar(&c,"Backup_1",&baixado)==DRIVE_LOCAL_RESPOSTA && !baixado);free(grande);
  configurar(&m,"{}\0extra",0);m.n=8;assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_RESPOSTA);
  RedeGrupo *grupo=rede_grupo_criar();assert(grupo);c.job=rede_job_criar(grupo);assert(c.job);
  configurar(&m,"{\"files\":[]}",0);m.cancelar=1;
  assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_CANCELADO && m.chamadas==1 && !pagina->n);
  m.chamadas=0;assert(drivelocal_listar(&c,NULL,pagina)==DRIVE_LOCAL_CANCELADO && !m.chamadas);
  rede_job_soltar(c.job);rede_grupo_soltar(grupo);free(pagina);free(snapshot);
  puts("PASS: Drive app-owned visible folder, no automatic sharing, multipart snapshots, pagination, owner/profile isolation, bounded responses, HTTP failures, no automatic POST retry and cancellation (mock transport)");
}
