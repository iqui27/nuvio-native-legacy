// A faixa de audio escolhida a mao, gravada por perfil. Ver audiomemoria.h.
#include "audiomemoria.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  char titulo[64];
  long long quandoS;
  AudioLembrada m;
} AudioMem;

static AudioMem tabela[AUDIOMEM_MAX];
static int nTab, carregado, perfil;

// Ate onde audiomem_esquecer varre (CONTA_PERFIL_MAX e 8; ver fontepref.c).
#define AUDIOMEM_PERFIS 16

static void nomeDoPerfil(char *dst, size_t tam, int p) {
  snprintf(dst, tam, "audiomemoria-p%d.txt", p);
}

// Byte de controle quebraria a linha do arquivo (o TAB e um deles).
static void limpar(char *s) {
  for (; *s; s++) if ((unsigned char)*s < 32) *s = ' ';
}

static char *campo(char **p) {
  char *ini = *p, *t;
  if (!ini) return (char *)"";
  t = strchr(ini, '\t');
  if (t) { *t = 0; *p = t + 1; } else { *p = NULL; }
  return ini;
}

// "tt1234567:2:4" -> "tt1234567". So o id do IMDb leva ":temporada:episodio";
// um NOME pode ter ":" de verdade (a mesma regra de legmem_id_titulo).
static void idTitulo(const char *id, char *dst, size_t tam) {
  const char *c;
  size_t n;
  dst[0] = 0;
  if (!id) return;
  n = strlen(id);
  if (!strncmp(id, "tt", 2) && (c = strchr(id, ':')) != NULL) n = (size_t)(c - id);
  if (n >= tam) n = tam - 1;
  memcpy(dst, id, n);
  dst[n] = 0;
  limpar(dst);
}

// Formato: "titulo\tquando\tidioma\tcodec\tcanais\tnumero\tbinge\trotulo"
// (rotulo por ultimo: e o unico texto livre).
static void gravarArquivo(void) {
  size_t cap = (size_t)AUDIOMEM_MAX * 320u + 64u, k = 0;
  char *buf = (char *)malloc(cap), nome[48];
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio audio v1\n");
  for (i = 0; i < nTab && k + 1 < cap; i++) {
    const AudioMem *e = &tabela[i];
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%lld\t%s\t%s\t%d\t%d\t%s\t%s\n", e->titulo, e->quandoS,
                          e->m.idioma, e->m.codec, e->m.canais, e->m.numero, e->m.binge, e->m.rotulo);
  }
  nomeDoPerfil(nome, sizeof nome, perfil);
  dados_gravar(nome, buf);
  free(buf);
}

static void carregar(void) {
  char *b, *linha, *prox, nome[48];
  if (carregado) return;
  carregado = 1;
  nTab = 0;
  nomeDoPerfil(nome, sizeof nome, perfil);
  b = dados_ler(nome);
  if (!b) return;
  for (linha = b; linha && *linha && nTab < AUDIOMEM_MAX; linha = prox) {
    char *p, *tit, *quando, *idi, *codec, *canais, *num, *binge;
    char *fim = strchr(linha, '\n');
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    tit = campo(&p); quando = campo(&p); idi = campo(&p); codec = campo(&p);
    canais = campo(&p); num = campo(&p); binge = campo(&p);
    if (!tit[0]) continue;
    { AudioMem *e = &tabela[nTab++];
      memset(e, 0, sizeof *e);
      snprintf(e->titulo, sizeof e->titulo, "%s", tit);
      e->quandoS = atoll(quando);
      snprintf(e->m.idioma, sizeof e->m.idioma, "%s", idi);
      snprintf(e->m.codec, sizeof e->m.codec, "%s", codec);
      e->m.canais = atoi(canais);
      e->m.numero = num[0] ? atoi(num) : -1;
      snprintf(e->m.binge, sizeof e->m.binge, "%s", binge);
      snprintf(e->m.rotulo, sizeof e->m.rotulo, "%s", p ? p : ""); }
  }
  free(b);
  printf("[audio] %d escolha(s) gravada(s) no perfil %d\n", nTab, perfil);
  fflush(stdout);
}

void audiomem_definir_perfil(int p) {
  if (p < 0) p = 0;
  if (p == perfil && carregado) return;
  perfil = p;
  nTab = 0;
  carregado = 0;
  audioesc_soltar();
}

void audiomem_esquecer(void) {
  int p;
  for (p = 0; p <= AUDIOMEM_PERFIS; p++) {
    char nome[48];
    nomeDoPerfil(nome, sizeof nome, p);
    dados_apagar(nome);
  }
  nTab = 0;
  carregado = 1;
  audioesc_soltar();
}

const AudioLembrada *audiomem_ler(const char *titulo) {
  char tit[64];
  int i;
  carregar();
  idTitulo(titulo, tit, sizeof tit);
  if (!tit[0]) return NULL;
  for (i = nTab - 1; i >= 0; i--)
    if (!strcmp(tabela[i].titulo, tit)) return &tabela[i].m;
  return NULL;
}

void audiomem_gravar(const char *titulo, const AudioLembrada *m) {
  char tit[64];
  AudioMem novo;
  int i;
  if (!m) return;
  carregar();
  idTitulo(titulo, tit, sizeof tit);
  if (!tit[0]) return;
  memset(&novo, 0, sizeof novo);
  snprintf(novo.titulo, sizeof novo.titulo, "%s", tit);
  novo.quandoS = (long long)time(NULL);
  novo.m = *m;
  limpar(novo.m.idioma); limpar(novo.m.codec); limpar(novo.m.binge); limpar(novo.m.rotulo);
  // A deste titulo vai para o FIM (a mais nova); cheia, sai a primeira.
  for (i = 0; i < nTab; i++)
    if (!strcmp(tabela[i].titulo, tit)) {
      memmove(&tabela[i], &tabela[i + 1], sizeof *tabela * (size_t)(nTab - i - 1));
      nTab--;
      break;
    }
  if (nTab >= AUDIOMEM_MAX) {
    memmove(&tabela[0], &tabela[1], sizeof *tabela * (size_t)(AUDIOMEM_MAX - 1));
    nTab = AUDIOMEM_MAX - 1;
  }
  tabela[nTab++] = novo;
  gravarArquivo();
}
