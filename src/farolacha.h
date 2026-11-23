// DESCOBERTA LOCAL DO FAROL (PLANO secao 2.9, issue 24).
//
// Fase 1 da descoberta ("Achar o que ver"), local e em todas as TVs: quatro
// intencoes de 1 clique sobre o catalogo que ja esta em memoria (cat_item),
// cruzando duracao (formatos de descoberta.c:379), genero, visto
// (cacavisto_filme) e retomada na memoria do item (catalogo.h:452).
//
// MODULO PURO: sem SDL, sem rede, sem dados.h, sem malloc, sem log e SEM
// ESTADO ESTATICO. Nenhum ponteiro de cat_item() nem indice fica guardado
// aqui de um quadro para o outro: cada chamada valida cat_revisao()
// (catalogo.h:636) do inicio ao fim da varredura, e farolacha_item() recusa
// qualquer FarolAchado assim que cat_revisao() muda.
#ifndef NV_FAROLACHA_H
#define NV_FAROLACHA_H

#include <stddef.h>
#include "catalogo.h"

// Teto padrao de FA_CURTO quando maxMin <= 0: 1h40 (100 min, secao 2.9).
#define FAROL_ACHA_CURTO_PADRAO 100

typedef enum { FA_CURTO, FA_NAO_VI, FA_RETOMAR, FA_SURPRESA } FarolAchaTipo;
typedef struct { int indice; int minutos; int motivo; } FarolAchado;   // indice em cat_item()

// Bits de FarolAchado.motivo: a tela so afirma na 2a linha o que o filtro
// garante em todos os itens devolvidos (ex.: FT_ACHA_L2_NAO_VISTO so quando
// todos trazem FA_MOT_NAO_VI).
enum {
  FA_MOT_NENHUM   = 0,
  FA_MOT_CURTO    = 1 << 0, // duracao conhecida <= limite pedido
  FA_MOT_NAO_VI   = 1 << 1, // nao visto (cacavisto_filme) nem iniciado
  FA_MOT_RETOMAR  = 1 << 2, // em andamento na memoria do item
  FA_MOT_SURPRESA = 1 << 3, // sorteio deterministico pela semente
  FA_MOT_SALVO    = 1 << 4, // marcado na lista do usuario (naLista)
  FA_MOT_NOTA     = 1 << 5  // bem avaliado no catalogo (nota >= 75)
};

// Lista ate `max` obras distintas do catalogo atual para a intencao `t`.
// Deterministico para o mesmo catalogo e a mesma `semente`; devolve quantos
// itens escreveu (0..max; se houver menos de 3 elegiveis, devolve o que tem).
int farolacha_listar(FarolAchaTipo t, int maxMin, FarolAchado *out, int max,
                     unsigned semente);

// Mesmo contrato de farolacha_listar, devolvendo tambem em `revisaoOut` (se
// nao NULL) o cat_revisao() sob o qual os indices de `out` valem, e podendo
// restringir a um `genero` (NULL ou "" = qualquer genero).
int farolacha_listar_rev(FarolAchaTipo t, int maxMin, FarolAchado *out, int max,
                         unsigned semente, unsigned *revisaoOut);
int farolacha_listar_genero(FarolAchaTipo t, int maxMin, const char *genero,
                            FarolAchado *out, int max, unsigned semente,
                            unsigned *revisaoOut);

// Resolve o item de `a` SOMENTE enquanto cat_revisao() == revisao. Mudou a
// revisao do catalogo, devolve NULL: quem desenha nao reaproveita indice
// velho entre quadros e rechama farolacha_listar_rev com a mesma semente.
const CatItem *farolacha_item(const FarolAchado *a, unsigned revisao);

// Duracao em minutos extraida de CatItem.meta ("2024 · 95 min", "1h 40min",
// "1h40", "2h", "45min", "90m", "PT1H35M"). Ano solto ("2024"), temporadas
// ("3 temporadas") ou texto sem unidade explicita devolvem 0.
int farolacha_minutos_meta(const char *meta);

// 1 se todos os `n` achados (validos em `revisao`) compartilham um genero em
// CatItem.genero (descartando rotulos de tipo como "Filme"/"Programa de TV"),
// copiando o nome em `dst`. 0 caso contrario (dst fica "").
int farolacha_genero_comum(const FarolAchado *achados, int n, unsigned revisao,
                           char *dst, size_t tam);

// Chave FT_INTENCAO_* (src/faroltexto.h) da intencao `t`, ou "" se invalida.
const char *farolacha_rotulo(FarolAchaTipo t);

#endif
