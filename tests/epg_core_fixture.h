#ifndef NV_TEST_EPG_CORE_FIXTURE_H
#define NV_TEST_EPG_CORE_FIXTURE_H
#include "stub_perfisessao.h"
#include "../src/epg.c"
#include <assert.h>
#include <sys/stat.h>
#include <unistd.h>
static const char *pasta;
static int pedidos;
static int (*responder)(const char *, int (*)(const char *,size_t,void *),void *,int *,long long *);
char *dados_caminho(char *out,unsigned cap,const char *nome) {
  int n=snprintf(out,cap,"%s/%s",pasta,nome);
  return n>=0 && (unsigned)n<cap?out:NULL;
}
char *dados_ler(const char *nome) {
  char cam[1024]; assert(dados_caminho(cam,sizeof cam,nome));
  FILE *f=fopen(cam,"rb"); if(!f)return NULL;
  assert(!fseek(f,0,SEEK_END)); long n=ftell(f);assert(n>=0);rewind(f);
  char *s=malloc((size_t)n+1);assert(s);
  assert(fread(s,1,(size_t)n,f)==(size_t)n);s[n]=0;fclose(f);return s;
}
int dados_gravar_leve(const char *nome,const char *s) {
  char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));FILE *f=fopen(cam,"wb");assert(f);
  assert(fwrite(s,1,strlen(s),f)==strlen(s));return fclose(f)==0;
}
int dados_apagar(const char *nome) {
  char cam[1024];assert(dados_caminho(cam,sizeof cam,nome));return unlink(cam)==0;
}
void dados_fs_travar(void) {}
void dados_fs_liberar(void) {}
void dados_marcar_sujo(int leve) {(void)leve;}
char *rede_baixar_bin_medido_controle(const char *url,int seg,const char *const *cab,
 const RedeControle *ctl,long *n,RedeMedida *m) {
  (void)url;(void)seg;(void)cab;(void)ctl;(void)n;(void)m;return NULL;
}
int rede_baixar_fluxo(const char *url,int seg,const char *const *cab,
 int (*cb)(const char *,size_t,void *),void *u,long long max,int *status,long long *bytes) {
  (void)seg;(void)cab;assert(max>0);pedidos++;
  return responder?responder(url,cb,u,status,bytes):0;
}
static void tempo(char *out,time_t t) {strftime(out,32,"%Y%m%d%H%M%S +0000",gmtime(&t));}
static void esperar(void) {
  for(int i=0;i<10000;i++) {
    epg_passo();if(!fioVivo && (epg_estado()==EPG_PRONTO || epg_estado()==EPG_FALHOU))return;
    struct timespec t={0,1000000};nanosleep(&t,NULL);
  }
  assert(!"schedule worker timed out");
}
#endif
