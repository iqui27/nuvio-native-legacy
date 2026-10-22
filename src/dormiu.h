// PEGOU NO SONO — e "Voltar para onde voce dormiu" (plano social-pessoal 7.2).
//
// DETECCAO (dormiu_regra, pura): tocando SEM NENHUMA TECLA ha >= 40 min (mais o
// que a pessoa ensinou com "Nao dormi") e >= 35% da duracao, E um destes:
//   1. a sessao acabou sem despedida (TV desligou / app morreu tocando, ou o
//      player fechou sozinho sem tecla);
//   2. passou dos creditos e a proxima tecla veio >= 10 min depois;
//   3. ao retomar depois, a pessoa voltou >= 10 min.
// PONTO PROVAVEL = posicao na ULTIMA TECLA + 5 min (a tela diz "por volta de").
//
// O QUE ACONTECE: a vez vai ao Diario com DIARIO_F_DORMIU_SUS e dormiuSeg (e
// PARCIAL se nao chegou ao fim). Na proxima vez que o titulo aparece (pagina do
// titulo), a ilha pergunta "Voce pegou no sono em X? Voltar para ~1:12"
// [Voltar] [Nao dormi]. Voltar poe a retomada no ponto e confirma (DORMIU).
// Nao dormi apaga a suspeita e sobe o limiar em 10 min (ate +50).
// PRIVADO SEMPRE: nada disto sai para amigos.
#ifndef NV_DORMIU_H
#define NV_DORMIU_H

#define DORMIU_SEM_TECLA_S   (40 * 60)
#define DORMIU_FRACAO        0.35
#define DORMIU_TARDE_S       (10 * 60)
#define DORMIU_MARGEM_S      (5 * 60)
#define DORMIU_LIMIAR_MAX_MIN 50

typedef struct {
  double durSeg;           // duracao do video da ultima tecla
  double semTeclaSeg;      // segundos TOCANDO sem tecla (o vao)
  double posTeclaSeg;      // posicao na ultima tecla antes do vao
  int    fimSemDespedida;  // caso 1
  double credTardeSeg;     // caso 2: creditos -> proxima tecla (<0 = nao houve)
  double voltouSeg;        // caso 3: quanto voltou ao retomar (<0 = nao houve)
  int    limiarExtraMin;   // aprendido com "Nao dormi"
} DormiuSinal;

// 1 = suspeita de sono; `ponto` recebe o ponto provavel em segundos.
int  dormiu_regra(const DormiuSinal *s, int *ponto);
// "1:12" / "48 min" — o rotulo do ponto ("~1:12").
void dormiu_rotulo(int seg, char *dst, unsigned tam);

// --- ganchos ---------------------------------------------------------------
void dormiu_iniciar(void);   // depois de diario_iniciar: avalia a sessao que morreu tocando
// Por quadro do player, so com titulo de verdade (mesma condicao de atividade).
// `tipo` 'm' | 's'; `concluiu` = creditos/90%.
void dormiu_player_passo(const char *id, char tipo, int t, int e, double pos,
                         double dur, int tocando, int concluiu, float dt);
void dormiu_player_saiu(double pos, double dur, int concluiu);
// Qualquer tecla/clique da pessoa, em qualquer tela (app_evento).
void dormiu_tecla(void);
// "Nao dormi" ensinou: +10 min. Lido/gravado por perfil.
int  dormiu_limiar_extra(void);
void dormiu_aprender_nao_dormi(void);

// Suspeita aberta deste titulo (a mais recente). 1 se houver; `saida` recebe a
// entrada do Diario (com dormiuSeg, durSeg, T/E).
#include "diario.h"
int  dormiu_suspeita(const char *id, DiarioEntrada *saida);
// As duas respostas. `e` e a entrada devolvida por dormiu_suspeita.
void dormiu_confirmar(const DiarioEntrada *e);   // Voltar
void dormiu_negar(const DiarioEntrada *e);       // Nao dormi

// --- ilha (dormiuilha.c) -------------------------------------------------------
// Laco principal: pergunta quando a pagina do titulo esta assentada.
void dormiu_passo(void);
// A pergunta em si (dormiu_passo chama; a captura tambem).
void dormiu_perguntar(const char *titulo, const char *poster, const char *arte, const DiarioEntrada *e);
// Botao do modal "dormiu:<id>" (ilhasinais.c entrega). 1 Voltar, 2 Nao dormi.
void dormiu_acao(const char *chave, int botao);

// Para o teste: relogio de parede falso (0 = o de verdade).
extern long long dormiu_relogio_teste;
#endif
