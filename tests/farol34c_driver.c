// FAROL34C P1-1: prova dirigida compilando o src/farolacao_rede.c REAL — a
// acao vinda dos Ajustes (player fechado) TEM de funcionar com sessao retida
// ou com a ultima fonte VOD. Em 43433cbcb3 executar() devolvia 0 porque so
// valia player_aberto(); na ponta nova devolve 1 e pede a reabertura na
// fonte certa. Compilar com -DFAROL34C_API para a API nova (2 parametros).
// Uso: farol34c_driver <pasta-NUVIO_DADOS-vazia>
#include "farolacao_rede.h"
#include "cachearte.h"
#include "farolregra.h"
#include "ilhaacao.h"
#include "rede.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// --- simulados ----------------------------------------------------------------
static pthread_mutex_t anelTrava = PTHREAD_MUTEX_INITIALIZER;
static struct { char tipo; int acao, ok; } anel[64];
static int nAnel;
#ifndef FAROL34C_API
#define farolacao_rede_reabrir_fim(reabriu) ((void)(reabriu))
#endif
void farol_registrar_acao(int acao) {
  pthread_mutex_lock(&anelTrava);
  if (nAnel < 64) { anel[nAnel].tipo = 'A'; anel[nAnel].acao = acao; anel[nAnel].ok = 0; nAnel++; }
  pthread_mutex_unlock(&anelTrava);
}
void farol_registrar_resultado(int acao, int ok) {
  pthread_mutex_lock(&anelTrava);
  if (nAnel < 64) { anel[nAnel].tipo = 'R'; anel[nAnel].acao = acao; anel[nAnel].ok = ok; nAnel++; }
  pthread_mutex_unlock(&anelTrava);
}
#define ANEL_ZERAR() do { pthread_mutex_lock(&anelTrava); nAnel = 0; pthread_mutex_unlock(&anelTrava); } while (0)

void rede_zerar_cache(void) {}
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; (void)r; assert(!"teste real nao deve rodar"); return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }
void fontecache_vod_limpar(void) {}

// Player: fechado, com retido e ultima fonte VOD sob controle do caso.
static int tocando, retido;
static float posicao;
int player_aberto(void) { return tocando; }
int player_retido(void) { return retido; }
float player_posicao_seg(void) { return posicao; }
static int fonteAtual = -1;
int stream_atual(void) { return fonteAtual; }

// Ajustes: valor gravado + overrides em memoria, como ajustes.c.
static int dvGravado = 1;
static int bufferOverride, hdrOverride;
void ajustes_farol_buffer_maior(int on) { bufferOverride = on ? 1 : 0; }
int  ajustes_farol_buffer_maior_ativo(void) { return bufferOverride; }
void ajustes_farol_hdr_off(int on) { hdrOverride = on ? 1 : 0; }
int  ajustes_farol_hdr_off_ativo(void) { return hdrOverride; }
int  ajustes_dolby_vision_gravado(void) { return dvGravado; }
int  ajustes_dolby_vision_definir(int ligado) { dvGravado = ligado ? 1 : 0; return 0; }

const char *i18n(const char *s) { return s ? s : ""; }
static IlhaAcao ilhaUlt;
void ilhaacao_feita(const IlhaAcao *a) { ilhaUlt = *a; (void)ilhaUlt; }

static int pedida(double *pos, int *fonte) {
#ifdef FAROL34C_API
  return farolacao_rede_reabertura_pedida(pos, fonte);
#else
  (void)fonte;
  return farolacao_rede_reabertura_pedida(pos);
#endif
}

int main(int argc, char **argv) {
  (void)argc; (void)argv;
  double pos = -1;
  int fonte = -1;

  // Sem reproducao, sem retido e sem fonte VOD: nao armada (igual antes).
  tocando = 0; retido = 0; fonteAtual = -1;
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 0);
  assert(!pedida(&pos, &fonte));

  // Sessao retida (saiu para os Ajustes): executa e pede reabertura na fonte 3.
  retido = 1; fonteAtual = 3; posicao = 42.0f;
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 1);
  assert(pedida(&pos, &fonte));
  assert(fonte == 3 && pos > 41.9 && pos < 42.1);

  // O descarte do retido dentro do player_abrir preserva o override da acao;
  // a reabertura concluida (reabrir_fim) deixa o encerrar limpar de novo.
#ifdef FAROL34C_API
  farolacao_rede_encerrar();
  assert(ajustes_farol_buffer_maior_ativo());
  farolacao_rede_reabrir_fim(1);
  farolacao_rede_encerrar();
  assert(!ajustes_farol_buffer_maior_ativo());
#else
  farolacao_rede_encerrar();
#endif

  // Sem retido, com a ultima fonte VOD: tambem executa na fonte 5.
  retido = 0; fonteAtual = 5;
  assert(farolacao_rede_executar(FAROL_A_DESLIGAR_HDR, 0) == 1);
  assert(pedida(&pos, &fonte));
  assert(fonte == 5);

#ifdef FAROL34C_API
  // Reabertura impossivel solta os overrides em vez de deixa-los armados.
  farolacao_rede_reabrir_fim(0);
  assert(!ajustes_farol_hdr_off_ativo());
#endif
  farolacao_rede_encerrar();

  puts("ok: reabrir com player fechado");
  return 0;
}
