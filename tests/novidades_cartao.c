// O CARTAO DE NOVIDADES DA VERSAO ATUAL (novidades_cartao.h), sem janela nem
// rede: quando abre, o que grava, a corrente com os cartoes antigos (2.0.1 e
// 2.0.2 nunca abrem depois dele), a porta da Home (nunca por cima do player
// nem da pagina do titulo), as paginas e os botoes, e o que cada plataforma
// ve. Cada cenario limpa as marcas e esquece as decisoes, como uma abertura
// nova do app.
//   bash tests/novidades_cartao.sh
#include "novidades_cartao.h"
#include "novidades20.h"
#include "novidades201.h"
#include "novidades202.h"
#include "dados.h"
#include "apoio.h"
#include "qr.h"
#include "ajustes.h"
#include "idioma.h"
#include "idiomacod.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int existe(const char *a) { char *s = dados_ler(a); int ok = s != NULL; free(s); return ok; }

static void tecla(SDL_Keycode k) {
  SDL_Event e;
  memset(&e, 0, sizeof e);
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  novcartao_evento(&e);
}

// Uma abertura do app: as marcas do disco ficam, as decisoes da sessao nao.
static void novaSessao(void) {
  novcartao_teste_esquecer();
  novidades202_teste_esquecer();
  novidades201_teste_esquecer();
}

static void limpar(void) {
  dados_apagar(novcartao_arquivo());
  dados_apagar(N202_ARQ);
  dados_apagar(N201_ARQ);
  dados_apagar("novidades-203.txt");
  dados_apagar("novidades-20-guia.txt");
  novaSessao();
}

// O QUADRO DA HOME COMO O app.c O RODA (atualizar, "primeira vez"): o cartao
// atual decide primeiro, com a porta dele; os antigos so com a mesma porta.
static void quadroHome(int homePronta, int player, int detalhe) {
  novcartao_decidir(homePronta, player, detalhe);
  if (homePronta && !player && !detalhe) {
    novidades202_primeira_vez();
    novidades201_primeira_vez();
  }
}

static void instalacaoNova(void) {
  limpar();
  quadroHome(1, 0, 0);
  assert(!novcartao_aberto());
  assert(existe(novcartao_arquivo()));
  assert(!novidades202_aberto() && !novidades201_aberto());
  assert(existe(N202_ARQ) && existe(N201_ARQ));
  puts("PASS: instalacao nova: o guia da 2.0 abre, este cartao fica visto (e os antigos)");
}

static void vindoDa202(void) {
  limpar();
  dados_gravar("novidades-20-guia.txt", "1\n");
  dados_gravar(N202_ARQ, "1\n");
  dados_gravar(N201_ARQ, "1\n");
  quadroHome(1, 0, 0);
  assert(novcartao_aberto());
  assert(!existe(novcartao_arquivo()));   // so ao fechar
  tecla(SDLK_ESCAPE);                      // Voltar na primeira pagina = Agora nao
  assert(!novcartao_aberto() && existe(novcartao_arquivo()));
  // Outra sessao: nao abre de novo, e os antigos tambem nao.
  novaSessao();
  quadroHome(1, 0, 0);
  assert(!novcartao_aberto() && !novidades202_aberto() && !novidades201_aberto());
  puts("PASS: vindo da 2.0.2: abre uma vez, grava a marca, nao volta");
}

// A marca da 2.0.3 nao pode esconder o hotfix; a nova marca so vem ao fechar.
static void vindoDa203(void) {
  limpar();
  dados_gravar("novidades-20-guia.txt", "1\n");
  dados_gravar("novidades-203.txt", "1\n");
  quadroHome(1, 0, 0);
  assert(novcartao_aberto() && !existe("novidades-204.txt"));
  tecla(SDLK_ESCAPE);
  assert(existe("novidades-203.txt") && existe("novidades-204.txt"));
  novaSessao();
  quadroHome(1, 0, 0);
  assert(!novcartao_aberto());
  puts("PASS: vindo da 2.0.3: marca independente, abre uma vez");
}

static void correnteAntigos(void) {
  // Vindo da 2.0.0 ou 2.0.1: o guia ja foi visto e nenhum cartao da 2.0.x.
  // Abre o da versao atual; os da 2.0.2 e 2.0.1 NAO abrem, nem no mesmo
  // quadro, nem depois de ele fechar, nem em outra sessao.
  limpar();
  dados_gravar("novidades-20-guia.txt", "1\n");
  quadroHome(1, 0, 0);
  assert(novcartao_aberto());
  assert(!novidades202_aberto() && !novidades201_aberto());
  assert(existe(N202_ARQ) && existe(N201_ARQ));
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);                      // Agora nao
  assert(!novcartao_aberto());
  quadroHome(1, 0, 0);
  assert(!novidades202_aberto() && !novidades201_aberto());
  novaSessao();
  quadroHome(1, 0, 0);
  assert(!novcartao_aberto() && !novidades202_aberto() && !novidades201_aberto());
  // Mesmo com a marca do atual e SEM as antigas (disco de uma versao de
  // teste, arquivo apagado a mao): o antigo nao abre depois do atual.
  dados_apagar(N202_ARQ);
  dados_apagar(N201_ARQ);
  novaSessao();
  quadroHome(1, 0, 0);
  assert(!novcartao_aberto() && !novidades202_aberto() && !novidades201_aberto());
  puts("PASS: o cartao da 2.0.2 (e o da 2.0.1) nunca abre depois do da versao atual");
}

static void portaDaHome(void) {
  // Com o player ou a pagina do titulo abertos, nao abre E nao gasta a
  // decisao: abre no primeiro quadro em que a Home esta livre.
  limpar();
  dados_gravar("novidades-20-guia.txt", "1\n");
  quadroHome(1, 1, 0);
  assert(!novcartao_aberto() && !existe(N202_ARQ));
  quadroHome(1, 0, 1);
  assert(!novcartao_aberto());
  quadroHome(1, 1, 1);
  assert(!novcartao_aberto());
  quadroHome(0, 0, 0);                     // outra tela, ou a Home ainda montando
  assert(!novcartao_aberto());
  quadroHome(1, 0, 0);
  assert(novcartao_aberto());
  tecla(SDLK_ESCAPE);
  assert(!novcartao_aberto());
  puts("PASS: nunca abre por cima do player nem da pagina do titulo; espera a Home");
}

static void paginas(void) {
  int n, i;
  limpar();
  novcartao_teste_plataforma(NOV_LG);
  novcartao_abrir();
  n = novcartao_paginas();
  assert(n >= 2);
  assert(novcartao_pagina() == 0 && novcartao_foco() == 1);
  // OK em Continuar anda ate "Apoie o projeto"; Concluir fecha e grava.
  for (i = 1; i < n; i++) { tecla(SDLK_RETURN); assert(novcartao_aberto() && novcartao_pagina() == i); }
  // Voltar (tecla) volta uma pagina; OK em Voltar tambem.
  tecla(SDLK_ESCAPE);
  assert(novcartao_aberto() && novcartao_pagina() == n - 2);
  tecla(SDLK_RETURN);
  assert(novcartao_pagina() == n - 1);
  tecla(SDLK_LEFT);
  tecla(SDLK_RETURN);
  assert(novcartao_aberto() && novcartao_pagina() == n - 2 && novcartao_foco() == 1);
  tecla(SDLK_RETURN);
  tecla(SDLK_RETURN);                      // Concluir
  assert(!novcartao_aberto() && existe(novcartao_arquivo()));
  // A esquerda/direita nao saem dos dois botoes.
  novcartao_abrir();
  tecla(SDLK_LEFT); tecla(SDLK_LEFT);
  assert(novcartao_foco() == 0);
  tecla(SDLK_RIGHT); tecla(SDLK_RIGHT);
  assert(novcartao_foco() == 1);
  tecla(SDLK_ESCAPE);
  assert(!novcartao_aberto());
  printf("PASS: %d paginas (a ultima e Apoie o projeto); Continuar, Voltar e Concluir\n", n);
}

static void plataformas(void) {
  int lg, tpk, wgt, and;
  novcartao_teste_plataforma(NOV_LG);
  lg = novcartao_itens_visiveis();
  assert(novcartao_cenas() == 0);
  novcartao_teste_plataforma(NOV_TPK);
  tpk = novcartao_itens_visiveis();
  assert(novcartao_cenas() == 0);
  novcartao_teste_plataforma(NOV_WGT);
  wgt = novcartao_itens_visiveis();
  assert(novcartao_cenas() == 0);
  novcartao_teste_plataforma(NOV_ANDROID);
  and = novcartao_itens_visiveis();
  assert(novcartao_cenas() == 0);
  assert(lg == 10 && tpk == 10 && wgt == 9 && and == 10);
  assert(novcartao_paginas() == 3);  // correcoes, fontes/plataforma, apoio
  assert(novcartao_previa_pronta());  // sem artes para esperar
  novcartao_teste_plataforma(NOV_OUTRAS);
  assert(novcartao_itens_visiveis() == 12);
  assert(novcartao_paginas() == 4);  // host mostra as tres plataformas
  novcartao_teste_plataforma(0);
  printf("PASS: itens por plataforma: LG %d, .tpk %d, .wgt %d, Android %d\n", lg, tpk, wgt, and);
}

static void discord(void) {
  Qr qr;
  // O terceiro codigo existe, mas nao faz parte do enumerador de doacoes.
  assert(!strcmp(apoio_url(APOIO_N), "https://discord.gg/9NWr6SHyzJ"));
  assert(!strcmp(apoio_url_curta(APOIO_N), "discord.gg/9NWr6SHyzJ"));
  assert(!strcmp(apoio_nome(APOIO_N), "Discord"));
  assert(!strcmp(NV_URL_DISCORD, apoio_url(APOIO_DISCORD)));
  assert(apoio_n() == 2 && apoio_qual(2) == -1);
  for (int i = 0; i < apoio_n(); i++) assert(apoio_qual(i) != APOIO_N);
  assert(qr_gerar(&qr, apoio_url(APOIO_N)) && qr.lado > 0);
  puts("PASS: Discord com URL correta e QR geravel, fora das doacoes");
}

static void espacamento(void) {
  FILE *f;
  char caminho[700];
  snprintf(caminho, sizeof caminho, "%s/ajustes.txt", dados_dir());
  f = fopen(caminho, "w"); assert(f);
  fputs("idioma 0\nfonteInterface 3\n", f); fclose(f);
  ajustes_dir(dados_dir());
  assert(txt_iniciar("deploy/app", 1)); // so TTF; sem janela ou contexto GL
  assert(txt_largura(TXT_CAPTION, "Abas de temporadas corrigidas em animes longos.") > 0);
  novcartao_teste_plataforma(NOV_LG);
  novcartao_abrir();
  // As seis frases da primeira pagina cabem em uma linha em pt/Montserrat.
  // Uma reserva de 94px aqui cria 30px de vao extra antes do proximo item.
  for (int i = 0; i < 6; i++) assert(novcartao_teste_altura_item(i) == 64.0f);
  assert(txt_rasterizadas == 0);
  int paginas = novcartao_paginas();
  for (int i = 0; i < 120; i++) {
    txt_novo_quadro(); novcartao_atualizar(1.0f / 60.0f, 0);
    assert(novcartao_paginas() == paginas);
    assert(novcartao_teste_altura_item(0) == 64.0f);
  }
  for (int idi = 0; idi < IDIOMA_N; idi++) {
    f = fopen(caminho, "w"); assert(f);
    fprintf(f, "idioma %d\nfonteInterface 3\n", idi); fclose(f);
    ajustes_dir(dados_dir());
    assert(ajustes_idioma() == idi);
    novcartao_atualizar(0, 0); // muda o idioma com o cartao ainda aberto
    float h = txt_bloco_corta(TXT_CAPTION,
      "O modelo de URL personalizado é salvo. Um aviso indica quando fica longo demais.",
      0, 0, 0, 0, 0, 1014, 30, 0, 2);
    assert(novcartao_teste_altura_item(5) == 34.0f + h);
    assert(txt_rasterizadas == 0);
    assert(novcartao_paginas() >= 3 && novcartao_paginas() <= 4);
    assert(txt_bloco(TXT_CAPTION, "Dúvidas, bugs e builds de teste.",
      0, 0, 0, 0, 0, 300, 28, 0, 0) <= 84);
  }
  f = fopen(caminho, "w"); assert(f);
  fputs("idioma 0\nfonteInterface 3\n", f); fclose(f);
  ajustes_dir(dados_dir()); novcartao_atualizar(0, 0);
  assert(novcartao_teste_altura_item(1) == 64);
  // O mesmo quebrador em caminhos distintos, sem GL: quebra dura, CJK, teto.
  assert(txt_bloco_corta(TXT_CAPTION, "Uma frase que ocupa varias linhas", 0, 0, 0, 0, 0, 80, 30, 0, 2) == 60);
  assert(txt_bloco_corta(TXT_CAPTION, "Uma\nOutra", 0, 0, 0, 0, 0, 1014, 30, 0, 2) == 60);
  assert(txt_bloco_corta(TXT_CAPTION, "Uma frase longa", 0, 0, 0, 0, 0, 1, 30, 0, 1) == 30);
  assert(txt_bloco(TXT_CAPTION, "字幕字幕字幕", 0, 0, 0, 0, 0, 40, 30, 0, 0) > 30);
  assert(txt_rasterizadas == 0); // medir nao cria texturas nem depende do orcamento
  txt_encerrar();
  puts("PASS: pt/Montserrat sem linhas vazias; alturas e paginas estaveis por 120 quadros");
  printf("PASS: troca de idioma com cartao aberto nos %d idiomas; sem rasterizacao\n", IDIOMA_N);
}

int main(int argc, char **argv) {
  const char *dir = getenv("NUVIO_DADOS");
  if (!dir || !dir[0]) return 2;
  dados_iniciar(dir);
  if (strcmp(dados_dir(), dir)) return 2;
  if (argc > 1) {
    if (!strcmp(argv[1], "discord")) discord();
    else espacamento();
    return 0;
  }
  assert(!strcmp(novcartao_versao(), "2.0.4"));
  assert(!strcmp(novcartao_arquivo(), "novidades-204.txt"));
  instalacaoNova();
  vindoDa202();
  vindoDa203();
  correnteAntigos();
  portaDaHome();
  paginas();
  plataformas();
  discord();
  espacamento();
  puts("novidades_cartao: ok");
  return 0;
}
