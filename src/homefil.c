// Opcoes de FILEIRA da Home 2.1 (design/2.1/home.html): "vistos ocultos" (#366),
// colecao fixada como fileira propria, e ocultar/mover uma fileira pela chave.
// Escolhas LOCAIS e por perfil (ocultavistos*.txt, colfixada*.txt): o servidor
// nao conhece nenhuma delas (NOTAS.md, perguntas em aberto 4), entao nada daqui
// vai para a conta.
#include "homefil.h"
#include "fileiras.h"
#include "catalogo.h"
#include "addons.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char *dados_ler(const char *nome);
int   dados_gravar(const char *nome, const char *conteudo);
int   perfis_ativo(void);

#define OV_CHAVE 200


static char ovChaves[OV_MAX][OV_CHAVE];
static int  ovN;
static int  ovPerfil = -1;     // -1 = ainda nao carregado
static unsigned ovRev;

static void ovNome(char *dst, size_t tam, int p) {
  if (p <= 1) snprintf(dst, tam, "ocultavistos.txt");
  else        snprintf(dst, tam, "ocultavistos-p%d.txt", p);
}

static void ovCarregar(int p) {
  char nome[64], *txt, *s, *fim;
  ovPerfil = p;
  ovN = 0;
  ovNome(nome, sizeof nome, p);
  txt = dados_ler(nome);
  // Sem arquivo proprio, o ovPerfil > 1 nao herda nada: a marca e um gosto tao
  // simples que prefere nascer desligada a arrastar a do outro ovPerfil. O
  // ovPerfil 1 usa o arquivo sem sufixo — o mesmo trato de fileirasui.txt.
  if (!txt) return;
  s = txt;
  while (*s && ovN < OV_MAX) {
    size_t n;
    while (*s == '\n' || *s == '\r') s++;
    fim = strchr(s, '\n');
    n = fim ? (size_t)(fim - s) : strlen(s);
    while (n && (s[n - 1] == '\r')) n--;
    if (n >= OV_CHAVE) n = OV_CHAVE - 1;
    if (n) {
      memcpy(ovChaves[ovN], s, n);
      ovChaves[ovN][n] = 0;
      ovN++;
    }
    if (!fim) break;
    s = fim + 1;
  }
  free(txt);
}

static void ovGarantir(void) {
  if (ovPerfil < 0) ovCarregar(perfis_ativo());
}

int ov_ativa(const char *chave) {
  int i;
  ovGarantir();
  if (!chave || !chave[0]) return 0;
  for (i = 0; i < ovN; i++) if (!strcmp(ovChaves[i], chave)) return 1;
  return 0;
}

static void ovGravar(void) {
  char nome[64];
  char buf[OV_MAX * (OV_CHAVE + 1)] = "";
  size_t uso = 0;
  int i;
  ovNome(nome, sizeof nome, perfis_ativo());
  for (i = 0; i < ovN; i++)
    uso += (size_t)snprintf(buf + uso, sizeof buf - uso, "%s\n", ovChaves[i]);
  dados_gravar(nome, buf);
}

int ov_alternar(const char *chave) {
  int i, ligar = 1;
  ovGarantir();
  if (!chave || !chave[0]) return 0;
  for (i = 0; i < ovN; i++)
    if (!strcmp(ovChaves[i], chave)) {
      ovN--;
      if (i < ovN) memmove(ovChaves[i], ovChaves[i + 1], (size_t)(ovN - i) * OV_CHAVE);
      ligar = 0;
      break;
    }
  if (ligar) {
    if (ovN >= OV_MAX) return 0;   // cheia: recusa em vez de despejar
    snprintf(ovChaves[ovN], OV_CHAVE, "%s", chave);
    ovN++;
  }
  ovRev++;
  ovGravar();
  return ligar;
}

unsigned ov_revisao(void) { return ovRev; }

void ov_definir_perfil(int p) {
  if (p == ovPerfil) return;
  ovCarregar(p);
  ovRev++;
}

void ov_esquecer(void) {
  ovN = 0;
  ovPerfil = perfis_ativo();
  ovRev++;
  ovGravar();   // arquivo vazio: a conta que saiu nao deixa marca para tras
}

typedef struct {
  char id[96];
  char titulo[128];
} CfFixa;

static CfFixa cfFixas[CF_MAX];
static int  cfN;
static int  cfPerfil = -1;
static unsigned cfRev;

static void cfNome(char *dst, size_t tam, int p) {
  if (p <= 1) snprintf(dst, tam, "colfixada.txt");
  else        snprintf(dst, tam, "colfixada-p%d.txt", p);
}

static void cfCarregar(int p) {
  char nome[64], *txt, *s;
  cfPerfil = p;
  cfN = 0;
  cfNome(nome, sizeof nome, p);
  txt = dados_ler(nome);
  if (!txt) return;
  s = txt;
  while (*s && cfN < CF_MAX) {
    char *tab, *fim;
    size_t nId, nTi;
    while (*s == '\n' || *s == '\r') s++;
    if (!*s) break;
    fim = strchr(s, '\n');
    if (!fim) fim = s + strlen(s);
    tab = strchr(s, '\t');
    if (tab && tab < fim) {
      nId = (size_t)(tab - s);
      nTi = (size_t)(fim - tab - 1);
      while (nTi && (tab[1 + nTi - 1] == '\r')) nTi--;
    } else {
      nId = (size_t)(fim - s);
      nTi = 0;
    }
    if (nId >= sizeof cfFixas[0].id) nId = sizeof cfFixas[0].id - 1;
    if (nTi >= sizeof cfFixas[0].titulo) nTi = sizeof cfFixas[0].titulo - 1;
    if (nId) {
      memcpy(cfFixas[cfN].id, s, nId); cfFixas[cfN].id[nId] = 0;
      memcpy(cfFixas[cfN].titulo, tab ? tab + 1 : "", nTi); cfFixas[cfN].titulo[nTi] = 0;
      cfN++;
    }
    s = *fim ? fim + 1 : fim;
  }
  free(txt);
}

static void cfGarantir(void) {
  if (cfPerfil < 0) cfCarregar(perfis_ativo());
}

static int cfIndice(const char *pastaId) {
  int i;
  if (!pastaId || !pastaId[0]) return -1;
  for (i = 0; i < cfN; i++) if (!strcmp(cfFixas[i].id, pastaId)) return i;
  return -1;
}

static void cfGravar(void) {
  char nome[64];
  char buf[CF_MAX * (96 + 1 + 128 + 1)] = "";
  size_t uso = 0;
  int i;
  cfNome(nome, sizeof nome, perfis_ativo());
  for (i = 0; i < cfN; i++)
    uso += (size_t)snprintf(buf + uso, sizeof buf - uso, "%s\t%s\n",
                            cfFixas[i].id, cfFixas[i].titulo);
  dados_gravar(nome, buf);
}

int cf_fixada(const char *pastaId) {
  cfGarantir();
  return cfIndice(pastaId) >= 0;
}

int cf_alternar(const ColFolder *pasta) {
  int i, ligar = 1;
  cfGarantir();
  if (!pasta || !pasta->id[0]) return 0;
  i = cfIndice(pasta->id);
  if (i >= 0) {
    cfN--;
    if (i < cfN) memmove(&cfFixas[i], &cfFixas[i + 1], (size_t)(cfN - i) * sizeof *cfFixas);
    ligar = 0;
  } else {
    if (cfN >= CF_MAX) return 0;
    snprintf(cfFixas[cfN].id, sizeof cfFixas[cfN].id, "%s", pasta->id);
    snprintf(cfFixas[cfN].titulo, sizeof cfFixas[cfN].titulo, "%s", pasta->title);
    cfN++;
  }
  cfRev++;
  cfGravar();
  printf("[colfixada] %s %s\n", ligar ? "fixada" : "desafixada", pasta->id);
  fflush(stdout);
  return ligar;
}

unsigned cf_revisao(void) { return cfRev; }

int cf_n(void) {
  cfGarantir();
  return cfN;
}

// col_n/col_folder vem de colecoes.h (via colfixada.h).

const ColFolder *cf_pasta(int i) {
  int k, n;
  cfGarantir();
  if (i < 0 || i >= cfN) return NULL;
  n = col_n();
  for (k = 0; k < n; k++) {
    const ColFolder *f = col_folder(k);
    if (f && !strcmp(f->id, cfFixas[i].id)) return f;
  }
  return NULL;
}

const char *cf_titulo(int i) {
  const ColFolder *f = cf_pasta(i);
  if (f) return f->title;
  cfGarantir();
  if (i < 0 || i >= cfN) return "";
  return cfFixas[i].titulo;
}

int cf_itens(const ColFolder *pasta, int *indicesCat, int max) {
  int n = 0;
  if (!pasta || !pasta->sources || pasta->nSources < 1 || !indicesCat || max < 1) return 0;
  // So janelas ja carregadas e sem filtros adicionais: nunca faz rede nem
  // apresenta outro genero como se fosse o conteudo da pasta.
  for (int s = 0; s < pasta->nSources; s++) {
    const ColSource *fonte = &pasta->sources[s];
    const char *base = fonte->base[0] ? fonte->base : addons_base_por_id(fonte->addonId);
    const CatFileira *f = NULL;
    if (fonte->prov[0] || fonte->genre[0] || fonte->filtros[0] || !base || !base[0]) return 0;
    for (int r = 0; r < cat_n_fileiras(); r++) {
      const CatFileira *c = cat_fileira(r);
      if (c && !strcmp(c->base, base) && !strcmp(c->tipo, fonte->type) &&
          !strcmp(c->catId, fonte->catId)) { f = c; break; }
    }
    if (!f) return 0;
    for (int c = 0; c < f->n && n < max; c++) {
      int idx = f->ini + c, repetido = 0;
      const CatItem *it = idx >= 0 && idx < cat_n() ? cat_item(idx) : NULL;
      if (!it) continue;
      for (int k = 0; k < n; k++) {
        const CatItem *antes = cat_item(indicesCat[k]);
        if (antes && !strcmp(antes->imdb, it->imdb) && !strcmp(antes->tipo, it->tipo)) {
          repetido = 1; break;
        }
      }
      if (!repetido) indicesCat[n++] = idx;
    }
  }
  return n;
}

void cf_definir_perfil(int p) {
  if (p == cfPerfil) return;
  cfCarregar(p);
  cfRev++;
}

void cf_esquecer(void) {
  cfN = 0;
  cfPerfil = perfis_ativo();
  cfRev++;
  cfGravar();
}

// --- ocultar / mover pela chave ---------------------------------------------
int homefil_indice(const char *chave) {
  int i, n = fil_n();
  if (!chave || !chave[0]) return -1;
  for (i = 0; i < n; i++) if (!strcmp(fil_chave(i), chave)) return i;
  return -1;
}
int homefil_ocultar(const char *chave) {
  int i = homefil_indice(chave);
  if (i < 0) return 0;
  fil_remover(i);
  return 1;
}
int homefil_mover(const char *chave, int dir) {
  int i = homefil_indice(chave), j;
  if (i < 0) return 0;
  j = fil_mover(i, dir);
  return j != i;
}

// --- de onde vem a fileira ---------------------------------------------------
int homefil_origem(const char *chave, int trakt, int simkl, const char *partes[3]) {
  int n = 0, cw;
  if (!chave) return 0;
  cw = !strcmp(chave, "continue_watching");
  if (!cw && strcmp(chave, "upcoming_section")) return 0;
  if (trakt) partes[n++] = "Trakt";
  if (simkl) partes[n++] = "Simkl";
  if (cw) partes[n++] = "Este aparelho";
  return n;
}
