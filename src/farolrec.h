// MOTOR DE RECOMENDACAO DO FAROL — pontuar, diversificar, frio e motivos
// (lote sense, issue 41; secao 3 inteira do PLANO-GOSTO).
//
// Puro como os irmaos (farolgosto, farolficha, farolinfo): sem SDL, sem rede,
// sem malloc, sem dados.h e sem relogio proprio — `dia` e `faixaHora` chegam
// injetados no contexto. Quem chama (o compositor da issue 44) ja tem os
// candidatos nas fontes da tabela 3.1 — catalogo, recs das sementes, proxima
// obra de pessoa, salvos/watchlist, amigos, recs recebidas e clima — e resume
// cada um num FarolRecCand de 48 bytes. O que so o app sabe (visto, abandono,
// classificacao do perfil, "ja esta no Continuar", imdb que abre fora do
// catalogo) entra por flags: o modulo nao consulta estado nenhum por fora.
//
// DETERMINISMO. Mesma entrada + mesma `ctx.semente` = mesma saida, bit a bit:
// a semente so entra no desempate (hash da chave com a semente) — a ordem dos
// pontos e as regras duras nao dependem dela. Nada de rand(), time() ou
// ponteiro decidindo ordem.
//
// PONTOS (3.2). Inteiros em ponto fixo: os termos sao medias dos pesos do
// modelo (Q8.8 do farolgosto, ja decaidos em `dia`) cortados em [-1, +1] e
// pesados em Q15 —
//   0,30*genero + 0,25*tema + 0,20*pessoa + 0,05*decada + 0,05*idioma
//   + 0,10*hora[agora][duracao] + 0,05*qualidade(bayes 200) + bonus
//   - 0,20*(mostrado em >= 3 dias).
// Qualidade usa media bayesiana com prior 6,8 e 200 votos, normalizada para
// [-1, +1]; sem nota e sem votos o termo vale 0. O bonus de F_AMIGO so e
// aplicado com gostoPct >= 70, como manda a fonte da tabela 3.1.
//
// DIVERSIDADE (3.3). Selecao gulosa MMR com lambda = 0,7 sobre os pontos:
// cada escolhido rebaixa quem divide semente, franquia, genero principal ou
// pessoa. Regras duras por fileira de 12: no maximo 2 da mesma semente, 3 do
// mesmo genero principal entre os 6 primeiros e 1 por franquia — sem excecao,
// nem para fechar a fileira. Uma vaga "fora da curva" (posicoes 5-8) vai para
// o melhor candidato cujo genero principal tem n < 3 evidencias no modelo,
// marcado com FMO_FORA_CURVA — exploracao dita na cara, nao ruido.
//
// FRIO (3.4). farolrec_nivel le o contador `titulos` do modelo: < 3 fichas =
// nivel 0 (escolher devolve 0 itens — nenhuma fileira); 3-9 = nivel 1 (so
// candidatos vindos das recs do TMDB das sementes, pontos so por genero +
// bonus, sem motivos "voce gosta de" e sem fora da curva); >= 10 = nivel 2
// (tudo).
//
// Nenhum titulo, nome ou gosto em log: o modulo nao loga nada. Os motivos
// saem ESTRUTURADOS (tipo + ids); a frase e montada depois pela camada de
// texto — FMO_* nao carrega string.
#ifndef NV_FAROLREC_H
#define NV_FAROLREC_H

#include <stdint.h>
#include "farolgosto.h"          // FarolGosto, farolgosto_peso/recusado/...
#include "farolficha.h"          // FarolFicha (a ficha da issue 36)

#define FAROL_REC_CAND_MAX 400    // teto de candidatos por chamada (3.1)
#define FAROL_REC_LINHA    12     // fileira padrao das regras duras (3.3)
#define FAROL_REC_MOTIVOS   2     // motivos por escolhido (3.5)
#define FRC_SEM_SEMENTE 0xffff    // candidato sem semente apontada

// Fontes da tabela 3.1 — MASCARA: um titulo que chega por dois caminhos marca
// os dois bits e soma os dois bonus (quem chama funde duplicatas pela chave).
enum {
  FRC_F_CATALOGO = 0x01,   // catalogos dos add-ons em memoria — sem bonus
  FRC_F_SEMENTE  = 0x02,   // recs do TMDB das sementes gostadas  — +0,10
  FRC_F_PESSOA   = 0x04,   // proxima obra de pessoa seguida      — +0,10
  FRC_F_SALVO    = 0x08,   // salvos / watchlist Trakt nao vistos — +0,15
  FRC_F_AMIGO    = 0x10,   // amigo com gosto parecido — +0,10 so se
                           // gostoPct >= 70 (conferido pelo motor)
  FRC_F_REC      = 0x20,   // recomendacao recebida e nao vista   — +0,20
  FRC_F_CLIMA    = 0x40    // clima mais afim                     — sem bonus
};

// Estado que so o app conhece — mascara de FRC_FL_*. O que sai antes de
// pontuar (3.1): VISTO, FORA_CLASS, CONTINUAR (so na Home), FORA_CAT sem
// TEM_IMDB, recusado no anel do modelo e abandono ha < 90 dias. As flags
// sao UNIDAS por chave antes dos filtros: duplicatas divergentes nao
// escapam pela copia "limpa" — um titulo VISTO por uma via e VISTO em
// todas (mesmo assim, quem chama deve fundir as duplicatas — inclusive
// `fontes`, que aqui nao se mescla: o bonus sai da copia que ficou).
// Bits fora do enum (0x40 e 0x80 hoje) sao do chamador: o motor os carrega
// na uniao por chave mas nunca os interpreta — nao sao reservados.
enum {
  FRC_FL_VISTO      = 0x01,  // cat_visto / Diario: nunca sai
  FRC_FL_FORA_CLASS = 0x02,  // fora da classificacao do perfil
  FRC_FL_CONTINUAR  = 0x04,  // ja esta no "Continuar" (regra so da Home)
  FRC_FL_FORA_CAT   = 0x08,  // fora do catalogo: so entra com TEM_IMDB
  FRC_FL_TEM_IMDB   = 0x10,  // abre pelo contrato de busca_pediu_abrir
  FRC_FL_EM_REC     = 0x20   // consta nas recomendacoes da semente apontada
};

// Um candidato, 48 bytes cravados (veto estatico de 400 = 19 KB da secao 3.1).
typedef struct {
  const FarolFicha *ficha;  // ficha da 36; NULL = sem ficha (so bonus/conta)
  int64_t  chave;           // identidade estavel p/ dedup (farolficha_chave
                            // ou id do item); 0 = nunca iguala outro
  uint32_t h;               // hash do titulo p/ aneis recusado/mostrado;
                            // 0 = nao consulta os aneis
  int32_t  franquia;        // id da colecao/franquia; 0 = sem franquia
  int32_t  quem;            // id de quem indicou (F_AMIGO/F_REC) — vira o
                            // argumento do motivo; 0 = anonimo
  uint16_t semente;         // indice em ctx->sementes ou FRC_SEM_SEMENTE
  uint16_t abandonoDia;     // dia do ultimo abandono cedo; 0 = nunca
  uint8_t  fontes;          // mascara FRC_F_*
  uint8_t  gostoPct;        // afinidade do amigo (0..100); F_AMIGO exige >= 70
  uint8_t  flags;           // mascara FRC_FL_*
  uint8_t  livre[13];
} FarolRecCand;             // 48 bytes cravados (vetor de 400 = 19 KB)

// Semente que a pessoa ja viu, para a regra "mesma semente" da diversidade e
// o motivo FMO_TERMINOU (3.5): `terminou` = a semente tem entrada no Diario
// sem DIARIO_F_PARCIAL. A ficha serve para "divide 2+ afinidades com ela".
typedef struct {
  const FarolFicha *ficha;  // NULL = nao conta afinidades (so a regra de rec)
  uint8_t terminou;
} FarolRecSemente;

// Tudo que a chamada precisa, injetado — inclusive o relogio.
typedef struct {
  const FarolGosto *g;              // modelo do perfil ativo (obrigatorio)
  const FarolRecSemente *sementes;  // sementes vistas; cand->semente indexa
  int nSementes;
  uint16_t dia;         // hoje, dias desde 1/1/2026 — o mesmo dia do modelo
  uint8_t faixaHora;    // faixa do dia agora (0..FG_HORA_F-1)
  uint8_t naHome;       // 1 = fileira da Home: aplica a saida do Continuar
  uint32_t semente;     // semente do sorteio (desempate deterministico)
} FarolRecCtx;

// Motivos estruturados (3.5). a/b: indice de titulo (semente em ctx->sementes)
// ou id de pessoa/tema/genero, conforme o tipo. A frase NAO mora aqui.
// CABE_HOJE so sai quando o modelo de hora nao e contra aquela duracao na
// faixa de agora (hora[agora][dur] >= 0): "cabe hoje" de um filme de 3 h
// as 23h seria enganoso.
typedef enum { FMO_TERMINOU, FMO_GENERO, FMO_PESSOA, FMO_TEMA, FMO_AMIGO,
               FMO_SALVO, FMO_CABE_HOJE, FMO_REVER, FMO_FORA_CURVA,
               FMO_RECOMENDADO } FarolMotivoTipo;
typedef struct { uint8_t tipo; int32_t a, b; } FarolMotivo;

// Um escolhido: `cand` indexa o vetor de entrada, `pontos` e a nota Q15 ja
// com o rebaixe de diversidade, `bruto` a nota antes dele. motivos em ordem
// de forca; nMotivo = 0 e valido — a peca sai sem linha de motivo (3.5).
typedef struct {
  int32_t cand;
  int32_t pontos;
  int32_t bruto;
  uint8_t nMotivo;
  uint8_t foraCurva;                  // 1 = a vaga de exploracao (3.3)
  uint8_t livre[2];
  FarolMotivo motivo[FAROL_REC_MOTIVOS];
} FarolRecItem;

// Nivel de frio pelo contador de obras com ficha do modelo (3.4):
// < 3 -> 0, 3-9 -> 1, >= 10 -> 2.
int farolrec_nivel(const FarolGosto *g);

// Filtra (3.1), pontua (3.2) e escolhe ate `maxOut` com diversidade (3.3) e
// motivos (3.5). Devolve o n de itens escritos em `out` (0 no nivel 0 ou sem
// elegivel). maxOut e a fileira pedida — 12 cobre a regra dura; acima disso
// as regras continuam valendo por posicao. Candidatos acima de 400 sao
// ignorados (o vetor de entrada e o teto, nao o de saida). Sem efeito
// colateral: so escreve em `out`.
int farolrec_escolher(const FarolRecCtx *ctx,
                      const FarolRecCand *cand, int nCand,
                      FarolRecItem *out, int maxOut);

#endif // NV_FAROLREC_H
