// Testa a fachada de diagnostico do Farol (issue #14, Onda 2, ADENDO 2.1 C).
// Compila o src/farol.c, src/farolanel.c, src/farolregra.c, src/resolverregra.c
// e src/farolerro.c reais (sem SDL, sem rede).
//
// Cobre:
//   - abrir->erro->abrir->erro (FE_ERRO_FONTE, FE_RECUSA_SINC, FE_ERRO_NATIVO)
//   - reset por FE_PRONTO, por farol_sessao_reiniciar, farol_esquecer e TTL > 6 h
//   - controles de audio (DTS, conversaoPossivel, outraFaixa, VIDEO_DTS_FALHOU)
//   - controles de video/stream/debrid/addons/sonda/memoria/atualizacao
//   - eventos FE_FAROL_* e preservacao dos 256 eventos mais recentes (#11)
//   - fatos fixos com RESOLVER_TESTE e restauracao para leitura ao vivo
//   - zero chamadas de rede e latencia media < 1 ms por julgamento
#include "farol.h"
#include "farolregra.h"
#include "farolanel.h"
#include "farolsonda.h"
#include "memlog.h"
#include "resolverregra.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ------------------------------------------------ stubs Home (lado 45 do .c)
const char *i18n(const char *s) { return s ? s : ""; }
int home_fileiras_na_tela(void) { return 0; }
const char *app_tela_nome(void) { return "ajustes"; }
int recomenda_pessoa_canonica(const char *id, char *dst, size_t tam) {
  (void)id; if (dst && tam) dst[0] = 0; return 0;
}
int cat_indice_por_imdb(const char *imdb) { (void)imdb; return -1; }
int cat_copiar_item(int idx, CatItem *dst) { (void)idx; (void)dst; return 0; }
int cat_copiar_por_id(const char *id, const char *tipo, CatItem *dst) {
  (void)id; (void)tipo; (void)dst; return 0;
}
int cat_fileira_imdb(const char *chave, char (*out)[64], int max, int stride) {
  (void)chave; (void)out; (void)max; (void)stride; return 0;
}
const CatItem *cat_item(int i) { (void)i; return NULL; }
int cat_n(void) { return 0; }
unsigned cat_revisao(void) { return 0; }
int cat_visto(const CatItem *ci) { (void)ci; return 0; }
int cat_remover_fileiras_prefixo(const char *d, const char *p) { (void)d; (void)p; return 0; }
int farolcena_home(char *dst, size_t cap, const void *ctx, void *a, size_t b) {
  (void)dst; (void)cap; (void)ctx; (void)a; (void)b; return 0;
}
int farolpeca_ler(void *p, const char *j, int nc) { (void)p; (void)j; (void)nc; return 0; }
int farolpeca_texto_tem(const void *t) { (void)t; return 0; }
const char *farolpeca_x(const void *c, const void *t) { (void)c; (void)t; return ""; }
int farolrec_nivel(const void *g) { (void)g; return 0; }
int farolficha_ler_item(const CatItem *ci, void *dst) { (void)ci; (void)dst; return 0; }
uint32_t farolficha_chave(const void *f) { (void)f; return 0; }
int farolsinal_modelo(void *dst) { (void)dst; return 0; }
int farolsinal_ficha_do_item(const CatItem *ci, void *dst) { (void)ci; (void)dst; return 0; }
void farolsinal_mostrado(const CatItem *ci) { (void)ci; }
int farolsinal_recomendar(void) { return 0; }
void farolsinal_zerar_gosto(void) {}
void farolsinal_definir_recomendar(int ligado) { (void)ligado; }
int seguro_perfil_ativo(void) { return 0; }
int perfil_infantil_ativo(void) { return 0; }
int perfis_ativo(void) { return 1; }
int fil_oculta(const char *chave) { (void)chave; return 0; }
int diario_n(int perfil) { (void)perfil; return 0; }
int diario_entrada(int perfil, int i, void *dst) { (void)perfil; (void)i; (void)dst; return 0; }
int diario_titulo(int perfil, const char *id, void *dst) { (void)perfil; (void)id; (void)dst; return 0; }
int diario_nome(const char *id, char *t, size_t nt, int *a, char *p, size_t np) {
  (void)id; if (t && nt) t[0] = 0; (void)a; if (p && np) p[0] = 0; return 0;
}
int salvos_n(void) { return 0; }
const void *salvos_item(int i) { (void)i; return NULL; }
int32_t mapa_hash_nome(const char *s) { (void)s; return -1; }
uint32_t mapa_hash_titulo(const char *s) { (void)s; return 1; }

// ----------------------------------------- produtores controlaveis (#14)
typedef struct { int dts; } FaixaFake;

static uint32_t c_agoraMs = 100000u;
static uint32_t relogioMock(void) { return c_agoraMs; }

static int c_redeChamadas;
char *rede_baixar(const char *u, int t) { (void)u; (void)t; c_redeChamadas++; return NULL; }
int rede_pedir(const void *r, void *s) { (void)r; (void)s; c_redeChamadas++; return 0; }

static int c_offline;
static int c_addonsN = 2;
static int c_addonsOcupado;
static char c_addonsMotivo[200];
static int c_debridSemPlano;
static char c_debridRecusa[64];
static long c_memLivreMB = 512;
static int c_webosMajor = 5;
static int c_dtsConv = 1;
static int c_dtsTpkDisp;
static int c_dtsEstado;
static int c_videoNegado;
static int c_audioNaoSup;
static int c_nAudio;
static int c_audioAtual = -1;
static FaixaFake c_faixas[8];
static int c_videoDv;
static char c_videoHdr[32] = "none";
static FarolSonda c_sonda = {-1, -1, 0, -1, -1, 0, 0};
static int c_sondaPronta = 1;
static char c_versaoNova[32];

static int c_stDv = -1, c_stHdr = -1, c_stGB = -1, c_stMbps = -1, c_stDeb = -1;
static int c_stOutra = -1, c_stMenor = -1, c_stSemDv = -1, c_stSemDts = -1;
static int c_stTvHdr = -1, c_stTvDv = -1, c_stVazao = -1;

static int c_temErro;
static FarolErro c_erro;
static int c_erroDebrid;
static uint64_t c_erroTentativa = 1;
static uint64_t c_erroSeq;
static int c_extDisp;

int rede_saude_offline(void) { return c_offline; }
int addons_n(void) { return c_addonsN; }
int addons_ocupado(void) { return c_addonsOcupado; }
int addons_motivo_vazio(char *dst, unsigned n) {
  if (!c_addonsMotivo[0]) { if (dst && n) dst[0] = 0; return 0; }
  snprintf(dst, n, "%s", c_addonsMotivo);
  return 1;
}
int debrid_sem_plano(void) { return c_debridSemPlano; }
int debrid_recusa(char *dst, unsigned n) {
  if (!c_debridRecusa[0]) { if (dst && n) dst[0] = 0; return 0; }
  snprintf(dst, n, "%s", c_debridRecusa);
  return 1;
}
long memlog_livre_mb(void) { return c_memLivreMB; }
int nv_webos_major(void) { return c_webosMajor; }
int dts_playback_enabled(void) { return c_dtsConv; }
int dts_tpk_disponivel(void) { return c_dtsTpkDisp; }
int video_dts_estado(void) { return c_dtsEstado; }
int video_registro_negado(void) { return c_videoNegado; }
int video_audio_nao_suportado(void) { return c_audioNaoSup; }
int video_n_audio(void) { return c_nAudio; }
// VideoFaixa e declarada opaca como em src/farol.c (sem SDL2 nesta fixture);
// FaixaFake preenche so o campo que a fachada testa.
typedef struct VideoFaixa VideoFaixa;
const VideoFaixa *video_audio(int i) {
  return (i >= 0 && i < c_nAudio) ? (const VideoFaixa *)&c_faixas[i] : NULL;
}
int video_audio_atual(void) { return c_audioAtual; }
int video_faixa_dts(const VideoFaixa *f) { return f ? ((const FaixaFake *)f)->dts : 0; }
int video_tem_dolby_vision(void) { return c_videoDv; }
const char *video_hdr(void) { return c_videoHdr; }
int farolsonda_estado(FarolSonda *out) {
  if (out) *out = c_sonda;
  return c_sondaPronta;
}
const char *atualizacao_nova(void) { return c_versaoNova; }
void stream_farol_fatos(int *fonteDv, int *fonteHdr, int *fonteGBx10,
                        int *fonteMbps, int *fonteDebrid, int *temOutraFonte,
                        int *temFonteMenor, int *temFonteSemDv,
                        int *temFonteSemDts, int *tvHdr, int *tvDv,
                        int *vazaoMbps) {
  if (fonteDv) *fonteDv = c_stDv;
  if (fonteHdr) *fonteHdr = c_stHdr;
  if (fonteGBx10) *fonteGBx10 = c_stGB;
  if (fonteMbps) *fonteMbps = c_stMbps;
  if (fonteDebrid) *fonteDebrid = c_stDeb;
  if (temOutraFonte) *temOutraFonte = c_stOutra;
  if (temFonteMenor) *temFonteMenor = c_stMenor;
  if (temFonteSemDv) *temFonteSemDv = c_stSemDv;
  if (temFonteSemDts) *temFonteSemDts = c_stSemDts;
  if (tvHdr) *tvHdr = c_stTvHdr;
  if (tvDv) *tvDv = c_stTvDv;
  if (vazaoMbps) *vazaoMbps = c_stVazao;
}
int player_ultimo_erro_farol(FarolErro *out, int *foiDebrid) {
  if (!c_temErro) return 0;
  if (out) *out = c_erro;
  if (foiDebrid) *foiDebrid = c_erroDebrid;
  return 1;
}
uint64_t player_ultimo_erro_tentativa(void) {
  return c_temErro ? c_erroTentativa : 0;
}
uint64_t player_ultimo_erro_seq(void) {
  return c_temErro ? c_erroSeq : 0;
}
int player_externo_disponivel_tentativa(uint64_t tentativa) {
  return tentativa && tentativa == c_erroTentativa && c_extDisp;
}
const char *player_id_canal(void) { return ""; }   // nunca em canal ao vivo

static void resetCenario(void) {
  farolanel_zerar();
  farol_sessao_reiniciar();
  resolverregra_falha_limpar();
  c_agoraMs = 100000u;
  c_redeChamadas = 0;
  c_offline = 0;
  c_addonsN = 2;
  c_addonsOcupado = 0;
  c_addonsMotivo[0] = 0;
  c_debridSemPlano = 0;
  c_debridRecusa[0] = 0;
  c_memLivreMB = 512;
  c_webosMajor = 5;
  c_dtsConv = 1;
  c_dtsTpkDisp = 0;
  c_dtsEstado = 0;
  c_videoNegado = 0;
  c_audioNaoSup = 0;
  c_nAudio = 0;
  c_audioAtual = -1;
  memset(c_faixas, 0, sizeof c_faixas);
  c_videoDv = 0;
  snprintf(c_videoHdr, sizeof c_videoHdr, "none");
  c_sonda = (FarolSonda){-1, -1, 0, -1, -1, 0, 0};
  c_sondaPronta = 1;
  c_versaoNova[0] = 0;
  c_stDv = c_stHdr = c_stGB = c_stMbps = c_stDeb = -1;
  c_stOutra = c_stMenor = c_stSemDv = c_stSemDts = -1;
  c_stTvHdr = c_stTvDv = c_stVazao = -1;
  c_temErro = 0;
  memset(&c_erro, 0, sizeof c_erro);
  c_erroDebrid = 0;
  c_erroTentativa = 1;
  c_erroSeq = 0;
  c_extDisp = 0;
}

static void registrarErroFormato(void) {
  c_temErro = 1;
  c_erro.classe = FC_FORMATO;
  c_erro.http = 0;
  c_erro.nativo = 204;
  snprintf(c_erro.cod, sizeof c_erro.cod, "ums_204");
  // Como player.c, a seq do anel marca a sessao do erro: os FE_ ja emitidos
  // (incluido o FE_ERRO_FONTE que o produtor grava com o erro) pertencem a ela.
  c_erroSeq = farolanel_seq();
  resolverregra_falha_registrar(RES_T_TV, "Erro de formato", "", "",
                                (long)time(NULL));
}

int main(void) {
  FarolFatos f;
  FarolVeredito v;
  FarolEvento ev[FAROL_ANEL_N];
  int n, cod;

  farolanel_relogio(relogioMock);
  resetCenario();

  // 1. Estado limpo antes do primeiro julgamento.
  assert(farol_veredito() == NULL);
  assert(farol_veredito_copiar(&v, &f) == 0);
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);
  assert(f.haFalha == 0);
  assert(c_redeChamadas == 0);

  // 2. open->error->open->error (async FE_ERRO_FONTE) -> falhasSeguidas == 2 -> TV_FORMATO_REPETE.
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  registrarErroFormato();   // como no player: erro registrado com o FE_ERRO_FONTE
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 2, 3, "starfish");
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");

  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 2);
  cod = farol_julgar(RES_S_NAO_COMECA);
#ifdef NV_ANDROID
  assert(cod == FAROL_C_TV_FORMATO_REPETE);
#else
  assert(cod == FAROL_C_TV_FORMATO_REPETE);
#endif
  assert(farol_veredito() != NULL);
  assert(farol_veredito()->regra->codigo == FAROL_C_TV_FORMATO_REPETE);
  assert(farol_veredito_copiar(&v, &f) == 1);
  assert(v.regra->codigo == FAROL_C_TV_FORMATO_REPETE);
  assert(f.sintoma == RES_S_NAO_COMECA);

  // Verifica FE_FAROL_VEREDITO gravado no anel.
  n = farol_eventos(ev, FAROL_ANEL_N);
  assert(n > 0);
  assert(ev[n - 1].ev == FE_FAROL_VEREDITO);
  assert(ev[n - 1].a == FAROL_C_TV_FORMATO_REPETE);
  assert(ev[n - 1].b == RES_S_NAO_COMECA);
  assert(!strcmp(ev[n - 1].tok, "tv_formato_repete"));
  puts("ok  open->error->open->error -> falhasSeguidas=2 e FE_FAROL_VEREDITO");

  // 3. FE_PRONTO zera sequencia consecutiva; proximo erro conta 1 (TV_FORMATO).
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_PRONTO, 0, 180, 0, "starfish");
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  c_agoraMs += 50;
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  cod = farol_julgar(RES_S_NAO_COMECA);
  assert(farol_veredito_copiar(&v, &f) == 1);
  assert(f.falhasSeguidas == 1);
  assert(cod == FAROL_C_TV_FORMATO);
  puts("ok  FE_PRONTO zera falhas seguidas");

  // 4. FE_RECUSA_SINC + FE_ERRO_FONTE na mesma tentativa conta 1 falha, nao 2.
  resetCenario();
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_FONTE, FE_RECUSA_SINC, 2, 0, 0, "ums_204");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  registrarErroFormato();
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_TV_FORMATO);
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 2, 3, "starfish");
  farol_ev(FM_FONTE, FE_RECUSA_SINC, 2, 0, 0, "ums_204");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_TV_FORMATO_REPETE);
  puts("ok  FE_RECUSA_SINC + FE_ERRO_FONTE nao duplica contagem");

  // 5. FE_ERRO_NATIVO isolado (sem FE_ERRO_FONTE) tambem conta; reconexao->pronto zera.
  resetCenario();
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, 0, "ums_204");
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 2, 3, "starfish");
  farol_ev(FM_VIDEO, FE_ERRO_NATIVO, 2, 204, 0, "ums_204");
  registrarErroFormato();
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 2);
  farol_ev(FM_VIDEO, FE_RECONEXAO, 1, 1, 500, "ums_204");
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 1); // segunda tentativa em reconexao nao fecha erro ainda
  farol_ev(FM_PLAYER, FE_PRONTO, 0, 100, 0, "starfish");
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);
  puts("ok  FE_ERRO_NATIVO + FE_RECONEXAO tratados corretamente");

  // 6. Controles de audio: DTS sem conversao / conversao possivel / outra faixa / falhou.
  resetCenario();
  c_webosMajor = 6;
  c_dtsConv = 1;
  c_dtsEstado = 5; // VIDEO_DTS_SEM_SOM
  c_nAudio = 2;
  c_audioAtual = 0;
  c_faixas[0].dts = 1;
  c_faixas[1].dts = 1; // outra faixa tambem DTS
  cod = farol_julgar(RES_S_SOM);
  assert(farol_veredito_copiar(&v, &f) == 1);
  assert(f.faixaDts == 1);
  assert(f.outraFaixa == 0);
#ifndef NV_ANDROID
  assert(f.conversaoPossivel == 1);
  assert(cod == FAROL_C_AUDIO_DTS_SEM_SAIDA);
#endif

  // Quando conversao falhou (4), oferece outra faixa nao-DTS se existir.
  c_dtsEstado = 4; // VIDEO_DTS_FALHOU
  c_faixas[1].dts = 0;
  cod = farol_julgar(RES_S_SOM);
  assert(farol_veredito_copiar(&v, &f) == 1);
#ifndef NV_ANDROID
  assert(f.conversaoPossivel == 0);
#endif
  assert(f.outraFaixa == 1);
#ifndef NV_ANDROID
  assert(cod == FAROL_C_AUDIO_OUTRA_FAIXA);
#endif

  // Sem conversao e sem faixa alternativa -> AUDIO_SEM_SAIDA.
  c_nAudio = 1;
  c_audioNaoSup = 1;
  cod = farol_julgar(RES_S_SOM);
  assert(farol_veredito_copiar(&v, &f) == 1);
  assert(f.outraFaixa == 0);
  assert(f.audioRecusado == 1);
  assert(cod == FAROL_C_AUDIO_SEM_SAIDA);

  // webOS < 5 (osMaior = 4) sem audio recusado nao dispara sem saida.
  c_webosMajor = 4;
  c_audioNaoSup = 0;
  c_dtsEstado = 1; // nativo
  assert(farol_julgar(RES_S_SOM) == FAROL_C_NADA_ACHADO);
  puts("ok  controles de audio (DTS, conversao, outraFaixa, webOS < 5)");

  // 7. Controles de DV / Sonda / Memoria / Paradas / Addons / Debrid / Rede.
  resetCenario();
  c_stDv = 1;
  c_stTvDv = 0;
  assert(farol_julgar(FAROL_S_CORES) == FAROL_C_IMAGEM_DV);
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_NADA_ACHADO);

  c_stDv = -1;
  c_stTvDv = -1;
  c_sonda.http = 200;
  c_sonda.range = 0;
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_FONTE_SEM_RANGE);
  c_sonda.http = 206;
  c_sonda.range = 1;
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_NADA_ACHADO);

  c_sonda.http = 0;
  farol_ev(FM_PLAYER, FE_PARADO, 1, 3000, 0, "trava");
  c_agoraMs += 1000;
  farol_ev(FM_PLAYER, FE_PARADO, 1, 3000, 0, "trava");
  c_stGB = 200;
  c_stMbps = 0;
  c_stVazao = 12;
  assert(farol_julgar(RES_S_TRAVA) == FAROL_C_REDE_CURTA);

  c_offline = 1;
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_REDE_FORA);
  c_offline = 0;
  snprintf(c_versaoNova, sizeof c_versaoNova, "2.0.0");
  farol_fatos_ler(&f);
  assert(f.versaoNova == 1);
  assert(c_redeChamadas == 0);
  puts("ok  controles de DV, Range, rede curta, offline e zero rede");

  // 8. Sessao/reset limpa veredito e contagem sem apagar os 256 eventos recentes (#11),
  //    e TTL > 6h expira eventos antigos do diagnostico.
  resetCenario();
  farol_ev(FM_ADDON, FE_ADDON_LENTO, 1, 3000, 0, "addon1");
  farol_ev(FM_ADDON, FE_ADDON_BUSCA, 0, 3200, 1, "busca");
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  registrarErroFormato();
  farol_julgar(RES_S_NAO_COMECA);
  assert(farol_veredito() != NULL);

  int antesEv = farol_eventos(ev, FAROL_ANEL_N);
  assert(antesEv == 5);
  farol_sessao_reiniciar();
  assert(farol_veredito() == NULL);
  assert(farol_veredito_copiar(&v, &f) == 0);
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);
  assert(f.paradas5min == 0);
  assert(f.addonsLentos == -1);
  assert(f.addonsMudos == -1);
  int aposEv = farol_eventos(ev, FAROL_ANEL_N);
  assert(aposEv == antesEv);

  // Re-julgamento logo apos o reset nao reaproveita o erro da sessao
  // anterior (revisao FAROL14 r1): sem evento novo, o veredito tem de ser
  // NADA_ACHADO, com haFalha=0 e nenhuma acao ligada a tentativa anterior.
  cod = farol_julgar(RES_S_NAO_COMECA);
  assert(cod == FAROL_C_NADA_ACHADO);
  farol_fatos_ler(&f);
  assert(f.haFalha == 0);
  assert(f.erroIdadeS == -1);
  assert(f.erro.classe == FC_NENHUMA);
  assert(f.externoDisponivel == 0);

  // farol_esquecer tambem reseta o veredito e a fronteira de sessao.
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  farol_julgar(RES_S_NAO_COMECA);
  assert(farol_veredito() != NULL);
  farol_esquecer();
  assert(farol_veredito() == NULL);
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);

  // Expiracao TTL (> 6 h).
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "ums_204");
  c_agoraMs += (uint32_t)(RES_REJEITA_S + 10) * 1000u;
  farol_fatos_ler(&f);
  assert(f.falhasSeguidas == 0);
  puts("ok  sessao/reset, farol_esquecer e expiracao 6h");

  // 9. Registro dos demais FE_FAROL_* e preservacao dos 256 eventos mais recentes.
  resetCenario();
  for (int k = 0; k < 310; k++) {
    c_agoraMs += 10;
    farol_ev(FM_FONTE, FE_FONTE_PROXIMA, 0, k, 10, "step");
  }
  farol_registrar_acao(FAROL_A_CONVERTER_AUDIO);
  farol_registrar_acao(FAROL_A_ENVIAR_REGISTRO);
  farol_registrar_resultado(FAROL_A_CONVERTER_AUDIO, 1);
  farol_registrar_envio(200);
  farol_registrar_consertou(FAROL_A_CONVERTER_AUDIO);
  n = farol_eventos(ev, FAROL_ANEL_N);
  assert(n == FAROL_ANEL_N);
  assert(ev[0].ev == FE_FONTE_PROXIMA);
  assert(ev[0].a == 315 - 256); // 310 + 5 = 315 eventos no total -> 59..314
  assert(ev[n - 5].ev == FE_FAROL_ACAO && !strcmp(ev[n - 5].tok, "converter_ac3"));
  assert(ev[n - 4].ev == FE_FAROL_ACAO && !strcmp(ev[n - 4].tok, "enviar_registro"));
  assert(ev[n - 3].ev == FE_FAROL_RESULTADO && ev[n - 3].b == 1);
  assert(ev[n - 2].ev == FE_FAROL_ENVIO && ev[n - 2].a == 200);
  assert(ev[n - 1].ev == FE_FAROL_CONSERTOU && !strcmp(ev[n - 1].tok, "converter_ac3"));
  // Leitura curta: farol_eventos(dst, 3) devolve os 3 MAIS RECENTES, em ordem
  // (revisao FAROL14 r1: o anel devolvia o inicio, nao o rabinho).
  n = farol_eventos(ev, 3);
  assert(n == 3);
  assert(ev[0].ev == FE_FAROL_RESULTADO && ev[0].b == 1);
  assert(ev[1].ev == FE_FAROL_ENVIO && ev[1].a == 200);
  assert(ev[2].ev == FE_FAROL_CONSERTOU && !strcmp(ev[2].tok, "converter_ac3"));
  puts("ok  FE_FAROL_* e preservacao dos 256 eventos mais recentes");

#ifdef RESOLVER_TESTE
  // 10. RESOLVER_TESTE preserva farol_teste_fatos.
  resetCenario();
  FarolFatos fix;
  farolregra_fatos_iniciar(&fix);
  fix.plat = FP_WEBOS;
  fix.debridSemPlano = 1;
  farol_teste_fatos(&fix);
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_DEBRID_SEM_PLANO);
  farol_teste_fatos(NULL);
  assert(farol_julgar(RES_S_NAO_COMECA) == FAROL_C_NADA_ACHADO);
  puts("ok  RESOLVER_TESTE farol_teste_fatos preservado");
#endif

  // 11a. HTTP 403 do debrid com a fonte atual desligada (player fora):
  // a origem do erro (c_erroDebrid) prevalece sobre a fonte atual
  // (c_stDeb = -1) — o playbook RECONECTAR_DEBRID nao pode se perder
  // (revisao FAROL14 r1; o Resolver do pai faz o mesmo em resolver.c).
  resetCenario();
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 403, "http_403");
  c_erroSeq = farolanel_seq();
  c_temErro = 1;
  c_erro.classe = FC_HTTP;
  c_erro.http = 403;
  snprintf(c_erro.cod, sizeof c_erro.cod, "http");
  c_erroDebrid = 1;
  c_stDeb = -1;                     // sem fonte atual
  resolverregra_falha_registrar(RES_T_DEBRID, "O servidor recusou o link (HTTP 403).",
                              "", "", (long)time(NULL));
  farol_fatos_ler(&f);
  assert(f.haFalha == 1);
  assert(f.fonteDebrid == 1);
  cod = farol_julgar(RES_S_NAO_COMECA);
  assert(cod == FAROL_C_DEBRID_DESCONECTOU);
  assert(farol_veredito() != NULL);
  assert(farol_veredito()->regra->acao == FAROL_A_RECONECTAR_DEBRID);
  puts("ok  HTTP 403 debrid sem fonte atual -> DEBRID_DESCONECTOU/RECONECTAR_DEBRID");

  // 11b. HTTP 401 sem fonte atual: idem — a fonte atual (-1) nunca
  // sobrescreve a origem do erro (revisao FAROL14 r1).
  resetCenario();
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 401, "http_401");
  c_erroSeq = farolanel_seq();
  c_temErro = 1;
  c_erro.classe = FC_HTTP;
  c_erro.http = 401;
  snprintf(c_erro.cod, sizeof c_erro.cod, "http");
  c_erroDebrid = 1;
  c_stDeb = -1;
  resolverregra_falha_registrar(RES_T_DEBRID, "O servidor recusou o link (HTTP 401).",
                              "", "", (long)time(NULL));
  cod = farol_julgar(RES_S_NAO_COMECA);
  assert(cod == FAROL_C_DEBRID_DESCONECTOU);
  puts("ok  HTTP 401 debrid sem fonte atual -> DEBRID_DESCONECTOU");

  // 11c. Erro de debrid mas a fonte atual NAO e debrid: a origem do erro
  // (0) prevalece sobre a folha, sem confundir com desconexao.
  resetCenario();
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "starfish");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 403, "http_403");
  c_erroSeq = farolanel_seq();
  c_temErro = 1;
  c_erro.classe = FC_HTTP;
  c_erro.http = 403;
  snprintf(c_erro.cod, sizeof c_erro.cod, "http");
  c_erroDebrid = 0;
  c_stDeb = 1;                      // folha diz que a fonte atual e debrid
  resolverregra_falha_registrar(RES_T_FONTE, "O servidor recusou o link (HTTP 403).",
                              "", "", (long)time(NULL));
  farol_fatos_ler(&f);
  assert(f.fonteDebrid == 0);
  cod = farol_julgar(RES_S_NAO_COMECA);
  assert(cod == FAROL_C_FONTE_RECUSOU);
  puts("ok  HTTP 403 sem debrid no erro -> FONTE_RECUSOU (origem prevalece)");

#ifdef NV_ANDROID
  // 11d. Android externoDisponivel respeita FE_PRONTO apos erro.
  resetCenario();
  c_extDisp = 1;
  c_erroTentativa = 77;
  farol_ev(FM_PLAYER, FE_ABRIR, 0, 1, 3, "media3");
  farol_ev(FM_PLAYER, FE_ERRO_FONTE, 2, 0, 0, "m3_4003");
  registrarErroFormato();
  farol_fatos_ler(&f);
  assert(f.externoDisponivel == 1);
  farol_ev(FM_PLAYER, FE_PRONTO, 0, 200, 0, "media3");
  farol_fatos_ler(&f);
  assert(f.externoDisponivel == 0);
  puts("ok  Android externoDisponivel apos FE_PRONTO");
#endif

  // 12. Latencia media < 1 ms por veredito (e zero rede).
  {
    struct timespec t0, t1;
    double elapsedMs;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int k = 0; k < 1000; k++) {
      farol_julgar(RES_S_NAO_COMECA);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    elapsedMs = (double)(t1.tv_sec - t0.tv_sec) * 1000.0 +
                (double)(t1.tv_nsec - t0.tv_nsec) / 1000000.0;
    assert(elapsedMs / 1000.0 < 1.0);
    assert(c_redeChamadas == 0);
    printf("ok  1000 vereditos em %.3f ms (%.4f ms/veredito, zero rede)\n",
           elapsedMs, elapsedMs / 1000.0);
  }

  printf("farol_fachada: tudo ok\n");
  return 0;
}
