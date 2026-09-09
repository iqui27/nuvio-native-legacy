#include "extras.h"
#include "vistoep.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include "descoberta.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static int  notaTrakt, votosTrakt;
static int  notas[EX_NFONTES];
static char mdbChave[80];
static char dirArteEx[512];

// Nome do provedor na api do mdbList E nome do arquivo de marca em art/marcas.
// A ordem e a do enum, que e a do renderExternalRatingsRow do web.
static const char *FONTE[EX_NFONTES] = {
  "trakt", "imdb", "tmdb", "tomatoes", "audience", "metacritic", "letterboxd"
};

// A ESCALA MUDA POR PROVEDOR, e nao do jeito que parece. CONFERIDO na api com
// tt9737326: imdb=6.2 (0..10), mas trakt=66 e tmdb=70 — os dois em PORCENTAGEM,
// junto com tomatoes=59, audience=46 e metacritic=53. Eu tinha suposto que
// trakt e tmdb viessem em 0..10 como o imdb, e o resultado era "10.0" nos dois
// (66 x 10 estourava o teto e grudava no maximo).
//
// Guardamos o valor CRU multiplicado por 10, para o imdb caber em inteiro sem
// perder a casa decimal; quem desenha divide de novo conforme o provedor.
//
// DIVERGENCIA ANOTADA: o web nao reescala nada — formatMdbListRating
// (metaDetailsScreen.js:732) imprime o numero como veio, entao la o TMDB
// aparece como "70.0" ao lado de um IMDb "6.2". Aqui o TMDB vira "70%", que e
// o que o numero de fato e.
static int emDecimos(double v) {
  int n = (int)(v * 10.0 + 0.5);
  if (n < 0) n = 0;
  if (n > 1000) n = 1000;
  return n;
}

// 1 quando a nota da fonte e uma PORCENTAGEM; 0 quando e nota de 0 a 10.
int extras_fonte_percentual(int fonte) {
  return fonte != EX_IMDB && fonte != EX_LETTERBOXD;
}

void extras_definir_chave(const char *chave) {
  if (!chave || !*chave) return;
  snprintf(mdbChave, sizeof mdbChave, "%s", chave);
  printf("[extras] mdblist: chave da conta\n");
  fflush(stdout);
}

void extras_carregar(const char *dirArte) {
  char caminho[600];
  FILE *f;
  snprintf(dirArteEx, sizeof dirArteEx, "%s", dirArte ? dirArte : ".");
  snprintf(caminho, sizeof caminho, "%s/mdblist.txt", dirArte ? dirArte : ".");
  f = fopen(caminho, "r");
  if (!f) { printf("[extras] mdblist ausente\n"); fflush(stdout); return; }
  if (fgets(mdbChave, sizeof mdbChave, f)) {
    char *fim = mdbChave + strlen(mdbChave);
    while (fim > mdbChave && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
  }
  fclose(f);
  printf("[extras] mdblist %s\n", mdbChave[0] ? "ok" : "vazio"); fflush(stdout);
}

int extras_nota(int fonte) {
  return (fonte >= 0 && fonte < EX_NFONTES) ? notas[fonte] : 0;
}
const char *extras_fonte_marca(int fonte) {
  return (fonte >= 0 && fonte < EX_NFONTES) ? FONTE[fonte] : "";
}

const char *extras_caminho_marca(int fonte) {
  // ABSOLUTO. O cache de textura chama IMG_Load com o caminho como veio, e o
  // diretorio de trabalho do app nao e a pasta da arte — com "marcas/x.png"
  // relativo o arquivo simplesmente nao era achado e o cartao saia sem logo,
  // sem erro nenhum. O catalogo ja faz assim (catalogo.c:79).
  static char cam[600];
  if (fonte < 0 || fonte >= EX_NFONTES) return "";
  snprintf(cam, sizeof cam, "%s/marcas/%s.png", dirArteEx, FONTE[fonte]);
  return cam;
}

// Caminho de uma marca que NAO e fonte de nota — o wordmark do Trakt, hoje.
// Existe pelo mesmo motivo absoluto de cima: caminho relativo faz o IMG_Load
// falhar em silencio, e o desenho some sem erro nenhum.
const char *extras_caminho_marca_nome(const char *nome) {
  static char cam[600];
  if (!nome || !nome[0]) return "";
  snprintf(cam, sizeof cam, "%s/marcas/%s.png", dirArteEx, nome);
  return cam;
}
// `nota` e o user_rating do Trakt (0..10); 0 quando quem comentou nao avaliou.
// A referencia mostra "10/10  17 curtidas" no rodape do cartao, e sem a nota o
// rodape ficava so com o numero de curtidas — metade da informacao.
static struct { char user[40]; char texto[420]; int curtidas; int nota; } coment[EX_COMENT_MAX];

// COMENTARIOS DO EPISODIO, o outro lado do seletor "Série | Episódio" que a
// referencia poe acima dos cartoes. Sao uma consulta DIFERENTE
// (/shows/<id>/seasons/<t>/episodes/<e>/comments/likes), nao um filtro da lista
// da serie: o Trakt guarda as duas separadas, e comentario de episodio nunca
// aparece na lista da serie.
//
// Vem sob demanda — so quando o dono escolhe "Episódio" —, porque o custo e uma
// viagem por episodio e a maioria das visitas nunca troca de aba.
static struct { char user[40]; char texto[420]; int curtidas; int nota; } comentEp[EX_COMENT_MAX];
static int  nComentEp;
static int  epTempAtual, epNumAtual;    // de que episodio a lista acima e
static int  epFioVivo;
static char epShow[24];
static int  epPedTemp, epPedNum;
static int  nComent;
static struct { char titulo[120], ano[8], imdb[16], poster[200]; } rel[EX_REL_MAX];
// Vistos: um bit por episodio, ate 40 episodios em 20 temporadas. Vetor fixo
// porque a consulta acontece no DESENHO de cada card, a cada quadro — uma
// busca em lista ali custaria mais que a resposta.
#define EX_VIS_T 20
#define EX_VIS_E 40
static unsigned char vistos[EX_VIS_T][EX_VIS_E];
static int progressoPronto, proximoT, proximoE;
static int  nRel;
static struct { int numero; int nEps; struct { int ep, nota; } eps[EX_EP_MAX]; }
            temps[EX_TEMP_MAX];
static int  nTemps;
static char colNome[80];
// Ficha tecnica e trailers: mesma viagem /movie/<id> da colecao.
static char fichaStatus[32], fichaPaises[160], fichaCert[12], fichaLanc[16];
static int  fichaDur;
static struct { char yt[16], nome[80], mini[80]; } trailer[EX_TRAILER_MAX];
static int  nTrailer;
static struct { char titulo[120], ano[8]; long tmdb; } col[EX_COL_MAX];
static int  nCol;
static long tmdbEmCurso;

static char idPedido[24], idEmCurso[24];
static int  serieEmCurso, seriePedido, fioVivo;
static long tmdbPedido;
static pthread_t fio;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static void *buscar(void *arg);

static int pedidoAindaAtual(const char *id) {
  int atual;
  pthread_mutex_lock(&trava);
  atual = !strcmp(id, idPedido);
  pthread_mutex_unlock(&trava);
  return atual;
}

// Termina um pedido e, se o usuario abriu outro titulo durante a consulta,
// inicia imediatamente o pedido mais recente. Antes idPedido era trocado mas
// nenhum novo fio nascia: a tela seguinte permanecia vazia indefinidamente.
static void finalizarBusca(const char *id) {
  int continuar = 0;
  pthread_mutex_lock(&trava);
  if (strcmp(idPedido, id)) {
    snprintf(idEmCurso, sizeof idEmCurso, "%s", idPedido);
    serieEmCurso = seriePedido;
    tmdbEmCurso = tmdbPedido;
    continuar = 1;
  } else {
    fioVivo = 0;
  }
  pthread_mutex_unlock(&trava);
  if (continuar) {
    if (pthread_create(&fio, NULL, buscar, NULL) != 0) {
      pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    } else pthread_detach(fio);
  }
}

// O Trakt devolve a nota como fracao de 0 a 10 com casas ("7.83521"); o resto
// do app guarda nota em 0..100 inteiro, como o campo `nota` do catalogo.
static int para100(double v) {
  int n = (int)(v * 10.0 + 0.5);
  if (n < 0) n = 0;
  if (n > 100) n = 100;
  return n;
}

// Um comentario pode ter quebras de linha e aspas escapadas; o desenho e de uma
// caixa de texto corrida, entao troca-se tudo por espaco. js_texto ja resolve o
// escape de aspas e transforma \\uXXXX em espaco.
static void numaLinha(char *s) {
  for (; *s; s++) if (*s == '\n' || *s == '\r' || *s == '\t') *s = ' ';
}

static void *buscar(void *arg) {
  const char *cab[4];
  char aut[200], chave[140], url[200], id[24];
  const char *tipo;
  char *corpo;
  int serie;
  long tmdbId;
  (void)arg;

  pthread_mutex_lock(&trava);
  snprintf(id, sizeof id, "%s", idEmCurso);
  serie = serieEmCurso;
  tmdbId = tmdbEmCurso;
  tipo = serie ? "shows" : "movies";
  pthread_mutex_unlock(&trava);

  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) {
    finalizarBusca(id);
    return NULL;
  }

  // O QUE JA FOI VISTO VEM PRIMEIRO.
  //
  // Estava por ULTIMO, depois de ratings, comentarios e de um
  // `seasons?extended=episodes,full` que traz a serie inteira — quatro viagens
  // antes de a marca de assistido aparecer no card do episodio. E ela e o dado
  // mais visivel da tela e o que decide o rotulo do botao primario
  // ("Retomar"/"Próximo"), entao era exatamente o ultimo a chegar e o primeiro
  // que o dono nota faltando.
  // --- episodios ja assistidos (so serie) ---
  if (serie) {
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/progress/watched", id);
    corpo = rede_baixar_com(url, 20, cab);
    if (corpo) {
      unsigned char novo[EX_VIS_T][EX_VIS_E];
      const char *p = js_array(corpo, NULL, "seasons");
      memset(novo, 0, sizeof novo);
      while (p) {
        const char *f = js_fim(p);
        int t = (int)js_num(p, f, "number", -1.0);
        // O TETO DA MATRIZ NAO PODE PODAR O MAPA. Este `if` era
        // `t >= 0 && t < EX_VIS_T` e envolvia o laco inteiro: uma serie com
        // 21 temporadas perdia a 21 em silencio. A matriz [20][40] continua
        // limitada — ela e um vetor fixo —, mas vistoep guarda por chave e nao
        // tem por que herdar o limite dela.
        if (t >= 0) {
          const char *q = js_array(p, f, "episodes");
          while (q) {
            const char *qf = js_fim(q);
            int en = (int)js_num(q, qf, "number", -1.0);
            // "completed" e booleano; js_num nao le true/false, entao a leitura
            // e pelo texto — foi assim que a primeira versao marcou tudo como
            // nao visto sem erro nenhum.
            const char *c = strstr(q, "\"completed\"");
            int visto = 0;
            if (c && c < qf) { const char *v = c + 12;
                               while (*v == ' ' || *v == ':') v++;
                               visto = (*v == 't'); }
            if (visto && t < EX_VIS_T && en > 0 && en < EX_VIS_E) novo[t][en] = 1;
            // O MAPA SEM TETO recebe o episodio inteiro, visto ou nao. A matriz
            // acima so guarda o "sim" e nao distingue "nao viu" de "nao sei";
            // vistoep distingue, e e nisso que as acoes de marcar em lote se
            // apoiam. A matriz continua porque extras_ep_visto tem chamador no
            // desenho, e trocar as duas coisas no mesmo passo seria demais.
            if (en > 0) vistoep_definir(id, t, en, visto);
            q = js_prox(qf);
          }
        }
        p = js_prox(f);
      }
      // UMA LINHA, e ela existe porque silencio nao se diagnostica. Este e o
      // unico ponto que enche o mapa de episodios vistos; sem o log, "a marca
      // nao aparece na lista" nao distingue resposta vazia de parser errado de
      // teto estourado. Foi exatamente essa duvida que custou tres deploys no
      // dia em que o mapa nasceu.
      printf("[vistoep] %s: %d episodios no mapa (%d vistos)\n",
             id, vistoep_conhecido(id) ? vistoep_n() : 0, vistoep_contar(id));
      fflush(stdout);
      int pt = 0, pe = 0;
      const char *prox = strstr(corpo, "\"next_episode\"");
      if (prox && (prox = strchr(prox, ':'))) {
        prox++;
        while (*prox == ' ' || *prox == '\n' || *prox == '\r' || *prox == '\t') prox++;
        if (*prox == '{') {
          const char *fim = js_fim(prox);
          pt = (int)js_num(prox, fim, "season", 0);
          pe = (int)js_num(prox, fim, "number", 0);
        }
      }
      int valido = strstr(corpo, "\"seasons\"") != NULL;
      free(corpo);
      pthread_mutex_lock(&trava);
      if (!strcmp(id, idPedido) && valido) {
        memcpy(vistos, novo, sizeof vistos);
        proximoT = pt; proximoE = pe; progressoPronto = 1;
      }
      pthread_mutex_unlock(&trava);
    }
  }

  // O usuario ja abriu outro titulo. Nao gastar varias viagens opcionais com
  // uma tela que nao existe mais; entrega o fio ao pedido pendente.
  if (!pedidoAindaAtual(id)) { finalizarBusca(id); return NULL; }


  // --- nota ---
  snprintf(url, sizeof url, "https://api.trakt.tv/%s/%s/ratings", tipo, id);
  corpo = rede_baixar_com(url, 12, cab);
  if (corpo) {
    int n = para100(js_num(corpo, NULL, "rating", 0.0));
    int v = (int)js_num(corpo, NULL, "votes", 0.0);
    free(corpo);
    pthread_mutex_lock(&trava);
    if (!strcmp(id, idPedido)) {
      notaTrakt = n; votosTrakt = v;
      // Sem chave do mdbList esta e a UNICA nota do Trakt que teremos; com
      // chave, o passo seguinte sobrescreve com a que o mdbList devolver, que
      // e a mesma fonte que o web mostra.
      // notaTrakt esta em 0..100 (a api do Trakt devolve 0..10). No vetor a
      // escala e "cru x 10" e a fonte trakt e percentual, entao 6.7 -> 67% ->
      // 670. Sem esta conversao o cartao mostrava 6.7% quando nao havia
      // mdbList.
      if (!notas[EX_TRAKT]) notas[EX_TRAKT] = n * 10;
    }
    pthread_mutex_unlock(&trava);
  }

  // --- notas do mdbList, se o dono tiver chave ---
  if (serie && pedidoAindaAtual(id)) {
    snprintf(url,sizeof url,"https://api.trakt.tv/shows/%s?extended=full",id);
    corpo=rede_baixar_com(url,8,cab);
    if(corpo) {
      char estado[32]="";
      js_texto(corpo,NULL,"status",estado,sizeof estado);
      free(corpo);
      pthread_mutex_lock(&trava);
      if(!strcmp(id,idPedido)) snprintf(fichaStatus,sizeof fichaStatus,"%s",estado);
      pthread_mutex_unlock(&trava);
    }
  }

  //
  // Um POST por provedor, como o web faz (fetchProviderRating): a api aceita
  // "ids" em lote mas so um provedor por chamada. Sao sete chamadas curtas; o
  // fio ja e proprio, entao nao atrapalha o desenho.
  if (mdbChave[0]) {
    const char *cabJ[3];
    char kj[64];
    char corpoPost[80];
    int k;
    snprintf(kj, sizeof kj, "content-type: application/json");
    cabJ[0] = kj; cabJ[1] = NULL; cabJ[2] = NULL;
    snprintf(corpoPost, sizeof corpoPost,
             "{\"ids\":[\"%s\"],\"provider\":\"imdb\"}", id);
    for (k = 0; k < EX_NFONTES; k++) {
      char u[300], *rp;
      snprintf(u, sizeof u, "https://api.mdblist.com/rating/%s/%s?apikey=%s",
               serie ? "show" : "movie", FONTE[k], mdbChave);
      rp = rede_postar(u, 12, cabJ, corpoPost);
      if (!rp) continue;
      { double v = js_num(rp, NULL, "rating", -1.0);
        free(rp);
        if (v >= 0.0) {
          int c = emDecimos(v);
          pthread_mutex_lock(&trava);
          if (!strcmp(id, idPedido)) notas[k] = c;
          pthread_mutex_unlock(&trava);
        } }
    }
  }

  // --- comentarios, os mais curtidos primeiro ---
  snprintf(url, sizeof url,
           "https://api.trakt.tv/%s/%s/comments/likes?limit=%d", tipo, id,
           EX_COMENT_MAX);
  corpo = rede_baixar_com(url, 12, cab);
  if (corpo) {
    struct { char u[40]; char t[420]; int c; int nota; } achado[EX_COMENT_MAX];
    int n = 0;
    // p+1 e nao js_prox: js_prox recebe o FIM do elemento anterior, e aqui
    // ainda nao ha anterior. Com js_prox o primeiro item era pulado e, em
    // resposta de tres itens, sobrava lixo — as duas listas vinham vazias.
    const char *p = strchr(corpo, '[');
    p = p ? p + 1 : NULL;
    while (p && n < EX_COMENT_MAX) {
      const char *f = js_fim(p);
      achado[n].u[0] = achado[n].t[0] = 0;
      js_texto(p, f, "comment", achado[n].t, sizeof achado[n].t);
      // "username" esta dentro do objeto `user`; js_texto varre a faixa toda e
      // a unica ocorrencia dessa chave no item e essa.
      js_texto(p, f, "username", achado[n].u, sizeof achado[n].u);
      achado[n].c = (int)js_num(p, f, "likes", 0.0);
      achado[n].nota = (int)js_num(p, f, "user_rating", 0.0);
      numaLinha(achado[n].t);
      if (achado[n].t[0]) n++;
      p = js_prox(f);
    }
    free(corpo);
    pthread_mutex_lock(&trava);
    if (!strcmp(id, idPedido)) {
      int k;
      for (k = 0; k < n; k++) {
        snprintf(coment[k].user, sizeof coment[k].user, "%s", achado[k].u);
        snprintf(coment[k].texto, sizeof coment[k].texto, "%s", achado[k].t);
        coment[k].curtidas = achado[k].c;
        coment[k].nota = achado[k].nota;
      }
      nComent = n;
    }
    pthread_mutex_unlock(&trava);
  }

  // --- notas por episodio, so em serie ---
  if (serie) {
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/seasons?extended=episodes,full", id);
    corpo = rede_baixar_com(url, 20, cab);
    if (corpo) {
      int nt = 0;
      const char *p = strchr(corpo, '[');
      p = p ? p + 1 : NULL;
      while (p && nt < EX_TEMP_MAX) {
        const char *f = js_fim(p);
        int num = (int)js_num(p, f, "number", -1.0);
        // Temporada 0 e "especiais"; o web filtra `value > 0`.
        if (num > 0) {
          const char *q = js_array(p, f, "episodes");
          int ne = 0;
          while (q && ne < EX_EP_MAX) {
            const char *qf = js_fim(q);
            int en = (int)js_num(q, qf, "number", -1.0);
            double r = js_num(q, qf, "rating", 0.0);
            if (en > 0) {
              temps[nt].eps[ne].ep = en;
              temps[nt].eps[ne].nota = (int)(r * 10.0 + 0.5);
              ne++;
            }
            q = js_prox(qf);
          }
          if (ne > 0) { temps[nt].numero = num; temps[nt].nEps = ne; nt++; }
        }
        p = js_prox(f);
      }
      free(corpo);
      pthread_mutex_lock(&trava);
      if (!strcmp(id, idPedido)) nTemps = nt;
      pthread_mutex_unlock(&trava);
    }
  }

  // --- colecao (so filme, e so quando ja sabemos o id do TMDB) ---
  if (!serie) {
    const char *chave = desc_chave_tmdb();
    long idCol = 0, idFilme = tmdbId;
    char nome[80] = "";
    // O id do TMDB so fica no catalogo DEPOIS do enriquecimento do elenco; na
    // PRIMEIRA abertura de um titulo ele ainda e 0, e a aba nao apareceria
    // justamente na visita em que o dono esta olhando. /find resolve na hora.
    if (chave && chave[0] && idFilme <= 0) {
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/find/%s?api_key=%s"
               "&external_source=imdb_id", id, chave);
      corpo = rede_baixar(url, 15);
      if (corpo) {
        const char *v = js_array(corpo, NULL, "movie_results");
        if (v) idFilme = (long)js_num(v, js_fim(v), "id", 0.0);
        free(corpo);
      }
    }
    if (chave && chave[0] && idFilme > 0) {
      // `append_to_response` faz o TMDB devolver release_dates e videos DENTRO
      // deste mesmo corpo. Antes esta chamada ja acontecia e o parse lia so
      // belongs_to_collection: status, runtime, release_date e os paises
      // chegavam e eram descartados. Agora a ficha inteira e os trailers saem
      // daqui, sem nenhuma viagem a mais.
      snprintf(url, sizeof url,
               "%s/movie/%ld?api_key=%s&language=%s"
               "&append_to_response=release_dates,videos",
               "https://api.themoviedb.org/3", idFilme, chave, desc_tmdb_idioma());
      corpo = rede_baixar(url, 15);
      if (corpo) {
        // A ficha abaixo escreve varios campos globais. Segura a mesma trava
        // usada por extras_pedir para que uma troca de titulo nao limpe os
        // campos no meio do parse e receba, logo depois, metade da ficha velha.
        pthread_mutex_lock(&trava);
        if (strcmp(id, idPedido)) {
          pthread_mutex_unlock(&trava);
          free(corpo);
          finalizarBusca(id);
          return NULL;
        }
        const char *fimC = corpo + strlen(corpo);
        const char *b = strstr(corpo, "\"belongs_to_collection\"");
        if (b) {
          const char *o = strchr(b, '{');
          if (o) { const char *of = js_fim(o);
                   idCol = (long)js_num(o, of, "id", 0.0);
                   js_texto(o, of, "name", nome, sizeof nome); }
        }

        // --- ficha tecnica ---
        js_texto(corpo, fimC, "status", fichaStatus, sizeof fichaStatus);
        js_texto(corpo, fimC, "release_date", fichaLanc, sizeof fichaLanc);
        fichaDur = (int)js_num(corpo, fimC, "runtime", 0.0);

        // production_countries e um array de objetos; junta os nomes com
        // virgula, como a referencia mostra ("United States of America,
        // Canada"). Para de acrescentar quando o campo enche, em vez de cortar
        // um nome pela metade.
        { const char *p2 = js_array(corpo, fimC, "production_countries");
          fichaPaises[0] = 0;
          while (p2) {
            char pn[80] = "";
            const char *pf = js_fim(p2);
            js_texto(p2, pf, "name", pn, sizeof pn);
            if (pn[0]) {
              size_t usado = strlen(fichaPaises);
              size_t cabe  = sizeof fichaPaises - usado;
              size_t quer  = strlen(pn) + (usado ? 2 : 0) + 1;
              if (quer > cabe) break;
              snprintf(fichaPaises + usado, cabe, "%s%s", usado ? ", " : "", pn);
            }
            p2 = js_prox(pf);
          } }

        // Classificacao etaria: release_dates.results[] tem um bloco por pais,
        // e cada bloco tem release_dates[] com `certification`. Preferimos BR;
        // na falta, US; na falta das duas, a primeira nao-vazia que aparecer.
        // Muitos paises trazem a chave com string VAZIA, e aceitar a primeira
        // ocorrencia sem olhar o conteudo enchia o selo de nada.
        { const char *res = js_array(corpo, fimC, "results");
          char br[12] = "", us[12] = "", qq[12] = "";
          while (res) {
            const char *rf = js_fim(res);
            char pais[8] = "", c[12] = "";
            js_texto(res, rf, "iso_3166_1", pais, sizeof pais);
            { const char *d = js_array(res, rf, "release_dates");
              while (d && !c[0]) {
                const char *df = js_fim(d);
                js_texto(d, df, "certification", c, sizeof c);
                d = js_prox(df);
              } }
            if (c[0]) {
              if      (!strcmp(pais, "BR")) snprintf(br, sizeof br, "%s", c);
              else if (!strcmp(pais, "US")) snprintf(us, sizeof us, "%s", c);
              else if (!qq[0])              snprintf(qq, sizeof qq, "%s", c);
            }
            res = js_prox(rf);
          }
          snprintf(fichaCert, sizeof fichaCert, "%s",
                   br[0] ? br : us[0] ? us : qq); }

        // Trailers: videos.results[]. So YouTube (o unico host cuja miniatura
        // e obtivel por URL previsivel) e so o que for Trailer ou Teaser — o
        // TMDB mistura ali featurette, clipe e cena de bastidor.
        { const char *v = js_array(corpo, fimC, "results");
          // `results` aparece duas vezes no corpo (release_dates e videos);
          // procura a partir do bloco de videos para nao pegar o errado.
          const char *vid = strstr(corpo, "\"videos\"");
          if (vid) v = js_array(vid, fimC, "results");
          while (v && nTrailer < EX_TRAILER_MAX) {
            const char *vf = js_fim(v);
            char site[24] = "", tipo[24] = "", key[16] = "", nm[80] = "";
            js_texto(v, vf, "site", site, sizeof site);
            js_texto(v, vf, "type", tipo, sizeof tipo);
            js_texto(v, vf, "key",  key,  sizeof key);
            js_texto(v, vf, "name", nm,   sizeof nm);
            if (key[0] && !strcmp(site, "YouTube") &&
                (!strcmp(tipo, "Trailer") || !strcmp(tipo, "Teaser"))) {
              int k = nTrailer++;
              snprintf(trailer[k].yt,   sizeof trailer[k].yt,   "%s", key);
              snprintf(trailer[k].nome, sizeof trailer[k].nome, "%s",
                       nm[0] ? nm : "Trailer");
              snprintf(trailer[k].mini, sizeof trailer[k].mini,
                       "https://img.youtube.com/vi/%s/hqdefault.jpg", key);
            }
            v = js_prox(vf);
          } }

        pthread_mutex_unlock(&trava);
        free(corpo);
      }
    }
    if (idCol > 0) {
      snprintf(url, sizeof url, "%s/collection/%ld?api_key=%s&language=%s",
               "https://api.themoviedb.org/3", idCol, chave, desc_tmdb_idioma());
      corpo = rede_baixar(url, 15);
      if (corpo) {
        struct { char t[120], a[8]; long id; } ach[EX_COL_MAX];
        int nc = 0;
        const char *p = js_array(corpo, NULL, "parts");
        while (p && nc < EX_COL_MAX) {
          const char *f = js_fim(p);
          char data[16] = "";
          ach[nc].t[0] = ach[nc].a[0] = 0;
          js_texto(p, f, "title", ach[nc].t, sizeof ach[nc].t);
          js_texto(p, f, "release_date", data, sizeof data);
          if (strlen(data) >= 4) { memcpy(ach[nc].a, data, 4); ach[nc].a[4] = 0; }
          ach[nc].id = (long)js_num(p, f, "id", 0.0);
          if (ach[nc].t[0] && ach[nc].id > 0) nc++;
          p = js_prox(f);
        }
        free(corpo);
        pthread_mutex_lock(&trava);
        if (!strcmp(id, idPedido)) {
          int k;
          snprintf(colNome, sizeof colNome, "%s", nome);
          for (k = 0; k < nc; k++) {
            snprintf(col[k].titulo, sizeof col[k].titulo, "%s", ach[k].t);
            snprintf(col[k].ano, sizeof col[k].ano, "%s", ach[k].a);
            col[k].tmdb = ach[k].id;
          }
          nCol = nc;
        }
        pthread_mutex_unlock(&trava);
      }
    }
  }

  // Relacionados sao opcionais e podem custar mais uma viagem. Se o usuario
  // ja abriu outra obra, encadeia a mais recente agora em vez de prolongar a
  // espera com dados que serao descartados.
  if (!pedidoAindaAtual(id)) { finalizarBusca(id); return NULL; }

  // --- relacionados ---
  snprintf(url, sizeof url,
           "https://api.trakt.tv/%s/%s/related?limit=%d&extended=images",
           tipo, id, EX_REL_MAX);
  corpo = rede_baixar_com(url, 15, cab);
  if (corpo) {
    struct { char t[120], a[8], i[16], po[200]; } achado[EX_REL_MAX];
    int n = 0;
    // p+1 e nao js_prox: js_prox recebe o FIM do elemento anterior, e aqui
    // ainda nao ha anterior. Com js_prox o primeiro item era pulado e, em
    // resposta de tres itens, sobrava lixo — as duas listas vinham vazias.
    const char *p = strchr(corpo, '[');
    p = p ? p + 1 : NULL;
    while (p && n < EX_REL_MAX) {
      const char *f = js_fim(p);
      double ano;
      achado[n].t[0] = achado[n].i[0] = 0;
      js_texto(p, f, "title", achado[n].t, sizeof achado[n].t);
      js_texto(p, f, "imdb", achado[n].i, sizeof achado[n].i);
      // Procurar "poster" no item inteiro pega o campo ERRADO: o Trakt manda
      // `"colors":{"poster":["#D8D5CB",...]}` ANTES de
      // `"images":{"poster":[...]}`, e o log da primeira versao mostrou
      // `poster=https://#D8D5CB` — a cor media da arte, nao a arte. A busca
      // comeca dentro do objeto `images`.
      { const char *img = strstr(p, "\"images\"");
        const char *v = (img && img < f) ? js_array(img, f, "poster") : NULL;
        achado[n].po[0] = 0;
        if (v && *v == '"') {
          const char *e = strchr(v + 1, '"');
          size_t k = e ? (size_t)(e - v - 1) : 0;
          // O Trakt devolve o caminho SEM esquema ("media.trakt.tv/..."); sem o
          // https o cache de textura trata como arquivo local e nao acha nada.
          if (k > 0 && k + 9 < sizeof achado[n].po) {
            memcpy(achado[n].po, "https://", 8);
            memcpy(achado[n].po + 8, v + 1, k);
            achado[n].po[8 + k] = 0;
            // .webp FORA. MEDIDO na TV: o SDL_image deste pacote nao tem WebP
            // e todo cartaz do Trakt chega como `...jpg.webp` — 19 linhas de
            // "decode falhou (Unsupported image format)" num unico arranque, e
            // o sintoma na tela era o card vazio dos relacionados, tanto na
            // pagina de titulo quanto no fim da reproducao.
            //
            // O servidor guarda o ORIGINAL sob o mesmo caminho sem o sufixo:
            // conferido com curl, `...jpg.webp` devolve image/webp e `...jpg`
            // devolve image/jpeg dos mesmos bytes de arte. Tirar cinco
            // caracteres custa menos que embarcar libwebp no pacote.
            { size_t t = strlen(achado[n].po);
              if (t > 5 && !strcmp(achado[n].po + t - 5, ".webp"))
                achado[n].po[t - 5] = 0; }
          }
        } }
      ano = js_num(p, f, "year", 0.0);
      if (ano > 1800.0) snprintf(achado[n].a, sizeof achado[n].a, "%d", (int)ano);
      else achado[n].a[0] = 0;
      if (achado[n].t[0] && achado[n].i[0]) n++;
      p = js_prox(f);
    }
    free(corpo);
    pthread_mutex_lock(&trava);
    if (!strcmp(id, idPedido)) {
      int k;
      for (k = 0; k < n; k++) {
        snprintf(rel[k].titulo, sizeof rel[k].titulo, "%s", achado[k].t);
        snprintf(rel[k].ano, sizeof rel[k].ano, "%s", achado[k].a);
        snprintf(rel[k].imdb, sizeof rel[k].imdb, "%s", achado[k].i);
        snprintf(rel[k].poster, sizeof rel[k].poster, "%s", achado[k].po);
      }
      nRel = n;
    }
    pthread_mutex_unlock(&trava);
  }

  { int k, q = 0;
    for (k = 0; k < EX_NFONTES; k++) if (notas[k]) q++;
    printf("[extras] %s -> notas=%d/%d coment=%d rel=%d temps=%d\n", id, q,
           EX_NFONTES, nComent, nRel, nTemps); }
  printf("[extras] colecao \"%s\" -> %d | rel[0] poster=%s\n", colNome, nCol,
         nRel ? rel[0].poster : "(sem)"); fflush(stdout);
  fflush(stdout);
  finalizarBusca(id);
  return NULL;
}

void extras_pedir(const char *imdb, int serie, long tmdbId) {
  char id[24];
  const char *dp;
  if (!imdb || imdb[0] != 't' || !trakt_ativo()) return;
  // O campo do catalogo pode vir com episodio ("tt9737326:2:1"), que e o
  // formato que os addons de fonte usam. O Trakt so conhece o id do TITULO —
  // com o sufixo ele responde 404 e as tres abas ficavam vazias em toda serie.
  dp = strchr(imdb, ':');
  if (dp) { size_t n = (size_t)(dp - imdb);
            if (n >= sizeof id) n = sizeof id - 1;
            memcpy(id, imdb, n); id[n] = 0; }
  else snprintf(id, sizeof id, "%s", imdb);
  imdb = id;
  pthread_mutex_lock(&trava);
  if (!strcmp(idPedido, imdb)) { pthread_mutex_unlock(&trava); return; }
  snprintf(idPedido, sizeof idPedido, "%s", imdb);
  seriePedido = serie;
  tmdbPedido = tmdbId;
  notaTrakt = votosTrakt = nComent = nRel = nTemps = nCol = 0;
  colNome[0] = 0;
  nTrailer = fichaDur = 0;
  fichaStatus[0] = fichaPaises[0] = fichaCert[0] = fichaLanc[0] = 0;
  memset(vistos, 0, sizeof vistos);
  progressoPronto = proximoT = proximoE = 0;
  memset(notas, 0, sizeof notas);
  if (fioVivo) { pthread_mutex_unlock(&trava); return; }
  snprintf(idEmCurso, sizeof idEmCurso, "%s", imdb);
  serieEmCurso = serie;
  tmdbEmCurso = tmdbId;
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&fio, NULL, buscar, NULL) != 0) fioVivo = 0;
  else pthread_detach(fio);
}

int extras_nota_trakt(void)  { return notaTrakt; }
int extras_votos_trakt(void) { return votosTrakt; }

int extras_n_comentarios(void) { return nComent; }
const char *extras_comentario_usuario(int i) {
  return (i >= 0 && i < nComent) ? coment[i].user : "";
}
const char *extras_comentario_texto(int i) {
  return (i >= 0 && i < nComent) ? coment[i].texto : "";
}
int extras_comentario_curtidas(int i) {
  return (i >= 0 && i < nComent) ? coment[i].curtidas : 0;
}

// --- comentarios do EPISODIO --------------------------------------------------

static void *buscarEpComent(void *arg) {
  const char *cab[4];
  char aut[200], chave[140], url[260], show[24];
  char *corpo;
  int t, e;
  (void)arg;

  pthread_mutex_lock(&trava);
  snprintf(show, sizeof show, "%s", epShow);
  t = epPedTemp; e = epPedNum;
  pthread_mutex_unlock(&trava);

  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) {
    pthread_mutex_lock(&trava); epFioVivo = 0; pthread_mutex_unlock(&trava);
    return NULL;
  }
  snprintf(url, sizeof url,
           "https://api.trakt.tv/shows/%s/seasons/%d/episodes/%d/comments/likes?limit=%d",
           show, t, e, EX_COMENT_MAX);
  corpo = rede_baixar_com(url, 12, cab);
  if (corpo) {
    struct { char u[40]; char t[420]; int c; int nota; } achado[EX_COMENT_MAX];
    int n = 0;
    // p+1 e nao js_prox, pelo mesmo motivo da lista da serie: ainda nao ha
    // elemento anterior de onde partir.
    const char *p = strchr(corpo, '[');
    p = p ? p + 1 : NULL;
    while (p && n < EX_COMENT_MAX) {
      const char *f = js_fim(p);
      achado[n].u[0] = achado[n].t[0] = 0;
      js_texto(p, f, "comment", achado[n].t, sizeof achado[n].t);
      js_texto(p, f, "username", achado[n].u, sizeof achado[n].u);
      achado[n].c = (int)js_num(p, f, "likes", 0.0);
      achado[n].nota = (int)js_num(p, f, "user_rating", 0.0);
      numaLinha(achado[n].t);
      if (achado[n].t[0]) n++;
      p = js_prox(f);
    }
    free(corpo);
    pthread_mutex_lock(&trava);
    // So publica se o dono ainda esta no mesmo episodio: trocar de episodio
    // enquanto isto volta faria a lista antiga aparecer sob o rotulo novo.
    if (t == epPedTemp && e == epPedNum) {
      int k;
      for (k = 0; k < n; k++) {
        snprintf(comentEp[k].user, sizeof comentEp[k].user, "%s", achado[k].u);
        snprintf(comentEp[k].texto, sizeof comentEp[k].texto, "%s", achado[k].t);
        comentEp[k].curtidas = achado[k].c;
        comentEp[k].nota = achado[k].nota;
      }
      nComentEp = n;
      epTempAtual = t; epNumAtual = e;
    }
    pthread_mutex_unlock(&trava);
  }
  pthread_mutex_lock(&trava); epFioVivo = 0; pthread_mutex_unlock(&trava);
  return NULL;
}

void extras_pedir_comentarios_ep(const char *imdbSerie, int temporada, int episodio) {
  pthread_t f;
  if (!imdbSerie || !imdbSerie[0] || temporada <= 0 || episodio <= 0) return;
  pthread_mutex_lock(&trava);
  // Mesmo episodio ja carregado (ou em voo): nao repete a viagem.
  if (epFioVivo ||
      (temporada == epTempAtual && episodio == epNumAtual && nComentEp > 0)) {
    pthread_mutex_unlock(&trava);
    return;
  }
  // O id pode vir como "tt123:2:4" da lista de episodios; o Trakt quer so a
  // serie.
  { const char *dp;
    snprintf(epShow, sizeof epShow, "%s", imdbSerie);
    dp = strchr(epShow, ':');
    if (dp) *(char *)dp = 0; }
  epPedTemp = temporada; epPedNum = episodio;
  nComentEp = 0;                 // limpa: a lista velha e de outro episodio
  epTempAtual = epNumAtual = 0;
  epFioVivo = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, buscarEpComent, NULL) != 0) {
    pthread_mutex_lock(&trava); epFioVivo = 0; pthread_mutex_unlock(&trava);
  } else {
    pthread_detach(f);
  }
}

int extras_n_comentarios_ep(void) { return nComentEp; }
int extras_comentarios_ep_carregando(void) { return epFioVivo; }

// O fio de extras ainda esta no ar. Serve para separar "ainda nao chegou" de
// "nao ha", que sao a mesma lista vazia — sem isso a pagina de filme mostrava
// um buraco onde as recomendacoes vao ficar, ate elas chegarem.
int extras_carregando(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = fioVivo;
  pthread_mutex_unlock(&trava);
  return v;
}
const char *extras_comentario_ep_usuario(int i) {
  return (i >= 0 && i < nComentEp) ? comentEp[i].user : "";
}
const char *extras_comentario_ep_texto(int i) {
  return (i >= 0 && i < nComentEp) ? comentEp[i].texto : "";
}
int extras_comentario_ep_curtidas(int i) {
  return (i >= 0 && i < nComentEp) ? comentEp[i].curtidas : 0;
}
int extras_comentario_ep_nota(int i) {
  return (i >= 0 && i < nComentEp) ? comentEp[i].nota : 0;
}

int extras_comentario_nota(int i) {
  return (i >= 0 && i < nComent) ? coment[i].nota : 0;
}

const char *extras_ficha_status(void)        { return fichaStatus; }
int         extras_ficha_duracao(void)       { return fichaDur; }
const char *extras_ficha_paises(void)        { return fichaPaises; }
const char *extras_ficha_classificacao(void) { return fichaCert; }
const char *extras_ficha_lancamento(void)    { return fichaLanc; }

int extras_n_trailers(void) { return nTrailer; }
const char *extras_trailer_yt(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].yt : "";
}
const char *extras_trailer_nome(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].nome : "";
}
const char *extras_trailer_miniatura(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].mini : "";
}

const char *extras_colecao_nome(void) { return colNome; }
int extras_n_colecao(void) { return nCol; }
const char *extras_colecao_titulo(int i) {
  return (i >= 0 && i < nCol) ? col[i].titulo : "";
}
const char *extras_colecao_ano(int i) {
  return (i >= 0 && i < nCol) ? col[i].ano : "";
}
long extras_colecao_tmdb(int i) { return (i >= 0 && i < nCol) ? col[i].tmdb : 0; }

int extras_n_temporadas(void) { return nTemps; }
int extras_temporada_numero(int t) {
  return (t >= 0 && t < nTemps) ? temps[t].numero : 0;
}
int extras_n_eps(int t) { return (t >= 0 && t < nTemps) ? temps[t].nEps : 0; }
int extras_ep_numero(int t, int i) {
  return (t >= 0 && t < nTemps && i >= 0 && i < temps[t].nEps) ? temps[t].eps[i].ep : 0;
}
int extras_ep_nota(int t, int i) {
  return (t >= 0 && t < nTemps && i >= 0 && i < temps[t].nEps) ? temps[t].eps[i].nota : 0;
}

int extras_n_relacionados(void) { return nRel; }
const char *extras_relacionado_titulo(int i) {
  return (i >= 0 && i < nRel) ? rel[i].titulo : "";
}
const char *extras_relacionado_ano(int i) {
  return (i >= 0 && i < nRel) ? rel[i].ano : "";
}
const char *extras_relacionado_imdb(int i) {
  return (i >= 0 && i < nRel) ? rel[i].imdb : "";
}
const char *extras_relacionado_poster(int i) {
  return (i >= 0 && i < nRel) ? rel[i].poster : "";
}

int extras_ep_visto(int temporada, int episodio) {
  if (temporada < 0 || temporada >= EX_VIS_T) return 0;
  if (episodio < 0 || episodio >= EX_VIS_E) return 0;
  pthread_mutex_lock(&trava);
  int visto = vistos[temporada][episodio];
  pthread_mutex_unlock(&trava);
  return visto;
}

int extras_progresso_pronto(void) {
  pthread_mutex_lock(&trava);
  int pronto = progressoPronto;
  pthread_mutex_unlock(&trava);
  return pronto;
}
int extras_proximo_episodio(int *t, int *e) {
  pthread_mutex_lock(&trava);
  int ok = progressoPronto && proximoT > 0 && proximoE > 0;
  if (ok) { *t = proximoT; *e = proximoE; }
  pthread_mutex_unlock(&trava);
  return ok;
}
