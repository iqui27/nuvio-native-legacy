// Exercise the real cold Next transition without external addon/intro HTTP.
#define addons_buscar nextBuscar
#define addons_buscar_legendas nextLegendas
#include "../src/app.c"
#undef addons_buscar
#undef addons_buscar_legendas
#include <assert.h>
#include "../src/intro.h"

static char requested[64];
static int nextIntro(const char *url,char **body,int *status) {
  (void)url;*body=NULL;*status=204;return 204;
}
void nextBuscar(const char *id,const char *tipo) {
  (void)tipo;snprintf(requested,sizeof requested,"%s",id);
}
void nextLegendas(const char *id,const char *tipo) {(void)id;(void)tipo;}

int main(void) {
  setvbuf(stdout,NULL,_IONBF,0);
  const char *dir=getenv("NUVIO_DADOS");assert(dir && strstr(dir,"nuvio-"));
  dados_iniciar(dir);assert(!strcmp(dados_dir(),dir));
  // player_definir_episodio also requests intro markers on detached workers.
  // They are unrelated to source matching and must not initialize live TLS
  // while this short fixture is already exiting under ASan/LSan.
  intro_definir_buscador(nextIntro);
  char path[512];snprintf(path,sizeof path,"%s/ajustes.txt",dir);
  FILE *f=fopen(path,"w");assert(f);
  fputs("escolherFonteManual 0\nproximoAutoLocal 0\nfonteGrupoLocal 0\nclassifPlayerLocal 1\n",f);fclose(f);
  ajustes_dir(dir);assert(ajustes_fonte_manual() && ajustes_proximo_auto());
  fontepref_definir_perfil(1);fontepref_configurar(1,3600,1,1);
  CatItem item={0};CatEp eps[2]={{0}};
  snprintf(item.imdb,sizeof item.imdb,"tt4000020");
  snprintf(item.tipo,sizeof item.tipo,"series");item.temporada=item.episodio=1;
  item.nTemporadas=1;item.temporadas[0]=1; // Complete cached metadata: no background fetch.
  eps[0].temporada=eps[1].temporada=1;eps[0].episodio=1;eps[1].episodio=2;
  cat_definir(&item,1);cat_definir_episodios(0,eps,2);
  Stream source={0};source.altura=1080;source.emCache=1;source.fileIdx=-1;
  snprintf(source.url,sizeof source.url,"https://fixture.invalid/episode-1.mkv");
  snprintf(source.provedor,sizeof source.provedor,"AIOStreams");
  snprintf(source.bingeGroup,sizeof source.bingeGroup,"aio|1080p|remux");
  snprintf(source.arquivo,sizeof source.arquivo,"Show.S01E01.1080p.BluRay.REMUX.AVC.DTS-HD.MA.5.1-GRP.mkv");
  player_abrir(0,NULL);player_definir_episodio(1,1);
  fontepref_guardar(item.imdb,&source);fontepref_tocando(item.imdb,&source);
  // This calls player close/open AND the source-search reset, as does the
  // countdown or the player's Next button when no prepared result is ready.
  abrirProximoEpisodio(1,2,1);
  assert(!strcmp(requested,"tt4000020:1:2") && aguardandoFonte==1);
  assert(escolhaDoNext(item.imdb,NULL,0));
  assert(!escolhaDoNext("tt4000021",NULL,0));
  Stream next[2]={source,source};
  snprintf(next[0].bingeGroup,sizeof next[0].bingeGroup,"other-release");
  snprintf(next[0].arquivo,sizeof next[0].arquivo,"Show.S01E02.2160p.WEB-DL.HEVC-OTHER.mkv");
  next[0].altura=2160;
  snprintf(next[1].arquivo,sizeof next[1].arquivo,"Show.S01E02.1080p.BluRay.REMUX.AVC.DTS-HD.MA.5.1-GRP.mkv");
  stream_definir_lista(next,2);
  assert(fontepref_binge_tocando(item.imdb)==1);
  // A regular Play/manual episode selection must clear Next intent again.
  buscarParaPlayer();assert(!escolhaDoNext(item.imdb,NULL,0));
  assert(ajustes_fonte_manual());
  // Disabled group preference must still prevent following the playing file.
  snprintf(buscaNextAlvo,sizeof buscaNextAlvo,"%s",requested);
  fontepref_configurar(1,3600,0,1);assert(!escolhaDoNext(item.imdb,NULL,0));
  limparFonteVOD();player_encerrar();intro_desligar();
  puts("PASS: cold Next preserves playing-source intent through search reset; manual Play and disabled group remain manual");
  return 0;
}
