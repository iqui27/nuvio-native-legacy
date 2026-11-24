// CLIENTE DA BUSCA POR FRASE DO FAROL (ADENDO-2.1.md #30, PLANO.md §3.3).
//
// `farolbusca_pedir(frase)` consulta POST /v1/farol/busca em fio proprio,
// cancelavel e com prazo rigido de 6 s. Quando a rede falha, estoura os 6 s,
// responde 429, devolve erro ou lista vazia, cai automaticamente na intencao
// local mais proxima (`farolacha`) com a frase da §3.3 (FT_BUSCA_FRASE_*).
//
// PRIVACIDADE:
//   - Perfil privado (`privado == 1`): envia APENAS {"frase":"..."}.
//   - Sem consentimento ou infantil: envia "consentiu":0 explicito e nada de
//     gosto/historico (ressalva da revisao farol-29).
//   - Nunca registra frase, token ou URL em log nem no anel.
//
// ESTABILIDADE DE INDICES (`cat_revisao`):
//   - Nenhum indice de `cat_item()` e guardado entre quadros: o modulo guarda
//     apenas identidades estaveis (`imdb`/`tmdb`/`tipo`) ou os parametros da
//     intencao local, e `farolbusca_resolver()` re-resolve contra o catalogo
//     vivo sob `cat_revisao()`.
//   - `farolbusca_item(it, revisao)` recusa qualquer item assim que
//     `cat_revisao()` muda.
//   - Contrato de abertura (`busca_pediu_abrir`, busca.h:27): titulo presente
//     no catalogo abre pelo indice vivo; fora do catalogo entra como cartaz
//     pelo id (`cat_acrescentar` com arte Metahub).
#ifndef NV_FAROLBUSCA_H
#define NV_FAROLBUSCA_H

#include <stddef.h>
#include <stdint.h>
#include "catalogo.h"
#include "farolbuscaparse.h"

#define FAROL_BUSCA_PRAZO_MS 6000u

typedef enum {
  FB_OCIOSO = 0,
  FB_BUSCANDO,
  FB_PRONTO,
  FB_CANCELADO
} FarolBuscaEstado;

typedef struct {
  FarolBuscaEstado estado;
  FarolBuscaMotivo motivo;
  FarolBuscaOrigem origem;
  int              http;         // codigo HTTP (0 se transporte/prazo/local)
  int              llm_status;   // llm_status da resposta remota (0 se local)
  int              usouLocal;    // 1 se caiu na intencao local (farolacha)
  FarolBuscaLocal  local;        // intencao local mais proxima da frase
  FarolBuscaFiltro filtro;       // filtro usado no servidor (quando remoto)
  const char      *fraseEstado;  // chave FT_* de faroltexto.h (ou "")
  uint64_t         geracao;      // sobe a cada pedido novo
} FarolBuscaInfo;

// Um item resolvido CONTRA A REVISAO ATUAL do catalogo (`revisaoOut` de
// farolbusca_resolver). Nunca guarde `indice` de um quadro para o outro.
typedef struct {
  int  noCatalogo;                  // 1 = indice em cat_item() valido sob revisao; 0 = cartaz pelo id
  int  indice;                      // >= 0 se noCatalogo; -1 se fora do catalogo
  long tmdb;                        // id TMDB (> 0 se conhecido)
  char imdb[64];                    // "tt..." ou "tmdb:<n>"
  char tipo[8];                     // "movie" | "series"
  char poster[NV_ARTE_URL_MAX];     // URL do cartaz (catalogo ou Metahub pelo id)
  int  minutos;                     // duracao (quando veio do farolacha), 0 senao
  int  motivoAcha;                  // bits FA_MOT_* (quando veio do farolacha)
} FarolBuscaItem;

// Inicia busca por frase em fio proprio (prazo 6 s, cancelavel). Um novo
// pedido invalida qualquer pedido anterior em voo. Devolve 1 se aceitou a
// frase, 0 se a frase for vazia/invalida.
int farolbusca_pedir(const char *frase);
int farolbusca_pedir_com(const char *frase, const FarolBuscaCtx *ctx);

// Cancela o pedido em voo sem bloquear a UI. O fio encerra em < 300 ms e
// publica FB_CANCELADO (salvo se um novo pedido ja tiver sido iniciado).
void farolbusca_cancelar(void);

// Cancela qualquer pedido em voo e espera todos os fios encerrarem (saida/testes).
void farolbusca_encerrar(void);

// Le o estado atual (copia sob trava; nunca bloqueia em rede).
FarolBuscaEstado farolbusca_estado(FarolBuscaInfo *info);

// Resolve os itens da resposta pronta contra o catalogo VIVO neste quadro,
// devolvendo quantos escreveu (0..max) e carimbando `revisaoOut` com
// `cat_revisao()`. Nenhum indice fica guardado entre quadros.
int farolbusca_resolver(FarolBuscaItem *out, int max, unsigned *revisaoOut);

// Devolve o CatItem de `it` SOMENTE enquanto `cat_revisao() == revisao`.
// Mudou a revisao do catalogo (ou `it` esta fora do catalogo), devolve NULL.
const CatItem *farolbusca_item(const FarolBuscaItem *it, unsigned revisao);

// Resolve `it` para um indice vivo de `cat_item()` no momento da abertura:
// se o titulo esta no catalogo, devolve o indice vivo; senao acrescenta o
// cartaz minimo pelo id (`cat_acrescentar`) e devolve o indice novo (-1 em falha).
int farolbusca_abrir_item(const FarolBuscaItem *it);

// Contrato de abertura igual a `busca_pediu_abrir` (busca.h:27):
// `farolbusca_selecionar` registra a identidade estavel do cartaz escolhido
// (nunca um indice velho), e `farolbusca_pediu_abrir` consome o pedido UMA vez
// resolvendo o indice vivo (ou acrescentando o cartaz pelo id).
int farolbusca_selecionar(const FarolBuscaItem *it);
int farolbusca_pediu_abrir(int *indiceCatalogo);

// Preferencias de privacidade / consentimento usadas por farolbusca_pedir().
// Consentimento nasce 0 (seguro por padrao: manda consentiu:0 explicito).
void farolbusca_definir_privado(int privado);
int  farolbusca_privado(void);
void farolbusca_definir_consentimento(int consentiu);
int  farolbusca_consentimento(void);

// Overrides de teste / integracao (NULL volta ao padrao do app).
void farolbusca_definir_url(const char *baseUrl);
void farolbusca_definir_auth(const char *autorizacao, const char *via);
void farolbusca_definir_semente(unsigned semente);
int  farolbusca_em_voo(void);

#endif
