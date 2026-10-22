// Amigos de um titulo. Ver amigostitulo.h.
#include "amigostitulo.h"
#include "idioma.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

#define AMT_NOTA_BOA 70    // nota de tracker (0..100) a partir da qual conta como "gostou"

static AmigosTitulo idx[AMT_TITULOS_MAX];
static int nIdx;
static unsigned rev, socialRev = ~0u;
static Uint32 conferidoEm;

static int acha(const char *imdb) {
  int i;
  for (i = 0; i < nIdx; i++) if (!strcmp(idx[i].imdb, imdb)) return i;
  return -1;
}

// Entre dois pontos do mesmo amigo no mesmo titulo, vale o mais adiantado.
static int adiante(int t1, int e1, int t2, int e2) {
  return t1 != t2 ? t1 > t2 : e1 > e2;
}

// Os eventos chegam do MAIS NOVO para o mais velho: a primeira reacao/nota
// vista de alguem e a que vale ("mudou de ideia" nao vira duas opinioes).
static void aplica(AmigoTit *a, const SvEvento *e) {
  if (e->acao == SV_MANDOU) {
    // Me mandou o titulo: e a informacao mais util de todas, mesmo sem ter
    // visto (quem manda costuma ter visto, mas isso nao se afirma).
    if (!a->recomendou) snprintf(a->recTexto, sizeof a->recTexto, "%s", e->texto);
    a->recomendou = 1;
    return;
  }
  switch (e->acao) {
    case SV_AGORA:
      a->viu = 1; a->agora = 1; break;
    case SV_INICIO:
      a->viu = 1; break;
    case SV_FIM:
      a->viu = 1; a->terminou = 1;
      // O servidor junta ao "terminou" a reacao do mesmo titulo.
      if (a->reacao == SV_REAC_NADA && e->reacao != SV_REAC_NADA) a->reacao = e->reacao;
      break;
    case SV_REACAO:
      // Quem reagiu ao "O que achou?" ja viu.
      a->viu = 1;
      if (a->reacao == SV_REAC_NADA) a->reacao = e->reacao;
      break;
    case SV_AVALIOU:
      a->viu = 1;
      if (!a->nota && e->nota > 0) a->nota = e->nota;
      break;
    default: return;       // salvo, abandono, atividade: nao e "viu" nem opiniao
  }
  if (!strcmp(e->tipo, "series") || e->temporada > 0) a->serie = 1;
  if (e->quando > a->quando) a->quando = e->quando;
  if (e->temporada > 0 && e->episodio > 0 && (e->acao == SV_AGORA || e->acao == SV_INICIO || e->acao == SV_FIM) &&
      adiante(e->temporada, e->episodio, a->temporada, a->episodio)) {
    a->temporada = e->temporada; a->episodio = e->episodio;
  }
}

// Nota sem reacao: abaixo de 5/10 e "nao gostou"; de 5 a 6,9, "mais ou menos".
static int opiniaoNegativa(const AmigoTit *a) {
  return a->reacao == SV_REAC_NAO || (a->reacao == SV_REAC_NADA && a->nota > 0 && a->nota < 50);
}
static int temOpiniao(const AmigoTit *a) { return a->reacao != SV_REAC_NADA || a->nota > 0; }
static int temProgresso(const AmigoTit *a) { return a->agora || (a->serie && a->episodio > 0); }
// 0 recomendou, 1 opinou, 2 progresso, 3 so viu.
static int prioridade(const AmigoTit *a) {
  if (a->recomendou) return 0;
  if (temOpiniao(a)) return 1;
  if (temProgresso(a)) return 2;
  return 3;
}

int amigostitulo_montar(const SvEvento *ev, int n) {
  AmigoTit tmp[AMT_MAX * 2];     // por titulo; o que passa disto e gente demais para o chip
  int i, j, k;
  nIdx = 0;
  for (i = 0; ev && i < n; i++) {
    const SvEvento *e = &ev[i];
    AmigosTitulo *t;
    int nt = 0;
    if (!e->imdb[0] || !e->pessoaId[0] || acha(e->imdb) >= 0 || nIdx >= AMT_TITULOS_MAX) continue;
    t = &idx[nIdx];
    memset(t, 0, sizeof *t);
    snprintf(t->imdb, sizeof t->imdb, "%s", e->imdb);
    // Todos os eventos deste titulo, um registro por amigo (dedupe por pessoaId).
    for (j = i; j < n; j++) {
      int p;
      if (strcmp(ev[j].imdb, e->imdb) || !ev[j].pessoaId[0]) continue;
      for (p = 0; p < nt; p++) if (!strcmp(tmp[p].id, ev[j].pessoaId)) break;
      if (p == nt) {
        if (nt >= AMT_MAX * 2) continue;
        memset(&tmp[p], 0, sizeof tmp[p]);
        snprintf(tmp[p].id, sizeof tmp[p].id, "%s", ev[j].pessoaId);
        snprintf(tmp[p].nome, sizeof tmp[p].nome, "%s", ev[j].pessoaNome);
        snprintf(tmp[p].avatar, sizeof tmp[p].avatar, "%s", ev[j].pessoaAvatar);
        tmp[p].reacao = SV_REAC_NADA;
        tmp[p].gosto = -1;
        nt++;
      }
      aplica(&tmp[p], &ev[j]);
    }
    for (j = 0; j < nt; j++)
      tmp[j].gostou = tmp[j].reacao != SV_REAC_NADA ? tmp[j].reacao == SV_REAC_GOSTOU
                                                   : tmp[j].nota >= AMT_NOTA_BOA;
    // Na ordem da prioridade; dentro dela, quem gostou antes; depois a ordem
    // de chegada (o mais novo).
    for (k = 0; k < 8; k++)
      for (j = 0; j < nt; j++) {
        const AmigoTit *a = &tmp[j];
        if (!a->viu && !a->recomendou) continue;
        if (prioridade(a) != k / 2 || (k % 2 == 0) != (a->gostou != 0)) continue;
        t->total++;
        if (a->gostou) t->nGostou++; else if (a->viu) t->nViu++;
        if (a->recomendou) t->nRecomendou++;
        if (temOpiniao(a)) {
          t->nOpiniao++;
          if (!a->gostou && opiniaoNegativa(a)) t->nNaoGostou++;
          else if (!a->gostou) t->nMeio++;
        }
        if (t->n < AMT_MAX) t->a[t->n++] = *a;
      }
    if (t->total > 0) nIdx++;
  }
  rev++;
  return nIdx;
}

void amigostitulo_atualizar(void) {
  Uint32 agora = SDL_GetTicks();
  unsigned sr;
  int i, n;
  if (rev && agora - conferidoEm < 250u) return;
  conferidoEm = agora;
  socialvis_atualizar();
  sr = socialvis_revisao();
  if (rev && sr == socialRev) return;
  socialRev = sr;
  {
    // Os CRUS, e nao o feed limpo: o feed tira o "comecou" de cada episodio e
    // junta linhas, e aqui o progresso e a reacao de cada um importam.
    static SvEvento ev[SV_EVENTOS_MAX];
    n = socialvis_n_brutos();
    if (n > SV_EVENTOS_MAX) n = SV_EVENTOS_MAX;
    for (i = 0; i < n; i++) ev[i] = *socialvis_bruto(i);
    amigostitulo_montar(ev, n);
  }
  // O nome e a foto do CONTATO ganham os do evento (e o que a pessoa escolheu).
  // A resposta a uma rec minha e o gosto parecido vem do perfil ja carregado.
  for (i = 0; i < nIdx; i++) {
    int j;
    for (j = 0; j < idx[i].n; j++) {
      AmigoTit *at = &idx[i].a[j];
      int k = socialvis_amigo_indice(at->id);
      const SvAmigo *am = k >= 0 ? socialvis_amigo(k) : NULL;
      SvEnviada m;
      if (socialvis_enviada_titulo(at->id, idx[i].imdb, &m) && m.resposta[0])
        snprintf(at->resposta, sizeof at->resposta, "%s", m.resposta);
      at->gosto = socialvis_gosto_pct(at->id);
      if (!am) continue;
      if (am->nome[0]) snprintf(at->nome, sizeof at->nome, "%s", am->nome);
      if (am->avatar[0]) snprintf(at->avatar, sizeof at->avatar, "%s", am->avatar);
    }
  }
}

unsigned amigostitulo_revisao(void) { return rev; }

int amigostitulo_obter(const char *imdb, AmigosTitulo *saida) {
  int i;
  if (!imdb || !imdb[0]) return 0;
  i = acha(imdb);
  if (i < 0) return 0;
  if (saida) *saida = idx[i];
  return 1;
}

void amigostitulo_primeiro_nome(const char *nome, char *dst, size_t tam) {
  size_t i = 0;
  if (!tam) return;
  while (nome && nome[i] == ' ') nome++;
  while (nome && nome[i] && nome[i] != ' ' && i + 1 < tam) { dst[i] = nome[i]; i++; }
  dst[i] = 0;
  if (!dst[0]) snprintf(dst, tam, "?");
  // Minuscula inicial vira maiuscula so no ASCII (o apelido "fabi" -> "Fabi").
  if (dst[0] >= 'a' && dst[0] <= 'z') dst[0] = (char)(dst[0] - 32);
}

// "X gostou" / "X e Y gostaram" / "X, Y e mais N gostaram" (e o mesmo para assistiram).
static void grupo(const AmigoTit *a, int n, int total, int gostou, char *dst, size_t tam) {
  char p[3][64];
  int i, q = 0;
  dst[0] = 0;
  for (i = 0; i < n && q < 2; i++) {
    if ((a[i].gostou != 0) != (gostou != 0) || !a[i].viu) continue;
    amigostitulo_primeiro_nome(a[i].nome, p[q++], sizeof p[0]);
  }
  if (total <= 0 || q == 0) return;
  if (total == 1)
    snprintf(dst, tam, i18n(gostou ? "%s gostou" : "%s assistiu"), p[0]);
  else if (total == 2)
    snprintf(dst, tam, i18n(gostou ? "%s e %s gostaram" : "%s e %s assistiram"), p[0], p[1]);
  else
    snprintf(dst, tam, i18n(gostou ? "%s, %s e mais %d gostaram" : "%s, %s e mais %d assistiram"),
             p[0], p[1], total - 2);
}

void amigostitulo_linha_destaque(const AmigosTitulo *t, char *dst, size_t tam) {
  char g[160], v[160];
  if (!tam) return;
  dst[0] = 0;
  if (!t || t->total <= 0) return;
  grupo(t->a, t->n, t->nGostou, 1, g, sizeof g);
  grupo(t->a, t->n, t->nViu, 0, v, sizeof v);
  snprintf(dst, tam, "%s%s%s", g, g[0] && v[0] ? "  \xc2\xb7  " : "", v);
}

void amigostitulo_linha_ilha(const AmigosTitulo *t, char *dst, size_t tam) {
  char frase[2][128];
  int i, q = 0;
  if (!tam) return;
  dst[0] = 0;
  if (!t || t->n <= 0) return;
  for (i = 0; i < t->n && q < 2; i++) {
    const AmigoTit *a = &t->a[i];
    char nome[64];
    amigostitulo_primeiro_nome(a->nome, nome, sizeof nome);
    if (a->gostou) snprintf(frase[q], sizeof frase[q], i18n("%s gostou"), nome);
    else if (a->episodio > 0) snprintf(frase[q], sizeof frase[q], i18n("%s viu até o E%d"), nome, a->episodio);
    else snprintf(frase[q], sizeof frase[q], i18n("%s viu"), nome);
    q++;
  }
  snprintf(dst, tam, "%s%s%s", frase[0], q > 1 ? "  \xc2\xb7  " : "", q > 1 ? frase[1] : "");
  if (t->total > q) {
    size_t z = strlen(dst);
    snprintf(dst + z, tam - z, "  \xc2\xb7  +%d", t->total - q);
  }
}

// --- a ilha e a lista da pagina do titulo -----------------------------------------

static void junta(char *dst, size_t tam, const char *pedaco) {
  size_t k;
  if (!pedaco || !pedaco[0] || !tam) return;
  k = strlen(dst);
  if (k >= tam - 1) return;
  snprintf(dst + k, tam - k, "%s%s", k ? "  \xc2\xb7  " : "", pedaco);
}

static void epTexto(int t, int e, char *dst, size_t tam) {
  if (t > 0 && e > 0) snprintf(dst, tam, i18n("T%dE%d"), t, e);
  else dst[0] = 0;
}

// 0 nada, 1 vendo agora, 2 viu ate TxEy, 3 terminou a serie, 4 parou (ha muito).
static int estadoProgresso(const AmigoTit *a, int ultT, int ultE, long long agora) {
  if (a->agora) return 1;
  if (!a->serie || a->episodio <= 0) return 0;
  if (ultT > 0 && ultE > 0 && a->terminou &&
      (a->temporada > ultT || (a->temporada == ultT && a->episodio >= ultE))) return 3;
  if (a->quando > 0 && agora - a->quando > AMT_PAROU_S) return 4;
  return 2;
}

// A frase da opiniao de um amigo: "gostou", "deu 8/10"... com o nome ou sem.
static void opiniao(const AmigoTit *a, const char *nome, char *dst, size_t tam) {
  dst[0] = 0;
  if (a->reacao == SV_REAC_GOSTOU)
    snprintf(dst, tam, nome ? i18n("%s gostou") : "%s", nome ? nome : i18n("Gostou"));
  else if (a->reacao == SV_REAC_NAO)
    snprintf(dst, tam, nome ? i18n("%s não gostou") : "%s", nome ? nome : i18n("Não gostou"));
  else if (a->reacao == SV_REAC_MEIO)
    snprintf(dst, tam, nome ? i18n("%s achou mais ou menos") : "%s", nome ? nome : i18n("Mais ou menos"));
  else if (a->nota > 0) {
    int n10 = (a->nota + 5) / 10;
    if (nome) snprintf(dst, tam, i18n("%s deu %d/10"), nome, n10);
    else snprintf(dst, tam, i18n("Nota %d/10"), n10);
  }
}

static void progressoComNome(const AmigoTit *a, const char *nome, int ultT, int ultE,
                             long long agora, char *dst, size_t tam) {
  char ep[24];
  epTexto(a->temporada, a->episodio, ep, sizeof ep);
  dst[0] = 0;
  switch (estadoProgresso(a, ultT, ultE, agora)) {
    case 1: snprintf(dst, tam, i18n("%s está vendo agora"), nome); break;
    case 2: snprintf(dst, tam, "%s: %s", nome, ep); break;
    case 3: snprintf(dst, tam, i18n("%s terminou a série"), nome); break;
    case 4: snprintf(dst, tam, i18n("%s parou no %s"), nome, ep); break;
    default: break;
  }
}

// "4 amigos: 3 gostaram" — ou, com uma opiniao so, a do amigo pelo nome.
static void resumoOpinioes(const AmigosTitulo *t, char *dst, size_t tam) {
  int i;
  dst[0] = 0;
  if (t->nOpiniao <= 0) return;
  if (t->nOpiniao == 1) {
    for (i = 0; i < t->n; i++)
      if (t->a[i].reacao != SV_REAC_NADA || t->a[i].nota > 0) {
        char nome[64];
        amigostitulo_primeiro_nome(t->a[i].nome, nome, sizeof nome);
        opiniao(&t->a[i], nome, dst, tam);
        return;
      }
    return;
  }
  if (t->nGostou > 0) {
    if (t->nGostou == 1) snprintf(dst, tam, i18n("%d amigos: 1 gostou"), t->total);
    else snprintf(dst, tam, i18n("%d amigos: %d gostaram"), t->total, t->nGostou);
  } else if (t->nNaoGostou > 0)
    snprintf(dst, tam, i18n("%d amigos: %d não gostaram"), t->total, t->nNaoGostou);
  else
    snprintf(dst, tam, i18n("%d amigos: %d acharam mais ou menos"), t->total, t->nMeio);
}

static void resumoRecs(const AmigosTitulo *t, char *dst, size_t tam) {
  char p[2][64];
  int i, q = 0;
  dst[0] = 0;
  for (i = 0; i < t->n && q < 2; i++)
    if (t->a[i].recomendou) amigostitulo_primeiro_nome(t->a[i].nome, p[q++], sizeof p[0]);
  if (!q) return;
  if (t->nRecomendou == 1) snprintf(dst, tam, i18n("%s recomendou para você"), p[0]);
  else if (t->nRecomendou == 2 && q == 2) snprintf(dst, tam, i18n("%s e %s recomendaram para você"), p[0], p[1]);
  else snprintf(dst, tam, i18n("%s e mais %d recomendaram para você"), p[0], t->nRecomendou - 1);
}

// O progresso do primeiro amigo que tem um (+N quando ha mais).
static void resumoProgresso(const AmigosTitulo *t, int ultT, int ultE, long long agora,
                            char *dst, size_t tam) {
  int i, n = 0;
  dst[0] = 0;
  for (i = 0; i < t->n; i++) {
    char nome[64], frase[160];
    if (!estadoProgresso(&t->a[i], ultT, ultE, agora)) continue;
    if (n++ > 0) continue;
    amigostitulo_primeiro_nome(t->a[i].nome, nome, sizeof nome);
    progressoComNome(&t->a[i], nome, ultT, ultE, agora, frase, sizeof frase);
    snprintf(dst, tam, "%s", frase);
  }
  if (n > 1) {
    size_t k = strlen(dst);
    snprintf(dst + k, tam - k, "  \xc2\xb7  +%d", n - 1);
  }
}

// Quem so assistiu (sem recomendar, opinar nem progresso a mostrar).
static void resumoViu(const AmigosTitulo *t, char *dst, size_t tam) {
  char p[2][64];
  int i, q = 0, n = 0;
  dst[0] = 0;
  for (i = 0; i < t->n; i++) {
    if (prioridade(&t->a[i]) != 3) continue;
    if (q < 2) amigostitulo_primeiro_nome(t->a[i].nome, p[q++], sizeof p[0]);
    n++;
  }
  if (!n) return;
  if (n == 1) snprintf(dst, tam, i18n("%s assistiu"), p[0]);
  else if (n == 2) snprintf(dst, tam, i18n("%s e %s assistiram"), p[0], p[1]);
  else snprintf(dst, tam, i18n("%s, %s e mais %d assistiram"), p[0], p[1], n - 2);
}

void amigostitulo_resumo(const AmigosTitulo *t, int ultT, int ultE, long long agora,
                         char *l1, size_t t1, char *l2, size_t t2) {
  char rec[160], opi[160], pro[200], viu[200], gosto[160];
  int i, usado = -1;
  if (t1) l1[0] = 0;
  if (t2) l2[0] = 0;
  if (!t || t->total <= 0 || !t1 || !t2) return;
  resumoRecs(t, rec, sizeof rec);
  resumoOpinioes(t, opi, sizeof opi);
  resumoProgresso(t, ultT, ultE, agora, pro, sizeof pro);
  resumoViu(t, viu, sizeof viu);
  gosto[0] = 0;
  for (i = 0; i < t->n; i++)
    if (t->a[i].gosto >= 70 && temOpiniao(&t->a[i])) {
      char nome[64];
      amigostitulo_primeiro_nome(t->a[i].nome, nome, sizeof nome);
      snprintf(gosto, sizeof gosto, i18n("%s tem gosto parecido com o seu"), nome);
      break;
    }
  // A primeira que existir e a manchete; as seguintes (ate duas) vao embaixo.
  { const char *ordem[4] = { rec, opi, pro, viu };
    int k, nl2 = 0;
    for (k = 0; k < 4; k++) {
      if (!ordem[k][0]) continue;
      if (usado < 0) { snprintf(l1, t1, "%s", ordem[k]); usado = k; continue; }
      // "So assistiu" e o menos util: so entra embaixo se nada mais coube.
      if (k == 3 && nl2 > 0) continue;
      if (nl2 < 2) { junta(l2, t2, ordem[k]); nl2++; }
    }
    // A frase de quem recomendou, quando ha uma so e nada mais a dizer.
    if (usado == 0 && nl2 == 0 && t->nRecomendou == 1)
      for (i = 0; i < t->n; i++)
        if (t->a[i].recomendou && t->a[i].recTexto[0]) {
          char q[96];
          snprintf(q, sizeof q, "\xe2\x80\x9c%s\xe2\x80\x9d", t->a[i].recTexto);
          junta(l2, t2, q);
          break;
        }
    if (gosto[0] && strlen(l2) + strlen(gosto) < 90) junta(l2, t2, gosto); }
  if (!l2[0]) snprintf(l2, t2, "%s", i18n("Abrir para ver o que acharam"));
}

void amigostitulo_frase_amigo(const AmigoTit *a, int ultT, int ultE, long long agora,
                              char *st, size_t tst, char *extra, size_t textra) {
  char b[160], ep[24];
  if (tst) st[0] = 0;
  if (textra) extra[0] = 0;
  if (!a || !tst || !textra) return;
  opiniao(a, NULL, b, sizeof b);
  junta(st, tst, b);
  if (a->reacao != SV_REAC_NADA && a->nota > 0) {
    snprintf(b, sizeof b, i18n("Nota %d/10"), (a->nota + 5) / 10);
    junta(st, tst, b);
  }
  epTexto(a->temporada, a->episodio, ep, sizeof ep);
  b[0] = 0;
  switch (estadoProgresso(a, ultT, ultE, agora)) {
    case 1:
      snprintf(b, sizeof b, "%s", i18n("Vendo agora"));
      if (ep[0]) { size_t k = strlen(b); snprintf(b + k, sizeof b - k, " %s", ep); }
      break;
    case 2: snprintf(b, sizeof b, i18n("Viu até %s"), ep); break;
    case 3: snprintf(b, sizeof b, "%s", i18n("Terminou a série")); break;
    case 4: {
      char h[48];
      snprintf(h, sizeof h, i18n("há %d dias"), (int)((agora - a->quando) / 86400));
      snprintf(b, sizeof b, i18n("Parou no %s"), ep);
      junta(b, sizeof b, h);
      break; }
    default:
      if (a->viu && !st[0]) snprintf(b, sizeof b, "%s", i18n("Assistiu"));
      break;
  }
  junta(st, tst, b);
  if (a->recomendou) {
    junta(extra, textra, i18n("Recomendou para você"));
    if (a->recTexto[0]) {
      snprintf(b, sizeof b, "\xe2\x80\x9c%s\xe2\x80\x9d", a->recTexto);
      junta(extra, textra, b);
    }
  }
  if (a->resposta[0]) {
    snprintf(b, sizeof b, "\xe2\x80\x9c%s\xe2\x80\x9d", a->resposta);
    junta(extra, textra, b);
  }
  if (a->gosto >= 0) {
    snprintf(b, sizeof b, "%s %d%%", i18n("Gosto parecido"), a->gosto);
    junta(extra, textra, b);
  }
}
