// Identidade de VOD, validade no disco e proxima fonte na ordem do addon.
#include "fontesmortas.h"
#include "streams.h"
#include "selospacote.h"
#include "fonteregra.h"
#include "stub_perfisessao.h"
#include "../src/streams.c"
#include "stub_player_source_actions.h"
#include <errno.h>
#include <assert.h>
int xtream_url_limitada(const char *url) { (void)url; return 0; }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "addonmodo.h"
#include "addons.h"

#ifndef FONTESMORTAS_ORCAMENTO  // fontesmortas_orcamento.c define estes
AddonModo addonmodo_get(uint64_t id) { (void)id; return AM_AUTO; }
int addonmodo_permite(uint64_t id, int direto, int debrid, int p2p) {
  (void)id; (void)direto; (void)debrid; (void)p2p; return 1;
}
#endif
int addons_parcial_abas(AddonAba *out, int max) { (void)out; (void)max; return 0; }

static time_t agora = 1000000;
static char *disco;
time_t time(time_t *t) { if (t) *t = agora; return agora; }
char *dados_ler(const char *nome) {
  assert(!strcmp(nome, "fontesmortas.txt"));
  return disco ? strdup(disco) : NULL;
}
int dados_gravar(const char *nome, const char *s) {
  assert(!strcmp(nome, "fontesmortas.txt"));
  free(disco); disco = strdup(s); return disco != NULL;
}
Uint32 SDL_GetTicks(void) { return 1000; }
int debrid_ativo(void) { return 0; }
int p2p_ativo(void) { return 0; }
int selospacote_ativo(void) { return -1; }
unsigned selospacote_versao(void) { return 1; }
int selospacote_casar(const char *const *p, int n, unsigned short *o, int m) {
  (void)p; (void)n; (void)o; (void)m; return 0;
}
void selospacote_colorido(int c) { (void)c; }
int selospacote_embutidos_ok(void) { return 0; }
const SeloFiltro *selospacote_filtro(unsigned short id) { (void)id; return NULL; }
int ajustes_selos_coloridos(void) { return 0; }
#ifndef FONTESMORTAS_ORCAMENTO  // fontesmortas_orcamento.c define estes
const char *ajustes_qualidade(void) { return "Automatica"; }
int ajustes_dolby_atmos(void) { return 0; }
int ajustes_dolby_vision(void) { return 0; }
int ajustes_dv_mkv(void) { return 0; }
int ajustes_fonte_escopo(void) { return 0; }
int ajustes_fonte_hdr(void) { return 0; }
int ajustes_fonte_manual(void) { return 0; }
int ajustes_fonte_ordem_uso(void) { return 0; }
int ajustes_fonte_primeira(void) { return 0; }
int ajustes_fonte_prioridade(void) { return 0; }
int ajustes_fonte_regex_modo(void) { return 0; }
#endif
int ajustes_fonte_texto_addon(void) { return 0; }
#ifndef FONTESMORTAS_ORCAMENTO  // fontesmortas_orcamento.c define estes
int ajustes_fonte_usar_outros(void) { return 0; }
int ajustes_fonte_idioma_audio(void) { return 0; }
int ajustes_tamanho_max_gb(void) { return 0; }
int ajustes_tamanho_min_gb(void) { return 0; }
int fonteregra_no_escopo(const FonteRegraCfg *c, int plugin)
  { (void)c; (void)plugin; return 1; }
int fonteregra_ordem_rank(const char *nome) { (void)nome; return 0; }
#endif
int fontepref_escolher(const char *id) { (void)id; return -1; }
int fontepref_escolher_manual(const char *id) { (void)id; return -1; }
int ajustes_fonte_ocultar(int regra) { (void)regra; return 0; }
int ajustes_audio_externo(void) { return 0; }
int ajustes_audio_externo_ocultar_fora_cache(void) { return 0; }
int ajustes_dts_desligado(void) { return 0; }
void audioext_alvo(const char *alvo) { (void)alvo; }
int audioext_alternar(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome; return -1;
}
void audioext_lembrar_serie(const AudioExtIdent *id) { (void)id; }
int audioext_serie_procura(const char *alvo, AudioExtIdent *id) { (void)alvo; (void)id; return 0; }
void audioext_serie_achou(const char *url, const char *cabecalhos, const char *nome) {
  (void)url; (void)cabecalhos; (void)nome;
}
int audioext_marcada(const char *url) { (void)url; return 0; }
TxtFamilia txt_fonte_interface(void) { return TXT_FAMILIA_INTER; }
void txt_estilo_medidas(TxtEstilo estilo, int *corpo, int *peso) {
  (void)estilo; if (corpo) *corpo = 20; if (peso) *peso = 400;
}
int txt_largura(TxtEstilo estilo, const char *s) {
  (void)estilo; return s ? (int)(8 * strlen(s)) : 0;
}
float txt_bloco(TxtEstilo e, const char *s, int r, int g, int b,
                float x, float y, float w, float leading, float alpha, int max) {
  (void)e; (void)s; (void)r; (void)g; (void)b; (void)x; (void)y; (void)w; (void)max;
  return s && s[0] ? leading : 0;
}
void vazao_fmt_mbps(char *dst, size_t n, int kbps, char sep) {
  (void)sep; if (dst && n) snprintf(dst, n, "%.1f", kbps / 1000.0);
}
void vazao_fmt_gb(char *dst, size_t n, double gb, char sep) {
  (void)sep; if (dst && n) snprintf(dst, n, "%.2f", gb);
}
float gfx_escala_ui(void) { return 1; }
#ifndef FONTESMORTAS_ORCAMENTO  // fontesmortas_orcamento.c define estes
const char *i18n(const char *s) { return s; }
#endif
int ajustes_idioma(void) { return IDIOMA_EN; }
#ifndef FONTESMORTAS_ORCAMENTO  // fontesmortas_orcamento.c define estes
unsigned fonteregra_versao(void) { return 0; }
#endif
int fonteregra_grupo(const FonteRegraCfg *c, const char *nome, int plugin, const char *texto) {
  (void)c; (void)nome; (void)plugin; (void)texto; return 0;
}
static int sondas, sondaTroca, sondaFalha, falhasAte;
int debrid_resolver_cache(const char *h, int f, char *u, unsigned t, int *fora) {
  (void)h; (void)f; (void)u; (void)t; (void)fora; assert(!"sem torrent nesta sonda"); return 0;
}
int rede_url_final_tipo(const char *u, int s, const char *const *c, char *d,
                        unsigned t, int *st, char *m, unsigned mt, long *b) {
  (void)s; (void)c; (void)st; (void)m; (void)mt; (void)b;
  sondas++;
  snprintf(d, t, "%s", u);
  if (sondaTroca) {
    Stream *nova = calloc(1, sizeof *nova); assert(nova);
    strcpy(nova->provedor, "Novo"); strcpy(nova->url, "https://nova.invalid/video");
    stream_definir_lista(nova, 1); free(nova);
  }
  return !sondaFalha && sondas > falhasAte;
}
char *rede_baixar_st(const char *u, int s, const char *const *c, int *st) {
  (void)u; (void)s; (void)c; (void)st; assert(!"sem HLS nesta sonda"); return NULL;
}
int fa_player_falhou_foi(unsigned r, int i) { (void)r; (void)i; return 0; }
void fa_concluir(unsigned r, int i, int ok) { (void)r; (void)i; (void)ok; }

#ifndef FONTESMORTAS_ORCAMENTO
static pthread_mutex_t eventoTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t evento = PTHREAD_COND_INITIALIZER;
static int iniciou, terminou;
static _Atomic int parar;
static void *varrerTrocas(void *u) {
  (void)u;
  while (!atomic_load(&parar)) (void)stream_proxima_vod(-1);
  return NULL;
}
static void *varrer(void *u) {
  (void)u;
  pthread_mutex_lock(&eventoTrava);
  iniciou = 1; pthread_cond_signal(&evento);
  pthread_mutex_unlock(&eventoTrava);
  (void)stream_proxima_vod(-1);
  pthread_mutex_lock(&eventoTrava);
  terminou = 1; pthread_cond_signal(&evento);
  pthread_mutex_unlock(&eventoTrava);
  return NULL;
}
static void concorrencia(void) {
  pthread_t fio;
  struct timespec limite;
  int r = 0, leuDuranteTroca;
  // A UI esta substituindo a lista: a varredura deve esperar a mesma trava.
  pthread_mutex_lock(&verTrava);
  assert(!pthread_create(&fio, NULL, varrer, NULL));
  pthread_mutex_lock(&eventoTrava);
  while (!iniciou) pthread_cond_wait(&evento, &eventoTrava);
  clock_gettime(CLOCK_REALTIME, &limite); limite.tv_sec++;
  while (!terminou && !r) r = pthread_cond_timedwait(&evento, &eventoTrava, &limite);
  assert(!r || r == ETIMEDOUT);
  leuDuranteTroca = terminou;
  pthread_mutex_unlock(&eventoTrava);
  pthread_mutex_unlock(&verTrava);
  assert(!pthread_join(fio, NULL));
  assert(!leuDuranteTroca && "varredura acessou a lista durante substituicao");
  Stream *l = calloc(3, sizeof *l); assert(l);
  for (int i = 0; i < 3; i++) {
    strcpy(l[i].provedor, "Addon"); strcpy(l[i].url, "https://exemplo.invalid/video");
  }
  assert(!pthread_create(&fio, NULL, varrerTrocas, NULL));
  for (int i = 0; i < 2000; i++) stream_definir_lista(l, i % 4);
  atomic_store(&parar, 1); assert(!pthread_join(fio, NULL));
  stream_definir_lista(NULL, 0); free(l);
}
#endif
#ifdef FONTESMORTAS_ORCAMENTO
void prepararOrcamento(int falhas) {
  Stream *l = calloc(4, sizeof *l); assert(l);
  for (int i = 0; i < 4; i++) {
    strcpy(l[i].provedor, "Addon");
    snprintf(l[i].url, sizeof l[i].url, "https://exemplo.invalid/%d", i);
  }
  stream_definir_lista(l, 4); free(l);
  sondas = 0; falhasAte = falhas;
}
int totalSondas(void) { return sondas; }
#else
int main(int argc, char **argv) {
  if (argc > 1 && !strcmp(argv[1], "concorrencia")) {
    concorrencia(); puts("fontesmortas: PASS (concorrencia)"); return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "ordem")) {
    Stream *l = calloc(2, sizeof *l); assert(l);
    for (int i = 0; i < 2; i++) {
      strcpy(l[i].provedor, "Addon");
      snprintf(l[i].url, sizeof l[i].url, "https://exemplo.invalid/%d", i);
      l[i].altura = i ? 2160 : 720;
    }
    stream_definir_lista(l, 2); free(l);
    stream_vod_falhou(1); // Vencedora inicial (2160p) no fim da lista.
    assert(stream_automatico_disponivel(0));
    assert(stream_proxima_vod(1) == 0);
    assert(stream_verificar_proxima_vod(1, 1, NULL) == 0 && sondas == 1);
    // Troca durante a sonda: aborta e nao exclui o mesmo indice da lista nova.
    sondaTroca = sondaFalha = 1;
    assert(stream_verificar_proxima_vod(1, 3, NULL) == -1 && sondas == 2);
    assert(stream_automatico_disponivel(0));
    stream_vod_falhou(0);
    assert(stream_proxima_vod(0) == -1);
    stream_definir_lista(NULL, 0); free(disco);
    puts("fontesmortas: PASS (vencedora no fim)"); return 0;
  }
  if (argc > 1 && !strcmp(argv[1], "identidade")) {
    Stream *l = calloc(2, sizeof *l); assert(l);
    strcpy(l[0].provedor, "Addon");
    strcpy(l[0].url, "https://proxy.example/play?video=filmeA&token=velho");
    l[1] = l[0]; strcpy(l[1].url, "https://proxy.example/play?video=filmeB&token=novo");
    fontesmortas_marcar(&l[0]); fontesmortas_iniciar();
    assert(fontesmortas_idade(&l[1]) < 0 && "filme B nunca foi aberto");
    strcpy(l[1].url, "https://proxy.example/play?token=novo&video=filmeA#fragmento");
    assert(fontesmortas_idade(&l[1]) >= 0);
    free(l); free(disco);
    puts("fontesmortas: PASS (seletor de conteudo)"); return 0;
  }
  Stream *s = calloc(4, sizeof *s);
  assert(s);
  strcpy(s[0].provedor, "Addon A");
  strcpy(s[0].url, "https://CDN.example/movie/1.mp4?token=abc&id=7&expires=123");
  s[0].fileIdx = -1;
  s[1] = s[0];
  strcpy(s[1].url, "https://cdn.EXAMPLE/movie/1.mp4?id=7&expires=999&token=xyz");
  s[2] = s[0]; strcpy(s[2].provedor, "Addon B");
  s[3] = s[0]; strcpy(s[3].url, "https://cdn.example/movie/1.mp4?id=8");
  fontesmortas_iniciar();
  fontesmortas_marcar(&s[0]);
  assert(fontesmortas_idade(&s[1]) == 0);
  assert(fontesmortas_idade(&s[2]) < 0 && fontesmortas_idade(&s[3]) < 0);
  assert(disco && !strstr(disco, "token") && !strstr(disco, "movie"));
  // Releitura de disco e poda: fonte continua marcada depois do reinicio.
  fontesmortas_iniciar();
  agora += 60;
  assert(fontesmortas_idade(&s[1]) == 60);
  // TV arrancando com o relogio atrasado nao apaga a tabela (fontepref.c).
  agora -= 3600;
  fontesmortas_iniciar();
  assert(fontesmortas_idade(&s[1]) >= 0);
  agora += 3600;
  assert(fontesmortas_idade(&s[1]) == 60);
  // Torrent resolvido ganha URL nova, mas a identidade e hash + arquivo.
  strcpy(s[0].infoHash, "AABBCCDD"); s[0].fileIdx = 2;
  s[1] = s[0]; strcpy(s[1].url, "https://outro.invalid/novo.mp4?sig=novo");
  fontesmortas_marcar(&s[0]);
  assert(fontesmortas_idade(&s[1]) == 0);
  s[1].fileIdx = 3; assert(fontesmortas_idade(&s[1]) < 0);
  agora += 6 * 3600;
  assert(fontesmortas_idade(&s[0]) < 0);
  fontesmortas_iniciar();
  assert(disco && !strstr(disco, "AABBCCDD"));
  // Ordem de exibicao difere dos indices de chegada: A(2), B(0), C(1).
  memset(s, 0, 4 * sizeof *s);
  for (int i = 0; i < 3; i++) {
    snprintf(s[i].url, sizeof s[i].url, "https://exemplo.invalid/%d", i);
    strcpy(s[i].provedor, "Addon A"); s[i].fileIdx = -1;
  }
  stream_definir_lista(NULL, 0);
  stream_lista_acrescentar(s, 2, 2); // B e C chegam primeiro
  stream_lista_acrescentar(&s[2], 1, 0); // A chega depois, mas aparece antes
  assert(stream_proxima_vod(2) == 0);
  // Nunca abriu nenhuma (indice -1): comeca do topo, pulando a morta.
  fontesmortas_marcar(&s[2]);
  assert(stream_proxima_vod(-1) == 0);
  fontesmortas_limpar(&s[2]);
  fontesmortas_marcar(&s[0]);
  assert(stream_proxima_vod(2) == 1);
  assert(stream_proxima_vod(1) == 2); // candidata anterior ainda nao tentada
  stream_automatico_excluir(2);
  assert(stream_proxima_vod(1) == -1);
  fontesmortas_limpar(&s[0]);
  assert(stream_proxima_vod(2) == 0);
  stream_definir_lista(NULL, 0);
  free(s); free(disco);
  puts("fontesmortas: PASS (identidade, TTL, disco, ordem, escolha manual)");
  return 0;
}
#endif
