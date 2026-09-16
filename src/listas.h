// LISTAS: o terceiro recorte da Biblioteca, ao lado de "Salvos" e "Coleção".
//
// O QUE E UMA LISTA AQUI. Nao e um titulo nem um catalogo de addon: e um
// CONJUNTO NOMEADO que alguem montou — uma lista do Trakt, uma pasta de colecao
// da conta Nuvio, um dos cinco estados do Simkl. A Biblioteca ja sabia mostrar
// TITULOS; o que faltava era mostrar as listas em si, entrar numa e decidir se
// ela fica fixada aqui ou vira fileira na Home.
//
// O QUE CADA FONTE DA DE VERDADE, medido no codigo que ja existe:
//
//   TRAKT  — as listas PESSOAIS do dono (/users/me/lists, precisa do token) e
//            as listas PUBLICAS de qualquer pessoa (/search/list,
//            /lists/trending, /lists/popular — estas tres so precisam do client
//            id do APLICATIVO, como o web faz em buildTraktHeaders e como
//            descoberta.c ja faz em /lists/<id>/items).
//   NUVIO  — as pastas de colecao da conta (colecoes.c, alimentado por
//            sync_pull_collections). Nao ha rede aqui: ja estao em memoria.
//   SIMKL  — o Simkl NAO TEM listas nomeadas na API. O que ele tem sao os CINCO
//            ESTADOS de acompanhamento (/sync/all-items/<tipo>/<estado>). Sao
//            listas de verdade e sao do dono, mas nao sao "listas do Simkl" no
//            sentido do Trakt, e esta diferenca aparece na tela em vez de ser
//            disfarcada. Sem o vinculo feito em Ajustes nao ha token e a fonte
//            diz isso, em vez de mostrar uma aba vazia.
//
// CONTAGEM DE PEDIDOS. Isto roda numa TV: cada aba custa NO MAXIMO UM pedido
// (Trakt: um; Nuvio: zero; Simkl: zero para LISTAR — os cinco estados sao
// fixos e so o que a pessoa abrir e baixado). Abrir uma lista custa um pedido
// de pagina.
#ifndef NV_LISTAS_H
#define NV_LISTAS_H
#include "catalogo.h"
#include "colecoes.h"

// Quantas listas a tela mostra de uma fonte (ou de uma busca). O Trakt devolve
// ate 30 por pagina e nao ha paginacao na tela: rolar 60 cartoes de lista num
// D-pad ja e mais do que alguem faz sentado.
#define LST_MAX      64
// Fixadas na Biblioteca. Teto pequeno de proposito: e uma tela de atalhos, nao
// um segundo catalogo, e cada fixada vira uma linha no arquivo do perfil.
#define LST_FIX_MAX  24
// Itens baixados de uma lista do Simkl. As outras duas fontes passam pelo
// desc_vertudo_*, que tem o teto proprio dele (VT_MAX).
#define LST_ITENS_MAX 120

typedef enum { LST_TRAKT = 0, LST_SIMKL, LST_NUVIO, LST_FONTE_N } LstFonte;

typedef struct {
  int  fonte;             // LstFonte
  char titulo[128];
  char autor[64];         // dono da lista no Trakt; "" nas outras fontes
  char descricao[192];
  long traktId;           // id numerico da lista no Trakt; 0 nas outras
  char colId[96];         // id da pasta de colecao (Nuvio)
  char estado[16];        // estado do Simkl ("plantowatch", "watching", ...)
  char midia[8];          // "MOVIE" | "TV" — ver a nota sobre tipo em listas.c
  int  itens;             // -1 quando a fonte nao informa
  int  curtidas;          // so o Trakt informa; -1 caso contrario
} LstLista;

// --- ciclo de vida -----------------------------------------------------------
// Le as fixadas do perfil ATIVO (perfis_ativo()) e reaplica as que vao para a
// Home. Idempotente; chamar ao abrir a Biblioteca.
void lst_iniciar(void);
// Solta o que esta em MEMORIA, sem tocar em arquivo. A proxima leitura recarrega
// do disco — e o par lst_esquecer/lst_iniciar e exatamente o que um reinicio do
// app faz, que e como o teste o usa.
void lst_esquecer(void);
// LOGOUT: solta a memoria E APAGA o arquivo do perfil ativo. As listas fixadas
// sao da conta que saiu; deixa-las no disco as devolveria para a proxima pessoa
// que usasse o mesmo perfil, e a fileira da lista do Trakt dela continuaria na
// home — o mesmo defeito que trakt_esquecer existe para evitar.
//
// FALTA UMA LINHA PARA ISTO ACONTECER SOZINHO, e ela mora no logout de
// ajustes.c, ao lado de `fil_esquecer()`:
//
//     fil_esquecer();
//     lst_esquecer_conta();      // <- esta
//
// Tentei pendurar em fil_esquecer para nao mexer em ajustes.c e QUEBROU
// tests/fileiras.sh: aquele teste compila fileiras.c SOZINHO, com dubles de
// dados_gravar/dados_ler, exatamente para nao arrastar rede nem disco — e o
// cabecalho dele diz isso. Uma dependencia de fileiras.c para ca desmonta essa
// garantia, que vale mais do que a conveniencia de nao editar outro arquivo.
void lst_esquecer_conta(void);

// --- catalogo de listas ------------------------------------------------------
// Pede as listas de `fonte`. Nao bloqueia: dispara um fio quando ha rede a
// fazer e volta na hora. Repetir a mesma fonte com resultado em maos nao
// refaz o pedido.
void lst_pedir(int fonte);
// Procura nas listas PUBLICAS do Trakt. `termo` vazio pede /lists/trending.
void lst_buscar(const char *termo);
// As FIXADAS do perfil, no lugar de uma fonte. Nao tem rede.
void lst_pedir_fixadas(void);

int  lst_carregando(void);
int  lst_n(void);
const LstLista *lst_lista(int i);
// Motivo de a lista ter vindo vazia, ja em portugues e pronto para i18n: "" =
// nao ha motivo a mostrar (veio cheia, ou ainda esta carregando).
const char *lst_aviso(void);

// --- fixar e levar para a Home -----------------------------------------------
int  lst_fixada(const LstLista *l);
int  lst_na_home(const LstLista *l);
// Fixa (ou desfixa) na Biblioteca. Grava na hora. Devolve o estado NOVO.
int  lst_alternar_fixada(const LstLista *l);
// Liga (ou desliga) a lista na Home. Devolve o estado NOVO.
//
// REUSA O CAMINHO QUE JA EXISTE, e nao inventa um segundo: uma lista do Trakt
// vira uma pasta de colecao com fonte `prov="trakt"` + `traktLista`, que e
// exatamente o que o editor do site grava (colecoes.c) e o que
// desc_vertudo_fonte ja sabe buscar (descoberta.c, /lists/<id>/items). Uma
// pasta Nuvio ja E uma colecao: o que muda e so o liga/desliga da fileira dela
// em fileiras.c.
//
// O SIMKL NAO VAI PARA A HOME e a tela diz por que: a fileira de colecao so
// sabe pedir catalogo de addon, TMDB ou Trakt. Empurrar Simkl ali exigiria um
// quarto provedor em descoberta.c, que nao e assunto desta tela.
int  lst_alternar_home(const LstLista *l);
// 0 quando esta lista nao pode ir para a Home, com o motivo em `porque`.
int  lst_aceita_home(const LstLista *l, const char **porque);

// --- abrir uma lista ---------------------------------------------------------
// Comeca a carregar os itens de `l`. Trakt e Nuvio vao por desc_vertudo_fonte /
// desc_vertudo_filtro; Simkl tem fio proprio (nao ha provedor Simkl no
// vertudo). `midia` sobrepoe l->midia ("MOVIE"/"TV") — ver a nota sobre o tipo
// unico por pedido em listas.c.
void lst_abrir(const LstLista *l, const char *midia);
int  lst_itens_n(void);
int  lst_item(int i, CatItem *dst);
int  lst_itens_carregando(void);
// Pede a proxima pagina, quando a fonte pagina. Sem efeito no Simkl (uma volta
// so) e quando a anterior veio curta.
void lst_itens_mais(void);

// --- leitura de JSON, exposta para os testes ---------------------------------
// Formatos que o Trakt devolve para listas. Os tres carregam o MESMO objeto de
// lista; muda so o que vem em volta dele.
enum { LST_JS_MINHAS = 0,   // /users/me/lists -> [ {lista} ]
       LST_JS_ENVELOPE };   // /search/list, /lists/trending -> [ {"list":{...}} ]
// Substitui a tabela pelo que houver em `json`. Devolve quantas entraram.
int lst_ler_trakt(const char *json, int envelope);
// Itens de /sync/all-items/<tipo>/<estado> do Simkl para `dst`. `serie` escolhe
// entre o vetor "shows" e o "movies". Devolve quantos.
int lst_ler_simkl_itens(const char *json, int serie, CatItem *dst, int max);

// Nome do arquivo de fixadas do perfil ativo. Exposto para o teste provar ONDE
// escreve.
const char *lst_arquivo(void);

#endif
