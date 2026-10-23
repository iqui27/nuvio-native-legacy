// Teto de "Continuar assistindo" (issue #205). descoberta.c (CONT_MAX) e
// home.c (CW_TETO) repetem este numero; a tabela de futuros tambem.
#define CWO_FILEIRA_MAX 40

// ORDENACAO DE "CONTINUAR ASSISTINDO" (issue #127).
//
// Ajustes -> Continuar assistindo -> Ordenacao sempre existiu (AJ_CW_ORDEM,
// chave `continueWatchingSortMode` da conta), mas NINGUEM lia
// ajustes_cw_ordem(): as tres escolhas davam a mesma fileira. "Separar
// futuros" prometia uma fileira de proximos episodios que nunca aparecia.
//
// A REGRA E A DO WEB (NuvioWeb 0.3.38, js/ui/screens/home/homeScreen.js):
//   sortContinueWatchingItemsForDisplay (linha 1771) e
//   partitionContinueWatchingRows (linha 1817).
// La um item e "futuro" quando e NEXT UP (isNextUp) e o episodio ainda nao foi
// ao ar (hasAired === false, que resolveNextUpReleaseState tira da data de
// estreia: `released <= Date.now()`; sem data, conta como exibido).
//
//   default          -> most recently watched first, across both groups (#610).
//   streaming_style  -> pausados, sugestoes exibidas e futuros, nessa ordem;
//                       recencia dentro de cada grupo.
//   split_upcoming   -> a mesma lista, mas os futuros saem da fileira e viram
//                       uma fileira propria logo abaixo ("Proximos", a
//                       `upcoming_section` do web, titulo upcoming_section_title).
//
// Repare que streaming_style e split_upcoming dao a MESMA ordem: o web monta
// [...main, ...upcoming] nos dois. A diferenca e so onde os futuros aparecem.
//
// O QUE O NATIVO CHAMA DE "A SEGUIR" e o item de progresso 0 que o Trakt (pelo
// historico) ou o Simkl (next_to_watch) mandou como proximo episodio —
// trakt_e_a_seguir/simkl_e_a_seguir. A DATA DE ESTREIA vem do `released` do
// episodio no meta do Cinemeta, lido em trakt.c no mesmo GET que ja confirmava
// que o episodio existe; ela mora numa tabela lateral aqui, e nao no CatItem,
// porque sizeof(CatItem) e o cabecalho do cache em disco.
//
// Modulo sem dependencia de rede, SDL ou catalogo: a regra roda num teste de
// host (tests/cwordem.sh) do jeito que roda na TV.
#ifndef NV_CWORDEM_H
#define NV_CWORDEM_H
#include <limits.h>
#include <stddef.h>

// Os valores de AJ_CW_ORDEM, na ordem de V_CW_ORDEM/W_CW_ORDEM em ajustes.c.
enum { CWO_PADRAO = 0, CWO_STREAMING = 1, CWO_SEPARAR = 2 };

// "Nao se sabe a data". LLONG_MIN pelo mesmo motivo de PROX_SEM_DATA: 0 e
// uma data legitima (1/1/1970).
#define CWO_SEM_DATA LLONG_MIN

// Um candidato da fileira, ja na ordem por instante (o mais recente primeiro)
// que montarContinuar deu. So o que a regra precisa.
typedef struct {
  int aSeguir;          // 1 = "a seguir" (progresso 0, proximo episodio)
  long long estreiaMs;  // estreia do episodio, CWO_SEM_DATA se nao se sabe
} CwoItem;

// 1 quando o item e futuro: "a seguir" com estreia conhecida e DEPOIS de agora.
// Sem data conta como exibido, como o `hasAired !== false` do web.
int cwo_futuro(const CwoItem *it, long long agoraMs);

// Reordena `perm` (indices em `v`, que chegam na ordem por instante) segundo o
// modo. Pausados primeiro; dentro de cada grupo a recencia recebida se mantem.
// Nos modos streaming/separar os futuros ficam no fim. Devolve quantos itens
// ficaram na parte principal — o resto, de perm[principal] em diante, sao os
// futuros. No modo padrao o retorno e `n`.
int cwo_ordenar(const CwoItem *v, int n, int modo, long long agoraMs, int *perm);

// O CORTE DA FILEIRA com futuros. `principal` exibidos e `nFut` futuros, na
// ordem de cwo_ordenar, cabem em `max` lugares: devolve em *nPrincipal quantos
// exibidos ficam (os primeiros) e em *nFuturos quantos futuros (os de estreia
// mais proxima).
//
// POR QUE HA RESERVA (Brothers, C9, 24/09): o corte era um slice simples da
// lista [exibidos..., futuros...], e a fileira do dono tem 22 candidatos para
// 12 lugares — os futuros, sempre no fim, eram SEMPRE os cortados. "Separar
// futuros" e "Estilo streaming" nao mudavam nada: o episodio que ainda nao foi
// ao ar sumia da tela em vez de ir para o fim ou para "Proximos episodios". O
// web nao tem esse problema porque o slice dele e de 300 (CW_MAX_VISIBLE_ITEMS);
// aqui a fileira e de 40 (CONT_MAX), entao os futuros ganham ate um terco dela (minimo 1).
// Lugar que os futuros nao usam fica com os exibidos, e vice-versa.
// montarContinuar reserva os pausados ANTES de chamar com os lugares restantes.
void cwo_corte(int principal, int nFut, int max, int *nPrincipal, int *nFuturos);

// --- Datas de estreia (escritas por trakt.c, lidas por descoberta.c) --------
// `id` e o composto "tt123:2:5". Chamada de varios fios do enfeite ao mesmo
// tempo; protegida por trava.
void      cwo_marcar_estreia(const char *id, long long ms);
long long cwo_estreia(const char *id);   // CWO_SEM_DATA se nao registrada

// --- Quem esta na fileira de futuros (escrito por descoberta.c, lido pela home)
// montarContinuar decide UMA vez quem e futuro e publica os ids aqui antes de
// publicar a fileira; a home so pergunta. Duas decisoes (uma na montagem, outra
// no desenho) poderiam discordar na virada da estreia.
void cwo_publicar_futuros(const char *const *ids, int n);
int  cwo_e_futuro(const char *id);
// Sobe quando o CONJUNTO muda. A home a poe no hash das fileiras: a lista
// publicada pode ser identica (mesmos ids, mesma ordem) e so a divisao mudar —
// uma estreia que passou, por exemplo —, e sem este termo o guarda de
// sincronizarFileiras achava que nada tinha mudado.
unsigned cwo_revisao(void);

// --- O rotulo do card futuro ------------------------------------------------
// O card de "Proximos episodios" dizia "A seguir", igual ao episodio que ja
// foi ao ar e pode tocar agora — e o hero, "A SEGUIR" (C9 do dono, Brothers
// T1E6, 24/09). O futuro diz QUANDO: "Estreia 21 out" / "Airs Oct 21".
// Esta e so a data: "21 out" (PT) / "Oct 21" (EN), com o ano quando nao e o
// de `agoraMs` ("21 out 2027" / "Oct 21, 2027"); `maiusc` para o hero
// ("21 OUT"). Dia pelo calendario UTC. Devolve 0, com dst vazio, sem data.
//
// `idioma` e um IDIOMA_* (idiomacod.h). 0 = pt e 1 = en, como o `ingles` que
// este parametro era: quem passa 0/1 continua certo. Romeno, ucraniano e russo
// escrevem "21 окт" (dia primeiro, como o portugues).
int cwo_data_curta(long long estreiaMs, long long agoraMs, int idioma, int maiusc,
                   char *dst, size_t cap);

// --- "A seguir" DA CONTA NUVIO (issue #199) ----------------------------------
// Sem Trakt e sem Simkl, o "a seguir" sai dos vistos da conta (watched_items),
// como no web com a fonte Nuvio Sync (getContinueWatchingNextUpSeedOptions,
// homeScreen.js:2202 — includeWatchedItemSeeds). Antes a fonte "conta" so dava
// os pausados: 804 episodios vistos na conta e nenhum "a seguir" na fileira
// (log da C9 do issue, 01/10). Os ids ("tt:S:E") moram aqui, ao lado das
// estreias, porque trakt.c (que confere no Cinemeta que o episodio existe) e a
// home (que escreve "A seguir"/"Estreia") os leem, e os dois ja linkam este
// arquivo. Trocado inteiro de uma vez, sob trava: nada de janela vazia.
void cwo_conta_definir(const char *const *ids, int n);
// Tambem diz sim para os ajustados pela desmarcacao (cwo_ajustados_definir).
int  cwo_conta_a_seguir(const char *id);
void cwo_ajustados_definir(const char *const *ids, int n);
// A virada de temporada do enfeite (S3E10 nao existe, vira S4E1) muda o id.
void cwo_conta_trocar(const char *velho, const char *novo);

// O "a seguir" que VIROU DE TEMPORADA so entra quando a estreia ja passou ou
// esta a ate 7 dias; sem data nao entra. shouldShowNextUpEpisodeForContinue
// Watching (homeScreen.js:1845, CW_NEXT_UP_NEW_SEASON_UNAIRED_WINDOW_DAYS):
// sem isto, toda serie terminada com a temporada seguinte anunciada no
// Cinemeta (sem data, ou para daqui a um ano) viraria um card "a seguir".
// Dias contados pelo calendario UTC, como cwo_data_curta.
int cwo_virada_aceita(long long estreiaMs, long long agoraMs);

#endif
