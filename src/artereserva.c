#include "artereserva.h"
#include <pthread.h>
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include "trakt.h"
#include "artefontes.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// images.metahub.space/<tipo>/<tamanho>/<ttNNN>/img  ->  tipo e id.
// So poster e background: logo nao tem par no /find.
static int lerMetahub(const char *url, char *tipo, size_t tTipo, char *id, size_t tId) {
  static const char PREFIXO[] = "images.metahub.space/";
  const char *p = strstr(url, PREFIXO), *barra;
  size_t n;
  if (!p) return 0;
  p += sizeof PREFIXO - 1;
  barra = strchr(p, '/');
  if (!barra) return 0;
  n = (size_t)(barra - p);
  if (n + 1 > tTipo) return 0;
  memcpy(tipo, p, n); tipo[n] = 0;
  if (strcmp(tipo, "poster") && strcmp(tipo, "background")) return 0;
  p = strchr(barra + 1, '/');               // pula o tamanho (small/medium/large)
  if (!p) return 0;
  p++;
  barra = strchr(p, '/');
  n = barra ? (size_t)(barra - p) : strlen(p);
  if (n < 3 || n + 1 > tId || p[0] != 't' || p[1] != 't') return 0;
  memcpy(id, p, n); id[n] = 0;
  return 1;
}

// episodes.metahub.space/<ttNNN>/<temp>/<ep>/<tam>.jpg  ->  id, temporada, episodio.
static int lerStill(const char *url, char *id, size_t tId, int *temp, int *ep) {
  static const char PREFIXO[] = "episodes.metahub.space/";
  const char *p = strstr(url, PREFIXO), *barra;
  size_t n;
  if (!p) return 0;
  p += sizeof PREFIXO - 1;
  barra = strchr(p, '/');
  if (!barra) return 0;
  n = (size_t)(barra - p);
  if (n < 3 || n + 1 > tId || p[0] != 't' || p[1] != 't') return 0;
  memcpy(id, p, n); id[n] = 0;
  if (sscanf(barra + 1, "%d/%d/", temp, ep) != 2 || *temp < 0 || *ep < 1) return 0;
  return 1;
}

// Still de episodio: duas viagens (o id do TMDB pela /find, depois o
// episodio). So depois de o metahub falhar, e o still e o card de Continue
// Watching inteiro — vale as duas.
static int reservaStill(const char *chave, const char *id, int temp, int ep,
                        char *saida, size_t tam) {
  char api[300], caminho[128] = "";
  char *resp;
  const char *v;
  long idTv = 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", id, chave);
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  v = js_array(resp, NULL, "tv_results");
  if (v) idTv = (long)js_num(v, js_fim(v), "id", 0.0);
  free(resp);
  if (idTv <= 0) return 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/tv/%ld/season/%d/episode/%d?api_key=%s", idTv, temp, ep, chave);
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  js_texto_raiz(resp, "still_path", caminho, sizeof caminho);
  free(resp);
  if (caminho[0] != '/') return 0;
  // Na Samsung nunca `original`: o decode do navegador devolve o still inteiro
  // (3840 px em varias series) e foi um fundo desses que zerou o heap no
  // registro 1450 — ver fundoOriginal() em artehero.c.
#ifdef __EMSCRIPTEN__
  snprintf(saida, tam, "https://image.tmdb.org/t/p/w1280%s", caminho);
#else
  snprintf(saida, tam, "https://image.tmdb.org/t/p/original%s", caminho);
#endif
  printf("[tex] reserva do TMDB para still de %s S%dE%d\n", id, temp, ep);
  fflush(stdout);
  return 1;
}

// Tabela (url -> imdb) para arte de host que nao carrega o id na URL. O hash
// e apenas o ponto inicial de um probing linear: a chave completa (bytes e
// tamanho) continua sendo comparada, portanto uma colisao nunca escolhe a
// reserva TMDB de outro titulo.
//
// A arena guarda somente os bytes das URLs efetivamente registradas, sem
// duplicar esses bytes em cada celula da tabela. O teto de 2 MiB continua
// reservado no BSS por desenho (a tabela acrescenta cerca de 144 KiB), para
// manter o uso previsivel no WASM/Tizen; nao e uma alocacao sob demanda.
// O registry vive pela sessao do processo e nao tem reset: depois de 4096
// URLs distintas, ou antes disso se a arena for preenchida por URLs longas,
// novas reservas sao omitidas de forma explicita.
#define AR_REG 4096u
#define AR_URL_MAX 512u
#define AR_TEXT_MAX (2u * 1024u * 1024u)
typedef struct {
  uint32_t h;
  uint32_t off;
  uint16_t len;
  unsigned char poster, usado;
  char imdb[24];
} ArteRegistro;
static ArteRegistro reg[AR_REG];
static char regTexto[AR_TEXT_MAX];
static uint32_t regTextoN;
static pthread_mutex_t regTrava = PTHREAD_MUTEX_INITIALIZER;
static int regAvisouCheia, regAvisouTexto, regAvisouUrlLonga;

static uint32_t hashUrl(const char *u) {
  uint32_t h = 2166136261u;
  for (; *u; u++) { h ^= (unsigned char)*u; h *= 16777619u; }
  return h;
}

static int regSlot(const char *url, size_t n, uint32_t h, int *achou) {
  unsigned i = (unsigned)(h % AR_REG), passo;
  for (passo = 0; passo < AR_REG; passo++) {
    ArteRegistro *r = &reg[i];
    if (!r->usado) { *achou = 0; return (int)i; }
    if (r->h == h && r->len == n && !memcmp(regTexto + r->off, url, n)) {
      *achou = 1; return (int)i;
    }
    i = (i + 1u) % AR_REG;
  }
  *achou = 0;
  return -1;
}

int arte_reserva_registrar(const char *url, const char *imdb, int poster) {
  uint32_t h;
  size_t n;
  int achou, slot;
  if (!url || !url[0] || !imdb || strncmp(imdb, "tt", 2)) return 0;
  if (strstr(url, "images.metahub.space") || strstr(url, "image.tmdb.org") ||
      strstr(url, "episodes.metahub.space")) return 0; // esses ja se resolvem sozinhos
  n = strlen(url);
  if (n >= AR_URL_MAX) {
    pthread_mutex_lock(&regTrava);
    if (!regAvisouUrlLonga) {
      regAvisouUrlLonga = 1;
      fprintf(stderr, "[tex] reserva addon: URL excede limite de %u bytes\n", AR_URL_MAX - 1u);
    }
    pthread_mutex_unlock(&regTrava);
    return 0;
  }
  h = hashUrl(url);
  pthread_mutex_lock(&regTrava);
  slot = regSlot(url, n, h, &achou);
  if (slot < 0) {
    if (!regAvisouCheia) {
      regAvisouCheia = 1;
      fprintf(stderr, "[tex] reserva addon: tabela cheia (%u entradas), fallback omitido\n", AR_REG);
    }
    pthread_mutex_unlock(&regTrava);
    return 0;
  }
  if (!achou) {
    if (regTextoN + n > AR_TEXT_MAX) {
      if (!regAvisouTexto) {
        regAvisouTexto = 1;
        fprintf(stderr, "[tex] reserva addon: arena de URLs cheia (%u bytes), fallback omitido\n", AR_TEXT_MAX);
      }
      pthread_mutex_unlock(&regTrava);
      return 0;
    }
    reg[slot].h = h;
    reg[slot].off = regTextoN;
    reg[slot].len = (uint16_t)n;
    memcpy(regTexto + regTextoN, url, n);
    regTextoN += (uint32_t)n;
    reg[slot].usado = 1;
  }
  reg[slot].poster = poster ? 1 : 0;
  snprintf(reg[slot].imdb, sizeof reg[slot].imdb, "%.*s", (int)strcspn(imdb, ":"), imdb);
  pthread_mutex_unlock(&regTrava);
  return 1;
}

static int lerRegistro(const char *url, char *id, size_t nId, int *poster) {
  uint32_t h = hashUrl(url);
  size_t n = strlen(url);
  int achou, slot;
  int ok = 0;
  pthread_mutex_lock(&regTrava);
  slot = regSlot(url, n, h, &achou);
  if (slot >= 0 && achou) {
    snprintf(id, nId, "%s", reg[slot].imdb); *poster = reg[slot].poster; ok = 1;
  }
  pthread_mutex_unlock(&regTrava);
  return ok;
}

int arte_reserva_url(const char *url, char *saida, size_t tam) {
  char tipo[16], id[24], api[300], caminho[128] = "";
  const char *chave, *corpo, *v;
  char *resp;
  int poster, temp = 0, ep = 0;
  if (!url || !saida || tam < 80) return 0;
  chave = desc_chave_tmdb_reserva();
  if (!chave[0]) {
    // UMA VEZ: e o unico jeito de saber pelo log que a reserva existe e nao
    // pode agir (#67: "the fallback didn't help" sem uma linha para provar).
    static int avisou;
    if (!avisou) { avisou = 1; printf("[tex] reserva do TMDB indisponivel: sem chave do TMDB neste pacote\n"); fflush(stdout); }
    return 0;
  }
  if (lerStill(url, id, sizeof id, &temp, &ep)) return reservaStill(chave, id, temp, ep, saida, tam);
  if (lerMetahub(url, tipo, sizeof tipo, id, sizeof id)) poster = !strcmp(tipo, "poster");
  else if (lerRegistro(url, id, sizeof id, &poster)) snprintf(tipo, sizeof tipo, "%s", poster ? "poster" : "background");
  else return 0;
  snprintf(api, sizeof api,
           "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
           id, chave);
  // 8 s, como a imagem: isto roda no fio de decode e ja e o segundo pedido de
  // uma arte que acabou de falhar.
  resp = rede_baixar(api, 8);
  if (!resp) return 0;
  corpo = resp;
  // O /find responde os dois vetores sempre; o cheio diz o tipo do titulo, que
  // a URL do metahub nao carrega.
  v = js_array(corpo, NULL, "movie_results");
  if (!v) v = js_array(corpo, NULL, "tv_results");
  if (v) js_texto(v, js_fim(v), poster ? "poster_path" : "backdrop_path",
                  caminho, sizeof caminho);
  free(resp);
  if (caminho[0] != '/') return 0;
  // w342 e o que a fileira desenha (o card de 288 promove ate 2x sem perder);
  // w1280 cobre o heroi e o detalhe pelo mesmo decode escalado do card.
  snprintf(saida, tam, "https://image.tmdb.org/t/p/%s%s", poster ? "w342" : "w1280", caminho);
  printf("[tex] reserva do TMDB para %s de %s\n", tipo, id);
  fflush(stdout);
  return 1;
}

// PRIMEIRA URL DE UMA LISTA DE IMAGENS do Trakt ("fanart":["media.trakt.tv/..."]).
// Copia curta do imagemTrakt de trakt.c, que e static e vive num modulo que
// este nao pode arrastar para o teste sem rede. MEDIDO em 22/09 com curl em
// /search/imdb/tt0111161?extended=full,images: o bloco vem dentro de
// movie/show, a url vem SEM esquema e em /medium/ (.jpg.webp — webp.c le).
static int fanartTrakt(const char *corpo, char *dst, size_t n) {
  const char *p = strstr(corpo, "\"fanart\""), *ini;
  size_t L;
  if (!p) return 0;
  p = strchr(p + 8, '[');
  if (!p) return 0;
  while (*++p && (unsigned char)*p <= ' ') { }
  if (*p != '"') return 0;               // lista vazia: nao inventar arte
  ini = ++p;
  while (*p && *p != '"') p++;
  L = (size_t)(p - ini);
  if (!L || L + 9 >= n) return 0;
  if (!strncmp(ini, "http", 4)) snprintf(dst, n, "%.*s", (int)L, ini);
  else                          snprintf(dst, n, "https://%.*s", (int)L, ini);
  return 1;
}

// MEMORIA DO RESOLVEDOR (22/09/2026). MEDIDO na C9: resolve_ms de 408 a
// 640 ms em 12% dos downloads — cada url virtual pagava a consulta ao TMDB ou
// ao Trakt, em serie e dentro do fio de rede, mesmo quando o MESMO titulo ja
// tinha sido resolvido minutos antes (card w1280 e destaque original sao a
// mesma foto; a arte que saiu da RAM e do disco volta a ser baixada). Guarda
// aqui o que a consulta respondeu:
//
//   chave = "<fonte>/<ttNNN>", SEM o tamanho: w1280 e original do TMDB sao o
//           mesmo backdrop_path, medium e full do Trakt a mesma fanart. So
//           fonte e id do IMDb, nunca chave de API nem cabecalho.
//   valor = o backdrop_path (TMDB) ou a url medium da fanart (Trakt); o
//           tamanho pedido e aplicado na saida, como antes.
//   negativa = a API RESPONDEU e nao ha fundo: vale ARF_NEG_MS e expira, porque
//           o TMDB ganha arte com o tempo. Falha de rede e falta de chave nao
//           sao guardadas — sao da conexao/do pacote, nao do titulo.
//
// Tabela FIXA de ARF_N entradas no BSS (~115 KiB), sem malloc: cheia, sai a
// usada ha mais tempo (LRU por contador). A busca linear e sob mutex porque os
// dois fios de rede do tex_cache resolvem ao mesmo tempo; 192 strcmp nao
// aparecem perto de uma ida ao TMDB.
// 384 e nao 192 (23/09): um titulo do TMDB agora ocupa ate tres celulas (o id
// do TMDB, o padrao e o outro backdrop), mais Apple/fanart/anime.
#define ARF_N 384u
#define ARF_CHAVE 32u
#define ARF_VALOR 256u
#define ARF_NEG_MS (5u * 60u * 1000u)
typedef struct {
  char chave[ARF_CHAVE];
  char valor[ARF_VALOR];    // vazio = negativa
  unsigned long long vence; // so para negativa; 0 = positiva, nao vence
  unsigned long uso;        // 0 = livre
} ArteFonteMem;
static ArteFonteMem arfMem[ARF_N];
static unsigned long arfUso;
static pthread_mutex_t arfTrava = PTHREAD_MUTEX_INITIALIZER;

static unsigned long long relogioMonotonico(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return (unsigned long long)t.tv_sec * 1000ull + (unsigned long long)(t.tv_nsec / 1000000);
}
static unsigned long long (*arfRelogio)(void) = relogioMonotonico;

void arte_fonte_cache_limpar(void) {
  pthread_mutex_lock(&arfTrava);
  memset(arfMem, 0, sizeof arfMem);
  arfUso = 0;
  pthread_mutex_unlock(&arfTrava);
}

void arte_fonte_cache_relogio(unsigned long long (*ms)(void)) {
  arfRelogio = ms ? ms : relogioMonotonico;
}

// 1 = positiva (valor copiado), -1 = negativa em vigor, 0 = nao sabe.
static int arfLer(const char *chave, char *valor, size_t n) {
  unsigned i;
  int r = 0;
  unsigned long long agora = arfRelogio();
  pthread_mutex_lock(&arfTrava);
  for (i = 0; i < ARF_N; i++) {
    ArteFonteMem *m = &arfMem[i];
    if (!m->uso || strcmp(m->chave, chave)) continue;
    if (!m->valor[0]) {
      if (agora >= m->vence) { m->uso = 0; break; }   // negativa vencida: pergunta de novo
      r = -1;
    } else {
      snprintf(valor, n, "%s", m->valor);
      r = 1;
    }
    m->uso = ++arfUso;
    break;
  }
  pthread_mutex_unlock(&arfTrava);
  return r;
}

// `valor` NULL/vazio = negativa. Valor maior que a celula nao e guardado (a
// consulta so se repete; truncar daria url errada).
static void arfGravar(const char *chave, const char *valor) {
  unsigned i, alvo = 0;
  unsigned long menor = (unsigned long)-1;
  if (valor && strlen(valor) >= ARF_VALOR) return;
  pthread_mutex_lock(&arfTrava);
  for (i = 0; i < ARF_N; i++) {
    ArteFonteMem *m = &arfMem[i];
    if (m->uso && !strcmp(m->chave, chave)) { alvo = i; break; }
    if (m->uso < menor) { menor = m->uso; alvo = i; }  // livre (0) ganha de tudo
  }
  { ArteFonteMem *m = &arfMem[alvo];
    snprintf(m->chave, sizeof m->chave, "%s", chave);
    snprintf(m->valor, sizeof m->valor, "%s", valor ? valor : "");
    m->vence = (valor && valor[0]) ? 0 : arfRelogio() + ARF_NEG_MS;
    m->uso = ++arfUso; }
  pthread_mutex_unlock(&arfTrava);
}

// O PEDIDO DE UMA URL VIRTUAL, ja lido:
//   <fonte>/<tamanho>/<id>[/<resto>]
// `resto` depende da fonte (artereserva.h): o id do TMDB ("m278"/"t1399") no
// tmdb/tmdbalt e fanart, e tipo/ano/titulo na apple e no anime.
typedef struct {
  char fonte[10], tamanho[12], id[40];
  char tipoTmdb;          // 'm' filme, 't' serie, 0 = nao veio
  long tmdbId;
  int serie, ano;
  char titulo[200];
} ArfPedido;

static int (*arfApple)(const char *imdb, const char *titulo, int ano, int serie,
                       char *modelo, size_t n);
void arte_fonte_definir_apple(int (*buscar)(const char *imdb, const char *titulo,
                                            int ano, int serie, char *modelo, size_t n)) {
  arfApple = buscar;
}

// A CHAVE PESSOAL DO FANART.TV. Copiada sob trava porque os fios de rede leem
// enquanto Ajustes pode estar gravando. Nunca vai para log: quem imprime a url
// da API passa por rede_url_publica, e aqui nada imprime a chave.
static char arfFanart[80];
static pthread_mutex_t arfFanartTrava = PTHREAD_MUTEX_INITIALIZER;
void arte_fonte_chave_fanart(const char *chave) {
  pthread_mutex_lock(&arfFanartTrava);
  snprintf(arfFanart, sizeof arfFanart, "%s", chave ? chave : "");
  pthread_mutex_unlock(&arfFanartTrava);
}
static int fanartChave(char *dst, size_t n) {
  pthread_mutex_lock(&arfFanartTrava);
  snprintf(dst, n, "%s", arfFanart);
  pthread_mutex_unlock(&arfFanartTrava);
  return dst[0] != 0;
}

static int idValido(const char *fonte, const char *id) {
  if (!strncmp(id, "tt", 2) && id[2] >= '0' && id[2] <= '9') return 1;
  if (strcmp(fonte, "anime")) return 0;
  return !strncmp(id, "kitsu:", 6) || !strncmp(id, "mal:", 4) || !strncmp(id, "anilist:", 8);
}

// 1 = lido; 0 = nao e virtual; -1 = virtual malformada.
static int lerPedido(const char *url, ArfPedido *p) {
  static const char PRE[] = ARTE_VIRTUAL_PREFIXO;
  const char *s, *b;
  size_t n;
  if (!url || strncmp(url, PRE, sizeof PRE - 1)) return 0;
  memset(p, 0, sizeof *p);
  s = url + sizeof PRE - 1;
  // fonte
  b = strchr(s, '/'); if (!b || (n = (size_t)(b - s)) == 0 || n >= sizeof p->fonte) return -1;
  memcpy(p->fonte, s, n); s = b + 1;
  b = strchr(s, '/'); if (!b || (n = (size_t)(b - s)) == 0 || n >= sizeof p->tamanho) return -1;
  memcpy(p->tamanho, s, n); s = b + 1;
  b = strchr(s, '/'); n = b ? (size_t)(b - s) : strlen(s);
  if (!n || n >= sizeof p->id) return -1;
  memcpy(p->id, s, n);
  s = b ? b + 1 : s + n;
  if (!idValido(p->fonte, p->id)) return -1;
  if (!strcmp(p->fonte, "tmdb") || !strcmp(p->fonte, "tmdbalt") || !strcmp(p->fonte, "fanart")) {
    // fanart leva m|s/<id do tmdb>; tmdb leva m<id>|t<id>.
    if (!strcmp(p->fonte, "fanart")) {
      if (*s == 'm' || *s == 's') { p->serie = *s == 's'; s++; if (*s == '/') s++; }
      if (*s >= '0' && *s <= '9') { p->tmdbId = atol(s); p->tipoTmdb = p->serie ? 't' : 'm'; }
    } else if ((*s == 'm' || *s == 't') && s[1] >= '0' && s[1] <= '9') {
      p->tipoTmdb = *s; p->tmdbId = atol(s + 1); p->serie = *s == 't';
    }
  } else if (!strcmp(p->fonte, "apple") || !strcmp(p->fonte, "anime")) {
    if (*s == 'm' || *s == 's') { p->serie = *s == 's'; s++; }
    if (*s == '/') s++;
    p->ano = atoi(s);
    b = strchr(s, '/');
    if (b) af_decodificar(b + 1, p->titulo, sizeof p->titulo);
    if (!strcmp(p->fonte, "apple") && (!p->titulo[0] || p->ano <= 0)) return -1;
  }
#ifdef __EMSCRIPTEN__
  // Samsung: nunca o fundo de 3840 (fundoOriginal() em artehero.c). A url
  // virtual ja nasce pequena la; isto e a segunda trava, para uma url gravada
  // por outra build ou montada por outro caminho.
  if (!strcmp(p->tamanho, "original")) snprintf(p->tamanho, sizeof p->tamanho, "w1280");
  if (!strcmp(p->tamanho, "full") && !strcmp(p->fonte, "trakt"))
    snprintf(p->tamanho, sizeof p->tamanho, "medium");
#endif
  if (!strcmp(p->fonte, "tmdb") || !strcmp(p->fonte, "tmdbalt")) {
    if (strcmp(p->tamanho, "w780") && strcmp(p->tamanho, "w1280") && strcmp(p->tamanho, "original")) return -1;
  } else if (!strcmp(p->fonte, "trakt")) {
    if (strcmp(p->tamanho, "medium") && strcmp(p->tamanho, "full")) return -1;
  } else if (!strcmp(p->fonte, "apple")) {
    if (strcmp(p->tamanho, "1920") && strcmp(p->tamanho, "1280")) return -1;
  } else if (!strcmp(p->fonte, "fanart") || !strcmp(p->fonte, "anime")) {
    // Um tamanho so: fanart.tv serve o 1920x1080 e a faixa do Kitsu e a large.
  } else return -1;
  return 1;
}

// TMDB. `alt` 0 = o backdrop_path padrao, 1 = OUTRO backdrop (/images).
//
// COM O ID DO TMDB (o Cinemeta manda `moviedb_id` no catalogo: 49 de 49
// filmes e 49 de 50 series do topo em 23/09, e o id bate com o do /find nos
// 29 conferidos) nao ha /find: UM pedido,
// /{movie|tv}/{id}?append_to_response=images&include_image_language=null,en,
// responde o padrao e o outro de uma vez e enche as duas celulas da memoria.
// Sem o id, o /find de sempre (e ele ja traz o padrao); o outro precisa de
// mais um /images.
static int consultarTmdb(ArfPedido *p, int alt, char *valor, size_t n) {
  char api[400], chaveMem[ARF_CHAVE], padrao[160] = "", outro[160] = "", tid[24];
  const char *chave = desc_chave_tmdb_reserva(), *v;
  char *resp;
  char tipo = p->tipoTmdb;
  long id = p->tmdbId;
  if (!chave[0]) return -2;
  snprintf(chaveMem, sizeof chaveMem, "tmdbid/%s", p->id);
  if (id <= 0 && arfLer(chaveMem, tid, sizeof tid) > 0 && (tid[0] == 'm' || tid[0] == 't')) {
    tipo = tid[0]; id = atol(tid + 1);
  }
  if (id <= 0) {
    long achado = 0;
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
             p->id, chave);
    resp = rede_baixar(api, 8);
    if (!resp) return -2;
    v = js_array(resp, NULL, "movie_results");
    tipo = 'm';
    if (!v) { v = js_array(resp, NULL, "tv_results"); tipo = 't'; }
    if (v) {
      js_texto(v, js_fim(v), "backdrop_path", padrao, sizeof padrao);
      achado = (long)js_num(v, js_fim(v), "id", 0.0);
    }
    free(resp);
    if (achado > 0) {
      snprintf(tid, sizeof tid, "%c%ld", tipo, achado);
      arfGravar(chaveMem, tid);
      id = achado;
    }
    snprintf(chaveMem, sizeof chaveMem, "tmdb/%s", p->id);
    arfGravar(chaveMem, padrao[0] == '/' ? padrao : NULL);
    if (!alt) {
      if (padrao[0] != '/') return -1;
      snprintf(valor, n, "%s", padrao);
      return 1;
    }
    if (id <= 0) {
      // O TMDB nao conhece o titulo: a negativa vale para o outro tambem
      // (arte_fonte_resolver nao grava as celulas do tmdb por conta propria).
      snprintf(chaveMem, sizeof chaveMem, "tmdbalt/%s", p->id);
      arfGravar(chaveMem, NULL);
      return -1;
    }
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/%s/%ld/images?include_image_language=null,en&api_key=%s",
             tipo == 't' ? "tv" : "movie", id, chave);
  } else {
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/%s/%ld?append_to_response=images&include_image_language=null,en&api_key=%s",
             tipo == 't' ? "tv" : "movie", id, chave);
  }
  resp = rede_baixar(api, 8);
  if (!resp) return -2;
  // O outro exclui o padrao que o /find deu, mesmo quando so veio o /images
  // (sem backdrop_path na raiz).
  { char padraoResp[160];
    af_tmdb_fundos(resp, padrao, padraoResp, sizeof padraoResp, outro, sizeof outro);
    if (padraoResp[0] == '/') snprintf(padrao, sizeof padrao, "%s", padraoResp); }
  free(resp);
  if (padrao[0] == '/') {
    snprintf(chaveMem, sizeof chaveMem, "tmdb/%s", p->id);
    arfGravar(chaveMem, padrao);
  }
  snprintf(chaveMem, sizeof chaveMem, "tmdbalt/%s", p->id);
  arfGravar(chaveMem, outro[0] == '/' ? outro : NULL);
  { const char *r = alt ? outro : padrao;
    if (r[0] != '/') return -1;
    snprintf(valor, n, "%s", r); }
  return 1;
}

static int consultarTrakt(ArfPedido *p, char *valor, size_t n) {
  // Chave PUBLICA do aplicativo, sem token: a busca por id nao precisa de
  // conta vinculada, e o fundo do Trakt nao pode depender de login.
  const char *cab[4];
  char chave[160], api[200];
  char *resp;
  int ok;
  if (!trakt_cabecalhos_publicos(cab, chave, sizeof chave)) return -2;
  snprintf(api, sizeof api,
           "https://api.trakt.tv/search/imdb/%s?type=movie,show&extended=full,images", p->id);
  resp = rede_baixar_com(api, 8, cab);
  if (!resp) return -2;
  ok = fanartTrakt(resp, valor, n);
  free(resp);
  return ok ? 1 : -1;
}

// fanart.tv: filme pelo id do IMDb direto; serie so pelo id do TheTVDB, que
// o TMDB da em /tv/{id}/external_ids (e o /find, quando o id do TMDB nao veio).
static int consultarFanart(ArfPedido *p, char *valor, size_t n) {
  char chave[80], api[400];
  char *resp;
  int ok;
  if (!fanartChave(chave, sizeof chave)) return -2;
  if (!p->serie) {
    snprintf(api, sizeof api, "https://webservice.fanart.tv/v3/movies/%s?api_key=%s", p->id, chave);
  } else {
    const char *ct = desc_chave_tmdb_reserva();
    long tvdb = 0, id = p->tmdbId;
    char chaveMem[ARF_CHAVE], tid[24];
    if (!ct[0]) return -2;
    snprintf(chaveMem, sizeof chaveMem, "tmdbid/%s", p->id);
    if (id <= 0 && arfLer(chaveMem, tid, sizeof tid) > 0 && tid[0] == 't') id = atol(tid + 1);
    if (id <= 0) {
      const char *v;
      snprintf(api, sizeof api,
               "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id", p->id, ct);
      resp = rede_baixar(api, 8);
      if (!resp) return -2;
      v = js_array(resp, NULL, "tv_results");
      if (v) id = (long)js_num(v, js_fim(v), "id", 0.0);
      free(resp);
      if (id <= 0) return -1;
      snprintf(tid, sizeof tid, "t%ld", id);
      arfGravar(chaveMem, tid);
    }
    snprintf(api, sizeof api, "https://api.themoviedb.org/3/tv/%ld/external_ids?api_key=%s", id, ct);
    resp = rede_baixar(api, 8);
    if (!resp) return -2;
    tvdb = (long)js_num(resp, NULL, "tvdb_id", 0.0);
    free(resp);
    if (tvdb <= 0) return -1;
    snprintf(api, sizeof api, "https://webservice.fanart.tv/v3/tv/%ld?api_key=%s", tvdb, chave);
  }
  resp = rede_baixar(api, 8);
  // 404 = o fanart.tv nao tem o titulo: rede_baixar devolve NULL sem dizer.
  // Tratar como "nao ha" (negativa de 5 min) e melhor que perguntar a cada
  // quadro; chave errada tambem cai aqui, e ela e trocada em Ajustes.
  if (!resp) return -1;
  ok = af_fanart_fundo(resp, p->serie, valor, n);
  free(resp);
  return ok ? 1 : -1;
}

// Anime: id proprio (kitsu:/mal:/anilist:, como os addons de anime mandam) ou
// busca pelo titulo+ano para um tt do Cinemeta. Kitsu primeiro (a faixa large
// tem 3360 px), AniList se o Kitsu nao tiver.
static int anilist(const char *consulta, int ano, char *valor, size_t n) {
  static const char *const CAB[] = { "Content-Type: application/json", "Accept: application/json", NULL };
  char *resp = rede_postar("https://graphql.anilist.co", 8, CAB, consulta);
  int ok;
  if (!resp) return -2;
  ok = af_anilist_banner(resp, ano, valor, n);
  free(resp);
  return ok ? 1 : -1;
}
static int consultarAnime(ArfPedido *p, char *valor, size_t n) {
  char api[600], enc[400];
  char *resp;
  int ok;
  if (!strncmp(p->id, "kitsu:", 6)) {
    snprintf(api, sizeof api,
             "https://kitsu.io/api/edge/anime/%ld?fields[anime]=coverImage,subtype,startDate",
             atol(p->id + 6));
    resp = rede_baixar(api, 8);
    if (!resp) return -2;
    ok = af_kitsu_capa(resp, 0, p->serie, valor, n);
    free(resp);
    return ok ? 1 : -1;
  }
  if (!strncmp(p->id, "mal:", 4) || !strncmp(p->id, "anilist:", 8)) {
    int mal = p->id[0] == 'm';
    snprintf(api, sizeof api,
             "{\"query\":\"query{Media(%s:%ld,type:ANIME){bannerImage startDate{year}}}\"}",
             mal ? "idMal" : "id", atol(strchr(p->id, ':') + 1));
    return anilist(api, 0, valor, n);
  }
  if (!p->titulo[0]) return -1;
  af_codificar(p->titulo, enc, sizeof enc);
  snprintf(api, sizeof api,
           "https://kitsu.io/api/edge/anime?filter[text]=%s&page[limit]=5"
           "&fields[anime]=coverImage,subtype,startDate", enc);
  resp = rede_baixar(api, 8);
  if (resp) {
    ok = af_kitsu_capa(resp, p->ano, p->serie, valor, n);
    free(resp);
    if (ok) return 1;
  }
  { char tit[200]; size_t i, k = 0;
    // O titulo vai DENTRO de uma string JSON: aspas e barra invertida saem.
    for (i = 0; p->titulo[i] && k + 1 < sizeof tit; i++)
      if (p->titulo[i] != '"' && p->titulo[i] != '\\') tit[k++] = p->titulo[i];
    tit[k] = 0;
    snprintf(api, sizeof api,
             "{\"query\":\"query($s:String){Media(search:$s,type:ANIME){bannerImage startDate{year}}}\","
             "\"variables\":{\"s\":\"%s\"}}", tit); }
  return anilist(api, p->ano, valor, n);
}

static int consultarApple(ArfPedido *p, char *valor, size_t n) {
  int r;
  if (!arfApple) return -2;
  r = arfApple(p->id, p->titulo, p->ano, p->serie, valor, n);
  return r > 0 ? 1 : r == 0 ? -1 : -2;
}

// A viagem de verdade. 1 = `valor` preenchido; -1 = a API respondeu sem
// fundo (vira negativa); -2 = nao deu para perguntar (sem chave, rede).
static int arfConsultar(ArfPedido *p, char *valor, size_t n) {
  if (!strcmp(p->fonte, "tmdb"))    return consultarTmdb(p, 0, valor, n);
  if (!strcmp(p->fonte, "tmdbalt")) return consultarTmdb(p, 1, valor, n);
  if (!strcmp(p->fonte, "trakt"))   return consultarTrakt(p, valor, n);
  if (!strcmp(p->fonte, "apple"))   return consultarApple(p, valor, n);
  if (!strcmp(p->fonte, "fanart"))  return consultarFanart(p, valor, n);
  if (!strcmp(p->fonte, "anime"))   return consultarAnime(p, valor, n);
  return -2;
}

// Da resposta guardada a url que se baixa, no tamanho pedido.
static int montarFinal(const ArfPedido *p, const char *valor, char *saida, size_t tam) {
  if (!strcmp(p->fonte, "tmdb") || !strcmp(p->fonte, "tmdbalt")) {
    snprintf(saida, tam, "https://image.tmdb.org/t/p/%s%s", p->tamanho, valor);
    return 1;
  }
  if (!strcmp(p->fonte, "apple"))
    return af_apple_tamanho(valor, atoi(p->tamanho), saida, tam);
  if (!strcmp(p->fonte, "trakt")) {
    const char *m = strstr(valor, "/medium/");
    if (m && !strcmp(p->tamanho, "full") && strlen(valor) + 2 < tam)
      snprintf(saida, tam, "%.*s/full/%s", (int)(m - valor), valor, m + 8);
    else snprintf(saida, tam, "%s", valor);
    return 1;
  }
  snprintf(saida, tam, "%s", valor);
  return 1;
}

int arte_fonte_resolver(const char *url, char *saida, size_t tam) {
  ArfPedido p;
  char chave[ARF_CHAVE], valor[400];
  int r = lerPedido(url, &p);
  if (r == 0) return 0;
  if (r < 0 || !saida || tam < 80) return -1;
  snprintf(chave, sizeof chave, "%s/%s", p.fonte, p.id);
  r = arfLer(chave, valor, sizeof valor);
  if (r < 0) return -1;
  if (r == 0) {
    r = arfConsultar(&p, valor, sizeof valor);
    if (r == -2) return -1;
    // A tmdb ja grava as duas celulas (padrao e outro) por conta propria.
    if (strcmp(p.fonte, "tmdb") && strcmp(p.fonte, "tmdbalt")) arfGravar(chave, r > 0 ? valor : NULL);
    if (r < 0) return -1;
    printf("[tex] fundo %s de %s: %.90s\n", p.fonte, p.id, valor);
    fflush(stdout);
  }
  return montarFinal(&p, valor, saida, tam) ? 1 : -1;
}

int arte_fonte_resolvida(const char *url, char *saida, size_t tam) {
  ArfPedido p;
  char chave[ARF_CHAVE], valor[400];
  if (lerPedido(url, &p) != 1 || !saida || tam < 80) return 0;
  snprintf(chave, sizeof chave, "%s/%s", p.fonte, p.id);
  if (arfLer(chave, valor, sizeof valor) <= 0) return 0;
  return montarFinal(&p, valor, saida, tam);
}

// ASSINATURA DOS BYTES BAIXADOS: url -> (FNV-1a 64 do corpo, tamanho). Serve
// para "a fonte escolhida devolveu a MESMA imagem do card" quando as urls
// diferem mas o arquivo e o mesmo (o catalogo do Cinemeta e o metahub pelo id
// sao o MESMO arquivo, 1763947 bytes nos dois no relatorio 1669). NAO pega a
// mesma foto reencodada (metahub x TMDB, ver af_tmdb_fundos): isso so com a
// imagem decodificada. Tabela fixa com LRU, como a memoria do resolvedor.
#define ARB_N 256u
typedef struct {
  uint32_t hUrl;
  uint64_t hBytes;
  long n;
  unsigned long uso;
} ArteBytes;
static ArteBytes arb[ARB_N];
static unsigned long arbUso;
static pthread_mutex_t arbTrava = PTHREAD_MUTEX_INITIALIZER;

// So os primeiros 256 KiB entram no hash (o tamanho inteiro e comparado a
// parte): um fundo de 1,7 MB custaria alguns ms de CPU da TV a cada download,
// e dois JPEGs diferentes com o mesmo tamanho e os mesmos 256 KiB iniciais
// nao aparecem na pratica.
uint64_t arte_bytes_hash(const void *b, long n) {
  const unsigned char *p = (const unsigned char *)b;
  uint64_t h = 1469598103934665603ull;
  long i, lim = n < 262144L ? n : 262144L;
  for (i = 0; p && i < lim; i++) { h ^= p[i]; h *= 1099511628211ull; }
  return h;
}

void arte_bytes_registrar(const char *url, const void *b, long n) {
  uint32_t h;
  unsigned i, alvo = 0;
  unsigned long menor = (unsigned long)-1;
  uint64_t hb;
  if (!url || !url[0] || !b || n <= 0) return;
  h = hashUrl(url);
  hb = arte_bytes_hash(b, n);
  pthread_mutex_lock(&arbTrava);
  for (i = 0; i < ARB_N; i++) {
    if (arb[i].uso && arb[i].hUrl == h) { alvo = i; break; }
    if (arb[i].uso < menor) { menor = arb[i].uso; alvo = i; }
  }
  arb[alvo].hUrl = h; arb[alvo].hBytes = hb; arb[alvo].n = n; arb[alvo].uso = ++arbUso;
  pthread_mutex_unlock(&arbTrava);
}

static int arbLer(const char *url, uint64_t *hb, long *n) {
  uint32_t h = hashUrl(url);
  unsigned i;
  int ok = 0;
  pthread_mutex_lock(&arbTrava);
  for (i = 0; i < ARB_N; i++)
    if (arb[i].uso && arb[i].hUrl == h) { *hb = arb[i].hBytes; *n = arb[i].n; ok = 1; break; }
  pthread_mutex_unlock(&arbTrava);
  return ok;
}

int arte_mesma_imagem(const char *a, const char *b) {
  char ra[600], rb[600];
  uint64_t ha, hb;
  long na, nb;
  if (!a || !b || !a[0] || !b[0]) return 0;
  if (arte_fonte_resolvida(a, ra, sizeof ra)) a = ra;
  if (arte_fonte_resolvida(b, rb, sizeof rb)) b = rb;
  if (!strcmp(a, b)) return 1;
  if (arbLer(a, &ha, &na) && arbLer(b, &hb, &nb)) return na == nb && ha == hb;
  return 0;
}
