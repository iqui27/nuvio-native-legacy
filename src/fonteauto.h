// A FILA DA FONTE AUTOMATICA E A VERIFICACAO UMA POR VEZ — issue #130.
//
// O relato: "depois de tocar Special Ops: Lioness uma vez, o painel do TorBox
// mostra 6 arquivos diferentes da Lioness carregados". Nao era o player: era a
// verificacao de streams.c, que conferia as candidatas EM PARALELO (4 fios,
// ate 8 candidatas). Conferir uma fonte de debrid nao e so perguntar: o GET com
// Range no link do AIOStreams faz o AIOStreams adicionar o torrent no TorBox,
// e o nosso proprio resolvedor (debrid.c) faz createtorrent. Cada candidata
// conferida vira um arquivo no painel e uma chamada na cota do mes — mesmo as
// que nunca iam tocar, porque a primeira ja servia.
//
// Aqui mora so a REGRA, sem rede, sem SDL e sem a lista de streams: quem
// monta a fila e em que ordem, e a garantia de que a verificacao para na
// primeira que serve. streams.c liga isto a rede; tests/fonteauto.c conta as
// URLs tocadas sem rede nenhuma.
#ifndef NV_FONTEAUTO_H
#define NV_FONTEAUTO_H

// Os dois modos de "Fonte automatica" em Ajustes. O INDICE e o gravado em
// ajustes.txt (fonteAutoLocal): nao reordenar.
//   MELHOR   = a regra de pontuacao do dono (MP4 4K DV > 4K > DV > altura),
//              conferindo em serie ate achar uma que serve.
//   PRIMEIRA = a primeira da lista, na ORDEM QUE O ADDON MANDOU, e so ela. E o
//              "Auto-play first source" do Nuvio: quem filtra e ordena no
//              AIOStreams ja decidiu, e conferir as outras so gasta cota.
enum { FONTEAUTO_MELHOR = 0, FONTEAUTO_PRIMEIRA = 1 };

// Monta em `fila` (capacidade `max`) as candidatas na ordem em que devem ser
// tentadas e devolve quantas entraram.
//   total      tamanho da lista de streams
//   preferida  indice lembrado pela fontepref (-1 = nenhum); entra PRIMEIRO
//   pontos     pontuacao de cada fonte (modo MELHOR); NULL = todas iguais
//   acimaTeto  1 = fonte acima de "Qualidade maxima" (modo PRIMEIRA: vai para
//              o fim, sem sair da fila); NULL = nenhuma
//   excluida   1 = ja falhou nesta lista, nao entra; NULL = nenhuma
// Em empate de pontuacao fica o de MENOR indice, que e a ordem do addon.
int fonteauto_fila(int modo, int total, int preferida, const long *pontos,
                   const unsigned char *acimaTeto, const unsigned char *excluida,
                   int max, int *fila);

// Quantas candidatas o modo pode conferir numa escolha. PRIMEIRA = 1 sempre:
// a garantia do #130 e que so a fonte que vai tocar e tocada.
int fonteauto_tentativas(int modo, int pedidas);

// Confere a fila EM SERIE e PARA na primeira que serve: verificar() nunca e
// chamada para uma candidata depois de outra ter servido. *tocadas (se nao
// NULL) recebe quantas foram conferidas; falhou(), se nao NULL, e chamada para
// cada uma que nao serviu (streams.c a tira da fila da lista atual, e a
// proxima escolha nao a confere de novo). Devolve o indice ou -1.
typedef int  (*FonteVerificar)(int indice, void *u);
typedef void (*FonteFalhou)(int indice, void *u);
int fonteauto_primeira(const int *fila, int n, FonteVerificar verificar,
                       FonteFalhou falhou, void *u, int *tocadas);

#endif
