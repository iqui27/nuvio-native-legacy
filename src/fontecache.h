// Cache de fontes POR CANAL, e o prefetch dos vizinhos no guia.
//
// O QUE ELE ENCURTA, e o que nao conserta. Abrir um canal pergunta as fontes a
// todos os addons e so entao toca; zapear e repetir essa espera a cada canal.
// Com o vizinho de cima e o de baixo ja perguntados enquanto a pessoa ainda
// esta lendo o guia, o zap para eles nao espera a rede. Isto NAO mexe no
// tempo que um addon leva para responder — o relato de "fonte demora minutos"
// continua valendo para o primeiro canal, para quem pula tres de uma vez e para
// todo canal cuja lista nao esta aqui. O cache tira a espera do caso comum;
// a lentidao em si mora nos addons e em addons.c.
//
// POR QUE E UM MODULO A PARTE. addons.c guarda UM alvo por vez (alvoId,
// resultado, estado, pendId): buscar o vizinho por aquele caminho atropelaria a
// busca do canal que a pessoa acabou de pedir. Entao a busca do vizinho roda
// por addons_consultar — a mesma consulta, mas reentrante, sem tocar em nada
// daquilo — e o resultado fica aqui, chaveado por id, ate alguem pedir.
//
// REGRAS, todas visiveis daqui:
//   - So canal ao vivo (tipo "tv"/"channel"). Filme e serie ja tem renovacao
//     de link propria em app.c (NV_LINK_VALIDO_MS) e nao zapeiam.
//   - Validade CURTA (FONTECACHE_VALIDADE_MS): o link de canal e assinado e
//     expira; ver stream_idade_ms em streams.h.
//   - ACERTO CONSOME a entrada. A lista guardada e um "proximo passo pronto",
//     nunca uma fonte para tentar de novo: quem reabre um canal que acabou de
//     falhar vai a rede, como antes.
//   - Lista VAZIA nao entra. Zero fontes e indistinguivel de rede fora, e o
//     pedido real tem de ir ver por si.
//   - O prefetch so ARRANCA com a busca principal ociosa e o player parado, e
//     CEDE (fontecache_ceder) assim que um pedido real vai a rede.
#ifndef NV_FONTECACHE_H
#define NV_FONTECACHE_H
#include "streams.h"

// --- a chamada do guia --------------------------------------------------------
//
// O foco do guia mudou: `idAntes` e `idDepois` sao os ids dos canais vizinhos
// (o de cima e o de baixo na lista publicada — os mesmos que CH-/CH+ dariam).
// Qualquer um pode ser NULL ou "" (ponta da lista). Volta na hora; nada
// bloqueia. Chamar a cada movimento e barato e e o esperado: o arranque so
// acontece quando o foco DESCANSA (FONTECACHE_ESPERA_MS), entao segurar a seta
// nao dispara nada pelo caminho.
//
// ALCANCE: um vizinho de cada lado, e so. Cada prefetch e uma requisicao a
// CADA addon de fonte por um canal que a pessoa talvez nunca abra; dois por
// parada de foco e o que cobre CH+ e CH- sem transformar a leitura do guia num
// rastreador de rede. A fileira inteira custaria N vezes isso por parada.
// `base*` e o addon que publicou cada canal (GCanal.base no guia): o prefetch
// pergunta so a ele. NULL/vazio = a todos.
void fontecache_engatilhar(const char *idAntes, const char *baseAntes,
                           const char *idDepois, const char *baseDepois);

// --- o lado de addons.c ---------------------------------------------------------

// A CONSULTA AOS ADDONS, SINCRONA E REENTRANTE — definida em addons.c e
// declarada aqui porque addons.h nao pode depender de streams.h (ver la).
// Pergunta a todos os addons de fonte por `id`/`tipo` com ate `fios` fios em
// paralelo e devolve a lista em *saida (o chamador libera com free). Nao toca no
// alvo, no estado nem no resultado de addons_buscar — e o que permite ao
// prefetch correr ao lado de uma busca real sem atropela-la. BLOQUEIA: chamar
// de fio proprio.
//
// `cancelado(ctx)`, quando dado, e consultado entre um addon e outro; devolvendo
// 1, os addons que faltam nao sao perguntados, o que ja veio e descartado e a
// funcao devolve -1 com *saida NULL. Devolve o numero de fontes (0 = nenhum
// addon respondeu com fonte) nos demais casos.
int  addons_consultar(const char *id, const char *tipo, const char *base, int fios,
                      int (*cancelado)(void *), void *ctx, Stream **saida);

enum { FC_NADA = 0, FC_ACERTO, FC_EM_CURSO };

// Uma chamada so, sob uma trava so, para que "acabou de chegar" e "ainda esta
// baixando" nao se percam entre duas perguntas:
//   FC_ACERTO    *lista/*n recebem uma copia (o chamador libera com free) e a
//                entrada e CONSUMIDA.
//   FC_EM_CURSO  o prefetch deste id esta na rede agora e NAO foi cancelado:
//                vale esperar por ele em vez de repetir as mesmas requisicoes.
//   FC_NADA      nao ha nada: ir a rede.
int  fontecache_pegar(const char *id, const char *tipo, Stream **lista, int *n);

// Guarda uma lista que a busca principal acabou de receber (o canal que esta
// no ar), para o zap de volta. Copia; o chamador continua dono de `lista`.
// Lista vazia, tipo que nao e canal ou lista maior que o teto por entrada nao
// entram — e isso e dito no log.
void fontecache_guardar(const char *id, const char *tipo, const Stream *lista, int n);

// UM PEDIDO REAL VAI A REDE AGORA. O prefetch em curso, se houver, para de
// pedir aos addons que faltam e descarta o que juntou: uma lista pela metade
// nao pode ficar guardada como se fosse inteira. O download que ja esta no ar
// termina sozinho (libcurl nao interrompe), por isso o custo do cancelamento e
// no maximo UMA resposta a mais chegando.
void fontecache_ceder(void);

// A busca principal esta OCIOSA neste quadro: se ha prefetch pendente e as
// condicoes deixam, arranca. addons_estado chama isto por quadro; e o que faz
// o segundo vizinho ser buscado depois do primeiro, e o que faz um pedido
// engatilhado durante uma busca real acontecer quando ela termina.
// O CHAMADOR garante que a busca principal esta ociosa: esta funcao nao
// pergunta, porque e de dentro de addons_estado que e chamada.
void fontecache_avancar(void);

// Cancela e espera o fio de prefetch, e esvazia o cache. addons_encerrar chama.
void fontecache_encerrar(void);

// Quantas entradas VALIDAS ha agora. Diagnostico e teste.
int  fontecache_n(void);

// --- dimensoes, expostas porque o teste as exercita ------------------------------
//
// TETO DE ENTRADAS E DE FONTES, com a conta. sizeof(Stream) e 7680 bytes
// (url[4096] + descricao[2048] + arquivo[512] + cabecalhos[512] + o resto;
// medido com o compilador do Mac, e o alinhamento no ARM/Wasm nao muda a
// ordem de grandeza). O pior caso do cache inteiro e
//   FONTECACHE_MAX * FONTECACHE_FONTES_MAX * 7680 = 4 * 16 * 7680 = 491.520 B,
// 480 KiB, ou 0,18% dos 256 MiB fixos do heap no Tizen. A memoria so e pedida
// quando a entrada e usada (malloc por entrada), entao o caso comum e menor.
//
// Quatro entradas: o canal no ar (guardado pela busca real, para o zap de
// volta), os dois vizinhos, e uma de folga para o vizinho que acabou de ser
// deixado para tras nao expulsar o que ainda serve.
//
// Dezesseis fontes por canal: canal ao vivo raramente passa de dez, e quem
// passa de dezesseis nao e cortado — e deixado fora do cache, para o cache
// nunca entregar uma lista diferente da que a rede entregaria.
#define FONTECACHE_MAX        4
#define FONTECACHE_FONTES_MAX 16

// VALIDADE. Metade de NV_LINK_VALIDO_MS (60 s, app.c): stream_definir_lista
// recomeca stream_idade_ms do zero quando o cache e servido, entao o app nao
// tem como saber que a lista ja tinha idade. Trinta segundos de cache mais o
// tempo de abrir ficam abaixo do que o app ja aceita para um link. Mais que
// isso e entregar link expirado com cara de fresco — que e exatamente o modo
// de falha que stream_idade_ms existe para evitar.
#define FONTECACHE_VALIDADE_MS 30000

// DESCANSO DO FOCO antes de arrancar. O auto-repeat da seta no controle da LG
// fica em torno de 100 ms; 350 ms so acontece quando a pessoa parou num canal.
#define FONTECACHE_ESPERA_MS 350

// Fios de rede do prefetch: dois, e nao os ADD_FIOS (4) da busca real. O
// prefetch divide o mesmo enlace com o pedido real que pode chegar a qualquer
// momento; metade dos fios e metade da concorrencia que ele impoe.
#define FONTECACHE_FIOS 2

#endif
