// Casos da sonda de revisao do Farol 10 sobre a fixture real de
// tests/fontefalha_worker.c (inclui streams.c com as funcoes dubladas):
//   manual:  escolha manual nao grava FE_FONTE_ESCOLHIDA (so FE_FONTE_PROXIMA).
//   teto:    FE_FONTE_PROXIMA reporta o teto real da sessao (fontefalha_teto).
//   tamanho: tamanhoMB gigante nao faz conversao indefinida -> int32 (UBSan).
//   corrida: pontuacao e capturada sob verTrava; sem releitura da lista viva
//            enquanto stream_lista_acrescentar realoca (TSan).
#include <assert.h>
#include <stdlib.h>
static int manualTeste;
static void (*ganchoPontuacao)(void);
static void tituloAddon(const void *s, char *d, unsigned n) { (void)s; (void)d; (void)n; abort(); }
#define main teste_worker_original
#include "farol_sonda_fontes_inc.h"
#undef main
#include "farolanel.h"

int ajustes_audio_externo(void) { return 0; }
int ajustes_dts_desligado(void) { return 0; }
int ajustes_fonte_ocultar(int regra) { (void)regra; return 0; }
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) { (void)alvo; (void)id; abort(); }
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome; abort();
}
int addons_parcial_abas(AddonAba *out, int max) { (void)out; (void)max; return 0; }
const char *i18n(const char *texto) { return texto; }

// macOS nao tem pthread_barrier_t: encontro de 2 rearmado por mutex+cond
// (a largada de tests/sessao_corrida.c, mas reutilizavel — o caso corrida
// faz dois encontros).
#if defined(__APPLE__)
typedef struct {
  pthread_mutex_t m;
  pthread_cond_t c;
  int n;
  unsigned era;
} pthread_barrier_t;
static int pthread_barrier_init(pthread_barrier_t *b, const void *attr, unsigned n) {
  (void)attr;
  assert(n == 2);
  pthread_mutex_init(&b->m, NULL);
  pthread_cond_init(&b->c, NULL);
  b->n = 0;
  b->era = 0;
  return 0;
}
static int pthread_barrier_wait(pthread_barrier_t *b) {
  pthread_mutex_lock(&b->m);
  if (++b->n == 2) {
    b->n = 0;
    b->era++;
    pthread_cond_broadcast(&b->c);
  } else {
    unsigned e = b->era;
    while (e == b->era) pthread_cond_wait(&b->c, &b->m);
  }
  pthread_mutex_unlock(&b->m);
  return 0;
}
static int pthread_barrier_destroy(pthread_barrier_t *b) {
  pthread_mutex_destroy(&b->m);
  pthread_cond_destroy(&b->c);
  return 0;
}
#endif
static pthread_barrier_t borda;
static pthread_t publicador, publicador2;
static Stream tardia;
static int iniciou, iniciou2;

static void *publicar(void *ctx) {
  (void)ctx;
  pthread_barrier_wait(&borda);
  stream_lista_acrescentar(&tardia, 1, 1);
  return NULL;
}

// Duas publicacoes: a 1a dispara na primeira pontuacao (dentro de
// falhaCandidatas, sob verTrava) — a thread fica parada na trava e so solta
// quando ela e liberada, mutando a lista durante o resto do verificador.
// A 2a dispara se a pontuacao for pedida DEPOIS da resolucao HTTP (nHttp==1):
// a leitura sem trava do codigo rejeitado; com a pontuacao carregada na
// candidata esse caminho nao existe e o gatilho nunca e alcancado.
static void lancarPublicacao(void) {
  if (!iniciou) {
    iniciou = 1;
    assert(!pthread_create(&publicador, NULL, publicar, NULL));
    pthread_barrier_wait(&borda);
  } else if (nHttp == 1 && !iniciou2) {
    iniciou2 = 1;
    assert(!pthread_create(&publicador2, NULL, publicar, NULL));
    pthread_barrier_wait(&borda);
  }
}

int main(int argc, char **argv) {
  const char *privada = "https://host-privado.invalid/segredo.mp4?token=CANARIO_TOKEN";
  FonteFalhaConfig c = cfg(1, 1, 3);
  FonteFalhaSessao *s;
  Stream item = fonte(&c, 0, "", 0);
  int idx, serv, casoManual, casoTeto, corrida;
  int nEscolhas = 0, nProximas = 0, tetoCorreto = 1;
  unsigned ger, total;
  FarolEvento eventos[16];
  char url[4096];
  assert(argc == 2);
  casoManual = !strcmp(argv[1], "manual");
  casoTeto = !strcmp(argv[1], "teto");
  corrida = !strcmp(argv[1], "corrida");
  if (!strcmp(argv[1], "tamanho")) item.tamanhoMB = 220000000000L;
  assert(casoManual || casoTeto || corrida || !strcmp(argv[1], "tamanho"));
  resetar();
  manualTeste = casoManual;
  farolanel_zerar();
  strcpy(item.url, privada);
  strcpy(item.rotulo, "CANARIO_TITULO");
  strcpy(item.cabecalhos, "Authorization: Bearer CANARIO_CHAVE");
  stream_definir_lista(&item, 1);
  ger = stream_lista_geracao();
  s = fontefalha_sessao_nova(&c, 15, casoManual ? 0 : -1);
  assert(s);
  if (corrida) {
    tardia = fonte(&c, 1, "", 0);
    strcpy(tardia.url, "https://addon.invalid/tardia.mp4");
    assert(!pthread_barrier_init(&borda, NULL, 2));
    ganchoPontuacao = lancarPublicacao;
  }
  assert(stream_falha_verificar(s, ger, casoManual ? 0 : -1, NULL, NULL,
                                &idx, &serv, url, sizeof url) == FONTEFALHA_PROXIMA);
  if (iniciou) assert(!pthread_join(publicador, NULL));
  if (iniciou2) assert(!pthread_join(publicador2, NULL));
  if (corrida) pthread_barrier_destroy(&borda);
  assert(idx == 0 && serv == FONTEFALHA_DIRETA && !strcmp(url, privada));
  assert(!strcmp(ultimoCab, item.cabecalhos));
  total = farolanel_ler(eventos, 16, 0);
  for (unsigned i = 0; i < total; i++) {
    FarolEvento *ev = &eventos[i];
    assert(ev->mod == FM_FONTE && !strstr(ev->tok, "CANARIO") && !strstr(ev->tok, "host-privado"));
    if (ev->ev == FE_FONTE_ESCOLHIDA) {
      nEscolhas++;
      assert(!strcmp(ev->tok, "1080p_sdr_nodv_outro"));
    } else if (ev->ev == FE_FONTE_PROXIMA) {
      nProximas++;
      if (ev->b != fontefalha_teto(s)) tetoCorreto = 0;
      printf("tentativa=%d teto_farol=%d teto_sessao=%d token=%s\n", ev->a, ev->b, fontefalha_teto(s), ev->tok);
    }
  }
  printf("%s: HTTP=%d escolha=%d tentativa=%d eventos=%u publicacoes=%d\n",
         argv[1], nHttp, nEscolhas, nProximas, total, iniciou + iniciou2);
  fontefalha_sessao_liberar(s);
  stream_definir_lista(NULL, 0);
  return nEscolhas != (casoManual ? 0 : 1) || nProximas != 1 ||
         (casoTeto && !tetoCorreto);
}
