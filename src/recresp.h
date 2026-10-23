// RESPONDER A UMA RECOMENDACAO RECEBIDA (dono, 03/10/2026): "Ja assisti" na
// aba Amigos, e depois — se a pessoa quiser — gostei / nao gostei e uma
// mensagem curta para quem mandou. A rec vai para o grupo "Assistidas".
//
// TAMBEM E AUTOMATICO: quando o player conclui um titulo (ou um episodio de
// uma serie) que veio de uma recomendacao, atividade.c chama
// recresp_marcar_assistida com o id dela — a MESMA regra de "fim" que o resto
// do app usa (player_regra_concluiu ou ATIV_FIM_PCT). O cartao "O que achou?"
// dos creditos (reacao.c) e quem pergunta a resposta, sem um segundo cartao.
//
// O ESTADO E DESTE APARELHO PRIMEIRO. Fica em recomendacoes-respostas.txt (uma
// linha por rec) e so depois vai ao servidor (POST /v1/rec/resposta, enviado
// pelo fio de recomenda.c). Falhar a rede nao desfaz nada: a linha continua
// "nao enviada" e sai no proximo ciclo. A lista de recs (recomenda.c) nao
// guarda isto de proposito: ela e reescrita pelo servidor; esta, nao.
//
// SEM NUVIO_REC_URL nada daqui e chamado (quem desenha esconde a aba inteira),
// mas as funcoes continuam seguras: guardam e nunca enviam.
#ifndef NV_RECRESP_H
#define NV_RECRESP_H
#include <stddef.h>

#define RECRESP_SEM_REACAO  (-9)   // a pessoa nao disse se gostou
#define RECRESP_TEXTO_MAX   60     // o mesmo TEXTO_MAX do servidor
#define RECRESP_MAX         120    // linhas guardadas (o dobro de REC_MAX)

typedef struct {
  long long rec;             // RecItem.id
  int  assistida;            // 1 = foi para "Assistidas"
  int  respondida;           // 1 = respondeu OU disse "agora nao": nao pergunta mais
  int  reacao;               // 1 | 0 | -1 | RECRESP_SEM_REACAO
  char texto[RECRESP_TEXTO_MAX + 4];
  long long quando;          // epoch da ultima mudanca
  unsigned versao, enviada;  // versao > enviada = o servidor ainda nao sabe
} RecResp;

// Os tres gestos. Todos gravam NA HORA e marcam a linha para envio.
//   evento ASSISTIU   "Ja assisti" (aba) ou fim no player. Nao mexe na resposta.
//   evento RESPONDEU  reacao e/ou texto. Tambem conta como assistida.
//   evento PULOU      "Agora nao": assistida e respondida, sem reacao.
enum { RECRESP_EV_ASSISTIU = 1, RECRESP_EV_RESPONDEU, RECRESP_EV_PULOU };

// --- pura, para o teste ----------------------------------------------------------
// Aplica `ev` em `r` (que pode estar zerada, com r->rec preenchido). Devolve 1
// quando algo mudou — e so entao a versao sobe. RESPONDEU com reacao
// RECRESP_SEM_REACAO nao apaga uma reacao ja dada; texto NULL/"" nao apaga um
// texto ja dado (o mesmo contrato do servidor).
int  recresp_aplicar(RecResp *r, int ev, int reacao, const char *texto, long long agora);
// Corpo JSON de POST /v1/rec/resposta para `r`. Devolve o tamanho, 0 se nao coube.
size_t recresp_json(const RecResp *r, char *dst, size_t tam);

// Funde o estado que o SERVIDOR tem de uma rec recebida (GET /v1/rec) em `r`,
// para "Assistidas" valer nos outros aparelhos da mesma pessoa:
//   terminou    rec.terminou (1 = assistida em algum aparelho)
//   reacao      1 | 0 | -1, ou RECRESP_SEM_REACAO (null)
//   texto       a mensagem (pode vir vazia)
//   respondido  epoch da resposta, 0 = nunca respondeu
// REGRAS: linha com mudanca local NAO enviada (versao > enviada) nao e tocada —
// o que a pessoa fez aqui vence ate o servidor confirmar. Fora isso o servidor
// so ACRESCENTA (assistida/respondida nunca voltam atras) e sua reacao/mensagem
// vence quando `respondido` e mais novo que a ultima mudanca local, ou quando
// aqui nao ha valor. A linha fundida fica "enviada" (nada a reenviar).
// Devolve 1 se algo mudou.
int  recresp_mesclar(RecResp *r, int terminou, int reacao, const char *texto,
                     long long respondido, long long agora);

// --- estado ----------------------------------------------------------------------
int  recresp_ler(long long rec, RecResp *saida);   // 1 = ha linha
int  recresp_assistida(long long rec);
int  recresp_respondida(long long rec);
void recresp_marcar_assistida(long long rec);
void recresp_responder(long long rec, int reacao, const char *texto);
void recresp_pular(long long rec);
// Sobe a cada mudanca local; quem desenha remonta a lista quando muda.
unsigned recresp_revisao(void);
// recresp_mesclar sobre a linha de `rec` (criada se preciso). Nao faz nada para
// um estado vazio (nem assistida, nem resposta, nem reacao).
void recresp_do_servidor(long long rec, int terminou, int reacao, const char *texto,
                         long long respondido);

// --- fio de rede (recomenda.c) ---------------------------------------------------
// A proxima linha a enviar: copia o corpo e devolve 1, com `rec`/`versao` para
// confirmar. 0 = nada pendente.
int  recresp_pendente(char *corpo, size_t tam, long long *rec, unsigned *versao);
// O servidor respondeu 2xx para `rec` na `versao`.
void recresp_confirmar(long long rec, unsigned versao);
// Sair da conta / trocar de perfil (recomenda_esquecer).
void recresp_esquecer(void);
void recresp_abrir_sessao(void); // invalida memoria sem apagar respostas salvas
#endif
