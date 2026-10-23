#include "trailer.h"
#include "plrui.h"
#include "text.h"
#include "layout.h"
#include "ajustes.h"
#include "trailerfonte.h"
#include "extras.h"
#include "linguas.h"
#include "video.h"      // trailer_osd_desenhar reads video_pos/duracao on every target
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#ifndef NV_REC_URL
#define NV_REC_URL ""
#endif

static int    aberto, cheia, comSom;
static unsigned geracao;   // invalida a prova do player mesmo depois do trailer fechar
static int    falhouUltima;
static GfxRect rect;
static char   fonteAtual[1024];
static Uint32 abertoEm;   // SDL_GetTicks da abertura da fonte atual, para o log
static int    dono;       // TRAILER_DONO_* (trailer.h)
static int    noCartao;   // trailer_no_cartao: quadro inteiro encaixado no .tpk
static char   donoImdb[32];
static int    falso;      // trailer_teste_falso: 0 off, 1 open without image, 2 playing

void trailer_teste_falso(int tocando) { falso = tocando < 0 ? 0 : 1 + (tocando != 0); }

void trailer_recorte(int vw, int vh, float z, float dw, float dh,
                     int *sx, int *sy, int *sw, int *sh) {
  int w, h;
  // QUADRO MATTED (Apple: 1920x804, 3836x1606) nao tem tarja embutida — a
  // tarja e o proprio plano encaixando 2.39 em 16:9. Encher a tela e
  // recortar as LATERAIS ate 16:9, sem o zoom fixo. Quadro 16:9 (IMDb, com
  // a tarja dentro da imagem) leva o zoom do ajuste. "Original" (1.0) nao
  // tira zoom nenhum em nenhum dos dois.
  if (z <= 1.001f) { w = vw; h = vh; }
  else if ((float)vw / (float)vh > 1.85f) { h = vh; w = (int)(vh * 16.0f / 9.0f); if (w > vw) w = vw; }
  else { w = (int)(vw / z); h = (int)(vh / z); }
  // E DEPOIS O "COVER" NO DESTINO. O plano estica a fonte no retangulo de
  // destino, sem guardar proporcao: a conta acima so servia a um destino
  // 16:9. O banner do layout Padrao e 1920x528 (3,6:1) e MEDIDO no log da C9
  // do dono (01/10/2026, 1.6.4): "fonte 244,138 1432x804 -> destino 0,0
  // 1920x528" — o trailer achatado pela metade na altura, a foto dele de
  // "so a imagem espremida". Corta-se a fonte na proporcao do destino, pelo
  // centro, como a arte do destaque faz (cover).
  //
  // Destino MAIS LARGO que o recorte do zoom: a largura volta a crescer ate
  // o quadro inteiro antes de se cortar altura. O zoom existe para tirar a
  // tarja de CIMA e de BAIXO; as laterais que ele cortava eram so para manter
  // 16:9. No banner a altura que sobra (528 de 1920) ja cai dentro da imagem,
  // sem tarja, e o trailer sai sem zoom a mais.
  if (dw > 1.0f && dh > 1.0f && w > 0 && h > 0) {
    float ad = dw / dh, af = (float)w / (float)h;
    if (af < ad * 0.995f) {
      float qw = (float)h * ad;
      if (qw > (float)vw) qw = (float)vw;
      w = (int)qw;
      h = (int)(qw / ad);
    } else if (af > ad * 1.005f) {
      w = (int)((float)h * ad);
    }
  }
  // PAR, como o player faz (player.c, aplicarAspecto): o escalonador
  // trabalha em 4:2:0 e origem ou tamanho impar da meio pixel de croma na
  // borda — e 803 de altura era o que saia daqui.
  w &= ~1; h &= ~1;
  *sw = w; *sh = h;
  *sx = ((vw - w) / 2) & ~1; *sy = ((vh - h) / 2) & ~1;
}

#ifdef __EMSCRIPTEN__
// O iframe fica ATRAS do canvas (z-index 0 contra 1 do canvas), no mesmo
// enquadramento 16:9 que a folha de estilo da ao canvas — por isso as
// medidas sao calculadas a partir do retangulo REAL do canvas na janela, e
// nao de 1920x1080 diretos. `origin` no embed e o que a IFrame API exige
// para aceitar postMessage; a origem de um wgt e "file://" ou "null", e ai
// vai sem.
// Legenda do trailer do YouTube: a do idioma de legenda da pessoa, nunca com
// audio dublado nesse idioma, nunca sem faixa nesse idioma. `sub`/`audio` sao
// ISO de 2 letras ("" = sem preferencia / audio desconhecido).
EM_JS(void, trailer_js_legenda, (const char *sub, const char *audio), {
  Module.nvLegendaEscolher = function (tracks, s, a) {
    if (!s || s === 'none' || s === a || !tracks) return '';
    for (var i = 0; i < tracks.length; i++) {
      var c = tracks[i] && tracks[i].languageCode;
      if (c && c.slice(0, 2).toLowerCase() === s) return c;
    }
    return '';
  };
  Module.nvLegenda = { sub: UTF8ToString(sub), audio: UTF8ToString(audio) };
});
EM_JS(int, trailer_js_abrir, (const char *fonte, float x, float y, float w, float h, int som, float zoom, const char *proxy), {
  var id = UTF8ToString(fonte);
  var base = proxy ? UTF8ToString(proxy) : '';
  // DOIS ELEMENTOS, um por fonte: id do YouTube -> <iframe> embed; URL http
  // (MP4 do IMDb, trailerimdb.h) -> <video> do proprio navegador, mudo pelo
  // atributo, sem AVPlay (o open do AVPlay custa ~1,8 s de fio principal
  // nesta TV, e o hero troca de titulo a cada seta).
  // Sem regex com "//" aqui: o pre-processador C le como comentario.
  var ehVideo = (id.indexOf('http://') === 0 || id.indexOf('https://') === 0);
  if (!ehVideo && !/^[A-Za-z0-9_-]{6,20}$/.test(id)) return 0;
  var T = Module.nvTrailer || (Module.nvTrailer = {
    f: null, id: '', estado: -1, som: 0, ehVideo: 0, geracao: 0
  });
  var cv = document.getElementById('canvas');
  if (!cv) return 0;
  var r = cv.getBoundingClientRect();
  var sx = r.width / 1920, sy = r.height / 1080;
  if (!T.f || T.id !== id) {
    var geracao = (T.geracao || 0) + 1;
    if (T.f && T.f.parentNode) { try { if (T.ehVideo) { T.f.pause(); T.f.removeAttribute('src'); T.f.load(); } } catch (e) {} T.f.parentNode.removeChild(T.f); }
    var f;
    if (ehVideo) {
      f = document.createElement('video');
      f.muted = !som; f.autoplay = true; f.playsInline = true; f.preload = 'auto';
      // O elemento anterior pode emitir `error`/`ended` depois de ser
      // removido. Publicar esse estado no objeto global fazia a sessao nova
      // fechar ou ficar em buffering assim que A -> B -> A acontecia.
      // DIAGNOSTICO (log 1646, Samsung 1.4.1: o C abria o HLS e depois nao
      // havia UMA linha sobre o video). Uma linha por transicao, pelo mesmo
      // canal do printf (window.__nvDiag -> painel, nv-log, "Enviar
      // registro"); waiting/stalled so a primeira de cada, para nao inundar.
      var t0 = Date.now(), vistos = {};
      var diz = function (ev, extra) {
        if (vistos[ev]) return; vistos[ev] = 1;
        var s = '[trailer-js] g' + geracao + ' ' + ev + ' +' + (Date.now() - t0) + 'ms' + (extra ? ' ' + extra : '');
        try { console.log(s); if (window.__nvDiag) window.__nvDiag(s); } catch (e) {}
      };
      f.addEventListener('loadedmetadata', function () { diz('loadedmetadata', f.videoWidth + 'x' + f.videoHeight + ' dur=' + (f.duration || 0).toFixed(1)); });
      f.addEventListener('canplay', function () { diz('canplay'); });
      f.addEventListener('stalled', function () { diz('stalled'); });
      f.addEventListener('playing', function () { diz('playing'); if (T.f === f && T.geracao === geracao) T.estado = 1; });
      f.addEventListener('waiting', function () { diz('waiting'); if (T.f === f && T.geracao === geracao && T.estado === 1) T.estado = 3; });
      f.addEventListener('ended', function () { diz('ended'); if (T.f === f && T.geracao === geracao) T.estado = 0; });
      f.addEventListener('error', function () {
        var me = f.error;
        diz('error', 'code=' + (me ? me.code : '?') + ' ' + (me && me.message ? me.message : ''));
        if (T.f === f && T.geracao === geracao) T.estado = -3;
      });
      setTimeout(function () { if (!vistos.playing && T.f === f) diz('sem playing em 8 s', 'readyState=' + f.readyState + ' networkState=' + f.networkState); }, 8000);
      T.f = f; T.id = id; T.estado = -1; T.som = som; T.ehVideo = 1; T.geracao = geracao;
      f.src = id;
      f.style.cssText = 'position:absolute;border:0;z-index:0;background:#000;pointer-events:none;object-fit:cover;';
      (document.body || document.documentElement).appendChild(f);
      var pr = f.play(); if (pr && pr.catch) pr.catch(function (e) { diz('play() recusado', e && e.name ? e.name : ''); if (T.f === f && T.geracao === geracao) T.estado = -3; });
    } else {
      f = document.createElement('iframe');
      // ERRO 153 (#136, AU7000): o embed direto de um wgt vai sem Referer e
      // sem origem, e o YouTube responde "Video player configuration error".
      // Com o servico de recomendacoes na build, o iframe abre a PAGINA dele
      // (/v1/trailer/yt), que embute o player com origem https valida e
      // repassa o postMessage. Sem o servico, o embed direto de antes.
      var viaProxy = base.indexOf('https://') === 0;
      var org = (location.origin && location.origin !== 'null' && location.origin.indexOf('http') === 0) ? '&origin=' + encodeURIComponent(location.origin) : '';
      f.src = viaProxy
        ? base + '/v1/trailer/yt?id=' + id + '&mute=' + (som ? 0 : 1)
        : 'https://www.youtube.com/embed/' + id + '?autoplay=1&mute=' + (som ? 0 : 1) +
          '&controls=0&enablejsapi=1&rel=0&modestbranding=1&playsinline=1&iv_load_policy=3&fs=0&disablekb=1' + org;
      f.nvOrigem = viaProxy ? base.replace(/^(https:[/][/][^/]+).*$/, '$1') : '';
      f.setAttribute('allow', 'autoplay; encrypted-media');
      f.setAttribute('frameborder', '0');
      f.tabIndex = -1;
      f.style.cssText = 'position:absolute;border:0;z-index:0;background:#000;pointer-events:none;';
      f.nvGeracao = geracao;
      T.f = f; T.id = id; T.estado = -1; T.som = som; T.ehVideo = 0; T.geracao = geracao;
      T.pronto = 0;
      (document.body || document.documentElement).appendChild(f);
      // SEM onReady EM 10 s = o player nao vai falar (rota fora do ar, erro
      // antes da API, rede): vira erro (-3) e quem chamou anda para a
      // proxima fonte ou fecha, em vez de deixar a tela de erro do YouTube.
      setTimeout(function () {
        if (T.f !== f || T.geracao !== geracao || T.pronto || T.estado === 1 || T.estado === 3) return;
        var s = '[trailer-js] g' + geracao + ' youtube sem onReady em 10 s';
        try { console.log(s); if (window.__nvDiag) window.__nvDiag(s); } catch (e) {}
        T.estado = -3;
      }, 10000);
      if (!T.ouvinte) {
        T.ouvinte = 1;
        window.addEventListener('message', function (ev) {
          // Direto: a mensagem vem do youtube.com. Pela pagina do servico:
          // vem da origem dele (f.nvOrigem), repassada do player la dentro.
          if (T.ehVideo || !T.f || !T.f.contentWindow || ev.source !== T.f.contentWindow ||
              T.f.nvGeracao !== T.geracao || typeof ev.data !== 'string' ||
              !(ev.origin.indexOf('youtube.com') >= 0 || (T.f.nvOrigem && ev.origin === T.f.nvOrigem))) return;
          var m; try { m = JSON.parse(ev.data); } catch (e) { return; }
          if (!m) return;
          if (m.event === 'onReady' && T.f) {
            T.pronto = 1;
            T.f.contentWindow.postMessage(JSON.stringify({ event: 'listening', id: 1 }), '*');
          }
          // 2/5/100/101/150/152/153: o video nao toca aqui. 153 e o da AU7000
          // (#136), sem Referer valido. -3 e o mesmo erro do <video>: o C
          // fecha e passa a vez, a tela de erro do YouTube nao fica.
          if (m.event === 'onError') {
            var s = '[trailer-js] g' + T.geracao + ' youtube onError ' + m.info;
            try { console.log(s); if (window.__nvDiag) window.__nvDiag(s); } catch (e) {}
            T.estado = -3;
            return;
          }
          if (T.estado === -3) return;   // erro e final para este elemento
          if (m.event === 'infoDelivery' && m.info && typeof m.info.playerState === 'number') T.estado = m.info.playerState;
          if (m.event === 'onStateChange' && typeof m.info === 'number') T.estado = m.info;
          // Legenda: com o video tocando pede a lista de faixas; a resposta
          // (info.tracklist) escolhe o idioma ou nenhuma. Sem lista, sem legenda.
          var L = Module.nvLegenda, pw = T.f.contentWindow;
          var cmd = function (fn, args) { try { pw.postMessage(JSON.stringify({ event: 'command', func: fn, args: args }), '*'); } catch (e) {} };
          if (L && Module.nvLegendaEscolher && !T.f.nvLegPedida && T.estado === 1) {
            T.f.nvLegPedida = 1; cmd('loadModule', ['captions']); cmd('getOption', ['captions', 'tracklist']);
          }
          if (L && Module.nvLegendaEscolher && m.info && m.info.tracklist && !T.f.nvLegAplicada) {
            T.f.nvLegAplicada = 1;
            var cod = Module.nvLegendaEscolher(m.info.tracklist, L.sub, L.audio);
            if (cod) cmd('setOption', ['captions', 'track', { languageCode: cod }]); else cmd('unloadModule', ['captions']);
          }
        });
      }
      // A IFrame API so fala depois de "listening"; mandamos ao carregar.
      f.addEventListener('load', function () {
        if (T.f !== f || T.geracao !== geracao) return;
        try { f.contentWindow.postMessage(JSON.stringify({ event: 'listening', id: 1 }), '*'); } catch (e) {}
      });
    }
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
  T.f = null; T.id = ''; T.estado = -1; T.geracao = (T.geracao || 0) + 1;
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
int trailer_suportado(void) {
#ifdef NV_TIZEN3_LITE
  return 0;
#else
  return 1;
#endif
}
#else
// LG (e Mac, onde video_iniciar devolve 0 e nada disto acontece): o trailer
// e um MP4 no plano de video da TV. O volume so pode ser mexido depois de o
// pipeline existir (mediaId), e o recorte que tira a tarja preta so depois de
// o quadro ter tamanho — os dois ficam pendentes e trailer_atualizar aplica.
#include "video.h"
static int volumePendente, recortePendente, pausado;
/* Prepared only means metadata/demuxer ready. A trailer must reach Playing
 * once before autoplay regards it as started. Sticky after that, so a paused
 * or buffering trailer keeps its existing image instead of closing the hole. */
static int tocouFonte;
// O recorte e REPETIDO nos primeiros segundos (ver reaplicarAte): o pipeline
// desta TV prende o plano em mais de um ponto depois do load (bind do ACB,
// `playing`), e um recorte pedido cedo demais pode ser engolido por um deles.
static Uint32 reaplicarAte, reaplicarEm, tocandoDesde;
static int quadroInteiroEnviado;
#ifdef NV_TPK
// O PLANO ESCONDIDO ATE O RECORTE ASSENTAR (#178, trailer_mostra_video).
// `recorteEnviadoEm` e o instante do primeiro recorte desta fonte; o ROI vai
// ao host por fila (Video.cs, Principal), e o respiro cobre essa viagem e o
// SetRoi. Sem recorte em NV_TRAILER_TPK_ROI_PRAZO_MS de tocando (quadro sem
// tamanho, host que nao responde), mostra assim mesmo.
#define NV_TRAILER_TPK_ROI_ASSENTA_MS 300
#define NV_TRAILER_TPK_ROI_PRAZO_MS   2000
// Sem nem `tocando` (o host sempre manda logo depois do Start), a tela cheia
// abre o furo mesmo assim neste prazo: e o comportamento de antes.
#define NV_TRAILER_TPK_SEM_TOCAR_MS   6000
static Uint32 recorteEnviadoEm;
static int    mostraLogado;
static int    volumeTocandoEnviado;   // ver nativoAplicar (#281)
// O FURO SO COM IMAGEM (#188, #195). O host manda `pronto` e `tocando` logo
// depois do prepare e do Start, antes do primeiro quadro; um furo aberto ai
// mostra o que esta atras do app (no Tizen 9, a tela inicial da Samsung) ate a
// imagem chegar. A posicao do player (Video.cs, Tique: so le com o estado
// Playing) andar e o sinal de que ha quadro no plano. Pegajoso por fonte: a
// pausa do trailer em tela cheia nao fecha o furo.
#define NV_TRAILER_TPK_POS_MIN 0.25
// A posicao nao andou (host que nao le a posicao): abre neste prazo de
// `tocando`, como antes.
#define NV_TRAILER_TPK_POS_PRAZO_MS 4000u
static int    imagemVista;
static Uint32 tocandoVisto;
#endif
int trailer_suportado(void) {
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
  return falso != 0;
#else
  // O FRACASSO NAO TRAVA, MAS TEM TETO. O deploy mata o processo e relanca
  // em seguida, e o hub LS2 pode demorar para soltar o nome anterior; so o
  // SUCESSO e definitivo. O recuo e o teto moram em video_iniciar_auto
  // (lsregistro.h): sem teto, os registros 1720-1774 chamaram LSRegister a cada
  // 15 s pela sessao inteira — 1327 recusas de PERMISSION, que nao muda
  // sozinha. O play continua tentando (video_iniciar, pedido da pessoa).
  static int sabe = 0;
#ifdef NV_TPK
  // Outro app ja tomou o video da TV nesta sessao (#178): abrir trailer de
  // novo so abriria o furo em cima do video DELE. Fica a arte.
  if (video_conflito_recurso()) return 0;
#endif
  if (sabe) return 1;
  if (video_iniciar_auto()) sabe = 1;
  return sabe;
#endif
}
static void nativoAplicar(void) {
  if (!aberto) return;
#ifdef NV_TPK
  video_tpk_cartao_marcar(noCartao);
#endif
  if(video_tocando())tocouFonte=1;
  // O uMS setVolume funciona nesta TV (provado ao contrario: sem ele o
  // trailer tocou com som).
  if (volumePendente && video_ativo()) {
    video_volume(comSom ? 100 : 0); volumePendente = 0;
#ifdef NV_TPK
    printf("[trailer] tpk volume %d enviado (%s)\n", comSom ? 100 : 0, video_tocando() ? "tocando" : "abertura");
    fflush(stdout);
#endif
  }
#ifdef NV_TPK
  // DE NOVO NO PRIMEIRO `tocando` DA FONTE (#281). O pedido da abertura chega
  // ao host com o Player ainda preparando; o host ja reaplica sozinho
  // (Video.cs, AplicaVolume), e este reenvio cobre um host que nao reaplique.
  // A linha "[audio] volume alvo=... lido=..." do host diz o que pegou.
  if (!volumeTocandoEnviado && video_tocando()) {
    volumeTocandoEnviado = 1;
    video_volume(comSom ? 100 : 0);
    printf("[trailer] tpk volume %d reenviado no primeiro tocando\n", comSom ? 100 : 0);
    fflush(stdout);
  }
#endif
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
    int sw, sh, sx, sy;
#ifdef NV_TPK
    // O cartao e cover puro (zoom 1): video_tpk.c recorta a fonte onde ha
    // prova (Tizen 6.x) e encaixa o quadro inteiro nas demais familias.
    trailer_recorte(video_largura(), video_altura(), noCartao ? 1.0f : ajustes_trailer_zoom(),
                    rect.w, rect.h, &sx, &sy, &sw, &sh);
    if (video_recorte_fonte_trailer() || noCartao)
#else
    trailer_recorte(video_largura(), video_altura(), ajustes_trailer_zoom(),
                    rect.w, rect.h, &sx, &sy, &sw, &sh);
    if (video_recorte_fonte_trailer())
#endif
      video_janela_fonte(sx, sy, sw, sh,
                         (int)rect.x, (int)rect.y, (int)rect.w, (int)rect.h);
    recortePendente = 0;
#ifdef NV_TPK
    if (!recorteEnviadoEm) recorteEnviadoEm = SDL_GetTicks() | 1;
#endif
    if (!reaplicarAte) { reaplicarAte = SDL_GetTicks() + 6000; reaplicarEm = SDL_GetTicks() + 1500; }
  }
  if (reaplicarAte && SDL_GetTicks() >= reaplicarEm) {
    if (SDL_GetTicks() >= reaplicarAte) reaplicarAte = 0;
    else { reaplicarEm = SDL_GetTicks() + 1500; video_recorte_reaplicar(); }
  }
}
#endif

// Callers pass the rect in their own layer px (home/detail run inside the text
// scale layer, #205); the video plane wants real 1920x1080 px. `rect` keeps the
// real one, trailer_retangulo hands it back in the caller's units.
static GfxRect naTela(GfxRect r) {
  float k = gfx_escala_tela_txt();
  return (GfxRect){ r.x * k, r.y * k, r.w * k, r.h * k };
}

static int bloqueadoMusica;
void trailer_bloquear_musica(int sim) { bloqueadoMusica = sim; }
void trailer_abrir(const char *fonte, GfxRect r, int som, int modoCheia) {
  int nova;
  if (bloqueadoMusica) return;
  if (!trailer_suportado() || !fonte || !fonte[0]) return;
  r = naTela(r);
  // SAMSUNG E SEMPRE MUDO, tambem em tela cheia (dono, 22/09/2026: "trailer
  // fica mudo"). A trava fica AQUI, no unico caminho ate o elemento, e nao so
  // em quem chama: um `som` 1 esquecido num chamador novo desmutaria o embed
  // do YouTube (mute=0) e voltaria a prometer som onde a Apple nao tem.
  if (!trailerfonte_com_som(trailerfonte_tizen())) som = 0;
  nova = strcmp(fonteAtual, fonte) != 0;
#ifdef __EMSCRIPTEN__
  // O ESTADO DE SESSAO a cada tentativa (dono: "tocou um trailer e depois
  // nenhum toca mais"). Se algo ficasse preso aqui — elemento velho ainda
  // aberto, falha antiga, estado JS que nao volta a -2 — esta linha mostra
  // no registro da proxima Samsung, sem precisar de TV na mesa.
  if (nova) {
    printf("[trailer] tentativa: aberto=%d cheia=%d falhou=%d estado=%d anterior=%.40s\n",
           aberto, cheia, falhouUltima, trailer_js_estado(), fonteAtual[0] ? fonteAtual : "-");
    fflush(stdout);
  }
#endif
  if (nova) falhouUltima = 0;
#ifdef __EMSCRIPTEN__
  {
    char sub[8] = "", aud[3] = "";
    ling_normalizar(ling_legenda(), sub, sizeof sub);
    sub[2] = 0;
    extras_trailer_audio_iso(aud);
    trailer_js_legenda(sub, aud);
  }
  if (!trailer_js_abrir(fonte, r.x, r.y, r.w, r.h, som, ajustes_trailer_zoom(), NV_REC_URL)) return;
#else
  if (nova) {
    if (!falso && !video_tocar(fonte)) return;
#ifdef NV_TPK
    video_tpk_trailer_marcar(1);
#endif
    tocouFonte = 0;
    volumePendente = 1; recortePendente = 1; pausado = 0; reaplicarAte = 0;
    tocandoDesde = 0; quadroInteiroEnviado = 0;
#ifdef NV_TPK
    recorteEnviadoEm = 0; mostraLogado = 0; imagemVista = 0; tocandoVisto = 0;
    volumeTocandoEnviado = 0;
#endif
  } else if (comSom != som) volumePendente = 1;
  video_janela((int)r.x, (int)r.y, (int)r.w, (int)r.h);
  if (!nova) recortePendente = 1;
#endif
  if (nova) {
    geracao++;
    snprintf(fonteAtual, sizeof fonteAtual, "%s", fonte);
    abertoEm = SDL_GetTicks();
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
  r = naTela(r);
#ifdef NV_TOUCH_UI
  if (rect.x == r.x && rect.y == r.y && rect.w == r.w && rect.h == r.h) return;
#endif
  rect = r;
#ifdef __EMSCRIPTEN__
  trailer_js_abrir(fonteAtual, r.x, r.y, r.w, r.h, comSom, ajustes_trailer_zoom(), NV_REC_URL);
#else
  video_janela((int)r.x, (int)r.y, (int)r.w, (int)r.h);
  recortePendente = 1;
  nativoAplicar();
#endif
}

void trailer_no_cartao(int sim) { noCartao = sim ? 1 : 0; }

void trailer_fechar(void) {
  noCartao = 0;
  if (!aberto) return;
#ifdef __EMSCRIPTEN__
  trailer_js_fechar();
#else
  video_parar();
#endif
  aberto = 0; cheia = 0; fonteAtual[0] = 0;
#ifndef __EMSCRIPTEN__
  tocouFonte = 0;
#endif
  dono = TRAILER_DONO_NENHUM; donoImdb[0] = 0;
}

void trailer_marcar_dono(int d, const char *imdb) {
  if (!aberto) return;
  dono = d;
  // A pagina do titulo ADOTOU o trailer do cartao: dali em diante o retangulo
  // e o dela, com o recorte de sempre.
  if (d == TRAILER_DONO_DETALHE) noCartao = 0;
  snprintf(donoImdb, sizeof donoImdb, "%s", imdb ? imdb : "");
}
int trailer_dono(void) { return aberto ? dono : TRAILER_DONO_NENHUM; }
const char *trailer_dono_imdb(void) { return aberto ? donoImdb : ""; }

int trailer_continuar(GfxRect r, int som) {
  char f[sizeof fonteAtual];
  if (!aberto || cheia || !fonteAtual[0]) return 0;
  // Copia: trailer_abrir le `fonte` e grava fonteAtual. Mesma fonte = nao e
  // "nova": so o volume e o retangulo (com o recorte do zoom) mudam.
  snprintf(f, sizeof f, "%s", fonteAtual);
  noCartao = 0;   // o destino ja pertence ao detalhe antes de recalcular
  trailer_abrir(f, r, som, 0);
  printf("[trailer] continua %.60s %s\n", f, comSom ? "com som" : "mudo");
  fflush(stdout);
  return 1;
}

int trailer_aberto(void)  { return aberto; }
unsigned trailer_geracao(void) { return geracao; }
int trailer_com_som(void) { return aberto && comSom; }
int trailer_cheia(void)   { return aberto && cheia; }
int trailer_tocando(void) {
#ifdef __EMSCRIPTEN__
  // 1 = PLAYING, 3 = BUFFERING (ja ha imagem), na IFrame API.
  int e = aberto ? trailer_js_estado() : -2;
  return e == 1 || e == 3;
#else
  if (falso) return aberto && falso == 2;
  if(!aberto||!video_pronto()||video_falhou()||video_terminou()||video_conflito_recurso())return 0;
  if(video_tocando())tocouFonte=1;
  return tocouFonte;
#endif
}
int trailer_mostra_video(void) {
#ifdef NV_ANDROID
  if (noCartao && !video_janela_pronta()) return 0;
#endif
#if defined(NV_TPK) && !defined(__EMSCRIPTEN__)
  Uint32 t = SDL_GetTicks();
  const char *porque = NULL;
  if (!aberto) return 0;
  if (!imagemVista) {
    if (video_tocando() && !tocandoVisto) tocandoVisto = t | 1;
    if (video_pos() < NV_TRAILER_TPK_POS_MIN &&
        !(tocandoVisto && (Sint32)(t - tocandoVisto) >= (Sint32)NV_TRAILER_TPK_POS_PRAZO_MS)) return 0;
    imagemVista = 1;
  }
  // "Original", ou o alvo sem recorte (video_tpk.c, o padrao desde #188): nada
  // a esperar, o LetterBox de sempre ja e a imagem certa.
  // "Original" so dispensa a espera quando o destino tambem nao pede corte
  // (o cover de trailer_recorte num destino que nao e da proporcao do video).
  // O cartao so espera o recorte onde ele e cover de fonte (Tizen 6.x); nas
  // demais familias o quadro inteiro encaixado ja e a imagem final.
  int cartaoCover = noCartao && video_tpk_cartao_cover();
  int semCorte = !video_recorte_fonte_trailer() && !cartaoCover;
  if (!semCorte && (cartaoCover || ajustes_trailer_zoom() <= 1.001f) && video_largura() > 0 && video_altura() > 0) {
    int sx, sy, sw, sh;
    trailer_recorte(video_largura(), video_altura(), 1.0f, rect.w, rect.h, &sx, &sy, &sw, &sh);
    semCorte = sw >= (video_largura() & ~1) && sh >= (video_altura() & ~1);
  }
  if (semCorte) porque = "sem recorte";
  else if (recorteEnviadoEm && (Sint32)(t - recorteEnviadoEm) >= NV_TRAILER_TPK_ROI_ASSENTA_MS) porque = "recorte assentou";
  else if (tocandoDesde && (Sint32)(t - tocandoDesde) >= NV_TRAILER_TPK_ROI_PRAZO_MS) porque = "sem recorte em 2 s, mostra assim";
  else if (!tocandoDesde && abertoEm && (Sint32)(t - abertoEm) >= NV_TRAILER_TPK_SEM_TOCAR_MS) porque = "sem tocando, mostra assim";
  if (!porque) return 0;
  if (!mostraLogado) {
    mostraLogado = 1;
    printf("[trailer] tpk: plano visivel +%ums (%s, posicao %.2fs)\n", (unsigned)(t - abertoEm), porque, video_pos());
    fflush(stdout);
  }
  return 1;
#else
  return 1;
#endif
}
GfxRect trailer_retangulo(void) {
  float k = gfx_escala_tela_txt();
  return (GfxRect){ rect.x / k, rect.y / k, rect.w / k, rect.h / k };
}
int trailer_estado(void) {
#ifdef __EMSCRIPTEN__
  return aberto ? trailer_js_estado() : -2;
#else
  return -9;
#endif
}

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
  // TRANSICAO DO ESTADO como o C a ve (-1 criado, 1 tocando, 3 buffering,
  // 0 fim, -3 erro, -2 sem elemento), com o tempo desde a abertura. Com o
  // `[trailer-js]` do elemento ao lado, o registro diz se o video falhou, se
  // ficou sem `playing` ou se quem fechou foi a tela. So na borda: 1 linha
  // por mudanca, nao por quadro.
  { static int visto = -2;
    int e = aberto ? trailer_js_estado() : -2;
    if (e != visto) {
      // `agora` vem do inicio do quadro e abertoEm de SDL_GetTicks no meio
      // dele: no quadro da abertura a diferenca e negativa, vira 0.
      Sint32 ms = abertoEm ? (Sint32)(agora - abertoEm) : 0;
      printf("[trailer] estado %d -> %d +%dms\n", visto, e, ms > 0 ? (int)ms : 0);
      fflush(stdout);
      visto = e;
    } }
  // Acabou (0 = ENDED) ou falhou (-3, so o <video>): fecha e a arte volta.
  if (aberto && trailer_js_estado() == -3) {
    falhouUltima = 1;
    trailer_fechar();
  } else if (aberto && trailer_js_estado() == 0) trailer_fechar();
#else
  if (!aberto) return;
  video_bombear();
  nativoAplicar();
  // Acabou ou a fonte falhou: fecha e a pagina volta a arte.
  // conflito: outro app (YouTube) tomou o plano de video — fecha, fica a arte.
  if (video_terminou() || video_falhou() || video_conflito_recurso()) {
    falhouUltima = video_falhou() || video_conflito_recurso() ? 1 : 0;
    trailer_fechar();
  }
#endif
}

int trailer_falhou(void) { return falhouUltima; }

// O OSD DO TRAILER EM TELA CHEIA (Glass UI, mockup de 03/10, proposta
// aprovada): so com o trailer PAUSADO — o trailer e o proprio heroi. A pilula
// da ilha diz o que e ("Trailer · titulo"), a barra fina do player embaixo e
// o tempo; o unico botao e Continuar (o que o OK faz). Quem desenha a tela
// cheia (detail.c) chama isto por cima do furo.
int trailer_pausado(void) {
#ifdef __EMSCRIPTEN__
  return aberto && cheia && trailer_js_pausado();
#else
  return aberto && cheia && pausado;
#endif
}
// MEDIDOR DE DESEMPENHO (#338, revisao do job 205): main chama isto por quadro
// para desempenho_trailer. No nativo (webOS/Android/.tpk) trailer_tocando
// fica preso em 1 durante a pausa da tela cheia (o furo segura o ultimo
// quadro), entao o medidor pergunta a pausa a parte e VOLTA quando o trailer
// pausa. No iframe (__EMSCRIPTEN__) o estado pausado ja nao e PLAYING nem
// BUFFERING, trailer_tocando ja responde 0 e o !trailer_pausado() e so
// cinto de seguranca.
int trailer_medidor_esconde(void) {
  return trailer_aberto() && trailer_tocando() && !trailer_pausado();
}
void trailer_osd_desenhar(const char *titulo, float a) {
  double pos = video_pos(), dur = video_duracao();
  if (!trailer_pausado() || a < 0.01f) return;
  gfx_veu_css((GfxRect){ 0, 0, NV_TELA_W, 260.0f }, 1, 1.38f, 1.0f, 0.52f * a);
  gfx_veu_css((GfxRect){ 0, NV_TELA_H - 300.0f, NV_TELA_W, 300.0f }, 0, 1.0f, 1.0f, 0.60f * a);
  { TxtLinha l1 = txt_linha(TXT_ILHA_NOME, "Trailer", 243, 242, 239, 255);
    TxtLinha l2 = txt_linha_corta(TXT_ILHA_CORPO, titulo ? titulo : "", 243, 242, 239, 179, 900.0f);
    float w = 24.0f + 24.0f + 12.0f + (float)l1.w + (l2.w ? 12.0f + 1.0f + 12.0f + (float)l2.w : 0.0f) + 24.0f;
    GfxRect p = { 96.0f, 48.0f, w, 56.0f };
    float x = p.x + 24.0f, yc = p.y + 28.0f;
    plrui_material(p, 28.0f, 0, a);
    gfx_icone((GfxRect){ x, yc - 12.0f, 24.0f, 24.0f }, "pl_clapperboard", 1, 1, 1, a);
    x += 36.0f;
    txt_desenhar_alpha(l1, x, yc - (float)l1.h * 0.5f, a);
    x += (float)l1.w + 12.0f;
    if (l2.w) { plrui_sep(x, yc, a); txt_desenhar_alpha(l2, x + 13.0f, yc - (float)l2.h * 0.5f, a); } }
  if (dur > 1.0) {
    char t1[24], d[24], t2[32];
    plrui_barra(96.0f, 1000.0f, NV_TELA_W - 192.0f, (float)(pos / dur),
                (float)(video_buffer_fim() / dur), 0, NULL, 0, a);
    plrui_tempo(t1, sizeof t1, pos); plrui_tempo(d, sizeof d, dur);
    snprintf(t2, sizeof t2, "/ %s", d);
    { TxtLinha l2 = txt_linha(TXT_ILHA_NOME, t2, 243, 242, 239, 128);
      TxtLinha l1 = txt_linha(TXT_ILHA_NOME, t1, 243, 242, 239, 235);
      float xr = NV_TELA_W - 96.0f - l2.w;
      txt_desenhar_alpha(l2, xr, 916.0f, a);
      txt_desenhar_alpha(l1, xr - 8.0f - l1.w, 916.0f, a); }
  }
  { TxtLinha l = txt_linha(TXT_G21B, "Continuar", 0, 0, 0, 255);
    GfxRect b = { 96.0f, 896.0f, 22.0f + 28.0f + 12.0f + (float)l.w + 28.0f, 68.0f };
    float k = plrui_tinta() / 255.0f;
    plrui_pilula_foco(b, a);
    gfx_icone((GfxRect){ b.x + 22.0f, b.y + 20.0f, 28.0f, 28.0f }, "pl_play-f", k, k, k, a);
    l = txt_linha(TXT_G21B, "Continuar", plrui_tinta(), plrui_tinta(), plrui_tinta(), 255);
    txt_desenhar_alpha(l, b.x + 22.0f + 28.0f + 12.0f, b.y + (68.0f - (float)l.h) * 0.5f, a); }
}
#ifdef NV_SHOT_HOOKS
void trailer_shot_pausado(int sim) { aberto = cheia = sim; pausado = sim; }
#endif
