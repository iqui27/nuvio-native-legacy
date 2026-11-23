// PARSER E PROTOCOLO PURO DA BUSCA POR FRASE DO FAROL (ADENDO-2.1.md #30,
// PLANO.md §3.3, contrato de servidor/recomendacoes/src/farol-busca.js #29).
//
// MODULO PURO: sem SDL, sem rede, sem pthread, sem dados.h, sem malloc, sem
// log e sem estado estatico mutavel.
//
// Responsabilidades:
//   1. Normalizar e validar a frase do usuario (<= 200 codepoints UTF-8, sem
//      controles proibidos, sem URL).
//   2. Montar o corpo JSON de POST /v1/farol/busca respeitando estritamente a
//      lista fechada de 8 chaves do servidor (chave extra -> 400):
//        - perfil privado (privado != 0): envia APENAS {"frase":"..."}.
//        - sem consentimento ou infantil: envia "consentiu":0 explicito
//          (ausencia conta como consentido no servidor) e nunca envia
//          generos, historico nem vistos.
//   3. Parsear a resposta do servidor no formato de ADENDO:14 / farol-busca.js
//      ({ok, ids:[{tmdb, imdb, tipo}], filtro, origem, llm_status}).
//   4. Mapear a frase livre para a intencao local mais proxima do farolacha
//      (FA_CURTO / FA_NAO_VI / FA_RETOMAR / FA_SURPRESA + maxMin + genero)
//      quando estiver offline, em 429, erro, prazo estourado ou ids vazio.
//   5. Entregar as frases de estado da §3.3 usando somente FT_* de faroltexto.h.
#ifndef NV_FAROLBUSCAPARSE_H
#define NV_FAROLBUSCAPARSE_H

#include <stddef.h>
#include "farolacha.h"

#define FAROL_BUSCA_FRASE_MAX    200
#define FAROL_BUSCA_FRASE_BYTES  804
#define FAROL_BUSCA_IDS_MAX       12
#define FAROL_BUSCA_GENEROS_MAX   12
#define FAROL_BUSCA_FIL_GEN_MAX    4
#define FAROL_BUSCA_HIST_MAX      50
#define FAROL_BUSCA_CORPO_MAX   8192
#define FAROL_BUSCA_PARECIDO_MAX 128

typedef enum {
  FB_ORIG_NENHUMA = 0,
  FB_ORIG_LLM,
  FB_ORIG_PALAVRAS,
  FB_ORIG_LOCAL
} FarolBuscaOrigem;

typedef enum {
  FB_MOTIVO_NENHUM = 0,
  FB_MOTIVO_BUSCANDO,
  FB_MOTIVO_OK,
  FB_MOTIVO_OFFLINE,
  FB_MOTIVO_PRAZO,
  FB_MOTIVO_LIMITE,
  FB_MOTIVO_ERRO,
  FB_MOTIVO_VAZIO,
  FB_MOTIVO_CANCELADO
} FarolBuscaMotivo;

typedef struct {
  long tmdb;      // > 0 se presente; 0 se ausente
  char imdb[24];  // "tt<digitos>" ou "" se ausente
  char tipo[8];   // "movie" | "series"
} FarolBuscaId;

typedef struct {
  char generos[FAROL_BUSCA_FIL_GEN_MAX][24];
  int  nGeneros;
  int  duracao_max; // 0 = ausente/null, senao 15..600
  int  decada;      // 0 = ausente/null, senao 1900..2030 (multiplo de 10)
  char parecido_com[FAROL_BUSCA_PARECIDO_MAX];
  char tipo[16];    // "filme" | "serie" | "qualquer"
} FarolBuscaFiltro;

typedef struct {
  int              ok;
  int              nIds;
  FarolBuscaId     ids[FAROL_BUSCA_IDS_MAX];
  FarolBuscaFiltro filtro;
  FarolBuscaOrigem origem;
  int              llm_status;
} FarolBuscaResp;

typedef struct {
  int                privado;    // 1 = perfil privado: manda APENAS "frase"
  int                infantil;   // 1 = perfil infantil
  int                consentiu;  // 0 = manda consentiu:0 explicito, sem gosto/historico
  const char        *idioma;     // ex.: "pt-BR", "en-US"; NULL = omite
  const char *const *generos;    int nGeneros;
  const char *const *historico;  int nHistorico;
  const char *const *vistos;     int nVistos;
} FarolBuscaCtx;

typedef struct {
  FarolAchaTipo intencao;   // FA_CURTO | FA_NAO_VI | FA_RETOMAR | FA_SURPRESA
  int           maxMin;     // > 0 para FA_CURTO (ex.: 90, 100); 0 nos demais
  char          genero[32]; // nome canonico PT para farolacha_listar_genero, ou ""
} FarolBuscaLocal;

// Normaliza `src` para `dst` (colapsa espacos, rejeita vazio, controles
// proibidos, UTF-8 invalido e URLs; corta no limite de 200 codepoints UTF-8).
// Devolve 1 se valida, 0 se rejeitada.
int farolbusca_normalizar_frase(const char *src, char *dst, size_t cap);

// Monta o corpo JSON de POST /v1/farol/busca em `dst` (<= FAROL_BUSCA_CORPO_MAX).
// Devolve o comprimento escrito (sem NUL), ou 0 em frase invalida / falta de espaco.
size_t farolbusca_montar_corpo(const char *frase, const FarolBuscaCtx *ctx,
                               char *dst, size_t cap);

// Parseia a resposta JSON de POST /v1/farol/busca (ADENDO:14).
// Devolve 1 se JSON valido com ok==1 e array `ids` presente (mesmo vazio);
// devolve 0 em JSON malformado, truncado ou ok!=1 (zera `out`).
int farolbusca_parsear_resposta(const char *json, size_t len,
                                FarolBuscaResp *out);

// Mapeia a frase livre para a intencao local mais proxima do farolacha.
void farolbusca_intencao_local(const char *frase, FarolBuscaLocal *out);

// Chave FT_* de src/faroltexto.h para o motivo/estado (ou "" sem mensagem).
const char *farolbusca_frase_estado(FarolBuscaMotivo motivo);

#endif
