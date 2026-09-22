#include "agenda.h"
#include "dados.h"
#include "perfis.h"
#include "catalogo.h"
#include "progresso.h"
#include "descoberta.h"
#include "idioma.h"
#include "rede.h"
#include "js.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>

// ---------------------------------------------------------------------------
// ESTADO
//
// Dois arquivos por perfil, pelo mesmo motivo de fileirasui-p<N>.txt: numa TV
// de sala o calendario de uma pessoa nao e o da outra, e o lembrete muito
// menos. Perfil <= 0 (ninguem escolheu ainda) usa o nome sem sufixo.
//
//   agenda-p<N>.txt     CACHE do que o TMDB disse. Re-obtivel: perder so custa
//                       um pedido. Por isso vai por dados_gravar_leve.
//   lembretes-p<N>.txt  INTENCAO DO DONO. Nao e re-obtivel de lugar nenhum —
//                       vai por dados_gravar, a atomica, como a sessao.
// ---------------------------------------------------------------------------
#define AG_CACHE_MAX 200

typedef struct {
  char imdb[24];
  char titulo[160];
  char poster[512];
  int  situacao;
  int  temporada, episodio;
  char nomeEp[120];
  char dataProx[12];
  char dataUlt[12];
  long long visto;        // time(NULL) do registro; 0 = nunca
  // Os cinco do mesmo corpo /tv/<id>. Ver a nota em agenda.h: entraram porque a
  // tela Agenda precisava de mais que titulo e dia, e vinham de graca.
  char sinopse[400];
  char tipoEp[24];
  char rede[64];
  int  duracao;
  int  temporadas;
} AgReg;

static AgReg cache[AG_CACHE_MAX];
static int   nCache;
static int   cacheSujo;

// Lembrete: o titulo, a data que estava valendo quando o dono pediu, e se o
// aviso daquela data ja foi dado. A data viaja junto de proposito — quando o
// TMDB adia o episodio, o lembrete continua valendo e o "avisado" e zerado
// porque a data mudou. Sem ela, um adiamento avisaria no dia velho e nunca
// mais.
typedef struct { char imdb[24]; char data[12]; int avisado; } AgLembrete;
#define AG_LEMB_MAX 120
static AgLembrete lembretes[AG_LEMB_MAX];
static int nLembretes;

static AgItem lista[AG_MAX];
static int    nLista;

static int perfilCarregado = -1;
static char hojeForcado[12];
static pthread_mutex_t trava = PTHREAD_MUTEX_INITIALIZER;
static int fioVivo;

// ---------------------------------------------------------------------------
// datas
// ---------------------------------------------------------------------------
static int ehIso(const char *s) {
  int i;
  if (!s || strlen(s) < 10) return 0;
  for (i = 0; i < 10; i++) {
    if (i == 4 || i == 7) { if (s[i] != '-') return 0; }
    else if (s[i] < '0' || s[i] > '9') return 0;
  }
  return 1;
}

// Dias desde 1970-01-01 pela formula civil (Howard Hinnant). NAO usa mktime de
// proposito: mktime interpreta a data no fuso do APARELHO, e numa TV o fuso
// costuma estar errado — "estreia hoje" viraria "estreia amanha" por causa de
// um -03:00 mal configurado. Aqui a conta e puramente de calendario, que e o
// que uma data de estreia e.
static long diasCivis(int a, int m, int d) {
  long y = a;
  long era, yoe, doy, doe;
  y -= m <= 2;
  era = (y >= 0 ? y : y - 399) / 400;
  yoe = y - era * 400;                                    // [0, 399]
  doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;   // [0, 365]
  doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0, 146096]
  return era * 146097 + doe - 719468;
}

static long diasDeIso(const char *s) {
  int a, m, d;
  if (!ehIso(s)) return 0;
  a = (s[0]-'0')*1000 + (s[1]-'0')*100 + (s[2]-'0')*10 + (s[3]-'0');
  m = (s[5]-'0')*10 + (s[6]-'0');
  d = (s[8]-'0')*10 + (s[9]-'0');
  if (m < 1 || m > 12 || d < 1 || d > 31) return 0;
  return diasCivis(a, m, d);
}

void agenda_definir_hoje(const char *iso) {
  if (iso && ehIso(iso)) snprintf(hojeForcado, sizeof hojeForcado, "%.10s", iso);
  else hojeForcado[0] = 0;
}

const char *agenda_hoje(void) {
  static char hoje[12];
  static time_t ultimo;
  time_t agora;
  struct tm tmv;
  if (hojeForcado[0]) return hojeForcado;
  agora = time(NULL);
  // UMA CONVERSAO POR SEGUNDO. A tela Agenda chama agenda_dias/agenda_quando
  // dezenas de vezes por quadro e cada uma passava por localtime_r, que na
  // libc da TV consulta o fuso a cada chamada; o dia nao muda dentro do
  // mesmo segundo.
  if (agora == ultimo && hoje[0]) return hoje;
  ultimo = agora;
  // localtime e o certo AQUI e so aqui: "hoje" para quem esta olhando a TV e o
  // dia do relogio dela. A comparacao entre duas datas ja e civil (diasDeIso).
  if (!localtime_r(&agora, &tmv)) { hoje[0] = 0; return hoje; }
  snprintf(hoje, sizeof hoje, "%04d-%02d-%02d",
           tmv.tm_year + 1900, tmv.tm_mon + 1, tmv.tm_mday);
  return hoje;
}

int agenda_dias(const char *iso) {
  long a, b;
  if (!ehIso(iso)) return AG_SEM_DATA;
  a = diasDeIso(iso);
  b = diasDeIso(agenda_hoje());
  if (!a || !b) return AG_SEM_DATA;
  return (int)(a - b);
}

void agenda_quando(const char *iso, char *dst, size_t tam) {
  int d;
  if (!dst || !tam) return;
  dst[0] = 0;
  d = agenda_dias(iso);
  if (d == AG_SEM_DATA) return;
  // Perto = palavra; longe = a data por extenso. O corte em 7 dias e o mesmo
  // do painel de episodios do web: dentro da semana o dia da semana nao ajuda
  // numa TV (ninguem confere o calendario), a CONTAGEM ajuda.
  if (d == 0)      snprintf(dst, tam, "%s", i18n("hoje"));
  else if (d == 1) snprintf(dst, tam, "%s", i18n("amanhã"));
  else if (d > 1 && d <= 7) snprintf(dst, tam, i18n("em %d dias"), d);
  else if (d == -1) snprintf(dst, tam, "%s", i18n("ontem"));
  // Data por extenso — a MESMA funcao do resto do app (descoberta.c), que ja
  // traduz o mes e a ordem das palavras. Nao ha segundo formatador de data
  // aqui de proposito: dois divergem, e divergem em silencio.
  else desc_data_extenso(iso, dst, tam);
}

// ---------------------------------------------------------------------------
// as pecas do calendario
//
// A linha do tempo desenha o dia em numeral grande, o mes como cabecalho de
// secao e o dia da semana em apoio. Sao tres recortes da MESMA data, e ficam
// aqui pelo motivo que agenda_quando ja registra: um segundo formatador de data
// diverge, e diverge em silencio.
// ---------------------------------------------------------------------------
int agenda_dia(const char *iso) {
  if (!ehIso(iso)) return 0;
  return (iso[8] - '0') * 10 + (iso[9] - '0');
}
int agenda_mes(const char *iso) {
  if (!ehIso(iso)) return 0;
  return (iso[5] - '0') * 10 + (iso[6] - '0');
}
int agenda_ano(const char *iso) {
  if (!ehIso(iso)) return 0;
  return (iso[0]-'0')*1000 + (iso[1]-'0')*100 + (iso[2]-'0')*10 + (iso[3]-'0');
}

int agenda_semana(const char *iso) {
  long d;
  if (!ehIso(iso)) return -1;
  d = diasDeIso(iso);
  if (!d) return -1;
  // 1970-01-01 foi QUINTA (4, contando domingo = 0). O resto negativo do C
  // obriga o "+ 7" antes do segundo modulo — sem ele uma data anterior a 1970
  // devolveria indice negativo e leria fora da tabela de nomes.
  return (int)(((d % 7) + 4 + 7) % 7);
}

const char *agenda_mes_nome(int mes) {
  // OS MESMOS de desc_data_extenso, byte a byte: a tabela de traducao ja tem
  // essas chaves e uma segunda lista aqui seria a que fica para tras.
  static const char *MES[12] = {
    "janeiro", "fevereiro", "mar\xc3\xa7o", "abril", "maio", "junho",
    "julho", "agosto", "setembro", "outubro", "novembro", "dezembro"
  };
  if (mes < 1 || mes > 12) return "";
  return MES[mes - 1];
}

const char *agenda_semana_nome(int dia) {
  // ABREVIADO de proposito: o nome inteiro nao cabe sob o numeral do dia e, na
  // linha do tempo, o dia da semana e apoio — quem procura "quando" ja leu
  // "em 3 dias" ao lado.
  // O "\xc3\xa1" de "s\xc3\xa1b" fecha em string PROPRIA: em C o escape hexadecimal e
  // guloso e "\xc3\xa1b" seria lido como UM caractere 0xA1B, nao como "\xc3\xa1" mais 'b'.
  static const char *SEM[7] = { "dom", "seg", "ter", "qua", "qui", "sex", ("s\xc3\xa1" "b") };
  if (dia < 0 || dia > 6) return "";
  return SEM[dia];
}

void agenda_falta(const char *iso, char *dst, size_t tam) {
  int d;
  if (!dst || !tam) return;
  dst[0] = 0;
  d = agenda_dias(iso);
  if (d == AG_SEM_DATA || d < 0) return;
  if (d == 0)      { snprintf(dst, tam, "%s", i18n("hoje")); return; }
  if (d == 1)      { snprintf(dst, tam, "%s", i18n("amanh\xc3\xa3")); return; }
  if (d <= 7)      { snprintf(dst, tam, i18n("em %d dias"), d); return; }
  // ARREDONDA PARA BAIXO, e a data exata esta desenhada ao lado no numeral: o
  // que esta frase faz e dar a ESCALA da espera de longe ("semanas" contra
  // "meses"), nao substituir o dia. Arredondar para o mais proximo faria
  // "em 2 meses" aparecer sobre um numeral de 45 dias, que le como erro.
  if (d < 56)      { snprintf(dst, tam, i18n("em %d semanas"), d / 7); return; }
  { int m = d / 30;
    if (m < 2) m = 2;
    snprintf(dst, tam, i18n("em %d meses"), m); }
}

int agenda_marco(const AgItem *it, char *dst, size_t tam) {
  if (!dst || !tam) return 0;
  dst[0] = 0;
  if (!it || !it->tipoEp[0]) return 0;
  // O `episode_type` do TMDB tem quatro valores e tres deles sao marco. NAO se
  // infere do numero do episodio: temporada com 8, 10 ou 23 episodios nao tem
  // um "ultimo" previsivel, e a serie que muda de tamanho entre temporadas faria
  // a inferencia mentir justamente na temporada em que o dono esta.
  if (!strcmp(it->tipoEp, "premiere"))
    snprintf(dst, tam, "%s", i18n("Estreia da temporada"));
  else if (!strcmp(it->tipoEp, "finale"))
    snprintf(dst, tam, "%s", i18n("Final da temporada"));
  else if (!strcmp(it->tipoEp, "mid_season"))
    snprintf(dst, tam, "%s", i18n("Volta da meia temporada"));
  else return 0;
  return 1;
}

void agenda_apoio(const AgItem *it, char *dst, size_t tam) {
  size_t u = 0;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!it) return;
  // " \xc2\xb7 " e o ponto medio que o resto do app usa como separador de meta.
  // O primeiro pedaco entra sem ele — juntar sempre e depois aparar o comeco ja
  // deixou um separador orfao na esquerda de outra tela deste repositorio.
  if (it->rede[0])
    u += (size_t)snprintf(dst + u, tam - u, "%s", it->rede);
  if (it->duracao > 0 && u + 1 < tam)
    u += (size_t)snprintf(dst + u, tam - u, "%s", u ? " \xc2\xb7 " : "");
  if (it->duracao > 0 && u < tam)
    u += (size_t)snprintf(dst + u, tam - u, i18n("%d min"), it->duracao);
  if (it->temporadas > 0 && u + 1 < tam)
    u += (size_t)snprintf(dst + u, tam - u, "%s", u ? " \xc2\xb7 " : "");
  if (it->temporadas > 0 && u < tam)
    snprintf(dst + u, tam - u,
             it->temporadas == 1 ? i18n("%d temporada") : i18n("%d temporadas"),
             it->temporadas);
}

// ---------------------------------------------------------------------------
// situacao
// ---------------------------------------------------------------------------
static int prefixoIgual(const char *s, const char *p) {
  size_t i;
  for (i = 0; p[i]; i++) {
    char a = s[i], b = p[i];
    if (a >= 'A' && a <= 'Z') a = (char)(a - 'A' + 'a');
    if (b >= 'A' && b <= 'Z') b = (char)(b - 'A' + 'a');
    if (a != b) return 0;
  }
  return 1;
}

AgSituacao agenda_situacao_de(const char *cru) {
  if (!cru || !cru[0]) return AG_DESCONHECIDA;
  // As duas grafias: o TMDB escreve "Returning Series" e "Canceled"; o Trakt
  // escreve tudo em minusculas e usa "canceled" tambem. "Cancelled" com dois
  // eles aparece em corpos antigos do TMDB — as duas entram.
  if (prefixoIgual(cru, "returning")) return AG_VOLTANDO;
  if (prefixoIgual(cru, "continuing")) return AG_VOLTANDO;   // grafia do Trakt
  if (prefixoIgual(cru, "cancel"))    return AG_CANCELADA;
  if (prefixoIgual(cru, "ended"))     return AG_ENCERRADA;
  if (prefixoIgual(cru, "in produc")) return AG_PRODUCAO;
  if (prefixoIgual(cru, "planned"))   return AG_PRODUCAO;
  if (prefixoIgual(cru, "upcoming"))  return AG_PRODUCAO;
  if (prefixoIgual(cru, "pilot"))     return AG_PRODUCAO;
  return AG_DESCONHECIDA;
}

// ---------------------------------------------------------------------------
// arquivos
// ---------------------------------------------------------------------------
static const char *arquivo(const char *base) {
  static char nome[64];
  int p = perfis_ativo();
  if (p <= 0) { snprintf(nome, sizeof nome, "%s.txt", base); return nome; }
  snprintf(nome, sizeof nome, "%s-p%d.txt", base, p);
  return nome;
}

// Campo de TSV: tabulacao e quebra de linha viram espaco. Um titulo com
// tabulacao dentro parte a linha em duas e o arquivo seguinte le lixo.
static void limpo(char *dst, size_t tam, const char *s) {
  size_t i = 0;
  if (!tam) return;
  if (!s) { dst[0] = 0; return; }
  for (; s[i] && i + 1 < tam; i++)
    dst[i] = (s[i] == '\t' || s[i] == '\n' || s[i] == '\r') ? ' ' : s[i];
  dst[i] = 0;
}

// Proximo campo da linha, ate a tabulacao. Devolve o ponteiro para depois do
// separador, ou NULL no fim da linha.
static const char *campo(const char *p, char *dst, size_t tam) {
  size_t i = 0;
  if (!tam) return NULL;
  if (!p) { dst[0] = 0; return NULL; }
  while (p[i] && p[i] != '\t' && p[i] != '\n') i++;
  { size_t n = i < tam - 1 ? i : tam - 1;
    memcpy(dst, p, n); dst[n] = 0; }
  if (p[i] == '\t') return p + i + 1;
  return NULL;
}

// OS CAMPOS NOVOS VAO NO FIM DA LINHA, e isso nao e arrumacao: `campo()`
// devolve NULL quando a linha acaba e os leitores seguintes recebem "" sem
// erro. Um agenda-p<N>.txt gravado pela versao anterior continua sendo lido —
// perde a sinopse e ganha de volta na proxima passada do fio. Inserir no MEIO
// deslocaria todos os campos seguintes em silencio, e o sintoma seria uma data
// no lugar do nome do episodio.
#define AG_LINHA_BYTES 1600

static void gravarCache(void) {
  char *txt;
  size_t cap = (size_t)nCache * AG_LINHA_BYTES + 64, usado = 0;
  int i;
  if (!cacheSujo) return;
  txt = malloc(cap);
  if (!txt) return;
  txt[0] = 0;
  for (i = 0; i < nCache; i++) {
    char t[160], po[512], ne[120], si[400], re[64];
    limpo(t, sizeof t, cache[i].titulo);
    limpo(po, sizeof po, cache[i].poster);
    limpo(ne, sizeof ne, cache[i].nomeEp);
    limpo(si, sizeof si, cache[i].sinopse);
    limpo(re, sizeof re, cache[i].rede);
    usado += (size_t)snprintf(txt + usado, cap - usado,
                              "%s\t%s\t%s\t%d\t%d\t%d\t%s\t%s\t%s\t%lld"
                              "\t%s\t%s\t%s\t%d\t%d\n",
                              cache[i].imdb, t, po, cache[i].situacao,
                              cache[i].temporada, cache[i].episodio, ne,
                              cache[i].dataProx, cache[i].dataUlt,
                              cache[i].visto,
                              si, cache[i].tipoEp, re,
                              cache[i].duracao, cache[i].temporadas);
    if (usado + AG_LINHA_BYTES >= cap) break;
  }
  dados_gravar_leve(arquivo("agenda"), txt);
  free(txt);
  cacheSujo = 0;
}

static void lerCache(void) {
  char *s = dados_ler(arquivo("agenda"));
  const char *p;
  nCache = 0;
  if (!s) return;
  p = s;
  while (*p && nCache < AG_CACHE_MAX) {
    AgReg r;
    char n[24];
    const char *q = p;
    memset(&r, 0, sizeof r);
    q = campo(q, r.imdb, sizeof r.imdb);
    q = campo(q, r.titulo, sizeof r.titulo);
    q = campo(q, r.poster, sizeof r.poster);
    q = campo(q, n, sizeof n); r.situacao = atoi(n);
    q = campo(q, n, sizeof n); r.temporada = atoi(n);
    q = campo(q, n, sizeof n); r.episodio = atoi(n);
    q = campo(q, r.nomeEp, sizeof r.nomeEp);
    q = campo(q, r.dataProx, sizeof r.dataProx);
    q = campo(q, r.dataUlt, sizeof r.dataUlt);
    q = campo(q, n, sizeof n); r.visto = atoll(n);
    q = campo(q, r.sinopse, sizeof r.sinopse);
    q = campo(q, r.tipoEp, sizeof r.tipoEp);
    q = campo(q, r.rede, sizeof r.rede);
    q = campo(q, n, sizeof n); r.duracao = atoi(n);
    q = campo(q, n, sizeof n); r.temporadas = atoi(n);
    (void)q;
    if (r.imdb[0]) cache[nCache++] = r;
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
  }
  free(s);
}

static void gravarLembretes(void) {
  char txt[AG_LEMB_MAX * 48 + 8];
  size_t usado = 0;
  int i;
  txt[0] = 0;
  for (i = 0; i < nLembretes; i++)
    usado += (size_t)snprintf(txt + usado, sizeof txt - usado, "%s\t%s\t%d\n",
                              lembretes[i].imdb, lembretes[i].data,
                              lembretes[i].avisado);
  dados_gravar(arquivo("lembretes"), txt);
}

static void lerLembretes(void) {
  char *s = dados_ler(arquivo("lembretes"));
  const char *p;
  nLembretes = 0;
  if (!s) return;
  p = s;
  while (*p && nLembretes < AG_LEMB_MAX) {
    AgLembrete l;
    char n[16];
    const char *q = p;
    memset(&l, 0, sizeof l);
    q = campo(q, l.imdb, sizeof l.imdb);
    q = campo(q, l.data, sizeof l.data);
    if (q) { campo(q, n, sizeof n); l.avisado = atoi(n); }
    if (l.imdb[0]) lembretes[nLembretes++] = l;
    while (*p && *p != '\n') p++;
    if (*p == '\n') p++;
  }
  free(s);
}

void agenda_iniciar(void) {
  int p = perfis_ativo();
  if (p == perfilCarregado) return;
  pthread_mutex_lock(&trava);
  cacheSujo = 0;
  perfilCarregado = p;
  lerCache();
  lerLembretes();
  pthread_mutex_unlock(&trava);
  nLista = 0;
}

void agenda_esquecer(void) {
  int p;
  pthread_mutex_lock(&trava);
  nCache = 0; nLembretes = 0; nLista = 0; cacheSujo = 0;
  dados_apagar("agenda.txt");
  dados_apagar("lembretes.txt");
  for (p = 1; p <= 8; p++) {
    char nome[48];
    snprintf(nome, sizeof nome, "agenda-p%d.txt", p);    dados_apagar(nome);
    snprintf(nome, sizeof nome, "lembretes-p%d.txt", p); dados_apagar(nome);
  }
  perfilCarregado = -1;
  pthread_mutex_unlock(&trava);
}

// ---------------------------------------------------------------------------
// registro
// ---------------------------------------------------------------------------
static AgReg *acharTrancado(const char *imdb) {
  int i;
  if (!imdb || !imdb[0]) return NULL;
  for (i = 0; i < nCache; i++)
    if (!strcmp(cache[i].imdb, imdb)) return &cache[i];
  return NULL;
}

// O id que o app usa para um episodio e composto ("tt123:4:9"). O registro da
// agenda e da SERIE — corta no primeiro ':'.
static void serieDe(char *dst, size_t tam, const char *imdb) {
  size_t i = 0;
  if (!tam) return;
  if (!imdb) { dst[0] = 0; return; }
  while (imdb[i] && imdb[i] != ':' && i + 1 < tam) { dst[i] = imdb[i]; i++; }
  dst[i] = 0;
}

void agenda_registrar(const char *imdb, const char *titulo, const char *poster,
                      const char *statusCru, int temporada, int episodio,
                      const char *nomeEp, const char *dataProx,
                      const char *dataUlt) {
  char id[24];
  AgReg *r;
  serieDe(id, sizeof id, imdb);
  if (!id[0]) return;
  pthread_mutex_lock(&trava);
  r = acharTrancado(id);
  if (!r) {
    if (nCache >= AG_CACHE_MAX) {
      // Teto batido: descarta o registro mais VELHO, que e o que menos custa
      // reobter. Sem esta regra o cache congelava no primeiro cheio e series
      // novas nunca entravam.
      int i, pior = 0;
      for (i = 1; i < nCache; i++)
        if (cache[i].visto < cache[pior].visto) pior = i;
      r = &cache[pior];
      memset(r, 0, sizeof *r);
    } else {
      r = &cache[nCache++];
      memset(r, 0, sizeof *r);
    }
    snprintf(r->imdb, sizeof r->imdb, "%s", id);
  }
  // Campo vazio NAO apaga o que ja estava: o parse do detalhe pode chegar sem
  // poster (a serie veio do Trakt) e o calendario perderia a arte que ja tinha.
  if (titulo && titulo[0]) snprintf(r->titulo, sizeof r->titulo, "%s", titulo);
  if (poster && poster[0]) snprintf(r->poster, sizeof r->poster, "%s", poster);
  if (statusCru && statusCru[0]) r->situacao = (int)agenda_situacao_de(statusCru);
  // O bloco do proximo episodio vem inteiro ou nao vem: quando o TMDB devolve
  // next_episode_to_air = null a serie DEIXOU de ter proximo (foi ao ar, ou
  // acabou), e manter a data velha e exatamente a mentira que este modulo
  // existe para nao contar.
  if (dataProx && ehIso(dataProx)) {
    snprintf(r->dataProx, sizeof r->dataProx, "%.10s", dataProx);
    r->temporada = temporada; r->episodio = episodio;
    if (nomeEp) snprintf(r->nomeEp, sizeof r->nomeEp, "%s", nomeEp);
  } else if (statusCru && statusCru[0]) {
    r->dataProx[0] = 0; r->nomeEp[0] = 0;
    r->temporada = 0; r->episodio = 0;
  }
  if (dataUlt && ehIso(dataUlt)) snprintf(r->dataUlt, sizeof r->dataUlt, "%.10s", dataUlt);
  r->visto = (long long)time(NULL);
  cacheSujo = 1;
  { // Adiamento: se a data do lembrete mudou, o aviso daquele dia nao vale
    // mais e volta a estar por dar.
    int i;
    for (i = 0; i < nLembretes; i++)
      if (!strcmp(lembretes[i].imdb, id) &&
          strcmp(lembretes[i].data, r->dataProx)) {
        snprintf(lembretes[i].data, sizeof lembretes[i].data, "%s", r->dataProx);
        lembretes[i].avisado = 0;
      }
  }
  gravarCache();
  pthread_mutex_unlock(&trava);
}

void agenda_registrar_extra(const char *imdb, const char *sinopse,
                            const char *tipoEp, const char *rede,
                            int duracao, int temporadas) {
  char id[24];
  AgReg *r;
  serieDe(id, sizeof id, imdb);
  if (!id[0]) return;
  pthread_mutex_lock(&trava);
  r = acharTrancado(id);
  // SEM CRIAR REGISTRO. Estes campos sao apoio: se agenda_registrar ainda nao
  // passou por aqui, nao ha data nem situacao, e uma linha de agenda com rede e
  // duracao e sem dia nao e nada que a tela saiba desenhar.
  if (!r) { pthread_mutex_unlock(&trava); return; }
  if (sinopse && sinopse[0]) snprintf(r->sinopse, sizeof r->sinopse, "%s", sinopse);
  if (tipoEp && tipoEp[0])   snprintf(r->tipoEp, sizeof r->tipoEp, "%s", tipoEp);
  if (rede && rede[0])       snprintf(r->rede, sizeof r->rede, "%s", rede);
  if (duracao > 0)    r->duracao = duracao;
  if (temporadas > 0) r->temporadas = temporadas;
  cacheSujo = 1;
  gravarCache();
  pthread_mutex_unlock(&trava);
}

const AgItem *agenda_registro(const char *imdb) {
  static AgItem it;
  char id[24];
  AgReg *r;
  serieDe(id, sizeof id, imdb);
  pthread_mutex_lock(&trava);
  r = acharTrancado(id);
  if (!r) { pthread_mutex_unlock(&trava); return NULL; }
  memset(&it, 0, sizeof it);
  snprintf(it.imdb, sizeof it.imdb, "%s", r->imdb);
  snprintf(it.titulo, sizeof it.titulo, "%s", r->titulo);
  snprintf(it.poster, sizeof it.poster, "%s", r->poster);
  it.situacao = r->situacao;
  it.temporada = r->temporada; it.episodio = r->episodio;
  snprintf(it.nomeEp, sizeof it.nomeEp, "%s", r->nomeEp);
  snprintf(it.dataProx, sizeof it.dataProx, "%s", r->dataProx);
  snprintf(it.dataUlt, sizeof it.dataUlt, "%s", r->dataUlt);
  snprintf(it.sinopse, sizeof it.sinopse, "%s", r->sinopse);
  snprintf(it.tipoEp, sizeof it.tipoEp, "%s", r->tipoEp);
  snprintf(it.rede, sizeof it.rede, "%s", r->rede);
  it.duracao = r->duracao; it.temporadas = r->temporadas;
  pthread_mutex_unlock(&trava);
  it.lembrete = agenda_lembrete(id);
  return &it;
}

// ---------------------------------------------------------------------------
// frase da pagina de titulo
// ---------------------------------------------------------------------------
int agenda_frase(const char *imdb, char *dst, size_t tam) {
  const AgItem *r;
  char quando[64];
  if (!dst || !tam) return 0;
  dst[0] = 0;
  r = agenda_registro(imdb);
  if (!r) return 0;

  if (r->dataProx[0]) {
    int d = agenda_dias(r->dataProx);
    agenda_quando(r->dataProx, quando, sizeof quando);
    if (d == AG_SEM_DATA || !quando[0]) return 0;
    if (r->temporada > 0 && r->episodio > 0) {
      // O T/E tambem e traduzido: em ingles a abreviacao e S/E. Mesma regra do
      // rotulo "Retomar T2E3" do botao primario (detail.c).
      if (d < 0) snprintf(dst, tam, i18n("T%dE%d foi ao ar em %s"),
                          r->temporada, r->episodio, quando);
      else snprintf(dst, tam, i18n("Próximo episódio T%dE%d · %s"),
                    r->temporada, r->episodio, quando);
    } else {
      snprintf(dst, tam, i18n("Próximo episódio · %s"), quando);
    }
    return 1;
  }

  // Sem data. So ha o que dizer quando a SITUACAO e conhecida — e ai a frase e
  // sobre a serie ter acabado, nunca uma data inventada.
  switch (r->situacao) {
    case AG_ENCERRADA:
      if (r->dataUlt[0]) {
        agenda_quando(r->dataUlt, quando, sizeof quando);
        snprintf(dst, tam, i18n("Série encerrada · último episódio em %s"), quando);
      } else snprintf(dst, tam, "%s", i18n("Série encerrada"));
      return 1;
    case AG_CANCELADA:
      if (r->dataUlt[0]) {
        agenda_quando(r->dataUlt, quando, sizeof quando);
        snprintf(dst, tam, i18n("Série cancelada · último episódio em %s"), quando);
      } else snprintf(dst, tam, "%s", i18n("Série cancelada"));
      return 1;
    case AG_VOLTANDO:
      snprintf(dst, tam, "%s", i18n("Sem data anunciada para o próximo episódio"));
      return 1;
    case AG_PRODUCAO:
      snprintf(dst, tam, "%s", i18n("Em produção · sem data anunciada"));
      return 1;
    default:
      return 0;
  }
}

// ---------------------------------------------------------------------------
// lembretes
// ---------------------------------------------------------------------------
int agenda_lembrete(const char *imdb) {
  char id[24];
  int i, tem = 0;
  serieDe(id, sizeof id, imdb);
  if (!id[0]) return 0;
  pthread_mutex_lock(&trava);
  for (i = 0; i < nLembretes; i++)
    if (!strcmp(lembretes[i].imdb, id)) { tem = 1; break; }
  pthread_mutex_unlock(&trava);
  return tem;
}

int agenda_pode_lembrar(const char *imdb) {
  const AgItem *r = agenda_registro(imdb);
  int d;
  if (!r || !r->dataProx[0]) return 0;
  d = agenda_dias(r->dataProx);
  return d != AG_SEM_DATA && d >= 0;
}

int agenda_alternar_lembrete(const char *imdb) {
  char id[24];
  int i, achou = -1, novo = 0;
  const AgItem *r;
  serieDe(id, sizeof id, imdb);
  if (!id[0]) return 0;
  r = agenda_registro(imdb);
  pthread_mutex_lock(&trava);
  for (i = 0; i < nLembretes; i++)
    if (!strcmp(lembretes[i].imdb, id)) { achou = i; break; }
  if (achou >= 0) {
    lembretes[achou] = lembretes[--nLembretes];
    novo = 0;
  } else if (r && r->dataProx[0] && nLembretes < AG_LEMB_MAX) {
    AgLembrete *l = &lembretes[nLembretes++];
    memset(l, 0, sizeof *l);
    snprintf(l->imdb, sizeof l->imdb, "%s", id);
    snprintf(l->data, sizeof l->data, "%s", r->dataProx);
    novo = 1;
  }
  gravarLembretes();
  pthread_mutex_unlock(&trava);
  return novo;
}

void agenda_marcar_avisado(const char *imdb) {
  char id[24];
  int i;
  serieDe(id, sizeof id, imdb);
  pthread_mutex_lock(&trava);
  for (i = 0; i < nLembretes; i++)
    if (!strcmp(lembretes[i].imdb, id)) { lembretes[i].avisado = 1; break; }
  gravarLembretes();
  pthread_mutex_unlock(&trava);
}

int agenda_devidos(const AgItem **saida, int max) {
  int i, n = 0;
  for (i = 0; i < nLista && n < max; i++) {
    int d;
    if (!lista[i].lembrete || !lista[i].dataProx[0]) continue;
    { int j, avisado = 0;
      pthread_mutex_lock(&trava);
      for (j = 0; j < nLembretes; j++)
        if (!strcmp(lembretes[j].imdb, lista[i].imdb)) { avisado = lembretes[j].avisado; break; }
      pthread_mutex_unlock(&trava);
      if (avisado) continue; }
    d = agenda_dias(lista[i].dataProx);
    if (d == AG_SEM_DATA || d > 0) continue;
    saida[n++] = &lista[i];
  }
  return n;
}

// ---------------------------------------------------------------------------
// montagem do calendario
// ---------------------------------------------------------------------------
static int ehSerieCat(const CatItem *ci) {
  return ci && (!strcmp(ci->tipo, "series") || ci->nTemporadas > 0);
}

static int jaNaLista(const char *imdb) {
  int i;
  for (i = 0; i < nLista; i++) if (!strcmp(lista[i].imdb, imdb)) return 1;
  return 0;
}

// Ordem do calendario, e ela e a tela inteira:
//   1. o que tem data, da mais proxima para a mais distante — inclusive as que
//      ja passaram HOJE (d == 0), que sao a razao de existir do lembrete;
//   2. o que nao tem data, agrupado por situacao (voltando, producao,
//      desconhecida, encerrada, cancelada) e depois por titulo.
// Data que ja passou nao entra: um calendario que mostra ontem e um historico.
static int chaveOrdem(const AgItem *a) {
  if (a->dataProx[0]) {
    int d = agenda_dias(a->dataProx);
    if (d != AG_SEM_DATA && d >= 0) return d;
  }
  switch (a->situacao) {
    case AG_VOLTANDO:     return 100000;
    case AG_PRODUCAO:     return 100001;
    case AG_DESCONHECIDA: return 100002;
    case AG_ENCERRADA:    return 100003;
    case AG_CANCELADA:    return 100004;
  }
  return 100005;
}

static void poe(const char *imdb, const char *titulo, const char *poster) {
  AgItem *it;
  const AgItem *r;
  char id[24];
  serieDe(id, sizeof id, imdb);
  if (!id[0] || nLista >= AG_MAX || jaNaLista(id)) return;
  it = &lista[nLista++];
  memset(it, 0, sizeof *it);
  snprintf(it->imdb, sizeof it->imdb, "%s", id);
  if (titulo) snprintf(it->titulo, sizeof it->titulo, "%s", titulo);
  if (poster) snprintf(it->poster, sizeof it->poster, "%s", poster);
  r = agenda_registro(id);
  if (r) {
    it->situacao = r->situacao;
    it->temporada = r->temporada; it->episodio = r->episodio;
    snprintf(it->nomeEp, sizeof it->nomeEp, "%s", r->nomeEp);
    snprintf(it->dataProx, sizeof it->dataProx, "%s", r->dataProx);
    snprintf(it->dataUlt, sizeof it->dataUlt, "%s", r->dataUlt);
    snprintf(it->sinopse, sizeof it->sinopse, "%s", r->sinopse);
    snprintf(it->tipoEp, sizeof it->tipoEp, "%s", r->tipoEp);
    snprintf(it->rede, sizeof it->rede, "%s", r->rede);
    it->duracao = r->duracao; it->temporadas = r->temporadas;
    // O cache e quem tem o titulo bom quando o catalogo ainda nao publicou —
    // e o caso do primeiro arranque, em que a tela abre antes da descoberta.
    if (!it->titulo[0] && r->titulo[0])
      snprintf(it->titulo, sizeof it->titulo, "%s", r->titulo);
    if (!it->poster[0] && r->poster[0])
      snprintf(it->poster, sizeof it->poster, "%s", r->poster);
  }
  it->lembrete = agenda_lembrete(id);
}

int agenda_montar(void) {
  int i, n;
  nLista = 0;

  // 1. A marca UNICA de "quero ver" (salvos local + watchlist do Trakt +
  //    biblioteca da conta). Ver salvos.h: existe UM destino de leitura.
  n = cat_n();
  for (i = 0; i < n && nLista < AG_MAX; i++) {
    const CatItem *ci = cat_item(i);
    if (!ci || !ci->naLista || !ehSerieCat(ci) || !ci->imdb[0]) continue;
    poe(ci->imdb, ci->titulo, ci->poster);
  }

  // 2. O que o dono esta VENDO: progresso guardado no perfil ativo. Quem esta
  //    no meio da 3a temporada segue a serie sem nunca ter apertado "+".
  { ProgRegistro regs[PROG_MAX];
    int q = prog_ler(regs, PROG_MAX), k;
    for (k = 0; k < q && nLista < AG_MAX; k++) {
      int idc;
      const CatItem *ci;
      if (strcmp(regs[k].tipo, "series") && regs[k].temporada <= 0) continue;
      if (!regs[k].contentId[0]) continue;
      idc = cat_indice_por_imdb(regs[k].contentId);
      ci = idc >= 0 ? cat_item(idc) : NULL;
      poe(regs[k].contentId, ci ? ci->titulo : NULL, ci ? ci->poster : NULL);
    } }

  // 3. Series que so o CACHE conhece (o catalogo ainda nao publicou, ou o
  //    titulo saiu da lista mas o lembrete continua). Um lembrete que some da
  //    tela sem o dono ter desligado e pior que uma linha a mais.
  { char ids[AG_LEMB_MAX][24];
    int k, q;
    pthread_mutex_lock(&trava);
    q = nLembretes;
    for (k = 0; k < q; k++) snprintf(ids[k], sizeof ids[k], "%s", lembretes[k].imdb);
    pthread_mutex_unlock(&trava);
    for (k = 0; k < q && nLista < AG_MAX; k++) poe(ids[k], NULL, NULL); }

  // Ordena por insercao direta: a lista tem dezenas de itens e acontece uma vez
  // por abertura da tela.
  for (i = 1; i < nLista; i++) {
    AgItem chave = lista[i];
    int ck = chaveOrdem(&chave), j = i - 1;
    while (j >= 0) {
      int cj = chaveOrdem(&lista[j]);
      if (cj < ck || (cj == ck && strcmp(lista[j].titulo, chave.titulo) <= 0)) break;
      lista[j + 1] = lista[j];
      j--;
    }
    lista[j + 1] = chave;
  }
  return nLista;
}

int agenda_n(void) { return nLista; }
const AgItem *agenda_lista(int i) {
  return (i >= 0 && i < nLista) ? &lista[i] : NULL;
}

// ---------------------------------------------------------------------------
// ATUALIZACAO EM SEGUNDO PLANO — o unico pedido de rede deste modulo
//
// Um GET /tv/<id> por serie SEGUIDA cujo registro esta faltando ou tem mais de
// 12 h. Isto E pedido novo: sem ele o calendario so conheceria as series que o
// dono abriu na tela de titulo. O custo e proporcional a lista de quem segue,
// nao ao catalogo, e acontece quando a tela Agenda abre — nao no arranque.
//
// Fio UNICO e sequencial de proposito: sao dezenas de pedidos curtos e a TV
// tem uma pilha de rede modesta; disparar todos juntos foi o que ja derrubou a
// descoberta em aparelho antigo.
// ---------------------------------------------------------------------------
#define AG_VALIDADE_S (12 * 3600)

static int precisaBuscar(const char *imdb) {
  AgReg *r;
  int precisa;
  pthread_mutex_lock(&trava);
  r = acharTrancado(imdb);
  precisa = !r || (long long)time(NULL) - r->visto > AG_VALIDADE_S;
  pthread_mutex_unlock(&trava);
  return precisa;
}

// Le status / next_episode_to_air / last_episode_to_air de um corpo /tv/<id>.
// Mesma forma do parse de extras.c — quem mexer num tem de olhar o outro.
// O CORPO INTEIRO, e nao so a data.
//
// Ate esta revisao este parse lia quatro campos e descartava o resto — e o
// resto e justamente o que faltava a tela Agenda: sinopse do episodio, se ele e
// estreia ou final de temporada, quanto dura, em que rede passa e quantas
// temporadas a serie tem. Tudo isso ja estava dentro do MESMO corpo, pelo qual
// o fio deste modulo ja pagou. Ler mais nao custa pedido nenhum; a alternativa
// era pedir noticia ou citacao a uma API que este app nao tem.
static void lerCorpoTv(const char *imdb, const char *titulo, const char *corpo) {
  const char *fim = corpo + strlen(corpo);
  char status[32] = "", dataProx[16] = "", dataUlt[16] = "", nomeEp[120] = "";
  char sinopse[400] = "", tipoEp[24] = "", rede[64] = "";
  int temp = 0, ep = 0, duracao = 0, temporadas = 0;
  const char *b;
  js_texto(corpo, fim, "status", status, sizeof status);
  b = strstr(corpo, "\"next_episode_to_air\"");
  if (b) {
    const char *o = strchr(b, '{');
    // `null` vem antes de qualquer '{' seguinte quando nao ha proximo — a
    // guarda e a virgula/quebra entre a chave e o objeto.
    const char *nulo = strstr(b, "null");
    if (o && (!nulo || nulo > o)) {
      const char *of = js_fim(o);
      temp = (int)js_num(o, of, "season_number", 0.0);
      ep   = (int)js_num(o, of, "episode_number", 0.0);
      js_texto(o, of, "air_date", dataProx, sizeof dataProx);
      js_texto(o, of, "name", nomeEp, sizeof nomeEp);
      js_texto(o, of, "overview", sinopse, sizeof sinopse);
      js_texto(o, of, "episode_type", tipoEp, sizeof tipoEp);
      duracao = (int)js_num(o, of, "runtime", 0.0);
    }
  }
  b = strstr(corpo, "\"last_episode_to_air\"");
  if (b) {
    const char *o = strchr(b, '{');
    const char *nulo = strstr(b, "null");
    if (o && (!nulo || nulo > o)) {
      const char *of = js_fim(o);
      js_texto(o, of, "air_date", dataUlt, sizeof dataUlt);
      // SINOPSE DO ULTIMO so quando nao ha proximo: a serie encerrada e a que
      // esperou anuncio sao as duas linhas da tela que ficariam so com o nome,
      // e o que se pode dizer delas com verdade e o episodio que JA foi ao ar.
      if (!sinopse[0]) js_texto(o, of, "overview", sinopse, sizeof sinopse);
      if (!tipoEp[0])  js_texto(o, of, "episode_type", tipoEp, sizeof tipoEp);
      if (duracao <= 0) duracao = (int)js_num(o, of, "runtime", 0.0);
    }
  }
  temporadas = (int)js_num(corpo, fim, "number_of_seasons", 0.0);
  // A REDE e a primeira de networks[]. Uma serie pode ter varias (coproducao),
  // e a primeira e a que o TMDB trata como principal — e a mesma que a fileira
  // de logos da tela de titulo desenha primeiro. js_texto_raiz_em e nao
  // js_texto: dentro do objeto da rede ha "logo_path" e "origin_country" antes
  // do "name" em alguns corpos, e a leitura crua pegaria o campo errado se um
  // dia aparecer um "name" aninhado.
  { const char *o = js_array(corpo, fim, "networks");
    if (o) js_texto_raiz_em(o, js_fim(o), "name", rede, sizeof rede); }
  // `episode_run_time` e um ARRAY de inteiros ("[52]") e serve de reserva
  // quando o objeto do episodio nao traz runtime proprio — o TMDB so preenche o
  // runtime do episodio depois que ele vai ao ar, e a agenda fala de episodio
  // que ainda NAO foi.
  if (duracao <= 0) {
    const char *k = strstr(corpo, "\"episode_run_time\"");
    if (k) { const char *c = strchr(k, '['); if (c) duracao = atoi(c + 1); }
  }
  // `status` sempre vem no corpo do TMDB; e ele que autoriza apagar uma data
  // velha em agenda_registrar. Sem ele nao se conclui nada.
  // NOME E CARTAZ DO PROPRIO CORPO quando quem chamou nao tinha (serie
  // seguida que nao esta no catalogo): sem isto a Agenda desenhava "Serie" e
  // um retangulo cinza para uma serie com temporada, episodio e rede certos
  // (foto da C9, 21/09/2026). `name` e `poster_path` sao da raiz do /tv.
  { char nome[160] = "", cartaz[600] = "", pp[200] = "";
    if (!titulo || !titulo[0]) js_texto_raiz_em(corpo, fim, "name", nome, sizeof nome);
    js_texto_raiz_em(corpo, fim, "poster_path", pp, sizeof pp);
    if (pp[0] == '/') snprintf(cartaz, sizeof cartaz, "https://image.tmdb.org/t/p/w342%s", pp);
  if (status[0] || dataProx[0]) {
    agenda_registrar(imdb, (titulo && titulo[0]) ? titulo : nome, cartaz[0] ? cartaz : NULL,
                     status, temp, ep, nomeEp, dataProx, dataUlt);
    agenda_registrar_extra(imdb, sinopse, tipoEp, rede, duracao, temporadas);
  } }
}

static void *fioAgenda(void *arg) {
  char pend[AG_MAX][24];
  char tit[AG_MAX][160];
  long ids[AG_MAX];
  int nPend = 0, i;
  const char *chave = desc_chave_tmdb();
  (void)arg;

  for (i = 0; i < nLista && nPend < AG_MAX; i++) {
    int idc;
    const CatItem *ci;
    if (!precisaBuscar(lista[i].imdb)) continue;
    snprintf(pend[nPend], sizeof pend[nPend], "%s", lista[i].imdb);
    snprintf(tit[nPend], sizeof tit[nPend], "%s", lista[i].titulo);
    idc = cat_indice_por_imdb(lista[i].imdb);
    ci = idc >= 0 ? cat_item(idc) : NULL;
    ids[nPend] = ci ? ci->tmdb : 0;
    nPend++;
  }

  for (i = 0; i < nPend && chave[0]; i++) {
    char url[400];
    char *corpo;
    long idT = ids[i];
    if (idT <= 0) {
      // Mesmo caminho de extras.c: o id do TMDB so existe no catalogo depois
      // do enriquecimento, e /find resolve por IMDb sem traducao no meio.
      snprintf(url, sizeof url,
               "https://api.themoviedb.org/3/find/%s?api_key=%s"
               "&external_source=imdb_id", pend[i], chave);
      corpo = rede_baixar(url, 15);
      if (corpo) {
        const char *v = js_array(corpo, NULL, "tv_results");
        if (v) idT = (long)js_num(v, js_fim(v), "id", 0.0);
        free(corpo);
      }
      if (idT <= 0) continue;
    }
    snprintf(url, sizeof url, "https://api.themoviedb.org/3/tv/%ld?api_key=%s&language=%s",
             idT, chave, desc_tmdb_idioma());
    corpo = rede_baixar(url, 15);
    if (!corpo) continue;
    lerCorpoTv(pend[i], tit[i], corpo);
    free(corpo);
  }

  pthread_mutex_lock(&trava);
  fioVivo = 0;
  pthread_mutex_unlock(&trava);
  return NULL;
}

void agenda_atualizar_seguidas(void) {
  pthread_t f;
  const char *chave = desc_chave_tmdb();
  // Sem chave nao ha o que pedir — e e assim que os testes nunca tocam a rede.
  if (!chave || !chave[0]) return;
  pthread_mutex_lock(&trava);
  if (fioVivo) { pthread_mutex_unlock(&trava); return; }
  fioVivo = 1;
  pthread_mutex_unlock(&trava);
  if (pthread_create(&f, NULL, fioAgenda, NULL) != 0) {
    pthread_mutex_lock(&trava); fioVivo = 0; pthread_mutex_unlock(&trava);
    return;
  }
  pthread_detach(f);
}

int agenda_atualizando(void) {
  int v;
  pthread_mutex_lock(&trava);
  v = fioVivo;
  pthread_mutex_unlock(&trava);
  return v;
}
