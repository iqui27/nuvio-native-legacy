#include "progresso.h"
#include "idbase.h"
#include "jfid.h"
#include "dados.h"
#include "perfis.h"
#include <pthread.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#define ARQ "progresso.txt"
#define CABECALHO "#nvprog2"

// O sync converte segundos em milissegundos inteiros. NaN, infinito e valores
// fora dessa faixa nao sao progresso e nao podem chegar ao arquivo ou ao cast.
static int temposValidos(double pos, double dur) {
  const double limite = (double)LLONG_MAX / 1000.0;
  return isfinite(pos) && isfinite(dur) && dur > 1.0 &&
         pos < limite && dur < limite;
}

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
  size_t L;
  if (!dst || !n) return;
  dst[0] = 0;
  if (!imdb) return;
  // idbase_len, e nao o corte no primeiro ':': "kitsu:41370" virava "kitsu", a
  // mesma chave de progresso para todo anime do addon (idbase.h).
  L = idbase_len(imdb);
  if (L >= n) L = n - 1;
  memcpy(dst, imdb, L);
  dst[L] = 0;
  if (temporada && episodio && imdb[idbase_len(imdb)] == ':') {
    int t = 0, e = 0;
    if (sscanf(imdb + idbase_len(imdb) + 1, "%d:%d", &t, &e) == 2 && t >= 0 && e > 0) { *temporada = t; *episodio = e; }
  }
}

void prog_chave(char *dst, unsigned n, const char *contentId, int temporada, int episodio) {
  char id[64];
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
  if (n < 3 || !temposValidos(pos, dur)) return 0;
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
  r->posSeg = pos < 0 ? 0 : pos;
  r->durSeg = dur;
  r->lastWatchedMs = 0;
  r->pendente = 1;        // nunca foi empurrado com a chave certa
  r->perfil = perfis_ativo();
  r->tocou = -1;
  return 1;
}

// Linha do formato NOVO (10 colunas, tab, mais `tocou` desde a #205):
// perfil chave contentId tipo temp ep posSeg durSeg lastMs pendente [tocou]
static int lerLinhaNova(const char *linha, ProgRegistro *r) {
  int n;
  memset(r, 0, sizeof *r);
  n = sscanf(linha, "%d\t%47s\t%23s\t%7s\t%d\t%d\t%lf\t%lf\t%lld\t%d\t%d",
             &r->perfil, r->chave, r->contentId, r->tipo,
             &r->temporada, &r->episodio, &r->posSeg, &r->durSeg,
             &r->lastWatchedMs, &r->pendente, &r->tocou);
  if (n != 11 || r->tocou < -1 || r->tocou > 1) r->tocou = -1;
  if (n < 10 || !r->chave[0] || !r->contentId[0] ||
      !temposValidos(r->posSeg, r->durSeg)) return 0;
  if (r->posSeg < 0) r->posSeg = 0;
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
  // Ate os limites dos campos e dos inteiros cabem em 256 bytes por linha.
  // Conferir snprintf tambem impede que uma mudanca futura desses limites
  // transforme um arquivo invalido em escrita alem do buffer.
  size_t tam = (size_t)nRegs * 256 + 64;
  char *buf = malloc(tam), *p;
  int i, ok, escrito;
  if (!buf) return 0;
  p = buf;
  p += snprintf(p, tam, "%s\n", CABECALHO);
  for (i = 0; i < nRegs; i++) {
    const ProgRegistro *r = &regs[i];
    size_t livre = tam - (size_t)(p - buf);
    escrito = snprintf(p, livre, "%d\t%s\t%s\t%s\t%d\t%d\t%.0f\t%.0f\t%lld\t%d\t%d\n",
                 r->perfil, r->chave, r->contentId, r->tipo,
                 r->temporada, r->episodio, r->posSeg, r->durSeg,
                 r->lastWatchedMs, r->pendente ? 1 : 0, r->tocou);
    if (escrito < 0 || (size_t)escrito >= livre) { free(buf); return 0; }
    p += escrito;
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
  int i, k = 0, maisVelho = -1, perfil = perfis_ativo();
  if (!saida || max < 1) return 0;
  TRANCAR();
  carregar();
  for (i = 0; i < nRegs; i++) {
    int j;
    if (regs[i].perfil != perfil) continue;
    if (k < max) { saida[k++] = regs[i]; continue; }
    // O limite vale DEPOIS da escolha dos mais novos. Truncar na ordem do
    // arquivo antes de ordenar descartava justamente a sessao recem-gravada.
    if (maisVelho < 0) {
      maisVelho = 0;
      for (j = 1; j < k; j++)
        if (maisNovoPrimeiro(&saida[j], &saida[maisVelho]) > 0) maisVelho = j;
    }
    if (maisNovoPrimeiro(&regs[i], &saida[maisVelho]) < 0) {
      saida[maisVelho] = regs[i];
      maisVelho = -1;
    }
  }
  DESTRANCAR();
  qsort(saida, (size_t)k, sizeof *saida, maisNovoPrimeiro);
  return k;
}

static int removidoVenceDe(int perfil, const char *imdb, long long instanteMs);

int prog_continuar_de_perfil(int perfil, int pctConcluido, ProgRegistro *saida) {
  return prog_continuar_de_perfil_min(perfil, 1, pctConcluido, saida);
}

int prog_continuar_de_perfil_min(int perfil, int pctMinimo, int pctConcluido, ProgRegistro *saida) {
  int i, melhor = -1;
  if (!saida) return 0;
  TRANCAR();
  carregar();
  for (i = 0; i < nRegs; i++) {
    const ProgRegistro *r = &regs[i];
    double p;
    if (r->perfil != perfil || r->durSeg < 60.0) continue;
    p = r->posSeg / r->durSeg;
    if (!prog_inicio_continuar(r, pctMinimo) || p * 100.0 >= pctConcluido) continue;
    if (removidoVenceDe(perfil, r->contentId, r->lastWatchedMs)) continue;
    if (melhor < 0 || maisNovoPrimeiro(r, &regs[melhor]) < 0) melhor = i;
  }
  if (melhor >= 0) *saida = regs[melhor];
  DESTRANCAR();
  return melhor >= 0;
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
  if (!saida || max < 1) return 0;
  TRANCAR();
  carregar();
  for (i = 0; i < nRegs && k < max; i++)
    if (regs[i].perfil == perfil && regs[i].pendente) saida[k++] = regs[i];
  DESTRANCAR();
  return k;
}

// ------------------------------------------------------------ escrita

int prog_gravar_local(const char *imdb, int temporada, int episodio,
                      double posSeg, double durSeg, int tocou) {
  ProgRegistro r;
  int i, ok;
  if (!imdb || !*imdb || !temposValidos(posSeg, durSeg)) return 0;
  if (jfid_e(imdb)) return 0;   // personal-server ids never sync to the account
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
  r.tocou = tocou;
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
  if (!rem || !rem->contentId[0] || !temposValidos(rem->posSeg, rem->durSeg) ||
      !memchr(rem->contentId, 0, sizeof rem->contentId) ||
      !memchr(rem->chave, 0, sizeof rem->chave) ||
      !memchr(rem->tipo, 0, sizeof rem->tipo)) return 0;
  r = *rem;
  r.perfil = perfis_ativo();
  r.pendente = 0;
  if (r.posSeg < 0) r.posSeg = 0;
  if (!r.chave[0]) prog_chave(r.chave, sizeof r.chave, r.contentId, r.temporada, r.episodio);
  if (!r.tipo[0]) snprintf(r.tipo, sizeof r.tipo, "%s", r.episodio > 0 ? "series" : "movie");
  TRANCAR();
  carregar();
  i = achar(r.perfil, r.chave);
  // `tocou` nao viaja: posicao de outro aparelho e desconhecida (-1), salvo o
  // eco do proprio push (mesma posicao e duracao, updated_at mais novo).
  r.tocou = i >= 0 && fabs(r.posSeg - regs[i].posSeg) <= 5.0 &&
            fabs(r.durSeg - regs[i].durSeg) < 1.0 ? regs[i].tocou : -1;
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

int prog_tocou(const char *imdb) {
  char chave[48];
  ProgRegistro r;
  if (!imdb || !*imdb) return -1;
  prog_chave(chave, sizeof chave, imdb, 0, 0);
  return prog_por_chave(chave, &r) ? r.tocou : -1;
}

void prog_confirmar_empurrados(const ProgRegistro *enviados, int n) {
  int i, k, mudou = 0;
  if (!enviados || n < 1) return;
  TRANCAR();
  carregar();
  for (k = 0; k < n; k++) {
    const ProgRegistro *e = &enviados[k];
    if (!memchr(e->chave, 0, sizeof e->chave)) continue;
    i = achar(e->perfil, e->chave);
    if (i >= 0 && regs[i].pendente && regs[i].lastWatchedMs == e->lastWatchedMs &&
        regs[i].posSeg == e->posSeg && regs[i].durSeg == e->durSeg) {
      regs[i].pendente = 0;
      mudou = 1;
    }
  }
  if (mudou) gravar();
  DESTRANCAR();
}

// Compatibilidade para confirmacoes imediatas por chave. Um pedido de rede
// em voo usa prog_confirmar_empurrados com a copia que de fato enviou.
void prog_marcar_empurrados(const char *const *chaves, int n) {
  int i, k, perfil = perfis_ativo(), mudou = 0;
  if (!chaves || n < 1) return;
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

// Vale para QUALQUER perfil: a escolha de perfil olha o ultimo item de quem
// ainda nao esta ativo. Chamar com a trava tomada.

int prog_removido_vence(const char *imdb, long long instanteMs) {
  int r;
  TRANCAR();
  r = removidoVenceDe(perfis_ativo(), imdb, instanteMs);
  DESTRANCAR();
  return r;
}

static int removidoVenceDe(int perfil, const char *imdb, long long instanteMs) {
  char obra[24];
  int i;
  long long ms = 0;
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return 0;
  for (i = 0; i < nRemovidos; i++)
    if (removidos[i].perfil == perfil && !strcmp(removidos[i].obra, obra)) { ms = removidos[i].ms; break; }
  if (!ms) return 0;
  // EMPATE FICA COM A REMOCAO: o mesmo milissegundo e o proprio item que foi
  // tirado, nao uma sessao nova.
  if (instanteMs > ms) return 0;
  // ASSISTIU DE NOVO AQUI: o player gravou depois da remocao. O item da
  // refacao pode vir do Trakt com o paused_at velho (o scrobble ainda nao
  // chegou la), e sem esta consulta o registro local novo perderia para ele.
  carregar();
  for (i = 0; i < nRegs; i++)
    if (regs[i].perfil == perfil && !strcmp(regs[i].contentId, obra) &&
        regs[i].lastWatchedMs > ms) return 0;
  return 1;
}

// --- "OCULTO DE CONTINUAR ASSISTINDO" (#203), PERSISTIDO ----------------------
//
// O "Tirar de Continuar assistindo" nao fazia nada num item "A seguir": ele nao
// tem ponto de retomada para apagar, e o proximo episodio e RECALCULADO a cada
// ciclo (Trakt/Simkl/conta). A lista abaixo guarda "esta obra, neste perfil,
// foi tirada em <instante>" EM DISCO (cwoculto.txt) e vale para todas as fontes
// (descoberta.c aplica em montarContinuar).
//
// VOLTA SOZINHA quando a pessoa assiste um episodio NOVO da obra: o item da
// fonte remota (ou o registro local) com instante MAIS NOVO que o carimbo vence
// a ocultacao. O "a seguir" velho tem o instante do ultimo episodio visto, que
// e anterior ao carimbo, e por isso continua fora. Sem esta regra a serie
// ficaria presa fora da fileira para sempre.
#define ARQ_OCULTO "cwoculto.txt"
#define OCULTOS_MAX 64
static struct { int perfil; char obra[24]; long long ms; } ocultos[OCULTOS_MAX];
static int nOcultos, ocultosCarregado;

static void ocultosCarregar(void) {
  char *buf, *linha, *ctx;
  if (ocultosCarregado) return;
  ocultosCarregado = 1;
  nOcultos = 0;
  buf = dados_ler(ARQ_OCULTO);
  if (!buf) return;
  for (linha = strtok_r(buf, "\n", &ctx); linha && nOcultos < OCULTOS_MAX;
       linha = strtok_r(NULL, "\n", &ctx)) {
    int perfil = 0, n = 0; long long ms = 0; char obra[24];
    if (sscanf(linha, "%d\t%23[^\t]\t%lld%n", &perfil, obra, &ms, &n) < 3) continue;
    ocultos[nOcultos].perfil = perfil;
    snprintf(ocultos[nOcultos].obra, sizeof ocultos[nOcultos].obra, "%s", obra);
    ocultos[nOcultos].ms = ms;
    nOcultos++;
  }
  free(buf);
}

static void ocultosGravar(void) {
  char *buf = (char *)malloc((size_t)nOcultos * 64 + 8), *p;
  int i;
  if (!buf) return;
  p = buf; *p = 0;
  for (i = 0; i < nOcultos; i++)
    p += sprintf(p, "%d\t%s\t%lld\n", ocultos[i].perfil, ocultos[i].obra, ocultos[i].ms);
  dados_gravar(ARQ_OCULTO, buf);
  free(buf);
}

static int ocultoIndice(int perfil, const char *obra) {
  int i;
  for (i = 0; i < nOcultos; i++)
    if (ocultos[i].perfil == perfil && !strcmp(ocultos[i].obra, obra)) return i;
  return -1;
}

void prog_ocultar_continuar(const char *imdb) {
  char obra[24];
  int i, perfil = perfis_ativo();
  long long agora = prog_agora_ms();
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return;
  TRANCAR();
  ocultosCarregar();
  i = ocultoIndice(perfil, obra);
  if (i < 0 && nOcultos < OCULTOS_MAX) i = nOcultos++;
  if (i < 0) {   // cheio: a vaga mais velha
    int k; i = 0;
    for (k = 1; k < nOcultos; k++) if (ocultos[k].ms < ocultos[i].ms) i = k;
  }
  ocultos[i].perfil = perfil;
  snprintf(ocultos[i].obra, sizeof ocultos[i].obra, "%s", obra);
  ocultos[i].ms = agora;
  ocultosGravar();
  DESTRANCAR();
}

// Chamar com a trava tomada. 1 = continua oculto.
static int ocultoVence(int perfil, const char *obra, long long instanteMs) {
  int i = ocultoIndice(perfil, obra), k;
  if (i < 0) return 0;
  if (instanteMs > ocultos[i].ms) return 0;
  carregar();
  for (k = 0; k < nRegs; k++)
    if (regs[k].perfil == perfil && !strcmp(regs[k].contentId, obra) &&
        regs[k].lastWatchedMs > ocultos[i].ms) return 0;
  return 1;
}

int prog_oculto_vence(const char *imdb, long long instanteMs) {
  char obra[24];
  int r;
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return 0;
  TRANCAR();
  ocultosCarregar();
  r = ocultoVence(perfis_ativo(), obra, instanteMs);
  DESTRANCAR();
  return r;
}

int prog_oculto_soltar(const char *imdb, long long instanteMs) {
  char obra[24];
  int i, r = 0, perfil = perfis_ativo();
  prog_content_id(obra, sizeof obra, imdb, NULL, NULL);
  if (!obra[0]) return 0;
  TRANCAR();
  ocultosCarregar();
  i = ocultoIndice(perfil, obra);
  if (i >= 0 && !ocultoVence(perfil, obra, instanteMs)) {
    ocultos[i] = ocultos[--nOcultos];
    ocultosGravar();
    r = 1;
  }
  DESTRANCAR();
  return r;
}

void prog_esquecer_tudo(void) {
  TRANCAR();
  dados_apagar(ARQ);
  dados_apagar(ARQ_OCULTO);
  carregado = 0;
  nRegs = 0;
  nOcultos = 0; ocultosCarregado = 0;
  // Logout: as remocoes eram da conta que saiu.
  nRemovidos = 0;
  DESTRANCAR();
}
