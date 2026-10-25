// ESCRITA NAS LISTAS DO TRAKT: criar, mudar a descricao, adicionar um titulo
// (com nota opcional) e reordenar. listas.c LE; este modulo so ESCREVE, e e o
// unico que sabe o formato dos corpos.
//
// UMA ESCRITA POR VEZ. O estado e um so (lstesc_estado): a tela mostra
// "Salvando..." / confirma / avisa a falha, e uma segunda escrita enquanto a
// primeira voa e recusada (devolve 0), em vez de empilhar pedidos que
// terminariam fora de ordem. A escrita sai num fio destacado, como a da
// watchlist em trakt.c.
//
// SEM TOKEN, SEM ESCRITA: tudo devolve 0 quando trakt_cabecalhos nao tem
// credencial. Nada e gravado localmente antes da resposta: quem chama so mexe
// no espelho depois de LSTESC_OK.
#ifndef NV_LISTASESCRITA_H
#define NV_LISTASESCRITA_H
#include <stddef.h>
#include "catalogo.h"
#include "listas.h"

enum { LSTESC_OCIOSO = 0, LSTESC_PENDENTE, LSTESC_OK, LSTESC_FALHA };
// O que a ultima escrita foi, para a tela dizer a coisa certa.
enum { LSTESC_CRIAR = 1, LSTESC_DESCRICAO, LSTESC_ADICIONAR, LSTESC_ORDENAR, LSTESC_NOTA };

// --- corpos (puros; testados) ----------------------------------------------
// Escapa `s` como TEXTO JSON (sem as aspas) em `dst`. Devolve o tamanho; corta
// em vez de estourar.
size_t lstesc_json_texto(char *dst, size_t n, const char *s);
// {"name":..,"description":..,"privacy":"private"|"public"}. 0 = nome vazio ou
// nao coube.
int lstesc_corpo_criar(char *dst, size_t n, const char *nome, const char *desc, int privada);
// {"description":..}
int lstesc_corpo_descricao(char *dst, size_t n, const char *desc);
// {"movies"|"shows":[{"ids":{"imdb":..},"notes":..}]}; `nota` vazia = sem notes.
// `tipo` "series" vira shows, o resto movies (mesma regra da watchlist).
int lstesc_corpo_item(char *dst, size_t n, const char *imdb, const char *tipo, const char *nota);
// {"rank":[id,id,...]} com os ids das ENTRADAS (DescItemLista.itemId).
int lstesc_corpo_ordem(char *dst, size_t n, const long *ids, int k);
// {"notes":..}
int lstesc_corpo_nota(char *dst, size_t n, const char *nota);

// --- escrita (assincrona) --------------------------------------------------
// Todas: 1 = pedido disparado, 0 = recusado (sem token, argumento vazio ou
// outra escrita em voo).
int lstesc_criar(const char *nome, const char *desc, int privada);
int lstesc_descricao(long traktId, const char *desc);
int lstesc_adicionar(long traktId, const char *imdb, const char *tipo, const char *nota);
int lstesc_ordenar(long traktId, const long *itemIds, int k);
int lstesc_nota(long traktId, long itemId, const char *nota);

// "Adicionar a uma lista" do menu do titulo: dispara lstesc_adicionar e guarda o
// que a ilha vai dizer quando a resposta chegar (lstesc_passo).
int  lstesc_adicionar_titulo(const CatItem *ci, const LstLista *l);
// Uma vez por quadro (ilhasinais.c): ao fim de uma escrita feita pelo menu, a
// ilha diz "Adicionado a <lista>" ou que falhou.
void lstesc_passo(void);

int  lstesc_estado(void);
int  lstesc_qual(void);
long lstesc_lista_id(void); // destino da ultima escrita; criar = 0
// OK/FALHA -> OCIOSO, para a tela mostrar o resultado uma vez. Devolve o que havia.
int  lstesc_consumir(void);
// Teste: troca o transporte. NULL volta ao rede_pedir. `resp` e um malloc que
// o modulo libera; devolve o status HTTP.
typedef int (*LstescEnvio)(const char *metodo, const char *url, const char *corpo, char **resp);
void lstesc_teste_envio(LstescEnvio f);
// Teste: espera a escrita em voo terminar.
void lstesc_teste_esperar(void);
#endif
