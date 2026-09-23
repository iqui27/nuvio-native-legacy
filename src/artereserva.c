#include "artereserva.h"
#include <pthread.h>
#include "descoberta.h"
#include "rede.h"
#include "js.h"
#include "trakt.h"
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
// Tabela FIXA de ARF_N entradas no BSS (~60 KiB), sem malloc: cheia, sai a
// usada ha mais tempo (LRU por contador). A busca linear e sob mutex porque os
// dois fios de rede do tex_cache resolvem ao mesmo tempo; 192 strcmp nao
// aparecem perto de uma ida ao TMDB.
#define ARF_N 192u
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

// A viagem de verdade. 1 = `valor` preenchido; -1 = a API respondeu sem
// fundo (vira negativa); -2 = nao deu para perguntar (sem chave, rede).
static int arfConsultar(const char *fonte, const char *id, char *valor, size_t n) {
  if (!strcmp(fonte, "tmdb")) {
    char api[300], caminho[128] = "";
    const char *chave = desc_chave_tmdb_reserva(), *v;
    char *resp;
    if (!chave[0]) return -2;
    snprintf(api, sizeof api,
             "https://api.themoviedb.org/3/find/%s?api_key=%s&external_source=imdb_id",
             id, chave);
    resp = rede_baixar(api, 8);
    if (!resp) return -2;
    v = js_array(resp, NULL, "movie_results");
    if (!v) v = js_array(resp, NULL, "tv_results");
    if (v) js_texto(v, js_fim(v), "backdrop_path", caminho, sizeof caminho);
    free(resp);
    if (caminho[0] != '/') return -1;
    snprintf(valor, n, "%s", caminho);
    return 1;
  } else {
    // Chave PUBLICA do aplicativo, sem token: a busca por id nao precisa de
    // conta vinculada, e o fundo do Trakt nao pode depender de login.
    const char *cab[4];
    char chave[160], api[200];
    char *resp;
    int ok;
    if (!trakt_cabecalhos_publicos(cab, chave, sizeof chave)) return -2;
    snprintf(api, sizeof api,
             "https://api.trakt.tv/search/imdb/%s?type=movie,show&extended=full,images", id);
    resp = rede_baixar_com(api, 8, cab);
    if (!resp) return -2;
    ok = fanartTrakt(resp, valor, n);
    free(resp);
    return ok ? 1 : -1;
  }
}

int arte_fonte_resolver(const char *url, char *saida, size_t tam) {
  static const char PRE[] = ARTE_VIRTUAL_PREFIXO;
  char fonte[8], tamanho[12], id[24], chave[ARF_CHAVE], valor[400];
  const char *p;
  int r, tmdb;
  if (!url || strncmp(url, PRE, sizeof PRE - 1)) return 0;
  if (!saida || tam < 80) return -1;
  p = url + sizeof PRE - 1;
  if (sscanf(p, "%7[^/]/%11[^/]/%23[^/]", fonte, tamanho, id) != 3 ||
      strncmp(id, "tt", 2)) return -1;
#ifdef __EMSCRIPTEN__
  // Samsung: nunca o fundo de 3840 (fundoOriginal() em artehero.c). A url
  // virtual ja nasce pequena la; isto e a segunda trava, para uma url gravada
  // por outra build ou montada por outro caminho.
  if (!strcmp(tamanho, "original")) snprintf(tamanho, sizeof tamanho, "w1280");
  if (!strcmp(tamanho, "full"))     snprintf(tamanho, sizeof tamanho, "medium");
#endif
  tmdb = !strcmp(fonte, "tmdb");
  if (tmdb) { if (strcmp(tamanho, "w1280") && strcmp(tamanho, "original")) return -1; }
  else if (!strcmp(fonte, "trakt")) { if (strcmp(tamanho, "medium") && strcmp(tamanho, "full")) return -1; }
  else return -1;
  snprintf(chave, sizeof chave, "%s/%s", fonte, id);
  r = arfLer(chave, valor, sizeof valor);
  if (r < 0) return -1;
  if (r == 0) {
    r = arfConsultar(fonte, id, valor, sizeof valor);
    if (r == -2) return -1;
    arfGravar(chave, r > 0 ? valor : NULL);
    if (r < 0) return -1;
    printf("[tex] fundo %s de %s: %.90s\n", fonte, id, valor);
    fflush(stdout);
  }
  if (tmdb) snprintf(saida, tam, "https://image.tmdb.org/t/p/%s%s", tamanho, valor);
  else {
    char *m = strstr(valor, "/medium/");
    if (m && !strcmp(tamanho, "full") && strlen(valor) + 2 < tam)
      snprintf(saida, tam, "%.*s/full/%s", (int)(m - valor), valor, m + 8);
    else snprintf(saida, tam, "%s", valor);
  }
  return 1;
}
