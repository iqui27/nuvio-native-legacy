// Modelo da tela do social. Ver socialvis.h para de onde vem cada dado hoje e
// o que se espera do socialsrv.
#include "horafmt.h"
#include "socialvis.h"
#include "catalogo.h"
#include "artemetahub.h"
#include "recomenda.h"
#include "perfis.h"
#include "dados.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Refaz o modelo no maximo uma vez por segundo, e so quando a fonte mudou. O
// recomenda.c sonda a cada 60 s e o catalogo e republicado algumas vezes por
// ciclo: um segundo de atraso nao se ve, e refazer por quadro seria copiar
// ~40 contatos sob mutex 60 vezes por segundo para nada.
#define SV_REFAZ_MS 1000u

// Tudo o que chegou, antes de filtrar e de agrupar (inclui SV_ATIVIDADE, que
// so serve para o rosto ter um titulo, e nunca entra no feed).
static SvEvento brutos[SV_EVENTOS_MAX];
static int nBrutos;
static SvEvento feed[SV_EVENTOS_MAX];
static int nFeed;
static SvAmigo amigos[SV_AMIGOS_MAX];
static int nAmigos;
static unsigned rev, hashUlt;
static Uint32 refeitoEm;
static unsigned catRevUlt = ~0u;
static int recNUlt = -1;

// O CAMINHO DO socialsrv (socialvis_definir_feed): com ele ligado o modelo
// para de ler o que existe hoje.
static SvEvento *externo;
static int nExterno, temExterno;
// Perfis completos vindos de fora (socialsrv ou demo).
typedef struct { char id[96]; SvPerfil p; } SvExtra;
static SvExtra *extras;
static int nExtras;
static char perfilPedido[96];
static unsigned geracaoVista;
static int temGeracao;

// --- vistos -----------------------------------------------------------------
//
// "NOVIDADE NAO VISTA" E POR ASSINATURA, e nao por hora: o feed do Trakt chega
// sem hora (ver socialvis.h). A assinatura e o titulo + acao + episodio do
// evento mais novo da pessoa; mudou, e novidade. Fica em amigos-vistos.txt
// (uma linha "id\tassinatura"), gravado so quando muda.
static const char *arquivoVistos(void) {
  static char nome[48];
  if (perfis_ativo() == 1) return "amigos-vistos.txt";
  snprintf(nome, sizeof nome, "amigos-vistos-p%d.txt", perfis_ativo());
  return nome;
}
#define SV_ARQ_VISTOS arquivoVistos()
typedef struct { char id[96]; unsigned ass; } SvVisto;
static SvVisto vistos[SV_AMIGOS_MAX * 2];
static int nVistos, vistosLidos;

static unsigned fnv(unsigned h, const void *p, size_t n) {
  const unsigned char *b = (const unsigned char *)p;
  size_t i;
  for (i = 0; i < n; i++) { h ^= b[i]; h *= 16777619u; }
  return h;
}

static unsigned assinatura(const SvAmigo *a) {
  unsigned h = 2166136261u;
  if (a->nTit < 1) return 0;
  h = fnv(h, a->tit[0].imdb, strlen(a->tit[0].imdb));
  h = fnv(h, &a->tit[0].acao, sizeof a->tit[0].acao);
  h = fnv(h, &a->tit[0].temporada, sizeof a->tit[0].temporada);
  h = fnv(h, &a->tit[0].episodio, sizeof a->tit[0].episodio);
  return h ? h : 1;
}

static void lerVistos(void) {
  char *s, *p;
  vistosLidos = 1;
  nVistos = 0;
  s = dados_ler(SV_ARQ_VISTOS);
  if (!s) return;
  for (p = s; *p && nVistos < (int)(sizeof vistos / sizeof *vistos);) {
    char *fim = strchr(p, '\n'), *tab;
    if (fim) *fim = 0;
    tab = strchr(p, '\t');
    if (tab && tab > p) {
      *tab = 0;
      snprintf(vistos[nVistos].id, sizeof vistos[nVistos].id, "%s", p);
      vistos[nVistos].ass = (unsigned)strtoul(tab + 1, NULL, 10);
      nVistos++;
    }
    if (!fim) break;
    p = fim + 1;
  }
  free(s);
}

static void gravarVistos(void) {
  char buf[sizeof vistos / sizeof *vistos * 112];
  size_t k = 0;
  int i;
  buf[0] = 0;
  for (i = 0; i < nVistos && k + 112 < sizeof buf; i++)
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\t%u\n", vistos[i].id, vistos[i].ass);
  dados_gravar_leve(SV_ARQ_VISTOS, buf);
}

static int vistoDe(const char *id, unsigned *ass) {
  int i;
  if (!vistosLidos) lerVistos();
  for (i = 0; i < nVistos; i++)
    if (!strcmp(vistos[i].id, id)) { *ass = vistos[i].ass; return 1; }
  return 0;
}

void socialvis_marcar_visto(const char *id) {
  int i, k;
  unsigned ass;
  if (!id || !id[0]) return;
  k = socialvis_amigo_indice(id);
  if (k < 0) return;
  ass = assinatura(&amigos[k]);
  amigos[k].novo = 0;
  if (!vistosLidos) lerVistos();
  for (i = 0; i < nVistos; i++)
    if (!strcmp(vistos[i].id, id)) {
      if (vistos[i].ass == ass) return;
      vistos[i].ass = ass; gravarVistos(); return;
    }
  if (nVistos >= (int)(sizeof vistos / sizeof *vistos)) {
    memmove(vistos, vistos + 1, sizeof *vistos * (size_t)(nVistos - 1));
    nVistos--;
  }
  snprintf(vistos[nVistos].id, sizeof vistos[nVistos].id, "%s", id);
  vistos[nVistos].ass = ass;
  nVistos++;
  gravarVistos();
}

// --- de onde vem hoje -------------------------------------------------------

static int acaoDoTexto(const char *s) {
  if (!s || !s[0]) return SV_ATIVIDADE;
  if (!strcmp(s, "assistindo agora")) return SV_AGORA;
  if (!strcmp(s, "registrou um check-in")) return SV_INICIO;
  if (!strcmp(s, "assistiu")) return SV_FIM;
  if (!strcmp(s, "avaliou")) return SV_AVALIOU;
  return SV_ATIVIDADE;
}

static int jaTemEvento(const char *pessoa, const char *imdb, const char *tipo, int acao, int t, int ep) {
  int i;
  for (i = 0; i < nBrutos; i++)
    if (!strcmp(brutos[i].pessoaId, pessoa) && !strcmp(brutos[i].imdb, imdb) &&
        !strcmp(brutos[i].tipo, tipo) && brutos[i].acao == acao &&
        brutos[i].temporada == t && brutos[i].episodio == ep) return 1;
  return 0;
}

static SvEvento *novoBruto(void) {
  SvEvento *e;
  if (nBrutos >= SV_EVENTOS_MAX) return NULL;
  e = &brutos[nBrutos++];
  memset(e, 0, sizeof *e);
  e->pct = -1; e->restanteMin = -1; e->reacao = SV_REAC_NADA;
  return e;
}

// A fileira social_activity do catalogo.
static void doCatalogo(void) {
  int r, k;
  for (r = 0; r < cat_n_fileiras(); r++) {
    const CatFileira *f = cat_fileira(r);
    if (!f || strcmp(f->chave, "social_activity") || f->socialGeracao != recomenda_geracao()) continue;
    for (k = 0; k < f->n; k++) {
      const CatItem *it = cat_item(f->ini + k);
      char pid[96];
      SvEvento *e;
      if (!it || !it->socialSlug[0] || !it->imdb[0]) continue;
      if (!strncmp(it->socialSlug, "nuvio:", 6)) snprintf(pid, sizeof pid, "%s", it->socialSlug);
      else snprintf(pid, sizeof pid, "trakt:%s", it->socialSlug);
      if (jaTemEvento(pid, it->imdb, it->tipo, acaoDoTexto(it->socialAcao), it->temporada, it->episodio) ||
          !(e = novoBruto())) continue;
      snprintf(e->pessoaId, sizeof e->pessoaId, "%s", pid);
      snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s",
               it->socialNome[0] ? it->socialNome : it->pais);
      snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", it->socialAvatar);
      e->fonte = !strncmp(pid, "nuvio:", 6) ? SV_FONTE_NUVIO : SV_FONTE_TRAKT;
      e->acao = acaoDoTexto(it->socialAcao);
      snprintf(e->imdb, sizeof e->imdb, "%s", it->imdb);
      snprintf(e->tipo, sizeof e->tipo, "%s", it->tipo);
      snprintf(e->titulo, sizeof e->titulo, "%s", it->titulo);
      snprintf(e->poster, sizeof e->poster, "%s", it->poster);
      snprintf(e->arte, sizeof e->arte, "%s", it->backdrop);
      e->temporada = it->temporada;
      e->episodio = it->episodio;
      if (it->progresso > 0) e->pct = it->progresso;
      if (it->restanteMin > 0) e->restanteMin = it->restanteMin;
      if (it->retomadoMs > 0) e->quando = it->retomadoMs / 1000;
    }
    break;
  }
}

// O feed do nosso servidor por amigo do Nuvio (recomenda_amigo_atividades).
static void doServidorPorAmigo(const RecContato *c) {
  RecAtivAmigo at[8];
  int n, i;
  if (strncmp(c->id, "nuvio:", 6)) return;
  n = recomenda_amigo_atividades(c->id, at, 8);
  for (i = 0; i < n; i++) {
    SvEvento *e;
    int k;
    if (!at[i].imdb[0] || jaTemEvento(c->id, at[i].imdb, at[i].tipo, at[i].agora ? SV_AGORA : SV_FIM, 0, 0) ||
        !(e = novoBruto())) continue;
    snprintf(e->pessoaId, sizeof e->pessoaId, "%s", c->id);
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", c->nome);
    snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", c->avatar);
    e->fonte = SV_FONTE_NUVIO;
    e->acao = at[i].agora ? SV_AGORA : SV_FIM;
    snprintf(e->imdb, sizeof e->imdb, "%s", at[i].imdb);
    snprintf(e->tipo, sizeof e->tipo, "%s", at[i].tipo);
    snprintf(e->titulo, sizeof e->titulo, "%s", at[i].titulo);
    e->quando = at[i].criado;
    // A arte, se o titulo esta no catalogo; sem ele o cartao cai no esqueleto.
    k = cat_indice_por_imdb(at[i].imdb);
    if (k >= 0) {
      const CatItem *it = cat_item(k);
      if (it) {
        snprintf(e->poster, sizeof e->poster, "%s", it->poster);
        snprintf(e->arte, sizeof e->arte, "%s", it->backdrop);
      }
    }
  }
}

// O que me mandaram vira evento SV_MANDOU.
static void dasRecomendacoes(void) {
  RecItem r;
  int i;
  for (i = 0; i < REC_MAX && recomenda_item(i, &r); i++) {
    SvEvento *e;
    if (!r.imdb[0] || !(e = novoBruto())) continue;
    snprintf(e->pessoaId, sizeof e->pessoaId, "%s", r.de);
    snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", r.deNome);
    snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", r.deAvatar);
    e->fonte = !strncmp(r.de, "trakt:", 6) ? SV_FONTE_TRAKT : SV_FONTE_NUVIO;
    e->acao = SV_MANDOU;
    snprintf(e->imdb, sizeof e->imdb, "%s", r.imdb);
    snprintf(e->tipo, sizeof e->tipo, "%s", r.tipo);
    snprintf(e->titulo, sizeof e->titulo, "%s", r.titulo);
    snprintf(e->poster, sizeof e->poster, "%s", r.poster);
    snprintf(e->texto, sizeof e->texto, "%s", rec_frase(&r));
    e->quando = r.criado;
  }
}

static RecContato ctts[REC_CONTATOS_MAX];
static int nCtts;

static void svDoQueExiste(void) {
  int i;
  nBrutos = 0;
  doCatalogo();
  nCtts = recomenda_contatos(ctts, REC_CONTATOS_MAX);
  for (i = 0; i < nCtts; i++) doServidorPorAmigo(&ctts[i]);
  dasRecomendacoes();
}

// A PONTE DO socialsrv (branch agente/socialsrv: recomenda_feed_unido,
// recomenda_amigo). Desligada ate o merge: com -DNV_SOCIAL_V2 o modelo passa a
// ler o feed unificado (nosso servidor + Trakt, deduplicado em recomenda.c) e o
// perfil do amigo do servidor. Sem a bandeira, -1 = "o servidor ainda nao da
// isso", e o modelo cai em svDoQueExiste().
#ifdef NV_SOCIAL_V2
// "Assistindo agora": o Trakt so manda INICIO para quem esta vendo (sem hora);
// o nosso servidor manda INICIO com hora, e "agora" vence em 15 min.
static int svAcaoDe(const RecEvento *r) {
  switch (r->acao) {
    case REC_ACAO_INICIO:
      if (r->fonte == REC_FONTE_TRAKT) return r->agora ? SV_AGORA : SV_INICIO;
      if (r->quando > 0 && (long long)time(NULL) - r->quando < 15 * 60) return SV_AGORA;
      return SV_INICIO;
    case REC_ACAO_FIM:      return SV_FIM;
    case REC_ACAO_ABANDONO: return SV_ABANDONO;
    case REC_ACAO_REACAO:   return SV_REACAO;
    case REC_ACAO_SALVO:    return SV_SALVO;
    case REC_ACAO_NOTA:     return SV_AVALIOU;
    default:                return SV_ATIVIDADE;
  }
}
static int svFonteDe(int f) {
  return f == REC_FONTE_TRAKT ? SV_FONTE_TRAKT : f == REC_FONTE_SIMKL ? SV_FONTE_SIMKL
       : f == REC_FONTE_LETTERBOXD ? SV_FONTE_LETTERBOXD : SV_FONTE_NUVIO;
}
static void svDeRecEvento(const RecEvento *r, SvEvento *e) {
  int k;
  memset(e, 0, sizeof *e);
  snprintf(e->pessoaId, sizeof e->pessoaId, "%s", r->pessoa);
  snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", r->pessoaNome);
  snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", r->pessoaAvatar);
  e->fonte = svFonteDe(r->fonte);
  e->acao = svAcaoDe(r);
  e->reacao = r->acao == REC_ACAO_REACAO ? r->reacao : SV_REAC_NADA;
  snprintf(e->imdb, sizeof e->imdb, "%s", r->imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", r->midia);
  snprintf(e->titulo, sizeof e->titulo, "%s", r->titulo);
  snprintf(e->poster, sizeof e->poster, "%s", r->poster);
  e->temporada = r->temporada; e->episodio = r->episodio;
  e->pct = r->pct > 0 ? r->pct : -1;
  e->restanteMin = -1;
  e->quando = r->quando;
  e->nota = r->acao == REC_ACAO_NOTA ? r->nota : 0;
  e->eps = r->eps;
  // "Terminou e gostou": a reacao que o servidor juntou ao "terminou".
  if (r->acao == REC_ACAO_FIM && r->temReacFim) e->reacao = r->reacFim;
  // A arte deitada vem do catalogo (o feed so traz o cartaz).
  k = cat_indice_por_imdb(r->imdb);
  if (k >= 0) {
    const CatItem *it = cat_item(k);
    if (it) {
      snprintf(e->arte, sizeof e->arte, "%s", it->backdrop);
      if (!e->poster[0]) snprintf(e->poster, sizeof e->poster, "%s", it->poster);
    }
  }
}
static int svDoServidor(SvEvento *saida, int max) {
  static RecEvento r[SV_EVENTOS_MAX];
  CatItem *trakt;
  int nT = 0, n, i, f;
  if (!recomenda_ativo()) return -1;
  // Os itens que o Trakt ja montou (a fileira social_activity do catalogo).
  trakt = (CatItem *)calloc(16, sizeof *trakt);
  if (!trakt) return -1;
  for (f = 0; f < cat_n_fileiras(); f++) {
    const CatFileira *cf = cat_fileira(f);
    if (!cf || strcmp(cf->chave, "social_activity") || cf->socialGeracao != recomenda_geracao()) continue;
    for (i = 0; i < cf->n && nT < 16; i++) {
      const CatItem *it = cat_item(cf->ini + i);
      if (it && strncmp(it->socialSlug, "nuvio:", 6)) trakt[nT++] = *it;
    }
    break;
  }
  if (max > SV_EVENTOS_MAX) max = SV_EVENTOS_MAX;
  n = recomenda_feed_unido(r, max, trakt, NULL, nT);
  free(trakt);
  for (i = 0; i < n; i++) svDeRecEvento(&r[i], &saida[i]);
  return n;
}
// O perfil do servidor. O pedido sai na primeira vez que o perfil e lido (e de
// novo a cada abertura); a resposta chega no fio e entra na leitura seguinte —
// amigoperfil.c rele o modelo a cada segundo.
static int svPerfilDoServidor(const char *id, SvPerfil *p) {
  static RecAmigo ra;
  int i;
  if (!recomenda_ativo() || !id || !id[0]) return -1;
  if (strcmp(perfilPedido, id)) socialvis_abrir_perfil(id);
  switch (recomenda_amigo_estado()) {
    case REC_SOC_INDO: p->estado = SV_PERFIL_INDO; break;
    case REC_SOC_OK: p->estado = SV_PERFIL_OK; break;
    case REC_SOC_FALHA: p->estado = SV_PERFIL_FALHA; break;
    case REC_SOC_NAO_ACHOU: p->estado = SV_PERFIL_NAO_ACHOU; break;
    case REC_SOC_NEGADO: p->estado = SV_PERFIL_NEGADO; break;
    default: p->estado = SV_PERFIL_NADA; break;
  }
  { SvCmpDados d;
    int tem = recomenda_amigo(&ra) && !strcmp(ra.id, id);
    memset(&d, 0, sizeof d);
    d.temDados = tem;
    d.carregando = p->estado == SV_PERFIL_INDO;
    d.compartilha = tem ? ra.compartilha : -1;
    d.euCompartilho = recomenda_alcance() >= 1;
    if (tem) {
      d.temGosto = ra.temGosto; d.total = ra.gostoTotal; d.iguais = ra.gostoIguais;
      d.temCmp = ra.temCmp;
      d.filmesTotal = ra.filmesTotal; d.filmesIguais = ra.filmesIguais;
      d.seriesTotal = ra.seriesTotal; d.seriesIguais = ra.seriesIguais;
      d.comumFilmes = ra.comumFilmes; d.comumSeries = ra.comumSeries;
    }
    socialvis_comparar(&d, p->cmp);
    if (!tem) return 0; }
  p->compartilha = ra.compartilha;
  p->desde = ra.desde;
  p->porOnde = !strcmp(ra.origem, "trakt") ? SV_FONTE_TRAKT : SV_FONTE_NUVIO;
  if (ra.compartilha && ra.temGosto && ra.gostoTotal > 0) { p->gostoPct = ra.gostoPct; p->emComum = ra.gostoIguais; p->gostoTotal = ra.gostoTotal; }
  if (ra.compartilha && ra.temMes) { p->minutosMes = (int)(ra.seg / 60); p->filmesMes = ra.filmes; p->seriesCurso = ra.series; }
  if (ra.compartilha && ra.temAgora && p->nAssistindo < SV_FILA_MAX) {
    svDeRecEvento(&ra.agora, &p->assistindo[p->nAssistindo]);
    p->assistindo[p->nAssistindo++].acao = SV_AGORA;
  }
  for (i = 0; ra.compartilha && i < ra.nGostou && p->nGostou < SV_FILA_MAX; i++) {
    svDeRecEvento(&ra.gostou[i], &p->gostou[p->nGostou]);
    p->gostou[p->nGostou++].reacao = SV_REAC_GOSTOU;
  }
  { int vistas = 0;
    for (i = 0; i < ra.nRecs; i++) {
      int est = ra.recs[i].estado;
      if (ra.recs[i].terminou || est == REC_REC_TERMINOU) vistas++;
      if (p->nMandou < SV_FILA_MAX) {
        SvEnviada *m = &p->mandou[p->nMandou++];
        memset(m, 0, sizeof *m);
        snprintf(m->imdb, sizeof m->imdb, "%s", ra.recs[i].imdb);
        snprintf(m->titulo, sizeof m->titulo, "%s", ra.recs[i].titulo);
        snprintf(m->poster, sizeof m->poster, "%s", ra.recs[i].poster);
        m->estado = ra.recs[i].terminou || est == REC_REC_TERMINOU ? SV_REC_VIU
                  : est == REC_REC_COMECOU ? SV_REC_COMECOU
                  : est == REC_REC_REAGIU ? SV_REC_REAGIU
                  : est == REC_REC_ABERTA ? SV_REC_ABRIU : SV_REC_ENTREGUE;
        m->reacao = ra.recs[i].temReacao ? ra.recs[i].reacao : SV_REAC_NADA;
        m->quando = ra.recs[i].criado;
        snprintf(m->resposta, sizeof m->resposta, "%s", ra.recs[i].resposta);
        m->respondido = ra.recs[i].respondido;
      }
    }
    if (ra.nRecs > 0) { p->recsVistas = vistas; p->recsTotal = ra.nRecs; } }
  // Guarda o que veio, para a cadeia da linha do amigo (socialvis_ultima_enviada).
  socialvis_definir_perfil_extra(id, p);
  return 1;
}
#else
static int svDoServidor(SvEvento *saida, int max) {
  (void)saida; (void)max;
  return -1;
}
static int svPerfilDoServidor(const char *id, SvPerfil *p) {
  (void)id; (void)p;
  return -1;
}
#endif

// --- agrupamento ------------------------------------------------------------

static int pesoAcao(int acao) { return acao == SV_AGORA ? 0 : 1; }

// Mais novo primeiro; "agora" na frente; sem hora vai para o fim, na ordem em
// que chegou (a fileira do Trakt ja vem do mais novo).
static int ordemEvento(const void *pa, const void *pb) {
  const SvEvento *a = (const SvEvento *)pa, *b = (const SvEvento *)pb;
  int d = pesoAcao(a->acao) - pesoAcao(b->acao);
  if (d) return d;
  if ((a->quando > 0) != (b->quando > 0)) return a->quando > 0 ? -1 : 1;
  if (a->quando != b->quando) return a->quando > b->quando ? -1 : 1;
  return 0;
}

// INSERCAO, e nao qsort: o empate (dois eventos sem hora) tem de manter a ordem
// de chegada, e qsort nao e estavel. Sao no maximo SV_EVENTOS_MAX itens.
static void ordenar(void *base, int n, size_t tam, int (*cmp)(const void *, const void *)) {
  unsigned char *v = (unsigned char *)base, t[sizeof(SvEvento) > sizeof(SvAmigo)
                                               ? sizeof(SvEvento) : sizeof(SvAmigo)];
  int i, j;
  for (i = 1; i < n; i++) {
    memcpy(t, v + (size_t)i * tam, tam);
    for (j = i; j > 0 && cmp(v + (size_t)(j - 1) * tam, t) > 0; j--)
      memcpy(v + (size_t)j * tam, v + (size_t)(j - 1) * tam, tam);
    memcpy(v + (size_t)j * tam, t, tam);
  }
}

static int achaAmigo(const char *id) {
  int i;
  for (i = 0; i < nAmigos; i++) if (!strcmp(amigos[i].id, id)) return i;
  return -1;
}

static SvAmigo *amigoDe(const SvEvento *e) {
  int k = achaAmigo(e->pessoaId);
  SvAmigo *a;
  if (k >= 0) return &amigos[k];
  if (nAmigos >= SV_AMIGOS_MAX) return NULL;
  a = &amigos[nAmigos++];
  memset(a, 0, sizeof *a);
  snprintf(a->id, sizeof a->id, "%s", e->pessoaId);
  snprintf(a->nome, sizeof a->nome, "%s", e->pessoaNome);
  snprintf(a->avatar, sizeof a->avatar, "%s", e->pessoaAvatar);
  a->fonte = e->fonte;
  return a;
}

static int ordemAmigo(const void *pa, const void *pb) {
  const SvAmigo *a = (const SvAmigo *)pa, *b = (const SvAmigo *)pb;
  // A NOVIDADE NAO ORDENA: ela apaga quando a pessoa olha o rosto, e se
  // ordenasse a fileira trocaria de lugar debaixo do foco nesse instante.
  if (a->agora != b->agora) return b->agora - a->agora;
  if ((a->nTit > 0) != (b->nTit > 0)) return a->nTit > 0 ? -1 : 1;
  if (a->nTit > 0 && b->nTit > 0 && a->tit[0].quando != b->tit[0].quando)
    return a->tit[0].quando > b->tit[0].quando ? -1 : 1;
  return 0;
}

// --- a limpeza do feed (ver socialvis.h) --------------------------------------

static int mesmoTitulo(const SvEvento *a, const SvEvento *b) {
  return !strcmp(a->pessoaId, b->pessoaId) && !strcmp(a->imdb, b->imdb);
}
// Fica de fora do agrupamento: o ao vivo e o que me mandaram tem linha propria.
static int agrupavel(int acao) {
  return acao == SV_FIM || acao == SV_REACAO || acao == SV_AVALIOU || acao == SV_INICIO;
}

int socialvis_feed_limpar(const SvEvento *ev, int n, SvEvento *saida, int max) {
  int i, j, k = 0;
  for (i = 0; ev && i < n && k < max; i++) {
    const SvEvento *e = &ev[i];
    SvEvento linha;
    const SvEvento *fim = NULL, *reac = NULL, *nota = NULL, *ini = NULL;
    int eps = 0, novo = 1;
    if (e->acao == SV_AGORA || e->acao == SV_MANDOU) { saida[k++] = *e; continue; }
    if (!agrupavel(e->acao)) continue;          // abandono, salvo, atividade
    // O grupo ja saiu na primeira (mais nova) linha dele.
    for (j = 0; j < i; j++) if (agrupavel(ev[j].acao) && mesmoTitulo(&ev[j], e)) { novo = 0; break; }
    if (!novo) continue;
    for (j = i; j < n; j++) {
      const SvEvento *o = &ev[j];
      if (!agrupavel(o->acao) || !mesmoTitulo(o, e)) continue;
      if (o->acao == SV_FIM) {
        int q, rep = 0;
        if (!fim) fim = o;
        // Episodios DISTINTOS: o mesmo episodio visto duas vezes conta um.
        for (q = i; q < j; q++)
          if (ev[q].acao == SV_FIM && mesmoTitulo(&ev[q], o) && ev[q].temporada == o->temporada &&
              ev[q].episodio == o->episodio) rep = 1;
        if (!rep) eps += o->eps > 1 ? o->eps : 1;
      }
      else if (o->acao == SV_REACAO) { if (!reac) reac = o; }
      else if (o->acao == SV_AVALIOU) { if (!nota) nota = o; }
      else if (!ini) ini = o;
    }
    if (fim) {
      linha = *fim;
      linha.eps = !strcmp(fim->tipo, "series") && eps > 1 ? eps : 0;
      if (linha.reacao == SV_REAC_NADA && reac) linha.reacao = reac->reacao;
      if (!linha.nota && nota) linha.nota = nota->nota;
    } else if (reac) {
      linha = *reac;
      if (!linha.nota && nota) linha.nota = nota->nota;
    } else if (nota) {
      linha = *nota;
    } else {
      // So comecou: e noticia uma vez, numa serie. Filme comecado, nao.
      if (!ini || strcmp(ini->tipo, "series")) continue;
      linha = *ini;
    }
    // A hora e a da linha MAIS NOVA do grupo (a ordem do feed nao muda).
    linha.quando = e->quando;
    saida[k++] = linha;
  }
  return k;
}

static void agrupar(void) {
  SvEvento *ord;
  int i;
  ord = brutos;
  // O vetor bruto e ordenado no lugar: ele so serve a isto.
  ordenar(ord, nBrutos, sizeof *ord, ordemEvento);
  nFeed = socialvis_feed_limpar(ord, nBrutos, feed, SV_EVENTOS_MAX);
  nAmigos = 0;
  for (i = 0; i < nBrutos; i++) {
    const SvEvento *e = &ord[i];
    SvAmigo *a;
    // O rosto mostra o que a PESSOA fez; o que ela me mandou fica na aba.
    // Sair do player e salvar nao sao noticia (o feed limpo, 06/10/2026):
    // tambem nao viram cartao ao lado do rosto.
    if (e->acao == SV_MANDOU || e->acao == SV_ABANDONO || e->acao == SV_SALVO) continue;
    a = amigoDe(e);
    if (!a) continue;
    if (e->acao == SV_AGORA) a->agora = 1;
    if (a->nTit < SV_TIT_MAX) {
      int j, rep = 0;
      for (j = 0; j < a->nTit; j++) if (!strcmp(a->tit[j].imdb, e->imdb)) rep = 1;
      if (!rep) a->tit[a->nTit++] = *e;
    }
  }
  // Os amigos sem atividade: ainda sao gente da lista e ganham o rosto.
  for (i = 0; i < nCtts && nAmigos < SV_AMIGOS_MAX; i++) {
    SvAmigo *a;
    if (achaAmigo(ctts[i].id) >= 0) {
      // O nome e a foto do CONTATO ganham do feed: e o que a pessoa escolheu.
      a = &amigos[achaAmigo(ctts[i].id)];
      if (ctts[i].nome[0]) snprintf(a->nome, sizeof a->nome, "%s", ctts[i].nome);
      if (ctts[i].avatar[0]) snprintf(a->avatar, sizeof a->avatar, "%s", ctts[i].avatar);
      snprintf(a->apelido, sizeof a->apelido, "%s", ctts[i].apelido);
      snprintf(a->selo, sizeof a->selo, "%s", ctts[i].selo);
      continue;
    }
    a = &amigos[nAmigos++];
    memset(a, 0, sizeof *a);
    snprintf(a->id, sizeof a->id, "%s", ctts[i].id);
    snprintf(a->nome, sizeof a->nome, "%s", ctts[i].nome);
    snprintf(a->avatar, sizeof a->avatar, "%s", ctts[i].avatar);
    snprintf(a->apelido, sizeof a->apelido, "%s", ctts[i].apelido);
    snprintf(a->selo, sizeof a->selo, "%s", ctts[i].selo);
    a->fonte = !strcmp(ctts[i].origem, "trakt") ? SV_FONTE_TRAKT : SV_FONTE_NUVIO;
  }
  for (i = 0; i < nAmigos; i++) {
    unsigned ass = assinatura(&amigos[i]), visto;
    amigos[i].novo = ass && (!vistoDe(amigos[i].id, &visto) || visto != ass);
  }
  ordenar(amigos, nAmigos, sizeof *amigos, ordemAmigo);
}

// The unified feed can contain more titles than discovery's social batch.
// Seed those titles together so Friends focus has a catalog item for the hero.
// This performs no network I/O: the shared image resolver owns throttling,
// fallback and negative caching, just as it does for other catalog rows.
static void completarArteFeed(void) {
  CatItem *novos = NULL;
  int nNovos = 0;
  for (int i = 0; i < nBrutos; i++) {
    SvEvento *e = &brutos[i];
    int k = cat_indice_por_imdb(e->imdb);
    const CatItem *it = k >= 0 ? cat_item(k) : NULL;
    if (!it) {
      for (int j = 0; j < nNovos; j++)
        if (!strcmp(novos[j].imdb, e->imdb)) { it = &novos[j]; break; }
    }
    if (!it && !strncmp(e->imdb, "tt", 2)) {
      if (!novos) novos = calloc((size_t)nBrutos, sizeof *novos);
      if (!novos) break;
      CatItem *d = &novos[nNovos++];
      snprintf(d->imdb, sizeof d->imdb, "%s", e->imdb);
      snprintf(d->tipo, sizeof d->tipo, "%s", e->tipo);
      snprintf(d->titulo, sizeof d->titulo, "%s", e->titulo);
      snprintf(d->poster, sizeof d->poster, "%s", e->poster);
      snprintf(d->backdrop, sizeof d->backdrop, "%s", e->arte);
      arte_metahub_preencher(d);
      it = d;
    }
    if (it) {
      if (!e->poster[0]) snprintf(e->poster, sizeof e->poster, "%s", it->poster);
      if (!e->arte[0]) snprintf(e->arte, sizeof e->arte, "%s", it->backdrop);
    }
  }
  if (nNovos) cat_acrescentar_lote(novos, nNovos, NULL);
  free(novos);
}

static void refazer(void) {
  unsigned h;
  if (temExterno) {
    nBrutos = nExterno < SV_EVENTOS_MAX ? nExterno : SV_EVENTOS_MAX;
    if (nBrutos) memcpy(brutos, externo, sizeof *brutos * (size_t)nBrutos);
    nCtts = recomenda_contatos(ctts, REC_CONTATOS_MAX);
  } else {
    int n = svDoServidor(brutos, SV_EVENTOS_MAX);
    if (n >= 0) { nBrutos = n; nCtts = recomenda_contatos(ctts, REC_CONTATOS_MAX); dasRecomendacoes(); }
    else svDoQueExiste();
  }
  completarArteFeed();
  agrupar();
  h = fnv(2166136261u, amigos, sizeof *amigos * (size_t)nAmigos);
  h = fnv(h, feed, sizeof *feed * (size_t)nFeed);
  if (h != hashUlt || !rev) { hashUlt = h; rev++; }
}

// Consume account changes here, on the UI thread. recomenda_esquecer also runs
// on the service thread: it only changes its own generation under its mutex.
static void conferirIdentidade(void) {
  unsigned g = recomenda_geracao();
  if (temGeracao && g != geracaoVista) {
    free(externo); externo = NULL; nExterno = temExterno = 0;
    free(extras); extras = NULL; nExtras = 0;
    nBrutos = nFeed = nAmigos = nCtts = nVistos = 0;
    memset(vistos, 0, sizeof vistos); vistosLidos = 0;
    perfilPedido[0] = 0;
    catRevUlt = ~0u; recNUlt = -1; hashUlt = 0; refeitoEm = 0;
    rev++;
  }
  temGeracao = 1; geracaoVista = g;
}

void socialvis_abrir_perfil(const char *id) {
  conferirIdentidade();
  if (!id || !id[0]) return;
  snprintf(perfilPedido, sizeof perfilPedido, "%s", id);
#ifdef NV_SOCIAL_V2
  if (recomenda_ativo()) recomenda_amigo_pedir(id);
#endif
}

void socialvis_atualizar(void) {
  Uint32 agora = SDL_GetTicks();
  conferirIdentidade();
  unsigned cr = cat_revisao();
  int rn = recomenda_n();
  if (rev && cr == catRevUlt && rn == recNUlt && agora - refeitoEm < SV_REFAZ_MS) return;
  if (rev && temExterno && cr == catRevUlt && rn == recNUlt) return;
  catRevUlt = cr; recNUlt = rn; refeitoEm = agora;
  refazer();
}

unsigned socialvis_revisao(void) { return rev; }
int socialvis_n_amigos(void) { return nAmigos; }
const SvAmigo *socialvis_amigo(int i) { return (i >= 0 && i < nAmigos) ? &amigos[i] : NULL; }
int socialvis_amigo_indice(const char *id) { return id ? achaAmigo(id) : -1; }
int socialvis_n_ao_vivo(void) {
  int i, n = 0;
  for (i = 0; i < nAmigos; i++) n += amigos[i].agora;
  return n;
}
int socialvis_n_eventos(void) { return nFeed; }
int socialvis_n_brutos(void) { return nBrutos; }
const SvEvento *socialvis_bruto(int i) { return (i >= 0 && i < nBrutos) ? &brutos[i] : NULL; }

int socialvis_enviada_titulo(const char *id, const char *imdb, SvEnviada *saida) {
  int i, j;
  if (!id || !imdb || !imdb[0]) return 0;
  for (i = 0; i < nExtras; i++) {
    if (strcmp(extras[i].id, id)) continue;
    for (j = 0; j < extras[i].p.nMandou && j < SV_FILA_MAX; j++)
      if (!strcmp(extras[i].p.mandou[j].imdb, imdb)) {
        if (saida) *saida = extras[i].p.mandou[j];
        return 1;
      }
  }
  return 0;
}

int socialvis_gosto_pct(const char *id) {
  int i;
  if (!id) return -1;
  for (i = 0; i < nExtras; i++)
    if (!strcmp(extras[i].id, id))
      return extras[i].p.gostoTotal >= SV_CMP_MIN && extras[i].p.gostoPct >= 0 ? extras[i].p.gostoPct : -1;
  return -1;
}
const SvEvento *socialvis_evento(int i) { return (i >= 0 && i < nFeed) ? &feed[i] : NULL; }

void socialvis_definir_feed(const SvEvento *ev, int n) {
  if (n < 0) n = 0;
  if (n > SV_EVENTOS_MAX) n = SV_EVENTOS_MAX;
  free(externo);
  externo = n ? (SvEvento *)malloc(sizeof *ev * (size_t)n) : NULL;
  nExterno = externo ? n : 0;
  if (externo) memcpy(externo, ev, sizeof *ev * (size_t)n);
  temExterno = 1;
  catRevUlt = ~0u;
  refazer();
}

void socialvis_definir_perfil_extra(const char *id, const SvPerfil *extra) {
  int i;
  SvExtra *novo;
  if (!id || !extra) return;
  for (i = 0; i < nExtras; i++)
    if (!strcmp(extras[i].id, id)) { extras[i].p = *extra; return; }
  novo = (SvExtra *)realloc(extras, sizeof *extras * (size_t)(nExtras + 1));
  if (!novo) return;
  extras = novo;
  snprintf(extras[nExtras].id, sizeof extras[nExtras].id, "%s", id);
  extras[nExtras].p = *extra;
  nExtras++;
}

int socialvis_perfil(const char *id, SvPerfil *p) {
  int k, i, servidor;
  if (!id || !p) return 0;
  conferirIdentidade();
  k = achaAmigo(id);
  if (k < 0) return 0;
  memset(p, 0, sizeof *p);
  p->a = amigos[k];
  p->compartilha = -1;
  p->porOnde = amigos[k].fonte;
  p->gostoPct = p->emComum = p->gostoTotal = -1;
  p->minutosMes = p->filmesMes = p->seriesCurso = -1;
  p->recsVistas = p->recsTotal = -1;
  { SvCmpDados nada; memset(&nada, 0, sizeof nada); nada.compartilha = -1;
    socialvis_comparar(&nada, p->cmp); }
  // O SERVIDOR PRIMEIRO (com NV_SOCIAL_V2): ele e a verdade e muda; o extra
  // guardado e o que valia na ultima leitura (ou os dados de exemplo).
  servidor = svPerfilDoServidor(id, p);
  if (servidor < 0)
    for (i = 0; i < nExtras; i++)
      if (!strcmp(extras[i].id, id)) {
        SvAmigo base = p->a;
        *p = extras[i].p;
        p->a = base;
        break;
      }
  if (p->estado == SV_PERFIL_NEGADO || p->compartilha == 0) {
    p->a.agora = p->a.nTit = 0;
    p->nAssistindo = p->nGostou = 0;
    return 1;
  }
  if (p->estado == SV_PERFIL_NAO_ACHOU) {
    p->a.nTit = p->a.agora = 0;
    for (i = 0; i < nBrutos; i++) {
      const SvEvento *e = &brutos[i];
      int j, repetido = 0;
      if (strcmp(e->pessoaId, id) || e->fonte != SV_FONTE_TRAKT || e->acao == SV_MANDOU) continue;
      p->a.fonte = SV_FONTE_TRAKT;
      if (e->pessoaNome[0]) snprintf(p->a.nome, sizeof p->a.nome, "%s", e->pessoaNome);
      if (e->pessoaAvatar[0]) snprintf(p->a.avatar, sizeof p->a.avatar, "%s", e->pessoaAvatar);
      if (e->acao == SV_AGORA) p->a.agora = 1;
      for (j = 0; j < p->a.nTit; j++) if (!strcmp(p->a.tit[j].imdb, e->imdb)) repetido = 1;
      if (!repetido && p->a.nTit < SV_TIT_MAX) p->a.tit[p->a.nTit++] = *e;
    }
  }
  // An unqualified rating does not prove a positive reaction. Only explicitly
  // shared positive reactions belong in Recently liked.
  if (!p->nAssistindo || !p->nGostou) {
    int fazA = !p->nAssistindo, fazG = !p->nGostou;
    for (i = 0; i < nBrutos; i++) {
      const SvEvento *e = &brutos[i];
      if (strcmp(e->pessoaId, id) || e->acao == SV_MANDOU) continue;
      // The worker does not own public Trakt data. Its generic 404 cannot
      // establish that an independently authorized tracker source is empty.
      if ((p->estado == SV_PERFIL_NAO_ACHOU || servidor == 1) && e->fonte != SV_FONTE_TRAKT) continue;
      if (fazA && (e->acao == SV_AGORA || e->acao == SV_INICIO) && p->nAssistindo < SV_FILA_MAX)
        p->assistindo[p->nAssistindo++] = *e;
      if (fazG && e->reacao == SV_REAC_GOSTOU && p->nGostou < SV_FILA_MAX) {
        int j;
        // The latest explicit opinion wins, even if it is negative/neutral.
        for (j = 0; j < i; j++) {
          const SvEvento *o = &brutos[j];
          if (o->reacao != SV_REAC_NADA && !strcmp(o->pessoaId, e->pessoaId) &&
              !strcmp(o->imdb, e->imdb) && !strcmp(o->tipo, e->tipo) &&
              o->temporada == e->temporada && o->episodio == e->episodio) break;
        }
        if (j == i) p->gostou[p->nGostou++] = *e;
      }
    }
  }
  return 1;
}

const char *socialvis_enviada_rotulo(const SvEnviada *m, int *ok) {
  int viu = m && m->estado == SV_REC_VIU;
  if (ok) *ok = viu;
  if (!m) return i18n("ainda não viu");
  if (viu)
    return i18n(m->reacao == SV_REAC_GOSTOU ? "viu · gostou"
              : m->reacao == SV_REAC_MEIO   ? "viu · mais ou menos"
              : m->reacao == SV_REAC_NAO    ? "viu · não gostou" : "viu");
  switch (m->estado) {
    case SV_REC_COMECOU: return i18n("começou");
    case SV_REC_REAGIU:
      return i18n(m->reacao == SV_REAC_GOSTOU ? "Gostou"
                : m->reacao == SV_REAC_NAO ? "Não gostou" : "Mais ou menos");
    case SV_REC_ABRIU: return i18n("abriu");
    default: return i18n("ainda não viu");
  }
}

int socialvis_ultima_enviada(const char *id, SvEnviada *saida) {
  int i;
  if (!id) return 0;
  conferirIdentidade();
  for (i = 0; i < nExtras; i++)
    if (!strcmp(extras[i].id, id) && extras[i].p.nMandou > 0) {
      if (saida) *saida = extras[i].p.mandou[0];
      return 1;
    }
  return 0;
}

int socialvis_meu_estado(const char *imdb, int *pct, int *t, int *e) {
  int k;
  const CatItem *it;
  if (pct) *pct = 0;
  if (t) *t = 0;
  if (e) *e = 0;
  if (!imdb || !imdb[0]) return 0;
  k = cat_indice_por_imdb(imdb);
  it = k >= 0 ? cat_item(k) : NULL;
  if (!it) return 0;
  if (cat_visto(it)) {
    if (t) *t = it->temporada;
    if (e) *e = it->episodio;
    return 2;
  }
  if (it->progresso > 0) {
    if (pct) *pct = it->progresso;
    if (t) *t = it->temporada;
    if (e) *e = it->episodio;
    return 1;
  }
  return 0;
}

// --- textos -----------------------------------------------------------------

// --- comparacao (F08) --------------------------------------------------------

static void cmpPct(SvCmp *c, int total, int iguais) {
  c->total = total; c->iguais = iguais;
  if (total < SV_CMP_MIN) { c->estado = SV_CMPE_POUCOS; c->pct = -1; return; }
  c->estado = SV_CMPE_OK;
  c->pct = (100 * iguais + total / 2) / total;
}

void socialvis_comparar(const SvCmpDados *d, SvCmp out[SV_CMP_N]) {
  int i, todos;
  memset(out, 0, sizeof(SvCmp) * SV_CMP_N);
  for (i = 0; i < SV_CMP_N; i++) out[i].pct = -1;
  // GENEROS: os eventos nao carregam genero em fonte nenhuma. Sempre explicito.
  out[SV_CMP_GENEROS].estado = SV_CMPE_SEM_FONTE;
  if (!d) return;
  todos = !d->temDados ? (d->carregando ? SV_CMPE_CARREGANDO : SV_CMPE_DESCONHECIDO)
        : d->compartilha == 0 ? SV_CMPE_PRIVADO
        : d->compartilha < 0 ? SV_CMPE_DESCONHECIDO
        : !d->euCompartilho ? SV_CMPE_EU_PRIVADO : -1;
  if (todos >= 0) {
    for (i = 0; i < SV_CMP_GENEROS; i++) out[i].estado = todos;
    return;
  }
  // Servidor novo: o objeto vem sempre que os dois compartilham, mesmo com
  // zero pares. Antigo: so ha o total geral, e so quando ha algum par.
  if (d->temCmp || d->temGosto) cmpPct(&out[SV_CMP_MATCH], d->total, d->iguais);
  if (d->temCmp) {
    cmpPct(&out[SV_CMP_FILMES], d->filmesTotal, d->filmesIguais);
    cmpPct(&out[SV_CMP_SERIES], d->seriesTotal, d->seriesIguais);
    out[SV_CMP_COMUM].estado = SV_CMPE_OK;
    out[SV_CMP_COMUM].filmes = d->comumFilmes;
    out[SV_CMP_COMUM].series = d->comumSeries;
  }
}

const char *socialvis_cmp_rotulo(int qual) {
  switch (qual) {
    case SV_CMP_MATCH:  return i18n("Match");
    case SV_CMP_FILMES: return i18n("Filmes");
    case SV_CMP_SERIES: return i18n("Séries");
    case SV_CMP_COMUM:  return i18n("Vistos pelos dois");
    default:            return i18n("Gêneros");
  }
}

void socialvis_cmp_texto(int qual, const SvCmp *c, char *dst, size_t tam, int *ok) {
  if (ok) *ok = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!c) return;
  switch (c->estado) {
    case SV_CMPE_OK:
      if (ok) *ok = 1;
      if (qual == SV_CMP_COMUM)
        snprintf(dst, tam, i18n("%d filmes · %d séries"), c->filmes, c->series);
      else snprintf(dst, tam, i18n("%d%% · %d de %d iguais"), c->pct, c->iguais, c->total);
      return;
    case SV_CMPE_POUCOS:
      snprintf(dst, tam, i18n("Poucos dados · %d de %d"), c->total, SV_CMP_MIN);
      return;
    case SV_CMPE_PRIVADO:    snprintf(dst, tam, "%s", i18n("Privado")); return;
    case SV_CMPE_EU_PRIVADO: snprintf(dst, tam, "%s", i18n("Ative sua atividade para comparar")); return;
    case SV_CMPE_SEM_FONTE:  snprintf(dst, tam, "%s", i18n("Sem dados de gênero")); return;
    case SV_CMPE_CARREGANDO: snprintf(dst, tam, "%s", i18n("Carregando…")); return;
    default:                 snprintf(dst, tam, "%s", i18n("Desconhecido")); return;
  }
}

void socialvis_ep(const SvEvento *ev, char *dst, size_t tam) {
  dst[0] = 0;
  if (ev && ev->temporada > 0 && ev->episodio > 0)
    snprintf(dst, tam, i18n("T%dE%d"), ev->temporada, ev->episodio);
}

void socialvis_quando(long long quando, char *dst, size_t tam) {
  long long d = (long long)time(NULL) - quando;
  dst[0] = 0;
  if (quando <= 0) return;
  if (d < 0) d = 0;
  if (d < 90) snprintf(dst, tam, "%s", i18n("agora mesmo"));
  else if (d < 5400) snprintf(dst, tam, i18n("há %d min"), (int)(d / 60));
  else {
    time_t q = (time_t)quando, n = time(NULL);
    struct tm tq, tn;
    localtime_r(&q, &tq);
    localtime_r(&n, &tn);
    if (tq.tm_year == tn.tm_year && tq.tm_yday == tn.tm_yday)
      hora_tela(dst, tam, &tq);
    else if (d < 172800) snprintf(dst, tam, "%s", i18n("ontem"));
    else if (d < 7 * 86400) snprintf(dst, tam, i18n("há %d dias"), (int)(d / 86400));
    else snprintf(dst, tam, "%02d/%02d", tq.tm_mday, tq.tm_mon + 1);
  }
}

const char *socialvis_verbo(const SvEvento *ev) {
  if (!ev) return "";
  if (ev->sobreMinhaRec) return i18n("viu o que você mandou");
  switch (ev->acao) {
    case SV_AGORA:    return i18n("está vendo");
    case SV_INICIO:   return !strcmp(ev->tipo, "series") ? i18n("começou a série") : i18n("começou");
    case SV_FIM:
      // "assistiu 3 episódios de" (o feed agrupado), com a reacao quando houve.
      if (ev->eps > 1) {
        static char buf[96];
        // Uma chamada de i18n por frase: a varredura so acha o literal direto.
        const char *f = ev->reacao == SV_REAC_GOSTOU ? i18n("viu %d episódios e gostou de")
                      : ev->reacao == SV_REAC_NAO ? i18n("viu %d episódios e não gostou de")
                      : i18n("assistiu %d episódios de");
        snprintf(buf, sizeof buf, f, ev->eps);
        return buf;
      }
      return ev->reacao == SV_REAC_GOSTOU ? i18n("terminou e gostou")
           : ev->reacao == SV_REAC_NAO ? i18n("terminou e não gostou")
           : ev->reacao == SV_REAC_MEIO ? i18n("terminou, achou mais ou menos")
           : i18n("terminou");
    case SV_REACAO:   return ev->reacao == SV_REAC_GOSTOU ? i18n("gostou de")
                           : ev->reacao == SV_REAC_NAO ? i18n("não gostou de")
                           : i18n("achou mais ou menos");
    case SV_SALVO:    return i18n("salvou");
    case SV_ABANDONO: return i18n("parou de ver");
    case SV_AVALIOU:  return i18n("avaliou");
    case SV_MANDOU:   return i18n("te mandou");
    default:          return "";
  }
}

void socialvis_status(const SvEvento *ev, char *dst, size_t tam) {
  char ep[24], q[48], falta[48];
  dst[0] = 0;
  if (!ev) return;
  socialvis_ep(ev, ep, sizeof ep);
  socialvis_quando(ev->quando, q, sizeof q);
  falta[0] = 0;
  if (ev->restanteMin > 0) snprintf(falta, sizeof falta, i18n("faltam %d min"), ev->restanteMin);
  switch (ev->acao) {
    case SV_AGORA: {
      // "Agora · T3E4 · faltam 12 min", cada pedaco so quando existe.
      size_t k = (size_t)snprintf(dst, tam, "%s", i18n("Agora"));
      if (ep[0] && k < tam) k += (size_t)snprintf(dst + k, tam - k, " \xc2\xb7 %s", ep);
      if (falta[0] && k < tam) snprintf(dst + k, tam - k, " \xc2\xb7 %s", falta);
      return; }
    case SV_ABANDONO:
      if (ev->pct >= 0) snprintf(dst, tam, i18n("Parou aos %d%%"), ev->pct);
      else snprintf(dst, tam, "%s", i18n("Parou de ver"));
      break;
    case SV_FIM:
      snprintf(dst, tam, "%s", ev->reacao == SV_REAC_GOSTOU ? i18n("Terminou e gostou")
                                                           : i18n("Terminou"));
      break;
    case SV_INICIO: snprintf(dst, tam, "%s", i18n("Começou")); break;
    case SV_REACAO:
      snprintf(dst, tam, "%s", ev->reacao == SV_REAC_GOSTOU ? i18n("Gostou")
                             : ev->reacao == SV_REAC_NAO ? i18n("Não gostou")
                             : i18n("Mais ou menos"));
      break;
    case SV_SALVO:   snprintf(dst, tam, "%s", i18n("Salvou para ver")); break;
    case SV_AVALIOU: snprintf(dst, tam, "%s", i18n("Avaliou")); break;
    case SV_MANDOU:  snprintf(dst, tam, "%s", i18n("Te mandou")); break;
    default:         snprintf(dst, tam, "%s", i18n("Assistiu")); break;
  }
  if (ep[0] && ev->acao != SV_AGORA) {
    size_t k = strlen(dst);
    if (k < tam) snprintf(dst + k, tam - k, " \xc2\xb7 %s", ep);
  }
  if (q[0]) {
    size_t k = strlen(dst);
    if (k < tam) snprintf(dst + k, tam - k, " \xc2\xb7 %s", q);
  }
}

const char *socialvis_fonte_nome(int fonte) {
  switch (fonte) {
    case SV_FONTE_TRAKT:      return "Trakt";
    case SV_FONTE_SIMKL:      return "Simkl";
    case SV_FONTE_LETTERBOXD: return "Letterboxd";
    default:                  return "Nuvio";
  }
}

int socialvis_dia(const SvEvento *ev, char *rot, size_t tam) {
  time_t q, n = time(NULL);
  struct tm tq, tn;
  long long d;
  if (ev->acao == SV_AGORA) { snprintf(rot, tam, "%s", i18n("Agora")); return 0; }
  if (ev->quando <= 0) { snprintf(rot, tam, "%s", i18n("Recentes")); return 4; }
  q = (time_t)ev->quando;
  localtime_r(&q, &tq);
  localtime_r(&n, &tn);
  if (tq.tm_year == tn.tm_year && tq.tm_yday == tn.tm_yday) {
    snprintf(rot, tam, "%s", i18n("Hoje")); return 1;
  }
  d = (long long)n - ev->quando;
  if (d < 172800 && (tn.tm_yday - tq.tm_yday == 1 || (tn.tm_yday == 0 && d < 86400 * 2))) {
    snprintf(rot, tam, "%s", i18n("Ontem")); return 2;
  }
  snprintf(rot, tam, "%02d/%02d", tq.tm_mday, tq.tm_mon + 1);
  return 3;
}

// --- dados de exemplo (so para as capturas) -----------------------------------
#ifdef NV_SOCIALVIS_DEMO
#define SV_DEMO_ARTE "deploy/app/art"
static void ev(SvEvento *e, const char *pid, const char *nome, const char *av, int fonte,
               int acao, int reacao, const char *imdb, const char *tipo, const char *titulo,
               int arte, int t, int epi, int pct, int falta, long long quando) {
  memset(e, 0, sizeof *e);
  snprintf(e->pessoaId, sizeof e->pessoaId, "%s", pid);
  snprintf(e->pessoaNome, sizeof e->pessoaNome, "%s", nome);
  snprintf(e->pessoaAvatar, sizeof e->pessoaAvatar, "%s", av);
  e->fonte = fonte; e->acao = acao; e->reacao = reacao;
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  snprintf(e->tipo, sizeof e->tipo, "%s", tipo);
  snprintf(e->titulo, sizeof e->titulo, "%s", titulo);
  snprintf(e->poster, sizeof e->poster, "%s/poster/%02d.jpg", SV_DEMO_ARTE, arte);
  snprintf(e->arte, sizeof e->arte, "%s/%02d.jpg", SV_DEMO_ARTE, arte);
  e->temporada = t; e->episodio = epi; e->pct = pct; e->restanteMin = falta;
  e->quando = quando;
}

void socialvis_demo(int cenario) {
  static SvEvento v[16];
  long long agora = (long long)time(NULL);
  int n = 0;
  SvPerfil p;
  nExtras = 0;
  if (cenario >= 1) {
    ev(&v[n++], "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", SV_FONTE_NUVIO,
       SV_FIM, SV_REAC_GOSTOU, "tt0000101", "movie", "Project Hail Mary", 3, 0, 0, -1, -1,
       agora - 3 * 3600);
    ev(&v[n++], "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", SV_FONTE_NUVIO,
       SV_REACAO, SV_REAC_GOSTOU, "tt0000102", "movie", "Duna: Parte Dois", 5, 0, 0, -1, -1,
       agora - 5 * 3600);
    v[n - 1].sobreMinhaRec = 1;
    ev(&v[n++], "nuvio:pedro", "Pedro", "deploy/app/art/elenco/00_0.jpg", SV_FONTE_NUVIO,
       SV_INICIO, SV_REAC_NADA, "tt0000103", "series", "Silo", 7, 2, 5, 40, -1,
       agora - 30 * 3600);
  }
  if (cenario >= 3) {
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_SALVO, SV_REAC_NADA, "tt0000104", "series", "Ruptura", 9, 0, 0, -1, -1,
       agora - 26 * 3600);
    ev(&v[n++], "nuvio:marina", "Marina", "", SV_FONTE_NUVIO,
       SV_ABANDONO, SV_REAC_NAO, "tt0000105", "movie", "Alien: Romulus", 11, 0, 0, 18, -1,
       agora - 4 * 86400);
    ev(&v[n++], "nuvio:marina", "Marina", "", SV_FONTE_NUVIO,
       SV_FIM, SV_REAC_GOSTOU, "tt0000106", "series", "O Urso", 13, 3, 3, -1, -1,
       agora - 2 * 86400);
  }
  if (cenario >= 4) {
    ev(&v[n++], "nuvio:marina", "Marina", "", SV_FONTE_NUVIO,
       SV_AGORA, SV_REAC_NADA, "tt0000106", "series", "O Urso", 13, 3, 4, 64, 12,
       agora - 18 * 60);
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_MANDOU, SV_REAC_NADA, "tt0000107", "series", "Andor", 15, 0, 0, -1, -1,
       agora - 11 * 86400);
    // O FEED LIMPO (06/10/2026): tres episodios de "Ruptura" do vlern viram UMA
    // linha "assistiu 3 episodios de"; o "salvou" dele some.
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_FIM, SV_REAC_NADA, "tt0000104", "series", "Ruptura", 9, 2, 3, -1, -1,
       agora - 20 * 3600);
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_FIM, SV_REAC_NADA, "tt0000104", "series", "Ruptura", 9, 2, 2, -1, -1,
       agora - 22 * 3600);
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_FIM, SV_REAC_NADA, "tt0000104", "series", "Ruptura", 9, 2, 1, -1, -1,
       agora - 24 * 3600);
    ev(&v[n++], "trakt:vlern", "vlern", "", SV_FONTE_TRAKT,
       SV_AVALIOU, SV_REAC_NADA, "tt0000109", "movie", "Blade Runner 2049", 11, 0, 0, -1, -1,
       agora - 3 * 86400);
    v[n - 1].nota = 90;
  }
  socialvis_definir_feed(v, n);
  if (cenario >= 1) {
    memset(&p, 0, sizeof p);
    p.estado = SV_PERFIL_OK; p.compartilha = 1;
    p.desde = agora - 60 * 86400;
    p.porOnde = SV_FONTE_NUVIO;
    p.gostoPct = 78; p.emComum = 14; p.gostoTotal = 18;
    p.minutosMes = 31 * 60; p.filmesMes = 12; p.seriesCurso = 4;
    p.recsVistas = 6; p.recsTotal = 7;
    p.nMandou = 3;
    snprintf(p.mandou[0].imdb, sizeof p.mandou[0].imdb, "tt0000102");
    snprintf(p.mandou[0].titulo, sizeof p.mandou[0].titulo, "Duna: Parte Dois");
    snprintf(p.mandou[0].poster, sizeof p.mandou[0].poster, "deploy/app/art/poster/05.jpg");
    p.mandou[0].estado = SV_REC_VIU; p.mandou[0].reacao = SV_REAC_GOSTOU;
    p.mandou[0].respondido = agora - 3600;
    snprintf(p.mandou[0].resposta, sizeof p.mandou[0].resposta, "valeu pela dica");
    snprintf(p.mandou[1].imdb, sizeof p.mandou[1].imdb, "tt0000108");
    snprintf(p.mandou[1].titulo, sizeof p.mandou[1].titulo, "A Chegada");
    snprintf(p.mandou[1].poster, sizeof p.mandou[1].poster, "deploy/app/art/poster/17.jpg");
    p.mandou[1].estado = SV_REC_VIU; p.mandou[1].reacao = SV_REAC_MEIO;
    p.mandou[1].respondido = agora - 7200;
    snprintf(p.mandou[1].resposta, sizeof p.mandou[1].resposta, "achei meio lento");
    snprintf(p.mandou[2].imdb, sizeof p.mandou[2].imdb, "tt0000109");
    snprintf(p.mandou[2].titulo, sizeof p.mandou[2].titulo, "Blade Runner 2049");
    snprintf(p.mandou[2].poster, sizeof p.mandou[2].poster, "deploy/app/art/poster/11.jpg");
    p.mandou[2].estado = SV_REC_ENTREGUE; p.mandou[2].reacao = SV_REAC_NADA;
    socialvis_definir_perfil_extra("nuvio:pedro", &p);
  }
}
#endif
