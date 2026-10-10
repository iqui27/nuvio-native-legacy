// APOIAR O PROJETO: os enderecos de doacao e o QR de cada um.
//
// Pedido do dono (06/10): "colocar o qr code do patreon e do ko-fi no final
// [do what's new] e na settings tb". Aparecem em dois lugares, os dois
// discretos e opcionais: a ultima pagina do cartao de novidades da 2.0.1
// (novidades201.c) e Ajustes › Sobre e ajuda › Apoiar o projeto.
//
// UM LUGAR SO PARA OS ENDERECOS: trocar aqui troca nos dois. Endereco vazio
// = aquele QR some (e o outro fica sozinho, centrado).
#ifndef NV_APOIO_H
#define NV_APOIO_H
#include "gfx.h"
#include "text.h"

#define NV_URL_PATREON "https://www.patreon.com/cw/CraaazyDevs"
#define NV_URL_KOFI    "https://ko-fi.com/iqui27"
#define NV_URL_DISCORD "https://discord.gg/9NWr6SHyzJ"

// Discord compartilha so o desenho; APOIO_N/apoio_qual continuam so doacoes.
enum { APOIO_PATREON = 0, APOIO_KOFI = 1, APOIO_N, APOIO_DISCORD = APOIO_N };

// Quantos enderecos estao preenchidos, e o i-esimo deles (APOIO_*); -1 fora.
int apoio_n(void);
int apoio_qual(int i);
const char *apoio_nome(int qual);       // "Patreon", "Ko-fi"
const char *apoio_url(int qual);
// O endereco sem "https://" e sem "www.", para ler embaixo do codigo.
const char *apoio_url_curta(int qual);

// O QR num cartao claro de lado `lado` (px), cantos arredondados, o simbolo
// em 93% dele (NEAREST, centrado). Ko-fi: o QR oficial do dono
// (marcas/kofi-qr.png, aponta para ko-fi.com/K0S82835VO), com o gerado de
// NV_URL_KOFI enquanto ele nao carrega. Devolve 0 se o endereco esta vazio
// ou nao cabe no gerador.
int apoio_qr(int qual, float x, float y, float lado, float a);

// A pasta da arte do pacote (app_iniciar): o selo oficial do Ko-fi mora em
// marcas/kofi-badge.png. Sem chamar, vale "deploy/app/art" (Mac e testes).
void apoio_dir(const char *dirArte);
// O ROTULO embaixo do QR, de altura `h`: o selo oficial "Support me on Ko-fi"
// (imagem) e, para o Patreon, uma placa no mesmo formato desenhada aqui (o
// repositorio nao tem o logo oficial do Patreon, entao vai so o nome). Devolve
// a largura usada; `centro` = 1 centra em x, senao x e a borda esquerda.
float apoio_rotulo(int qual, float x, float y, float h, int centro, float a);

// Libera as texturas (ao trocar de contexto GL; o app nao precisa chamar).
void apoio_soltar(void);
#endif
