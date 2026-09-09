#include "streams.h"
#include "catalogo.h"
#include "episodios.h"
#include "player.h"
#include "ajustes.h"
#include "faixas.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include "extras.h"
#include "trakt.h"
#include "addons.h"
#include "rede.h"
#include "detail.h"
#include "home.h"
#include "menu.h"
#include "continuar.h"
#include "legenda.h"
#include "intro.h"
#include "perfil.h"
#include "salvospainel.h"
#include "salvos.h"
#include "social.h"
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern int cat_historico_estado_item(int indice);
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

// O PAR KEYDOWN+KEYUP, e nao so o KEYDOWN. A folha de episodios passou a
// decidir o OK na SUBIDA da tecla, porque e ela que conhece a DURACAO — e a
// duracao e o que separa "abrir o episodio" de "segurar para marcar como
// assistido". Mandar so a descida deixava o toque pela metade, e o teste pegou
// isso na primeira rodada. Um controle de verdade sempre manda os dois.
static void tecla(SDL_Keycode k) {
  SDL_Event e={0};
  e.type=SDL_KEYDOWN;e.key.keysym.sym=k;episodios_evento(&e);
  e.type=SDL_KEYUP;episodios_evento(&e);
}
static void teclaPlayer(SDL_Keycode k) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;player_evento(&e);
}
static void teclaMenu(SDL_Keycode k) {
  SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=k;menu_evento(&e);
}
static void testar(void) {
  perfil_iniciar();perfil_abrir();
  assert(perfil_aberto());
  { PerfilDados pd={0};
    snprintf(pd.nome,sizeof pd.nome,"Perfil de teste");
    snprintf(pd.usuario,sizeof pd.usuario,"teste");
    pd.periodo[0]='J'; pd.periodo[1]=0;
    perfil_definir_dados(&pd);
    perfil_definir_carregando(1);
    perfil_definir_erro("rede indisponível");
    perfil_definir_dados(NULL);
  }
  // Sem snapshot, OK na tela inteira pede nova tentativa. (Este trecho cobria
  // o painel lateral "Sua atividade", removido: os tres botoes dele — Fechar,
  // Atualizar, Ver perfil completo — nao existem mais, porque os dois atalhos
  // que abriam o painel agora vao direto ao destino. O que sobrou de testavel
  // e o pedido de atualizacao e o Voltar.)
  SDL_Event pe={0};pe.type=SDL_KEYDOWN;pe.key.keysym.sym=SDLK_RETURN;
  perfil_evento(&pe);
  assert(perfil_pediu_atualizar());
  pe.key.keysym.sym=SDLK_ESCAPE;perfil_evento(&pe);
  assert(!perfil_aberto() && perfil_quer_sair());

  // PAINEL DE SALVOS, o que a tecla AZUL abre agora. Com a lista vazia ele nao
  // tem linha nenhuma, entao OK nao pode inventar um pedido de abrir — era o
  // caminho mais facil de um indice fora da faixa.
  spainel_abrir();
  assert(spainel_aberto() && spainel_visivel());
  { SDL_Event se={0};se.type=SDL_KEYDOWN;
    se.key.keysym.sym=SDLK_DOWN;spainel_evento(&se);
    se.key.keysym.sym=SDLK_RETURN;spainel_evento(&se);
    assert(salvos_n()>0 || spainel_pediu_abrir()==NULL);
    se.key.keysym.sym=SDLK_LEFT;spainel_evento(&se);
    assert(!spainel_aberto()); }
  char json[24000];size_t p=0;
  LegendaCue *lc=NULL;
  int nc=legenda_extrair("WEBVTT\n\n00:00:01.000 --> 00:00:03.250\n<i>Olá &amp; bem-vindo</i>\n\n2\n00:00:04,000 --> 00:00:06,000\nSegunda linha\n",&lc);
  assert(nc==2&&lc[0].inicio==1.0&&lc[0].fim==3.25&&!strcmp(lc[0].texto,"Olá & bem-vindo"));free(lc);
  IntroTrecho it[3];int ni=intro_extrair("{\"intro\":{\"start_sec\":12,\"end_sec\":44},\"recap\":null,\"outro\":{\"start_sec\":3000,\"end_sec\":3060}}",it,3);
  assert(ni==2&&it[0].tipo==INTRO_ABERTURA&&it[0].fim==44&&it[1].tipo==INTRO_CREDITOS);
  char legurl[256];
  video_normalizar_url_legenda("https://subs.example/file/123",legurl,sizeof legurl);
  assert(!strcmp(legurl,"https://subs.example/file/123.srt"));
  video_normalizar_url_legenda("https://subs.example/file/123?x=1",legurl,sizeof legurl);
  assert(!strcmp(legurl,"https://subs.example/file/123.srt?x=1"));
  video_normalizar_url_legenda("https://subs.example/file/123.VTT?x=1",legurl,sizeof legurl);
  assert(!strcmp(legurl,"https://subs.example/file/123.VTT?x=1"));
  p+=snprintf(json+p,sizeof json-p,"{\"streams\":[");
  for(int i=0;i<40;i++) p+=snprintf(json+p,sizeof json-p,
    "%s{\"name\":\"Fonte %d\",\"url\":\"https://example.invalid/%d\",\"behaviorHints\":{\"filename\":\"Silo.S02E04.%s\"}}",
    i?",":"",i,i,i==37?"2160p.DV.Atmos.mp4":"1080p.DVDRip.mkv");
  snprintf(json+p,sizeof json-p,"]}");
  Stream *v=NULL;int n=stream_extrair(json,"Fixture",&v);
  assert(n==40 && v[37].dolbyVision && v[37].mp4 && v[37].dolbyAtmos && v[37].altura==2160);
  assert(!v[0].dolbyVision);
  stream_definir_lista(v,n);free(v);
  assert(stream_n()==40 && stream_automatico()==37 && stream_item(40)==NULL);
  stream_definir_lista(NULL,0);assert(stream_n()==0 && stream_automatico()==-1);
  n=stream_extrair("{\"streams\":[{\"infoHash\":\"abc\"},{\"externalUrl\":\"https://example.invalid\"},{\"url\":\"https://example.invalid/a.mp4\",\"title\":\"4K [DV] Atmos\"}]}","Fixture",&v);
  // TRES, E NAO UMA. Esta asserção dizia `n==1`, de quando fonte sem URL era
  // lixo a descartar. O suporte a debrid mudou o contrato e a linha ficou para
  // tras: infoHash puro E uma fonte valida — a URL nasce depois, em
  // stream_definir_lista (streams.c:146), e stream_parse.c:40 registra o
  // porque ("como o AIOStreams manda"). externalUrl tambem entra, como URL.
  //
  // O que este caso realmente prova, e por isso ele existe, e que as flags
  // saem do campo "title" quando nao ha "name" nem "description" — e o unico
  // lugar onde muitos addons poem a qualidade.
  assert(n==3);
  assert(!v[0].url[0] && !strcmp(v[0].infoHash,"abc"));      // torrent puro
  assert(!strcmp(v[1].url,"https://example.invalid"));       // externalUrl vira url
  assert(v[2].mp4 && v[2].dolbyVision && v[2].altura==2160); // flags do "title"
  free(v);
  char longa[4500];memset(longa,'a',sizeof longa);memcpy(longa,"https://example.invalid/",24);longa[4090]=0;
  snprintf(json,sizeof json,"{\"streams\":[{\"url\":\"%s\",\"name\":\"DV/HDR 4K MP4\"}]}",longa);
  n=stream_extrair(json,"Fixture",&v);assert(n==1 && strlen(v[0].url)==4090 && v[0].dolbyVision);free(v);
  longa[4090]='a';longa[4400]=0;
  snprintf(json,sizeof json,"{\"streams\":[{\"url\":\"%s\"}]}",longa);
  n=stream_extrair(json,"Fixture",&v);assert(n==0);free(v);
  CatItem c={0};snprintf(c.tipo,sizeof c.tipo,"series");snprintf(c.titulo,sizeof c.titulo,"Silo");
  c.nTemporadas=2;c.temporadas[0]=1;c.temporadas[1]=2;cat_definir(&c,1);
  CatEp eps[40]={0};
  for(int i=0;i<40;i++){eps[i].temporada=i/20+1;eps[i].episodio=i%20+1;snprintf(eps[i].nome,sizeof eps[i].nome,"Episódio %d",i+1);}
  cat_definir_episodios(0,eps,40);assert(cat_n_episodios(0)==40);
  episodios_abrir(0,2,4);episodios_atualizar(.016f);tecla(SDLK_RETURN);
  int t=0,e=0;assert(!episodios_aberto() && !episodios_escolheu(&t,&e));
  episodios_abrir(0,2,4);episodios_atualizar(.016f);tecla(SDLK_DOWN);tecla(SDLK_RETURN);
  assert(episodios_escolheu(&t,&e) && t==2 && e==5);
  assert(!episodios_escolheu(&t,&e));
  player_abrir(0,NULL);player_definir_episodio(2,4);
  // A ABREVIACAO SEGUE O IDIOMA: T de Temporada em portugues, S de Season em
  // ingles. Foi o conserto do issue #12 — a frase e montada com snprintf, e o
  // i18n passou a envolver o FORMATO, entao "T%dE%d" tem par na tabela.
  //
  // Esta linha exigia "T2E4" fixo, e por isso dependia do IDIOMA DA MAQUINA:
  // ajustes.c le o ajustes.txt de verdade, e nesta bancada ele esta em ingles
  // (ingles=1), entao o rotulo saia "S2E4" e o teste morria aqui. Perguntar ao
  // proprio ajustes tira a maquina da conta.
  assert(strstr(player_linha_episodio(), ajustes_idioma_ingles() ? "S2E4" : "T2E4"));
  assert(strstr(player_linha_episodio(),"Episódio 24"));
  assert(player_proximo_episodio()&&player_proximo_episodio()->temporada==2&&player_proximo_episodio()->episodio==5);
  teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RETURN);
  assert(player_pediu_faixas()==2); /* Play, proporção, legendas: sem saltos. */
  assert(player_controles_visiveis());
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  teclaPlayer(SDLK_DOWN);assert(player_controles_visiveis());
  teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RETURN);
  assert(player_pediu_fontes());
  assert(player_carregando());player_encerrar();
  strcpy(c.tipo,"movie");cat_definir(&c,1);player_abrir(0,NULL);
  player_definir_episodio(2,4);assert(!player_linha_episodio()[0]);
  for(int i=0;i<10;i++)teclaPlayer(SDLK_RIGHT);
  teclaPlayer(SDLK_RETURN);assert(player_pediu_fontes() && !episodios_aberto());
  player_encerrar();strcpy(c.tipo,"series");
  menu_iniciar();menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_RETURN);
  assert(menu_destino()==MENU_BUSCAR && menu_mudou_destino() && !menu_mudou_destino());
  menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_ESCAPE);
  assert(!menu_aberto() && menu_destino()==MENU_BUSCAR);
  char pasta[]="/tmp/nuvio-progress-test-XXXXXX",arquivo[256];assert(mkdtemp(pasta));
  snprintf(arquivo,sizeof arquivo,"%s/catalogo.txt",pasta);
  FILE *fp=fopen(arquivo,"w");assert(fp);fclose(fp);
  cat_carregar(pasta);strcpy(c.imdb,"tt0000001");cat_definir(&c,1);
  c.progresso=95;assert(cat_historico_estado_item(0)<0);
  cat_historico_definir_id("tt0000001","series",1);
  assert(cat_historico_estado_item(0)==1);
  cat_historico_definir_id("tt0000001:2:4","series",0);
  assert(cat_historico_estado_item(0)==0);
  cat_salvar_progresso_ep(0,500,1000,2,4);
  c.temporada=1;c.episodio=1;strcpy(c.nomeEpisodio,"Nome antigo");
  cat_definir(&c,1);
  assert(cat_item(0)->temporada==2 && cat_item(0)->episodio==4 && cat_item(0)->progresso==50);
  assert(!cat_item(0)->nomeEpisodio[0]);
  unlink(arquivo);snprintf(arquivo,sizeof arquivo,"%s/progresso.txt",pasta);unlink(arquivo);rmdir(pasta);
  puts("PASS: 40 fontes, DV em filename, DVD negativo, sem fontes fictícias, 40 episódios, foco/seleção e título do episódio.");
}

static void captura(const char *nome,SDL_Window *win,int painel) {
  for(int i=0;i<90;i++) {
    SDL_PumpEvents();txt_novo_quadro();tex_novo_quadro();tex_bombear(6);
    player_atualizar(1.f/60,SDL_GetTicks());episodios_atualizar(1.f/60);
    stream_folha_atualizar(1.f/60,SDL_GetTicks());faixas_atualizar(1.f/60,SDL_GetTicks());
    glClearColor(.025,.025,.03,1);glClear(GL_COLOR_BUFFER_BIT);
    if(painel<4) player_desenhar(SDL_GetTicks());
    if(painel==5) {
      perfil_atualizar(1.f/60,SDL_GetTicks());perfil_desenhar(SDL_GetTicks());
    }
    if(painel==4) {
      CatItem c=*cat_item(0);c.temporada=2;c.episodio=4;c.restanteMin=85;c.progresso=34;
      strcpy(c.nomeEpisodio,"The Harmonium");
      for(int k=0;k<3;k++) {
        GfxRect r={180+k*470.f,360,440,248};
        gfx_cor(r,.045f,.18f+.05f*k,.24f,.30f,1);
        continuar_desenhar(&c,r);
      }
      menu_atualizar(1.f/60,SDL_GetTicks());menu_desenhar(SDL_GetTicks());
    }
    if(painel==1) episodios_desenhar();
    if(painel==2) stream_folha_desenhar(SDL_GetTicks());
    if(painel==3) faixas_desenhar(SDL_GetTicks());
    if(i==89) {
      unsigned char *pix=malloc(1920*1080*4);assert(pix);
      glReadPixels(0,0,1920,1080,GL_RGBA,GL_UNSIGNED_BYTE,pix);
      SDL_Surface *s=SDL_CreateRGBSurfaceWithFormat(0,1920,1080,32,SDL_PIXELFORMAT_RGBA32);assert(s);
      for(int y=0;y<1080;y++)memcpy((char*)s->pixels+y*s->pitch,pix+(1079-y)*1920*4,1920*4);
      assert(SDL_SaveBMP(s,nome)==0);SDL_FreeSurface(s);free(pix);
    }
    SDL_GL_SwapWindow(win);SDL_Delay(8);
  }
}
int main(int argc,char **argv) {
  assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER)==0);
  testar();
  if(argc<2)return 0;
  if(!strcmp(argv[1],"--social")) {
    rede_preparar();assert(trakt_carregar("deploy/app/art"));
    CatItem *social=calloc(8,sizeof *social);assert(social);
    int n=trakt_social(social,8);assert(n>0);
    for(int i=0;i<n;i++)assert(social[i].imdb[0]&&social[i].pais[0]&&social[i].titulo[0]);
    printf("PASS: %d atividades reais, com autoria e titulo.\n",n);free(social);return 0;
  }
  if(!strcmp(argv[1],"--live")) {
    rede_preparar();
    addons_carregar("deploy/app/art");addons_buscar("tt14688458:2:4","series");
    for(int i=0;i<900 && addons_estado()==ADD_BUSCANDO;i++)SDL_Delay(100);
    assert(addons_estado()!=ADD_BUSCANDO);
    int dv=0,mp4dv=0;
    for(int i=0;i<stream_n();i++) {
      const Stream *s=stream_item(i);dv+=!!s->dolbyVision;
      if(s->mp4 && s->dolbyVision) {mp4dv++;printf("MP4/DV na posição %d de %d\n",i+1,stream_n());}
    }
    printf("LIVE Silo T2E4: %d fontes diretas, %d DV, %d MP4/DV (nenhuma URL exposta).\n",stream_n(),dv,mp4dv);
    addons_buscar_legendas("tt14688458:2:4","series");
    for(int i=0;i<300 && addons_n_legendas()==0;i++)SDL_Delay(100);
    assert(addons_n_legendas()>0);
    for(int i=0;i<addons_n_legendas();i++) {
      const Legenda *l=addons_legenda(i);
      assert(l && strstr(l->rotulo,"T2E4"));
    }
    { const Legenda *l=addons_legenda(0);char *s=l?rede_baixar(l->url,20):NULL;
      LegendaCue *v=NULL;int n=s?legenda_extrair(s,&v):0;
      assert(n>10);free(v);free(s);
      printf("LIVE OpenSubtitles: arquivo baixado e interpretado pelo overlay (%d blocos).\n",n); }
    printf("LIVE OpenSubtitles: %d legendas, todas confirmadas para T2E4 (nenhuma URL exposta).\n",
           addons_n_legendas());
    addons_encerrar();
    if(trakt_carregar("deploy/app/art")) {
      extras_pedir("tt14688458",1,0);
      for(int i=0;i<300 && !extras_progresso_pronto();i++)SDL_Delay(100);
      int t=0,e=0;
      printf("LIVE Trakt: histórico=%s, próximo=%s",extras_progresso_pronto()?"recebido":"indisponível",extras_proximo_episodio(&t,&e)?"sim":"não informado");
      if(t&&e)printf(" T%dE%d",t,e);
      puts(" (consulta somente leitura)");
      if(t&&e) {
        CatItem c={0};strcpy(c.imdb,"tt14688458");strcpy(c.tipo,"series");strcpy(c.titulo,"Silo");
        cat_definir(&c,1);
        HomeItem it={0};it.indice=0;it.rect=(GfxRect){0,0,320,180};it.titulo=c.titulo;
        detail_abrir(&it);int dt=0,de=0;
        assert(detail_ep_foco(&dt,&de) && dt==t && de==e);
        puts("PASS: alvo do botão Reproduzir coincide com o próximo episódio real do Trakt.");
      }
    }
    return 0;
  }
  IMG_Init(IMG_INIT_PNG|IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
  SDL_Window *w=SDL_CreateWindow("Nuvio: revisão do player",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,1920,1080,SDL_WINDOW_OPENGL|SDL_WINDOW_SHOWN);assert(w);
  SDL_GLContext gl=SDL_GL_CreateContext(w);assert(gl);SDL_GL_SetSwapInterval(0);
  glViewport(0,0,1920,1080);gfx_tamanho_alvo(1920,1080);assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app",1));tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  if(!strcmp(argv[1],"--profile")) {
    PerfilDados d={0};rede_preparar();
    tex_cache_dir("deploy/app/art/cache");
    assert(trakt_carregar("deploy/app/art"));assert(trakt_perfil(&d));
    perfil_iniciar();perfil_abrir();perfil_definir_dados(&d);
    captura("/tmp/nuvio-profile.bmp",w,5);
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
    perfil_evento(&e);captura("/tmp/nuvio-profile-calendar.bmp",w,5);
    for(int i=0;i<6;i++)perfil_evento(&e);
    captura("/tmp/nuvio-profile-ranks.bmp",w,5);
    // O painel lateral do perfil deixou de existir; a captura equivalente hoje
    // e a do painel de Salvos, que ocupa a mesma moldura.
    spainel_abrir();spainel_atualizar(1.0f,SDL_GetTicks());
    captura("/tmp/nuvio-salvos-painel.bmp",w,5);
    return 0;
  }
  cat_carregar("deploy/app/art");
  CatItem c={0};if(cat_n())c=*cat_item(0);
  for(int i=0;i<cat_n();i++) if(!strcmp(cat_item(i)->titulo,"Silo")){c=*cat_item(i);break;}
  /* O pacote pode nao conter Silo: nao combinar o logo de outra obra. */
  if(strcmp(c.titulo,"Silo"))c.logo[0]=c.backdrop[0]=c.poster[0]=0;
  c.imdb[0]=0;snprintf(c.tipo,sizeof c.tipo,"series");snprintf(c.titulo,sizeof c.titulo,"Silo");
  c.nTemporadas=3;c.temporadas[0]=1;c.temporadas[1]=2;c.temporadas[2]=3;cat_definir(&c,1);
  const char *nomes[]={"The Engineer","Order","Solo","The Harmonium","Descent"};
  CatEp ep[5]={0};
  for(int i=0;i<5;i++) {
    ep[i].temporada=2;ep[i].episodio=i+1;snprintf(ep[i].nome,sizeof ep[i].nome,"%s",nomes[i]);
    snprintf(ep[i].data,sizeof ep[i].data,"5 de dezembro de 2024");snprintf(ep[i].duracao,sizeof ep[i].duracao,"53 min");
    snprintf(ep[i].sinopse,sizeof ep[i].sinopse,"Juliette sets out on a dangerous quest to retrieve a suit so she can return home.");
  }
  cat_definir_episodios(0,ep,5);player_abrir(0,NULL);player_definir_episodio(2,4);
  captura("/tmp/nuvio-player-loading.bmp",w,0);
  episodios_abrir(0,2,4);captura("/tmp/nuvio-player-episodes.bmp",w,1);episodios_fechar();
  Stream s[5]={0};
  for(int i=0;i<5;i++) {
    snprintf(s[i].rotulo,sizeof s[i].rotulo,"Silo S02 E04");snprintf(s[i].provedor,sizeof s[i].provedor,"Addon de teste");
    snprintf(s[i].descricao,sizeof s[i].descricao,"English | Portuguese\nSilo.S02E04.2160p.WEB.DV.Atmos.mp4");
    s[i].mp4=s[i].dolbyVision=s[i].dolbyAtmos=1;s[i].altura=2160;s[i].tamanhoMB=11264;
  }
  stream_definir_lista(s,5);stream_folha_contexto(player_linha_episodio());stream_folha_abrir();
  captura("/tmp/nuvio-player-sources.bmp",w,2);
  faixas_abrir_em(1);captura("/tmp/nuvio-player-subtitles.bmp",w,3);
  menu_iniciar();captura("/tmp/nuvio-player-cards.bmp",w,4);
  menu_abrir();captura("/tmp/nuvio-player-menu.bmp",w,4);
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();
  puts("PASS: seis capturas de revisão em /tmp/nuvio-player-*.bmp (dados de teste, sem reprodução real).");return 0;
}
