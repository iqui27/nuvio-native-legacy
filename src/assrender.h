#ifndef NV_ASSRENDER_H
#define NV_ASSRENDER_H

#include <stddef.h>

/*
 * A legenda ASS nao e texto para o renderer de UI. Ela e uma pequena cena:
 * cada evento pode ter varias camadas, desenho, fontes, movimento e efeitos
 * no relogio. Esta interface deixa o player consumir somente imagens RGBA e
 * mantem a escolha da biblioteca (libass no ARM/WASM, fallback no parser
 * antigo) fora da tela de reproducao.
 */

/* Carrega um documento ASS completo. O corpo precisa permanecer em UTF-8 e
 * pode conter qualquer tamanho; o modulo faz a propria copia. */
int  assrender_carregar(const char *corpo, size_t tamanho, unsigned geracao);
void assrender_limpar(void);
void assrender_limpar_fontes(void);

/* Fontes extraidas de attachments Matroska. O nome e apenas informativo para
 * libass; os bytes sao copiados antes do retorno. */
int  assrender_adicionar_fonte(const char *nome, const void *dados, size_t tamanho);

/* Sobrescreve apenas a cor dos glifos quando a pessoa mudou a cor no player.
 * Mantem os bitmaps de contorno/sombra e os tempos de karaoke produzidos pelo
 * libass. enabled=0 preserva todas as cores do arquivo ASS. */
void assrender_definir_cor(int enabled, int r, int g, int b);

/* Executa no thread grafico a limpeza de texturas pendente apos troca de
 * faixa ou seek, mesmo quando a nova faixa nao e ASS. */
void assrender_aplicar_invalidacao(void);

/* Gera os quads do instante atual no contexto GL do player. Retorna o numero
 * de imagens libass desenhadas, inclusive zero quando a faixa esta carregada
 * mas nao ha evento vivo nesse instante. */
int  assrender_desenhar(double posSeg, int atrasoMs, float alpha,
                        float x, float y, float w, float h);

/* 1 quando libass aceitou o documento atual. Sem a biblioteca compilada, o
 * retorno e 0 e legenda.c continua sendo o fallback SRT/VTT/ASS reduzido. */
int  assrender_ativo(void);
/* Teste/diagnostico: renderiza o instante no fio de quem chama, sem GL.
 * Devolve o numero de imagens, ou -1 sem faixa/sem libass. */
int  assrender_quadro_cpu(double posSeg);
const char *assrender_diagnostico(void);

/* Cada troca de faixa/seek invalida resultados antigos antes de o worker
 * publicar o proximo quadro. */
void assrender_geracao(unsigned geracao);

#endif
