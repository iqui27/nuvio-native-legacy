#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "backupdisco.h"
#include "backupstores.h"
#include "backupfonts.h"
#include "backupcrypto.h"
#ifdef __EMSCRIPTEN__
#include "dados.h"
#endif
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BackupDisco {int raiz;unsigned seq;BackupSessaoAtual atual;void *ctx;};
static int nome(const char *dono,int perfil,int tipo,char out[160]) {
  if(tipo==BACKUP_IO_DIARIO) {
    if(dono || perfil)return 0;
    strcpy(out,"backup-restore-journal.txt");return 1;
  }
  // Fixed file names only, bound to the original Nuvio account/profile.
  if(!dono || perfil<1)return 0;
  size_t n=strlen(dono);if(!n || n>80)return 0;
  for(size_t i=0;i<n;i++) {
    unsigned char c=(unsigned char)dono[i];
    if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-'))return 0;
  }
  switch(tipo) {
    case BACKUP_AJUSTES_APARELHO:strcpy(out,"ajustes.txt");break;
    case BACKUP_AJUSTES_PERFIL:snprintf(out,160,"ajustes-p%d.txt",perfil);break;
    case BACKUP_PLAYER:strcpy(out,"player.txt");break;
    case BACKUP_FONTES_COMPARTILHADAS:strcpy(out,"fontefalha.txt");break;
    case BACKUP_FONTES_PERFIL:snprintf(out,160,"fontefalha-%s-p%d.txt",dono,perfil);break;
    // Raw home row keys may contain configured URLs. No file access until
    // their identity adapter is implemented and included in restore planning.
    default:return 0;
  }
  return 1;
}
static int regular(BackupDisco *d,const char *arq,int *existe) {
  struct stat st;
  if(fstatat(d->raiz,arq,&st,AT_SYMLINK_NOFOLLOW)==0){*existe=1;return S_ISREG(st.st_mode);}
  *existe=0;return errno==ENOENT;
}
static int lerArquivo(BackupDisco *d,const char *arq,size_t limite,char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  if(!d || !out || !n)return -1;
  int fd=openat(d->raiz,arq,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(fd<0)return errno==ENOENT?0:-1;
  struct stat st;
  if(fstat(fd,&st)!=0 || !S_ISREG(st.st_mode) || st.st_size<0 || (uintmax_t)st.st_size>limite){close(fd);return -1;}
  size_t tamanho=(size_t)st.st_size,lido=0;char *s=malloc(tamanho+1);
  if(!s){close(fd);return -1;}
  int ok=1;
  while(lido<tamanho) {
    ssize_t k=read(fd,s+lido,tamanho-lido);
    if(k<0 && errno==EINTR)continue;
    if(k<=0){ok=0;break;}lido+=(size_t)k;
  }
  char extra;ssize_t k;
  do{k=read(fd,&extra,1);}while(k<0 && errno==EINTR);
  if(k!=0)ok=0;
  if(close(fd)!=0)ok=0;
  if(!ok){free(s);return -1;}
  s[tamanho]=0;*out=s;*n=tamanho;return 1;
}
static int sincronizar(int fd) {
  int r;do{r=fsync(fd);}while(r<0 && errno==EINTR);return r==0;
}
static int gravarArquivo(BackupDisco *d,const char *arq,const char *s,size_t n) {
  char tmp[80];int existe;
  if(!d || !s || !regular(d,arq,&existe))return 0;
  int fd=-1;
  for(int i=0;i<100;i++) {
    snprintf(tmp,sizeof tmp,".nuvio-restore-%ld-%u.tmp",(long)getpid(),++d->seq);
    fd=openat(d->raiz,tmp,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
    if(fd>=0 || errno!=EEXIST)break;
  }
  if(fd<0)return 0;
  int ok=fchmod(fd,0600)==0;size_t feito=0;
  while(ok && feito<n) {
    ssize_t k=write(fd,s+feito,n-feito);
    if(k<0 && errno==EINTR)continue;
    if(k<=0)ok=0;else feito+=(size_t)k;
  }
#ifdef __EMSCRIPTEN__
  if(ok && existe) {
    struct stat antes,novo;
    if(fstatat(d->raiz,arq,&antes,AT_SYMLINK_NOFOLLOW) || fstat(fd,&novo))ok=0;
    else if(novo.st_mtim.tv_sec<antes.st_mtim.tv_sec ||
      (novo.st_mtim.tv_sec==antes.st_mtim.tv_sec && novo.st_mtim.tv_nsec/1000000<=antes.st_mtim.tv_nsec/1000000)) {
      struct timespec tempo[2]={novo.st_atim,antes.st_mtim};tempo[1].tv_nsec+=1000000;
      if(tempo[1].tv_nsec>=1000000000){tempo[1].tv_sec++;tempo[1].tv_nsec-=1000000000;}
      ok=futimens(fd,tempo)==0;
    }
  }
#endif
  if(ok)ok=sincronizar(fd);
  if(close(fd)!=0)ok=0;
  if(ok)ok=renameat(d->raiz,tmp,d->raiz,arq)==0;
  if(ok)ok=sincronizar(d->raiz);
  // A failed directory fsync may follow a successful rename. The transaction
  // engine treats false as uncertain and retains/recovers its private journal.
  if(!ok)unlinkat(d->raiz,tmp,0);
  return ok;
}
static int apagarArquivo(BackupDisco *d,const char *arq) {
  int existe;
  if(!d || !regular(d,arq,&existe))return 0;
  if(existe && unlinkat(d->raiz,arq,0)!=0)return 0;
  // ENOENT still needs a barrier: a previous removal may have succeeded just
  // before its directory fsync failed, so absence alone is not durable proof.
  return sincronizar(d->raiz);
}
/* One typed bundle is journalled as a module, while every individual file
 * uses the same no-follow, private, durable atomic writer as scalar stores. */
static int ler(void *ctx,const char *dono,int perfil,int tipo,char **out,size_t *n) {
  BackupDisco *d=ctx;char arq[160];
  if(out)*out=NULL;
  if(n)*n=0;
  if(!d || !out || !n)return -1;
  if(tipo==BACKUP_FONTES_IMPORTADAS) {
    if(!backupstores_nome(BS_HOME_ROWS,0,dono,perfil,arq,sizeof arq))return -1;
    return backupfonts_disco_ler(d->raiz,out,n)?1:-1;
  }
  if(tipo!=BACKUP_LOCAIS) {
    if(!nome(dono,perfil,tipo,arq))return -1;
    return lerArquivo(d,arq,tipo==BACKUP_IO_DIARIO?BACKUP_LOCAL_MAX+128u:BACKUP_PARTE_MAX,out,n);
  }
  if(!backupstores_nome(BS_HOME_ROWS,0,dono,perfil,arq,sizeof arq))return -1;
  BackupStores *b=backupstores_novo();if(!b)return -1;
  int fd=openat(d->raiz,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
  DIR *dir=fd<0?NULL:fdopendir(fd);
  if(!dir){if(fd>=0)close(fd);backupstores_liberar(b);return -1;}
  int ok=1;struct dirent *e;
  errno=0;
  while((e=readdir(dir))) {
    int t;uint32_t sub;
    if(!backupstores_reconhecer(e->d_name,dono,perfil,&t,&sub))continue;
    char *s=NULL;size_t len=0;
    int estado=lerArquivo(d,e->d_name,BACKUP_STORE_BYTES,&s,&len);
    if(estado<0 || (estado==1 && !backupstores_adicionar(b,t,sub,1,s,len)))ok=0;
    backupcrypto_limpar(s,len);free(s);
    if(!ok)break;
    errno=0;
  }
  if(errno)ok=0;
  if(closedir(dir)!=0)ok=0;
  if(ok)ok=backupstores_exportar(b,out,n);
  backupstores_liberar(b);return ok?1:-1;
}
static int gravarInterno(void *ctx,const char *dono,int perfil,int tipo,const char *s,size_t n) {
  BackupDisco *d=ctx;char arq[160];
  if(!d || !s || n>(tipo==BACKUP_IO_DIARIO?BACKUP_LOCAL_MAX+128u:BACKUP_PARTE_MAX))return 0;
  if(tipo==BACKUP_FONTES_IMPORTADAS)return backupstores_nome(BS_HOME_ROWS,0,dono,perfil,arq,sizeof arq) && backupfonts_disco_gravar(d->raiz,s,n);
  if(tipo!=BACKUP_LOCAIS)return nome(dono,perfil,tipo,arq) && gravarArquivo(d,arq,s,n);
  BackupStores *b=NULL;
  if(!backupstores_nome(BS_HOME_ROWS,0,dono,perfil,arq,sizeof arq) || !backupstores_ler(s,n,&b))return 0;
  // Resolve every path before the first mutation. Content validation/merging
  // belongs to planning; rollback must accept the exact original file bytes.
  int ok=1;
  for(int i=0;i<backupstores_n(b) && ok;i++) {
    BackupStore item;ok=backupstores_item(b,i,&item) && backupstores_nome(item.tipo,item.sub,dono,perfil,arq,sizeof arq);
  }
  for(int i=0;i<backupstores_n(b) && ok;i++) {
    BackupStore item;backupstores_item(b,i,&item);backupstores_nome(item.tipo,item.sub,dono,perfil,arq,sizeof arq);
    ok=item.existe?gravarArquivo(d,arq,item.texto,item.n):apagarArquivo(d,arq);
  }
  backupstores_liberar(b);return ok;
}
static int apagarInterno(void *ctx,const char *dono,int perfil,int tipo) {
  BackupDisco *d=ctx;char arq[160];
  // A bundle is always an existing logical module, even when it has no files.
  // Deleting it without a typed footprint would erase unrelated connections.
  return d && tipo!=BACKUP_LOCAIS && nome(dono,perfil,tipo,arq) && apagarArquivo(d,arq);
}
static int gravar(void *ctx,const char *dono,int perfil,int tipo,const char *s,size_t n) {
#ifdef __EMSCRIPTEN__
  if(!dados_backup_aguardar())return 0;
  dados_fs_travar();
#endif
  int ok=gravarInterno(ctx,dono,perfil,tipo,s,n);
#ifdef __EMSCRIPTEN__
  dados_fs_liberar();
  if(ok)ok=dados_backup_sincronizar();
#endif
  return ok;
}
static int apagar(void *ctx,const char *dono,int perfil,int tipo) {
#ifdef __EMSCRIPTEN__
  if(!dados_backup_aguardar())return 0;
  dados_fs_travar();
#endif
  int ok=apagarInterno(ctx,dono,perfil,tipo);
#ifdef __EMSCRIPTEN__
  dados_fs_liberar();
  if(ok)ok=dados_backup_sincronizar();
#endif
  return ok;
}
static int atual(void *ctx,const char *dono,int perfil,uint64_t sessao) {
  BackupDisco *d=ctx;return d && d->atual && d->atual(d->ctx,dono,perfil,sessao);
}
BackupDisco *backupdisco_abrir(const char *dir,BackupSessaoAtual verificar,void *ctx) {
#ifdef __EMSCRIPTEN__
  if(!dados_persistente())return NULL;
#endif
  if(!dir || !*dir)return NULL;
  int fd=open(dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return NULL;
  BackupDisco *d=calloc(1,sizeof *d);if(!d){close(fd);return NULL;}
  d->raiz=fd;d->atual=verificar;d->ctx=ctx;return d;
}
void backupdisco_fechar(BackupDisco *d){if(d){close(d->raiz);free(d);}}
BackupIO backupdisco_io(BackupDisco *d){return (BackupIO){d,ler,gravar,apagar,atual};}
int backupdisco_recuperar_arranque(const char *dir) {
  if(!dir || !*dir)return BACKUP_RESTORE_INVALIDO;
  // Probe only the fixed private journal. No change to ordinary startup and
  // no fsync/temporary file when this installation has no pending restore.
  char caminho[4096];int n=snprintf(caminho,sizeof caminho,"%s/backup-restore-journal.txt",dir);
  if(n<0 || (size_t)n>=sizeof caminho)return BACKUP_RESTORE_INVALIDO;
  struct stat st;
  if(lstat(caminho,&st)!=0)return errno==ENOENT?BACKUP_RESTORE_NADA:BACKUP_RESTORE_RECUPERAR;
  if(!S_ISREG(st.st_mode))return BACKUP_RESTORE_RECUPERAR;
  BackupDisco *d=backupdisco_abrir(dir,NULL,NULL);if(!d)return BACKUP_RESTORE_RECUPERAR;
  BackupIO io=backupdisco_io(d);int resultado=backuprestauro_recuperar(&io);
  backupdisco_fechar(d);return resultado;
}
