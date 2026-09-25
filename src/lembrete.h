// LEMBRETES DE PROGRAMA DO GUIA — "avisar de um programa que ainda vai
// comecar" (pedido do dono, 25/09/2026).
//
// O QUE E: a lista de programas FUTUROS que a pessoa marcou no guia. Cada um e
// identificado pelo que o XMLTV da e que sobrevive a uma republicacao da grade:
// id do canal + inicio + titulo. O aviso na tela (guialembrete.c) le daqui.
//
// POR PERFIL, EM DISCO: guia-lembretes-p<N>.txt na pasta de dados, uma linha
// por lembrete. NAO e o lembretes-p<N>.txt da Agenda (estreias de serie, outro
// modulo, outro formato) — o nome proprio evita que um apague o outro.
//
// VALIDADE: o lembrete morre quando o programa acaba (lembrete_podar). Avisado
// fica marcado no arquivo, e o app reiniciado no meio do programa nao avisa de
// novo.
//
// GRADE QUE MUDA: o XMLTV e rebaixado a cada 12 h e um programa pode mudar de
// horario. lembrete_achar casa o mesmo canal e titulo a ate
// LEMBRETE_TOLERANCIA_S do inicio guardado, e lembrete_ajustar leva o
// lembrete para o horario novo — quem chama e o guia, que ve a grade.
//
// Esta parte e so a LISTA: sem desenho, sem SDL, testavel sozinha
// (tests/lembrete.c).
#ifndef NV_LEMBRETE_H
#define NV_LEMBRETE_H
#include <time.h>
#include <stddef.h>

#define LEMBRETE_MAX          64
#define LEMBRETE_TOLERANCIA_S (30 * 60)
// Avisa um minuto antes do inicio: o tempo de a pessoa chegar ao sofa.
#define LEMBRETE_ANTECEDE_S   60

typedef struct {
  char   canal[80];     // id do canal no guia (o mesmo de GCanal.id)
  char   nome[120];     // nome do canal, para o aviso
  char   titulo[120];   // titulo do programa
  char   base[600];     // addon de origem do canal ("" para portal)
  time_t ini, fim;
  int    avisado;
} Lembrete;

// Le a lista do perfil `perfil` (zera a anterior). Sem arquivo = lista vazia.
void lembrete_carregar(int perfil);
int  lembrete_perfil(void);       // o perfil carregado, -1 = nenhum
int  lembrete_n(void);
const Lembrete *lembrete_item(int i);
// Indice do lembrete deste programa, ou -1.
int  lembrete_achar(const char *canal, time_t ini, const char *titulo);
// Marca ou desmarca. Devolve 1 = marcado, 0 = desmarcado, -1 = lista cheia.
int  lembrete_alternar(const char *canal, const char *nome, const char *titulo,
                       const char *base, time_t ini, time_t fim);
// A grade mudou o horario do programa: leva o lembrete junto (e grava).
void lembrete_ajustar(int i, time_t ini, time_t fim);
// Tira os que ja acabaram. Devolve quantos saiam.
int  lembrete_podar(time_t agora);
// O primeiro que ja deve avisar (ini - LEMBRETE_ANTECEDE_S <= agora < fim) e
// ainda nao avisou, ou -1.
int  lembrete_vencido(time_t agora);
void lembrete_marcar_avisado(int i);
// Serializacao do arquivo (exposta para o teste).
size_t lembrete_texto(char *dst, size_t tam);
int  lembrete_de_texto(const char *t);
// Logout: apaga as listas de todos os perfis.
void lembrete_esquecer_todos(void);

#endif
