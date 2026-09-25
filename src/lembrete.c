// Lembretes de programa do guia — ver lembrete.h.
#include "lembrete.h"
#include "dados.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static Lembrete lista[LEMBRETE_MAX];
static int      n, perfilCarregado = -1;

static void nomeArquivo(char *dst, size_t tam, int perfil) {
  snprintf(dst, tam, "guia-lembretes-p%d.txt", perfil > 0 ? perfil : 1);
}

// Tabulacao e quebra de linha sao os separadores do arquivo: nao podem entrar
// nos campos (titulo de XMLTV pode trazer qualquer coisa).
static void copiarCampo(char *dst, size_t tam, const char *src) {
  size_t k = 0;
  if (!src) src = "";
  for (; *src && k + 1 < tam; src++)
    dst[k++] = (*src == '\t' || *src == '\n' || *src == '\r') ? ' ' : *src;
  dst[k] = 0;
}

size_t lembrete_texto(char *dst, size_t tam) {
  size_t k = 0;
  int i;
  if (!dst || !tam) return 0;
  dst[0] = 0;
  for (i = 0; i < n && k + 1 < tam; i++) {
    int w = snprintf(dst + k, tam - k, "%lld\t%lld\t%d\t%s\t%s\t%s\t%s\n",
                     (long long)lista[i].ini, (long long)lista[i].fim, lista[i].avisado,
                     lista[i].canal, lista[i].nome, lista[i].titulo, lista[i].base);
    if (w < 0 || (size_t)w >= tam - k) { dst[k] = 0; break; }
    k += (size_t)w;
  }
  return k;
}

int lembrete_de_texto(const char *t) {
  const char *p = t;
  n = 0;
  while (p && *p && n < LEMBRETE_MAX) {
    const char *fimLinha = strchr(p, '\n');
    char linha[1200], *campo[7];
    size_t len = fimLinha ? (size_t)(fimLinha - p) : strlen(p);
    int k = 0;
    char *q;
    if (len >= sizeof linha) len = sizeof linha - 1;
    memcpy(linha, p, len); linha[len] = 0;
    p = fimLinha ? fimLinha + 1 : p + len;
    q = linha;
    while (k < 7) {
      campo[k++] = q;
      q = strchr(q, '\t');
      if (!q) break;
      *q++ = 0;
    }
    if (k < 6) continue;   // linha quebrada: pula, nao derruba a lista
    { Lembrete *l = &lista[n];
      memset(l, 0, sizeof *l);
      l->ini = (time_t)strtoll(campo[0], NULL, 10);
      l->fim = (time_t)strtoll(campo[1], NULL, 10);
      l->avisado = atoi(campo[2]);
      copiarCampo(l->canal, sizeof l->canal, campo[3]);
      copiarCampo(l->nome, sizeof l->nome, campo[4]);
      copiarCampo(l->titulo, sizeof l->titulo, campo[5]);
      copiarCampo(l->base, sizeof l->base, k > 6 ? campo[6] : "");
      if (l->canal[0] && l->ini > 0 && l->fim > l->ini) n++; }
  }
  return n;
}

static void gravar(void) {
  char nome[48];
  // 64 lembretes x ~1 KB no pior caso (base de 600): cabe folgado.
  static char buf[LEMBRETE_MAX * 1100];
  if (perfilCarregado < 0) return;
  nomeArquivo(nome, sizeof nome, perfilCarregado);
  lembrete_texto(buf, sizeof buf);
  dados_gravar(nome, buf);
}

void lembrete_carregar(int perfil) {
  char nome[48], *t;
  n = 0;
  perfilCarregado = perfil;
  nomeArquivo(nome, sizeof nome, perfil);
  t = dados_ler(nome);
  if (!t) return;
  lembrete_de_texto(t);
  free(t);
}

int lembrete_perfil(void) { return perfilCarregado; }
int lembrete_n(void) { return n; }
const Lembrete *lembrete_item(int i) { return (i >= 0 && i < n) ? &lista[i] : NULL; }

int lembrete_achar(const char *canal, time_t ini, const char *titulo) {
  int i;
  if (!n || !canal || !titulo) return -1;
  for (i = 0; i < n; i++) {
    time_t d = lista[i].ini > ini ? lista[i].ini - ini : ini - lista[i].ini;
    if (d <= LEMBRETE_TOLERANCIA_S && !strcmp(lista[i].canal, canal) &&
        !strcmp(lista[i].titulo, titulo))
      return i;
  }
  return -1;
}

int lembrete_alternar(const char *canal, const char *nome, const char *titulo,
                      const char *base, time_t ini, time_t fim) {
  int i = lembrete_achar(canal, ini, titulo);
  if (i >= 0) {
    memmove(&lista[i], &lista[i + 1], sizeof(Lembrete) * (size_t)(n - i - 1));
    n--;
    gravar();
    return 0;
  }
  if (n >= LEMBRETE_MAX) return -1;
  { Lembrete *l = &lista[n];
    memset(l, 0, sizeof *l);
    copiarCampo(l->canal, sizeof l->canal, canal);
    copiarCampo(l->nome, sizeof l->nome, nome);
    copiarCampo(l->titulo, sizeof l->titulo, titulo);
    copiarCampo(l->base, sizeof l->base, base);
    l->ini = ini; l->fim = fim > ini ? fim : ini + 60;
    n++; }
  gravar();
  return 1;
}

void lembrete_ajustar(int i, time_t ini, time_t fim) {
  if (i < 0 || i >= n) return;
  if (lista[i].ini == ini && lista[i].fim == fim) return;
  lista[i].ini = ini;
  lista[i].fim = fim > ini ? fim : ini + 60;
  gravar();
}

int lembrete_podar(time_t agora) {
  int i, w = 0, tirou;
  for (i = 0; i < n; i++)
    if (lista[i].fim > agora) { if (w != i) lista[w] = lista[i]; w++; }
  tirou = n - w;
  n = w;
  if (tirou) gravar();
  return tirou;
}

int lembrete_vencido(time_t agora) {
  int i;
  for (i = 0; i < n; i++)
    if (!lista[i].avisado && agora >= lista[i].ini - LEMBRETE_ANTECEDE_S &&
        agora < lista[i].fim)
      return i;
  return -1;
}

void lembrete_marcar_avisado(int i) {
  if (i < 0 || i >= n || lista[i].avisado) return;
  lista[i].avisado = 1;
  gravar();
}

void lembrete_esquecer_todos(void) {
  int p;
  for (p = 1; p <= 8; p++) {
    char nome[48];
    nomeArquivo(nome, sizeof nome, p);
    dados_apagar(nome);
  }
  n = 0; perfilCarregado = -1;
}
