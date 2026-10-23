#include "intro.h"
#include "arteurl.h"
#include "imdbnota.h"
#include "notasfontes.h"
#include "extras.h"
#include "posterprov.h"
#include "streams.h"
#include "marco.h"
#include "vistoep.h"
#include "perfisessao.h"
#include "contalib.h"
#include "trakt.h"
#include "rede.h"
#include "js.h"
#include "descoberta.h"
#include "ajustes.h"
#include "agenda.h"
#include "comentordem.h"
#include "idiomacod.h"
#include "linguas.h"
extern void cat_historico_definir_id(const char *imdb, const char *tipo, int visto);
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifdef NUVIO_TRAILER_TEST
// Costura de teste (tests/notas0.c): a rede sai por macros para que o fio de
// extras_pedir rode num binario de mesa, sem libcurl e sem conta.
char *rede_baixar_teste(const char *url, int segundos);
char *rede_baixar_com_teste(const char *url, int segundos, const char *const *cab);
char *rede_baixar_st_teste(const char *url, int segundos, const char *const *cab,
                           int *status);
char *rede_postar_teste(const char *url, int segundos, const char *const *cab,
                        const char *corpo);
#define rede_baixar      rede_baixar_teste
#define rede_baixar_com  rede_baixar_com_teste
#define rede_baixar_st   rede_baixar_st_teste
#define rede_postar      rede_postar_teste
static const char *testeAudioPref = "";
const char *ling_conta_audio_valor(void) { return testeAudioPref; }
void extras_teste_audio_pref(const char *v) { testeAudioPref = v ? v : ""; }
#endif

static int  notaTrakt, votosTrakt;
static int  notas[EX_NFONTES];
static char mdbChave[80];
static char mdbChavePacote[80];
static char dirArteEx[512];

// Nome do provedor na api do mdbList E nome do arquivo de marca em art/marcas.
// A ordem e a do enum, que e a do renderExternalRatingsRow do web.
static const char *FONTE[EX_NFONTES] = {
  "trakt", "imdb", "tmdb", "tomatoes", "audience", "metacritic", "letterboxd",
  // Nomes de provedor da api do MDBList. Estas quatro NAO tem arquivo de marca
  // (art/marcas): notasui.c desenha a marca delas, e o nome so serve a api.
  "metacriticuser", "myanimelist", "rogerebert", "score"
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
  return fonte != EX_IMDB && fonte != EX_LETTERBOXD && fonte != EX_METAUSER &&
         fonte != EX_MAL && fonte != EX_EBERT;
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
  snprintf(mdbChavePacote, sizeof mdbChavePacote, "%s", mdbChave);
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
typedef struct {
  char user[40], lingua[4], data[40];
  char *texto;
  int curtidas, nota, spoiler, respostas;
} ExComentario;
static ExComentario coment[EX_COMENT_MAX];

static void comentariosLimpar(ExComentario *v) {
  for (int i = 0; i < EX_COMENT_MAX; i++) { free(v[i].texto); memset(&v[i], 0, sizeof v[i]); }
}
static void comentarioPublicar(ExComentario *dst, ComentAchado *src) {
  snprintf(dst->user, sizeof dst->user, "%s", src->u);
  free(dst->texto); dst->texto = src->t; src->t = NULL;
  dst->curtidas = src->c; dst->nota = src->nota;
  snprintf(dst->lingua, sizeof dst->lingua, "%s", src->l);
  snprintf(dst->data, sizeof dst->data, "%s", src->data);
  dst->spoiler = src->spoiler; dst->respostas = src->respostas;
}
static int comentarioLer(ComentAchado *a, const char *p, const char *f) {
  char cru[8] = "";
  memset(a, 0, sizeof *a);
  // Decoded UTF-8 cannot exceed the JSON object's byte length.
  size_t cap = (size_t)(f - p) + 1;
  a->t = malloc(cap);
  if (!a->t) return 0;
  a->t[0] = 0;
  js_texto_linhas(p, f, "comment", a->t, cap);
  js_texto(p, f, "username", a->u, sizeof a->u);
  js_texto(p, f, "created_at", a->data, sizeof a->data);
  js_bruto(p, f, "spoiler", cru, sizeof cru);
  a->spoiler = !strcmp(cru, "true");
  a->respostas = (int)js_num(p, f, "replies", 0);
  a->c = (int)js_num(p, f, "likes", 0);
  a->nota = (int)js_num(p, f, "user_rating", 0);
  char lg[12] = ""; js_texto(p, f, "language", lg, sizeof lg);
  if (lg[0] && lg[1]) { a->l[0] = lg[0] | 32; a->l[1] = lg[1] | 32; }
  if (!a->t[0]) { free(a->t); a->t = NULL; return 0; }
  return 1;
}

// COMENTARIOS DO EPISODIO, o outro lado do seletor "Série | Episódio" que a
// referencia poe acima dos cartoes. Sao uma consulta DIFERENTE
// (/shows/<id>/seasons/<t>/episodes/<e>/comments/likes), nao um filtro da lista
// da serie: o Trakt guarda as duas separadas, e comentario de episodio nunca
// aparece na lista da serie.
//
// Vem sob demanda — so quando o dono escolhe "Episódio" —, porque o custo e uma
// viagem por episodio e a maioria das visitas nunca troca de aba.
static ExComentario *comentEp;
static int epCap, epPagina, epMais, epTmdb;
#define EP_PAGINA 8
static RedeGrupo *epGrupo;
typedef struct {
  char show[24], chaveTmdb[160];
  int t, e, pagina, tmdb, serie;
  long idTmdb;
  NvPerfilSessao sessao;
  RedeJob *job;
} EpPedido;
static ExEpNotas epNotas;
static int  nComentEp;
static int  epTempAtual, epNumAtual;    // de que episodio a lista acima e
static int  epFioVivo;
static char epShow[24];
static int  epPedTemp, epPedNum;
static NvPerfilSessao epComentSessao;
static int  nComent;
static struct { char titulo[120], ano[8], imdb[16], poster[NV_ARTE_URL_MAX]; } rel[EX_REL_MAX];
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
// QUANTOS VISTOS O MAPA (vistoep) TINHA quando epsVistos foi lido do Trakt. O
// hero mostra epsVistos + (mapa agora - epsBase): marcar/desmarcar pelo menu,
// player ou Agenda mexe no mapa na hora, e sem isto a linha "% assistido"
// ficava congelada ate reabrir a pagina.
static int epsBase;
// Revisao do mapa vistoep na leitura do Trakt: se mudou, o que o Trakt disse do
// "proximo episodio" e da matriz `vistos` ficou velho e o mapa manda.
static unsigned epsRev;
static int  nRel;
static struct { int numero; int nEps; struct { int ep, nota; } eps[EX_EP_MAX]; }
            temps[EX_TEMP_MAX];
static int  nTemps;
static char colNome[80];
// Arte propria da colecao (poster w342 e backdrop w780, URL absoluta) e a
// sinopse dela: o mini card da pagina e a tela de lista da saga.
static char colCapa[160], colFundo[160], colSinopse[600];
// Ficha tecnica e trailers: mesma viagem /movie/<id> da colecao.
static char fichaStatus[32], fichaPaises[160], fichaCert[12], fichaLanc[16], fichaDigital[16];
// IDIOMA ORIGINAL do titulo, do `original_language` do TMDB. Vem no MESMO
// corpo de /movie/<id> e /tv/<id> que este arquivo ja baixa — nenhuma viagem a
// mais. Vazio quando o TMDB nao responde, e vazio e um estado a tratar, nao um
// erro: quem le cai no comportamento de antes.
//
// O Cinemeta NAO tem esse campo. Conferido em 17/09 em tt0118799 (La vita e
// bella): ele devolve `country` = "Italy", que e o pais de PRODUCAO. Deduzir
// idioma de pais erra em toda co-producao e em todo pais multilingue, entao a
// deducao nao existe aqui.
static char fichaIdiomaOrig[8];
static int  fichaDur;
// AGENDA DA SERIE — proximo episodio e situacao, do MESMO corpo /tv/<id>.
// Ver agenda.h para o porque de nao ser o Trakt. Os campos sempre estiveram
// neste corpo; o parse antigo os jogava fora, como jogava fora status e
// runtime do filme antes da ficha tecnica existir.
static char agStatus[32], agDataProx[16], agDataUlt[16], agNomeEp[120];
static int  agTemp, agEp;
static struct { char yt[16], nome[80], mini[80]; int idiomaPreferido; } trailer[EX_TRAILER_MAX];
static int  nTrailer;
// O hero precisa de um trailer antes de a pessoa abrir a pagina de detalhe,
// mas carregar `extras_pedir` inteiro nesse ponto faria creditos, ficha,
// relacionados e imagens competirem com a arte. Mantemos uma fila separada
// que consulta apenas /videos. O fio e unico; pedidos novos substituem a
// geracao pendente e uma resposta velha e descartada.
typedef struct { char yt[16]; int idiomaPreferido; } HeroTrailer;
static HeroTrailer heroTrailer[EX_TRAILER_MAX];
static int nHeroTrailer;
static char heroTrailerPedido[24];
static int heroTrailerSerie;
static long heroTrailerTmdb;
// Trailer audio the cached ranking was made for: a change refetches.
static char heroTrailerAudio[16];
static unsigned long heroTrailerGer, heroTrailerExecutada;
static int heroTrailerVivo, heroTrailerFio;
static int heroTrailerTentativas;
static time_t heroTrailerTentativaEm;
static pthread_t heroTrailerThread;
static pthread_cond_t heroTrailerCv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t heroTrailerTrava = PTHREAD_MUTEX_INITIALIZER;
// Um vazio pode ser uma falha passageira (timeout, 5xx, Wi-Fi acordando).
// A mesma obra pode tentar de novo depois deste intervalo; resultado valido
// continua sendo idempotente e nao repete a viagem.
#define HERO_TRAILER_RETRY_S 30
static struct { char titulo[120], ano[8], poster[160], sinopse[420]; long tmdb;
                int nota; } col[EX_COL_MAX];
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

static char idPedido[24], idEmCurso[24], audioPedido[16];
static NvPerfilSessao sessaoPedido;
static pthread_key_t sessaoDoFio;
static pthread_once_t sessaoDoFioUma = PTHREAD_ONCE_INIT;
static void criarSessaoDoFio(void) { pthread_key_create(&sessaoDoFio, free); }
static void definirSessaoDoFio(NvPerfilSessao sessao) {
  NvPerfilSessao *p;
  pthread_once(&sessaoDoFioUma, criarSessaoDoFio);
  p = pthread_getspecific(sessaoDoFio);
  if (!p) { p = malloc(sizeof *p); if (p) pthread_setspecific(sessaoDoFio, p); }
  if (p) *p = sessao;
}
static int sessaoDoFioAtual(void) {
  NvPerfilSessao *p;
  pthread_once(&sessaoDoFioUma, criarSessaoDoFio);
  p = pthread_getspecific(sessaoDoFio);
  return nv_perfil_sessao_atual(p ? *p : 0);
}
static int  serieEmCurso, seriePedido, fioVivo;
// DOIS FIOS PERMANENTES, UMA GERACAO POR PEDIDO (19/09).
//
// Antes cada titulo aberto criava um fio novo que fazia de 7 a 9 pedidos EM
// SERIE — Trakt (progresso, notas, ficha, comentarios, temporadas,
// relacionados) e TMDB (find, ficha com imagens e videos, colecao) — e
// morria. Fio novo e handle novo da libcurl (rede.c guarda um por fio), ou
// seja, handshake TLS de novo com o Trakt e com o TMDB a cada titulo: MEDIDO
// na C9, ~30 s entre "meta basico na tela" e as notas aparecerem. Agora dois
// fios ficam vivos a sessao inteira, um com a parte do Trakt e outro com a do
// TMDB, rodando em PARALELO com as conexoes quentes; o ultimo a terminar uma
// geracao faz os relacionados (que dependem dos dois) e publica. Um pedido
// novo no meio so sobe a geracao: os fios abandonam o que estavam fazendo
// nos pontos de conferencia (pedidoAindaAtual) e recomecam.
static pthread_cond_t cvTrabalho = PTHREAD_COND_INITIALIZER;
static int gerPedida, gerFeita[2], fiosProntos;
static void *lacoParte(void *arg);
static long tmdbPedido;
static pthread_t fio;
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static void *buscar(void *arg);

static int pedidoAindaAtual(const char *id) {
  int atual;
  pthread_mutex_lock(&trava);
  atual = !strcmp(id, idPedido) && sessaoDoFioAtual();
  pthread_mutex_unlock(&trava);
  return atual;
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

// --- VIDEOS DO TMDB (trailers da pagina e do hero) --------------------------
//
// Id do YouTube: [A-Za-z0-9_-]{6,15}. Vai para URL de miniatura e para o
// window.open/luna-send; qualquer outra coisa fica de fora.
static int heroTrailerIdValido(const char *id) {
  size_t n, i;
  if (!id) return 0;
  n = strlen(id);
  if (n < 6 || n > 15) return 0;
  for (i = 0; i < n; i++)
    if (!((id[i] >= 'A' && id[i] <= 'Z') ||
          (id[i] >= 'a' && id[i] <= 'z') ||
          (id[i] >= '0' && id[i] <= '9') || id[i] == '_' || id[i] == '-')) return 0;
  return 1;
}

typedef struct { char yt[16], nome[80], pub[32]; int peso, ordem, serie, idiomaPreferido; } VideoTmdb;

// IDIOMAS DE VIDEO. Com `language=pt-BR` sozinho o TMDB devolve em /videos
// SO os videos marcados em portugues — e trailer de serie quase sempre esta
// em ingles ou sem idioma (iso_639_1 null). Era a lista vazia do #123.
// `include_video_language` alarga para o idioma da interface, ingles e os
// sem idioma: "pt,en,null"; em ingles, "en,null".
// AUDIO DO TRAILER (Ajustes > Trailers). "" = automatico (idioma da interface,
// then original language and English), "~" = original then English, or an
// codigo ISO da lista de idiomas. O teste injeta o valor, sem Ajustes.
#ifdef NUVIO_TRAILER_TEST
static const char *testeAudio = "";
void extras_teste_trailer_audio(const char *cod) { testeAudio = cod ? cod : ""; }
#define PREF_AUDIO() testeAudio
#else
#define PREF_AUDIO() ajustes_trailer_audio()
#endif

// Idioma-alvo do trailer, "xx" ou "xx-RR": o que o ranking quer e o que
// include_video_language pede. "pt" escolhido sem interface em portugues cai em
// pt-BR (a lista de idiomas nao distingue a variante).
static void alvoDeVideo(const char *idioma, const char *original, char *dst, size_t cap) {
  const char *p = PREF_AUDIO();
  const char *audioLang = idioma;
  if (!p || !p[0]) {
    audioLang = ling_conta_audio_valor();
    if (!audioLang || !audioLang[0]) audioLang = idioma;
    snprintf(dst, cap, "%s", audioLang && audioLang[0] && audioLang[1] ? audioLang : "en");
  }
  else if (!strcmp(p, "~")) snprintf(dst, cap, "%s", original && original[0] ? original : "en");
  else if (!strcmp(p, "pt") && idioma && !strncasecmp(idioma, "pt-", 3)) snprintf(dst, cap, "%s", idioma);
  else if (!strcmp(p, "pt")) snprintf(dst, cap, "pt-BR");
  else snprintf(dst, cap, "%s", p);
}

void extras_trailer_audio_iso(char *dst) {
  char alvo[16];
  dst[0] = 0;
  if (!strcmp(PREF_AUDIO(), "~")) return;
  alvoDeVideo(desc_tmdb_idioma(), "", alvo, sizeof alvo);
  if (alvo[0] && alvo[1]) { dst[0] = (char)(alvo[0] | 32); dst[1] = (char)(alvo[1] | 32); dst[2] = 0; }
}

// The chosen trailer audio may not be English, so callers wait for TMDB.
// Original is only known once TMDB answers.
static int idiomaPedido(void) {
  char alvo[16];
  if (!strcmp(PREF_AUDIO(), "~")) return 1;
  alvoDeVideo(desc_tmdb_idioma(), "", alvo, sizeof alvo);
  return strncasecmp(alvo, "en", 2) != 0;
}

static void idiomasDeVideo(const char *idioma, const char *original, char *dst, size_t cap) {
  char alvo[16], iso[3] = "";
  alvoDeVideo(idioma, original, alvo, sizeof alvo);
  if (((alvo[0] | 32) >= 'a' && (alvo[0] | 32) <= 'z') &&
      ((alvo[1] | 32) >= 'a' && (alvo[1] | 32) <= 'z')) {
    iso[0] = (char)(alvo[0] | 32); iso[1] = (char)(alvo[1] | 32);
  }
  if (!iso[0] || !strcmp(iso, "en")) snprintf(dst, cap, "en,null");
  else snprintf(dst, cap, "%s,en,null", iso);
  if ((!PREF_AUDIO()[0] || !strcmp(PREF_AUDIO(), "~")) && original && original[0] &&
      strcmp(original, iso) && strcmp(original, "en"))
    snprintf(dst, cap, "%s,%s,en,null", iso, original);
}

// URL da ficha /movie|tv/<id> com o append que os toggles pedem.
// `videos` vale para os dois tipos: a serie nao pedia (a fileira de trailers
// nao existia no layout dela) e ficava sem trailer do TMDB — na Samsung, onde
// o YouTube e a fonte que sobra, sem trailer nenhum (#123).
static void urlFicha(char *dst, size_t cap, int serie, long idT,
                     const char *chave, const char *idioma,
                     int datas, int trailers, int mais, const char *original) {
  char append[80] = "", langs[24] = "", extra[48] = "";
  if (!serie && datas) snprintf(append, sizeof append, "release_dates");
  if (trailers) {
    snprintf(append + strlen(append), sizeof append - strlen(append), "%svideos", append[0] ? "," : "");
    idiomasDeVideo(idioma, original, langs, sizeof langs);
    snprintf(extra, sizeof extra, "&include_video_language=%s", langs);
  }
  if (mais) snprintf(append + strlen(append), sizeof append - strlen(append), "%srecommendations", append[0] ? "," : "");
  // keywords junto da ficha: o sinal de cena pos-creditos vem no MESMO corpo
  // (TMDB 179430/179431), sem viagem nova.
  if (!serie) snprintf(append + strlen(append), sizeof append - strlen(append), "%skeywords", append[0] ? "," : "");
  snprintf(dst, cap, "%s/%s/%ld?api_key=%s&language=%s%s%s%s",
           "https://api.themoviedb.org/3", serie ? "tv" : "movie", idT,
           chave, idioma, append[0] ? "&append_to_response=" : "", append, extra);
}

// /videos solto: o do hero (temporada 0 = da obra) e o recuo da ficha para a
// temporada mais recente (/tv/<id>/season/<n>/videos).
static void urlVideos(char *dst, size_t cap, int serie, long idT, int temp,
                      const char *chave, const char *idioma, const char *original) {
  char langs[24], seg[24] = "";
  idiomasDeVideo(idioma, original, langs, sizeof langs);
  if (serie && temp > 0) snprintf(seg, sizeof seg, "/season/%d", temp);
  snprintf(dst, cap,
           "https://api.themoviedb.org/3/%s/%ld%s/videos?api_key=%s&language=%s"
           "&include_video_language=%s",
           serie ? "tv" : "movie", idT, seg, chave, idioma, langs);
}

// Promo de episodio nao e trailer da serie ("Episode 4 Preview",
// "Episódio 3 | Prévia"). Ficam de fora so na serie.
static int nomeDeEpisodio(const char *nm) {
  static const char *const M[] = { "episode", "episodio", "episódio", NULL };
  char low[80];
  size_t i;
  int k;
  for (i = 0; nm[i] && i + 1 < sizeof low; i++)
    low[i] = (nm[i] >= 'A' && nm[i] <= 'Z') ? (char)(nm[i] | 32) : nm[i];
  low[i] = 0;
  for (k = 0; M[k]; k++) if (strstr(low, M[k])) return 1;
  return 0;
}

// Language and region rank first. Within one language rank, series prefer
// the newest publication, then Trailer over Teaser; movies keep TMDB order.
// O tipo vai em cada elemento, e nao num global: o fio do hero e o da pagina
// ordenam ao mesmo tempo.
static int cmpVideo(const void *pa, const void *pb) {
  const VideoTmdb *a = pa, *b = pb;
  int c;
  if (a->peso / 4 != b->peso / 4) return b->peso / 4 - a->peso / 4;
  if (a->serie && (c = strcmp(b->pub, a->pub)) != 0) return c;
  if (a->peso != b->peso) return b->peso - a->peso;
  return a->ordem - b->ordem;
}

// videos.results[] -> so YouTube, so Trailer/Teaser. `v` e o primeiro
// elemento de results (js_array). Le TODOS antes de cortar em `max`: a
// ordenacao da serie precisa ver o mais novo, que o TMDB nao poe primeiro.
#define VIDEOS_LIDOS 40
static int parsearVideos(const char *v, int serie, const char *idioma, const char *original,
                         VideoTmdb *saida, int max) {
  VideoTmdb todos[VIDEOS_LIDOS];
  char alvo[16], iso[3] = "", reg[3] = "";
  int n = 0, i;
  alvoDeVideo(idioma, original, alvo, sizeof alvo);
  iso[0] = (char)(alvo[0] | 32); iso[1] = (char)(alvo[1] | 32);
  if (alvo[2] == '-' && alvo[3] && alvo[4]) {
    reg[0] = (char)(alvo[3] & ~32); reg[1] = (char)(alvo[4] & ~32);
  }
  while (v && n < VIDEOS_LIDOS) {
    const char *vf = js_fim(v);
    char site[24] = "", tipo[24] = "", key[16] = "", nm[80] = "", lg[8] = "", rg[8] = "";
    js_texto(v, vf, "site", site, sizeof site);
    js_texto(v, vf, "type", tipo, sizeof tipo);
    js_texto(v, vf, "key",  key,  sizeof key);
    js_texto(v, vf, "name", nm,   sizeof nm);
    js_texto(v, vf, "iso_639_1", lg, sizeof lg);
    js_texto(v, vf, "iso_3166_1", rg, sizeof rg);
    if (heroTrailerIdValido(key) && !strcmp(site, "YouTube") &&
        (!strcmp(tipo, "Trailer") || !strcmp(tipo, "Teaser")) &&
        !(serie && nomeDeEpisodio(nm))) {
      VideoTmdb *o = &todos[n];
      memset(o, 0, sizeof *o);
      snprintf(o->yt, sizeof o->yt, "%s", key);
      snprintf(o->nome, sizeof o->nome, "%s", nm[0] ? nm : "Trailer");
      js_texto(v, vf, "published_at", o->pub, sizeof o->pub);
      // peso: 8 idioma-alvo, +4 a regiao (pt-BR sobre pt-PT), +1 Trailer
      // (desempate da serie).
      o->peso = (!strcmp(lg, iso) ? 8 + (reg[0] && !strcmp(rg, reg) ? 4 : 0) : ((!PREF_AUDIO()[0] && original && original[0] && !strcmp(lg, original)) ? 4 :
                        !strcmp(lg, "en") ? 2 : 0)) +
                (serie && !strcmp(tipo, "Trailer") ? 1 : 0);
      o->idiomaPreferido = strcmp(iso, "en") && !strcmp(lg, iso);
      o->ordem = n;
      o->serie = serie;
      n++;
    }
    v = js_prox(vf);
  }
  qsort(todos, (size_t)n, sizeof todos[0], cmpVideo);
  if (n > max) n = max;
  for (i = 0; i < n; i++) saida[i] = todos[i];
  return n;
}

// Os trailers de dentro do corpo da ficha (append videos).
static int trailersDaFicha(const char *corpo, const char *fim, int serie,
                           const char *idioma, VideoTmdb *saida, int max) {
  const char *vid;
  char original[8] = "";
  if (!corpo) return 0;
  // `results` aparece mais de uma vez no corpo (release_dates, videos,
  // recommendations); procura a partir do bloco de videos.
  js_texto_raiz(corpo, "original_language", original, sizeof original);
  vid = strstr(corpo, "\"videos\"");
  if (!vid) return 0;
  return parsearVideos(js_array(vid, fim, "results"), serie, idioma, original, saida, max);
}

// Copia para a fileira da pagina (chamar com `trava`).
static void publicarTrailers(const VideoTmdb *vt, int nv) {
  int k;
  for (k = 0; k < nv && k < EX_TRAILER_MAX; k++) {
    trailer[k].idiomaPreferido = vt[k].idiomaPreferido;
    snprintf(trailer[k].yt,   sizeof trailer[k].yt,   "%s", vt[k].yt);
    snprintf(trailer[k].nome, sizeof trailer[k].nome, "%s", vt[k].nome);
    snprintf(trailer[k].mini, sizeof trailer[k].mini,
             "https://img.youtube.com/vi/%s/hqdefault.jpg", vt[k].yt);
  }
  nTrailer = k;
}

#ifdef NUVIO_TRAILER_TEST
void extras_teste_idiomas_video(const char *idioma, const char *original, char *dst, unsigned cap) {
  idiomasDeVideo(idioma, original, dst, cap);
}
void extras_teste_url_ficha(char *dst, unsigned cap, int serie, long id,
                            const char *idioma, int trailers) {
  urlFicha(dst, cap, serie, id, "K", idioma, 1, trailers, 1, "");
}
int extras_teste_trailers_ficha(const char *corpo, int serie, const char *idioma,
                                char yt[][16], int max) {
  VideoTmdb v[EX_TRAILER_MAX];
  int n, i;
  if (max > EX_TRAILER_MAX) max = EX_TRAILER_MAX;
  n = trailersDaFicha(corpo, corpo ? corpo + strlen(corpo) : NULL, serie, idioma, v, max);
  for (i = 0; i < n; i++) snprintf(yt[i], 16, "%s", v[i].yt);
  publicarTrailers(v, n);
  return n;
}
#endif

// `parte` 0 = Trakt, 1 = TMDB/MDBList, 2 = as duas em serie (sem fio). Os
// relacionados e a publicacao final ficam com quem terminar por ultimo
// (relacionadosEPublicar), porque dependem das duas partes.
static void relacionadosEPublicar(const char *id, int serie, int temTrakt);

static int fonteDe(const char *fonte) {
  int k;
  if (!fonte || !fonte[0]) return -1;
  for (k = 0; k < EX_NFONTES; k++)
    if (!strcmp(fonte, FONTE[k])) return k;
  return -1;
}

static void amostraCorpo(const char *corpo, char *dst, unsigned tam) {
  unsigned i, n = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!corpo) return;
  for (i = 0; corpo[i] && n + 1 < tam && n < 120; i++) {
    unsigned char c = (unsigned char)corpo[i];
    if (c == '\n' || c == '\r' || c == '\t') c = ' ';
    dst[n++] = (char)c;
  }
  dst[n] = 0;
}

static int publicarMdbNota(const char *id, int fonte, int cru, long tmdbId,
                           int serie) {
  if (fonte < 0 || fonte >= EX_NFONTES || cru <= 0) return 0;
  pthread_mutex_lock(&trava);
  if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
    notas[fonte] = cru;
    if (fonte == EX_IMDB) {
      imdbnota_publicar(id, cru);
      imdbnota_alias_tmdb(id, tmdbId, serie);
    }
  }
  pthread_mutex_unlock(&trava);
  return 1;
}

static int mdblistPostProvedor(const char *id, int fonte, long tmdbId, int serie) {
  const char *cabJ[2] = { "content-type: application/json", NULL };
  char u[300], corpoPost[80], *rp;
  double v;
  snprintf(corpoPost, sizeof corpoPost,
           "{\"ids\":[\"%s\"],\"provider\":\"imdb\"}", id);
  snprintf(u, sizeof u, "https://api.mdblist.com/rating/%s/%s?apikey=%s",
           serie ? "show" : "movie", FONTE[fonte], mdbChave);
  rp = rede_postar(u, 12, cabJ, corpoPost);
  if (!rp) return 0;
  v = js_num(rp, NULL, "rating", -1.0);
  free(rp);
  if (v < 0.0) return 0;
  return publicarMdbNota(id, fonte, nf_cru(fonte, v), tmdbId, serie);
}

static int aplicarMdblist(const char *id, const char *corpo, long tmdbId,
                          int serie, int *pedidas) {
  const char *p, *fim;
  int k, comNota = 0;
  if (!corpo) return 0;
  fim = corpo + strlen(corpo);
  p = js_array(corpo, fim, "ratings");
  if (p) {
    for (; p; p = js_prox(js_fim(p))) {
      char fonte[24];
      double v;
      int f;
      fim = js_fim(p);
      if (!js_texto(p, fim, "source", fonte, sizeof fonte)) continue;
      f = fonteDe(fonte);
      if (f < 0) continue;
      if (pedidas) (*pedidas)++;
      if (f >= EX_METAUSER && !ajustes_nota_titulo(f)) continue;
      v = js_num(p, fim, "score", -1.0);
      if (v < 0.0) v = js_num(p, fim, "value", -1.0);
      if (v < 0.0) v = js_num(p, fim, "rating", -1.0);
      if (v < 0.0) continue;
      if (publicarMdbNota(id, f, nf_cru(f, v), tmdbId, serie)) comNota++;
    }
    return comNota;
  }
  for (k = 0; k < EX_NFONTES; k++) {
    double v;
    if (k >= EX_METAUSER && !ajustes_nota_titulo(k)) continue;
    if (pedidas) (*pedidas)++;
    v = js_num(corpo, fim, FONTE[k], -1.0);
    if (v < 0.0) continue;
    if (publicarMdbNota(id, k, nf_cru(k, v), tmdbId, serie)) comNota++;
  }
  return comNota;
}

static void *buscar(void *arg) {
  const char *cab[4];
  char aut[200], chave[140], url[200], id[24];
  const char *tipo;
  char *corpo;
  int serie;
  long tmdbId;
  int parte = (int)(intptr_t)arg;
  unsigned long long historicoGeracao = cat_historico_geracao();
  unsigned long long credencialGeracao = trakt_credencial_geracao();
  NvPerfilSessao perfilSessao;

  pthread_mutex_lock(&trava);
  perfilSessao = sessaoPedido;
  snprintf(id, sizeof id, "%s", idEmCurso);
  serie = serieEmCurso;
  tmdbId = tmdbEmCurso;
  tipo = serie ? "shows" : "movies";
  pthread_mutex_unlock(&trava);
  definirSessaoDoFio(perfilSessao);
  if (!id[0] || !nv_perfil_sessao_atual(perfilSessao)) return NULL;

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
  if (parte != 1 && serie && temTrakt) {
    snprintf(url, sizeof url,
             "https://api.trakt.tv/shows/%s/progress/watched", id);
    printf("[vistoep] %s: consulta trakt progress/watched\n", id);
    corpo = rede_baixar_com(url, 20, cab);
    if (corpo) {
      unsigned char novo[EX_VIS_T][EX_VIS_E];
      VistoFonte fonte;
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
      memset(&fonte, 0, sizeof fonte);
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
            // O MAPA SEM TETO recebe o episodio inteiro, visto ou nao. A matriz
            // abaixo so guarda o "sim" e nao distingue "nao viu" de "nao sei";
            // vistoep distingue, e e nisso que as acoes de marcar em lote se
            // apoiam. A matriz continua porque extras_ep_visto tem chamador no
            // desenho, e trocar as duas coisas no mesmo passo seria demais.
            //
            // PELA PORTA DAS FONTES (vistoep_fonte), e nao por vistoep_definir:
            // o Trakt repete `completed:true` enquanto o historico dele tiver o
            // episodio, e era esta linha que re-marcava o que a pessoa tinha
            // acabado de desmarcar (Silo, tt14688458). last_watched_at vai junto
            // porque um visto MAIS NOVO que a desmarcacao ganha dela.
            if (en > 0) {
              int barrados = fonte.bloqueados;
              long long quandoMs = 0;
              if (visto) {
                char quando[40];
                if (js_texto(q, qf, "last_watched_at", quando, sizeof quando) && quando[0])
                  quandoMs = js_ms_iso(quando);
              }
              vistoep_fonte_se_sessao(id, t, en, visto, quandoMs, &fonte, perfilSessao);
              { char cru[16] = "ausente";
                js_bruto(q, qf, "completed", cru, sizeof cru);
                printf("[vistoep] %s T%dE%d: trakt completed=%s lido=%d last_ms=%lld mapa=%d\n",
                       id, t, en, cru, visto, quandoMs, vistoep_estado(id, t, en)); }
              // Barrado nao entra na matriz nem na conta de vistos do topo.
              if (fonte.bloqueados != barrados) visto = 0;
            }
            if (visto && t < EX_VIS_T && en > 0 && en < EX_VIS_E) novo[t][en] = 1;
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
             id, vistoep_total(id), vistoep_contar(id));
      // E A LINHA POR FONTE: quantos o TRAKT marcou nesta leitura, quantos ele
      // tentou marcar e a desmarcacao da pessoa barrou (e quais). A conta Nuvio
      // nao escreve no mapa com o Trakt ativo (sync.c), mas o que ela TEM para
      // este titulo sai na linha seguinte — e o que responde, lendo o log da
      // TV, "quem ainda acha que o episodio foi visto?".
      vistoep_fonte_log(id, "trakt", &fonte);
      { int discordam = 0, daConta = contalib_vistos_do_titulo(id, &discordam);
        printf("[vistoep] %s: conta Nuvio tem %d episodios (%d deles desmarcados aqui); "
               "nao aplicada ao mapa com Trakt ativo\n", id, daConta, discordam); }
      // O TOPO DO TRAKT conta o que a pessoa desmarcou: sem descontar, a serie
      // voltava a "100% assistida" e ao selo de vista com episodios desmarcados.
      vist -= fonte.bloqueados;
      if (vist < 0) vist = 0;
      // A SERIE INTEIRA (#212): o selo do cartaz e o olho do detalhe leem o
      // historico de titulo (cat_visto), e /sync/history nunca diz "serie
      // vista" — so episodios. Os contadores do topo dizem: tudo o que ja foi
      // ao ar foi visto. Com 0 exibidos nao se afirma nada.
      if (exib > 0) trakt_historico_aplicar(id, "series", vist >= exib, historicoGeracao, credencialGeracao);
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
      // O "PROXIMO" DO TRAKT e o seguinte ao ultimo que ELE tem como visto. Com
      // episodio barrado ele aponta para depois do que a pessoa desmarcou; o
      // primeiro nao visto do mapa e a resposta certa (a mesma regra de
      // extras_proximo_episodio quando o mapa muda depois da leitura).
      if (fonte.bloqueados > 0) {
        int mt = 0, me = 0;
        if (vistoep_primeiro_nao_visto(id, &mt, &me)) { pt = mt; pe = me; }
      }
      int valido = strstr(corpo, "\"seasons\"") != NULL;
      free(corpo);
      pthread_mutex_lock(&trava);
      if (!strcmp(id, idPedido) && sessaoDoFioAtual() && valido) {
        memcpy(vistos, novo, sizeof vistos);
        proximoT = pt; proximoE = pe; progressoPronto = 1;
        epsExibidos = exib; epsVistos = vist;
        epsBase = vistoep_contar(id);
        epsRev = vistoep_revisao();
      }
      pthread_mutex_unlock(&trava);
    } else {
      printf("[vistoep] %s: consulta trakt sem corpo; mapa preservado\n", id);
      fflush(stdout);
    }
  }

  // O usuario ja abriu outro titulo. Nao gastar varias viagens opcionais com
  // uma tela que nao existe mais; entrega o fio ao pedido pendente.
  if (!pedidoAindaAtual(id)) return NULL;

  // --- nota ---
  if (parte != 1 && temTrakt) {
  snprintf(url, sizeof url, "https://api.trakt.tv/%s/%s/ratings", tipo, id);
  corpo = rede_baixar_com(url, 12, cab);
  // Diagnostico: "notas=0/11" nao dizia se o Trakt nao respondeu ou respondeu
  // sem rating; uma linha por resultado separa rede de parse.
  if (!corpo) printf("[extras] %s: ratings trakt sem resposta\n", id);
  else if (js_num(corpo, NULL, "rating", 0.0) <= 0.0)
    printf("[extras] %s: ratings trakt respondeu sem nota\n", id);
  if (corpo) {
    int n = para100(js_num(corpo, NULL, "rating", 0.0));
    int v = (int)js_num(corpo, NULL, "votes", 0.0);
    free(corpo);
    pthread_mutex_lock(&trava);
    if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
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
  if (parte != 1 && serie && temTrakt && pedidoAindaAtual(id)) {
    snprintf(url,sizeof url,"https://api.trakt.tv/shows/%s?extended=full",id);
    corpo=rede_baixar_com(url,8,cab);
    if(corpo) {
      char estado[32]="";
      js_texto(corpo,NULL,"status",estado,sizeof estado);
      free(corpo);
      pthread_mutex_lock(&trava);
      if(!strcmp(id,idPedido) && sessaoDoFioAtual()) snprintf(fichaStatus,sizeof fichaStatus,"%s",estado);
      pthread_mutex_unlock(&trava);
    }
  }

  //
  // Um GET por titulo: /imdb/{movie|show}/{id} devolve todas as fontes em ratings[].
  // O antigo /?i= agora so responde a pagina de ajuda da api (http 200, sem notas).
  // O POST por provedor so lia "rating" e voltava 0/11 com o corpo novo.
  // mdblist_enabled corta a CONSULTA — as notas que o app tem por conta
  // propria (Trakt direto, IMDb do catalogo) nao dependem desta chave.
  if (parte != 0 && mdbChave[0] && ajustes_mdblist_ligado()) {
    char u[300], amostra[128];
    int status = 0, pedidas = 0, comNota = 0;
    snprintf(u, sizeof u, "https://api.mdblist.com/imdb/%s/%s?apikey=%s",
             serie ? "show" : "movie", id, mdbChave);
    corpo = rede_baixar_st(u, 12, NULL, &status);
    if (corpo) {
      comNota = aplicarMdblist(id, corpo, tmdbId, serie, &pedidas);
      if (!comNota) {
        amostraCorpo(corpo, amostra, sizeof amostra);
        printf("[extras] %s: mdblist http=%d sem nota corpo=%s\n",
               id, status, amostra);
      }
      free(corpo);
    } else {
      printf("[extras] %s: mdblist http=%d sem resposta\n", id, status);
    }
    if (!comNota) {
      int k;
      for (k = 0; k < EX_NFONTES; k++) {
        if (k >= EX_METAUSER && !ajustes_nota_titulo(k)) continue;
        pedidas++;
        if (mdblistPostProvedor(id, k, tmdbId, serie)) comNota++;
      }
    }
    printf("[extras] %s: mdblist pediu %d fontes, %d com nota\n",
           id, pedidas, comNota);
  } else if (parte != 0) {
    // Ate aqui um "notas=0/11" parecia igual com ou sem chave configurada.
    printf("[extras] %s: mdblist nao consultado (%s)\n", id,
           !mdbChave[0] ? "sem chave" : "ajuste desligado");
  }

  // --- comentarios, os mais curtidos primeiro ---
  // Public like the episode reviews: the comments row must not need a linked account.
  if (parte != 1 && (temTrakt || temChave)) {
  snprintf(url, sizeof url,
           "https://api.trakt.tv/%s/%s/comments/likes?limit=%d", tipo, id,
           EX_COMENT_MAX);
  corpo = rede_baixar_com(url, 12, temTrakt ? cab : cabPub);
  if (corpo) {
    ComentAchado achado[EX_COMENT_MAX];
    int n = 0;
    const char *p = strchr(corpo, '[');
    p = p ? p + 1 : NULL;
    // js_fim expects an object, not array whitespace or the closing bracket.
    while (p && *p && (unsigned char)*p <= ' ') p++;
    while (p && *p == '{' && n < EX_COMENT_MAX) {
      const char *f = js_fim(p);
      if (comentarioLer(&achado[n], p, f)) n++;
      p = js_prox(f);
    }
    free(corpo);
    // Os que estao no idioma da interface primeiro (comentordem.h). Sem pedido
    // novo: e so a ordem das oito linhas que ja chegaram.
    coment_ordenar(achado, n, idioma_iso(ajustes_idioma()));
    pthread_mutex_lock(&trava);
    if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
      int k;
      comentariosLimpar(coment);
      for (k = 0; k < n; k++) comentarioPublicar(&coment[k], &achado[k]);
      nComent = n;
    }
    pthread_mutex_unlock(&trava);
    for (int k = 0; k < n; k++) free(achado[k].t);
  }
  }

  // --- notas por episodio, so em serie ---
  // SEM CONTA TAMBEM. Ver a nota de cabPub la em cima: a chave do aplicativo
  // basta para esta chamada, entao a condicao e "ha alguma credencial", e nao
  // "ha token". Com token o cabecalho continua sendo o completo — nao ha
  // motivo para pedir anonimamente quem esta logado.
  if (parte != 1 && serie && (temTrakt || temChave)) {
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
      if (!strcmp(id, idPedido) && sessaoDoFioAtual()) nTemps = nt;
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
  if (parte != 0) {
    const char *chave = desc_chave_tmdb();
    long idCol = 0, idT = tmdbId;
    char nome[80] = "";
    int tNums[EX_TEMP_MAX], nTNum = 0;
    int ultTemp = 0;   // > 0: recuo do trailer para esta temporada
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
    if (idT > 0) imdbnota_alias_tmdb(id,idT,serie);
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
      urlFicha(url, sizeof url, serie, idT, chave, desc_tmdb_idioma(),
               ajustes_tmdb_datas(), ajustes_tmdb_trailers(), ajustes_tmdb_mais(), "");
      corpo = rede_baixar(url, 15);
      if (corpo && ajustes_tmdb_trailers() &&
          (!PREF_AUDIO()[0] || !strcmp(PREF_AUDIO(), "~"))) {
        char original[8] = "";
        js_texto_raiz(corpo, "original_language", original, sizeof original);
        if (original[0] && strcmp(original, "en")) {
          char *comOriginal;
          urlFicha(url, sizeof url, serie, idT, chave, desc_tmdb_idioma(),
                   ajustes_tmdb_datas(), 1, ajustes_tmdb_mais(), original);
          comOriginal = rede_baixar(url, 15);
          if (comOriginal) { free(corpo); corpo = comOriginal; }
        }
      }
      if (corpo) {
        // A ficha abaixo escreve varios campos globais. Segura a mesma trava
        // usada por extras_pedir para que uma troca de titulo nao limpe os
        // campos no meio do parse e receba, logo depois, metade da ficha velha.
        pthread_mutex_lock(&trava);
        if (strcmp(id, idPedido) || !sessaoDoFioAtual()) {
          pthread_mutex_unlock(&trava);
          free(corpo);
          return NULL;
        }
        const char *fimC = corpo + strlen(corpo);
        intro_registrar_tmdb(id,idT,corpo);
        // CENA POS-CREDITOS (TMDB 179430/179431), no mesmo corpo da ficha.
        // Serie traz keywords em /tv/<id>/keywords (viagem propria em
        // descoberta.c); aqui e so filme. Grava o bit direto no item: este fio
        // nao tem o CatItem `edit` do detalhe.
        if (!serie) cat_marcar_pos_creditos(id, desc_tmdb_pos_creditos(corpo));
        // Uma linha, no corpo que ja esta na mao. Ver a nota em
        // fichaIdiomaOrig.
        js_texto(corpo, fimC, "original_language",
                 fichaIdiomaOrig, sizeof fichaIdiomaOrig);
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
            // Filme sem saga traz `"belongs_to_collection":null`, e a busca
            // por '{' pulava para o objeto seguinte (o primeiro genero): o id
            // do genero virava um /collection/<id> inutil. Mesma guarda do
            // `null` da agenda acima.
            const char *o = strchr(b, '{');
            const char *nulo = strstr(b, "null");
            if (o && nulo && nulo < o) o = NULL;
            if (o) { const char *of = js_fim(o);
                     idCol = (long)js_num(o, of, "id", 0.0);
                     js_texto(o, of, "name", nome, sizeof nome); }
          }
        }

        // Movie alert eligibility uses dates already present in this response.
        if (!serie) {
          js_texto_raiz_em(corpo, fimC, "release_date", fichaLanc, sizeof fichaLanc);
          const char *rd = strstr(corpo, "\"release_dates\"");
          if (rd) rd = strchr(rd, '{');
          const char *res = rd ? js_array(rd, js_fim(rd), "results") : NULL;
          for (; res; res = js_prox(js_fim(res))) {
            const char *d = js_array(res, js_fim(res), "release_dates");
            for (; d; d = js_prox(js_fim(d))) if ((int)js_num(d, js_fim(d), "type", 0) == 4) {
              char dia[40] = "";
              js_texto(d, js_fim(d), "release_date", dia, sizeof dia);
              if (strlen(dia) >= 10 && (!fichaDigital[0] || strncmp(dia, fichaDigital, 10) < 0))
                snprintf(fichaDigital, sizeof fichaDigital, "%.10s", dia);
            }
          }
        }

        // --- ficha tecnica (so filme: a serie nao tem secao de Detalhes) ---
        if (!serie && ajustes_tmdb_ficha()) {
        js_texto(corpo, fimC, "status", fichaStatus, sizeof fichaStatus);
        js_texto(corpo, fimC, "release_date", fichaLanc, sizeof fichaLanc);
        fichaDur = (int)js_num(corpo, fimC, "runtime", 0.0);
        // StreamFit (F03): TMDB's movie runtime for THIS id (still under the
        // trava and the idPedido check above). The movie's stream target is
        // its imdb id, the same string. Zero/absent says nothing.
        if (fichaDur > 0 && fichaDur < 1440) stream_fit_duracao(id, fichaDur * 60.0, SF_DUR_METADATA);

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

        // Trailers: videos.results[] (trailersDaFicha).
        if (ajustes_tmdb_trailers())
        { VideoTmdb vt[EX_TRAILER_MAX];
          int nv = trailersDaFicha(corpo, fimC, serie, desc_tmdb_idioma(),
                                      vt, EX_TRAILER_MAX);
          publicarTrailers(vt, nv);
          // Serie sem video na obra: o recuo abaixo tenta a temporada mais
          // recente. `seasons` vem no mesmo corpo; temporada 0 e Especiais.
          if (serie && !nv) {
            const char *p2 = js_array(corpo, fimC, "seasons");
            while (p2) {
              const char *pf = js_fim(p2);
              int sn = (int)js_num(p2, pf, "season_number", -1.0);
              if (sn > ultTemp) ultTemp = sn;
              p2 = js_prox(pf);
            }
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
        // TRAILER DA TEMPORADA MAIS RECENTE, quando a obra nao tem nenhum
        // (#123). Um GET a mais so nesse caso; o trailer de temporada e da
        // serie, nao de episodio.
        if (ultTemp > 0 && pedidoAindaAtual(id)) {
          char *cs;
          urlVideos(url, sizeof url, 1, idT, ultTemp, chave, desc_tmdb_idioma(), fichaIdiomaOrig);
          cs = rede_baixar(url, 15);
          if (cs) {
            VideoTmdb vt[EX_TRAILER_MAX];
            int nv = parsearVideos(js_array(cs, NULL, "results"), 1,
                                      desc_tmdb_idioma(), fichaIdiomaOrig, vt, EX_TRAILER_MAX);
            pthread_mutex_lock(&trava);
            if (!strcmp(id, idPedido) && sessaoDoFioAtual() && !nTrailer) {
              publicarTrailers(vt, nv);
            }
            pthread_mutex_unlock(&trava);
            free(cs);
          }
        }
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
          if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
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
        struct ColAch { char t[120], a[8], d[16], po[160], sin[420]; long id; int nota; }
            ach[EX_COL_MAX];
        char capa[160] = "", fundo[160] = "", sinC[600] = "", cam[96];
        int nc = 0;
        const char *p = js_array(corpo, NULL, "parts");
        // Chaves da RAIZ: "poster_path" e "overview" tambem existem dentro de
        // cada parte, e js_texto pegaria a da primeira parte.
        if (js_texto_raiz(corpo, "poster_path", cam, sizeof cam) && cam[0] == '/')
          snprintf(capa, sizeof capa, "https://image.tmdb.org/t/p/w342%s", cam);
        if (js_texto_raiz(corpo, "backdrop_path", cam, sizeof cam) && cam[0] == '/')
          snprintf(fundo, sizeof fundo, "https://image.tmdb.org/t/p/w780%s", cam);
        js_texto_raiz(corpo, "overview", sinC, sizeof sinC);
        while (p && nc < EX_COL_MAX) {
          const char *f = js_fim(p);
          ach[nc].t[0] = ach[nc].a[0] = ach[nc].d[0] = 0;
          ach[nc].po[0] = ach[nc].sin[0] = 0;
          js_texto(p, f, "title", ach[nc].t, sizeof ach[nc].t);
          js_texto(p, f, "release_date", ach[nc].d, sizeof ach[nc].d);
          if (strlen(ach[nc].d) >= 4) { memcpy(ach[nc].a, ach[nc].d, 4); ach[nc].a[4] = 0; }
          ach[nc].id = (long)js_num(p, f, "id", 0.0);
          ach[nc].nota = (int)(js_num(p, f, "vote_average", 0.0) * 10.0 + 0.5);
          cam[0] = 0;
          js_texto(p, f, "poster_path", cam, sizeof cam);
          if (cam[0] == '/')
            snprintf(ach[nc].po, sizeof ach[nc].po, "https://image.tmdb.org/t/p/w342%s", cam);
          js_texto(p, f, "overview", ach[nc].sin, sizeof ach[nc].sin);
          if (ach[nc].t[0] && ach[nc].id > 0) nc++;
          p = js_prox(f);
        }
        free(corpo);
        // ORDEM DA SAGA = data de lancamento. O TMDB devolve `parts` na ordem
        // em que as partes foram cadastradas, que nem sempre e a cronologica;
        // parte sem data (anunciada) vai para o fim. Insercao estavel: sao 12.
        { int i2, j2;
          for (i2 = 1; i2 < nc; i2++) {
            struct ColAch v = ach[i2];
            for (j2 = i2; j2 > 0; j2--) {
              const char *da = ach[j2 - 1].d, *db = v.d;
              int depois = (!da[0] && db[0]) || (da[0] && db[0] && strcmp(da, db) > 0);
              if (!depois) break;
              ach[j2] = ach[j2 - 1];
            }
            ach[j2] = v;
          } }
        pthread_mutex_lock(&trava);
        if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
          int k;
          snprintf(colNome, sizeof colNome, "%s", nome);
          snprintf(colCapa, sizeof colCapa, "%s", capa);
          snprintf(colFundo, sizeof colFundo, "%s", fundo);
          snprintf(colSinopse, sizeof colSinopse, "%s", sinC);
          for (k = 0; k < nc; k++) {
            snprintf(col[k].titulo, sizeof col[k].titulo, "%s", ach[k].t);
            snprintf(col[k].ano, sizeof col[k].ano, "%s", ach[k].a);
            snprintf(col[k].poster, sizeof col[k].poster, "%s", ach[k].po);
            snprintf(col[k].sinopse, sizeof col[k].sinopse, "%s", ach[k].sin);
            col[k].tmdb = ach[k].id;
            col[k].nota = ach[k].nota;
          }
          nCol = nc;
        }
        pthread_mutex_unlock(&trava);
      }
    }
  }

  marco(parte == 0 ? "extras: parte trakt pronta" : parte == 1 ? "extras: parte tmdb pronta" : "extras: partes prontas");
  // As duas partes em serie (sem fio): os relacionados vem aqui mesmo.
  if (parte == 2) relacionadosEPublicar(id, serie, temTrakt);
  return NULL;
}

static void relacionadosEPublicar(const char *id, int serie, int temTrakt) {
  const char *cab[4];
  char aut[200], chave[140], url[200];
  const char *tipo = serie ? "shows" : "movies";
  char *corpo;
  if (!trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave)) temTrakt = 0;
  // Relacionados sao opcionais e podem custar mais uma viagem. Se o usuario
  // ja abriu outra obra, nem comeca.
  if (!pedidoAindaAtual(id)) return;

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
    if (!strcmp(id, idPedido) && sessaoDoFioAtual()) {
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
  { char lb[160];   // o poster vem do addon de meta: pode levar a config dele
    printf("[extras] colecao \"%s\" -> %d | rel[0] poster=%s\n", colNome, nCol,
           nRel ? rede_url_log(rel[0].poster, lb, sizeof lb) : "(sem)"); }
  fflush(stdout);
  fflush(stdout);
  marco("extras: publicados");
}

// O LACO DE CADA FIO PERMANENTE. Espera uma geracao nova, faz a sua parte,
// marca feita; quem fecha a geracao (as duas partes feitas e nenhuma mais
// nova pedida) faz os relacionados e solta `fioVivo`.
static void *lacoParte(void *arg) {
  int parte = (int)(intptr_t)arg;
  pthread_mutex_lock(&trava);
  for (;;) {
    int g, serie, temTrakt;
    char id[24];
    while (gerFeita[parte] == gerPedida) pthread_cond_wait(&cvTrabalho, &trava);
    g = gerPedida;
    snprintf(id, sizeof id, "%s", idEmCurso);
    serie = serieEmCurso;
    pthread_mutex_unlock(&trava);
    buscar((void *)(intptr_t)parte);
    pthread_mutex_lock(&trava);
    gerFeita[parte] = g;
    if (gerFeita[0] == g && gerFeita[1] == g && g == gerPedida) {
      pthread_mutex_unlock(&trava);
      { const char *cab[4]; char aut[200], chave[140];
        temTrakt = trakt_cabecalhos(cab, aut, sizeof aut, chave, sizeof chave); }
      relacionadosEPublicar(id, serie, temTrakt);
      pthread_mutex_lock(&trava);
      if (g == gerPedida) fioVivo = 0;
    }
  }
  return NULL;
}

// ZERA O QUE A TELA MOSTRA, com a trava. E o que cada pedido novo faz antes
// de buscar — e o que um pedido que NAO VAI BUSCAR tambem tem de fazer.
static void zerarPublicado(void) {
  comentariosLimpar(coment);
  notaTrakt = votosTrakt = nComent = nRel = nTemps = nCol = 0;
  colNome[0] = colCapa[0] = colFundo[0] = colSinopse[0] = 0;
  nTrailer = fichaDur = nEstudio = 0;
  fichaStatus[0] = fichaPaises[0] = fichaCert[0] = fichaLanc[0] = fichaDigital[0] = 0;
  fichaIdiomaOrig[0] = 0;
  agStatus[0] = agDataProx[0] = agDataUlt[0] = agNomeEp[0] = 0;
  agTemp = agEp = 0;
  memset(vistos, 0, sizeof vistos);
  progressoPronto = proximoT = proximoE = 0;
  epsExibidos = epsVistos = epsBase = 0;
  memset(notas, 0, sizeof notas);
}

// Titulo sem id que estas fontes conhecam: nada a buscar, e NADA A MOSTRAR.
//
// ISSUE #60: "trailers de um filme aparecem em outro; recomendacoes, estudios
// e ficha tambem". Um titulo sem imdb (item de catalogo proprio, id "kitsu:",
// "xperience:"...) fazia esta funcao voltar na primeira linha SEM zerar o que
// o titulo anterior deixou publicado — e o detalhe desenhava os trailers, o
// "Mais como este", as produtoras e a ficha do filme aberto antes. Com o fio
// ainda no ar, idPedido vazio faz cada bloco de publicacao (strcmp com
// idPedido) recusar o que chegar.
static void esquecerPedido(void) {
  pthread_mutex_lock(&trava);
  idPedido[0] = 0;
  zerarPublicado();
  pthread_mutex_unlock(&trava);
}

void extras_pedir(const char *imdb, int serie, long tmdbId) {
  char id[24];
  const char *dp;
  if (!imdb || imdb[0] != 't' || !strncmp(imdb, "tmdb:", 5)) { esquecerPedido(); return; }
  // Sem nenhuma fonte online nao ha o que buscar: o Trakt alimenta vistos,
  // nota, comentarios e related de sobra; a chave TMDB alimenta ficha,
  // produtoras e "Mais como este"; o MDBList alimenta as notas por fonte.
  // Antes o teste era so trakt_ativo — e uma conta sem Trakt perdia ate o
  // que nao depende dele.
  if (!trakt_ativo() && !desc_chave_tmdb()[0] &&
      !(mdbChave[0] && ajustes_mdblist_ligado())) { esquecerPedido(); return; }
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
  // Mesmo id com OUTRO tipo e outro pedido: um titulo de tipo incerto
  // ("anime") abre como filme e o /meta o resolve como serie — ai os extras
  // tem de ser refeitos por /tv e /shows (detail.c repede).
  if (!strcmp(idPedido, imdb) && seriePedido == serie &&
      !strcmp(audioPedido, PREF_AUDIO()) && nv_perfil_sessao_atual(sessaoPedido)) {
    printf("[vistoep] %s: extras reutilizados; fio=%d pronto=%d; sem nova consulta\n",
           imdb, fioVivo, progressoPronto);
    fflush(stdout);
    pthread_mutex_unlock(&trava); return; }
  snprintf(idPedido, sizeof idPedido, "%s", imdb);
  snprintf(audioPedido, sizeof audioPedido, "%s", PREF_AUDIO());
  sessaoPedido = nv_perfil_sessao_ler();
  seriePedido = serie;
  tmdbPedido = tmdbId;
  zerarPublicado();
  snprintf(idEmCurso, sizeof idEmCurso, "%s", imdb);
  serieEmCurso = serie;
  tmdbEmCurso = tmdbId;
  if (!fiosProntos) {
    pthread_t t;
    int k, ok = 1;
    for (k = 0; k < 2 && ok; k++)
      if (pthread_create(&t, NULL, lacoParte, (void *)(intptr_t)k) == 0) pthread_detach(t);
      else ok = 0;
    fiosProntos = ok ? 1 : -1;
  }
  fioVivo = 1;
  gerPedida++;
  pthread_cond_broadcast(&cvTrabalho);
  pthread_mutex_unlock(&trava);
  if (fiosProntos < 0) {
    // Sem fios: as duas partes em serie num fio descartavel, como antes.
    if (pthread_create(&fio, NULL, buscar, (void *)(intptr_t)2) == 0) pthread_detach(fio);
    else { pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava); }
  }
}

int extras_nota_trakt(void)  { return notaTrakt; }
int extras_votos_trakt(void) { return votosTrakt; }

int extras_n_comentarios(void) { return nComent; }
const char *extras_comentario_usuario(int i) {
  return (i >= 0 && i < nComent) ? coment[i].user : "";
}
// UI snapshots outlive worker publication/replacement of the heap text.
static const char *comentarioTexto(int ep, int i) {
  static char *copia[EX_COMENT_MAX], **copiaEp;
  static int capEp;
  if (i < 0 || (!ep && i >= EX_COMENT_MAX)) return "";
  pthread_mutex_lock(&trava);
  if (ep && i >= capEp) {
    int cap = i + 16;
    char **v = realloc(copiaEp, cap * sizeof *v);
    if (!v) { pthread_mutex_unlock(&trava); return ""; }
    memset(v + capEp, 0, (cap - capEp) * sizeof *v);
    copiaEp = v; capEp = cap;
  }
  const char *src = i < (ep ? nComentEp : nComent) ? (ep ? comentEp[i].texto : coment[i].texto) : NULL;
  if (!src) src = "";
  char **dst = ep ? &copiaEp[i] : &copia[i];
  if (!*dst || strcmp(*dst, src)) {
    char *nova = strdup(src);
    if (!nova) { pthread_mutex_unlock(&trava); return ""; }
    free(*dst); *dst = nova;
  }
  pthread_mutex_unlock(&trava);
  return *dst;
}
const char *extras_comentario_texto(int i) { return comentarioTexto(0, i); }
int extras_comentario_curtidas(int i) {
  return (i >= 0 && i < nComent) ? coment[i].curtidas : 0;
}

// --- comentarios do EPISODIO --------------------------------------------------

static int epAtual(const EpPedido *p) {
  return rede_job_estado(p->job) == REDE_OK && nv_perfil_sessao_atual(p->sessao) &&
    !strcmp(p->show, epShow) && p->t == epPedTemp && p->e == epPedNum;
}
static char *epBaixar(EpPedido *p, const char *url, const char *const *cab) {
  RedeResposta r = {0};
  RedePedido q = {.url=url, .cabecalhos=cab, .prazo_ms=12000, .seguir=1, .job=p->job};
  rede_pedir(&q, &r);
  char *body = NULL;
  if (r.erro == REDE_OK && r.status >= 200 && r.status < 300) { body = r.corpo; r.corpo = NULL; }
  rede_resposta_limpar(&r);
  return body;
}
static void *buscarEpComent(void *arg) {
  EpPedido *p = arg;
  const char *cab[4] = {0}; char chave[140], url[600];
  char *corpo = NULL;
  int n = 0, total = 0, raw = 0, source = p->tmdb, more = 0;
  ComentAchado achado[20] = {0};
  if (!source && trakt_cabecalhos_publicos(cab, chave, sizeof chave)) {
    snprintf(url, sizeof url,
      "https://api.trakt.tv/shows/%s/seasons/%d/episodes/%d/comments/likes?page=%d&limit=%d",
      p->show, p->t, p->e, p->pagina, EP_PAGINA);
    corpo = epBaixar(p, url, cab);
    const char *v = corpo ? strchr(corpo, '[') : NULL;
    v = v ? v + 1 : NULL;
    while (v && *v && (unsigned char)*v <= ' ') v++;
    while (v && *v == '{' && raw < EP_PAGINA) {
      const char *end = js_fim(v); raw++;
      if (comentarioLer(&achado[n], v, end)) n++;
      v = js_prox(end);
    }
    more = raw == EP_PAGINA;
    free(corpo); corpo = NULL;
  }
  if (!source && p->pagina == 1 && rede_job_estado(p->job) == REDE_OK) {
    snprintf(url, sizeof url, "https://api.trakt.tv/shows/%s/seasons/%d/episodes/%d/ratings", p->show, p->t, p->e);
    corpo = cab[0] ? epBaixar(p, url, cab) : NULL;
    ExEpNotas notas = {0};
    if (corpo) {
      const char *dist = strstr(corpo, "\"distribution\"");
      notas.nota = (int)(js_num(corpo, NULL, "rating", 0) * 10 + .5);
      notas.votos = (int)js_num(corpo, NULL, "votes", 0);
      for (int k=1; dist && k<=10; k++) {
        char key[4]; snprintf(key, sizeof key, "%d", k);
        notas.distribuicao[k] = (int)js_num(dist, NULL, key, 0);
      }
      free(corpo); corpo = NULL;
    }
    pthread_mutex_lock(&trava);
    if (epAtual(p)) epNotas = notas;
    pthread_mutex_unlock(&trava);
  }
  // TMDB has title-level reviews only. Never present these as episode reviews.
  if ((source || (p->pagina == 1 && n == 0)) && p->chaveTmdb[0] && rede_job_estado(p->job) == REDE_OK) {
    source = 1;
    if (p->idTmdb <= 0) {
      snprintf(url, sizeof url, "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", p->show, p->chaveTmdb);
      corpo = epBaixar(p, url, NULL);
      const char *v = corpo ? js_array(corpo, NULL, p->serie ? "tv_results" : "movie_results") : NULL;
      if (v) p->idTmdb = (long)js_num(v, js_fim(v), "id", 0);
      free(corpo); corpo = NULL;
    }
    if (p->idTmdb > 0) {
      snprintf(url, sizeof url, "https://api.themoviedb.org/3/%s/%ld/reviews?api_key=%s&page=%d",
        p->serie ? "tv" : "movie", p->idTmdb, p->chaveTmdb, p->pagina);
      corpo = epBaixar(p, url, NULL);
      total = corpo ? (int)js_num(corpo, NULL, "total_pages", 0) : 0;
      const char *v = corpo ? js_array(corpo, NULL, "results") : NULL;
      raw = 0;
      while (v && *v == '{') {
        const char *end = js_fim(v);
        // TMDB pages contain at most 20 reviews; publish through the same feed.
        ComentAchado a = {0};
        a.t = malloc((size_t)(end-v)+1);
        if (a.t) {
          js_texto_linhas(v,end,"content",a.t,(size_t)(end-v)+1);
          js_texto(v,end,"author",a.u,sizeof a.u);
          js_texto(v,end,"created_at",a.data,sizeof a.data);
          a.nota = (int)js_num(v,end,"rating",0);
          if (n < 20 && a.t[0]) achado[n++] = a;
          else free(a.t);
        }
        raw++; v = js_prox(end);
      }
      free(corpo); corpo = NULL;
      more = p->pagina < total;
    } else more = 0;
  }
  coment_ordenar(achado,n,idioma_iso(ajustes_idioma()));
  pthread_mutex_lock(&trava);
  if (epAtual(p)) {

    for (int k=0; k<n; k++) {
      int duplicate = 0;
      for (int j=0; j<nComentEp; j++)
        if (!strcmp(comentEp[j].user,achado[k].u) && !strcmp(comentEp[j].data,achado[k].data) &&
            !strcmp(comentEp[j].texto,achado[k].t)) { duplicate=1; break; }
      if (duplicate) continue;
      if (nComentEp == epCap) {
        int cap = epCap + 16;
        ExComentario *v = realloc(comentEp,cap*sizeof *v);
        if (!v) { more=0; break; }
        memset(v+epCap,0,(cap-epCap)*sizeof *v); comentEp=v; epCap=cap;
      }
      comentarioPublicar(&comentEp[nComentEp++], &achado[k]);
    }
    epTempAtual=p->t; epNumAtual=p->e; epPagina=p->pagina;
    epMais=more; epTmdb=source; epFioVivo=0;
  }
  pthread_mutex_unlock(&trava);
  for (int k=0;k<n;k++) free(achado[k].t);
  rede_job_soltar(p->job); free(p);
  return NULL;
}
static void epIniciar(void) {
  EpPedido *p=calloc(1,sizeof *p); pthread_t f;
  if (!p) { epFioVivo=0; epMais=0; return; }
  snprintf(p->show,sizeof p->show,"%s",epShow);
  snprintf(p->chaveTmdb,sizeof p->chaveTmdb,"%s",desc_chave_tmdb());
  p->t=epPedTemp; p->e=epPedNum; p->pagina=epPagina+1;
  p->tmdb=epTmdb; p->sessao=epComentSessao; p->serie=1;
  int idx=cat_indice_por_imdb(epShow); const CatItem *ci=cat_item(idx);
  if (ci) { p->idTmdb=ci->tmdb; p->serie=strcmp(ci->tipo,"movie")!=0; }
  if (!epGrupo) epGrupo=rede_grupo_criar();
  p->job=rede_job_criar(epGrupo);
  epFioVivo=1;
  if (!p->job || pthread_create(&f,NULL,buscarEpComent,p)!=0) {
    if (p->job) rede_job_soltar(p->job);
    free(p); epFioVivo=0; epMais=0;
  } else pthread_detach(f);
}
void extras_comentarios_ep_cancelar(void) {
  pthread_mutex_lock(&trava);
  if (epGrupo) rede_grupo_avancar(epGrupo);
  epFioVivo=0;
  pthread_mutex_unlock(&trava);
}
void extras_pedir_comentarios_ep(const char *imdbSerie,int t,int e) {
  if (!imdbSerie || !*imdbSerie || t<0 || e<=0) return;
  char show[24]; snprintf(show,sizeof show,"%s",imdbSerie);
  char *dp=strchr(show,':'); if (dp) *dp=0;
  pthread_mutex_lock(&trava);
  if (!strcmp(show,epShow) && t==epPedTemp && e==epPedNum &&
      nv_perfil_sessao_atual(epComentSessao) && (epFioVivo || (t==epTempAtual && e==epNumAtual))) {
    pthread_mutex_unlock(&trava); return;
  }
  if (epGrupo) rede_grupo_avancar(epGrupo);
  for (int i=0;i<nComentEp;i++) { free(comentEp[i].texto); memset(&comentEp[i],0,sizeof comentEp[i]); }
  snprintf(epShow,sizeof epShow,"%s",show);
  epPedTemp=t; epPedNum=e; epComentSessao=nv_perfil_sessao_ler();
  epPagina=epTmdb=nComentEp=epTempAtual=epNumAtual=0; epMais=1;
  memset(&epNotas,0,sizeof epNotas);
  epIniciar();
  pthread_mutex_unlock(&trava);
}
void extras_comentarios_ep_mais(void) {
  pthread_mutex_lock(&trava);
  if (epMais && !epFioVivo && nv_perfil_sessao_atual(epComentSessao)) epIniciar();
  pthread_mutex_unlock(&trava);
}
int extras_comentarios_ep_tem_mais(void) { pthread_mutex_lock(&trava); int n=epMais; pthread_mutex_unlock(&trava); return n; }
int extras_comentarios_ep_tmdb(void) { pthread_mutex_lock(&trava); int n=epTmdb; pthread_mutex_unlock(&trava); return n; }

int extras_comentarios_ep_do(const char *imdb, int t, int e) {
  char show[24]; int ok;
  snprintf(show, sizeof show, "%s", imdb ? imdb : "");
  { char *dp = strchr(show, ':'); if (dp) *dp = 0; }
  pthread_mutex_lock(&trava);
  ok = !strcmp(show, epShow) && t == epPedTemp && e == epPedNum && nv_perfil_sessao_atual(epComentSessao);
  pthread_mutex_unlock(&trava);
  return ok;
}
void extras_notas_ep(ExEpNotas *saida) {
  pthread_mutex_lock(&trava); *saida = epNotas; pthread_mutex_unlock(&trava);
}
// Copy short fields while locked: append may realloc the feed on another thread.
static const char *epCampo(int i,int field) {
  static char copy[3][40];
  pthread_mutex_lock(&trava);
  const char *s="";
  if (i>=0 && i<nComentEp) s=field==0?comentEp[i].user:field==1?comentEp[i].data:comentEp[i].lingua;
  snprintf(copy[field],sizeof copy[field],"%s",s);
  pthread_mutex_unlock(&trava); return copy[field];
}
static int epNumero(int i,int field) {
  pthread_mutex_lock(&trava); int n=0;
  if (i>=0 && i<nComentEp) n=field==0?comentEp[i].nota:field==1?comentEp[i].curtidas:field==2?comentEp[i].respostas:comentEp[i].spoiler;
  pthread_mutex_unlock(&trava); return n;
}
const char *extras_comentario_ep_data(int i) { return epCampo(i,1); }
int extras_comentario_ep_respostas(int i) { return epNumero(i,2); }
int extras_comentario_ep_spoiler(int i) { return epNumero(i,3); }
int extras_n_comentarios_ep(void) { pthread_mutex_lock(&trava); int n=nComentEp; pthread_mutex_unlock(&trava); return n; }
int extras_comentarios_ep_carregando(void) { pthread_mutex_lock(&trava); int n=epFioVivo; pthread_mutex_unlock(&trava); return n; }

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
  return epCampo(i,0);
}
const char *extras_comentario_ep_texto(int i) { return comentarioTexto(1, i); }
int extras_comentario_ep_curtidas(int i) {
  return epNumero(i,1);
}
int extras_comentario_ep_nota(int i) {
  return epNumero(i,0);
}

// Idioma do comentario quando NAO e o da interface ("" quando e, ou quando o
// Trakt nao disse): e o que a etiqueta "EN" do cartao pergunta.
const char *extras_comentario_lingua(int i) {
  return (i >= 0 && i < nComent && !coment_mesmo_idioma(coment[i].lingua,
          idioma_iso(ajustes_idioma()))) ? coment[i].lingua : "";
}
const char *extras_comentario_ep_lingua(int i) {
  const char *s=epCampo(i,2);
  return !coment_mesmo_idioma(s,idioma_iso(ajustes_idioma())) ? s : "";
}
const char *extras_comentario_data(int ep, int i) {
  return ep ? extras_comentario_ep_data(i) : i>=0 && i<nComent ? coment[i].data : "";
}
int extras_comentario_spoiler(int ep, int i) {
  return ep ? extras_comentario_ep_spoiler(i) : i>=0 && i<nComent ? coment[i].spoiler : 0;
}
int extras_comentario_respostas(int ep, int i) {
  return ep ? extras_comentario_ep_respostas(i) : i>=0 && i<nComent ? coment[i].respostas : 0;
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
const char *extras_ficha_digital(void)       { return fichaDigital; }

int extras_n_trailers(void) { return nTrailer; }
int extras_trailer_idioma_preferido(int i) {
  if (!nTrailer && extras_carregando()) return idiomaPedido() ? -1 : 0;
  return i >= 0 && i < nTrailer && trailer[i].idiomaPreferido;
}
const char *extras_trailer_yt(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].yt : "";
}
const char *extras_trailer_nome(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].nome : "";
}
const char *extras_trailer_miniatura(int i) {
  return (i >= 0 && i < nTrailer) ? trailer[i].mini : "";
}

static void normalizarHeroId(const char *imdb, char *dst, size_t cap) {
  const char *p;
  size_t n;
  if (!dst || !cap) return;
  dst[0] = 0;
  if (!imdb || imdb[0] != 't' || !strncmp(imdb, "tmdb:", 5)) return;
  p = strchr(imdb, ':');
  n = p ? (size_t)(p - imdb) : strlen(imdb);
  if (n >= cap) n = cap - 1;
  memcpy(dst, imdb, n);
  dst[n] = 0;
}

// A resposta e a mesma selecao (e a mesma ordem) usada pelos extras da
// pagina: parsearVideos. O hero so guarda o id.
static int parsearHeroVideos(const char *corpo, int serie, const char *idioma,
                             HeroTrailer *saida) {
  VideoTmdb v[EX_TRAILER_MAX];
  int n, i;
  if (!corpo || !saida) return 0;
  if (strstr(corpo, "\"videos\""))
    n = trailersDaFicha(corpo, NULL, serie, idioma, v, EX_TRAILER_MAX);
  else n = parsearVideos(js_array(corpo, NULL, "results"), serie, idioma, "", v, EX_TRAILER_MAX);
  for (i = 0; i < n; i++) {
    snprintf(saida[i].yt, sizeof saida[i].yt, "%s", v[i].yt);
    saida[i].idiomaPreferido = v[i].idiomaPreferido;
  }
  return n;
}

#ifdef NUVIO_TRAILER_TEST
// Ponto de teste pequeno para o parser usado pelo worker real. O alvo normal
// nao exporta este auxiliar; o harness compila com NUVIO_TRAILER_TEST e assim
// exercita o mesmo filtro de site/tipo/chave sem simular o JSON em JavaScript.
void extras_teste_publicar_hero(const char *id, const char *corpo, const char *idioma) {
  snprintf(heroTrailerPedido, sizeof heroTrailerPedido, "%s", id);
  snprintf(heroTrailerAudio, sizeof heroTrailerAudio, "%s", PREF_AUDIO());
  nHeroTrailer = parsearHeroVideos(corpo, 0, idioma, heroTrailer);
  heroTrailerVivo = 0;
  heroTrailerExecutada = heroTrailerGer;
}
int extras_hero_trailer_parse(const char *corpo, char *dst, unsigned cap) {
  HeroTrailer encontrados[EX_TRAILER_MAX];
  int n = parsearHeroVideos(corpo, 0, "", encontrados);
  if (dst && cap) {
    dst[0] = 0;
    if (n > 0) snprintf(dst, cap, "%s", encontrados[0].yt);
  }
  return n;
}
int extras_teste_hero_serie(const char *corpo, const char *idioma, char *dst, unsigned cap) {
  HeroTrailer encontrados[EX_TRAILER_MAX];
  int n = parsearHeroVideos(corpo, 1, idioma, encontrados);
  dst[0] = 0;
  if (n > 0) snprintf(dst, cap, "%s", encontrados[0].yt);
  return n;
}
void extras_teste_url_hero(char *dst, unsigned cap, int serie, long id, const char *idioma) {
  urlVideos(dst, cap, serie, id, 0, "K", idioma, "");
}
#endif

static void *lacoHeroTrailer(void *ignorado) {
  (void)ignorado;
  for (;;) {
    char id[24], chave[140], url[640];
    int serie;
    long tmdb;
    unsigned long ger;
    HeroTrailer encontrados[EX_TRAILER_MAX];
    int n = 0;
    pthread_mutex_lock(&heroTrailerTrava);
    while (heroTrailerExecutada == heroTrailerGer)
      pthread_cond_wait(&heroTrailerCv, &heroTrailerTrava);
    ger = heroTrailerGer;
    snprintf(id, sizeof id, "%s", heroTrailerPedido);
    serie = heroTrailerSerie;
    tmdb = heroTrailerTmdb;
    heroTrailerExecutada = ger;
    pthread_mutex_unlock(&heroTrailerTrava);

    snprintf(chave, sizeof chave, "%s", desc_chave_tmdb());
    if (chave[0]) {
      long idT = tmdb;
      if (idT <= 0) {
        snprintf(url, sizeof url,
                 "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
                 id, chave);
        { char *corpo = rede_baixar(url, 4);
          if (corpo) {
            const char *v = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
            if (v) idT = (long)js_num(v, js_fim(v), "id", 0.0);
            free(corpo);
          } }
      }
      if (idT > 0) {
        urlFicha(url, sizeof url, serie, idT, chave, desc_tmdb_idioma(), 0, 1, 0, "");
        { char *corpo = rede_baixar(url, 4);
          if (corpo) {
            char original[8] = "";
            js_texto_raiz(corpo, "original_language", original, sizeof original);
            if ((!PREF_AUDIO()[0] || !strcmp(PREF_AUDIO(), "~")) &&
                original[0] && strcmp(original, "en")) {
              char *comOriginal;
              urlFicha(url, sizeof url, serie, idT, chave, desc_tmdb_idioma(), 0, 1, 0, original);
              comOriginal = rede_baixar(url, 4);
              if (comOriginal) { free(corpo); corpo = comOriginal; }
            }
            n = parsearHeroVideos(corpo, serie, desc_tmdb_idioma(), encontrados);
            free(corpo);
          } }
      }
    }

    pthread_mutex_lock(&heroTrailerTrava);
    // A pessoa pode ter mudado o destaque durante a rede_baixar. Nesse caso
    // nao publicar nem mesmo um resultado vazio: o pedido novo continua sendo
    // processado no proximo ciclo do mesmo fio.
    if (ger == heroTrailerGer && !strcmp(id, heroTrailerPedido)) {
      int i;
      nHeroTrailer = n;
      for (i = 0; i < n; i++) heroTrailer[i] = encontrados[i];
      heroTrailerVivo = 0;
    }
    pthread_mutex_unlock(&heroTrailerTrava);
  }
  return NULL;
}

void extras_hero_trailer_pedir(const char *imdb, int serie, long tmdbId) {
  char id[24];
  time_t agora = time(NULL);
  normalizarHeroId(imdb, id, sizeof id);
  if (!id[0] || !desc_chave_tmdb()[0] || !ajustes_tmdb_trailers()) return;
  pthread_mutex_lock(&heroTrailerTrava);
  if (!strcmp(heroTrailerPedido, id) && heroTrailerSerie == serie &&
      heroTrailerTmdb == tmdbId && !strcmp(heroTrailerAudio, PREF_AUDIO())) {
    if (heroTrailerVivo || nHeroTrailer > 0 ||
        agora - heroTrailerTentativaEm < HERO_TRAILER_RETRY_S) {
      pthread_mutex_unlock(&heroTrailerTrava);
      return;
    }
  } else {
    heroTrailerTentativas = 0;
  }
  snprintf(heroTrailerPedido, sizeof heroTrailerPedido, "%s", id);
  snprintf(heroTrailerAudio, sizeof heroTrailerAudio, "%s", PREF_AUDIO());
  heroTrailerSerie = serie;
  heroTrailerTmdb = tmdbId;
  nHeroTrailer = 0;
  heroTrailerVivo = 1;
  heroTrailerTentativas++;
  heroTrailerTentativaEm = agora;
  heroTrailerGer++;
  if (!heroTrailerFio) {
    if (pthread_create(&heroTrailerThread, NULL, lacoHeroTrailer, NULL) == 0) {
      pthread_detach(heroTrailerThread);
      heroTrailerFio = 1;
    } else {
      heroTrailerVivo = 0;
      pthread_mutex_unlock(&heroTrailerTrava);
      return;
    }
  }
  pthread_cond_signal(&heroTrailerCv);
  pthread_mutex_unlock(&heroTrailerTrava);
}

int extras_hero_trailer_obter(const char *imdb, char *dst, unsigned cap) {
  char id[24];
  int ok = 0;
  if (dst && cap) dst[0] = 0;
  normalizarHeroId(imdb, id, sizeof id);
  if (!id[0] || !dst || cap < 2) return 0;
  pthread_mutex_lock(&heroTrailerTrava);
  if (!strcmp(heroTrailerPedido, id) && !heroTrailerVivo &&
      heroTrailerExecutada == heroTrailerGer && nHeroTrailer > 0 &&
      !strcmp(heroTrailerAudio, PREF_AUDIO())) {
    snprintf(dst, cap, "%s", heroTrailer[0].yt);
    ok = 1;
  }
  pthread_mutex_unlock(&heroTrailerTrava);
  return ok;
}

int extras_hero_trailer_idioma_preferido(const char *imdb) {
  char id[24];
  int preferido;
  normalizarHeroId(imdb, id, sizeof id);
  pthread_mutex_lock(&heroTrailerTrava);
  if (id[0] && !strcmp(heroTrailerPedido, id) && !heroTrailerVivo &&
      heroTrailerExecutada == heroTrailerGer && !strcmp(heroTrailerAudio, PREF_AUDIO()))
    preferido = nHeroTrailer > 0 && heroTrailer[0].idiomaPreferido;
  else preferido = idiomaPedido() ? -1 : 0;
  pthread_mutex_unlock(&heroTrailerTrava);
  return preferido;
}

// Abre o trailer no app nativo da plataforma. O app nao tem reprodutor de
// YouTube embutido; em vez de prometer e nao cumprir, entrega o video ao
// componente que cada plataforma ja tem: o navegador do webOS (via luna-send)
// ou o browser do desktop (open). Na Samsung, nada (ver abaixo).
void extras_trailer_abrir(int i) {
  const char *yt = extras_trailer_yt(i);
  if (!yt[0]) return;
  char url[128];
  snprintf(url, sizeof url, "https://www.youtube.com/watch?v=%s", yt);
#ifdef NV_VIDAA
  printf("[extras] youtube externo indisponivel no VIDAA\n");
  fflush(stdout);
#elif defined(__EMSCRIPTEN__) || defined(NV_TPK) || defined(NV_ANDROID)
  // SAMSUNG (e Android, por ora; TODO Intent ACTION_VIEW): NAO abre mais nada (#136). No wgt o window.open trocava a
  // propria pagina do app pelo youtube.com/watch — o video tocava, mas o
  // Voltar nao tinha mais o Nuvio para onde voltar. O trailer da Samsung
  // toca dentro do app (detail.c, SEC_TRAILERS); chegar aqui e defeito de
  // quem chamou, e fica no registro.
  printf("[trailer] extras_trailer_abrir na Samsung ignorado (%s)\n", url);
  fflush(stdout);
#elif defined(NV_LINUX_DESKTOP)
  SDL_OpenURL(url);
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
const char *extras_colecao_capa(void) { return colCapa; }
const char *extras_colecao_fundo(void) { return colFundo; }
const char *extras_colecao_sinopse(void) { return colSinopse; }
const char *extras_colecao_poster(int i) {
  return (i >= 0 && i < nCol) ? posterprov_card(NULL, col[i].tmdb, "movie", col[i].poster) : "";
}
const char *extras_colecao_sinopse_parte(int i) {
  return (i >= 0 && i < nCol) ? col[i].sinopse : "";
}
int extras_colecao_nota(int i) { return (i >= 0 && i < nCol) ? col[i].nota : 0; }

// PRODUTORAS/REDES — ver a declaracao de `estudio` la em cima.
int extras_n_estudios(void) { return nEstudio; }
// O idioma original do titulo que esta na ficha, ou "" quando nao se sabe.
const char *extras_idioma_original(void) { return fichaIdiomaOrig; }

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
  char id[sizeof idPedido];
  int visto, m;
  pthread_mutex_lock(&trava);
  snprintf(id, sizeof id, "%s", idPedido);
  visto = (temporada >= 0 && temporada < EX_VIS_T && episodio >= 0 && episodio < EX_VIS_E)
              ? vistos[temporada][episodio] : 0;
  pthread_mutex_unlock(&trava);
  // O MAPA LOCAL MANDA: marcar/desmarcar muda o vistoep na hora, a matriz so
  // muda no proximo fetch. -1 = o mapa nao sabe, vale a matriz.
  m = id[0] ? vistoep_estado(id, temporada, episodio) : -1;
  return m >= 0 ? m : visto;
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
  int pronto, v, e, base;
  char idAtual[sizeof idPedido];
  pthread_mutex_lock(&trava);
  pronto = progressoPronto; v = epsVistos; e = epsExibidos;
  base = epsBase; snprintf(idAtual, sizeof idAtual, "%s", idPedido);
  pthread_mutex_unlock(&trava);
  if (!pronto || e <= 0) return 0;
  if (idAtual[0] && vistoep_conhecido(idAtual))
    v = vistoep_ajustar_vistos(v, base, vistoep_contar(idAtual), e);
  if (v > e) v = e;      // o Trakt conta reprise; a porcentagem nao passa de 100
  if (vistosEp) *vistosEp = v;
  if (exibidos) *exibidos = e;
  return 1;
}

int extras_proximo_episodio(int *t, int *e) {
  pthread_mutex_lock(&trava);
  int ok = progressoPronto && proximoT >= 0 && proximoE > 0;
  unsigned rev = epsRev;
  char id[sizeof idPedido];
  snprintf(id, sizeof id, "%s", idPedido);
  if (ok) { *t = proximoT; *e = proximoE; }
  pthread_mutex_unlock(&trava);
  // Mapa mexido depois do fetch: o proximo e o primeiro nao visto DO MAPA.
  if (ok && id[0] && rev != vistoep_revisao() && vistoep_conhecido(id)) {
    int pt, pe;
    if (!vistoep_primeiro_nao_visto(id, &pt, &pe)) return 0;
    *t = pt; *e = pe;
  }
  return ok;
}

#ifdef NV_SHOT_HOOKS
// Capturas: relacionados fixos (titulo, ano, imdb e poster por item).
void extras_shot_relacionados(const char *const *titulo, const char *const *ano,
                              const char *const *poster, int n) {
  extras_shot_relacionados_ids(titulo, ano, poster, NULL, n);
}

void extras_shot_relacionados_ids(const char *const *titulo, const char *const *ano,
                                  const char *const *poster, const char *const *imdb,
                                  int n) {
  int i;
  nRel = n < EX_REL_MAX ? n : EX_REL_MAX;
  for (i = 0; i < nRel; i++) {
    snprintf(rel[i].titulo, sizeof rel[i].titulo, "%s", titulo[i]);
    snprintf(rel[i].ano, sizeof rel[i].ano, "%s", ano[i]);
    if (imdb && imdb[i] && imdb[i][0])
      snprintf(rel[i].imdb, sizeof rel[i].imdb, "%s", imdb[i]);
    else
      snprintf(rel[i].imdb, sizeof rel[i].imdb, "tt%07d", 100 + i);
    snprintf(rel[i].poster, sizeof rel[i].poster, "%s", poster[i]);
  }
}
#endif

// --- RESUMO PARA O MENU DO CARTAZ ---------------------------------------------
//
// A extensao de informacoes do menu (ctxinfo.c) quer as notas de um titulo QUE
// NAO ESTA ABERTO. extras_pedir nao serve: ele e o estado da pagina de titulo,
// e o menu abre tambem POR CIMA dela (Recomendacoes, filmografia, saga) — pedir
// ali trocaria as notas, os comentarios e a ficha da pagina que esta embaixo.
//
// Por isso um caminho a parte, como o do trailer do hero: um fio, uma vaga de
// pedido (o ultimo vence — quem passeia segurando OK em varios cartazes so
// precisa do atual) e um cache pequeno por IMDb, para reabrir o menu no mesmo
// titulo nao custar viagem. O que se pede e o minimo:
//   1. Trakt /<tipo>/<id>?extended=full com a chave publica do pacote: nota do
//      Trakt, duracao, classificacao, sinopse (ingles) e situacao — UMA viagem;
//   2. TMDB /find so quando o titulo nao trouxe sinopse (a do TMDB vem no
//      idioma da interface) — e a nota do TMDB vem junto;
//   3. MDBList so para as fontes que a linha do titulo mostra
//      (ajustes_nota_titulo), um POST por fonte, como extras.c faz.
// Cada passo publica ao terminar; quem desenha mostra o que ja chegou.
#define EXR_CACHE 16
typedef struct { char id[24]; int serie; ExResumo r; } ExrVaga;
static ExrVaga exrCache[EXR_CACHE];
static int exrProx;
static char exrPedido[24];
static int exrSerie, exrSinopse, exrFio;
static unsigned long exrGer, exrFeita;
static NvPerfilSessao exrSessao;
static pthread_mutex_t exrTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t exrCv = PTHREAD_COND_INITIALIZER;

static void exrId(const char *imdb, char *dst, size_t cap) {
  const char *dp;
  size_t n;
  dst[0] = 0;
  if (!imdb || imdb[0] != 't' || imdb[1] != 't') return;
  dp = strchr(imdb, ':');
  n = dp ? (size_t)(dp - imdb) : strlen(imdb);
  if (n >= cap) n = cap - 1;
  memcpy(dst, imdb, n); dst[n] = 0;
}

// Vaga do id (com a trava). `criar` recicla a mais antiga.
static ExrVaga *exrVaga(const char *id, int criar) {
  int i;
  for (i = 0; i < EXR_CACHE; i++)
    if (exrCache[i].id[0] && !strcmp(exrCache[i].id, id)) return &exrCache[i];
  if (!criar) return NULL;
  i = exrProx; exrProx = (exrProx + 1) % EXR_CACHE;
  memset(&exrCache[i], 0, sizeof exrCache[i]);
  snprintf(exrCache[i].id, sizeof exrCache[i].id, "%s", id);
  return &exrCache[i];
}

static void *lacoResumo(void *ignorado) {
  (void)ignorado;
  for (;;) {
    char id[24], url[400], chaveT[140], chaveTmdb[140];
    const char *cabPub[3];
    char *corpo;
    int serie, querSinopse, cortado = 0;
    NvPerfilSessao sessao;
    pthread_mutex_lock(&exrTrava);
    while (exrFeita == exrGer) pthread_cond_wait(&exrCv, &exrTrava);
    exrFeita = exrGer;
    snprintf(id, sizeof id, "%s", exrPedido);
    serie = exrSerie; querSinopse = exrSinopse;
    sessao = exrSessao;
    pthread_mutex_unlock(&exrTrava);
    if (!id[0] || !nv_perfil_sessao_atual(sessao)) continue;

    // 1. Trakt, uma viagem.
    if (trakt_cabecalhos_publicos(cabPub, chaveT, sizeof chaveT)) {
      snprintf(url, sizeof url, "https://api.trakt.tv/%s/%s?extended=full",
               serie ? "shows" : "movies", id);
      corpo = rede_baixar_com(url, 8, cabPub);
      if (corpo) {
        ExResumo t;
        memset(&t, 0, sizeof t);
        t.cru[EX_TRAKT] = para100(js_num(corpo, NULL, "rating", 0.0)) * 10;
        t.duracao = (int)js_num(corpo, NULL, "runtime", 0.0);
        js_texto_raiz(corpo, "certification", t.cert, sizeof t.cert);
        js_texto_raiz(corpo, "overview", t.sinopseEn, sizeof t.sinopseEn);
        js_texto_raiz(corpo, "status", t.status, sizeof t.status);
        numaLinha(t.sinopseEn);
        free(corpo);
        pthread_mutex_lock(&exrTrava);
        if (nv_perfil_sessao_atual(sessao)) { ExrVaga *v = exrVaga(id, 1);
          if (!v->r.cru[EX_TRAKT]) v->r.cru[EX_TRAKT] = t.cru[EX_TRAKT];
          v->r.duracao = t.duracao;
          snprintf(v->r.cert, sizeof v->r.cert, "%s", t.cert);
          snprintf(v->r.sinopseEn, sizeof v->r.sinopseEn, "%s", t.sinopseEn);
          snprintf(v->r.status, sizeof v->r.status, "%s", t.status); }
        pthread_mutex_unlock(&exrTrava);
      }
    }
    if (!nv_perfil_sessao_atual(sessao)) continue;

    // 2. TMDB, so pela sinopse no idioma da interface.
    snprintf(chaveTmdb, sizeof chaveTmdb, "%s", desc_chave_tmdb());
    if (querSinopse && chaveTmdb[0]) {
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id&language=%s",
               id, chaveTmdb, desc_tmdb_idioma());
      corpo = rede_baixar(url, 6);
      if (corpo) {
        const char *v = js_array(corpo, NULL, serie ? "tv_results" : "movie_results");
        if (v) {
          char sin[sizeof ((ExResumo *)0)->sinopse] = "";
          double nota = js_num(v, js_fim(v), "vote_average", 0.0);
          js_texto(v, js_fim(v), "overview", sin, sizeof sin);
          numaLinha(sin);
          pthread_mutex_lock(&exrTrava);
          if (nv_perfil_sessao_atual(sessao)) { ExrVaga *g = exrVaga(id, 1);
            if (sin[0]) snprintf(g->r.sinopse, sizeof g->r.sinopse, "%s", sin);
            if (!g->r.cru[EX_TMDB] && nota > 0.0) g->r.cru[EX_TMDB] = emDecimos(nota * 10.0); }
          pthread_mutex_unlock(&exrTrava);
        }
        free(corpo);
      }
    }
    if (!nv_perfil_sessao_atual(sessao)) continue;

    // 3. MDBList, so o que a linha do titulo mostra.
    if (mdbChave[0] && ajustes_mdblist_ligado()) {
      int status = 0, comNota = 0;
      snprintf(url, sizeof url, "https://api.mdblist.com/imdb/%s/%s?apikey=%s",
               serie ? "show" : "movie", id, mdbChave);
      corpo = rede_baixar_st(url, 8, NULL, &status);
      if (corpo) {
        const char *p, *fim = corpo + strlen(corpo);
        p = js_array(corpo, fim, "ratings");
        if (p) {
          for (; p; p = js_prox(js_fim(p))) {
            char fonte[24];
            double v;
            int f, cru;
            fim = js_fim(p);
            if (!js_texto(p, fim, "source", fonte, sizeof fonte)) continue;
            f = fonteDe(fonte);
            if (f < 0 || !ajustes_nota_titulo(f) || !ajustes_mdblist_fonte(f)) continue;
            pthread_mutex_lock(&exrTrava);
            { ExrVaga *g = exrVaga(id, 0);
              if (exrGer != exrFeita || !nv_perfil_sessao_atual(sessao)) { cortado = 1; pthread_mutex_unlock(&exrTrava); break; }
              if (g && g->r.cru[f] > 0 && f == EX_TRAKT) { pthread_mutex_unlock(&exrTrava); continue; } }
            pthread_mutex_unlock(&exrTrava);
            if (cortado) break;
            v = js_num(p, fim, "score", -1.0);
            if (v < 0.0) v = js_num(p, fim, "value", -1.0);
            if (v < 0.0) v = js_num(p, fim, "rating", -1.0);
            if (v < 0.0) continue;
            cru = nf_cru(f, v);
            pthread_mutex_lock(&exrTrava);
            if (nv_perfil_sessao_atual(sessao)) { ExrVaga *g = exrVaga(id, 1); g->r.cru[f] = cru; }
            pthread_mutex_unlock(&exrTrava);
            if (f == EX_IMDB && nv_perfil_sessao_travar(sessao)) {
              imdbnota_publicar(id, cru);
              nv_perfil_sessao_destravar();
            }
            comNota++;
          }
        }
        if (!comNota && !cortado) {
          char amostra[128];
          amostraCorpo(corpo, amostra, sizeof amostra);
          printf("[extras] %s: mdblist http=%d sem nota corpo=%s\n",
                 id, status, amostra);
        }
        free(corpo);
      } else if (!cortado) {
        printf("[extras] %s: mdblist http=%d sem resposta\n", id, status);
      }
    }

    pthread_mutex_lock(&exrTrava);
    if (nv_perfil_sessao_atual(sessao)) {
      ExrVaga *g = exrVaga(id, 1); g->r.pronto = !cortado; g->r.carregando = 0;
    }
    pthread_mutex_unlock(&exrTrava);
  }
  return NULL;
}

void extras_resumo_pedir(const char *imdb, int serie, int semSinopse) {
  char id[24];
  exrId(imdb, id, sizeof id);
  if (!id[0]) return;
  pthread_mutex_lock(&exrTrava);
  { ExrVaga *v = exrVaga(id, 0);
    if (v && (v->r.pronto || v->r.carregando)) { pthread_mutex_unlock(&exrTrava); return; }
    v = exrVaga(id, 1);
    v->serie = serie;
    v->r.carregando = 1; }
  snprintf(exrPedido, sizeof exrPedido, "%s", id);
  exrSerie = serie; exrSinopse = semSinopse;
  exrSessao = nv_perfil_sessao_ler();
  exrGer++;
  if (!exrFio) {
    pthread_t t;
    if (pthread_create(&t, NULL, lacoResumo, NULL) == 0) { pthread_detach(t); exrFio = 1; }
    else { ExrVaga *v = exrVaga(id, 0); if (v) v->r.carregando = 0; }
  }
  // Pedido anterior ainda sem resposta deixa de estar "carregando": o fio so
  // atende o ultimo, e o anterior volta a ser pedido quando o menu reabrir.
  { int i;
    for (i = 0; i < EXR_CACHE; i++)
      if (exrCache[i].id[0] && strcmp(exrCache[i].id, id) && exrCache[i].r.carregando &&
          !exrCache[i].r.pronto)
        exrCache[i].r.carregando = 0; }
  pthread_cond_signal(&exrCv);
  pthread_mutex_unlock(&exrTrava);
}

int extras_resumo_obter(const char *imdb, ExResumo *saida) {
  char id[24];
  int ok = 0, k;
  exrId(imdb, id, sizeof id);
  if (!id[0] || !saida) return 0;
  pthread_mutex_lock(&exrTrava);
  { ExrVaga *v = exrVaga(id, 0);
    if (v) { *saida = v->r; ok = 1; } }
  pthread_mutex_unlock(&exrTrava);
  // O que a pessoa escondeu em Ajustes > MDBList some na LEITURA, como em
  // extras_nota.
  if (ok) for (k = 0; k < EX_NFONTES; k++) if (!ajustes_mdblist_fonte(k)) saida->cru[k] = 0;
  return ok;
}

void extras_resumo_definir(const char *imdb, const ExResumo *r) {
  char id[24];
  exrId(imdb, id, sizeof id);
  if (!id[0] || !r) return;
  pthread_mutex_lock(&exrTrava);
  { ExrVaga *v = exrVaga(id, 1); v->r = *r; }
  pthread_mutex_unlock(&exrTrava);
}

// Costura de teste: o estado que o fetch de /progress/watched deixa. Sem rede.
// INTENCIONAL na build de release: nenhum codigo de src/ a chama (so tests/progresso_serie.c); inalcancavel na TV.
void extras_teste_progresso(const char *id, int nVistos, int exibidos, int pt, int pe) {
  pthread_mutex_lock(&trava);
  snprintf(idPedido, sizeof idPedido, "%s", id);
  proximoT = pt; proximoE = pe; progressoPronto = 1;
  epsExibidos = exibidos; epsVistos = nVistos;
  epsBase = vistoep_contar(id); epsRev = vistoep_revisao();
  memset(vistos, 0, sizeof vistos);
  { int t, e; for (t = 1; t <= 3; t++) for (e = 1; e < 8 && t < 3; e++) vistos[t][e] = 1; }
  pthread_mutex_unlock(&trava);
}

void extras_abrir_sessao(void) {
  snprintf(mdbChave, sizeof mdbChave, "%s", mdbChavePacote);
  pthread_mutex_lock(&trava);
  idPedido[0] = idEmCurso[0] = 0;
  sessaoPedido = nv_perfil_sessao_ler();
  gerPedida++;
  gerFeita[0] = gerFeita[1] = gerPedida;
  fioVivo = 0;
  zerarPublicado();
  epShow[0] = 0;
  memset(&epNotas, 0, sizeof epNotas);
  if (epGrupo) rede_grupo_avancar(epGrupo);
  for (int i=0;i<nComentEp;i++) { free(comentEp[i].texto); memset(&comentEp[i],0,sizeof comentEp[i]); }
  epFioVivo=epMais=epPagina=epTmdb=0;
  nComentEp = epTempAtual = epNumAtual = epPedTemp = epPedNum = 0;
  pthread_mutex_unlock(&trava);
  pthread_mutex_lock(&heroTrailerTrava);
  heroTrailerGer++;
  heroTrailerPedido[0] = 0;
  nHeroTrailer = heroTrailerTentativas = 0;
  heroTrailerTentativaEm = 0;
  pthread_mutex_unlock(&heroTrailerTrava);
  pthread_mutex_lock(&exrTrava);
  memset(exrCache, 0, sizeof exrCache);
  exrProx = 0;
  exrPedido[0] = 0;
  exrSessao = nv_perfil_sessao_ler();
  exrGer++;
  exrFeita = exrGer;
  pthread_mutex_unlock(&exrTrava);
}
