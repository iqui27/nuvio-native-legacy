// Regra do "Resolver um problema" (2.1): a ordem dos testes, a parada na
// primeira falha, o ultimo erro de reproducao e os registros enviados que a TV
// guarda por 30 dias (#436).
//
// SEPARADA DA TELA de proposito: nada aqui depende de SDL, de rede nem de
// desenho, entao o teste (tests/resolver.sh) linka so este arquivo. Quem junta
// os fatos do app (rede, add-ons, debrid, player) e resolver.c.
//
// OS TESTES SO LEEM. Nenhum toca midia nem faz pedido: usam o estado de agora
// (rede, add-ons, debrid) e o ULTIMO erro de reproducao (design/2.1/NOTAS.md,
// pergunta 7: a proposta era ler o ultimo erro, nao tocar 2 s da fonte).
#ifndef NV_RESOLVERREGRA_H
#define NV_RESOLVERREGRA_H
#include <stddef.h>

enum { RES_S_NAO_COMECA = 0, RES_S_TRAVA, RES_S_SOM, RES_S_LEGENDA, RES_S_CONTINUAR, RES_S_CONTAS, RES_S_N };
// Na ordem em que rodam.
enum { RES_T_INTERNET = 0, RES_T_ADDONS, RES_T_DEBRID, RES_T_FONTE, RES_T_TV, RES_T_AUDIO, RES_T_N };
enum { RES_NAO_TESTADO = 0, RES_OK, RES_FALHOU };
// O que o painel oferece para a primeira falha.
enum { RES_ACAO_NENHUMA = 0, RES_ACAO_REPETIR, RES_ACAO_FONTES, RES_ACAO_ENVIAR };

#define RES_FRASE_MAX 200
#define RES_REJEITA_S 21600L          // um erro com mais de 6 h nao conta
#define RES_GUARDA_S  2592000L        // 30 dias
#define RES_ENVIADOS_MAX 12

// Os fatos do app, ja lidos. Texto vazio = sem problema naquele teste.
typedef struct {
  int  offline;                       // a TV esta sem internet
  int  addonsQtd;                     // quantos add-ons ha instalados
  char addonsMotivo[RES_FRASE_MAX];   // por que a lista veio vazia / fora do ar
  char debridMotivo[RES_FRASE_MAX];   // conta de debrid sem plano ou recusada
  char tvMotivo[RES_FRASE_MAX];       // a TV nao deixou abrir o player
  char audioMotivo[RES_FRASE_MAX];    // ainda sem fonte de dado: sempre vazio
  int  haFalha;                       // ha um erro de reproducao recente
} ResolverFatos;

typedef struct {
  int  estado[RES_T_N];               // RES_*
  int  testado[RES_T_N];              // 1 = o teste entra na cadeia do sintoma
  int  primeira;                      // RES_T_* da primeira falha, -1 = nenhuma
  int  acao;                          // RES_ACAO_*
} ResolverResultado;

// Quais testes cada sintoma roda (mascara de bits 1<<RES_T_*). Legenda e
// Continuar errado nao tem cadeia: o painel mostra os passos manuais.
unsigned resolverregra_mascara(int sintoma);
// Roda a cadeia na ordem e PARA na primeira falha: o que vem depois fica
// RES_NAO_TESTADO.
void resolverregra_testar(int sintoma, const ResolverFatos *f, ResolverResultado *r);

// ---- O ultimo erro de reproducao (memoria; alimentado pelo player).
typedef struct {
  long agora;                         // quando (segundos, epoch)
  int  etapa;                         // RES_T_* sugerido por quem registrou
  char titulo[RES_FRASE_MAX];         // a frase mostrada ao usuario
  char dica[RES_FRASE_MAX];
  char midia[96];                     // o que tocava ("T2:E5"); "" = nao sei
} ResolverFalha;
void resolverregra_falha_registrar(int etapa, const char *titulo, const char *dica,
                                   const char *midia, long agora);
// 1 se ha erro e ele tem menos de RES_REJEITA_S segundos em `agora`.
int  resolverregra_falha_recente(ResolverFalha *out, long agora);
void resolverregra_falha_limpar(void);

// ---- Registros enviados (resolver-enviados.txt: "codigo epoch sintoma").
typedef struct { char codigo[8]; long quando; int sintoma; } ResolverEnviado;
// Le o texto, descarta o que passou de 30 dias e o lixo, ordena do mais novo.
int  resolverregra_enviados_ler(const char *txt, long agora, ResolverEnviado *v, int max);
int  resolverregra_enviados_serializar(const ResolverEnviado *v, int n, char *dst, size_t tam);
// Poe no topo (mesmo codigo substitui). Devolve o novo total.
int  resolverregra_enviados_add(ResolverEnviado *v, int n, int max, const char *codigo, long quando, int sintoma);
// Tira por codigo. Devolve o novo total.
int  resolverregra_enviados_remover(ResolverEnviado *v, int n, const char *codigo);
// "4F7K92" -> "NV-4F7K-92". `dst` >= 12 bytes; devolve 0 se o codigo nao tem 6.
int  resolverregra_codigo_exibir(const char *codigo, char *dst, size_t tam);

// ---- "agora", "ha 4 min", "ha 3 h", "ha 2 dias".
enum { RES_HA_AGORA = 0, RES_HA_MIN, RES_HA_H, RES_HA_DIAS };
int  resolverregra_ha(long segundos, int *n);

#endif
