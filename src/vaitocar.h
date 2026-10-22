// VAI TOCAR — a faixa da pagina do titulo (2.1, tela 4 "Titulo e Fontes") que
// diz A QUE A PESSOA ESTA PRESTES A APERTAR PLAY: a fonte que o automatico
// escolheria agora (resolucao, HDR, audio, idioma, tamanho, cache) e o que da
// para pular no episodio (abertura, resumo, creditos e a origem das marcas).
//
// PURO, de proposito: nada aqui desenha, abre textura ou fala com a rede.
// detail.c pede `vt_montar` por quadro (barato: le a lista de streams ja em
// memoria e o cache de marcas do intro.c) e desenha o que esta struct
// descreve; os testes validam o TEXTO sem GL (tests/vaitocar.c).
//
// DE ONDE VEM CADA CAMPO. Nada aqui e inventado: ou o dado ja existe hoje
// (streams.c escolhe a fonte, badges.h mede HDR, intro.c guarda as marcas,
// debrid diz o cache) ou o campo sai vazio e a linha correspondente some —
// a mesma regra do resto da pagina: "campo ausente fica ausente".
//
// O QUE FICOU FORA (o mapa D do mockup, N classificando cada dado):
//   - "abre em ~2 s" (estimativa de abertura da fonte em cache) = PRECISA DO
//     NOSSO SERVIDOR: ninguem mede nem guarda o tempo de abertura por fonte.
//     `vt->cache` diz so "em cache", sem o "~2 s". Linha no relatorio.
//   - Marcas A/R/C de TODOS os episodios de uma temporada = PRECISA DO NOSSO
//     SERVIDOR (18 pedidos por temporada ao TheIntroDB e abuso; a resposta
//     deveria vir em lote do nosso lado). detail.c so mostra as marcas dos
//     episodios que o intro.c ja TEM em cache nesta sessao (o que esta
//     tocando e os ja tocados); o resto do card nao mostra chip nenhum.
//   - Cobertura de "Prévia" (INTRO_PREVIA) nas frases: o TheIntroDB nao tem o
//     tipo; so o AniSkip o manda (animes). Quando a origem nao e AniSkip, a
//     faixa diz "Prévia sem marcas" apagado, como o mockup.
//   - "Ordem / melhor para esta TV" no chip da folha ja existe (o ajuste de
//     ordem do addon, #400); a EXPLICACAO da escolha ("cada criterio, o que
//     ficou de fora, copias agrupadas") e o que esta struct alimenta na folha.
#ifndef NV_VAITOCAR_H
#define NV_VAITOCAR_H
#include <stddef.h>
#include "streams.h"

#define VT_CRITERIA_MAX 8    // 6 criterios + 1-2 linhas do que ficou de fora
#define VT_MARCAS 4          // A, R, C, P(revia)

enum {
  VT_T_A = 0x1,   // abertura
  VT_T_R = 0x2,   // resumo
  VT_T_C = 0x4,   // creditos
  VT_T_P = 0x8    // previa (so AniSkip manda)
};

typedef struct {
  // FAIXA 1 — a fonte que vai tocar. `tem` 0 = nada a dizer (lista vazia ou
  // de outro alvo) e a faixa nao aparece.
  int   tem;
  char  tipo[12];      // "Remux", "BluRay", "WEB-DL", "WEB-Rip" — vazio = desconhecido
  char  res[8];        // "4K", "1080p"... vazio = resolucao desconhecida
  char  hdr[24];       // "Dolby Vision", "HDR10"... vazio = SDR
  char  audio[24];     // "EAC3 5.1", "AAC"... vazio = desconhecido
  char  idiomas[64];   // "Português · legendas PT" — vazio = desconhecido
  char  tamanho[24];   // "14,2 GB" — vazio = desconhecido
  int   cache;         // 1 = em cache no debrid (sem previsao de tempo: ambar)
  char  provedor[96];  // "AIOStreams" — quem entrega
  // FAIXA 2 — o que da para pular no episodio da faixa 1.
  int   pularOn;       // 0 (ajuste desligado) = a faixa 2 nao existe
  int   marcas;        // VT_T_* das marcas CONHECIDAS do episodio
  int   origem;        // VT_O_*: texto e i18n, ver vt_origem_rotulo()
  // FILME: os dois blocos valem para o titulo inteiro (ep 0/0).
  int   temporada, episodio;   // 0/0 = filme
} VaiTocar;

enum {
  VT_O_NENHUMA = 0,      // sem origem (nenhuma marca respondeu ainda)
  VT_O_THEINTRODB = 1,   // TheIntroDB
  VT_O_ANISKIP = 2,      // AniSkip (animes)
  VT_O_CAPITULOS = 4     // capitulos do proprio arquivo; origens combinam com |
};

// MONTA a faixa a partir do que ja esta em memoria (streams.c + intro.c).
// `imdb` e o titulo da pagina; `temporada`/`episodio` e o alvo do botao
// primario (0/0 em filme). Devolve vt->tem. Sem lista de fontes do mesmo
// alvo, vt sai zerado e a faixa some — nunca meia informacao estampada.
//
// Esta funcao NAO escolhe nada: pergunta ao streams.c qual fonte tocaria
// agora e so DESCREVE a resposta. A regra de escolha continua sendo a de
// streams.c, unica, para o botao e a faixa nunca discordarem.
int vt_montar(VaiTocar *vt, const char *imdb, int temporada, int episodio);

// DESCREVE uma fonte (res, hdr, audio, idiomas, tamanho, cache, provedor).
// Nao escolhe nada: `s` ja e a escolhida. Usado por vt_montar e pelo cartao.
void vt_descrever(const Stream *s, VaiTocar *vt);

// Nome amigavel no lugar do nome cru do arquivo (#9 do mockup, decidido):
// "WEB-DL · 4K Dolby Vision" na linha; o nome cru fica no detalhe da fonte
// (folha, modal "Detalhes da fonte" de #355), que ja o mostra hoje.
void vt_nome_amigavel(const VaiTocar *vt, char *dst, size_t n);

// Texto i18n da origem das marcas (a linha pequena a direita da faixa 2).
// Chaves novas em idioma_tab.h, devolvidas no idioma da pagina.
const char *vt_origem_rotulo(int origem);

// 1 = algum ajuste de pular (abertura, resumo, creditos) esta ligado. Sem
// isto as marcas nao aparecem em lugar nenhum da pagina (nem chips, nem faixa).
int vt_pular_ligado(void);

// Marcas conhecidas de UM episodio (para o card dele): bits VT_T_*. 0 = nada
// em cache — o card nao desenha chip nenhum (nunca o placeholder das marcas
// da lista inteira: "sem dado" e "sem a marca" nao sao a mesma coisa).
int vt_marcas_episodio(const char *imdb, int temporada, int episodio);

// --- CRITERIOS DA ESCOLHA AUTOMATICA (folha de Fontes, 2.1) -------------------
//
// O cartao que a folha abre no topo: a escolha automatica e o PORQUE, um
// criterio por linha, em linguagem de quem assiste ("em cache no debrid:
// abre rapido"), nao de quem mantem addon. `fora` conta o que NAO entrou e
// diz o motivo de cada cartao ("3 fontes com Dolby Vision perfil 5 ficaram
// de fora: esta TV nao toca"). Duplicatas do mesmo arquivo viram UMA linha
// com "2 add-ons · mesmo arquivo" no lugar de N linhas repetidas (#458).
typedef struct {
  int   tem;                     // 0 = folha sem escolha automatica (lista vazia)
  int   indice;                  // indice em streams.c da fonte escolhida
  char  nome[256];               // nome amigavel (vt_nome_amigavel)
  char  nomeCru[256];            // nome cru do arquivo, para o detalhe
  char  prov[96];                // quem entrega ("AIOStreams")
  char  tamanho[24];             // "14,2 GB" — vazio = desconhecido
  // Linhas do cartao. `fora[i]` 1 = linha CINZA do que ficou de fora (icone
  // x), 0 = criterio cumprido (icone ok). Os textos sao chaves i18n prontas.
  char  linha[VT_CRITERIA_MAX][192];
  int   fora[VT_CRITERIA_MAX];
  int   nLinha;
} VtEscolha;

// Monta o cartao da escolha. 0 = sem escolha (a folha cai no que ja tinha).
int vt_escolha(VtEscolha *e);

// AGRUPAMENTO DE COPIAS (#457, #458). Duas fontes sao a MESMA COPIA quando
// trazem o mesmo torrent (infoHash + arquivo) ou, sem hash, o mesmo NOME DE
// ARQUIVO (normalizado, 16+ letras/numeros) E o mesmo tamanho conhecido.
// bingeGroup NAO serve: ele diz "mesmo tipo de release entre episodios"
// ("torrentio|1080p"), nao "mesmo arquivo". Tamanho desconhecido nunca agrupa
// (melhor mostrar uma linha a mais do que esconder uma fonte diferente — #132).
//
// Implementado em streams.c (usa campos internos do Stream). Indices sao os
// de stream_item().
int vt_copias_mesmas(int a, int b);
// Quantas fontes de OUTROS add-ons sao copia de `i` (0 = nenhuma). A copia
// "mostrada" e a primeira na ordem da folha; as demais dobram nela.
int vt_copias_n(int i);
// Indice da primeira copia de `i` na ordem da folha (i quando e a primeira).
int vt_copia_rep(int i);

#endif
