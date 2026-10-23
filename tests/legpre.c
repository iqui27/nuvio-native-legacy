// Rede falsa + decisao real do harness de memoria/faixas.
#define LEG_PRE_REAL 1
#define main legmemoria_roteiro
#include "legmemoria.c"
#undef main
#include "rede.h"
#include <pthread.h>
#include <unistd.h>
#ifdef LEG_PRE_ANTES
static void legendaPreDownload(void) {}
void legenda_pre_reiniciar(void) {}
void legenda_pre_descartar(const char *m) { (void)m; }
#endif
// Executa a definicao de fonte real com os demais efeitos do player falsos.
static int aberto = 1, mini, comVideo, durFonteAvisada, pedProximaFonte;
static int durFonteEsperadaOk, duracaoReal, durCurtaAvisado;
static int durPassouAvisado, esperandoFonte, erroFonte;
static Uint32 toastAte, durFonteAcaoAte;
static double retomarPct;
static void dvTelaFonte(const char *url) { (void)url; }
static void tocarFonte(const char *url) { (void)url; comVideo = 1; }
static int prebuscaCabe(const char *url) { (void)url; return 0; }
static int escolherLegendaPrebusca(const char *const *idi, int n) { (void)idi; (void)n; return -1; }
int mkvass_prebuscar(const char *url, MkvassEscolher escolher, double frac) {
  (void)url; (void)escolher; (void)frac; return 0;
}
#include "fonte.h"
static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cv = PTHREAD_COND_INITIALIZER;
static int chamadas, portao, terminou, callbacks, vigente;
static const char srt[] = "1\n00:00:01,000 --> 00:00:03,000\npre legenda\n\n";
char *rede_baixar_bin(const char *url, int prazo, long *nb) {
  (void)url; (void)prazo;
  pthread_mutex_lock(&m);
  chamadas++; pthread_cond_broadcast(&cv);
  while (!portao) pthread_cond_wait(&cv, &m);
  terminou++; pthread_cond_broadcast(&cv);
  pthread_mutex_unlock(&m);
  *nb = sizeof srt - 1; return strdup(srt);
}
static void pronto(const char *corpo, unsigned g, int v, void *u) {
  (void)g; (void)u;
  assert(corpo && !strcmp(corpo, srt));
  pthread_mutex_lock(&m); callbacks++; vigente = v;
  pthread_cond_broadcast(&cv); pthread_mutex_unlock(&m);
}
void legsync_primaria_externa(const char *url, const char *idi, const char *prov) {
  (void)idi; (void)prov; legenda_carregar_com(url, pronto, NULL);
}
static void esperar(int *valor, int alvo) {
  struct timespec t; clock_gettime(CLOCK_REALTIME, &t); t.tv_sec += 3;
  pthread_mutex_lock(&m);
  while (*valor < alvo) assert(pthread_cond_timedwait(&cv, &m, &t) == 0);
  pthread_mutex_unlock(&m);
}
static void soltar(void) {
  pthread_mutex_lock(&m); portao = 1; pthread_cond_broadcast(&cv); pthread_mutex_unlock(&m);
}
static void abrirPre(void) {
  legenda_desligar(); legenda_pre_reiniciar();
  comVideo = 0; esperandoFonte = 1;
  videoPronto = 0; legAuto = 1; legAutoDesde = 0;
  legExterna = -1; ativa = -1; aberta = 0;
  pthread_mutex_lock(&m); portao = 0; pthread_mutex_unlock(&m);
  legendaPreDownload();
}
int main(void) {
  LegMem e = {0};
  titulo("tt0137523:1:1"); nLeg = 0; nAdds = 2;
  strcpy(adds[0].url, "fake-a"); strcpy(adds[1].url, "fake-b");
  strcpy(adds[1].idioma, "por");
  strcpy(e.titulo, "tt0137523"); strcpy(e.idioma, "por"); e.tipo = 'a';
  legendasui_id_addon(&adds[1], e.id); legmem_guardar(&e);
  // Lista chega: inicia antes do pronto, sem ativar o overlay.
  abrirPre(); esperar(&chamadas, 1);
  assert(!videoPronto && !legenda_ligada_em(legenda_geracao()));
  legendaPreDownload(); assert(chamadas == 1);
  // Corpo pronto durante preparo: publica e entrega ao callback uma unica vez.
  soltar(); esperar(&terminou, 1); usleep(20000);
  player_definir_fonte("fake-video");
  videoPronto = 1; legendaAutomatica(1000); esperar(&callbacks, 1);
  assert(chamadas == 1 && vigente && legExterna == 1);
  char texto[80]; assert(legenda_texto(2, 0, texto, sizeof texto) && !strcmp(texto, "pre legenda"));
  // Pronto enquanto baixa: adota o fio vivo, sem segunda requisicao.
  abrirPre(); esperar(&chamadas, 2);
  player_definir_fonte("fake-video");
  videoPronto = 1; legendaAutomatica(1000); soltar(); esperar(&callbacks, 2);
  assert(chamadas == 2 && vigente);
  // Novo titulo invalida o corpo antigo e nao publica nem chama pronto.
  abrirPre(); esperar(&chamadas, 3);
  titulo("tt0000001:2:3"); legenda_desligar(); legenda_pre_reiniciar();
  soltar(); esperar(&terminou, 3); usleep(20000);
  assert(callbacks == 2 && !legenda_ligada_em(legenda_geracao()));
  // Outra escolha ignora o candidato; descartar fonte nao rearma por play.
  titulo("tt0137523:1:1"); abrirPre(); esperar(&chamadas, 4);
  comVideo = 1; player_definir_fonte("outra-fonte"); legendaPreDownload();
  assert(chamadas == 4);
  soltar(); esperar(&terminou, 4);
  legenda_carregar_com("fake-a", pronto, NULL); esperar(&callbacks, 3);
  assert(chamadas == 5 && vigente);
  // Escolha final diferente: o carregador descarta sozinho pelo link.
  abrirPre(); esperar(&chamadas, 6);
  soltar(); esperar(&terminou, 6); usleep(20000);
  legenda_carregar_com("fake-a", pronto, NULL); esperar(&callbacks, 4);
  assert(chamadas == 7 && vigente);
  legenda_desligar();
  for (int i = 0; i < 4; i++) free(discoDado[i]);
  puts("legpre: antes do video, corpo/fio adotados, callback, titulo/fonte e escolha diferente ok");
  return 0;
}
