#include "homeestado.h"
#include "dados.h"
#include "sessao.h"
#include "perfis.h"
#include "ajustes.h"
#include "fileiras.h"
#include "colecoes.h"
#include "catordem.h"
#include "addons.h"
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define HOMEESTADO_LEGADO "home-estado-v1.txt"
#define HOMEESTADO_MAX CAT_FIL_MAX
#define HOMEESTADO_BUF (CAT_FIL_MAX * 220 + 640)

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned geracao, donoHash;
static int valido;
static char contexto[512], caminho[64];
static char chaves[CAT_FIL_MAX][192];
static int nChaves;

static unsigned hashBytes(unsigned h, const void *data, size_t n) {
  const unsigned char *p = (const unsigned char *)data;
  while (n--) h = (h ^ *p++) * 16777619u;
  return h;
}

static unsigned hashTexto(unsigned h, const char *s) {
  if (s) h = hashBytes(h, s, strlen(s));
  return (h ^ 0xffu) * 16777619u;
}

static unsigned hashDono(const char *usuario) {
  unsigned h = 2166136261u;
  return hashTexto(h, usuario ? usuario : "");
}

static void caminhoIdentidade(char *dst, size_t tam, unsigned dono, int perfil) {
  snprintf(dst, tam, "home-estado-%08x-p%d.txt", dono, perfil);
}

static unsigned assinaturaConfig(void) {
  unsigned h = 2166136261u;
  int i;
  const char *fonte;
  h = hashBytes(h, &(int){fil_limite()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_cw_ligado()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_cw_estilo()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_posteres_deitados()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_rotulos_poster()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_hero_fonte()}, sizeof(int));
  fonte = fil_hero_fonte(); h = hashTexto(h, fonte);
  // A lista escolhida de add-ons é configuração explícita: incluí-la permite
  // incorporar catálogos quando um add-on é adicionado, sem seguir alterações
  // de payload no manifesto de um add-on já configurado.
  h = hashBytes(h, &(int){addons_n()}, sizeof(int));
  for (i = 0; i < addons_n(); i++) {
    h = hashTexto(h, addons_base(i));
    h = hashBytes(h, &(int){addons_tem_catalogo(i)}, sizeof(int));
  }

  // Fingerprint do conteudo escolhido, estavel entre processos. Revisoes em
  // memoria como fil_revisao/col_revisao mudam ao carregar e nao identificam
  // uma configuracao persistida.
  h = hashBytes(h, &(int){fil_n()}, sizeof(int));
  for (i = 0; i < fil_n(); i++) {
    h = hashTexto(h, fil_chave(i));
    h = hashBytes(h, &(int){fil_linha_oculta(i)}, sizeof(int));
    h = hashBytes(h, &(int){fil_linha_tipo(i)}, sizeof(int));
    h = hashBytes(h, &(int){fil_linha_tam(i)}, sizeof(int));
    h = hashBytes(h, &(int){catordem_oculta(fil_chave(i), "")}, sizeof(int));
  }
  h = hashBytes(h, &(int){catordem_tem_ordem()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_n()}, sizeof(int));
  for (i = 0; i < catordem_n(); i++) h = hashTexto(h, catordem_chave(i));
  h = hashBytes(h, &(int){catordem_tem_ocultar_nao_lancados()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_ocultar_nao_lancados()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_tem_ocultar_sublinhado()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_ocultar_sublinhado()}, sizeof(int));

  // Colecoes definem fileiras estruturais. Nomes e ids fazem parte da
  // configuracao; URLs de arte podem atualizar sem criar outra fileira.
  h = hashBytes(h, &(int){col_n()}, sizeof(int));
  for (i = 0; i < col_n(); i++) {
    const ColFolder *f = col_folder(i);
    int s;
    if (!f) continue;
    h = hashTexto(h, f->group);
    h = hashTexto(h, f->id);
    h = hashBytes(h, &f->nSources, sizeof f->nSources);
    for (s = 0; s < f->nSources; s++) {
      const ColSource *src = &f->sources[s];
      h = hashTexto(h, src->prov); h = hashTexto(h, src->addonId);
      h = hashTexto(h, src->base); h = hashTexto(h, src->type);
      h = hashTexto(h, src->catId); h = hashTexto(h, src->tmdbTipo);
      h = hashBytes(h, &src->tmdbId, sizeof src->tmdbId);
      h = hashTexto(h, src->midia); h = hashTexto(h, src->ordenar);
      h = hashTexto(h, src->ordem); h = hashBytes(h, &src->traktLista, sizeof src->traktLista);
    }
  }
  return h;
}

static void contextoAtual(char *out, size_t tam, unsigned *dono, int *perfil) {
  const char *u = sessao_usuario();
  unsigned uh = hashDono(u);
  int p = perfis_ativo();
  // O hash só escolhe o nome do arquivo; a identidade completa no conteúdo
  // impede aceitar snapshot de outra conta mesmo em caso de colisão do hash.
  snprintf(out, tam, "owner=%zu:%s|p=%d|l=%d|cfg=%08x",
           u ? strlen(u) : 0, u ? u : "", p,
           ajustes_idioma_ingles(), assinaturaConfig());
  if (dono) *dono = uh;
  if (perfil) *perfil = p;
}

static void carregarSnapshot(const char *atual, const char *arquivo) {
  char *blob = dados_ler(arquivo), *linha, *prox;
  valido = 0;
  nChaves = 0;
  if (!blob) return;
  linha = blob;
  prox = strchr(linha, '\n');
  if (prox) *prox++ = 0;
  if (!strncmp(linha, "ctx\t", 4) && !strcmp(linha + 4, atual)) {
    valido = 1;
    while (prox && *prox && nChaves < HOMEESTADO_MAX) {
      char *fim = strchr(prox, '\n');
      char chave[192];
      int n;
      if (fim) *fim = 0;
      if (!strncmp(prox, "row\t", 4)) {
        n = (int)strcspn(prox + 4, "\t\r\n");
        if (n > 0 && n < (int)sizeof chave) {
          memcpy(chave, prox + 4, (size_t)n); chave[n] = 0;
          snprintf(chaves[nChaves++], sizeof chaves[0], "%s", chave);
        }
      }
      if (!fim) break;
      prox = fim + 1;
    }
  }
  free(blob);
}

void homeestado_iniciar(void) {
  char atual[512], arquivo[64];
  unsigned dono;
  int perfil;
  contextoAtual(atual, sizeof atual, &dono, &perfil);
  caminhoIdentidade(arquivo, sizeof arquivo, dono, perfil);
  pthread_mutex_lock(&trava);
  snprintf(contexto, sizeof contexto, "%s", atual);
  snprintf(caminho, sizeof caminho, "%s", arquivo);
  donoHash = dono;
  carregarSnapshot(atual, arquivo);
  geracao++;
  pthread_mutex_unlock(&trava);
}

unsigned homeestado_geracao(void) {
  char atual[512], arquivo[64];
  unsigned dono;
  int perfil, trocou;
  contextoAtual(atual, sizeof atual, &dono, &perfil);
  caminhoIdentidade(arquivo, sizeof arquivo, dono, perfil);
  pthread_mutex_lock(&trava);
  trocou = strcmp(atual, contexto) != 0;
  if (trocou) {
    snprintf(contexto, sizeof contexto, "%s", atual);
    snprintf(caminho, sizeof caminho, "%s", arquivo);
    donoHash = dono;
    // This process may still hold CatItems from the previous owner/profile.
    // A persisted snapshot for the destination is useful after a fresh app
    // start (homeestado_iniciar), but cannot attest that the live catalogue
    // has already switched to that identity. Keep runtime state invalid until
    // the new generation publishes and saves its own rows.
    valido = 0; nChaves = 0;
    geracao++;
  }
  pthread_mutex_unlock(&trava);
  return geracao;
}

int homeestado_contexto_valido(void) {
  int r;
  pthread_mutex_lock(&trava); r = valido; pthread_mutex_unlock(&trava);
  return r;
}

int homeestado_tem_fileira(const char *chave) {
  int i, r = 0;
  if (!chave || !chave[0]) return 0;
  pthread_mutex_lock(&trava);
  if (valido)
    for (i = 0; i < nChaves; i++) if (!strcmp(chaves[i], chave)) { r = 1; break; }
  pthread_mutex_unlock(&trava);
  return r;
}

int homeestado_ordem_fileira(const char *chave) {
  int i, r = -1;
  if (!chave || !chave[0]) return -1;
  pthread_mutex_lock(&trava);
  if (valido)
    for (i = 0; i < nChaves; i++) if (!strcmp(chaves[i], chave)) { r = i; break; }
  pthread_mutex_unlock(&trava);
  return r;
}

int homeestado_quantidade_fileiras(void) {
  int n;
  pthread_mutex_lock(&trava); n = valido ? nChaves : 0; pthread_mutex_unlock(&trava);
  return n;
}

int homeestado_salvar_se_geracao(const CatFileira *fils, int n, unsigned esperada) {
  char atual[512], arquivo[64], buf[HOMEESTADO_BUF];
  char novas[HOMEESTADO_MAX][192];
  unsigned dono;
  int perfil, i, j, novoN = 0;
  size_t used = 0;
  contextoAtual(atual, sizeof atual, &dono, &perfil);
  caminhoIdentidade(arquivo, sizeof arquivo, dono, perfil);
  if (!fils || n < 0 || n > HOMEESTADO_MAX) return 0;
  // Reconcile account/profile/config first. A publisher started under an old
  // context must not make that context current again while saving its snapshot.
  (void)homeestado_geracao();
  pthread_mutex_lock(&trava);
  if (geracao != esperada || strcmp(atual, contexto) || strcmp(arquivo, caminho)) {
    pthread_mutex_unlock(&trava); return 0;
  }
  for (i = 0; i < n; i++) {
    if (!fils[i].chave[0]) continue;
    for (j = 0; j < novoN; j++) if (!strcmp(novas[j], fils[i].chave)) break;
    if (j == novoN) snprintf(novas[novoN++], sizeof novas[0], "%s", fils[i].chave);
  }
  used = (size_t)snprintf(buf, sizeof buf, "ctx\t%s\n", atual);
  for (i = 0; i < novoN && used < sizeof buf; i++) {
    int escrito = snprintf(buf + used, sizeof buf - used, "row\t%s\n", novas[i]);
    if (escrito < 0 || (size_t)escrito >= sizeof buf - used) break;
    used += (size_t)escrito;
  }
  // A estrutura aceita em memoria/disco so avanca quando o snapshot inteiro
  // foi gravado. Falha de armazenamento conserva a ultima configuracao boa.
  if (used < sizeof buf && dados_gravar(arquivo, buf)) {
    nChaves = novoN;
    memcpy(chaves, novas, sizeof novas);
    valido = 1;
    pthread_mutex_unlock(&trava);
    return 1;
  }
  pthread_mutex_unlock(&trava);
  return 0;
}

void homeestado_salvar(const CatFileira *fils, int n) {
  (void)homeestado_salvar_se_geracao(fils, n, homeestado_geracao());
}

int homeestado_identidade_geracao(unsigned esperada, char *dono, unsigned tamDono,
                                  int *perfil) {
  char atual[512], arquivo[64]; unsigned hash; int p, ok;
  contextoAtual(atual, sizeof atual, &hash, &p);
  caminhoIdentidade(arquivo, sizeof arquivo, hash, p);
  (void)homeestado_geracao();
  pthread_mutex_lock(&trava);
  ok = geracao == esperada && !strcmp(atual, contexto) && !strcmp(arquivo, caminho);
  if (ok && dono && tamDono) snprintf(dono, tamDono, "%s", sessao_usuario() ? sessao_usuario() : "");
  if (ok && perfil) *perfil = p;
  pthread_mutex_unlock(&trava);
  return ok;
}

void homeestado_esquecer(void) {
  unsigned dono;
  int p;
  pthread_mutex_lock(&trava);
  dono = donoHash;
  for (p = 1; p <= CONTA_PERFIL_MAX; p++) {
    char arquivo[64];
    caminhoIdentidade(arquivo, sizeof arquivo, dono, p);
    dados_apagar(arquivo);
  }
  dados_apagar(HOMEESTADO_LEGADO);
  valido = 0; nChaves = 0; geracao++;
  pthread_mutex_unlock(&trava);
}
