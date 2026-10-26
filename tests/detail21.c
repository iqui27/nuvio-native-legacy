#define extras_nota ratingFixture
#define seriefrases_n quoteCountFixture
#define seriefrases_n_fatos factCountFixture
#define extras_comentario_spoiler comSpoilerFixture
#define extras_n_comentarios comCountFixture
#define extras_relacionado_imdb relFixture
#define juntos_disponivel togetherAvailableFixture
#define travaui_disponivel lockAvailableFixture
#define juntosui_abrir_folha togetherOpenFixture
#define travaui_abrir lockOpenFixture
#include "../src/detail.c"
static int eligible, roomT, roomE, lockIdx=-1;
int ratingFixture(int f) { return f==EX_IMDB ? 80 : f==EX_TOMATOES ? 920 : 0; }
int togetherAvailableFixture(void) { return eligible; }
int lockAvailableFixture(void) { return eligible; }
void togetherOpenFixture(int i,int t,int e) { (void)i; roomT=t; roomE=e; }
int lockOpenFixture(const CatItem *ci,int i) { (void)ci; lockIdx=i; return 1; }
#undef extras_relacionado_imdb
static int quoteCount;
int quoteCountFixture(void) { return quoteCount; }
int factCountFixture(void) { return 0; }
int comSpoilerFixture(int ep,int i) { (void)ep; return i == 0; }
int comCountFixture(void) { return 3; }
const char *relFixture(int i) { (void)i; return "tt9000011"; }
#include <assert.h>
#include "dados.h"

int main(int argc, char **argv) {
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
  // TV retains legacy Play + Save, with other actions in More.
  assert(nBotoes()==3 && acaoEm(0)==ACAO_PRIMARIO && acaoEm(1)==ACAO_LISTA && acaoEm(2)==ACAO_MAIS);
  int available[DMAIS_N]; maisDisponiveis(available);
  assert(available[DMAIS_VISTO] && !available[DMAIS_VISTO_SAI] && available[DMAIS_NOTA]);
  executarMais(DMAIS_VISTO); assert(detail_pediu_assistido());
  executarMais(DMAIS_NOTA); assert(notaAberta); notaAberta=0;
  servidoresDisponiveis=1;
  assert(nBotoes()==3 && acaoEm(1)==ACAO_LISTA);
  maisDisponiveis(available); assert(available[DMAIS_NOTA]);
  servidoresDisponiveis=0;
  // Existing per-source settings govern both cards and their summary.
  FILE *ratings=fopen(path,"w"); assert(ratings);
  fputs("notaTituloImdb 1\nnotaTituloTomates 0\nmdblist_show_tomatoes 0\n",ratings); fclose(ratings);
  ajustes_dir(dados_dir());
  assert(notaFonte(0)==EX_TOMATOES && notaFonte(1)==-1);
  assert(notaMedia()->n==1 && notaMedia()->media==92);
  ratings=fopen(path,"w"); assert(ratings);
  fputs("notaTituloImdb 0\nnotaTituloTomates 0\n",ratings); fclose(ratings); ajustes_dir(dados_dir());
  if (argc > 1) {
    SDL_Event key = {0}; key.type = SDL_KEYDOWN;
    if (!strcmp(argv[1], "actions")) {
      agenda_definir_hoje("2026-10-22");
      agenda_registrar(itemTeste.imdb,"Fixture",NULL,"Returning Series",2,1,"Premiere","2027-01-01","");
      eligible=1;
      itemTeste.temporada=2; itemTeste.episodio=9; itemTeste.progresso=40;
      cat_definir_tudo(&itemTeste,1,NULL,0);
      int d[DMAIS_N]; maisDisponiveis(d);
      assert(d[DMAIS_JUNTOS] && d[DMAIS_TRAVAR] && d[DMAIS_INICIO]);
      executarMais(DMAIS_JUNTOS); assert(roomT==2 && roomE==9);
      executarMais(DMAIS_TRAVAR); assert(lockIdx==0);
      executarMais(DMAIS_INICIO); assert(pedDoInicio);
      int before=agenda_lembrete(itemTeste.imdb);
      executarMais(DMAIS_LEMBRAR); assert(agenda_lembrete(itemTeste.imdb)!=before);
      int found = 0;
      detmais_abrir(d);
      for (int i=0; i<detmais_n(); i++)
        if (!strcmp(detmais_rotulo(detmais_acao(i)), "Lembrar-me")) found=1;
      assert(found); // landscape must retain the eligible reminder
    } else if (!strcmp(argv[1], "sources")) {
      itemTeste.temporada=2; itemTeste.episodio=9; itemTeste.progresso=40;
      cat_definir_tudo(&itemTeste,1,NULL,0);
      episodios_menu_visto(0,1,3,"Selected episode");
      // This fixture has no watched later episodes: sources is row five.
      key.key.keysym.sym=SDLK_DOWN;
      for (int i=0;i<4;i++) detail_evento(&key);
      key.key.keysym.sym=SDLK_RETURN; detail_evento(&key);
      int t=0, ep=0;
      assert(detail_pediu_fontes());
      assert(detail_pediu_fontes_ep(&t,&ep) && t==1 && ep==3);
    } else if (!strcmp(argv[1], "back")) {
      SDL_Keycode backs[]={SDLK_ESCAPE,SDLK_BACKSPACE,SDLK_AC_BACK,SDLK_DELETE,SDLK_UNKNOWN};
      for (int i=0;i<5;i++) {
        notaAbrir(); key.key.keysym.sym=backs[i];
        key.key.keysym.scancode=i==4 ? NV_SCANCODE_BACK : 0;
        detail_evento(&key); assert(!notaAberta);
        assert(!saindo && notaMinha()==0);
      }
    }
    SDL_Quit(); return 0;
  }
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
  nivel=0; focoVersao=1; pedFontes=0; botao=0;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_DOWN; detail_evento(&e);
  assert(!pedFontes && !focoVersao && nivel==1);
  gfx_escala_txt_definir(.85f);
  float priorScale=gfx_escala_txt_entrar();
  recalcularLayout();
  assert(fabsf(topoSec[SEC_TEMPORADAS]*.85f-(NV_DET21_FIM+56))<.01f);
  gfx_escala_sair(priorScale); gfx_escala_txt_definir(1);
  recalcularLayout();
  assert(NV_DET21_BTN_H==72 && NV_DET21_CIRC==72);
  assert(COL_CARD_W==760 && COL_CARD_H==300 && COM_CARD_H==340);
  assert(secaoAlvo(SEC_NOTAS)==.42f);
  nivel=1; foco.fileira=SEC_NOTAS; assert(secaoAlvo(SEC_NOTAS)==1.f);
  nivel=0; assert(secaoAlvo(SEC_NOTAS)==1.f);
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
  // A browsed episode stays selected; an unopened page instead seeds the default.
  epEscolhido=1;
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
  // Up focuses the nearest global control; it never opens a rating editor.
  nivel=0; botao=0; focoVersao=0; aberto=1; pediuRelogio=0;
  e.type=SDL_KEYDOWN; e.key.keysym.sym=SDLK_UP; detail_evento(&e);
  assert(pediuRelogio && !reacao_aberta() && !notaAberta);
  puts("detail21: content, navigation, profile isolation and personal status OK");
  SDL_Quit();
  return 0;
}
