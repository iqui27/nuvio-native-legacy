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
#include <stddef.h>

// Teto da lista local. ERA 300 EM VETOR ESTATICO, e o 301o titulo salvo era
// RECUSADO — so uma linha no log, com o botao "+" acendendo do mesmo jeito. A
// pessoa via exatamente o que o issue do Owlphibia29 descreve: "o que eu
// adiciono agora nao aparece". Agora a lista mora no heap e cresce com o uso
// (~800 bytes por titulo, nada alocado para quem nao salva), e este numero e so
// o teto de seguranca. Quando ele e atingido SAI O MAIS ANTIGO da lista local,
// nunca o que acabou de ser salvo — e o log diz qual saiu.
#define SALVOS_MAX 2000

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

// O MESMO TITULO COM DOIS IDS. O catalogo guarda a serie com progresso como
// "tt123:1:2" (o card de "Continuar assistindo", ver descoberta.c/trakt.c), e a
// lista local, a conta e a watchlist guardam "tt123". Comparar com strcmp fazia
// o painel de Salvos mostrar Widows Bay DUAS vezes, as duas no mesmo episodio
// (a linha local puxa o progresso da copia "tt123:1:2" do catalogo, e a copia
// entra de novo como linha propria). A regra e a de catalogo.c (mesmoTitulo): o
// unico sufixo que se ignora e ":<temporada>:<episodio>" em digitos. "tmdb:55"
// e "kitsu:12" continuam ids inteiros — cortar no primeiro ':' ja fez todo
// canal "cs:channel:..." virar o mesmo titulo (#37).
int  salvos_mesmo_titulo(const char *a, const char *b);
// `id` sem o sufixo ":<t>:<e>" em `out`. Id sem esse sufixo sai igual.
void salvos_id_titulo(const char *id, char *out, size_t tam);

// A UNIAO QUE A INTERFACE MOSTRA COMO "SALVOS": a lista local, na ordem de
// insercao, e depois cada TITULO do catalogo com naLista — uma vez so, por
// mais copias que o catalogo tenha dele (o mesmo titulo vive em varias
// fileiras, cada uma com a sua copia) e por mais ids que ele use.
//   local >= 0: indice em salvos_item(); `cat` e a copia do catalogo que tem o
//               progresso dele (-1 sem nenhuma).
//   local <  0: so o catalogo tem; `cat` e o indice.
// Devolve quantas entradas escreveu (no maximo `cap`). FIO PRINCIPAL apenas.
typedef struct { int local, cat; } SalvosEntrada;
int  salvos_uniao(SalvosEntrada *out, int cap);

// Apaga a lista do aparelho. Chamar de sync_esquecer_usuario: a lista de "quero
// ver" e tao pessoal quanto o token, e numa TV de sala sair da conta tem de
// apagar de verdade.
void salvos_esquecer(void);

#endif
