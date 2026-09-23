#include "progresso.h"
#include "dados.h"
#include "perfis.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define ARQ "progresso.txt"
#define CABECALHO "#nvprog2"

// Dois fios tocam aqui: o principal (player fechando, olho, pos-play, catalogo
// reaplicando) e o do sync (pendentes para o push, marcar empurrados). Um
// mutex por chamada publica; as funcoes internas assumem o mutex tomado.
static pthread_mutex_t tranca = PTHREAD_MUTEX_INITIALIZER;
#define TRANCAR()   pthread_mutex_lock(&tranca)
#define DESTRANCAR() pthread_mutex_unlock(&tranca)

static ProgRegistro regs[PROG_MAX];
static int nRegs;
static int carregado;
static long long (*relogio)(void);

static long long relogioPadrao(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (long long)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

long long prog_agora_ms(void) { return relogio ? relogio() : relogioPadrao(); }
void prog_definir_relogio(long long (*r)(void)) { relogio = r; }
void prog_invalidar(void) { TRANCAR(); carregado = 0; nRegs = 0; DESTRANCAR(); }

// ------------------------------------------------------------ identidade (sem estado)

void prog_content_id(char *dst, unsigned n, const char *imdb, int *temporada, int *episodio) {
  const char *dp;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!imdb) return;
  dp = strchr(imdb, ':');
  if (dp) {
    unsigned L = (unsigned)(dp - imdb);
    if (L >= n) L = n - 1;
    memcpy(dst, imdb, L);
    dst[L] = 0;
    if (temporada && episodio) {
      int t = 0, e = 0;
      if (sscanf(dp + 1, "%d:%d", &t, &e) == 2 && t >= 0 && e > 0) { *temporada = t; *episodio = e; }
    }
  } else {
    snprintf(dst, n, "%s", imdb);
  }
}

void prog_chave(char *dst, unsigned n, const char *contentId, int temporada, int episodio) {
  char id[24];
  if (!dst || !n) return;
  prog_content_id(id, sizeof id, contentId, NULL, NULL);
  if (id[0] && temporada >= 0 && episodio > 0)
    snprintf(dst, n, "%s_s%de%d", id, temporada, episodio);
  else
    snprintf(dst, n, "%s", id);
}

// ------------------------------------------------------------ disco (mutex tomado)

// Linha do formato ANTIGO: "imdb pos dur [temp ep]" (separado por tab ou
// espaco), escrita por catalogo.c. `imdb` podia ser composto ("tt123:4:9").
static int lerLinhaAntiga(const char *linha, ProgRegistro *r) {
  char id[40];
  double pos, dur;
  int temp = 0, ep = 0, n;
  n = sscanf(linha, "%39s %lf %lf %d %d", id, &pos, &dur, &temp, &ep);
  if (n < 3 || dur <= 1.0) return 0;
  memset(r, 0, sizeof *r);
  { int tI = 0, eI = 0;
    prog_content_id(r->contentId, sizeof r->contentId, id, &tI, &eI);
    // Colunas 4-5 ganham; o id composto e o fallback.
    if (!(temp >= 0 && ep > 0)) { temp = tI; ep = eI; } }
  if (!r->contentId[0]) return 0;
  r->temporada = ep > 0 ? temp : 0;
  r->episodio  = ep > 0 ? ep : 0;
  snprintf(r->tipo, sizeof r->tipo, "%s", ep > 0 ? "series" : "movie");
  prog_chave(r->chave, sizeof r->chave, r->contentId, r->temporada, r->episodio);
  r->posSeg = pos;
  r->durSeg = dur;
  r->lastWatchedMs = 0;
  r->pendente = 1;        // nunca foi empurrado com a chave certa
  r->perfil = perfis_ativo();
  return 1;
}

// Linha do formato NOVO (10 colunas, tab):
// perfil chave contentId tipo temp ep posSeg durSeg lastMs pendente
static int lerLinhaNova(const char *linha, ProgRegistro *r) {
  int n;
  memset(r, 0, sizeof *r);
  n = sscanf(linha, "%d\t%47s\t%23s\t%7s\t%d\t%d\t%lf\t%lf\t%lld\t%d",
             &r->perfil, r->chave, r->contentId, r->tipo,
             &r->temporada, &r->episodio, &r->posSeg, &r->durSeg,
             &r->lastWatchedMs, &r->pendente);
  if (n != 10 || !r->chave[0] || !r->contentId[0]) return 0;
  return 1;
}

static void carregar(void) {
  char *buf, *linha, *ctx;
  int novo = 0;
  if (carregado) return;
  carregado = 1;
  nRegs = 0;
  buf = dados_ler(ARQ);
  if (!buf) return;
  for (linha = strtok_r(buf, "\n", &ctx); linha && nRegs < PROG_MAX;
       linha = strtok_r(NULL, "\n", &ctx)) {
    ProgRegistro r;
    if (!linha[0] || linha[0] == '\r') continue;
    if (!strncmp(linha, CABECALHO, strlen(CABECALHO))) { novo = 1; continue; }
    if (novo ? lerLinhaNova(linha, &r) : lerLinhaAntiga(linha, &r)) {
      // Duplicata (mesmo perfil e chave) fica com a mais nova.
      int i, dup = -1;
      for (i = 0; i < nRegs; i++)
        if (regs[i].perfil == r.perfil && !strcmp(regs[i].chave, r.chave)) { dup = i; break; }
      if (dup >= 0) {
        if (r.lastWatchedMs >= regs[dup].lastWatchedMs) regs[dup] = r;
      } else {
        regs[nRegs++] = r;
      }
    }
  }
  free(buf);
  if (!novo && nRegs) printf("[progresso] %d linhas migradas do formato antigo\n", nRegs);
}

static int gravar(void) {
  // 10 colunas curtas cabem folgadas em 160 bytes por linha.
  size_t tam = (size_t)nRegs * 160 + 64;
  char *buf = malloc(tam), *p;
  int i, ok;
  if (!buf) return 0;
  p = buf;
  p += sprintf(p, "%s\n", CABECALHO);
  for (i = 0; i < nRegs; i++) {
    const ProgRegistro *r = &regs[i];
    p += sprintf(p, "%d\t%s\t%s\t%s\t%d\t%d\t%.0f\t%.0f\t%lld\t%d\n",
                 r->perfil, r->chave, r->contentId, r->tipo,
                 r->temporada, r->episodio, r->posSeg, r->durSeg,
                 r->lastWatchedMs, r->pendente ? 1 : 0);
  }
  ok = dados_gravar(ARQ, buf);
  free(buf);
  return ok;
}

// Quando o arquivo esta cheio, sai a linha NAO pendente mais antiga. Pendente
// nunca sai: e informacao que so este aparelho tem.
static int abrirVaga(void) {
  int i, alvo = -1;
  if (nRegs < PROG_MAX) return nRegs;
  for (i = 0; i < nRegs; i++) {
    if (regs[i].pendente) continue;
    if (alvo < 0 || regs[i].lastWatchedMs < regs[alvo].lastWatchedMs) alvo = i;
  }
  return alvo;
}

static int achar(int perfil, const char *chave) {
  int i;
  for (i = 0; i < nRegs; i++)
    if (regs[i].perfil == perfil && !strcmp(regs[i].chave, chave)) return i;
  return -1;
}

static int maisNovoPrimeiro(const void *a, const void *b) {
  const ProgRegistro *x = a, *y = b;
  if (x->lastWatchedMs != y->lastWatchedMs) return x->lastWatchedMs < y->lastWatchedMs ? 1 : -1;
  return strcmp(x->chave, y->chave);
}

// ------------------------------------------------------------ leitura

int prog_ler(ProgRegistro *saida, int max) {
  int i, k = 0, perfil = perfis_ativo();
  TRANCAR();
  carregar();
  for (i = 0; i < nRegs && k < max; i++)
    if (regs[i].perfil == perfil) saida[k++] = regs[i];
  DESTRANCAR();
  qsort(saida, (size_t)k, sizeof *saida, maisNovoPrimeiro);
  return k;
}

int prog_por_chave(const char *chave, ProgRegistro *saida) {
  int i;
  if (!chave || !*chave) return 0;
  TRANCAR();
  carregar();
  i = achar(perfis_ativo(), chave);
  if (i >= 0 && saida) *saida = regs[i];
  DESTRANCAR();
  return i >= 0;
}

int prog_pendentes(ProgRegistro *saida, int max) {
  int i, k = 0, perfil = perfis_ativo();
  TRANCAR();
  carregar();
  for (i = 0; i < nRegs && k < max; i++)
    if (regs[i].perfil == perfil && regs[i].pendente) saida[k++] = regs[i];
  DESTRANCAR();
  return k;
}

// ------------------------------------------------------------ escrita

int prog_gravar_local(const char *imdb, int temporada, int episodio,
                      double posSeg, double durSeg) {
  ProgRegistro r;
  int i, ok;
  if (!imdb || !*imdb || durSeg <= 1.0) return 0;
  memset(&r, 0, sizeof r);
  { int tI = 0, eI = 0;
    prog_content_id(r.contentId, sizeof r.contentId, imdb, &tI, &eI);
    if (!(episodio > 0)) { temporada = tI; episodio = eI; } }
  if (!r.contentId[0]) return 0;
  r.perfil = perfis_ativo();
  r.temporada = episodio > 0 ? temporada : 0;
  r.episodio  = episodio > 0 ? episodio : 0;
  snprintf(r.tipo, sizeof r.tipo, "%s", episodio > 0 ? "series" : "movie");
  prog_chave(r.chave, sizeof r.chave, r.contentId, r.temporada, r.episodio);
  r.posSeg = posSeg < 0 ? 0 : posSeg;
  r.durSeg = durSeg;
  r.lastWatchedMs = prog_agora_ms();
  r.pendente = 1;
  TRANCAR();
  carregar();
  i = achar(r.perfil, r.chave);
  if (i < 0) { i = abrirVaga(); if (i >= 0 && i == nRegs) nRegs++; }
  if (i < 0) { DESTRANCAR(); return 0; }
  regs[i] = r;
  ok = gravar();
  DESTRANCAR();
  return ok;
}

int prog_aplicar_remoto(const ProgRegistro *rem) {
  ProgRegistro r;
  int i;
  if (!rem || !rem->contentId[0] || rem->durSeg <= 1.0) return 0;
  r = *rem;
  r.perfil = perfis_ativo();
  r.pendente = 0;
  if (!r.chave[0]) prog_chave(r.chave, sizeof r.chave, r.contentId, r.temporada, r.episodio);
  if (!r.tipo[0]) snprintf(r.tipo, sizeof r.tipo, "%s", r.episodio > 0 ? "series" : "movie");
  TRANCAR();
  carregar();
  i = achar(r.perfil, r.chave);
  if (i >= 0) {
    if (regs[i].pendente || r.lastWatchedMs <= regs[i].lastWatchedMs) { DESTRANCAR(); return 0; }
  } else {
    i = abrirVaga();
    if (i < 0) { DESTRANCAR(); return 0; }
    if (i == nRegs) nRegs++;
  }
  regs[i] = r;
  gravar();
  DESTRANCAR();
  return 1;
}

void prog_marcar_empurrados(const char *const *chaves, int n) {
  int i, k, perfil = perfis_ativo(), mudou = 0;
  TRANCAR();
  carregar();
  for (k = 0; k < n; k++) {
    if (!chaves[k]) continue;
    i = achar(perfil, chaves[k]);
    if (i >= 0 && regs[i].pendente) { regs[i].pendente = 0; mudou = 1; }
  }
  if (mudou) gravar();
  DESTRANCAR();
}

void prog_remover(const char *chave) {
  int i;
  if (!chave || !*chave) return;
  TRANCAR();
  carregar();
  i = achar(perfis_ativo(), chave);
  if (i >= 0) { regs[i] = regs[--nRegs]; gravar(); }
  DESTRANCAR();
}

// --- REMOCOES DE "CONTINUAR ASSISTINDO" ---------------------------------------
//
// SO EM MEMORIA, e de proposito. A janela que isto cobre e a do DELETE em voo
// e a do servidor que ainda nao refletiu (segundos, no pior caso o ciclo de
// sync seguinte). Numa abertura nova do app os tres servidores ja receberam o
// DELETE — ou ele falhou, e ai mostrar o card de volta e a verdade, e a pessoa
// tira de novo. Persistir exigiria um formato novo em progresso.txt, que o
// push para a conta le linha a linha como progresso (prog_pendentes).
//
// 32 cabem com folga: a fileira mostra 12, e ninguem tira mais que isso numa
// sessao. Cheio, a vaga mais velha e reaproveitada — e a que o servidor ja
// teve mais tempo de refletir.
#define PROG_REMOVIDOS_MAX 32
static struct { int perfil; char obra[24]; long long ms; } removidos[PROG_REMOVIDOS_MAX];
static int nRemovidos;

void prog_marcar_removido(const char *imdb) {
  char obra[24];
  int i, alvo = -1, perfil = perfis_ativo();
  long long agora = prog_agora_ms();
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return;
  TRANCAR();
  for (i = 0; i < nRemovidos && alvo < 0; i++)
    if (removidos[i].perfil == perfil && !strcmp(removidos[i].obra, obra)) alvo = i;
  if (alvo < 0 && nRemovidos < PROG_REMOVIDOS_MAX) alvo = nRemovidos++;
  if (alvo < 0) {
    alvo = 0;
    for (i = 1; i < nRemovidos; i++) if (removidos[i].ms < removidos[alvo].ms) alvo = i;
  }
  removidos[alvo].perfil = perfil;
  snprintf(removidos[alvo].obra, sizeof removidos[alvo].obra, "%s", obra);
  removidos[alvo].ms = agora;
  DESTRANCAR();
}

int prog_removido_vence(const char *imdb, long long instanteMs) {
  char obra[24];
  int i, perfil = perfis_ativo();
  long long ms = 0;
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return 0;
  TRANCAR();
  for (i = 0; i < nRemovidos; i++)
    if (removidos[i].perfil == perfil && !strcmp(removidos[i].obra, obra)) { ms = removidos[i].ms; break; }
  if (!ms) { DESTRANCAR(); return 0; }
  // EMPATE FICA COM A REMOCAO: o mesmo milissegundo e o proprio item que foi
  // tirado, nao uma sessao nova.
  if (instanteMs > ms) { DESTRANCAR(); return 0; }
  // ASSISTIU DE NOVO AQUI: o player gravou depois da remocao. O item da
  // refacao pode vir do Trakt com o paused_at velho (o scrobble ainda nao
  // chegou la), e sem esta consulta o registro local novo perderia para ele.
  carregar();
  for (i = 0; i < nRegs; i++)
    if (regs[i].perfil == perfil && !strcmp(regs[i].contentId, obra) &&
        regs[i].lastWatchedMs > ms) { DESTRANCAR(); return 0; }
  DESTRANCAR();
  return 1;
}

void prog_esquecer_tudo(void) {
  TRANCAR();
  dados_apagar(ARQ);
  carregado = 0;
  nRegs = 0;
  // Logout: as remocoes eram da conta que saiu.
  nRemovidos = 0;
  DESTRANCAR();
}
