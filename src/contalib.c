#include "contalib.h"
#include "vistoep.h"
#include <time.h>
#include "catalogo.h"
#include "js.h"
#include "idioma.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Nao esta em catalogo.h — ctxmenu.c e trakt.c ja a declaram assim. Repetir a
// declaracao em vez de mexer no cabecalho mantem a mudanca dentro dos arquivos
// deste conserto; o lugar certo dela e catalogo.h, e isso esta no relatorio.
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);

static int (*listaOculta)(const char *id, long long ms);
static int (*vistoOculto)(const char *id, int t, int e, long long ms);
void contalib_filtros(int (*lista)(const char *id, long long ms),
                      int (*visto)(const char *id, int t, int e, long long ms)) {
  listaOculta = lista;
  vistoOculto = visto;
}

static ContaLibItem *itens;
static int nItens;
static ContaVisto *vistos;
static int nVistos;
// So a TROCA do vetor de vistos e a copia das sementes (fio da descoberta)
// passam por aqui. O resto deste modulo e do fio principal.
static pthread_mutex_t vistosTrava = PTHREAD_MUTEX_INITIALIZER;
static unsigned vistosRev;
// Quando (s, relogio de parede) os vistos foram puxados COM SUCESSO pela ultima
// vez. 0 = nunca nesta sessao: os vistos vivem so em memoria.
static time_t vistosPuxadoEm;
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
  // PASSA POR i18n() COMO TODO ROTULO DE TIPO. Este campo vai direto para a
  // tela (CatItem.genero, a linha "Programa de TV · Drama"), e a traducao que
  // existe em catalogo.c so roda no catalogo do PACOTE, na leitura do extra.txt
  // — item vindo da CONTA nao passa por la. Com o app em ingles a linha saia
  // "TV Show" nos titulos do pacote e "Programa de TV" nos da conta, na mesma
  // home. E a metade do #23 que a correcao da 1.0.28 nao alcancou.
  k = snprintf(dst, tam, "%s",
               i18n(strcmp(tipo, "series") ? "Filme" : "Programa de TV"));
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

// Onde a linha `v` entra num vetor que ja tem `k` de `cap` vagas: -2 quando o
// titulo ja esta la e a copia guardada e tao nova quanto ela; o indice da vaga
// a sobrescrever quando o titulo repete (pagina que se sobrepoe a anterior) ou
// quando o vetor esta cheio e `v` e mais nova que a MAIS VELHA guardada; -1
// quando nao ha lugar para ela.
//
// A VELHA SAI, NUNCA A NOVA. Era o defeito do issue do Owlphibia29 ("o contador
// nunca passou de 205; o que eu acabei de adicionar nao aparece"): o teto
// cortava a resposta na ORDEM EM QUE O SERVIDOR RESPONDE, que nao e contrato
// nenhum, e o titulo recem-salvo era justamente o que ficava de fora.
static int vagaPara(const ContaLibItem *novo, int k, int cap,
                    const ContaLibItem *v) {
  int i, velho = -1;
  for (i = 0; i < k; i++)
    if (!strcmp(novo[i].id, v->id))
      return novo[i].addedMs >= v->addedMs ? -2 : i;
  if (k < cap) return k;
  for (i = 0; i < k; i++)
    if (velho < 0 || novo[i].addedMs < novo[velho].addedMs) velho = i;
  return (velho >= 0 && v->addedMs > novo[velho].addedMs) ? velho : -1;
}

// URL de arte inteira ou nenhuma (#361): um prefixo cortado por js_texto e um
// 404 calado. Sem ela, montarItem cai no metahub pelo id; o registro diz qual.
static void urlInteira(const char *p, const char *f, const char *chave,
                       char *dst, size_t tam, const char *id) {
  char v[NV_ARTE_URL_MAX + 1];
  size_t n;
  dst[0] = 0;
  if (!js_texto(p, f, chave, v, sizeof v)) return;
  n = strlen(v);
  if (n >= tam) {
    printf("[contalib] %s de %s tem %s%zu bytes e o campo guarda %zu: descartada, "
           "nao cortada (#361)\n", chave, id, n + 1 >= sizeof v ? ">=" : "", n, tam - 1);
    fflush(stdout);
    return;
  }
  memcpy(dst, v, n + 1);
}

int contalib_ler_biblioteca(const char *json) {
  ContaLibItem *novo;
  ContaLibItem linha;
  const char *p;
  int *seq;
  int total, cap, k = 0, lidas = 0, fora = 0, i, j;

  total = contarLinhas(json);
  // A REGRA 1 DA SECAO 1.6, e ela e a razao de esta funcao devolver -1 em vez
  // de 0. Resposta vazia com lista guardada NAO substitui nada: pode ser perfil
  // errado, 401 mal tratado ou o servidor fora do ar, e trocar uma lista boa
  // por nada e a unica falha desta area que a pessoa nao consegue desfazer.
  if (total < 1) return nItens > 0 ? -1 : 0;
  // LE TODAS AS LINHAS e guarda as CONTALIB_MAX mais recentes. O vetor tem o
  // tamanho do teto, nao o da resposta: a memoria continua limitada por
  // CONTALIB_MAX, e a resposta inteira ja esta no corpo que sync.c baixou
  // (limitado por CONTALIB_PAGINAS paginas).
  cap = total > CONTALIB_MAX ? CONTALIB_MAX : total;
  novo = (ContaLibItem *)calloc((size_t)cap, sizeof *novo);
  seq = (int *)calloc((size_t)cap, sizeof *seq);
  if (!novo || !seq) { free(novo); free(seq); return -1; }

  for (p = js_raiz_array(json); p; p = js_prox(js_fim(p))) {
    const char *f = js_fim(p);
    ContaLibItem *d = &linha;
    char t[24];
    int vaga;
    // ZERA A LINHA a cada volta. A linha seguinte nao pode herdar os campos que
    // ela mesma nao trouxer — o caso real e um poster: linha sem content_id com
    // poster, linha seguinte com content_id e sem poster, e o titulo aparecia
    // com a arte do vizinho.
    memset(d, 0, sizeof *d);
    if (!js_texto(p, f, "content_id", d->id, sizeof d->id)) continue;
    snprintf(d->tipo, sizeof d->tipo, "%s",
             tipoBase(js_texto(p, f, "content_type", t, sizeof t) ? t : NULL));
    if (!js_texto(p, f, "name", d->titulo, sizeof d->titulo))
      js_texto(p, f, "title", d->titulo, sizeof d->titulo);
    urlInteira(p, f, "poster", d->poster, sizeof d->poster, d->id);
    urlInteira(p, f, "background", d->backdrop, sizeof d->backdrop, d->id);
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
    lidas++;
    vaga = vagaPara(novo, k, cap, d);
    if (vaga == -2) continue;                 // repetida, e a guardada vale
    if (vaga == -1) { fora++; continue; }     // mais velha que todas as guardadas
    if (vaga < k && strcmp(novo[vaga].id, d->id)) fora++;   // desalojou a mais velha
    novo[vaga] = *d;
    seq[vaga] = lidas;
    if (vaga == k) k++;
  }
  if (k < 1) { free(novo); free(seq); return nItens > 0 ? -1 : 0; }
  if (fora > 0)
    printf("[contalib] biblioteca da conta tem %d itens; guardando os %d mais "
           "recentes (teto %d)\n", k + fora, k, CONTALIB_MAX);

  // Ordem por `added_at` decrescente. O seletor "Ordenar" da biblioteca tem
  // "Ordem da lista" como padrao, e sem isto essa ordem seria a que o servidor
  // resolvesse devolver — que nao e contrato nenhum. Empate (as duas sem
  // `added_at`) fica na ordem em que as linhas chegaram.
  //
  // ORDENA INDICES, nao os itens. Com o teto em 500 uma insercao sobre o vetor
  // de itens moveria ate ~90 MB de struct por ciclo de sync nesta TV; sobre
  // inteiros sao ~60 mil trocas de 4 bytes, e os itens sao copiados uma vez so.
  {
    int *ord = (int *)malloc(sizeof(int) * (size_t)k);
    ContaLibItem *ordenado = (ContaLibItem *)malloc(sizeof *ordenado * (size_t)k);
    if (ord && ordenado) {
      for (i = 0; i < k; i++) ord[i] = i;
      for (i = 1; i < k; i++) {
        int v = ord[i];
        for (j = i - 1; j >= 0 && (novo[ord[j]].addedMs < novo[v].addedMs
                                   || (novo[ord[j]].addedMs == novo[v].addedMs
                                       && seq[ord[j]] > seq[v])); j--)
          ord[j + 1] = ord[j];
        ord[j + 1] = v;
      }
      for (i = 0; i < k; i++) ordenado[i] = novo[ord[i]];
      free(novo);
      novo = ordenado;
      ordenado = NULL;
    }
    free(ord);
    free(ordenado);
  }
  free(seq);

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
  pthread_mutex_lock(&vistosTrava);
  if (k != nVistos || !vistos || memcmp(novo, vistos, sizeof *novo * (size_t)k))
    vistosRev++;
  free(vistos);
  vistos = novo;
  nVistos = k;
  vistosPuxadoEm = time(NULL);
  pthread_mutex_unlock(&vistosTrava);
  printf("[contalib] vistos da conta: %d linhas\n", k);
  return k;
}

int contalib_n(void) { return nItens; }
const ContaLibItem *contalib_item(int i) {
  return (i >= 0 && i < nItens) ? &itens[i] : NULL;
}
int contalib_n_vistos(void) { return nVistos; }
unsigned contalib_vistos_revisao(void) { return vistosRev; }
const ContaVisto *contalib_visto(int i) {
  return (i >= 0 && i < nVistos) ? &vistos[i] : NULL;
}
int contalib_tem_conta(void) { return temConta && nItens > 0; }

// ---------------------------------------------------------------- catalogo

static void montarItem(CatItem *d, const ContaLibItem *s) {
  memset(d, 0, sizeof *d);
  // CatItem.imdb tem 64 desde o #37 (id de canal e comprido); ContaLibItem.id
  // tem 24, entao cabe inteiro e o `%.15s` que cortava aqui saiu.
  snprintf(d->imdb, sizeof d->imdb, "%s", s->id);
  snprintf(d->tipo, sizeof d->tipo, "%.7s", s->tipo);
  snprintf(d->titulo, sizeof d->titulo, "%s", s->titulo);
  snprintf(d->genero, sizeof d->genero, "%s", s->genero);
  snprintf(d->meta, sizeof d->meta, "%s", s->meta);
  d->nota = s->nota;
  d->naLista = 1;
  d->fontes = CAT_FONTE_NUVIO;
  if (s->poster[0]) snprintf(d->poster, sizeof d->poster, "%s", s->poster);
  if (s->backdrop[0]) snprintf(d->backdrop, sizeof d->backdrop, "%s", s->backdrop);
  snprintf(d->backdropCatalogo, sizeof d->backdropCatalogo, "%s", d->backdrop);
  if (strstr(d->backdrop, "image.tmdb.org/t/p/"))
    snprintf(d->backdropTmdb, sizeof d->backdropTmdb, "%s", d->backdrop);
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
    int k;
    // TIRADO NESTA TV e ainda nao refletido na conta (contapend.h): a linha
    // remota nao re-marca. Sem isto o titulo voltava no ciclo seguinte.
    if (listaOculta && listaOculta(itens[i].id, itens[i].addedMs)) continue;
    k = cat_indice_por_imdb(itens[i].id);
    if (k >= 0) { cat_marcar_fonte(k, CAT_FONTE_NUVIO); marcados++; ultimo = k; }
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
  // `naLista` SOZINHO NAO PROVA NADA. A descoberta tambem republica titulos
  // que estao no Trakt com `naLista=1` e so o bit Trakt: aceitar a marca ai
  // deixava o bit Nuvio apagado para sempre e o titulo sumia de "Conta Nuvio".
  // A marca e sempre um item da conta — sem o bit Nuvio nele, o catalogo foi
  // republicado por cima e tudo que a conta marcou tem de ser reaplicado.
  if (c && !strcmp(c->imdb, marcaId) && c->naLista
      && (c->fontes & CAT_FONTE_NUVIO)) return;
  contalib_aplicar_catalogo();
}

int contalib_vistos_do_titulo(const char *id, int *desmarcados) {
  int i, k = 0, d = 0;
  if (desmarcados) *desmarcados = 0;
  if (!id || !id[0]) return 0;
  pthread_mutex_lock(&vistosTrava);
  for (i = 0; i < nVistos; i++) {
    if (vistos[i].episodio <= 0 || strcmp(vistos[i].id, id)) continue;
    k++;
    if (vistoep_estado(id, vistos[i].temporada, vistos[i].episodio) == 0) d++;
  }
  pthread_mutex_unlock(&vistosTrava);
  if (desmarcados) *desmarcados = d;
  return k;
}

// Titulos com linha barrada num mesmo pull que ganham linha propria no log. O
// resto entra so no total: e log de diagnostico, nao relatorio.
#define CL_BARRADOS_LOG 6

int contalib_aplicar_vistos(void) {
  struct { char id[24]; VistoFonte f; } barrado[CL_BARRADOS_LOG];
  VistoFonte total;
  int i, j, k = 0, ke = 0, nBarrado = 0, jornal = 0;
  memset(&total, 0, sizeof total);
  for (i = 0; i < nVistos; i++) {
    if (!vistos[i].id[0]) continue;
    // DESMARCADO NESTA TV depois desta linha: ela nao re-marca (contapend.h).
    if (vistoOculto && vistoOculto(vistos[i].id, vistos[i].temporada,
                                   vistos[i].episodio, vistos[i].vistoMs)) {
      jornal++;
      continue;
    }
    // LINHA DE EPISODIO VAI PARA O MAPA DE EPISODIOS, e nao para o historico
    // de titulo. `season` e `episode` sempre vieram nesta resposta
    // (PLANO-CONTA-SYNC.md secao 1.5) e eram lidos e descartados aqui: o
    // `continue` abaixo pulava a linha inteira. E por isso que o app nunca
    // soube quais episodios a pessoa viu, so quais SERIES.
    if (vistos[i].temporada > 0 || vistos[i].episodio > 0) {
      // PELA PORTA DAS FONTES (vistoep.h): o jornal acima so segura a linha
      // ate a conta confirmar o delete e a entrada ser podada (contapend_podar).
      // Uma linha que ainda venha depois disso — copia guardada, pagina velha,
      // delete aceito e nao aplicado — re-marcava o episodio. A desmarcacao da
      // pessoa fica guardada a parte e e consultada aqui, com o watched_at da
      // linha: so um visto MAIS NOVO que o gesto volta a marcar.
      VistoFonte um;
      memset(&um, 0, sizeof um);
      vistoep_fonte(vistos[i].id, vistos[i].temporada, vistos[i].episodio, 1,
                    vistos[i].vistoMs, &um);
      total.vistos += um.vistos;
      total.venceu += um.venceu;
      if (um.bloqueados) {
        total.bloqueados++;
        for (j = 0; j < nBarrado && strcmp(barrado[j].id, vistos[i].id); j++) {}
        if (j == nBarrado && nBarrado < CL_BARRADOS_LOG) {
          memset(&barrado[j], 0, sizeof barrado[j]);
          snprintf(barrado[j].id, sizeof barrado[j].id, "%s", vistos[i].id);
          nBarrado++;
        }
        if (j < nBarrado) {
          VistoFonte *f = &barrado[j].f;
          if (f->bloqueados < VE_FONTE_PARES) f->par[f->bloqueados] = um.par[0];
          f->bloqueados++;
        }
        continue;
      }
      ke++;
      continue;
    }
    cat_historico_definir_id(vistos[i].id, vistos[i].tipo, 1);
    k++;
  }
  if (k) printf("[visto] marcado por conta-importacao pos=0 dur=0 assistido=0s n=%d titulos (sem reproducao local)\n", k);
  if (ke) printf("[visto] marcado por conta-importacao pos=0 dur=0 assistido=0s n=%d episodios (sem reproducao local)\n", ke);
  // O LOG POR FONTE (vistoep.h). A conta fala de TODOS os titulos num pull so,
  // e este pull roda a cada ciclo: uma linha de total, e uma por titulo SO
  // onde algo foi barrado — e la que se le "a conta ainda acha que foi visto".
  if (ke || total.bloqueados || total.venceu || jornal) {
    printf("[vistoep] conta: +%d episodios (bloqueados %d; remoto mais novo %d)",
           total.vistos, total.bloqueados, total.venceu);
    if (jornal) printf(" | %d seguros pelo jornal (delete ainda nao podado)", jornal);
    printf("\n");
  }
  for (j = 0; j < nBarrado; j++) {
    char quais[VE_FONTE_PARES * 10 + 8];
    size_t u = 0;
    int q, lim = barrado[j].f.bloqueados < VE_FONTE_PARES ? barrado[j].f.bloqueados
                                                          : VE_FONTE_PARES;
    quais[0] = 0;
    for (q = 0; q < lim && u + 12 < sizeof quais; q++)
      u += (size_t)snprintf(quais + u, sizeof quais - u, " T%dE%d",
                            barrado[j].f.par[q].temporada, barrado[j].f.par[q].episodio);
    printf("[vistoep] %s: conta bloqueados %d:%s%s\n", barrado[j].id,
           barrado[j].f.bloqueados, quais, barrado[j].f.bloqueados > lim ? " ..." : "");
  }
  fflush(stdout);
  return k;
}

void contalib_esquecer(void) {
  free(itens);
  itens = NULL;
  nItens = 0;
  pthread_mutex_lock(&vistosTrava);
  free(vistos);
  vistos = NULL;
  nVistos = 0;
  pthread_mutex_unlock(&vistosTrava);
  temConta = 0;
  aplicado = 0;
  marcaIdx = -1;
  marcaCatN = -1;
  marcaId[0] = 0;
}

// ------------------------------------------------- sementes do "a seguir"

static int porSerieEpisodio(const void *a, const void *b) {
  const ContaVisto *x = a, *y = b;
  int c = strcmp(x->id, y->id);
  if (c) return c;
  if (x->temporada != y->temporada) return x->temporada < y->temporada ? -1 : 1;
  if (x->episodio != y->episodio) return x->episodio < y->episodio ? -1 : 1;
  return 0;
}

static int maisRecente(const void *a, const void *b) {
  const ContaSemente *x = a, *y = b;
  if (x->vistoMs != y->vistoMs) return x->vistoMs > y->vistoMs ? -1 : 1;
  return strcmp(x->id, y->id);
}

int contalib_sementes_de(const ContaVisto *v, int n, ContaSemente *saida, int max,
                         int doMaisAlto) {
  ContaVisto *ep;
  ContaSemente *todas;
  int i, nEp = 0, nS = 0;
  if (!v || n < 1 || !saida || max < 1) return 0;
  ep = (ContaVisto *)malloc(sizeof *ep * (size_t)n);
  todas = (ContaSemente *)malloc(sizeof *todas * (size_t)n);
  if (!ep || !todas) { free(ep); free(todas); return 0; }
  // So linhas de EPISODIO. A de titulo inteiro (temporada/episodio 0) diz "a
  // obra foi vista", nao onde a pessoa parou. O content_type nao entra: linha
  // com temporada e episodio e episodio, venha rotulada como vier.
  for (i = 0; i < n; i++)
    if (v[i].id[0] && v[i].temporada > 0 && v[i].episodio > 0)
      ep[nEp++] = v[i];
  qsort(ep, (size_t)nEp, sizeof *ep, porSerieEpisodio);
  for (i = 0; i < nEp; ) {
    int j = i, a, prox, k;
    while (j < nEp && !strcmp(ep[j].id, ep[i].id)) j++;
    // [i, j) e uma serie, em ordem de temporada/episodio. A ancora: o mais
    // alto (o ultimo do grupo), ou o visto mais recente — empate, o mais alto.
    a = j - 1;
    if (!doMaisAlto)
      for (k = i; k < j; k++) if (ep[k].vistoMs >= ep[a].vistoMs) a = k;
    prox = ep[a].episodio + 1;
    // Pula o que ja foi visto na mesma temporada, depois da ancora.
    for (k = a + 1; k < j && ep[k].temporada == ep[a].temporada; k++)
      if (ep[k].episodio == prox) prox++;
    snprintf(todas[nS].id, sizeof todas[nS].id, "%s", ep[a].id);
    todas[nS].temporada = ep[a].temporada;
    todas[nS].episodio = prox;
    todas[nS].vistoMs = ep[a].vistoMs;
    nS++;
    i = j;
  }
  qsort(todas, (size_t)nS, sizeof *todas, maisRecente);
  if (nS > max) nS = max;
  memcpy(saida, todas, sizeof *todas * (size_t)nS);
  free(ep);
  free(todas);
  return nS;
}

int contalib_sementes_a_seguir(ContaSemente *saida, int max, int doMaisAlto) {
  int n;
  pthread_mutex_lock(&vistosTrava);
  // -1: vistos nunca puxados nesta sessao, ou o ultimo pull bom e velho. "Sem
  // semente" (0) e "nao sei" (-1) sao coisas diferentes: com outra fonte
  // vinculada, semear de um retrato velho ressuscita o que ja foi visto.
  if (!vistos || !vistosPuxadoEm ||
      time(NULL) - vistosPuxadoEm > CONTALIB_VISTOS_VALIDADE_S) {
    pthread_mutex_unlock(&vistosTrava);
    return -1;
  }
  n = contalib_sementes_de(vistos, nVistos, saida, max, doMaisAlto);
  pthread_mutex_unlock(&vistosTrava);
  return n;
}
