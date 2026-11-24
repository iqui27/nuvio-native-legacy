// FAROLCONSENT — consentimento explicito por perfil para o compositor LLM
// (issue 53). Sem SDL, sem rede, sem _Thread_local, sem log de titulo/perfil.
#include "farolconsent.h"
#include <ctype.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define FC_PERFIL_MAX 8

// Ganchos fracos dos modulos do app: na build completa as definicoes fortes
// vencem; em testes isolados os padroes seguros permitem linkar sem SDL.
__attribute__((weak)) int  perfis_ativo(void) { return 1; }
__attribute__((weak)) int  perfil_infantil_ativo(void) { return 0; }
__attribute__((weak)) int  perfil_infantil(int idx) { (void)idx; return 0; }
__attribute__((weak)) int  perfil_ajustes_travados(int idx) { (void)idx; return 0; }
__attribute__((weak)) int  farolsinal_recomendar(void) { return 1; }
__attribute__((weak)) void farolcompor_definir_consentimento(int consentiu) { (void)consentiu; }

__attribute__((weak)) const char *dados_dir(void) { return ""; }
__attribute__((weak)) char *dados_ler(const char *nome) { (void)nome; return NULL; }
__attribute__((weak)) int   dados_gravar(const char *nome, const char *conteudo) {
  (void)nome; (void)conteudo; return 0;
}
__attribute__((weak)) int   dados_apagar(const char *nome) { (void)nome; return 0; }
__attribute__((weak)) void  dados_fs_travar(void) {}
__attribute__((weak)) void  dados_fs_liberar(void) {}
__attribute__((weak)) void  dados_marcar_sujo(int leve) { (void)leve; }

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int  perfilAtivo;
static int  lastPerfisAtivo;
static int  estadoAtivo = FAROL_CONSENT_NAO_PERGUNTADO;
static char pastaOverride[512];

static int perfilValido(int p) {
  return (p >= 1 && p <= FC_PERFIL_MAX) ? p : 1;
}

static void nomeArquivo(int perfil, char *dst, size_t cap) {
  snprintf(dst, cap, "farol-consent-p%d.txt", perfilValido(perfil));
}

static int pastaEfetiva(char *dst, size_t cap, int *usaDados) {
  const char *d;
  if (usaDados) *usaDados = 0;
  pthread_mutex_lock(&trava);
  if (pastaOverride[0]) {
    snprintf(dst, cap, "%s", pastaOverride);
    pthread_mutex_unlock(&trava);
    return 1;
  }
  pthread_mutex_unlock(&trava);
  d = dados_dir();
  if (d && d[0]) {
    snprintf(dst, cap, "%s", d);
    if (usaDados) *usaDados = 1;
    return 1;
  }
  dst[0] = '\0';
  return 0;
}

static char *lojaLer(const char *nome) {
  char dir[512];
  int usaDados = 0;
  if (pastaEfetiva(dir, sizeof dir, &usaDados)) {
    char p[640];
    FILE *f;
    long n;
    char *b;
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    f = fopen(p, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    n = ftell(f);
    if (n < 0 || n > 4096L) { fclose(f); return NULL; }
    rewind(f);
    b = (char *)malloc((size_t)n + 1);
    if (!b || (n > 0 && fread(b, 1, (size_t)n, f) != (size_t)n)) {
      free(b);
      fclose(f);
      return NULL;
    }
    b[n] = '\0';
    fclose(f);
    return b;
  }
  return dados_ler(nome);
}

static int lojaGravar(const char *nome, const char *txt) {
  char dir[512];
  int usaDados = 0;
  if (pastaEfetiva(dir, sizeof dir, &usaDados)) {
    char p[640], pt[660];
    FILE *f;
    size_t n = strlen(txt);
    int ok;
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    snprintf(pt, sizeof pt, "%s/%s.tmp", dir, nome);
    if (usaDados) dados_fs_travar();
    f = fopen(pt, "wb");
    if (!f) {
      if (usaDados) dados_fs_liberar();
      return 0;
    }
    ok = (n == 0) || (fwrite(txt, 1, n, f) == n);
    if (fclose(f) != 0 || !ok) {
      unlink(pt);
      if (usaDados) dados_fs_liberar();
      return 0;
    }
    if (rename(pt, p) != 0) {
      unlink(pt);
      if (usaDados) dados_fs_liberar();
      return 0;
    }
    if (usaDados) {
      dados_marcar_sujo(0);
      dados_fs_liberar();
    }
    return 1;
  }
  return dados_gravar(nome, txt);
}

static void lojaApagar(const char *nome) {
  char dir[512];
  int usaDados = 0;
  if (pastaEfetiva(dir, sizeof dir, &usaDados)) {
    char p[640];
    snprintf(p, sizeof p, "%s/%s", dir, nome);
    if (usaDados) dados_fs_travar();
    if (unlink(p) == 0 && usaDados) dados_marcar_sujo(0);
    if (usaDados) dados_fs_liberar();
    return;
  }
  (void)dados_apagar(nome);
}

static int parseEstado(const char *txt) {
  int val = -1, pos = -1;
  if (!txt || !txt[0]) return FAROL_CONSENT_NAO_PERGUNTADO;
  if (sscanf(txt, "FC_CONSENT 1\nestado=%d%n", &val, &pos) != 1 || pos <= 0)
    return FAROL_CONSENT_NAO_PERGUNTADO;
  while (txt[pos] == '\r' || txt[pos] == '\n' || txt[pos] == ' ' || txt[pos] == '\t')
    pos++;
  if (txt[pos] != '\0') return FAROL_CONSENT_NAO_PERGUNTADO;
  if (val != FAROL_CONSENT_NAO_PERGUNTADO &&
      val != FAROL_CONSENT_SO_TV &&
      val != FAROL_CONSENT_SIM)
    return FAROL_CONSENT_NAO_PERGUNTADO;
  return val;
}

static int lerDiscoPerfil(int perfil) {
  char nome[64];
  char *txt;
  int st;
  nomeArquivo(perfil, nome, sizeof nome);
  txt = lojaLer(nome);
  if (!txt) return FAROL_CONSENT_NAO_PERGUNTADO;
  st = parseEstado(txt);
  free(txt);
  return st;
}

static int infantilEfetivo(int p) {
  int hook = perfilValido(perfis_ativo());
  if (p == hook && perfil_infantil_ativo()) return 1;
  if (perfil_infantil(p)) return 1;
  return 0;
}

static int travadoEfetivo(int p) {
  return perfil_ajustes_travados(p) != 0;
}

static int garantirCarregado(int *outPerfil) {
  int hook = perfilValido(perfis_ativo());
  int precisa = 0;
  int p, st;
  pthread_mutex_lock(&trava);
  if (perfilAtivo == 0 || hook != lastPerfisAtivo) {
    precisa = 1;
    p = hook;
  } else {
    p = perfilAtivo;
    st = estadoAtivo;
  }
  pthread_mutex_unlock(&trava);
  if (precisa) {
    farolconsent_trocar_perfil(p);
    pthread_mutex_lock(&trava);
    p = perfilAtivo;
    st = estadoAtivo;
    pthread_mutex_unlock(&trava);
  }
  if (outPerfil) *outPerfil = p;
  return st;
}

int farolconsent_presente(void) { return 1; }

void farolconsent_trocar_perfil(int perfil) {
  int p = perfilValido(perfil);
  int hook = perfilValido(perfis_ativo());
  int st = lerDiscoPerfil(p);
  int comporOn;
  pthread_mutex_lock(&trava);
  perfilAtivo = p;
  lastPerfisAtivo = hook;
  estadoAtivo = st;
  pthread_mutex_unlock(&trava);
  comporOn = (st == FAROL_CONSENT_SIM) && !infantilEfetivo(p) && !travadoEfetivo(p);
  farolcompor_definir_consentimento(comporOn);
}

void farolconsent_sincronizar_ativo(void) {
  (void)garantirCarregado(NULL);
}

void farolconsent_invalidar(void) {
  pthread_mutex_lock(&trava);
  perfilAtivo = 0;
  lastPerfisAtivo = 0;
  estadoAtivo = FAROL_CONSENT_NAO_PERGUNTADO;
  pthread_mutex_unlock(&trava);
}

int farolconsent_estado(void) {
  return garantirCarregado(NULL);
}

int farolconsent_estado_perfil(int perfil) {
  int p = perfilValido(perfil);
  int cur, st;
  st = garantirCarregado(&cur);
  if (p == cur) return st;
  return lerDiscoPerfil(p);
}

int farolconsent_llm_ativo(void) {
  int p = 1;
  int st = garantirCarregado(&p);
  if (st != FAROL_CONSENT_SIM) return 0;
  if (infantilEfetivo(p) || travadoEfetivo(p)) return 0;
  if (!farolsinal_recomendar()) return 0;
  return 1;
}

int farolconsent_deve_perguntar(void) {
  int p = 1;
  int st = garantirCarregado(&p);
  if (st != FAROL_CONSENT_NAO_PERGUNTADO) return 0;
  if (infantilEfetivo(p) || travadoEfetivo(p)) return 0;
  if (!farolsinal_recomendar()) return 0;
  return 1;
}

int farolconsent_definir_perfil(int perfil, int estado) {
  int p = perfilValido(perfil);
  int hook = perfilValido(perfis_ativo());
  char nome[64], buf[64];
  int cur, comporOn;

  if (estado != FAROL_CONSENT_NAO_PERGUNTADO &&
      estado != FAROL_CONSENT_SO_TV &&
      estado != FAROL_CONSENT_SIM)
    return 0;
  if (infantilEfetivo(p) || travadoEfetivo(p)) return 0;

  nomeArquivo(p, nome, sizeof nome);
  snprintf(buf, sizeof buf, "FC_CONSENT 1\nestado=%d\n", estado);
  if (!lojaGravar(nome, buf)) return 0;

  pthread_mutex_lock(&trava);
  if (perfilAtivo == 0) {
    perfilAtivo = p;
    lastPerfisAtivo = hook;
  }
  cur = perfilAtivo;
  if (p == cur) estadoAtivo = estado;
  pthread_mutex_unlock(&trava);

  if (p == cur) {
    comporOn = (estado == FAROL_CONSENT_SIM) && !infantilEfetivo(p) && !travadoEfetivo(p);
    farolcompor_definir_consentimento(comporOn);
  }
  return 1;
}

int farolconsent_definir(int estado) {
  int p = 1;
  (void)garantirCarregado(&p);
  return farolconsent_definir_perfil(p, estado);
}

void farolconsent_esquecer(void) {
  int p;
  for (p = 1; p <= FC_PERFIL_MAX; p++) {
    char nome[64];
    nomeArquivo(p, nome, sizeof nome);
    lojaApagar(nome);
  }
  pthread_mutex_lock(&trava);
  perfilAtivo = 0;
  lastPerfisAtivo = 0;
  estadoAtivo = FAROL_CONSENT_NAO_PERGUNTADO;
  pthread_mutex_unlock(&trava);
  farolcompor_definir_consentimento(0);
}

// --- Fonte unica da verdade ("Vai junto / Nunca vai" == farolcompor.c) ---

static const char *const VAI_JUNTO[FAROLCONSENT_VAI_N] = {
  FAROLCONSENT_VAI_IDIOMA,
  FAROLCONSENT_VAI_HORA,
  FAROLCONSENT_VAI_GOSTO,
  FAROLCONSENT_VAI_CANDIDATOS,
  FAROLCONSENT_VAI_ANCORAS,
  FAROLCONSENT_VAI_FRASE
};

static const char *const NUNCA_VAI[FAROLCONSENT_NUNCA_N] = {
  FAROLCONSENT_NUNCA_DIARIO,
  FAROLCONSENT_NUNCA_NOTAS,
  FAROLCONSENT_NUNCA_PERFIS,
  FAROLCONSENT_NUNCA_AMIGOS,
  FAROLCONSENT_NUNCA_ADDONS,
  FAROLCONSENT_NUNCA_VETOR,
  FAROLCONSENT_NUNCA_INFANTIL
};

// Cada campo/prefixo real emitido por farolcompor_montar_corpo_mapa() em
// src/farolcompor.c mapeia para uma linha visivel de VAI_JUNTO[].
static const FarolConsentGrupo ESPELHO_GRUPOS[] = {
  { "v",                           FAROLCONSENT_VAI_IDIOMA     },
  { "modo",                        FAROLCONSENT_VAI_IDIOMA     },
  { "idioma",                      FAROLCONSENT_VAI_IDIOMA     },
  { "infantil",                    FAROLCONSENT_VAI_IDIOMA     },
  { "consentiu",                   FAROLCONSENT_VAI_IDIOMA     },
  { "hora",                        FAROLCONSENT_VAI_HORA       },
  { "gosto.fim_semana",            FAROLCONSENT_VAI_HORA       },
  { "gosto.nivel",                 FAROLCONSENT_VAI_GOSTO      },
  { "gosto.generos",               FAROLCONSENT_VAI_GOSTO      },
  { "gosto.temas",                 FAROLCONSENT_VAI_GOSTO      },
  { "gosto.pessoas",               FAROLCONSENT_VAI_GOSTO      },
  { "gosto.ancoras",               FAROLCONSENT_VAI_ANCORAS    },
  { "ancoras",                     FAROLCONSENT_VAI_ANCORAS    },
  { "candidatos[].c",              FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].id",             FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].tipo",           FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].titulo",         FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].ano",            FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].generos",        FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].minutos",        FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].nota",           FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].pontos",         FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].motivos",        FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos[].eixos",          FAROLCONSENT_VAI_CANDIDATOS },
  { "candidatos",                  FAROLCONSENT_VAI_CANDIDATOS },
  { "fatos[].alvo",                FAROLCONSENT_VAI_FRASE      },
  { "fatos[].frase",               FAROLCONSENT_VAI_FRASE      },
  { "fatos[].perguntas",           FAROLCONSENT_VAI_FRASE      },
  { "fatos",                       FAROLCONSENT_VAI_FRASE      },
  { "frase",                       FAROLCONSENT_VAI_FRASE      },
  { "perguntas",                   FAROLCONSENT_VAI_FRASE      }
};

// Subcaminhos exatos de folhas permitidos dentro dos grupos estruturados.
static const char *const CAMINHOS_EXATOS[] = {
  "v", "modo", "idioma", "hora", "infantil", "consentiu",
  "gosto", "gosto.nivel", "gosto.fim_semana",
  "gosto.generos", "gosto.generos[]", "gosto.generos[].id", "gosto.generos[].nome", "gosto.generos[].peso",
  "gosto.temas",   "gosto.temas[]",   "gosto.temas[].id",   "gosto.temas[].nome",   "gosto.temas[].peso",
  "gosto.pessoas", "gosto.pessoas[]", "gosto.pessoas[].id", "gosto.pessoas[].nome", "gosto.pessoas[].peso",
  "gosto.ancoras", "gosto.ancoras[]", "gosto.ancoras[].id", "gosto.ancoras[].titulo",
  "ancoras", "ancoras[]", "ancoras[].id", "ancoras[].titulo",
  "candidatos", "candidatos[]",
  "candidatos[].c", "candidatos[].id", "candidatos[].tipo", "candidatos[].titulo",
  "candidatos[].ano", "candidatos[].generos", "candidatos[].generos[]",
  "candidatos[].minutos", "candidatos[].nota", "candidatos[].pontos",
  "candidatos[].motivos", "candidatos[].motivos[]",
  "candidatos[].motivos[].tipo", "candidatos[].motivos[].id",
  "candidatos[].motivos[].a", "candidatos[].motivos[].b",
  "candidatos[].eixos", "candidatos[].eixos.faltam",
  "candidatos[].eixos.min_noite", "candidatos[].eixos.cabe_hoje",
  "fatos", "fatos[]", "fatos[].alvo", "fatos[].frase", "fatos[].perguntas",
  "frase", "perguntas"
};

static int segmentoProibido(const char *seg, size_t len) {
  static const char *const PROIB[] = {
    "diario", "notas", "nota_data", "datas", "data",
    "perfil", "profile", "usuario", "amigos", "amigo",
    "addons", "addon", "fontes", "fonte", "vetor",
    "token", "senha", "email", "ip", "historico"
  };
  size_t i;
  for (i = 0; i < sizeof PROIB / sizeof *PROIB; i++) {
    if (strlen(PROIB[i]) == len && memcmp(seg, PROIB[i], len) == 0)
      return 1;
  }
  return 0;
}

static int normalizarCaminho(const char *caminho, char *out, size_t cap) {
  size_t i = 0, o = 0;
  if (!caminho || !caminho[0] || !out || cap < 4) return 0;
  while (caminho[i]) {
    if (caminho[i] == '[') {
      if (o + 2 >= cap) return 0;
      out[o++] = '[';
      i++;
      while (isdigit((unsigned char)caminho[i])) i++;
      if (caminho[i] != ']') return 0;
      out[o++] = ']';
      i++;
    } else {
      if (o + 1 >= cap) return 0;
      out[o++] = caminho[i++];
    }
  }
  out[o] = '\0';
  // Recusar qualquer segmento proibido ("diario", "perfil", "amigos", ...).
  {
    const char *p = out;
    while (*p) {
      size_t slen = strcspn(p, ".[]");
      if (slen > 0 && segmentoProibido(p, slen)) return 0;
      p += slen;
      while (*p == '.' || *p == '[' || *p == ']') p++;
    }
  }
  return 1;
}

static int fatoChaveSegura(const char *k) {
  size_t i;
  if (!k || !k[0] || strlen(k) > 32) return 0;
  for (i = 0; k[i]; i++) {
    unsigned char c = (unsigned char)k[i];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_'))
      return 0;
  }
  if (segmentoProibido(k, strlen(k))) return 0;
  return 1;
}

static int caminhoPermitidoNorm(const char *norm) {
  size_t i;
  for (i = 0; i < sizeof CAMINHOS_EXATOS / sizeof *CAMINHOS_EXATOS; i++) {
    if (strcmp(norm, CAMINHOS_EXATOS[i]) == 0) return 1;
  }
  if (strncmp(norm, "fatos[].", 8) == 0 && fatoChaveSegura(norm + 8)) return 1;
  return 0;
}

const FarolConsentGrupo *farolconsent_espelho_grupos(size_t *n) {
  if (n) *n = sizeof ESPELHO_GRUPOS / sizeof *ESPELHO_GRUPOS;
  return ESPELHO_GRUPOS;
}

const char *const *farolconsent_vai_junto(size_t *n) {
  if (n) *n = FAROLCONSENT_VAI_N;
  return VAI_JUNTO;
}

const char *const *farolconsent_nunca_vai(size_t *n) {
  if (n) *n = FAROLCONSENT_NUNCA_N;
  return NUNCA_VAI;
}

const char *farolconsent_espelho_para(const char *caminho) {
  char norm[256];
  size_t i;
  if (!normalizarCaminho(caminho, norm, sizeof norm)) return NULL;
  if (!caminhoPermitidoNorm(norm)) return NULL;
  if (strcmp(norm, "gosto") == 0) return FAROLCONSENT_VAI_GOSTO;
  for (i = 0; i < sizeof ESPELHO_GRUPOS / sizeof *ESPELHO_GRUPOS; i++) {
    const char *pref = ESPELHO_GRUPOS[i].prefixo;
    size_t plen = strlen(pref);
    if (strncmp(norm, pref, plen) == 0) {
      char prox = norm[plen];
      if (prox == '\0' || prox == '.' || prox == '[')
        return ESPELHO_GRUPOS[i].frase;
    }
  }
  return NULL;
}

int farolconsent_campo_autorizado(const char *caminho) {
  return farolconsent_espelho_para(caminho) != NULL;
}

void farolconsent_pasta(const char *dir) {
  pthread_mutex_lock(&trava);
  if (dir && dir[0]) snprintf(pastaOverride, sizeof pastaOverride, "%s", dir);
  else pastaOverride[0] = '\0';
  pthread_mutex_unlock(&trava);
}

void farolconsent_teste_zera(void) {
  pthread_mutex_lock(&trava);
  perfilAtivo = 0;
  lastPerfisAtivo = 0;
  estadoAtivo = FAROL_CONSENT_NAO_PERGUNTADO;
  pastaOverride[0] = '\0';
  pthread_mutex_unlock(&trava);
}
