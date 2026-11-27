// SESSAO: A SALA DECIDE PELO CELULAR — a regra, sem rede nem tela.
//
// POR QUE EXISTE (dono, pesquisa de mercado 4.6): quem esta no sofa entra por
// um QR, cada um diz sim/nao aos mesmos cartoes no proprio celular, e a TV
// abre o PRIMEIRO titulo em que todos disseram sim. Esta etapa roda na rede
// local (celular.h), sem servidor nosso; a regra aqui nao sabe disso.
//
// REGRA:
//   - SALA_ENTRANDO: as pessoas entram (ate SALA_GENTE_MAX). Ninguem vota.
//   - SALA_VOTANDO (sala_comecar, com pelo menos 1 pessoa): cada pessoa vota
//     uma vez em cada cartao; o primeiro voto vale, repetir nao muda nada.
//     Quem chega atrasado ainda entra e conta para a unanimidade.
//   - o PRIMEIRO cartao (no tempo) a ter sim de TODOS encerra: SALA_DECIDIDO,
//     unanime = 1.
//   - todos viram tudo sem unanimidade, ou a TV encerrou (sala_encerrar): vence
//     o cartao com mais sins (empate: o de menor indice), unanime = 0. Nenhum
//     sim em lugar nenhum: SALA_SEM_ACORDO.
//
// O QUE O CELULAR MANDA (um texto curto, vindo de um visitante: tudo e
// conferido aqui, nada e confiado):
//   "j:<chave>:<nome>"            entrar
//   "v:<chave>:<cartao>:<0|1>"    votar
// <chave> = 12 hex minusculos gerados no celular: os 4 primeiros sao o id
// PUBLICO (vai no estado que todos leem), os 8 seguintes o segredo de quem
// vota. O estado nunca leva o segredo.
#ifndef NV_SALA_H
#define NV_SALA_H
#include <stddef.h>

#define SALA_CARTAS_MAX 24
#define SALA_GENTE_MAX  8
#define SALA_NOME_MAX   24   // bytes, UTF-8 inteiro

enum { SALA_ENTRANDO = 0, SALA_VOTANDO, SALA_DECIDIDO, SALA_SEM_ACORDO };
enum {
  SALA_OK = 0,
  SALA_IGUAL,      // pedido valido que nao mudou nada (reentrada, voto repetido)
  SALA_INVALIDO,   // texto fora do formato
  SALA_CHEIA,
  SALA_ESTRANHO,   // chave que nao esta na sala
  SALA_FORA_DE_HORA
};

typedef struct {
  char pub[5], seg[9];
  char nome[SALA_NOME_MAX + 1];
  unsigned visto, sim;       // um bit por cartao
} SalaPessoa;

typedef struct {
  int nCartas, nGente, fase;
  int vence;                 // indice do cartao; -1 sem vencedor
  int unanime;
  SalaPessoa g[SALA_GENTE_MAX];
} Sala;

void sala_iniciar(Sala *s, int nCartas);
int  sala_comecar(Sala *s);                 // 1 = passou a votar
void sala_encerrar(Sala *s);                // a TV decide com os votos de agora
int  sala_mensagem(Sala *s, const char *msg);
int  sala_votos(const Sala *s, int pessoa); // quantos cartoes ja viu
int  sala_sins(const Sala *s, int cartao);
// O que todos os celulares leem: {"f":fase,"n":cartoes,"v":vence,"u":unanime,
// "g":[{"i":"<pub>","n":"<nome>","k":<vistos>}]}. Devolve o tamanho, 0 se nao coube.
size_t sala_estado_json(const Sala *s, char *dst, size_t n);
// Texto para dentro de uma string JSON embutida em <script>: escapa aspas,
// barra, controles e '<' '>' '&' (\u00XX). Devolve o tamanho, 0 se nao coube.
size_t sala_json_texto(char *dst, size_t n, const char *s);
#endif
