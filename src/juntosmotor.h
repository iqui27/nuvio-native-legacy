// WATCH TOGETHER — o motor do membro: o que uma TV faz com as mensagens da
// sala e com o proprio player. Sem SDL, sem rede, sem UI: o transporte e o
// player entram por ponteiro de funcao, e o relogio por parametro (`agora`, ms
// monotonicos). Assim o mesmo motor roda no app (juntos.c liga em video_* e no
// websocket) e no teste contra uma sala falsa com rede ruim
// (tests/juntosmotor.c).
//
// O que ele faz (plano, secao 6):
//  - rajada de 8 pings ao conectar, 3 a cada 60 s; relogio pela menor RTT;
//  - aplica o relogio da sala: pausa no quadro comum, play AGENDADO (t0 no
//    futuro, descontada a latencia de play medida), entra no meio extrapolando;
//  - a cada 500 ms decide a correcao (juntossync) e aplica no player;
//  - mede latencia de seek e de play, e prova a velocidade: se o empurrao de
//    0,95x/1,05x nao anda, cai para o nivel B pelo resto da sala;
//  - reporta o proprio estado (carregando/pronto/tocando), inclusive pela
//    PARADA DE POSICAO (o .wgt nao tem sinal de buffering);
//  - junta a rajada de seek do D-pad: so manda o ultimo, 700 ms depois;
//  - pausa otimista: quem aperta pausa para na hora.
// Eventos para a UI saem numa fila (jsm_evento).
#ifndef NV_JUNTOSMOTOR_H
#define NV_JUNTOSMOTOR_H
#include "juntosproto.h"
#include "juntossync.h"

typedef struct {
  double   (*pos)(void *u, double *tAmostraMs);   // ultima posicao lida e quando
  int      (*tocando)(void *u);                   // video andando de verdade
  int      (*pronto)(void *u);                    // fonte aberta e primeiro quadro carregado
  unsigned (*bufferando_ms)(void *u);
  double   (*duracao)(void *u);
  void     (*pausar)(void *u, int pausado);
  void     (*buscar)(void *u, double seg);
  void     (*velocidade)(void *u, int centesimos);
  void *u;
} JsPlayer;

typedef int (*JsEnviar)(void *u, const char *json);

typedef enum {
  JSV_NADA = 0,
  JSV_SALA,          // o retrato da sala mudou (membros, estado): redesenhar
  JSV_ACAO,          // alguem agiu: k = op (play pausa seek proximo comecar esperar prazo pronto), quem = autor, n = delta s
  JSV_AVISO,         // k = entrou voltou ausente saiu anfitriao esperando seguindo; quem; n = prazo
  JSV_REACAO, JSV_FRASE,   // k, quem
  JSV_CHAT,          // quem, txt
  JSV_PEDIDO,        // entrada por codigo: quem, nome, txt = avatar (so o anfitriao recebe)
  JSV_PEDIR_PAUSA,   // quem
  JSV_RECUSADO,      // k = so_anfitriao | velho | rapido | anfitriao ; quem = anfitriao
  JSV_EPISODIO,      // a sala trocou de episodio: txt = ep
  JSV_SINCRONIZANDO, // pulo de sync grande (> 2 s)
  JSV_FROUXO,        // seeks demais: tolerancia de 2 s daqui em diante
  JSV_NIVEL_B,       // a velocidade nao anda nesta TV
  JSV_AGUARDANDO, JSV_ACEITO,
  JSV_CELULAR,       // k = codigo do QR do celular (a UI monta web + sala + codigo)
  JSV_FIM,           // k = motivo, n = duracao da sala (s)
  JSV_ERRO,          // k
} JsEvTipo;

typedef struct {
  JsEvTipo tipo;
  char k[32], quem[JSP_ID], nome[JSP_NOME], txt[256];
  int n;
} JsEvento;

#define JSM_FILA 24

typedef struct {
  char eu[JSP_ID], plat[12], app[16];
  JsPlayer p;
  JsEnviar enviar; void *envU;
  JsRelogio rel;
  JsSync sync;
  JsSala sala; int temSala;
  int conectado;
  // relogio
  double proxPing; int rajada; int pongs;
  // correcao
  double proxTick;
  int velCent;                    // velocidade que o motor pediu (100 = normal)
  // play agendado e o que o motor quer do player
  int queroTocar; double ordemEm;
  int playArmado; double playEm;
  int medindoPlay; double playPedidoEm, playPedidoPos;
  int seekEmCurso; double seekEm, seekPos, seekRef;
  // estado reportado a sala
  char stApp[16]; int pctApp;
  char stEnviado[16]; int pctEnviado; double estadoEm;
  char fonte[12];
  // parada de posicao
  double ultPos, ultPosEm;
  // rajada de seek do D-pad
  int rajadaSeek; double rajadaPos, rajadaEm;
  int ecoSeq; double ecoAte;      // comando proprio esperando o eco da sala
  double tocandoDesde;
  // fila
  JsEvento ev[JSM_FILA]; int evIni, evN;
  int frouxoAvisado;
} JsMotor;

void jsm_iniciar(JsMotor *m, const char *eu, const char *plat, const char *app, char nivel,
                 const JsPlayer *p, JsEnviar enviar, void *envU);
void jsm_conectou(JsMotor *m, double agora);   // socket aberto: rajada de pings e "ola"
void jsm_caiu(JsMotor *m);                     // socket caiu: segue o ultimo relogio
void jsm_receber(JsMotor *m, const char *json, double agora);
void jsm_passo(JsMotor *m, double agora);      // a cada quadro

// Antes do player abrir: "procurando", "carregando" (pct), "semfonte", "erro".
// Depois que o player fica pronto, o motor deriva o estado sozinho.
void jsm_estado_app(JsMotor *m, const char *st, int pct);
void jsm_fonte(JsMotor *m, const char *tipo);  // mesma parecida diferente nenhuma

// Comandos da pessoa. 1 = mandado; 0 = recusado aqui (trava do anfitriao: a UI
// mostra "So X controla"); -1 = sem conexao.
int jsm_cmd(JsMotor *m, const char *op, double pos, const char *ep, double agora);
// D-pad repetindo: o player ja buscou localmente; a sala recebe so o ultimo.
int jsm_seek_rajada(JsMotor *m, double pos, double agora);
int jsm_reacao(JsMotor *m, const char *k);
int jsm_frase(JsMotor *m, const char *k);
int jsm_pedir_pausa(JsMotor *m);
int jsm_aceitar(JsMotor *m, const char *quem, int ok);
int jsm_controle(JsMotor *m, int soAnfitriao);
int jsm_sair(JsMotor *m);
int jsm_pedir_celular(JsMotor *m, int chat);

int  jsm_evento(JsMotor *m, JsEvento *e);       // 1 = tirou um
int  jsm_sou_anfitriao(const JsMotor *m);
int  jsm_pode_controlar(const JsMotor *m);
const JsMembro *jsm_membro(const JsMotor *m, const char *id);
double jsm_pos_alvo(const JsMotor *m, double agora);

#endif
