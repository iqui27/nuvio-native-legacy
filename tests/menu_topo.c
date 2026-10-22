// #461: menu na barra do topo. A rail some (conteudo usa a largura toda) e as
// setas giram: ESQUERDA/DIREITA andam pelos itens, BAIXO devolve o foco ao
// conteudo, CIMA nao faz nada, OK escolhe, Voltar fecha. Desligado, a rail volta.
#include "ajustes.h"
#include "layout.h"
#include "menu.h"
#include "biblioteca.h"
#include "agendaui.h"
#include "busca.h"
#include "buscasrec.h"
#include "catalogo.h"
#include "dados.h"
#include "sistexto.h"
#include <SDL2/SDL.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>

static SDL_Event evento(SDL_Keycode k) {
  SDL_Event e;
  SDL_memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  return e;
}
static void tecla(SDL_Keycode k) {
  SDL_Event e = evento(k);
  menu_evento(&e);
}
static void agendaTecla(SDL_Keycode k) {
  SDL_Event e = evento(k);
  agendaui_evento(&e);
}
static void buscaTecla(SDL_Keycode k) {
  SDL_Event e = evento(k);
  busca_evento(&e);
}
static void assentar(void) { for (int i = 0; i < 200; i++) menu_atualizar(0.016f, 0); }
static void abrir(int destino) {
  menu_fechar(); assentar();
  menu_escolheu(); menu_mudou_destino();
  menu_definir_destino(destino);
  menu_abrir();
}

static char dir[] = "/tmp/nuvio-menu-topo-XXXXXX", caminho[700];
static void gravar(const char *txt) {
  FILE *f = fopen(caminho, "w"); assert(f);
  fputs(txt, f); fclose(f);
  ajustes_dir(dir);
}

int main(void) {
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir);
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dir);

  // Padrao: a rail de sempre.
  gravar("homeLayoutLocal 0\nmodernSidebar 0\n");
  assert(!ajustes_menu_topo() && ajustes_rail_largura_fixa() > 0.0f);

  gravar("homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 1\n");
  assert(ajustes_menu_topo());
  assert(ajustes_rail_largura_fixa() == 0.0f);
  assert(ajustes_conteudo_x() == NV_CONTENT_PAD);
  menu_iniciar();
  assert(menu_barra_borda() == 0.0f);

  abrir(MENU_INICIO);
  tecla(SDLK_RIGHT); tecla(SDLK_UP);          // anda; cima nao fecha nem escolhe
  assert(menu_aberto() && !menu_escolheu());
  tecla(SDLK_RIGHT); tecla(SDLK_LEFT); tecla(SDLK_RETURN);
  assert(menu_escolheu() && menu_destino() == MENU_EXPLORAR);

  abrir(MENU_INICIO);
  tecla(SDLK_RIGHT); tecla(SDLK_DOWN);        // baixo volta ao conteudo
  assert(!menu_aberto() && !menu_escolheu() && menu_destino() == MENU_INICIO);

  abrir(MENU_INICIO);
  tecla(SDLK_LEFT); tecla(SDLK_RETURN);       // sem volta na ponta esquerda
  assert(menu_destino() == MENU_INICIO);

  abrir(MENU_INICIO);
  tecla(SDLK_ESCAPE);
  assert(!menu_aberto() && !menu_escolheu());

  // No Dinamica a opcao nao vale: a pilula dele ja e o topo.
  gravar("homeLayoutLocal 2\nmodernSidebar 0\nmenuTopoLocal 1\n");
  assert(!ajustes_menu_topo());

  // Desligado de novo: a rail e as setas dela voltam.
  gravar("homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 0\n");
  assert(!ajustes_menu_topo() && ajustes_rail_largura_fixa() > 0.0f);
  abrir(MENU_INICIO);
  tecla(SDLK_DOWN); tecla(SDLK_RIGHT);
  assert(menu_escolheu() && menu_destino() == MENU_EXPLORAR);

  // #461: com a barra no topo, CIMA no primeiro item da tela abre a barra.
  gravar("homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 1\n");
  menu_fechar(); assentar();
  assert(ajustes_menu_topo());
  biblioteca_iniciar();
  { SDL_Event e; SDL_memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
    biblioteca_evento(&e);
    assert(biblioteca_quer_sair()); }
  menu_fechar(); assentar();
  ajustes_iniciar();
  { SDL_Event e; int i; SDL_memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
    for (i = 0; i < 8 && !ajustes_quer_sair(); i++) ajustes_evento(&e);
    assert(ajustes_quer_sair()); }

  // Agenda: o cabecalho interno sempre ganha; so CIMA nele abre o menu.
  agendaui_iniciar();
  agendaTecla(SDLK_UP);
  { int cab; agendaui_teste_foco(NULL, &cab, NULL, NULL, NULL, NULL);
    assert(cab != 0 && !agendaui_quer_sair()); }
  agendaTecla(SDLK_UP);
  assert(agendaui_quer_sair());
  agendaui_iniciar();
  agendaTecla(SDLK_UP); agendaTecla(SDLK_RIGHT); agendaTecla(SDLK_RETURN); // Mes
  { int cel;
    agendaui_teste_foco(NULL, NULL, NULL, NULL, &cel, NULL);
    while (cel >= 7) { agendaTecla(SDLK_UP); agendaui_teste_foco(NULL, NULL, NULL, NULL, &cel, NULL); }
  }
  agendaTecla(SDLK_UP);
  { int hoje, cab; agendaui_teste_foco(NULL, &hoje, NULL, NULL, NULL, NULL);
    assert(hoje != 0 && !agendaui_quer_sair());
    agendaTecla(SDLK_LEFT); agendaui_teste_foco(NULL, &cab, NULL, NULL, NULL, NULL);
    assert(cab != 0 && cab != hoje && !agendaui_quer_sair()); // Anterior
    agendaTecla(SDLK_RIGHT); agendaTecla(SDLK_RIGHT);
    agendaui_teste_foco(NULL, &cab, NULL, NULL, NULL, NULL);
    assert(cab != 0 && cab != hoje && !agendaui_quer_sair()); } // Proximo
  agendaTecla(SDLK_UP);
  assert(agendaui_quer_sair());

  // Busca: Recentes e o campo sao destinos internos acima de Populares/resultados.
  { CatItem itens[3]; CatFileira fil;
    memset(itens, 0, sizeof itens); memset(&fil, 0, sizeof fil);
    for (int i = 0; i < 3; i++) {
      snprintf(itens[i].imdb, sizeof itens[i].imdb, "tt-menu-%d", i);
      snprintf(itens[i].titulo, sizeof itens[i].titulo, "Batman %d", i);
      snprintf(itens[i].tipo, sizeof itens[i].tipo, "movie");
      snprintf(itens[i].poster, sizeof itens[i].poster, "poster-%d", i);
    }
    snprintf(fil.chave, sizeof fil.chave, "menu-topo");
    snprintf(fil.titulo, sizeof fil.titulo, "Populares");
    snprintf(fil.tipo, sizeof fil.tipo, "movie");
    fil.n = 3;
    cat_definir_tudo(itens, 3, &fil, 1);
  }
  st_teste_ligar(1); layout_modo_definir(1);
  assert(buscasrec_registrar("Matrix"));
  busca_iniciar(); busca_teste_layout_recentes();
  buscaTecla(SDLK_DOWN); buscaTecla(SDLK_DOWN); // campo -> Recentes -> Populares
  buscaTecla(SDLK_UP);
  assert(!busca_quer_sair() && busca_foco_campo() == 0);
  buscaTecla(SDLK_UP);
  assert(!busca_quer_sair() && busca_foco_campo() == 1);
  buscaTecla(SDLK_b); buscaTecla(SDLK_a); buscaTecla(SDLK_DOWN); // resultado de "ba"
  assert(busca_foco_campo() == 0);
  buscaTecla(SDLK_UP);
  assert(!busca_quer_sair() && busca_foco_campo() == 1);
  buscaTecla(SDLK_UP);
  assert(busca_quer_sair());
  busca_encerrar(); st_teste_ligar(0); layout_modo_definir(0);
  busca_iniciar(); busca_teste_layout_recentes();
  buscaTecla(SDLK_TAB); buscaTecla(SDLK_DOWN); // teclado -> Recentes -> Populares
  buscaTecla(SDLK_UP);
  assert(!busca_quer_sair());
  buscaTecla(SDLK_UP);
  assert(busca_quer_sair()); // agora no alto de Recentes
  busca_encerrar();

  // Sem a barra no topo, CIMA na Biblioteca nao pede saida.
  gravar("homeLayoutLocal 0\nmodernSidebar 0\nmenuTopoLocal 0\n");
  biblioteca_iniciar();
  { SDL_Event e; SDL_memset(&e, 0, sizeof e); e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_UP;
    biblioteca_evento(&e);
    assert(!biblioteca_quer_sair()); }
  agendaui_iniciar(); agendaTecla(SDLK_UP); agendaTecla(SDLK_UP);
  assert(!agendaui_quer_sair());
  busca_iniciar(); busca_teste_layout_recentes();
  buscaTecla(SDLK_TAB); buscaTecla(SDLK_DOWN); buscaTecla(SDLK_UP); buscaTecla(SDLK_UP);
  assert(!busca_quer_sair());
  busca_encerrar();

  unlink(caminho);
  { char t[700]; snprintf(t, sizeof t, "%s/ajustes.tmp", dir); unlink(t);
    snprintf(t, sizeof t, "%s/envio-151.txt", dir); unlink(t);
    snprintf(t, sizeof t, "%s/buscas-p1.txt", dir); unlink(t); }
  rmdir(dir);
  puts("menu_topo: ok");
  return 0;
}
