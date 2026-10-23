#include "addonmodo.h"
#include "dados.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#define MAX 256
static struct { uint64_t id; int modo, visto, http, valida, diretos, torrents; } tab[MAX];
static int n;
static char arquivo[96];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
uint64_t addonmodo_id(const char *s) {
  uint64_t h = UINT64_C(14695981039346656037);
  if (!s || !*s) return 0;
  for (; *s; s++) { h ^= (unsigned char)*s; h *= UINT64_C(1099511628211); }
  return h ? h : 1;
}
static int slot(uint64_t id, int criar) {
  if (!id) return -1;
  for (int i=0;i<n;i++) if(tab[i].id==id) return i;
  if (!criar || n==MAX) return -1;
  tab[n].id=id; return n++;
}
static int salvar(void) {
  char buf[MAX*32]; size_t u=0;
  for(int i=0;i<n;i++) u+=(size_t)snprintf(buf+u,sizeof buf-u,"%016" PRIx64 " %d %d\n",tab[i].id,tab[i].modo,tab[i].visto);
  buf[u]=0;
  return dados_gravar(arquivo,buf);
}
static void carregar(const char *nome) {
  char *t=dados_ler(nome), *p=t;
  memset(tab,0,sizeof tab); n=0; snprintf(arquivo,sizeof arquivo,"%s",nome);
  while(p && *p && n<MAX) {
    uint64_t id; int modo,visto;
    if(sscanf(p,"%" SCNx64 " %d %d",&id,&modo,&visto)==3 && id && modo>=AM_AUTO && modo<=AM_DIRETO && (visto==0||visto==1)) {
      int i=slot(id,1); tab[i].modo=modo; tab[i].visto=visto;
    }
    p=strchr(p,'\n'); if(p) p++;
  }
  free(t);
}
static void nomeArquivo(const char *conta,int perfil,char *out,size_t tam) {
  snprintf(out,tam,"addon-modes-%016" PRIx64 "-p%d.txt",addonmodo_id(conta),perfil>0?perfil:1);
}
void addonmodo_carregar(const char *conta,int perfil) {
  char nome[96]; nomeArquivo(conta,perfil,nome,sizeof nome);
  pthread_mutex_lock(&trava); carregar(nome); pthread_mutex_unlock(&trava);
}
void addonmodo_preparar(const char *conta,int perfil) {
  char nome[96]; nomeArquivo(conta,perfil,nome,sizeof nome);
  pthread_mutex_lock(&trava); if(strcmp(nome,arquivo)) carregar(nome); pthread_mutex_unlock(&trava);
}
AddonModo addonmodo_get(uint64_t id) {
  pthread_mutex_lock(&trava); int i=slot(id,0), m=i<0?AM_AUTO:tab[i].modo; pthread_mutex_unlock(&trava); return m;
}
int addonmodo_visto(uint64_t id) {
  pthread_mutex_lock(&trava); int i=slot(id,0), v=i>=0&&tab[i].visto; pthread_mutex_unlock(&trava); return v;
}
int addonmodo_set(uint64_t id,int modo) {
  if(modo<AM_AUTO||modo>AM_DIRETO) return 0;
  pthread_mutex_lock(&trava); int i=slot(id,1), ok=0;
  if(i>=0) { int old=tab[i].modo; tab[i].modo=modo; ok=salvar(); if(!ok)tab[i].modo=old; }
  pthread_mutex_unlock(&trava); return ok;
}
int addonmodo_marcar(uint64_t id) {
  pthread_mutex_lock(&trava); int i=slot(id,1),ok=0;
  if(i>=0) { tab[i].visto=1; ok=salvar(); if(!ok)tab[i].visto=0; }
  pthread_mutex_unlock(&trava); return ok;
}
void addonmodo_resposta(uint64_t id,int http,int valida,int diretos,int torrents) {
  pthread_mutex_lock(&trava); int i=slot(id,1);
  if(i>=0) {tab[i].http=http;tab[i].valida=valida;tab[i].diretos=diretos;tab[i].torrents=torrents;}
  pthread_mutex_unlock(&trava);
}
void addonmodo_fatos(uint64_t id,int *http,int *valida,int *diretos,int *torrents) {
  pthread_mutex_lock(&trava); int i=slot(id,0);
  if(http)*http=i<0?0:tab[i].http; if(valida)*valida=i<0?0:tab[i].valida;
  if(diretos)*diretos=i<0?0:tab[i].diretos; if(torrents)*torrents=i<0?0:tab[i].torrents;
  pthread_mutex_unlock(&trava);
}
int addonmodo_permite(uint64_t id,int direto,int debrid,int p2p) {
  if(direto) return 1;
  switch(addonmodo_get(id)) {
    case AM_DIRETO:return 0; case AM_P2P:return p2p; case AM_DEBRID:return debrid;
    default:return debrid||p2p;
  }
}
AddonGuiaRazao addonguia_razao(int http,int offline,int p2p,int debrid,int hdr,int vazio) {
  if(offline)return AG_OFFLINE;
  if(http==401)return AG_401; if(http==403)return AG_403;
  if(p2p)return AG_P2P; if(debrid)return AG_DEBRID; if(hdr)return AG_HDR;
  return vazio?AG_VAZIO:AG_ERRO;
}
AddonGuiaAcao addonguia_acao(AddonGuiaRazao r,int motor,int proxima) {
  if(r==AG_P2P)return motor?AGA_P2P:AGA_CONECTAR;
  if(r==AG_401||r==AG_403)return AGA_CONFIG;
  return proxima?AGA_PROXIMA:AGA_BUSCAR;
}
