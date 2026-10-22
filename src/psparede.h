#include "arteurl.h"
// A PAREDE DE CADA PERFIL no fundo "Filmes" da escolha de perfil (2.0).
//
// A tela de perfil abre ANTES do sync e com o catalogo do ultimo perfil usado,
// entao os cartazes de outra pessoa nao estao em memoria. Por isso, toda vez
// que a tela abre, os cartazes do perfil ATIVO (o que ele assistiu por ultimo,
// depois a lista, depois o comeco da Home dele) vao para perfilparede.txt, e a
// parede de cada perfil sai dessa copia. Perfil que nunca foi aberto nesta TV,
// ou com PIN, nao tem parede propria: a tela usa o mural do catalogo.
//
// Le e grava so o disco local; nenhuma rede. Apagado no logout.
#ifndef NV_PSPAREDE_H
#define NV_PSPAREDE_H

#define PSPAREDE_MAX   12   // cartazes guardados por perfil
#define PSPAREDE_URL NV_ARTE_URL_MAX   // mesmo teto do CatItem.poster

// Guarda a parede do perfil ativo a partir do catalogo e do progresso.
void psparede_registrar(void);
// Quantos cartazes o perfil `indice` (profile_index) tem guardados.
int  psparede_n(int indice);
const char *psparede_url(int indice, int k);
void psparede_esquecer(void);

// PURA (teste): junta `n` URLs candidatas numa lista sem repetidas e sem
// vazias, ate `max`. Devolve quantas entraram em `saida`.
int  psparede_juntar(const char *const *cand, int n, char saida[][PSPAREDE_URL], int max);

#endif
