// A legenda escolhida a mao, lembrada por perfil. Ver a nota em legmemoria.h
// (o defeito do log da TCL, o que se guarda e a ordem da decisao).
#include "legmemoria.h"
#include "linguas.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static LegMem tabela[LEGMEM_MAX];
static int nTab, carregado, perfil;
static char ultima[16];

// Ate onde legmem_esquecer varre (CONTA_PERFIL_MAX e 8; ver fontepref.c).
#define LEGMEM_PERFIS 16

static const char *arquivoDoPerfil(void) {
  static char nome[48];
  snprintf(nome, sizeof nome, "legmemoria-p%d.txt", perfil);
  return nome;
}

// Byte de controle no titulo/nome quebraria a linha do arquivo (o TAB e um deles).
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

// Formato: a primeira linha util e "*\t<ultima>"; depois uma por titulo,
// "titulo\tquando\tidioma\ttipo\tnumero\tid\tnome" (nome por ultimo: e o
// unico texto livre). Tipo "-" = nenhuma.
static void gravar(void) {
  size_t cap = (size_t)LEGMEM_MAX * 200u + 64u, k = 0;
  char *buf = (char *)malloc(cap);
  int i;
  if (!buf) return;
  k += (size_t)snprintf(buf + k, cap - k, "# nuvio legendas v1\n*\t%s\n", ultima);
  for (i = 0; i < nTab && k + 1 < cap; i++) {
    const LegMem *m = &tabela[i];
    k += (size_t)snprintf(buf + k, cap - k, "%s\t%lld\t%s\t%c\t%d\t%s\t%s\n", m->titulo, m->quandoS,
                          m->idioma, m->tipo ? m->tipo : '-', m->numero, m->id, m->nome);
  }
  dados_gravar(arquivoDoPerfil(), buf);
  free(buf);
}

static void carregar(void) {
  char *b, *linha, *prox;
  if (carregado) return;
  carregado = 1;
  nTab = 0; ultima[0] = 0;
  b = dados_ler(arquivoDoPerfil());
  if (!b) return;
  for (linha = b; linha && *linha && nTab < LEGMEM_MAX; linha = prox) {
    char *p, *tit, *quando, *idi, *tipo, *num, *id;
    char *fim = strchr(linha, '\n');
    prox = fim ? fim + 1 : NULL;
    if (fim) *fim = 0;
    if (linha[0] == '#' || !linha[0]) continue;
    p = linha;
    tit = campo(&p);
    if (!strcmp(tit, "*")) { snprintf(ultima, sizeof ultima, "%s", campo(&p)); continue; }
    quando = campo(&p); idi = campo(&p); tipo = campo(&p); num = campo(&p); id = campo(&p);
    if (!tit[0]) continue;
    { LegMem *m = &tabela[nTab++];
      memset(m, 0, sizeof *m);
      snprintf(m->titulo, sizeof m->titulo, "%s", tit);
      snprintf(m->idioma, sizeof m->idioma, "%s", idi);
      m->tipo = (tipo[0] == 'e' || tipo[0] == 'a') ? tipo[0] : 0;
      m->numero = atoi(num);
      snprintf(m->id, sizeof m->id, "%s", id);
      snprintf(m->nome, sizeof m->nome, "%s", p ? p : "");
      m->quandoS = atoll(quando); }
  }
  free(b);
  printf("[legenda] %d escolha(s) lembrada(s) no perfil %d, ultima '%s'\n", nTab, perfil, ultima);
  fflush(stdout);
}

void legmem_definir_perfil(int p) {
  if (p < 0) p = 0;
  if (p == perfil && carregado) return;
  perfil = p;
  nTab = 0; ultima[0] = 0;
  carregado = 0;
}

void legmem_esquecer(void) {
  int p;
  for (p = 0; p <= LEGMEM_PERFIS; p++) {
    char nome[48];
    snprintf(nome, sizeof nome, "legmemoria-p%d.txt", p);
    dados_apagar(nome);
  }
  nTab = 0; ultima[0] = 0;
  carregado = 1;
}

void legmem_id_titulo(const char *id, char *dst, unsigned tam) {
  const char *c;
  size_t n;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!id) return;
  n = strlen(id);
  // So o id do IMDb leva ":temporada:episodio"; um NOME pode ter ":" de verdade.
  if (!strncmp(id, "tt", 2) && (c = strchr(id, ':')) != NULL) n = (size_t)(c - id);
  if (n >= tam) n = tam - 1;
  memcpy(dst, id, n);
  dst[n] = 0;
  limpar(dst);
}

void legmem_guardar(const LegMem *e) {
  char tit[64];
  int i;
  LegMem novo;
  if (!e) return;
  carregar();
  legmem_id_titulo(e->titulo, tit, sizeof tit);
  // Idioma conhecido (ou "none") vira a ultima escolha. Faixa sem etiqueta so
  // fica no titulo: ela nao diz que idioma a pessoa quer no proximo.
  if (e->idioma[0]) snprintf(ultima, sizeof ultima, "%s", e->idioma);
  if (tit[0]) {
    novo = *e;
    snprintf(novo.titulo, sizeof novo.titulo, "%s", tit);
    limpar(novo.nome); limpar(novo.idioma); limpar(novo.id);
    novo.quandoS = (long long)time(NULL);
    // A escolha deste titulo vai para o FIM (a mais nova); cheia, sai a primeira.
    for (i = 0; i < nTab; i++)
      if (!strcmp(tabela[i].titulo, tit)) {
        memmove(&tabela[i], &tabela[i + 1], sizeof *tabela * (size_t)(nTab - i - 1));
        nTab--;
        break;
      }
    if (nTab >= LEGMEM_MAX) {
      memmove(&tabela[0], &tabela[1], sizeof *tabela * (size_t)(LEGMEM_MAX - 1));
      nTab = LEGMEM_MAX - 1;
    }
    tabela[nTab++] = novo;
  }
  gravar();
}

const char *legmem_ultima(void) { carregar(); return ultima; }

const LegMem *legmem_do_titulo(const char *titulo) {
  char tit[64];
  int i;
  carregar();
  legmem_id_titulo(titulo, tit, sizeof tit);
  if (!tit[0]) return NULL;
  for (i = nTab - 1; i >= 0; i--)
    if (!strcmp(tabela[i].titulo, tit)) return &tabela[i];
  return NULL;
}

static int ehIdioma(const char *s) { return s && s[0] && strcasecmp(s, "none"); }

const char *legmem_preferencia(const char *ajuste, const LegMem *doTitulo,
                               const char *ult, int *origem) {
  int o = LEGMEM_DE_NADA;
  const char *r = ajuste ? ajuste : "";
  if (doTitulo && (doTitulo->idioma[0] || doTitulo->tipo)) {
    o = LEGMEM_DE_TITULO; r = doTitulo->idioma;
  } else if (ehIdioma(ajuste)) {
    o = LEGMEM_DE_AJUSTE;
  } else if (ult && ult[0]) {
    // "Nenhuma" de Ajustes perde para a escolha a mao: ela e mais nova, e a
    // regra do dono e "quem usa legenda, tem que vir ligada".
    o = LEGMEM_DE_ULTIMA; r = ult;
  } else if (r[0]) {
    o = LEGMEM_DE_AJUSTE;
  }
  if (origem) *origem = o;
  return r;
}

int legmem_exata(const LegMem *m,
                 const int *numeros, const char *const *idiomas, const char *const *nomes, int nEmb,
                 const char *const *idsAdd, int nAdd) {
  int i;
  if (!m) return -1;
  if (m->tipo == 'e') {
    for (i = 0; i < nEmb; i++) {
      const char *idi = idiomas && idiomas[i] ? idiomas[i] : "";
      if (!numeros || numeros[i] != m->numero) continue;
      if (m->idioma[0] && idi[0] && ling_casa(idi, m->idioma)) return i;
      if (!m->idioma[0] && !idi[0] && nomes && nomes[i] && !strcmp(nomes[i], m->nome)) return i;
    }
  } else if (m->tipo == 'a' && m->id[0]) {
    for (i = 0; i < nAdd; i++)
      if (idsAdd && idsAdd[i] && !strcmp(idsAdd[i], m->id)) return nEmb + i;
  }
  return -1;
}
