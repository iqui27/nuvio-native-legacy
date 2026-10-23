// Native filesystem adapter integration. Transaction fault injection is in
// backuprestauro.c; this checks actual private files and restart recovery.
#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "backupdisco.h"
#include "backupplano_interno.h"
#include "backupstores.h"
#include "backupfonts.h"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct {BackupIO disco;char *preparado,*confirmado;} Espia;
static const char *const arqs[]={"ajustes.txt","ajustes-p7.txt","player.txt","fontefalha.txt","fontefalha-user-1-p7.txt"};
static const char *const antigos[]={"old-device","old-profile",NULL,"","old-policy"};
static const char *const novos[]={"new-device","new-profile","new-player","new-shared","new-policy"};
static int sessao(void *ctx,const char *dono,int perfil,uint64_t geracao) {
  return *(int *)ctx && !strcmp(dono,"user-1") && perfil==7 && geracao==33;
}
static int ler(void *ctx,const char *d,int p,int t,char **s,size_t *n) {
  Espia *e=ctx;return e->disco.ler(e->disco.ctx,d,p,t,s,n);
}
static int gravar(void *ctx,const char *d,int p,int t,const char *s,size_t n) {
  Espia *e=ctx;
  if(t==BACKUP_IO_DIARIO) {
    char **c=strstr(s,"JOURNAL 1 P\n")?&e->preparado:&e->confirmado;
    free(*c);*c=malloc(n+1);assert(*c);memcpy(*c,s,n);(*c)[n]=0;
  }
  return e->disco.gravar(e->disco.ctx,d,p,t,s,n);
}
static int apagar(void *ctx,const char *d,int p,int t) {
  Espia *e=ctx;return e->disco.apagar(e->disco.ctx,d,p,t);
}
static int atual(void *ctx,const char *d,int p,uint64_t s) {
  Espia *e=ctx;return e->disco.atual(e->disco.ctx,d,p,s);
}
static void conferir(BackupIO *io,int novo) {
  for(int i=0;i<5;i++) {
    char *s=NULL;size_t n=0;const char *esperado=novo?novos[i]:antigos[i];
    assert(io->ler(io->ctx,"user-1",7,i,&s,&n)==(esperado?1:0));
    if(esperado)assert(s && n==strlen(esperado) && !strcmp(s,esperado));else assert(!s && !n);
    free(s);
  }
}
static void caminho(char out[512],const char *dir,const char *nome) {assert(snprintf(out,512,"%s/%s",dir,nome)<512);}
static void bundleRecovery(const char *dir,BackupIO io) {
  BackupStores *before=backupstores_novo(),*after=backupstores_novo(),*partial=backupstores_novo();assert(before && after && partial);
  assert(backupstores_adicionar(before,BS_LIBRARY_UI,0,1,"old-layout",10));
  assert(backupstores_adicionar(before,BS_SAVED_TABS,0,0,NULL,0));
  assert(backupstores_adicionar(after,BS_LIBRARY_UI,0,1,"new-layout",10));
  assert(backupstores_adicionar(after,BS_SAVED_TABS,0,1,"new-tabs",8));
  assert(backupstores_adicionar(partial,BS_SAVED_TABS,0,1,"partial",7));
  char *old=NULL,*new=NULL,*part=NULL;size_t no,nn,np;
  assert(backupstores_exportar(before,&old,&no) && backupstores_exportar(after,&new,&nn) && backupstores_exportar(partial,&part,&np));
  assert(io.gravar(io.ctx,"user-1",7,BACKUP_LOCAIS,old,no));
  const unsigned char fontBytes[]="new-immutable-font-fixture";uint64_t hash=UINT64_C(14695981039346656037);
  for(size_t i=0;i<sizeof fontBytes;i++)hash=(hash^fontBytes[i])*UINT64_C(1099511628211);
  BackupFontManifest m={.n=1};snprintf(m.font[0].id,21,"%016llx.ttf",(unsigned long long)hash);
  m.font[0].n=sizeof fontBytes;m.font[0].chunks=1;strcpy(m.font[0].remoto[0],"Fixture");
  BackupFontStage *stage=backupfonts_stage(dir);assert(stage);
  assert(backupfonts_stage_chunk(stage,&m.font[0],0,fontBytes,sizeof fontBytes) && backupfonts_stage_completo(stage,&m));
  char *fontPlan=NULL,*fontBefore=NULL,*fontRollback=NULL,*fontAfter=NULL;size_t nf;
  assert(backupfonts_stage_plano(stage,&fontPlan,&nf));int root=open(dir,O_RDONLY|O_DIRECTORY);assert(root>=0);
  assert(backupfonts_disco_ler(root,&fontBefore,&nf) && backupfonts_plano(fontBefore,fontPlan,&fontRollback,&fontAfter));
  BackupPlano *p=calloc(1,sizeof *p);assert(p);strcpy(p->dono,"user-1");p->perfil=7;p->sessao=33;p->criado=1792330000;
  p->parte[BACKUP_LOCAIS].usada=p->parte[BACKUP_LOCAIS].existia=1;
  p->parte[BACKUP_LOCAIS].antes=old;p->parte[BACKUP_LOCAIS].nAntes=no;
  p->parte[BACKUP_LOCAIS].depois=new;p->parte[BACKUP_LOCAIS].nDepois=nn;
  p->parte[BACKUP_FONTES_IMPORTADAS].usada=p->parte[BACKUP_FONTES_IMPORTADAS].existia=1;
  p->parte[BACKUP_FONTES_IMPORTADAS].antes=fontRollback;p->parte[BACKUP_FONTES_IMPORTADAS].nAntes=strlen(fontRollback);
  p->parte[BACKUP_FONTES_IMPORTADAS].depois=fontAfter;p->parte[BACKUP_FONTES_IMPORTADAS].nDepois=strlen(fontAfter);
  Espia spy={.disco=io};BackupIO wrapped={&spy,ler,gravar,apagar,atual};
  assert(backuprestauro_aplicar(p,&wrapped)==BACKUP_RESTORE_OK);
  assert(io.gravar(io.ctx,NULL,0,BACKUP_IO_DIARIO,spy.preparado,strlen(spy.preparado)));
  /* Simulate a power loss after arbitrary files were replaced and before the
   * newly introduced font finished copying. Staging can already be gone. */
  assert(io.gravar(io.ctx,"user-1",7,BACKUP_LOCAIS,part,np));
  int fonts=openat(root,"fontes-importadas",O_RDONLY|O_DIRECTORY);assert(fonts>=0);
  int fd=openat(fonts,m.font[0].id,O_WRONLY|O_TRUNC);assert(fd>=0 && !close(fd));
  backupfonts_soltar(stage);
  assert(backupdisco_recuperar_arranque(dir)==BACKUP_RESTORE_OK);
  char *value=NULL;size_t nv;assert(io.ler(io.ctx,"user-1",7,BACKUP_LOCAIS,&value,&nv)==1);
  BackupStores *restored=NULL;assert(backupstores_ler(value,nv,&restored));BackupStore v;
  assert(backupstores_buscar(restored,BS_LIBRARY_UI,0,&v) && v.existe && !strcmp(v.texto,"old-layout"));
  assert(!backupstores_buscar(restored,BS_SAVED_TABS,0,&v));
  assert(faccessat(fonts,m.font[0].id,F_OK,0)!=0);
  close(fonts);close(root);backupplano_liberar(p);backupstores_liberar(before);backupstores_liberar(after);backupstores_liberar(partial);backupstores_liberar(restored);
  free(value);free(part);free(fontPlan);free(fontBefore);free(spy.preparado);free(spy.confirmado);
  char path[512];caminho(path,dir,"fontes-importadas");assert(!rmdir(path));
}
int main(void) {
  char dir[]="/tmp/nuvio-backup-disk.XXXXXX",arquivo[512],outro[512];assert(mkdtemp(dir));int valido=1;
  BackupDisco *d=backupdisco_abrir(dir,sessao,&valido);assert(d);BackupIO io=backupdisco_io(d);
  assert(backupdisco_recuperar_arranque(dir)==BACKUP_RESTORE_NADA);
  assert(backupdisco_recuperar_arranque("/no-such-nuvio-backup-directory")==BACKUP_RESTORE_NADA);
  char *s=NULL;size_t n=0;
  assert(!backupdisco_abrir("/no-such-nuvio-backup-directory",sessao,&valido));
  assert(io.ler(io.ctx,"../escape",7,BACKUP_PLAYER,&s,&n)==-1 && !s && !n);
  assert(!io.gravar(io.ctx,"user-1",0,BACKUP_PLAYER,"bad",3));
  assert(!io.gravar(io.ctx,"user-1",7,BACKUP_FILEIRAS,"bad",3));
  assert(!io.gravar(io.ctx,"user-1",7,BACKUP_IO_DIARIO,"bad",3));
  BackupPlano *p=calloc(1,sizeof *p);assert(p);strcpy(p->dono,"user-1");p->perfil=7;p->sessao=33;p->criado=1792330000;
  for(int i=0;i<5;i++) {
    p->parte[i].usada=1;p->parte[i].existia=antigos[i]!=NULL;
    p->parte[i].antes=antigos[i]?strdup(antigos[i]):NULL;p->parte[i].nAntes=antigos[i]?strlen(antigos[i]):0;
    p->parte[i].depois=strdup(novos[i]);p->parte[i].nDepois=strlen(novos[i]);
    if(antigos[i])assert(io.gravar(io.ctx,"user-1",7,i,antigos[i],strlen(antigos[i])));
  }
  conferir(&io,0);Espia espia={.disco=io};BackupIO teste={&espia,ler,gravar,apagar,atual};
  assert(backuprestauro_aplicar(p,&teste)==BACKUP_RESTORE_OK);conferir(&io,1);
  assert(espia.preparado && espia.confirmado);
  for(int i=0;i<5;i++) {
    struct stat st;caminho(arquivo,dir,arqs[i]);assert(!stat(arquivo,&st) && (st.st_mode&0777)==0600);
  }
  assert(io.ler(io.ctx,"user-1",8,BACKUP_FONTES_PERFIL,&s,&n)==0);
  assert(io.ler(io.ctx,"user-2",7,BACKUP_FONTES_PERFIL,&s,&n)==0);
  // Restart from an interrupted prepared restore, with a different session.
  assert(io.gravar(io.ctx,NULL,0,BACKUP_IO_DIARIO,espia.preparado,strlen(espia.preparado)));
  backupdisco_fechar(d);valido=0;d=backupdisco_abrir(dir,sessao,&valido);assert(d);io=backupdisco_io(d);
  assert(backupdisco_recuperar_arranque(dir)==BACKUP_RESTORE_OK);conferir(&io,0);
  // COMMITTED cleanup after restart retains every new store.
  for(int i=0;i<5;i++)assert(io.gravar(io.ctx,"user-1",7,i,novos[i],strlen(novos[i])));
  assert(io.gravar(io.ctx,NULL,0,BACKUP_IO_DIARIO,espia.confirmado,strlen(espia.confirmado)));
  backupdisco_fechar(d);d=backupdisco_abrir(dir,sessao,&valido);assert(d);io=backupdisco_io(d);
  assert(backupdisco_recuperar_arranque(dir)==BACKUP_RESTORE_OK);conferir(&io,1);
  assert(backuprestauro_recuperar(&io)==BACKUP_RESTORE_NADA);
  assert(io.gravar(io.ctx,NULL,0,BACKUP_IO_DIARIO,"corrupt-journal",15));
  assert(backupdisco_recuperar_arranque(dir)==BACKUP_RESTORE_RECUPERAR);conferir(&io,1);
  assert(io.ler(io.ctx,NULL,0,BACKUP_IO_DIARIO,&s,&n)==1 && n==15 && !strcmp(s,"corrupt-journal"));free(s);
  assert(io.apagar(io.ctx,NULL,0,BACKUP_IO_DIARIO));
  valido=1;bundleRecovery(dir,io);
  // Symlinks, directories and FIFOs cannot turn fixed stores into other files
  // or block reads of a malformed private store.
  caminho(arquivo,dir,"player.txt");caminho(outro,dir,"outside.txt");
  int fd=open(outro,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0 && write(fd,"safe",4)==4 && !close(fd));
  assert(!unlink(arquivo) && !symlink(outro,arquivo));
  assert(io.ler(io.ctx,"user-1",7,BACKUP_PLAYER,&s,&n)==-1);
  assert(!io.gravar(io.ctx,"user-1",7,BACKUP_PLAYER,"bad",3));
  assert(!io.apagar(io.ctx,"user-1",7,BACKUP_PLAYER));
  assert(!unlink(arquivo) && !mkdir(arquivo,0700));
  assert(io.ler(io.ctx,"user-1",7,BACKUP_PLAYER,&s,&n)==-1 && !io.apagar(io.ctx,"user-1",7,BACKUP_PLAYER));
  assert(!rmdir(arquivo) && !mkfifo(arquivo,0600));
  assert(io.ler(io.ctx,"user-1",7,BACKUP_PLAYER,&s,&n)==-1);
  assert(!unlink(arquivo));
  // Oversized reads are bounded before allocation; writes reject oversize.
  fd=open(arquivo,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0 && !ftruncate(fd,BACKUP_PARTE_MAX+1u) && !close(fd));
  assert(io.ler(io.ctx,"user-1",7,BACKUP_PLAYER,&s,&n)==-1 && !s && !n);
  assert(!io.gravar(io.ctx,"user-1",7,BACKUP_PLAYER,"",BACKUP_PARTE_MAX+1u));
  backupdisco_fechar(d);backupplano_liberar(p);free(espia.preparado);free(espia.confirmado);
  DIR *lista=opendir(dir);assert(lista);struct dirent *e;
  while((e=readdir(lista)))if(strcmp(e->d_name,".") && strcmp(e->d_name,"..")) {
    assert(strncmp(e->d_name,".nuvio-restore-",15));caminho(arquivo,dir,e->d_name);assert(!unlink(arquivo));
  }
  closedir(lista);assert(!rmdir(dir));
  puts("PASS: private native files, atomic replacement, restart recovery, original profile, empty/absent stores and bounded nonblocking file reads");
}
