#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utime.h>
#include "../src/dados.c"
#ifdef NV_ANDROID
const char *nv_android_cache_dir(void) { return ""; }
int nv_android_cache_name(const char *name) { (void)name; return 0; }
void nv_android_cache_touch(const char *path) { (void)path; }
#endif

#ifndef NV_ESPACO_IMPLEMENTADO
static void espacoLimpar(const char *d, const char *v) { (void)d; (void)v; }
static void espacoRelatar(FILE *f, const char *d, const char *a, double l) {
  (void)f; (void)d; (void)a; (void)l;
}
static void espacoLogs(void) { }
#endif
static void arquivo(const char *d, const char *n, long bytes, time_t t) {
  char p[1024]; struct utimbuf u = {t,t};
  snprintf(p, sizeof p, "%s/%s", d, n);
  FILE *f = fopen(p, "w"); assert(f); assert(ftruncate(fileno(f), bytes) == 0);
  /* Totais medem blocos reais, como percorrerL do P2P. */
  rewind(f); char bloco[4096] = {0};
  for(long k=0;k<bytes;k+=sizeof bloco) {
    size_t n=bytes-k<(long)sizeof bloco ? (size_t)(bytes-k) : sizeof bloco;
    assert(fwrite(bloco,1,n,f)==n);
  }
  fclose(f); assert(utime(p, &u) == 0);
}
static int existe(const char *d, const char *n) {
  char p[1024]; snprintf(p, sizeof p, "%s/%s", d, n); return access(p,F_OK)==0;
}
int main(int argc, char **argv) {
  assert(argc == 3); const char *d = argv[1]; char p[1024];
  if (!strcmp(argv[2], "atualizacao")) {
    snprintf(p,sizeof p,"%s/atualizacao",d); assert(mkdir(p,0700)==0);
    arquivo(p,"libnuvio-1.so",1,1); arquivo(p,"libnuvio-2.so",1,2);
    arquivo(p,"libnuvio-3.so",1,3); arquivo(p,"libnuvio-4.so",1,0);
    arquivo(d,"ajustes.txt",1,1); espacoLimpar(d,"4");
    assert(!existe(p,"libnuvio-1.so") && !existe(p,"libnuvio-2.so"));
    assert(existe(p,"libnuvio-3.so") && existe(p,"libnuvio-4.so"));
    assert(existe(d,"ajustes.txt"));
  } else if (!strcmp(argv[2], "sidecars")) {
    arquivo(d,"mkvass-old.ass",12<<20,1); arquivo(d,"mkvass-new.ass.fonts",12<<20,2);
    arquivo(d,"sessao.txt",1,0); arquivo(d,"salvos.txt",1,0);
    arquivo(d,"mkvass-write.ass.tmp",1,0);
    snprintf(p,sizeof p,"%s/mkvass-link.ass",d); assert(symlink("sessao.txt",p)==0);
    espacoLimpar(d,"4");
    assert(!existe(d,"mkvass-old.ass") && existe(d,"mkvass-new.ass.fonts"));
    assert(existe(d,"sessao.txt") && existe(d,"salvos.txt"));
    assert(existe(d,"mkvass-write.ass.tmp"));
    /* Verifica tambem o caminho de escrita usado por ASS/texto/fontes. */
    assert(setenv("NUVIO_DADOS",d,1)==0); dados_iniciar(d);
    char *corpo=malloc(12*1024*1024+1); assert(corpo);
    memset(corpo,'x',12*1024*1024); corpo[12*1024*1024]=0;
    assert(dados_gravar_leve("mkvass-write.ass",corpo)); free(corpo);
    assert(!existe(d,"mkvass-new.ass.fonts") && existe(d,"mkvass-write.ass"));
  } else if (!strcmp(argv[2], "logs")) {
    snprintf(p,sizeof p,"%s/nuvio-anterior.log",d); setenv("NUVIO_LOG_ANTERIOR",p,1);
    arquivo(d,"nuvio-anterior.log",3<<20,1); arquivo(d,"nuvio.log",1<<20,1);
    espacoLogs(); assert(!existe(d,"nuvio-anterior.log") && existe(d,"nuvio.log"));
  } else if (!strcmp(argv[2], "caches")) {
    const char *pastas[]={"trailer","extras"};
    for(int i=0;i<2;i++) {
      snprintf(p,sizeof p,"%s/%s",d,pastas[i]); assert(mkdir(p,0700)==0);
      arquivo(p,"old",12<<20,1); arquivo(p,"new",12<<20,2);
    }
    arquivo(d,"noticias2-old.txt",12<<20,1); arquivo(d,"noticias2-new.txt",12<<20,2);
    espacoLimpar(d,"4");
    for(int i=0;i<2;i++) {
      snprintf(p,sizeof p,"%s/%s",d,pastas[i]); assert(!existe(p,"old") && existe(p,"new"));
    }
    assert(!existe(d,"noticias2-old.txt") && existe(d,"noticias2-new.txt"));
  } else if (!strcmp(argv[2], "sessao")) {
    setenv("NUVIO_DADOS",d,1); dados_iniciar(d);
    FILE *f=tmpfile(); assert(f); int salvo=dup(fileno(stdout)); assert(salvo>=0);
    fflush(stdout); assert(dup2(fileno(f),fileno(stdout))>=0);
    dados_espaco_iniciar(d); dados_espaco_iniciar(d);
    assert(ftell(f)==0); sleep(6); fflush(stdout); rewind(f);
    char linha[512]; int n=0;
    while(fgets(linha,sizeof linha,f)) if(!strncmp(linha,"[espaco]",8)) n++;
    assert(n==1); assert(dup2(salvo,fileno(stdout))>=0); close(salvo); fclose(f);
#ifdef NV_TPK
  } else if (!strcmp(argv[2], "sandbox")) {
    snprintf(p,sizeof p,"%s/apps_rw",d); assert(mkdir(p,0700)==0);
    char raiz[1200],dados[1200]; snprintf(raiz,sizeof raiz,"%s/pkg",p); assert(mkdir(raiz,0700)==0);
    snprintf(dados,sizeof dados,"%s/data",raiz); assert(mkdir(dados,0700)==0);
    arquivo(dados,"perfil.txt",1<<20,1); arquivo(raiz,"cache-player",2<<20,1);
    FILE *f=tmpfile(); assert(f); espacoRelatar(f,dados,NULL,123); rewind(f);
    char linha[512]; assert(fgets(linha,sizeof linha,f)); fclose(f);
    assert(strstr(linha,"outros=3.0\n"));
#endif
  } else {
    const char *nomes[] = {"cache","trailer","atualizacao","p2p"};
    for(int i=0;i<4;i++) { snprintf(p,sizeof p,"%s/%s",d,nomes[i]); assert(mkdir(p,0700)==0); arquivo(p,"a",1<<20,1); }
    arquivo(d,"mkvass-texto.ass",2<<20,1); arquivo(d,"catalogo-rede.bin",3<<20,1);
    arquivo(d,"nuvio-anterior.log",4<<20,1); arquivo(d,"perfil.txt",5<<20,1);
    snprintf(p,sizeof p,"%s/res",d); assert(mkdir(p,0700)==0);
    char arte[1200]; snprintf(arte,sizeof arte,"%s/art",p); assert(mkdir(arte,0700)==0);
    arquivo(arte,"icone.png",1<<20,1);
    FILE *f=tmpfile(); assert(f); espacoRelatar(f,d,d,123); rewind(f);
    char linha[512]; assert(fgets(linha,sizeof linha,f)); fclose(f);
    assert(!strcmp(linha,"[espaco] livre=123.0 app: arte=2.0 legendas=2.0 catalogo=3.0 trailer=1.0 atualizacao=1.0 p2p=1.0 logs=4.0 outros=5.0\n"));
  }
  puts("PASS armazenamento"); return 0;
}
