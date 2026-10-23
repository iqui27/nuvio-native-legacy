#include "backuplocal.h"
#include "backupfonts.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static const char *const tipos[BACKUP_PARTES_N]={
  "device-settings","profile-settings","player-style","sources-shared","sources-profile","home-rows",
  "hardware-settings","local-stores","imported-fonts"
};
struct BackupLocal {
  BackupFontStage *fontes;
  char dono[81];int perfil;long long criado;
  struct {char *texto;size_t n;} parte[BACKUP_PARTES_N];
};
static int donoValido(const char *s) {
  if(!s)return 0;
  size_t n=0;
  for(;s[n];n++) {
    unsigned char c=(unsigned char)s[n];
    if(n>=80 || !((c>='a'&&c<='z') || (c>='A'&&c<='Z') || (c>='0'&&c<='9') || c=='_' || c=='-'))return 0;
  }
  return n>0;
}
BackupLocal *backuplocal_novo(const char *dono,int perfil,long long criado) {
  if(!donoValido(dono) || perfil<1 || criado<1 || criado>253402300799LL)return NULL;
  BackupLocal *b=calloc(1,sizeof *b);if(!b)return NULL;
  snprintf(b->dono,sizeof b->dono,"%s",dono);b->perfil=perfil;b->criado=criado;return b;
}
void backuplocal_liberar(BackupLocal *b) {
  if(!b)return;
  for(int i=0;i<BACKUP_PARTES_N;i++) {
    volatile char *p=b->parte[i].texto;size_t n=b->parte[i].n;
    if(p)while(n--)*p++=0;
    free(b->parte[i].texto);
  }
  backupfonts_soltar(b->fontes);free(b);
}
void backuplocal_fontes_vincular(BackupLocal *b,BackupFontStage *fontes) {
  if(!b || b->fontes==fontes)return;
  backupfonts_reter(fontes);backupfonts_soltar(b->fontes);b->fontes=fontes;
}
BackupFontStage *backuplocal_fontes(const BackupLocal *b){return b?b->fontes:NULL;}
int backuplocal_adicionar(BackupLocal *b,int tipo,const char *texto,size_t n) {
  if(!b || tipo<0 || tipo>=BACKUP_PARTES_N || b->parte[tipo].texto || !texto || !n || n>BACKUP_PARTE_MAX || memchr(texto,0,n))return 0;
  char *p=malloc(n+1);if(!p)return 0;
  memcpy(p,texto,n);p[n]=0;b->parte[tipo].texto=p;b->parte[tipo].n=n;return 1;
}
const char *backuplocal_parte(const BackupLocal *b,int tipo,size_t *n) {
  if(n)*n=0;
  if(!b || tipo<0 || tipo>=BACKUP_PARTES_N)return NULL;
  if(n)*n=b->parte[tipo].n;
  return b->parte[tipo].texto;
}
int backuplocal_quantidade(const BackupLocal *b) {
  int n=0;if(b)for(int i=0;i<BACKUP_PARTES_N;i++)n+=b->parte[i].texto!=NULL;return n;
}
long long backuplocal_criado(const BackupLocal *b) {return b?b->criado:0;}
int backuplocal_exportar(const BackupLocal *b,char **saida,size_t *n) {
  if(saida)*saida=NULL;
  if(n)*n=0;
  int partes=backuplocal_quantidade(b);
  if(!saida || !n || !b || !partes)return 0;
  size_t capacidade=256;
  for(int i=0;i<BACKUP_PARTES_N;i++)if(b->parte[i].texto) {
    if(b->parte[i].n+100u>BACKUP_LOCAL_MAX-capacidade)return 0;
    capacidade+=b->parte[i].n+100u;
  }
  char *out=malloc(capacidade);if(!out)return 0;
  size_t usado=(size_t)snprintf(out,capacidade,"NUVIO-LOCAL-BACKUP 1\nowner %s\nprofile %d\ncreated %lld\nparts %d\n",b->dono,b->perfil,b->criado,partes);
  for(int i=0;i<BACKUP_PARTES_N;i++)if(b->parte[i].texto) {
    int k=snprintf(out+usado,capacidade-usado,"part %s %zu\n",tipos[i],b->parte[i].n);
    if(k<0 || (size_t)k>=capacidade-usado){free(out);return 0;}
    usado+=(size_t)k;
    if(b->parte[i].n+1u>=capacidade-usado){free(out);return 0;}
    memcpy(out+usado,b->parte[i].texto,b->parte[i].n);usado+=b->parte[i].n;out[usado++]='\n';
  }
  uLong crc=crc32(0L,(const Bytef *)out,(uInt)usado);
  int k=snprintf(out+usado,capacidade-usado,"crc32 %08lx\n",(unsigned long)crc);
  if(k!=15 || (size_t)k>capacidade-usado){free(out);return 0;}
  *saida=out;*n=usado+(size_t)k;return 1;
}
typedef struct {const char *p,*fim;} BlLeitor;
static int linha(BlLeitor *r,char *out,size_t cap) {
  const char *fim=memchr(r->p,'\n',(size_t)(r->fim-r->p));
  if(!fim || (size_t)(fim-r->p)>=cap)return 0;
  size_t n=(size_t)(fim-r->p);memcpy(out,r->p,n);out[n]=0;r->p=fim+1;return 1;
}
static int numero(const char *s,unsigned long long max,unsigned long long *out) {
  if(!s || !*s)return 0;
  unsigned long long n=0;
  for(;*s;s++) {
    if(*s<'0' || *s>'9')return 0;
    unsigned d=(unsigned)(*s-'0');if(d>max || n>(max-d)/10u)return 0;n=n*10u+d;
  }
  *out=n;return 1;
}
static int campoNumero(BlLeitor *r,const char *chave,unsigned long long max,unsigned long long *out) {
  char l[128];if(!linha(r,l,sizeof l))return 0;
  size_t n=strlen(chave);return !strncmp(l,chave,n) && l[n]==' ' && numero(l+n+1,max,out);
}
static int verificarCrc(const char *s,size_t n) {
  if(n<15 || memcmp(s+n-15,"crc32 ",6) || s[n-1]!='\n')return 0;
  uint32_t v=0;
  for(size_t i=n-9;i<n-1;i++) {
    unsigned char c=(unsigned char)s[i];unsigned d;
    if(c>='0' && c<='9')d=c-'0';
    else if(c>='a' && c<='f')d=c-'a'+10;
    else return 0;
    v=(v<<4)|d;
  }
  return (uint32_t)crc32(0L,(const Bytef *)s,(uInt)(n-15))==v;
}
int backuplocal_ler(const char *texto,size_t n,const char *dono,int perfil,BackupLocal **saida) {
  if(saida)*saida=NULL;
  if(!saida || !texto || !n || n>BACKUP_LOCAL_MAX || !donoValido(dono) || perfil<1 || memchr(texto,0,n) || !verificarCrc(texto,n))return 0;
  BlLeitor r={texto,texto+n-15};char l[160];unsigned long long p,t,partes;
  if(!linha(&r,l,sizeof l) || strcmp(l,"NUVIO-LOCAL-BACKUP 1") ||
     !linha(&r,l,sizeof l) || strncmp(l,"owner ",6) || strcmp(l+6,dono) ||
     !campoNumero(&r,"profile",2147483647,&p) || p!=(unsigned)perfil ||
     !campoNumero(&r,"created",253402300799ULL,&t) || !t ||
     !campoNumero(&r,"parts",BACKUP_PARTES_N,&partes) || !partes)return 0;
  BackupLocal *b=backuplocal_novo(dono,perfil,(long long)t);if(!b)return 0;
  for(unsigned i=0;i<partes;i++) {
    if(!linha(&r,l,sizeof l) || strncmp(l,"part ",5))goto falha;
    char *espaco=strchr(l+5,' ');if(!espaco)goto falha;*espaco++=0;
    int tipo=0;while(tipo<BACKUP_PARTES_N && strcmp(l+5,tipos[tipo]))tipo++;
    unsigned long long tamanho;
    if(tipo==BACKUP_PARTES_N || !numero(espaco,BACKUP_PARTE_MAX,&tamanho) || !tamanho ||
       tamanho>=(size_t)(r.fim-r.p) || r.p[tamanho]!='\n' ||
       !backuplocal_adicionar(b,tipo,r.p,(size_t)tamanho))goto falha;
    r.p+=(size_t)tamanho+1;
  }
  if(r.p!=r.fim)goto falha;
  *saida=b;return 1;
falha:
  backuplocal_liberar(b);return 0;
}

int backuplocal_compativel(const BackupLocal *b,const char *dono,int perfil) {
  return b && dono && b->perfil==perfil && !strcmp(b->dono,dono);
}
int backuplocal_identidade(const char *texto,size_t n,char *dono,size_t cap,int *perfil) {
  if(!texto || !dono || !perfil || n>BACKUP_LOCAL_MAX || memchr(texto,0,n) || !verificarCrc(texto,n))return 0;
  BlLeitor r={texto,texto+n-15};char l[160],usuario[81];unsigned long long p;
  if(!linha(&r,l,sizeof l) || strcmp(l,"NUVIO-LOCAL-BACKUP 1") ||
     !linha(&r,l,sizeof l) || strncmp(l,"owner ",6) || !donoValido(l+6) || strlen(l+6)>=sizeof usuario)return 0;
  memcpy(usuario,l+6,strlen(l+6)+1);
  if(!campoNumero(&r,"profile",2147483647,&p) || !p || strlen(usuario)>=cap)return 0;
  BackupLocal *b=NULL;if(!backuplocal_ler(texto,n,usuario,(int)p,&b))return 0;
  backuplocal_liberar(b);memcpy(dono,usuario,strlen(usuario)+1);*perfil=(int)p;return 1;
}
