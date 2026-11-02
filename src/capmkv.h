// CAPITULOS DO MKV NO ANDROID E NO .tpk (203-capitulos).
//
// Na LG (video.c) e no .wgt (video_tizen.c) o capitulo ja saia da descida do
// cabecalho que busca as faixas. No Android e no .tpk video_creditos() devolvia
// 0: ninguem lia o Chapters, e o "Proximo episodio"/"Pular creditos" caia no
// tempo fixo mesmo com o arquivo dizendo exatamente onde os creditos comecam.
//
// UM fio lateral por reproducao, depois de uma folga (o video abre primeiro),
// que usa o cabecalho ja baixado pela pre-busca do mkvass quando existe (sem
// rede) e, senao, UM Range de 320 KB; se o Chapters nao esta nessa janela, UM
// Range pelo SeekHead (mkv_capitulos_alem). Cache por URL (inclusive o "nao ha
// capitulos"), recuo em recusa do CDN (429/403/503) e desiste em 3 tentativas.
// Nada disso toca na reproducao: so o fio lateral espera.
//
// ORDEM de fonte dos creditos (credfonte.c): capitulo DESTE arquivo > TheIntroDB
// /AniSkip (validado pela duracao real) > vizinho > aprendido > tempo fixo.
#ifndef NV_CAPMKV_H
#define NV_CAPMKV_H
#include "mkv.h"
#include "intro.h"

// Nova reproducao em `url`: zera o estado anterior e dispara a leitura lateral.
// NAO bloqueia.
void capmkv_iniciar(const char *url);
// Fim da reproducao / fonte trocada: zera e invalida o fio em voo.
void capmkv_zerar(void);
// Segundo em que os creditos comecam segundo os capitulos, ou 0. `dur` (s,
// 0 = ainda desconhecida): o capitulo NOMEADO vale sempre; o ultimo capitulo so
// vale se comecar no ultimo quarto (mesma regra da LG).
double capmkv_creditos(double dur);
// Entrega capitulos ja lidos (LG/.wgt): guarda para capmkv_creditos e alimenta
// o modulo de intro (abertura/creditos/previa).
void capmkv_aplicar(const MkvCap *caps, int n);
// A reproducao corrente (muda a cada capmkv_zerar/capmkv_iniciar). Quem le os
// capitulos num fio proprio (LG, video.c lerMkv) guarda o valor ao disparar o
// fio e publica com capmkv_aplicar_ger: se o titulo mudou no meio, o fio do
// anterior nao publica no novo (mesma checagem do fio lateral do .tpk/Android).
unsigned capmkv_geracao(void);
// capmkv_aplicar, mas so se `g` ainda e a reproducao corrente. Devolve 1 se
// publicou (n == 0 tambem conta), 0 se `g` ficou velho e nada mudou.
int capmkv_aplicar_ger(const MkvCap *caps, int n, unsigned g);
// So a regra, sem estado: capitulos -> trechos de intro.h pelas regras de
// mkv_segmentos (abertura, resumo, creditos ate o capitulo seguinte, previa
// final, cena pos-creditos). Devolve quantos preencheu em `out`.
int capmkv_trechos(const MkvCap *caps, int n, IntroTrecho *out, int max);
// Os capitulos da reproducao corrente (todos, com nome), para a barra e a
// fileira de capitulos do player. Devolve quantos copiou; 0 = nenhum ainda.
int capmkv_lista(MkvCap *out, int max);
// Folga antes da leitura lateral (ms): o video abre primeiro. Ajustavel so por teste.
extern int capmkv_espera_inicial_ms;
// So para teste (NULL em producao): chamado pelo fio lateral depois de ler os
// capitulos e antes de publica-los, para o teste trocar de video nessa janela.
extern void (*capmkv_teste_antes_de_publicar)(void);
// Para teste: roda a leitura de `url` no fio atual (bloqueia). Devolve capitulos.
int capmkv_ler_agora(const char *url, MkvCap *caps, int max, int esperaMs);
#endif
