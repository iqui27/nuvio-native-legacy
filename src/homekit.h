#ifndef NV_HOMEKIT_H
#define NV_HOMEKIT_H
// KIT DE HOME: um conjunto pronto de fileiras de colecoes, importado por URL.
//
// O que e: um JSON publico com colecoes no MESMO formato que a conta guarda
// (colecoes.h, o collectionsStore.js do Nuvio web), mais nome e autor:
//   { "nuvioKit": 1, "name": "...", "author": "...",
//     "collections": [ { "title": "...", "backdropImageUrl": "...",
//       "folders": [ { "title": "...", "coverImageUrl": "...", "tileShape": "POSTER",
//         "sources": [ { "provider": "tmdb", ... } | { "provider": "trakt", ... } |
//                      { "provider": "addon", "addonId": "...", "type": "...", "catalogId": "..." } ] } ] } ] }
// Tambem vale o array de colecoes sozinho (o que o editor do web exporta).
//
// REGRAS DE SEGURANCA (o kit vem de um endereco de terceiro):
//   - o kit so ACRESCENTA fileiras; nunca instala addon, nunca mexe em ajuste;
//   - fonte de addon vale so pelo addonId e so se o addon ja esta instalado: a
//     URL de addon que o kit trouxer e IGNORADA (ela pode carregar a chave de
//     outra pessoa, e faria a TV chamar um servidor que o dono nao escolheu);
//   - arte so por http(s); URL que nao coube inteira fica vazia, nunca cortada;
//   - tetos fixos de tamanho, de pastas e de fontes.
//
// ESTE ARQUIVO E A REGRA PURA: texto -> pastas, e pastas -> texto. Sem rede,
// sem disco, sem colecoes.c. Quem guarda e poe na Home e homekitapp.c.
#include "colecoes.h"

#define HK_VERSAO       1
#define HK_CORPO_MAX    (256 * 1024)   // bytes do JSON baixado
#define HK_GRUPOS_MAX   4              // colecoes do kit = fileiras na Home
#define HK_PASTAS_MAX   16             // pastas, somando todas as colecoes
#define HK_FONTES_PASTA 8
#define NV_HK_URL_MAX   1024           // endereco do kit; o que nao cabe e recusado

typedef enum {
  HK_OK = 0,
  HK_ERR_JSON,     // nao e JSON completo, ou nao tem colecoes
  HK_ERR_VERSAO,   // nuvioKit maior que o que este app entende
  HK_ERR_VAZIO,    // tem a forma, mas nenhuma pasta com fonte utilizavel
  HK_ERR_GRANDE    // passa de HK_CORPO_MAX
} HkResultado;

typedef struct {
  char nome[96], autor[64];
  int nGrupos, nPastas;
  // pastas[i].sources aponta para fontes[i]: a struct NAO pode ser copiada com
  // memcpy/atribuicao (o ponteiro ficaria apontando para a copia velha).
  ColFolder pastas[HK_PASTAS_MAX];
  ColSource fontes[HK_PASTAS_MAX][HK_FONTES_PASTA];
  int grupoDe[HK_PASTAS_MAX];   // 0..nGrupos-1, crescente
  // O que ficou de fora: vai para o log e para o aviso da importacao.
  int pulAddon;    // fontes de addon que nao esta instalado (ou sem addonId)
  int pulFonte;    // fontes de provedor desconhecido ou incompletas
  int pulPasta;    // pastas sem titulo ou sem nenhuma fonte, e as alem do teto
  int pulArte;     // URLs de arte recusadas (nao-http ou longas demais)
} HkKit;

// `addon_instalado(id)`: 1 se ha addon instalado com esse id de manifesto.
// NULL aceita qualquer addonId (releitura do que ja foi aceito e guardado).
HkResultado hk_ler(const char *json, HkKit *k, int (*addon_instalado)(const char *id));

// O kit ACEITO de volta em JSON (o mesmo formato; so o que hk_ler guardou).
// malloc; NULL sem memoria. hk_ler(hk_serializar(k)) devolve o mesmo kit.
char *hk_serializar(const HkKit *k);

// groupId das pastas do grupo `g` ("kit_home_<g>"): a chave da fileira na Home
// e collection_<groupId>, estavel entre importacoes.
void hk_grupo_id(int g, char *dst, unsigned n);

// 1 se `url` e um endereco http(s) com host. O kit so entra por um desses.
int hk_url_valida(const char *url);
#endif
