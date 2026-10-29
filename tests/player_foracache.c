// Run production duration detection, provider exclusion and auto-play decisions.
#include "streams.h"
#include "debrid.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

typedef unsigned Uint32;
static int durFonteAvisada, durFontePulou, durFonteCurta, durFonteForaCache;
static int durFonteEsperadaOk, durFonteManter, tentativaN = 1, comVideo = 1;
static unsigned durFonteEsperadaVer, durFonteEsperadaRev, durFonteAcaoAte, toastAte;
static unsigned durFonteSkipEm, loadEm;
static double durFonteEsperadaCache, expected = 2580;
static int kept, parts, stopped, failed, requested, reconnecting;
static int fonteVODAutomatica = 1, aguardandoFonte, fonteVODTentativas;
static unsigned fonteVODDesde = 1, fonteFalhaLista;
static FonteFalhaSessao *fonteFalha;
static int current, n = 5, excluded[5], refused = -1;
static Stream lista[5];
static pthread_mutex_t verTrava = PTHREAD_MUTEX_INITIALIZER;
static char title[160], hint[320];
#define DUR_CONTAGEM_MS 8000u
#define VOD_FONTE_MAX_TENTATIVAS 8
unsigned SDL_GetTicks(void) { return 100; }
static int ehCanal(void) { return 0; }
static int partesAtiva(void) { return parts; }
static void alvoStream(char *a, unsigned size) { snprintf(a, size, "fixture"); }
static int partes_arquivo_de(const char *a, void *b) { (void)a; (void)b; return 0; }
static unsigned cat_versao_episodios(void) { return 0; }
static unsigned cat_revisao(void) { return 0; }
static double duracaoFonteEsperada(void) { return expected; }
static const char *sessao_usuario(void) { return "fixture"; }
static int perfis_ativo(void) { return 0; }
static const char *durFonteTitulo(void) { return "fixture"; }
static int duracaook_tem(const char *u, int p, const char *t) { (void)u; (void)p; (void)t; return kept; }
static const char *i18n(const char *s) { return s; }
static void player_toast_ex(const char *s, int ms, const char *icon, int color) {
  (void)s; (void)ms; (void)icon; (void)color;
}
static void player_toast(const char *s, int ms) { (void)s; (void)ms; }
static void video_parar(void) { stopped++; }
static void player_erro_fonte_motivo(const char *t, const char *h) {
  failed = 1; snprintf(title, sizeof title, "%s", t); snprintf(hint, sizeof hint, "%s", h);
}
static int player_fonte_fora_cache(void) { return durFonteForaCache; }
static const char *player_id_canal(void) { return ""; }
static int player_aberto(void) { return 1; }
static int player_quer_sair(void) { return 0; }
static int video_reconectando(void) { return reconnecting; }
int stream_atual(void) { return current; }
const Stream *stream_item(int i) { return i >= 0 && i < n ? &lista[i] : NULL; }
int stream_automatico_excluir(int i) { int fresh = !excluded[i]; excluded[i] = 1; return fresh; }
int stream_proxima_vod(int i) { (void)i; for (int k = 0; k < n; k++) if (!excluded[k]) return k; return -1; }
void fontefalha_provedor_recusado(FonteFalhaSessao *s, int i) { (void)s; refused = i; }
int stream_falha_tem_proxima(const FonteFalhaSessao *s, unsigned g) { (void)s; (void)g; return 1; }
static void player_voltar_a_esperar(void) { failed = 0; }
static int pedirProximaFonteVOD(int i) { (void)i; requested++; return 1; }
static int nv_res_tem(const char *s, const char *part) { return strstr(s, part) != NULL; }
#include "decision.c"

static void reset(void) {
  durFonteAvisada = durFontePulou = durFonteCurta = durFonteForaCache = 0;
  durFonteEsperadaOk = durFonteManter = durFonteSkipEm = 0;
  durFonteAcaoAte = toastAte = 0;
  stopped = failed = requested = reconnecting = kept = parts = 0;
  fonteVODAutomatica = comVideo = 1; fonteFalha = NULL;
  memset(excluded, 0, sizeof excluded);
  memset(lista, 0, sizeof lista);
  for (int i = 0; i < n; i++) {
    strcpy(lista[i].addonChave, i == 4 ? "other" : "debridio");
    strcpy(lista[i].rotulo, i == 3 ? "[RD] Debridio 1080p" : "[TB] Debridio 720p");
  }
  strcpy(lista[1].rotulo, "[TB] Debridio 1080p");
  lista[2].debridServico = DEBRID_PM + 1;
  current = 0;
}
int main(void) {
  reset(); durFonteAtualizar(30, 100);
  assert(stopped == 1 && failed && !comVideo && !durFonteCurta);
  assert(strstr(title, "Fora do cache") && strstr(hint, "vídeo de aviso"));
  assert(!durFonteSkipEm && !durFonteManter);
  reconnecting = 1; tentarProximaFonteVOD();
  assert(requested == 1 && excluded[0] && excluded[1]);
  assert(!excluded[2] && !excluded[3] && !excluded[4]);
  reset(); n = 2; durFonteAtualizar(27, 100); tentarProximaFonteVOD();
  assert(!requested && !fonteVODAutomatica && failed);
  n = 5;
  reset(); kept = 1; durFonteAtualizar(90, 100);
  assert(!stopped && !failed && comVideo && !durFonteForaCache && !durFonteSkipEm);
  assert(durFonteCurta);
  tentarProximaFonteVOD(); assert(!requested && !excluded[0]);
  reset(); durFonteAtualizar(121, 100); assert(!stopped && durFonteSkipEm);
  reset(); expected = 30; durFonteAtualizar(27, 100); assert(!stopped && !failed);
  expected = 2580;
  reset(); tentativaN = 0; durFonteAtualizar(90, 100);
  assert(!stopped && !failed && comVideo && !durFonteForaCache && !durFonteSkipEm);
  assert(durFonteCurta && durFonteAcaoAte);
  tentarProximaFonteVOD(); assert(!requested && !excluded[0]);
  reset(); kept = 1; durFonteAtualizar(90, 100);
  assert(!stopped && !failed && !durFonteForaCache && !durFonteAcaoAte);
  tentativaN = 1;
  reset(); parts = 1; durFonteAtualizar(30, 100); assert(!stopped && !failed);
  reset(); lista[0].debridServico = lista[1].debridServico = DEBRID_TB + 1;
  strcpy(lista[1].addonChave, "other");
  fonteFalha = (FonteFalhaSessao *)1;
  durFonteAtualizar(30, 100); tentarProximaFonteVOD();
  assert(refused == DEBRID_TB && requested == 1);
  assert(excluded[0] && excluded[1] && !excluded[2]);
  puts("PASS: auto warning clips stop; manual and kept clips play; provider alternatives preserved");
}
