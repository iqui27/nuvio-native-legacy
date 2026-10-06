// WATCH TOGETHER — a matematica do sync, PURA (sem SDL, sem video, sem rede).
//
// Plano: docs/plans/watch-together/assistir-juntos-plano.md, secao 6. Tudo em
// milissegundos de um relogio MONOTONICO local (SDL_GetTicks64 no app, um
// contador no teste), nunca a hora de parede: a TV pode estar com o fuso ou a
// hora errados, e isso nao pode mudar nada.
//
// Ninguem transmite posicao a cada segundo. O servidor manda um par
// (pos0, t0) em ms do relogio DELE, e cada TV deriva:
//   posAlvo = estado == tocando ? pos0 + (agoraServidor - t0) / 1000 : pos0
// O relogio (JsRelogio) estima agoraServidor = agoraLocal + offset, no estilo
// NTP (amostra de menor RTT). A decisao (jss_decidir) compara a posicao local
// com o alvo e devolve UMA acao: nada, velocidade levemente alterada (nivel A,
// onde a plataforma prova que muda) ou seek (nivel B, ou desvio grande).
//
// Por que modulo separado do player: e o que sobrevive a uma reescrita do
// player (a Plex perdeu o recurso assim). Testes: tests/juntossync.c.
#ifndef NV_JUNTOSSYNC_H
#define NV_JUNTOSSYNC_H

// ---- relogio (6.2) ----------------------------------------------------------------
#define JSR_AMOSTRAS 8
typedef struct {
  double rtt[JSR_AMOSTRAS], off[JSR_AMOSTRAS];
  int n, i;         // amostras guardadas, proxima posicao no anel
  int tem;          // 1 depois da primeira amostra
  double offset;    // ms: servidor = local + offset
  double melhorRtt; // RTT da amostra que deu o offset
} JsRelogio;

void   jsr_zerar(JsRelogio *r);
// Um pong: `c` e o instante local em que o ping saiu (eco do servidor), `s` o
// relogio do servidor quando respondeu, `agora` o instante local da chegada.
// Fica a amostra de menor RTT entre as ultimas 8; o offset anda por EMA
// (alfa 0,3), e saltos acima de 250 ms entram direto (TV acordou do repouso).
void   jsr_amostra(JsRelogio *r, double c, double s, double agora);
double jsr_servidor(const JsRelogio *r, double agoraLocal);
double jsr_local(const JsRelogio *r, double servidor);

// ---- relogio da sala (6.1) -----------------------------------------------------------
enum { JSE_LOBBY = 0, JSE_TOCANDO, JSE_PAUSADO, JSE_ESPERANDO, JSE_FIM };
typedef struct {
  int estado;
  double pos0;   // s de midia
  double t0;     // ms do relogio do SERVIDOR
  int seq;
} JsRel;
double jsr_pos_alvo(const JsRel *rel, double agoraServidor);
int    jsr_estado_de(const char *nome);   // "tocando" -> JSE_TOCANDO; desconhecido -> -1

// Posicao local entre duas leituras do backend (LG le a cada ~200 ms):
// pos = posAmostra + (agora - tAmostra) * taxa, so tocando.
double jss_pos_local(double posAmostra, double tAmostraMs, double agoraMs, int tocando, double taxa);

// ---- decisao de correcao (6.5) ---------------------------------------------------------
#define JSS_ZONA_A        0.08   // s: abaixo disto, nada
#define JSS_ZONA_B        0.60   // s: tolerancia de quem so tem seek
#define JSS_VEL_MAX       1.50   // s: acima disto e seek, mesmo no nivel A
#define JSS_AVISO_SEEK    2.0    // s: seek maior que isto mostra "Sincronizando..."
#define JSS_FROUXO        2.0    // s: tolerancia depois de seeks demais
#define JSS_FROUXO_MS     120000.0  // ...por dois minutos
#define JSS_COOLDOWN_MS   3000.0
#define JSS_SEEKS_MIN     3      // seeks de sync por minuto antes do "sync frouxo"
#define JSS_VEL_LENTA     95     // centesimos: adiantado
#define JSS_VEL_RAPIDA    105    // centesimos: atrasado
#define JSS_PASSO_MS      500.0  // a decisao roda a cada meio segundo
#define JSS_SEGURAR_MAX   10.0   // s: adiantado mais que isto e seek, nao pausa

// SEGURAR (nivel B): quem esta ADIANTADO para por `ms` e solta no instante em
// que o relogio da sala o alcanca. Quem so tem seek corrige o atraso buscando
// um pouco A FRENTE (a margem aprendida cobre o keyframe) e depois segura: a
// precisao vira a do play, que e medido, e nao a do keyframe da fonte.
typedef enum { JSA_NADA = 0, JSA_VELOCIDADE, JSA_SEEK, JSA_SEGURAR } JsAcaoTipo;
typedef struct {
  JsAcaoTipo tipo;
  double pos;     // JSA_SEEK: para onde
  double ms;      // JSA_SEGURAR: quanto tempo parado
  int cent;       // JSA_VELOCIDADE: centesimos (100 = volta ao normal)
  int aviso;      // 1 = mostrar "Sincronizando..." (pulo grande)
  int frouxo;     // 1 = entrou em sync frouxo AGORA (o chamador loga uma vez)
} JsAcao;

typedef struct {
  char nivel;            // 'A' (velocidade provada ou a provar) ou 'B' (so seek)
  double cooldownAte;    // ms local
  double seeks[JSS_SEEKS_MIN]; int nSeeks;   // instantes dos ultimos seeks de sync
  int frouxo; double frouxoAte;
  double velAte; int velCent;                 // empurrao de velocidade em curso
  double velT0, velP0;                        // para medir a taxa efetiva
  int velFalhas;                              // empurroes que nao andaram
  double latSeekMs;      // media movel: quanto o alvo anda durante um seek
  int nLatSeek;
  int residualOk;        // nivel B: um segundo seek curto, uma vez
  double margemSeek;     // s a mais no alvo do seek do nivel B (keyframe aprendido)
  double seguraAte;      // ms local: proximo "segurar" so depois disto
  double latPlayMs;      // media movel: pedido de play -> posicao andando
  int nLatPlay;
} JsSync;

void jss_zerar(JsSync *s, char nivel);
// A cada JSS_PASSO_MS. `desvio` = posLocal - posAlvo (s; positivo = adiantado).
// `valido` = 0 enquanto pausado, buscando, em buffering ou com a sala fora de
// "tocando": encerra um empurrao em curso e nao decide nada.
JsAcao jss_decidir(JsSync *s, double agoraMs, double posLocal, double posAlvo, int valido);
// O seek terminou (a posicao voltou a andar): `ms` desde o pedido e onde caiu
// contra onde devia estar AGORA (residuo, s). Alimenta latSeekMs e libera o
// segundo seek curto do nivel B.
void jss_seek_concluiu(JsSync *s, double ms, double residuo);
// Play pedido -> posicao andando, em ms (play agendado usa a media).
void jss_play_concluiu(JsSync *s, double ms);
// Instante LOCAL para pedir o play de um t0 do servidor: t0 - offset - latPlay.
double jss_instante_play(const JsSync *s, const JsRelogio *r, double t0Servidor);
// Medida do empurrao de velocidade: chamada quando ele acaba. Devolve 1 se a
// plataforma NAO andou na velocidade pedida duas vezes (o motor cai para B).
int jss_vel_medida(JsSync *s, double agoraMs, double posAgora);

#endif
