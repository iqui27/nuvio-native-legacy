// Ver atividade.h. A disciplina e a de recomenda.c: o estado atras de um mutex,
// a rede num fio, e quem chama do laco de desenho nunca espera rede nenhuma.
#include "atividade.h"
#include "recresp.h"
#include "recomenda.h"
#include "dados.h"
#include "perfis.h"
#include "rede.h"
#include "sessao.h"
#include "trakt.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

#define ATIV_TEMPO_REDE 12

static SDL_mutex *mtx;
static int  permitido;            // 0 = nada sai (padrao)
static char fila[ATIV_FILA_MAX][1600];
static int  nFila;
static int  fioVivo;
static int  perfilLido = -1;      // de que perfil a fila em memoria veio

// O trecho de reproducao em curso. So o fio principal toca nisto.
static struct {
  int    ativo;                   // houve "inicio"
  char   imdb[24];
  int    t, e;
  AtivEvento base;                // titulo, midia, poster, rec ja resolvidos
  double seg;                     // assistido desde o ultimo evento
  double desdeProg;               // tocando desde o ultimo "progresso"
  int    fimEnviado;
  double pos, dur;                // os ultimos vistos, para a parada implicita
} trecho;

// A origem marcada pelo painel Social (atividade_marcar_origem).
static long long origemRec;
static char      origemImdb[24];
static char      origemNome[64];

static void travar(void)   { if (!mtx) mtx = SDL_CreateMutex(); SDL_LockMutex(mtx); }
static void destravar(void){ SDL_UnlockMutex(mtx); }

// --- puro ---------------------------------------------------------------------

void atividade_id_puro(char *dst, size_t tam, const char *src) {
  size_t k = 0;
  if (!tam) return;
  if (!src) src = "";
  while (src[k] && src[k] != ':' && k + 1 < tam) { dst[k] = src[k]; k++; }
  dst[k] = 0;
}

// Escapa para dentro de uma string JSON: aspas, barra e controles (os mesmos
// tres casos de recomenda.c). UTF-8 passa como esta — JSON aceita.
static size_t esc(char *dst, size_t tam, const char *s) {
  size_t k = 0;
  if (!tam) return 0;
  if (!s) s = "";
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c == '"' || c == '\\') {
      if (k + 2 >= tam) return 0;
      dst[k++] = '\\'; dst[k++] = (char)c;
    } else if (c < 0x20) {
      if (k + 6 >= tam) return 0;
      k += (size_t)snprintf(dst + k, tam - k, "\\u%04x", c);
    } else {
      if (k + 1 >= tam) return 0;
      dst[k++] = (char)c;
    }
  }
  dst[k] = 0;
  return k;
}

static int pct100(int p) { return p < 0 ? 0 : p > 100 ? 100 : p; }

size_t atividade_json(const AtivEvento *e, char *dst, size_t tam) {
  char ev[32], im[64], md[24], ti[480], po[1100];
  int n;
  if (!e || !dst || tam < 2) return 0;
  // Um campo que nao coube vira vazio, e nao uma string cortada no meio de um
  // escape: "\\" cortado depois da primeira barra quebraria o JSON inteiro.
  if (!esc(ev, sizeof ev, e->ev) && e->ev[0]) return 0;
  if (!esc(im, sizeof im, e->imdb)) im[0] = 0;
  if (!esc(md, sizeof md, e->midia)) md[0] = 0;
  if (!esc(ti, sizeof ti, e->titulo)) ti[0] = 0;
  if (!esc(po, sizeof po, e->poster)) po[0] = 0;
  n = snprintf(dst, tam,
               "{\"ev\":\"%s\",\"imdb\":\"%s\",\"midia\":\"%s\",\"titulo\":\"%s\","
               "\"poster\":\"%s\",\"temporada\":%d,\"episodio\":%d,\"pct\":%d,"
               "\"seg\":%d,\"reacao\":%d,\"rec\":%lld}",
               ev, im, md, ti, po,
               e->temporada > 0 ? e->temporada : 0,
               e->episodio > 0 ? e->episodio : 0,
               pct100(e->pct), e->seg > 0 ? e->seg : 0,
               e->reacao > 0 ? 1 : e->reacao < 0 ? -1 : 0,
               e->rec > 0 ? e->rec : 0LL);
  if (n < 0 || (size_t)n >= tam) { dst[0] = 0; return 0; }
  return (size_t)n;
}

// --- disco --------------------------------------------------------------------

static const char *arquivo(void) {
  static char nome[48];
  int p = perfis_ativo();
  if (p <= 0) snprintf(nome, sizeof nome, "atividade-fila.txt");
  else snprintf(nome, sizeof nome, "atividade-fila-p%d.txt", p);
  return nome;
}

// Com o mutex TOMADO.
static void gravar(void) {
  // ESTATICO, e nao na pilha: ~51 KB, e o fio de envio tambem grava (a pilha
  // de um fio no webOS nao e lugar para isso — a nota de recomenda.c).
  static char buf[ATIV_FILA_MAX * (sizeof fila[0] + 1) + 1];
  size_t k = 0;
  int i;
  if (!nFila) { dados_apagar(arquivo()); return; }
  for (i = 0; i < nFila; i++) {
    size_t l = strlen(fila[i]);
    memcpy(buf + k, fila[i], l); k += l;
    buf[k++] = '\n';
  }
  buf[k] = 0;
  dados_gravar(arquivo(), buf);
}

// Com o mutex TOMADO.
static void ler(void) {
  char *b, *p;
  nFila = 0;
  perfilLido = perfis_ativo();
  b = dados_ler(arquivo());
  if (!b) return;
  for (p = b; *p && nFila < ATIV_FILA_MAX;) {
    char *fim = strchr(p, '\n');
    size_t l = fim ? (size_t)(fim - p) : strlen(p);
    // So linha que parece um objeto inteiro: um arquivo cortado no meio de uma
    // gravacao nao vira um POST com JSON quebrado.
    if (l > 2 && l < sizeof fila[0] && p[0] == '{' && p[l - 1] == '}') {
      memcpy(fila[nFila], p, l); fila[nFila][l] = 0; nFila++;
    }
    if (!fim) break;
    p = fim + 1;
  }
  free(b);
}

// --- envio --------------------------------------------------------------------

int atividade_permitido(void) { return permitido; }
int atividade_envia(void) { return recomenda_ativo() && permitido > 0; }

// Os cabecalhos sao os de recomenda.c (recomenda_cabecalhos): Authorization,
// X-Nuvio-Auth e, num perfil que nao e o principal, X-Nuvio-Perfil — sem este o
// evento cairia na pessoa do perfil principal. 0 = sem identidade ainda.
static char fioAut[3200], fioVia[32], fioPerfil[48], fioUrl[600];
static int identidade(const char **cab) {
  return recomenda_cabecalhos(cab, fioAut, sizeof fioAut, fioVia, sizeof fioVia,
                              fioPerfil, sizeof fioPerfil);
}

static int fioEnviar(void *u) {
  const char *cab[4];
  static char corpo[sizeof fila[0]];   // so este fio usa; um fio vivo por vez
  (void)u;
  snprintf(fioUrl, sizeof fioUrl, "%s/v1/atividade", NV_REC_URL);
  for (;;) {
    int st = 0, manter;
    char *r;
    travar();
    if (nFila < 1 || !atividade_envia() || !identidade(cab)) { fioVivo = 0; destravar(); return 0; }
    snprintf(corpo, sizeof corpo, "%s", fila[0]);
    destravar();
    r = rede_postar_st(fioUrl, ATIV_TEMPO_REDE, cab, corpo, &st);
    free(r);
    // FALHA DE TRANSPORTE, 5xx, 429 e 401 (identidade ainda nao reconhecida)
    // ficam na fila para a proxima vez. Qualquer outro status — 2xx, e tambem
    // 400/404 de um servidor que ainda nao conhece o evento — sai da fila:
    // reenviar para sempre um evento que o servidor recusa so gastaria rede.
    manter = st == 0 || st >= 500 || st == 429 || st == 401;
    printf("[atividade] envio HTTP %d%s\n", st, manter ? " (fica na fila)" : "");
    fflush(stdout);
    travar();
    if (manter) { fioVivo = 0; destravar(); return 0; }
    // O primeiro ainda e o que saiu: so este fio remove, e enfileirar acrescenta
    // no fim (ou descarta o mais VELHO quando cheia — ver enfileirar).
    if (nFila > 0 && !strcmp(fila[0], corpo)) {
      memmove(fila, fila + 1, sizeof fila[0] * (size_t)(nFila - 1));
      nFila--;
      gravar();
    }
    destravar();
  }
}

// Com o mutex TOMADO.
static void acordarFio(void) {
#ifndef ATIV_SEM_FIO
  SDL_Thread *f;
  if (fioVivo || nFila < 1 || !atividade_envia()) return;
  fioVivo = 1;
  f = SDL_CreateThread(fioEnviar, "atividade", NULL);
  if (!f) { fioVivo = 0; return; }
  SDL_DetachThread(f);
#endif
}

static void enfileirar(const AtivEvento *e) {
  char linha[sizeof fila[0]];
  if (!atividade_envia()) return;
  if (!atividade_json(e, linha, sizeof linha)) return;
  travar();
  if (perfilLido != perfis_ativo()) ler();
  // Cheia: sai o mais VELHO. O evento novo descreve o agora; o velho, se nao
  // saiu ate aqui, provavelmente nao sai mais.
  if (nFila >= ATIV_FILA_MAX) {
    memmove(fila, fila + 1, sizeof fila[0] * (size_t)(ATIV_FILA_MAX - 1));
    nFila = ATIV_FILA_MAX - 1;
  }
  snprintf(fila[nFila++], sizeof fila[0], "%s", linha);
  gravar();
  acordarFio();
  destravar();
  printf("[atividade] %s %s T%dE%d %d%% %ds\n", e->ev, e->imdb, e->temporada,
         e->episodio, e->pct, e->seg);
  fflush(stdout);
}

void atividade_iniciar(void) {
  travar();
  ler();
  acordarFio();
  destravar();
}

void atividade_definir_permitido(int nivel) {
  travar();
  permitido = nivel > 0 ? nivel : 0;
  // DESLIGAR APAGA O QUE NAO SAIU — a mesma promessa de recomenda.c.
  if (!permitido) { nFila = 0; gravar(); }
  else acordarFio();
  destravar();
}

int atividade_fila_n(void) {
  int n;
  travar(); n = nFila; destravar();
  return n;
}

int atividade_fila_linha(int i, char *dst, size_t tam) {
  int ok = 0;
  travar();
  if (i >= 0 && i < nFila && dst && tam) { snprintf(dst, tam, "%s", fila[i]); ok = 1; }
  destravar();
  return ok;
}

// --- origem ---------------------------------------------------------------------

void atividade_marcar_origem(long long rec, const char *imdb, const char *nome) {
  origemRec = rec > 0 ? rec : 0;
  atividade_id_puro(origemImdb, sizeof origemImdb, imdb);
  snprintf(origemNome, sizeof origemNome, "%s", nome ? nome : "");
}

long long atividade_origem(const char *imdb, char *nome, size_t tamNome) {
  char id[24];
  long long melhor = 0, criado = -1;
  int i, n;
  if (nome && tamNome) nome[0] = 0;
  atividade_id_puro(id, sizeof id, imdb);
  if (!id[0]) return 0;
  if (origemRec > 0 && !strcmp(origemImdb, id)) {
    if (nome && tamNome) snprintf(nome, tamNome, "%s", origemNome);
    return origemRec;
  }
  // A MAIS NOVA recomendacao recebida deste titulo. A lista guardada ja e o
  // que o servidor retem (90 dias), entao "veio de alguem" nao envelhece mais
  // que isso. Sem servico, recomenda_n e 0.
  n = recomenda_ativo() ? recomenda_n() : 0;
  for (i = 0; i < n; i++) {
    RecItem r;
    char ri[24];
    if (!recomenda_item(i, &r)) continue;
    atividade_id_puro(ri, sizeof ri, r.imdb);
    if (strcmp(ri, id) || r.criado <= criado) continue;
    criado = r.criado; melhor = r.id;
    if (nome && tamNome) snprintf(nome, tamNome, "%s", r.deNome);
  }
  return melhor;
}

// --- ganchos ----------------------------------------------------------------------

static void baseDe(AtivEvento *e, const CatItem *ci) {
  memset(e, 0, sizeof *e);
  atividade_id_puro(e->imdb, sizeof e->imdb, ci->imdb);
  snprintf(e->midia, sizeof e->midia, "%s", !strcmp(ci->tipo, "series") ? "series" : "movie");
  snprintf(e->titulo, sizeof e->titulo, "%s", ci->titulo);
  snprintf(e->poster, sizeof e->poster, "%s", ci->poster);
}

static int pctDe(double pos, double dur) {
  if (dur <= 1.0 || pos <= 0.0) return 0;
  return pct100((int)(pos * 100.0 / dur));
}

static void emitir(const char *ev, int pct) {
  AtivEvento e = trecho.base;
  snprintf(e.ev, sizeof e.ev, "%s", ev);
  // TERMINOU O QUE UM AMIGO MANDOU: a rec vai para "Assistidas" NESTE
  // aparelho e no servidor (recresp.h), com ou sem o envio de atividade
  // permitido — e o mesmo "fim" que vale para o resto do app, e a resposta
  // direta a quem mandou nao e atividade automatica.
  if (!strcmp(ev, "fim") && e.rec > 0) recresp_marcar_assistida(e.rec);
  e.pct = pct;
  e.seg = (int)(trecho.seg + 0.5);
  trecho.seg = 0.0;
  enfileirar(&e);
}

// Fecha o trecho: a parada. `concluiu` = creditos/fim pela regra do player.
static void fecharTrecho(double pos, double dur, int concluiu) {
  int pct = pctDe(pos, dur);
  if (trecho.ativo && !trecho.fimEnviado) {
    if (concluiu || pct >= ATIV_FIM_PCT) emitir("fim", concluiu ? 100 : pct);
    else if (pct < ATIV_ABANDONO_PCT)    emitir("abandono", pct);
    else                                  emitir("progresso", pct);
  }
  memset(&trecho, 0, sizeof trecho);
}

void atividade_player_passo(const CatItem *ci, int t, int e, double pos, double dur,
                            int tocando, int concluiu, float dt) {
  char id[24];
  int pct;
  if (!ci || !ci->imdb[0]) return;
  atividade_id_puro(id, sizeof id, ci->imdb);
  if (strcmp(ci->tipo, "series")) t = e = 0;
  // OUTRO TITULO (ou outro episodio) sem a saida do player no meio: o trecho
  // anterior termina aqui, com a ultima posicao que se viu dele.
  if (trecho.ativo && (strcmp(trecho.imdb, id) || trecho.t != t || trecho.e != e))
    fecharTrecho(trecho.pos, trecho.dur, 0);
  trecho.pos = pos; trecho.dur = dur;
  if (!tocando) return;
  pct = pctDe(pos, dur);
  if (!trecho.ativo) {
    memset(&trecho, 0, sizeof trecho);
    trecho.ativo = 1;
    snprintf(trecho.imdb, sizeof trecho.imdb, "%s", id);
    trecho.t = t; trecho.e = e;
    trecho.pos = pos; trecho.dur = dur;
    baseDe(&trecho.base, ci);
    trecho.base.temporada = t; trecho.base.episodio = e;
    trecho.base.rec = atividade_origem(id, NULL, 0);
    emitir("inicio", pct);
  }
  if (dt > 0.0f && dt < 2.0f) { trecho.seg += dt; trecho.desdeProg += dt; }
  if (!trecho.fimEnviado && (concluiu || pct >= ATIV_FIM_PCT)) {
    trecho.fimEnviado = 1;
    emitir("fim", concluiu ? 100 : pct);
    return;
  }
  if (trecho.desdeProg >= ATIV_PROGRESSO_S) {
    trecho.desdeProg = 0.0;
    // Depois do fim nao ha mais "progresso": os creditos rolando nao sao
    // tempo de filme, e o servidor ja sabe que acabou.
    if (!trecho.fimEnviado) emitir("progresso", pct);
  }
}

void atividade_player_saiu(double pos, double dur, int concluiu) {
  if (!trecho.ativo) { memset(&trecho, 0, sizeof trecho); return; }
  fecharTrecho(pos > 0.0 ? pos : trecho.pos, dur > 1.0 ? dur : trecho.dur, concluiu);
}

void atividade_salvo(const CatItem *ci, int salvar) {
  AtivEvento e;
  if (!ci || !ci->imdb[0] || !salvar) return;
  baseDe(&e, ci);
  if (strncmp(e.imdb, "tt", 2)) return;
  snprintf(e.ev, sizeof e.ev, "salvo");
  e.rec = atividade_origem(e.imdb, NULL, 0);
  enfileirar(&e);
}

// "Assistiram juntos" (Watch Together, decisao 8): cada TV manda o PROPRIO
// evento quando a sessao acaba; o servidor junta no feed quem assistiu o mesmo
// titulo na mesma hora, e so quem tem o alcance ligado aparece.
void atividade_junto(const char *imdb, const char *midia, const char *titulo, const char *poster,
                     int temporada, int episodio, int seg) {
  AtivEvento e;
  memset(&e, 0, sizeof e);
  atividade_id_puro(e.imdb, sizeof e.imdb, imdb);
  if (strncmp(e.imdb, "tt", 2)) return;
  snprintf(e.ev, sizeof e.ev, "junto");
  snprintf(e.midia, sizeof e.midia, "%s", midia && !strcmp(midia, "series") ? "series" : "movie");
  snprintf(e.titulo, sizeof e.titulo, "%s", titulo ? titulo : "");
  snprintf(e.poster, sizeof e.poster, "%s", poster ? poster : "");
  e.temporada = temporada; e.episodio = episodio;
  e.seg = seg;
  enfileirar(&e);
}

void atividade_reacao(const char *imdb, const char *midia, const char *titulo,
                      const char *poster, int reacao, long long rec) {
  AtivEvento e;
  memset(&e, 0, sizeof e);
  atividade_id_puro(e.imdb, sizeof e.imdb, imdb);
  if (strncmp(e.imdb, "tt", 2)) return;
  snprintf(e.ev, sizeof e.ev, "reacao");
  snprintf(e.midia, sizeof e.midia, "%s", midia && !strcmp(midia, "series") ? "series" : "movie");
  snprintf(e.titulo, sizeof e.titulo, "%s", titulo ? titulo : "");
  snprintf(e.poster, sizeof e.poster, "%s", poster ? poster : "");
  e.reacao = reacao;
  e.rec = rec;
  enfileirar(&e);
}
