// MEDIDOR DE DESEMPENHO NA ILHA DO RELOGIO (mockup do registro, quadros 15 e
// 16, aprovado em 03/10; dentro da ilha a pedido do dono, 03/10 a noite). Os
// numeros sao os da linha FPS= que main.c ja escreve a cada 3 s: quadros por
// segundo, o pior quadro da janela, janks, texturas na tela, a fila e os
// despejos de textura, a memoria do app e o cache de imagens. Liga em Ajustes >
// Desempenho desta TV > "Medidor de desempenho" (desligado de fabrica). Custa
// uma amostra a cada 3 s, nao por quadro.
//
// NAO E MAIS UMA CAMADA SOLTA: era uma ilha propria no canto oposto ao relogio
// e cobria as animacoes da ilha. Agora ele e CONTEUDO da ilha do relogio —
// ilha.c fora do player, plrilha.c dentro dele — no material dela:
//   Minimo  na MESMA linha da hora: fio, ponto, "60 fps", pior quadro e RAM;
//   Menor   a linha da hora com o fps, e uma segunda linha compacta embaixo;
//   Grande  a ilha cresce abaixo da hora com o painel inteiro (grafico do pior
//           quadro, memoria, texturas, cache).
// Quem desenha a ilha decide quando ha lugar: com aviso, atividade, modal,
// voo na ilha ou o trailer tocando (destaque da home, pagina do titulo) o
// medidor some (Minimo ao lado de um cartao, se couber) e volta quando ela
// fica livre. Abaixo de 45 fps (o corte de [gpu-modos] lento) o
// ponto e o numero ficam ambar — e a unica cor.
#ifndef NV_DESEMPENHO_H
#define NV_DESEMPENHO_H
#include <SDL2/SDL.h>
#include "gfx.h"

// O indice e o de Ajustes (V_MEDIDOR, ajustes_medidor_desempenho).
enum { DS_DESLIGADO = 0, DS_MINIMO, DS_MENOR, DS_GRANDE };

void desempenho_amostra(float fps, float piorMs, int janks, int texTela, float texTelaMb,
                        int filaTex, int despejos, int despejosTela, float rssMb);
// A forma escolhida em Ajustes; DS_DESLIGADO enquanto nao ha amostra.
int   desempenho_forma(void);
// Com um trailer tocando (destaque da Home ou pagina do titulo; main.c manda a
// cada quadro) o medidor some: a forma vira DS_DESLIGADO ate o trailer parar.
void  desempenho_trailer(int tocando);
// O TRECHO NA LINHA DA HORA, logo depois dela: largura (com o vao e o fio da
// frente; 0 = nada) e desenho, com `x` na ponta direita da hora e `yc` o meio
// da linha. Em Menor/Grande e so o ponto e o fps.
float desempenho_linha_w(int forma);
void  desempenho_linha(int forma, float x, float yc, float a);
// O CORPO, abaixo da linha da hora (Menor e Grande; 0 x 0 em Minimo). `r` e a
// area inteira abaixo da linha, na largura da ilha.
void  desempenho_corpo_tam(int forma, float *w, float *h);
void  desempenho_corpo(int forma, GfxRect r, float a);

#ifdef DESEMPENHO_TESTE
void desempenho_teste_serie(const float *pior, int n);
// Forma fixa para capturas (-1 = a de Ajustes).
void desempenho_teste_forma(int forma);
#endif
#endif
