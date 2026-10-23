// Player externo (#501) — ver extplayer.h.
#include "extplayer.h"
#include <stddef.h>

int extplayer_decidir(int modo, const char *cabs, int *avisar) {
  if (avisar) *avisar = 0;
  if (modo != EXTPLAYER_EXTERNO && modo != EXTPLAYER_PERGUNTAR) return EXTPLAYER_INTERNO;
  if (cabs && cabs[0]) { if (avisar) *avisar = 1; return EXTPLAYER_INTERNO; }
  return modo;
}

int extplayer_escolher(int anime, int modoFilme, const char *pkgFilme,
                       int modoAnime, const char *pkgAnime,
                       int (*instalado)(const char *pkg), const char **pkg, int *recuou) {
  int m = modoFilme;
  const char *p = pkgFilme;
  if (anime && modoAnime >= 0) { m = modoAnime; p = pkgAnime; }
  if (pkg) *pkg = NULL;
  if (recuou) *recuou = 0;
  if (m != EXTPLAYER_APP) return m >= EXTPLAYER_INTERNO && m <= EXTPLAYER_PERGUNTAR ? m : EXTPLAYER_INTERNO;
  if (!p || !p[0] || !instalado || !instalado(p)) {
    if (recuou) *recuou = 1;
    return EXTPLAYER_PERGUNTAR;
  }
  if (pkg) *pkg = p;
  return EXTPLAYER_EXTERNO;
}

#ifdef NV_ANDROID
#include "ajustes.h"
#include "android.h"
#include "idioma.h"
#include "ilha.h"
#include "player.h"
#include "video.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHAVE_PERGUNTA "extplayer:onde"

static struct { char pkg[96]; char nome[48]; } apps[EXTPLAYER_APPS_MAX];
static int nApps;

void extplayer_atualizar_apps(void) {
  char *lista = android_listar_players(), *linha = lista;
  int n = 0;
  while (linha && *linha && n < EXTPLAYER_APPS_MAX) {
    char *nl = strchr(linha, '\n'), *tab;
    if (nl) *nl = 0;
    tab = strchr(linha, '\t');
    if (tab && tab > linha && tab[1]) {
      *tab++ = 0;
      snprintf(apps[n].pkg, sizeof apps[n].pkg, "%s", linha);
      snprintf(apps[n].nome, sizeof apps[n].nome, "%s", tab);
      n++;
    }
    linha = nl ? nl + 1 : NULL;
  }
  if (lista) nApps = n;   // consulta que falhou mantem a lista anterior
  free(lista);
}
int extplayer_n_apps(void) { return nApps; }
const char *extplayer_app_pkg(int i) { return i >= 0 && i < nApps ? apps[i].pkg : ""; }
const char *extplayer_app_nome(int i) { return i >= 0 && i < nApps ? apps[i].nome : ""; }
int extplayer_app_instalado(const char *pkg) {
  int i;
  for (i = 0; i < nApps; i++) if (!strcmp(apps[i].pkg, pkg)) return 1;
  return 0;
}

static int perguntaAberta;
static Uint32 perguntaDesde;
static char pendUrl[2048], pendTitulo[160];
static void (*pendInterno)(const char *);
static void (*pendSair)(void);

static void abrirExterno(const char *url, const char *titulo, const char *pkg,
                         void (*interno)(const char *), void (*sair)(void)) {
  if (android_abrir_externo(url, titulo, pkg)) {
    printf("[extplayer] aberto no player externo\n");
    sair();   // sem sincronia: o item fica como iniciado
    return;
  }
  printf("[extplayer] nenhum app abriu; player interno\n");
  player_toast(i18n("Player externo indisponível; usando o interno"), 3500u);
  interno(url);
}

int extplayer_tentar(const char *url, const char *titulo, int anime,
                     void (*interno)(const char *url), void (*sair)(void)) {
  int avisar, d, recuou;
  const char *pkg;
  if (!url || !url[0]) return 0;
  if (perguntaAberta) return 1;   // modal aberto: o segundo pedido espera a resposta
  extplayer_atualizar_apps();
  d = extplayer_escolher(anime, ajustes_player_externo(0), ajustes_player_pacote(0),
                         ajustes_player_externo(1), ajustes_player_pacote(1),
                         extplayer_app_instalado, &pkg, &recuou);
  if (recuou) printf("[extplayer] app salvo nao esta instalado; perguntando\n");
  d = extplayer_decidir(d, video_cabecalhos(), &avisar);
  if (avisar) player_toast(i18n("Player externo indisponível; usando o interno"), 3500u);
  if (d == EXTPLAYER_INTERNO) return 0;
  if (d == EXTPLAYER_EXTERNO) { abrirExterno(url, titulo, pkg ? pkg : "", interno, sair); return 1; }
  {
    static IlhaModal m;
    IlhaAvisoEx e;
    snprintf(pendUrl, sizeof pendUrl, "%s", url);
    snprintf(pendTitulo, sizeof pendTitulo, "%s", titulo ? titulo : "");
    pendInterno = interno; pendSair = sair;
    memset(&m, 0, sizeof m);
    snprintf(m.kicker, sizeof m.kicker, "%s", i18n("Player"));
    snprintf(m.titulo, sizeof m.titulo, "%s", i18n("Onde reproduzir?"));
    snprintf(m.icone, sizeof m.icone, "aj_circle-play");
    m.tipo = ILHA_ACENTO;
    m.nBotoes = 2;
    snprintf(m.botao[0], sizeof m.botao[0], "%s", i18n("Interno"));
    snprintf(m.botao[1], sizeof m.botao[1], "%s", i18n("Externo"));
    memset(&e, 0, sizeof e);
    e.chave = CHAVE_PERGUNTA; e.tipo = ILHA_ACENTO; e.icone = "aj_circle-play";
    e.texto = i18n("Onde reproduzir?"); e.ms = 60000u; e.modal = &m; e.abrir = 1;
    ilha_avisar_ex(&e);
    perguntaAberta = 1;
    perguntaDesde = SDL_GetTicks();
    return 1;
  }
}

static void concluir(int externo) {
  void (*interno)(const char *) = pendInterno;
  void (*sair)(void) = pendSair;
  perguntaAberta = 0;
  if (externo) abrirExterno(pendUrl, pendTitulo, "", interno, sair);
  else interno(pendUrl);
  pendUrl[0] = 0;
}

void extplayer_acao(const char *chave, int botao) {
  if (!chave || strcmp(chave, CHAVE_PERGUNTA) || !perguntaAberta) return;
  concluir(botao == 2);
}

void extplayer_passo(unsigned agora) {
  // VOLTAR: o modal recolheu sem botao. O padrao e o interno.
  if (perguntaAberta && agora - perguntaDesde > 600u && !ilha_tem(CHAVE_PERGUNTA) &&
      !ilha_modal_aberto() && !ilha_modal_visivel())
    concluir(0);
}
#endif
