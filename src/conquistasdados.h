// Dados das Conquistas (2.1): o que a tela le e escreve. Ver conquistas.h.
//
// LOCAL de verdade: o estado por conta e perfil (conquistas-<conta>-p<N>.txt, regra em
// conquistasregra.h). O resto sao FONTES QUE PRECISAM DO NOSSO SERVIDOR: cada
// funcao abaixo devolve "nada" hoje e a tela mostra o vazio honesto. Quando o
// servidor existir, so o corpo de cada uma muda — a tela ja pergunta por elas.
// Nenhuma faz rede aqui, e nenhuma guarda token ou URL.
#ifndef NV_CONQUISTASDADOS_H
#define NV_CONQUISTASDADOS_H
#include "conquistasregra.h"

// Estado da conta e perfil ativos. Arquivos antigos sem conta sao ignorados.
// Arquivo ausente ou ruim = estado vazio.
void cqd_carregar(CqEstado *s);
// Grava o estado do perfil ativo (atomico). 1 se gravou.
int  cqd_gravar(const CqEstado *s);
// Reconta o catalogo local do Oscar pelo historico e progresso do perfil. Sem rede.
void cqd_atualizar_local(void);

// ---- Fonte: nosso servidor (ambar). Hoje nenhuma entrega nada.
// Raridade anonima de um emblema, em partes de mil; -1 = nao se sabe.
int  cqd_raridade(const char *chave);

// "Seu mapa de descobertas": decadas, paises e idiomas ja vistos. O app nao
// guarda ano, pais nem idioma de um titulo, so o servidor (Wikidata) sabe.
typedef struct {
  int ok;                  // 0 = sem dados
  unsigned char decada[10];   // 1 = viu algum filme da decada (1920..2010)
  int decadas, paises, idiomas;
} CqMapa;
void cqd_mapa(CqMapa *m);

// "Amigos nesta trilha": quantos amigos mostram esta trilha. 0 = nenhum dado.
int  cqd_amigos_na_trilha(const char *chave);

// Comunidade (8I) e temporada (8J): contagem de emblemas que o servidor
// reconhece. Hoje 0.
int  cqd_comunidade(void);
int  cqd_temporada(void);

#endif
