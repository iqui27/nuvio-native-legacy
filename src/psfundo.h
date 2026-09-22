// A ARTE DE FUNDO DA TELA DE ESCOLHA DE PERFIL, vinda do catalogo (issue #90).
//
// A tela de perfil ja tinha uma camada de arte, mas ela e OUTRA COISA: sai de
// `profile_background_url` da conta (`perfis.h:fundoUrl`). A maioria das contas
// nao tem esse campo, e para elas a tela e preto liso — que e literalmente a
// queixa da issue. Este modulo e a SEGUNDA fonte: dado o catalogo que ja esta
// em memoria quando a tela abre, devolve uma lista estavel de URLs de arte.
//
// NAO FAZ REDE E NAO TOCA DISCO. A tela de perfil abre no primeiro segundo do
// app, disputando com sync_iniciar() e com os manifestos; a regra aqui e usar
// so o que `cat_ler_cache` ja pos na memoria (a ordem esta documentada em
// catalogo.c: home_iniciar roda dentro de app_iniciar, antes de
// TELA_ESCOLHA_PERFIL). Catalogo vazio = lista vazia = a tela fica como era.
//
// DUPLICA O SORTEIO de home.c (heroMontarSet) de proposito. Extrair a funcao
// para ca seria o certo, mas `src/home.c` esta sendo editado em paralelo e a
// extracao viraria conflito garantido. A unificacao fica para um commit
// proprio, quando aquele arquivo estabilizar.
#ifndef NV_PSFUNDO_H
#define NV_PSFUNDO_H

// Dez, o mesmo HOME_HERO_LISTA da home: e quanto de arte diferente uma sessao
// de escolha de perfil consegue mostrar antes de alguem apertar OK.
#define PSFUNDO_MAX 10

// Monta a lista se ela ainda nao existe, ou se o catalogo/a fonte mudaram.
// `fonte` tem a mesma semantica de `fil_hero_fonte()`:
//   ""   os primeiros do catalogo
//   "*"  um sorteio do catalogo
//   ...  a chave (`CatFileira.chave`) de uma fileira
// `semente` e usada so no sorteio; 0 = a lista sai na ordem do catalogo, o que
// deixa o modulo determinista para os testes.
void psfundo_garantir(const char *fonte, unsigned semente);

// Quantas artes a lista tem (0..PSFUNDO_MAX).
int psfundo_n(void);

// URL de arte de tela cheia na posicao `pos`, ou NULL fora da faixa / sem arte.
// O buffer e proprio deste modulo e sobrevive a chamada seguinte de artehero_*.
const char *psfundo_url(int pos);

// Indice de catalogo na posicao `pos`, ou -1. Para quem quiser o titulo.
int psfundo_indice(int pos);

// QUAL POSICAO A ROTACAO MOSTRA no instante `agoraMs`, tendo comecado em
// `inicioMs` com um passo de `intervaloMs`. Funcao PURA e exportada de proposito:
// e o unico jeito de testar a rotacao sem GL e sem TV.
// Devolve 0 quando `n` <= 0 ou o intervalo e invalido.
int psfundo_pos_no_tempo(unsigned agoraMs, unsigned inicioMs,
                         unsigned intervaloMs, int n);

// Esquece a lista (a tela chama no iniciar; o proximo garantir remonta).
void psfundo_limpar(void);

#endif
