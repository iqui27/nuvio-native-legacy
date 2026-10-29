/* Real registry + SFNT/FreeType validation. The Android picker and account
 * epoch are typed external boundaries; no document provider or SDL window. */
#define NV_PERFIL_SESSAO_FORTE 1
#include "legfontes.h"
#include "android.h"
#include "perfis.h"
#include <SDL2/SDL_ttf.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static uint64_t epoch = 1;
uint64_t perfis_sessao_geracao(void) { return epoch; }
int perfis_sessao_atual(uint64_t e) { return e == epoch; }
int perfis_sessao_travar(uint64_t e) { return e == epoch; }
void perfis_sessao_destravar(void) {}
static char event[80]; static int requests;
int android_importar_fonte(int slot) { assert(slot >= 0 && slot < LEGFONTES_SLOTS); requests++; return 1; }
int android_fonte_evento(char *out, size_t n) {
  if (!event[0]) return 0;
  snprintf(out, n, "%s", event); event[0] = 0; return 1;
}
static int checks;
#define CHECK(x) do { assert(x); checks++; } while (0)
static int commits, commitResult;
static int commit(void *u) { assert(u == &commitResult); commits++; return commitResult; }
static void put32(unsigned char *p, uint32_t n) {
  p[0] = n >> 24; p[1] = n >> 16; p[2] = n >> 8; p[3] = n;
}
static void path(char *out, size_t n, const char *root, const char *file) {
  snprintf(out, n, "%s/fontes-importadas/%s", root, file);
}
static void stage(const char *root, const char *name, const void *data, size_t n) {
  char p[1024]; path(p, sizeof p, root, name);
  FILE *f = fopen(p, "wb"); assert(f);
  assert(fwrite(data, 1, n, f) == n); assert(!fclose(f));
}
static unsigned char *font(const char *file, size_t *n) {
  FILE *f = fopen(file, "rb"); assert(f);
  assert(!fseek(f, 0, SEEK_END)); long z = ftell(f); assert(z > 0);
  rewind(f); unsigned char *b = malloc((size_t)z); assert(b);
  assert(fread(b, 1, (size_t)z, f) == (size_t)z); fclose(f); *n = (size_t)z; return b;
}
static int failRemoval, failBackup;
/* Android's app SELinux policy forbids hard links, including inside the
 * app's own data directory. Keep that restriction in desktop coverage. */
int checkedLink(const char *from, const char *to) {
  (void)from; (void)to; errno = EACCES; return -1;
}
static FILE *checkedFopen(const char *file, const char *mode) {
  size_t n = strlen(file);
  if (failBackup && n >= 9 && !strcmp(file + n - 9, ".anterior") && !strcmp(mode, "wb")) {
    errno = ENOSPC; return NULL;
  }
  return fopen(file, mode);
}
static int checkedUnlink(const char *file) {
  size_t n = strlen(file);
  if (failRemoval && n >= 8 && !strcmp(file + n - 8, ".excluir")) { errno = EACCES; return -1; }
  return unlink(file);
}
/* Inject only a filesystem failure at the owned delete boundary. The whole
 * registry, rollback and preference file implementation remain production. */
#define unlink checkedUnlink
#define link checkedLink
#define fopen checkedFopen
#include "../src/legfontes.c"
#undef fopen
#undef link
#undef unlink
int main(int argc, char **argv) {
  assert(argc == 2); CHECK(TTF_Init() == 0);
  unsigned char sfnt[64] = {0,1,0,0,0,1};
  memcpy(sfnt + 12, "head", 4); put32(sfnt + 20, 28); put32(sfnt + 24, 36);
  CHECK(legfontes_sfnt_valido(sfnt, sizeof sfnt));
  CHECK(!legfontes_sfnt_valido(NULL, 64));
  CHECK(!legfontes_sfnt_valido(sfnt, 11));
  CHECK(!legfontes_sfnt_valido(sfnt, LEGFONTE_MAX_BYTES + 1));
  sfnt[0] = 1; CHECK(!legfontes_sfnt_valido(sfnt, sizeof sfnt)); sfnt[0] = 0;
  put32(sfnt + 20, UINT32_MAX); CHECK(!legfontes_sfnt_valido(sfnt, sizeof sfnt));
  put32(sfnt + 20, 28); put32(sfnt + 24, UINT32_MAX); CHECK(!legfontes_sfnt_valido(sfnt, sizeof sfnt));
  put32(sfnt + 24, 36); sfnt[5] = 2;
  memcpy(sfnt + 28, sfnt + 12, 16); CHECK(!legfontes_sfnt_valido(sfnt, sizeof sfnt));
  sfnt[5] = 1;
  unsigned char named[96] = {0,1,0,0,0,1}; char faceName[96];
  memcpy(named + 12, "name", 4); put32(named + 20, 28); put32(named + 24, 68);
  unsigned char *names = named + 28;
  names[3] = 1; names[5] = 18; /* one record, string pool after it */
  names[7] = 3; names[9] = 1; names[10] = 4; names[11] = 9;
  names[13] = 4; names[15] = 8;
  memcpy(names + 18, "\0T\0e\0s\0t", 8);
  CHECK(legfontes_nome_sfnt(named, sizeof named, faceName, sizeof faceName));
  CHECK(!strcmp(faceName, "Test"));
  CHECK(!legfontes_nome_sfnt(named, sizeof named, faceName, 4));
  names[18] = 0xd8; names[19] = 0; /* unpaired surrogate */
  CHECK(!legfontes_nome_sfnt(named, sizeof named, faceName, sizeof faceName));
  names[18] = 0; names[19] = 'T';
  memcpy(named, "OTTO", 4); names[13] = 6;
  CHECK(legfontes_nome_sfnt(named, sizeof named, faceName, sizeof faceName));
  CHECK(!strcmp(faceName, "Test"));
  names[17] = 90; CHECK(!legfontes_nome_sfnt(named, sizeof named, faceName, sizeof faceName));
  char root[] = "/tmp/nuvio-fonts-XXXXXX", other[] = "/tmp/nuvio-fonts-other-XXXXXX";
  CHECK(mkdtemp(root) != NULL); CHECK(mkdtemp(other) != NULL);
  legfontes_dir(root); CHECK(legfontes_n() == 0);
  CHECK(legfontes_escolher(0, -1)); CHECK(legfontes_caminho(0) == NULL);
  CHECK(!legfontes_importar(0, "../font.ttf"));
  stage(root, "entrada-1.tmp", sfnt, sizeof sfnt);
  CHECK(!legfontes_importar(0, "entrada-1.tmp")); /* SFNT header alone is not a font. */
  char p[1024]; path(p, sizeof p, root, "entrada-1.tmp"); CHECK(access(p, F_OK) != 0);
  size_t n; unsigned char *b = font(argv[1], &n);
  CHECK(legfontes_nome_sfnt(b, n, faceName, sizeof faceName));
  stage(root, "entrada-2.tmp", b, n);
  CHECK(legfontes_importar(0, "entrada-2.tmp")); CHECK(legfontes_n() == 1);
  CHECK(legfontes_selecionada(0) == 0); CHECK(legfontes_selecionada(1) == -1);
  CHECK(legfontes_nome(0)[0]); CHECK(!strcmp(legfontes_nome_ass(0), faceName)); CHECK(legfontes_caminho(0) != NULL);
  CHECK(legfontes_escolher(1, 0));
  stage(root, "entrada-3.tmp", b, n); CHECK(legfontes_importar(1, "entrada-3.tmp"));
  CHECK(legfontes_n() == 1); /* byte-identical import reuses the private copy. */
  legfontes_dir(other); legfontes_dir(root);
  CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  /* A failed related preference commit preserves the exact old font file,
   * not just the in-memory choice. Both slots use the same transaction. */
  char pref[1024]; snprintf(pref, sizeof pref, "%s/fontes-legenda.txt", root);
  size_t priorN; unsigned char *prior = font(pref, &priorN);
  for (int slot = 0; slot < 2; slot++) {
    CHECK(!legfontes_escolher_transacao(slot, -1, commit, &commitResult));
    CHECK(commits == slot + 1);
    CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
    size_t afterN; unsigned char *after = font(pref, &afterN);
    CHECK(afterN == priorN && !memcmp(after, prior, priorN)); free(after);
  }
  free(prior);
  /* A backup failure must stop before either preference or callback changes. */
  failBackup = 1;
  CHECK(!legfontes_escolher_transacao(0, -1, commit, &commitResult));
  CHECK(commits == 2 && legfontes_selecionada(0) == 0);
  failBackup = 0;
  commitResult = 1;
  CHECK(legfontes_escolher_transacao(1, -1, commit, &commitResult));
  CHECK(commits == 3 && legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == -1);
  CHECK(legfontes_escolher(1, 0));
  /* Primary picker: imported -> bundled -> imported with hard links denied. */
  CHECK(legfontes_escolher_transacao(0, -1, commit, &commitResult));
  CHECK(commits == 4 && legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == 0);
  CHECK(legfontes_escolher_transacao(0, 0, commit, &commitResult));
  CHECK(commits == 5 && legfontes_selecionada(0) == 0);
  legfontes_dir(other); legfontes_dir(root);
  CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  /* Old two-line files remain readable. A third independent choice must not
   * change either subtitle slot, including after reload or deletion. */
  char first[64], second[64];
  snprintf(pref, sizeof pref, "%s/fontes-legenda.txt", root);
  FILE *legacy = fopen(pref, "r"); CHECK(legacy != NULL);
  CHECK(fgets(first, sizeof first, legacy) && fgets(second, sizeof second, legacy)); fclose(legacy);
  legacy = fopen(pref, "w"); CHECK(legacy != NULL);
  CHECK(fputs(first, legacy) >= 0 && fputs(second, legacy) >= 0); CHECK(!fclose(legacy));
  legfontes_dir(other); legfontes_dir(root);
  CHECK(legfontes_selecionada(LEGFONTE_INTERFACE) == -1);
  CHECK(legfontes_escolher(LEGFONTE_INTERFACE, 0));
  legfontes_dir(other); legfontes_dir(root);
  CHECK(legfontes_selecionada(LEGFONTE_INTERFACE) == 0);
  CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  CHECK(!legfontes_escolher(3, 0)); CHECK(!legfontes_escolher(0, 99));
  /* Failed atomic preference write must keep the selected font and file. */
  snprintf(p, sizeof p, "%s/fontes-legenda.txt.tmp", root); CHECK(!mkdir(p, 0700));
  CHECK(!legfontes_escolher_transacao(0, -1, commit, &commitResult)); CHECK(commits == 5);
  CHECK(!legfontes_remover(0)); CHECK(legfontes_caminho(0) != NULL);
  CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  CHECK(!rmdir(p));
  prior = font(pref, &priorN); failRemoval = 1;
  CHECK(!legfontes_remover(0)); CHECK(legfontes_caminho(0) != NULL);
  CHECK(legfontes_selecionada(0) == 0 && legfontes_selecionada(1) == 0);
  size_t afterN; unsigned char *after = font(pref, &afterN);
  CHECK(afterN == priorN && !memcmp(after, prior, priorN)); free(after); free(prior);
  failRemoval = 0;
  CHECK(legfontes_escolher(0, -1)); CHECK(legfontes_selecionada(1) == 0);
  CHECK(legfontes_remover(0)); CHECK(legfontes_n() == 0);
  CHECK(legfontes_caminho(0) == NULL && legfontes_caminho(1) == NULL && legfontes_caminho(LEGFONTE_INTERFACE) == NULL);
  /* A late provider result owns the OLD staging directory, never a new one. */
  CHECK(legfontes_pedir_importacao(0)); CHECK(!legfontes_pedir_importacao(1));
  stage(root, "entrada-ab.tmp", b, n);
  legfontes_dir(other); stage(other, "entrada-ab.tmp", b, n);
  snprintf(event, sizeof event, "R:entrada-ab.tmp"); CHECK(legfontes_atualizar() == NULL);
  path(p, sizeof p, root, "entrada-ab.tmp"); CHECK(access(p, F_OK) != 0);
  path(p, sizeof p, other, "entrada-ab.tmp"); CHECK(!access(p, F_OK)); CHECK(!unlink(p));
  CHECK(legfontes_pedir_importacao(1)); stage(other, "entrada-cd.tmp", b, n); epoch++;
  snprintf(event, sizeof event, "R:entrada-cd.tmp"); CHECK(legfontes_atualizar() == NULL);
  path(p, sizeof p, other, "entrada-cd.tmp"); CHECK(access(p, F_OK) != 0);
  CHECK(legfontes_n() == 0);
  CHECK(legfontes_pedir_importacao(1)); snprintf(event, sizeof event, "C");
  CHECK(legfontes_atualizar() == NULL); CHECK(requests == 3);
  CHECK(legfontes_pedir_importacao(1)); epoch++; snprintf(event, sizeof event, "F");
  CHECK(legfontes_atualizar() == NULL); /* no old-profile failure toast */
  /* The Android result targets the interface slot captured before launch. */
  CHECK(legfontes_pedir_importacao(LEGFONTE_INTERFACE));
  stage(other, "entrada-de.tmp", b, n);
  snprintf(event, sizeof event, "R:entrada-de.tmp");
  CHECK(legfontes_atualizar() != NULL);
  CHECK(legfontes_selecionada(LEGFONTE_INTERFACE) == 0);
  CHECK(legfontes_selecionada(0) == -1 && legfontes_selecionada(1) == -1);
  CHECK(legfontes_remover(0));
  /* CJK-sized valid files are accepted, and the byte cap rejects before load. */
  size_t largeN = 9u * 1024u * 1024u; unsigned char *large = calloc(1, largeN); assert(large);
  assert(n < largeN); memcpy(large, b, n); stage(other, "entrada-ef.tmp", large, largeN);
  CHECK(legfontes_importar(1, "entrada-ef.tmp")); free(large);
  char owned[1024]; snprintf(owned, sizeof owned, "%s", legfontes_caminho(1));
  CHECK(!unlink(owned)); CHECK(legfontes_caminho(1) == NULL);
  legfontes_dir(root); legfontes_dir(other); CHECK(legfontes_selecionada(1) == -1);
  path(p, sizeof p, other, "entrada-ff.tmp"); FILE *f = fopen(p, "wb"); assert(f);
  assert(!ftruncate(fileno(f), LEGFONTE_MAX_BYTES + 1)); fclose(f);
  CHECK(!legfontes_importar(0, "entrada-ff.tmp")); CHECK(access(p, F_OK) != 0);
  free(b); TTF_Quit();
  snprintf(p, sizeof p, "%s/fontes-legenda.txt", root); unlink(p);
  snprintf(p, sizeof p, "%s/fontes-legenda.txt", other); unlink(p);
  path(p, sizeof p, root, ""); CHECK(!rmdir(p)); CHECK(!rmdir(root));
  path(p, sizeof p, other, ""); CHECK(!rmdir(p)); CHECK(!rmdir(other));
  printf("legfontes: %d checks passed\n", checks);
  return 0;
}
