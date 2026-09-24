// Reproducao de video no alvo Hisense VIDAA OS, por um <video> comum do
// Chromium embutido — nao ha webapis.avplay (Samsung) nem uMS por LS2 (LG)
// neste navegador. Companheiro de src/video_tizen.c: mesmo video.h, mesmo
// modelo de composicao (plano atras do canvas GL, furo transparente por cima,
// ver gfx_furo em player.c), mesma porta unica para o JS.
//
// NADA AQUI FOI EXECUTADO NUMA TV VIDAA — nao ha uma nesta bancada. O que esta
// verificado (ver tests/vidaa-video-contract.cjs e o relatorio desta tarefa):
// a sintaxe compila sob emcc; a ponte nv_av roda contra um <video>/document
// falsos escritos para este teste, cobrindo estado, geracao, buffering,
// faixas, legenda oculta e a decisao HLS nativo/hls.js/falha. Isso NAO prova
// que o Chromium da TV aceita HLS, que o layout de recorte fica correto numa
// tela de verdade, ou que hls.js 1.7.3 roda no motor dela.
//
// POR QUE O <video> NAO FICA MUDO NEM object-fit:cover (ao contrario do
// trailer mudo de trailer.c, que reusa o MESMO padrao de elemento atras do
// canvas): o trailer e decorativo e sempre corta para preencher; o player
// principal precisa de volume de verdade e do recorte de FONTE que o proprio
// video.h pede (video_janela_fonte) — um object-fit fixo inviabilizaria o
// zoom "recortar" that video_recorte_fonte() promete aqui.
//
// RECORTE DE FONTE: SEM PLANO DE HARDWARE, O CSS FAZ O TRABALHO.
// No webOS/Tizen a fonte e recortada pelo pipeline (AcbAPI/setVideoRoi) e o
// C so manda coordenadas; aqui nao ha pipeline nenhum sob nosso controle, e a
// unica coisa que a pagina pode recortar e o proprio elemento DOM. A tecnica:
// um DIV embrulho (`overflow:hidden`) do tamanho do DESTINO, com o <video> por
// dentro AMPLIADO e DESLOCADO para que so o pedaco da FONTE pedido caia dentro
// do embrulho. A conta e a mesma ampliacao-e-deslocamento que o comentario de
// video_tizen.c descreve para o caso em que o AVPlay recusa o ROI — so que
// aqui ela e o caminho NORMAL, garantido pelo proprio CSS, e nao uma aposta
// sobre o que um plano de hardware faz com um retangulo fora da tela.
// Consequencia: video_recorte_fonte() aqui SEMPRE devolve 1.
//
// FIO: OS CALLBACKS DO <video> SO EXISTEM NO DOM DA PAGINA (fio principal).
// Mesmo problema documentado em video_tizen.c para webapis.avplay: um EM_JS
// chamado de um worker roda no escopo JS DO WORKER, onde nao ha document nem
// elemento <video> nenhum. A solucao e a mesma dali, adaptada: avChamar()
// verifica emscripten_is_main_browser_thread() e, fora dele, atravessa por
// emscripten_proxy_sync ate o fio principal. A diferenca aqui e que este
// arquivo tambem precisa compilar na variante --um-fio (tools/tizen.sh
// --vidaa --um-fio), que roda sem -pthread — la so existe o fio principal
// mesmo, entao a travessia e desnecessaria e o codigo que a implementa fica
// fora com #if defined(__EMSCRIPTEN_PTHREADS__), a macro que o proprio emcc
// define quando -pthread esta ligado.
//
// O QUE NAO TEM EQUIVALENTE (degrada com honestidade, igual ao Tizen):
//   * cabecalhos HTTP por requisicao (Referer): um <video src> nao manda
//     cabecalho nenhum. video_definir_cabecalhos fica stub, como no Tizen.
//   * legenda externa por URL: o app ja sabe desenhar legenda em GL sozinho
//     (src/legenda.c); video_legenda_externa recusa e conta com esse caminho,
//     igual ao Tizen — ver o comentario na funcao.
//   * HDR/Dolby Vision/Atmos: nao ha API de navegador para nenhum dos tres
//     aqui (o <video>/MediaCapabilities nao expoe isso de forma confiavel
//     entre motores); video_hdr() diz "desconhecido" e os selos ficam fora,
//     mesma decisao do Tizen e pela mesma razao — selo ausente e honesto,
//     selo inventado mente para o dono no momento em que ele mais confia nele.
//   * sondagem de MKV (creditos por capitulo, idioma de faixa) SEM -pthread:
//     mkv_faixas_e_caps BLOQUEIA por Range HTTP e precisa de um fio proprio
//     (ver a nota em video_tizen.c); a variante --um-fio nao tem fio nenhum
//     alem do principal, entao video_mkv_sondado() devolve 2 ("sem sonda")
//     ali de proposito, documentado, em vez de travar a pagina numa
//     requisicao sincrona no fio que desenha.
#if defined(__EMSCRIPTEN__) && defined(NV_VIDAA)

#include "video.h"
#include "linguas.h"
#include "idioma.h"
#include "marco.h"
#include "mkv.h"
#include <SDL2/SDL.h>
#include <emscripten.h>
#if defined(__EMSCRIPTEN_PTHREADS__)
#include <emscripten/threading.h>
#include <emscripten/proxying.h>
#include <pthread.h>
#endif
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ============================================================================
// A PORTA UNICA PARA O JS
// ============================================================================
//
// Mesmo desenho de src/video_tizen.c: um comando em TEXTO (aparece no log do
// duble de teste sem precisar consultar tabela nenhuma), oito ou mais doubles
// de saida em "estado" (um tipo so, sem deslocamento/alinhamento a acertar
// entre C e JS), e dst/dstTam para as operacoes que devolvem texto.
//
// Estado da sessao no objeto GLOBAL (window.__nvav), pela mesma razao do
// Tizen: este bloco tambem e emitido no bundle dos workers, e cada worker
// teria a sua copia se o estado vivesse num var de modulo — so a copia do fio
// principal, o unico que chega ate aqui, deve valer.
EM_JS(double, nv_av, (const char *cmd, const char *txt,
                      double a, double b, double c, double d,
                      char *dst, int dstTam), {
  var op = UTF8ToString(cmd);
  var s  = txt ? UTF8ToString(txt) : "";
  var G = (typeof window !== "undefined") ? window : self;
  if (!G.__nvav) {
    G.__nvav = {
      v: null, wrap: null,     // <video> e o div embrulho que recorta
      hls: null,               // instancia do Hls.js, quando em uso
      aberto: 0, tocando: 0, pronto: 0, fim: 0,
      erro: 0, erroCodigo: 0, erroMsg: "",
      larg: 0, alt: 0,
      bufDesde: 0,             // performance.now() do ultimo "waiting"; 0 = nao esta bufferizando
      geracao: 0,              // identidade da sessao, para eventos tardios se ignorarem
      rect: [0, 0, 1920, 1080],
      crop: null,              // [sx,sy,sw,sh] no espaco do quadro decodificado, ou null
      legTxt: "", legN: 0
    };
  }
  var S = G.__nvav;

  function avAgora() {
    try {
      if (typeof performance !== "undefined" && performance && performance.now) return performance.now();
    } catch (e) {}
    return Date.now();
  }

  // Aplica retangulo de destino (S.rect, em 1920x1080) e, se houver recorte de
  // fonte pedido (S.crop), amplia e desloca o <video> DENTRO do embrulho para
  // que so aquele pedaco do quadro fique visivel — ver a nota grande no topo
  // do arquivo C. Sem crop, o video preenche o embrulho inteiro (o mesmo
  // "encaixar e esticar" que o Tizen faz com FULL_SCREEN, porque o retangulo
  // de destino que player.c manda ja tem a proporcao certa calculada).
  function aplicarLayout() {
    if (!S.wrap || !S.v) return;
    var cv = document.getElementById("canvas");
    if (!cv) return;
    var r = cv.getBoundingClientRect();
    var esx = r.width / 1920, esy = r.height / 1080;
    var dx = S.rect[0], dy = S.rect[1], dw = S.rect[2], dh = S.rect[3];
    var cssW = dw * esx, cssH = dh * esy;
    S.wrap.style.left = (r.left + dx * esx) + "px";
    S.wrap.style.top  = (r.top  + dy * esy) + "px";
    S.wrap.style.width  = cssW + "px";
    S.wrap.style.height = cssH + "px";
    var c = S.crop;
    if (c && c[2] > 0 && c[3] > 0 && S.v.videoWidth > 0 && S.v.videoHeight > 0) {
      var qw = S.v.videoWidth, qh = S.v.videoHeight;
      var ex = cssW / c[2], ey = cssH / c[3];
      S.v.style.width  = (qw * ex) + "px";
      S.v.style.height = (qh * ey) + "px";
      S.v.style.left = (-c[0] * ex) + "px";
      S.v.style.top  = (-c[1] * ey) + "px";
    } else {
      S.v.style.width  = "100%";
      S.v.style.height = "100%";
      S.v.style.left = "0px";
      S.v.style.top  = "0px";
    }
  }

  // Ate onde o buffer cobre a POSICAO ATUAL, em segundos; 0 quando nao ha
  // faixa de buffer cobrindo o instante corrente (video.h define isso como
  // "desconhecido"). Pequena folga de 0.5s para o caso comum de currentTime
  // cair uma fracao antes do inicio da faixa por arredondamento do proprio
  // navegador.
  function bufferFimAgora() {
    try {
      var b = S.v.buffered, t = S.v.currentTime;
      var i;
      for (i = 0; i < b.length; i++) {
        if (t >= b.start(i) - 0.5 && t <= b.end(i) + 0.001) return b.end(i);
      }
      if (b.length) return b.end(b.length - 1);
    } catch (e) {}
    return 0;
  }

  function fecharTudo() {
    if (S.hls) { try { S.hls.destroy(); } catch (e) {} S.hls = null; }
    if (S.v) {
      try { S.v.pause(); } catch (e) {}
      try { S.v.removeAttribute("src"); S.v.load(); } catch (e) {}
    }
    if (S.wrap && S.wrap.parentNode) { try { S.wrap.parentNode.removeChild(S.wrap); } catch (e) {} }
    S.v = null; S.wrap = null; S.crop = null;
  }

  if (op === "disp") {
    try {
      var t = document.createElement("video");
      return (t && typeof t.play === "function") ? 1 : 0;
    } catch (e) { return 0; }
  }

  if (op === "abrir") {
    var geracao = ++S.geracao;
    fecharTudo();
    S.aberto = 0; S.tocando = 0; S.pronto = 0; S.fim = 0;
    S.erro = 0; S.erroCodigo = 0; S.erroMsg = "";
    S.larg = 0; S.alt = 0; S.bufDesde = 0;
    S.legTxt = ""; S.legN = 0;

    var v;
    try { v = document.createElement("video"); } catch (e) { return 0; }
    if (!v) return 0;
    // NAO mudo (ao contrario do trailer): esta e a reproducao principal, com
    // volume de verdade. NAO object-fit:cover: o recorte de fonte pedido por
    // video_janela_fonte precisa que o <video> possa crescer para ALEM do
    // embrulho, e cover cortaria do jeito errado.
    v.muted = false;
    v.playsInline = true;
    v.preload = "auto";
    v.controls = false;
    v.style.cssText = "position:absolute;z-index:0;pointer-events:none;background:#000;";
    var wrap = document.createElement("div");
    wrap.style.cssText = "position:absolute;z-index:0;overflow:hidden;pointer-events:none;background:#000;";
    wrap.appendChild(v);
    (document.body || document.documentElement).appendChild(wrap);
    S.v = v; S.wrap = wrap;

    function ehDaSessao() { return S.geracao === geracao && S.v === v; }

    v.addEventListener("loadedmetadata", function () {
      if (!ehDaSessao()) return;
      S.larg = v.videoWidth || 0; S.alt = v.videoHeight || 0;
      S.pronto = 1;
      aplicarLayout();
    });
    v.addEventListener("canplay", function () { if (ehDaSessao()) S.pronto = 1; });
    v.addEventListener("playing", function () { if (ehDaSessao()) { S.tocando = 1; S.bufDesde = 0; } });
    v.addEventListener("pause",   function () { if (ehDaSessao()) S.tocando = 0; });
    // So marca o INICIO do buffering se nao houver um em curso: dois "waiting"
    // seguidos sem "playing" no meio nao devem esticar o relogio.
    v.addEventListener("waiting", function () { if (ehDaSessao() && !S.bufDesde) S.bufDesde = avAgora(); });
    v.addEventListener("ended",   function () { if (ehDaSessao()) { S.fim = 1; S.tocando = 0; } });
    v.addEventListener("error",   function () {
      if (!ehDaSessao()) return;
      var e = v.error;
      S.erro = 1; S.erroCodigo = e ? e.code : 0;
      S.erroMsg = "MediaError code=" + (e ? e.code : "?") + " " + (e && e.message ? e.message : "");
    });

    function tentarTocar() {
      if (!ehDaSessao()) return;
      try {
        var pr = v.play();
        if (pr && pr.catch) pr.catch(function () {});
      } catch (e) {}
    }

    S.aberto = 1;
    // Caminho da fonte, sem a query string: e so contra ISSO que ".m3u8" e
    // testado, para uma URL assinada tipo "...m3u8?token=..." nao escapar da
    // deteccao por causa da query.
    var caminho = s.split("?")[0].split("#")[0];
    var ehM3u8 = /\\.m3u8$/i.test(caminho);

    if (!ehM3u8) {
      v.src = s;
      tentarTocar();
      aplicarLayout();
      return 1;   // progressivo, sem HLS
    }

    // HLS NATIVO: Safari/WebKit sabem tocar .m3u8 direto num <video src>. O
    // Chromium da TV (VIDAA) nao sabe — canPlayType devolve "" — mas a
    // checagem fica aqui, e nao um "if e Chromium", porque e exatamente o
    // contrato que a MDN documenta para esta API: perguntar ao proprio motor,
    // nao adivinhar por user agent.
    var nativo = "";
    try { nativo = v.canPlayType("application/vnd.apple.mpegurl"); } catch (e) {}
    if (nativo) {
      v.src = s;
      tentarTocar();
      aplicarLayout();
      return 2;   // HLS por suporte nativo do motor
    }

    if (!window.MediaSource) {
      S.erro = 1; S.erroMsg = "HLS sem suporte nativo e sem MediaSource neste motor";
      return 0;
    }

    function iniciarHls() {
      if (!ehDaSessao()) return;
      try {
        var hls = new window.Hls({ maxBufferLength: 30, maxMaxBufferLength: 60, enableWorker: true });
        S.hls = hls;
        hls.on(window.Hls.Events.ERROR, function (evt, data) {
          if (!ehDaSessao() || !S.hls || S.hls !== hls) return;
          if (!data || !data.fatal) return;
          S.erro = 1;
          S.erroMsg = "hls.js fatal: " + (data.type || "?") + "/" + (data.details || "?");
          try { hls.destroy(); } catch (e2) {}
          if (S.hls === hls) S.hls = null;
        });
        hls.on(window.Hls.Events.MANIFEST_PARSED, function () { tentarTocar(); });
        hls.loadSource(s);
        hls.attachMedia(v);
        aplicarLayout();
      } catch (e3) {
        S.erro = 1; S.erroMsg = "hls.js init: " + e3;
      }
    }

    // Carrega hls.min.js SOB DEMANDA, do mesmo diretorio da pagina (o
    // tools/tizen.sh --vidaa ja copia tools/vendor/hls.min.js para la).
    // "ignore if already loaded": tanto window.Hls quanto uma tag <script>
    // marcada ja presente pulam o download de novo.
    if (window.Hls) {
      iniciarHls();
    } else {
      var sc = document.querySelector('script[data-nv-hls]');
      if (!sc) {
        sc = document.createElement("script");
        sc.src = "hls.min.js";
        sc.setAttribute("data-nv-hls", "1");
        sc.onload = function () { iniciarHls(); };
        sc.onerror = function () {
          if (!ehDaSessao()) return;
          S.erro = 1; S.erroMsg = "hls.min.js falhou ao carregar";
        };
        (document.head || document.documentElement).appendChild(sc);
      } else {
        sc.addEventListener("load", function () { iniciarHls(); });
      }
    }
    return 3;   // HLS por hls.js (carregamento pode ainda estar em curso)
  }

  if (op === "parar" || op === "encerrar") {
    ++S.geracao;
    fecharTudo();
    S.aberto = 0; S.tocando = 0; S.pronto = 0; S.fim = 0;
    S.erro = 0; S.erroCodigo = 0; S.erroMsg = "";
    S.larg = 0; S.alt = 0; S.bufDesde = 0; S.legTxt = ""; S.legN = 0;
    return 1;
  }

  if (op === "pausar") {
    if (!S.v || !S.aberto) return 0;
    try {
      if (a) { S.v.pause(); }
      else { var pr2 = S.v.play(); if (pr2 && pr2.catch) pr2.catch(function () {}); }
    } catch (e) { return 0; }
    return 1;
  }

  if (op === "buscar") {
    if (!S.v || !S.aberto) return 0;
    S.legTxt = "";   // a fala de antes do salto nao vale no destino (igual ao Tizen, #122)
    try { S.v.currentTime = a; } catch (e) { return 0; }
    return 1;
  }

  if (op === "volume") {
    if (!S.v) return 0;
    var vv = a; if (vv < 0) vv = 0; if (vv > 100) vv = 100;
    try { S.v.volume = vv / 100; } catch (e) { return 0; }
    return 1;
  }

  // Guarda o recorte de FONTE (no espaco do quadro decodificado) e aplica na
  // hora: nao da para esperar pela proxima "rect", porque quem chama
  // video_janela_fonte manda "recorte" e so DEPOIS o destino (ver o C), e as
  // duas chamadas podem pedir o mesmo destino de antes — sem aplicar aqui, um
  // recorte novo com destino repetido nunca apareceria na tela.
  if (op === "recorte") {
    S.crop = (c > 0 && d > 0) ? [a, b, c, d] : null;
    aplicarLayout();
    return 1;
  }

  if (op === "rect") {
    S.rect = [a, b, c, d];
    aplicarLayout();
    return 1;
  }

  if (op === "estado") {
    // Doze doubles, nesta ordem — tem de casar com o enum EST_* no C. dstTam
    // em BYTES, mesmo contrato do Tizen.
    if (!dst || dstTam < 96) return 0;
    var o = dst >> 3;
    var pos = 0, dur = 0;
    if (S.v) {
      try { pos = +S.v.currentTime || 0; } catch (e) {}
      try { dur = +S.v.duration; if (!isFinite(dur) || dur < 0) dur = 0; } catch (e) {}
      if (S.larg <= 0 && S.v.videoWidth)  S.larg = S.v.videoWidth;
      if (S.alt  <= 0 && S.v.videoHeight) S.alt  = S.v.videoHeight;
    }
    var bufFim = S.v ? bufferFimAgora() : 0;
    var bufMs  = S.bufDesde ? Math.max(0, avAgora() - S.bufDesde) : 0;
    HEAPF64[o + 0]  = pos;
    HEAPF64[o + 1]  = dur;
    HEAPF64[o + 2]  = S.tocando ? 1 : 0;
    HEAPF64[o + 3]  = S.pronto  ? 1 : 0;
    HEAPF64[o + 4]  = S.aberto  ? 1 : 0;
    HEAPF64[o + 5]  = S.larg;
    HEAPF64[o + 6]  = S.alt;
    HEAPF64[o + 7]  = S.erro ? 1 : 0;
    HEAPF64[o + 8]  = S.fim  ? 1 : 0;
    HEAPF64[o + 9]  = bufFim;
    HEAPF64[o + 10] = bufMs;
    HEAPF64[o + 11] = S.erroCodigo || 0;
    return 1;
  }

  if (op === "erro_msg") {
    if (!dst || dstTam < 2) return 0;
    stringToUTF8(S.erroMsg || "", dst, dstTam);
    return 1;
  }

  // "<A|T>\t<indice>\t<idioma>\t<rotulo>\t<ordinalMkv>\n" por faixa — mesmo
  // formato de linha que video_tizen.c usa em "faixas", para reusar o mesmo
  // parser no C (lerFaixas). ordinalMkv fica sempre -1 aqui: o navegador nao
  // da o TrackNumber do Matroska, so a sonda de MKV (aplicarIdiomasDoMkv)
  // preenche idioma depois, por CONTAGEM, igual ao Tizen.
  if (op === "faixas") {
    if (!S.v || !dst || dstTam < 4) return 0;
    var out = "", n = 0;
    try {
      var ats = S.v.audioTracks;
      if (ats) {
        for (var i = 0; i < ats.length; i++) {
          var at = ats[i];
          var lang = ("" + (at.language || "")).toLowerCase();
          if (lang === "und") lang = "";
          var rot = "" + (at.label || "");
          out += "A\\t" + i + "\\t" + lang + "\\t" + rot.replace(/[\\t\\n]/g, " ") + "\\t-1\\n";
          n++;
        }
      }
    } catch (e) {}
    try {
      var tts = S.v.textTracks;
      if (tts) {
        for (var j = 0; j < tts.length; j++) {
          var tt = tts[j];
          var lang2 = ("" + (tt.language || "")).toLowerCase();
          if (lang2 === "und") lang2 = "";
          var rot2 = "" + (tt.label || "");
          out += "T\\t" + j + "\\t" + lang2 + "\\t" + rot2.replace(/[\\t\\n]/g, " ") + "\\t-1\\n";
          n++;
        }
      }
    } catch (e) {}
    stringToUTF8(out, dst, dstTam);
    return n;
  }

  if (op === "faixa") {
    if (!S.v) return 0;
    if (s === "AUDIO") {
      var ats2 = S.v.audioTracks;
      if (!ats2) return 0;
      for (var k = 0; k < ats2.length; k++) ats2[k].enabled = (k === (a | 0));
      return 1;
    }
    if (s === "TEXT") {
      var tts2 = S.v.textTracks;
      if (!tts2) return 0;
      for (var m = 0; m < tts2.length; m++) {
        if (m === (a | 0)) {
          // 'hidden' e nunca 'showing': o app desenha a legenda por conta
          // propria (video_legenda_nativa), o mesmo contrato do onsubtitle-
          // change do AVPlay (#122) — o motor entrega o texto, nao o desenho.
          tts2[m].mode = "hidden";
          if (!tts2[m].__nvOuvida) {
            tts2[m].__nvOuvida = 1;
            tts2[m].addEventListener("cuechange", function () {
              var cues = this.activeCues;
              var ativa = (cues && cues.length) ? cues[cues.length - 1] : null;
              S.legTxt = ativa ? ("" + (ativa.text || "")) : "";
              S.legN = (S.legN | 0) + 1;
            });
          }
        } else {
          tts2[m].mode = "disabled";
        }
      }
      return 1;
    }
    return 0;
  }

  if (op === "leg_mudo") {
    if (!S.v) return 0;
    if (a) {
      try {
        var tts3 = S.v.textTracks;
        if (tts3) for (var p = 0; p < tts3.length; p++) tts3[p].mode = "disabled";
      } catch (e) {}
      S.legTxt = "";
    }
    return 1;
  }

  if (op === "leg_texto") {
    if (!dst || dstTam < 2) return 0;
    stringToUTF8(S.legTxt || "", dst, dstTam);
    return (S.legN | 0) + 1;
  }

  return 0;
});

// Indices do vetor de "estado". Tem de casar com a ordem escrita no JS acima.
enum { EST_POS, EST_DUR, EST_TOCANDO, EST_PRONTO, EST_ABERTO,
       EST_LARG, EST_ALT, EST_ERRO, EST_FIM, EST_BUFFIM, EST_BUFMS,
       EST_ERRCODE, EST_N };

#if defined(__EMSCRIPTEN_PTHREADS__)
// Mesma travessia de fio que video_tizen.c usa para webapis.avplay: o pedido
// viaja como PONTEIRO para uma struct na pilha do chamador (memoria
// compartilhada com -pthread, chamada sincrona, a pilha continua viva).
typedef struct {
  const char *cmd;
  const char *txt;
  double a, b, c, d;
  char  *dst;
  int    dstTam;
  double res;
} Pedido;

static int executarNoPrincipal(int p) {
  Pedido *q = (Pedido *)(uintptr_t)p;
  q->res = nv_av(q->cmd, q->txt, q->a, q->b, q->c, q->d, q->dst, q->dstTam);
  return 0;
}

static em_proxying_queue *filaPrincipal;
static pthread_once_t filaUmaVez = PTHREAD_ONCE_INIT;
static void criarFila(void) { filaPrincipal = em_proxying_queue_create(); }
static void executarNoPrincipalV(void *arg) { (void)executarNoPrincipal((int)(uintptr_t)arg); }
#endif

static double avChamar(const char *cmd, const char *txt,
                       double a, double b, double c, double d,
                       char *dst, int dstTam) {
#if defined(__EMSCRIPTEN_PTHREADS__)
  Pedido p;
  p.cmd = cmd; p.txt = txt;
  p.a = a; p.b = b; p.c = c; p.d = d;
  p.dst = dst; p.dstTam = dstTam; p.res = 0;
  if (emscripten_is_main_browser_thread()) {
    executarNoPrincipal((int)(uintptr_t)&p);
  } else {
    pthread_once(&filaUmaVez, criarFila);
    emscripten_proxy_sync(filaPrincipal,
                          emscripten_main_runtime_thread_id(),
                          executarNoPrincipalV, &p);
  }
  return p.res;
#else
  // Build --um-fio (sem -pthread, ver tools/tizen.sh): so ha o fio principal,
  // entao chamar nv_av direto e sempre chamar no fio que importa. Nao ha fila
  // de proxy para criar nem para vazar.
  return nv_av(cmd, txt, a, b, c, d, dst, dstTam);
#endif
}

#define AV0(cmd)              avChamar((cmd), NULL, 0, 0, 0, 0, NULL, 0)
#define AVS(cmd, s)           avChamar((cmd), (s), 0, 0, 0, 0, NULL, 0)
#define AVN(cmd, n)           avChamar((cmd), NULL, (n), 0, 0, 0, NULL, 0)

// ============================================================================
// ESTADO, espelhando o que video.h expoe
// ============================================================================
static int    ligado;        // video_iniciar deu certo
static int    temVideo;      // document.createElement('video') funciona
static int    ativo;         // ha sessao aberta — e o que abre o furo
static int    tocando, pronto, fim;
static double posSeg, durSeg, bufFimSeg;
static unsigned bufMs;
static int    vidW, vidH;
static int    houveErro;

static char   urlAtual[1024];
static int    fonteMp4;
static int    dvPedido;      // afirmacao do addon; sem uso aqui (ver video_hdr)

static VideoFaixa faixaAudio[NV_FAIXA_MAX], faixaLeg[NV_FAIXA_MAX];
static int nAudio, nLeg, audioAtual, legAtual = -1;
static int faixasLidas;

// Retangulo de DESTINO pedido pela interface, em 1920x1080. Guardado para nao
// repetir a mesma chamada a cada quadro (mesmo cuidado do Tizen/LG).
static int janX, janY, janW = 1920, janH = 1080;

// Recorte de FONTE em vigor (espaco do quadro decodificado), para
// video_recorte_reaplicar poder mandar tudo de novo sem o dedup.
static int cropSX, cropSY, cropSW, cropSH, cropAtivo;

// Mesmo avanco-com-repouso do Tizen/LG e pela mesma razao medida la: segurar
// a seta produz varios seeks em menos de 1s, e so o ultimo interessa.
#define SEEK_REPOUSO_MS 350
static double seekAlvo;
static Uint32 seekEm;

// Sonda de MKV: so existe com -pthread de verdade (ver a nota grande no topo
// do arquivo). Sem isto, video_creditos() fica em 0 ("arquivo nao diz") e
// video_mkv_sondado() devolve 2 ("sem sonda"), os dois ja definidos por
// video.h para o caso em que a informacao simplesmente nao existe.
static double creditosNomeado, creditosUltimo;
#if defined(__EMSCRIPTEN_PTHREADS__)
static pthread_t fioMkv;
static int       fioMkvVivo, mkvPendente;
// Definida bem abaixo (junto de aplicarIdiomasDoMkv); video_sondar_mkv_agora
// a usa antes dela existir no arquivo.
static void *lerMkv(void *arg);
#endif

static VideoLegendaEstilo estilo = { 120, 0, 0, 3, 1, 0, 0, 0 };
static int temEstilo;

// Definida bem abaixo: video_bombear() a chama a cada transicao para
// "pronto", antes dela existir no arquivo (mesma ordem de declaracao adiantada
// que video_tizen.c usa para aplicarEstilo).
static void lerFaixas(void);

// URLs de addons podem carregar token/assinatura na query — nunca vao para o
// log inteiras (#8). So o HOST, que basta para correlacionar duas linhas sem
// expor credencial nenhuma.
static void hospedeApenas(const char *url, char *dst, size_t tam) {
  const char *ini, *fim2, *esq;
  size_t n;
  if (!dst || !tam) return;
  dst[0] = 0;
  if (!url) return;
  esq = strstr(url, "://");
  ini = esq ? esq + 3 : url;
  fim2 = ini;
  while (*fim2 && *fim2 != '/' && *fim2 != '?' && *fim2 != '#') fim2++;
  n = (size_t)(fim2 - ini);
  if (n >= tam) n = tam - 1;
  memcpy(dst, ini, n);
  dst[n] = 0;
}

// ============================================================================

int video_iniciar(void) {
  if (ligado) return 1;
  temVideo = (int)AV0("disp");
  ligado = 1;
  if (!temVideo) {
    // Nao fatal e nao pode parecer fatal — mesma postura do Tizen/LG quando o
    // pipeline nativo nao existe: o resto do app segue utilizavel.
    printf("[video] elemento <video> indisponivel: sem reproducao neste ambiente\n");
    fflush(stdout);
    marco("video html5 ausente");
    return 0;
  }
  printf("[video] elemento <video> disponivel\n"); fflush(stdout);
  return 1;
}

int video_iniciar_auto(void) { return video_iniciar(); }
// Sem hub LS2 neste alvo: nada aqui pode negar registro por permissao.
int video_registro_negado(void) { return 0; }

int video_tocar(const char *url) {
  double r;
  char host[256];
  if (!url || !*url) return 0;
  if (!ligado) video_iniciar();
  if (!temVideo) return 0;

  creditosNomeado = creditosUltimo = 0.0;
  snprintf(urlAtual, sizeof urlAtual, "%s", url);
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; faixasLidas = 0;
  posSeg = durSeg = bufFimSeg = 0; bufMs = 0; vidW = vidH = 0; houveErro = 0; fim = 0;
  seekEm = 0;
  cropAtivo = 0;
#if defined(__EMSCRIPTEN_PTHREADS__)
  mkvPendente = !fonteMp4;
#endif

  r = AVS("abrir", url);
  if (r < 1) {
    marco("html5 video: abrir falhou");
    ativo = 0;
    return 0;
  }
  // ativo=1 JA, antes do loadedmetadata: e ele que abre o furo em player.c, e
  // abrir cedo nao custa nada (atras do furo so ha o <video> preto por
  // enquanto) — mesma decisao do Tizen/LG.
  ativo = 1;
  hospedeApenas(url, host, sizeof host);
  printf("[video] html5 tocar hls=%s url=%s\n",
         r >= 3 ? "1 hls.js" : (r >= 2 ? "1 nativo" : "0"), host);
  fflush(stdout);
  marco("html5 video aberto");
  // O retangulo corrente vai junto: quem pediu a janela antes de haver sessao
  // (main.c faz isso) seria ignorado de outra forma.
  avChamar("rect", NULL, janX, janY, janW, janH, NULL, 0);
  return 1;
}

void video_bombear(void) {
  double est[EST_N];
  int estavaPronto = pronto;

  if (seekEm && SDL_GetTicks() >= seekEm) {
    seekEm = 0;
    AVN("buscar", seekAlvo);
    { char m[48]; snprintf(m, sizeof m, "seek para %ds", (int)seekAlvo); marco(m); }
  }

  if (!temVideo || !ativo) return;

  memset(est, 0, sizeof est);
  if (avChamar("estado", NULL, 0, 0, 0, 0, (char *)est, (int)sizeof est) < 1) return;
  posSeg    = est[EST_POS];
  durSeg    = est[EST_DUR];
  tocando   = est[EST_TOCANDO] != 0;
  pronto    = est[EST_PRONTO]  != 0;
  fim       = est[EST_FIM]     != 0;
  bufFimSeg = est[EST_BUFFIM];
  bufMs     = est[EST_BUFMS] > 0 ? (unsigned)est[EST_BUFMS] : 0;
  if (est[EST_LARG] > 0) vidW = (int)est[EST_LARG];
  if (est[EST_ALT]  > 0) vidH = (int)est[EST_ALT];
  if (est[EST_ERRO] != 0 && !houveErro) {
    houveErro = 1;
    { char msg[256]; msg[0] = 0;
      avChamar("erro_msg", NULL, 0, 0, 0, 0, msg, (int)sizeof msg);
      printf("[video] falhou: codigo=%d %s\n", (int)est[EST_ERRCODE], msg);
      fflush(stdout); }
    marco("html5 video: erro");
  }

  if (pronto && !estavaPronto) {
    printf("[video] metadados %dx%d dur=%.1f\n", vidW, vidH, durSeg);
    fflush(stdout);
    marco("html5 video pronto");
    lerFaixas();
  }
  if (pronto && !faixasLidas) lerFaixas();

#if defined(__EMSCRIPTEN_PTHREADS__)
  if (mkvPendente && !fioMkvVivo && urlAtual[0] && posSeg >= 5.0)
    video_sondar_mkv_agora();
#endif
}

void video_parar(void) {
  seekEm = 0;
#if defined(__EMSCRIPTEN_PTHREADS__)
  mkvPendente = 0;
#endif
  if (temVideo) AV0("parar");
  ativo = tocando = pronto = fim = 0;
  posSeg = durSeg = bufFimSeg = 0; bufMs = 0;
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; faixasLidas = 0;
  cropAtivo = 0;
  urlAtual[0] = 0;
}

void video_pausar(int pausado) {
  if (!temVideo || !ativo) return;
  AVN("pausar", pausado ? 1 : 0);
  tocando = !pausado;
}

void video_volume(int pct) {
  if (!temVideo) return;
  AVN("volume", pct);
}

void video_buscar(double segundos) {
  if (!temVideo || !ativo) return;
  if (segundos < 0) segundos = 0;
  posSeg   = segundos;         // a barra responde na hora
  seekAlvo = segundos;
  seekEm   = SDL_GetTicks() + SEEK_REPOUSO_MS;
}

// Aplica o retangulo de DESTINO. Grampeado a tela por consistencia de
// contrato com os outros dois alvos (video.h documenta isso como universal),
// ainda que aqui um <div> comum nao tivesse problema nenhum em ficar fora —
// nao ha plano de hardware para "apagar".
static int aplicarRect(int x, int y, int w, int h) {
  if (w < 1 || h < 1) return 0;
  if (x == janX && y == janY && w == janW && h == janH) return 1;
  if (!temVideo || !ativo) { janX = x; janY = y; janW = w; janH = h; return 1; }
  if (avChamar("rect", NULL, x, y, w, h, NULL, 0) < 1) return 0;
  janX = x; janY = y; janW = w; janH = h;
  return 1;
}

void video_janela(int x, int y, int w, int h) {
  if (w < 1 || h < 1) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 1920) w = 1920 - x;
  if (y + h > 1080) h = 1080 - y;
  aplicarRect(x, y, w, h);
}

// Recorte de FONTE de verdade, feito em CSS (ver a nota grande no topo do
// arquivo). "recorte" e mandado ANTES do destino de proposito: o JS aplica o
// layout na hora em cada uma das duas chamadas, e se o destino nao mudar
// (aplicarRect dedup) e so o crop tiver mudado, e a chamada "recorte" quem
// precisa ter atualizado a tela — nao ha uma terceira chamada para isso.
void video_janela_fonte(int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh) {
  if (sw <= 0 || sh <= 0) {
    cropAtivo = 0;
    avChamar("recorte", NULL, 0, 0, 0, 0, NULL, 0);
    video_janela(dx, dy, dw, dh);
    return;
  }
  cropAtivo = 1;
  cropSX = sx; cropSY = sy; cropSW = sw; cropSH = sh;
  avChamar("recorte", NULL, sx, sy, sw, sh, NULL, 0);
  video_janela(dx, dy, dw, dh);
}

// Sempre 1: o recorte aqui e um <div overflow:hidden> comum, nao uma
// negociacao com firmware que pode recusar (compare com o Tizen, onde
// setVideoRoi pode nao existir e o retorno importa).
int video_recorte_fonte(void) { return 1; }

void video_recorte_reaplicar(void) {
  if (!temVideo || !ativo) return;
  if (cropAtivo) avChamar("recorte", NULL, cropSX, cropSY, cropSW, cropSH, NULL, 0);
  else           avChamar("recorte", NULL, 0, 0, 0, 0, NULL, 0);
  avChamar("rect", NULL, janX, janY, janW, janH, NULL, 0);   // sem dedup, de proposito
}

const char *video_url_atual(void) { return urlAtual; }
double video_pos(void)        { return posSeg; }
double video_duracao(void)    { return durSeg; }
double video_buffer_fim(void) { return bufFimSeg; }
unsigned video_bufferando_ms(void) { return bufMs; }
int    video_tocando(void)    { return tocando; }
int    video_pronto(void)     { return pronto; }
int    video_ativo(void)      { return ativo; }
int    video_falhou(void)     { return houveErro; }
// O navegador nao distingue "audio da fonte sem codec suportado" de qualquer
// outro erro de midia: o evento 'error' do <video> tem so o MediaError.code
// generico (1..4), sem um "so o audio falhou" separado do "tudo falhou".
// Inventar essa distincao aqui seria abrir uma segunda porta de erro que a
// API do navegador nao da como fechar direito — fica 0, como no Tizen.
int    video_audio_nao_suportado(void) { return 0; }
int    video_terminou(void)   { return fim; }

double video_creditos(void) {
  double dur;
  if (creditosNomeado > 1.0) return creditosNomeado;
  dur = video_duracao();
  if (creditosUltimo > 1.0 && dur > 1.0 && creditosUltimo > dur * 0.75)
    return creditosUltimo;
  return 0.0;
}

int  video_n_audio(void)   { return nAudio; }
int  video_n_legenda(void) { return nLeg; }
const VideoFaixa *video_audio(int i)   { return (i >= 0 && i < nAudio) ? &faixaAudio[i] : NULL; }
const VideoFaixa *video_legenda(int i) { return (i >= 0 && i < nLeg) ? &faixaLeg[i] : NULL; }
int video_legenda_ordinal_mkv(int i) { return i >= 0 && i < nLeg ? faixaLeg[i].ordinalMkv : -1; }

int video_mkv_sondado(void) {
#if defined(__EMSCRIPTEN_PTHREADS__)
  if (!urlAtual[0] || fonteMp4) return 2;
  return (mkvPendente || fioMkvVivo) ? 0 : 1;
#else
  (void)fonteMp4;
  return 2;   // build --um-fio: sem fio proprio para a sonda bloqueante (ver a nota no topo)
#endif
}

void video_sondar_mkv_agora(void) {
#if defined(__EMSCRIPTEN_PTHREADS__)
  if (!mkvPendente || fioMkvVivo || !urlAtual[0]) return;
  mkvPendente = 0;
  fioMkvVivo = 1;
  if (pthread_create(&fioMkv, NULL, lerMkv, NULL) != 0) fioMkvVivo = 0;
  else pthread_detach(fioMkv);
#endif
}

int video_audio_atual(void)   { return audioAtual; }
int video_legenda_atual(void) { return legAtual; }

// Definida abaixo (API publica): escolherAudioPreferido a usa antes dela ser
// definida no arquivo — mesma ordem de video_tizen.c.
void video_escolher_audio(int i);

// Mesma logica do Tizen/LG, palavra por palavra: sem preferencia, ou sem
// faixa que case, nao mexe — a escolha do navegador (ou do arquivo) e melhor
// que uma trocada por chute.
static void escolherAudioPreferido(void) {
  const char *pref = ling_audio();
  int i;
  if (!pref[0] || nAudio < 2) return;
  if (audioAtual >= 0 && audioAtual < nAudio &&
      faixaAudio[audioAtual].idioma[0] &&
      ling_casa(faixaAudio[audioAtual].idioma, pref)) return;
  for (i = 0; i < nAudio; i++) {
    if (!faixaAudio[i].idioma[0] || !ling_casa(faixaAudio[i].idioma, pref)) continue;
    printf("[video] audio preferido: %s (faixa %d de %d)\n",
           ling_nome(faixaAudio[i].idioma), i + 1, nAudio);
    fflush(stdout);
    video_escolher_audio(i);
    return;
  }
  printf("[video] nenhuma faixa de audio em '%s' entre as %d: fica a do arquivo\n",
         pref, nAudio);
  fflush(stdout);
}

#if defined(__EMSCRIPTEN_PTHREADS__)
// Mesma logica de casamento por CONTAGEM do Tizen (aplicarIdiomasDoMkv): o
// navegador nao da o TrackNumber do Matroska nem etiqueta idioma de forma
// confiavel em AudioTrack/TextTrack para faixas embutidas (varia por motor e
// por container), entao o idioma vem do CABECALHO do proprio arquivo — a
// mesma sonda que a LG e o Tizen ja usam para os creditos, aqui reaproveitada
// para idioma tambem, e com a MESMA guarda de seguranca: so aplica quando a
// quantidade de faixas daquele tipo bate entre o container e o navegador.
// Idioma errado e pior que idioma nenhum.
static void aplicarIdiomasDoMkv(const MkvFaixa *fx, int n) {
  int j, kA = 0, kL = 0, nA = 0, nL = 0, casou = 0;
  const int TIPO_AUDIO = 2, TIPO_LEG = 17;   // TrackType do Matroska

  for (j = 0; j < n; j++) {
    if (fx[j].tipo == TIPO_AUDIO) nA++;
    else if (fx[j].tipo == TIPO_LEG) nL++;
  }
  if (nA != nAudio && nL != nLeg) {
    printf("[mkv] contagem nao bate (container %d audio / %d legenda; navegador "
           "%d / %d): idioma NAO aplicado\n", nA, nL, nAudio, nLeg);
    fflush(stdout);
    return;
  }

  for (j = 0; j < n; j++) {
    VideoFaixa *f = NULL;
    if (fx[j].tipo == TIPO_AUDIO && nA == nAudio && kA < nAudio)
      f = &faixaAudio[kA++];
    else if (fx[j].tipo == TIPO_LEG && nL == nLeg && kL < nLeg)
      f = &faixaLeg[kL++];
    if (!f) continue;
    if (f->idioma[0]) continue;                 // o navegador ja sabia: nao mexer
    if (!fx[j].idioma[0] || !strcmp(fx[j].idioma, "und")) continue;
    snprintf(f->idioma, sizeof f->idioma, "%s", fx[j].idioma);
    casou++;
    if (fx[j].nome[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s  \xc2\xb7  %s",
               i18n(ling_nome(f->idioma)), fx[j].nome);
    else
      snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(f->idioma)));
  }
  printf("[mkv] %d faixa(s) ganharam idioma do container\n", casou);
  fflush(stdout);
  if (casou) escolherAudioPreferido();
}

static void *lerMkv(void *arg) {
  MkvFaixa fx[MKV_MAX_FAIXAS];
  MkvCap   caps[MKV_MAX_CAPS];
  char url[1024];
  int n, nCaps = 0;
  (void)arg;
  snprintf(url, sizeof url, "%s", urlAtual);
  n = mkv_faixas_e_caps(url, fx, MKV_MAX_FAIXAS, caps, MKV_MAX_CAPS, &nCaps);
  if (nCaps > 0) {
    creditosNomeado = mkv_creditos_nomeados(caps, nCaps);
    creditosUltimo  = nCaps > 1 ? caps[nCaps - 1].inicio : 0.0;
    printf("[mkv] %d capitulos; creditos nomeados em %.0fs, ultimo em %.0fs\n",
           nCaps, creditosNomeado, creditosUltimo);
    fflush(stdout);
  }
  if (n < 1) marco("mkv: nenhuma faixa lida (nao e MKV, ou Range falhou)");
  else aplicarIdiomasDoMkv(fx, n);
  fioMkvVivo = 0;
  return NULL;
}
#endif // __EMSCRIPTEN_PTHREADS__

// Le do JS a lista de faixas de audio/legenda e monta os rotulos. Chamada
// quando o navegador chega a "pronto" — antes disso audioTracks/textTracks
// podem ainda nao estar povoados.
static void lerFaixas(void) {
  char buf[4096];
  char *linha, *fim2;
  int n;
  buf[0] = 0;
  n = (int)avChamar("faixas", NULL, 0, 0, 0, 0, buf, (int)sizeof buf);
  if (n < 1) return;
  faixasLidas = 1;
  nAudio = nLeg = 0;
  for (linha = buf; *linha; linha = fim2) {
    char tipo;
    int idx = 0, ordinalMkv = -1;
    char idioma[8] = "", rot[48] = "";
    VideoFaixa *f;
    fim2 = strchr(linha, '\n');
    if (fim2) *fim2++ = 0; else fim2 = linha + strlen(linha);
    if (!*linha) continue;
    tipo = linha[0];
    { char *p1 = strchr(linha, '\t');
      char *p2 = p1 ? strchr(p1 + 1, '\t') : NULL;
      char *p3 = p2 ? strchr(p2 + 1, '\t') : NULL;
      char *p4 = p3 ? strchr(p3 + 1, '\t') : NULL;
      if (!p1 || !p2 || !p3 || !p4) continue;
      *p1 = *p2 = *p3 = *p4 = 0;
      idx = atoi(p1 + 1);
      snprintf(idioma, sizeof idioma, "%s", p2 + 1);
      snprintf(rot, sizeof rot, "%s", p3 + 1);
      ordinalMkv = atoi(p4 + 1);
    }
    if (tipo == 'A') {
      if (nAudio >= NV_FAIXA_MAX) continue;
      f = &faixaAudio[nAudio++];
    } else {
      if (nLeg >= NV_FAIXA_MAX) continue;
      f = &faixaLeg[nLeg++];
    }
    memset(f, 0, sizeof *f);
    f->numero = idx;
    f->ordinalMkv = tipo == 'T' ? ordinalMkv : -1;
    snprintf(f->idioma, sizeof f->idioma, "%s", idioma);
    if (idioma[0] && rot[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s  \xc2\xb7  %s", i18n(ling_nome(idioma)), rot);
    else if (idioma[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(idioma)));
    else if (rot[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
    else
      snprintf(f->rotulo, sizeof f->rotulo, "%s %d",
               i18n(tipo == 'A' ? "Audio" : "Legenda"),
               tipo == 'A' ? nAudio : nLeg);
  }
  printf("[video] faixas: %d audio, %d legenda\n", nAudio, nLeg);
  fflush(stdout);
  escolherAudioPreferido();
}

// SEM EQUIVALENTE: nem MediaCapabilities nem videoInfo do <video> publicam
// hdrType/Atmos de forma confiavel entre motores. Selo ausente e honesto;
// selo inventado mente para o dono no momento em que ele mais confia nele —
// mesma decisao do Tizen, pela mesma razao.
int  video_tem_atmos(void)        { return 0; }
int  video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void)       { return "desconhecido"; }

int  video_largura(void) { return vidW; }
int  video_altura(void)  { return vidH; }

// SEM EQUIVALENTE: quem decide o pipeline de HDR/cor aqui e o proprio motor
// do Chromium da TV a partir do fluxo, sem um campo que este lado possa
// contradizer — mesma decisao do Tizen.
int  video_pode_forcar_sdr(void) { return 0; }
void video_forcar_sdr(void) {}

void video_definir_dv(int dv) { dvPedido = dv ? 1 : 0; (void)dvPedido; }

// Um <video src> nao manda cabecalho nenhum por requisicao (nem Referer):
// stub de proposito, mesma limitacao documentada em video_tizen.c.
void video_definir_cabecalhos(const char *cabs) { (void)cabs; }

void video_definir_mp4(int ehMp4) { fonteMp4 = ehMp4; }

void video_escolher_audio(int i) {
  const VideoFaixa *f = video_audio(i);
  if (!temVideo || !ativo || !f) return;
  avChamar("faixa", "AUDIO", f->numero, 0, 0, 0, NULL, 0);
  audioAtual = i;
}

void video_escolher_legenda(int i) {
  if (!temVideo || !ativo) return;
  if (i < 0) {
    AVN("leg_mudo", 1);
    legAtual = -1;
    return;
  }
  { const VideoFaixa *f = video_legenda(i);
    if (!f) return;
    AVN("leg_mudo", 0);
    avChamar("faixa", "TEXT", f->numero, 0, 0, 0, NULL, 0);
    legAtual = i; }
}

int video_legenda_nativa(char *dst, int tam) {
  static int avisou;
  int n;
  if (!dst || tam < 2) return 0;
  dst[0] = 0;
  if (!temVideo || !ativo || legAtual < 0) return 0;
  n = (int)avChamar("leg_texto", NULL, 0, 0, 0, 0, dst, tam) - 1;
  if (n > 0 && !avisou) {
    avisou = 1;
    printf("[video] legenda embutida: cuechange entregando texto (%d evento(s))\n", n);
    fflush(stdout);
  }
  return dst[0] != 0;
}

// SEM CAMINHO UTIL: baixar a legenda para o app desenhar em GL (src/legenda.c)
// ja e o caminho que funciona nos tres alvos e ja tem estilo/sincronismo
// proprios (VideoLegendaEstilo.familia existe para ele). Um <track kind=
// "subtitles" src=...> poderia, em teoria, funcionar aqui — mas so quando o
// servidor da legenda manda CORS, o que os provedores tipicos (OpenSubtitles
// atras de um proxy de addon) nao garantem, e abriria um SEGUNDO caminho de
// legenda sem TV nenhuma para confirmar que ele presta. Recusa, com log —
// mesma semantica do Tizen (video_legenda_externa la tambem recusa e conta
// com o overlay GL).
void video_legenda_externa(const char *url) {
  char host[256];
  if (!url || !*url) return;
  hospedeApenas(url, host, sizeof host);
  printf("[video] legenda externa nao vai pelo <video> (sem CORS garantido); "
         "o overlay GL continua sendo o caminho; host=%s\n", host);
  fflush(stdout);
  marco("html5 video: legenda externa recusada (overlay GL cobre)");
}

// GUARDADO, nao aplicado ao <video>: o navegador nao tem os cinco metodos de
// estilo do uMS (fonte/cor/fundo/posicao/borda para legenda NATIVA). Legendas
// embutidas aqui seguem 'hidden' e desenho proprio (video_legenda_nativa);
// legendas externas usam o overlay GL de player.c/legenda.c, que tem seu
// proprio estilo — mesma nao-conexao que o Tizen documenta.
void video_legenda_estilo(const VideoLegendaEstilo *e) {
  if (!e) return;
  estilo = *e;
  temEstilo = 1;
  (void)estilo; (void)temEstilo;
}

void video_encerrar(void) {
  if (!ligado) return;
  video_parar();
  if (temVideo) AV0("encerrar");
  ligado = 0;
}

#endif // defined(__EMSCRIPTEN__) && defined(NV_VIDAA)
