#define _POSIX_C_SOURCE 200809L
#include "fontefalha.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

static FonteFalhaConfig configuracao;
static char diretorio[1024];
static char arquivoAtivo[128] = "fontefalha.txt";
static int iniciada;
static pthread_mutex_t configTrava = PTHREAD_MUTEX_INITIALIZER;
static const char *const filtroChaves[FONTEFILTRO_N] = {
  "filtro_resolucao", "filtro_qualidade", "filtro_visual", "filtro_audio",
  "filtro_canais", "filtro_encode", "filtro_linguas"
};

static int hexDigito(unsigned char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}
static int filtroMascara(const char *texto, uint64_t *saida) {
  size_t n = strlen(texto);
  if (!n || n > 16) return 0;
  uint64_t v = 0;
  for (size_t i = 0; i < n; i++) {
    int d = hexDigito((unsigned char)texto[i]);
    if (d < 0) return 0;
    v = (v << 4) | (unsigned)d;
  }
  *saida = v; return 1;
}
static int filtroMaximo(const char *texto, unsigned *saida) {
  if (!*texto) return 0;
  unsigned v = 0;
  for (const unsigned char *p = (const unsigned char *)texto; *p; p++) {
    if (*p < '0' || *p > '9') return 0;
    v = v * 10 + *p - '0';
    if (v > FONTEFILTRO_MAX_GB) return 0;
  }
  *saida = v; return 1;
}
static int filtroGruposLer(const char *texto, char *saida, size_t n) {
  if (!strcmp(texto, "-")) { saida[0] = 0; return 1; }
  size_t l = strlen(texto);
  if (!l || (l & 1) || l / 2 >= n) return 0;
  char grupos[sizeof ((FonteFiltroConfig *)0)->grupos] = {0};
  for (size_t i = 0; i < l; i += 2) {
    int a = hexDigito((unsigned char)texto[i]), b = hexDigito((unsigned char)texto[i + 1]);
    if (a < 0 || b < 0) return 0;
    unsigned c = (unsigned)(a * 16 + b);
    if (c < 32 || c >= 127) return 0;
    grupos[i / 2] = (char)c;
  }
  return fontefiltro_grupos_normalizar(grupos, saida, (unsigned)n);
}

static void padrao(FonteFalhaConfig *c) {
  memset(c, 0, sizeof *c);
  for (int i = 0; i < 4; i++) { c->ordem[i] = i; c->limites[i] = 3; }
}
static void iniciar(void) {
  pthread_mutex_lock(&configTrava);
  if (!iniciada) { padrao(&configuracao); iniciada = 1; }
  pthread_mutex_unlock(&configTrava);
}
static int limiteVale(int n) { return n >= 1 && n <= FONTEFALHA_MAX_LINKS; }
static int chaveVale(const char *s) {
  if (!s || strlen(s) != 32) return 0;
  for (int i = 0; i < 32; i++) if (!isxdigit((unsigned char)s[i])) return 0;
  return 1;
}
static int ordemVale(const int *o) {
  unsigned v = 0;
  for (int i = 0; i < 4; i++) {
    if (o[i] < 0 || o[i] >= 4 || (v & (1u << o[i]))) return 0;
    v |= 1u << o[i];
  }
  return v == 15;
}
static int addonIndice(const FonteFalhaConfig *c, const char *chave) {
  if (!chave) return -1;
  for (int i = 0; i < c->nAddons; i++) if (!strcmp(c->addon[i].chave, chave)) return i;
  return -1;
}
static int addonAdicionar(FonteFalhaConfig *c, const char *chave) {
  int i = addonIndice(c, chave);
  if (i >= 0) return i;
  if (!chaveVale(chave) || c->nAddons >= FONTEFALHA_MAX_ADDONS) return -1;
  i = c->nAddons++;
  snprintf(c->addon[i].chave, sizeof c->addon[i].chave, "%s", chave);
  c->addon[i].limite = 3;
  c->addon[i].aoVivo = 0;
  return i;
}
static int nomeVale(const char *nome) {
  if (!nome || !*nome || strlen(nome) >= sizeof arquivoAtivo || nome[0] == '.') return 0;
  for (const unsigned char *p = (const unsigned char *)nome; *p; p++)
    if (!isalnum(*p) && *p != '-' && *p != '_' && *p != '.') return 0;
  return 1;
}
static int caminhoNome(char *dst, size_t n, const char *nome, const char *sufixo) {
  if (!diretorio[0] || !nomeVale(nome)) return 0;
  int r = snprintf(dst, n, "%s/%s%s", diretorio, nome, sufixo);
  return r >= 0 && (size_t)r < n;
}
void fontefalha_dir(const char *dir) {
  diretorio[0] = 0;
  if (dir && strlen(dir) < sizeof diretorio) strcpy(diretorio, dir);
  snprintf(arquivoAtivo, sizeof arquivoAtivo, "fontefalha.txt");
  fontefalha_config_ler();
}
static void lerArquivo(const char *arquivo, FonteFalhaConfig *saida) {
  FonteFalhaConfig c;
  char nome[1100], linha[1200], k[40], valor[1100], extra;
  int filtroInvalido = 0;
  padrao(&c);
  FILE *f = caminhoNome(nome, sizeof nome, arquivo, "") ? fopen(nome, "r") : NULL;
  if (f) {
    while (fgets(linha, sizeof linha, f)) {
      int a, b, d, e;
      // Nao deixar o restante de uma linha enorme virar uma nova preferencia.
      if (!strchr(linha, '\n') && !feof(f)) {
        int ch; while ((ch = fgetc(f)) != '\n' && ch != EOF) {}
        if (!strncmp(linha, "filtro_", 7)) filtroInvalido = 1;
        continue;
      }
      if (sscanf(linha, "%39s", k) == 1 && !strncmp(k, "filtro_", 7)) {
        int categoria = -1;
        for (int i = 0; i < FONTEFILTRO_N; i++) if (!strcmp(k, filtroChaves[i])) categoria = i;
        if (categoria >= 0 || !strcmp(k, "filtro_maxgb") || !strcmp(k, "filtro_grupos")) {
          int ok = sscanf(linha, "%39s %1099s %c", k, valor, &extra) == 2;
          if (ok && categoria >= 0) ok = filtroMascara(valor, &c.filtro.excluir[categoria]);
          else if (ok && !strcmp(k, "filtro_maxgb")) ok = filtroMaximo(valor, &c.filtro.maxGB);
          else if (ok) ok = filtroGruposLer(valor, c.filtro.grupos, sizeof c.filtro.grupos);
          if (!ok) filtroInvalido = 1;
        }
        continue;
      }
      if (sscanf(linha, "provedores %d %c", &a, &extra) == 1 && (a == 0 || a == 1)) c.provedores = a;
      else if (sscanf(linha, "addons %d %c", &a, &extra) == 1 && (a == 0 || a == 1)) c.addons = a;
      else if (sscanf(linha, "ordem %d %d %d %d %c", &a, &b, &d, &e, &extra) == 4) {
        int o[4] = {a, b, d, e}; if (ordemVale(o)) memcpy(c.ordem, o, sizeof o);
      } else if (sscanf(linha, "limites %d %d %d %d %c", &a, &b, &d, &e, &extra) == 4) {
        int l[4] = {a, b, d, e};
        if (limiteVale(a) && limiteVale(b) && limiteVale(d) && limiteVale(e)) memcpy(c.limites, l, sizeof l);
      } else if (sscanf(linha, "addon %39s %d %c", k, &a, &extra) == 2 && limiteVale(a)) {
        int i = addonAdicionar(&c, k); if (i >= 0) c.addon[i].limite = a;
      } else if (sscanf(linha, "addon_live %39s %d %c", k, &a, &extra) == 2 && (a == 0 || a == 1)) {
        int i = addonAdicionar(&c, k); if (i >= 0) c.addon[i].aoVivo = a;
      }
    }
    fclose(f);
  }
  // Filtros invalidos nao apagam a ordem/limites desta copia.
  if (filtroInvalido || !fontefiltro_config_validar(&c.filtro)) memset(&c.filtro, 0, sizeof c.filtro);
  *saida = c;
}
void fontefalha_config_ler(void) {
  FonteFalhaConfig c;
  lerArquivo(arquivoAtivo, &c);
  pthread_mutex_lock(&configTrava);
  configuracao = c; iniciada = 1;
  pthread_mutex_unlock(&configTrava);
}
static int gravarArquivo(const FonteFalhaConfig *c, const char *arquivo) {
  char nome[1100], temp[1120];
  if (!caminhoNome(nome, sizeof nome, arquivo, "") ||
      !caminhoNome(temp, sizeof temp, arquivo, ".tmp.XXXXXX")) return 0;
  int fd = mkstemp(temp);
  if (fd < 0) return 0;
  FILE *f = fdopen(fd, "w");
  if (!f) { close(fd); unlink(temp); return 0; }
  int ok = fprintf(f, "versao 1\nprovedores %d\naddons %d\nordem %d %d %d %d\nlimites %d %d %d %d\n",
                   c->provedores, c->addons, c->ordem[0], c->ordem[1], c->ordem[2], c->ordem[3],
                   c->limites[0], c->limites[1], c->limites[2], c->limites[3]) >= 0;
  for (int i = 0; i < c->nAddons && ok; i++)
    ok = fprintf(f, "addon %s %d\n", c->addon[i].chave, c->addon[i].limite) >= 0;
  for (int i = 0; i < c->nAddons && ok; i++)
    if (c->addon[i].aoVivo) ok = fprintf(f, "addon_live %s 1\n", c->addon[i].chave) >= 0;
  for (int i = 0; i < FONTEFILTRO_N && ok; i++)
    ok = fprintf(f, "%s %016" PRIx64 "\n", filtroChaves[i], c->filtro.excluir[i]) >= 0;
  if (ok) ok = fprintf(f, "filtro_maxgb %u\nfiltro_grupos ", c->filtro.maxGB) >= 0;
  if (ok && !c->filtro.grupos[0]) ok = fputc('-', f) != EOF;
  for (const unsigned char *p = (const unsigned char *)c->filtro.grupos; ok && *p; p++)
    ok = fprintf(f, "%02x", (unsigned)*p) >= 0;
  if (ok) ok = fputc('\n', f) != EOF;
  if (ok) ok = fflush(f) == 0 && fsync(fd) == 0;
  if (fclose(f) != 0) ok = 0;
  if (ok) ok = rename(temp, nome) == 0;
  if (!ok) unlink(temp);
  return ok;
}
int fontefalha_preparar_privado(const char *arquivo) {
  char nome[1100];
  struct stat st;
  FonteFalhaConfig compartilhada;
  if (!caminhoNome(nome, sizeof nome, arquivo, "")) return 0;
  // Uma copia existente, mesmo vazia, pertence a este perfil. Nao substitui-la
  // por causa de erro de leitura ou por uma configuracao de outro perfil.
  if (lstat(nome, &st) == 0) return S_ISREG(st.st_mode);
  if (errno != ENOENT) return 0;
  lerArquivo("fontefalha.txt", &compartilhada);
  return gravarArquivo(&compartilhada, arquivo);
}
int fontefalha_arquivo(const char *arquivo) {
  if (!nomeVale(arquivo) || !diretorio[0]) return 0;
  snprintf(arquivoAtivo, sizeof arquivoAtivo, "%s", arquivo);
  fontefalha_config_ler();
  return 1;
}
static int publicar(const FonteFalhaConfig *c) {
  if (!gravarArquivo(c, arquivoAtivo)) return 0;
  pthread_mutex_lock(&configTrava);
  configuracao = *c;
  pthread_mutex_unlock(&configTrava);
  return 1;
}
void fontefalha_config(FonteFalhaConfig *c) {
  iniciar();
  pthread_mutex_lock(&configTrava);
  if (c) *c = configuracao;
  pthread_mutex_unlock(&configTrava);
}
int fontefalha_ativa(void) { FonteFalhaConfig c; fontefalha_config(&c); return c.provedores || c.addons; }
int fontefalha_provedores(void) { FonteFalhaConfig c; fontefalha_config(&c); return c.provedores; }
int fontefalha_addons(void) { FonteFalhaConfig c; fontefalha_config(&c); return c.addons; }
void fontefalha_filtro(FonteFiltroConfig *c) {
  iniciar();
  pthread_mutex_lock(&configTrava);
  if (c) *c = configuracao.filtro;
  pthread_mutex_unlock(&configTrava);
}
int fontefalha_filtro_ativo(void) {
  FonteFiltroConfig filtro;
  fontefalha_filtro(&filtro);
  for (int i = 0; i < FONTEFILTRO_N; i++) if (filtro.excluir[i]) return 1;
  return filtro.maxGB != 0 || filtro.grupos[0] != 0;
}
int fontefalha_filtro_definir(const FonteFiltroConfig *filtro) {
  iniciar();
  if (!filtro || !memchr(filtro->grupos, 0, sizeof filtro->grupos)) return 0;
  FonteFiltroConfig novo = {0};
  memcpy(novo.excluir, filtro->excluir, sizeof novo.excluir); novo.maxGB = filtro->maxGB;
  if (!fontefiltro_grupos_normalizar(filtro->grupos, novo.grupos, sizeof novo.grupos) ||
      !fontefiltro_config_validar(&novo)) return 0;
  FonteFalhaConfig c; fontefalha_config(&c); c.filtro = novo;
  return publicar(&c);
}
int fontefalha_definir_provedores(int sim) {
  iniciar(); FonteFalhaConfig c; fontefalha_config(&c); c.provedores = !!sim; return publicar(&c);
}
int fontefalha_definir_addons(int sim) {
  iniciar(); FonteFalhaConfig c; fontefalha_config(&c); c.addons = !!sim; return publicar(&c);
}
int fontefalha_provedor_posicao(int s) {
  FonteFalhaConfig c; fontefalha_config(&c);
  for (int i = 0; i < 4; i++) if (c.ordem[i] == s) return i;
  return -1;
}
int fontefalha_provedor_limite(int s) { FonteFalhaConfig c; fontefalha_config(&c); return s >= 0 && s < 4 ? c.limites[s] : 3; }
int fontefalha_provedor_mover(int s, int delta) {
  iniciar(); int i = fontefalha_provedor_posicao(s), j = i + delta;
  if ((delta != -1 && delta != 1) || i < 0 || j < 0 || j >= 4) return 0;
  FonteFalhaConfig c; fontefalha_config(&c);
  c.ordem[i] = c.ordem[j]; c.ordem[j] = s;
  return publicar(&c);
}
int fontefalha_provedor_definir_limite(int s, int l) {
  iniciar(); if (s < 0 || s >= 4 || !limiteVale(l)) return 0;
  FonteFalhaConfig c; fontefalha_config(&c); c.limites[s] = l; return publicar(&c);
}
void fontefalha_addon_chave(const char *base, char *dst, unsigned n) {
  uint64_t a = UINT64_C(14695981039346656037), b = UINT64_C(7809847782465536322);
  if (!dst || !n) return;
  if (!base || !*base) { dst[0] = 0; return; }
  for (const unsigned char *p = (const unsigned char *)base; *p; p++) {
    a = (a ^ *p) * UINT64_C(1099511628211);
    b = (b ^ *p) * UINT64_C(14029467366897019727);
  }
  snprintf(dst, n, "%016llx%016llx", (unsigned long long)a, (unsigned long long)b);
}
void fontefalha_addons_registrar(const char *const *chaves, int n) {
  iniciar(); if (!chaves) return;
  pthread_mutex_lock(&configTrava);
  for (int i = 0; i < n; i++) addonAdicionar(&configuracao, chaves[i]);
  pthread_mutex_unlock(&configTrava);
}
int fontefalha_addon_posicao(const char *chave) { FonteFalhaConfig c; fontefalha_config(&c); return addonIndice(&c, chave); }
int fontefalha_addon_limite(const char *chave) {
  FonteFalhaConfig c; fontefalha_config(&c); int i = addonIndice(&c, chave); return i >= 0 ? c.addon[i].limite : 3;
}
int fontefalha_addon_mover(const char *chave, int delta) {
  FonteFalhaConfig c; fontefalha_config(&c); int i = addonIndice(&c, chave);
  if (!delta || delta <= -c.nAddons || delta >= c.nAddons || i < 0) return 0;
  int j = i + delta;
  if (j < 0 || j >= c.nAddons) return 0;
  FonteFalhaAddon a = c.addon[i]; c.addon[i] = c.addon[j]; c.addon[j] = a;
  return publicar(&c);
}
int fontefalha_addon_definir_limite(const char *chave, int l) {
  iniciar(); if (!limiteVale(l)) return 0;
  FonteFalhaConfig c; fontefalha_config(&c); int i = addonAdicionar(&c, chave);
  if (i < 0) return 0;
  c.addon[i].limite = l; return publicar(&c);
}
int fontefalha_addon_ao_vivo(const char *chave) {
  if (!chave || !chave[0]) return 0;
  iniciar(); pthread_mutex_lock(&configTrava);
  int i = addonIndice(&configuracao, chave);
  int ligado = i >= 0 && configuracao.addon[i].aoVivo;
  pthread_mutex_unlock(&configTrava); return ligado;
}
int fontefalha_addon_definir_ao_vivo(const char *chave, int ligado) {
  FonteFalhaConfig c; fontefalha_config(&c); int i = addonAdicionar(&c, chave);
  if (i < 0) return 0;
  c.addon[i].aoVivo = !!ligado; return publicar(&c);
}

typedef struct { char addon[FONTEFALHA_CHAVE]; int usadas[5]; } UsoAddon;
typedef struct { char *identidade; int servico, addon, descontada, url; } Tentativa;
struct FonteFalhaSessao {
  FonteFalhaConfig cfg;
  unsigned disponiveis;
  int fase, preferido, aberta, diretas, total, usadas[4];
  UsoAddon *uso; int nUso;
  Tentativa *tentada; int nTentada;
  char **urls; int nUrls;
};
static int grupo(int servico) { return servico < 0 ? 4 : servico; }
static int usoIndice(const FonteFalhaSessao *s, const char *chave) {
  for (int i = 0; i < s->nUso; i++) if (!strcmp(s->uso[i].addon, chave)) return i;
  return -1;
}
static int jaTentada(const FonteFalhaSessao *s, const FonteFalhaCandidata *c, int servico) {
  if (!c->identidade || !c->identidade[0]) return 1;
  for (int i = 0; i < s->nTentada; i++)
    if (s->tentada[i].servico == servico && !strcmp(s->tentada[i].identidade, c->identidade)) return 1;
  return 0;
}
static int limiteAddon(const FonteFalhaSessao *s, const char *chave) {
  int i = addonIndice(&s->cfg, chave); return i >= 0 ? s->cfg.addon[i].limite : 3;
}
static int candidataVale(const FonteFalhaSessao *s, const FonteFalhaCandidata *c, int servico) {
  if (c->indice < 0 || !fontefiltro_permite(&s->cfg.filtro, &c->filtro) || jaTentada(s, c, servico)) return 0;
  if (servico >= 0) {
    if (servico >= 4 || !s->cfg.provedores || !c->nativa || !(s->disponiveis & (1u << servico)) ||
        s->usadas[servico] >= s->cfg.limites[servico]) return 0;
  } else {
    if (s->cfg.provedores && c->nativa) return 0;
    if (!s->cfg.addons && (s->aberta || s->diretas)) return 0;
  }
  int i = usoIndice(s, c->addon);
  return !s->cfg.addons || i < 0 || s->uso[i].usadas[grupo(servico)] < limiteAddon(s, c->addon);
}
FonteFalhaSessao *fontefalha_sessao_nova(const FonteFalhaConfig *cfg, unsigned disponiveis, int preferido) {
  FonteFalhaSessao *s = calloc(1, sizeof *s);
  if (!s) return NULL;
  if (cfg) s->cfg = *cfg; else fontefalha_config(&s->cfg);
  if (!ordemVale(s->cfg.ordem) || s->cfg.nAddons < 0 || s->cfg.nAddons > FONTEFALHA_MAX_ADDONS ||
      !fontefiltro_config_validar(&s->cfg.filtro)) {
    free(s); return NULL;
  }
  for (int i = 0; i < 4; i++) if (!limiteVale(s->cfg.limites[i])) { free(s); return NULL; }
  for (int i = 0; i < s->cfg.nAddons; i++) if (!limiteVale(s->cfg.addon[i].limite)) { free(s); return NULL; }
  s->disponiveis = disponiveis & 15;
  s->preferido = preferido;
  if (!s->cfg.provedores) s->fase = 4;
  return s;
}
void fontefalha_sessao_filtro(const FonteFalhaSessao *s, FonteFiltroConfig *saida) {
  if (saida) { if (s) *saida = s->cfg.filtro; else memset(saida, 0, sizeof *saida); }
}
void fontefalha_sessao_liberar(FonteFalhaSessao *s) {
  if (!s) return;
  for (int i = 0; i < s->nTentada; i++) free(s->tentada[i].identidade);
  for (int i = 0; i < s->nUrls; i++) free(s->urls[i]);
  free(s->tentada); free(s->urls); free(s->uso); free(s);
}
FonteFalhaSessao *fontefalha_sessao_clonar(const FonteFalhaSessao *s) {
  if (!s) return NULL;
  FonteFalhaSessao *d = malloc(sizeof *d);
  if (!d) return NULL;
  *d = *s; d->uso = NULL; d->tentada = NULL; d->urls = NULL; d->nTentada = d->nUrls = 0;
  if (s->nUso) {
    d->uso = malloc((size_t)s->nUso * sizeof *d->uso);
    if (!d->uso) goto erro;
    memcpy(d->uso, s->uso, (size_t)s->nUso * sizeof *d->uso);
  }
  if (s->nTentada) {
    d->tentada = calloc((size_t)s->nTentada, sizeof *d->tentada);
    if (!d->tentada) goto erro;
    for (int i = 0; i < s->nTentada; i++) {
      d->tentada[i] = s->tentada[i]; d->tentada[i].identidade = strdup(s->tentada[i].identidade);
      if (!d->tentada[i].identidade) goto erro;
      d->nTentada++;
    }
  }
  if (s->nUrls) {
    d->urls = calloc((size_t)s->nUrls, sizeof *d->urls);
    if (!d->urls) goto erro;
    for (int i = 0; i < s->nUrls; i++) {
      d->urls[i] = strdup(s->urls[i]); if (!d->urls[i]) goto erro; d->nUrls++;
    }
  }
  return d;
erro:
  fontefalha_sessao_liberar(d); return NULL;
}
static int escolher(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int n, int servico) {
  int melhor = -1, rankMelhor = 0;
  for (int i = 0; i < n; i++) {
    if (!candidataVale(s, &c[i], servico)) continue;
    if (!s->nTentada && c[i].indice == s->preferido) return i;
    int rank = s->cfg.addons ? addonIndice(&s->cfg, c[i].addon) : -1;
    if (rank < 0) rank = FONTEFALHA_MAX_ADDONS + (c[i].ordemAddon >= 0 ? c[i].ordemAddon : 0);
    if (melhor < 0 || rank < rankMelhor) { melhor = i; rankMelhor = rank; }
  }
  return melhor;
}
int fontefalha_proxima(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int n, int completa, FonteFalhaRota *r) {
  if (!s || !r || n < 0 || (n && !c)) return FONTEFALHA_FIM;
  // A escolha explicita pode abrir enquanto outros addons respondem. Depois
  // dessa primeira tentativa, esperar a coleta evita gastar o limite num
  // addon de prioridade menor so porque ele respondeu primeiro.
  if (!completa && s->nTentada && s->preferido >= 0) {
    int pode = s->cfg.addons || (!s->aberta && !s->diretas);
    if (s->cfg.provedores) for (int p = 0; p < 4; p++)
      if ((s->disponiveis & (1u << p)) && s->usadas[p] < s->cfg.limites[p]) pode = 1;
    if (pode) return FONTEFALHA_ESPERAR;
  }
  // Uma escolha manual HTTP continua sendo a primeira escolha; a ordem dos
  // resolvedores nativos so se aplica aos torrents que eles podem resolver.
  if (!s->nTentada && s->preferido >= 0) {
    for (int i = 0; i < n; i++) if (c[i].indice == s->preferido && !c[i].nativa && candidataVale(s, &c[i], -1)) {
      *r = (FonteFalhaRota){c[i].indice, -1, i}; return FONTEFALHA_PROXIMA;
    }
  }
  while (s->fase < 4) {
    int p = s->cfg.ordem[s->fase];
    if (!(s->disponiveis & (1u << p)) || s->usadas[p] >= s->cfg.limites[p]) { s->fase++; continue; }
    int i = escolher(s, c, n, p);
    if (i >= 0) { *r = (FonteFalhaRota){c[i].indice, p, i}; return FONTEFALHA_PROXIMA; }
    if (!completa) return FONTEFALHA_ESPERAR;
    s->fase++;
  }
  int i = escolher(s, c, n, -1);
  if (i >= 0) { *r = (FonteFalhaRota){c[i].indice, -1, i}; return FONTEFALHA_PROXIMA; }
  if (!completa && (s->cfg.addons || (!s->aberta && !s->diretas))) return FONTEFALHA_ESPERAR;
  return FONTEFALHA_FIM;
}
int fontefalha_tentar(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int servico) {
  if (!s || !c || servico < -1 || servico >= 4 || !candidataVale(s, c, servico)) return 0;
  char *id = strdup(c->identidade);
  if (!id) return 0;
  int a = usoIndice(s, c->addon);
  if (a < 0) {
    UsoAddon *u = realloc(s->uso, (size_t)(s->nUso + 1) * sizeof *u);
    if (!u) { free(id); return 0; }
    s->uso = u; a = s->nUso++;
    memset(&s->uso[a], 0, sizeof s->uso[a]);
    snprintf(s->uso[a].addon, sizeof s->uso[a].addon, "%s", c->addon);
  }
  Tentativa *t = realloc(s->tentada, (size_t)(s->nTentada + 1) * sizeof *t);
  if (!t) { free(id); return 0; }
  s->tentada = t;
  t[s->nTentada++] = (Tentativa){id, servico, a, 0, -1};
  s->uso[a].usadas[grupo(servico)]++;
  if (servico >= 0) s->usadas[servico]++; else s->diretas++;
  s->total++;
  return 1;
}
int fontefalha_url_registrar(FonteFalhaSessao *s, const char *url) {
  if (!s || !url || !*url || !s->nTentada) return 0;
  for (int i = 0; i < s->nUrls; i++) if (!strcmp(s->urls[i], url)) {
    Tentativa *t = &s->tentada[s->nTentada - 1];
    if (t->url == i) return 1;
    if (!t->descontada) {
      s->uso[t->addon].usadas[grupo(t->servico)]--;
      if (t->servico >= 0) s->usadas[t->servico]--; else s->diretas--;
      s->total--; t->descontada = 1;
    }
    return 0;
  }
  char *copia = strdup(url);
  if (!copia) return 0;
  char **v = realloc(s->urls, (size_t)(s->nUrls + 1) * sizeof *v);
  if (!v) { free(copia); return 0; }
  s->urls = v; s->urls[s->nUrls] = copia;
  s->tentada[s->nTentada - 1].url = s->nUrls++;
  return 1;
}
void fontefalha_provedor_recusado(FonteFalhaSessao *s, int p) { if (s && p >= 0 && p < 4) s->disponiveis &= ~(1u << p); }
int fontefalha_semear(FonteFalhaSessao *s, const FonteFalhaCandidata *c, int p, const char *url) {
  if (!s || !url || !*url || !fontefalha_tentar(s, c, p)) return 0;
  if (!fontefalha_url_registrar(s, url)) return 0;
  if (p >= 0) for (int i = 0; i < 4; i++) if (s->cfg.ordem[i] == p) { s->fase = i; break; }
  fontefalha_marcar_aberta(s);
  return 1;
}
void fontefalha_marcar_aberta(FonteFalhaSessao *s) { if (s) s->aberta = 1; }
int fontefalha_tentativas(const FonteFalhaSessao *s) { return s ? s->total : 0; }
int fontefalha_teto(const FonteFalhaSessao *s) {
  if (!s) return 0;
  int n = 0;
  if (s->cfg.provedores) for (int i = 0; i < 4; i++) if (s->disponiveis & (1u << i)) n += s->cfg.limites[i];
  if (s->cfg.addons) {
    for (int i = 0; i < s->cfg.nAddons; i++) n += s->cfg.addon[i].limite;
    for (int i = 0; i < s->nUso; i++) if (addonIndice(&s->cfg, s->uso[i].addon) < 0) n += 3;
    if (!s->cfg.nAddons && !s->nUso) n += 3;
  } else n++;
  return n < s->total ? s->total : n;
}
