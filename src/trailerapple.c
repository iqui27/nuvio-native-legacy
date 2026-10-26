#include "trailerapple.h"
// O .tpk toca pelo mesmo servidor de midia do .wgt (muse-server), e o master
// HLS da Apple o trava ("[DEADLOCK] ... 90 second", ver varianteMidia): la
// vale o caminho da Samsung, a playlist de UMA variante, e nao o reduzido em
// file:// da LG (o player roda em outro processo, fora da pasta do app).
#if defined(__EMSCRIPTEN__) || defined(NV_TPK)
#define NV_TRAILER_SAMSUNG 1
#endif
// ANDROID: NV_TRAILER_SAMSUNG fica DESLIGADO de proposito. O ExoPlayer toca HLS
// master, mas o ramo da LG (reduzido de UMA variante em <dados>/trailer, entregue
// como file://) nao depende de nada do webOS: so de dados_caminho e de
// rede_baixar_com. E o que se quer aqui: variante fixa, sem ABR trocando o
// tamanho do quadro no meio e invalidando o recorte. NAO PROVADO no aparelho
// (o video_android precisa aceitar file:// de HLS).
#include "rede.h"
#include "js.h"
#include "dados.h"
#include "ajustes.h"
#include "extras.h"
#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#define TA_MAX 32
typedef struct {
  char imdb[48];      // IMDb + requested language; isolates in-flight and disk caches
  char audio[16];     // actual audio verified in the HLS master
  char url[1024];       // master da Apple (ABR completo)
  char toca[600];       // LG: file:// do master reduzido a UMA variante; Samsung: URL da playlist de midia dela (varianteMidia); ou vazio
  int  tocaQual;        // teto de qualidade com que `toca` foi montado
  long expira;
  int  emVoo, respondeu;
} Entrada;
static Entrada tab[TA_MAX];
static int nTab, prox;
static SDL_mutex *mtx;

#define UTS_HOST "https://tv.apple.com/api/uts/v3"
#define UTS_QUERY "caller=web&sf=143441&v=76&pfm=appletv&locale=en-US&l=en&utsk=6e3013c6d6fae3c2::::::235656c069bb0efb"
// Storefronts used by Apple's public TV catalog. Unknown locales keep the
// existing US fallback; a localized page alone is not proof of dubbed audio.
typedef struct { const char *iso, *locale; int sf; } Regiao;
static const Regiao REGIOES[] = {
  { "en", "en-US", 143441 }, { "de", "de-DE", 143443 },
  { "fr", "fr-FR", 143442 }, { "es", "es-ES", 143454 },
  { "it", "it-IT", 143450 }, { "pt", "pt-BR", 143503 },
  { "pt-PT", "pt-PT", 143453 }, { "tr", "tr-TR", 143480 },
  { "nl", "nl-NL", 143452 }, { "ja", "ja-JP", 143462 },
  { "ko", "ko-KR", 143466 }, { "zh", "zh-CN", 143465 },
  { "zh-TW", "zh-TW", 143470 }, { "pl", "pl-PL", 143478 },
  { "sv", "sv-SE", 143456 }, { "da", "da-DK", 143458 },
  { "no", "nb-NO", 143457 }, { "nb", "nb-NO", 143457 },
  { "fi", "fi-FI", 143447 }, { "cs", "cs-CZ", 143489 },
  { "sk", "sk-SK", 143496 }, { "hu", "hu-HU", 143482 },
  { "ro", "ro-RO", 143487 }, { "el", "el-GR", 143448 },
  { "ru", "ru-RU", 143469 }, { "uk", "uk-UA", 143492 },
  { "ar", "ar-SA", 143479 }, { "hi", "hi-IN", 143467 },
  { "id", "id-ID", 143476 }, { "vi", "vi-VN", 143471 },
  { "th", "th-TH", 143475 }, { "he", "he-IL", 143491 }
};
static const Regiao *regiaoDe(const char *idioma) {
  unsigned i;
  for (i = 0; i < sizeof REGIOES / sizeof *REGIOES; i++)
    if (!strcasecmp(idioma, REGIOES[i].iso)) return &REGIOES[i];
  for (i = 0; i < sizeof REGIOES / sizeof *REGIOES; i++)
    if (!strncasecmp(idioma, REGIOES[i].iso, 2)) return &REGIOES[i];
  return &REGIOES[0];
}
static void consultaIdioma(const char *idioma, char *dst, unsigned cap) {
  const Regiao *r = regiaoDe(idioma);
  snprintf(dst, cap, "caller=web&sf=%d&v=76&pfm=appletv&locale=%s&l=%.2s&utsk=6e3013c6d6fae3c2::::::235656c069bb0efb",
           r->sf, r->locale, r->locale);
}
static void chaveIdioma(const char *imdb, char *chave, unsigned cap, char *idioma, unsigned n) {
  char pref[16];
  extras_trailer_audio_idioma(pref, sizeof pref);
  if (!strcmp(pref, "~")) snprintf(idioma, n, "~");
  else {
    const Regiao *r = regiaoDe(pref);
    // Preserve an unsupported request in diagnostics/cache identity even
    // though its catalog lookup can only use the existing English fallback.
    if (strncasecmp(pref, "en", 2) && !strncmp(r->locale, "en", 2)) {
      unsigned k = 0;
      while (pref[k] && k + 1 < n &&
             ((pref[k] >= 'a' && pref[k] <= 'z') || (pref[k] >= 'A' && pref[k] <= 'Z') || pref[k] == '-')) {
        idioma[k] = pref[k]; k++;
      }
      idioma[k] = 0;
      if (!k) snprintf(idioma, n, "en-US");
    } else snprintf(idioma, n, "%s", r->locale);
  }
  snprintf(chave, cap, "%s%s%s", imdb, !strcmp(idioma, "en-US") ? "" : "-",
           !strcmp(idioma, "en-US") ? "" : !strcmp(idioma, "~") ? "original" : idioma);
}
static const char *const CABS[] = { "Accept: application/json", "Origin: https://tv.apple.com", NULL };

static void trancar(void)   { if (!mtx) mtx = SDL_CreateMutex(); SDL_LockMutex(mtx); }
static void destrancar(void){ SDL_UnlockMutex(mtx); }

static Entrada *achar(const char *imdb) {
  int i;
  for (i = 0; i < nTab; i++) if (!strcmp(tab[i].imdb, imdb)) return &tab[i];
  return NULL;
}
static Entrada *reservar(const char *imdb) {
  Entrada *e = achar(imdb);
  if (e) return e;
  if (nTab < TA_MAX) e = &tab[nTab++];
  else { e = &tab[prox]; prox = (prox + 1) % TA_MAX; }
  memset(e, 0, sizeof *e);
  snprintf(e->imdb, sizeof e->imdb, "%s", imdb);
  return e;
}

static void caminhoDisco(const char *imdb, char *dst, unsigned tam) {
  char pasta[512];
  dst[0] = 0;
  if (!dados_caminho(pasta, sizeof pasta, "trailer")) return;
  mkdir(pasta, 0755);
  snprintf(dst, tam, "%s/%s-apple", pasta, imdb);
}
static int lerDisco(Entrada *e) {
  char c[600], linha[1200];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "r"))) return 0;
  if (!fgets(linha, sizeof linha, f)) { fclose(f); return 0; }
  e->expira = atol(linha);
  e->url[0] = 0;
  if (fgets(linha, sizeof linha, f)) { linha[strcspn(linha, "\r\n")] = 0; if (strcmp(linha, "-")) snprintf(e->url, sizeof e->url, "%.1023s", linha); }
  e->toca[0] = 0;
  if (fgets(linha, sizeof linha, f)) { linha[strcspn(linha, "\r\n")] = 0; if (strcmp(linha, "-")) snprintf(e->toca, sizeof e->toca, "%.599s", linha); }
  e->tocaQual = -1;
  snprintf(e->audio, sizeof e->audio, "en");
  if (fgets(linha, sizeof linha, f)) { linha[strcspn(linha, "\r\n")] = 0; snprintf(e->audio, sizeof e->audio, "%.15s", linha); }
  fclose(f);
  return 1;
}
static void gravarDisco(const Entrada *e) {
  char c[600];
  FILE *f;
  caminhoDisco(e->imdb, c, sizeof c);
  if (!c[0] || !(f = fopen(c, "w"))) return;
  // Terceira linha: a variante de midia da Samsung ("-" na LG, que monta o
  // reduzido do master em disco). Cache antigo sem ela e refeito (ver
  // trailerapple_pedir).
  fprintf(f, "%ld\n%s\n%s\n%s\n", e->expira, e->url[0] ? e->url : "-", e->toca[0] && strncmp(e->toca, "file://", 7) ? e->toca : "-", e->audio);
  fclose(f);
  dados_marcar_sujo(1);   // IDBFS (Samsung): cache re-obtivel, descarga leve
}
static int valido(const Entrada *e) { return e->respondeu && e->expira > (long)time(NULL); }

static void caminhoMaster(const char *imdb, char *dst, unsigned tam, const char *sufixo) {
  char pasta[512];
  dst[0] = 0;
  if (!dados_caminho(pasta, sizeof pasta, "trailer")) return;
  snprintf(dst, tam, "%s/%s-apple%s", pasta, imdb, sufixo);
}

// UMA VARIANTE, NAO O ABR. Medido na C9 (20/09/2026): entregue o master
// inteiro, o uMS comeca pela variante mais baixa (556x232) e sobe aos poucos —
// e cada troca muda o tamanho do quadro, o que invalida o recorte de fonte
// calculado no videoInfo anterior. Um master reduzido a uma variante (com o
// grupo de audio dela) toca fixo na definicao escolhida desde o primeiro
// quadro. Escolha: a maior largura que cabe no teto dos Ajustes, sem Dolby
// Vision (dvh1/dvhe: trailer mudo no fundo nao e lugar para trocar o modo da
// TV), avc1 preferido a hvc1 na mesma largura (decodifica em qualquer LG).
static int atributo(const char *linha, const char *nome, char *dst, unsigned tam) {
  const char *p = linha;
  size_t n = strlen(nome);
  unsigned k = 0;
  while ((p = strstr(p, nome)) != NULL) {
    if ((p == linha || p[-1] == ',' || p[-1] == ':') && p[n] == '=') {
      p += n + 1;
      if (*p == '"') { p++; while (*p && *p != '"' && k + 1 < tam) dst[k++] = *p++; }
      else while (*p && *p != ',' && *p != '\r' && *p != '\n' && k + 1 < tam) dst[k++] = *p++;
      dst[k] = 0;
      return 1;
    }
    p += n;
  }
  dst[0] = 0;
  return 0;
}

#ifndef NV_TRAILER_SAMSUNG   /* so a LG monta o reduzido em disco */
static int montarReduzido(const char *imdb, int teto, const char *audio, char *saida, unsigned tam) {
  char cm[600], cr[600], linha[2048];
  char melhorInf[2048] = "", melhorUri[1024] = "", melhorAudio[128] = "";
  int melhorW = 0, melhorAvc = 0;
  FILE *f, *g;
  char grupos[32][128];
  int nGrupos = 0;
#ifdef NV_ANDROID
  // ANDROID: "Máxima" vira 1080p. Na TCL Smart TV Pro (Mali-G52) a variante
  // 3840px hvc1 travou a interface 8 s e depois 20 s ao abrir o trailer de
  // fundo (log de 30/09: clr=19535 ms). O trailer toca atras do texto da
  // pagina; 1080p ja enche a tela.
  if (!teto || teto > 1080) teto = 1080;
#endif
  caminhoMaster(imdb, cm, sizeof cm, ".m3u8");
  caminhoMaster(imdb, cr, sizeof cr, "-play.m3u8");
  if (!cm[0] || !(f = fopen(cm, "r"))) return 0;
  // Only video variants whose audio group contains the selected language.
  // A master may expose German audio but attach the first/widest video to
  // an English-only group; dropping that group's audio would make it silent.
  while (audio[0] && fgets(linha, sizeof linha, f)) {
    char lg[16], grp[128];
    if (strncmp(linha, "#EXT-X-MEDIA:", 13) || !strstr(linha, "TYPE=AUDIO")) continue;
    atributo(linha, "LANGUAGE", lg, sizeof lg);
    atributo(linha, "GROUP-ID", grp, sizeof grp);
    if (grp[0] && !strncasecmp(lg, audio, 2) && nGrupos < 32)
      snprintf(grupos[nGrupos++], sizeof grupos[0], "%s", grp);
  }
  rewind(f);
  // Passo 1: escolher a variante.
  while (fgets(linha, sizeof linha, f)) {
    char res[32], cod[128], aud[128], uri[1024];
    int w = 0, avc;
    if (strncmp(linha, "#EXT-X-STREAM-INF:", 18)) continue;
    atributo(linha, "RESOLUTION", res, sizeof res);
    atributo(linha, "CODECS", cod, sizeof cod);
    atributo(linha, "AUDIO", aud, sizeof aud);
    if (nGrupos) {
      int k;
      for (k = 0; k < nGrupos && strcmp(aud, grupos[k]); k++) {}
      if (k == nGrupos) continue;
    }
    w = atoi(res);
    if (w <= 0) continue;
    if (strstr(cod, "dvh1") || strstr(cod, "dvhe")) continue;
    if (teto && w > (teto >= 1080 ? 1920 : teto >= 720 ? 1280 : 864)) continue;
    avc = strstr(cod, "avc1") != NULL;
    if (!fgets(uri, sizeof uri, f)) break;
    uri[strcspn(uri, "\r\n")] = 0;
    if (uri[0] == '#' || strncmp(uri, "http", 4)) continue;
    if (w > melhorW || (w == melhorW && avc && !melhorAvc)) {
      melhorW = w; melhorAvc = avc;
      snprintf(melhorInf, sizeof melhorInf, "%s", linha);
      snprintf(melhorUri, sizeof melhorUri, "%s", uri);
      snprintf(melhorAudio, sizeof melhorAudio, "%s", aud);
    }
  }
  if (!melhorW) { fclose(f); return 0; }
  // Passo 2: escrever o reduzido com o grupo de audio da variante.
  g = fopen(cr, "w");
  if (!g) { fclose(f); return 0; }
  fprintf(g, "#EXTM3U\n#EXT-X-VERSION:6\n#EXT-X-INDEPENDENT-SEGMENTS\n");
  rewind(f);
  while (fgets(linha, sizeof linha, f)) {
    char grp[128], lg[16];
    if (strncmp(linha, "#EXT-X-MEDIA:", 13)) continue;
    if (!strstr(linha, "TYPE=AUDIO")) continue;
    atributo(linha, "GROUP-ID", grp, sizeof grp);
    atributo(linha, "LANGUAGE", lg, sizeof lg);
    if (melhorAudio[0] && !strcmp(grp, melhorAudio) &&
        (!audio[0] || !lg[0] || !strncasecmp(audio, lg, 2))) { fputs(linha, g); break; }
  }
  fputs(melhorInf, g);
  fprintf(g, "%s\n", melhorUri);
  fclose(g); fclose(f);
  printf("[trailer] apple %s: variante %dpx %s (teto %d)\n", imdb, melhorW, melhorAvc ? "avc1" : "hvc1", teto);
  fflush(stdout);
  snprintf(saida, tam, "file://%s", cr);
  return 1;
}
#endif  /* !NV_TRAILER_SAMSUNG */

#ifdef NV_TRAILER_SAMSUNG
// SAMSUNG: NUNCA O MASTER NO <video>. Provado no emulador Tizen 10 (winpc,
// 22/09/2026, klog do muse-server): entregue o master da Apple (93 a 165
// #EXT-X-STREAM-INF, tres "pathways" de CDN, content steering), o motor HLS
// da Samsung (STREAMING_ENGINE, dentro do muse-server — o servidor de midia
// do SISTEMA, nao do app) as vezes monta a tabela de variantes errada
// ("total streams are = 4" para o mesmo master que noutra vez deu 7) e, na
// primeira troca de ABR ("Bitrate Change: 0 -> 3"), pede a variante 3 com uma
// URL de lixo de memoria (".../MZPlayLocal.woa/hls/%A5%DE%96..."). Dali em
// diante repete o pedido a cada 10 ms para sempre; o fechamento do elemento
// fica preso nele e o muse-server acusa "[DEADLOCK] player ... does not return
// value within (90) second" e se reinicia. Enquanto isso NENHUM <video> toca:
// e o "tocou um trailer e depois nenhum toca mais" do dono, e o silencio total
// do log 1646 (nem erro, nem playing).
//
// O remedio e o mesmo do reduzido da LG, por outro caminho: a playlist de
// MIDIA de uma unica variante (so video, avc1, na largura do teto). Sem
// master nao ha tabela, nem troca de ABR, nem a URL corrompida. Medido no
// mesmo emulador, quatro titulos seguidos criando e destruindo o elemento:
// `playing` em 1,1 a 3,2 s, na definicao escolhida desde o primeiro quadro
// (o master levava 10,5 s e comecava em 556x232). O preco: sem audio — a
// Apple entrega o audio em playlist separada, e um <audio> ao lado do <video>
// travou o video no emulador (dois players de uma vez). No fundo o trailer e
// mudo, e a tela cheia tambem (dono, 22/09/2026: "trailer fica mudo" — nada
// troca para o YouTube por causa de som; ver trailerfonte.h).
//
// Escolha: igual a montarReduzido (maior largura que cabe no teto, sem Dolby
// Vision), mas avc1 OBRIGATORIO quando existe: decodifica em qualquer Samsung
// e e o que o emulador provou. hvc1 so quando o titulo nao tem avc1. As URIs
// do master sao absolutas; relativa e descartada (resolver contra o master
// seria refazer justamente o que o motor da Samsung erra).
static int grupoTemAudio(const char *master, const char *grupo, const char *audio) {
  const char *p = master;
  while ((p = strstr(p, "#EXT-X-MEDIA:"))) {
    char linha[2048], tipo[24], grp[128], lg[16];
    size_t n = strcspn(p, "\r\n");
    if (n >= sizeof linha) n = sizeof linha - 1;
    memcpy(linha, p, n); linha[n] = 0;
    atributo(linha, "TYPE", tipo, sizeof tipo);
    atributo(linha, "GROUP-ID", grp, sizeof grp);
    atributo(linha, "LANGUAGE", lg, sizeof lg);
    if (!strcmp(tipo, "AUDIO") && !strcmp(grp, grupo) && !strncasecmp(lg, audio, 2)) return 1;
    p += n ? n : 1;
  }
  return 0;
}

static int varianteMidia(const char *master, int teto, const char *audio, char *dst, unsigned tam) {
  const char *p = master;
  char melhorUri[1024] = "";
  int melhorW = 0, melhorAvc = 0;
  if (!master) return 0;
  while ((p = strstr(p, "#EXT-X-STREAM-INF:")) != NULL) {
    char linha[2048], res[32], cod[128], uri[1024], aud[128];
    const char *fimLinha = strchr(p, '\n'), *u, *fimU;
    size_t n;
    int w, avc;
    if (!fimLinha) break;
    n = (size_t)(fimLinha - p);
    if (n >= sizeof linha) n = sizeof linha - 1;
    memcpy(linha, p, n); linha[n] = 0;
    u = fimLinha + 1;
    fimU = strchr(u, '\n');
    n = fimU ? (size_t)(fimU - u) : strlen(u);
    if (n >= sizeof uri) n = sizeof uri - 1;
    memcpy(uri, u, n); uri[n] = 0;
    uri[strcspn(uri, "\r")] = 0;
    p = fimLinha;
    atributo(linha, "RESOLUTION", res, sizeof res);
    atributo(linha, "CODECS", cod, sizeof cod);
    atributo(linha, "AUDIO", aud, sizeof aud);
    if (audio[0] && aud[0] && !grupoTemAudio(master, aud, audio)) continue;
    w = atoi(res);
    if (w <= 0 || strncmp(uri, "http", 4)) continue;
    if (strstr(cod, "dvh1") || strstr(cod, "dvhe")) continue;
    if (teto && w > (teto >= 1080 ? 1920 : teto >= 720 ? 1280 : 864)) continue;
    avc = strstr(cod, "avc1") != NULL;
    if ((avc && !melhorAvc) || (avc == melhorAvc && w > melhorW)) {
      melhorW = w; melhorAvc = avc;
      snprintf(melhorUri, sizeof melhorUri, "%s", uri);
    }
  }
  if (!melhorW) return 0;
  snprintf(dst, tam, "%s", melhorUri);
  printf("[trailer] apple: variante de midia %dpx %s (teto %d), sem master\n", melhorW, melhorAvc ? "avc1" : "hvc1", teto);
  fflush(stdout);
  return 1;
}
#endif

// NORMALIZACAO igual a do tvOS: minusculas, sem acento (so o bloco latino de
// dois bytes, que e o que titulo de filme tem), "&" vira " and ", tudo que
// nao e letra ou digito vira espaco, espacos colapsados. Artigos FICAM: "The
// Batman" e "Batman" sao filmes diferentes.
static void normalizar(const char *s, char *dst, unsigned tam) {
  static const char *const acent = "ÀÁÂÃÄÅàáâãäåÈÉÊËèéêëÌÍÎÏìíîïÒÓÔÕÖØòóôõöøÙÚÛÜùúûüÝýÿÑñÇç";
  static const char *const base  = "AAAAAAaaaaaaEEEEeeeeIIIIiiiiOOOOOOooooooUUUUuuuuYyyNnCc";
  unsigned k = 0;
  int espaco = 1;
  const unsigned char *p = (const unsigned char *)s;
  while (*p && k + 2 < tam) {
    int c = -1;
    if (*p == '&') { c = ' '; /* " and " abaixo */
      if (!espaco && k + 1 < tam) dst[k++] = ' ';
      if (k + 4 < tam) { memcpy(dst + k, "and ", 4); k += 4; }
      espaco = 1; p++; continue; }
    if (*p < 0x80) { c = *p++; }
    else if ((*p & 0xE0) == 0xC0 && p[1]) {
      const char *q = acent; int i = 0; c = ' ';
      while (*q) { if ((unsigned char)q[0] == p[0] && (unsigned char)q[1] == p[1]) { c = base[i]; break; } q += 2; i++; }
      p += 2;
    } else { c = ' '; while ((*p & 0xC0) == 0x80 || *p >= 0xC0) { p++; if (!*p) break; if ((*p & 0xC0) != 0x80) break; } }
    if (isalnum(c)) { dst[k++] = (char)tolower(c); espaco = 0; }
    else if (!espaco) { dst[k++] = ' '; espaco = 1; }
  }
  while (k && dst[k - 1] == ' ') k--;
  dst[k] = 0;
}

static int anoDe(const char *meta) {
  const char *p = meta;
  while (p && *p) {
    if (isdigit((unsigned char)p[0]) && isdigit((unsigned char)p[1]) && isdigit((unsigned char)p[2]) && isdigit((unsigned char)p[3]) &&
        !isdigit((unsigned char)p[4])) { int a = atoi(p); if (a > 1880 && a < 2100) return a; }
    p++;
  }
  return 0;
}

static void urlEncode(const char *s, char *dst, unsigned tam) {
  static const char hex[] = "0123456789ABCDEF";
  unsigned k = 0;
  for (; *s && k + 4 < tam; s++) {
    unsigned char c = (unsigned char)*s;
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') dst[k++] = (char)c;
    else { dst[k++] = '%'; dst[k++] = hex[c >> 4]; dst[k++] = hex[c & 15]; }
  }
  dst[k] = 0;
}

static int anoDeEpoca(double ms) {
  time_t t = (time_t)(ms / 1000.0);
  struct tm tmv;
  if (ms <= 0) return 0;
  gmtime_r(&t, &tmv);
  return tmv.tm_year + 1900;
}

// A CHAVE NO NIVEL DE CIMA do objeto, e nao a primeira ocorrencia: um item da
// busca traz genres[0].type="Genre" e genres[0].id ANTES do type/id dele
// proprio, e js_texto (que acha a primeira) devolvia o genero. Anda pelo
// objeto pulando cada valor aninhado inteiro. Devolve o ponteiro para a chave
// (para js_texto/js_num lerem dali) ou NULL.
static const char *chaveTopo(const char *obj, const char *fim, const char *chave) {
  const char *p = obj;
  size_t n = strlen(chave);
  if (!p || *p != '{') return NULL;
  p++;
  while (p < fim && *p) {
    const char *k;
    while (p < fim && (*p == ' ' || *p == ',' || *p == '\n' || *p == '\r' || *p == '\t')) p++;
    if (p >= fim || *p == '}') return NULL;
    if (*p != '"') return NULL;
    k = p + 1;
    p = k;
    while (p < fim && *p && *p != '"') { if (*p == '\\') p++; p++; }
    if (p >= fim) return NULL;
    if ((size_t)(p - k) == n && !strncmp(k, chave, n)) return k - 1;
    p++;
    while (p < fim && (*p == ' ' || *p == ':')) p++;
    if (*p == '{' || *p == '[') p = js_fim(p);
    else if (*p == '"') { p++; while (p < fim && *p && *p != '"') { if (*p == '\\') p++; p++; } if (p < fim) p++; }
    else while (p < fim && *p && *p != ',' && *p != '}') p++;
  }
  return NULL;
}

// A ARTE QUE A MESMA BUSCA JA TRAZ (23/09/2026). Cada item da busca vem com
// images.shelfImageBackground: a arte-chave do titulo, 3840x2160, SEM o nome
// escrito (conferido em Dune: Part Two — duas figuras contra o por do sol,
// nada de letreiro), servida pelo mzstatic em qualquer tamanho pelo modelo
// "{w}x{h}.{f}". MEDIDO na C9: 1920x1080.jpg = 300 KB em 0,25-0,29 s com
// conexao nova (o mzstatic esta a 30 ms; o TMDB a 150). E a fonte "Apple TV"
// do destaque: nenhum pedido a mais quando o trailer ja buscou o titulo.
static void arteDoItem(const char *it, const char *ie, char *arte, unsigned tam) {
  const char *b = strstr(it, "\"shelfImageBackground\"");
  if (!arte || !tam) return;
  arte[0] = 0;
  if (!b || b > ie) return;
  if (!js_texto(b, ie, "url", arte, tam) || !strstr(arte, "{w}x{h}") ||
      strncmp(arte, "https://", 8)) arte[0] = 0;
}

// Busca: um unico id com tipo certo, titulo normalizado igual e ano +-1.
// Dois candidatos distintos = nenhum (remake do mesmo ano e o caso em que
// adivinhar erra). Sem rede: `corpo` e a resposta de /search.
static int escolherNaBusca(const char *corpo, const char *titulo, int ano, int serie,
                           char *id, unsigned tam, char *arte, unsigned tamArte) {
  char norm[256], achado[80] = "", arteAchada[600] = "";
  const char *fim, *shelves, *sh;
  int n = 0, anoAchado = 0;
  if (arte && tamArte) arte[0] = 0;
  normalizar(titulo, norm, sizeof norm);
  if (!norm[0] || !corpo) return 0;
  fim = corpo + strlen(corpo);
  shelves = js_array(corpo, fim, "shelves");
  for (sh = shelves; sh; sh = js_prox(js_fim(sh))) {
    const char *she = js_fim(sh), *it;
    for (it = js_array(sh, she, "items"); it; it = js_prox(js_fim(it))) {
      const char *ie = js_fim(it);
      char tipo[16] = "", iid[80], tit[256], tn[256];
      const char *kt = chaveTopo(it, ie, "type"), *ki = chaveTopo(it, ie, "id"),
                 *kn = chaveTopo(it, ie, "title"), *kd = chaveTopo(it, ie, "releaseDate");
      int a;
      if (kt) js_texto(kt, ie, "type", tipo, sizeof tipo);
      if (strcmp(tipo, serie ? "Show" : "Movie")) continue;
      if (!ki || !kn || !js_texto(ki, ie, "id", iid, sizeof iid) || !js_texto(kn, ie, "title", tit, sizeof tit)) continue;
      normalizar(tit, tn, sizeof tn);
      if (strcmp(tn, norm)) continue;
      a = kd ? anoDeEpoca(js_num(kd, ie, "releaseDate", 0)) : 0;
      if (!a || abs(a - ano) > 1) continue;
      if (achado[0] && strcmp(achado, iid)) { n = 2; break; }
      if (!achado[0]) {
        snprintf(achado, sizeof achado, "%s", iid); n = 1; anoAchado = a;
        arteDoItem(it, ie, arteAchada, sizeof arteAchada);
      }
    }
    if (n > 1) break;
  }
  printf("[arte-apple] busca titulo=%.160s ano=%d tipo=%s candidatos=%d id=%s ano_encontrado=%d arte=%d\n",
         titulo, ano, serie ? "Show" : "Movie", n,
         n == 1 ? achado : "-", n == 1 ? anoAchado : 0, n == 1 && arteAchada[0] != 0);
  if (n != 1) return 0;
  snprintf(id, tam, "%s", achado);
  if (arte && tamArte) snprintf(arte, tamArte, "%s", arteAchada);
  return 1;
}

static int buscarIdCom(const char *titulo, int ano, int serie, char *id, unsigned tam,
                    char *arte, unsigned tamArte, const char *query) {
  char norm[256], enc[768], url[1100];
  char *corpo;
  int r;
  normalizar(titulo, norm, sizeof norm);
  if (!norm[0]) return 0;
  urlEncode(titulo, enc, sizeof enc);
  snprintf(url, sizeof url, UTS_HOST "/search?%s&searchTerm=%s", query, enc);
  corpo = rede_baixar_com(url, 12, CABS);
  if (!corpo) return -1;
  r = escolherNaBusca(corpo, titulo, ano, serie, id, tam, arte, tamArte);
  free(corpo);
  return r;
}

static int buscarId(const char *titulo, int ano, int serie, char *id, unsigned tam,
                    char *arte, unsigned tamArte) {
  return buscarIdCom(titulo, ano, serie, id, tam, arte, tamArte, UTS_QUERY);
}

// ARTE POR IMDb + ano + tipo, inclusive a semeada pelo trailer: uma busca
// com tipo corrigido nao pode contaminar pedidos de arte do tipo original.
#define TA_ARTE_MAX 64
typedef struct { char imdb[16]; char modelo[600]; int sabida, ano, serie; } ArteApple;
static ArteApple tabArte[TA_ARTE_MAX];
static int proxArte;
// Trava PROPRIA e estatica: trailerapple_arte roda nos fios de rede do
// tex_cache, que podem chegar antes de qualquer trailerapple_pedir — e o
// SDL_mutex de trancar() nasce preguicoso, sem protecao contra dois fios.
static pthread_mutex_t arteTrava = PTHREAD_MUTEX_INITIALIZER;
static void guardarArte(const char *imdb, int ano, int serie, const char *modelo) {
  int i;
  ArteApple *a = NULL;
  for (i = 0; i < TA_ARTE_MAX; i++) if (tabArte[i].sabida && tabArte[i].ano == ano && tabArte[i].serie == serie && !strcmp(tabArte[i].imdb, imdb)) { a = &tabArte[i]; break; }
  if (!a) { a = &tabArte[proxArte]; proxArte = (proxArte + 1) % TA_ARTE_MAX; }
  snprintf(a->imdb, sizeof a->imdb, "%s", imdb);
  snprintf(a->modelo, sizeof a->modelo, "%s", modelo ? modelo : "");
  a->sabida = 1; a->ano = ano; a->serie = serie;
}

int trailerapple_arte(const char *imdb, const char *titulo, int ano, int serie,
                      char *modelo, size_t n) {
  char id[80], arte[600] = "";
  int i, r;
  if (!imdb || !imdb[0] || !titulo || !titulo[0] || ano <= 0 || !modelo || !n) return 0;
  pthread_mutex_lock(&arteTrava);
  for (i = 0; i < TA_ARTE_MAX; i++)
    if (tabArte[i].sabida && tabArte[i].ano == ano && tabArte[i].serie == serie && !strcmp(tabArte[i].imdb, imdb)) {
      snprintf(modelo, n, "%s", tabArte[i].modelo);
      pthread_mutex_unlock(&arteTrava);
      return modelo[0] ? 1 : 0;
    }
  pthread_mutex_unlock(&arteTrava);
  r = buscarId(titulo, ano, serie, id, sizeof id, arte, sizeof arte);
  // Arte exige o tipo pedido; tentar o oposto pode aceitar um homonimo.
  if (r < 0) return -1;
  pthread_mutex_lock(&arteTrava);
  guardarArte(imdb, ano, serie, r > 0 ? arte : "");
  pthread_mutex_unlock(&arteTrava);
  snprintf(modelo, n, "%s", r > 0 ? arte : "");
  return modelo[0] ? 1 : 0;
}

// backgroundVideo primeiro (e o loop que a propria Apple toca atras do titulo
// e traz o 4K mais vezes), movieClips depois. canvas.shelves NAO: e "mais como
// este", trailer de OUTRO filme.
static const char *objetoTopo(const char *obj, const char *fim, const char *chave) {
  const char *p = obj ? chaveTopo(obj, fim, chave) : NULL;
  if (!p || !(p = strchr(p, ':'))) return NULL;
  do { p++; } while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t');
  return p < fim && *p == '{' ? p : NULL;
}

static int hlsDe(const char *id, int serie, const char *query, char *url, unsigned tam,
                 char *original, unsigned nOriginal) {
  char u[1100];
  char *corpo;
  const char *fim, *data, *content, *bg, *assets, *pl;
  int ok = 0;
  url[0] = 0;
  if (original && nOriginal) original[0] = 0;
  snprintf(u, sizeof u, UTS_HOST "/%s/%s?%s", serie ? "shows" : "movies", id, query);
  corpo = rede_baixar_com(u, 12, CABS);
  if (!corpo) return -1;
  fim = corpo + strlen(corpo);
  data = objetoTopo(corpo, fim, "data");
  content = data ? objetoTopo(data, js_fim(data), "content") : NULL;
  bg = content ? objetoTopo(content, js_fim(content), "backgroundVideo") : NULL;
  assets = bg ? objetoTopo(bg, js_fim(bg), "assets") : NULL;
  if (original && nOriginal) {
    const char *langs = content ? js_array(content, js_fim(content), "originalSpokenLanguages") : NULL;
    original[0] = 0;
    if (langs) js_texto(langs, js_fim(langs), "locale", original, nOriginal);
  }
  if (assets && js_texto(assets, js_fim(assets), "hlsUrl", url, tam) && url[0]) ok = 1;
  if (!ok) {
    pl = data ? objetoTopo(data, js_fim(data), "playables") : NULL;
    if (pl) { const char *mc = strstr(pl, "\"movieClips\"");
      if (mc && js_texto(mc, js_fim(pl), "hlsUrl", url, tam) && url[0]) ok = 1; }
  }
  free(corpo);
  return ok;
}

// Inspect the trailer's audio, not the movie's audioTrackLocales or the page
// language (a Turkish storefront can still supply an English trailer).
static int audioDoMaster(const char *master, const char *idioma, char *audio, unsigned tam) {
  const char *p = master;
  while (p && (p = strstr(p, "#EXT-X-MEDIA:"))) {
    char linha[2048], tipo[24], lg[16];
    size_t n = strcspn(p, "\r\n");
    if (n >= sizeof linha) n = sizeof linha - 1;
    memcpy(linha, p, n); linha[n] = 0;
    atributo(linha, "TYPE", tipo, sizeof tipo);
    atributo(linha, "LANGUAGE", lg, sizeof lg);
    if (!strcmp(tipo, "AUDIO") && lg[0] && !strncasecmp(lg, idioma, 2)) {
      snprintf(audio, tam, "%s", lg); return 1;
    }
    p += n ? n : 1;
  }
  return 0;
}

typedef struct { char imdb[16], chave[48], idioma[16], titulo[200], meta[96]; int serie; } Pedido;

static void *buscar(void *arg) {
  Pedido *p = arg;
  char id[80], url[1024] = "", query[240], audio[16] = "", alvo[16];
  char *master = NULL;
  int ano = anoDe(p->meta), r = 0, semResposta = 0, serie = p->serie;
  Entrada *e;
  consultaIdioma(p->idioma, query, sizeof query);
  snprintf(alvo, sizeof alvo, "%s", p->idioma);
  if (ano) {
    char arte[600] = "";
    // Search in the requested catalog so translated title names can match.
    r = buscarIdCom(p->titulo, ano, serie, id, sizeof id, arte, sizeof arte, query);
    if (r == 0 && strcmp(query, UTS_QUERY))
      r = buscarId(p->titulo, ano, serie, id, sizeof id, arte, sizeof arte);
    if (r == 0) {
      serie = !serie;
      r = buscarIdCom(p->titulo, ano, serie, id, sizeof id, arte, sizeof arte, query);
      if (r == 0 && strcmp(query, UTS_QUERY))
        r = buscarId(p->titulo, ano, serie, id, sizeof id, arte, sizeof arte);
    }
    if (r >= 0) { pthread_mutex_lock(&arteTrava); guardarArte(p->imdb, ano, serie, r > 0 ? arte : ""); pthread_mutex_unlock(&arteTrava); }
    if (r < 0) semResposta = 1;
    else if (r == 1) {
      char original[16] = "";
      r = hlsDe(id, serie, query, url, sizeof url, original, sizeof original);
      if (r < 0) semResposta = 1;
      if (!strcmp(alvo, "~")) {
        snprintf(alvo, sizeof alvo, "%s", original[0] ? original : "en");
        consultaIdioma(alvo, query, sizeof query);
        if (strncasecmp(alvo, "en", 2)) r = hlsDe(id, serie, query, url, sizeof url, NULL, 0);
      }
      if (url[0]) master = rede_baixar_com(url, 12, CABS);
      // Explicit English fallback. Never describe an English track as dubbed.
      if (master && !audioDoMaster(master, alvo, audio, sizeof audio) &&
          !audioDoMaster(master, "en", audio, sizeof audio)) { free(master); master = NULL; }
      if (!master && strcmp(query, UTS_QUERY)) {
        r = hlsDe(id, serie, UTS_QUERY, url, sizeof url, NULL, 0);
        if (url[0]) master = rede_baixar_com(url, 12, CABS);
        if (master) audioDoMaster(master, "en", audio, sizeof audio);
      }
      if (r < 0 || (url[0] && !master)) semResposta = 1;
    }
  }
#ifdef NV_TRAILER_SAMSUNG
  char midia[600] = "";
  if (master && !varianteMidia(master, ajustes_trailer_qualidade(), audio, midia, sizeof midia)) url[0] = 0;
#else
  if (master) {
    char cm[600];
    caminhoMaster(p->chave, cm, sizeof cm, ".m3u8");
    FILE *g = cm[0] ? fopen(cm, "w") : NULL;
    if (g) { fputs(master, g); fclose(g); } else url[0] = 0;
  }
#endif
  if (!master) url[0] = 0;
  free(master);
  printf("[trailer] apple %s (%s %d): pedido=%s audio=%s fallback=%d %s\n",
         p->imdb, p->titulo, ano, p->idioma, audio[0] ? audio : "unknown",
         audio[0] && strncasecmp(audio, alvo, 2) != 0,
         url[0] ? "HLS" : semResposta ? "sem resposta" : !ano ? "sem ano" : "sem trailer");
  fflush(stdout);
  trancar();
  e = reservar(p->chave);
  e->emVoo = 0; e->respondeu = 1;
  snprintf(e->url, sizeof e->url, "%s", url);
  snprintf(e->audio, sizeof e->audio, "%s", audio);
  e->toca[0] = 0; e->tocaQual = -1;
#ifdef NV_TRAILER_SAMSUNG
  if (url[0]) { snprintf(e->toca, sizeof e->toca, "%s", midia); e->tocaQual = ajustes_trailer_qualidade(); }
#endif
  e->expira = (long)time(NULL) + (url[0] ? semResposta ? 60 : 3600 : semResposta ? 0 : 12 * 3600);
  if (!semResposta) gravarDisco(e);
  destrancar();
  free(p);
  return NULL;
}

void trailerapple_pedir(const char *imdb, const char *titulo, const char *meta, int serie) {
  Entrada *e;
  Pedido *p;
  char chave[48], idioma[16];
  pthread_t f;
  if (!imdb || !imdb[0] || !titulo || !titulo[0]) return;
  chaveIdioma(imdb, chave, sizeof chave, idioma, sizeof idioma);
  trancar();
  e = reservar(chave);
  if (!e->respondeu && !e->emVoo && lerDisco(e)) {
    // Resposta do disco so vale com o master junto: sem ele nao ha reduzido,
    // e o master inteiro (ABR) nao serve — o quadro muda de tamanho no meio
    // e o recorte fica errado.
    char cm[600];
    struct stat st;
    caminhoMaster(chave, cm, sizeof cm, ".m3u8");
#ifdef NV_TRAILER_SAMSUNG
    // Cache de antes desta versao guarda so o master: sem variante de midia
    // ele nao serve (o master nao vai mais ao <video>), entao busca de novo.
    (void)st; (void)cm; e->respondeu = !e->url[0] || e->toca[0];
#else
    e->respondeu = !e->url[0] || (cm[0] && stat(cm, &st) == 0);
#endif
  }
  if (e->emVoo || valido(e)) { destrancar(); return; }
  e->emVoo = 1; e->respondeu = 0;
  destrancar();
  p = calloc(1, sizeof *p);
  if (!p) { trancar(); e = achar(chave); if (e) e->emVoo = 0; destrancar(); return; }
  snprintf(p->imdb, sizeof p->imdb, "%s", imdb);
  snprintf(p->chave, sizeof p->chave, "%s", chave);
  snprintf(p->idioma, sizeof p->idioma, "%s", idioma);
  snprintf(p->titulo, sizeof p->titulo, "%s", titulo);
  snprintf(p->meta, sizeof p->meta, "%s", meta ? meta : "");
  p->serie = serie;
  if (pthread_create(&f, NULL, buscar, p) == 0) pthread_detach(f);
  else { free(p); trancar(); e->emVoo = 0; destrancar(); }
}

const char *trailerapple_url(const char *imdb) {
  Entrada *e;
  const char *r = NULL;
  char chave[48], idioma[16];
  if (!imdb || !imdb[0]) return NULL;
  chaveIdioma(imdb, chave, sizeof chave, idioma, sizeof idioma);
  trancar();
  e = achar(chave);
  if (e && valido(e) && e->url[0]) {
#ifdef NV_TRAILER_SAMSUNG
    r = e->toca[0] ? e->toca : NULL;
#else
    int teto = ajustes_trailer_qualidade();
    if (e->tocaQual != teto) {
      e->tocaQual = teto;
      if (!montarReduzido(chave, teto, e->audio, e->toca, sizeof e->toca)) e->toca[0] = 0;
    }
    // Sem reduzido, sem Apple: quem chama cai no IMDb.
    r = e->toca[0] ? e->toca : NULL;
#endif
  }
  destrancar();
  return r;
}

int trailerapple_respondeu(const char *imdb) {
  Entrada *e;
  int r;
  char chave[48], idioma[16];
  if (!imdb || !imdb[0]) return 1;
  chaveIdioma(imdb, chave, sizeof chave, idioma, sizeof idioma);
  trancar();
  e = achar(chave);
  r = !e || e->respondeu;
  destrancar();
  return r;
}

int trailerapple_idioma_preferido(const char *imdb) {
  char chave[48], idioma[16];
  int r = 0;
  if (!imdb || !imdb[0]) return 0;
  chaveIdioma(imdb, chave, sizeof chave, idioma, sizeof idioma);
  if (!strncasecmp(idioma, "en", 2)) return 0;
  trancar();
  Entrada *e = achar(chave);
  if (e && !e->respondeu) r = -1;
  else if (e && valido(e) && e->url[0] && e->audio[0])
    r = !strcmp(idioma, "~") ? strncasecmp(e->audio, "en", 2) != 0 : !strncasecmp(e->audio, idioma, 2);
  destrancar();
  return r;
}
