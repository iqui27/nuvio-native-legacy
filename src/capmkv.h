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
// So a regra, sem estado: capitulos -> trechos de intro.h. Abertura ("Opening"/
// "OP"), creditos nomeados (ate a previa, se houver) e previa final ("Preview"/
// "Next Episode"). Devolve quantos preencheu em `out`.
int capmkv_trechos(const MkvCap *caps, int n, IntroTrecho *out, int max);
// Folga antes da leitura lateral (ms): o video abre primeiro. Ajustavel so por teste.
extern int capmkv_espera_inicial_ms;
// So para teste (NULL em producao): chamado pelo fio lateral depois de ler os
// capitulos e antes de publica-los, para o teste trocar de video nessa janela.
extern void (*capmkv_teste_antes_de_publicar)(void);
// Para teste: roda a leitura de `url` no fio atual (bloqueia). Devolve capitulos.
int capmkv_ler_agora(const char *url, MkvCap *caps, int max, int esperaMs);
#endif
