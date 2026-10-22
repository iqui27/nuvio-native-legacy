#include "fontevolta.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Uma entrada, sob trava: guardar/pegar sao do fio principal, mas a sonda
// escreve o resultado de outro fio.
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int tem;
static char eAlvo[64], eConta[96];
static int ePerfil;
static Stream eFonte;
static Uint32 eObtido;   // SDL_GetTicks de quando o link saiu do addon

void fontevolta_guardar(const char *alvo, const char *conta, int perfil,
                        const Stream *s, Uint32 idadeLink, Uint32 agora) {
  if (!alvo || !*alvo || !conta || !*conta || !s || !s->url[0]) return;
  pthread_mutex_lock(&trava);
  tem = 1;
  snprintf(eAlvo, sizeof eAlvo, "%s", alvo);
  snprintf(eConta, sizeof eConta, "%s", conta);
  ePerfil = perfil;
  eFonte = *s;
  eObtido = agora - idadeLink;
  pthread_mutex_unlock(&trava);
  printf("[voltafonte] guardada para %s (link com %u s)\n", alvo, (unsigned)(idadeLink / 1000u));
  fflush(stdout);
}

int fontevolta_pegar(const char *alvo, const char *conta, int perfil,
                     Uint32 agora, Stream *saida) {
  const char *por = NULL;
  Uint32 idade;
  pthread_mutex_lock(&trava);
  if (!tem || !alvo || !*alvo) { pthread_mutex_unlock(&trava); return 0; }
  idade = agora - eObtido;   // unsigned: relogio "para tras" vira idade enorme = vencida
  if (idade >= FONTEVOLTA_VALIDADE_MS) por = "vencida";
  else if (!conta || strcmp(conta, eConta) || perfil != ePerfil) por = "outra conta/perfil";
  if (por) {
    tem = 0;
    pthread_mutex_unlock(&trava);
    printf("[voltafonte] descartada: %s (%u s)\n", por, (unsigned)(idade / 1000u));
    fflush(stdout);
    return 0;
  }
  // Outro titulo/episodio nao apaga: o Retomar do anterior ainda pode vir.
  if (strcmp(alvo, eAlvo)) { pthread_mutex_unlock(&trava); return 0; }
  if (saida) *saida = eFonte;
  pthread_mutex_unlock(&trava);
  printf("[voltafonte] usando a fonte guardada de %s (link com %u s)\n", alvo,
         (unsigned)(idade / 1000u));
  fflush(stdout);
  return 1;
}

int fontevolta_tem_url(const char *url) {
  int r;
  if (!url || !*url) return 0;
  pthread_mutex_lock(&trava);
  r = tem && !strcmp(url, eFonte.url);
  pthread_mutex_unlock(&trava);
  return r;
}

void fontevolta_esquecer(const char *porque) {
  int tinha;
  pthread_mutex_lock(&trava);
  tinha = tem;
  tem = 0;
  pthread_mutex_unlock(&trava);
  if (tinha) {
    printf("[voltafonte] esquecida: %s\n", porque ? porque : "?");
    fflush(stdout);
  }
}

// --- conferencia --------------------------------------------------------------

static int (*sonda)(const char *, const char *) = NULL;
static unsigned confGeracao;
static int confEstado;

void fontevolta_definir_sonda(int (*s)(const char *, const char *)) { sonda = s; }

typedef struct { unsigned geracao; char url[4096]; char cab[512]; } Pedido;

static void *conferir(void *u) {
  Pedido *p = u;
  int ok = (sonda ? sonda : stream_url_serve)(p->url, p->cab);
  pthread_mutex_lock(&trava);
  // Um pedido mais novo (ou o fim da tentativa) ja passou por cima deste.
  if (p->geracao == confGeracao) confEstado = ok ? FV_OK : FV_FALHOU;
  pthread_mutex_unlock(&trava);
  free(p);
  return NULL;
}

void fontevolta_conferir(const char *url, const char *cabecalhos) {
  pthread_t t;
  Pedido *p = calloc(1, sizeof *p);
  pthread_mutex_lock(&trava);
  confGeracao++;
  confEstado = FV_CONFERINDO;
  if (p) p->geracao = confGeracao;
  pthread_mutex_unlock(&trava);
  // Sem memoria ou sem fio: nao ha conferencia, e o player decide sozinho.
  if (!p) { pthread_mutex_lock(&trava); confEstado = FV_NADA; pthread_mutex_unlock(&trava); return; }
  snprintf(p->url, sizeof p->url, "%s", url ? url : "");
  snprintf(p->cab, sizeof p->cab, "%s", cabecalhos ? cabecalhos : "");
  if (pthread_create(&t, NULL, conferir, p) == 0) pthread_detach(t);
  else {
    free(p);
    pthread_mutex_lock(&trava); confEstado = FV_NADA; pthread_mutex_unlock(&trava);
  }
}

int fontevolta_conferencia(void) {
  int e;
  pthread_mutex_lock(&trava);
  e = confEstado;
  pthread_mutex_unlock(&trava);
  return e;
}

int fontevolta_decidir(const FontevoltaSinais *g, const char **motivo) {
  const char *m = NULL;
  if (motivo) *motivo = NULL;
  if (!g) return FV_ESPERAR;
  if (g->falhou) m = "erro do player";
  // O clipe de aviso do debrid (30 s, sem audio) TOCA: so a duracao denuncia.
  else if (g->pronto && g->duracao > 1.0 && g->duracao < 120.0) m = "clipe curto";
  else if (!g->pronto && g->conferencia == FV_FALHOU) m = "conferencia falhou";
  else if (g->carregando && !g->quadro && g->desdeMs > FONTEVOLTA_PRAZO_MS) m = "prazo";
  else if (g->pronto && !g->carregando && g->conferencia != FV_CONFERINDO) return FV_ABRIU;
  if (!m) return FV_ESPERAR;
  if (motivo) *motivo = m;
  return FV_RECUAR;
}

int fontevolta_abertura_vencida(const FontevoltaSinais *g, unsigned prazoMs) {
  if (!g || g->falhou || g->pronto || !g->carregando || g->quadro) return 0;
  return g->desdeMs > prazoMs;
}
