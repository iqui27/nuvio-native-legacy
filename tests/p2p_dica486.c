// #486: addon so de torrent (Torrentio sem debrid), P2P desligado. Os torrents
// eram descartados calados: a folha so falava do debrid e o cartao do player
// era o generico. Com P2P neste pacote, a lista tem de dizer quantos o P2P
// tocaria; ligado (ou com debrid), nada a sugerir.
//
// Revisao do #486: duas formas de a dica sair errada. O motor e perguntado
// UMA VEZ, no fio da consulta (cargaMotor conta as perguntas ao motor: no
// .tpk cada uma abrira a libnuvio_engine.so por dlopen) — a folha e o cartao
// chamam stream_torrents_para_p2p a cada quadro e nada podem perguntar ali.
// E no .wgt (compilado segunda vez com -D__EMSCRIPTEN__) o P2P fica oculto
// nos Ajustes: "ligue o P2P" seria uma dica impossivel de seguir.
#include <assert.h>
int fontefalha_addon_ao_vivo(const char *key) { (void)key; return 0; }
#include "../src/streams.c"
#include "stub_player_source_actions.h"

static int p2pLig, motor = 1, debrid, cargaMotor;
static pthread_t fioUi;
static const char *p2pUrl = "";
int debrid_resolver_cache(const char *h, int f, char *u, unsigned t, int *fora) {
  (void)h; (void)f; (void)u; (void)t; (void)fora; return 0;
}
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *d,
                        unsigned t, int *st, char *m, unsigned mt, long *b) {
  (void)u; (void)s; (void)c; (void)d; (void)t; (void)st; (void)m; (void)mt; (void)b; return 0;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; return NULL;
}
int ajustes_selos_coloridos(void) { return 0; }
void selospacote_colorido(int ligado) { (void)ligado; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
const char *i18n(const char *s) { return s; }
int vazao_url_aviso(const char *url) { (void)url; return 0; }
int ajustes_fonte_ordem_addon(void) { return 0; }
int ajustes_fonte_texto_addon(void) { return 0; }
int debrid_ativo(void) { return debrid; }
int p2p_ativo(void) { return p2pLig; }
// Conta cada pergunta: no .tpk isso e um dlopen perto do fio que pergunta.
int p2pmotor_disponivel(void) {
  assert(!pthread_equal(pthread_self(), fioUi));
  cargaMotor++; return motor;
}
const char *ajustes_p2p_url(void) { return p2pUrl; }
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int q, unsigned short *o, int m) {
  (void)p; (void)q; (void)o; (void)m; return 0;
}
int ondever_n(const char *id) { (void)id; return 0; }
Uint32 SDL_GetTicks(void) { return 1000; }
char *dados_ler(const char *nome) { (void)nome; return NULL; }
int dados_gravar(const char *nome, const char *s) { (void)nome; (void)s; return 1; }

static const char *TORRENTIO = "{\"streams\":["
  "{\"name\":\"Torrentio\\n2160p\",\"title\":\"Movie.2160p.WEB-DL\",\"infoHash\":\"0123456789abcdef0123456789abcdef01234567\",\"fileIdx\":0},"
  "{\"name\":\"Torrentio\\n1080p\",\"title\":\"Movie.1080p.WEB-DL\",\"infoHash\":\"1123456789abcdef0123456789abcdef01234567\",\"fileIdx\":1},"
  "{\"name\":\"Torrentio\\n720p\",\"title\":\"Movie.720p.WEB-DL\",\"infoHash\":\"2123456789abcdef0123456789abcdef01234567\"}]}";

void *p2p486_enfileirar(void *a);
void p2p486_publicar(void);
static void chegar(void) {
  Stream *a = NULL;
  pthread_t trabalhador;
  assert(stream_extrair(TORRENTIO, "Torrentio", &a) == 3);
  stream_definir_lista(NULL, 0);
  assert(pthread_create(&trabalhador, NULL, p2p486_enfileirar, a) == 0);
  assert(pthread_join(trabalhador, NULL) == 0);
  int cargas = cargaMotor;
  p2p486_publicar();
  assert(cargaMotor == cargas);
  stream_definir_lista_idade(a, 3, 100);
  assert(cargaMotor == cargas);
  free(a);
}

int main(void) {
  fioUi = pthread_self();
  // CONSULTA VAZIA: a folha e o cartao perguntam A CADA QUADRO; sem torrent
  // descartado, nenhuma pergunta ao motor pode sair daqui (no .tpk seria um
  // dlopen no fio da tela, com uma consulta vazia).
  stream_definir_lista(NULL, 0);
  assert(stream_torrents_para_p2p() == 0);
  assert(cargaMotor == 0);

  // TORRENT SEM DEBRID CHEGOU: o motor e perguntado UMA vez, no fio da
  // consulta, e a tela so le o que ficou guardado.
  chegar();
  assert(n == 0);                                   // descartados: nada toca
  assert(cargaMotor == 1);
#ifdef __EMSCRIPTEN__
  // .WGT COM CONFIGURACAO ANTIGA: URL P2P persistida (ou motor), torrents
  // descartados — mas o P2P fica oculto nos Ajustes e ajustes_p2p_ligado() e
  // sempre 0. A dica "ligue o P2P" seria impossivel de seguir: nada a sugerir.
  p2pUrl = "http://servidor:1"; chegar();
  assert(stream_torrents_para_p2p() == 0);
  p2pUrl = ""; chegar();
  assert(stream_torrents_para_p2p() == 0);
  puts("p2p_dica486: PASS (.wgt: P2P oculto, nenhuma dica)");
  return 0;
#else
  if (stream_torrents_para_p2p() != 3) {
    fprintf(stderr, "FAIL: P2P desligado, 3 torrents descartados, dica=%d\n", stream_torrents_para_p2p());
    return 1;
  }
  assert(cargaMotor == 1);                           // a tela nao pergunta nada
#endif

  // PACOTE SEM MOTOR: nada a sugerir (perguntado na chegada, nao na tela).
  motor = 0; cargaMotor = 0; chegar();
  assert(cargaMotor == 1 && stream_torrents_para_p2p() == 0);
  motor = 1;

  // SERVIDOR P2P PESSOAL: toca sem motor.
#ifndef __EMSCRIPTEN__
  p2pUrl = "http://servidor:1"; motor = 0; chegar();
  assert(stream_torrents_para_p2p() == 3);
  p2pUrl = ""; motor = 1;

  debrid = 1; chegar(); assert(stream_torrents_para_p2p() == 0); debrid = 0;
  p2pLig = 1; chegar();
  assert(n == 3 && stream_qtd_torrents() == 3 && stream_torrents_para_p2p() == 0);
#endif
#ifdef __EMSCRIPTEN__
  puts("p2p_dica486: PASS (.wgt: P2P oculto, nenhuma dica)");
  return 0;
#endif
  puts("p2p_dica486: PASS (P2P desligado aponta o ajuste; ligado, sem motor ou com debrid, nada)");
  return 0;
}
