// #205 "tudo meio duro": o que ganhou mola NAO PODE MAIS SALTAar. Aqui os cinco
// componentes mais visiveis do lote sao dirigidos QUADRO A QUADRO (dt de 60 fps)
// e o valor INTERMEDIARIO e conferido no meio do voo — o codigo de antes
// (binario, um quadro) nao compila/nao passa nisto.
//
// Inclui os modulos donos do estado para alcancar os passos e as molas
// (estaticos); o resto do src/ e ligado normal. Sem janela, rede ou GL.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>
#include <SDL2/SDL.h>
#include "../src/aovivoui.c"
#include "../src/plrui.c"
// `gravar` e estatico EM AMBOS (central.c e ajustes.c): renomeia o da Central
// enquanto os dois moram nesta unidade de traducao.
#define gravar centralGravarTeste
#define dicas centralDicasTeste
#define botao centralBotaoTeste
#include "../src/central.c"
#undef botao
#undef dicas
#undef gravar
#include "../src/episodios.c"
#include "nvtmp.h"
#include "../src/ajustes.c"

static int falhas;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHA: " __VA_ARGS__); printf("\n"); falhas++; } } while (0)
#define DT (1.0f / 60.0f)
#define MEIO(v) ((v) > 0.02f && (v) < 0.98f)

// --- 1. OSD ao vivo: a pilula do botao cresce em mola, nao num quadro --------
static void testeAovivo(void) {
  AoVivoOsd o;
  memset(&o, 0, sizeof o);
  o.nBotoes = 4;
  o.botoes[0] = AV_B_PAUSA; o.botoes[1] = AV_B_GUIA;
  o.botoes[2] = AV_B_FAV;    o.botoes[3] = AV_B_AUDIO;
  // Semeia: o foco nasce no Pausa, ja assentado (1a vez e sempre "no lugar").
  osdFocoAlvo = AV_B_PAUSA;
  osdPasso(0.0f);
  CHECK(fabsf(osdFoco[AV_B_PAUSA] - 1.0f) < 0.001f, "semeio do foco do OSD");
  // A tecla anda para o Guia: UM quadro depois NINGUEM esta nos extremos.
  osdFocoAlvo = AV_B_GUIA;
  osdPasso(DT);
  CHECK(MEIO(osdFoco[AV_B_PAUSA]), "OSD: o antigo ainda apagando (%.3f)", osdFoco[AV_B_PAUSA]);
  CHECK(MEIO(osdFoco[AV_B_GUIA]), "OSD: o novo ainda acendendo (%.3f)", osdFoco[AV_B_GUIA]);
  // A LARGURA acompanha a mola: no meio do caminho, a media dos extremos.
  { float w0 = larguraBotaoAv(&o, AV_B_FAV, 0.0f), w1 = larguraBotaoAv(&o, AV_B_FAV, 1.0f),
        wm = larguraBotaoAv(&o, AV_B_FAV, 0.5f);
    CHECK(fabsf(w1 - w0) > 20.0f, "OSD: pilula do Favorito cresce (%.1f -> %.1f)", w0, w1);
    CHECK(fabsf(wm - (w0 + w1) * 0.5f) < 1.0f, "OSD: largura no meio (%.1f)", wm); }
  // REAPARECER NASCE ASSENTADO: some o alvo (OSD escondido), as molas apagam
  // e o "pronto" zera; a proxima aparecida ja pousa no foco da vez, sem voo.
  osdFocoAlvo = -1;
  osdPasso(DT * 30.0f);
  CHECK(fabsf(osdFoco[AV_B_GUIA]) < 0.02f, "OSD: apagou escondido (%.3f)", osdFoco[AV_B_GUIA]);
  osdFocoAlvo = AV_B_AUDIO;
  osdPasso(DT);
  CHECK(fabsf(osdFoco[AV_B_AUDIO] - 1.0f) < 0.001f, "OSD: reapareceu assentado (%.3f)",
        osdFoco[AV_B_AUDIO]);
  // Cartao de erro: molas e relogio PROPRIOS — o passo do OSD nao o balanca.
  erroFocoA[0] = 0.5f;
  osdPasso(DT);
  CHECK(fabsf(erroFocoA[0] - 0.5f) < 0.001f, "erro: imune ao passo do OSD (%.3f)", erroFocoA[0]);
  // Mesmas molas nos tres botoes (seed: o Recarregar nasce assentado).
  erroFocoAlvo = 0;
  erroPasso(0.0f);
  CHECK(fabsf(erroFocoA[0] - 1.0f) < 0.001f, "erro: semeio do foco");
  erroFocoAlvo = 2;
  erroPasso(DT);
  CHECK(MEIO(erroFocoA[0]) && MEIO(erroFocoA[2]), "erro: foco em mola (%.3f/%.3f)",
        erroFocoA[0], erroFocoA[2]);
}

// --- 2. plrui_seg: a pilula de foco DESLIZA entre os chips -------------------
static void testeSeg(void) {
  float w[3] = { 100.0f, 200.0f, 100.0f };
  GfxRect a = segPilulaRect(w, 3, 0.0f, 0.0f, 0.0f);
  GfxRect b = segPilulaRect(w, 3, 1.0f, 0.0f, 0.0f);
  GfxRect m = segPilulaRect(w, 3, 0.5f, 0.0f, 0.0f);
  CHECK(fabsf(m.x - (a.x + b.x) * 0.5f) < 0.01f, "seg: x interpolado (%.1f)", m.x);
  CHECK(fabsf(m.w - (a.w + b.w) * 0.5f) < 0.01f, "seg: largura interpolada (%.1f)", m.w);
  CHECK(a.x == 5.0f && fabsf(a.w - 100.0f) < 0.01f, "seg: chip 0 no lugar (%.1f)", a.x);
}

// --- 3. Episodios: o realce da linha e o foco dos chips em mola --------------
static void testeEpisodios(void) {
  aberto = 1; grupo = 1; foco = 3;
  focoLinhaPasso(0.0f);
  focoLinhaPasso(DT);
  CHECK(MEIO(focoLin), "episodios: linha acendendo (%.3f)", focoLin);
  CHECK(focoLinIdx == 3, "episodios: linha alvo (%d)", focoLinIdx);
  foco = 4;
  focoLinhaPasso(DT);
  CHECK(focoLinAntIdx == 3 && focoLinAnt > 0.02f, "episodios: pos-brilho da anterior (%d/%.3f)",
        focoLinAntIdx, focoLinAnt);
  CHECK(MEIO(focoLin), "episodios: nova linha em mola (%.3f)", focoLin);
  // O foco SOBE para os chips de temporada: a pilula esvai da linha e o
  // segmentado ganha foco em mola.
  grupo = 0;
  focoLinhaPasso(DT);
  CHECK(MEIO(segFoc), "episodios: foco dos chips em mola (%.3f)", segFoc);
  focoLinhaPasso(DT * 20.0f);
  CHECK(segFoc > 0.98f, "episodios: foco dos chips assenta (%.3f)", segFoc);
  CHECK(focoLinIdx == -1, "episodios: linha solta o foco");
}

// --- 4. Central: anel e interruptor em mola ---------------------------------
static void testeCentral(void) {
  int op[CENTRAL_MAX + 1], item[CENTRAL_MAX + 1], n, i, tog = -1;
  central_abrir();
  n = botoes(op, item);
  CHECK(n > 0, "central: botoes (%d)", n);
  centralPassoFoco(DT);
  CHECK(MEIO(animFoco[0]), "central: anel acendendo (%.3f)", animFoco[0]);
  for (i = 0; i < n; i++) {
    if (tog < 0 && ajustes_rapido_ligado(op[i]) >= 0) tog = i;
  }
  CHECK(tog >= 0, "central: ha interruptor na lista");
  // A PREVIA inverte o interruptor: o realce do acento troca em mola.
  { float antes = animLig[tog], alvoAntes = ajustes_rapido_ligado(op[tog]) == 1 ? 1.0f : 0.0f;
    previa = 1; previaInverte = tog;
    centralPassoFoco(DT);
    CHECK(fabsf(animLig[tog] - antes) > 0.001f, "central: interruptor andando (%.3f->%.3f)",
          antes, animLig[tog]);
    CHECK((animLig[tog] > antes) == (alvoAntes < 0.5f), "central: na direcao certa (%.3f)", animLig[tog]);
    previa = 0; previaInverte = -1; }
  // PREVIA DESENHA SEM UPDATE (aberta=0). Sem o snap, o anel fica no botao
  // da ultima sessao e o flip do interruptor nao aparece. dt=0 pousa no alvo.
  { int alvo = tog > 0 ? tog : 1, lig = ajustes_rapido_ligado(op[alvo]);
    float salvoF = animFoco[0], salvoL = animLig[tog];
    previa = 1; previaInverte = alvo; foco = alvo;
    centralPassoFoco(0.0f);
    CHECK(fabsf(animFoco[alvo] - 1.0f) < 0.001f, "previa: anel no alvo (%.3f)", animFoco[alvo]);
    CHECK(animFoco[0] < 0.001f || alvo == 0, "previa: anel saiu do botao velho (%.3f)", animFoco[0]);
    if (lig >= 0)
      CHECK(fabsf(animLig[alvo] - (lig ? 0.0f : 1.0f)) < 0.001f,
            "previa: interruptor virado (lig=%d anim=%.3f)", lig, animLig[alvo]);
    previa = 0; previaInverte = -1;
    animFoco[0] = salvoF; animLig[tog] = salvoL; }
  central_fechar();
}

// --- 6. Revisao: reaparicao por quadro (nao relogio) e pilula sem opcao --------
static void testeRevisao(void) {
  // P2: engasgo de 300 ms COM o OSD na tela (quadros consecutivos) segue em voo.
  osdUlt = 0; osdQuadro = 0; memset(osdFocoPronto, 0, sizeof osdFocoPronto);
  gfx_quadro_n = 10; osdFocoAlvo = AV_B_PAUSA; osdTic();
  gfx_quadro_n = 11; osdFocoAlvo = AV_B_GUIA; SDL_Delay(300); osdTic();
  CHECK(MEIO(osdFoco[AV_B_GUIA]), "OSD: engasgo nao salta (%.3f)", osdFoco[AV_B_GUIA]);
  // O cartao de erro desenhado todo quadro nao mexe no OSD; depois de sumir
  // alguns quadros o OSD reaparece assentado no foco da vez.
  erroUlt = 0; erroQuadro = 0; memset(erroFocoPronto, 0, sizeof erroFocoPronto);
  for (gfx_quadro_n = 12; gfx_quadro_n < 40; gfx_quadro_n++) { erroFocoAlvo = 1; erroTic(); }
  osdFocoAlvo = AV_B_AUDIO; osdTic();
  CHECK(fabsf(osdFoco[AV_B_AUDIO] - 1.0f) < 0.001f, "OSD: reaparece assentado (%.3f)", osdFoco[AV_B_AUDIO]);
  // Cartao: o OSD ser o ultimo desenhado nao o faz saltar.
  erroFocoAlvo = 2; SDL_Delay(300); erroTic();   // quadro 40, o seguinte ao 39
  CHECK(MEIO(erroFocoA[2]), "erro: engasgo nao salta (%.3f)", erroFocoA[2]);
  // P1: linha de plugin (op = -1) segue `ligado` e nao toca nas molas.
  CHECK(aj2PilulaOn(-1, 1) == 1.0f && aj2PilulaOn(-1, 0) == 0.0f, "ajustes: pilula de plugin segue ligado");
  CHECK(aj2PilulaOn(AJ_N, 1) == 1.0f, "ajustes: op fora da faixa nao escreve");
}

// --- 5. Ajustes: a pilula Ligado/Desligado troca em mola ----------------------
static void testeAjustes(void) {
  int op = -1, i;
  for (i = 0; i < AJ_N; i++)
    if (ehInterruptor(i) && !soLeitura(i) && !inativa(i) && valor[i] == 0) { op = i; break; }
  CHECK(op >= 0, "ajustes: interruptor ligado disponivel");
  { float on0 = 1.0f - ajMov(op, uxValorAtual(op));   // semeia a mola
    CHECK(on0 > 0.98f, "ajustes: comeca ligado (%.3f)", on0);
    CHECK(definirValorDireto(op, 1), "ajustes: desliga");
    ajMovAtualizar(DT);
    { float meio = 1.0f - ajMovValor[op];
      CHECK(MEIO(meio), "ajustes: pilula no meio do voo (%.3f)", meio); }
    ajMovAtualizar(DT * 20.0f);
    CHECK(1.0f - ajMovValor[op] < 0.02f, "ajustes: assenta desligado (%.3f)", ajMovValor[op]); }
}

int main(void) {
  char dir[1024];
  assert(SDL_Init(SDL_INIT_TIMER) == 0);
  snprintf(dir, sizeof dir, "%s/nuvio-animacoes205-XXXXXX", nvt_dir());
  assert(mkdtemp(dir));
  assert(setenv("NUVIO_DADOS", dir, 1) == 0);
  dados_iniciar(dir); assert(!strcmp(dados_dir(), dir));
  snprintf(dirAjustes, sizeof dirAjustes, "%s", dir);
  ajustes_iniciar();
  testeAovivo();
  testeSeg();
  testeEpisodios();
  testeCentral();
  testeAjustes();
  testeRevisao();
  if (falhas) { printf("%d falha(s)\n", falhas); return 1; }
  printf("animacoes205: tudo em mola\n");
  return 0;
}
