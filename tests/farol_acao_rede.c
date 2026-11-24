// Acoes de rede, cache, buffer e HDR do Farol (issue #34). Liga o codigo REAL de
// src/farolacao_rede.c e de src/cachearte.c (limpeza de arquivos); o resto
// (rede, ajustes, player, ilha, anel) e simulado aqui para observar o que a
// acao pede. Uso: farol_acao_rede <pasta-NUVIO_DADOS-vazia>
#include "farolacao_rede.h"
#include "cachearte.h"
#include "farolregra.h"
#include "ilhaacao.h"
#include "rede.h"
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// --- simulados ----------------------------------------------------------------
// Anel do Farol: so a ordem das chamadas.
// farolacao_rede.c chama do fio de teste: o anel simulado precisa de trava para
// o ASan/TSan nao reclamar de corrida do proprio teste.
static pthread_mutex_t anelTrava = PTHREAD_MUTEX_INITIALIZER;
static struct { char tipo; int acao, ok; } anel[64];
static int nAnel;
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

// Rede.
static int zerarChamadas;
void rede_zerar_cache(void) { zerarChamadas++; }
int rede_pedir(const RedePedido *p, RedeResposta *r) { (void)p; (void)r; assert(!"teste real nao deve rodar"); return 0; }
void rede_resposta_limpar(RedeResposta *r) { (void)r; }

// Cache de fontes em memoria.
static int fontesLimpas;
void fontecache_vod_limpar(void) { fontesLimpas++; }

// Player.
static int tocando;
static float posicao;
int player_aberto(void) { return tocando; }
float player_posicao_seg(void) { return posicao; }

// Ajustes: o valor GRAVADO e os overrides em memoria, como em ajustes.c.
static int dvGravado = 1, bufferGravado = 2, escritas;
static int bufferOverride, hdrOverride;
void ajustes_farol_buffer_maior(int on) { bufferOverride = on ? 1 : 0; }
int  ajustes_farol_buffer_maior_ativo(void) { return bufferOverride; }
void ajustes_farol_hdr_off(int on) { hdrOverride = on ? 1 : 0; }
int  ajustes_farol_hdr_off_ativo(void) { return hdrOverride; }
int  ajustes_dolby_vision_gravado(void) { return dvGravado; }
void ajustes_dolby_vision_definir(int ligado) { if (dvGravado != (ligado ? 1 : 0)) escritas++; dvGravado = ligado ? 1 : 0; }

// Idioma e ilha: a frase da acao feita e a copia do contexto do Desfazer.
const char *i18n(const char *s) { return s ? s : ""; }
static IlhaAcao ilhaUlt;
static int ilhaFeitas;
static char ilhaFrase[160];
void ilhaacao_feita(const IlhaAcao *a) {
  ilhaUlt = *a;
  snprintf(ilhaFrase, sizeof ilhaFrase, "%s", a->frase ? a->frase : "");
  ilhaUlt.frase = ilhaFrase;
  ilhaFeitas++;
}
static void ilhaDesfazer(void) { assert(ilhaUlt.desfazer); ilhaUlt.desfazer(ilhaUlt.ctx); }

// --- ajudantes ----------------------------------------------------------------
static char raiz[512];
static void escrever(const char *rel, const char *conteudo) {
  char p[1024];
  FILE *f;
  snprintf(p, sizeof p, "%s/%s", raiz, rel);
  f = fopen(p, "wb");
  assert(f);
  fputs(conteudo, f);
  fclose(f);
}
static int existe(const char *rel) {
  char p[1024];
  struct stat st;
  snprintf(p, sizeof p, "%s/%s", raiz, rel);
  return lstat(p, &st) == 0;
}
static int conteudoIgual(const char *rel, const char *esperado) {
  char p[1024], buf[256] = {0};
  FILE *f;
  snprintf(p, sizeof p, "%s/%s", raiz, rel);
  f = fopen(p, "rb");
  if (!f) return 0;
  size_t n = fread(buf, 1, sizeof buf - 1, f);
  fclose(f);
  buf[n] = 0;
  return !strcmp(buf, esperado);
}
static void pasta(const char *rel) {
  char p[1024];
  snprintf(p, sizeof p, "%s/%s", raiz, rel);
  assert(mkdir(p, 0777) == 0);
}
static int acaoNoAnel(int i, char tipo, int acao, int ok) {
  return anel[i].tipo == tipo && anel[i].acao == acao && (tipo == 'A' || anel[i].ok == ok);
}

// --- (a) RECONECTAR_REDE ---------------------------------------------------------
static int testeChamadas, testeValor = 1, testeSegura;
static pthread_mutex_t testeTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t testeCond = PTHREAD_COND_INITIALIZER;
static int testeFn(void) {
  pthread_mutex_lock(&testeTrava);
  testeChamadas++;
  while (testeSegura) pthread_cond_wait(&testeCond, &testeTrava);
  pthread_mutex_unlock(&testeTrava);
  return testeValor;
}
static void soltarTeste(void) {
  pthread_mutex_lock(&testeTrava);
  testeSegura = 0;
  pthread_cond_broadcast(&testeCond);
  pthread_mutex_unlock(&testeTrava);
}
static void caso_reconectar(void) {
  farolacao_rede_teste_conectividade(testeFn);

  // Limpa o DNS/conexoes e refaz o teste curto; o resultado so vai ao anel
  // quando o teste termina.
  ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_RECONECTAR_REDE, 0) == 1);
  assert(farolacao_rede_aguardar(5000));
  assert(zerarChamadas == 1 && testeChamadas == 1);
  assert(nAnel == 2 && acaoNoAnel(0, 'A', FAROL_A_RECONECTAR_REDE, 0) &&
         acaoNoAnel(1, 'R', FAROL_A_RECONECTAR_REDE, 1));

  // A rede segue fora: o resultado e 0, mas o cache foi zerado do mesmo jeito.
  testeValor = 0; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_RECONECTAR_REDE, 0) == 1);
  assert(farolacao_rede_aguardar(5000));
  assert(zerarChamadas == 2 && testeChamadas == 2);
  assert(nAnel == 2 && acaoNoAnel(1, 'R', FAROL_A_RECONECTAR_REDE, 0));

  // Build sem URL de saude (teste -1): nao reprova.
  testeValor = -1; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_RECONECTAR_REDE, 0) == 1);
  assert(farolacao_rede_aguardar(5000));
  assert(nAnel == 2 && acaoNoAnel(1, 'R', FAROL_A_RECONECTAR_REDE, 1));

  // Segundo pedido enquanto o teste roda: nao zera outra vez nem empilha fios.
  testeValor = 1; testeSegura = 1; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_RECONECTAR_REDE, 0) == 1);
  assert(!farolacao_rede_aguardar(50));            // ainda testando
  assert(farolacao_rede_executar(FAROL_A_RECONECTAR_REDE, 0) == 0);
  assert(zerarChamadas == 4);                      // 3 anteriores + este fio, nao 5
  soltarTeste();
  assert(farolacao_rede_aguardar(5000));
  pthread_mutex_lock(&anelTrava);
  // A(1) A(2, recusada) R(2: 0) R(1: 1) — a ordem das duas pontas e do fio.
  assert(nAnel == 4);
  assert(acaoNoAnel(0, 'A', FAROL_A_RECONECTAR_REDE, 0));
  pthread_mutex_unlock(&anelTrava);
  farolacao_rede_teste_conectividade(NULL);
  printf("ok: RECONECTAR_REDE\n");
}

// --- (b) LIMPAR_CACHE ---------------------------------------------------------------
static void caso_limpar_cache(void) {
  // NUVIO_DADOS temporario com o que NUNCA pode sumir, e a pasta de cache.
  escrever("ajustes.txt", "qualidade=1\ndolbyVision=0\n");
  escrever("contas.txt", "trakt=conta-1\nrd=conta-2\n");
  escrever("sessao.txt", "token=SECRETO-DE-SESSAO\n");
  escrever("tokens.txt", "refresh=SECRETO-REFRESH\n");
  escrever("diario.txt", "tt0111161\t1700000000\n");
  escrever("perfis.txt", "1\tAna\n2\tBia\n");
  escrever("fontefalha.txt", "filtro=1\n");
  escrever("deadbeefdeadbeef.jpg", "imagem-na-raiz-nao-e-cache");    // fora da pasta de cache
  pasta("cache");
  escrever("cache/0123456789abcdef.jpg", "img1");
  escrever("cache/deadbeef01.png", "img2");
  escrever("cache/CAFEBABE99.webp", "img3");
  escrever("cache/aabbccdd.gif", "img4");
  escrever("cache/catalogo-0000000000000001-p1.bin", "catalogo");   // outro cache: nao e imagem
  escrever("cache/catalogo-rede.bin", "catalogo-rede");
  escrever("cache/0123456789abcdef.jpg.tmp", "gravando");           // gravacao em curso
  escrever("cache/LEIAME.txt", "texto");
  escrever("cache/fontefalha.txt", "copia-no-cache-intocada");
  pasta("cache/aaaaaaaa.jpg");                                       // pasta com nome de imagem
  escrever("cache/aaaaaaaa.jpg/dentro.png", "dentro");

  char dir[600];
  snprintf(dir, sizeof dir, "%s/cache", raiz);
  cachearte_nativo_configurar_diretorio(dir);

  ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_LIMPAR_CACHE, 0) == 1);
  assert(fontesLimpas == 1);
  assert(nAnel == 2 && acaoNoAnel(0, 'A', FAROL_A_LIMPAR_CACHE, 0) &&
         acaoNoAnel(1, 'R', FAROL_A_LIMPAR_CACHE, 1));

  // Foi embora so o cache descartavel...
  assert(!existe("cache/0123456789abcdef.jpg"));
  assert(!existe("cache/deadbeef01.png"));
  assert(!existe("cache/CAFEBABE99.webp"));
  assert(!existe("cache/aabbccdd.gif"));
  // ...e nada do que e da pessoa.
  assert(conteudoIgual("ajustes.txt", "qualidade=1\ndolbyVision=0\n"));
  assert(conteudoIgual("contas.txt", "trakt=conta-1\nrd=conta-2\n"));
  assert(conteudoIgual("sessao.txt", "token=SECRETO-DE-SESSAO\n"));
  assert(conteudoIgual("tokens.txt", "refresh=SECRETO-REFRESH\n"));
  assert(conteudoIgual("diario.txt", "tt0111161\t1700000000\n"));
  assert(conteudoIgual("perfis.txt", "1\tAna\n2\tBia\n"));
  assert(conteudoIgual("fontefalha.txt", "filtro=1\n"));
  assert(conteudoIgual("deadbeefdeadbeef.jpg", "imagem-na-raiz-nao-e-cache"));
  assert(conteudoIgual("cache/catalogo-0000000000000001-p1.bin", "catalogo"));
  assert(conteudoIgual("cache/catalogo-rede.bin", "catalogo-rede"));
  assert(conteudoIgual("cache/0123456789abcdef.jpg.tmp", "gravando"));
  assert(conteudoIgual("cache/LEIAME.txt", "texto"));
  assert(conteudoIgual("cache/fontefalha.txt", "copia-no-cache-intocada"));
  assert(conteudoIgual("cache/aaaaaaaa.jpg/dentro.png", "dentro"));

  // Repetir com a pasta ja limpa e inofensivo.
  assert(farolacao_rede_executar(FAROL_A_LIMPAR_CACHE, 0) == 1);
  assert(conteudoIgual("diario.txt", "tt0111161\t1700000000\n"));
  // Pasta de cache inexistente: nada a limpar, sem falha.
  cachearte_nativo_configurar_diretorio("/nao/existe/farol34");
  assert(farolacao_rede_executar(FAROL_A_LIMPAR_CACHE, 0) == 1);
  cachearte_nativo_configurar_diretorio(dir);
  printf("ok: LIMPAR_CACHE\n");
}

// --- (c) BUFFER_MAIOR ----------------------------------------------------------------
static void caso_buffer(void) {
  double pos = 0;
  tocando = 0;
  ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 0);   // sem reproducao
  assert(!bufferOverride && !farolacao_rede_reabertura_pedida(&pos));
  assert(nAnel == 2 && acaoNoAnel(1, 'R', FAROL_A_BUFFER_MAIOR, 0));

  tocando = 1; posicao = 1234.5f; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 1);
  assert(bufferOverride && farolacao_rede_buffer_maior_ativo());
  assert(bufferGravado == 2 && escritas == 0);                      // ajuste da pessoa intacto
  assert(farolacao_rede_reabertura_pedida(&pos) && pos > 1234.4 && pos < 1234.6);
  assert(!farolacao_rede_reabertura_pedida(&pos));                  // uma vez so
  assert(nAnel == 2 && acaoNoAnel(0, 'A', FAROL_A_BUFFER_MAIOR, 0) &&
         acaoNoAnel(1, 'R', FAROL_A_BUFFER_MAIOR, 1));

  // Ja esta maior: nao reabre de novo.
  ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 0);
  assert(!farolacao_rede_reabertura_pedida(&pos));

  // Fim da reproducao: o override some; o ajuste persistente continua o mesmo.
  farolacao_rede_encerrar();
  assert(!bufferOverride && !farolacao_rede_buffer_maior_ativo());
  assert(bufferGravado == 2 && escritas == 0);

  // Pedido nao consumido nao sobrevive a reproducao.
  assert(farolacao_rede_executar(FAROL_A_BUFFER_MAIOR, 0) == 1);
  farolacao_rede_encerrar();
  assert(!farolacao_rede_reabertura_pedida(&pos));
  printf("ok: BUFFER_MAIOR\n");
}

// --- (d) DESLIGAR_HDR ------------------------------------------------------------------
static void caso_hdr(void) {
  double pos = 0;
  tocando = 1; posicao = 60; dvGravado = 1; escritas = 0; ilhaFeitas = 0;

  // Padrao: so esta reproducao. Nada gravado, nada na ilha.
  ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_DESLIGAR_HDR, 0) == 1);
  assert(hdrOverride && farolacao_rede_hdr_desligado_ativo());
  assert(dvGravado == 1 && escritas == 0 && ilhaFeitas == 0);
  assert(farolacao_rede_reabertura_pedida(&pos) && pos > 59.9 && pos < 60.1);
  assert(nAnel == 2 && acaoNoAnel(0, 'A', FAROL_A_DESLIGAR_HDR, 0) &&
         acaoNoAnel(1, 'R', FAROL_A_DESLIGAR_HDR, 1));
  farolacao_rede_encerrar();
  assert(!hdrOverride && dvGravado == 1);
  assert(!farolacao_rede_desfazer_hdr());                            // nada a desfazer

  // Sem reproducao, o temporario nao tem o que desligar.
  tocando = 0; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_DESLIGAR_HDR, 0) == 0);
  assert(!hdrOverride && dvGravado == 1 && nAnel == 2 && acaoNoAnel(1, 'R', FAROL_A_DESLIGAR_HDR, 0));

  // "Sempre nesta TV": grava, avisa na ilha com Desfazer, e vale ja nesta reproducao.
  tocando = 1; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_DESLIGAR_HDR, 1) == 1);
  assert(dvGravado == 0 && escritas == 1 && hdrOverride);
  assert(ilhaFeitas == 1 && ilhaUlt.desfazer != NULL);
  assert(!strcmp(ilhaFrase, "Desligar o HDR sempre nesta TV"));
  assert(farolacao_rede_reabertura_pedida(&pos));
  // Desfazer devolve o valor ANTERIOR (ligado), e solta o override.
  ilhaDesfazer();
  assert(dvGravado == 1 && escritas == 2 && !hdrOverride);
  assert(!farolacao_rede_desfazer_hdr());                            // uma vez so
  assert(dvGravado == 1 && escritas == 2);
  farolacao_rede_encerrar();

  // Persistente com o DV ja desligado pela pessoa: nada a desfazer, e o
  // Desfazer nunca liga o que ela tinha desligado.
  dvGravado = 0; escritas = 0; ilhaFeitas = 0; tocando = 0; ANEL_ZERAR();
  assert(farolacao_rede_executar(FAROL_A_DESLIGAR_HDR, 1) == 1);     // sem player: so a preferencia
  assert(dvGravado == 0 && escritas == 0 && !hdrOverride);
  assert(ilhaFeitas == 1 && ilhaUlt.desfazer == NULL);
  assert(!farolacao_rede_reabertura_pedida(&pos));
  assert(!farolacao_rede_desfazer_hdr() && dvGravado == 0);
  printf("ok: DESLIGAR_HDR\n");
}

static void caso_geral(void) {
  ANEL_ZERAR();
  assert(!farolacao_rede_acao_valida(FAROL_A_NENHUMA));
  assert(!farolacao_rede_acao_valida(FAROL_A_TENTAR_DE_NOVO));
  assert(!farolacao_rede_acao_valida(FAROL_A_ALIVIAR_REABRIR));
  assert(farolacao_rede_acao_valida(FAROL_A_RECONECTAR_REDE) && farolacao_rede_acao_valida(FAROL_A_LIMPAR_CACHE) &&
         farolacao_rede_acao_valida(FAROL_A_BUFFER_MAIOR) && farolacao_rede_acao_valida(FAROL_A_DESLIGAR_HDR));
  assert(farolacao_rede_executar(FAROL_A_ISOLAR_ADDON, 0) == 0 && nAnel == 0);   // de outra unidade: nem registra
  assert(farolacao_rede_executar(-1, 0) == 0 && nAnel == 0);
  printf("ok: geral\n");
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "uso: %s <pasta-vazia>\n", argv[0]); return 2; }
  snprintf(raiz, sizeof raiz, "%s", argv[1]);
  setenv("NUVIO_DADOS", raiz, 1);
  caso_geral();
  caso_reconectar();
  caso_limpar_cache();
  caso_buffer();
  caso_hdr();
  printf("farol_acao_rede: tudo ok\n");
  return 0;
}
