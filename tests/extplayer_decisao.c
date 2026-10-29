// Player setting + stream headers -> internal / external / ask, and the
// ask-modal guard: a second tentar while the modal is open must not start
// internal playback (returns 1, consumed), and a stray button != 2 is Interno.
#include "extplayer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#ifdef NV_ANDROID
#include "ilha.h"
static int chamouInterno, chamouSair;
static unsigned relogio;
static IlhaModal *modalVisto;
static void interno(const char *url) { (void)url; chamouInterno++; }
static void sair(void) { chamouSair++; }
int ajustes_player_externo(int anime) { return anime ? -1 : EXTPLAYER_PERGUNTAR; }
const char *ajustes_player_pacote(int anime) { (void)anime; return ""; }
char *android_listar_players(void) { return NULL; }
const char *video_cabecalhos(void) { return ""; }
int android_abrir_externo(const char *u, const char *t, const char *p) { (void)u; (void)t; (void)p; return 1; }
const char *i18n(const char *s) { return s; }
void player_toast(const char *t, unsigned ms) { (void)t; (void)ms; }
void ilha_avisar_ex(const IlhaAvisoEx *e) { if (e) modalVisto = (IlhaModal *)e->modal; }
int ilha_tem(const char *c) { (void)c; return 0; }
int ilha_modal_aberto(void) { return 0; }
int ilha_modal_visivel(void) { return 0; }
Uint32 SDL_GetTicks(void) { return relogio; }
#endif

static int instalado(const char *pkg) { return !strcmp(pkg, "org.videolan.vlc") || !strcmp(pkg, "com.mxtech.videoplayer.ad"); }

int main(void) {
  int avisar;
  avisar = 9; assert(extplayer_decidir(0, "", &avisar) == EXTPLAYER_INTERNO && !avisar);
  avisar = 9; assert(extplayer_decidir(0, "Referer: x\r\n", &avisar) == EXTPLAYER_INTERNO && !avisar);
  avisar = 9; assert(extplayer_decidir(1, "", &avisar) == EXTPLAYER_EXTERNO && !avisar);
  avisar = 9; assert(extplayer_decidir(1, NULL, &avisar) == EXTPLAYER_EXTERNO && !avisar);
  avisar = 9; assert(extplayer_decidir(2, "", &avisar) == EXTPLAYER_PERGUNTAR && !avisar);
  // Headers cannot ride an Intent: fall back to internal and tell the user.
  avisar = 0; assert(extplayer_decidir(1, "User-Agent: x\r\n", &avisar) == EXTPLAYER_INTERNO && avisar);
  avisar = 0; assert(extplayer_decidir(2, "User-Agent: x\r\n", &avisar) == EXTPLAYER_INTERNO && avisar);
  // Out-of-range value acts as Internal.
  avisar = 9; assert(extplayer_decidir(7, "", &avisar) == EXTPLAYER_INTERNO && !avisar);
  assert(extplayer_decidir(1, "", NULL) == EXTPLAYER_EXTERNO);
  // #205: player por anime / nao anime, e app que sumiu.
  { const char *pkg; int recuou;
    // Anime com "Igual ao Player" (-1) segue o Player.
    assert(extplayer_escolher(1, EXTPLAYER_INTERNO, "", -1, "", instalado, &pkg, &recuou) == EXTPLAYER_INTERNO && !recuou);
    // Anime com escolha propria: so o anime muda; filme/serie segue o Player.
    assert(extplayer_escolher(1, EXTPLAYER_INTERNO, "", EXTPLAYER_PERGUNTAR, "", instalado, &pkg, &recuou) == EXTPLAYER_PERGUNTAR);
    assert(extplayer_escolher(0, EXTPLAYER_INTERNO, "", EXTPLAYER_PERGUNTAR, "", instalado, &pkg, &recuou) == EXTPLAYER_INTERNO);
    // App instalado vira Externo direto, com o pacote.
    assert(extplayer_escolher(1, EXTPLAYER_INTERNO, "", EXTPLAYER_APP, "org.videolan.vlc", instalado, &pkg, &recuou) == EXTPLAYER_EXTERNO
           && !recuou && pkg && !strcmp(pkg, "org.videolan.vlc"));
    assert(extplayer_escolher(0, EXTPLAYER_APP, "com.mxtech.videoplayer.ad", -1, "", instalado, &pkg, &recuou) == EXTPLAYER_EXTERNO
           && !strcmp(pkg, "com.mxtech.videoplayer.ad"));
    // App desinstalado (ou pacote vazio): cai em Perguntar e avisa.
    assert(extplayer_escolher(1, EXTPLAYER_INTERNO, "", EXTPLAYER_APP, "gone.app", instalado, &pkg, &recuou) == EXTPLAYER_PERGUNTAR
           && recuou && !pkg);
    assert(extplayer_escolher(0, EXTPLAYER_APP, "", -1, "", instalado, &pkg, &recuou) == EXTPLAYER_PERGUNTAR && recuou);
    assert(extplayer_escolher(0, EXTPLAYER_APP, "x", -1, "", NULL, &pkg, &recuou) == EXTPLAYER_PERGUNTAR && recuou);
  }
#ifdef NV_ANDROID
  // Perguntar: abre o modal (dois botoes) e segura a fonte.
  relogio = 100;
  assert(extplayer_tentar("u", "t", 0, interno, sair) == 1 && modalVisto && modalVisto->nBotoes == 2);
  // Segundo tentar com o modal aberto: pendente, nao cai no player interno.
  assert(extplayer_tentar("u2", "t2", 0, interno, sair) == 1 && !chamouInterno);
  // Botao 1 = "Interno": roda o player interno.
  extplayer_acao("extplayer:onde", 1); assert(chamouInterno == 1 && !chamouSair);
  // Valor espurio tambem e Interno: so 2 abre o externo.
  extplayer_tentar("u", "t", 0, interno, sair);
  extplayer_acao("extplayer:onde", 9); assert(chamouInterno == 2 && !chamouSair);
  // Botao 2 = "Externo": sem app externo no stub, cai no interno e sai.
  extplayer_tentar("u", "t", 0, interno, sair);
  extplayer_acao("extplayer:onde", 2); assert(chamouInterno == 2 && chamouSair == 1);
#endif
  puts("extplayer_decisao: ok");
  return 0;
}
