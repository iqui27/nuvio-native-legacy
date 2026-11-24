// FAROL — ACOES DE REDE, CACHE, BUFFER E HDR (issue #34). Contrato em
// farolacao_rede.h.
#include "farolacao_rede.h"
#include "ajustes.h"
#include "cachearte.h"
#include "farol.h"
#include "farolregra.h"
#include "faroltexto.h"
#include "fontecache.h"
#include "idioma.h"
#include "ilha.h"
#include "ilhaacao.h"
#include "player.h"
#include "rede.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// Teste curto: a rota de saude do Worker do proprio app (a mesma da sonda do
// Farol). Sem URL nesta build nao ha o que testar: -1, e o teste nao reprova.
#ifdef NV_REC_URL
#define FAROL_REDE_SAUDE NV_REC_URL
#else
#define FAROL_REDE_SAUDE ""
#endif
#define TESTE_PRAZO_MS 2500u

// Estado folha: uma trava, nenhuma chamada externa com ela.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t fim = PTHREAD_COND_INITIALIZER;
static int testando;                 // fio do teste de conectividade vivo
static FarolRedeTeste testeFn;       // NULL = o real
static atomic_int reabrirPedido;
static double reabrirPos = -1;
// "Desligar o HDR sempre nesta TV": o que a acao MUDOU, para o Desfazer
// devolver exatamente o valor anterior (-1 = nada a desfazer).
static int hdrAnterior = -1;

int farolacao_rede_acao_valida(int acao) {
  return acao == FAROL_A_RECONECTAR_REDE || acao == FAROL_A_LIMPAR_CACHE ||
         acao == FAROL_A_BUFFER_MAIOR || acao == FAROL_A_DESLIGAR_HDR;
}

int farolacao_rede_buffer_maior_ativo(void) { return ajustes_farol_buffer_maior_ativo(); }
int farolacao_rede_hdr_desligado_ativo(void) { return ajustes_farol_hdr_off_ativo(); }

void farolacao_rede_teste_conectividade(FarolRedeTeste f) {
  pthread_mutex_lock(&trava);
  testeFn = f;
  pthread_mutex_unlock(&trava);
}

// --- reabertura na posicao atual ---------------------------------------------
static void pedirReabertura(void) {
  double pos = player_posicao_seg();
  pthread_mutex_lock(&trava);
  reabrirPos = pos >= 0 ? pos : -1;
  pthread_mutex_unlock(&trava);
  atomic_store(&reabrirPedido, 1);
}

int farolacao_rede_reabertura_pedida(double *posSeg) {
  if (!atomic_exchange(&reabrirPedido, 0)) return 0;
  pthread_mutex_lock(&trava);
  if (posSeg) *posSeg = reabrirPos;
  pthread_mutex_unlock(&trava);
  return 1;
}

void farolacao_rede_encerrar(void) {
  ajustes_farol_buffer_maior(0);
  ajustes_farol_hdr_off(0);
  atomic_store(&reabrirPedido, 0);
  pthread_mutex_lock(&trava);
  reabrirPos = -1;
  pthread_mutex_unlock(&trava);
}

// --- RECONECTAR_REDE ----------------------------------------------------------
static int parar(void *u) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  long long agora = (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000;
  return agora >= *(long long *)u;
}

static int testeReal(void) {
  char url[sizeof FAROL_REDE_SAUDE + sizeof "/v1/saude"];
  RedePedido req;
  RedeResposta res;
  struct timespec t;
  long long fimMs;
  if (!FAROL_REDE_SAUDE[0]) return -1;
  snprintf(url, sizeof url, "%s/v1/saude", FAROL_REDE_SAUDE);
  clock_gettime(CLOCK_MONOTONIC, &t);
  fimMs = (long long)t.tv_sec * 1000 + t.tv_nsec / 1000000 + TESTE_PRAZO_MS;
  memset(&req, 0, sizeof req);
  memset(&res, 0, sizeof res);
  req.url = url;
  req.prazo_ms = TESTE_PRAZO_MS;
  req.max_bytes = 512;
  req.max_cabecalhos = 8192;
  req.seguir = 1;
  req.sonda = 1;
  req.parar = parar;
  req.parar_usuario = &fimMs;
  rede_pedir(&req, &res);
  int chegou = res.status > 0;     // qualquer HTTP prova que a rede chegou
  rede_resposta_limpar(&res);
  return chegou;
}

static void *fioTeste(void *arg) {
  FarolRedeTeste fn;
  int r;
  (void)arg;
  pthread_mutex_lock(&trava);
  fn = testeFn;
  pthread_mutex_unlock(&trava);
  r = fn ? fn() : testeReal();
  // "Nao testavel" nao reprova: o cache foi zerado e nada indica rede fora.
  farol_registrar_resultado(FAROL_A_RECONECTAR_REDE, r != 0);
  pthread_mutex_lock(&trava);
  testando = 0;
  pthread_cond_broadcast(&fim);
  pthread_mutex_unlock(&trava);
  return NULL;
}

int farolacao_rede_aguardar(unsigned ms) {
  struct timespec lim;
  int ok;
  clock_gettime(CLOCK_REALTIME, &lim);
  lim.tv_sec += ms / 1000;
  lim.tv_nsec += (long)(ms % 1000) * 1000000L;
  if (lim.tv_nsec >= 1000000000L) { lim.tv_sec++; lim.tv_nsec -= 1000000000L; }
  pthread_mutex_lock(&trava);
  while (testando)
    if (pthread_cond_timedwait(&fim, &trava, &lim) != 0) break;
  ok = !testando;
  pthread_mutex_unlock(&trava);
  return ok;
}

static int reconectarRede(void) {
  pthread_t fio;
  pthread_attr_t attr;
  int erro;
  pthread_mutex_lock(&trava);
  if (testando) { pthread_mutex_unlock(&trava); return 0; }   // ja reconectando
  testando = 1;
  pthread_mutex_unlock(&trava);
  rede_zerar_cache();
  erro = pthread_attr_init(&attr);
  if (!erro) {
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    erro = pthread_create(&fio, &attr, fioTeste, NULL);
    pthread_attr_destroy(&attr);
  }
  if (erro) {
    pthread_mutex_lock(&trava);
    testando = 0;
    pthread_cond_broadcast(&fim);
    pthread_mutex_unlock(&trava);
    return 0;
  }
  return 1;
}

// --- LIMPAR_CACHE -------------------------------------------------------------
static int limparCache(void) {
  long bytes = 0;
#ifndef __EMSCRIPTEN__
  long n = cachearte_nativo_limpar(&bytes);
#else
  long n = 0;
#endif
  fontecache_vod_limpar();     // listas de fontes em memoria: descartaveis
  printf("[farol] cache limpo: %ld imagem(ns), %ld bytes\n", n, bytes);
  fflush(stdout);
  return 1;
}

// --- BUFFER_MAIOR -------------------------------------------------------------
static int bufferMaior(void) {
  if (!player_aberto()) return 0;                 // sem reproducao nao ha o que reabrir
  if (ajustes_farol_buffer_maior_ativo()) return 0;   // ja esta maior; reabrir de novo nao ajuda
  ajustes_farol_buffer_maior(1);
  pedirReabertura();
  return 1;
}

// --- DESLIGAR_HDR -------------------------------------------------------------
int farolacao_rede_desfazer_hdr(void) {
  int ant;
  pthread_mutex_lock(&trava);
  ant = hdrAnterior;
  hdrAnterior = -1;
  pthread_mutex_unlock(&trava);
  if (ant < 0) return 0;
  ajustes_dolby_vision_definir(ant);
  ajustes_farol_hdr_off(0);
  return 1;
}
static void desfazerHdrCb(const void *ctx) { (void)ctx; farolacao_rede_desfazer_hdr(); }

static void avisarHdrPersistido(int desfazivel) {
  IlhaAcao a;
  memset(&a, 0, sizeof a);
  a.icone = "check";
  a.frase = i18n(FT_ACAO_DESLIGAR_HDR_SEMPRE);
  a.onde = i18n("Ajustes");
  a.tipo = ILHA_INFO;
  a.desfazer = desfazivel ? desfazerHdrCb : NULL;
  ilhaacao_feita(&a);
}

static int desligarHdr(int persistir) {
  int aberto = player_aberto();
  if (!aberto && !persistir) return 0;
  if (persistir) {
    int antes = ajustes_dolby_vision_gravado();
    // Gravacao que falhou NAO vira "sempre nesta TV": sem isto o ilha-aviso
    // prometia um desligado permanente que o proximo arranque esqueceria.
    if (ajustes_dolby_vision_definir(0) < 0) {
      printf("[farol] acao=desligar_hdr: ajuste nao gravou; "
             "aplicando so a esta reproducao\n");
      fflush(stdout);
      persistir = 0;
    } else {
      pthread_mutex_lock(&trava);
      hdrAnterior = antes ? 1 : -1;   // so desfaz o que ESTA acao mudou
      pthread_mutex_unlock(&trava);
      avisarHdrPersistido(antes != 0);
    }
  }
  if (aberto) {
    ajustes_farol_hdr_off(1);
    pedirReabertura();
  }
  return 1;
}

// --- executor -----------------------------------------------------------------
int farolacao_rede_executar(int acao, int persistir) {
  int ok = 0;
  if (!farolacao_rede_acao_valida(acao)) return 0;
  farol_registrar_acao(acao);
  switch (acao) {
    case FAROL_A_RECONECTAR_REDE:
      // O resultado vai ao anel quando o teste termina (fioTeste).
      if (reconectarRede()) return 1;
      farol_registrar_resultado(acao, 0);
      return 0;
    case FAROL_A_LIMPAR_CACHE: ok = limparCache(); break;
    case FAROL_A_BUFFER_MAIOR: ok = bufferMaior(); break;
    case FAROL_A_DESLIGAR_HDR: ok = desligarHdr(persistir); break;
    default: break;
  }
  farol_registrar_resultado(acao, ok);
  return ok;
}
