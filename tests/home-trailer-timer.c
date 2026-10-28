// Drives the production home trailer state machine with deterministic platform
// boundaries. The state machine itself comes from src/home.c; only catalog,
// settings, network-result, and trailer-player edges are stubbed here.
#include "../src/home.h"
#include "../src/ajustes.h"
#include "../src/trailer.h"
#include "../src/trailerapple.h"
#include "../src/extras.h"
#include "../src/anim.h"
#include "../src/trailerfonte.h"
#include "../src/trailercinema.h"   // static inline: antes do `#define static`, senao vira inline sem corpo
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
static int gateLogs;
static int fixturePrintf(const char *format, ...);
#define printf fixturePrintf

#define static
#include "../src/home.c"
void desc_sinopse_hero(const int *idx, int n) { (void)idx; (void)n; }
#undef static
#undef printf
static int fixturePrintf(const char *format, ...) {
  if (!strcmp(format,"[home-trailer] autoplay gate=%s\n")) gateLogs++;
  va_list args;va_start(args,format);int result=vprintf(format,args);va_end(args);return result;
}
// A espera virou ajuste (ajustes_trailer_hero_espera_ms); o stub devolve o
// padrao de fabrica, 2,2 s, e a janela da Apple conta a partir dela.
#define NV_TRAILER_HERO_ESPERA_MS 2200
#define NV_TRAILER_HERO_MAX_ESPERA_MS (NV_TRAILER_HERO_ESPERA_MS + NV_TRAILER_HERO_JANELA_MS)

static CatItem item;
static int trailerSetting = 1;
static int heroEnabled = 1;
static int posterSetting;
// "Fonte do trailer" (trailerfonte.h). A regra e a de producao
// (src/trailerfonte.c entra na linha do emcc); so o valor gravado e do teste.
static int fonteSetting = TRF_AUTO;
static int lastSom = -1;
static int appleReady;
static int appleOpenFails;
static int imdbOpenFails;
static int layoutSetting;
static int youtubeReady;
static int imdbReady, imdbAnswered;
static int opened;
static int playing;
static int openedCount;
static int sourceRequests;
static int appleFailure;
static char lastSource[128];

int cat_n(void) { return 1; }
const CatItem *cat_item(int i) { return i == 0 ? &item : NULL; }

int ajustes_social(void) { return 1; }
int ajustes_hist_conta(void) { return 1; }
int ajustes_hero_ligado(void) { return heroEnabled; }
int ajustes_trailer_hero(void) { return trailerSetting; }
int ajustes_trailer_hero_vetado(void) { return 0; }
int video_registro_negado(void) { return 0; }
static int somSetting;
int ajustes_trailer_som(void) { return somSetting; }
Uint32 ajustes_trailer_hero_espera_ms(void) { return NV_TRAILER_HERO_ESPERA_MS; }
int ajustes_tmdb_trailers(void) { return 1; }
int ajustes_home_layout(void) { return layoutSetting; }
int ajustes_trailer_cartaz(void) { return posterSetting; }
float ajustes_expandir_poster_atraso(void) { return 0.5f; }
int ajustes_animacoes_reduzidas(void) { return 0; }
int ajustes_trailer_fonte(void) { return fonteSetting; }

int trailer_suportado(void) { return 1; }
int trailer_aberto(void) { return opened; }
int trailer_cheia(void) { return 0; }
int trailer_tocando(void) { return opened && playing; }
static int geometryReady = 1, asyncGeometry;
int trailer_mostra_video(void) { return geometryReady; }
static int donoTrailer;
void trailer_marcar_dono(int dono, const char *imdb) { (void)imdb; donoTrailer = dono; }
int trailer_dono(void) { return opened ? donoTrailer : 0; }
int trailer_falhou(void) {
  int r = appleFailure;
  appleFailure = 0;
  return r;
}
void trailer_fechar(void) { opened = 0; playing = 0; }
int trailer_estado(void) { return opened ? (playing ? 1 : -1) : -2; }
static GfxRect lastRect;
#ifdef NV_TPK
// cartaoVideoRect (LetterBox no cartao): quadro ainda sem tamanho = 16:9.
int video_largura(void) { return 0; }
int video_altura(void) { return 0; }
// Scheduling fixture: the video backend accepts the card destination unchanged.
void video_tpk_cartao_destino(float x,float y,float w,float h,float *ox,float *oy,float *ow,float *oh) {
  if (ox) *ox=x; if (oy) *oy=y; if (ow) *ow=w; if (oh) *oh=h;
}
#endif
static int lastNoCartao, rectCalls;
void trailer_no_cartao(int sim) { lastNoCartao = sim; }
void trailer_rect(GfxRect r) { lastRect = r; rectCalls++; if (asyncGeometry) geometryReady = 0; }
GfxRect trailer_retangulo(void) { return lastRect; }
float gfx_escala_txt_entrar(void) { return 1.0f; }
void gfx_escala_sair(float anterior) { (void)anterior; }
static int fraco;
int memlog_android_fraco(void) { return fraco; }
void trailer_abrir(const char *source, GfxRect r, int som, int cheia) {
  (void)cheia; lastRect = r;
  lastSom = som;
  snprintf(lastSource, sizeof lastSource, "%s", source);
  openedCount++;
  if ((appleOpenFails && strstr(source,"apple")) || (imdbOpenFails&&strstr(source,"imdb"))) {
    appleFailure = 1;
    opened = 0;
    playing = 0;
    return;
  }
  opened = 1;
  playing = 0;
}

void trailerapple_pedir(const char *imdb, const char *titulo, const char *meta, int serie) {
  (void)imdb; (void)titulo; (void)meta; (void)serie;
  sourceRequests++;
}
const char *trailerapple_url(const char *imdb) {
  (void)imdb;
  return appleReady ? "https://media.test/apple.m3u8" : NULL;
}
int trailerapple_respondeu(const char *imdb) { (void)imdb; return appleReady; }

// IMDb (#136): na Samsung ele existe quando a build tem o servico de
// recomendacoes; aqui trailerfonte_definir_imdb_tizen decide.
void trailerimdb_pedir(const char *imdb) { (void)imdb; sourceRequests++; }
const char *trailerimdb_url(const char *imdb, const char **nome) {
  (void)imdb; if (nome) *nome = "Trailer";
  return imdbReady ? "https://media.test/imdb.mp4" : NULL;
}
int trailerimdb_respondeu(const char *imdb) { (void)imdb; return imdbReady || imdbAnswered; }

void extras_hero_trailer_pedir(const char *imdb, int serie, long tmdbId) {
  (void)imdb; (void)serie; (void)tmdbId;
  sourceRequests++;
}
int extras_hero_trailer_obter(const char *imdb, char *dst, unsigned cap) {
  (void)imdb;
  if (!youtubeReady || !dst || cap < 12) return 0;
  snprintf(dst, cap, "%s", "dQw4w9WgXcQ");
  return 1;
}

// Trailer audio (Ajustes): -1 TMDB pending, 1 its first trailer is the chosen language.
static int youtubeIdioma;
int extras_hero_trailer_idioma_preferido(const char *imdb) { (void)imdb; return youtubeIdioma; }

static int check(const char *name, int ok) {
  if (!ok) { fprintf(stderr, "FALHOU: %s\n", name); return 1; }
  printf("ok %s\n", name);
  return 0;
}

static void resetState(const char *id) {
  memset(&item, 0, sizeof item);
  snprintf(item.imdb, sizeof item.imdb, "%s", id);
  snprintf(item.titulo, sizeof item.titulo, "Fixture");
  snprintf(item.meta, sizeof item.meta, "2026");
  snprintf(item.tipo, sizeof item.tipo, "movie");
  heroAtual = 0;
  heroDesejado = -1;
  heroEntra = 1.0f;
  heroSai = 0.0f;
  heroDesliza = 1.0f;
  focoHero = 1;
  foco.fileira = -1;
  heroTrailerItem = -1;
  heroTrailerImdb[0] = 0;
  heroTrailerDesde = 0;
  heroTrailerTentado = 0;
  heroTrailerPreparandoAte = 0;
  heroTrailerFonte = 0;
  heroTrailerAppleFalhou = 0;
  heroTrailerFalhas = 0;heroTrailerQual = 0;
  heroTrailerFade = 0.0f;
  heroTrailerTocouN = 0;   // cada caso reinicia a memoria da sessao no teste
  heroTrailerMemoriaFalhou = 0;
  trailerSetting = 1;
  heroEnabled = 1;
  posterSetting = 0;
  nFileiras = 0;
  fonteSetting = TRF_AUTO;
  lastSom = -1;
  somSetting = 0;
  appleReady = 0;
  appleOpenFails = 0;
  imdbOpenFails = 0;layoutSetting = HOME_LAYOUT_PADRAO;
  youtubeReady = 0; youtubeIdioma = 0;
  imdbReady = 0; imdbAnswered = 1;
  trailerfonte_definir_imdb_tizen(1);
  trailerfonte_definir_imdb_primeiro_destaque(0); /* baseline LG/WGT order */
  opened = 0;
  playing = 0;
  openedCount = 0;
  appleFailure = 0;
  lastSource[0] = 0;
}

int main(void) {
  const Uint32 start = 100;
  int rc = 0;

  // UM TRAILER POR TITULO NA SESSAO (dono, 30/09). Tocou (o `playing` chegou),
  // o foco sai do destaque e volta: a arte fica, o trailer nao recomeca.
  resetState("tt0000080");
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("primeira vez: o trailer abre", opened && openedCount == 1);
  playing = 1;
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("tocando marca o titulo como ja tocado", heroTrailerJaTocou("tt0000080"));
  rc |= check("tocando ainda segura a rotacao", heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS + 1));
  home_trailer_passo(0, 0.016f, start + 5000);   // o foco saiu do destaque: fecha
  rc |= check("saiu de cena: fecha o trailer", !opened);
  focoHero = 1;
  sourceRequests = 0;
  home_trailer_passo(1, 0.016f, start + 6000);
  home_trailer_passo(1, 0.016f, start + 6000 + NV_TRAILER_HERO_MAX_ESPERA_MS + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("voltou ao mesmo titulo: nao toca de novo", !opened && openedCount == 1);
  rc |= check("titulo ja tocado nao consulta fontes novamente", sourceRequests == 0);
  rc |= check("titulo ja tocado nao segura a rotacao", !heroTrailerSegurando(start + 6000 + NV_TRAILER_HERO_ESPERA_MS));
  // Outro titulo continua tocando normalmente.
  snprintf(item.imdb, sizeof item.imdb, "%s", "tt0000081");
  home_trailer_passo(1, 0.016f, start + 20000);
  home_trailer_passo(1, 0.016f, start + 20000 + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("outro titulo ainda toca", opened && openedCount == 2);
  // Trailer que abriu mas NUNCA tocou (sem `playing`) nao conta como tocado.
  resetState("tt0000082");
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("abriu sem tocar: nao marca", opened && !heroTrailerJaTocou("tt0000082"));

  // Um catalogo longo nao pode expulsar titulo ja tocado da memoria da sessao.
  heroTrailerMarcarTocou("tt-session-old");
  for (int i = 0; i < 160; i++) {
    char id[24];
    snprintf(id, sizeof id, "tt-session-%07d", i);
    heroTrailerMarcarTocou(id);
  }
  rc |= check("played title stays blocked beyond 96 titles",
              heroTrailerJaTocou("tt-session-old"));

  resetState("tt0000083");
  heroTrailerMemoriaFalhou = 1;
  sourceRequests = 0;
  home_trailer_passo(1, 0.016f, start);
  rc |= check("sem memoria: nao consulta fonte nem prende rotacao",
              sourceRequests == 0 && !heroTrailerSegurando(start + 1));

  // Apple is ready and opens, but fails before playback. The next source on
  // Samsung is IMDb (#136) and it is attempted exactly once. YouTube, even
  // with an id in hand, never opens in the hero: its iframe costs ~1 s of main
  // thread on the AU7000 and then fails with error 153.
  resetState("tt0000001");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  appleOpenFails = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("Apple abre somente depois da janela", openedCount == 1 && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("falha Apple libera IMDb", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("falha Apple nao repete IMDb", openedCount == 2);

#ifdef __EMSCRIPTEN__
  // TRAILER AUDIO: Apple answers first, TMDB later. The English Apple waits;
  // the dubbed TMDB trailer opens once TMDB answers.
  resetState("tt0000205");
  appleReady = 1; imdbReady = 1; youtubeIdioma = -1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("audio do trailer: Apple pronta espera o TMDB", openedCount == 0);
  youtubeIdioma = 1; youtubeReady = 1;
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("audio do trailer: TMDB depois abre o dublado", opened && !strcmp(lastSource, "dQw4w9WgXcQ"));
  // TMDB never answers: the window bounds the wait, then English.
  resetState("tt0000206");
  appleReady = 1; youtubeIdioma = -1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("audio do trailer: TMDB mudo cai na Apple no fim da janela", opened && strstr(lastSource, "apple"));
#endif

  // A source which never reaches playing gets its own preparation window.
  // The fallback receives a fresh window too.
  resetState("tt0000002");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  const Uint32 appleDeadline = start + NV_TRAILER_HERO_ESPERA_MS + NV_TRAILER_HERO_PREPARA_MS;
  const Uint32 nextDeadline = appleDeadline + NV_TRAILER_HERO_PREPARA_MS;
  home_trailer_passo(1, 0.016f, appleDeadline - 1);
  rc |= check("Apple ainda prepara antes do prazo proprio", opened && strstr(lastSource, "apple"));
  home_trailer_passo(1, 0.016f, appleDeadline);
  rc |= check("timeout Apple abre IMDb com prazo novo", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, appleDeadline + 1);
  rc |= check("IMDb permanece aberto durante a janela nova", opened && strstr(lastSource, "imdb"));
  home_trailer_passo(1, 0.016f, nextDeadline);
  rc |= check("timeout IMDb encerra fonte e libera hero (sem YouTube)", !opened && heroTrailerFonte == 3 &&
              openedCount == 2 && !heroTrailerSegurando(nextDeadline));

  // A source resolved at 3199 ms still gets a complete preparation window;
  // the resolution budget is not reused as its playback deadline.
  resetState("tt0000002b");
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS - 1);
  rc |= check("Apple no ultimo instante ganha janela util", opened && strstr(lastSource, "apple") &&
              heroTrailerPreparandoAte == start + NV_TRAILER_HERO_MAX_ESPERA_MS - 1 + NV_TRAILER_HERO_PREPARA_MS);

  // Turning the preference off during playback closes the source, clears the
  // fade immediately, and leaves the rotation helper free.
  playing = 1;
  heroTrailerFade = 0.8f;
  trailerSetting = 0;
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1);
  rc |= check("OFF durante playback reseta trailer e fade", !opened && heroTrailerItem < 0 && heroTrailerFade == 0.0f);
  rc |= check("OFF libera rotacao", !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1));

  // Apple ainda sem resposta e o IMDb ja em maos: o hero espera a Apple a
  // janela inteira; so no fim dela, sem Apple, o IMDb entra.
  resetState("tt0000002c");
  imdbReady = 1;
  youtubeReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("IMDb nao atropela a Apple sem resposta", !opened && openedCount == 0);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("sem Apple no fim da janela, IMDb entra", opened && strstr(lastSource, "imdb"));

  // So o YouTube tem trailer: no hero da Samsung, nada abre (fica a arte).
  resetState("tt0000002d");
  youtubeReady = 1;
  appleReady = 0;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("hero da Samsung nunca abre o iframe do YouTube", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS));

  // Without any source, the source wait is finite and the helper stops
  // holding the hero at the exact configured budget.
  resetState("tt0000003");
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS);
  rc |= check("sem fontes encerra no limite", !opened && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_MAX_ESPERA_MS));

  // Reusing a hero slot for another identity cannot retain the old fade or
  // preparation state.
  heroTrailerFade = 0.9f;
  snprintf(item.imdb, sizeof item.imdb, "%s", "tt0000004");
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_MAX_ESPERA_MS + 1);
  rc |= check("troca de identidade reseta fade", heroTrailerFade == 0.0f);

  // --- "Fonte do trailer" no hero da Samsung (este binario e -D__EMSCRIPTEN__).
  // Apple fixa: erro da Apple NAO cai em outra fonte.
  resetState("tt0000010");
  fonteSetting = TRF_APPLE;
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  appleOpenFails = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("Apple fixa: erro fica a arte, sem outra fonte", openedCount == 1 && !opened &&
              heroTrailerFonte == 3 && strstr(lastSource, "apple"));

  // YouTube fixo: o hero da Samsung nao abre o iframe; fica a arte, com
  // prazo finito (a pagina do titulo ainda toca o YouTube).
  resetState("tt0000011");
  fonteSetting = TRF_YOUTUBE;
  youtubeReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("YouTube fixo: hero nao abre nada", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS));

  // IMDb fixo com o servico: so IMDb, mudo, sem esperar a Apple.
  resetState("tt0000012");
  fonteSetting = TRF_IMDB;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("IMDb fixo com servico: so IMDb", openedCount == 1 && opened && strstr(lastSource, "imdb"));
  rc |= check("hero da Samsung abre mudo", lastSom == 0);

  // IMDb fixo SEM o servico na build: nada, com prazo finito.
  resetState("tt0000012b");
  trailerfonte_definir_imdb_tizen(0);
  fonteSetting = TRF_IMDB;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
#ifdef NV_TPK
  rc |= check("native IMDb needs no browser proxy capability",openedCount==1&&opened&&strstr(lastSource,"imdb"));
#else
  rc |= check("IMDb fixo sem servico: nada abre", openedCount == 0 && heroTrailerTentado &&
              !heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS));
#endif

  // Automatico mantem a ordem: com todas prontas, a Apple.
  resetState("tt0000013");
  youtubeReady = 1;
  imdbReady = 1;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("Automatico: Apple primeiro", openedCount == 1 && strstr(lastSource, "apple"));

  // Espera do ajuste: um quadro antes dela nada abre; nela, abre.
  resetState("tt0000014");
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS - 1);
  rc |= check("antes da espera do ajuste nada abre", openedCount == 0 &&
              heroTrailerSegurando(start + NV_TRAILER_HERO_ESPERA_MS - 1));
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("na espera do ajuste abre", openedCount == 1);

  // "Som do trailer" ligado (#509: um ajuste para todo trailer): o destaque
  // pede som (quem tira o som na Samsung e trailer_abrir, fora deste stub).
  resetState("tt0000015");
  somSetting = 1;
  layoutSetting = HOME_LAYOUT_MODERNA;
  appleReady = 1;
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("ajuste de som ligado: destaque pede som", openedCount == 1 && lastSom == 1);
  playing = 1;
  for (int q = 0; q < 60; q++) home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + q * 16);
  rc |= check("destaque: cinema mode still follows the hero trailer", heroCinema.oculta && heroCinema.v > 0.5f);
  playing = 0; trailercinema_zerar(&heroCinema);

  // #500: opening uses last frame's artwork geometry. The next draw can
  // expand the hero from a corner banner to fullscreen without a new title.
  resetState("tt500rect");
  appleReady = 1;
  heroArteRect = (GfxRect){555, 0, 1365, 670};
  home_trailer_passo(1, 0.016f, start);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS);
  rc |= check("hero opens in the current artwork rectangle", opened && lastRect.x == 555);
  heroArteRect = (GfxRect){0, 0, 1920, 1080};
  playing = 1;
  int movesBefore = rectCalls;
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 1);
  rc |= check("hero video follows expanded artwork without reopening",
              lastRect.x == 0 && lastRect.w == 1920 && lastRect.h == 1080 && openedCount == 1);
  home_trailer_passo(1, 0.016f, start + NV_TRAILER_HERO_ESPERA_MS + 2);
  rc |= check("unchanged hero geometry does not reset the video plane", rectCalls == movesBefore + 1);
  home_trailer_passo(0, 0.016f, start + 4000);

  for (int social=0; social<2; social++) {
    resetState(social ? "tt228social" : "tt228catalog");
    nFileiras=1;foco.fileira=0;fileiras[0].tipo=social ? FILEIRA_SOCIAL : FILEIRA_CATALOGOS;
    appleReady=1;
    home_trailer_passo(1,0.016f,start);
    home_trailer_passo(1,0.016f,start+NV_TRAILER_HERO_ESPERA_MS);
    rc |= check(social ? "hero after cached social row opens" : "hero after cached catalog row opens",openedCount==1);
    resetState(social ? "tt228socialfocus" : "tt228catalogfocus");
    nFileiras=1;foco.fileira=0;fileiras[0].tipo=social ? FILEIRA_SOCIAL : FILEIRA_CATALOGOS;
    focoHero=0;posterSetting=1;heroPendente=heroAtual;heroPendenteEm=start-1000;appleReady=1;
    home_trailer_passo(1,0.016f,start);
    home_trailer_passo(1,0.016f,start+NV_TRAILER_HERO_ESPERA_MS);
    rc |= check(social ? "focused social row remains blocked" : "focused catalog row remains blocked",openedCount==0 && heroTrailerGateAnterior==HERO_GATE_NONCONTENT);
    int logsBefore=gateLogs;
    for (int frame=0;frame<100;frame++) home_trailer_passo(1,0.016f,start+2500+frame);
    rc |= check("unchanged gate emits no per-frame logs",gateLogs==logsBefore);
    home_trailer_passo(0,0.016f,start+3000);
    rc |= check("overlay gate reported",heroTrailerGateAnterior==HERO_GATE_OVERLAY);
  }

  resetState("tt228poster");
  focoHero=0;posterSetting=1;trailerSetting=0;nFileiras=1;foco.fileira=0;
  fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
  home_trailer_passo(1,.016f,start+499);
  rc|=check("focused poster waits for expansion delay",!opened&&heroTrailerGateAnterior==HERO_GATE_POSTER_WAIT);
  home_trailer_passo(1,.016f,start+500);
  home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
  rc|=check("focused poster (tela cheia) opens muted with the sound switch off",opened&&lastSom==0);
  home_trailer_passo(0,.016f,start+4000);rc|=check("poster overlay closes Home trailer",!opened);
  // #509: UM ajuste de som vale em qualquer lugar que o trailer toca.
  resetState("tt228posterSom");
  focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CHEIA;trailerSetting=0;somSetting=1;nFileiras=1;foco.fileira=0;
  fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
  home_trailer_passo(1,.016f,start+500);
  home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
  rc|=check("tela cheia poster asks for sound with the switch on",opened&&lastSom==1);

  // Dinamica Stage/Split borrow Moderna geometry but keep opaque artwork.
  // Fullscreen poster trailers must follow the saved layout, even with sound.
  { const TipoFileira rows[] = {FILEIRA_PALCO, FILEIRA_HEROI_DIVIDIDO};
    for (int row = 0; row < 2; row++) {
      for (int layout = HOME_LAYOUT_MODERNA; layout <= HOME_LAYOUT_DINAMICA; layout++) {
        resetState("tt-dinpalco-trailer");
        focoHero = 0; foco.fileira = 0; nFileiras = 1;
        fileiras[0].tipo = rows[row];
        layoutSetting = layout; posterSetting = AJ_TRAILER_CARTAZ_CHEIA;
        trailerSetting = 0; somSetting = 1; appleReady = 1;
        heroPendente = heroAtual; heroPendenteEm = start; sourceRequests = 0;
        home_trailer_passo(1, .016f, start + 500);
        home_trailer_passo(1, .016f, start + 500 + NV_TRAILER_HERO_ESPERA_MS);
        if (layout == HOME_LAYOUT_DINAMICA) {
          rc |= check(row ? "Dinamica Split blocks hidden fullscreen playback" :
                            "Dinamica Stage blocks hidden fullscreen playback",
                      !opened && openedCount == 0 && sourceRequests == 0 &&
                      heroTrailerItem == -1 && heroTrailerGateAnterior == HERO_GATE_DYNAMIC);
        } else {
          rc |= check("Stage/Split fullscreen trailers remain eligible in other saved layouts",
                      opened && openedCount == 1 && lastSom == 1);
        }
      }
    }
  }

  // TRES MODOS. No card: so com o cartaz que o desenho marcou,
  // no retangulo dele, sem modo cinema; Tela cheia: no destaque, com a Home
  // no lugar (#509: o modo cinema ficava para o trailer do destaque).
  { const GfxRect card = { 300.0f, 600.0f, 568.0f, 320.0f };
    resetState("tt-card");
    focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CARD;trailerSetting=0;nFileiras=1;foco.fileira=0;
    layoutSetting=HOME_LAYOUT_MODERNA;trailercinema_zerar(&heroCinema);
    fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
    cartaoTrailerItem=-1;cartaoTrailerPode=cartaoTrailerServe=0;heroTrailerNoCartao=0;
    home_trailer_passo(1,.016f,start+500);
    home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: portrait without expand never plays",!opened&&heroTrailerGateAnterior==HERO_GATE_POSTER_WAIT);
    cartaoTrailerItem=heroAtual;cartaoTrailerRect=card;cartaoTrailerPode=1;cartaoTrailerServe=0;
    home_trailer_passo(1,.016f,start+600);
    home_trailer_passo(1,.016f,start+600+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: expanding portrait waits until fully open",!opened);
    cartaoTrailerServe=1;
    home_trailer_passo(1,.016f,start+700+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: opens muted inside the card rect with the switch off",opened&&lastSom==0&&heroTrailerNoCartao&&lastNoCartao&&
              lastRect.x==card.x&&lastRect.y==card.y&&lastRect.w==card.w&&lastRect.h==card.h);
    playing=1;
    for (int q=0;q<60;q++) home_trailer_passo(1,.016f,start+800+NV_TRAILER_HERO_ESPERA_MS+q*16);
    rc|=check("no card: fades in, no cinema mode",heroTrailerFade>0.9f&&!heroCinema.oculta);
    rectCalls=0;cartaoTrailerRect.x+=40.0f;
    home_trailer_passo(1,.016f,start+2000+NV_TRAILER_HERO_ESPERA_MS);
    home_trailer_passo(1,.016f,start+2016+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: plane follows the card once per move",rectCalls==1&&lastRect.x==card.x+40.0f);
    focoHero=1;trailerSetting=1;
    home_trailer_passo(1,.016f,start+2100+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: focus to the hero closes the card trailer",!opened&&!heroTrailerNoCartao);
    focoHero=0;cartaoTrailerItem=-1;
    home_trailer_passo(1,.016f,start+2200+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card: card gone (focus left) stays closed",!opened);

    resetState("tt-card-som");
    focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CARD;trailerSetting=0;somSetting=1;nFileiras=1;foco.fileira=0;
    layoutSetting=HOME_LAYOUT_MODERNA;trailercinema_zerar(&heroCinema);
    fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
    cartaoTrailerItem=heroAtual;cartaoTrailerRect=card;cartaoTrailerPode=cartaoTrailerServe=1;
    home_trailer_passo(1,.016f,start+500);
    home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("no card asks for sound with the switch on",opened&&lastSom==1&&heroTrailerNoCartao&&lastNoCartao);


    // Low-end Android (Onn): no window moves while the card still moves.
    resetState("tt-card-fraco");fraco=1;
    focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CARD;trailerSetting=0;nFileiras=1;foco.fileira=0;
    layoutSetting=HOME_LAYOUT_MODERNA;trailercinema_zerar(&heroCinema);
    fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
    cartaoTrailerItem=heroAtual;cartaoTrailerRect=card;cartaoTrailerPode=1;
    { Uint32 t=start+500+NV_TRAILER_HERO_ESPERA_MS;
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,start+500);
      for (int q=0;q<10;q++) { cartaoTrailerRect.x+=10.0f;cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+q*16); }
      rc|=check("low-end: card trailer waits while the card rect moves",!opened);
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+144+NV_CARTAO_ASSENTA_MS-1);
      rc|=check("low-end: still waiting just before the rect settles",!opened);
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+144+NV_CARTAO_ASSENTA_MS);
      rc|=check("low-end: opens once the card rect settled",opened&&lastRect.x==cartaoTrailerRect.x);
      playing=1;rectCalls=0;
      for (int q=0;q<60;q++) { cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+144+NV_CARTAO_ASSENTA_MS+q*16); }
      rc|=check("low-end: video fully visible before the card moves",heroTrailerFade>0.9f);
      // Production order: step sees the previous rect, then draw publishes the
      // new rect and chooses the hole alpha before the next step can hide it.
      Uint32 movingAt=t+500+NV_CARTAO_ASSENTA_MS+1000;
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,movingAt);
      rc|=check("low-end: stationary draw keeps the video visible",cartaoTrailerAlpha(heroAtual,movingAt)>0.9f);
      cartaoTrailerRect.x+=40.0f;
      float movingAlpha=cartaoTrailerAlpha(heroAtual,movingAt);
      printf("first-moving-frame alpha: %.6f\n",movingAlpha);
      rc|=check("low-end: draw hides hole before the next trailer step",movingAlpha==0.0f&&rectCalls==0);
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,movingAt+16);
      rc|=check("low-end: hole gone on the first moving frame",heroTrailerFade<0.01f);
      for (int q=1;q<5;q++) { cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+500+NV_CARTAO_ASSENTA_MS+1000+q*16); }
      rc|=check("low-end: plane holds while the card moves",rectCalls==0);
      rc|=check("low-end: poster returns (no hole) while the card moves",heroTrailerFade<0.01f);
      asyncGeometry=1;
      cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+500+NV_CARTAO_ASSENTA_MS+1000+80+NV_CARTAO_ASSENTA_MS);
      rc|=check("low-end: plane follows once, after the card settled",rectCalls==1&&lastRect.x==cartaoTrailerRect.x);
      for (int q=0;q<60;q++) { cartaoTrailerServe=1;home_trailer_passo(1,.016f,movingAt+500+q*16); }
      rc|=check("low-end: no hole while Android geometry acknowledgement is delayed",
                heroTrailerFade==0.0f&&cartaoTrailerAlpha(heroAtual,movingAt+1500)==0.0f&&rectCalls==1);
      heroTrailerFade=0.9f;
      rc|=check("low-end: draw rejects pending native geometry",cartaoTrailerAlpha(heroAtual,movingAt+1500)==0.0f);
      geometryReady=1;asyncGeometry=0;
      for (int q=0;q<60;q++) { cartaoTrailerServe=1;home_trailer_passo(1,.016f,t+700+1000+80+2*NV_CARTAO_ASSENTA_MS+q*16); }
      rc|=check("low-end: video fades back in once settled",heroTrailerFade>0.9f);
    }
    fraco=0;

    // Card playback uses the focused title, even when the hero is hidden or
    // its artwork still belongs to another title. Fullscreen still needs it.
    for (int hidden = 0; hidden < 2; hidden++) {
      resetState(hidden ? "tt-card-no-hero" : "tt-card-dynamic");
      focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CARD;trailerSetting=0;
      nFileiras=1;foco.fileira=0;fileiras[0].tipo=FILEIRA_NORMAL;
      layoutSetting=HOME_LAYOUT_DINAMICA;heroEnabled=!hidden;
      heroPendente=0;heroPendenteEm=start;appleReady=1;
      cartaoTrailerItem=0;cartaoTrailerRect=card;cartaoTrailerPode=1;cartaoTrailerServe=0;
      if (hidden) { heroAtual=1;heroDesejado=0;heroEntra=0;heroSai=1;heroDesliza=0; }
      home_trailer_passo(1,.016f,start+499);
      rc|=check("independent card still waits for focus delay",!opened);
      home_trailer_passo(1,.016f,start+500);
      home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
      rc|=check("independent card still waits for full expansion",!opened);
      cartaoTrailerServe=1;
      home_trailer_passo(1,.016f,start+501+NV_TRAILER_HERO_ESPERA_MS);
      rc|=check(hidden ? "no card plays without hero using focused identity" : "no card plays in Dynamic layout",
                opened&&openedCount==1&&heroTrailerItem==0&&heroTrailerNoCartao&&lastNoCartao&&
                !strcmp(heroTrailerImdb,item.imdb)&&lastRect.x==card.x&&lastRect.y==card.y&&
                lastRect.w==card.w&&lastRect.h==card.h);
      playing=1;
      for (int q=0;q<60;q++) home_trailer_passo(1,.016f,start+600+NV_TRAILER_HERO_ESPERA_MS+q*16);
      rc|=check("independent card fades in without cinema",heroTrailerFade>0.9f&&!heroCinema.oculta);
      home_trailer_passo(0,.016f,start+4000);
      rc|=check("independent card closes beneath overlay",!opened);
      posterSetting=AJ_TRAILER_CARTAZ_CHEIA;
      home_trailer_passo(1,.016f,start+5000);
      rc|=check("fullscreen poster still requires visible hero",
                !opened&&heroTrailerGateAnterior==(hidden ? HERO_GATE_DISABLED : HERO_GATE_DYNAMIC));
    }

    resetState("tt-cheia");
    focoHero=0;posterSetting=AJ_TRAILER_CARTAZ_CHEIA;trailerSetting=0;nFileiras=1;foco.fileira=0;
    layoutSetting=HOME_LAYOUT_MODERNA;trailercinema_zerar(&heroCinema);heroTrailerNoCartao=0;
    fileiras[0].tipo=FILEIRA_NORMAL;heroPendente=heroAtual;heroPendenteEm=start;appleReady=1;
    cartaoTrailerItem=-1;cartaoTrailerPode=cartaoTrailerServe=0;
    home_trailer_passo(1,.016f,start+500);
    home_trailer_passo(1,.016f,start+500+NV_TRAILER_HERO_ESPERA_MS);
    rc|=check("tela cheia: portrait poster plays in the hero rect",opened&&!heroTrailerNoCartao&&!lastNoCartao);
    playing=1;
    for (int q=0;q<60;q++) home_trailer_passo(1,.016f,start+600+NV_TRAILER_HERO_ESPERA_MS+q*16);
    rc|=check("tela cheia: home stays visible, no cinema for the poster trailer",!heroCinema.oculta&&heroCinema.v<0.1f);
    trailercinema_zerar(&heroCinema);
  }

#ifdef NV_TPK
  resetState("tt228imdb-timeout");trailerfonte_definir_imdb_primeiro_destaque(1);
  appleReady=imdbReady=1;
  home_trailer_passo(1,.016f,start);
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS);
  rc|=check("native TPK Home actually selects IMDb first",opened&&strstr(lastSource,"imdb"));
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+NV_TRAILER_HERO_PREPARA_MS);
  rc|=check("native IMDb timeout falls back to Apple",opened&&strstr(lastSource,"apple")&&openedCount==2);
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+NV_TRAILER_HERO_PREPARA_MS+1);
  rc|=check("native fallback has a fresh preparation window",opened&&strstr(lastSource,"apple")&&openedCount==2);
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+2*NV_TRAILER_HERO_PREPARA_MS);
  rc|=check("native second failure stops without reopening either source",!opened&&openedCount==2&&heroTrailerFonte==3);
  resetState("tt228imdb-open-fail");trailerfonte_definir_imdb_primeiro_destaque(1);
  appleReady=imdbReady=imdbOpenFails=1;
  home_trailer_passo(1,.016f,start);home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS);
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+1);
  rc|=check("native rejected IMDb open tries Apple once",opened&&strstr(lastSource,"apple")&&openedCount==2);
  resetState("tt228imdb-async-fail");trailerfonte_definir_imdb_primeiro_destaque(1);appleReady=imdbReady=1;
  home_trailer_passo(1,.016f,start);home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS);
  opened=playing=0;appleFailure=1;
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+1);
  rc|=check("native async IMDb error falls back immediately",opened&&strstr(lastSource,"apple")&&openedCount==2);
  resetState("tt228fixed-imdb");trailerfonte_definir_imdb_primeiro_destaque(1);fonteSetting=TRF_IMDB;
  appleReady=imdbReady=imdbOpenFails=1;
  home_trailer_passo(1,.016f,start);home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS);
  home_trailer_passo(1,.016f,start+NV_TRAILER_HERO_ESPERA_MS+1);
  rc|=check("native explicit IMDb never silently switches to Apple",!opened&&openedCount==1&&heroTrailerFonte==3);
#endif

  puts(rc ? "home-trailer-timer: FALHOU" : "home-trailer-timer: tudo ok");
  return rc ? 1 : 0;
}
