// "O QUE EU PERDI?" (pesquisa de mercado 4.3): resumo sem spoiler ate o minuto
// atual, feito do TEXTO DA LEGENDA que o player ja tem carregada.
//
// Este modulo e a REGRA PURA (sem SDL, sem rede, sem fio): corta a legenda no
// instante atual, monta o pedido do Gemini e le a resposta. tests/perdi.sh
// cobre os tres. Quem fala com a rede e perdifio.c; quem desenha e player.c.
//
// DECISOES DO DONO: o modelo e o Gemini, com a chave DO PROPRIO USUARIO,
// digitada em Ajustes. A chave nao faz parte de nada que este modulo monta
// (nem url, nem corpo): vai so no cabecalho x-goog-api-key, direto ao Google.
// Nenhum servidor nosso entra no caminho.
//
// SEM SPOILER, em duas camadas: (1) so entra no pedido fala que JA COMECOU no
// instante atual, entao o modelo nao recebe o que vem depois; (2) a instrucao
// proibe usar conhecimento previo do titulo. A (1) e garantida por teste; a
// (2) depende do modelo e nao e garantia.
#ifndef NV_PERDI_H
#define NV_PERDI_H
#include "legenda.h"
#include <stddef.h>

// Alias mantido pelo Google para o Flash corrente (generativelanguage v1beta).
#define PERDI_MODELO "gemini-flash-latest"
#define PERDI_URL "https://generativelanguage.googleapis.com/v1beta/models/" PERDI_MODELO ":generateContent"
// Teto do texto enviado. Um filme de 2 h tem ~60-100 KiB de falas; acima do
// teto cai o COMECO (o que a pessoa perdeu e o trecho recente).
#define PERDI_TRECHO_MAX (160u * 1024u)
#define PERDI_RESUMO_MAX 1400
// Menos falas que isto nao dao resumo: o cartao avisa sem gastar a cota.
#define PERDI_FALAS_MIN 4

typedef enum {
  PERDI_OK = 0,
  PERDI_CHAVE,      // chave recusada (400 API key / 401 / 403)
  PERDI_COTA,       // 429
  PERDI_BLOQUEADO,  // o modelo recusou (safety/recitation)
  PERDI_REDE,       // sem resposta
  PERDI_ERRO        // qualquer outra coisa
} PerdiResultado;

// Chave digitada -> so [A-Za-z0-9_-] (o formato das chaves do Google). Assim
// uma linha torta nunca vira um segundo cabecalho. Devolve o tamanho.
size_t perdi_chave_limpar(const char *txt, char *dst, size_t tam);

// As falas que comecaram ate `posSeg` (+ atraso da legenda), uma por linha,
// com marcas [h:mm:ss] a cada minuto. malloc; o chamador libera. `falas`
// recebe quantas entraram. NULL quando nenhuma. `v` em ordem de tempo.
char *perdi_trecho(const LegendaCue *v, int n, double posSeg, int atrasoMs,
                   size_t maxBytes, int *falas);

// Corpo JSON do generateContent. `episodio` pode ser NULL/"". `idioma` e a
// tag da interface (pt-BR, en...). malloc; o chamador libera.
char *perdi_pedido(const char *titulo, const char *episodio, double posSeg,
                   const char *idioma, const char *trecho);

// Le a resposta. PERDI_OK poe o resumo limpo (sem markdown) em dst.
PerdiResultado perdi_resposta(int http, const char *corpo, char *dst, size_t tam);

#endif
