#include "fontevolta.h"
#include "mediaurl.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// AS ENTRADAS, sob trava: guardar/pegar sao do fio principal, mas a sonda
// escreve o resultado de outro fio.
//
// DOIS MODOS (fontevolta_configurar, o ajuste "Reusar o ultimo link"):
//   desligado (padrao)  o de sempre: UMA entrada, so em memoria, validade
//                       FONTEVOLTA_VALIDADE_MS contada em SDL_GetTicks;
//   ligado              uma entrada POR titulo/episodio (FONTEVOLTA_MAX), no
//                       disco (fontevolta.txt, pelas funcoes de
//                       fontevolta_definir_arquivo), validade do ajuste contada
//                       no relogio de parede — os ticks recomecam a cada
//                       arranque e nao servem entre sessoes.
// A URL continua fora do log nos dois modos.
typedef struct {
  char alvo[64], conta[96];
  int perfil;
  Uint32 obtido;       // SDL_GetTicks de quando o link saiu do addon (modo desligado)
  long long obtidoS;   // time(NULL) do mesmo instante (modo ligado)
  Stream s;
} Entrada;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static Entrada umaSo;  // desligado: a unica entrada, sem alocar a tabela
static Entrada *ent = &umaSo;   // ligado: FONTEVOLTA_MAX, alocado na primeira vez
static int nEnt;
static int persistir;
static unsigned validadeS = 24u * 3600u;
static int carregado;
static char *(*arqLer)(const char *);
static int (*arqGravar)(const char *, const char *);
static int (*arqApagar)(const char *);
#define ARQUIVO "fontevolta.txt"

static int garantir(void) {
  if (!persistir) { ent = &umaSo; return 1; }
  if (ent == &umaSo) {
    Entrada *t = calloc(FONTEVOLTA_MAX, sizeof *t);
    if (!t) return 0;
    ent = t;
  }
  return 1;
}

// --- disco (so no modo ligado) ----------------------------------------------
// Uma linha por entrada, campos separados por TAB. Texto com TAB, quebra de
// linha ou barra invertida e escapado (\t \n \\): os cabecalhos do addon sao
// "Nome: valor" UMA linha cada.
static size_t esc(char *d, size_t cap, const char *src) {
  size_t k = 0;
  for (; src && *src && k + 2 < cap; src++) {
    char c = *src;
    if (c == '\t' || c == '\n' || c == '\\' || c == '\r') {
      d[k++] = '\\'; d[k++] = c == '\t' ? 't' : c == '\n' ? 'n' : c == '\r' ? 'r' : '\\';
    } else d[k++] = c;
  }
  d[k] = 0;
  return k;
}
static void desesc(char *d, size_t cap, const char *src) {
  size_t k = 0;
  for (; *src && k + 1 < cap; src++) {
    if (*src == '\\' && src[1]) {
      src++;
      d[k++] = *src == 't' ? '\t' : *src == 'n' ? '\n' : *src == 'r' ? '\r' : *src;
    } else d[k++] = *src;
  }
  d[k] = 0;
}
static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else *p = NULL;
  return ini;
}

static void gravarSemTrava(void) {
  size_t cap, k = 0;
  char *buf, *tmp;
  size_t tmpTam = 2 * 4096 + 8;
  int i;
  if (!persistir || !arqGravar) return;
  if (!nEnt) { if (arqApagar) arqApagar(ARQUIVO); return; }
  cap = (size_t)nEnt * (2u * sizeof(Stream) + 2048u) + 64u;
  for (i = 0; i < nEnt; i++) {
    size_t len = strlen(nv_media_url(ent[i].s.url));
    if (len > 4096) cap += 2 * (len - 4096);
    if (2 * len + 8 > tmpTam) tmpTam = 2 * len + 8;
  }
  buf = malloc(cap); tmp = malloc(tmpTam);
  if (!buf || !tmp) { free(buf); free(tmp); return; }
  k += (size_t)snprintf(buf, cap, "# nuvio fonte guardada v2\n");
  for (i = 0; i < nEnt && k + 1 < cap; i++) {
    const Entrada *e = &ent[i];
    const Stream *s = &e->s;
#define CAMPO(txt) do { esc(tmp, tmpTam, (txt)); k += (size_t)snprintf(buf + k, cap - k, "%s\t", tmp); } while (0)
    k += (size_t)snprintf(buf + k, cap - k, "%lld\t%d\t", e->obtidoS, e->perfil);
    CAMPO(e->alvo); CAMPO(e->conta); CAMPO(nv_media_url(s->url)); CAMPO(s->cabecalhos);
    CAMPO(s->rotulo); CAMPO(s->provedor); CAMPO(s->addonChave); CAMPO(s->arquivo);
    CAMPO(s->bingeGroup); CAMPO(s->infoHash);
    CAMPO(s->audioIdiomas); CAMPO(s->legendaIdiomas); CAMPO(s->descricao);
    CAMPO(s->filtro.grupo); CAMPO(s->hashTorrent); CAMPO(s->videoHash);
#undef CAMPO
    k += (size_t)snprintf(buf + k, cap - k, "%d\t%llu\t%llu\t", s->filtroLido,
                          (unsigned long long)s->tamanhoBytes, (unsigned long long)s->filtro.tamanhoBytes);
    for (int j = 0; j < FONTEFILTRO_N; j++)
      k += (size_t)snprintf(buf + k, cap - k, "%llu\t", (unsigned long long)s->filtro.valores[j]);
    if (k + 1 < cap)
      k += (size_t)snprintf(buf + k, cap - k, "%d\t%d\t%d\t%d\t%d\t%ld\t%d\n", s->altura, s->dolbyVision,
                            s->dolbyAtmos, s->mp4, s->fileIdx, s->tamanhoMB, s->debridServico);
  }
  arqGravar(ARQUIVO, buf);
  free(buf); free(tmp);
}

static void carregarSemTrava(void) {
  char *b, *linha, *prox;
  if (carregado || !persistir || !arqLer || !garantir()) return;
  carregado = 1;
  nEnt = 0;
  b = arqLer(ARQUIVO);
  if (!b) return;
  // Older entries lack structured classification: fetch fresh sources instead.
  if (strncmp(b, "# nuvio fonte guardada v2\n", 26)) { free(b); return; }
  for (linha = b; linha && *linha && nEnt < FONTEVOLTA_MAX; linha = prox) {
    char *fim = strchr(linha, '\n'), *p;
    Entrada *e = &ent[nEnt];
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    memset(e, 0, sizeof *e);
    p = linha;
    e->obtidoS = atoll(campo(&p));
    e->perfil = atoi(campo(&p));
    desesc(e->alvo, sizeof e->alvo, campo(&p));
    desesc(e->conta, sizeof e->conta, campo(&p));
    { const char *salva = campo(&p);
      char *url = malloc(strlen(salva) + 1);
      if (!url) continue;
      desesc(url, strlen(salva) + 1, salva);
      int ok = nv_media_url_guardar(url, e->s.url, sizeof e->s.url);
      free(url);
      if (!ok) continue;
    }
    desesc(e->s.cabecalhos, sizeof e->s.cabecalhos, campo(&p));
    desesc(e->s.rotulo, sizeof e->s.rotulo, campo(&p));
    desesc(e->s.provedor, sizeof e->s.provedor, campo(&p));
    desesc(e->s.addonChave, sizeof e->s.addonChave, campo(&p));
    desesc(e->s.arquivo, sizeof e->s.arquivo, campo(&p));
    desesc(e->s.bingeGroup, sizeof e->s.bingeGroup, campo(&p));
    desesc(e->s.infoHash, sizeof e->s.infoHash, campo(&p));
    desesc(e->s.audioIdiomas, sizeof e->s.audioIdiomas, campo(&p));
    desesc(e->s.legendaIdiomas, sizeof e->s.legendaIdiomas, campo(&p));
    desesc(e->s.descricao, sizeof e->s.descricao, campo(&p));
    desesc(e->s.filtro.grupo, sizeof e->s.filtro.grupo, campo(&p));
    desesc(e->s.hashTorrent, sizeof e->s.hashTorrent, campo(&p));
    desesc(e->s.videoHash, sizeof e->s.videoHash, campo(&p));
    e->s.filtroLido = atoi(campo(&p));
    e->s.tamanhoBytes = strtoull(campo(&p), NULL, 10);
    e->s.filtro.tamanhoBytes = strtoull(campo(&p), NULL, 10);
    for (int j = 0; j < FONTEFILTRO_N; j++)
      e->s.filtro.valores[j] = strtoull(campo(&p), NULL, 10);
    if (!p) continue;
    e->s.altura = atoi(campo(&p));
    e->s.dolbyVision = atoi(campo(&p));
    e->s.dolbyAtmos = atoi(campo(&p));
    e->s.mp4 = atoi(campo(&p));
    e->s.fileIdx = atoi(campo(&p));
    e->s.tamanhoMB = atol(campo(&p));
    e->s.debridServico = atoi(campo(&p));
    if (!e->alvo[0] || !e->conta[0] || !e->s.url[0] || e->obtidoS <= 0) continue;
    nEnt++;
  }
  free(b);
}

void fontevolta_definir_arquivo(char *(*ler)(const char *), int (*gravar)(const char *, const char *),
                                int (*apagar)(const char *)) {
  pthread_mutex_lock(&trava);
  arqLer = ler; arqGravar = gravar; arqApagar = apagar;
  pthread_mutex_unlock(&trava);
}

void fontevolta_configurar(int ligar, unsigned validade) {
  int mudou;
  pthread_mutex_lock(&trava);
  if (validade) validadeS = validade;
  mudou = (ligar ? 1 : 0) != persistir;
  if (!mudou) { pthread_mutex_unlock(&trava); return; }
  persistir = ligar ? 1 : 0;
  // Trocar de modo comeca do zero: ligado le o arquivo; desligado apaga o
  // arquivo (quem desliga nao quer links guardados na TV) e volta a uma entrada.
  nEnt = 0;
  carregado = 0;
  if (!persistir) {
    if (ent != &umaSo) free(ent);
    ent = &umaSo;
    if (arqApagar) arqApagar(ARQUIVO);
  }
  pthread_mutex_unlock(&trava);
  printf("[voltafonte] reusar o ultimo link: %s\n", ligar ? "ligado" : "desligado");
  fflush(stdout);
}

static int acharSemTrava(const char *alvo, const char *conta, int perfil) {
  int i;
  for (i = 0; i < nEnt; i++)
    if (!strcmp(ent[i].alvo, alvo) && !strcmp(ent[i].conta, conta) && ent[i].perfil == perfil) return i;
  return -1;
}
static void removerSemTrava(int i) {
  if (i < 0 || i >= nEnt) return;
  if (i != nEnt - 1) ent[i] = ent[nEnt - 1];
  nEnt--;
}

void fontevolta_guardar(const char *alvo, const char *conta, int perfil,
                        const Stream *s, Uint32 idadeLink, Uint32 agora) {
  int i;
  if (!alvo || !*alvo || !conta || !*conta || !s || !s->url[0]) return;
  pthread_mutex_lock(&trava);
  if (!garantir()) { pthread_mutex_unlock(&trava); return; }
  carregarSemTrava();
  if (!persistir) nEnt = 0;            // o de sempre: so a ultima
  i = acharSemTrava(alvo, conta, perfil);
  if (i < 0) {
    if (nEnt >= FONTEVOLTA_MAX) {      // cheia: sai a mais antiga
      int j, velho = 0;
      for (j = 1; j < nEnt; j++) if (ent[j].obtidoS < ent[velho].obtidoS) velho = j;
      i = velho;
    } else i = nEnt++;
  }
  snprintf(ent[i].alvo, sizeof ent[i].alvo, "%s", alvo);
  snprintf(ent[i].conta, sizeof ent[i].conta, "%s", conta);
  ent[i].perfil = perfil;
  ent[i].s = *s;
  ent[i].obtido = agora - idadeLink;
  ent[i].obtidoS = (long long)time(NULL) - (long long)(idadeLink / 1000u);
  gravarSemTrava();
  pthread_mutex_unlock(&trava);
  printf("[voltafonte] guardada para %s (link com %u s)\n", alvo, (unsigned)(idadeLink / 1000u));
  fflush(stdout);
}

int fontevolta_toca_guardada(int escolherManual, int canal, int playerAberto) {
  if (escolherManual || canal || !playerAberto) return 0;
  return 1;
}

int fontevolta_pegar(const char *alvo, const char *conta, int perfil,
                     Uint32 agora, Stream *saida) {
  const char *por = NULL;
  unsigned idadeS;
  int i;
  pthread_mutex_lock(&trava);
  carregarSemTrava();
  if (!nEnt || !alvo || !*alvo) { pthread_mutex_unlock(&trava); return 0; }
  if (!persistir) {
    // O de sempre, inalterado: uma entrada, validade em ticks.
    Uint32 idade = agora - ent[0].obtido;   // unsigned: relogio "para tras" = vencida
    if (idade >= FONTEVOLTA_VALIDADE_MS) por = "vencida";
    else if (!conta || strcmp(conta, ent[0].conta) || perfil != ent[0].perfil) por = "outra conta/perfil";
    if (por) {
      nEnt = 0;
      pthread_mutex_unlock(&trava);
      printf("[voltafonte] descartada: %s (%u s)\n", por, (unsigned)(idade / 1000u));
      fflush(stdout);
      return 0;
    }
    // Outro titulo/episodio nao apaga: o Retomar do anterior ainda pode vir.
    if (strcmp(alvo, ent[0].alvo)) { pthread_mutex_unlock(&trava); return 0; }
    if (saida) *saida = ent[0].s;
    pthread_mutex_unlock(&trava);
    printf("[voltafonte] usando a fonte guardada de %s (link com %u s)\n", alvo,
           (unsigned)(idade / 1000u));
    fflush(stdout);
    return 1;
  }
  if (!conta) { pthread_mutex_unlock(&trava); return 0; }
  i = acharSemTrava(alvo, conta, perfil);
  if (i < 0) { pthread_mutex_unlock(&trava); return 0; }
  { long long agoraS = (long long)time(NULL), d = agoraS - ent[i].obtidoS;
    // Relogio para tras (a TV arranca com a hora errada): vencida, e a busca
    // normal assume. Errar para o lado da busca custa so o caminho de sempre.
    idadeS = d < 0 ? 0u : (unsigned)(d > 0x7fffffffLL ? 0x7fffffff : d);
    if (d < 0) por = "relogio para tras";
    else if (d >= (long long)validadeS) por = "vencida"; }
  if (por) {
    removerSemTrava(i);
    gravarSemTrava();
    pthread_mutex_unlock(&trava);
    printf("[voltafonte] descartada para %s: %s (%u s)\n", alvo, por, idadeS);
    fflush(stdout);
    return 0;
  }
  if (saida) *saida = ent[i].s;
  pthread_mutex_unlock(&trava);
  printf("[voltafonte] usando a fonte guardada de %s (link com %u s)\n", alvo, idadeS);
  fflush(stdout);
  return 1;
}

int fontevolta_tem_url(const char *url) {
  int r = 0, i;
  if (!url || !*url) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nEnt && !r; i++) r = !strcmp(url, ent[i].s.url);
  pthread_mutex_unlock(&trava);
  return r;
}

void fontevolta_esquecer(const char *porque) {
  int tinha;
  pthread_mutex_lock(&trava);
  // Ligado, as entradas sao por perfil e por conta: trocar de perfil nao tem o
  // que apagar. Desligado, o de sempre.
  if (persistir) { pthread_mutex_unlock(&trava); return; }
  tinha = nEnt;
  nEnt = 0;
  pthread_mutex_unlock(&trava);
  if (tinha) {
    printf("[voltafonte] esquecida: %s\n", porque ? porque : "?");
    fflush(stdout);
  }
}

void fontevolta_esquecer_alvo(const char *alvo, const char *conta, int perfil, const char *porque) {
  int i = -1;
  pthread_mutex_lock(&trava);
  if (!persistir) { pthread_mutex_unlock(&trava); fontevolta_esquecer(porque); return; }
  carregarSemTrava();
  if (alvo && conta) i = acharSemTrava(alvo, conta, perfil);
  if (i >= 0) { removerSemTrava(i); gravarSemTrava(); }
  pthread_mutex_unlock(&trava);
  if (i >= 0) {
    printf("[voltafonte] esquecida para %s: %s\n", alvo, porque ? porque : "?");
    fflush(stdout);
  }
}

void fontevolta_esquecer_tudo(void) {
  pthread_mutex_lock(&trava);
  nEnt = 0;
  carregado = persistir;   // ja sabemos o que ha no disco: nada
  if (arqApagar) arqApagar(ARQUIVO);
  pthread_mutex_unlock(&trava);
}

// --- conferencia --------------------------------------------------------------

static int (*sonda)(const char *, const char *) = NULL;
static unsigned confGeracao;
static int confEstado;

void fontevolta_definir_sonda(int (*s)(const char *, const char *)) { sonda = s; }

typedef struct { unsigned geracao; char url[NV_MEDIA_URL_MAX]; char cab[512]; } Pedido;

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
