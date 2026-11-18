// Settings > Addons of "From another source": per-profile keys switched off,
// a new add-on on at once, the list of add-ons with sources (audioextaddons.h).
#include "../src/audioextaddons.h"
#include "../src/addons.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- stubs: files in memory, two profiles, four add-ons ------------------------
static char arqNome[4][48], arqTexto[4][2048];
static int arqN, perfil = 1;
static int achar(const char *n) { for (int i = 0; i < arqN; i++) if (!strcmp(arqNome[i], n)) return i; return -1; }
char *dados_ler(const char *n) { int i = achar(n); if (i < 0) return NULL; char *d = malloc(strlen(arqTexto[i]) + 1); strcpy(d, arqTexto[i]); return d; }
int dados_gravar(const char *n, const char *t) {
  int i = achar(n);
  if (i < 0) { i = arqN++; snprintf(arqNome[i], sizeof arqNome[i], "%s", n); }
  snprintf(arqTexto[i], sizeof arqTexto[i], "%s", t); return 1;
}
int dados_apagar(const char *n) { int i = achar(n); if (i >= 0) arqNome[i][0] = 0; return 1; }
int perfis_ativo(void) { return perfil; }
static const char *const NOMES[] = { "AIOStreams", "Torrentio", "Cinemeta", "MediaFusion", "Napisy24 Addon" };
static const char *const BASES[] = { "https://aio.elfhosted.com/stremio/abc", "https://torrentio.strem.fun/tb=x",
                                     "https://v3-cinemeta.strem.io", "https://mediafusion.elfhosted.com/xyz",
                                     "https://napisy24.example/sub" };
// Napisy24's manifest has not answered yet: presumed to have streams.
static int napisyEnviou;
int addons_n(void) { return 5; }
int addons_sondado(int i) { return i != 4; }
int stream_lista_tem_addon(const char *k) { return napisyEnviou && k && strstr(k, "napisy24"); }
const char *addons_nome(int i) { return NOMES[i]; }
const char *addons_base(int i) { return BASES[i]; }
int addons_ativo(int i) { (void)i; return 1; }
int addons_fornece(int i, int oque) { return oque == ADD_STREAM && i != 2; }   // Cinemeta: no streams
void fontefalha_addon_chave(const char *base, char *dst, unsigned n) { snprintf(dst, n, "k:%.30s", base + 8); }

int main(void) {
  AudioExtAddon a[8];
  int n = audioext_addons_lista(a, 8), on, tot;
  unsigned v;
  assert(n == 3 && !strcmp(a[0].nome, "AIOStreams") && !strcmp(a[0].host, "aio.elfhosted.com"));
  assert(!strcmp(a[1].host, "torrentio.strem.fun") && !strcmp(a[2].nome, "MediaFusion"));
  // An add-on whose manifest has not answered counts only once it sent a source.
  napisyEnviou = 1;
  assert(audioext_addons_lista(a, 8) == 4 && !strcmp(a[3].nome, "Napisy24 Addon"));
  napisyEnviou = 0;
  assert(audioext_addons_lista(a, 8) == 3);
  // All on by default; a source without a key always passes.
  assert(!audioext_addons_resumo(&on, &tot) && on == 3 && tot == 3);
  assert(audioext_addon_ligado(a[1].chave) && audioext_addon_ligado("") && audioext_addon_ligado(NULL));
  v = audioext_addons_versao();
  audioext_addon_definir(a[1].chave, 0);
  assert(!audioext_addon_ligado(a[1].chave) && audioext_addons_versao() != v);
  assert(audioext_addons_resumo(&on, &tot) && on == 2 && tot == 3);
  { char *t = dados_ler("audioext-addons-p1.txt"); assert(t && strstr(t, a[1].chave)); free(t); }
  // Another profile: its own list (empty); back: the saved one.
  perfil = 2;
  assert(audioext_addon_ligado(a[1].chave));
  audioext_addon_definir(a[2].chave, 0);
  perfil = 1;
  assert(!audioext_addon_ligado(a[1].chave) && audioext_addon_ligado(a[2].chave));
  // On again: the file goes when nothing is off.
  audioext_addon_definir(a[1].chave, 1);
  assert(audioext_addon_ligado(a[1].chave) && !dados_ler("audioext-addons-p1.txt"));
  // Same state twice: no change.
  v = audioext_addons_versao();
  audioext_addon_definir(a[0].chave, 1);
  assert(audioext_addons_versao() == v);
  puts("audioextaddons: per-profile add-on filter, new add-ons on, list (unprobed add-ons only with sources) and summary passed");
  return 0;
}
