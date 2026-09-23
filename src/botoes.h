// A TABELA UNICA DE BOTOES (dono, 21/09/2026: "botoes e badge — cada
// detalhe"). Ate aqui cada modal desenhava a sua pilula com numeros proprios:
// 64 px em avisos.c e atualizacao.c, 86 em ctxmenu.c, 72 em detail.c, 76 no
// painel de Salvos; repouso em 0.176, 0.20 ou 0.133 conforme o arquivo. O
// registro de PRODUTO pede um vocabulario so — se o "salvar" tem duas caras,
// uma esta errada. Esta e a cara.
//
//   PRIMARIO    72 px de altura (a medida do Play em detail.c), raio pilula
//               (0,5), TXT_DET_BOTAO 25/500. Repouso: superficie 0.14/0.15/
//               0.17 (o cinza tingido do painel, um degrau acima dele) com
//               texto 235. Foco: cor de realce, tinta por ajustes_acento_tinta
//               (branca salvo realce claro) e o brilho difuso GFX_SOMBRA 0,35
//               por tras — a mesma luz da pilula do menu lateral.
//   SECUNDARIO  56 px, mesmo raio e mesmo corpo de texto. Repouso: SEM miolo,
//               contorno de 1,5 px a 22% de branco, texto 235. Foco: igual ao
//               primario. O secundario e mais baixo E mais leve de proposito:
//               a hierarquia vem de escala e peso, nao de uma segunda cor.
//   DISCO       botao redondo so com icone (os tres da tela de titulo):
//               repouso 0.14/0.15/0.17, foco realce + tinta + brilho.
//
// `foco` e a MOLA (0..1), nao um booleano: o fundo interpola, o texto troca em
// DEGRAU em 0,5 — a cor faz parte da chave do cache de text.c e uma cor por
// quadro rasterizaria uma textura nova a cada frame (ver ctxmenu.c).
//
// O icone e um nome de deploy/app/art/icones (gfx_icone): "mais", "visto",
// "avancar", "recomendar", ... NULL = sem icone. Ele vai a ESQUERDA do rotulo
// e o par (icone + rotulo) fica centrado na pilula.
#ifndef NV_BOTOES_H
#define NV_BOTOES_H
#include "gfx.h"

#define BOTAO_H_PRIMARIO   72.0f
#define BOTAO_H_SECUNDARIO 56.0f
#define BOTAO_PAD_X        36.0f   // folga lateral do primario
#define BOTAO_PAD_X2       28.0f   // do secundario (mais baixo, menos ar)
#define BOTAO_ICONE        26.0f   // lado do icone dentro da pilula
#define BOTAO_ICONE_GAP    14.0f   // do icone ate o rotulo
#define BOTAO_GAP          16.0f   // entre botoes lado a lado

// Largura que botao_pilula vai ocupar, para quem alinha pela direita ou
// empilha varios: e a mesma conta, entao nunca desencontra do desenho.
float botao_largura(const char *rotulo, const char *icone, int primario);

// Desenha a pilula em `r` (a altura vem de quem chama, para o rect ser o
// mesmo do teste de foco; use BOTAO_H_*). `alinhar`: 0 centra o par no
// botao, 1 encosta a esquerda com BOTAO_PAD_X (linhas de lista, como o menu
// do cartaz, onde os rotulos tem de alinhar entre si).
void botao_pilula(GfxRect r, const char *rotulo, const char *icone,
                  float foco, int primario, int alinhar, float a);

// So a SUPERFICIE do primario (luz + preenchimento repouso->realce), para uma
// linha que leva mais que icone e rotulo — a do amigo, com a foto dele. Devolve
// a cor do texto em 0..255 para ESSE foco: a tinta do realce a partir de 0,5,
// o 235 de repouso antes. Assim a linha custom e a pilula nunca divergem.
int  botao_superficie(GfxRect r, float foco, float a);

// Botao redondo so com icone. `r` e o quadrado do disco.
void botao_disco(GfxRect r, const char *icone, float foco, float a);

// So a luz difusa por tras de um botao em foco, para quem ja tem a pilula
// desenhada de outro jeito (o Play de detail.c, que cresce no foco).
void botao_luz(GfxRect r, float foco, float a);

#endif
