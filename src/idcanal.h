#ifndef NUVIO_IDCANAL_H
#define NUVIO_IDCANAL_H
#include <stddef.h>

// ID DE CANAL LONGO DEMAIS PARA O CatItem (CNCVerse Bridge, comunidade 17/10).
//
// O CNCVerse publica o canal como "cnc:" + base64url("<plugin>::<dataUrl>"),
// sempre bem acima de 80 bytes. O guia guardava o id em 80 bytes e o CatItem
// (cache em disco, favoritos, player) em 64: o canal sumia da aba ou, com 64 a
// 79 bytes, o /stream/ saia com o id CORTADO e voltava vazio ("0 fontes").
//
// O id inteiro fica aqui, numa tabela da sessao, e o resto do app carrega um
// apelido curto e estavel ("~c:" + 16 hex do FNV-1a), que cabe em qualquer
// buffer de 64. So quem monta a URL do /stream/ (addons.c) expande de volta.
#define IDCANAL_CURTO_MAX 63    // cabe em CatItem.imdb[64]
#define IDCANAL_LONGO_MAX 1024  // id maior que isto o addon nao devia mandar

// Id que cabe fica como esta. Id longo e registrado e `dst` recebe o apelido.
// 0 se o id e vazio, passa de IDCANAL_LONGO_MAX, `dst` nao comporta o apelido
// ou a tabela esta cheia.
int idcanal_curto(const char *id, char *dst, size_t n);

// Apelido registrado vira o id inteiro; qualquer outro texto volta igual.
// Sempre escreve em `dst` (NUL garantido) e devolve `dst`.
const char *idcanal_longo(const char *id, char *dst, size_t n);

#endif
