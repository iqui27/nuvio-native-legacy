// "Addons" of "From another source" (audioextaddons.h).
#include "audioextaddons.h"
#include "addons.h"
#include "dados.h"
#include "fontefalha.h"
#include "perfis.h"
#include "streams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char desligadas[AUDIOEXT_ADDONS_MAX][40];
static int nDesligadas;
static int perfilLido = -1;
static unsigned versao;

static void arquivo(int perfil, char *dst, size_t n) { snprintf(dst, n, "audioext-addons-p%d.txt", perfil); }

// The active profile's list, read again when the profile changes.
static void carregar(void) {
  int p = perfis_ativo();
  char nome[48], *s, *linha, *ctx = NULL;
  if (p == perfilLido) return;
  perfilLido = p; nDesligadas = 0; versao++;
  arquivo(p, nome, sizeof nome);
  s = dados_ler(nome);
  if (!s) return;
  for (linha = strtok_r(s, "\r\n", &ctx); linha && nDesligadas < AUDIOEXT_ADDONS_MAX; linha = strtok_r(NULL, "\r\n", &ctx)) {
    size_t l = strlen(linha);
    if (!l || l >= sizeof desligadas[0]) continue;
    snprintf(desligadas[nDesligadas++], sizeof desligadas[0], "%s", linha);
  }
  free(s);
}
static void gravar(void) {
  char nome[48], texto[AUDIOEXT_ADDONS_MAX * 41 + 1];
  size_t u = 0;
  texto[0] = 0;
  for (int i = 0; i < nDesligadas; i++) u += (size_t)snprintf(texto + u, sizeof texto - u, "%s\n", desligadas[i]);
  arquivo(perfilLido, nome, sizeof nome);
  if (nDesligadas) dados_gravar(nome, texto);
  else dados_apagar(nome);
}

int audioext_addon_ligado(const char *chave) {
  if (!chave || !*chave) return 1;
  carregar();
  for (int i = 0; i < nDesligadas; i++) if (!strcmp(desligadas[i], chave)) return 0;
  return 1;
}
void audioext_addon_definir(const char *chave, int ligado) {
  int k = -1;
  if (!chave || !*chave || strlen(chave) >= sizeof desligadas[0]) return;
  carregar();
  for (int i = 0; i < nDesligadas; i++) if (!strcmp(desligadas[i], chave)) k = i;
  if (ligado && k >= 0) {
    memmove(desligadas[k], desligadas[k + 1], sizeof desligadas[0] * (size_t)(nDesligadas - k - 1));
    nDesligadas--;
  } else if (!ligado && k < 0 && nDesligadas < AUDIOEXT_ADDONS_MAX) {
    snprintf(desligadas[nDesligadas++], sizeof desligadas[0], "%s", chave);
  } else return;
  versao++;
  gravar();
  printf("[ext-audio] addons: %.12s… %s for From another source (%d off)\n", chave, ligado ? "on" : "off", nDesligadas);
  fflush(stdout);
}
unsigned audioext_addons_versao(void) { carregar(); return versao; }

int audioext_addons_lista(AudioExtAddon *out, int max) {
  int k = 0, n = addons_n();
  for (int i = 0; i < n && k < max; i++) {
    const char *base = addons_base(i), *h, *fim;
    if (!addons_ativo(i) || !addons_fornece(i, ADD_STREAM) || !base || !*base) continue;
    fontefalha_addon_chave(base, out[k].chave, sizeof out[k].chave);
    // Before its manifest answers, every add-on is presumed to have streams:
    // a subtitles add-on (Napisy24) only counts once it sent a source here.
    if (!addons_sondado(i) && !stream_lista_tem_addon(out[k].chave)) continue;
    snprintf(out[k].nome, sizeof out[k].nome, "%s", addons_nome(i)[0] ? addons_nome(i) : base);
    h = strstr(base, "://"); h = h ? h + 3 : base;
    fim = h + strcspn(h, "/?");
    snprintf(out[k].host, sizeof out[k].host, "%.*s", (int)(fim - h), h);
    k++;
  }
  return k;
}
int audioext_addons_resumo(int *ligados, int *total) {
  AudioExtAddon a[AUDIOEXT_ADDONS_MAX];
  int n = audioext_addons_lista(a, AUDIOEXT_ADDONS_MAX), on = 0;
  for (int i = 0; i < n; i++) on += audioext_addon_ligado(a[i].chave);
  if (ligados) *ligados = on;
  if (total) *total = n;
  return on < n;
}
