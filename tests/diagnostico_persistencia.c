/* Confirmation of the measured image profile without a window/network.
 * Cache/storage doubles model accepted, capped, locked and failed writes;
 * production diagnostic control flow and platform policy remain real. */
#include "../src/diagnostico.c"
#include <assert.h>
#include <pthread.h>

static PtvPerfil live;
static long ram;
static int fixedBudget, cacheCap, autoBudget;
static int failCheckpoint, failProfile, failDelete, writeCancel, rollbackWon;
static int cancelDuringRead, manualOnCandidate, checkpointCancel;
static char saved[256], checkpoint[256];
static void *cancelWorker(void *unused);

void tex_orcamento_info(int *mb, long *mem, int *fixed, int *slots) {
  if (mb) *mb = live.texMb;
  if (mem) *mem = ram;
  if (fixed) *fixed = fixedBudget;
  if (slots) *slots = 100;
  if (cancelDuringRead && atomic_load(&d.experimento) == 2) {
    pthread_t worker;
    assert(!pthread_create(&worker, NULL, cancelWorker, NULL));
    assert(!pthread_join(worker, NULL));
  }
}
void tex_definir_orcamento_auto_mb(int mb) {
  if (fixedBudget == 1 || fixedBudget == 2) return;
  autoBudget = mb > cacheCap ? cacheCap : mb;
  if (!fixedBudget) live.texMb = autoBudget;
  if (manualOnCandidate && mb == 192) { live.texMb = 128; fixedBudget = 3; }
}
int tex_orcamento_auto_mb(void) { return autoBudget; }
void tex_definir_orcamento_mb(int mb) {
  if (mb > 0) { live.texMb = mb > cacheCap ? cacheCap : mb; fixedBudget = 3; }
  else { live.texMb = autoBudget; if (fixedBudget == 3) fixedBudget = 0; }
}
int tex_fios_rede(void) { return live.fiosRede; }
void tex_definir_fios_rede(int n) { live.fiosRede = n; }
int tex_teto_heroi_perfil(void) { return live.heroiLarg; }
void tex_definir_teto_heroi(int n) { live.heroiLarg = n; }
static void *cancelWorker(void *unused) {
  (void)unused;
  rollbackWon = cancelarPorPrazo();
  return NULL;
}
int dados_gravar(const char *name, const char *content) {
  char *target;
  if (!strcmp(name, "diagnostico-otimizacao.checkpoint")) {
    if (checkpointCancel) {
      pthread_t worker;
      assert(atomic_load(&d.experimento) == 2);
      assert(!pthread_create(&worker, NULL, cancelWorker, NULL));
      assert(!pthread_join(worker, NULL));
      assert(!rollbackWon && !atomic_load(&d.cancelado));
    }
    if (failCheckpoint) return 0;
    target = checkpoint;
  } else {
    assert(!strcmp(name, "diagnostico-otimizacao.cfg"));
    if (writeCancel) {
      // Deterministic cross-thread interleaving while the storage write is
      // in progress. Timeout must not restore the active cache mid-commit.
      pthread_t worker;
      assert(atomic_load(&d.experimento) == 2);
      assert(!pthread_create(&worker, NULL, cancelWorker, NULL));
      assert(!pthread_join(worker, NULL));
      assert(!rollbackWon && atomic_load(&d.experimento) == 2);
      assert(!atomic_load(&d.cancelado) && !atomic_load(&d.passesProntos));
    }
    if (failProfile) return 0;
    target = saved;
  }
  snprintf(target, 256, "%s", content);
  return 1;
}
char *dados_ler(const char *name) {
  const char *v = !strcmp(name, "diagnostico-otimizacao.cfg") ? saved : checkpoint;
  return *v ? strdup(v) : NULL;
}
int dados_apagar(const char *name) {
  if (failDelete && !strcmp(name, "diagnostico-otimizacao.cfg")) return 0;
  char *v = !strcmp(name, "diagnostico-otimizacao.cfg") ? saved : checkpoint;
  *v = 0; return 1;
}

static void reset(void) {
  free(d.cfgAntes);
  memset(&d, 0, sizeof d);
  d.modo = DIAG_QUALIDADE;
  ram = 2245;
  ptv_padrao(ptv_plataforma(), ram, &live);
  autoBudget = live.texMb;
  fixedBudget = failCheckpoint = failProfile = failDelete = writeCancel = rollbackWon = 0;
  cancelDuringRead = manualOnCandidate = checkpointCancel = 0;
  cacheCap = ptv_tex_teto_mb(ptv_plataforma(), ram);
  saved[0] = checkpoint[0] = 0;
}
static void sample(int before, int after) {
  d.medAntes = (PtvMedida){ .artesMs = before, .prontas = 10, .piorQuadroMs = 20 };
  d.medDepois = (PtvMedida){ .artesMs = after, .prontas = 10, .piorQuadroMs = 20 };
}
static PtvPerfil approved(void) {
  PtvPerfil pf = {0};
  assert(ptv_ler(saved, &pf));
  return pf;
}
static void actualMatchesResult(void) {
  PtvPerfil before, now;
  perfilDoResultado(&before, &now);
  assert(!memcmp(&now, &live, sizeof now));
}

int main(void) {
  PtvPerfil pf;
  reset();
  assert(live.texMb == 128 && cacheCap == 192);
  assert(aplicarCandidato() && live.texMb == 192);
  sample(1000, 950);
  concluirComparacao();
  assert(d.aplicacao == DA_RUIDO && live.texMb == 128 && !saved[0]);
  assert(d.perfCand.texMb == 192);
  actualMatchesResult();
  puts("ok  Android 128 -> 300 without gain restores actual 128");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  failProfile = 1;
  concluirComparacao();
  assert(!strcmp(aplicacaoNome(d.aplicacao), "sem_perfil"));
  assert(live.texMb == 128 && !saved[0] && !checkpoint[0]);
  assert(!atomic_load(&d.experimento));
  actualMatchesResult();
  puts("ok  refused profile write restores previous cache and reports failure");

  reset();
  snprintf(saved, sizeof saved, "versao=2\nmodo=qualidade\ntex_mb=128\nfios_rede=4\nheroi=1920\n");
  assert(aplicarCandidato());
  sample(1000, 800);
  failProfile = 1;
  concluirComparacao();
  pf = approved();
  assert(pf.texMb == 128 && live.texMb == 128);
  diagnostico_recuperar_checkpoint();
  assert(live.texMb == 128);
  puts("ok  failed replacement preserves approved profile across restart");

  reset();
  cacheCap = 160; /* runtime cache rejects part of requested budget */
  assert(aplicarCandidato() && d.perfCand.texMb == 192 && live.texMb == 160);
  sample(1000, 800);
  concluirComparacao();
  assert(d.aplicacao == DA_MANTIDO);
  pf = approved();
  assert(pf.texMb == 160 && pf.texMb == live.texMb);
  actualMatchesResult();
  puts("ok  capped candidate persists applied 160, never requested 300");

  reset();
  cacheCap = 128; /* every requested change was rejected */
  assert(!aplicarCandidato());
  assert(!strcmp(aplicacaoNome(d.aplicacao), "limitada"));
  assert(live.texMb == 128 && !saved[0] && !checkpoint[0]);
  actualMatchesResult();
  puts("ok  unapplied candidate is not measured or approved");

  reset();
  manualOnCandidate = 1;
  assert(!aplicarCandidato() && d.aplicacao == DA_LIMITADO);
  assert(fixedBudget == 3 && live.texMb == 128 && autoBudget == 128);
  tex_definir_orcamento_mb(0);
  assert(live.texMb == 128 && !saved[0]);
  puts("ok  limited candidate also restores hidden automatic budget");

  reset();
  assert(aplicarCandidato() && live.texMb == 192);
  sample(1000, 800);
  fixedBudget = 3; live.texMb = 128; /* user chose 128 during retest */
  concluirComparacao();
  assert(!strcmp(aplicacaoNome(d.aplicacao), "alterada_durante_teste"));
  assert(live.texMb == 128 && fixedBudget == 3 && !saved[0] && !checkpoint[0]);
  assert(autoBudget == 128);
  tex_definir_orcamento_mb(0);
  assert(live.texMb == 128); /* must never resurrect unapproved 300 */
  actualMatchesResult();
  puts("ok  later manual choice survives and is not approved using stale sample");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  tex_definir_orcamento_auto_mb(160); /* later automatic profile override */
  concluirComparacao();
  assert(d.aplicacao == DA_ALTERADO && autoBudget == 160 && live.texMb == 160);
  assert(!saved[0] && !checkpoint[0]);
  puts("ok  later automatic choice is not overwritten by stale rollback");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  concluirComparacao();
  assert(d.aplicacao == DA_MANTIDO && live.texMb == 192);
  pf = approved(); assert(pf.texMb == 192);
  live.texMb = autoBudget = 128;
  diagnostico_recuperar_checkpoint();
  assert(live.texMb == 192);
  fixedBudget = 3; live.texMb = autoBudget = 128;
  diagnostico_recuperar_checkpoint();
  assert(live.texMb == 128 && autoBudget == 192);
  tex_definir_orcamento_mb(0);
  assert(live.texMb == 192);
  puts("ok  approved 300 survives restart; manual 128 continues to override it");

  reset();
  tex_definir_orcamento_mb(160);
  d.modo = DIAG_DESEMPENHO;
  assert(aplicarCandidato());
  sample(1000, 800);
  concluirComparacao();
  pf = approved();
  assert(d.aplicacao == DA_MANTIDO && live.texMb == 160 && pf.texMb == 128);
  tex_definir_orcamento_mb(0);
  assert(live.texMb == 128);
  puts("ok  approving workers under manual budget preserves automatic budget");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  writeCancel = 1;
  concluirComparacao();
  pf = approved();
  assert(d.aplicacao == DA_MANTIDO && live.texMb == 192 && pf.texMb == 192);
  assert(!atomic_load(&d.experimento) && !checkpoint[0]);
  assert(!atomic_load(&d.cancelado) && atomic_load(&d.passesProntos));
  assert(botaoVisivel(B_RESTAURAR));
  assert(!cancelarPorPrazo() && !atomic_load(&d.cancelado));
  puts("ok  timeout losing commit preserves result, active cache and Restore action");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  writeCancel = failProfile = 1;
  concluirComparacao();
  assert(d.aplicacao == DA_SEM_PERFIL && live.texMb == 128 && autoBudget == 128);
  assert(!atomic_load(&d.experimento) && !saved[0] && !checkpoint[0]);
  assert(!atomic_load(&d.cancelado) && atomic_load(&d.passesProntos));
  puts("ok  timeout during failed commit leaves restoration to commit owner");

  reset();
  cancelWorker(NULL); /* deadline expired while no candidate was active */
  assert(rollbackWon && atomic_load(&d.cancelado));
  assert(!aplicarCandidato() && d.aplicacao == DA_CANCELADO);
  assert(!atomic_load(&d.experimento) && !saved[0] && !checkpoint[0]);
  assert(live.texMb == 128 && autoBudget == 128);
  puts("ok  timeout before preparation prevents a late candidate from appearing");

  reset();
  checkpointCancel = 1;
  assert(aplicarCandidato() && live.texMb == 192);
  assert(!rollbackWon && !atomic_load(&d.cancelado));
  cancelWorker(NULL); /* timeout retries after preparation releases its owner */
  assert(rollbackWon && d.aplicacao == DA_CANCELADO && atomic_load(&d.cancelado));
  assert(!atomic_load(&d.experimento) && !saved[0] && !checkpoint[0]);
  assert(live.texMb == 128 && autoBudget == 128);
  puts("ok  timeout respects preparation then restores the still-unapproved test");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  cancelWorker(NULL);
  assert(rollbackWon && !atomic_load(&d.experimento) && live.texMb == 128);
  concluirComparacao();
  assert(!saved[0] && live.texMb == 128);
  puts("ok  cancellation before commit wins and no candidate is saved");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  cancelDuringRead = 1;
  concluirComparacao();
  assert(!rollbackWon && d.aplicacao == DA_MANTIDO);
  assert(!atomic_load(&d.experimento) && saved[0] && !checkpoint[0]);
  assert(live.texMb == 192 && autoBudget == 192);
  assert(!atomic_load(&d.cancelado) && atomic_load(&d.passesProntos));
  puts("ok  commit ownership also protects the profile read from timeout");

  reset();
  snprintf(saved, sizeof saved, "versao=2\nmodo=qualidade\ntex_mb=128\nfios_rede=4\nheroi=1920\n");
  assert(aplicarCandidato());
  sample(1000, 800);
  concluirComparacao();
  failProfile = 1;
  restaurarManual();
  pf = approved();
  assert(!strcmp(aplicacaoNome(d.aplicacao), "restaurada_sessao"));
  assert(live.texMb == 128 && autoBudget == 128 && pf.texMb == 192);
  assert(botaoVisivel(B_RESTAURAR));
  failProfile = 0;
  restaurarManual();
  pf = approved();
  assert(d.aplicacao == DA_RESTAURADO_MANUAL && pf.texMb == 128);
  puts("ok  failed manual restore write reports session-only and can retry");

  reset();
  assert(aplicarCandidato());
  sample(1000, 800);
  concluirComparacao();
  failDelete = 1;
  restaurarManual();
  pf = approved();
  assert(d.aplicacao == DA_RESTAURADO_SESSAO && live.texMb == 128 && pf.texMb == 192);
  failDelete = 0;
  restaurarManual();
  assert(d.aplicacao == DA_RESTAURADO_MANUAL && !saved[0]);
  puts("ok  failed manual restore deletion reports session-only and can retry");

  reset();
  failCheckpoint = 1;
  assert(!aplicarCandidato() && d.aplicacao == DA_SEM_CHECKPOINT);
  assert(live.texMb == 128 && !saved[0] && !checkpoint[0]);
  free(d.cfgAntes); d.cfgAntes = NULL;
  puts("ok  failed checkpoint changes no image setting");
  return 0;
}
