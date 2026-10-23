// A TECLA DE ESPACO da Busca e do Spotlight: "the office" digitado tem de dar
// "the office", e a tecla tem de se chamar ESPACO (a barra) em todo idioma, nao
// "espaco sideral".
//
// A COLISAO MEDIDA: a chave de i18n "espaço" servia a dois textos — o rotulo da
// tecla (busca.c, spotlight.c, teclado.c) e o nome do TEMA "space" do mapa
// (mapa.c, keyword do TMDB). As traducoes foram feitas pelo tema: a tecla saia
// "Weltraum" em alemao, "космос" em russo, "宇宙" em japones. Uma tabela, uma
// traducao por chave: o tema ganhou chave propria ("espaço sideral").
//
// Digitacao por dois caminhos: D-pad + OK na grade da tela (o controle) e tecla
// fisica (SDLK_SPACE).
#include "../src/ajustes.c"
#include "busca.h"
#include "spotlight.h"
#include "teclado.h"
#include "mapa.h"
#include "idioma.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void (*alvo)(const SDL_Event *);

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN; e.key.keysym.sym = k; alvo(&e);
  e.type = SDL_KEYUP;   alvo(&e);
}
static void repetir(SDL_Keycode k, int n) { while (n-- > 0) tecla(k); }

// Grade 6 x 6 de teclado_alfabeto() e, embaixo, a fileira de comandos com o
// espaco na coluna 0 — nas duas telas.
static void posicao(char ch, int *f, int *c) {
  const char *p;
  if (ch == ' ') { *f = 6; *c = 0; return; }
  p = strchr(teclado_alfabeto(), ch);
  assert(p);
  *f = (int)(p - teclado_alfabeto()) / 6;
  *c = (int)(p - teclado_alfabeto()) % 6;
}

// Busca: ESQUERDA na coluna 0 sai da tela, entao o foco e rastreado aqui
// (focus_mover_grade mantem a coluna ao trocar de fileira).
static void dpadBusca(const char *s) {
  int f = 0, c = 0, tf, tc;
  for (; *s; s++) {
    posicao(*s, &tf, &tc);
    if (tf == 6 && c > 2) { repetir(SDLK_LEFT, c - 2); c = 2; }  // fileira de 3
    repetir(tf > f ? SDLK_DOWN : SDLK_UP, tf > f ? tf - f : f - tf); f = tf;
    repetir(tc > c ? SDLK_RIGHT : SDLK_LEFT, tc > c ? tc - c : c - tc); c = tc;
    tecla(SDLK_RETURN);
  }
}

// Spotlight: a troca de fileira escolhe a tecla mais perto pelo x, entao cada
// letra parte do canto (ESQUERDA na coluna 0 nao faz nada aqui). O teclado do
// app so abre com OK no campo, e CIMA da primeira fileira volta ao campo: por
// isso cada letra sobe ate o campo e desce uma para a primeira fileira.
static void dpadSpot(const char *s) {
  int tf, tc;
  tecla(SDLK_RETURN);                        // OK no campo: o teclado do app
  for (; *s; s++) {
    posicao(*s, &tf, &tc);
    repetir(SDLK_UP, 8); tecla(SDLK_DOWN); repetir(SDLK_LEFT, 8);
    repetir(SDLK_DOWN, tf); repetir(SDLK_LEFT, 8);
    repetir(SDLK_RIGHT, tc);
    tecla(SDLK_RETURN);
  }
}

static void fisico(const char *s) {
  for (; *s; s++) tecla(*s == ' ' ? SDLK_SPACE : (SDL_Keycode)*s);
}

static void confere(const char *onde, const char *veio, const char *quer) {
  if (strcmp(veio, quer)) {
    fprintf(stderr, "FALHA %s: \"%s\" em vez de \"%s\"\n", onde, veio, quer);
    assert(0);
  }
  printf("ok   %-22s \"%s\"\n", onde, veio);
}

int main(int argc, char **argv) {
  static const struct { int lg; const char *tecla; } ROTULO[] = {
    { IDIOMA_EN, "space" }, { IDIOMA_DE, "Leertaste" }, { IDIOMA_RU, "пробел" },
    { IDIOMA_UK, "пробіл" }, { IDIOMA_JA, "スペース" }, { IDIOMA_ZHCN, "空格" },
    { IDIOMA_PL, "spacja" }, { IDIOMA_NL, "spatie" },
  };
  size_t i;

  if (argc > 1 && !strcmp(argv[1], "spacing")) {
    int ops[] = { AJ_ESPACO_FILEIRAS, AJ_ESPACO_TITULOS };
    for (int o = 0; o < 2; o++) {
      for (int v = 50; v <= 300; v += 10) {
        valor[ops[o]] = v;
        assert(passoAdiante(ops[o], -1) == (v <= 50 ? 50 : ((v - 51) / 25) * 25 + 50));
        assert(passoAdiante(ops[o], 1) == (v >= 300 ? 300 : ((v - 50) / 25 + 1) * 25 + 50));
        assert(valor[ops[o]] == v);  // preserva o valor carregado
      }
    }
    puts("spacing legacy: ok"); return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "flow-i18n")) {
    const int langs[] = { IDIOMA_PT, IDIOMA_EN, IDIOMA_JA, IDIOMA_ZHCN, IDIOMA_ZHTW };
    const char *names[] = { "Fluxo de capas", "Cover Flow", "カバーフロー", "封面流", "封面流" };
    for (int l = 0; l < 5; l++) {
      valor[AJ_IDIOMA] = langs[l] + 1;
      confere("flow chip", i18n("Fluxo de capas"), names[l]);
    }
    puts("flow i18n: ok"); return 0;
  }
  valor[AJ_IDIOMA] = IDIOMA_PT + 1;

  alvo = busca_evento;
  busca_iniciar(); dpadBusca("the office");
  confere("busca D-pad", busca_consulta(), "the office");
  busca_iniciar(); fisico("the office");
  confere("busca teclado fisico", busca_consulta(), "the office");

  alvo = spot_evento;
  spot_abrir(0); dpadSpot("the office");
  confere("spotlight D-pad", spot_consulta(), "the office");
  spot_fechar(); spot_abrir(0); fisico("the office");
  confere("spotlight teclado fisico", spot_consulta(), "the office");
  spot_fechar();

  // O tema do mapa nao pode usar a chave da tecla.
  assert(strcmp(mapa_tema_nome("space"), "espaço") != 0);
  confere("tema \"space\" (pt)", mapa_tema_nome("space"), "espaço sideral");

  for (i = 0; i < sizeof ROTULO / sizeof ROTULO[0]; i++) {
    char onde[40];
    valor[AJ_IDIOMA] = ROTULO[i].lg + 1;
    snprintf(onde, sizeof onde, "rotulo tecla (%s)", idioma_iso(ROTULO[i].lg));
    confere(onde, i18n("espaço"), ROTULO[i].tecla);
  }
  valor[AJ_IDIOMA] = IDIOMA_DE + 1;
  confere("tema \"space\" (de)", i18n(mapa_tema_nome("space")), "Weltraum");
  puts("espaco: tudo certo");
  return 0;
}
