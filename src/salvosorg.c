// Organizacao dos Salvos. Ver a nota longa em salvosorg.h.
#include "salvosorg.h"
#include "salvos.h"
#include "dados.h"
#include "perfis.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define SORG_VERSAO 1
// Teto do mapa titulo -> categoria. Cabe a lista local inteira e o catalogo
// inteiro; passando disso a atribuicao mais antiga sai (como em salvos.c).
#define SORG_MAPA_MAX (SALVOS_MAX + 2000)

typedef struct { int id; char nome[SORG_NOME_MAX]; } SorgCat;
typedef struct { char id[24]; int cat; } SorgItem;

static int perfilLido = -1;
static unsigned revisao;
static int ordem, grupo, estilo, social;
static SorgCat cats[SORG_CAT_MAX];
static int nCats, proximoId = 1;
static SorgItem *mapa;
static int nMapa, capMapa;

unsigned sorg_revisao(void) { return revisao; }

static const char *arquivo(int p) {
  static char nome[48];
  snprintf(nome, sizeof nome, "salvos-org-p%d.txt", p > 0 ? p : 1);
  return nome;
}

static int garantirMapa(int n) {
  SorgItem *novo;
  int cap;
  if (n <= capMapa) return 1;
  if (n > SORG_MAPA_MAX) return 0;
  cap = capMapa ? capMapa * 2 : 64;
  while (cap < n) cap *= 2;
  if (cap > SORG_MAPA_MAX) cap = SORG_MAPA_MAX;
  novo = (SorgItem *)realloc(mapa, sizeof *mapa * (size_t)cap);
  if (!novo) return 0;
  mapa = novo;
  capMapa = cap;
  return 1;
}

static int clampi(int v, int n) { return v < 0 || v >= n ? 0 : v; }

// Campo de arquivo: TAB e quebra de linha viram espaco.
static void semTab(char *s) {
  for (; *s; s++) if (*s == '\t' || *s == '\n' || *s == '\r') *s = ' ';
}

static void gravar(void) {
  size_t cap = 256u + (size_t)nCats * (SORG_NOME_MAX + 16u) + (size_t)nMapa * 40u;
  char *b = (char *)malloc(cap);
  size_t k = 0;
  int i;
  if (!b) return;
  k += (size_t)snprintf(b + k, cap - k,
                        "# nuvio salvos-org v%d\nordem\t%d\ngrupo\t%d\nestilo\t%d\n"
                        "social\t%d\n", SORG_VERSAO, ordem, grupo, estilo, social);
  for (i = 0; i < nCats && k + 1 < cap; i++)
    k += (size_t)snprintf(b + k, cap - k, "cat\t%d\t%s\n", cats[i].id, cats[i].nome);
  for (i = 0; i < nMapa && k + 1 < cap; i++)
    k += (size_t)snprintf(b + k, cap - k, "item\t%d\t%s\n", mapa[i].cat, mapa[i].id);
  dados_gravar(arquivo(perfilLido), b);
  free(b);
}

void sorg_recarregar(void) { perfilLido = -1; sorg_carregar(); }
void sorg_carregar(void) {
  int p = perfis_ativo();
  char *b, *linha, *prox;
  if (p == perfilLido) return;
  perfilLido = p;
  ordem = grupo = estilo = social = 0;
  nCats = 0; nMapa = 0; proximoId = 1;
  revisao++;
  b = dados_ler(arquivo(p));
  if (!b) return;
  for (linha = b; linha && *linha; linha = prox) {
    char *fim = strchr(linha, '\n'), *t1, *t2;
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    t1 = strchr(linha, '\t');
    if (!t1) continue;
    *t1++ = 0;
    if (!strcmp(linha, "ordem"))  { ordem  = clampi(atoi(t1), SORG_ORDEM_N);  continue; }
    if (!strcmp(linha, "grupo"))  { grupo  = clampi(atoi(t1), SORG_GRUPO_N);  continue; }
    if (!strcmp(linha, "estilo")) { estilo = clampi(atoi(t1), SORG_ESTILO_N); continue; }
    if (!strcmp(linha, "social")) { social = clampi(atoi(t1), SORG_SOCIAL_N); continue; }
    t2 = strchr(t1, '\t');
    if (!t2) continue;
    *t2++ = 0;
    if (!strcmp(linha, "cat")) {
      int id = atoi(t1);
      if (id <= 0 || !t2[0] || nCats >= SORG_CAT_MAX || sorg_categoria_indice(id) >= 0) continue;
      cats[nCats].id = id;
      snprintf(cats[nCats].nome, sizeof cats[nCats].nome, "%s", t2);
      nCats++;
      if (id >= proximoId) proximoId = id + 1;
    } else if (!strcmp(linha, "item")) {
      int c = atoi(t1);
      if (c <= 0 || !t2[0] || !garantirMapa(nMapa + 1)) continue;
      salvos_id_titulo(t2, mapa[nMapa].id, sizeof mapa[nMapa].id);
      mapa[nMapa].cat = c;
      nMapa++;
    }
  }
  free(b);
  // Atribuicao a categoria que nao existe mais (arquivo editado, ou gravado no
  // meio de uma exclusao) nao vale: cai fora agora, e nao no desenho.
  { int i, k = 0;
    for (i = 0; i < nMapa; i++)
      if (sorg_categoria_indice(mapa[i].cat) >= 0) mapa[k++] = mapa[i];
    nMapa = k; }
  printf("[salvos] organizacao do perfil %d: %d categorias, %d titulos nelas\n",
         p, nCats, nMapa);
  fflush(stdout);
}

int sorg_ordem(void)  { sorg_carregar(); return ordem; }
int sorg_grupo(void)  { sorg_carregar(); return grupo; }
int sorg_estilo(void) { sorg_carregar(); return estilo; }
int sorg_social(void) { sorg_carregar(); return social; }

static void definir(int *campo, int v, int n) {
  sorg_carregar();
  v = clampi(v, n);
  if (*campo == v) return;
  *campo = v;
  revisao++;
  gravar();
}
void sorg_definir_ordem(int v)  { definir(&ordem, v, SORG_ORDEM_N); }
void sorg_definir_grupo(int v)  { definir(&grupo, v, SORG_GRUPO_N); }
void sorg_definir_estilo(int v) { definir(&estilo, v, SORG_ESTILO_N); }
void sorg_definir_social(int v) { definir(&social, v, SORG_SOCIAL_N); }

int sorg_n_categorias(void) { sorg_carregar(); return nCats; }
int sorg_categoria_id(int i) { return i >= 0 && i < nCats ? cats[i].id : 0; }
int sorg_categoria_indice(int id) {
  int i;
  for (i = 0; i < nCats; i++) if (cats[i].id == id) return i;
  return -1;
}
const char *sorg_categoria_nome_id(int id) {
  int i = sorg_categoria_indice(id);
  return i >= 0 ? cats[i].nome : NULL;
}

int sorg_nome_limpo(const char *in, char *out, int tam) {
  int k = 0, espaco = 0;
  if (!out || tam < 1) return 0;
  out[0] = 0;
  if (!in) return 0;
  for (; *in && k + 1 < tam; in++) {
    unsigned char ch = (unsigned char)*in;
    if (ch == '\t' || ch == '\n' || ch == '\r') ch = ' ';
    if (ch == ' ') { espaco = k > 0; continue; }
    if (espaco && k + 2 < tam) out[k++] = ' ';
    espaco = 0;
    out[k++] = (char)ch;
  }
  out[k] = 0;
  // So ASCII ganha maiuscula: o teclado do app digita a-z, e o resto (um nome
  // que veio de outro lugar com acento) fica como esta.
  if (k > 0 && out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 'a' + 'A');
  return k;
}

static int acharNome(const char *nome) {
  int i;
  for (i = 0; i < nCats; i++) if (!strcasecmp(cats[i].nome, nome)) return i;
  return -1;
}

int sorg_criar_categoria(const char *nome) {
  char limpo[SORG_NOME_MAX];
  int i;
  sorg_carregar();
  if (sorg_nome_limpo(nome, limpo, sizeof limpo) < 1) return 0;
  semTab(limpo);
  i = acharNome(limpo);
  if (i >= 0) return cats[i].id;
  if (nCats >= SORG_CAT_MAX) return 0;
  cats[nCats].id = proximoId++;
  snprintf(cats[nCats].nome, sizeof cats[nCats].nome, "%s", limpo);
  nCats++;
  revisao++;
  gravar();
  return cats[nCats - 1].id;
}

int sorg_renomear_categoria(int id, const char *nome) {
  char limpo[SORG_NOME_MAX];
  int i, j;
  sorg_carregar();
  i = sorg_categoria_indice(id);
  if (i < 0 || sorg_nome_limpo(nome, limpo, sizeof limpo) < 1) return 0;
  semTab(limpo);
  j = acharNome(limpo);
  if (j >= 0 && j != i) return 0;          // duas "Kids" nao se distinguem
  if (!strcmp(cats[i].nome, limpo)) return 1;
  snprintf(cats[i].nome, sizeof cats[i].nome, "%s", limpo);
  revisao++;
  gravar();
  return 1;
}

void sorg_excluir_categoria(int id) {
  int i, k = 0;
  sorg_carregar();
  i = sorg_categoria_indice(id);
  if (i < 0) return;
  memmove(&cats[i], &cats[i + 1], sizeof *cats * (size_t)(nCats - i - 1));
  nCats--;
  for (i = 0; i < nMapa; i++) if (mapa[i].cat != id) mapa[k++] = mapa[i];
  nMapa = k;
  revisao++;
  gravar();
}

static int acharMapa(const char *imdb) {
  char chave[24];
  int i;
  if (!imdb || !imdb[0]) return -1;
  salvos_id_titulo(imdb, chave, sizeof chave);
  for (i = 0; i < nMapa; i++) if (!strcmp(mapa[i].id, chave)) return i;
  return -1;
}

int sorg_categoria_de(const char *imdb) {
  int i;
  sorg_carregar();
  if (nMapa < 1) return 0;
  i = acharMapa(imdb);
  return i >= 0 ? mapa[i].cat : 0;
}

void sorg_mover(const char *imdb, int id) {
  int i;
  sorg_carregar();
  if (!imdb || !imdb[0]) return;
  if (id && sorg_categoria_indice(id) < 0) return;
  i = acharMapa(imdb);
  if (i >= 0 && mapa[i].cat == id) return;
  if (!id) {
    if (i < 0) return;
    memmove(&mapa[i], &mapa[i + 1], sizeof *mapa * (size_t)(nMapa - i - 1));
    nMapa--;
  } else if (i >= 0) {
    mapa[i].cat = id;
  } else {
    if (!garantirMapa(nMapa + 1)) {
      if (nMapa < 1) return;
      memmove(&mapa[0], &mapa[1], sizeof *mapa * (size_t)(nMapa - 1));
      nMapa--;
    }
    salvos_id_titulo(imdb, mapa[nMapa].id, sizeof mapa[nMapa].id);
    mapa[nMapa].cat = id;
    nMapa++;
  }
  revisao++;
  gravar();
}

void sorg_esquecer(void) {
  int p;
  for (p = 1; p <= 8; p++) dados_apagar(arquivo(p));
  free(mapa);
  mapa = NULL;
  nMapa = capMapa = 0;
  nCats = 0;
  proximoId = 1;
  ordem = grupo = estilo = social = 0;
  perfilLido = -1;
  revisao++;
}
