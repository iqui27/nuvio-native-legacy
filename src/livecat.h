// livecat — secao sugerida (Jev, via Worker de recomendacoes) para canal de TV
// ao vivo que a heuristica local do guia (guia.c, categoriaPorNome) nao
// classificou. Sai SO o nome-base do canal (nv_nome_base), nunca URL nem conta.
//
// A resposta e uma das secoes que o guia ja usa (as chaves de idioma_tab.h);
// "Outros" e baixa confianca voltam como "sem sugestao". O que o Worker
// respondeu fica em disco por nome-base (livecat.txt, 30 dias): a segunda carga
// do guia nao pergunta de novo.
#ifndef NUVIO_LIVECAT_H
#define NUVIO_LIVECAT_H

#include <stddef.h>

// POST `corpo` (JSON) em `url`; devolve o corpo (free pelo chamador) e o status
// HTTP em `*status`. NULL = rede = rede_postar_st. Existe para o teste.
typedef char *(*LiveCatPostar)(const char *url, const char *corpo, int *status);
void livecat_postar_teste(LiveCatPostar p);

// Cached, unexpired section or NULL; resolvido includes cached negative answers.
// Never requests the network. resolvido may be NULL.
const char *livecat_secao(const char *nome, int *resolvido);

// Para cada nome[i] com sem[i] != 0 (canal sem categoria), out[i] = secao
// sugerida ou NULL (sem sugestao, falha de rede ou desligado). Canal com
// sem[i] == 0 nunca sai do aparelho e out[i] fica NULL. `ativo` (pode ser
// NULL) devolve 0 quando a carga foi cancelada; so consulta a rede em lotes.
// Devolve quantos canais ganharam secao. Roda no fio de carga, nunca no da UI.
int livecat_sugerir(const char *const *nomes, const unsigned char *sem, int n,
                    const char **out, int (*ativo)(void));

#endif
