// Regras de "PROXIMO" (Next Up) para a fileira "Continuar assistindo".
//
// Portadas do app web, onde vivem em tres modulos de REGRA PURA (nenhum toca
// interface) dentro de js/ui/screens/home/:
//
//   nextUpEpisodeAnchor.js:22    findAbsoluteEpisodeAnchorIndex
//   nextUpWatchingPolicy.js:16   NEXT_UP_NEW_RELEASE_WINDOW_MS
//   nextUpWatchingPolicy.js:29   shouldSurfaceNextUpForUntrackedSeries
//   nextUpCandidateResolver.js:3 resolveNextUpCandidates (teto de 24 consultas)
//
// e no varrimento que os consome, homeScreen.js:11416-11482
// (resolveNextUpEpisode) — a regra dos tres modulos so faz sentido junto dele.
//
// O QUE A REGRA RESOLVE. O historico diz em que episodio a pessoa parou, nao
// qual e o proximo. Quando o episodio semeado JA ACABOU, o card de "Continuar"
// esta oferecendo algo que ela ja viu; o proximo item da lista unica de
// episodios — que atravessa as temporadas, entao o fim de uma temporada cai
// naturalmente no comeco da seguinte — e o que ela quer ver. Acabada a ultima
// temporada nao ha proximo, e o certo e nao oferecer nada.
//
// A POLITICA DA SERIE NAO ACOMPANHADA existe porque a semente vem do historico
// e nunca da lista do rastreador: sem ela, uma serie terminada ha anos fica
// para sempre oferecendo o episodio seguinte ao ultimo visto. Rastreador modela
// franquia como uma entrada por temporada/cour, entao "terminada" quer dizer
// terminada AQUELA entrada, enquanto a lista do addon vai ate o fim da
// franquia. O unico caso que vale e NOTICIA: episodio que estreia dentro de 60
// dias de agora, para qualquer lado. O resto e acervo que a pessoa ja decidiu
// nao ver.
//
// --- O QUE O NATIVO NAO TEM, E O CAMINHO ESCOLHIDO --------------------------
//
// 1. `watchProgressRepository.isTrackedAsWatching` (homeScreen.js:11563) nao
//    existe aqui: o app nativo nao guarda a lista de "acompanhando" do Trakt,
//    so o /sync/playback. Sem esse sinal, TODA serie e tratada como NAO
//    acompanhada e a politica dos 60 dias vale sempre — o caminho conservador,
//    que erra para o lado de nao mostrar.
//
// 2. Nao ha carimbo de tempo do progresso (CatItem nao guarda `updatedAt`).
//    Entra 0, que e o proprio padrao do JS (nextUpWatchingPolicy.js:30).
//
// 3. Nao ha mapa de progresso POR EPISODIO nem conjunto de episodios vistos:
//    CatItem guarda UM par temporada/episodio e UM progresso. Entao os desvios
//    de homeScreen.js:11463-11472 (pular episodio ja visto, desistir quando o
//    candidato ja esta em andamento) nao tem dado para rodar e ficaram de fora;
//    o varrimento aqui e o passo seguinte a ancora, e nada mais.
//
// 4. `showUnairedNextUp` e a janela de temporada nova
//    (shouldShowNextUpEpisodeForContinueWatching, homeScreen.js:1853) nao foram
//    portadas: sao preferencia de interface, e a janela de 60 dias ja limita o
//    futuro pelos dois lados.
//
// 5. A data do episodio chega por EXTENSO ("27 de janeiro de 2023"), montada
//    por desc_data_extenso; o web recebe ISO. Data que nao da para ler vira
//    "sem data", e sem data nao se mostra nada — igual ao JS, que devolve false
//    quando `parseReleaseEpochMs` da null (nextUpWatchingPolicy.js:36).
#ifndef NV_PROXIMO_H
#define NV_PROXIMO_H
#include <limits.h>
#include "catalogo.h"

// Janela em que um episodio ainda conta como noticia, e nao como acervo.
// nextUpWatchingPolicy.js:16 — 60 dias.
#define PROX_JANELA_NOVIDADE_MS (60LL * 24LL * 60LL * 60LL * 1000LL)

// Teto de consultas por rodada. resolveNextUpCandidates corta a lista de
// candidatos em `maxLookups` ANTES de sair para a rede
// (nextUpCandidateResolver.js:12). Aqui vale como teto de itens da fileira que
// merecem o trabalho, pelo mesmo motivo: cada um custa uma lista de episodios.
#define PROX_MAX_BUSCAS 24

// A partir de quantos por cento o episodio semeado conta como ACABADO. E o
// mesmo limiar que a home ja usa para esconder um titulo terminado das outras
// fileiras (home.c:1741); dois numeros diferentes para "acabou" fariam a mesma
// serie parecer terminada numa fileira e em andamento na outra.
#define PROX_CONCLUIDO 90

typedef struct {
  int  temporada, episodio;
  char nome[120];
  char data[40];
} ProxSugestao;

// Indice de `episodio` lido como numero ABSOLUTO, ou -1 quando essa leitura nao
// vale. Porte 1:1 de findAbsoluteEpisodeAnchorIndex (nextUpEpisodeAnchor.js:22).
//
// So existe para o caso do Simkl sem mapeamento TVDB, que reporta a contagem
// corrida do anime como temporada 1 (One Piece episodio 66 chega como S1E66,
// enquanto a temporada 1 do addon acaba no 8). Numeracao absoluta SEMPRE chega
// como temporada 1, e a lista de episodios ja vem ordenada e sem especiais,
// entao o numero absoluto e um indice base 1 nela. Nunca chamar para uma serie
// cuja numeracao ja bate com a do addon.
int prox_ancora_absoluta(const CatEp *eps, int n, int temporada, int episodio);

// Indice do episodio semeado na lista, ou -1. Tenta o par temporada/episodio e,
// so quando `absolutaSimkl`, cai na leitura absoluta acima
// (homeScreen.js:11421-11433).
int prox_ancora(const CatEp *eps, int n, int temporada, int episodio,
                int absolutaSimkl);

// "Nao ha data". Nao pode ser -1 nem 0: data ANTES de 1970 e um numero
// negativo legitimo, e 0 e 1 de janeiro de 1970 — os dois apareceriam como
// ausencia de data se o sentinela fosse um deles.
#define PROX_SEM_DATA LLONG_MIN

// Data por extenso ("27 de janeiro de 2023") ou ISO ("2023-01-27") em ms desde
// a epoca, UTC. Devolve PROX_SEM_DATA quando nao da para ler — inclusive quando
// so o ano sobrou, que e o que desc_data_extenso deixa passar com ISO curto.
long long prox_data_ms(const char *data);

// shouldSurfaceNextUpForUntrackedSeries (nextUpWatchingPolicy.js:29), 1:1.
// `lancamentoMs` == PROX_SEM_DATA e o `null` do parseReleaseEpochMs de la.
int prox_mostrar_nao_acompanhada(long long semeadoEmMs, long long lancamentoMs,
                                 long long agoraMs);

// A regra inteira aplicada a um item da fileira "Continuar". Devolve 1 e
// preenche `saida` quando o card deve passar a oferecer o PROXIMO episodio; 0
// quando nada muda.
int prox_para_item(const CatItem *ci, const CatEp *eps, int n,
                   long long agoraMs, ProxSugestao *saida);

// O CARD DE "CONTINUAR ASSISTINDO" cujo episodio a pessoa TERMINOU. Mesma
// varredura de prox_para_item (ancora e o episodio valido seguinte), com duas
// diferencas, as duas pelo mesmo motivo — este item nao e semente de
// historico, e o episodio que ela acabou de ver:
//   - o limiar de "acabou" e o ajuste Percentual assistido (`limiarPct`), e
//     nao PROX_CONCLUIDO;
//   - a politica de serie nao acompanhada (janela de 60 dias) NAO vale: quem
//     esta maratonando uma serie antiga tem o proximo episodio lancado ha anos,
//     e a janela deixava o card preso no episodio terminado. So nao se oferece
//     episodio que ainda nao foi ao ar (data legivel no futuro); sem data
//     legivel, oferece — a lista do addon ja diz que o episodio existe.
// Devolve 0 quando nada muda: em andamento, sem lista, fim da serie, ou o
// proximo ainda nao estreou.
int prox_seguinte(const CatItem *ci, const CatEp *eps, int n, int limiarPct,
                  long long agoraMs, ProxSugestao *saida);

// O EPISODIO SEGUINTE a (temporada, episodio) que esta tocando: o menor par
// ESTRITAMENTE maior, nao "o indice seguinte" (2.0.3). Indice em `eps` ou -1.
//   - fim da temporada cai no primeiro episodio da seguinte que exista;
//   - episodio faltando na lista (E5 ausente) pula para o E6;
//   - entrada repetida do MESMO episodio (dois videos no /meta) nao e o proximo
//     — com "indice + 1" o cartao oferecia o episodio que estava tocando;
//   - temporada 0 (especiais) so e proximo de outro especial;
//   - o episodio corrente nem precisa estar na lista (numeracao que o addon
//     nao tem): o seguinte e o primeiro maior que ele.
// O cartao (posplay.c) e a janela (player.c) usam esta mesma regra: antes cada
// um tinha a sua e podiam discordar.
int prox_indice_seguinte(const CatEp *eps, int n, int temporada, int episodio);
#endif
