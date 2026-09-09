// SALVOS: a lista de "quero ver" que sobrevive ao proximo arranque.
//
// O DEFEITO QUE ISTO CONSERTA, e ele e grande. O "+" do detalhe (app.c,
// detail_pediu_marcar) e o "Adicionar a biblioteca" do menu de cartaz
// (ctxmenu.c, OP_LISTA) faziam exatamente tres coisas:
//
//   biblioteca_alternar_lista(i)  -> so remonta a grade, nao guarda nada
//   trakt_watchlist(imdb, ...)    -> precisa de token do Trakt
//   cat_definir_na_lista(i, ...)  -> marca o item EM MEMORIA
//
// Nenhuma das tres escreve em disco. E `CatItem.naLista` e apagado a cada
// republicacao do catalogo (cat_definir_tudo), coisa que a descoberta faz
// varias vezes por ciclo — o mesmo motivo pelo qual contalib.c precisou de
// contalib_reconciliar. Ou seja: QUEM NAO TEM TRAKT VINCULADO NAO CONSEGUIA
// SALVAR NADA. O botao acendia, o item entrava na Biblioteca, e sumia sozinho
// no primeiro ciclo de descoberta ou no fechamento do app. Sem log, sem erro.
//
// Este modulo e o lastro que faltava: uma lista local, gravada em salvos.txt
// pela mesma dados_gravar atomica que a sessao usa, reaplicada no catalogo por
// salvos_reconciliar a cada quadro. Ele NAO substitui o Trakt nem a conta —
// ele e a terceira fonte que cai na MESMA marca `CatItem.naLista`, junto da
// watchlist do Trakt (descoberta.c) e da biblioteca da conta (contalib.c).
//
// POR QUE NAO UMA QUARTA SUPERFICIE. O app irmao do dono (tvOS) chegou a ter
// quatro botoes "+" gravando em quatro lugares diferentes, e o sintoma era o
// titulo salvo aparecer em uma tela e nao em outra. Aqui a regra e explicita:
// existe UM destino de leitura (`CatItem.naLista`) e todo mundo escreve nele.
// Este modulo apenas garante que a marca sobreviva ao disco.
//
// ONDE ELE PARA: nao fala com a rede. Quem publica no Trakt continua sendo
// app.c/ctxmenu.c, e quem le a conta continua sendo contalib.c.
#ifndef NV_SALVOS_H
#define NV_SALVOS_H

#include "catalogo.h"

// Teto da lista local. 300 itens x ~800 bytes = ~240 KB, alocados estaticamente
// porque esta lista e consultada por quadro no desenho do painel e uma
// indirecao a mais ali nao paga. O teto do web para a mesma tela e maior, mas
// esta TV ja bateu no limite de 128 MiB do WebAssembly com o catalogo publicado
// (TIZEN-MEMORIA.md) e a lista de "quero ver" de uma pessoa real nao chega
// perto disso.
#define SALVOS_MAX 300

typedef struct {
  char id[24];        // IMDb ("tt0111161"). Vazio nunca entra na lista.
  char tipo[8];       // "movie" | "series"
  char titulo[160];
  char poster[512];
  char meta[96];      // "2002" ou "2022 · 3 temporadas", como vem do catalogo
  int  nota;          // imdb_rating em porcentagem; 0 = desconhecida
  long long quandoS;  // time(NULL) do salvamento, para "Salvo há 2 horas"
} SalvoItem;

// Le salvos.txt. Chamar UMA vez no arranque, depois de dados_iniciar — sem
// pasta de dados isto e no-op silencioso e a lista comeca vazia (o log de
// dados.c ja explicou por que nao ha pasta).
void salvos_iniciar(void);

int  salvos_n(void);
const SalvoItem *salvos_item(int i);        // NULL fora da faixa

// 1 quando `imdb` esta na lista LOCAL. Nao responde pela watchlist do Trakt nem
// pela biblioteca da conta: para "este titulo esta salvo?" na interface, o
// certo continua sendo `CatItem.naLista`, que funde as tres fontes.
int  salvos_tem(const char *imdb);

// Poe ou tira o titulo da lista local e GRAVA. Idempotente nos dois sentidos.
// Devolve 1 se a lista mudou.
//
// Recebe o CatItem inteiro porque a lista precisa guardar titulo, poster e meta
// para conseguir se desenhar ANTES de a descoberta trazer o catalogo de volta —
// no arranque seguinte o painel abre antes de existir catalogo nenhum, e uma
// lista de ids nus desenharia oito retangulos cinza sem nome.
int  salvos_definir(const CatItem *ci, int salvo);

// Marca no catalogo (naLista) tudo que esta na lista local. Devolve quantos
// ficaram marcados. FIO PRINCIPAL apenas.
//
// NUNCA DESMARCA, pela mesma razao que contalib_aplicar_catalogo nao desmarca:
// a ausencia aqui nao prova ausencia no Trakt nem na conta.
int  salvos_aplicar_catalogo(void);

// Reaplica se o catalogo foi TROCADO desde a ultima aplicacao. Chamar por
// quadro; no caso comum custa uma comparacao de inteiro e um strcmp curto.
// Mesmo mecanismo e mesmo motivo de contalib_reconciliar — ver a nota longa
// la, ela vale palavra por palavra para este modulo.
void salvos_reconciliar(void);

// Apaga a lista do aparelho. Chamar de sync_esquecer_usuario: a lista de "quero
// ver" e tao pessoal quanto o token, e numa TV de sala sair da conta tem de
// apagar de verdade.
void salvos_esquecer(void);

#endif
