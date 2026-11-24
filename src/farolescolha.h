// FAROLESCOLHA — o assistente de escolha P5 sem LLM (lote sense, issue 50;
// PLANO-GOSTO secoes 3.4, 4.2 P5, 4.3, 4.5 e 6 issue 50).
//
// MODULO PURO como os irmaos (farolficha, farolgosto, farolrec, farolpeca):
// sem SDL, sem rede, sem dados.h, sem malloc, sem log, sem relogio e sem
// estado estatico — o estado inteiro mora no FarolEsc de quem chama.
//
// O BANCO. 9 perguntas fixas (FESC_*), cada uma com 2 ou 3 opcoes de filtro
// mais "Tanto faz" (sempre a ULTIMA opcao). Os rotulos sao as chaves FT_ESC_*
// de src/faroltexto.h; os ids ("tempo", "t45", "tanto"...) sao os da cena P5
// e os do /v1/farol/escolha (issue 54).
//
// OS CANDIDATOS. Quem chama resume cada titulo num FarolEscCand: pontos (a
// ordem final) e, por pergunta, a MASCARA das opcoes de filtro que o titulo
// atende (bit o = opcao o). Mascara 0 = sem dado: o titulo nao atende opcao
// nenhuma daquela pergunta (nunca inventa). farolescolha_cand_de monta a
// mascara a partir de fatos simples (duracao, ano, genero, visto...).
//
// O QUE O MOTOR GARANTE (aceite da issue 50)
//   - Nunca faz a 4a pergunta: no maximo FESC_PERG_MAX (3) perguntas feitas,
//     menos as que o #54 ja pre-respondeu (farolescolha_pre_responder).
//   - A proxima pergunta e a que mais DIVIDE os vivos de agora (ganho de
//     informacao pela impureza de Gini: soma de c_o * (N - c_o) sobre as
//     opcoes oferecidas; empate = ordem do banco). Pergunta com ganho 0 (nao
//     separa ninguem) nunca e feita.
//   - Pula o que o contexto responde: tarde da noite (hora >= 23 ou < 5) nao
//     oferece "A noite toda"; perfil infantil nao pergunta a companhia; sem
//     nada em andamento nao pergunta "continuar ou comecar".
//   - Para cedo: com menos de 3 vivos (antes de qualquer pergunta ou depois
//     de qualquer resposta) encerra e entrega o que tem.
//   - "Tanto faz" nunca filtra: marca a pergunta como respondida e so.
//   - Sem beco: so oferece opcao que tem ao menos 1 vivo, entao responder
//     nunca zera os vivos. Zero so acontece com catalogo vazio ou com
//     pre-respostas do #54 que nao batem — e ai a saida e FESC_SAIDA_NADA
//     (a frase honesta "Nada exato…"), nunca uma tela sem saida.
//   - farolescolha_voltar desfaz a ultima resposta e devolve EXATAMENTE os
//     vivos, a pergunta, as opcoes e o "de" anteriores (os vivos sao refeitos
//     das respostas; o motor e deterministico). Sem resposta para desfazer
//     devolve 0: quem chama sai (Voltar na 1a pergunta).
//   - Deterministico: mesma entrada = mesma sequencia, bit a bit.
#ifndef NV_FAROLESCOLHA_H
#define NV_FAROLESCOLHA_H

#include <stdint.h>

#define FESC_CAND_MAX  400   // mesmo teto de candidatos do motor (farolrec 3.1)
#define FESC_PERG_MAX    3   // nunca a 4a pergunta
#define FESC_OPC_MAX     4   // ate 3 opcoes de filtro + "Tanto faz"
#define FESC_RES_MAX     3   // P4 compara ate 3
#define FESC_SEM_RESP  (-1)

typedef enum {
  FESC_TEMPO = 0, FESC_PESO, FESC_COMPANHIA, FESC_NOVIDADE, FESC_FORMATO,
  FESC_IDIOMA, FESC_EPOCA, FESC_HUMOR, FESC_CONTINUAR, FESC_N
} FarolEscPergunta;

// Opcoes de filtro por pergunta (o indice e o bit da mascara). "Tanto faz"
// e sempre farolescolha_n_opcoes_banco(p) - 1 e nao tem bit.
enum { FESC_T45 = 0, FESC_T100, FESC_T999 };               // tempo
enum { FESC_LEVE = 0, FESC_INTENSO };                       // peso
enum { FESC_SO = 0, FESC_DOIS, FESC_CRIANCA };              // companhia
enum { FESC_NOVO = 0, FESC_CONHECIDO };                     // novidade
enum { FESC_FILME = 0, FESC_SERIE };                        // formato
enum { FESC_DUBLADO = 0, FESC_LEGENDADO };                  // idioma
enum { FESC_RECENTE = 0, FESC_ANTIGA };                     // epoca
enum { FESC_RIR = 0, FESC_EMOCIONAR, FESC_TENSAO };         // humor
enum { FESC_CONT_SIM = 0, FESC_COMECAR };                   // continuar

// Um titulo candidato. `id` e opaco para o motor (o indice no catalogo, na
// UI); `pontos` ordena o resultado (maior primeiro; empate = menor posicao).
typedef struct {
  int32_t id;
  int32_t pontos;
  uint8_t m[FESC_N];        // mascara de opcoes atendidas por pergunta
  uint8_t livre[3];
} FarolEscCand;

// Fatos simples de um titulo para farolescolha_cand_de. 0 = sem dado.
typedef struct {
  int32_t     id;
  int32_t     pontos;
  int         minutos;      // duracao (filme) ou do episodio; 0 = sem dado
  int         ano;          // ano de lancamento; 0 = sem dado
  int         serie;        // 1 = serie, 0 = filme
  int         visto;        // 1 = ja visto
  int         andamento;    // 1 = comecou e nao terminou
  int         idiomas;      // bits 1<<FESC_DUBLADO | 1<<FESC_LEGENDADO; 0 = sem dado
  const char *genero;       // "Filme · Comédia · Drama" (NULL = sem dado)
} FarolEscFatos;

// O contexto que responde pergunta sozinho.
typedef struct {
  int hora;                 // 0..23 agora; fora da faixa = sem dado
  int infantil;             // 1 = perfil infantil: pula a companhia
  int emAndamento;          // 0 = nada em andamento: pula "continuar"
} FarolEscCtx;

#define FESC_SAIDA_PERGUNTA 0   // ainda perguntando
#define FESC_SAIDA_COMPARAR 1   // 2 ou 3 titulos -> P4 (farolcompara_abrir_titulos)
#define FESC_SAIDA_ACHADOS  2   // 1 titulo -> P7 (abrir/tocar)
#define FESC_SAIDA_NADA     3   // 0 titulos -> "Nada exato…" + Voltar

// Origem de cada resposta.
enum { FESC_ORIG_NADA = 0, FESC_ORIG_PRE, FESC_ORIG_PESSOA, FESC_ORIG_CONTEXTO };

typedef struct {
  int     saida;                    // FESC_SAIDA_*
  int     n;                        // titulos em id/pontos (0..3)
  int32_t id[FESC_RES_MAX];
  int32_t pontos[FESC_RES_MAX];
  int     tempoMin;                 // 45, 100, 999 ou 0 (sem resposta de tempo)
  int     vivos;                    // quantos sobraram antes do corte em 3
} FarolEscResultado;

// Estado do assistente (do chamador; ~1 KB). Nao mexer por fora.
typedef struct {
  const FarolEscCand *cand;
  int      nCand;
  FarolEscCtx ctx;
  int8_t   resp[FESC_N];            // opcao respondida ou FESC_SEM_RESP
  uint8_t  orig[FESC_N];            // FESC_ORIG_*
  int8_t   pilha[FESC_PERG_MAX];    // perguntas feitas a pessoa, em ordem
  int      nPilha;
  int      nPre;                    // pre-respostas aceitas (#54)
  uint8_t  vivo[FESC_CAND_MAX];
  int      nVivo;
  int      perg;                    // pergunta atual; -1 = terminou
  int      opc[FESC_OPC_MAX];       // opcoes oferecidas (indices do banco)
  int      nOpc;
  int      de;                      // total previsto agora (<= 3)
  int8_t   falta;                   // pergunta sugerida pelo #54 ou -1
} FarolEsc;

// ------------------------------------------------------------ banco
int         farolescolha_n_opcoes_banco(int perg);          // 0 fora da faixa
int         farolescolha_tanto(int perg);                   // indice de "Tanto faz"
const char *farolescolha_pergunta_id(int perg);             // "tempo"...; "" fora
const char *farolescolha_pergunta_rotulo(int perg);         // FT_ESC_*; "" fora
const char *farolescolha_opcao_id(int perg, int opcao);     // "t45"...; "" fora
const char *farolescolha_opcao_rotulo(int perg, int opcao); // FT_ESC_*; "" fora
int         farolescolha_pergunta_por_id(const char *id);   // -1 desconhecida
int         farolescolha_opcao_por_id(int perg, const char *id); // -1 desconhecida
// Minutos que a resposta de tempo da (45, 100, 999); 0 = tanto/sem dado.
int         farolescolha_tempo_min(int opcao);

// Mascara de um titulo a partir de fatos simples (tempo aninhado: um filme
// de 40 min cabe em "45", "filme" e "noite"; generos por radical PT/EN).
void farolescolha_cand_de(FarolEscCand *out, const FarolEscFatos *f);

// ------------------------------------------------------------ fluxo
// Comeca com os `n` candidatos (cortados em FESC_CAND_MAX; o vetor tem de
// viver enquanto o FarolEsc viver). Ja aplica o contexto e escolhe a 1a
// pergunta (ou ja termina: < 3 vivos, nada que divida).
void farolescolha_iniciar(FarolEsc *e, const FarolEscCand *cand, int n,
                          const FarolEscCtx *ctx);
// Gancho do #54: pre-responde `perg` com a opcao `opcaoId` vinda do LLM, so
// antes da 1a resposta da pessoa. "tanto" marca sem filtrar. Pre-resposta
// conta no teto de 3 (o motor pergunta so o que falta, ou encerra direto).
// 1 = aceita; 0 = id invalido, pergunta ja respondida ou a pessoa ja comecou.
int  farolescolha_pre_responder(FarolEsc *e, int perg, const char *opcaoId);
// Gancho do #54: prioriza a pergunta `perg` ("falta" do servidor) como proxima
// se o motor ainda perguntar, antes da 1a resposta da pessoa, e `perg` dividir
// os candidatos vivos (ganho > 0). 1 = aceita; 0 = ignorada.
int  farolescolha_priorizar(FarolEsc *e, int perg);
// Responde a pergunta atual com a opcao oferecida de indice `i` (0..nOpc-1).
// 1 = aceita; 0 = terminado ou `i` fora.
int  farolescolha_responder(FarolEsc *e, int i);
// Desfaz a ultima resposta da pessoa. 1 = desfez; 0 = nada a desfazer (sair).
int  farolescolha_voltar(FarolEsc *e);

// ------------------------------------------------------------ leitura
int  farolescolha_terminou(const FarolEsc *e);
int  farolescolha_pergunta(const FarolEsc *e);     // FESC_* ou -1
int  farolescolha_n_opcoes(const FarolEsc *e);     // 2..4 perguntando; 0 terminado
int  farolescolha_opcao(const FarolEsc *e, int i); // indice no banco; -1 fora
int  farolescolha_passo(const FarolEsc *e);        // 1..de perguntando; 0 terminado
int  farolescolha_de(const FarolEsc *e);           // <= 3
int  farolescolha_feitas(const FarolEsc *e);       // perguntas respondidas pela pessoa
int  farolescolha_vivos(const FarolEsc *e);
int  farolescolha_resposta(const FarolEsc *e, int perg);   // opcao ou FESC_SEM_RESP
int  farolescolha_origem(const FarolEsc *e, int perg);     // FESC_ORIG_*
// Quantos vivos atendem a opcao `opcao` de `perg` agora ("tanto" = todos).
int  farolescolha_conta(const FarolEsc *e, int perg, int opcao);
// O resultado: so com terminou() (perguntando devolve saida PERGUNTA, n 0).
void farolescolha_resultado(const FarolEsc *e, FarolEscResultado *r);

#endif
