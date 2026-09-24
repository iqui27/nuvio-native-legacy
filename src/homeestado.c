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
#define HOMEESTADO_CTX 640

static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static unsigned geracao, donoHash;
static int valido;
static char contexto[HOMEESTADO_CTX], caminho[64];
// As partes do contexto corrente, para o log dizer O QUE mudou.
static HomeContexto partesAtuais;
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

// A ASSINATURA E EM PARTES, e cada parte responde a uma pergunta diferente.
//
// Ate a 1.4.4 era um hash so, e qualquer mudanca nele descartava a montagem em
// voo inteira ("montagem descartada: conta/perfil/config mudou no meio") e
// recusava o cache no fim. MEDIDO no log de campo do @rawldon (Tizen 6, 241
// colecoes): a home da conta so apareceu aos 85 s em vez de 49 s, e o cache da
// segunda volta tambem foi recusado — o arranque seguinte comecava sem cache.
// A mesma linha aparece 115 vezes nos logs de quase todo mundo.
//
// A separacao que importa para quem esta montando:
//   IDENTIDADE (dono, perfil, idioma) e ADDONS (quais bases estao ligadas)
//     mudam O QUE foi buscado. Dado buscado sob a identidade velha nao pode ir
//     para a tela nem para o disco da nova. Continua descartando.
//   ESTRUTURA (ajustes da home, fileiras.c, ordem da conta, colecoes) muda so
//     COMO o que ja chegou e arrumado. O que foi buscado continua valendo:
//     publica e remonta sem rede (descoberta.c).
static unsigned hashAjustes(void) {
  unsigned h = 2166136261u;
  const char *fonte;
  h = hashBytes(h, &(int){fil_limite()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_cw_ligado()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_cw_estilo()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_posteres_deitados()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_rotulos_poster()}, sizeof(int));
  h = hashBytes(h, &(int){ajustes_hero_fonte()}, sizeof(int));
  fonte = fil_hero_fonte(); h = hashTexto(h, fonte);
  return h;
}

// A lista escolhida de add-ons e configuracao explicita: inclui-la permite
// incorporar catalogos quando um add-on e adicionado, sem seguir alteracoes
// de payload no manifesto de um add-on ja configurado.
//
// `addons_ativo`, e NAO `addons_tem_catalogo`, que era o que estava aqui. A
// capacidade "tem catalogo" e APRENDIDA do manifesto, e quem le o manifesto e
// a propria montagem: a lista da conta chega com catalogo=1 (suposicao
// otimista, addons.c) e o addon so de stream vira 0 no meio da volta. A
// assinatura mudava por causa da leitura que a montagem acabou de fazer, e ela
// se descartava sozinha no arranque de qualquer conta com um addon so de
// stream ou so de legenda (pela leitura do codigo; o log de campo nao separa
// esta causa das colecoes). Ligado/desligado e escolha da pessoa; capacidade
// nao e.
static unsigned hashAddons(void) {
  unsigned h = 2166136261u;
  int i;
  h = hashBytes(h, &(int){addons_n()}, sizeof(int));
  for (i = 0; i < addons_n(); i++) {
    h = hashTexto(h, addons_base(i));
    h = hashBytes(h, &(int){addons_ativo(i)}, sizeof(int));
  }
  return h;
}

// fileiras.c e DUAS coisas na mesma tabela: a escolha da pessoa (oculta,
// forma, tamanho, ordem local) e o REGISTRO de toda chave ja vista, que a
// descoberta (fil_registrar de todos os candidatos) e a home (fileiras de
// colecao, fil_espelhar_ordem) reescrevem sozinhas a cada montagem. Fazer hash
// da tabela crua fazia o registro contar como "config mudou". No mesmo log o
// cache da segunda montagem foi recusado 0,9 s depois de "[home] 24 fileiras
// na tela" — que e quando a home registra as fileiras de colecao. A ligacao
// e pela leitura do codigo, nao foi medida no aparelho.
//
// Sem ordem local a posicao na tabela e so espelho da home, entao entram so
// as linhas com escolha, e sem ordem (soma, que nao depende da posicao). Com
// ordem local a posicao E escolha, e a tabela inteira entra em sequencia.
static unsigned hashFileiras(void) {
  unsigned h = 2166136261u, soma = 0;
  int i, n = fil_n(), ordem = fil_tem_ordem();
  h = hashBytes(h, &ordem, sizeof ordem);
  for (i = 0; i < n; i++) {
    const char *c = fil_chave(i);
    int oculta = fil_linha_oculta(i), tipo = fil_linha_tipo(i);
    int tam = fil_linha_tam(i), contaOculta = catordem_oculta(c, "");
    unsigned l;
    if (!ordem && !oculta && tipo == FIL_TIPO_AUTO && tam == FIL_TAM_PADRAO &&
        !contaOculta)
      continue;
    l = hashTexto(2166136261u, c);
    l = hashBytes(l, &oculta, sizeof oculta);
    l = hashBytes(l, &tipo, sizeof tipo);
    l = hashBytes(l, &tam, sizeof tam);
    l = hashBytes(l, &contaOculta, sizeof contaOculta);
    if (ordem) h = hashBytes(h, &l, sizeof l);
    else soma += l;
  }
  return hashBytes(h, &soma, sizeof soma);
}

static unsigned hashOrdemConta(void) {
  unsigned h = 2166136261u;
  int i;
  h = hashBytes(h, &(int){catordem_tem_ordem()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_n()}, sizeof(int));
  for (i = 0; i < catordem_n(); i++) h = hashTexto(h, catordem_chave(i));
  h = hashBytes(h, &(int){catordem_tem_ocultar_nao_lancados()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_ocultar_nao_lancados()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_tem_ocultar_sublinhado()}, sizeof(int));
  h = hashBytes(h, &(int){catordem_ocultar_sublinhado()}, sizeof(int));
  return h;
}

// Colecoes definem fileiras estruturais. Nomes e ids fazem parte da
// configuracao; URLs de arte podem atualizar sem criar outra fileira.
static unsigned hashColecoes(void) {
  unsigned h = 2166136261u;
  int i;
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

void homeestado_contexto(HomeContexto *c) {
  const char *u = sessao_usuario();
  if (!c) return;
  // O hash so escolhe o nome do arquivo; a identidade completa no conteudo
  // impede aceitar snapshot de outra conta mesmo em caso de colisao do hash.
  snprintf(c->identidade, sizeof c->identidade, "owner=%zu:%s|p=%d|l=%d",
           u ? strlen(u) : 0, u ? u : "", c->perfil = perfis_ativo(),
           ajustes_idioma_ingles());
  c->addons = hashAddons();
  c->ajustes = hashAjustes();
  c->fileiras = hashFileiras();
  c->ordemConta = hashOrdemConta();
  c->colecoes = hashColecoes();
}

int homeestado_mudancas(const HomeContexto *a, const HomeContexto *b) {
  int m = 0;
  if (!a || !b) return HOMEESTADO_MUDOU_IDENTIDADE;
  if (strcmp(a->identidade, b->identidade)) m |= HOMEESTADO_MUDOU_IDENTIDADE;
  if (a->addons != b->addons) m |= HOMEESTADO_MUDOU_ADDONS;
  if (a->ajustes != b->ajustes) m |= HOMEESTADO_MUDOU_AJUSTES;
  if (a->fileiras != b->fileiras) m |= HOMEESTADO_MUDOU_FILEIRAS;
  if (a->ordemConta != b->ordemConta) m |= HOMEESTADO_MUDOU_ORDEM_CONTA;
  if (a->colecoes != b->colecoes) m |= HOMEESTADO_MUDOU_COLECOES;
  return m;
}

const char *homeestado_mudancas_texto(int m, char *buf, unsigned tam) {
  static const struct { int bit; const char *nome; } partes[] = {
    { HOMEESTADO_MUDOU_IDENTIDADE,   "identidade" },
    { HOMEESTADO_MUDOU_ADDONS,       "addons" },
    { HOMEESTADO_MUDOU_AJUSTES,      "ajustes" },
    { HOMEESTADO_MUDOU_FILEIRAS,     "fileiras" },
    { HOMEESTADO_MUDOU_ORDEM_CONTA,  "ordem-da-conta" },
    { HOMEESTADO_MUDOU_COLECOES,     "colecoes" },
  };
  size_t k, usado = 0;
  if (!buf || !tam) return "";
  buf[0] = 0;
  for (k = 0; k < sizeof partes / sizeof partes[0]; k++) {
    int e;
    if (!(m & partes[k].bit)) continue;
    e = snprintf(buf + usado, tam - usado, "%s%s", usado ? "+" : "", partes[k].nome);
    if (e < 0 || (size_t)e >= tam - usado) break;
    usado += (size_t)e;
  }
  if (!buf[0]) snprintf(buf, tam, "nada");
  return buf;
}

static void contextoTexto(const HomeContexto *c, char *out, size_t tam) {
  snprintf(out, tam, "%s|ad=%08x|cfg=%08x.%08x.%08x.%08x", c->identidade,
           c->addons, c->ajustes, c->fileiras, c->ordemConta, c->colecoes);
}

static void contextoAtual(char *out, size_t tam, unsigned *dono, int *perfil,
                          HomeContexto *partes) {
  HomeContexto c;
  homeestado_contexto(&c);
  contextoTexto(&c, out, tam);
  if (dono) *dono = hashDono(sessao_usuario());
  if (perfil) *perfil = c.perfil;
  if (partes) *partes = c;
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
  char atual[HOMEESTADO_CTX], arquivo[64];
  unsigned dono;
  int perfil;
  HomeContexto partes;
  contextoAtual(atual, sizeof atual, &dono, &perfil, &partes);
  caminhoIdentidade(arquivo, sizeof arquivo, dono, perfil);
  pthread_mutex_lock(&trava);
  partesAtuais = partes;
  snprintf(contexto, sizeof contexto, "%s", atual);
  snprintf(caminho, sizeof caminho, "%s", arquivo);
  donoHash = dono;
  carregarSnapshot(atual, arquivo);
  geracao++;
  pthread_mutex_unlock(&trava);
}

unsigned homeestado_geracao(void) {
  char atual[HOMEESTADO_CTX], arquivo[64];
  unsigned dono;
  int perfil, trocou, mudou = 0;
  unsigned g;
  HomeContexto partes;
  contextoAtual(atual, sizeof atual, &dono, &perfil, &partes);
  caminhoIdentidade(arquivo, sizeof arquivo, dono, perfil);
  pthread_mutex_lock(&trava);
  trocou = strcmp(atual, contexto) != 0;
  if (trocou) {
    mudou = homeestado_mudancas(&partesAtuais, &partes);
    partesAtuais = partes;
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
  g = geracao;
  pthread_mutex_unlock(&trava);
  // O QUE MUDOU, com nome. Sem isto o log de campo so tinha o efeito ("montagem
  // descartada") e nunca a causa — e foi preciso ler codigo para adivinhar que
  // eram as colecoes chegando e o registro de fileiras.
  if (trocou) {
    char txt[96];
    printf("[homeestado] contexto mudou (%s): geracao %u\n",
           homeestado_mudancas_texto(mudou, txt, sizeof txt), g);
    fflush(stdout);
  }
  return g;
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
  char atual[HOMEESTADO_CTX], arquivo[64], buf[HOMEESTADO_BUF];
  char novas[HOMEESTADO_MAX][192];
  unsigned dono;
  int perfil, i, j, novoN = 0;
  size_t used = 0;
  contextoAtual(atual, sizeof atual, &dono, &perfil, NULL);
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
  char atual[HOMEESTADO_CTX], arquivo[64]; unsigned hash; int p, ok;
  contextoAtual(atual, sizeof atual, &hash, &p, NULL);
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
