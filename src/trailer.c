#include "trailer.h"
#include "layout.h"
#include <stdio.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int    aberto, cheia, comSom;
static GfxRect rect;
static char   ytAtual[24];

#ifdef __EMSCRIPTEN__
// O iframe fica ATRAS do canvas (z-index 0 contra 1 do canvas), no mesmo
// enquadramento 16:9 que a folha de estilo da ao canvas — por isso as
// medidas sao calculadas a partir do retangulo REAL do canvas na janela, e
// nao de 1920x1080 diretos. `origin` no embed e o que a IFrame API exige
// para aceitar postMessage; a origem de um wgt e "file://" ou "null", e ai
// vai sem.
EM_JS(int, trailer_js_abrir, (const char *yt, float x, float y, float w, float h, int som, float zoom), {
  var id = UTF8ToString(yt);
  if (!/^[A-Za-z0-9_-]{6,20}$/.test(id)) return 0;
  var T = Module.nvTrailer || (Module.nvTrailer = { f: null, id: '', estado: -1, som: 0 });
  var cv = document.getElementById('canvas');
  if (!cv) return 0;
  var r = cv.getBoundingClientRect();
  var sx = r.width / 1920, sy = r.height / 1080;
  if (!T.f || T.id !== id) {
    if (T.f && T.f.parentNode) T.f.parentNode.removeChild(T.f);
    var f = document.createElement('iframe');
    var org = (location.origin && location.origin !== 'null' && location.origin.indexOf('http') === 0) ? '&origin=' + encodeURIComponent(location.origin) : '';
    f.src = 'https://www.youtube.com/embed/' + id + '?autoplay=1&mute=' + (som ? 0 : 1) +
            '&controls=0&enablejsapi=1&rel=0&modestbranding=1&playsinline=1&iv_load_policy=3&fs=0&disablekb=1' + org;
    f.setAttribute('allow', 'autoplay; encrypted-media');
    f.setAttribute('frameborder', '0');
    f.tabIndex = -1;
    f.style.cssText = 'position:absolute;border:0;z-index:0;background:#000;pointer-events:none;';
    (document.body || document.documentElement).appendChild(f);
    T.f = f; T.id = id; T.estado = -1; T.som = som;
    if (!T.ouvinte) {
      T.ouvinte = 1;
      window.addEventListener('message', function (ev) {
        if (typeof ev.data !== 'string' || ev.origin.indexOf('youtube.com') < 0) return;
        var m; try { m = JSON.parse(ev.data); } catch (e) { return; }
        if (!m) return;
        if (m.event === 'onReady' && T.f) {
          T.f.contentWindow.postMessage(JSON.stringify({ event: 'listening', id: 1 }), '*');
        }
        if (m.event === 'infoDelivery' && m.info && typeof m.info.playerState === 'number') T.estado = m.info.playerState;
        if (m.event === 'onStateChange' && typeof m.info === 'number') T.estado = m.info;
      });
    }
    // A IFrame API so fala depois de "listening"; mandamos ao carregar.
    f.addEventListener('load', function () {
      try { f.contentWindow.postMessage(JSON.stringify({ event: 'listening', id: 1 }), '*'); } catch (e) {}
    });
  }
  // `zoom` > 1 amplia o embed em volta do centro do retangulo: o furo do
  // canvas so deixa ver o retangulo, entao o que sobra e cortado — e o que
  // tira as tarjas pretas de um trailer 2.39:1 dentro do player 16:9.
  T.f.style.left = (r.left + (x - w * (zoom - 1) / 2) * sx) + 'px';
  T.f.style.top = (r.top + (y - h * (zoom - 1) / 2) * sy) + 'px';
  T.f.style.width = (w * zoom * sx) + 'px';
  T.f.style.height = (h * zoom * sy) + 'px';
  if (T.som !== som && T.f.contentWindow) {
    T.som = som;
    try { T.f.contentWindow.postMessage(JSON.stringify({ event: 'command', func: som ? 'unMute' : 'mute', args: [] }), '*'); } catch (e) {}
  }
  return 1;
});
EM_JS(void, trailer_js_cmd, (const char *cmd), {
  var T = Module.nvTrailer;
  if (!T || !T.f || !T.f.contentWindow) return;
  try { T.f.contentWindow.postMessage(JSON.stringify({ event: 'command', func: UTF8ToString(cmd), args: [] }), '*'); } catch (e) {}
});
EM_JS(void, trailer_js_fechar, (), {
  var T = Module.nvTrailer;
  if (!T) return;
  if (T.f && T.f.parentNode) T.f.parentNode.removeChild(T.f);
  T.f = null; T.id = ''; T.estado = -1;
});
EM_JS(int, trailer_js_estado, (), {
  var T = Module.nvTrailer;
  return (T && T.f) ? T.estado : -2;
});
int trailer_suportado(void) { return 1; }
#else
int trailer_suportado(void) { return 0; }
#endif

void trailer_abrir(const char *yt, GfxRect r, int som, int modoCheia) {
  if (!trailer_suportado() || !yt || !yt[0]) return;
#ifdef __EMSCRIPTEN__
  if (!trailer_js_abrir(yt, r.x, r.y, r.w, r.h, som, NV_TRAILER_ZOOM)) return;
#endif
  if (strcmp(ytAtual, yt)) {
    snprintf(ytAtual, sizeof ytAtual, "%s", yt);
    printf("[trailer] %s %s%s\n", yt, modoCheia ? "tela cheia" : "no fundo", som ? " com som" : " mudo");
    fflush(stdout);
  }
  rect = r; aberto = 1; cheia = modoCheia; comSom = som;
}

void trailer_rect(GfxRect r) {
  if (!aberto) return;
  rect = r;
#ifdef __EMSCRIPTEN__
  trailer_js_abrir(ytAtual, r.x, r.y, r.w, r.h, comSom, NV_TRAILER_ZOOM);
#endif
}

void trailer_fechar(void) {
  if (!aberto) return;
#ifdef __EMSCRIPTEN__
  trailer_js_fechar();
#endif
  aberto = 0; cheia = 0; ytAtual[0] = 0;
}

int trailer_aberto(void)  { return aberto; }
int trailer_cheia(void)   { return aberto && cheia; }
int trailer_tocando(void) {
#ifdef __EMSCRIPTEN__
  // 1 = PLAYING, 3 = BUFFERING (ja ha imagem), na IFrame API.
  int e = aberto ? trailer_js_estado() : -2;
  return e == 1 || e == 3;
#else
  return 0;
#endif
}
GfxRect trailer_retangulo(void) { return rect; }

int trailer_evento(const SDL_Event *e) {
  SDL_Keycode k;
  if (!aberto || !cheia || e->type != SDL_KEYDOWN) return 0;
  k = e->key.keysym.sym;
  if (k == SDLK_AC_BACK || k == SDLK_ESCAPE || k == SDLK_BACKSPACE || k == SDLK_DELETE ||
      e->key.keysym.scancode == NV_SCANCODE_BACK) { trailer_fechar(); return 1; }
  if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
#ifdef __EMSCRIPTEN__
    trailer_js_cmd(trailer_js_estado() == 1 ? "pauseVideo" : "playVideo");
#endif
    return 1;
  }
  return 1;   // em tela cheia o resto do teclado nao vai a pagina de tras
}

void trailer_atualizar(Uint32 agora) {
  (void)agora;
#ifdef __EMSCRIPTEN__
  // Acabou (0 = ENDED): fecha e a pagina volta a arte.
  if (aberto && trailer_js_estado() == 0) trailer_fechar();
#endif
}
