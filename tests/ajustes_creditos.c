// Focused persistence test using ajustes.c and the real JSON parser.
// The argument is an existing, disposable directory. Presentation, media
// languages and unrelated settings stores are inert; profile files use stdio.
// Build with function/data sections and linker garbage collection.
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED

#ifdef _WIN32
static int replaceFile(const char *from, const char *to);
#define rename replaceFile
#endif
#include "../src/ajustes.c"
#ifdef _WIN32
#undef rename
// Windows CRT rename cannot replace a file. Model the POSIX operation used
// by the TV targets, only inside the disposable test directory.
static int replaceFile(const char *from, const char *to) {
  assert(!strncmp(from, dirAjustes, strlen(dirAjustes)));
  assert(!strncmp(to, dirAjustes, strlen(dirAjustes)));
  if (remove(to) && errno != ENOENT) return -1;
  return rename(from, to);
}
#endif

static void dataPath(char *out, size_t size, const char *name) {
  int n = snprintf(out, size, "%s/%s", dirAjustes, name);
  assert(n >= 0 && (size_t)n < size);
}
char *dados_ler(const char *name) {
  char path[640];
  dataPath(path, sizeof path, name);
  FILE *f = fopen(path, "rb");
  if (!f) return NULL;
  assert(!fseek(f, 0, SEEK_END));
  long size = ftell(f);
  assert(size >= 0 && !fseek(f, 0, SEEK_SET));
  char *out = malloc((size_t)size + 1);
  assert(out && fread(out, 1, (size_t)size, f) == (size_t)size);
  out[size] = 0;
  assert(!fclose(f));
  return out;
}
int dados_gravar(const char *name, const char *text) {
  char path[640];
  dataPath(path, sizeof path, name);
  FILE *f = fopen(path, "wb");
  if (!f) return 0;
  size_t n = strlen(text);
  int ok = fwrite(text, 1, n, f) == n;
  int closeError = fclose(f);
  return ok && !closeError;
}
int dados_apagar(const char *name) {
  char path[640];
  dataPath(path, sizeof path, name);
  return !remove(path) || errno == ENOENT;
}
void dados_marcar_sujo(int light) { (void)light; }

void fonteregra_perfil_guardar(int profile) { (void)profile; }
int fonteregra_perfil_restaurar(int profile) { (void)profile; return 0; }
void fonteregra_perfil_esquecer(void) {}
int fonteregra_do_blob(const char *json) { (void)json; return 0; }
int fonteregra_mesclar(const char *base, char **out) { (void)base; *out = NULL; return 0; }
void selospacote_conta_do_blob(const char *json) { (void)json; }
int selospacote_n(void) { return 0; }
static PosterProvCfg poster;
const PosterProvCfg *posterprov_cfg(void) { return &poster; }
void posterprov_configurar(const PosterProvCfg *cfg) { poster = *cfg; }
const char *ling_opcao_codigo(int i) { (void)i; return ""; }
void ling_local_legenda(const char *code) { (void)code; }
void ling_local_legenda2(const char *code) { (void)code; }
void ling_local_audio(const char *code) { (void)code; }
void ling_conta_legenda(const char *code) { (void)code; }
void ling_conta_legenda2(const char *code) { (void)code; }
void ling_conta_audio(const char *code) { (void)code; }
const char *ling_legenda(void) { return ""; }
const char *ling_audio(void) { return ""; }

int main(int argc, char **argv) {
  assert(argc == 2 && strlen(argv[1]) < sizeof dirAjustes);
  snprintf(dirAjustes, sizeof dirAjustes, "%s", argv[1]);
  assert(!ajustes_auto_creditos() && valorPadrao[AJ_AUTO_CREDITOS] == 1);
  assert(somenteDesteAparelho(AJ_AUTO_CREDITOS) && dePerfil(AJ_AUTO_CREDITOS));
  assert(!strcmp(uxEscopo(AJ_AUTO_CREDITOS), "Este perfil nesta TV"));

  valor[AJ_AUTO_CREDITOS] = 0;
  valor[AJ_HERO] = 1;
  ajustes_perfil_guardar(1);
  char *snapshot = dados_ler("ajustes-p1.txt");
  assert(snapshot && strstr(snapshot, "autoCreditosLocal 0\n"));
  free(snapshot);
  valor[AJ_AUTO_CREDITOS] = 1;
  valor[AJ_HERO] = 0;
  ajustes_perfil_guardar(2);
  // Clear memory before loading a saved file: no in-memory profile model.
  memcpy(valor, valorPadrao, sizeof valor);
  assert(ajustes_perfil_restaurar(1) && ajustes_auto_creditos());
  assert(valor[AJ_HERO] == 1);
  assert(ajustes_perfil_restaurar(2) && !ajustes_auto_creditos());
  assert(valor[AJ_HERO] == 0);

  assert(dados_gravar("ajustes-p4.txt", "heroSectionEnabled 1\n"));
  valor[AJ_AUTO_CREDITOS] = 0;
  assert(ajustes_perfil_restaurar(4) && !ajustes_auto_creditos());
  // Same primary-profile fallback used by sync_trocar_perfil on first visit.
  assert(ajustes_perfil_restaurar(1) && ajustes_auto_creditos());
  int hero = valor[AJ_HERO];
  assert(!ajustes_perfil_restaurar(5));
  ajustes_auto_creditos_restaurar(5);
  assert(!ajustes_auto_creditos() && valor[AJ_HERO] == hero);
  assert(dados_gravar("ajustes-p6.txt", "autoCreditosLocal 99\n"));
  valor[AJ_AUTO_CREDITOS] = 0;
  ajustes_auto_creditos_restaurar(6);
  assert(!ajustes_auto_creditos());

  valor[AJ_IDIOMA] = IDIOMA_EN + 1;
  valor[AJ_PAUSA_OVERLAY] = 0;
  valor[AJ_AUTO_CREDITOS] = 1;
  assert(ajustes_aplicar_blob("{\"autoCreditosLocal\":true,\"auto_creditos_local\":true}") == 0);
  assert(!ajustes_auto_creditos());
  valor[AJ_AUTO_CREDITOS] = 0;
  assert(ajustes_aplicar_blob("{\"autoCreditosLocal\":false,\"auto_creditos_local\":false}") == 0);
  assert(ajustes_auto_creditos());
  char *merged = NULL;
  assert(ajustes_mesclar_blob("{\"pauseOverlayEnabled\":false}", &merged) > 0);
  assert(merged && !strstr(merged, "autoCreditos") && !strstr(merged, "auto_creditos"));
  free(merged);

  ajustes_perfil_esquecer();
  assert(!ajustes_auto_creditos() && !dados_ler("ajustes-p1.txt"));
  snapshot = dados_ler("ajustes.txt");
  assert(snapshot && (strstr(snapshot, "autoCreditosLocal 1\n") ||
                      strstr(snapshot, "autoCreditosLocal 1\r\n")));
  free(snapshot);
  puts("ajustes_creditos: defaults, profile files, old/new profiles, account isolation and logout passed");
  return 0;
}
