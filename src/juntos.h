// WATCH TOGETHER ("Assistir Juntos" em pt-BR) — a ligacao com o app.
//
// Plano: docs/plans/watch-together/assistir-juntos-plano.md. A regra do sync
// mora em juntossync/juntosmotor (puros, testados); a fonte em juntosfonte;
// as mensagens em juntosproto. Este modulo e a cola: as rotas /v1/sala* do
// nuvio-recomendacoes num fio proprio, o websocket da sala (discordws), o
// motor ligado ao player de verdade, os convites que chegam e o que a UI le.
//
// TUDO NO FIO PRINCIPAL nas funcoes abaixo; a rede roda em fio proprio e nunca
// segura um quadro. Sem NV_REC_URL no pacote, ou sem conta social, nada liga.
#ifndef NV_JUNTOS_H
#define NV_JUNTOS_H
#include <SDL2/SDL.h>
#include "juntosmotor.h"
#include "streams.h"

#define JUNTOS_CONVIDADOS_MAX 10

typedef enum {
  JF_OCIOSO = 0,
  JF_PREPARADO,     // anfitriao: a sala nasce quando a fonte estiver tocando
  JF_CRIANDO,       // POST /v1/sala no ar
  JF_ENTRANDO,      // POST /v1/sala/entrar no ar
  JF_CONECTANDO,    // websocket abrindo
  JF_DENTRO,
  JF_RECONECTANDO,  // caiu; o video segue pelo ultimo relogio
  JF_AGUARDANDO,    // entrada por codigo esperando o anfitriao aceitar
} JuntosFase;

int  juntos_disponivel(void);   // pacote com servico social e conta ligada
int  juntos_ativo(void);        // numa sala (de PREPARADO em diante)
JuntosFase juntos_fase(void);

// A cada quadro (ilhasinais_passo), com o player aberto ou nao.
void juntos_passo(Uint32 agora);
// Botao de um modal da ilha de chave "juntos:..." (ilhasinais.c entrega).
void juntos_acao(const char *chave, int botao);
void juntos_perfil_trocado(void);

// --- anfitriao ---------------------------------------------------------------------
// "Abrir sala" na folha: guarda o pedido; a sala e criada assim que a fonte
// escolhida estiver tocando (a impressao do release precisa da duracao).
void juntos_preparar(const char *imdb, int temporada, int episodio, const char *titulo, const char *poster,
                     const char *const *convidados, int nConvidados, int soAnfitriao);
// Convidar mais gente para a sala aberta (anfitriao).
void juntos_convidar(const char *const *convidados, int n);
int  juntos_controle_anfitriao(int soAnfitriao);   // a trava "So eu controlo"
const char *juntos_codigo(void);                   // "K7M2QX" da sala aberta ("" sem)

// --- convidado ------------------------------------------------------------------------
void juntos_entrar_id(const char *id);
void juntos_entrar_codigo(const char *codigo);
void juntos_recusar(const char *id);       // "Agora nao" num convite
void juntos_silenciar(const char *de);     // "Nao receber convites de X" ("*" = de ninguem)

// --- ambos --------------------------------------------------------------------------------
void juntos_sair(void);
int  juntos_comecar(void);       // "Comecar" no cartao da sala
int  juntos_encerrar(void);      // anfitriao: "Encerrar sessao"
void juntos_pedir_celular(void); // pede o codigo do QR; a URL sai em juntos_url_celular
const char *juntos_url_celular(void);   // "" ate a sala responder; vale 10 min

// --- app.c: abrir o titulo da sala e escolher a fonte pela impressao ---------------------
// 1 uma vez quando a sala pede que o app abra um titulo (o convidado entrou,
// ou a sala trocou de episodio). `alvo` = "tt..." ou "tt...:T:E".
int  juntos_pediu_abrir(char *imdb, size_t ni, int *temporada, int *episodio, char *titulo, size_t nt,
                        char *poster, size_t np);
// 1 enquanto a fonte deste titulo deve sair da impressao da sala.
int  juntos_escolhendo_fonte(void);
// Com a lista do alvo: o indice a tocar, ou -1 para esperar mais addons.
// `desdeMs` = quando a lista comecou a chegar.
int  juntos_escolher_fonte(const Stream *lista, int n, Uint32 agora, Uint32 desdeMs);

// --- player.c ---------------------------------------------------------------------------
// A pessoa pausou/continuou (OK, PAUSE). 1 = a sala cuidou (o player nao mexe
// no estado local; quem pausa e o motor, pelo relogio da sala).
int  juntos_ao_alternar(int tocandoAgora);
// A pessoa buscou (fim do avanco, pular abertura). 1 = a sala cuidou.
int  juntos_ao_buscar(double seg);
// O player pediu o proximo episodio. 1 = a sala cuidou (todos vao juntos).
int  juntos_ao_proximo(const char *alvo);
// VOLTAR no player com a sala aberta: 1 = a pergunta "Sair da sessao?" foi
// mostrada e o player nao deve fechar ainda.
int  juntos_ao_voltar(void);

// --- leitura para a UI (juntosui.c) -----------------------------------------------------
const JsMotor *juntos_motor(void);     // NULL fora de sala
const char *juntos_titulo(void);
int  juntos_sou_anfitriao(void);
int  juntos_pode_controlar(void);

#endif
