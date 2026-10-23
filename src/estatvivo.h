// ESTATISTICAS AO VIVO DO PLAYER (Central de controle > "Estatisticas",
// mockup stats-velocidade aprovado em 07/10): o que a reproducao esta fazendo
// AGORA — velocidade, buffer, fonte, servidor, erros e quadros —, cada numero
// com a ORIGEM escrita do lado. Esta metade e a CONTA, sem SDL, sem rede e sem
// desenho: tests/estatvivo.sh prende a regra. O cartao mora em estatvivoui.c.
//
// A ORIGEM NUNCA MENTE. Cada alvo so entrega o que o pipeline dele de fato
// informa (video_estatisticas, video.h), e o que falta ou e ESTIMADO aqui, com
// o selo "Estimada", ou fica "Sem dado":
//   Android  velocidade MEDIDA (bytes do socket), buffer em segundos, conexoes,
//            servidor final, 1o byte e quadros perdidos: tudo "Medido".
//   webOS    buffer em segundos "Medido"; velocidade ESTIMADA pelo ritmo em que
//            o buffer enche (segundos de video baixados por segundo x a taxa do
//            arquivo); servidor "Do link"; quadros "Sem dado".
//   Samsung  buffer "So em %"; velocidade ESTIMADA pelo ritmo da reproducao (a
//            TV nao da bytes nem segundos de buffer); servidor "Do link";
//            quadros "Sem dado".
// Erros e reconexoes sao do proprio app (video_reconectando/video_falhou):
// "Medido" em todos.
//
// CUSTO: uma amostra por segundo (ev_amostra), so leituras e soma; o
// historico e um anel de EV_HIST inteiros.
#ifndef NV_ESTATVIVO_H
#define NV_ESTATVIVO_H
#include "video.h"
#include <stddef.h>

#define EV_HIST 40   // pontos da linha de velocidade: os ultimos 40 s

typedef enum {
  EVO_SEM = 0,     // "Sem dado"
  EVO_MEDIDO,      // "Medido"
  EVO_ESTIMADA,    // "Estimada"
  EVO_SO_PCT,      // "So em %"
  EVO_ADDON,       // "Do add-on"
  EVO_LINK         // "Do link"
} EvOrigem;

// O rotulo do selo de cada origem, em portugues (a tela passa por i18n).
const char *ev_origem_rotulo(EvOrigem o);

// O que o app sabe NESTE segundo. `taxaKbps` e a taxa media do arquivo pelo
// que o add-on declara (tamanho / duracao), 0 sem ela: e o que converte
// "segundos de video" em bits na estimativa.
typedef struct {
  VideoStats vs;
  double pos;          // posicao, s
  int tocando;         // tocando de fato (nem pausado nem parado no buffer)
  int bufferando;      // parado esperando dados
  int reconectando;    // video_reconectando() != 0
  int falhou;          // video_falhou()
  int taxaKbps;
} EvEntrada;

typedef struct {
  // VELOCIDADE. kbps -1 = sem numero (nada medido nem estimavel).
  int kbps, velOrigem;
  int picoKbps, mediaKbps;
  int hist[EV_HIST], nHist;          // hist[nHist-1] e o mais novo
  // BUFFER. bufSeg < 0 = nao ha segundos; bufPct < 0 = nao ha porcentagem;
  // bufMb < 0 = sem taxa do arquivo para converter.
  double bufSeg, bufMb, bufAlvo;
  int bufPct, bufOrigem;
  // CONEXOES E SERVIDOR.
  int conexoes, primeiroByteMs, servOrigem;
  char host[96];
  // ERROS E RECONEXOES (do app). reconHora/reconMin: a hora da ultima.
  int erros, reconexoes, reconHora, reconMin;
  // QUADROS.
  long long perdidos, quadros;
  int quadrosOrigem;
  // Interno: a amostra anterior.
  unsigned tUlt;
  long long bytesUlt;
  double avancoUlt, posUlt;
  int temUlt, estavaRecon, estavaFalhou;
  long long somaKbps;
  int nMedia;
} EvEstado;

void ev_zerar(EvEstado *e);
// Uma amostra; chamar ~1x/s com o relogio monotono em ms. `hora`/`minuto` sao
// os da parede, para "1 reconexao as 21:12".
void ev_amostra(EvEstado *e, unsigned agoraMs, const EvEntrada *in, int hora, int minuto);

// Taxa media do arquivo em kbps pelo tamanho declarado e a duracao; 0 sem um
// dos dois.
int ev_taxa_arquivo(unsigned long long bytes, long tamanhoMB, double duracaoS);

// "42,8" (uma casa abaixo de 100), "412" dali para cima. `sep` ',' ou '.'.
void ev_fmt_mbps(char *dst, size_t n, int kbps, char sep);
// "0,01" — porcentagem de quadros perdidos com duas casas.
void ev_fmt_pct2(char *dst, size_t n, long long parte, long long todo, char sep);
// "41 280": milhar com espaco comum (toda fonte embarcada tem o glifo).
void ev_fmt_milhar(char *dst, size_t n, long long v);

#endif
