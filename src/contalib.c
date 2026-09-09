#include "contalib.h"
#include "vistoep.h"
#include "catalogo.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nao esta em catalogo.h — ctxmenu.c e trakt.c ja a declaram assim. Repetir a
// declaracao em vez de mexer no cabecalho mantem a mudanca dentro dos arquivos
// deste conserto; o lugar certo dela e catalogo.h, e isso esta no relatorio.
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

static ContaLibItem *itens;
static int nItens;
static ContaVisto *vistos;
static int nVistos;
static int temConta;

// Marca de reconciliacao. `marcaCatN` comeca em -1 de proposito: catalogo com
// zero itens e um estado real (arranque sem cache), e -1 garante que a primeira
// conferencia com o catalogo ja publicado nao seja confundida com "igual".
static int aplicado, marcaIdx = -1, marcaCatN = -1;
static char marcaId[24];

// ---------------------------------------------------------------- leitura

static const char *tipoBase(const char *t) {
  if (t && (!strcmp(t, "series") || !strcmp(t, "show") || !strcmp(t, "tv")))
    return "series";
  return "movie";
}

static int contarLinhas(const char *json) {
  const char *p;
  int k = 0;
  for (p = js_raiz_array(json); p; p = js_prox(js_fim(p))) k++;
  return k;
}

// `added_at` chega das duas formas. O web faz `Number(row.added_at)` e cai em
// Date.now() quando nao e finito, ou seja ele espera NUMERO; mas a mesma coluna
// sai do Postgres como timestamp ISO em varias RPC deste servidor (o
// `watched_at` e o `updated_at` do progresso ja obrigaram js_ms_iso a existir).
// Ler as duas custa uma comparacao e evita uma ordem embaralhada em silencio.
static long long instante(const char *ini, const char *fim, const char *chave) {
  char t[48];
  double d;
  if (js_texto(ini, fim, chave, t, sizeof t)) {
    // VALOR ENTRE ASPAS, e a ordem destes dois testes importa. js_num pula a
    // aspa de abertura e chama atof, entao para "2026-09-01T10:00:00+00:00" ele
    // devolve 2026 — um instante de 1970 disfarcado de sucesso, que sairia como
    // uma ordem embaralhada e nenhum erro. Perguntar primeiro se o valor e
    // texto, e so depois o que ele parece, e o que separa os dois casos.
    if (strchr(t, '-') || strchr(t, 'T') || strchr(t, ' ')) return js_ms_iso(t);
    d = atof(t);
  } else {
    d = js_num(ini, fim, chave, 0.0);
  }
  if (d <= 0.0) return 0;
  // Menor que 1e12 e SEGUNDO, nao milissegundo — a mesma regra que syncprog.c
  // aplica ao `last_watched`, e pelo mesmo motivo: 1e12 ms e setembro de 2001,
  // entao qualquer valor abaixo disso so faz sentido em segundos.
  return (long long)(d < 1e12 ? d * 1000.0 : d);
}

// "Filme · Drama · Crime". Os generos NAO sao traduzidos aqui: desc_genero_pt
// vive em descoberta.c, e depender dele arrastaria a descoberta inteira para
// dentro deste modulo e do teste. Quem desenha a linha (o detalhe) ja recebe
// texto pronto de varias fontes em ingles.
static void comporGenero(char *dst, size_t tam, const char *tipo,
                         const char *ini, const char *fim) {
  char cru[600];
  const char *p;
  int k;
  k = snprintf(dst, tam, "%s", strcmp(tipo, "series") ? "Filme" : "Programa de TV");
  if (k < 0 || (size_t)k + 1 >= tam) return;
  if (!js_bruto(ini, fim, "genres", cru, sizeof cru)) return;
  for (p = cru; *p;) {
    const char *a;
    int q;
    while (*p && *p != '"') p++;
    if (!*p) break;
    a = ++p;
    while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
    if (p > a) {
      q = snprintf(dst + k, tam - (size_t)k, " · %.*s", (int)(p - a), a);
      if (q < 0 || (size_t)(k + q) + 1 >= tam) { dst[tam - 1] = 0; return; }
      k += q;
    }
    if (*p) p++;
  }
}

int contalib_ler_biblioteca(const char *json) {
  ContaLibItem *novo;
  const char *p;
  int total, k = 0, i, j;

  total = contarLinhas(json);
  // A REGRA 1 DA SECAO 1.6, e ela e a razao de esta funcao devolver -1 em vez
  // de 0. Resposta vazia com lista guardada NAO substitui nada: pode ser perfil
  // errado, 401 mal tratado ou o servidor fora do ar, e trocar uma lista boa
  // por nada e a unica falha desta area que a pessoa nao consegue desfazer.
  if (total < 1) return nItens > 0 ? -1 : 0;
  if (total > CONTALIB_MAX) {
    printf("[contalib] biblioteca da conta tem %d itens; guardando os %d "
           "primeiros\n", total, CONTALIB_MAX);
    total = CONTALIB_MAX;
  }
  novo = (ContaLibItem *)calloc((size_t)total, sizeof *novo);
  if (!novo) return -1;

  for (p = js_raiz_array(json); p && k < total; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    ContaLibItem *d = &novo[k];
    char t[24];
    // ZERA A VAGA a cada linha, e nao so uma vez no calloc. `k` NAO avanca
    // quando a linha e recusada, entao a vaga e reaproveitada — e sem isto a
    // linha seguinte herdaria os campos que ela mesma nao trouxer. O caso real
    // e um poster: linha sem content_id com poster, linha seguinte com
    // content_id e sem poster, e o titulo aparece com a arte do vizinho.
    memset(d, 0, sizeof *d);
    if (!js_texto(p, f, "content_id", d->id, sizeof d->id)) continue;
    snprintf(d->tipo, sizeof d->tipo, "%s",
             tipoBase(js_texto(p, f, "content_type", t, sizeof t) ? t : NULL));
    if (!js_texto(p, f, "name", d->titulo, sizeof d->titulo))
      js_texto(p, f, "title", d->titulo, sizeof d->titulo);
    js_texto(p, f, "poster", d->poster, sizeof d->poster);
    js_texto(p, f, "background", d->backdrop, sizeof d->backdrop);
    js_texto(p, f, "release_info", d->meta, sizeof d->meta);
    comporGenero(d->genero, sizeof d->genero, d->tipo, p, f);
    // imdb_rating vem 0..10 (e as vezes como string; js_num aceita as duas).
    // CatItem.nota e porcentagem, que e o que o selo do detalhe desenha.
    d->nota = (int)(js_num(p, f, "imdb_rating", 0.0) * 10.0 + 0.5);
    if (d->nota < 0 || d->nota > 100) d->nota = 0;
    d->addedMs = instante(p, f, "added_at");
    // `poster_shape` e `addon_base_url` sao LIDOS pelo web e nao tem onde
    // morar aqui: a grade da biblioteca e 2:3 fixa (NV_BIB_POSTER_H) e o
    // detalhe pergunta fontes a todos os addons instalados, nao a um so.
    // Registrado para a proxima pessoa nao achar que foram esquecidos.
    k++;
  }
  if (k < 1) { free(novo); return nItens > 0 ? -1 : 0; }

  // Ordem por `added_at` decrescente. O seletor "Ordenar" da biblioteca tem
  // "Ordem da lista" como padrao, e sem isto essa ordem seria a que o servidor
  // resolvesse devolver — que nao e contrato nenhum. Insercao: sao no maximo
  // CONTALIB_MAX itens, uma vez por ciclo de sync.
  for (i = 1; i < k; i++) {
    ContaLibItem v = novo[i];
    for (j = i - 1; j >= 0 && novo[j].addedMs < v.addedMs; j--) novo[j + 1] = novo[j];
    novo[j + 1] = v;
  }

  free(itens);
  itens = novo;
  nItens = k;
  temConta = 1;
  // Forca uma aplicacao nova: a lista mudou, e a marca antiga fala do catalogo
  // como ele estava para a lista anterior.
  aplicado = 1;
  marcaCatN = -1;
  marcaIdx = -1;
  printf("[contalib] biblioteca da conta: %d itens\n", k);
  return k;
}

int contalib_ler_vistos(const char *json) {
  ContaVisto *novo;
  const char *p;
  int total, k = 0;

  total = contarLinhas(json);
  if (total < 1) return nVistos > 0 ? -1 : 0;
  if (total > CONTALIB_VISTO_MAX) total = CONTALIB_VISTO_MAX;
  novo = (ContaVisto *)calloc((size_t)total, sizeof *novo);
  if (!novo) return -1;

  for (p = js_raiz_array(json); p && k < total; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    ContaVisto *d = &novo[k];
    char t[24];
    memset(d, 0, sizeof *d);   // mesma razao da leitura de biblioteca
    if (!js_texto(p, f, "content_id", d->id, sizeof d->id)) continue;
    snprintf(d->tipo, sizeof d->tipo, "%s",
             tipoBase(js_texto(p, f, "content_type", t, sizeof t) ? t : NULL));
    d->temporada = (int)js_num(p, f, "season", 0.0);
    d->episodio  = (int)js_num(p, f, "episode", 0.0);
    d->vistoMs   = instante(p, f, "watched_at");
    k++;
  }
  if (k < 1) { free(novo); return nVistos > 0 ? -1 : 0; }
  free(vistos);
  vistos = novo;
  nVistos = k;
  printf("[contalib] vistos da conta: %d linhas\n", k);
  return k;
}

int contalib_n(void) { return nItens; }
const ContaLibItem *contalib_item(int i) {
  return (i >= 0 && i < nItens) ? &itens[i] : NULL;
}
int contalib_n_vistos(void) { return nVistos; }
const ContaVisto *contalib_visto(int i) {
  return (i >= 0 && i < nVistos) ? &vistos[i] : NULL;
}
int contalib_tem_conta(void) { return temConta && nItens > 0; }

// ---------------------------------------------------------------- catalogo

static void montarItem(CatItem *d, const ContaLibItem *s) {
  memset(d, 0, sizeof *d);
  // Precisao no formato, e nao snprintf cru: CatItem.imdb tem 16 bytes e
  // ContaLibItem.id tem 24. Sem o `%.15s` o gcc acusa -Wformat-truncation e o
  // corte fica implicito.
  snprintf(d->imdb, sizeof d->imdb, "%.15s", s->id);
  snprintf(d->tipo, sizeof d->tipo, "%.7s", s->tipo);
  snprintf(d->titulo, sizeof d->titulo, "%s", s->titulo);
  snprintf(d->genero, sizeof d->genero, "%s", s->genero);
  snprintf(d->meta, sizeof d->meta, "%s", s->meta);
  d->nota = s->nota;
  d->naLista = 1;
  if (s->poster[0]) snprintf(d->poster, sizeof d->poster, "%s", s->poster);
  if (s->backdrop[0]) snprintf(d->backdrop, sizeof d->backdrop, "%s", s->backdrop);
  // Arte de reserva pelo id do IMDb, do mesmo jeito que trakt_lista faz — e
  // pelo mesmo motivo, com o mesmo tamanho: "medium" e nao "small", porque o
  // metahub serve poster/small como WEBP e o libSDL2_image DESTA TV nao carrega
  // libwebp. Com "small" o card nunca decodifica e o cache nao guarda falha,
  // entao cada quadro tenta de novo.
  //
  // So para id do IMDb: a conta pode guardar item de addon com id proprio
  // (kitsu, anime), e o metahub nao responde por esses — montar a URL assim
  // daria um 404 por quadro em vez de um card sem arte.
  if (!strncmp(s->id, "tt", 2)) {
    if (!d->poster[0])
      snprintf(d->poster, sizeof d->poster,
               "https://images.metahub.space/poster/medium/%.15s/img", s->id);
    if (!d->backdrop[0])
      snprintf(d->backdrop, sizeof d->backdrop,
               "https://images.metahub.space/background/medium/%.15s/img", s->id);
    snprintf(d->logo, sizeof d->logo,
             "https://images.metahub.space/logo/medium/%.15s/img", s->id);
  }
}

int contalib_aplicar_catalogo(void) {
  char faltam[CONTALIB_MAX];
  int i, marcados = 0, nFaltam = 0, ultimo = -1;

  if (nItens < 1) return 0;
  // Catalogo vazio nao aceita acrescimo (cat_acrescentar_lote recusa com n<1) e
  // nao tem onde marcar. Sair aqui, sem tocar na marca, faz a reconciliacao
  // tentar de novo assim que o catalogo existir.
  if (cat_n() < 1) return 0;

  memset(faltam, 0, sizeof faltam);
  for (i = 0; i < nItens && i < CONTALIB_MAX; i++) {
    int k = cat_indice_por_imdb(itens[i].id);
    if (k >= 0) { cat_definir_na_lista(k, 1); marcados++; ultimo = k; }
    else { faltam[i] = 1; nFaltam++; }
  }

  if (nFaltam > 0) {
    CatItem *novos = (CatItem *)malloc(sizeof(CatItem) * (size_t)nFaltam);
    int *idx = (int *)malloc(sizeof(int) * (size_t)nFaltam);
    if (novos && idx) {
      int q = 0, entraram;
      for (i = 0; i < nItens && i < CONTALIB_MAX; i++)
        if (faltam[i]) montarItem(&novos[q++], &itens[i]);
      entraram = cat_acrescentar_lote(novos, q, idx);
      if (entraram > 0) { marcados += entraram; ultimo = idx[entraram - 1]; }
      if (entraram < q)
        printf("[contalib] %d itens da conta nao couberam no catalogo\n",
               q - entraram);
    }
    free(novos);
    free(idx);
  }

  aplicado = 1;
  marcaCatN = cat_n();
  marcaIdx = ultimo;
  marcaId[0] = 0;
  if (ultimo >= 0 && ultimo < cat_n()) {
    const CatItem *c = cat_item(ultimo);
    if (c) snprintf(marcaId, sizeof marcaId, "%s", c->imdb);
  }
  printf("[contalib] biblioteca da conta aplicada: %d no catalogo (%d novos)\n",
         marcados, nFaltam);
  return marcados;
}

void contalib_reconciliar(void) {
  const CatItem *c;
  if (!aplicado || nItens < 1) return;
  if (cat_n() < 1) return;
  if (cat_n() != marcaCatN) { contalib_aplicar_catalogo(); return; }
  // Contagem igual nao prova catalogo igual: a descoberta pode republicar o
  // mesmo numero de titulos com outro conteudo. A marca e o ULTIMO item que
  // esta funcao tocou — o que mais depressa deixa de bater numa troca, porque
  // os acrescentados ficam no fim do vetor.
  if (marcaIdx < 0 || marcaIdx >= cat_n()) return;
  c = cat_item(marcaIdx);
  if (c && !strcmp(c->imdb, marcaId) && c->naLista) return;
  contalib_aplicar_catalogo();
}

int contalib_aplicar_vistos(void) {
  int i, k = 0, ke = 0;
  for (i = 0; i < nVistos; i++) {
    if (!vistos[i].id[0]) continue;
    // LINHA DE EPISODIO VAI PARA O MAPA DE EPISODIOS, e nao para o historico
    // de titulo. `season` e `episode` sempre vieram nesta resposta
    // (PLANO-CONTA-SYNC.md secao 1.5) e eram lidos e descartados aqui: o
    // `continue` abaixo pulava a linha inteira. E por isso que o app nunca
    // soube quais episodios a pessoa viu, so quais SERIES.
    if (vistos[i].temporada > 0 || vistos[i].episodio > 0) {
      vistoep_definir(vistos[i].id, vistos[i].temporada, vistos[i].episodio, 1);
      ke++;
      continue;
    }
    cat_historico_definir_id(vistos[i].id, vistos[i].tipo, 1);
    k++;
  }
  if (k) printf("[contalib] %d titulos marcados como vistos pela conta\n", k);
  if (ke) printf("[contalib] %d episodios vistos vindos da conta\n", ke);
  return k;
}

void contalib_esquecer(void) {
  free(itens);
  itens = NULL;
  nItens = 0;
  free(vistos);
  vistos = NULL;
  nVistos = 0;
  temConta = 0;
  aplicado = 0;
  marcaIdx = -1;
  marcaCatN = -1;
  marcaId[0] = 0;
}
