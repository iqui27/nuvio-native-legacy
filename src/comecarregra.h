// Regra do "Começar" (primeiro uso guiado): quando aparece e o que fica gravado.
//
// SEPARADA DA TELA de proposito: a decisao nao depende de SDL, de rede nem de
// desenho, entao o teste (tests/comecar.sh) linka so este arquivo.
//
// QUANDO APARECE (design/2.1/NOTAS.md, pergunta 2: "aparece so na primeira
// abertura"):
//   - "feito" gravado: nunca mais.
//   - conta que acabou de entrar nesta TV (login novo): os 4 passos.
//   - conta que ja usava o app (sessao restaurada, nada gravado): so o tour,
//     oferecido uma vez. Quem ja tem lista e addons nao precisa de perfil nem
//     de importacao.
//   - "andamento" gravado (fechou o app no meio): recomeca do passo 1. Nada do
//     que foi preenchido e perdido, o nome e a cor voltam preenchidos.
#ifndef NV_COMECARREGRA_H
#define NV_COMECARREGRA_H
#include <stddef.h>

enum { COMECAR_ESTADO_NENHUM = 0, COMECAR_ESTADO_ANDAMENTO, COMECAR_ESTADO_FEITO };
enum { COMECAR_NADA = 0, COMECAR_COMPLETO, COMECAR_SO_TOUR };

#define COMECAR_NOME_MAX 32
#define COMECAR_CORES 6

typedef struct {
  int  estado;                     // COMECAR_ESTADO_*
  char nome[COMECAR_NOME_MAX + 1]; // nome digitado no passo 1; "" = ainda nao
  int  cor;                        // 0..COMECAR_CORES-1
  int  infantil;                   // 0/1
} ComecarSalvo;

// Texto de comecar.txt -> struct. NULL ou lixo caem no padrao (nenhum, cor 0).
void comecarregra_ler(const char *txt, ComecarSalvo *s);
// struct -> texto de comecar.txt. Devolve o tamanho, ou 0 se nao coube.
int  comecarregra_serializar(const ComecarSalvo *s, char *dst, size_t tam);
// O que mostrar. `loginNovo` = 1 quando a conta entrou agora por login, 0 quando
// a sessao veio restaurada do disco.
int  comecarregra_decidir(const ComecarSalvo *s, int loginNovo);
// Tira o que quebraria a linha do arquivo (CR/LF) e corta em COMECAR_NOME_MAX
// sem partir um caractere UTF-8 ao meio. Devolve o tamanho.
int  comecarregra_nome_limpo(const char *in, char *out, size_t tam);

#endif
