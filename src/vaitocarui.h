// DESENHO da faixa "Vai tocar" e dos chips A/R/C do card de episodio (2.1, tela
// 4 do mockup design/2.1/titulo.html). Os DADOS vem de vaitocar.h (puro); aqui
// so se mede e se pinta. Separado para o teste de captura desenhar a mesma
// faixa que o detail.c, sem arrastar o detail.c inteiro.
#ifndef NV_VAITOCARUI_H
#define NV_VAITOCARUI_H
#include "gfx.h"
#include "vaitocar.h"

// Altura da faixa (0 = nao ha faixa: lista de fontes vazia). Nao inclui folga.
float vtui_altura(const VaiTocar *vt);

// Pinta a faixa com o canto superior esquerdo em (x, y) e `larg` de largura.
void vtui_desenhar(const VaiTocar *vt, float x, float y, float larg, float a);

// Chips A/R/C sob a meta do card do episodio. `marcas` = bits VT_T_* conhecidos
// (0 = nada em cache: nao pinta chip nenhum). Chip cheio = marca existe;
// tracejado = o episodio e conhecido mas nao tem aquela marca. Devolve a
// largura pintada (0 = nada). x < 0 so mede.
float vtui_chips_episodio(int marcas, float x, float y, float a);
#define VTUI_CHIP_H 20.0f

#endif
