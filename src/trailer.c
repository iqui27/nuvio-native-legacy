#include "trailer.h"
#include "layout.h"
#include "ajustes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

static int    aberto, cheia, comSom;
static GfxRect rect;
static char   fonteAtual[1024];

#ifdef __EMSCRIPTEN__
// O iframe fica ATRAS do canvas (z-index 0 contra 1 do canvas), no mesmo
// enquadramento 16:9 que a folha de estilo da ao canvas — por isso as
// medidas sao calculadas a partir do retangulo REAL do canvas na janela, e
// nao de 1920x1080 diretos. `origin` no embed e o que a IFrame API exige
// para aceitar postMessage; a origem de um wgt e "file://" ou "null", e ai
// vai sem.
EM_JS(int, trailer_js_abrir, (const char *fonte, float x, float y, float w, float h, int som, float zoom), {
  var id = UTF8ToString(fonte);
  // DOIS ELEMENTOS, um por fonte: id do YouTube -> <iframe> embed; URL http
  // (MP4 do IMDb, trailerimdb.h) -> <video> do proprio navegador, mudo pelo
  // atributo, sem AVPlay (o open do AVPlay custa ~1,8 s de fio principal
  // nesta TV, e o hero troca de titulo a cada seta).
  // Sem regex com "//" aqui: o pre-processador C le como comentario.
  var ehVideo = (id.indexOf('http://') === 0 || id.indexOf('https://') === 0);
  if (!ehVideo && !/^[A-Za-z0-9_-]{6,20}$/.test(id)) return 0;
  var T = Module.nvTrailer || (Module.nvTrailer = { f: null, id: '', estado: -1, som: 0, ehVideo: 0 });
  var cv = document.getElementById('canvas');
  if (!cv) return 0;
  var r = cv.getBoundingClientRect();
  var sx = r.width / 1920, sy = r.height / 1080;
  if (!T.f || T.id !== id) {
    if (T.f && T.f.parentNode) { try { if (T.ehVideo) { T.f.pause(); T.f.removeAttribute('src'); T.f.load(); } } catch (e) {} T.f.parentNode.removeChild(T.f); }
    var f;
    if (ehVideo) {
      f = document.createElement('video');
      f.muted = !som; f.autoplay = true; f.playsInline = true; f.preload = 'auto';
      f.addEventListener('playing', function () { T.estado = 1; });
      f.addEventListener('waiting', function () { if (T.estado === 1) T.estado = 3; });
      f.addEventListener('ended', function () { T.estado = 0; });
      f.addEventListener('error', function () { T.estado = -3; });
      f.src = id;
      f.style.cssText = 'position:absolute;border:0;z-index:0;background:#000;pointer-events:none;object-fit:cover;';
      (document.body || document.documentElement).appendChild(f);
      var pr = f.play(); if (pr && pr.catch) pr.catch(function () { T.estado = -3; });
    } else {
      f = document.createElement('iframe');
      var org = (location.origin && location.origin !== 'null' && location.origin.indexOf('http') === 0) ? '&origin=' + encodeURIComponent(location.origin) : '';
      f.src = 'https://www.youtube.com/embed/' + id + '?autoplay=1&mute=' + (som ? 0 : 1) +
              '&controls=0&enablejsapi=1&rel=0&modestbranding=1&playsinline=1&iv_load_policy=3&fs=0&disablekb=1' + org;
      f.setAttribute('allow', 'autoplay; encrypted-media');
      f.setAttribute('frameborder', '0');
      f.tabIndex = -1;
      f.style.cssText = 'position:absolute;border:0;z-index:0;background:#000;pointer-events:none;';
      (document.body || document.documentElement).appendChild(f);
      if (!T.ouvinte) {
        T.ouvinte = 1;
        window.addEventListener('message', function (ev) {
          if (T.ehVideo || typeof ev.data !== 'string' || ev.origin.indexOf('youtube.com') < 0) return;
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
    T.f = f; T.id = id; T.estado = -1; T.som = som; T.ehVideo = ehVideo ? 1 : 0;
  }
  // `zoom` > 1 amplia o elemento em volta do centro do retangulo: o furo do
  // canvas so deixa ver o retangulo, entao o que sobra e cortado — e o que
  // tira as tarjas pretas de um trailer 2.39:1 dentro de um quadro 16:9.
  T.f.style.left = (r.left + (x - w * (zoom - 1) / 2) * sx) + 'px';
  T.f.style.top = (r.top + (y - h * (zoom - 1) / 2) * sy) + 'px';
  T.f.style.width = (w * zoom * sx) + 'px';
  T.f.style.height = (h * zoom * sy) + 'px';
  if (T.som !== som) {
    T.som = som;
    if (T.ehVideo) T.f.muted = !som;
    else if (T.f.contentWindow) { try { T.f.contentWindow.postMessage(JSON.stringify({ event: 'command', func: som ? 'unMute' : 'mute', args: [] }), '*'); } catch (e) {} }
  }
  return 1;
});
EM_JS(void, trailer_js_cmd, (const char *cmd), {
  var T = Module.nvTrailer;
  if (!T || !T.f) return;
  var c = UTF8ToString(cmd);
  if (T.ehVideo) { try { if (c === 'pauseVideo') T.f.pause(); else if (c === 'playVideo') T.f.play(); } catch (e) {} return; }
  if (!T.f.contentWindow) return;
  try { T.f.contentWindow.postMessage(JSON.stringify({ event: 'command', func: c, args: [] }), '*'); } catch (e) {}
});
EM_JS(void, trailer_js_fechar, (), {
  var T = Module.nvTrailer;
  if (!T) return;
  if (T.f) { try { if (T.ehVideo) { T.f.pause(); T.f.removeAttribute('src'); T.f.load(); } } catch (e) {} if (T.f.parentNode) T.f.parentNode.removeChild(T.f); }
  T.f = null; T.id = ''; T.estado = -1;
});
EM_JS(int, trailer_js_estado, (), {
  var T = Module.nvTrailer;
  return (T && T.f) ? T.estado : -2;
});
EM_JS(int, trailer_js_pausado, (), {
  var T = Module.nvTrailer;
  if (!T || !T.f) return 0;
  return T.ehVideo ? (T.f.paused ? 1 : 0) : (T.estado === 2 ? 1 : 0);
});
int trailer_suportado(void) { return 1; }
#else
// LG (e Mac, onde video_iniciar devolve 0 e nada disto acontece): o trailer
// e um MP4 no plano de video da TV. O volume so pode ser mexido depois de o
// pipeline existir (mediaId), e o recorte que tira a tarja preta so depois de
// o quadro ter tamanho — os dois ficam pendentes e trailer_atualizar aplica.
#include "video.h"
static int volumePendente, recortePendente, pausado;
// O recorte e REPETIDO nos primeiros segundos (ver reaplicarAte): o pipeline
// desta TV prende o plano em mais de um ponto depois do load (bind do ACB,
// `playing`), e um recorte pedido cedo demais pode ser engolido por um deles.
static Uint32 reaplicarAte, reaplicarEm, tocandoDesde;
static int quadroInteiroEnviado;
int trailer_suportado(void) {
#ifdef __APPLE__
  return 0;
#else
  static int sabe = -1;
  if (sabe < 0) sabe = video_iniciar() ? 1 : 0;
  return sabe;
#endif
}
static void nativoAplicar(void) {
  if (!aberto) return;
  // O uMS setVolume funciona nesta TV (provado ao contrario: sem ele o
  // trailer tocou com som).
  if (volumePendente && video_ativo()) { video_volume(comSom ? 100 : 0); volumePendente = 0; }
  // O RECORTE SO DEPOIS DE `playing` + um respiro. E a ordem do player, a
  // unica em que o recorte comprovadamente pega nesta TV: la o modo salvo vai
  // ao plano no videoInfo como quadro INTEIRO e o zoom de verdade so e pedido
  // pela pessoa com o filme ja tocando. Pedido antes de tocar, o recorte era
  // aceito (-> 1) e ignorado (20/09/2026, meia noite de tentativas).
  if (recortePendente && video_pronto() && video_largura() > 0 && video_altura() > 0 &&
      !video_tocando() && !quadroInteiroEnviado) {
    video_janela_fonte(0, 0, video_largura(), video_altura(),
                       (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    quadroInteiroEnviado = 1;
  }
  if (recortePendente && video_tocando() && !tocandoDesde) tocandoDesde = SDL_GetTicks();
  if (recortePendente && video_pronto() && video_largura() > 0 && video_altura() > 0 &&
      tocandoDesde && SDL_GetTicks() - tocandoDesde >= 800) {
    int vw = video_largura(), vh = video_altura();
    int sw = (int)(vw / ajustes_trailer_zoom()), sh = (int)(vh / ajustes_trailer_zoom());
    int sx, sy;
    // PAR, como o player faz (player.c, aplicarAspecto): o escalonador
    // trabalha em 4:2:0 e origem ou tamanho impar da meio pixel de croma na
    // borda — e 803 de altura era o que saia daqui.
    sw &= ~1; sh &= ~1;
    sx = ((vw - sw) / 2) & ~1; sy = ((vh - sh) / 2) & ~1;
    if (video_recorte_fonte())
      video_janela_fonte(sx, sy, sw, sh,
                         (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    recortePendente = 0;
    if (!reaplicarAte) { reaplicarAte = SDL_GetTicks() + 6000; reaplicarEm = SDL_GetTicks() + 1500; }
  }
  if (reaplicarAte && SDL_GetTicks() >= reaplicarEm) {
    if (SDL_GetTicks() >= reaplicarAte) reaplicarAte = 0;
    else { reaplicarEm = SDL_GetTicks() + 1500; video_recorte_reaplicar(); }
  }
}
#endif

void trailer_abrir(const char *fonte, GfxRect r, int som, int modoCheia) {
  int nova;
  if (!trailer_suportado() || !fonte || !fonte[0]) return;
  nova = strcmp(fonteAtual, fonte) != 0;
#ifdef __EMSCRIPTEN__
  if (!trailer_js_abrir(fonte, r.x, r.y, r.w, r.h, som, ajustes_trailer_zoom())) return;
#else
  if (nova) {
    if (!video_tocar(fonte)) return;
    volumePendente = 1; recortePendente = 1; pausado = 0; reaplicarAte = 0;
    tocandoDesde = 0; quadroInteiroEnviado = 0;
  } else if (comSom != som) volumePendente = 1;
  video_janela((int)r.x, (int)r.y, (int)r.w, (int)r.h);
  if (!nova) recortePendente = 1;
#endif
  if (nova) {
    snprintf(fonteAtual, sizeof fonteAtual, "%s", fonte);
    printf("[trailer] %.60s %s%s\n", fonte, modoCheia ? "tela cheia" : "no fundo", som ? " com som" : " mudo");
    fflush(stdout);
  }
  rect = r; aberto = 1; cheia = modoCheia; comSom = som;
#ifndef __EMSCRIPTEN__
  nativoAplicar();
#endif
}

void trailer_rect(GfxRect r) {
  if (!aberto) return;
  rect = r;
#ifdef __EMSCRIPTEN__
  trailer_js_abrir(fonteAtual, r.x, r.y, r.w, r.h, comSom, ajustes_trailer_zoom());
#else
  video_janela((int)r.x, (int)r.y, (int)r.w, (int)r.h);
  recortePendente = 1;
  nativoAplicar();
#endif
}

void trailer_fechar(void) {
  if (!aberto) return;
#ifdef __EMSCRIPTEN__
  trailer_js_fechar();
#else
  video_parar();
#endif
  aberto = 0; cheia = 0; fonteAtual[0] = 0;
}

int trailer_aberto(void)  { return aberto; }
int trailer_cheia(void)   { return aberto && cheia; }
int trailer_tocando(void) {
#ifdef __EMSCRIPTEN__
  // 1 = PLAYING, 3 = BUFFERING (ja ha imagem), na IFrame API.
  int e = aberto ? trailer_js_estado() : -2;
  return e == 1 || e == 3;
#else
  return aberto && video_pronto() && !video_falhou() && !video_terminou();
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
    trailer_js_cmd(trailer_js_pausado() ? "playVideo" : "pauseVideo");
#else
    pausado = !pausado;
    video_pausar(pausado);
#endif
    return 1;
  }
  return 1;   // em tela cheia o resto do teclado nao vai a pagina de tras
}

void trailer_atualizar(Uint32 agora) {
  (void)agora;
#ifdef __EMSCRIPTEN__
  // Acabou (0 = ENDED) ou falhou (-3, so o <video>): fecha e a arte volta.
  if (aberto && (trailer_js_estado() == 0 || trailer_js_estado() == -3)) trailer_fechar();
#else
  if (!aberto) return;
  video_bombear();
  nativoAplicar();
  // Acabou ou a fonte falhou: fecha e a pagina volta a arte.
  if (video_terminou() || video_falhou()) trailer_fechar();
#endif
}
