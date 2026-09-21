// CAPTURAS DA AGENDA, sem interacao e sem rede.
//
// Existe pelo motivo que tests/ajustes_shot.c e tests/social_shot.c ja
// registram: interface de TV julgada so por codigo sai ilegivel a 3 m. Duas
// armadilhas ja pegas neste repositorio SO aparecem olhando: o raio do
// gfx_cor e FRACAO DA ALTURA (passar pixel desenha capsula), e texto claro
// sobre superficie clara some.
//
// O que cada foto prova:
//   -vazia          o estado de quem nao acompanha nada — a tela tem de
//                   explicar o que fazer, nao ficar preta;
//   -hoje           a LINHA DO TEMPO no topo: a faixa de origem "HOJE", o no com
//                   aura da estreia de hoje, os numerais alinhados contra o eixo
//                   e a sinopse que a linha focada abre;
//   -curta          a MESMA coluna da direita com uma sinopse de UMA linha: e o
//                   caso que fazia a laje antiga ficar com o terco de baixo
//                   vazio, e agora a linha tem a mesma altura das outras;
//   -mes            a faixa de mes quando o calendario vira, a espera longa
//                   ("em N semanas") sem a data por extenso engolindo a coluna,
//                   e a linha focada SEM sinopse — a coluna da direita fica
//                   vazia de proposito, em vez de receber enchimento;
//   -mesvazio       hoje em AGOSTO com a primeira estreia em SETEMBRO. Prova o
//                   defeito que o dono fotografou: saiam DOIS cabecalhos de mes
//                   e o de cima nao tinha uma linha embaixo. Tem de sair UM.
//   -semdata        o separador, o eixo TRACEJADO e as series encerradas /
//                   canceladas com a SITUACAO no lugar do numeral;
//   -reduzido       a mesma tela com "reduzir animacoes": o despertador nao
//                   treme e as ondas ficam paradas e opacas — o estado do
//                   lembrete NAO pode depender de animar;
//   -aviso          o cartao do lembrete vencido, que e o unico aviso que uma
//                   TV sem push consegue dar;
//   -menu           a barra lateral aberta com o item Agenda e o icone novo;
//   -lembrete-*     os quatro estados do botao circular do lembrete no hero:
//                   desligado/ligado, em repouso/em foco. O ligado em repouso e
//                   o circulo ESMERALDA; o focado e a superficie clara com o
//                   relogio escuro, e a linha acima dele vira a legenda do
//                   botao — que e o unico nome que um circular mudo tem.
//
// OS DADOS ENTRAM PELO DISCO (agenda-p1.txt e lembretes-p1.txt dentro de
// NUVIO_DADOS), como em social_shot.c: assim a foto prova tambem o FORMATO do
// arquivo — se ele mudar e a leitura nao acompanhar, a tela sai vazia.
#include "agenda.h"
#include "agendaui.h"
#include "noticias.h"
#include "layout.h"
#include "agendaviso.h"
#include "menu.h"
#include "detail.h"
#include "home.h"
#include "catalogo.h"
#include "dados.h"
#include "ajustes.h"
#include "perfis.h"
#include "gfx.h"
#include "text.h"
#include "tex_cache.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { DES_AGENDA = 0, DES_AVISO, DES_MENU, DES_DETALHE };
// Ajustes gravados em disco e lidos por ajustes_dir: e o caminho publico para
// escolher idioma e "reduzir animacoes" sem setter de teste. As chaves sao as
// mesmas de ajustes.txt no aparelho.
// IDIOMA POR VARIAVEL DE AMBIENTE, com o padrao em portugues.
//
// As capturas deste harness servem a DOIS publicos: a conferencia do trabalho,
// que e feita em portugues como o resto do repositorio, e o album do post em
// ingles. Recompilar para trocar a lingua e o tipo de atrito que faz alguem
// publicar a captura errada — NUVIO_SHOT_EN=1 resolve sem tocar no codigo.
static void ajustesDeTeste(int idiomaIngles, int animReduzidas) {
  char caminho[600];
  FILE *f;
  { const char *en = getenv("NUVIO_SHOT_EN");
    if (en && *en && *en != '0') idiomaIngles = 1; }
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w");
  if (!f) return;
  fprintf(f, "idioma %d\nanimacoes %d\n", idiomaIngles, animReduzidas);
  fclose(f);
  ajustes_dir(dados_dir());
}
static int oQue;

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  if (oQue == DES_MENU) menu_evento(&e);
  else { agendaui_evento(&e); e.type = SDL_KEYUP; agendaui_evento(&e); }
}
// OK SEGURADO na Agenda: KEYDOWN, espera passar NV_HOLD_MS, KEYUP — e no
// KEYUP que agendaui decide entre lembrete (toque) e menu de contexto.
static void segurarOk(void) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
  agendaui_evento(&e);
  SDL_Delay(NV_HOLD_MS + 100);
  e.type = SDL_KEYUP;
  agendaui_evento(&e);
}

static void teclaDet(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  detail_evento(&e);
  e.type = SDL_KEYUP;
  detail_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  for (i = 0; i < 90; i++) {
    SDL_PumpEvents();
    txt_novo_quadro();
    tex_novo_quadro();
    // ORCAMENTO LARGO de decodificacao por quadro. Com 6 (o valor do aparelho,
    // onde a arte chega ao longo de varios segundos) as capturas do hero saiam
    // com o fundo vazio ou nao, conforme a sorte da corrida — e uma foto que
    // muda de uma rodada para a outra nao serve para julgar nada.
    tex_bombear(32);
    gfx_novo_quadro();
    switch (oQue) {
      case DES_AVISO:   agendaviso_atualizar(1.0f / 60.0f, SDL_GetTicks()); break;
      case DES_MENU:    menu_atualizar(1.0f / 60.0f, SDL_GetTicks());       break;
      case DES_DETALHE: detail_atualizar(1.0f / 60.0f, SDL_GetTicks());     break;
      default:          agendaui_atualizar(1.0f / 60.0f, SDL_GetTicks());   break;
    }
    glClearColor(0.051f, 0.051f, 0.051f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    switch (oQue) {
      case DES_AVISO:   agendaviso_desenhar(SDL_GetTicks()); break;
      case DES_MENU:    menu_desenhar(SDL_GetTicks());       break;
      case DES_DETALHE: detail_desenhar(SDL_GetTicks());     break;
      default:          agendaui_desenhar(SDL_GetTicks());   break;
    }
    if (i == 89) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      free(pix);
    }
    SDL_GL_SwapWindow(win);
  }
  printf("captura: %s\n", nome);
  // CUSTO DE TEXTURA DA TELA, por captura. A Agenda desenha um cartaz por linha
  // e o teto de decodificacao sai da LARGURA de desenho (tex_obter_larg), entao
  // mexer no tamanho do cartaz mexe na memoria da tela — e sem numero a
  // discussao vira gosto. `quentes` e o que foi desenhado neste quadro ou no
  // anterior, que e exatamente o conjunto que a tela precisa.
  { int it, pend, q; long b, bq;
    tex_estatisticas(&it, &pend, &b, &q, &bq);
    printf("[tex] %s: quentes=%d %ld KB (cache %d itens, %ld KB)\n",
           nome, q, bq / 1024, it, b / 1024); }
}

// Uma linha de agenda-p1.txt, no formato que agenda.c le:
// imdb TAB titulo TAB poster TAB situacao TAB temp TAB ep TAB nomeEp TAB
// dataProx TAB dataUlt TAB visto TAB sinopse TAB tipoEp TAB rede TAB duracao
// TAB temporadas
//
// Os cinco ultimos entraram com a linha do tempo. Escrever por AQUI, e nao por
// agenda_registrar_extra, e o que faz a foto provar tambem o FORMATO do
// arquivo: se a gravacao e a leitura discordarem, a tela sai sem sinopse e a
// captura mostra.
static void poeLinha(char *dst, size_t tam, const char *imdb, const char *titulo,
                     const char *poster, int sit, int t, int e,
                     const char *nomeEp, const char *prox, const char *ult,
                     const char *sinopse, const char *tipoEp, const char *rede,
                     int duracao, int temporadas) {
  size_t n = strlen(dst);
  snprintf(dst + n, tam - n,
           "%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%lld\t%s\t%s\t%s\t%d\t%d\n",
           imdb, titulo, poster, sit, t, e, nomeEp, prox, ult, 4000000000LL,
           sinopse, tipoEp, rede, duracao, temporadas);
}

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : "/tmp/nuvio-agenda";
  char nome[600];
  char cache[20000] = "";
  SDL_Window *w;
  SDL_GLContext gl;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao da Agenda", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(160);
  gfx_icones_dir("deploy/app/art");

  dados_iniciar(".");
  { const char *esperado = getenv("NUVIO_DADOS");
    if (!esperado || !esperado[0] || strcmp(dados_dir(), esperado)) {
      printf("FALHA: dados_dir() e [%s], esperado [%s]. Rode por tests/agenda_shot.sh.\n",
             dados_dir(), esperado ? esperado : "(vazia)");
      return 1;
    } }
  agenda_definir_hoje("2026-09-16");
  // Portugues: a chave da tabela de traducao E o portugues, e e nele que o dono
  // julga a tela. Com a build em ingles as palavras novas (as que ainda nao
  // entraram em idioma_tab.h) apareceriam soltas no meio do ingles e a foto
  // diria mais sobre a tabela que sobre o desenho.
  ajustesDeTeste(0, 0);

  // --- 1. O ESTADO VAZIO ----------------------------------------------------
  agenda_iniciar();
  agenda_montar();
  snprintf(nome, sizeof nome, "%s-vazia.bmp", saida);
  captura(nome, w);

  // --- os dados, pelo disco -------------------------------------------------
  //
  // As sinopses sao INVENTADAS, e so podem ser aqui: o corpo do TMDB nao entra
  // no teste (nao ha rede) e o que a foto precisa provar e que duas linhas de
  // sinopse cabem e cortam. Nenhuma delas chega perto do aparelho.
  poeLinha(cache, sizeof cache, "tt10255564", "Foundation",
           "deploy/app/art/00.jpg", AG_VOLTANDO, 3, 9, "The Last Empress",
           "2026-09-16", "2026-09-12",
           "Gaal and Salvor reach Trantor on the day the Empire announces the "
           "end of the genetic dynasty, and the Foundation has to decide whether "
           "the Plan is still worth anything after three hundred years.",
           "finale", "Apple TV+", 58, 3);
  poeLinha(cache, sizeof cache, "tt1520211", "The Last of Us",
           "deploy/app/art/01.jpg", AG_VOLTANDO, 2, 4, "Day One",
           "2026-09-17", "2026-09-10",
           "Ellie atravessa Seattle sozinha.",
           "standard", "HBO", 52, 2);
  poeLinha(cache, sizeof cache, "tt2661044", "Severance",
           "deploy/app/art/02.jpg", AG_VOLTANDO, 3, 1, "The Other Half",
           "2026-09-19", "2025-03-21",
           "Mark acorda do outro lado do andar severado e encontra uma porta que "
           "não existia no mapa do departamento.",
           "premiere", "Apple TV+", 47, 3);
  poeLinha(cache, sizeof cache, "tt7366338", "Andor",
           "deploy/app/art/03.jpg", AG_VOLTANDO, 3, 1, "", "2026-11-04", "",
           "", "premiere", "Disney+", 44, 2);
  poeLinha(cache, sizeof cache, "tt0944947", "Succession",
           "deploy/app/art/04.jpg", AG_VOLTANDO, 0, 0, "", "", "2026-02-11",
           "", "", "HBO", 62, 4);
  poeLinha(cache, sizeof cache, "tt0903747", "Breaking Bad",
           "deploy/app/art/05.jpg", AG_ENCERRADA, 0, 0, "", "", "2013-09-29",
           "", "", "AMC", 47, 5);
  poeLinha(cache, sizeof cache, "tt9999991", "Uma Série Cancelada",
           "deploy/app/art/06.jpg", AG_CANCELADA, 0, 0, "", "", "2023-05-26",
           "", "", "", 0, 1);
  dados_gravar("agenda-p1.txt", cache);
  // Dois lembretes ligados, um deles VENCIDO (estreia hoje) — e o que o cartao
  // de abertura vai mostrar.
  dados_gravar("lembretes-p1.txt",
               "tt10255564\t2026-09-16\t0\n"
               "tt2661044\t2026-09-19\t0\n");
  // RELER O DISCO. agenda_iniciar() so recarrega quando o PERFIL muda — e o
  // que evita uma leitura de arquivo por quadro no aparelho. Aqui os arquivos
  // nasceram depois da primeira leitura, entao a ida e volta ao perfil 2 e o
  // caminho publico para forcar a releitura. A primeira versao desta captura
  // saiu com o cartao fechado exatamente por causa disso.
  perfis_definir_ativo(2); agenda_iniciar();
  perfis_definir_ativo(1); agenda_iniciar();

  // O catalogo entra tambem: a lista de "series que eu sigo" sai de
  // CatItem.naLista, e sem ele so os dois com lembrete apareceriam.
  { CatItem ci[7];
    static const char *ID[7] = { "tt10255564", "tt1520211", "tt2661044",
                                 "tt7366338", "tt0944947", "tt0903747",
                                 "tt9999991" };
    static const char *TIT[7] = { "Foundation", "The Last of Us", "Severance",
                                  "Andor", "Succession", "Breaking Bad",
                                  "Uma Série Cancelada" };
    int i;
    memset(ci, 0, sizeof ci);
    for (i = 0; i < 7; i++) {
      snprintf(ci[i].imdb, sizeof ci[i].imdb, "%s", ID[i]);
      snprintf(ci[i].tipo, sizeof ci[i].tipo, "%s", "series");
      snprintf(ci[i].titulo, sizeof ci[i].titulo, "%s", TIT[i]);
      snprintf(ci[i].poster, sizeof ci[i].poster, "deploy/app/art/0%d.jpg", i);
      snprintf(ci[i].meta, sizeof ci[i].meta, "%s", "2026 · 3 temporadas");
      ci[i].naLista = 1;
      ci[i].nTemporadas = 3;
    }
    cat_definir_tudo(ci, 7, NULL, 0); }

  // --- 2. O TOPO DO EIXO: a origem "HOJE" e a estreia de hoje em foco -------
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-hoje.bmp", saida);
  captura(nome, w);

  // --- 2b. A ULTIMA NOTICIA como citacao, o menu de contexto e o painel ------
  // Rede de verdade (Google News): espera ate 8 s pela resposta da primeira
  // linha. Sem rede a foto sai com a sinopse, que e o comportamento certo.
  { int i; for (i = 0; i < 80 && !noticias_respondeu("tt10255564"); i++) SDL_Delay(100); }
  printf("noticias Foundation: %d manchete(s)\n", noticias_n("tt10255564"));
  snprintf(nome, sizeof nome, "%s-noticia.bmp", saida);
  captura(nome, w);
  segurarOk();
  snprintf(nome, sizeof nome, "%s-ctx.bmp", saida);
  captura(nome, w);
  tecla(SDLK_DOWN); tecla(SDLK_RETURN);
  snprintf(nome, sizeof nome, "%s-ctx-noticias.bmp", saida);
  captura(nome, w);
  tecla(SDLK_ESCAPE); tecla(SDLK_ESCAPE);

  // --- 3. UMA LINHA DE SINOPSE: The Last of Us --------------------------------
  // O caso que o pedido do dono nomeia. Com a sinopse embaixo, uma frase curta
  // abria os mesmos 86px e sobrava um terco de laje em branco; na coluna da
  // direita ela simplesmente ocupa menos linhas e a altura nao muda.
  tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-curta.bmp", saida);
  captura(nome, w);

  // --- 4. A VIRADA DE MES: Andor estreia em novembro ------------------------
  { int i; for (i = 0; i < 2; i++) tecla(SDLK_DOWN); }
  snprintf(nome, sizeof nome, "%s-mes.bmp", saida);
  captura(nome, w);

  // --- 5. O FIM DA LISTA: o separador, o eixo tracejado e as sem data -------
  { int i; for (i = 0; i < 3; i++) tecla(SDLK_DOWN); }
  snprintf(nome, sizeof nome, "%s-semdata.bmp", saida);
  captura(nome, w);

  // --- 6. O CABECALHO DE MES SEM LINHAS EMBAIXO -----------------------------
  //
  // Com "hoje" em 30 de agosto e a primeira estreia em 16 de setembro, a faixa
  // de origem escrevia AGOSTO 2026 e o cabecalho de virada escrevia SETEMBRO
  // 2026 logo abaixo, sem nada entre os dois. Era o que estava na captura do
  // dono (SETEMBRO vazio, OUTUBRO com a primeira serie). A foto tem de mostrar
  // UM cabecalho, o do mes que realmente comeca ali.
  agenda_definir_hoje("2026-08-30");
  agenda_montar();
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-mesvazio.bmp", saida);
  captura(nome, w);
  agenda_definir_hoje("2026-09-16");
  agenda_montar();

  // --- 7. REDUZIR ANIMACOES -------------------------------------------------
  // O despertador nao treme e as ondas ficam PARADAS e opacas. A foto existe
  // porque o estado ligado nao pode depender de movimento: quem liga este
  // ajuste continua precisando de saber quais linhas estao marcadas.
  ajustesDeTeste(0, 1);
  agendaui_iniciar();
  snprintf(nome, sizeof nome, "%s-reduzido.bmp", saida);
  captura(nome, w);
  ajustesDeTeste(0, 0);

  // --- 8. O CARTAO DO LEMBRETE VENCIDO -------------------------------------
  oQue = DES_AVISO;
  agendaviso_mostrar_se_houver();
  printf("cartao de lembrete aberto: %d\n", agendaviso_aberto());
  snprintf(nome, sizeof nome, "%s-aviso.bmp", saida);
  captura(nome, w);

  // --- 9. A BARRA LATERAL com o item Agenda em foco ------------------------
  oQue = DES_MENU;
  menu_iniciar();
  menu_abrir();
  tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN); tecla(SDLK_DOWN);
  snprintf(nome, sizeof nome, "%s-menu.bmp", saida);
  captura(nome, w);

  // --- 10. OS QUATRO ESTADOS DO BOTAO DO LEMBRETE --------------------------
  //
  // O registro de tt10255564 no cache ganha data FUTURA, que e a condicao do
  // botao: com a data de hoje ele apareceria tambem, mas a foto ficaria sem o
  // caso mais comum ("em 3 dias").
  agenda_registrar("tt10255564", "Foundation", "", "Returning Series", 3, 9,
                   "The Last Empress", "2026-09-19", "2026-09-12");
  oQue = DES_DETALHE;
  { HomeItem it;
    memset(&it, 0, sizeof it);
    it.indice = 0;
    it.rect = (GfxRect){ 760.0f, 340.0f, 248.0f, 372.0f };
    it.titulo = "Foundation";
    it.arte = "deploy/app/art/00.jpg";
    detail_abrir(&it); }

  // LIGADO em repouso: o circulo esmeralda. (tt10255564 ja tem lembrete no
  // arquivo de lembretes gravado acima.)
  printf("lembrete de tt10255564: %d\n", agenda_lembrete("tt10255564"));
  snprintf(nome, sizeof nome, "%s-lembrete-ligado.bmp", saida);
  captura(nome, w);

  // LIGADO em foco: superficie clara, relogio escuro-esmeralda, e a linha acima
  // vira a legenda do botao.
  teclaDet(SDLK_RIGHT);
  snprintf(nome, sizeof nome, "%s-lembrete-ligado-foco.bmp", saida);
  captura(nome, w);

  // DESLIGADO em foco: o OK sobre o botao focado alterna e grava.
  teclaDet(SDLK_RETURN);
  printf("lembrete depois do OK: %d\n", agenda_lembrete("tt10255564"));
  snprintf(nome, sizeof nome, "%s-lembrete-desligado-foco.bmp", saida);
  captura(nome, w);

  // DESLIGADO em repouso: o foco volta para o botao primario.
  teclaDet(SDLK_LEFT);
  snprintf(nome, sizeof nome, "%s-lembrete-desligado.bmp", saida);
  captura(nome, w);

  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da Agenda gravadas.");
  return 0;
}
