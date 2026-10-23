// Cache em disco do catalogo: ONDE ele e gravado e DE QUEM ele e.
//
// Os dois defeitos que este arquivo cobre eram invisiveis no Mac e fatais no
// Tizen e depois de um logout, respectivamente:
//
//   (a) o cache ia para a pasta do PACOTE (`dirArte`). No Tizen /app/art vem de
//       --preload-file, ou seja MEMFS: RAM apagada a cada recarga. A escrita
//       nao falha, o arquivo simplesmente nao existe na proxima abertura, e o
//       app pagava de novo os 14,5 s de rede medidos na TV.
//
//   (b) o cabecalho validava so a ESTRUTURA (magia, versao, sizeof, contagens).
//       Depois de trocar de conta ou de perfil, a home nascia com o catalogo da
//       ANTERIOR — e cada CatFileira leva `base[600]`, campo desse tamanho
//       porque o Xperience embute um JWT no CAMINHO. O arquivo e credencial.
//
// So src/catalogo.c e linkado, com dubles para tudo o resto: o cache nao
// depende de SDL, de rede nem da descoberta, e linkar o app inteiro aqui
// tornaria o teste lento e fragil (mesma razao de tests/catordem.sh).
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "../src/catalogo.h"
#include "../src/progresso.h"

// O cache grava o idioma no cabecalho (texto ja montado nao pode ser lido de
// volta no idioma errado). 0 = portugues, que e o padrao.
int ajustes_idioma_ingles(void) { return 0; }
int ajustes_idioma(void) { return 0; }
// cat_carregar traduz o rotulo do tipo do catalogo do pacote por aqui. Devolver
// a entrada e o que i18n faz em portugues, que e o idioma deste teste.
const char *i18n(const char *s) { return s; }
const char *idioma_mes_data(int mes, const char *nomePt) { (void)mes; return nomePt; }

// --- DUBLES ------------------------------------------------------------------
// A pasta gravavel e a identidade sao entradas do teste, nao do ambiente: e
// mexendo nelas que se exercita a regra.
static char  dirDados[512];
static char  usuarioAtual[64];
static int   perfilAtual = 1;

const char *dados_dir(void)     { return dirDados; }
const char *sessao_usuario(void){ return usuarioAtual; }
int         perfis_ativo(void)  { return perfilAtual; }

const char *desc_genero_pt(const char *g) { return g; }
int prog_ler(ProgRegistro *saida, int max) { (void)saida; (void)max; return 0; }
int prog_gravar_local(const char *imdb, int t, int e, double p, double d, int tocou) {
  (void)imdb; (void)t; (void)e; (void)p; (void)d; (void)tocou; return 0;
}


#include <time.h>
#include "nvtmp.h"
static double agora(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
int main(int argc,char**argv){
  int nit = argc>1?atoi(argv[1]):1600, nf=16, i;
  CatItem *l=calloc(nit,sizeof *l); CatFileira *f=calloc(nf,sizeof *f);
  double t0,t1,t2;
  snprintf(dirDados,sizeof dirDados,"%s",nvt("/catbench-dir")); mkdir(dirDados,0755);
  snprintf(usuarioAtual,sizeof usuarioAtual,"u1");
  for(i=0;i<nit;i++){snprintf(l[i].imdb,sizeof l[i].imdb,"tt%07d",i);snprintf(l[i].titulo,sizeof l[i].titulo,"Titulo %d",i);
    snprintf(l[i].poster,sizeof l[i].poster,"https://images.metahub.space/poster/medium/tt%07d/img",i);
    snprintf(l[i].backdrop,sizeof l[i].backdrop,"https://images.metahub.space/background/medium/tt%07d/img",i);
    snprintf(l[i].logo,sizeof l[i].logo,"https://images.metahub.space/logo/medium/tt%07d/img",i);
    snprintf(l[i].genero,sizeof l[i].genero,"Filme · Drama · Misterio");
    memset(l[i].sinopse,'s',350);
    for(int e=0;e<6;e++){snprintf(l[i].elenco[e].nome,64,"Ator Numero %d",e);snprintf(l[i].elenco[e].papel,64,"Personagem %d",e);snprintf(l[i].elenco[e].foto,512,"https://image.tmdb.org/t/p/w185/abcdefghijklmnop%d.jpg",e);}}
  for(i=0;i<nf;i++){snprintf(f[i].chave,sizeof f[i].chave,"row%d",i);f[i].ini=i*(nit/nf);f[i].n=nit/nf;}
  for(int r=0;r<3;r++){
    t0=agora(); cat_definir_tudo(l,nit,f,nf); t1=agora();
    printf("ret=%d\n",cat_gravar_cache(dirDados)); t2=agora();
    { double t3=agora(); int ok=cat_ler_cache(dirDados); double t4=agora();
      int same = ok && cat_n()==nit && !memcmp(cat_item(7),&l[7],sizeof(CatItem)) && !memcmp(cat_item(nit-1),&l[nit-1],sizeof(CatItem));
      printf("ler_cache=%.1f ms roundtrip=%s\n",t4-t3,same?"IGUAL":"DIFERENTE"); }
    printf("itens=%d sizeof(CatItem)=%zu definir_tudo=%.1f ms gravar_cache=%.1f ms (%.1f MB)\n",nit,sizeof(CatItem),t1-t0,t2-t1,nit*(double)sizeof(CatItem)/1048576);
  }
  return 0;}
