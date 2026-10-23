#include "streams.h"
#include "nvtmp.h"
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
#include "traktscrobble.h"
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
  e.type=SDL_KEYUP;menu_evento(&e);
}
// A decisao de "visto" de fecharSessao: o fim pela regra do cartao E a prova
// de reproducao real. As sessoes inteiras (pipeline falso, destinos contados)
// estao em tests/player_visto_trailer.c; este binario liga o video.c de verdade.
static int saidaVisto(double pos, double dur, double cred, double assistido, double de) {
  int chegou;
  return player_regra_visto(de, assistido, pos, player_regra_concluiu(pos, dur, cred), &chegou);
}
// Fonte simulada pronta: com a fonte ainda abrindo, BAIXO expande o cartao
// (8bf0ae88) em vez de esconder os controles, que e o que estes casos testam.
static void comFonte(void) {
  static const VideoSimulacao sim={.aceita=1,.pronto=1};
  video_simular(&sim);player_definir_fonte("https://example.invalid/e.mp4");
}
// Serie com fonte pronta (cca9acf95): Play, Proximo, Recarregar, Legendas.
static void paraLegendas(void) {
  teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);
}
static void testar(void) {
  // #205 NAO PODE REGREDIR O #100. Aqui em cima porque nao dependem de estado.
  assert(saidaVisto(2400, 2700, 2400, 2400, 0));    // aceitou o proximo nos creditos (88,9%)
  assert(saidaVisto(1260, 1320, 0, 1170, 0));       // pulou 90 s de abertura, saiu a 60 s do fim
  assert(saidaVisto(2526, 2580, 0, 2526, 0));       // 1,5x: conta a midia tocada, nao o relogio
  assert(saidaVisto(5950, 6000, 0, 2350, 3600));    // retomou em 60% e foi ate o fim
  assert(!saidaVisto(2824, 2843, 0, 133, 0));       // tocou 2 min e buscou o fim: nao viu
  assert(!saidaVisto(69, 129, 0, 69, 0));           // duracao falsa de trailer, sem catalogo
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
  // FORMATO DO TheIntroDB, que substituiu o api.introdb.app: chave com ARRAY,
  // tempos em MILISSEGUNDOS e "credits" no lugar de "outro". A carga antiga
  // ("intro":{"start_sec":12,...}) nao existe mais em resposta nenhuma. A
  // cobertura fina do leitor esta em tests/intro.sh; aqui fica so o fio que
  // prova que o player ainda enxerga os dois tipos.
  IntroTrecho it[3];int ni=intro_extrair("{\"intro\":[{\"start_ms\":12000,\"end_ms\":44000}],"
                                         "\"credits\":[{\"start_ms\":3000000,\"end_ms\":3060000}]}",it,3);
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
  // infoHash e midia resolvivel; externalUrl sozinho abre fora e fica oculto.
  // As flags ainda saem do title sem name/description.
  assert(n==2);
  assert(!v[0].url[0] && !strcmp(v[0].infoHash,"abc"));      // torrent puro
  assert(v[1].mp4 && v[1].dolbyVision && v[1].altura==2160); // flags do "title"
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
  assert(player_carregando());comFonte();assert(!player_carregando());
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
  paraLegendas();teclaPlayer(SDLK_RETURN);
  assert(player_pediu_faixas()==2); /* Play, legendas (ordem do Glass UI): sem saltos. */
  assert(player_controles_visiveis());
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  teclaPlayer(SDLK_DOWN);assert(player_controles_visiveis());
  // #305: CIMA na barra (topo, sem trecho pulavel) esconde; a tecla seguinte revela.
  teclaPlayer(SDLK_UP);assert(player_controles_visiveis());   // botoes -> barra
  teclaPlayer(SDLK_UP);assert(!player_controles_visiveis());  // barra -> esconde
  teclaPlayer(SDLK_UP);assert(player_controles_visiveis());
  teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RIGHT);teclaPlayer(SDLK_RETURN);
  assert(player_pediu_fontes());   /* legendas -> audio -> proporcao -> fontes */
  player_encerrar();
  // #109: a tecla que revela os controles decide o foco. Com o foco em
  // Legendas e os controles escondidos, OK pausa e os controles sobem NO PLAY
  // — o OK seguinte alterna de novo em vez de abrir a folha de legendas.
  // Sessao sem URL tambem nao tem video proprio (registro 1518).
  player_abrir(0,NULL);assert(!player_tem_video());player_definir_episodio(2,4);comFonte();
  paraLegendas();      // foco em Legendas
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  teclaPlayer(SDLK_RETURN);assert(player_controles_visiveis());
  teclaPlayer(SDLK_RETURN);assert(player_pediu_faixas()==0);
  // A tecla fisica Play/Pause (SDLK_PAUSE, traduzida pela casca da Samsung)
  // com os controles EM PE e o foco em Legendas: alterna, nao abre a folha.
  paraLegendas();
  teclaPlayer(SDLK_PAUSE);teclaPlayer(SDLK_RETURN);assert(player_pediu_faixas()==0);
  player_encerrar();
  // #122: o texto que o AVPlay entrega no onsubtitlechange vira texto puro
  // para o overlay (SRT com tags HTML, ASS com {\tags} e \N).
  { char t[256];
    snprintf(t,sizeof t,"<i>Olá</i><br/><font color=\"#ff0\">mundo</font>\n");
    player_limpar_legenda_nativa(t);assert(!strcmp(t,"Olá\nmundo"));
    snprintf(t,sizeof t,"{\\an8}{\\i1}Letreiro\\Nsegunda\\hlinha{\\i0}");
    player_limpar_legenda_nativa(t);assert(!strcmp(t,"Letreiro\nsegunda linha"));
    // Como o AVPlay do Tizen 10 entregou de verdade (emulador, MKV com SRT):
    // "Primeira fala\r\u0000" — o NUL corta a string, o \r sobra.
    snprintf(t,sizeof t,"Primeira fala\r");
    player_limpar_legenda_nativa(t);assert(!strcmp(t,"Primeira fala"));
    snprintf(t,sizeof t,"Tom &amp; Jerry &lt;3");
    player_limpar_legenda_nativa(t);assert(!strcmp(t,"Tom & Jerry <3"));
    // Fora da Samsung nao ha legenda embutida para o app desenhar.
    assert(!player_texto_legenda_nativa(t,sizeof t)&&!t[0]); }
  // #121: com os controles ESCONDIDOS, esquerda/direita e busca, como no
  // YouTube e na Netflix — os controles sobem com o foco NA BARRA e o primeiro
  // toque ja anda 10 s. Antes subiam com o foco nos botoes e o toque seguinte
  // trocava de botao em vez de avancar.
  player_abrir(0,NULL);player_definir_episodio(2,4);comFonte();
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  { float p0=player_posicao_seg();
    teclaPlayer(SDLK_RIGHT);
    assert(player_controles_visiveis() && player_foco_na_barra());
    assert(player_posicao_seg()>p0+5.0f);
    // #128: a busca que comeca escondida sobe SO a barra e o tempo, e segue
    // assim enquanto a pessoa so busca.
    assert(player_so_barra());
    teclaPlayer(SDLK_RIGHT);assert(player_foco_na_barra()&&player_posicao_seg()>p0+15.0f);
    teclaPlayer(SDLK_LEFT);assert(player_foco_na_barra()&&player_posicao_seg()<p0+25.0f);
    assert(player_so_barra()); }
  // BAIXO leva o foco a fileira; la esquerda/direita voltam a trocar de botao.
  // E os controles voltam INTEIROS (#128).
  teclaPlayer(SDLK_DOWN);assert(!player_foco_na_barra()&&player_controles_visiveis());
  assert(!player_so_barra());
  teclaPlayer(SDLK_RETURN);assert(player_controles_visiveis());   // OK no Play: alterna
  { float p1=player_posicao_seg();
    paraLegendas();
    assert(!player_foco_na_barra()&&player_posicao_seg()==p1);
    teclaPlayer(SDLK_RETURN);assert(player_pediu_faixas()==2); }   // Legendas
  // Escondido de novo por BAIXO e revelado por BAIXO: fileira, nao barra.
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  teclaPlayer(SDLK_DOWN);assert(player_controles_visiveis()&&!player_foco_na_barra());
  assert(!player_so_barra());
  // #128: busca escondida e depois OK — o OK confirma a busca e traz tudo.
  teclaPlayer(SDLK_DOWN);assert(!player_controles_visiveis());
  teclaPlayer(SDLK_LEFT);assert(player_so_barra());
  teclaPlayer(SDLK_RETURN);assert(player_controles_visiveis()&&!player_so_barra());
  player_encerrar();
  // FOCO NA BARRA (pedido do dono, 03/10): a fileira de botoes FECHA e volta
  // quando o foco desce; e CIMA na barra nao faz NADA — abria o Audio.
  player_abrir(0,NULL);player_definir_episodio(2,4);comFonte();
  assert(player_controles_visiveis()&&!player_foco_na_barra());
  { Uint32 t0=SDL_GetTicks();int i;
    for(i=0;i<60;i++)player_atualizar(1.f/60,t0);
    assert(player_fileira()>0.95f);
    teclaPlayer(SDLK_UP);assert(player_foco_na_barra());
    for(i=0;i<60;i++)player_atualizar(1.f/60,t0);
    assert(player_fileira()<0.05f);
    teclaPlayer(SDLK_UP);   // #305: CIMA na barra esconde, nao abre o Audio
    assert(player_pediu_faixas()==0&&!player_controles_visiveis());
    teclaPlayer(SDLK_UP);teclaPlayer(SDLK_UP);
    assert(player_pediu_faixas()==0&&player_foco_na_barra());
    teclaPlayer(SDLK_DOWN);assert(!player_foco_na_barra()&&player_controles_visiveis());
    for(i=0;i<60;i++)player_atualizar(1.f/60,t0);
    assert(player_fileira()>0.95f); }
  player_encerrar();video_simular(NULL);
  strcpy(c.tipo,"movie");cat_definir(&c,1);player_abrir(0,NULL);
  player_definir_episodio(2,4);assert(!player_linha_episodio()[0]);
  for(int i=0;i<10;i++)teclaPlayer(SDLK_RIGHT);
  teclaPlayer(SDLK_RETURN);assert(player_pediu_fontes() && !episodios_aberto());
  player_encerrar();strcpy(c.tipo,"series");

  // --- ISSUE #101: A LISTA DE FONTES E DE UM EPISODIO --------------------------
  //
  // O relato: terminar o T2E4, descer no carrossel para o T2E5, e o player
  // abrir a fonte do T2E4 — e a folha "Fontes" dentro do player listar as do
  // T2E4. A lista de streams.c e UMA, global, e ate aqui era ANONIMA: ninguem
  // conseguia perguntar de que episodio ela era, entao bastava um caminho nao
  // refazer a busca para o episodio seguinte tocar a fonte do anterior sem
  // nenhum erro no log.
  //
  // As tres assercoes abaixo falham na 1.3.12: a lista sobrevivia intacta a
  // troca de episodio.
  strcpy(c.imdb,"tt0000101");cat_definir(&c,1);cat_definir_episodios(0,eps,40);
  { Stream fs;memset(&fs,0,sizeof fs);
    snprintf(fs.rotulo,sizeof fs.rotulo,"Fonte do T2E4");
    snprintf(fs.url,sizeof fs.url,"https://example.invalid/s02e04.mp4");
    fs.mp4=1;fs.altura=1080;
    player_abrir(0,NULL);player_definir_episodio(2,4);
    stream_definir_alvo("tt0000101:2:4");stream_definir_lista(&fs,1);
    assert(stream_n()==1 && stream_lista_do_alvo("tt0000101:2:4"));
    // RE-CHAMADA TARDIA com o MESMO episodio nao pode descartar nada:
    // player_atualizar chama esta funcao por quadro enquanto o nome do
    // episodio nao chega, e descartar ali custaria uma busca por quadro.
    player_definir_episodio(2,4);
    assert(stream_n()==1);
    // A pessoa desceu para o E5: a lista do E4 nao serve mais.
    player_definir_episodio(2,5);
    assert(stream_n()==0 && !stream_lista_do_alvo("tt0000101:2:4"));
    player_encerrar(); }
  c.imdb[0]=0;cat_definir(&c,1);cat_definir_episodios(0,eps,40);

  // --- ISSUE #102: A FOLHA ABRE NO EPISODIO ATUAL, E FICA NELE -----------------
  //
  // Duas metades do mesmo relato. A primeira: a folha nascia com scroll=0 e a
  // mola levava ate o episodio — "carrega no E1 e depois salta". A segunda: o
  // catalogo republicado (cat_definir_tudo zera nEps de proposito) esvazia a
  // lista por alguns quadros, e o clamp de foco punha a folha no primeiro
  // episodio da temporada quando ela voltava. E o "volta sozinho para o E1 se
  // a pessoa demorar" — quem segura a pessoa parada e o menu por cima.
  //
  // T2E12 e a 12a linha da temporada 2 (eps[20..39] = T2E1..T2E20), fora da
  // area visivel: e preciso rolar, e por isso a rolagem serve de prova.
  episodios_abrir(0,2,12);episodios_atualizar(.016f);
  assert(episodios_foco_linha()==11);
  { float r=episodios_rolagem();
    assert(r>1200.f);                 // ja rolada no PRIMEIRO quadro, sem mola
    episodios_atualizar(.016f);
    assert(episodios_rolagem()==r); } // e parada: nao ha salto nenhum a ver
  cat_definir(&c,1);                  // a descoberta republicou o catalogo
  episodios_atualizar(.016f);         // a folha fica sem lista por uns quadros
  assert(cat_n_episodios(0)==0);
  cat_definir_episodios(0,eps,40);    // e ela volta
  episodios_atualizar(.016f);
  assert(episodios_foco_linha()==11);
  assert(episodios_rolagem()>1200.f);
  episodios_fechar();

  // --- ISSUE #100: UM SO LIMIAR DE "ASSISTIDO" ---------------------------------
  //
  // O app tinha dois numeros para o mesmo acontecimento. Estes casos sao a
  // distancia entre eles:
  //
  //   18 min (1080 s), sem marcador de creditos. O cartao de "proximo
  //   episodio" sobe aos 960 s (regra dos 2 minutos) = 88,9% do episodio. A
  //   regra VELHA de player_encerrar so arredondava para o fim dentro dos
  //   ultimos 60 s, e sem esse arredondamento trakt.c manda /scrobble/pause,
  //   que nao marca nada. Aceitar o proximo episodio que o proprio app ofereceu
  //   deixava o episodio por marcar — o relato do #100.
  //   2.0.3: sem marcador o cartao sobe a 40 s do fim (tempo fixo), aos 1040 s.
  assert(!player_regra_proximo(960,1080,0));
  assert(player_regra_proximo(1040,1080,0));  // o app diz: acabou, toma o proximo
  assert(player_regra_concluiu(1040,1080,0)); // e as duas dizem o mesmo
  //   Marcador de creditos aceito manda sozinho, aqui como no cartao: 50 min
  //   com creditos aos 45 (sobram 300 s = a janela de um episodio de 50 min).
  assert(player_regra_concluiu(2700,3000,2700));
  assert(!player_regra_concluiu(2600,3000,2700));
  //   E o que NAO acabou continua nao acabando: metade do episodio nao marca
  //   nada, com ou sem marcador.
  assert(!player_regra_concluiu(540,1080,0) && !player_regra_proximo(540,1080,0));
  /* R8: the old 10% window refused credits at 1346s of 1500s (154s left). */
  assert(player_regra_proximo(1346,1500,1346) && !player_regra_proximo(1300,1500,1346));
  assert(!player_regra_proximo(1200,1500,1100));   /* 400s left: still refused */
  assert(!player_regra_concluiu(0,0,0));      // sem duracao nao ha o que concluir

  // Ordem da rail: Inicio, Explorar, Guia TV, Busca. Um DOWN para em Explorar,
  // o Guia fica a dois e Buscar a tres. A ordem e a assercao dos itens novos.
  menu_iniciar();menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_RETURN);
  assert(menu_destino()==MENU_EXPLORAR && menu_mudou_destino() && !menu_mudou_destino());
  menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_RETURN);
  assert(menu_destino()==MENU_GUIA && menu_mudou_destino());
  menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_RETURN);
  assert(menu_destino()==MENU_BUSCAR && menu_mudou_destino());
  menu_abrir();teclaMenu(SDLK_DOWN);teclaMenu(SDLK_ESCAPE);
  assert(!menu_aberto() && menu_destino()==MENU_BUSCAR);
  char pasta[NVT_MAX],arquivo[256]; nvt_set(pasta, sizeof pasta, "/nuvio-progress-test-XXXXXX");assert(mkdtemp(pasta));
  snprintf(arquivo,sizeof arquivo,"%s/catalogo.txt",pasta);
  FILE *fp=fopen(arquivo,"w");assert(fp);fclose(fp);
  cat_carregar(pasta);strcpy(c.imdb,"tt0000001");cat_definir(&c,1);
  c.progresso=95;assert(cat_historico_estado_item(0)<0);
  cat_historico_definir_id("tt0000001","series",1);
  assert(cat_historico_estado_item(0)==1);
  cat_historico_definir_id("tt0000001:2:4","series",0);
  assert(cat_historico_estado_item(0)==0);
  cat_salvar_progresso_ep(0,500,1000,2,4, -1);
  c.temporada=1;c.episodio=1;strcpy(c.nomeEpisodio,"Nome antigo");
  cat_definir(&c,1);
  assert(cat_item(0)->temporada==2 && cat_item(0)->episodio==4 && cat_item(0)->progresso==50);
  assert(!cat_item(0)->nomeEpisodio[0]);
  unlink(arquivo);snprintf(arquivo,sizeof arquivo,"%s/progresso.txt",pasta);unlink(arquivo);rmdir(pasta);

  // CANAL AO VIVO: os scancodes do controle da LG (SDL_webOS.h) viram pedidos
  // — CH+/- = zap na ordem do guia, BAIXO e AZUL = overlay. Num FILME as
  // mesmas teclas nao podem disparar nada disso.
  { CatItem tv={0};
    snprintf(tv.tipo,sizeof tv.tipo,"channel");
    snprintf(tv.titulo,sizeof tv.titulo,"Canal Teste");
    snprintf(tv.imdb,sizeof tv.imdb,"cs:channel:teste");
    cat_definir(&tv,1);
    player_abrir(0,NULL);
    assert(player_aberto());
    // ZAPPING COM DEBOUNCE (aovivo.h): o toque NAO troca na hora — soma, e o
    // pedido sai 600 ms depois do ultimo toque com o deslocamento total.
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=480; /* CH_UP */
      player_evento(&e);assert(player_pediu_zap()==0);
      player_atualizar(0.016f,SDL_GetTicks()+100);assert(player_pediu_zap()==0);
      player_atualizar(0.016f,SDL_GetTicks()+900);
      assert(player_pediu_zap()==1 && !player_pediu_zap()); }
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=481; /* CH_DOWN */
      player_evento(&e);player_evento(&e);
      player_atualizar(0.016f,SDL_GetTicks()+900);assert(player_pediu_zap()==-2); }
    // CH+ e CH- que se cancelam nao trocam nada.
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=480;
      player_evento(&e);e.key.keysym.scancode=481;player_evento(&e);
      player_atualizar(0.016f,SDL_GetTicks()+900);assert(player_pediu_zap()==0); }
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
      player_evento(&e);assert(player_pediu_guia()==1 && !player_pediu_guia()); }
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=489; /* BLUE */
      player_evento(&e);assert(player_pediu_guia()==1); }
    // REPUBLICACAO EM PLENA SESSAO: a descoberta troca o vetor de itens por
    // fileira (cat_definir), e o indice do player passa a apontar para outro
    // titulo. A sessao de canal NAO pode morrer junto — Baixo continua
    // abrindo o overlay e player_id_canal devolve o id congelado.
    { CatItem filme={0};
      snprintf(filme.tipo,sizeof filme.tipo,"movie");
      snprintf(filme.titulo,sizeof filme.titulo,"Filme Que Entrou No Lugar");
      snprintf(filme.imdb,sizeof filme.imdb,"tt9999999");
      cat_definir(&filme,1); }
    assert(!strcmp(player_id_canal(),"cs:channel:teste"));
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
      player_evento(&e);assert(player_pediu_guia()==1); }
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=480;
      player_evento(&e);player_atualizar(0.016f,SDL_GetTicks()+900);assert(player_pediu_zap()==1); }
    // player_marcar_canal: quem abriu pelo guia marca a sessao mesmo que o
    // indice ja tenha vindo errado de uma republicacao anterior.
    player_encerrar();
    { CatItem filme={0}, marcado={0};
      snprintf(filme.tipo,sizeof filme.tipo,"movie");
      snprintf(filme.imdb,sizeof filme.imdb,"tt8888888");
      cat_definir(&filme,1);
      player_abrir(0,NULL);
      assert(!player_id_canal()[0]);
      snprintf(marcado.imdb,sizeof marcado.imdb,"cs:channel:marcado");
      snprintf(marcado.titulo,sizeof marcado.titulo,"Canal Marcado");
      snprintf(marcado.tipo,sizeof marcado.tipo,"channel");
      player_marcar_canal(&marcado);
      assert(!strcmp(player_id_canal(),"cs:channel:marcado"));
      { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
        player_evento(&e);assert(player_pediu_guia()==1); }
    }
    // E fora de canal, CH+/- nao e zap: segue o caminho normal do player.
    player_encerrar();
    strcpy(tv.tipo,"movie");cat_definir(&tv,1);player_abrir(0,NULL);
    { SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.scancode=480;
      player_evento(&e);assert(!player_pediu_zap()); }
    player_encerrar();
    strcpy(tv.tipo,"series");cat_definir(&tv,1);
  }
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
        continuar_desenhar(&c,r,.045f);
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
    captura(nvt("/nuvio-profile.bmp"),w,5);
    SDL_Event e={0};e.type=SDL_KEYDOWN;e.key.keysym.sym=SDLK_DOWN;
    perfil_evento(&e);captura(nvt("/nuvio-profile-calendar.bmp"),w,5);
    for(int i=0;i<6;i++)perfil_evento(&e);
    captura(nvt("/nuvio-profile-ranks.bmp"),w,5);
    // O painel lateral do perfil deixou de existir; a captura equivalente hoje
    // e a do painel de Salvos, que ocupa a mesma moldura.
    spainel_abrir();spainel_atualizar(1.0f,SDL_GetTicks());
    captura(nvt("/nuvio-salvos-painel.bmp"),w,5);
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
  captura(nvt("/nuvio-player-loading.bmp"),w,0);
  // #128: busca comecada com os controles escondidos — so barra e tempo. O
  // par de capturas mostra o antes (so a barra) e o depois do BAIXO (tudo).
  { SDL_Event e={0};e.type=SDL_KEYDOWN;
    e.key.keysym.sym=SDLK_DOWN;player_evento(&e);captura(nvt("/nuvio-player-escondido.bmp"),w,0);
    e.key.keysym.sym=SDLK_RIGHT;player_evento(&e);assert(player_so_barra());
    captura(nvt("/nuvio-player-busca.bmp"),w,0);
    e.key.keysym.sym=SDLK_DOWN;player_evento(&e);assert(!player_so_barra());
    captura(nvt("/nuvio-player-busca-cheio.bmp"),w,0); }
  episodios_abrir(0,2,4);captura(nvt("/nuvio-player-episodes.bmp"),w,1);episodios_fechar();
  Stream s[5]={0};
  for(int i=0;i<5;i++) {
    snprintf(s[i].rotulo,sizeof s[i].rotulo,"Silo S02 E04");snprintf(s[i].provedor,sizeof s[i].provedor,"Addon de teste");
    snprintf(s[i].descricao,sizeof s[i].descricao,"English | Portuguese\nSilo.S02E04.2160p.WEB.DV.Atmos.mp4");
    s[i].mp4=s[i].dolbyVision=s[i].dolbyAtmos=1;s[i].altura=2160;s[i].tamanhoMB=11264;
  }
  stream_definir_lista(s,5);stream_folha_contexto(player_linha_episodio());stream_folha_abrir();
  captura(nvt("/nuvio-player-sources.bmp"),w,2);
  faixas_abrir_em(1);captura(nvt("/nuvio-player-subtitles.bmp"),w,3);
  menu_iniciar();captura(nvt("/nuvio-player-cards.bmp"),w,4);
  menu_abrir();captura(nvt("/nuvio-player-menu.bmp"),w,4);
  tex_encerrar();txt_encerrar();gfx_encerrar();SDL_GL_DeleteContext(gl);SDL_DestroyWindow(w);SDL_Quit();
  printf("PASS: seis capturas de revisão em %s/nuvio-player-*.bmp (dados de teste, sem reprodução real).\n", nvt_dir());return 0;
}
