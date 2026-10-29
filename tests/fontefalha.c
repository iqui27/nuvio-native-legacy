// Production policy, persistence and worker snapshots; no SDL or network.
#define _POSIX_C_SOURCE 200809L
#include "fontefalha.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char A[FONTEFALHA_CHAVE], B[FONTEFALHA_CHAVE], C[FONTEFALHA_CHAVE];
static FonteFalhaConfig config(int native, int addons) {
  FonteFalhaConfig c = {0}; c.provedores = native; c.addons = addons;
  for (int i = 0; i < 4; i++) { c.ordem[i] = i; c.limites[i] = 3; }
  c.nAddons = 2;
  strcpy(c.addon[0].chave, A); c.addon[0].limite = 3;
  strcpy(c.addon[1].chave, B); c.addon[1].limite = 3;
  return c;
}
static FonteFalhaCandidata candidate(int index, int native, const char *addon,
                                    int order, const char *identity) {
  FonteFalhaCandidata c = {0}; c.indice = index; c.nativa = native;
  c.ordemAddon = order; c.identidade = identity; strcpy(c.addon, addon); return c;
}
static FonteFalhaSessao *session(const FonteFalhaConfig *cfg, unsigned mask, int preferred) {
  FonteFalhaSessao *s = fontefalha_sessao_nova(cfg, mask, preferred); assert(s); return s;
}
static void attempt(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int n,
                    int complete, int service, int index) {
  FonteFalhaRota r = {0}; int before = fontefalha_tentativas(s);
  int result = fontefalha_proxima(s, c, n, complete, &r);
  if (result != FONTEFALHA_PROXIMA || r.servico != service || r.indice != index)
    fprintf(stderr, "route: result=%d service=%d index=%d; expected service=%d index=%d\n",
            result, r.servico, r.indice, service, index);
  assert(result == FONTEFALHA_PROXIMA && r.servico == service && r.indice == index);
  assert(r.candidata >= 0 && r.candidata < n && c[r.candidata].indice == index);
  assert(fontefalha_tentativas(s) == before); // Proposing does not spend a link.
  assert(fontefalha_tentar(s, &c[r.candidata], service));
  assert(fontefalha_tentativas(s) == before + 1);
}
static void status(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int n,
                   int complete, int expected) {
  FonteFalhaRota r; assert(fontefalha_proxima(s, c, n, complete, &r) == expected);
}

static void provider_first(void) {
  FonteFalhaConfig cfg = config(1, 1);
  for (int i = 0; i < 4; i++) cfg.limites[i] = 2;
  cfg.addon[0].limite = 1; cfg.addon[1].limite = 2;
  FonteFalhaCandidata c[] = {
    candidate(201, 1, B, 0, "hash-b/file-0"), candidate(101, 1, A, 1, "hash-a/file-0"),
    candidate(102, 1, A, 1, "hash-a2/file-0"), candidate(202, 1, B, 0, "hash-b2/file-0")};
  FonteFalhaSessao *s = session(&cfg, 15, -1);
  for (int service = 0; service < 4; service++) {
    attempt(s, c, 4, 1, service, 101); attempt(s, c, 4, 1, service, 201);
  }
  status(s, c, 4, 1, FONTEFALHA_FIM); assert(fontefalha_tentativas(s) == 8);
  fontefalha_sessao_liberar(s);

  // Configured order wins over arrival order; unavailable PM is skipped.
  cfg = config(1, 1); cfg.ordem[0] = 3; cfg.ordem[1] = 1; cfg.ordem[2] = 2; cfg.ordem[3] = 0;
  cfg.limites[3] = 1; cfg.limites[1] = 2; cfg.limites[0] = 1;
  s = session(&cfg, 11, -1);
  attempt(s, c, 4, 1, 3, 101); attempt(s, c, 4, 1, 1, 101);
  attempt(s, c, 4, 1, 1, 102); attempt(s, c, 4, 1, 0, 101);
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);
  cfg = config(1, 1); FonteFalhaAddon swapped = cfg.addon[0];
  cfg.addon[0] = cfg.addon[1]; cfg.addon[1] = swapped;
  FonteFalhaCandidata mixed[] = {
    candidate(301, 1, C, 5, "unknown-addon/file-0"),
    candidate(101, 1, A, 0, "hash-a/file-0"), candidate(201, 1, B, 9, "hash-b/file-0")};
  s = session(&cfg, 1, -1);
  attempt(s, mixed, 3, 1, 0, 201); attempt(s, mixed, 3, 1, 0, 101);
  attempt(s, mixed, 3, 1, 0, 301); status(s, mixed, 3, 1, FONTEFALHA_FIM);
  fontefalha_sessao_liberar(s);
  s = session(&cfg, 1, 101);
  attempt(s, mixed, 3, 1, 0, 101); attempt(s, mixed, 3, 1, 0, 201);
  fontefalha_sessao_liberar(s);
  cfg.addons = 0; // Retain saved B-first preferences, but use installation order while OFF.
  s = session(&cfg, 1, -1);
  attempt(s, mixed, 3, 1, 0, 101); attempt(s, mixed, 3, 1, 0, 301);
  attempt(s, mixed, 3, 1, 0, 201); status(s, mixed, 3, 1, FONTEFALHA_FIM);
  fontefalha_sessao_liberar(s);
  s = session(&cfg, 1, 201); // Explicit selection overrides either addon-order setting.
  attempt(s, mixed, 3, 1, 0, 201); attempt(s, mixed, 3, 1, 0, 101);
  fontefalha_sessao_liberar(s);
  puts("provider-first: RD/TB/PM/AD, configured order, availability and total caps PASS");
}

static void exhausted_and_partial(void) {
  FonteFalhaConfig cfg = config(1, 1); cfg.addon[0].limite = cfg.addon[1].limite = 1;
  FonteFalhaCandidata c[] = {
    candidate(101, 1, A, 0, "hash-a/file-0"), candidate(201, 1, B, 1, "hash-b/file-0")};
  FonteFalhaSessao *s = session(&cfg, 3, -1);
  attempt(s, c, 2, 1, 0, 101); attempt(s, c, 2, 1, 0, 201);
  attempt(s, c, 2, 1, 1, 101); attempt(s, c, 2, 1, 1, 201);
  status(s, c, 2, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  cfg = config(1, 1); cfg.limites[0] = cfg.limites[1] = 2;
  c[1] = candidate(102, 1, A, 0, "hash-a2/file-0");
  s = session(&cfg, 3, -1);
  status(s, NULL, 0, 0, FONTEFALHA_ESPERAR);
  attempt(s, c, 1, 0, 0, 101);
  status(s, c, 1, 0, FONTEFALHA_ESPERAR); // Remaining RD budget waits for late links.
  attempt(s, c, 2, 0, 0, 102);
  attempt(s, c, 2, 0, 1, 101); // Reaching the cap permits advancing a partial list.
  attempt(s, c, 2, 0, 1, 102);
  status(s, c, 2, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  s = session(&cfg, 3, -1); attempt(s, c, 2, 1, 0, 101);
  fontefalha_provedor_recusado(s, 0); attempt(s, c, 2, 1, 1, 101);
  fontefalha_sessao_liberar(s);
  // Explicit native/HTTP selections may open early; retries must wait for addon priority.
  for (int native = 0; native <= 1; native++) {
    cfg = config(1, 1); cfg.limites[0] = 2;
    FonteFalhaCandidata manual[] = {
      candidate(201, native, B, 1, "chosen-b"),
      candidate(202, 1, B, 1, "arrived-b/file-0"),
      candidate(101, 1, A, 0, "late-priority-a/file-0")};
    s = session(&cfg, 1, 201);
    attempt(s, manual, 2, 0, native ? 0 : -1, 201);
    status(s, manual, 2, 0, FONTEFALHA_ESPERAR);
    assert(fontefalha_tentativas(s) == 1); // Eligible B cannot spend the budget before A arrives.
    attempt(s, manual, 3, 1, 0, 101);
    fontefalha_sessao_liberar(s);
  }
  puts("exhaustion: addon budgets reset per provider; partial lists wait; refused providers skip PASS");
}

static void first_counts_and_next_links(void) {
  FonteFalhaCandidata c[11]; char ids[11][30];
  for (int i = 0; i < 11; i++) {
    snprintf(ids[i], sizeof ids[i], "hash-%d/file-0", i); c[i] = candidate(i, 1, A, 0, ids[i]);
  }
  for (int limit = 1; limit <= 10; limit++) {
    // Addon limits stay at their default 3 and do not restrict native-only mode.
    FonteFalhaConfig cfg = config(1, 0); cfg.limites[0] = limit;
    FonteFalhaSessao *s = session(&cfg, 1, -1);
    for (int i = 0; i < limit; i++) attempt(s, c, 11, 1, 0, i);
    status(s, c, 11, 1, FONTEFALHA_FIM); assert(fontefalha_tentativas(s) == limit);
    fontefalha_sessao_liberar(s);
    cfg.addons = 1; cfg.limites[0] = 10; cfg.addon[0].limite = limit;
    s = session(&cfg, 1, -1);
    for (int i = 0; i < limit; i++) attempt(s, c, 11, 1, 0, i);
    status(s, c, 11, 1, FONTEFALHA_FIM); assert(fontefalha_tentativas(s) == limit);
    fontefalha_sessao_liberar(s);
  }
  puts("link budgets: 1..10 include the first link and every attempt chooses the next link PASS");
}

static void duplicate_routes_and_urls(void) {
  FonteFalhaConfig cfg = config(1, 1); cfg.limites[0] = cfg.limites[1] = 2;
  cfg.addon[0].limite = cfg.addon[1].limite = 1;
  FonteFalhaCandidata c[] = {
    candidate(101, 1, A, 0, "same-hash/file-0"), candidate(201, 1, B, 1, "same-hash/file-0"),
    candidate(202, 1, B, 1, "next-hash/file-0")};
  FonteFalhaSessao *s = session(&cfg, 3, -1);
  attempt(s, c, 3, 1, 0, 101); assert(!fontefalha_tentar(s, &c[1], 0));
  assert(fontefalha_tentativas(s) == 1); attempt(s, c, 3, 1, 0, 202);
  attempt(s, c, 3, 1, 1, 101); attempt(s, c, 3, 1, 1, 202);
  status(s, c, 3, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  // Refund the latest provider AND addon, not the earlier successful reservation.
  FonteFalhaCandidata native[] = {
    candidate(101, 1, A, 0, "hash-a/file-0"), candidate(201, 1, B, 1, "hash-b/file-0"),
    candidate(102, 1, A, 0, "hash-a2/file-0"), candidate(202, 1, B, 1, "hash-b2/file-0")};
  s = session(&cfg, 1, -1);
  attempt(s, native, 4, 1, 0, 101); assert(fontefalha_url_registrar(s, "https://video.invalid/shared"));
  assert(fontefalha_url_registrar(s, "https://video.invalid/shared")); // Same pair is idempotent.
  assert(fontefalha_tentativas(s) == 1);
  attempt(s, native, 4, 1, 0, 201); assert(!fontefalha_url_registrar(s, "https://video.invalid/shared"));
  assert(fontefalha_tentativas(s) == 1);
  attempt(s, native, 4, 1, 0, 202); assert(fontefalha_url_registrar(s, "https://video.invalid/other"));
  status(s, native, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  cfg = config(0, 1); cfg.addon[0].limite = cfg.addon[1].limite = 1;
  c[0] = candidate(301, 0, A, 0, "url-a/headers-a");
  c[1] = candidate(401, 0, B, 1, "url-b/headers-b");
  c[2] = candidate(402, 0, B, 1, "url-b2/headers-b2");
  s = session(&cfg, 0, -1);
  attempt(s, c, 3, 1, -1, 301); assert(fontefalha_url_registrar(s, "https://video.invalid/same"));
  attempt(s, c, 3, 1, -1, 401); assert(!fontefalha_url_registrar(s, "https://video.invalid/same"));
  assert(fontefalha_tentativas(s) == 1);
  assert(!fontefalha_url_registrar(s, "https://video.invalid/same")); // Refund at most once.
  assert(fontefalha_tentativas(s) == 1 && !fontefalha_tentar(s, &c[1], -1));
  attempt(s, c, 3, 1, -1, 402); assert(fontefalha_url_registrar(s, "https://video.invalid/next"));
  status(s, c, 3, 1, FONTEFALHA_FIM); assert(fontefalha_tentativas(s) == 2);
  fontefalha_sessao_liberar(s);
  puts("deduplication: native identity/service pairs across addons; final URL refunds exact last budget once PASS");
}

static void direct_and_legacy(void) {
  FonteFalhaConfig cfg = config(0, 0);
  FonteFalhaCandidata c[] = {
    candidate(101, 1, A, 0, "hash-a/file-0"), candidate(301, 0, A, 0, "url-a"),
    candidate(302, 0, A, 0, "url-a2"), candidate(401, 0, B, 1, "url-b")};
  FonteFalhaSessao *s = session(&cfg, 0, 301);
  attempt(s, c, 4, 1, -1, 301); status(s, c, 4, 0, FONTEFALHA_FIM);
  fontefalha_sessao_liberar(s);
  cfg.addons = 1; cfg.addon[0].limite = 2; cfg.addon[1].limite = 1;
  s = session(&cfg, 0, -1);
  attempt(s, c, 4, 1, -1, 101); // Native OFF sends hashes through the legacy resolver.
  fontefalha_marcar_aberta(s);
  attempt(s, c, 4, 1, -1, 301); attempt(s, c, 4, 1, -1, 401);
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  cfg = config(1, 0); cfg.limites[0] = 1;
  s = session(&cfg, 1, -1);
  attempt(s, c, 4, 1, 0, 101); fontefalha_marcar_aberta(s);
  status(s, c, 4, 1, FONTEFALHA_FIM); // Addon OFF forbids direct retries after any opened link.
  fontefalha_sessao_liberar(s);
  s = session(&cfg, 1, -1);
  attempt(s, c, 4, 1, 0, 101); attempt(s, c, 4, 1, -1, 301); // Never opened: initial HTTP still works.
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);
  s = session(&cfg, 1, 301);
  attempt(s, c, 4, 1, -1, 301); // Explicit HTTP selection is honored before native groups.
  fontefalha_marcar_aberta(s); attempt(s, c, 4, 1, 0, 101);
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);
  puts("direct/legacy: initial HTTP, explicit choice, addon-enabled retries and opened-link guard PASS");
}

static void manual_success(void) {
  FonteFalhaConfig cfg = config(1, 0); cfg.limites[0] = 2;
  FonteFalhaCandidata c[] = {
    candidate(101, 1, A, 0, "seed-hash/file-0"),
    candidate(201, 1, B, 1, "seed-hash/file-0"),
    candidate(102, 1, A, 0, "seed-hash/file-1"),
    candidate(301, 0, A, 0, "initial-direct")};
  FonteFalhaSessao *s = session(&cfg, 1, 101);
  assert(fontefalha_semear(s, &c[0], 0, "https://video.invalid/seed"));
  assert(fontefalha_tentativas(s) == 1);
  assert(!fontefalha_semear(s, &c[1], 0, "https://video.invalid/seed"));
  assert(fontefalha_tentativas(s) == 1);
  assert(fontefalha_url_registrar(s, "https://video.invalid/seed"));
  // The original file is skipped across addons; another file in the hash is distinct.
  attempt(s, c, 4, 1, 0, 102); assert(fontefalha_url_registrar(s, "https://video.invalid/file-1"));
  status(s, c, 4, 1, FONTEFALHA_FIM); // Seeded playback was opened, so addon OFF blocks HTTP retries.
  assert(fontefalha_tentativas(s) == 2); fontefalha_sessao_liberar(s);

  cfg = config(1, 0);
  for (int i = 0; i < 4; i++) cfg.limites[i] = 1;
  s = session(&cfg, 15, 101);
  assert(fontefalha_semear(s, &c[0], 1, "https://video.invalid/seed"));
  // Resume at the actual winning provider TB, rather than restarting RD.
  attempt(s, c, 4, 1, 2, 101);
  assert(!fontefalha_url_registrar(s, "https://video.invalid/seed"));
  assert(fontefalha_tentativas(s) == 1);
  attempt(s, c, 4, 1, 2, 102); assert(fontefalha_url_registrar(s, "https://video.invalid/pm"));
  attempt(s, c, 4, 1, 3, 101); assert(fontefalha_url_registrar(s, "https://video.invalid/ad"));
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);

  cfg = config(1, 1); cfg.limites[0] = cfg.addon[0].limite = 1;
  s = session(&cfg, 1, 101);
  assert(fontefalha_semear(s, &c[0], 0, "https://video.invalid/seed"));
  attempt(s, c, 4, 1, -1, 301); // Addon ON permits a direct fallback after seeded playback fails.
  status(s, c, 4, 1, FONTEFALHA_FIM); fontefalha_sessao_liberar(s);
  puts("manual success: first winner counts, file identity, provider continuation and opened-link policy PASS");
}

static void snapshot_and_cancel(void) {
  FonteFalhaConfig cfg = config(1, 1); cfg.limites[0] = 2;
  char identity[40] = "owned-hash/file-0";
  FonteFalhaCandidata c[] = {
    candidate(101, 1, A, 0, identity), candidate(102, 1, A, 0, "second-hash/file-0")};
  FonteFalhaSessao *s = session(&cfg, 1, -1); int ceiling = fontefalha_teto(s);
  cfg.limites[0] = 1; cfg.addon[0].limite = 1; cfg.addons = 0;
  assert(fontefalha_teto(s) == ceiling); // Caller changes cannot rewrite the playback snapshot.
  attempt(s, c, 2, 1, 0, 101); assert(fontefalha_url_registrar(s, "https://video.invalid/first"));
  strcpy(identity, "caller-reused-buffer");
  c[0].identidade = "owned-hash/file-0"; assert(!fontefalha_tentar(s, &c[0], 0));
  FonteFalhaSessao *worker = fontefalha_sessao_clonar(s); assert(worker);
  attempt(worker, c, 2, 1, 0, 102); assert(!fontefalha_url_registrar(worker, "https://video.invalid/first"));
  fontefalha_provedor_recusado(worker, 0); fontefalha_marcar_aberta(worker);
  fontefalha_sessao_liberar(worker); // Canceled worker work is discarded, not committed to the owner.
  assert(fontefalha_tentativas(s) == 1 && fontefalha_teto(s) == ceiling);
  attempt(s, c, 2, 1, 0, 102); assert(fontefalha_url_registrar(s, "https://video.invalid/second"));
  assert(fontefalha_tentativas(s) == 2); fontefalha_sessao_liberar(s);
  puts("sessions: owned identities, immutable config and canceled worker clone isolation PASS");
}

static void persistence(const char *dir) {
  FonteFalhaConfig before, after; char path[1200], backup[1200];
  fontefalha_dir(dir); fontefalha_config(&before);
  assert(!fontefalha_ativa() && !before.provedores && !before.addons && !before.nAddons);
  for (int i = 0; i < 4; i++) assert(before.ordem[i] == i && before.limites[i] == 3);
  const char *keys[] = {A, B, C}; fontefalha_addons_registrar(keys, 2);
  assert(fontefalha_addon_limite(A) == 3 && fontefalha_addon_limite(B) == 3);
  assert(!fontefalha_addon_ao_vivo(A) && !fontefalha_addon_ao_vivo(B));
  assert(fontefalha_addon_definir_ao_vivo(A, 1));
  assert(fontefalha_addon_ao_vivo(A) && !fontefalha_addon_ao_vivo(B));
  assert(!fontefalha_ativa()); // Classification never enables failover.
  fontefalha_config_ler();
  assert(fontefalha_addon_ao_vivo(A) && !fontefalha_addon_ao_vivo(B));
  assert(!fontefalha_addon_definir_ao_vivo("invalid", 1));
  assert(fontefalha_definir_provedores(1));
  assert(fontefalha_ativa() && fontefalha_provedores() && !fontefalha_addons());
  assert(fontefalha_definir_provedores(0) && fontefalha_definir_addons(1));
  assert(fontefalha_ativa() && !fontefalha_provedores() && fontefalha_addons());
  assert(fontefalha_definir_provedores(1));
  assert(fontefalha_provedor_definir_limite(0, 1) && fontefalha_provedor_definir_limite(3, 10));
  assert(fontefalha_provedor_mover(3, -1) && fontefalha_provedor_mover(3, -1));
  assert(fontefalha_addon_definir_limite(B, 1) && fontefalha_addon_mover(B, -1));
  fontefalha_addons_registrar(keys, 3); assert(fontefalha_provedor_definir_limite(1, 2));
  assert(fontefalha_addon_posicao(B) == 0 && fontefalha_addon_posicao(A) == 1 && fontefalha_addon_posicao(C) == 2);
  fontefalha_addons_registrar(keys, 1); // Uninstalled addons retain their preferences.
  fontefalha_config(&before); assert(before.nAddons == 3);
  assert(!fontefalha_provedor_definir_limite(0, 0) && !fontefalha_provedor_definir_limite(0, 11));
  assert(!fontefalha_addon_definir_limite(A, 0) && !fontefalha_addon_definir_limite(A, 11));
  fontefalha_config(&after); assert(!memcmp(&before, &after, sizeof before));
  fontefalha_config_ler(); fontefalha_config(&after); assert(!memcmp(&before, &after, sizeof before));
  snprintf(path, sizeof path, "%s/fontefalha.txt", dir);
  FILE *f = fopen(path, "r"); assert(f); char body[20000]; size_t n = fread(body, 1, sizeof body - 1, f);
  body[n] = 0; fclose(f); assert(!strstr(body, "https://") && !strstr(body, "secret"));
  // The temporary write succeeds, then rename fails: no configuration is published.
  snprintf(backup, sizeof backup, "%s/saved-config.txt", dir);
  assert(!rename(path, backup) && !mkdir(path, 0700));
  assert(!fontefalha_definir_addons(0) && !fontefalha_provedor_definir_limite(1, 4));
  assert(!fontefalha_addon_mover(A, -1));
  assert(!fontefalha_addon_definir_ao_vivo(A, 0) && fontefalha_addon_ao_vivo(A));
  fontefalha_config(&after); assert(!memcmp(&before, &after, sizeof before));
  DIR *d = opendir(dir); assert(d); struct dirent *entry;
  while ((entry = readdir(d))) assert(!strstr(entry->d_name, ".tmp."));
  closedir(d); assert(!rmdir(path) && !rename(backup, path));
  fontefalha_config_ler(); fontefalha_config(&after); assert(!memcmp(&before, &after, sizeof before));
  f = fopen(path, "w"); assert(f);
  fputs("provedores invalid\naddons 7\nordem 0 0 2 3\nlimites 0 3 3 3\n", f); fclose(f);
  fontefalha_config_ler(); fontefalha_config(&after);
  assert(!after.provedores && !after.addons);
  for (int i = 0; i < 4; i++) assert(after.ordem[i] == i && after.limites[i] == 3);
  puts("settings: OFF/3 defaults, stable addon instances, save/reload, validation and failed-save rollback PASS");
}

int main(int argc, char **argv) {
  assert(argc == 2);
  fontefalha_addon_chave("https://aio.invalid/same-name/secret-config-a/manifest.json", A, sizeof A);
  fontefalha_addon_chave("https://aio.invalid/same-name/secret-config-b/manifest.json", B, sizeof B);
  fontefalha_addon_chave("https://another.invalid/manifest.json", C, sizeof C);
  char repeated[FONTEFALHA_CHAVE];
  fontefalha_addon_chave("https://aio.invalid/same-name/secret-config-a/manifest.json", repeated, sizeof repeated);
  assert(strlen(A) == 32 && !strcmp(A, repeated) && strcmp(A, B) && strcmp(A, C));
  persistence(argv[1]); provider_first(); exhausted_and_partial(); first_counts_and_next_links();
  duplicate_routes_and_urls(); direct_and_legacy(); manual_success(); snapshot_and_cancel();
  puts("fontefalha: all policy and persistence regressions PASS"); return 0;
}
