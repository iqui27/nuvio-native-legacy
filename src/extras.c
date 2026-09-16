#include "extras.h"
#include "vistoep.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include "descoberta.h"
#include "ajustes.h"
#include "agenda.h"
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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

int extras_mdblist_tem_chave(void) { return mdbChave[0] != 0; }

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
  // mdblist_show_* esconde a fonte na hora de LER, nao na de buscar: o cartao
  // some ja no primeiro ajuste, sem esperar o titulo ser reaberto.
  if (fonte < 0 || fonte >= EX_NFONTES) return 0;
  if (!ajustes_mdblist_fonte(fonte)) return 0;
  return notas[fonte];
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
// QUANTOS EPISODIOS JA EXIBIDOS E QUANTOS VISTOS, os dois numeros do TOPO da
// resposta de /progress/watched. Nao sao derivaveis da matriz `vistos`: ela e
// [EX_VIS_T][EX_VIS_E] e uma serie maior que isso cai fora dela em silencio — e
// "aired" do Trakt ja desconta episodio que ainda nao foi ao ar, que e
// justamente o denominador certo para uma porcentagem.
static int epsExibidos, epsVistos;
static int  nRel;
static struct { int numero; int nEps; struct { int ep, nota; } eps[EX_EP_MAX]; }
            temps[EX_TEMP_MAX];
static int  nTemps;
static char colNome[80];
// Ficha tecnica e trailers: mesma viagem /movie/<id> da colecao.
static char fichaStatus[32], fichaPaises[160], fichaCert[12], fichaLanc[16];
static int  fichaDur;
// AGENDA DA SERIE — proximo episodio e situacao, do MESMO corpo /tv/<id>.
// Ver agenda.h para o porque de nao ser o Trakt. Os campos sempre estiveram
// neste corpo; o parse antigo os jogava fora, como jogava fora status e
// runtime do filme antes da ficha tecnica existir.
static char agStatus[32], agDataProx[16], agDataUlt[16], agNomeEp[120];
static int  agTemp, agEp;
static struct { char yt[16], nome[80], mini[80]; } trailer[EX_TRAILER_MAX];
static int  nTrailer;
static struct { char titulo[120], ano[8]; long tmdb; } col[EX_COL_MAX];
static int  nCol;
// PRODUTORAS E REDES, para a fileira de logos da pagina de detalhe. No web sao
// duas secoes ("Production", "Network"); aqui viram uma so lista — a rede vem
// primeiro nas series, como no renderCompanySections da referencia.
// `rede` decide o tipo da fonte TMDB que o OK abre (NETWORK x COMPANY).
#define EX_EST_MAX 10
static struct { char nome[80], logo[220]; long tmdb; int rede; }
            estudio[EX_EST_MAX];
static int  nEstudio;
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

  // O Trakt E OPCIONAL. Os blocos que dependem dele (vistos, nota Trakt,
  // comentarios, notas por episodio, related de sobra) rodam so quando ha
  // sessao; a ficha TMDB, o MDBList e as recomendacoes nao precisam dele.
  // Antes disto um Trakt desvinculado matava o fio inteiro — e junto com ele
  // morriam os relacionados TMDB, que nao consultam o Trakt em nada.
  int temTrakt = trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave);
  // CABECALHOS SEM CONTA, para o unico bloco daqui que nao precisa de uma.
  // MEDIDO em 16/09/2026: `seasons?extended=episodes,full` responde 200 com
  // `trakt-api-key` sozinho — o token nunca fez falta ali, e era so por ele
  // estar amarrado a trakt_cabecalhos() que quem nao vinculou o Trakt ficava
  // sem NENHUMA nota por episodio (a aba "Avaliações" caia nos cartoes do
  // filme) e sem os graficos de audiencia, que leem a mesma lista.
  // Sao vetores PROPRIOS e nao um remendo em `cab`: o `cab` vale para o resto
  // da funcao, que continua exigindo token de verdade.
  const char *cabPub[3];
  char chavePub[140];
  int temChave = trakt_cabecalhos_publicos(cabPub, chavePub, sizeof chavePub);

  // O QUE JA FOI VISTO VEM PRIMEIRO.
  //
  // Estava por ULTIMO, depois de ratings, comentarios e de um
  // `seasons?extended=episodes,full` que traz a serie inteira — quatro viagens
  // antes de a marca de assistido aparecer no card do episodio. E ela e o dado
  // mais visivel da tela e o que decide o rotulo do botao primario
  // ("Retomar"/"Próximo"), entao era exatamente o ultimo a chegar e o primeiro
  // que o dono nota faltando.
  // --- episodios ja assistidos (so serie) ---
  if (serie && temTrakt) {
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/progress/watched", id);
    corpo = rede_baixar_com(url, 20, cab);
    if (corpo) {
      unsigned char novo[EX_VIS_T][EX_VIS_E];
      const char *p = js_array(corpo, NULL, "seasons");
      // OS DOIS CONTADORES SAEM DO CABECALHO, antes do array — e tem de ser
      // antes MESMO: "completed" reaparece como BOOLEANO em cada episodio, e
      // procurar no corpo inteiro acharia o do primeiro episodio em vez do
      // total. js_num sobre a fatia que termina onde o array comeca resolve.
      int exib = 0, vist = 0;
      { const char *cab = strstr(corpo, "\"seasons\"");
        const char *fimCab = cab ? cab : corpo + strlen(corpo);
        exib = (int)js_num(corpo, fimCab, "aired", 0.0);
        vist = (int)js_num(corpo, fimCab, "completed", 0.0); }
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
        epsExibidos = exib; epsVistos = vist;
      }
      pthread_mutex_unlock(&trava);
    }
  }

  // O usuario ja abriu outro titulo. Nao gastar varias viagens opcionais com
  // uma tela que nao existe mais; entrega o fio ao pedido pendente.
  if (!pedidoAindaAtual(id)) { finalizarBusca(id); return NULL; }


  // --- nota ---
  if (temTrakt) {
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
  }

  // --- status da serie (Trakt; o TMDB ja trouxe o seu na ficha) ---
  if (serie && temTrakt && pedidoAindaAtual(id)) {
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
  // mdblist_enabled corta a CONSULTA — as notas que o app tem por conta
  // propria (Trakt direto, IMDb do catalogo) nao dependem desta chave.
  if (mdbChave[0] && ajustes_mdblist_ligado()) {
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
  if (temTrakt) {
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
  }

  // --- notas por episodio, so em serie ---
  // SEM CONTA TAMBEM. Ver a nota de cabPub la em cima: a chave do aplicativo
  // basta para esta chamada, entao a condicao e "ha alguma credencial", e nao
  // "ha token". Com token o cabecalho continua sendo o completo — nao ha
  // motivo para pedir anonimamente quem esta logado.
  if (serie && (temTrakt || temChave)) {
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/seasons?extended=episodes,full", id);
    corpo = rede_baixar_com(url, 20, temTrakt ? cab : cabPub);
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

  // --- TMDB: ficha, trailers, produtoras, recomendacoes (filme E serie) ---
  //
  // Antes so o filme entrava aqui — a serie ficava sem os logos de rede, sem
  // "Mais como este" do TMDB e sem nada do append. O que muda por tipo: o
  // endpoint (movie x tv), o /find (movie_results x tv_results) e os extras
  // do append (release_dates so existe em filme; em serie a classificacao e
  // content_ratings, e ainda nao ha onde mostra-la — o Detalhes e so de filme).
  {
    const char *chave = desc_chave_tmdb();
    long idCol = 0, idT = tmdbId;
    char nome[80] = "";
    int tNums[EX_TEMP_MAX], nTNum = 0;
    // O id do TMDB so fica no catalogo DEPOIS do enriquecimento do elenco; na
    // PRIMEIRA abertura de um titulo ele ainda e 0, e a aba nao apareceria
    // justamente na visita em que o dono esta olhando. /find resolve na hora.
    if (chave[0] && idT <= 0) {
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/find/%s?api_key=%s"
               "&external_source=imdb_id", id, chave);
      corpo = rede_baixar(url, 15);
      if (corpo) {
        const char *v = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
        if (v) idT = (long)js_num(v, js_fim(v), "id", 0.0);
        free(corpo);
      }
    }
    // Nao buscar quando nao ha NADA ligado que saia desta viagem: economiza o
    // pedido quando o dono desligou ficha, trailers, produtoras e recomenda-
    // coes de uma vez — o toggle de cada uma ja diz que nao vale ir.
    // A SERIE PASSA A PEDIR SEMPRE, e o filme continua sob os toggles.
    //
    // Este corpo traz `status`, `next_episode_to_air` e `last_episode_to_air`,
    // que sao a AGENDA da serie — informacao base da pagina, do mesmo estatuto
    // que status e runtime tem no filme, e nao um extra opcional. Amarra-la a
    // "Produtoras"/"Redes"/"Mais como este"/"Episodios" faria a data de
    // estreia sumir por causa de um toggle que fala de outra coisa.
    //
    // O QUE ISTO CUSTA, dito por inteiro: na configuracao padrao, NADA — os
    // quatro toggles vem ligados e o pedido ja acontecia. Para quem desligou
    // os quatro, e um GET /tv/<id> por serie aberta que antes nao havia.
    if (chave[0] && idT > 0 &&
        (serie ||
         (ajustes_tmdb_ficha() || ajustes_tmdb_datas() ||
          ajustes_tmdb_trailers() || ajustes_tmdb_col() ||
          ajustes_tmdb_prod()))) {
      // `append_to_response` faz o TMDB devolver extras DENTRO deste mesmo
      // corpo. O conjunto segue os toggles da conta: release_dates responde a
      // tmdb_use_release_dates, videos a tmdb_use_trailers, recommendations a
      // tmdb_use_more_like_this. O corpo sempre traz status, runtime e as
      // produtoras/redes — ler nao custa viagem nenhuma.
      char append[80] = "";
      if (!serie && ajustes_tmdb_datas())    snprintf(append, sizeof append, "release_dates");
      if (!serie && ajustes_tmdb_trailers()) snprintf(append + strlen(append), sizeof append - strlen(append), "%svideos", append[0] ? "," : "");
      if (ajustes_tmdb_mais())               snprintf(append + strlen(append), sizeof append - strlen(append), "%srecommendations", append[0] ? "," : "");
      snprintf(url, sizeof url,
               "%s/%s/%ld?api_key=%s&language=%s%s%s",
               "https://api.themoviedb.org/3", serie ? "tv" : "movie", idT,
               chave, desc_tmdb_idioma(),
               append[0] ? "&append_to_response=" : "", append);
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
        // --- AGENDA DA SERIE (nenhuma viagem a mais) ----------------------
        //
        // `status`, `next_episode_to_air` e `last_episode_to_air` ja vinham
        // neste corpo e eram descartados. Sao os tres campos de que a tela de
        // titulo e o calendario precisam para dizer QUANDO sai o proximo
        // episodio — ou para dizer que a serie acabou, em vez de inventar uma
        // data. Ver agenda.h.
        //
        // `null` e um valor legitimo dos dois blocos: serie encerrada nao tem
        // proximo episodio. A guarda compara a posicao do "null" com a da
        // primeira chave; sem ela, a busca por '{' pularia para o objeto
        // seguinte do corpo e a serie encerrada herdaria a data de outra coisa.
        if (serie) {
          const char *b = strstr(corpo, "\"next_episode_to_air\"");
          agTemp = agEp = 0;
          agDataProx[0] = agNomeEp[0] = agDataUlt[0] = 0;
          js_texto(corpo, fimC, "status", agStatus, sizeof agStatus);
          if (b) {
            const char *o = strchr(b, '{');
            const char *nulo = strstr(b, "null");
            if (o && (!nulo || nulo > o)) {
              const char *of = js_fim(o);
              agTemp = (int)js_num(o, of, "season_number", 0.0);
              agEp   = (int)js_num(o, of, "episode_number", 0.0);
              js_texto(o, of, "air_date", agDataProx, sizeof agDataProx);
              js_texto(o, of, "name", agNomeEp, sizeof agNomeEp);
            }
          }
          b = strstr(corpo, "\"last_episode_to_air\"");
          if (b) {
            const char *o = strchr(b, '{');
            const char *nulo = strstr(b, "null");
            if (o && (!nulo || nulo > o))
              js_texto(o, js_fim(o), "air_date", agDataUlt, sizeof agDataUlt);
          }
        }
        if (!serie) {
          const char *b = strstr(corpo, "\"belongs_to_collection\"");
          if (b) {
            const char *o = strchr(b, '{');
            if (o) { const char *of = js_fim(o);
                     idCol = (long)js_num(o, of, "id", 0.0);
                     js_texto(o, of, "name", nome, sizeof nome); }
          }
        }

        // --- ficha tecnica (so filme: a serie nao tem secao de Detalhes) ---
        if (!serie && ajustes_tmdb_ficha()) {
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
        }

        // Classificacao etaria: release_dates.results[] tem um bloco por pais,
        // e cada bloco tem release_dates[] com `certification`. Preferimos BR;
        // na falta, US; na falta das duas, a primeira nao-vazia que aparecer.
        // Muitos paises trazem a chave com string VAZIA, e aceitar a primeira
        // ocorrencia sem olhar o conteudo enchia o selo de nada.
        if (!serie && ajustes_tmdb_datas())
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
        // Filme apenas: a fileira de trailers nao existe no layout de serie.
        if (!serie && ajustes_tmdb_trailers())
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

        // PRODUTORAS E REDES — a fileira de logos da pagina de detalhe. No web
        // (renderCompanySections) sao duas secoes, rede primeiro na serie;
        // aqui uma lista so, mesma ordem. O toggle separa os dois: desligar
        // "Produtoras" nao derruba as redes da serie.
        { int ne = 0, passo;
          static const char *CAMPO[2] = { "production_companies", "networks" };
          // Serie: redes primeiro, como o web; filme: so produtoras (a /movie
          // nao tem campo networks).
          for (passo = 0; passo < 2; passo++) {
            int rede = (passo == 1);
            const char *p2;
            if (!serie && rede) break;
            if (rede && !ajustes_tmdb_redes()) continue;
            if (!rede && !ajustes_tmdb_prod()) continue;
            p2 = js_array(corpo, fimC, CAMPO[passo]);
            while (p2 && ne < EX_EST_MAX) {
              const char *pf = js_fim(p2);
              char nm[80] = "", lg[220] = "", lp[160] = "";
              long tid;
              js_texto(p2, pf, "name", nm, sizeof nm);
              tid = (long)js_num(p2, pf, "id", 0.0);
              // logo_path costuma ser .svg — o SDL_image do pacote nao decoda.
              // Guarda so quando for raster (png/jpg); o card cai no nome.
              if (js_texto(p2, pf, "logo_path", lp, sizeof lp) &&
                  lp[0] == '/' && !strstr(lp, ".svg"))
                snprintf(lg, sizeof lg,
                         "https://image.tmdb.org/t/p/w185%s", lp);
              if (nm[0] || tid > 0) {
                snprintf(estudio[ne].nome, sizeof estudio[ne].nome, "%s", nm);
                snprintf(estudio[ne].logo, sizeof estudio[ne].logo, "%s", lg);
                estudio[ne].tmdb = tid;
                estudio[ne].rede = rede;
                ne++;
              }
              p2 = js_prox(pf);
            }
          }
          nEstudio = ne; }

        // "MAIS COMO ESTE" DO TMDB — recommendations.results[]. Preenche rel[]
        // com o id do TMDB (sem imdb ainda); o detalhe abre pelo prefixo
        // "tmdb:", como o credito de ator ja faz. O Trakt continua sendo a
        // resposta quando este append falhar ou vier vazio — ver a guarda
        // `!nRel` na consulta de relacionados logo abaixo.
        if (ajustes_tmdb_mais()) {
          const char *rec = strstr(corpo, "\"recommendations\"");
          const char *p2 = rec ? js_array(rec, fimC, "results") : NULL;
          int nr = 0;
          while (p2 && nr < EX_REL_MAX) {
            const char *pf = js_fim(p2);
            char t[120] = "", dt[16] = "", pp[200] = "";
            long tid;
            js_texto(p2, pf, serie ? "name" : "title", t, sizeof t);
            js_texto(p2, pf, serie ? "first_air_date" : "release_date",
                     dt, sizeof dt);
            if (js_texto(p2, pf, "poster_path", pp, sizeof pp) && pp[0] == '/') {
              char url2[230];
              snprintf(url2, sizeof url2,
                       "https://image.tmdb.org/t/p/w342%s", pp);
              snprintf(pp, sizeof pp, "%s", url2);
            } else pp[0] = 0;
            tid = (long)js_num(p2, pf, "id", 0.0);
            if (t[0] && tid > 0) {
              snprintf(rel[nr].titulo, sizeof rel[nr].titulo, "%s", t);
              if (strlen(dt) >= 4) { memcpy(rel[nr].ano, dt, 4); rel[nr].ano[4] = 0; }
              else rel[nr].ano[0] = 0;
              // O marcador "tmdb:" e o que detail.c checa no OK — o item nao
              // tem imdb ate o titulo ser aberto.
              snprintf(rel[nr].imdb, sizeof rel[nr].imdb, "tmdb:%ld", tid);
              snprintf(rel[nr].poster, sizeof rel[nr].poster, "%s", pp);
              nr++;
            }
            p2 = js_prox(pf);
          }
          nRel = nr;
        }

        // Temporadas declaradas — a lista de numeros que o enriquecimento de
        // episodios (abaixo, fora do corpo) vai pedir uma a uma.
        if (serie && ajustes_tmdb_eps())
        { const char *p2 = js_array(corpo, fimC, "seasons");
          while (p2 && nTNum < EX_TEMP_MAX) {
            const char *pf = js_fim(p2);
            int sn = (int)js_num(p2, pf, "season_number", -1.0);
            if (sn > 0) tNums[nTNum++] = sn;
            p2 = js_prox(pf);
          } }

        pthread_mutex_unlock(&trava);
        free(corpo);
        // FORA DA TRAVA de proposito: agenda_registrar grava em disco e tem
        // trava propria. Segurar a deste modulo durante uma escrita de arquivo
        // faria o desenho esperar o disco no fio principal.
        if (serie && (agStatus[0] || agDataProx[0]))
          agenda_registrar(id, NULL, NULL, agStatus, agTemp, agEp, agNomeEp,
                           agDataProx, agDataUlt);
      }
    }

    // NOTAS DE EPISODIO DO TMDB — um pedido por temporada, como o
    // fetchEpisodeEnrichment do web. Os votos entram em temps[] completando o
    // que o Trakt nao tem (nota 0 ou episodio ausente); a fileira de notas
    // por episodio nao precisa saber de onde vieram.
    if (serie && ajustes_tmdb_eps() && idT > 0) {
      int sn[EX_TEMP_MAX], nsn = 0, t2;
      // Uniao das temporadas que o Trakt ja deu com as que o TMDB declarou.
      for (t2 = 0; t2 < nTemps && nsn < EX_TEMP_MAX; t2++)
        sn[nsn++] = temps[t2].numero;
      for (t2 = 0; t2 < nTNum && nsn < EX_TEMP_MAX; t2++) {
        int k, tem = 0;
        for (k = 0; k < nsn; k++) if (sn[k] == tNums[t2]) tem = 1;
        if (!tem) sn[nsn++] = tNums[t2];
      }
      for (t2 = 0; t2 < nsn; t2++) {
        // Temporada que o Trakt ja deu INTEIRA (todos os episodios com nota)
        // nao precisa de uma viagem ao TMDB — era o rabo mais longo da cadeia:
        // uma serie de 12 temporadas fazia 12 GETs para preencher lacuna nenhuma.
        { int t3, completa = 0, e3;
          pthread_mutex_lock(&trava);
          for (t3 = 0; t3 < nTemps; t3++)
            if (temps[t3].numero == sn[t2]) break;
          if (t3 < nTemps && temps[t3].nEps > 0) {
            completa = 1;
            for (e3 = 0; e3 < temps[t3].nEps; e3++)
              if (!temps[t3].eps[e3].nota) { completa = 0; break; }
          }
          pthread_mutex_unlock(&trava);
          if (completa) continue; }
        snprintf(url, sizeof url,
                 "%s/tv/%ld/season/%d?api_key=%s&language=%s",
                 "https://api.themoviedb.org/3", idT, sn[t2], chave,
                 desc_tmdb_idioma());
        corpo = rede_baixar(url, 15);
        if (!corpo) continue;
        { struct { int ep, nota; } eps[EX_EP_MAX];
          int ne = 0;
          const char *p2 = js_array(corpo, NULL, "episodes");
          while (p2 && ne < EX_EP_MAX) {
            const char *pf = js_fim(p2);
            int en = (int)js_num(p2, pf, "episode_number", -1.0);
            double r = js_num(p2, pf, "vote_average", 0.0);
            if (en > 0 && r > 0.0) {
              eps[ne].ep = en;
              eps[ne].nota = (int)(r * 10.0 + 0.5);
              ne++;
            }
            p2 = js_prox(pf);
          }
          free(corpo);
          if (!ne) continue;
          pthread_mutex_lock(&trava);
          if (!strcmp(id, idPedido)) {
            int t3;
            for (t3 = 0; t3 < nTemps; t3++)
              if (temps[t3].numero == sn[t2]) break;
            if (t3 == nTemps && nTemps < EX_TEMP_MAX) {
              temps[t3].numero = sn[t2];
              temps[t3].nEps = 0;
              nTemps++;
            }
            if (t3 < nTemps) {
              int e2;
              for (e2 = 0; e2 < ne; e2++) {
                int k;
                for (k = 0; k < temps[t3].nEps; k++)
                  if (temps[t3].eps[k].ep == eps[e2].ep) break;
                if (k < temps[t3].nEps) {
                  // Trakt sem nota (0) recebe a do TMDB; nota de verdade
                  // fica — a do Trakt reflete a comunidade que o app ja usa.
                  if (!temps[t3].eps[k].nota)
                    temps[t3].eps[k].nota = eps[e2].nota;
                } else if (temps[t3].nEps < EX_EP_MAX) {
                  temps[t3].eps[k].ep = eps[e2].ep;
                  temps[t3].eps[k].nota = eps[e2].nota;
                  temps[t3].nEps++;
                }
              }
            }
          }
          pthread_mutex_unlock(&trava);
        }
      }
    }
    if (!serie && ajustes_tmdb_col() && idCol > 0) {
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
  // "Mais como este": quando o TMDB ja encheu rel[] pelo append
  // recommendations (tmdb_use_more_like_this), o Trakt nao e consultado —
  // e o web faz a mesma escolha (TMDB primeiro, Trakt como o que sobra).
  // Quando o append falhou ou veio vazio, o Trakt responde como sempre.
  if (nRel == 0 && temTrakt)
  {
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
  if (!imdb || imdb[0] != 't') return;
  // Sem nenhuma fonte online nao ha o que buscar: o Trakt alimenta vistos,
  // nota, comentarios e related de sobra; a chave TMDB alimenta ficha,
  // produtoras e "Mais como este"; o MDBList alimenta as notas por fonte.
  // Antes o teste era so trakt_ativo — e uma conta sem Trakt perdia ate o
  // que nao depende dele.
  if (!trakt_ativo() && !desc_chave_tmdb()[0] &&
      !(mdbChave[0] && ajustes_mdblist_ligado())) return;
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
  nTrailer = fichaDur = nEstudio = 0;
  fichaStatus[0] = fichaPaises[0] = fichaCert[0] = fichaLanc[0] = 0;
  agStatus[0] = agDataProx[0] = agDataUlt[0] = agNomeEp[0] = 0;
  agTemp = agEp = 0;
  memset(vistos, 0, sizeof vistos);
  progressoPronto = proximoT = proximoE = 0;
  epsExibidos = epsVistos = 0;
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
const char *extras_agenda_status(void)       { return agStatus; }
const char *extras_agenda_data(void)         { return agDataProx; }
const char *extras_agenda_data_ultimo(void)  { return agDataUlt; }
const char *extras_agenda_nome_ep(void)      { return agNomeEp; }
int         extras_agenda_temporada(void)    { return agTemp; }
int         extras_agenda_episodio(void)     { return agEp; }
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

// Abre o trailer no app nativo da plataforma. O app nao tem reprodutor de
// YouTube embutido; em vez de prometer e nao cumprir, entrega o video ao
// componente que cada plataforma ja tem: o navegador do webOS (via luna-send),
// a aba do Tizen (window.open) ou o browser do desktop (open).
void extras_trailer_abrir(int i) {
  const char *yt = extras_trailer_yt(i);
  if (!yt[0]) return;
  char url[128];
  snprintf(url, sizeof url, "https://www.youtube.com/watch?v=%s", yt);
#if defined(__EMSCRIPTEN__)
  char js[200];
  snprintf(js, sizeof js, "window.open('%s','_blank')", url);
  emscripten_run_script(js);
#elif defined(__APPLE__)
  char cmd[160];
  snprintf(cmd, sizeof cmd, "open '%s'", url);
  system(cmd);
#else
  // webOS: luna-send lanca o navegador com a URL. O app roda como root e
  // /usr/bin/luna-send e acessivel dentro do jail.
  char cmd[512];
  snprintf(cmd, sizeof cmd,
    "luna-send -n 1 luna://com.webos.applicationManager/launch "
    "'{\"id\":\"com.webos.app.browser\",\"params\":{\"target\":\"%s\"}}'",
    url);
  system(cmd);
#endif
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

// PRODUTORAS/REDES — ver a declaracao de `estudio` la em cima.
int extras_n_estudios(void) { return nEstudio; }
const char *extras_estudio_nome(int i) {
  return (i >= 0 && i < nEstudio) ? estudio[i].nome : "";
}
const char *extras_estudio_logo(int i) {
  return (i >= 0 && i < nEstudio) ? estudio[i].logo : "";
}
long extras_estudio_tmdb(int i) {
  return (i >= 0 && i < nEstudio) ? estudio[i].tmdb : 0;
}
int extras_estudio_rede(int i) {
  return (i >= 0 && i < nEstudio) ? estudio[i].rede : 0;
}

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
// Devolve 1 so quando o historico chegou E a serie tem episodio exibido. Zero
// exibidos nao e "0% assistido": e serie que ainda nao estreou, e 0% ali seria
// uma afirmacao sobre nada.
int extras_progresso_serie(int *vistosEp, int *exibidos) {
  int pronto, v, e;
  pthread_mutex_lock(&trava);
  pronto = progressoPronto; v = epsVistos; e = epsExibidos;
  pthread_mutex_unlock(&trava);
  if (!pronto || e <= 0) return 0;
  if (v > e) v = e;      // o Trakt conta reprise; a porcentagem nao passa de 100
  if (vistosEp) *vistosEp = v;
  if (exibidos) *exibidos = e;
  return 1;
}

int extras_proximo_episodio(int *t, int *e) {
  pthread_mutex_lock(&trava);
  int ok = progressoPronto && proximoT > 0 && proximoE > 0;
  if (ok) { *t = proximoT; *e = proximoE; }
  pthread_mutex_unlock(&trava);
  return ok;
}
