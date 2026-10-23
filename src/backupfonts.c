#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include "backupfonts.h"
#include "backupcrypto.h"
#include <stdatomic.h>
#include <stdint.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
struct BackupFontStage {
  _Atomic int refs;int raiz,fd,completo,validado;
  char chave[7];BackupFontManifest manifest;
  size_t gravado[BACKUP_FONT_N];
};
static const char pasta[]="fontes-importadas";
static const char stagePrefix[]=".nuvio-backup-fonts-";
int backupfonts_id(const char *id) {
  if(!id || strlen(id)!=20 || (strcmp(id+16,".ttf") && strcmp(id+16,".otf")))return 0;
  for(int i=0;i<16;i++)if(!((id[i]>='0' && id[i]<='9') || (id[i]>='a' && id[i]<='f')))return 0;
  return 1;
}
static int remoto(const char *id) {
  size_t n=id?strlen(id):0;if(!n || n>=200)return 0;
  for(size_t i=0;i<n;i++)if(!((id[i]>='a' && id[i]<='z') || (id[i]>='A' && id[i]<='Z') ||
    (id[i]>='0' && id[i]<='9') || id[i]=='-' || id[i]=='_'))return 0;
  return 1;
}
static int syncfd(int fd){int r;do{r=fsync(fd);}while(r<0 && errno==EINTR);return r==0;}
static int fonteN(const BackupFontManifest *m,const char *id){for(int i=0;i<m->n;i++)if(!strcmp(m->font[i].id,id))return i;return -1;}
static int bytesNumero(const char *s,size_t max,size_t *out) {
  size_t n=0;if(!*s)return 0;
  for(;*s;s++){if(*s<'0' || *s>'9' || (unsigned)(*s-'0')>max || n>(max-(unsigned)(*s-'0'))/10)return 0;n=n*10+(unsigned)(*s-'0');}
  *out=n;return 1;
}
int backupfonts_manifest_ler(const char *s,size_t n,BackupFontManifest *out) {
  if(out)memset(out,0,sizeof *out);
  static const char cab[]="NUVIO-FONT-BACKUP 1\n";
  if(!s || !out || n>=16384 || n<sizeof cab-1 || memchr(s,0,n) || memcmp(s,cab,sizeof cab-1))return 0;
  BackupFontManifest m={0};const char *p=s+sizeof cab-1,*fim=s+n;
  while(p<fim) {
    const char *e=memchr(p,'\n',(size_t)(fim-p));char l[256],id[21],r[200],numero[20];int usado=0;
    if(!e || e-p>255)return 0;
    memcpy(l,p,(size_t)(e-p));l[e-p]=0;
    if(sscanf(l,"font %20s %19s%n",id,numero,&usado)==2 && usado && !l[usado]) {
      size_t bytes;if(!backupfonts_id(id) || fonteN(&m,id)>=0 || m.n>=BACKUP_FONT_N || !bytesNumero(numero,BACKUP_FONT_BYTES,&bytes) || !bytes)return 0;
      BackupFont *v=&m.font[m.n++];strcpy(v->id,id);v->n=bytes;v->chunks=(int)((bytes+BACKUP_FONT_CHUNK-1)/BACKUP_FONT_CHUNK);
    } else if(sscanf(l,"chunk %20s %19s %199s%n",id,numero,r,&usado)==3 && usado && !l[usado]) {
      size_t k;int i=fonteN(&m,id);
      if(i<0 || !bytesNumero(numero,BACKUP_FONT_CHUNKS-1,&k) || (int)k>=m.font[i].chunks || *m.font[i].remoto[k] || !remoto(r))return 0;
      strcpy(m.font[i].remoto[k],r);
    } else return 0;
    p=e+1;
  }
  for(int i=0;i<m.n;i++)for(int j=0;j<m.font[i].chunks;j++)if(!*m.font[i].remoto[j])return 0;
  *out=m;return 1;
}
int backupfonts_manifest_exportar(const BackupFontManifest *m,char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  if(!m || !out || !n || m->n<0 || m->n>BACKUP_FONT_N)return 0;
  char *s=malloc(16384);if(!s)return 0;
  size_t pos=(size_t)snprintf(s,16384,"NUVIO-FONT-BACKUP 1\n");
  for(int i=0;i<m->n;i++) {
    const BackupFont *v=&m->font[i];if(!backupfonts_id(v->id) || !v->n || v->n>BACKUP_FONT_BYTES || v->chunks!=(int)((v->n+BACKUP_FONT_CHUNK-1)/BACKUP_FONT_CHUNK))goto falha;
    int k=snprintf(s+pos,16384-pos,"font %s %zu\n",v->id,v->n);if(k<0 || (size_t)k>=16384-pos)goto falha;pos+=(size_t)k;
    for(int j=0;j<v->chunks;j++) {
      if(!remoto(v->remoto[j]))goto falha;
      k=snprintf(s+pos,16384-pos,"chunk %s %d %s\n",v->id,j,v->remoto[j]);if(k<0 || (size_t)k>=16384-pos)goto falha;pos+=(size_t)k;
    }
  }
  BackupFontManifest validado;if(!backupfonts_manifest_ler(s,pos,&validado))goto falha;
  *out=s;*n=pos;return 1;
falha:free(s);return 0;
}
static int abrirRaiz(const char *dir){return dir && *dir?open(dir,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW):-1;}
static int listar(int fd,BackupFontManifest *m) {
  memset(m,0,sizeof *m);int dup=openat(fd,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(dup<0)return 0;
  DIR *d=fdopendir(dup);if(!d){close(dup);return 0;}
  int ok=1;struct dirent *e;errno=0;
  while((e=readdir(d))) {
    if(!backupfonts_id(e->d_name))continue;
    struct stat st;
    if(m->n>=BACKUP_FONT_N || fstatat(fd,e->d_name,&st,AT_SYMLINK_NOFOLLOW) || !S_ISREG(st.st_mode) || st.st_size<1 || st.st_size>BACKUP_FONT_BYTES){ok=0;break;}
    BackupFont *v=&m->font[m->n++];strcpy(v->id,e->d_name);v->n=(size_t)st.st_size;v->chunks=(int)((v->n+BACKUP_FONT_CHUNK-1)/BACKUP_FONT_CHUNK);errno=0;
  }
  if(errno)ok=0;
  if(closedir(d))ok=0;
  return ok;
}
int backupfonts_enumerar(const char *dir,BackupFontManifest *m) {
  if(!m)return 0;
  memset(m,0,sizeof *m);int root=abrirRaiz(dir);if(root<0)return 0;
  int fd=openat(root,pasta,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);int faltou=errno==ENOENT;close(root);
  if(fd<0)return faltou;
  int ok=listar(fd,m);close(fd);return ok;
}
static int lerExato(int fd,void *out,size_t n) {
  size_t pos=0;while(pos<n){ssize_t k=read(fd,(char *)out+pos,n-pos);if(k<0 && errno==EINTR)continue;if(k<=0)return 0;pos+=(size_t)k;}return 1;
}
static int gravarExato(int fd,const void *s,size_t n) {
  size_t pos=0;while(pos<n){ssize_t k=write(fd,(const char *)s+pos,n-pos);if(k<0 && errno==EINTR)continue;if(k<=0)return 0;pos+=(size_t)k;}return 1;
}
int backupfonts_chunk_ler(const char *dir,const BackupFont *f,int chunk,unsigned char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  if(!out || !n || !f || !backupfonts_id(f->id) || !f->n || f->n>BACKUP_FONT_BYTES || chunk<0 || chunk>=f->chunks || (size_t)chunk*BACKUP_FONT_CHUNK>=f->n)return 0;
  int root=abrirRaiz(dir);if(root<0)return 0;
  int fonts=openat(root,pasta,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);close(root);if(fonts<0)return 0;
  int fd=openat(fonts,f->id,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);close(fonts);if(fd<0)return 0;
  struct stat st;int ok=!fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_size==(off_t)f->n && lseek(fd,(off_t)((size_t)chunk*BACKUP_FONT_CHUNK),SEEK_SET)>=0;
  size_t bytes=f->n-(size_t)chunk*BACKUP_FONT_CHUNK;if(bytes>BACKUP_FONT_CHUNK)bytes=BACKUP_FONT_CHUNK;
  unsigned char *s=ok?malloc(bytes):NULL;if(!s)ok=0;
  if(ok)ok=lerExato(fd,s,bytes);
  if(close(fd))ok=0;
  if(!ok){free(s);return 0;}*out=s;*n=bytes;return 1;
}
BackupFontStage *backupfonts_stage(const char *dir) {
  int root=abrirRaiz(dir);if(root<0)return NULL;
  char caminho[4096];int n=snprintf(caminho,sizeof caminho,"%s/%sXXXXXX",dir,stagePrefix);
  if(n<0 || (size_t)n>=sizeof caminho || !mkdtemp(caminho)){close(root);return NULL;}
  const char *key=caminho+strlen(caminho)-6;char nome[64];snprintf(nome,sizeof nome,"%s%s",stagePrefix,key);
  int fd=openat(root,nome,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);
  BackupFontStage *s=fd>=0?calloc(1,sizeof *s):NULL;
  if(!s){if(fd>=0)close(fd);unlinkat(root,nome,AT_REMOVEDIR);close(root);return NULL;}
  s->fd=fd;s->raiz=root;strcpy(s->chave,key);atomic_init(&s->refs,1);return s;
}
void backupfonts_reter(BackupFontStage *s){if(s)atomic_fetch_add(&s->refs,1);}
void backupfonts_soltar(BackupFontStage *s) {
  if(!s || atomic_fetch_sub(&s->refs,1)!=1)return;
  for(int i=0;i<s->manifest.n;i++)unlinkat(s->fd,s->manifest.font[i].id,0);
  close(s->fd);char nome[64];snprintf(nome,sizeof nome,"%s%s",stagePrefix,s->chave);unlinkat(s->raiz,nome,AT_REMOVEDIR);syncfd(s->raiz);close(s->raiz);free(s);
}
int backupfonts_stage_chunk(BackupFontStage *s,const BackupFont *f,int chunk,const void *data,size_t n) {
  if(!s || s->completo || !f || !backupfonts_id(f->id) || !f->n || f->n>BACKUP_FONT_BYTES || !data || chunk<0 || chunk>=BACKUP_FONT_CHUNKS)return 0;
  size_t inicio=(size_t)chunk*BACKUP_FONT_CHUNK;if(inicio>=f->n)return 0;
  size_t esperado=f->n-inicio;if(esperado>BACKUP_FONT_CHUNK)esperado=BACKUP_FONT_CHUNK;if(n!=esperado)return 0;
  int i=fonteN(&s->manifest,f->id),novo=0;
  if(i<0){if(chunk || s->manifest.n>=BACKUP_FONT_N)return 0;i=s->manifest.n++;s->manifest.font[i]=*f;novo=1;}
  if(s->manifest.font[i].n!=f->n || s->gravado[i]!=inicio)return 0;
  int fd=openat(s->fd,f->id,O_WRONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK|(novo?(O_CREAT|O_EXCL):0),0600);
  if(fd<0)return 0;
  struct stat st;int ok=!fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_size==(off_t)inicio && lseek(fd,0,SEEK_END)>=0 && gravarExato(fd,data,n) && syncfd(fd);
  if(close(fd))ok=0;
  if(ok)s->gravado[i]+=n;
  return ok;
}
static int fingerprint(int fd,const char *id,size_t n) {
  uint64_t h=UINT64_C(14695981039346656037);unsigned char buf[8192];size_t pos=0;
  if(lseek(fd,0,SEEK_SET)<0)return 0;
  while(pos<n){size_t k=n-pos;if(k>sizeof buf)k=sizeof buf;if(!lerExato(fd,buf,k))return 0;for(size_t i=0;i<k;i++)h=(h^buf[i])*UINT64_C(1099511628211);pos+=k;}
  char nome[17];snprintf(nome,sizeof nome,"%016llx",(unsigned long long)h);char extra;return !memcmp(nome,id,16) && read(fd,&extra,1)==0;
}
int backupfonts_stage_completo(BackupFontStage *s,const BackupFontManifest *m) {
  if(!s || !m || m->n!=s->manifest.n || m->n<0 || m->n>BACKUP_FONT_N)return 0;
  for(int i=0;i<m->n;i++) {
    const BackupFont *v=&m->font[i];int k=fonteN(&s->manifest,v->id);if(k<0 || s->gravado[k]!=v->n)return 0;
    int fd=openat(s->fd,v->id,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return 0;
    int ok=fingerprint(fd,v->id,v->n);close(fd);if(!ok)return 0;
  }
  if(!syncfd(s->fd))return 0;
  s->completo=1;return 1;
}
int backupfonts_stage_validado(const BackupFontStage *s){return s && s->completo && s->validado;}
void backupfonts_stage_validar(BackupFontStage *s){if(s && s->completo)s->validado=1;}
const BackupFontManifest *backupfonts_stage_manifest(const BackupFontStage *s){return s && s->completo?&s->manifest:NULL;}
int backupfonts_stage_ler(const BackupFontStage *s,int i,unsigned char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  if(!s || !s->completo || !out || !n || i<0 || i>=s->manifest.n)return 0;
  const BackupFont *f=&s->manifest.font[i];int fd=openat(s->fd,f->id,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return 0;
  unsigned char *v=malloc(f->n);int ok=v && lerExato(fd,v,f->n);close(fd);
  if(!ok){free(v);return 0;}*out=v;*n=f->n;return 1;
}
/* A before image records existence only: existing immutable fonts are never
 * overwritten. A rollback deletes only names introduced by this transaction. */
typedef struct {char id[21];int existe;} Arquivo;
typedef struct {Arquivo a[BACKUP_FONT_N*2];int n;char stage[7];} Arquivos;
static int chaveStage(const char *s){if(strlen(s)!=6)return 0;for(int i=0;i<6;i++)if(!((s[i]>='A' && s[i]<='Z') || (s[i]>='a' && s[i]<='z') || (s[i]>='0' && s[i]<='9')))return 0;return 1;}
static int lerArquivos(const char *s,Arquivos *a) {
  memset(a,0,sizeof *a);static const char cab[]="NUVIO-FONT-FILES 1\n";
  if(!s || strnlen(s,2049)>2048 || strncmp(s,cab,sizeof cab-1))return 0;
  for(const char *p=s+sizeof cab-1;*p;) {
    const char *e=strchr(p,'\n');if(!e || e-p>63)return 0;
    char l[64],id[21],key[7],existe[2];int usado=0;memcpy(l,p,(size_t)(e-p));l[e-p]=0;
    if(sscanf(l,"font %20s %1[01]%n",id,existe,&usado)==2 && usado && !l[usado]) {
      if(!backupfonts_id(id) || a->n>=BACKUP_FONT_N*2)return 0;
      for(int i=0;i<a->n;i++)if(!strcmp(a->a[i].id,id))return 0;
      strcpy(a->a[a->n].id,id);a->a[a->n++].existe=existe[0]-'0';
    } else if(sscanf(l,"stage %6s%n",key,&usado)==1 && usado && !l[usado] && !*a->stage && chaveStage(key))strcpy(a->stage,key);
    else return 0;
    p=e+1;
  }
  return 1;
}
static int exportarArquivos(const Arquivos *a,char **out,size_t *n) {
  char *s=malloc(2049);if(!s)return 0;
  size_t pos=(size_t)snprintf(s,2049,"NUVIO-FONT-FILES 1\n");
  if(*a->stage)pos+=(size_t)snprintf(s+pos,2049-pos,"stage %s\n",a->stage);
  for(int i=0;i<a->n;i++)pos+=(size_t)snprintf(s+pos,2049-pos,"font %s %d\n",a->a[i].id,a->a[i].existe);
  *out=s;*n=pos;return 1;
}
int backupfonts_stage_plano(const BackupFontStage *s,char **out,size_t *n) {
  if(out)*out=NULL;
  if(n)*n=0;
  if(!s || !s->completo || !out || !n)return 0;
  Arquivos a={0};strcpy(a.stage,s->chave);a.n=s->manifest.n;
  for(int i=0;i<a.n;i++){strcpy(a.a[i].id,s->manifest.font[i].id);a.a[i].existe=1;}
  return exportarArquivos(&a,out,n);
}
int backupfonts_disco_ler(int raiz,char **out,size_t *n) {
  int fd=openat(raiz,pasta,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);Arquivos a={0};
  if(fd<0){if(errno!=ENOENT)return 0;return exportarArquivos(&a,out,n);}
  BackupFontManifest m;int ok=listar(fd,&m);close(fd);if(!ok)return 0;
  a.n=m.n;for(int i=0;i<m.n;i++){strcpy(a.a[i].id,m.font[i].id);a.a[i].existe=1;}
  return exportarArquivos(&a,out,n);
}
int backupfonts_iguais(const char *x,const char *y) {
  Arquivos a,b;if(!lerArquivos(x,&a) || !lerArquivos(y,&b))return 0;
  for(int lado=0;lado<2;lado++) {
    const Arquivos *s=lado?&a:&b,*d=lado?&b:&a;
    for(int i=0;i<s->n;i++)if(s->a[i].existe) {
      int achou=0;for(int j=0;j<d->n;j++)if(d->a[j].existe && !strcmp(s->a[i].id,d->a[j].id))achou=1;
      if(!achou)return 0;
    }
  }
  return 1;
}
int backupfonts_plano(const char *antes,const char *entrada,char **rollback,char **depois) {
  if(rollback)*rollback=NULL;
  if(depois)*depois=NULL;
  Arquivos a,b;if(!rollback || !depois || !lerArquivos(antes,&a) || !lerArquivos(entrada,&b) || !*b.stage)return 0;
  Arquivos novo=a;strcpy(novo.stage,b.stage);
  for(int i=0;i<b.n;i++) {
    if(!b.a[i].existe)return 0;
    int achou=0;for(int j=0;j<a.n;j++)if(!strcmp(b.a[i].id,a.a[j].id) && a.a[j].existe)achou=1;
    if(!achou){if(novo.n>=BACKUP_FONT_N)return 0;novo.a[novo.n++]=b.a[i];a.a[a.n]=b.a[i];a.a[a.n++].existe=0;}
  }
  size_t n;int ok=exportarArquivos(&a,rollback,&n) && exportarArquivos(&novo,depois,&n);
  if(!ok){free(*rollback);free(*depois);*rollback=*depois=NULL;}return ok;
}
static int iguaisFd(int a,int b,size_t n) {
  unsigned char x[8192],y[8192];if(lseek(a,0,SEEK_SET)<0 || lseek(b,0,SEEK_SET)<0)return 0;
  while(n){size_t k=n<sizeof x?n:sizeof x;if(!lerExato(a,x,k) || !lerExato(b,y,k) || memcmp(x,y,k))return 0;n-=k;}return 1;
}
int backupfonts_disco_gravar(int raiz,const char *s,size_t n) {
  if(!s || memchr(s,0,n) || n>2048 || strlen(s)!=n)return 0;
  Arquivos a;if(!lerArquivos(s,&a))return 0;
  if(mkdirat(raiz,pasta,0700) && errno!=EEXIST)return 0;
  int fd=openat(raiz,pasta,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 0;
  int stage=-1,ok=1;
  if(*a.stage) {
    char nome[64];snprintf(nome,sizeof nome,"%s%s",stagePrefix,a.stage);
    stage=openat(raiz,nome,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(stage<0)ok=0;
  }
  for(int i=0;i<a.n && ok;i++) {
    struct stat st;int existe=!fstatat(fd,a.a[i].id,&st,AT_SYMLINK_NOFOLLOW);
    if(!existe && errno!=ENOENT){ok=0;break;}
    if(existe && (!S_ISREG(st.st_mode) || st.st_size<0 || st.st_size>BACKUP_FONT_BYTES || (a.a[i].existe && st.st_size<1))){ok=0;break;}
    if(!a.a[i].existe){if(existe && unlinkat(fd,a.a[i].id,0))ok=0;continue;}
    if(stage<0){if(!existe)ok=0;continue;}
    int src=openat(stage,a.a[i].id,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
    if(src<0){if(existe)continue;ok=0;break;}
    struct stat ss;ok=!fstat(src,&ss) && S_ISREG(ss.st_mode) && ss.st_size>0 && ss.st_size<=BACKUP_FONT_BYTES && fingerprint(src,a.a[i].id,(size_t)ss.st_size);
    if(ok && existe) {
      int dst=openat(fd,a.a[i].id,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
      ok=st.st_size==ss.st_size && dst>=0 && iguaisFd(src,dst,(size_t)ss.st_size);if(dst>=0)close(dst);
    } else if(ok) {
      // Android SELinux forbids hard links, even in app-private storage.
      // O_EXCL preserves immutable existing files. PREPARED is already durable
      // before this write; rollback removes this new name even after a crash
      // during a short/empty copy. No font registry is published until commit.
      int dst=openat(fd,a.a[i].id,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0600);
      ok=dst>=0 && lseek(src,0,SEEK_SET)>=0;
      unsigned char buf[8192];size_t left=(size_t)ss.st_size;
      while(ok && left) {
        size_t k=left<sizeof buf?left:sizeof buf;
        ok=lerExato(src,buf,k) && gravarExato(dst,buf,k);left-=k;
      }
      if(ok)ok=syncfd(dst);
      if(dst>=0 && close(dst))ok=0;
    }
    close(src);
  }
  if(ok)ok=syncfd(fd) && syncfd(raiz);
  if(stage>=0)close(stage);
  close(fd);return ok;
}

void backupfonts_limpar_pendentes(const char *dir) {
  int root=abrirRaiz(dir);if(root<0)return;
  int fd=openat(root,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);DIR *d=fd<0?NULL:fdopendir(fd);
  if(!d){if(fd>=0)close(fd);close(root);return;}
  struct dirent *e;int mudou=0;
  while((e=readdir(d))) {
    if(strncmp(e->d_name,stagePrefix,sizeof stagePrefix-1u) || !chaveStage(e->d_name+sizeof stagePrefix-1u))continue;
    int stage=openat(root,e->d_name,O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);if(stage<0)continue;
    int dup=openat(stage,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC|O_NOFOLLOW);DIR *inside=dup<0?NULL:fdopendir(dup);
    if(inside) {
      struct dirent *f;while((f=readdir(inside)))if(backupfonts_id(f->d_name))unlinkat(stage,f->d_name,0);
      closedir(inside);
    } else if(dup>=0)close(dup);
    close(stage);if(!unlinkat(root,e->d_name,AT_REMOVEDIR))mudou=1;
  }
  closedir(d);if(mudou)syncfd(root);close(root);
}
