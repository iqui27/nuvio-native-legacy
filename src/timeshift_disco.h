// TIMESHIFT LOCAL DA TV AO VIVO — o ANEL EM DISCO (ver timeshift_regras.h).
//
// Um diretorio, um arquivo por segmento (ts-<id>.ts), indice so em memoria: a
// gravacao vale para a sessao do canal e some ao fechar. Um fio grava
// (tsd_marco / tsd_gravar), outros leem (tsd_ler); a trava e so do indice, a
// E/S de disco fica fora dela. Falha de escrita (disco cheio, pendrive
// arrancado) desliga a gravacao e o ao vivo segue sem ela.
//
// So funcoes static: entra em quem inclui (proxyts.c) sem mexer nas listas de
// fontes dos builds e dos testes.
#ifndef NV_TIMESHIFT_DISCO_H
#define NV_TIMESHIFT_DISCO_H
#include "timeshift_regras.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <unistd.h>

typedef struct {
  pthread_mutex_t trava;
  TsrIndice ix;
  char dir[700];
  int fd;          // segmento em escrita; -1 = nenhum
  int erro;        // errno da escrita que desligou a gravacao; 0 = gravando
} TsDisco;
typedef struct { int fd; unsigned id; } TsLeitor;   // fd = -1 ao comecar

static void tsd_nome(const TsDisco *d, unsigned id, char *out, size_t n) {
  snprintf(out, n, "%s/ts-%08u.ts", d->dir, id);
}
// Apaga os ts-*.ts de `dir` (sobra de uma sessao que morreu sem fechar).
static void tsd_limpar_dir(const char *dir) {
  DIR *p = opendir(dir);
  struct dirent *e;
  char c[1024];
  if (!p) return;
  while ((e = readdir(p)) != NULL) {
    size_t n = strlen(e->d_name);
    if (n < 7 || strncmp(e->d_name, "ts-", 3) || strcmp(e->d_name + n - 3, ".ts")) continue;
    snprintf(c, sizeof c, "%s/%s", dir, e->d_name);
    unlink(c);
  }
  closedir(p);
}
// Bytes livres para quem nao e root no volume de `dir`; 0 se nao deu para ler.
static uint64_t tsd_livre(const char *dir) {
  struct statvfs v;
  if (statvfs(dir, &v)) return 0;
  return (uint64_t)v.f_bavail * (uint64_t)v.f_frsize;
}
// Cria `dir` se faltar, limpa a sobra e mede o espaco. NULL = sem disco
// gravavel ou sem espaco (tsr_teto_bytes); `teto` > 0 troca o orcamento medido
// (testes).
static TsDisco *tsd_abrir(const char *dir, uint64_t teto) {
  TsDisco *d;
  if (!dir || !dir[0] || strlen(dir) >= sizeof d->dir - 20) return NULL;
  if (mkdir(dir, 0755) && errno != EEXIST) return NULL;
  if (access(dir, W_OK)) return NULL;
  tsd_limpar_dir(dir);
  if (!teto) teto = tsr_teto_bytes(tsd_livre(dir));
  if (!teto) return NULL;
  d = calloc(1, sizeof *d);
  if (!d) return NULL;
  if (pthread_mutex_init(&d->trava, NULL)) { free(d); return NULL; }
  tsr_iniciar(&d->ix, teto);
  snprintf(d->dir, sizeof d->dir, "%s", dir);
  d->fd = -1;
  return d;
}
static void tsd_desligar(TsDisco *d, int e) {
  if (d->fd >= 0) { close(d->fd); d->fd = -1; }
  pthread_mutex_lock(&d->trava); d->erro = e ? e : EIO; pthread_mutex_unlock(&d->trava);
}
// Grava `n` bytes (multiplo de 188, como o proxy entrega). `marco` = 1 quando
// `p` comeca num ponto bom de corte. 1 = gravado; 0 = gravacao desligada.
static int tsd_gravar(TsDisco *d, const void *p, size_t n, int marco, int64_t agoraMs) {
  const char *b = p;
  unsigned apagar[8], nApagar = 0, i;
  char nome[800];
  int novo;
  if (!d || d->erro) return 0;
  pthread_mutex_lock(&d->trava);
  novo = tsr_precisa_novo(&d->ix, marco, agoraMs);
  pthread_mutex_unlock(&d->trava);
  if (novo) {
    unsigned id, fora = 0;
    int temFora = 0, fd;
    if (d->fd >= 0) { close(d->fd); d->fd = -1; }
    // O arquivo existe ANTES de entrar no indice: leitor nunca acha um id sem arquivo.
    pthread_mutex_lock(&d->trava); id = d->ix.proxId; pthread_mutex_unlock(&d->trava);
    tsd_nome(d, id, nome, sizeof nome);
    fd = open(nome, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) { tsd_desligar(d, errno); return 0; }
    d->fd = fd;
    pthread_mutex_lock(&d->trava);
    tsr_abrir(&d->ix, agoraMs, &fora, &temFora);
    pthread_mutex_unlock(&d->trava);
    if (temFora) apagar[nApagar++] = fora;
  }
  while (n) {
    ssize_t r = write(d->fd, b, n);
    if (r < 0 && errno == EINTR) continue;
    if (r <= 0) { tsd_desligar(d, r < 0 ? errno : ENOSPC); return 0; }
    pthread_mutex_lock(&d->trava); tsr_somar(&d->ix, (uint64_t)r); pthread_mutex_unlock(&d->trava);
    b += r; n -= (size_t)r;
  }
  pthread_mutex_lock(&d->trava);
  while (nApagar < 8 && tsr_expurgar(&d->ix, &apagar[nApagar])) nApagar++;
  pthread_mutex_unlock(&d->trava);
  for (i = 0; i < nApagar; i++) { tsd_nome(d, apagar[i], nome, sizeof nome); unlink(nome); }
  return 1;
}
static int64_t tsd_janela_ms(TsDisco *d, int64_t agoraMs) {
  int64_t j;
  if (!d) return 0;
  pthread_mutex_lock(&d->trava);
  j = d->erro ? 0 : tsr_janela_ms(&d->ix, agoraMs);
  pthread_mutex_unlock(&d->trava);
  return j;
}
static uint64_t tsd_pos_atras(TsDisco *d, int64_t atrasMs, int64_t agoraMs) {
  uint64_t p;
  pthread_mutex_lock(&d->trava);
  p = tsr_pos_atras(&d->ix, atrasMs, agoraMs);
  pthread_mutex_unlock(&d->trava);
  return p;
}
// Le ate `n` bytes a partir de `*pos` (que avanca). > 0 = lidos; 0 = na ponta,
// esperar; -1 = falha de leitura. `*pulou` = 1 quando a posicao pedida ja
// tinha sido apagada e a leitura recomecou no segmento mais antigo.
static long tsd_ler(TsDisco *d, TsLeitor *l, uint64_t *pos, void *buf, size_t n, int *pulou) {
  unsigned id = 0;
  uint64_t off = 0, resto = 0;
  int achou, tentativa;
  char nome[800];
  ssize_t r;
  if (pulou) *pulou = 0;
  for (tentativa = 0; tentativa < 3; tentativa++) {
    int p = 0;
    pthread_mutex_lock(&d->trava);
    achou = tsr_localizar(&d->ix, pos, &id, &off, &resto, &p);
    pthread_mutex_unlock(&d->trava);
    if (p && pulou) *pulou = 1;
    if (!achou) return 0;
    if (l->fd < 0 || l->id != id) {
      if (l->fd >= 0) close(l->fd);
      tsd_nome(d, id, nome, sizeof nome);
      l->fd = open(nome, O_RDONLY);
      l->id = id;
      if (l->fd < 0) continue;   // apagado entre o indice e o open: localizar de novo
    }
    if (n > resto) n = (size_t)resto;
    r = pread(l->fd, buf, n, (off_t)off);
    if (r < 0 && errno == EINTR) continue;
    if (r < 0) return -1;
    if (r == 0) return 0;        // o indice andou antes do write chegar ao disco
    *pos += (uint64_t)r;
    return (long)r;
  }
  return -1;
}
static void tsd_leitor_fechar(TsLeitor *l) { if (l->fd >= 0) close(l->fd); l->fd = -1; }
// Fecha e APAGA tudo: a gravacao e so da sessao do canal.
static void tsd_fechar(TsDisco *d) {
  if (!d) return;
  if (d->fd >= 0) close(d->fd);
  tsd_limpar_dir(d->dir);
  pthread_mutex_destroy(&d->trava);
  free(d);
}
#endif
