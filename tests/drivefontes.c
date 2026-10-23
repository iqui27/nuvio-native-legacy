#define _POSIX_C_SOURCE 200809L
#include "drivefontes.h"
#include "backupfonts.h"
#include "backupstores.h"
#include "backupcrypto.h"
#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct {unsigned char *data;size_t n;} File;
typedef struct {File files[40];int n,posts,gets;} Mock;
static int contains(const void *s,size_t n,const char *str) {
  size_t k=strlen(str);if(k>n)return 0;
  for(size_t i=0;i<=n-k;i++)if(!memcmp((const char *)s+i,str,k))return 1;
  return 0;
}
static int pedir(void *ctx,const RedePedido *p,RedeResposta *r) {
  Mock *m=ctx;const char *body=NULL;char reply[256];
  assert(!p->seguir && !strcmp(p->cabecalhos[0],"Authorization: Bearer fixture-token"));
  if(strstr(p->url,"pageSize=1&"))body="{\"files\":[{\"id\":\"Folder_1\",\"mimeType\":\"application/vnd.google-apps.folder\",\"appProperties\":{\"nuvioBackupFolder\":\"1\"}}]}";
  else if(!strcmp(p->metodo,"POST")) {
    m->posts++;assert(m->n<40 && p->corpo && p->n_corpo);
    assert(!contains(p->corpo,p->n_corpo,"fixture-backup-password"));
    assert(!contains(p->corpo,p->n_corpo,"local-provider-fixture-key"));
    const char *binary=strstr(p->corpo,"NUVIO-AES256GCM1");assert(binary);
    const char *boundary=strstr(p->cabecalhos[1],"boundary=");assert(boundary);
    char tail[160];snprintf(tail,sizeof tail,"\r\n--%s--\r\n",boundary+9);
    size_t n=p->n_corpo-(size_t)(binary-(const char *)p->corpo)-strlen(tail);
    assert(!memcmp((const char *)p->corpo+p->n_corpo-strlen(tail),tail,strlen(tail)));
    File *f=&m->files[m->n++];f->n=n;f->data=malloc(n);assert(f->data);memcpy(f->data,binary,n);
    snprintf(reply,sizeof reply,"{\"id\":\"File_%d\"}",m->n);body=reply;
  } else {
    int id=0;assert(sscanf(p->url,"https://www.googleapis.com/drive/v3/files/File_%d?alt=media",&id)==1 && id>0 && id<=m->n);
    m->gets++;File *f=&m->files[id-1];r->corpo=malloc(f->n+1);assert(r->corpo);memcpy(r->corpo,f->data,f->n);r->corpo[f->n]=0;r->n_corpo=f->n;r->status=200;return 1;
  }
  r->corpo=strdup(body);assert(r->corpo);r->n_corpo=strlen(body);r->status=200;return 1;
}
static void hashId(const unsigned char *b,size_t n,char id[21]) {
  uint64_t h=UINT64_C(14695981039346656037);for(size_t i=0;i<n;i++)h=(h^b[i])*UINT64_C(1099511628211);
  snprintf(id,21,"%016llx.ttf",(unsigned long long)h);
}
int main(int argc,char **argv) {
  assert(argc==3);char src[4096],dst[4096],fontdir[4096],path[4096];
  assert(snprintf(src,sizeof src,"%s/source",argv[1])>0 && !mkdir(src,0700));
  assert(snprintf(dst,sizeof dst,"%s/destination",argv[1])>0 && !mkdir(dst,0700));
  assert(snprintf(fontdir,sizeof fontdir,"%s/fontes-importadas",src)>0 && !mkdir(fontdir,0700));
  FILE *f=fopen(argv[2],"rb");assert(f && !fseek(f,0,SEEK_END));long length=ftell(f);assert(length>0 && length<BACKUP_FONT_BYTES && !fseek(f,0,SEEK_SET));
  size_t nf=(size_t)length;unsigned char *font=malloc(nf);assert(font && fread(font,1,nf,f)==nf && !fclose(f));
  char id[21];hashId(font,nf,id);assert(snprintf(path,sizeof path,"%s/%s",fontdir,id)>0);
  f=fopen(path,"wb");assert(f && fwrite(font,1,nf,f)==nf && !fclose(f));
  BackupStores *stores=backupstores_novo();assert(stores);
  const char key[]="torbox=local-provider-fixture-key\n";assert(backupstores_adicionar(stores,BS_DEBRID_PROFILE,0,1,key,sizeof key-1));
  char prefs[80];snprintf(prefs,sizeof prefs,"%s\n\n%s\n",id,id);assert(backupstores_adicionar(stores,BS_IMPORTED_FONT_CHOICES,0,1,prefs,strlen(prefs)));
  char *storeText=NULL,*snapshot=NULL,*complete=NULL;size_t ns,nb,nc;
  assert(backupstores_exportar(stores,&storeText,&ns));BackupLocal *b=backuplocal_novo("user-1",7,1792330000);assert(b && backuplocal_adicionar(b,BACKUP_LOCAIS,storeText,ns));
  assert(backuplocal_exportar(b,&snapshot,&nb));backuplocal_liberar(b);backupstores_liberar(stores);free(storeText);
  Mock m={0};DriveLocal c={.token="fixture-token",.dono="user-1",.perfil=7,.pedir=pedir,.ctx=&m,.senha="fixture-backup-password",.exigirCriptografia=1};
  assert(drivefontes_enviar(&c,src,snapshot,nb,&complete,&nc)==DRIVE_LOCAL_OK);
  int chunks=(int)((nf+BACKUP_FONT_CHUNK-1)/BACKUP_FONT_CHUNK);assert(m.n==chunks);
  char mainId[200];assert(drivelocal_enviar(&c,complete,nc,mainId)==DRIVE_LOCAL_OK && m.n==chunks+1);
  c.senha="wrong-password";b=NULL;assert(drivelocal_baixar(&c,mainId,&b)==DRIVE_LOCAL_SENHA && !b);
  c.senha="fixture-backup-password";
  assert(drivelocal_baixar(&c,mainId,&b)==DRIVE_LOCAL_OK && b);
  m.files[0].data[m.files[0].n-1]^=1;
  assert(drivefontes_baixar(&c,dst,b)==DRIVE_LOCAL_SENHA && !backuplocal_fontes(b));
  m.files[0].data[m.files[0].n-1]^=1;
  assert(drivefontes_baixar(&c,dst,b)==DRIVE_LOCAL_OK && backuplocal_fontes(b));
  BackupFontManifest empty;assert(backupfonts_enumerar(dst,&empty) && !empty.n); /* no active file changes before confirmation */
  unsigned char *bytes=NULL;size_t n;
  assert(backupfonts_stage_ler(backuplocal_fontes(b),0,&bytes,&n) && n==nf && !memcmp(bytes,font,n));free(bytes);
  char *plan=NULL,*before=NULL,*rollback=NULL,*after=NULL;size_t np,na;
  assert(backupfonts_stage_plano(backuplocal_fontes(b),&plan,&np));int root=open(dst,O_RDONLY|O_DIRECTORY);assert(root>=0);
  assert(backupfonts_disco_ler(root,&before,&na) && backupfonts_plano(before,plan,&rollback,&after));
  assert(backupfonts_disco_gravar(root,after,strlen(after)) && backupfonts_enumerar(dst,&empty) && empty.n==1);
  backuplocal_liberar(b);assert(backupfonts_disco_gravar(root,rollback,strlen(rollback)));close(root);
  for(int i=0;i<m.n;i++)free(m.files[i].data);
  free(plan);free(before);free(rollback);free(after);free(font);free(snapshot);free(complete);
  puts("PASS: encrypted Drive settings/font round trip, no password/key in wire metadata, wrong password/tampered asset rejection, staged-only download, immutable commit and rollback (mock HTTP; real binary font)");
}
