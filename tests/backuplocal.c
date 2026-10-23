#include "backuplocal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
static void recusar(const char *s,size_t n,const char *dono,int perfil) {
  BackupLocal *b=(BackupLocal*)1;assert(!backuplocal_ler(s,n,dono,perfil,&b) && !b);
}
static char *alterar(const char *s,size_t n,const char *de,const char *para) {
  const char *p=strstr(s,de);assert(p && strlen(de)==strlen(para));
  char *out=malloc(n+1);assert(out);memcpy(out,s,n+1);memcpy(out+(p-s),para,strlen(para));
  unsigned long crc=(unsigned long)crc32(0L,(const Bytef *)out,(uInt)(n-15));
  assert(snprintf(out+n-15,16,"crc32 %08lx\n",crc)==15);return out;
}
int main(void) {
  BackupLocal *b=backuplocal_novo("user-1",7,1792330000),*l=NULL;assert(b);
  const char prefs[]="NUVIO-LOCAL-SETTINGS 1\nscope profile\nlocal 1\n";
  const char fontes[]="Sources\nPortuguês / Türkçe\n\tdata\n";
  assert(backuplocal_adicionar(b,BACKUP_AJUSTES_PERFIL,prefs,sizeof prefs-1));
  assert(backuplocal_adicionar(b,BACKUP_FONTES_PERFIL,fontes,sizeof fontes-1));
  assert(!backuplocal_adicionar(b,BACKUP_FONTES_PERFIL,fontes,sizeof fontes-1));
  assert(!backuplocal_adicionar(b,99,prefs,sizeof prefs-1));
  assert(!backuplocal_adicionar(b,BACKUP_PLAYER,"a\0b",3));
  char *s=NULL;size_t n=0;assert(backuplocal_exportar(b,&s,&n));
  assert(n==strlen(s) && backuplocal_quantidade(b)==2);
  assert(backuplocal_ler(s,n,"user-1",7,&l));size_t tam=0;
  assert(!strcmp(backuplocal_parte(l,BACKUP_AJUSTES_PERFIL,&tam),prefs) && tam==sizeof prefs-1);
  assert(!strcmp(backuplocal_parte(l,BACKUP_FONTES_PERFIL,&tam),fontes) && tam==sizeof fontes-1);
  assert(backuplocal_criado(l)==1792330000 && backuplocal_quantidade(l)==2);
  backuplocal_liberar(l);
  char dono[81]="untouched";int perfil=99;
  assert(backuplocal_identidade(s,n,dono,sizeof dono,&perfil) && !strcmp(dono,"user-1") && perfil==7);
  assert(!backuplocal_identidade(s,n,dono,2,&perfil));
  assert(backuplocal_compativel(b,"user-1",7) && !backuplocal_compativel(b,"user-2",7));
  recusar(s,n,"user-2",7);recusar(s,n,"user-1",8);recusar(s,n,"",7);
  for(size_t i=0;i<n;i++){char c=s[i];s[i]^=1;recusar(s,n,"user-1",7);s[i]=c;}
  for(size_t i=0;i<n;i++)recusar(s,i,"user-1",7);
  const char *de[]={"BACKUP 1","profile 7","parts 2","profile-settings","part sources-profile"};
  const char *para[]={"BACKUP 9","profile 0","parts 9","../../sessao.txt","part profile-setting"};
  for(size_t i=0;i<sizeof de/sizeof *de;i++){char *m=alterar(s,n,de[i],para[i]);recusar(m,n,"user-1",7);free(m);}
  // Many small modules and one large store fit within the aggregate bound;
  // independently valid parts must still be rejected if their total is too big.
  char *parte=malloc(BACKUP_PARTE_MAX);assert(parte);memset(parte,'x',BACKUP_PARTE_MAX);
  BackupLocal *cheio=backuplocal_novo("user-1",7,1);assert(cheio);
  for(int i=0;i<BACKUP_PARTES_N;i++)assert(backuplocal_adicionar(cheio,i,parte,i==BACKUP_LOCAIS?BACKUP_PARTE_MAX:65536));
  char *grande=NULL;size_t ng;assert(backuplocal_exportar(cheio,&grande,&ng));
  assert(backuplocal_ler(grande,ng,"user-1",7,&l));backuplocal_liberar(l);free(grande);backuplocal_liberar(cheio);
  cheio=backuplocal_novo("user-1",7,1);assert(cheio);
  for(int i=0;i<3;i++)assert(backuplocal_adicionar(cheio,i,parte,BACKUP_PARTE_MAX));
  assert(!backuplocal_exportar(cheio,&grande,&ng) && !grande && !ng);free(parte);backuplocal_liberar(cheio);
  BackupLocal *vazio=backuplocal_novo("user-1",7,1);char *out=(char*)1;size_t no=1;
  assert(!backuplocal_exportar(vazio,&out,&no) && !out && !no);backuplocal_liberar(vazio);
  assert(!backuplocal_novo("../sessao",7,1));assert(!backuplocal_novo("user-1",0,1));
  free(s);backuplocal_liberar(b);puts("PASS: bounded backup envelope, module allowlist, account/profile binding, corruption and truncation rejection, and UTF-8 payloads");
}
