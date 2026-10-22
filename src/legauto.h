// ESCOLHA DA LEGENDA AUTOMATICA (puro, sem SDL nem rede).
//
// legenda_automatica (faixas.c) ligava a PRIMEIRA legenda de addon que casava
// com o idioma, na ordem em que os addons responderam. Agora:
//   1. idioma: o idioma pedido primeiro; no portugues, pt-BR antes de pt (ling_afinidade);
//   2. release: a legenda cujo nome divide mais palavras com o arquivo que
//      esta tocando (WEB-DL, 1080p, grupo...) vem na frente;
//   3. a que ja deu certo neste titulo+idioma (legauto_lembrar) vence tudo;
//   4. as ja tentadas e recusadas pelo AutoSync (`excl`) ficam fora.
// Quem chama aplica a escolhida na hora e o AutoSync refina/troca depois.
#ifndef NV_LEGAUTO_H
#define NV_LEGAUTO_H
#include "addons.h"
#include <stdint.h>

// Indice em v[0..n) ou -1 (nenhuma serve o idioma). `midia` = URL ou nome do
// arquivo que toca (so o ultimo trecho conta); `lembrada` = hash da que ja deu
// certo (0 = nenhuma).
int legauto_escolher(const Legenda *v, int n, const char *pref, const char *midia,
                     const uint64_t *excl, int nExcl, uint64_t lembrada);
// Palavras em comum entre o nome da legenda e o da midia (so para teste/log).
int legauto_afinidade_release(const char *arquivo, const char *midia);

// O que comparar com o nome das legendas: o nome do arquivo da fonte que toca
// (Stream.arquivo, dado por legauto_fonte) quando ha, senao `url`.
void legauto_fonte(const char *arquivo);
const char *legauto_midia(const char *url);

// Memoria por titulo+idioma, so da execucao (4 entradas, a mais velha sai).
void     legauto_lembrar(const char *titulo, const char *idioma, uint64_t hash);
uint64_t legauto_lembrada(const char *titulo, const char *idioma);
#endif
