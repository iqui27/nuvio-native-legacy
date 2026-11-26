// CODIGO DE 6 DIGITOS DO LOGIN (2.1): as regras, sem rede e sem tela.
//
// O login da TV e por QR porque o codigo do servidor tem 32 digitos
// hexadecimais. O QR so serve a quem esta NA FRENTE da TV com um celular; quem
// configura a TV de um parente esta longe. A TV troca o endereco do QR por 6
// digitos (servidor/recomendacoes/src/parear.js), a pessoa dita os 6 digitos
// por telefone, e quem ajuda os digita numa pagina e cai na mesma autorizacao.
//
// Este arquivo decide o que e um codigo valido, como ele aparece, o que a TV
// aceita do servidor e quando vale tentar de novo. A rede mora em
// parearrede.c; a tela, em login.c. Teste: tests/parear.sh.
#ifndef NV_PAREAR_H
#define NV_PAREAR_H
#include <stddef.h>

#define PAREAR_DIGITOS 6

// 1 quando `s` tem exatamente 6 digitos ASCII.
int parear_codigo_valido(const char *s);
// "483921" -> "483 921" (dois grupos de tres: e como se dita um numero).
// Codigo invalido deixa dst vazio.
void parear_formatar(const char *codigo, char *dst, size_t tam);
// O endereco que a TV aceita mandar: http(s), sem espaco nem aspas, ate 400.
int parear_endereco_ok(const char *url);
// Le a resposta de POST /v1/parear. 1 = codigo de 6 digitos, pagina http(s) e
// validade > 0; qualquer outra coisa e 0 e nada e escrito.
int parear_ler_resposta(int status, const char *corpo, char *codigo, size_t nCodigo,
                        char *pagina, size_t nPagina, int *validade);
// "https://exemplo.com/c/" -> "exemplo.com/c": o que se le em voz alta.
void parear_pagina_curta(const char *pagina, char *dst, size_t tam);
// Depois de uma resposta sem codigo: segundos ate a proxima tentativa, ou -1
// para desistir ate a tela de login abrir de novo. Servidor que nao conhece a
// rota ou esta com o recurso desligado (4xx, 503) nao muda em um minuto;
// queda de rede e 5xx passageiro sim. No maximo 3 tentativas.
int parear_espera(int status, int tentativa);
// 1 quando falta pouco (menos de 30 s) para o codigo vencer: hora de pedir outro.
int parear_vencendo(long agora, long expira);

// O ESTADO DO CODIGO NA TV, sem rede nem trava (parearrede.c guarda um destes
// sob a trava; tests/parear.c o exercita direto).
//   - `geracao` muda a cada zerar/renovar: resposta de pedido antigo e jogada fora;
//   - `novo` = o proximo POST leva "novo":1 e o servidor troca o codigo do
//     MESMO endereco na hora (o antigo morre). Fica ligado ate um sucesso,
//     porque um novo pedido sem ele traria o codigo antigo de volta.
#define PAREAR_RENOVAR_INTERVALO 3   // segundos minimos entre dois "Novo codigo"
typedef struct {
  char codigo[16];
  char anterior[8];
  long expira, proxima, renovadoEm;   // segundos; proxima = -1: desistiu
  int tentativa, novo;
  unsigned geracao;
} ParearCtl;
// Esquece tudo (endereco mudou, tela reabriu).
void parear_ctl_zerar(ParearCtl *c);
// "Novo codigo": apaga o codigo da tela JA e marca o proximo pedido como novo.
// Vale mesmo depois de desistir. 0 = ignorado (antes do intervalo minimo).
int parear_ctl_renovar(ParearCtl *c, long agora);
int parear_ctl_falhou(const ParearCtl *c, int fioVivo);
// 1 = hora de soltar um pedido (o chamador ja sabe se ha fio vivo). Codigo
// vencendo some aqui. Ao devolver 1, conta a tentativa.
int parear_ctl_quer_pedir(ParearCtl *c, long agora, int fioVivo);
// Resposta do pedido feito na geracao `g`. ok = parear_ler_resposta deu 1.
void parear_ctl_resposta(ParearCtl *c, unsigned g, int ok, const char *codigo,
                         int validade, int status, long agora);

#endif
