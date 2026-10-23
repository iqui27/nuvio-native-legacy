// CAPTURA DA TELA DE AJUSTES, sem interacao e sem rede.
//
// Existe pelo motivo que tests/player_regression.c ja registra: interface de TV
// julgada so por codigo sai ilegivel a 3 m. Aqui as duas telas que mudam neste
// trabalho — a lista de Ajustes e a folha "Ordenar e ativar fileiras" — sao
// desenhadas com dados de mentira e gravadas em BMP, para serem OLHADAS.
//
// NAO CHAMA dados_iniciar DE PROPOSITO. Sem ela `dados_dir()` e "", entao
// fileiras.c nao le nem escreve arquivo nenhum: a lista comeca vazia (o estado
// de quem nunca abriu o app) e a captura nao mexe no fileirasui.txt de quem
// roda o teste.
#include "ajustes.h"
#include "nvtmp.h"
#include "ajustes_ux.h"
#include "atualizacao.h"
#include "badges.h"
#include "rail_shot.h"
#include "spotlight.h"
#include "fileiras.h"
#include "catalogo.h"
#include "dados.h"
#include "gfx.h"
#include "gpunivel.h"
#include "ilha.h"
#include "ltv.h"
#include "perfis.h"
#include "iconeapp.h"
#include "logoapp.h"
#include "text.h"
#include "tex_cache.h"
#include "vidro_telas.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "menu_frost_probe.h"

static const char *const AJ_IDS[] = {
  "AJ_QUALIDADE", "AJ_DV", "AJ_ATMOS", "AJ_LEG_LINGUA",
  "AJ_AUD_LINGUA", "AJ_PAUSA_OVERLAY", "AJ_FONTE_MANUAL", "AJ_FONTE_AUTO",
  "AJ_FONTE_REPOR", "AJ_LANDSCAPE", "AJ_HERO_CHEIO", "AJ_HERO_FUNDO",
  "AJ_HERO_ARTE_DIF", "AJ_HERO_TRAILER", "AJ_FIL_LIMITE", "AJ_FIL_ORDEM",
  "AJ_RAIL", "AJ_RAIL_MODERNA", "AJ_RAIL_BLUR", "AJ_HERO",
  "AJ_HERO_CATALOGOS", "AJ_PS_FUNDO", "AJ_DESCOBRIR", "AJ_ROTULOS",
  "AJ_NOME_ADDON", "AJ_SUFIXO_TIPO", "AJ_OCULTAR_NLANC", "AJ_NOTAS_HOME",
  "AJ_GRAD_CLASSICO", "AJ_CW_LIGADO", "AJ_CW_OK", "AJ_CW_FONTE",
  "AJ_CW_ESTILO", "AJ_CW_THUMB", "AJ_CW_BLUR_PROX", "AJ_CW_FURTHEST",
  "AJ_CW_NAO_EXIBIDOS", "AJ_CW_ORDEM", "AJ_DET_BLUR_NAO_VISTOS", "AJ_DET_TRAILER",
  "AJ_DET_META_EXT", "AJ_DET_DATA_CHEIA", "AJ_DET_VEU", "AJ_DET_TRAILER_AUTO",
  "AJ_TRAILER_QUAL", "AJ_TRAILER_ASPECTO", "AJ_TRAILER_FONTE", "AJ_EXPANDIR",
  "AJ_EXPANDIR_ATRASO", "AJ_NAV_RAPIDA", "AJ_BORDA_FOCO", "AJ_PROF",
  "AJ_PROF_BORDA", "AJ_PROF_BRILHO", "AJ_PROF_COBERTURA", "AJ_PROF_POSTERS",
  "AJ_PROF_CW", "AJ_PROF_EPS", "AJ_PROF_ELENCO", "AJ_PROF_TRAILERS",
  "AJ_LARGURA_DP", "AJ_RAIO_DP", "AJ_QUALIDADE_IMG", "AJ_IDIOMA",
  "AJ_ANIM", "AJ_RESOLUCAO", "AJ_TEMA", "AJ_COR_LOGO",
  "AJ_PERFIL_ATIVO", "AJ_SYNC", "AJ_ADDONS", "AJ_STALKER_PORTAL",
  "AJ_STALKER_MAC", "AJ_STALKER_LIMPAR", "AJ_XTREAM_SERVIDOR", "AJ_XTREAM_USUARIO",
  "AJ_XTREAM_SENHA", "AJ_XTREAM_LIMPAR", "AJ_SALVOS_DEST", "AJ_TRAKT",
  "AJ_SIMKL", "AJ_SAIR", "AJ_VERSAO_I", "AJ_ATUALIZAR",
  "AJ_ENVIAR_LOG", "AJ_ENVIO_AUTO", "AJ_ESPACO", "AJ_TEX_MB",
  "AJ_TMDB_LIGADO", "AJ_TMDB_IDIOMA", "AJ_TMDB_ARTE", "AJ_TMDB_BASICO",
  "AJ_TMDB_FICHA", "AJ_TMDB_DATAS", "AJ_TMDB_ELENCO", "AJ_TMDB_PROD",
  "AJ_TMDB_REDES", "AJ_TMDB_EPS", "AJ_TMDB_TRAILERS", "AJ_TMDB_MAIS",
  "AJ_TMDB_COL", "AJ_TMDB_CW", "AJ_MDB_LIGADO", "AJ_MDB_CHAVE",
  "AJ_MDB_TRAKT", "AJ_MDB_IMDB", "AJ_MDB_TMDB", "AJ_MDB_LETTER",
  "AJ_MDB_TOMATES", "AJ_MDB_AUDIENCIA", "AJ_MDB_META", "AJ_MDB_MAL",
  "AJ_FANART_CHAVE", "AJ_DIAGNOSTICO", "AJ_VELOCIDADE", "AJ_FONTE_UI",
};

extern int ajustes_teste_focar_opcao(int op);
extern int ajustes_teste_cena_item(int i, int *op, int *sec, const char **chave, const char **rot);
extern void ajustes_teste_cena_desenhar(int op, float t, float x, float y, float w);
extern void ajustes_teste_ux_captura(int cenario);
extern void atualizacao_teste_estado(int busca, const char *tag);
extern int ajustes_teste_op_atualizar(void);
extern int ajustes_teste_op_icone(int escolha);
extern int ajustes_teste_familia_previa(int op);
extern void ajustes_teste_fonte_interface(int familia);
extern void ajustes_teste_tema(int tema, int vidro);
extern void ajustes_teste_vidro_env(void);
extern void ajustes_teste_layout(int lista);
extern int ajustes_teste_quadro(const char *id);
static int quadrosCaptura = 60;

static void tecla(SDL_Keycode k) {
  SDL_Event e = { 0 };
  e.type = SDL_KEYDOWN;
  e.key.keysym.sym = k;
  ajustes_evento(&e);
}

static void captura(const char *nome, SDL_Window *win) {
  int i;
  rail_shot_aplicar();
  for (i = 0; i < quadrosCaptura; i++) {
    SDL_PumpEvents();
    if (getenv("NUVIO_SHOT_PERF") || getenv("NUVIO_SHOT_MENU_PROBE")) gfx_novo_quadro();   // os contadores do quadro, como app.c
    txt_novo_quadro();
    gfx_rastro_grandes = getenv("NUVIO_SHOT_RASTRO") && i == quadrosCaptura - 1;
    tex_novo_quadro();
    tex_bombear(6);
    ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
    if (getenv("NUVIO_SHOT_UPSCALE")) gpun_quadro_inicio();   // 720p target, as on the C9
    glClearColor(0.025f, 0.025f, 0.03f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ajustes_desenhar(SDL_GetTicks());
    rail_shot_desenhar(MENU_AJUSTES);
    if (getenv("NUVIO_SHOT_UPSCALE")) gpun_quadro_fim();
    // NUVIO_SHOT_MENU=1: the app menu drawn in its own fixed factor (0.9) on
    // top, as app.c does every frame on the TV.
    if (getenv("NUVIO_SHOT_MENU")) {
      menu_definir_destino(MENU_AJUSTES);
      menu_pilula_mostrar(1.0f);
      if (atoi(getenv("NUVIO_SHOT_MENU")) == 2) {   // 2 = sidebar open over Settings
        if (!menu_aberto()) menu_abrir();
        menu_atualizar(1.0f / 60.0f, SDL_GetTicks());
      }
      menu_probe_draw();
      menu_desenhar(SDL_GetTicks());
      if (getenv("NUVIO_SHOT_MENU_OPAQUE")) gfx_cor((GfxRect){330, 175, 81, 605}, 0, .1f, .1f, .1f, 1);
    }
    if (spot_visivel()) {   // "Buscar no guia" (o Spotlight no modo guia)
      spot_atualizar(1.0f / 60.0f, SDL_GetTicks());
      spot_desenhar(SDL_GetTicks(), 0);   // o veu de 55% e do proprio Spotlight
    }
    if (getenv("NUVIO_AJ_QUADROS")) {   // a ilha do relogio, como o app poe
      ilha_relogio_visivel(ajustes_relogio_cabe());
      ilha_posicionar(1);
      ilha_desenhar(SDL_GetTicks());
    }
    // NUVIO_SHOT_TXT=1: text lines rasterized per frame once the screen has
    // settled. A static screen must reach 0; a steady non-zero count starves
    // later lines on a slow TV (the 4-lines-per-frame floor).
    if (getenv("NUVIO_SHOT_TXT") && i >= quadrosCaptura - 4) {
      static int r0, p0, d0;
      if (i > quadrosCaptura - 4)
        printf("txt %s quadro %d: +%d rasterizadas +%d pendentes +%d despejos\n", nome, i,
               txt_rasterizadas - r0, txt_pendentes - p0, txt_despejos - d0);
      r0 = txt_rasterizadas; p0 = txt_pendentes; d0 = txt_despejos;
    }
    if (getenv("NUVIO_SHOT_PERF") && i == quadrosCaptura - 1)
      printf("perf %s fill=%.3f rects=%d cheios=%d snap=%.3f cor=%.3f luz=%.3f sombra=%.3f gpu=%.3f mist=%.3f\n", nome,
             gfx_fill, gfx_n_rect, gfx_n_cheio, gfx_fill_modo[GFX_SNAP], gfx_fill_modo[GFX_COR],
             gfx_fill_modo[GFX_LUZ], gfx_fill_modo[GFX_SOMBRA], gfx_fill_gpu, gfx_fill_gpu_mist);
    // #205: the C9 (no glass) must not blend Frost + island core full-screen: index fill <= 2.3x.
    if (getenv("NUVIO_SHOT_FILL_MAX") && i == quadrosCaptura - 1 && strstr(nome, "indice")) {
      if (gfx_fill > atof(getenv("NUVIO_SHOT_FILL_MAX"))) { printf("FAIL: fill %.3f > %s\n", gfx_fill, getenv("NUVIO_SHOT_FILL_MAX")); exit(1); }
    }
    if (i == quadrosCaptura - 1) {
      unsigned char *pix = malloc(1920 * 1080 * 4);
      SDL_Surface *s;
      int y;
      assert(pix);
      glReadPixels(0, 0, 1920, 1080, GL_RGBA, GL_UNSIGNED_BYTE, pix);
      s = SDL_CreateRGBSurfaceWithFormat(0, 1920, 1080, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 1080; y++)
        memcpy((char *)s->pixels + y * s->pitch, pix + (1079 - y) * 1920 * 4, 1920 * 4);
      assert(strstr(nome, ".png") ? IMG_SavePNG(s, nome) == 0 : SDL_SaveBMP(s, nome) == 0);
      SDL_FreeSurface(s);
      // NUVIO_SHOT_MENU=2: blur proof, as in detalhe_glass_shot.c (fraction of
      // pixels with a luma step > 6 in the empty right strip of the sidebar).
      if (getenv("NUVIO_SHOT_MENU") && atoi(getenv("NUVIO_SHOT_MENU")) == 2) {
        menu_probe_check(pix);
      }
      free(pix);
    }
    SDL_GL_SwapWindow(win);
    if (getenv("NUVIO_AJ_QUADROS")) SDL_Delay(16);   // a mola da ilha anda no relogio
  }
  printf("captura: %s\n", nome);
}

#ifdef AJUSTES_PERF
extern int perf_uploads;
#include "ajustes_perf.h"
#endif

int main(int argc, char **argv) {
  const char *saida = argc > 1 ? argv[1] : nvt("/nuvio-ajustes");
  char nome[600];
  SDL_Window *w;
  SDL_GLContext gl;
  int i;

  assert(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) == 0);
  IMG_Init(IMG_INIT_PNG | IMG_INIT_JPG);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  w = SDL_CreateWindow("Nuvio: revisao dos Ajustes", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1920, 1080,
                       SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN);
  assert(w);
  gl = SDL_GL_CreateContext(w);
  assert(gl);
  SDL_GL_SetSwapInterval(0);
  glViewport(0, 0, 1920, 1080);
  gfx_tamanho_alvo(1920, 1080);
  assert(gfx_iniciar());
  assert(txt_iniciar("deploy/app", 1));
  tex_iniciar(64);
  gfx_icones_dir("deploy/app/art");
  ajustes_recursos("deploy/app/art");
  badges_carregar("deploy/app/art");   // marcas de formato (Dolby Vision, 4K...); no app quem faz e home_iniciar

  // NUVIO_SHOT_URLS=1: duas fontes por URL (M9) para a linha "Listas e guias por URL".
  if (getenv("NUVIO_SHOT_URLS")) {
    ltv_urls_carregar(perfis_ativo());
    ltv_urls_adicionar(LTV_URL_M3U, "iptv.exemplo.com", "https://iptv.exemplo.com/get.php?username=x&password=y&type=m3u");
    ltv_urls_adicionar(LTV_URL_XMLTV, "epg.exemplo.org", "https://epg.exemplo.org/guia.xml.gz");
  }

  // FILEIRAS DE MENTIRA cobrindo as origens que a folha sabe distinguir: o
  // catalogo de addon (com nome de addon e tipo), o grupo de colecoes, e as
  // fileiras que o proprio app monta.
  // NUVIO_SHOT_DADOS=<pasta>: the rows of a real fileirasui*.txt (a COPY in a
  // scratch folder, never the app's own) instead of the fake ones below.
  if (getenv("NUVIO_SHOT_DADOS")) { setenv("NUVIO_DADOS", getenv("NUVIO_SHOT_DADOS"), 1); dados_iniciar("deploy/app/art"); goto fileiras_prontas; }
  fil_registrar("continue_watching", "Continuar assistindo", "", "", 12);
  fil_registrar("social_activity", "Entre amigos", "", "", 6);
  // NUVIO_SHOT_CANAIS=iguais|mistos|distintos (mistos: os 8 primeiros repetem o logo): no lugar de "Popular", a fileira de
  // canais de TV do FrostView (itens so com logo) e o catalogo que a alimenta.
  if (getenv("NUVIO_SHOT_CANAIS")) {
    static CatItem it[12];
    CatFileira f;
    int k, iguais = !strcmp(getenv("NUVIO_SHOT_CANAIS"), "iguais"), mistos = !strcmp(getenv("NUVIO_SHOT_CANAIS"), "mistos");
    fil_registrar("frostview_channel_canais", "Canais de TV - Channels", "FrostView TV", "channel", 12);
    memset(it, 0, sizeof it);
    memset(&f, 0, sizeof f);
    for (k = 0; k < 12; k++) {
      snprintf(it[k].titulo, sizeof it[k].titulo, "Canal %d", k);
      snprintf(it[k].imdb, sizeof it[k].imdb, "cs:channel:c%d", k);
      snprintf(it[k].tipo, sizeof it[k].tipo, "channel");
      snprintf(it[k].poster, sizeof it[k].poster, "deploy/app/art/logo/%02d.png", (iguais || (mistos && k < 8)) ? 0 : (k + 1) % 8);
      snprintf(it[k].backdrop, sizeof it[k].backdrop, "%s", it[k].poster);
    }
    snprintf(f.chave, sizeof f.chave, "frostview_channel_canais");
    snprintf(f.titulo, sizeof f.titulo, "Canais de TV - Channels");
    snprintf(f.tipo, sizeof f.tipo, "channels");
    f.ini = 0; f.n = 12;
    cat_definir_tudo(it, 12, &f, 1);
  } else
  fil_registrar("com.linvo.cinemeta_movie_top", "Popular", "Cinemeta", "movie", 40);
  fil_registrar("xperience_series_foryou", "For You", "Xperience", "series", 24);
  fil_registrar("collection_a24", "A24", "", "", 9);
  fil_registrar("tmdb.addon_movie_trending", "Em alta", "TMDB", "movie", 20);
  fil_registrar("aiostreams_series_novos", "Séries novas", "AIOStreams", "series", 18);
  fil_registrar("akashi_movie_anime", "Anime", "Akashi", "movie", 30);
  fil_registrar("mdblist_movie_oscar", "Vencedores do Oscar", "MDBList", "movie", 15);
  fil_registrar("sem.nome_movie_x", "", "", "", -1);
  // Mais catalogos do que o limite (7): os que passam ficam NA FILA. E dois
  // removidos, para a aba "Fora da Home" ter o que agrupar por addon.
  fil_registrar("xperience_movie_acao", "Ação", "Xperience", "movie", 12);
  fil_registrar("xperience_movie_terror", "Terror", "Xperience", "movie", 12);
  fil_registrar("xperience_series_animes", "Animes", "Xperience", "series", 12);
  fil_registrar("aiostreams_movie_top", "Top 100", "AIOStreams", "movie", 12);
  fil_registrar("akashi_series_dorama", "Doramas", "Akashi", "series", 12);
  fil_registrar("akashi_movie_bollywood", "Bollywood", "Akashi", "movie", 12);
  fil_remover(7);   // Anime
  fil_remover(9);   // sem nome
  fil_remover(12);  // Animes
  fileiras_prontas:

  ajustes_iniciar();
  // NUVIO_SHOT_UPSCALE=<algorithm 0..5> (NUVIO_SHOT_NITIDEZ=0..4): every capture
  // is drawn at 720p and upscaled to 1920x1080 by gpunivel, as on the C9.
  if (getenv("NUVIO_SHOT_UPSCALE")) {
    gpun_iniciar(1920, 1080);
    gpun_forcar_720();
    gpun_ampliar_app(1);
    gpun_ampliacao(atoi(getenv("NUVIO_SHOT_UPSCALE")),
                   getenv("NUVIO_SHOT_NITIDEZ") ? atoi(getenv("NUVIO_SHOT_NITIDEZ")) : 2);
  }
  if (getenv("NV_TEXT_SCALE_NAV")) {
    int op, sec, i, found = 0;
    const char *key, *label;
    for (i = 0; ajustes_teste_cena_item(i, &op, &sec, &key, &label); i++) {
      if (strcmp(key, "escalaTextoLocal")) continue;
      assert(ajustes_teste_focar_opcao(op));
      found++;
    }
    assert(found == 1);
    puts("PASS text scale settings navigation");
    return 0;
  }
  if (getenv("NUVIO_SHOT_AJUSTES_ESCALA"))
    ajustes_teste_escala(atoi(getenv("NUVIO_SHOT_AJUSTES_ESCALA")));
  ajustes_teste_vidro_env();   // NUVIO_SHOT_VIDRO_OPAC / _FOSCO
  // NUVIO_SHOT_LAYOUT=1: Settings as one stacked list (#339).
  if (getenv("NUVIO_SHOT_LAYOUT")) ajustes_teste_layout(atoi(getenv("NUVIO_SHOT_LAYOUT")));
  // NUVIO_SHOT_TEMA=<indice> (0 branco, 12 Dinamica) e NUVIO_SHOT_VIDRO=1: o
  // foco no acento claro com e sem vidro (#202).
  if (getenv("NUVIO_SHOT_TEMA") || getenv("NUVIO_SHOT_VIDRO"))
    ajustes_teste_tema(getenv("NUVIO_SHOT_TEMA") ? atoi(getenv("NUVIO_SHOT_TEMA")) : -1,
                       getenv("NUVIO_SHOT_VIDRO") && atoi(getenv("NUVIO_SHOT_VIDRO")));

  // NUVIO_VIDRO_TELAS=<dir> (tests/vidro_telas.sh): glass budget, first
  // category (the settings list) static and while moving focus down it.
  if (vt_ligado()) {
    vt_vidro();
    ajustes_teste_ux_captura(10);
    for (i = 0; i < 150; i++) {
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro(); vt_antes();
      ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
      glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
      ajustes_desenhar(SDL_GetTicks()); vt_depois();
      if (i == 119) vt_zerar();
      if (i == 149) { vt_png("ajustes"); vt_relatar("settings", 1); }
      SDL_Delay(2);
    }
    vt_zerar();
    for (i = 0; i < 32; i++) {
      if (i % 4 == 0) tecla(i % 8 ? SDLK_UP : SDLK_DOWN);
      SDL_PumpEvents(); txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6); gfx_novo_quadro(); vt_antes();
      ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
      glClearColor(0.025f, 0.025f, 0.03f, 1.0f); glClear(GL_COLOR_BUFFER_BIT);
      ajustes_desenhar(SDL_GetTicks()); vt_depois();
    }
    vt_relatar("settings", 0);
    return vtFalhas ? 1 : 0;
  }

#ifdef AJUSTES_PERF
  if (getenv("NUVIO_AJUSTES_PERF")) { perf_walk(w); return 0; }
#endif

  // NUVIO_SHOT_CENAS=<pasta>: the generated preview of EVERY visible option,
  // drawn alone at a fixed instant (960x540 PNG each) plus indice.tsv, to find
  // options that share the same scene.
  if (getenv("NUVIO_SHOT_CENAS")) {
    const char *dir = getenv("NUVIO_SHOT_CENAS");
    float t = getenv("NUVIO_SHOT_CENAS_T") ? (float)atof(getenv("NUVIO_SHOT_CENAS_T")) : 1.7f;
    int op, sec, n;
    const char *chave, *rot;
    FILE *ix;
    snprintf(nome, sizeof nome, "%s/indice.tsv", dir);
    ix = fopen(nome, "w");
    assert(ix);
    for (n = 0; ajustes_teste_cena_item(n, &op, &sec, &chave, &rot); n++) {
      unsigned char *pix = malloc(960 * 540 * 4);
      SDL_Surface *s;
      int q, y;
      assert(pix);
      for (q = 0; q < 8; q++) {   // text lines rasterize over a few frames
        SDL_PumpEvents();
        txt_novo_quadro(); tex_novo_quadro(); tex_bombear(6);
        glClearColor(0, 0, 0, 1);
        glClear(GL_COLOR_BUFFER_BIT);
        ajustes_teste_cena_desenhar(op, t, 0, 0, 960);
        if (q == 7) glReadPixels(0, 1080 - 540, 960, 540, GL_RGBA, GL_UNSIGNED_BYTE, pix);
        SDL_GL_SwapWindow(w);
      }
      s = SDL_CreateRGBSurfaceWithFormat(0, 960, 540, 32, SDL_PIXELFORMAT_RGBA32);
      assert(s);
      for (y = 0; y < 540; y++) memcpy((char *)s->pixels + y * s->pitch, pix + (539 - y) * 960 * 4, 960 * 4);
      snprintf(nome, sizeof nome, "%s/%03d-%s.png", dir, n, chave && chave[0] ? chave : "sem");
      assert(IMG_SavePNG(s, nome) == 0);
      fprintf(ix, "%d\t%d\t%d\t%s\t%s\n", n, sec, op, chave ? chave : "", rot ? rot : "");
      SDL_FreeSurface(s); free(pix);
    }
    fclose(ix);
    printf("cenas: %d em %s\n", n, dir);
    return 0;
  }

  // Reuse the compiled harness to compare Settings at 80% and 90%.
  // NUVIO_AJUSTES_ESCALA_COMPARAR=1 /tmp/nuvio-ajustes-shot /tmp/settings
  if (getenv("NUVIO_AJUSTES_ESCALA_COMPARAR")) {
    static const int percentuais[] = {80, 90};
    static const char *const cenas[] = {"v2-menu", "v2-aberto"};
    const float zoomGlobal = gfx_escala_ui(), escalaAtiva = gfx_escala();
    for (int p = 0; p < 2; p++) {
      ajustes_teste_escala(percentuais[p]);
      for (int c = 0; c < 2; c++) {
        assert(ajustes_teste_quadro(cenas[c]));
        assert(fabsf(ajustes_tamanho_ajustes() - percentuais[p] / 100.0f) < 0.001f);
        snprintf(nome, sizeof nome, "%s-%d-%s.png", saida, percentuais[p], cenas[c]);
        captura(nome, w);
        assert(gfx_escala_ui() == zoomGlobal);
        assert(gfx_escala() == escalaAtiva);
      }
    }
    goto fim_capturas;
  }

  // OS QUADROS DO MOCKUP (ajustes-mockup.html): NUVIO_AJ_QUADROS="principal
  // cor ..." grava <saida>-<id>.png de cada um, com a ilha do relogio.
  if (getenv("NUVIO_AJ_QUADROS")) {
    char lista[1024], *id, *ctx = NULL;
    snprintf(lista, sizeof lista, "%s", getenv("NUVIO_AJ_QUADROS"));
    for (id = strtok_r(lista, " ", &ctx); id; id = strtok_r(NULL, " ", &ctx)) {
      // "id@N": grava o quadro N depois de chegar nele (o padrao e 60, a tela
      // ja parada). Para ver a TROCA de cena no meio do caminho:
      //   NUVIO_AJ_QUADROS="reproducao op:pausaOverlay@8"
      { char *arroba = strchr(id, '@');
        quadrosCaptura = 60;
        if (arroba) { *arroba = 0; quadrosCaptura = atoi(arroba + 1) > 0 ? atoi(arroba + 1) : 60; } }
      // "busca:<texto>": a busca de Ajustes (Spotlight no modo Ajustes) com o
      // texto digitado, por cima do indice. '_' vira espaco.
      if (!strncmp(id, "busca:", 6)) {
        char q[128]; int k;
        SDL_Event e = { 0 };
        snprintf(q, sizeof q, "%s", id + 6);
        for (k = 0; q[k]; k++) if (q[k] == '_') q[k] = ' ';
        ajustes_teste_quadro("v2-menu");
        spot_abrir_ajustes(0);
        spot_texto_externo(q);
        e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
        for (k = 0; k < 4 && spot_linha_focada() < 0; k++) spot_evento(&e);
        for (k = 0; id[k]; k++) if (id[k] == ':') id[k] = '-';
        snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
        captura(nome, w);
        if (spot_aberto()) spot_fechar();
        continue;
      }
      if (!ajustes_teste_quadro(id)) { printf("quadro desconhecido: %s\n", id); continue; }
      if (getenv("NUVIO_SHOT_LAYOUT")) ajustes_teste_layout(atoi(getenv("NUVIO_SHOT_LAYOUT")));
      // NUVIO_SHOT_FONTE=3 (TXT_FAMILIA_*): the interface font the TV uses
      // (Montserrat on the owner's Android TV), so truncation shows like there.
      if (getenv("NUVIO_SHOT_FONTE")) ajustes_teste_fonte_interface(atoi(getenv("NUVIO_SHOT_FONTE")));
      // NUVIO_SHOT_TECLAS="dddr": keys sent after the frame is set up (u d l r,
      // o = OK, b = Back), to capture a scrolled list or a changed value.
      if (getenv("NUVIO_SHOT_TECLAS")) {
        const char *t;
        // One frame first: the header pills measure their rows while drawing,
        // and the navigation reads that layout.
        ajustes_atualizar(1.0f / 60.0f, SDL_GetTicks());
        ajustes_desenhar(SDL_GetTicks());
        for (t = getenv("NUVIO_SHOT_TECLAS"); *t; t++)
          tecla(*t == 'u' ? SDLK_UP : *t == 'd' ? SDLK_DOWN : *t == 'l' ? SDLK_LEFT : *t == 'r' ? SDLK_RIGHT
                : *t == 'o' ? SDLK_RETURN : SDLK_ESCAPE);
      }
      if (!strcmp(id, "guia-busca")) {
        int k;
        SDL_Event e = { 0 };
        spot_abrir_guia();
        spot_texto_externo("legenda");
        e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_DOWN;
        for (k = 0; k < 4 && spot_linha_focada() < 0; k++) spot_evento(&e);
      }
      if (quadrosCaptura != 60) snprintf(nome, sizeof nome, "%s-%s-q%d.png", saida, id, quadrosCaptura);
      else snprintf(nome, sizeof nome, "%s-%s.png", saida, id);
      captura(nome, w);
      quadrosCaptura = 60;
      if (spot_aberto()) spot_fechar();
    }
    goto fim_capturas;
  }

  // A LINHA "Procurar atualização" NOS ESTADOS (01/10/2026): sem versao nova
  // (antes de procurar, procurando, em dia, sem rede) e com versao nova.
  if (getenv("NUVIO_AJUSTES_ATUALIZAR")) {
    static const struct { int busca; const char *tag; const char *nome; } est[] = {
      { ATUALIZACAO_BUSCA_NADA, "", "nada" },
      { ATUALIZACAO_BUSCA_PROCURANDO, "", "procurando" },
      { ATUALIZACAO_BUSCA_EM_DIA, "", "em-dia" },
      { ATUALIZACAO_BUSCA_ERRO, "", "erro" },
      { ATUALIZACAO_BUSCA_NOVA, "9.9.9", "nova" },
    };
    int op = ajustes_teste_op_atualizar();
    for (i = 0; i < (int)(sizeof est / sizeof *est); i++) {
      atualizacao_teste_estado(est[i].busca, est[i].tag);
      assert(ajustes_teste_focar_opcao(op));
      snprintf(nome, sizeof nome, "%s-atualizar-%s.png", saida, est[i].nome);
      captura(nome, w);
    }
    goto fim_capturas;
  }

  // A GALERIA DO "Ícone do app" (apoiadores), forcada pelo portao do dono:
  // NUVIO_AJUSTES_ICONE=1 grava -icone-<id>.png com tres escolhas diferentes.
  if (getenv("NUVIO_AJUSTES_ICONE")) {
    static const int esc[] = { 0, 1, 7 };
    setenv("NUVIO_APOIADOR", "1", 1);
    apoiador_reler();
    iconeapp_iniciar("deploy/app/art");
    logoapp_iniciar("deploy/app/art");
    for (i = 0; i < 3; i++) {
      int op = ajustes_teste_op_icone(esc[i]);
      assert(ajustes_teste_focar_opcao(op));
      snprintf(nome, sizeof nome, "%s-icone-%s.png", saida, iconeapp_id(esc[i]));
      SDL_Event e = {0}; e.type = SDL_KEYDOWN; e.key.keysym.sym = SDLK_RETURN;
      ajustes_evento(&e);
      captura(nome, w);
      e.key.keysym.sym = SDLK_ESCAPE; ajustes_evento(&e);
    }
    goto fim_capturas;
  }

  if (getenv("NUVIO_AJUSTES_OPCOES")) {
    static const char *const FAMILIAS[] = {
      "reproducao", "home", "continuar", "detalhe", "foco", "profundidade",
      "cartaz", "interface", "conta", "rastreio", "sobre", "tmdb", "mdblist",
      "tv", "acao"
    };
    FILE *manifest;
    const char *dir = saida;
    quadrosCaptura = 60;
    snprintf(nome, sizeof nome, "%s/coverage.tsv", dir);
    manifest = fopen(nome, "w");
    assert(manifest);
    assert(sizeof AJ_IDS / sizeof *AJ_IDS == 116);
    for (i = 0; i < (int)(sizeof AJ_IDS / sizeof *AJ_IDS); i++) {
      int familia = ajustes_teste_familia_previa(i);
      assert(familia >= 0 && familia < (int)(sizeof FAMILIAS / sizeof *FAMILIAS));
      assert(ajustes_teste_focar_opcao(i));
      snprintf(nome, sizeof nome, "%s/%s.png", dir, AJ_IDS[i]);
      captura(nome, w);
      fprintf(manifest, "%s\t%s\n", AJ_IDS[i], FAMILIAS[familia]);
    }
    /* A amostra mostra também as alternativas mais pedidas, sem gravá-las. */
    { static const struct { int familia; const char *sufixo; } fontes[] = {
        { TXT_FAMILIA_INTER, "Inter" }, { TXT_FAMILIA_MONTSERRAT, "Montserrat" },
        { TXT_FAMILIA_ROBOTO, "Roboto" }, { TXT_FAMILIA_ATKINSON, "Atkinson" }
      };
      for (i = 0; i < (int)(sizeof fontes / sizeof *fontes); i++) {
        ajustes_teste_fonte_interface(fontes[i].familia);
        assert(ajustes_teste_focar_opcao((int)(sizeof AJ_IDS / sizeof *AJ_IDS) - 1));
        snprintf(nome, sizeof nome, "%s/AJ_FONTE_UI-%s.png", dir, fontes[i].sufixo);
        captura(nome, w);
      }
    }
    fclose(manifest);
    goto fim_capturas;
  }

  // NUVIO_SHOT_PERFIL=1 (2.1): so as capturas de ajustes por perfil. Use com
  // NUVIO_SHOT_DADOS=<pasta vazia> para as marcas de perfil infantil gravarem.
  if (getenv("NUVIO_SHOT_PERFIL")) {
    static const char *const nomes[] = { "chip", "selos", "menu", "copiar", "pin-admin", "bloqueio", "editando" };
    static const int ordem[7] = { 0, 1, 2, 3, 5, 6, 4 };   /* o PIN por ultimo: o teclado fica aberto */
    for (i = 0; i < 7; i++) {
      ajustes_teste_ux_captura(30 + ordem[i]);
      snprintf(nome, sizeof nome, "%s-perfil-%s.png", saida, nomes[ordem[i]]);
      captura(nome, w);
    }
    goto fim_capturas;
  }

  // Cenários do novo contrato: categoria, seleção, cancelamento,
  // dependência e restauração. Só fixtures em memória, sem dados pessoais.
  static const char *const cenarios[] = {
    "indice", "tela-inicial", "seletor", "dependencia", "diferencas", "restaurar", "avancados", "numero", "cabecalho", "english"
  };
  for (i = 0; i < (int)(sizeof cenarios / sizeof *cenarios); i++) {
    ajustes_teste_ux_captura(i);
    snprintf(nome, sizeof nome, "%s-%s.png", saida, cenarios[i]);
    captura(nome, w);
  }
  for (i = 21; i <= 24; i++) {
    static const char *const n[] = { "trailers", "trailer-seletor", "ampliacao", "ampliacao-seletor" };
    ajustes_teste_ux_captura(i);
    snprintf(nome, sizeof nome, "%s-%s.png", saida, n[i - 21]);
    captura(nome, w);
  }
  if (!getenv("NUVIO_AJUSTES_UX")) {
    for (i = 0; i < 11; i++) {
      ajustes_teste_ux_captura(10 + i);
      snprintf(nome, sizeof nome, "%s-categoria-%02d.png", saida, i + 1);
      captura(nome, w);
    }
  }

  fim_capturas:
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(gl);
  SDL_DestroyWindow(w);
  SDL_Quit();
  puts("PASS: capturas da tela de Ajustes gravadas.");
  return 0;
}
