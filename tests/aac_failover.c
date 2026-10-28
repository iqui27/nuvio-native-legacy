// Run the real auto-next completion consumer without SDL, network or a TV.
#include <assert.h>
#include <stdio.h>
typedef unsigned Uint32;
typedef struct { char *url, *rotulo, *cabecalhos; int dolbyVision; } Stream;
static Stream next = {"https://example.invalid/next.mp4", "next", "", 0};
static int aguardandoFonte, fonteEscolhida, antAberta = -1, fonteFalha;
static int fonteVODAutomatica, fonteVODTentativas, fonteVODDesde, autoEsperaN;
static int folhaParaTocar, canalFonteIdx, canalFonteDesde, opened, sheet, message;
#define VOD_FONTE_MAX_TENTATIVAS 3
static const Stream *stream_item(int i) { return i == 2 ? &next : NULL; }
static int fa_ja_tocando(int a, int b) { return a >= 0 && a == b; }
static void desfazerAntecipada(const char *s) { antAberta = -1; }
static void video_definir_dv(int v) {}
static void video_definir_cabecalhos(const char *s) {}
static void video_definir_mp4(int v) {}
static int stream_e_mp4(const Stream *s) { return 1; }
static const char *player_id_canal(void) { return ""; }
static int ajustes_fonte_primeira(void) { return 0; }
static int addons_busca_parcial(void) { return 0; }
static void marco(const char *s) {}
static int debrid_recusa(char *s, unsigned n) { return 0; }
static int player_aberto(void) { return 1; }
static int player_mini_ativo(void) { return 0; }
static int player_quer_sair(void) { return 0; }
static int stream_n(void) { return 3; }
static void stream_definir_atual(int i) {}
static const char *urlCanal(const Stream *s, char *dst, unsigned n) { return s->url; }
#define NV_MEDIA_URL_MAX 4096
static void player_definir_fonte(const char *s) { assert(s == next.url); opened++; }
static void definirFonteVOD(const char *s) { player_definir_fonte(s); }
static int debrid_sem_plano_novo(void) { return 0; }
static const char *debrid_sem_plano_frase(int v) { return ""; }
static const char *i18n(const char *s) { return s; }
static void player_toast_ex(const char *s, unsigned ms, const char *icon, int v) {}
static void stream_folha_contexto(const char *s) { assert(*s); message++; }
static void fontefalha_marcar_aberta(int s) {}
static int fontefalha_tentativas(int s) { return 1; }
static int fontefalha_teto(int s) { return 3; }
static unsigned SDL_GetTicks(void) { return 100; }
static void player_definir_tentativa(int n, int max) {}
static int stream_atual(void) { return 1; }
static int stream_proxima_vod(int i) { return -1; }
static int pedirProximaFonteVOD(int i) { return 0; }
static int stream_regra_bloqueou(void) { return 0; }
static void limparFonteVOD(void) { fonteVODAutomatica = 0; }
static void nomeParaFolha(void) {}
static void stream_folha_abrir(void) { sheet++; }
static void erroSemFonte(void) { message++; }
#include "consumer.c"
int main(void) {
  aguardandoFonte = 2; fonteEscolhida = 2; fonteVODTentativas = 2;
  consume();
  assert(opened == 1 && !aguardandoFonte && fonteVODAutomatica);
  assert(fonteVODTentativas == 3);
  aguardandoFonte = 2; fonteEscolhida = -1;
  consume();
  assert(!aguardandoFonte && !fonteVODAutomatica);
  assert(sheet == 1 && folhaParaTocar && message);
  puts("PASS auto-next: resolved candidate starts; exhausted candidates open Sources with a message");
}
