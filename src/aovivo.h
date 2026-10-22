// PLAYER DE TV AO VIVO — o OSD proprio dos canais, no lugar dos controles de
// filme. Pedido do dono: "o player da live TV tem que ser personalizado, com
// botoes e informacoes uteis e a logo do canal, diferente do player
// tradicional".
//
// O QUE ESTE MODULO E: a parte que nao depende do player.c — a conta do zapping
// com debounce, a escolha do programa "agora / a seguir" e o desenho do OSD, do
// banner de zapping e do cartao de erro. Quem sabe se o canal esta tocando,
// qual botao esta em foco e o que o OK faz continua sendo o player.c; aqui
// entra tudo pronto em AoVivoOsd. Assim o desenho fotografa sem pipeline de
// video (tests/aovivo_shot) e a conta se testa sem janela (tests/aovivo.c).
#ifndef NV_AOVIVO_H
#define NV_AOVIVO_H
#include <SDL2/SDL.h>
#include <time.h>
#include "epg.h"
#include "gfx.h"

// --- ZAPPING COM DEBOUNCE -----------------------------------------------------
// Apertar CH+ tres vezes seguidas nao pode abrir tres canais (cada um custa uma
// busca de fonte e uma sessao de video). Cada toque soma ao deslocamento e
// empurra o relogio; o canal so troca depois de AV_ZAP_MS sem novo toque, e
// aponta direto para o alvo somado. Enquanto isso o banner mostra quem vai
// tocar. CH+ e CH- que se cancelam nao trocam nada.
#define AV_ZAP_MS 600u
#define AV_ZAP_MAX 200   // teto do deslocamento: mais que isso e tecla presa
typedef struct { int pend; Uint32 ultimo; } AoVivoZap;
void aovivo_zap_apertar(AoVivoZap *z, int dir, Uint32 agora);
// 1 quando o prazo venceu COM deslocamento: `*offset` recebe o total e o estado
// zera. Vencido a zero, zera e devolve 0 (nada a trocar).
int  aovivo_zap_pronto(AoVivoZap *z, Uint32 agora, int *offset);
// A posicao `offset` canais depois (negativo: antes) de `atual` numa lista de
// `n`, dando a volta nas pontas. -1 com lista vazia; `atual` fora da faixa
// conta como 0 (canal que saiu da lista).
int  aovivo_ordem(int n, int atual, int offset);

// --- AGORA / A SEGUIR ---------------------------------------------------------
typedef struct {
  int temAgora, temProx;
  char agoraTit[160], proxTit[160];
  time_t agoraIni, agoraFim, proxIni;
  float progresso;   // 0..1 do programa no ar
} AoVivoEpg;
// Escolha pura sobre uma lista ORDENADA por inicio: o programa que cobre `t`
// (o de inicio mais recente, se a grade sobrepor) e o primeiro que comeca
// quando ele acaba. Sem programa cobrindo `t`, o proximo e o primeiro que ainda
// nao comecou. Devolve os indices em *iAg/*iPx (-1 = nao ha).
void aovivo_selecionar(time_t t, const EpgProg *l, int n, int *iAg, int *iPx);
// Monta a partir da grade: `epgIdx` >= 0 usa a XMLTV (epg.c); senao, `xtId`
// (canal Xtream) usa a grade curta do painel (xtepg.c). 1 se achou algo.
int  aovivo_epg_montar(int epgIdx, const char *xtId, time_t t, AoVivoEpg *o);

// --- OSD ----------------------------------------------------------------------
// Os botoes, na ordem em que aparecem. Quem monta a fileira decide quais
// existem (Favorito some sem lista de guia; Pausa so quando o fluxo pausa).
enum { AV_B_PAUSA, AV_B_GUIA, AV_B_ANT, AV_B_PROX, AV_B_FAV, AV_B_AUDIO,
       AV_B_LEGENDA, AV_B_INFO, AV_B_RECARREGAR, AV_B_FONTE,
       AV_B_AOVIVO,   // "Voltar ao vivo": so atras da transmissao (depois do Pausar)
       AV_B_ASPECTO,  // proporcao/zoom, os modos do player de filme
       AV_B_N };

#define AV_INFO_LINHAS 7
typedef struct {
  const char *nome, *categoria, *logo, *desc;
  int numero, total;                 // posicao na ordem do guia; 0 = desconhecida
  AoVivoEpg epg;
  int nBotoes, botoes[AV_B_N];       // ids AV_B_*, na ordem de desenho
  int foco;                          // indice em botoes[]; -1 = nenhum
  int favorito, pausado, bufferando;
  // Canal com janela de tempo (DVR): quanto atras do ao vivo (s), ha quanto
  // tempo pausado (s) e o tamanho da janela para voltar (s). 0 = nao se aplica.
  int atrasoS, pausaS, janelaS;
  char res[16];                      // "4K", "1080p", "720p" (viram marca), "SD" ou ""
  const char *aspecto;               // rotulo do modo de proporcao em vigor
  int infoAberta, nInfo;
  char info[AV_INFO_LINHAS][72];     // "Rotulo: valor"
  // As MESMAS marcas do OSD de filme (FormatoMarca) no topo do painel de
  // Informacoes, no lugar de "Imagem: HDR10" em texto.
  int nMarcas, marcas[4];
} AoVivoOsd;
// Foco, barra e botoes leem o acento de ajustes_acento() (via botoes.c), como
// o guia: o OSD nao recebe cor de quem chama.
const char *aovivo_rotulo(int botao);
// O OSD NAO desenha a hora nem as Informacoes: as duas sao a ilha do relogio
// do player (plrilha.h). player.c chama plrilha_relogio e, com infoAberta,
// pede a ilha com o corpo abaixo (u = o AoVivoOsd, vivo ate o proximo quadro).
void aovivo_osd_desenhar(const AoVivoOsd *o, float a);
float aovivo_info_altura(const AoVivoOsd *o);
void aovivo_info_corpo(GfxRect c, float a, void *u);

// Banner do zapping: quem vai tocar quando o debounce vencer.
typedef struct {
  const char *nome, *logo, *agoraTit;
  int numero, salto;                 // salto: quantos canais desde o atual (+/-)
} AoVivoBanner;
void aovivo_banner_desenhar(const AoVivoBanner *b, float a);

// Cartao de erro no estilo do modo ao vivo (causas do provedor / da conta).
// Os botoes do modal do erro do canal (Recarregar, Fonte, Guia): `foco` em
// 0..2. Quem trata a tecla e player.c.
void aovivo_erro_foco(int foco);
void aovivo_erro_desenhar(const char *nome, const char *logo, const char *titulo,
                          const char *dica, float a);
#ifdef NV_TOUCH_UI
typedef void (*AoVivoToqueFn)(int acao, int erro, int quadro);
// The player owns channel/state validation and the existing action dispatch.
void aovivo_toque_definir(AoVivoToqueFn fn, int quadro);
void aovivo_toque_reiniciar(void);
#endif

#endif

