#define seriefrases_n quoteCountFixture
#define seriefrases_n_fatos factCountFixture
#define extras_comentario_spoiler comSpoilerFixture
#define extras_n_comentarios comCountFixture
#define extras_relacionado_imdb relFixture
#include "../src/detail.c"
#undef extras_relacionado_imdb
static int quoteCount;
int quoteCountFixture(void) { return quoteCount; }
int factCountFixture(void) { return 0; }
int comSpoilerFixture(int ep,int i) { (void)ep; return i == 0; }
int comCountFixture(void) { return 3; }
const char *relFixture(int i) { (void)i; return "tt9000011"; }
#include <assert.h>
#include "dados.h"

int main(void) {
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  dados_iniciar("deploy/app/art");
  ajustes_iniciar();
  ajustes_dir(dados_dir());
  assert(ajustes_detalhe_topo() == 0);
  char path[1024];
  snprintf(path, sizeof path, "%s/ajustes.txt", dados_dir());
  for (int mode = 0; mode < 3; mode++) {
    FILE *f = fopen(path, "w"); assert(f);
    fprintf(f, "detalheTopoLocal %d\n", mode); fclose(f);
    ajustes_dir(dados_dir());
    assert(ajustes_detalhe_topo() == mode);
    assert(topoVersao() == (mode != 1));
    assert(topoPessoal() == (mode != 0));
  }
  CatItem itemTeste = {0};
  snprintf(itemTeste.tipo, sizeof itemTeste.tipo, "series");
  snprintf(itemTeste.imdb, sizeof itemTeste.imdb, "tt9000011");
  itemTeste.temporadas[0] = 1; itemTeste.nTemporadas = 1;
  cat_definir_tudo(&itemTeste, 1, NULL, 0); idx = 0;
  CatEp ep = {0}; ep.temporada = 1; ep.episodio = 1;
  cat_definir_episodios(0, &ep, 1);
  FILE *f = fopen(path, "w"); assert(f);
  fputs("blurUnwatchedEpisodes 0\n", f); fclose(f); ajustes_dir(dados_dir());
  assert(epOculto(&ep)); // unknown history must not leak a spoiler
  vistoep_definir(itemTeste.imdb, 1, 1, 1);
  assert(!epOculto(&ep));
  vistoep_definir(itemTeste.imdb, 1, 1, 0);
  assert(epOculto(&ep));
  nivel = 1; foco.fileira = SEC_EPISODIOS; foco.coluna = 0;
  saindo = 0; pedReproduzir = 0;
  SDL_Event e = {0}; e.type = SDL_KEYUP; e.key.keysym.sym = SDLK_RETURN;
  okDesceEm = SDL_GetTicks() - NV_HOLD_MS - 2;
  detail_evento(&e);
  assert(!epOculto(&ep) && !pedReproduzir);
  ep.episodio = 2; assert(epOculto(&ep)); // reveal belongs to one episode
  CatEp grid[4] = {{0}};
  grid[0].temporada=1; grid[0].episodio=1; grid[0].nota=72;
  grid[1].temporada=1; grid[1].episodio=2; grid[1].nota=81;
  grid[2].temporada=2; grid[2].episodio=1; grid[2].nota=90;
  grid[3].temporada=2; grid[3].episodio=2; grid[3].nota=79;
  cat_definir_episodios(0, grid, 4);
  assert(progVizinho(1, 1) == 3);
  assert(progVizinho(2, -1) == 0);
  assert(progVizinho(0, -1) == -1);
  assert(secaoN(SEC_PROGTEMP) == 4);
  char mono[12];
  elencoMonograma("Ryan Gosling", mono, sizeof mono); assert(!strcmp(mono,"RG"));
  elencoMonograma("Óscar", mono, sizeof mono); assert(!strcmp(mono,"Ó"));
  elencoMonograma("", mono, sizeof mono); assert(!strcmp(mono,""));
  assert(!relacionadoVisto(0));
  cat_historico_definir_id(itemTeste.imdb,itemTeste.tipo,1);
  assert(relacionadoVisto(0));
  assert(!strcmp(cabecalhoDe(SEC_RELACIONADOS),"Mais como este"));
  comentEp=0; memset(comRevelados,0,sizeof comRevelados); comInteiro=-1;
  assert(comOculto(0));
  comAbrir(0); assert(!comOculto(0) && comInteiro==-1);
  comAbrir(0); assert(comInteiro==0);
  comInteiro=-1; comAbrir(1); assert(comInteiro==1);
  quoteCount=0; assert(secaoN(SEC_FRASES)==0);
  quoteCount=1; frasesAberta=0; assert(secaoN(SEC_FRASES)==0); // prior title data stays hidden
  frasesAberta=1; assert(secaoN(SEC_FRASES)==1);
  snprintf(itemTeste.titulo,sizeof itemTeste.titulo,"Test title");
  cat_definir_tudo(&itemTeste,1,NULL,0);
  assert(secaoN(SEC_DETALHES)>=0); // missing original metadata leaves no fabricated facts
  assert(ORDEM_FILME[0]==SEC_NOTAS && ORDEM_FILME[1]==SEC_TRAILERS);
  assert(ORDEM_FILME[3]==SEC_COLECAO);
  snprintf(itemTeste.tipo,sizeof itemTeste.tipo,"movie");
  cat_definir_tudo(&itemTeste,1,NULL,0);
  nivel=0; botao=1; focoVersao=0; vaiTocar.tem=1;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_DOWN;
  detail_evento(&e); assert(!focoVersao); // other buttons can reach the body
  nivel=1; foco.fileira=SEC_RELACIONADOS; foco.coluna=2; scrollY=400;
  memset(foco.nColunas,0,sizeof foco.nColunas);
  foco.nColunas[SEC_FRASES]=1; foco.fileira=SEC_FRASES;
  quoteCount=0; sincronizarColunas();
  assert(nivel==0 || secaoN(foco.fileira)>0); // a vanished row never traps focus
  nivel=1; foco.fileira=SEC_RELACIONADOS; foco.coluna=2; scrollY=400;
  aberto=1; saindo=0; nVolta=0;
  detail_volta_notar(1);
  assert(nVolta==1 && voltaEstado[0].coluna==2 && voltaEstado[0].fileira==SEC_RELACIONADOS);
  FILE *perfil=fopen(path,"w"); assert(perfil); fputs("detalheTopoLocal 2\n",perfil); fclose(perfil); ajustes_dir(dados_dir());
  assert(ajustes_detalhe_topo()==2);
  char perfilPath[1024]; snprintf(perfilPath,sizeof perfilPath,"%s/ajustes-p9.txt",dados_dir());
  perfil=fopen(perfilPath,"w"); assert(perfil); fputs("menuTopoLocal 0\n",perfil); fclose(perfil);
  ajustes_perfil_restaurar(9);
  assert(ajustes_detalhe_topo()==0); // pre-existing profiles must not inherit Both
  CatItem cast[2]={{0}};
  snprintf(cast[0].tipo,sizeof cast[0].tipo,"movie"); snprintf(cast[1].tipo,sizeof cast[1].tipo,"movie");
  snprintf(cast[0].imdb,sizeof cast[0].imdb,"tt9011001"); snprintf(cast[1].imdb,sizeof cast[1].imdb,"tt9011002");
  cast[0].elenco[0].tmdb=42; cast[0].nElenco=1;
  cast[1].elenco[0].tmdb=42; cast[1].nElenco=1;
  cat_definir_tudo(cast,2,NULL,0); idx=0;
  assert(elencoVistoEm(42)==-1);
  cat_historico_definir_id(cast[1].imdb,"movie",1);
  assert(elencoVistoEm(42)==1);
  assert(elencoVistoEm(0)==-1);
  assert(!extras_titulo_original()[0]);
  LinhaDet facts[NV_DETF_DET_MAXL];
  int nf=montarDetalhes(facts,NV_DETF_DET_MAXL);
  for(int i=0;i<nf;i++) assert(facts[i].valor[0]);
  itemTeste.nTemporadas=1; itemTeste.temporadas[0]=1; snprintf(itemTeste.tipo,sizeof itemTeste.tipo,"series");
  cat_definir_tudo(&itemTeste,1,NULL,0); idx=0;
  assert(secaoN(SEC_TEMPORADAS)==3); // shuffle, real season, spoiler switch
  temporada=0; foco.fileira=SEC_TEMPORADAS; foco.coluna=2; nivel=1;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RETURN; detail_evento(&e);
  e.type=SDL_KEYUP; detail_evento(&e);
  assert(temporada==0); // switch never becomes a season index
  agenda_definir_hoje("2026-10-22");
  agenda_registrar(itemTeste.imdb,"Fixture",NULL,"Returning Series",2,1,"Premiere","2027-01-01","");
  assert(temLembreteTemporada() && secaoN(SEC_TEMPORADAS)==4);
  assert(temporadaLembrete(2));
  int bestT=0,bestE=0;
  grid[0].nota=72; grid[1].nota=81; grid[2].nota=90; grid[3].nota=79;
  cat_definir_episodios(0,grid,4);
  assert(progMelhorNaoVisto(&bestT,&bestE)==90 && bestT==2 && bestE==1);
  vistoep_definir(itemTeste.imdb,2,1,1);
  assert(progMelhorNaoVisto(&bestT,&bestE)==81 && bestT==1 && bestE==2);
  vaiTocar.tem=1; stream_invalidar("fixture target changed");
  assert(!topoFonteDisponivel()); // stale rendered source cannot take focus
  assert(secaoAlpha(SEC_NOTAS)==.42f);
  nivel=1; foco.fileira=SEC_NOTAS; assert(secaoAlpha(SEC_NOTAS)==1.f);
  nivel=0; assert(secaoAlpha(SEC_NOTAS)==1.f);
  itemTeste.temporadas[0]=1; itemTeste.nTemporadas=1; snprintf(itemTeste.tipo,sizeof itemTeste.tipo,"series");
  cat_definir_tudo(&itemTeste,1,NULL,0); idx=0; cat_definir_episodios(0,grid,2);
  temporada=0; nivel=1; foco.fileira=SEC_EPISODIOS; foco.coluna=1;
  assert(!detail_ep_borda(1) && temporada==0); // switch/reminder are never adjacent seasons
  // Footer is one horizontal row, including when a card disappears.
  quoteCount=1; frasesAberta=1; sincronizarColunas(); recalcularLayout();
  nivel=1; foco.fileira=SEC_FRASES; foco.coluna=0;
  assert(topoSec[SEC_FRASES]==topoSec[SEC_DETALHES]);
  assert(parDir(SEC_FRASES)==SEC_DETALHES);
  assert(!moverFileira(1));
  assert(moverFileira(-1) && foco.fileira==SEC_COMENTARIOS);
  foco.fileira=SEC_DETALHES;
  assert(!moverFileira(1));
  assert(moverFileira(-1) && foco.fileira==SEC_COMENTARIOS);
  quoteCount=3; seriefrases_selecionar(1);
  foco.fileira=SEC_FRASES; e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP;
  detail_evento(&e); assert(foco.fileira==SEC_COMENTARIOS);
  quoteCount=0; assert(parEsq(SEC_DETALHES)==-1);
  // Hero enters the current episode; vertical moves keep that episode.
  sincronizarColunas(); nivel=0; botao=1; focoAmigos=0; carro=0; saindo=0;
  foco.colunaLembrada[SEC_EPISODIOS]=1;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_DOWN; detail_evento(&e);
  assert(nivel==1 && foco.fileira==SEC_EPISODIOS && foco.coluna==1);
  assert(moverFileira(1) && foco.fileira==SEC_PROGTEMP && foco.coluna==1);
  assert(moverFileira(-1) && foco.fileira==SEC_EPISODIOS && foco.coluna==1);
  foco.colunaLembrada[SEC_TEMPORADAS]=2;
  assert(moverFileira(-1) && foco.fileira==SEC_TEMPORADAS && foco.coluna==colTemp(temporada));
  char status[256];
  itemTeste.restanteMin=18; itemTeste.progresso=40;
  snprintf(itemTeste.meta,sizeof itemTeste.meta,"2026 · 120 min");
  cat_definir_tudo(&itemTeste,1,NULL,0); idx=0;
  topoEstadoTexto(status,sizeof status,120,1);
  assert(strstr(status,"18"));
  assert(detalheExpo(0)==0 && detalheExpo(1)==1);
  assert(fabsf(detalheExpo(.5f)-.96875f)<.0001f);
  char duration[32]; desc_duracao_min_curta(156,duration,sizeof duration);
  assert(!strcmp(duration,"2h36"));
  assert(ORDEM_SERIE[7]==SEC_COLECAO && ORDEM_SERIE[8]==SEC_TRAILERS);
  diario_iniciar(); notaAbrir(); assert(notaAberta && notaEscolhida==5);
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_RIGHT; detail_evento(&e);
  assert(notaEscolhida==6);
  e.type=SDL_KEYUP; e.key.keysym.sym=SDLK_RETURN; detail_evento(&e);
  assert(!notaAberta && notaMinha()==6);
  notaAbrir(); e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_ESCAPE; detail_evento(&e);
  assert(!notaAberta && notaMinha()==6);
  puts("detail21: content, navigation, profile isolation and personal status OK");
  SDL_Quit();
  return 0;
}
