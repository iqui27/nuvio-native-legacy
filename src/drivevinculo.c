#ifdef __APPLE__
#define _DARWIN_C_SOURCE 1
#endif
#define _POSIX_C_SOURCE 200809L
#include "drivevinculo.h"
#include "backupcrypto.h"
#include "js.h"
#include "jsw.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int ascii(const char *s,size_t max) {
  if(!s || !*s)return 0;
  for(size_t i=0;s[i];i++)if(i>=max || (unsigned char)s[i]<33 || (unsigned char)s[i]>126)return 0;
  return 1;
}
static int caminho(const char *dir,const char *dono,int perfil,char out[4096]) {
  if(!dir || !*dir || !ascii(dono,80) || perfil<1)return 0;
  for(const char *p=dono;*p;p++)if(!((*p>='a' && *p<='z') || (*p>='A' && *p<='Z') || (*p>='0' && *p<='9') || *p=='_' || *p=='-'))return 0;
  int n=snprintf(out,4096,"%s/drive-auth-%s-p%d.json",dir,dono,perfil);
  return n>0 && n<4096;
}
char *drivevinculo_ler(const char *dir,const char *dono,int perfil,const char *cliente) {
  char path[4096],body[8193],id[513],token[4097];struct stat st;
  if(!ascii(cliente,512) || !caminho(dir,dono,perfil,path))return NULL;
  int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
  if(fd<0)return NULL;
  size_t n=0;int ok=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_size>0 && st.st_size<=8192;
  while(ok && n<(size_t)st.st_size) {
    ssize_t k=read(fd,body+n,(size_t)st.st_size-n);
    if(k<0 && errno==EINTR)continue;
    if(k<=0)ok=0;else n+=(size_t)k;
  }
  if(close(fd))ok=0;
  body[n]=0;char *out=NULL;
  if(ok && !memchr(body,0,n) && js_texto_raiz(body,"client",id,sizeof id) && !strcmp(id,cliente) &&
     js_texto_raiz(body,"refresh",token,sizeof token) && ascii(token,4096))out=strdup(token);
  backupcrypto_limpar(body,sizeof body);backupcrypto_limpar(token,sizeof token);
  return out;
}
int drivevinculo_gravar(const char *dir,const char *dono,int perfil,const char *cliente,const char *token) {
  char path[4096],tmp[4120];struct stat st;
  if(!token || !ascii(cliente,512) || (*token && !ascii(token,4096)) || !caminho(dir,dono,perfil,path))return 0;
  if(!*token)return unlink(path)==0 || errno==ENOENT;
  // Refuse unexpected targets; never follow a symlink while reading or writing.
  if(lstat(path,&st)==0){if(!S_ISREG(st.st_mode))return 0;}
  else if(errno!=ENOENT)return 0;
  snprintf(tmp,sizeof tmp,"%s.XXXXXX",path);int fd=mkstemp(tmp);
  if(fd<0)return 0;
  Jsw w;jsw_iniciar(&w);jsw_obj_ini(&w);jsw_cs(&w,"client",cliente);jsw_cs(&w,"refresh",token);jsw_obj_fim(&w);
  const char *s=jsw_texto_final(&w);size_t n=s?strlen(s):0,pos=0;
  int ok=n && fchmod(fd,0600)==0;
  while(ok && pos<n) {
    ssize_t k=write(fd,s+pos,n-pos);
    if(k<0 && errno==EINTR)continue;
    if(k<=0)ok=0;else pos+=(size_t)k;
  }
  if(ok && fsync(fd))ok=0;
  if(close(fd))ok=0;
  if(ok && rename(tmp,path))ok=0;
  if(!ok)unlink(tmp);
  if(s)backupcrypto_limpar((void *)s,n);
  jsw_livre(&w);return ok;
}
