// DESENHO DAS NOTAS na pagina de titulo: a linha do hero (marca + valor na
// escala do site), a secao "Notas" (cartao de nota + blocos por fonte) e o
// cartao "Notas por episodio" do bloco "Numeros da temporada" (serieaud.c). As decisoes — escala, cor, ordem, encaixe — moram em
// notasfontes.c; aqui so se desenha.
#ifndef NV_NOTASUI_H
#define NV_NOTASUI_H
#include "extras.h"
#include "notasfontes.h"
#include "gfx.h"

// --- linha do titulo ---------------------------------------------------------
typedef struct {
  int   n;                       // itens que ficaram
  int   fonte[EX_NFONTES];
  int   cru[EX_NFONTES];
  float larg[EX_NFONTES];        // largura do item, SEM o vao que o precede
} NotasPlano;

// Escolhe as fontes (ajustes_nota_titulo E nota > 0), na ordem fixa da linha, e
// tira as de menor prioridade ate caber em `disp` (largura livre a partir do
// ponto em que a linha comeca). `leadPrimeiro` e o que precede o PRIMEIRO item
// (o ponto separador, ou 0 no comeco da linha); os demais levam 24 px.
// `cru[f]` e o extras_nota(f) — 0 = sem nota. `querer` (opcional, EX_NFONTES
// bytes) substitui ajustes_nota_titulo: serve ao teste.
void  notasui_planejar(NotasPlano *p, const int cru[EX_NFONTES], float disp,
                       float leadPrimeiro, const unsigned char *querer);
// Desenha o plano com o centro vertical em `yc`. Devolve o x onde parou.
float notasui_desenhar_linha(const NotasPlano *p, float x, float yc, float a);
// 1 = o Trakt vai pelo logotipo (check vermelho) e o Metacritic pelo logo (vale ate desligar).
void notasui_modo_icone(int ligado);
// O mesmo com outro vao entre as notas (o painel de pausa respira mais).
float notasui_desenhar_linha_vao(const NotasPlano *p, float x, float yc, float vao, float a);
// Largura do plano com esse vao.
float notasui_plano_largura(const NotasPlano *p, float vao);

// --- secao "Notas" -------------------------------------------------------------
typedef struct {
  int cru[EX_NFONTES];           // extras_nota(f) (IMDb ja com a reserva do catalogo)
  // Notas por episodio (serie). nTemp 0 = sem grade. Decimos: 72 = 7.2.
  int nTemp;
  int (*tempNum)(int t);
  int (*nEps)(int t);
  int (*epNum)(int t, int i);
  int (*epNota)(int t, int i);
  int notaPessoal;              // Half-star value, 0 when unrated.
} NotasSecao;

// A secao (Glass UI 1.8): cartao de nota a esquerda (media, critica x publico,
// menor/maior) e uma grade de DUAS colunas com um bloco por fonte. Cada bloco
// e so desenho: o foco para no cartao da media (o numero grande).
int   notasui_fontes_tem(const NotasSecao *s);
int   notasui_fontes_n(const NotasSecao *s);      // blocos de fonte (desenho)
// Altura do CONTEUDO (sem o cabecalho que detail.c desenha). Barata: nao
// rasteriza nada.
float notasui_fontes_altura(const NotasSecao *s);
// Desenha com o canto superior esquerdo em (x, y); `y` pode estar fora da tela
// (nao custa nada). `foco[0]` e a mola de foco 0..1 do cartao da media (NULL = nenhum).
// Devolve a altura ocupada.
float notasui_fontes_desenhar(const NotasSecao *s, float x, float y, float a,
                              const float *foco);

// Cartao "Notas por episodio" do bloco "Numeros da temporada" (serie):
// temporadas x episodios dentro de `card`; (selT, selI) e o episodio em foco
// no bloco (indice da temporada em `s`, indice do episodio), -1 = nenhum.
void  notasui_mapa_card(const NotasSecao *s, GfxRect card, int selT, int selI, float a);

// ESCALA DE COR comum as notas e aos graficos: laranja < 70, amarelo 70..82,
// verde > 82 (0..100). A rampa e a mesma paleta, continua, para nota de
// episodio em decimos (6,0 laranja .. 9,0+ menta).
void  notasui_cor_faixa(int n100, float *r, float *g, float *b);
void  notasui_cor_rampa(int decimos, float *r, float *g, float *b);
// Material dos cartoes (vidro ou solido, conforme Ajustes), o mesmo dos blocos.
void  notasui_painel(GfxRect r, float raio, float a);
// Volta o portao do texto (nova pagina de titulo).
void  notasui_reiniciar(void);

// O LOGO DA FONTE, sozinho (PNG de art/marcas): a esquerda em `x`, centrado em
// `yc`, com `h` de altura. Devolve a largura; 0 quando a fonte nao tem PNG.
// notasui_marca_largura mede sem desenhar — a mesma largura antes e depois de
// a textura carregar, para o selo nao mudar de tamanho no segundo quadro.
float notasui_marca_largura(int fonte, float h);
float notasui_marca(int fonte, float x, float yc, float h, float a);

// Uma marca sozinha (para os cartoes da aba de notas): centralizada em (xc, yc)
// dentro de uma caixa `h` de altura. Devolve a largura usada.
float notasui_marca_cartao(int fonte, int cru, float xc, float yc, float h, float a);

#endif
