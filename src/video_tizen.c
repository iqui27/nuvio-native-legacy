// Reproducao de video no alvo Samsung Tizen, pela API AVPlay do firmware.
//
// POR QUE UM ARQUIVO SEPARADO E NAO MAIS UM #else DENTRO DE video.c. O video.c
// tem 1600 linhas de LS2, dlopen e libAcbAPI — nada disso existe no navegador
// da TV Samsung, e o pior e que TUDO AQUILO COMPILA sob o emcc: o dlfcn.h do
// Emscripten traz cotos de dlopen/dlsym que devolvem NULL sem erro. O alvo
// Tizen caia nesse ramo por engano, linkava, subia, e simplesmente nao tinha
// video — sem uma linha de log dizendo por que. Agora video.c inteiro esta
// dentro de #ifndef __EMSCRIPTEN__ e o corpo do alvo Tizen e este arquivo, no
// mesmo padrao que src/rede.c ja usa para o mesmo problema (la a libcurl por
// dlopen virou XHR).
//
// O MODELO DE COMPOSICAO E O MESMO DO webOS E NAO MUDA. O video toca num PLANO
// DE HARDWARE atras da pagina (o <object type="application/avplayer"> do
// tools/tizen-shell.html, z-index 0); o canvas GL fica por cima (z-index 1) com
// fundo transparente, e gfx_furo() escreve alpha 0 na area do video para abrir
// o buraco por onde o plano aparece. Consequencia herdada: glReadPixels NUNCA
// vai fotografar o video, aqui como na LG.
//
// -----------------------------------------------------------------------------
// FIO: como este arquivo garante que o AVPlay so e tocado no fio principal
// -----------------------------------------------------------------------------
// As funcoes video_* sao chamadas do laco de quadro E de fios de trabalho (o
// caminho da legenda, por exemplo). `webapis.avplay` e um objeto do DOM da
// pagina: ele NAO existe no escopo de um Web Worker. Um EM_JS chamado de um
// worker roda no JS DO WORKER, onde `webapis` e undefined — a excecao morre
// dentro do worker e o unico sintoma e o video nunca comecar, calado.
//
// A solucao aqui e uma PORTA UNICA: existe exatamente UM ponto de contato com o
// JS (a funcao nv_av abaixo), e todo mundo passa por avChamar(), que confere
// emscripten_is_main_browser_thread() e, quando nao esta no fio principal,
// entrega o pedido por emscripten_sync_run_in_main_runtime_thread(). O pedido
// viaja como PONTEIRO para uma struct na pilha do chamador — com -pthread a
// memoria e compartilhada, e a chamada e SINCRONA, entao a pilha ainda esta
// viva quando o fio principal le. Uma assinatura so (EM_FUNC_SIG_II, "int
// f(int)") serve para as ~15 operacoes, em vez de uma por funcao.
//
// O fio principal escoa essa fila ao ceder o quadro (nv_ceder_quadro em
// main.c, que aguarda requestAnimationFrame): enquanto houver quadro, ha
// escoamento. Um fio de trabalho que peca algo durante um quadro caro espera no
// maximo esse quadro.
//
// POR QUE NAO MAIN_THREAD_EM_ASM, que faria o mesmo: o corpo dele e argumento
// de macro variadica e o pre-processador C QUEBRA NA PRIMEIRA VIRGULA de
// primeiro nivel do JS — literal de objeto, `var a = 1, b = 2`, lista de
// argumentos. Com um bloco de JS deste tamanho isso e questao de tempo. EM_JS e
// variadico no CORPO justamente para aceitar virgulas, e por isso o JS todo
// mora em EM_JS e so o desvio de fio e feito em C.
//
// POR QUE NAO EM_ASYNC_JS, mesmo com -sASYNCIFY ligado: aguardar promessa nao
// resolve nada aqui. prepareAsync do AVPlay nao devolve promessa, entrega dois
// callbacks; e ASYNCIFY suspenderia o fio CHAMADOR, que pode ser um worker —
// exatamente o lugar onde o AVPlay nao existe. O que faz falta e mudar de fio,
// nao esperar.
//
// -----------------------------------------------------------------------------
// O QUE NAO TEM EQUIVALENTE NO AVPlay (degrada com honestidade, nao inventa)
// -----------------------------------------------------------------------------
//   * hdrType e Dolby Atmos: no webOS saem do videoInfo/sourceInfo do uMS. O
//     AVPlay nao expoe nem um nem outro. video_hdr() devolve "desconhecido" e
//     video_tem_atmos()/video_tem_dolby_vision() devolvem 0 — com isso
//     player.c simplesmente NAO DESENHA selo nenhum (ver player.c:1480), que e
//     o resultado certo: selo ausente e honesto, selo inventado mente para o
//     dono justamente no ponto em que ele confia para saber se pegou a versao
//     boa do arquivo.
//   * recorte de FONTE (video_janela_fonte): FEITO com setVideoRoi. Esta nota
//     dizia o contrario ate 17/09, quando listar webapis.avplay na TV mostrou
//     os 46 metodos que ela tem de verdade — setVideoRoi entre eles. O que
//     faltava era ler o objeto em vez da documentacao.
//   * legenda externa por URL: ver video_legenda_externa.
//   * fim do buffer (video_buffer_fim): o AVPlay so informa PORCENTAGEM de
//     buffering (onbufferingprogress), nunca um instante. Devolve 0, que
//     video.h ja define como "desconhecido".
//
// NADA AQUI FOI EXECUTADO EM TV SAMSUNG — nao ha uma nesta bancada, e
// `webapis.avplay` nao existe no Chrome do desktop. O que esta verificado e:
// compila e linka; com o AVPlay ausente todas as funcoes falham sem derrubar o
// app; e, contra um duble de `webapis.avplay` que registra chamadas, a
// sequencia de abertura sai open -> setListener -> setDisplayMethod ->
// prepareAsync -> setDisplayRect -> play com os argumentos certos.
#ifdef __EMSCRIPTEN__

#include "video.h"
#include "linguas.h"
#include "idioma.h"
#include "marco.h"
#include "mkv.h"
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/threading.h>
#include <emscripten/proxying.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// ============================================================================
// A PORTA UNICA PARA O JS
// ============================================================================
//
// Uma funcao so, com um comando em texto. O comando em TEXTO e nao um enum
// numerico de proposito: e ele que aparece no log do duble de teste, e um
// numero ali obrigaria a consultar esta tabela para ler a sequencia de
// chamadas — que e justamente o unico teste que da para fazer sem a TV.
//
// `dst`/`dstTam` sao o buffer de saida quando a operacao devolve dados. Duas
// regras que ja custaram tempo em outros pontos deste port:
//   * o buffer vem de malloc/pilha do C e a escrita e feita DEPOIS, entao a
//     view (HEAPF64/HEAPU8) tem de ser indexada na hora do uso — nunca guardada
//     entre chamadas. Aqui isso e automatico porque cada operacao reindexa.
//   * "estado" devolve SO DOUBLES, oito deles, e nao uma struct mista. Um
//     campo int no meio obrigaria a acertar deslocamento e alinhamento em duas
//     linguagens; um vetor de um tipo so nao tem como sair de fase.
EM_JS(double, nv_av, (const char *cmd, const char *txt,
                      double a, double b, double c, double d,
                      char *dst, int dstTam), {
  var op = UTF8ToString(cmd);
  var s  = txt ? UTF8ToString(txt) : "";

  // Estado da sessao, no escopo da PAGINA. Nao pode viver num var de modulo:
  // este mesmo codigo JS e emitido tambem no bundle dos workers, e cada worker
  // teria a sua copia. Pendurar no objeto global deixa claro que a unica copia
  // que vale e a do fio principal — que e o unico que chega ate aqui.
  // window, e nao globalThis: globalThis e Chrome 71 e a TV 2020 (Tizen 5.5) e
  // M69. O polyfill de tools/tizen-globalthis.js ja cobriria, mas este codigo
  // nao precisa depender da ordem em que o tizen.sh monta o arquivo.
  var G = (typeof window !== "undefined") ? window : self;
  if (!G.__nvav) {
    G.__nvav = {
      pl: null,          // referencia ao webapis.avplay
      aberto: 0,         // open() foi aceito
      tocando: 0,
      pronto: 0,         // prepareAsync terminou
      posMs: 0,
      erro: "",
      fim: 0,
      bufPct: 0,
      larg: 0, alt: 0,
      trilhas: null,     // cache de getTotalTrackInfo(), ver a nota em "prepareAsync"
      geracao: 0,        // identidade monotônica da sessão que instalou callbacks
      legTxt: "",        // ultimo texto do onsubtitlechange (issue #122)
      legAte: 0,         // posMs em que ele vence; 0 = ate o proximo evento
      legN: 0,           // eventos recebidos nesta sessao (diagnostico)
      rect: [0, 0, 1920, 1080]
    };
  }
  var S = G.__nvav;

  // O AVPlay so existe dentro do .wgt na TV. No Chrome do desktop nao existe, e
  // isso NAO E DEFEITO: e o modo degradado que o app tem de aguentar de pe.
  function pl() {
    if (S.pl) return S.pl;
    try {
      if (typeof webapis !== "undefined" && webapis && webapis.avplay) {
        S.pl = webapis.avplay;
        return S.pl;
      }
    } catch (e) {}
    return null;
  }

  // IDENTIDADE, DE PROPOSITO: o espaco de coordenadas do app JA E o que o
  // setDisplayRect quer.
  //
  // A Samsung documenta que "the TV screen resolution is always treated as
  // 1920x1080 px, regardless of the application resolution", e manda converter
  // do viewport da aplicacao PARA esse espaco. O nosso buffer de desenho e
  // 1920x1080 fixo (atributos width/height do canvas) e video.h entrega
  // retangulo nessa mesma escala — entao a conversao pedida e a identidade.
  //
  // ESTA FUNCAO JA FEZ A CONTA AO CONTRARIO: escalava de 1920 PARA o tamanho do
  // canvas na tela, usando getBoundingClientRect. Era invisivel enquanto o
  // viewport CSS da TV fosse exatamente 1920 (razao 1), e poria o video a 2/3
  // do tamanho no canto superior esquerdo numa TV cujo viewport CSS fosse 1280.
  // A escala existia para o DESKTOP, onde o CSS reduz o canvas — mas no desktop
  // NAO HA webapis.avplay, entao nenhuma das duas chamadas daqui chega a rodar
  // la. Era conveniencia de teste contaminando o unico caminho que importa.
  function paraTela(x, y, w, h) {
    return [Math.round(x), Math.round(y), Math.round(w), Math.round(h)];
  }

  function ouvir(p, geracao) {
    // Os quatro eventos que o video.h precisa espelhar. No webOS eles chegam
    // pela assinatura LS2 e caem no aoEvento; aqui chegam por callback e caem
    // neste mesmo objeto S, que o "estado" abaixo entrega ao C uma vez por
    // quadro. O C nunca e chamado DE DENTRO do callback: reentrar no C a partir
    // do JS a qualquer momento e como o app trava sem mensagem.
    try {
      p.setListener({
        onbufferingprogress: function (pct) {
          if (S.geracao !== geracao) return;
          S.bufPct = pct | 0;
        },
        onbufferingcomplete: function () {
          if (S.geracao !== geracao) return;
          S.bufPct = 100;
        },
        onstreamcompleted:   function () {
          if (S.geracao !== geracao) return;
          S.tocando = 0; S.fim = 1;
        },
        oncurrentplaytime:   function (ms) {
          if (S.geracao !== geracao) return;
          S.posMs = +ms || 0;
        },
        onerror:             function (e) {
          if (S.geracao !== geracao) return;
          S.erro = "" + e; S.tocando = 0;
        },
        onevent:             function () {},
        // A LEGENDA EMBUTIDA E DO APP (issue #122). O AVPlay entrega o texto
        // aqui e NAO o desenha: a amostra oficial da Samsung
        // (SamsungDForum/PlayerAVPlaySubtitle, main.js) faz
        // innerHTML = text neste callback. Ate a 1.4.2 isto era vazio e a
        // faixa escolhida nunca aparecia. `duration` em ms a partir de agora;
        // texto vazio apaga.
        onsubtitlechange:    function (duration, text) {
          if (S.geracao !== geracao) return;
          var d = parseInt(duration, 10) || 0;
          S.legTxt = text ? "" + text : "";
          S.legAte = S.legTxt && d > 0 ? S.posMs + d : 0;
          S.legN = (S.legN | 0) + 1;
        },
        ondrmevent:          function () {}
      });
    } catch (e) {}
  }

  // MARCA CADA CHAMADA SINCRONA DO AVPLAY QUE FICA DENTRO DO CALLBACK DE
  // prepareAsync. O registro D1 1106 mostrou um longtask de 6,4 s antes de
  // "avplay prepared", mas sem esta separacao nao da para saber se o tempo
  // veio do firmware em prepareAsync, de setDisplayRect/play ou da leitura de
  // metadados. O shell ja expõe __nvDiag; em Chrome/contrato ele pode nao
  // existir, entao o medidor precisa ser inerte fora da build de diagnostico.
  function avAgora() {
    try {
      if (typeof performance !== "undefined" && performance && performance.now)
        return performance.now();
    } catch (e) {}
    return Date.now();
  }
  function avMedir(nome, antes) {
    var dt = avAgora() - antes;
    try {
      if (window.__nvDiag)
        window.__nvDiag("[video] " + nome + " " + (dt | 0) + " ms", 0);
    } catch (e) {}
  }

  if (op === "disp") return pl() ? 1 : 0;

  if (op === "abrir") {
    var p = pl();
    if (!p) return 0;
    var geracao = ++S.geracao;
    // Fechar antes de abrir. Sem isto o segundo titulo da sessao encontra o
    // player em estado PLAYING e o open() e recusado — o mesmo formato do bug
    // "abrir -> sair -> abrir" que o caminho da LG ja tinha.
    try { if (S.aberto) { p.stop(); p.close(); } } catch (e) {}
    S.aberto = 0; S.tocando = 0; S.pronto = 0; S.posMs = 0;
    S.erro = ""; S.fim = 0; S.bufPct = 0; S.larg = 0; S.alt = 0;
    S.trilhas = null;   // titulo novo: a lista de faixas do titulo anterior nao vale
    S.legTxt = ""; S.legAte = 0; S.legN = 0;
    try { p.open(s); } catch (e) { S.erro = "open: " + e; return 0; }
    S.aberto = 1;
    ouvir(p, geracao);
    // PLAYER_DISPLAY_MODE_FULL_SCREEN preenche o retangulo pedido, sem
    // letterbox proprio. E o certo aqui: player.c JA calculou o destino a
    // partir da proporcao do quadro (ver player.c:594), e deixar o AVPlay
    // encaixar de novo dentro dele aplicaria a mesma barra preta duas vezes.
    try { p.setDisplayMethod("PLAYER_DISPLAY_MODE_FULL_SCREEN"); } catch (e) {}
    // Dois tempos diferentes: `retorno` mede se a chamada JS bloqueou antes de
    // devolver; `ate-callback` inclui a espera normal de rede/demuxer ate o
    // firmware avisar que terminou. Sem os dois, um callback tardio parece
    // automaticamente um bloqueio do nosso fio.
    var tPrepare = avAgora();
    try {
      p.prepareAsync(function () {
        if (S.geracao !== geracao) return;
        avMedir("prepareAsync.ate-callback", tPrepare);
        S.pronto = 0;
        // Reaplica o retangulo apos o preparo. A API tambem permite IDLE;
        // o duble nao prova que uma chamada anterior se perderia na TV.
        // Ordem deste callback:
        // open -> setListener -> setDisplayMethod -> prepareAsync ->
        // setDisplayRect -> play -> getTotalTrackInfo.
        var r = paraTela(S.rect[0], S.rect[1], S.rect[2], S.rect[3]);
        var tRect = avAgora();
        try { p.setDisplayRect(r[0], r[1], r[2], r[3]); }
        catch (e) {}
        finally { avMedir("prepare.setDisplayRect", tRect); }
        // READY permite getTotalTrackInfo apenas com prepare SINCRONO.
        // Aqui usamos prepareAsync: entrar em PLAYING antes de ler metadados.
        var tPlay = avAgora();
        try { p.play(); S.tocando = 1; S.pronto = 1; }
        catch (e) {
          S.pronto = 0; S.tocando = 0; S.aberto = 0;
          S.trilhas = null; S.erro = "play: " + e;
          ++S.geracao;
          try { p.stop(); } catch (e2) {}
          try { p.close(); } catch (e3) {}
          return;
        }
        finally { avMedir("prepare.play", tPlay); }
        // Dimensoes usadas pelos modos de zoom do player.
        //
        // A LISTA FICA EM CACHE (S.trilhas), e nao so as dimensoes: getTotalTrackInfo
        // e uma chamada sincrona do firmware, e ate 21/09/2026 este mesmo prepare
        // pedia essa lista DUAS VEZES — aqui, so para achar a faixa VIDEO, e de
        // novo pela op "faixas" (lerFaixas em video_tizen.c), so para AUDIO/TEXT,
        // a poucos quadros de distancia. Registro 1106 (D1, Tizen 6) mede
        // raf-max=7508 bem em cima da marca "avplay prepared", que e exatamente
        // esta janela: prepareAsync -> play -> getTotalTrackInfo (aqui) seguido,
        // no proximo quadro, de mais getTotalTrackInfo (faixas) e getCurrentStreamInfo
        // (infofluxo). Nao ha prova de que ESTA chamada sozinha custe segundos —
        // sem TV Samsung na bancada nao da para medir o firmware — mas chama-la
        // duas vezes custa o dobro do que uma custaria, e a segunda nao precisa
        // existir: a "faixas" abaixo agora le S.trilhas em vez de pedir de novo.
        var tTracks = avAgora();
        try {
          var tr = p.getTotalTrackInfo();
          if (!tr) tr = [];
          S.trilhas = tr;
          // Diagnóstico somente de forma/quantidade: não registra extra_info,
          // nomes ou qualquer valor que pudesse carregar URL. Esta é a leitura
          // bruta feita após play; a operação `faixas` abaixo pode reutilizar o
          // mesmo vetor sem transformar cache vazio em "sem legenda" silencioso.
          if (window.__nvDiag) {
            var tiposBrutos = [];
            for (var ti = 0; ti < (tr || []).length; ti++)
              tiposBrutos.push(("" + (tr[ti] && tr[ti].type || "?")).toUpperCase());
            window.__nvDiag("[video] getTotalTrackInfo bruto n=" +
                            ((tr || []).length | 0) + " tipos=" + tiposBrutos.join(","), 0);
          }
          for (var i = 0; i < tr.length; i++) {
            if (("" + tr[i].type).toUpperCase() !== "VIDEO") continue;
            var x = tr[i].extra_info;
            if (typeof x === "string") { try { x = JSON.parse(x); } catch (e2) { x = {}; } }
            S.larg = parseInt(x.Width || x.width || 0, 10) || 0;
            S.alt  = parseInt(x.Height || x.height || 0, 10) || 0;
          }
        } catch (e) {}
        finally { avMedir("prepare.getTotalTrackInfo", tTracks); }
      }, function (e) {
        if (S.geracao !== geracao) return;
        S.erro = "prepare: " + e;
      });
    } catch (e) { S.erro = "prepareAsync: " + e; return 0; }
    finally { avMedir("prepareAsync.retorno", tPrepare); }
    return 1;
  }

  if (op === "parar") {
    var p2 = pl();
    ++S.geracao;
    S.tocando = 0; S.pronto = 0; S.fim = 0; S.posMs = 0;
    S.trilhas = null; S.legTxt = ""; S.legAte = 0;
    if (!p2 || !S.aberto) { S.aberto = 0; return 0; }
    S.aberto = 0;
    try { p2.stop(); } catch (e) {}
    try { p2.close(); } catch (e) {}
    return 1;
  }

  if (op === "pausar") {
    var p3 = pl();
    if (!p3 || !S.aberto) return 0;
    try {
      if (a) { p3.pause(); S.tocando = 0; }
      else   { p3.play();  S.tocando = 1; }
    } catch (e) { return 0; }
    return 1;
  }

  if (op === "buscar") {
    var p4 = pl();
    if (!p4 || !S.aberto) return 0;
    // seekTo quer MILISSEGUNDOS. Os dois callbacks sao obrigatorios em varios
    // firmwares: omitir lanca TypeError e o seek nao sai.
    // MEDIDO NOS REGISTROS DE 20/09/2026 (muresis, Tizen 6.0; 5ac57481): cada
    // seek deixa o fio principal parado de 1 a 8,5 s DENTRO de video_bombear
    // ([quadro] upd=8280 logo depois de "[t] seek para 820s"). Este relogio
    // diz se e o proprio seekTo que bloqueia (a documentacao o chama de
    // assincrono, os callbacks existem, mas o firmware pode esperar o demuxer
    // aqui mesmo) ou outra coisa no mesmo quadro. So depois disso vale
    // tentar pause/seek/play ou outra estrategia.
    S.legTxt = ""; S.legAte = 0;   // a fala de antes do salto nao vale no destino
    var t0 = performance.now();
    try { p4.seekTo(a | 0, function () {}, function () {}); } catch (e) { return 0; }
    var dt = performance.now() - t0;
    if (dt > 50 && window.__nvDiag) window.__nvDiag("[video] seekTo bloqueou " + (dt | 0) + " ms", 0);
    return 1;
  }

  // RECORTE DE FONTE DE VERDADE: setVideoRoi.
  //
  // Este arquivo afirmava em tres lugares que o AVPlay "nao recorta a FONTE" e
  // que so havia setDisplayRect. Errado. Listando webapis.avplay na propria
  // QN85Q70AAGXZD em 17/09 aparecem 46 metodos, e entre eles setVideoRoi — o
  // equivalente Samsung do par sourceInput/displayOutput do tv.display da LG.
  //
  // A afirmacao nasceu de ler a documentacao, nao o objeto. O aparelho estava a
  // um Object.keys de distancia o tempo todo.
  //
  // Devolve 1 so quando a chamada passou: quem chama precisa saber, porque a
  // alternativa (aumentar o destino para fora da tela) nao funciona aqui.
  if (op === "roi") {
    var p8 = pl();
    if (!p8 || !S.aberto) return 0;
    try { p8.setVideoRoi(a, b, c, d); } catch (e) {
      if (!S.avisouRoi) { S.avisouRoi = 1;
        try { console.log("[video] setVideoRoi recusou: " + e); } catch (e2) {} }
      return 0;
    }
    return 1;
  }

  if (op === "rect") {
    S.rect = [a, b, c, d];
    var p5 = pl();
    if (!p5 || !S.aberto) return 0;
    var r5 = paraTela(a, b, c, d);
    try { p5.setDisplayRect(r5[0], r5[1], r5[2], r5[3]); } catch (e) { return 0; }
    return 1;
  }

  if (op === "estado") {
    // OITO DOUBLES, nesta ordem, e a mesma no C (ver EST_*). Um tipo so: sem
    // deslocamento a acertar, sem alinhamento a supor.
    // dstTam vem em BYTES, como em "faixas": um so contrato para o buffer.
    if (!dst || dstTam < 64) return 0;
    var p6 = pl();
    var dur = 0;
    if (p6 && S.aberto && S.pronto) {
      try { dur = (+p6.getDuration() || 0) / 1000.0; } catch (e) {}
      // oncurrentplaytime cobre o caso normal; getCurrentTime cobre o instante
      // logo apos um seek, em que o evento ainda nao veio.
      try { var t = +p6.getCurrentTime(); if (t >= 0) S.posMs = t; } catch (e) {}
    }
    var o = dst >> 3;
    HEAPF64[o + 0] = S.posMs / 1000.0;
    HEAPF64[o + 1] = dur;
    HEAPF64[o + 2] = S.tocando ? 1 : 0;
    HEAPF64[o + 3] = S.pronto ? 1 : 0;
    HEAPF64[o + 4] = S.aberto ? 1 : 0;
    HEAPF64[o + 5] = S.larg;
    HEAPF64[o + 6] = S.alt;
    HEAPF64[o + 7] = S.erro ? 1 : 0;
    return 1;
  }

  // O QUE O DECODER ESTA REALMENTE RECEBENDO: getCurrentStreamInfo.
  //
  // Descoberto junto com setVideoRoi, listando webapis.avplay na TV. Este
  // arquivo dizia que "o AVPlay nao publica hdrType" e que getTotalTrackInfo
  // traz "codec e idioma, nada sobre HDR" — as duas afirmacoes saem da
  // documentacao, nao do objeto.
  //
  // getCurrentStreamInfo descreve a faixa EM USO, que e a diferenca que importa
  // para o selo: a afirmacao do addon mente (a LG ja provou isso com um arquivo
  // anunciado como DV que chegava HDR10), e isto aqui e o que chegou.
  //
  // Devolve o JSON cru. Quem interpreta e o C, uma vez por titulo. Enquanto o
  // formato exato nao estiver visto numa TV, isto serve de DIAGNOSTICO: a linha
  // sai no log e o proximo passo e escrito com o dado na mao, nao com palpite.
  if (op === "infofluxo") {
    var p9 = pl();
    if (!p9 || !S.pronto || !dst || dstTam < 8) return 0;
    var info;
    try { info = p9.getCurrentStreamInfo(); } catch (e) { return 0; }
    if (!info) return 0;
    var txt;
    try { txt = JSON.stringify(info); } catch (e) { txt = "" + info; }
    stringToUTF8(txt, dst, dstTam);
    return 1;
  }

  // O TEXTO DO ERRO, e nao so o "houve erro" do estado. O C so via o 1 de
  // EST_ERRO e escrevia "avplay: onerror": um log de campo mostrou oito fontes
  // seguidas caindo assim, sem uma pista de por que (rede, codec, DRM, formato)
  // — e a razao estava aqui, em S.erro, desde sempre. Lido uma vez por erro.
  if (op === "erro") {
    if (!S.erro || !dst || dstTam < 2) return 0;
    stringToUTF8("" + S.erro, dst, dstTam);
    return 1;
  }

  if (op === "faixas") {
    var p7 = pl();
    if (!p7 || !S.pronto || !dst || dstTam < 4) return 0;
    // Reusa a lista que o prepare ja pediu (ver a nota la em cima, S.trilhas):
    // so cai em getTotalTrackInfo() de novo se o cache nao existir (prepare
    // caiu no catch, ou algo limpou S.trilhas antes desta chamada chegar).
    var lista = S.trilhas;
    if (!lista) {
      var tTracksFallback = avAgora();
      try {
        lista = p7.getTotalTrackInfo();
        S.trilhas = lista;
      }
      catch (e) { return 0; }
      finally { avMedir("faixas.getTotalTrackInfo", tTracksFallback); }
    }
    if (!lista || !lista.length) {
      if (window.__nvDiag)
        window.__nvDiag("[video] getTotalTrackInfo bruto n=0 na leitura de faixas", 1);
      return 0;
    }
    // Uma linha por faixa: "<A|T>\t<indice>\t<idioma>\t<rotulo>\n". Texto e nao
    // uma struct porque o numero de faixas e o tamanho dos rotulos variam, e
    // atravessar isso como vetor de structs obrigaria a combinar o layout dos
    // dois lados por nada — sao doze faixas no maximo, uma vez por titulo.
    var out = "", n = 0;
    // Letra ASCII, e caractere que pode fazer parte de um NOME de campo. Sao
    // duas perguntas diferentes: "_" nao e letra mas continua um nome, e e por
    // isso que "lang" nao pode casar dentro de "track_lang".
    function ehLetra(c) { return (c >= "a" && c <= "z") || (c >= "A" && c <= "Z"); }
    function ehNome(c) { return ehLetra(c) || (c >= "0" && c <= "9") || c === "_"; }
    for (var i = 0; i < lista.length; i++) {
      var t = ("" + lista[i].type).toUpperCase();
      if (t !== "AUDIO" && t !== "TEXT") continue;
      var x = lista[i].extra_info;
      var cru = typeof x === "string" ? x : "";
      if (typeof x === "string") { try { x = JSON.parse(x); } catch (e) { x = null; } }
      // O EXTRA_INFO NEM SEMPRE E JSON. O dono reportou em 17/09 que na
      // QN85Q70AAGXZD as faixas de audio saem como "Audio 1", "Audio 2" — ou
      // seja, idioma E rotulo vazios. Se o firmware entrega uma string que nao
      // e JSON, o catch de cima transformava tudo num objeto vazio e o conteudo
      // era descartado sem deixar rastro.
      //
      // Duas saidas, nesta ordem: continua preferindo o objeto quando ele
      // existe, e cai numa busca por texto na string crua quando nao existe. A
      // busca cobre os nomes que o proprio firmware usa nas duas formas que ja
      // se viu em campo (JSON e "chave=valor").
      if (!x) x = {};
      var lang = x.language || x.lang || x.track_lang || x.Language || "";
      // SEM REGEX AQUI, de proposito: o corpo de um EM_JS e ARGUMENTO DE MACRO
      // do pre-processador C, e ele separa argumentos por virgula olhando so
      // PARENTESES. Um quantificador como {2,3} tem virgula fora de parenteses
      // e parte a macro ao meio — o erro sai como "unterminated function-like
      // macro invocation" apontando para a linha do EM_JS, dezenas de linhas
      // acima do culpado. Ja custou um build.
      //
      // A VERSAO ANTERIOR DESTA BUSCA LIA O CAMPO SEGUINTE COMO IDIOMA. Ela
      // procurava "lang" em qualquer lugar e colhia as 3 primeiras letras que
      // aparecessem depois. Contra o TEXT que esta MEDIDO logo acima
      // ({"track_num":"0","track_lang":"","subtitle_type":"-1"}) ela achava
      // "lang" dentro de track_lang, atravessava o valor vazio e devolvia
      // "sub", de subtitle_type. Toda legenda virava idioma "sub": a tela
      // mostrava "SUB" e, pior, aplicarIdiomasDoMkv pula faixa que ja tem
      // idioma, entao o idioma REAL vindo da sonda de MKV nunca era aplicado —
      // anulando o proprio conserto que esta busca existe para apoiar. De
      // quebra, "language=eng", a forma que ela dizia cobrir, devolvia vazio:
      // ao achar "uag" o laco terminava antes do valor.
      //
      // Agora a chave tem de casar em FRONTEIRA e tem de vir seguida de
      // separador (":" ou "="). Valor vazio encerra a busca daquela chave em
      // vez de seguir colhendo letras do campo seguinte.
      if (!lang && cru) {
        // split(" ") em vez de literal com virgula: o corpo de um EM_JS e
        // argumento de macro do pre-processador C. Ver a nota logo acima.
        var chaves = "track_lang language lang".split(" ");
        var bc = cru.toLowerCase();
        for (var ni = 0; ni < chaves.length; ni++) {
          if (lang) break;
          var k = chaves[ni];
          var de = 0;
          for (;;) {
            var bx = bc.indexOf(k, de);
            if (bx < 0) break;
            de = bx + k.length;
            var antes = bx > 0 ? bc.charAt(bx - 1) : " ";
            var dps = de < bc.length ? bc.charAt(de) : " ";
            if (ehNome(antes) || ehNome(dps)) continue;
            var q = de;
            while (q < cru.length && (cru.charAt(q) === '"' || cru.charAt(q) === " ")) q++;
            if (q >= cru.length) break;
            if (cru.charAt(q) !== ":" && cru.charAt(q) !== "=") continue;
            q++;
            while (q < cru.length && (cru.charAt(q) === " " || cru.charAt(q) === '"')) q++;
            var ac = "";
            while (q < cru.length && ac.length < 3 && ehLetra(cru.charAt(q))) {
              ac += cru.charAt(q);
              q++;
            }
            if (ac.length >= 2) lang = ac;
            // Achou a chave: o valor dela e a resposta, mesmo vazio. Seguir
            // procurando letras adiante e o defeito que se esta consertando.
            break;
          }
        }
      }
      if (lang === "und" || lang === "(null)" || lang === "null") lang = "";
      // DIAGNOSTICO, uma vez por sessao e so quando nao houve idioma: sem o
      // extra_info CRU nao da para saber se o firmware nao manda o idioma ou se
      // manda com outro nome. Adivinhar nome de chave sem ver o dado foi o que
      // deixou este defeito de pe.
      if (!lang && !S.avisouFaixa) {
        S.avisouFaixa = 1;
        try {
          console.log("[video] faixa sem idioma, extra_info cru = " +
                      (cru || JSON.stringify(lista[i].extra_info)));
        } catch (e) {}
      }
      // Rotulo cru do firmware, quando houver. O nome legivel do idioma e
      // montado do lado C, que ja tem a tabela (linguas.c) — duplicar aqui
      // criaria uma segunda lista de idiomas para divergir da primeira.
      var rot = x.track_name || x.name || "";
      // ATENCAO: as barras invertidas vao DOBRADAS. O corpo de um EM_JS
      // atravessa o compilador C como literal de string, entao "\n" viraria uma
      // QUEBRA DE LINHA de verdade dentro do literal JS — que e erro de sintaxe,
      // e o build morre no acorn com "Unterminated regular expression", a
      // dezenas de milhares de colunas de distancia do culpado. Ja aconteceu.
      var ordinalMkv = parseInt(x.track_num, 10);
      if (isNaN(ordinalMkv) || ordinalMkv < 0) ordinalMkv = -1;
      out += (t === "AUDIO" ? "A" : "T") + "\\t" + (lista[i].index | 0) +
             "\\t" + lang + "\\t" + ("" + rot).replace(/[\\t\\n]/g, " ") +
             "\\t" + ordinalMkv + "\\n";
      n++;
    }
    stringToUTF8(out, dst, dstTam);
    return n;
  }

  if (op === "faixa") {
    var p8 = pl();
    if (!p8 || !S.aberto) return 0;
    if (s === "TEXT") { S.legTxt = ""; S.legAte = 0; }
    try { p8.setSelectTrack(s, a | 0); } catch (e) { return 0; }
    return 1;
  }

  if (op === "leg_mudo") {
    // O AVPlay nao tem "desligar a faixa de texto": tem setSilentSubtitle, que
    // manda o player parar de DESENHAR a legenda. E o mais proximo que existe.
    var p9 = pl();
    if (!p9 || !S.aberto) return 0;
    if (a) { S.legTxt = ""; S.legAte = 0; }
    try { p9.setSilentSubtitle(a ? true : false); } catch (e) { return 0; }
    return 1;
  }

  // O TEXTO DA LEGENDA EMBUTIDA EM VIGOR, para o player desenhar (#122).
  // Devolve o numero de eventos recebidos (+1), e o texto em dst quando vale.
  if (op === "leg_texto") {
    if (!dst || dstTam < 2) return 0;
    var vale = S.aberto && S.legTxt && (!S.legAte || S.posMs < S.legAte);
    stringToUTF8(vale ? S.legTxt : "", dst, dstTam);
    return (S.legN | 0) + 1;
  }

  if (op === "leg_arquivo") {
    var pa = pl();
    if (!pa || !S.aberto) return 0;
    try { pa.setExternalSubtitlePath(s); } catch (e) { return 0; }
    return 1;
  }

  if (op === "encerrar") {
    var pb = pl();
    ++S.geracao;
    try { if (pb && S.aberto) { pb.stop(); pb.close(); } } catch (e) {}
    S.aberto = 0; S.tocando = 0; S.pronto = 0; S.trilhas = null;
    return 1;
  }

  return 0;
});

// Indices do vetor devolvido por "estado". Precisam casar com a ordem escrita
// no JS acima, e sao a UNICA coisa combinada entre os dois lados.
enum { EST_POS, EST_DUR, EST_TOCANDO, EST_PRONTO, EST_ABERTO,
       EST_LARG, EST_ALT, EST_ERRO, EST_N };

// O pedido que atravessa para o fio principal. Fica na PILHA do chamador: com
// -pthread a memoria e compartilhada e avChamar() e sincrona, entao o quadro
// ainda esta vivo quando o fio principal le.
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

// Fila de proxy criada UMA VEZ: por chamada vazaria uma fila por operacao de
// video, e duas threads chegando juntas na primeira criariam duas.
static em_proxying_queue *filaPrincipal;
static pthread_once_t filaUmaVez = PTHREAD_ONCE_INIT;
static void criarFila(void) { filaPrincipal = em_proxying_queue_create(); }

// emscripten_proxy_sync quer void(void*); executarNoPrincipal e int(int) porque
// nasceu para a assinatura EM_FUNC_SIG_II da API antiga. O resultado nao se
// perde: sai em p.res, dentro do proprio Pedido.
static void executarNoPrincipalV(void *arg) { (void)executarNoPrincipal((int)(uintptr_t)arg); }


static void logarErro(const char *onde);

static double avChamar(const char *cmd, const char *txt,
                       double a, double b, double c, double d,
                       char *dst, int dstTam) {
  Pedido p;
  p.cmd = cmd; p.txt = txt;
  p.a = a; p.b = b; p.c = c; p.d = d;
  p.dst = dst; p.dstTam = dstTam; p.res = 0;
  if (emscripten_is_main_browser_thread()) {
    executarNoPrincipal((int)(uintptr_t)&p);
  } else {
    // emscripten_sync_run_in_main_runtime_thread FOI REMOVIDA do runtime. Ela
    // ainda e DECLARADA em emscripten/threading_legacy.h, entao o compilador
    // aceita e so o wasm-ld reclama, com "undefined symbol:
    // emscripten_sync_run_in_main_runtime_thread_" repetido por chamada — e
    // nada na mensagem aponta para o cabecalho legado. A substituta e
    // emscripten_proxy_sync, de emscripten/proxying.h, que troca a assinatura
    // EM_FUNC_SIG por uma funcao void(void*) e uma fila explicita.
    pthread_once(&filaUmaVez, criarFila);
    emscripten_proxy_sync(filaPrincipal,
                          emscripten_main_runtime_thread_id(),
                          executarNoPrincipalV, &p);
  }
  return p.res;
}

#define AV0(cmd)              avChamar((cmd), NULL, 0, 0, 0, 0, NULL, 0)
#define AVS(cmd, s)           avChamar((cmd), (s), 0, 0, 0, 0, NULL, 0)
#define AVN(cmd, n)           avChamar((cmd), NULL, (n), 0, 0, 0, NULL, 0)

// ============================================================================
// ESTADO, espelhando o que video.h expoe
// ============================================================================
//
// Os mesmos campos que o aoEvento do webOS preenche (video.c:519). O C nunca e
// chamado de dentro de um callback JS: video_bombear() puxa o estado UMA VEZ
// POR QUADRO, no fio principal, e todos os getters passam a ler memoria C. Duas
// razoes: um getter como video_pos() e chamado varias vezes por quadro e uma
// travessia de fio por chamada seria cara; e reentrar no C a partir de um
// callback do navegador e um dos jeitos conhecidos de travar tudo sem mensagem.
static int    ligado;        // video_iniciar deu certo
static int    temAvplay;     // o firmware oferece webapis.avplay
static int    ativo;         // ha sessao aberta — e o que abre o furo
static int    tocando, pronto;
static double posSeg, durSeg;
static int    vidW, vidH;
static int    houveErro;

static char   urlAtual[1024];
static int    fonteMp4;
static int    dvPedido;      // afirmacao do addon; sem uso no AVPlay (ver hdr)

// URLs de addons podem conter credenciais, tokens ou assinaturas na query.
// O diagnóstico precisa correlacionar duas operações sem despejar a fonte no
// log, então usa somente um identificador FNV-1a estável e não reversível na
// prática para este volume de eventos.
static unsigned video_id_fonte(const char *url) {
  unsigned h = 2166136261u;
  const unsigned char *p = (const unsigned char *)url;
  if (!p) return 0;
  while (*p) { h ^= *p++; h *= 16777619u; }
  return h;
}

static VideoFaixa faixaAudio[NV_FAIXA_MAX], faixaLeg[NV_FAIXA_MAX];
static int nAudio, nLeg, audioAtual, legAtual = -1;
static int faixasLidas;

// Retangulo pedido pela interface, em 1920x1080. Guardado para nao repetir a
// mesma chamada a cada quadro — o mesmo cuidado do caminho da LG.
static int janX, janY, janW = 1920, janH = 1080;

// AVANCO COM REPOUSO, identico ao do webOS e pela mesma razao medida la:
// segurar a seta produzia quatro seeks em 0,8 s, e os tres primeiros sao
// descartados assim que o quarto chega. A posicao MOSTRADA muda na hora; o
// comando ao player e que espera.
#define SEEK_REPOUSO_MS 350
static double seekAlvo;
static Uint32 seekEm;

// Sonda de MKV: mesma da LG (le capitulos por Range para saber onde comecam os
// creditos), com OUTRO gatilho. Na LG ela espera 20 s de buffer a frente, que
// o bufferRange do uMS informa; o AVPlay nao informa instante nenhum de buffer,
// so porcentagem. Aqui o gatilho e "a posicao ja andou 5 s de verdade", que e o
// mesmo sinal indireto — se o tempo corre, a fonte esta entregando.
static double creditosNomeado, creditosUltimo;
static pthread_t fioMkv;
static int       fioMkvVivo, mkvPendente;

static VideoLegendaEstilo estilo = { 120, 0, 0, 3, 1, 0, 0, 0 };
static int temEstilo;

// ============================================================================

int video_iniciar(void) {
  if (ligado) return 1;
  temAvplay = (int)AV0("disp");
  ligado = 1;
  if (!temAvplay) {
    // NAO e fatal e nao pode parecer fatal: e exatamente o que acontece no
    // Chrome do desktop, onde o resto do app tem de continuar utilizavel. O
    // aviso existe para que "o video nao toca" na TV nao vire caca ao fantasma.
    printf("[video] webapis.avplay ausente: sem plano de video neste ambiente\n");
    fflush(stdout);
    marco("avplay ausente");
    return 0;
  }
  printf("[video] avplay presente\n"); fflush(stdout);
  return 1;
}

int video_iniciar_auto(void) { return video_iniciar(); }
int video_registro_negado(void) { return 0; }

int video_tocar(const char *url) {
  if (!url || !*url) return 0;
  if (!ligado) video_iniciar();
  if (!temAvplay) return 0;

  // Titulo novo: o marcador do anterior nao vale. Sem isto um filme sem
  // capitulos herdaria os creditos do filme de antes.
  creditosNomeado = creditosUltimo = 0.0;
  snprintf(urlAtual, sizeof urlAtual, "%s", url);
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; faixasLidas = 0;
  posSeg = durSeg = 0; vidW = vidH = 0; houveErro = 0;
  seekEm = 0;
  // Sonda de MKV so faz sentido em MKV. Num MP4 e descida garantidamente
  // perdida pela MESMA conexao que esta transmitindo — o log da LG dizia
  // "nenhuma faixa lida" toda vez.
  mkvPendente = !fonteMp4;

  printf("[video] fonte id=%08x\n", video_id_fonte(url)); fflush(stdout);
  if (AVS("abrir", url) < 1) {
    marco("avplay: open/prepareAsync falhou");
    logarErro("abrir");
    ativo = 0;
    return 0;
  }
  // ativo=1 JA, antes do prepare terminar: e ele que abre o furo em player.c, e
  // abrir o buraco cedo nao custa nada (atras dele so ha o plano de video),
  // enquanto esperar o evento deixaria a interface desenhada por cima do video
  // caso o evento nao venha. Mesma decisao do caminho da LG (video_ativo la usa
  // o mediaId, nao o loadCompleted).
  ativo = 1;
  marco("avplay open");
  // O retangulo corrente vai junto: quem pediu a janela antes de haver sessao
  // (main.c faz exatamente isso) seria ignorado de outra forma.
  avChamar("rect", NULL, janX, janY, janW, janH, NULL, 0);
  return 1;
}

// Definida bem abaixo (API publica): escolherAudioPreferido a usa.
void video_escolher_audio(int i);

// APLICA A PREFERENCIA DE AUDIO. Esta metade tambem nao tinha sido portada: o
// video.c (LG) escolhe a faixa que casa com ling_audio() e este arquivo nunca
// escolheu nada, entao no Tizen o ajuste de idioma de audio nao fazia efeito
// nenhum.
//
// RODA DEPOIS DO IDIOMA CHEGAR, e por isso e chamada de dois lugares: do
// lerFaixas (caso o player ja soubesse o idioma) e do fim de
// aplicarIdiomasDoMkv (o caso real nesta TV, em que o idioma so existe depois
// da sonda). Chamar so no primeiro nunca acertaria aqui.
//
// SEM PREFERENCIA, OU SEM FAIXA QUE CASE, NAO SE MEXE: a escolha do pipeline e
// melhor que uma trocada por chute, e quem quer outra abre a folha de faixas.
// Mesma regra do video.c, palavra por palavra.
static void escolherAudioPreferido(void) {
  const char *pref = ling_audio();
  int i;
  if (!pref[0] || nAudio < 2) return;
  if (audioAtual >= 0 && audioAtual < nAudio &&
      faixaAudio[audioAtual].idioma[0] &&
      ling_casa(faixaAudio[audioAtual].idioma, pref)) return;   // ja esta boa
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

// O IDIOMA DAS FAIXAS VEM DO CONTAINER, porque o AVPlay nao o entrega.
//
// MEDIDO em 17/09 na QN85Q70AAGXZD, em dois filmes diferentes, lendo
// getTotalTrackInfo ao vivo pelo inspector:
//   AUDIO #1 {"language":"","channels":"6","fourCC":"audio/x-eac3"}
//   TEXT  #3 {"track_num":"0","track_lang":"","subtitle_type":"-1"}
// `language` e `track_lang` vem SEMPRE vazios. Por isso a tela mostrava
// "Audio 1", "Audio 2", "Legenda 1"... — nao era parse errado deste lado, e o
// firmware que nao le a etiqueta do MKV.
//
// A sonda de MKV ja rodava aqui e ja lia as faixas para `fx`, mas o porte
// aproveitava SO OS CAPITULOS (creditos) e descartava o vetor. A metade que
// faltava e esta.
//
// CASAMENTO POR ORDINAL DENTRO DO TIPO, e nao por TrackNumber. Na LG o
// sourceInfo entrega o TrackNumber do Matroska e video.c casa por ele, com uma
// nota dizendo que casar por ordem trocaria os idiomas de lugar. Aqui nao ha
// essa chave: o getTotalTrackInfo da o `index` da lista dele (1,2 para audio;
// 3,4,5,6 para legenda) e um `track_num` que e 0,1,2,3 — ordinal entre as
// legendas, nao o TrackNumber do container.
//
// A GUARDA QUE TORNA ISSO ACEITAVEL: so aplica quando a QUANTIDADE de faixas
// daquele tipo bate entre o container e o AVPlay. Se as duas listas tem
// tamanhos diferentes, uma delas filtrou algo e o ordinal ja nao alinha —
// nesse caso nao escreve nada. Idioma errado e pior que idioma nenhum: a
// pessoa escolhe "Portugues" e ouve ingles, e nao tem como saber que foi o app
// que mentiu.
static void aplicarIdiomasDoMkv(const MkvFaixa *fx, int n) {
  int j, kA = 0, kL = 0, nA = 0, nL = 0, casou = 0;
  const int TIPO_AUDIO = 2, TIPO_LEG = 17;   // TrackType do Matroska

  for (j = 0; j < n; j++) {
    if (fx[j].tipo == TIPO_AUDIO) nA++;
    else if (fx[j].tipo == TIPO_LEG) nL++;
  }
  if (nA != nAudio && nL != nLeg) {
    printf("[mkv] contagem nao bate (container %d audio / %d legenda; player "
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
    if (f->idioma[0]) continue;                 // o player ja sabia: nao mexer
    if (!fx[j].idioma[0] || !strcmp(fx[j].idioma, "und")) continue;
    snprintf(f->idioma, sizeof f->idioma, "%s", fx[j].idioma);
    casou++;
    // O rotulo por ULTIMO, depois do idioma, e de uma vez so — a mesma regra do
    // lerFaixas: e ele que a tela desenha e este arquivo nao tem mutex.
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

// DESPEJA O getCurrentStreamInfo NO LOG, uma vez por titulo.
//
// Nao interpreta nada de proposito. O dono relatou em 17/09 que na Samsung o
// HDR nao aparece e que as faixas de audio saem como "Audio 1"/"Audio 2"; as
// duas coisas dependem de campos cujo NOME este lado nunca viu. Escrever o
// parse agora seria adivinhar pela terceira vez no mesmo arquivo — as duas
// primeiras (nao ha recorte de fonte; nao ha informacao de HDR) sairam da
// documentacao e as duas estavam erradas.
//
// Com a linha no log, o proximo passo e escrito a partir do dado.
static void lerInfoFluxo(void) {
  char buf[2048];
  if (avChamar("infofluxo", NULL, 0, 0, 0, 0, buf, (int)sizeof buf) < 1) return;
  buf[sizeof buf - 1] = 0;
  printf("[video] getCurrentStreamInfo = %s\n", buf);
  fflush(stdout);
}

// A razao que o AVPlay deu (S.erro no JS: o objeto do onerror, ou o
// "open:/prepare:/play:" que os catch gravam). 200 bytes cobrem a mensagem do
// AVPlay sem despejar pilha no log.
static void logarErro(const char *onde) {
  char buf[200];
  buf[0] = 0;
  if (avChamar("erro", NULL, 0, 0, 0, 0, buf, (int)sizeof buf) < 1 || !buf[0]) return;
  buf[sizeof buf - 1] = 0;
  printf("[video] avplay erro (%s): %s\n", onde, buf);
  fflush(stdout);
}

// Le do JS as faixas de audio e legenda e monta os rotulos. Chamada uma vez por
// sessao, quando o prepare termina — antes disso getTotalTrackInfo devolve
// vazio.
static void lerFaixas(void) {
  char buf[4096];
  char *linha, *fim;
  int n;
  buf[0] = 0;
  n = (int)avChamar("faixas", NULL, 0, 0, 0, 0, buf, (int)sizeof buf);
  if (n < 1) return;
  faixasLidas = 1;
  nAudio = nLeg = 0;
  for (linha = buf; *linha; linha = fim) {
    char tipo;
    int idx = 0, ordinalMkv = -1;
    char idioma[8] = "", rot[48] = "";
    VideoFaixa *f;
    fim = strchr(linha, '\n');
    if (fim) *fim++ = 0; else fim = linha + strlen(linha);
    if (!*linha) continue;
    tipo = linha[0];
    { // "<tipo>\t<indice>\t<idioma>\t<rotulo>"
      char *p1 = strchr(linha, '\t');
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
    // O rotulo e escrito por ULTIMO, depois do idioma, e de uma vez so: e o
    // campo que a tela desenha, e este arquivo — como o da LG — nao tem mutex
    // nenhum. O dano possivel e um rotulo lido pela metade em UM quadro.
    //
    // i18n() ANTES de montar, e nao depois: "Português  ·  AAC 5.1" (idioma +
    // rot) nunca casa com chave nenhuma da tabela (varredura-i18n.py, porta
    // 2), entao ficava em portugues mesmo com o app em ingles. Mesmo bug do
    // video.c (LG) — este arquivo e o espelho Tizen, ver #42.
    if (idioma[0] && rot[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s  \xc2\xb7  %s", i18n(ling_nome(idioma)), rot);
    else if (idioma[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s", i18n(ling_nome(idioma)));
    else if (rot[0])
      snprintf(f->rotulo, sizeof f->rotulo, "%s", rot);
    else
      snprintf(f->rotulo, sizeof f->rotulo, "%s %d",
               i18n(tipo == 'A' ? "Áudio" : "Legenda"),
               tipo == 'A' ? nAudio : nLeg);
  }
  printf("[video] faixas: %d audio, %d legenda\n", nAudio, nLeg);
  fflush(stdout);
  escolherAudioPreferido();
}

// Le o cabecalho do Matroska pela rede para descobrir onde comecam os creditos.
// O modulo mkv.c usa src/rede.c, que no alvo Tizen ja e XHR — nada aqui e
// especifico do webOS.
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

void video_bombear(void) {
  double est[EST_N];
  int estavaPronto = pronto;

  // Avanco pendente que ja repousou. Fica ANTES da leitura do estado para que a
  // posicao lida ja seja a de depois do seek quando os dois caem no mesmo
  // quadro.
  if (seekEm && SDL_GetTicks() >= seekEm) {
    seekEm = 0;
    AVN("buscar", seekAlvo * 1000.0);
    { char m[48]; snprintf(m, sizeof m, "seek para %ds", (int)seekAlvo); marco(m); }
  }

  if (!temAvplay || !ativo) return;

  memset(est, 0, sizeof est);
  if (avChamar("estado", NULL, 0, 0, 0, 0, (char *)est, (int)sizeof est) < 1) return;
  posSeg   = est[EST_POS];
  durSeg   = est[EST_DUR];
  tocando  = est[EST_TOCANDO] != 0;
  pronto   = est[EST_PRONTO]  != 0;
  if (est[EST_LARG] > 0) vidW = (int)est[EST_LARG];
  if (est[EST_ALT]  > 0) vidH = (int)est[EST_ALT];
  if (est[EST_ERRO] != 0 && !houveErro) {
    houveErro = 1;
    marco("avplay: onerror");
    logarErro("onerror");
  }

  if (pronto && !estavaPronto) {
    marco("avplay prepared");
    lerFaixas();
    lerInfoFluxo();
  }
  if (pronto && !faixasLidas) lerFaixas();

  // SONDA DE MKV. Gatilho diferente do da LG e explicado no bloco de
  // `mkvPendente`: sem bufferRange, o sinal de que ha banda sobrando e o tempo
  // ja ter andado.
  if (mkvPendente && !fioMkvVivo && urlAtual[0] && posSeg >= 5.0)
    video_sondar_mkv_agora();
}

// Ver video.h e a mesma dupla em video.c.
int video_mkv_sondado(void) {
  if (!urlAtual[0] || fonteMp4) return 2;
  return (mkvPendente || fioMkvVivo) ? 0 : 1;
}

void video_sondar_mkv_agora(void) {
  if (!mkvPendente || fioMkvVivo || !urlAtual[0]) return;
  mkvPendente = 0;
  fioMkvVivo = 1;
  if (pthread_create(&fioMkv, NULL, lerMkv, NULL) != 0) fioMkvVivo = 0;
  else pthread_detach(fioMkv);
}

void video_parar(void) {
  seekEm = 0; mkvPendente = 0;
  if (temAvplay) AV0("parar");
  ativo = tocando = pronto = 0;
  posSeg = durSeg = 0;
  nAudio = nLeg = 0; audioAtual = 0; legAtual = -1; faixasLidas = 0;
  urlAtual[0] = 0;
}

// Trailer mudo no fundo nao passa pelo AVPlay neste alvo (trailer.c usa o
// embed do YouTube); o volume fica por conta da TV.
void video_volume(int pct) { (void)pct; }
void video_recorte_reaplicar(void) {}

void video_pausar(int pausado) {
  if (!temAvplay || !ativo) return;
  AVN("pausar", pausado ? 1 : 0);
  tocando = !pausado;
}

void video_buscar(double segundos) {
  if (!temAvplay || !ativo) return;
  if (segundos < 0) segundos = 0;
  posSeg   = segundos;         // a barra responde na hora
  seekAlvo = segundos;
  seekEm   = SDL_GetTicks() + SEEK_REPOUSO_MS;
}

// O retangulo do plano, em 1920x1080. O limite de tela e o mesmo do caminho da
// LG e por precaucao equivalente: um plano de hardware nao recorta o excedente,
// e nao ha razao para descobrir na TV Samsung se o comportamento e "recorta" ou
// "apaga".
// Aplica o retangulo COMO VEIO, sem grampear. E o ponto unico que fala com o
// AVPlay e que lembra o que ja esta valendo — a lembranca importa porque quem
// pede a janela antes de haver sessao (main.c faz isso) precisa que ela seja
// reenviada no open.
static int aplicarRect(int x, int y, int w, int h) {
  if (w < 1 || h < 1) return 0;
  if (x == janX && y == janY && w == janW && h == janH) return 1;
  if (!temAvplay || !ativo) { janX = x; janY = y; janW = w; janH = h; return 1; }
  printf("[video] plano %d,%d %dx%d\n", x, y, w, h); fflush(stdout);
  // O RETORNO IMPORTA, e era jogado fora. A op "rect" do JS devolve 0 quando o
  // setDisplayRect levanta excecao, e este lado ja tinha gravado janX/janY/
  // janW/janH ANTES de chamar. Resultado: o plano continuava onde estava, o C
  // passava a acreditar que estava no lugar novo, e a proxima chamada com o
  // mesmo retangulo saia pelo `return` de igualdade sem tentar de novo. O
  // fracasso era silencioso nos dois sentidos — nem log, nem nova tentativa.
  if (avChamar("rect", NULL, x, y, w, h, NULL, 0) < 1) {
    printf("[video] o setDisplayRect RECUSOU %d,%d %dx%d (a tela e 1920x1080)\n",
           x, y, w, h);
    fflush(stdout);
    return 0;
  }
  janX = x; janY = y; janW = w; janH = h;
  return 1;
}

void video_janela(int x, int y, int w, int h) {
  // GRAMPEIA A TELA, e so este caminho grampeia. Ver video_janela_fonte: o
  // recorte emulado PRECISA de um retangulo que saia da tela, e passar por aqui
  // era o que anulava o zoom inteiro.
  if (w < 1 || h < 1) return;
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > 1920) w = 1920 - x;
  if (y + h > 1080) h = 1080 - y;
  aplicarRect(x, y, w, h);
}

// RECORTE DE FONTE: setVideoRoi (ver a op "roi" no JS).
//
// No webOS o zoom de verdade e o par sourceInput/displayOutput do tv.display
// (via AcbAPI_setCustomDisplayWindow): pedir um pedaco MENOR do quadro
// decodificado para o mesmo destino e o que tira da vista a barra preta
// embutida no arquivo. O AVPlay so tem setDisplayRect, que e o DESTINO, e
// setDisplayMethod, que escolhe entre encaixar e preencher — nenhum dos dois le
// coordenadas do quadro decodificado.
//
// Entao aqui o recorte de fonte e DESCARTADO e vale so o destino. O efeito
// pratico: os modos de zoom continuam mudando o tamanho da area de video na
// tela (o que player.c ja calcula), mas um arquivo 2.39:1 entregue dentro de um
// quadro 16:9 mantem a barra preta embutida. Preferi perder o zoom a fingir que
// ele existe — inflar o destino para alem da tela seria o outro caminho, e e
// justamente o que APAGA o plano na LG.
// RECORTE EMULADO PELO RETANGULO DE DESTINO.
//
// O webOS recorta pela FONTE: o ACB aceita (sx,sy,sw,sh) do quadro decodificado
// mais um destino, e os modos de aspecto do player saem disso. O AVPlay NAO TEM
// retangulo de fonte — so setDisplayRect. A primeira versao disto descartava a
// fonte e aplicava so o destino, e o resultado era que TODO modo de aspecto
// desenhava o mesmo retangulo: na TV o botao de recorte nao mudava nada, em
// nenhum modo. Foi assim que o defeito apareceu.
//
// A conta que substitui: desenhar o recorte (sx,sy,sw,sh) dentro de (dx,dy,dw,dh)
// e o MESMO que desenhar o quadro INTEIRO num retangulo maior, deslocado para
// que o pedaco desejado caia sobre o destino.
//
//   escala = dw/sw            (quanto a fonte e ampliada)
//   W = qw * escala           (o quadro inteiro nessa escala)
//   X = dx - sx * escala      (recua a origem para o recorte cair em dx)
//
// O que sobra para fora da tela e o que o recorte descartaria.
//
// QUEM GRAMPEAVA ERA ESTE CODIGO, E NAO O FIRMWARE. O comentario anterior aqui
// dizia "nao verificado se o firmware aceita retangulo que sai da tela; se ele
// grampear, o zoom continua sem efeito" — e a suspeita apontava para fora. A
// causa estava duas chamadas acima: este calculo produzia o retangulo maior que
// a tela, de proposito, e entregava a video_janela, que GRAMPEIA (x<0 vira 0 e
// encolhe a largura junto). O recorte era desfeito no caminho, sempre, e o
// botao de aspecto nao mudava nada — que e o relato "no Tizen nao funciona o
// zoom da imagem no player".
//
// Agora vai por aplicarRect, que nao grampeia. Continua NAO VERIFICADO se o
// firmware da Samsung honra um retangulo fora da tela ou se ele proprio
// grampeia; a diferenca e que agora, se nao funcionar, a causa esta do lado
// dele — e o log abaixo mostra exatamente o retangulo pedido.
void video_janela_fonte(int sx, int sy, int sw, int sh,
                        int dx, int dy, int dw, int dh) {
  double qw = video_largura(), qh = video_altura();
  double ex, ey;
  int X, Y, W, H;

  // Sem as dimensoes do quadro, ou sem recorte de verdade, o destino cru serve.
  if (qw < 2.0 || qh < 2.0 || sw <= 0 || sh <= 0) { video_janela(dx, dy, dw, dh); return; }
  // Recorte que cobre o quadro inteiro E o caso sem zoom: mesma coisa.
  if (sx <= 0 && sy <= 0 && sw >= (int)qw && sh >= (int)qh) { video_janela(dx, dy, dw, dh); return; }

  ex = (double)dw / (double)sw;
  ey = (double)dh / (double)sh;
  W  = (int)(qw * ex + 0.5);
  H  = (int)(qh * ey + 0.5);
  X  = (int)(dx - sx * ex + 0.5);
  Y  = (int)(dy - sy * ey + 0.5);

  printf("[video] recorte %d,%d %dx%d de %.0fx%.0f -> plano %d,%d %dx%d\n",
         sx, sy, sw, sh, qw, qh, X, Y, W, H);
  fflush(stdout);

  // PRIMEIRO O CAMINHO CERTO: setVideoRoi recorta a FONTE, que e exatamente o
  // que sx/sy/sw/sh descrevem. Com ele o destino continua sendo o retangulo
  // GRAMPEADO, sempre valido, e nao ha aposta nenhuma sobre o que o plano faz
  // com coordenadas fora da tela.
  //
  // A Samsung documenta o ROI junto do PLAYER_DISPLAY_MODE_FULL_SCREEN, que e
  // o modo que este arquivo ja seleciona no open (ver a nota la em cima).
  if (avChamar("roi", NULL, sx, sy, sw, sh, NULL, 0) >= 1) {
    printf("[video] roi %d,%d %dx%d de %.0fx%.0f (recorte na fonte)\n",
           sx, sy, sw, sh, qw, qh);
    fflush(stdout);
    video_janela(dx, dy, dw, dh);
    return;
  }

  // SE O PLANO NAO ACEITAR, VOLTA PARA UM RETANGULO VALIDO.
  //
  // O recorte aqui e emulado: o AVPlay nao recorta a FONTE, entao a unica saida
  // e aumentar o DESTINO para alem da tela e contar que o excedente seja
  // descartado. Isso e uma APOSTA sobre o comportamento do plano, e no webOS a
  // mesma aposta se provou errada (ver player.c: retangulo invalido nao vira
  // recorte, o plano apaga). Este arquivo nunca rodou numa TV Samsung quando
  // foi escrito.
  //
  // O dono reportou em 17/09 que o botao de recorte "nao funciona" nesta TV.
  // Sem este ramo, um setDisplayRect recusado deixava o video parado no
  // retangulo anterior para sempre — que e exatamente "o botao nao faz nada".
  //
  // Agora: tenta o recorte; se o plano recusar, cai no destino GRAMPEADO, que e
  // sempre valido. O zoom nao acontece — o AVPlay nao tem como — mas a imagem
  // continua na tela e o log diz qual dos dois aconteceu.
  if (!aplicarRect(X, Y, W, H)) {
    printf("[video] recorte nao suportado nesta TV: usando o destino inteiro\n");
    fflush(stdout);
    video_janela(dx, dy, dw, dh);
  }
}

const char *video_url_atual(void) { return urlAtual; }
double video_pos(void)        { return posSeg; }
double video_duracao(void)    { return durSeg; }
// O AVPlay so informa PORCENTAGEM de buffering (onbufferingprogress), nunca um
// instante. 0 e o que video.h define como "desconhecido"; quem chama ja trata.
double video_buffer_fim(void) { return 0; }
// O AVPlay nao expoe o par bufferingStart/End; sem sinal, nunca afirma travo.
unsigned video_bufferando_ms(void) { return 0; }
int    video_tocando(void)    { return tocando; }
int    video_pronto(void)     { return pronto; }
int    video_ativo(void)      { return ativo; }
// 1 depois de um onerror do AVPlay na fonte atual. E o mesmo contrato do
// video.c (webOS): app.c usa isto no watchdog de canal para pular a fonte
// morta sem esperar o prazo. Sem esta definicao o alvo Tizen nem linkava.
int    video_falhou(void)     { return houveErro; }
// O AVPlay nao separa "audio nao suportado" de erro geral; sem sinal proprio.
int    video_audio_nao_suportado(void) { return 0; }
// O trailer deste alvo nao passa pelo AVPlay (ver trailer.c); nao ha fim a
// contar aqui.
int    video_terminou(void)   { return 0; }

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
int  video_audio_atual(void)   { return audioAtual; }
int  video_legenda_atual(void) { return legAtual; }

// SEM EQUIVALENTE, e por isso devolvem "nao sei" em vez de um palpite.
//
// No webOS estes tres saem do videoInfo/sourceInfo do uMS, que descreve a
// camada que CHEGOU ao decoder. O AVPlay nao publica hdrType nem a marca
// "immersive"/ATMOS de faixa de audio — getTotalTrackInfo traz codec e idioma,
// nada sobre HDR ou objeto de audio.
//
// Com "desconhecido" e 0, player.c (linhas 1480-1482) nao desenha selo nenhum:
// nem "Dolby Vision", nem "HDR10", nem "Dolby Atmos". E o resultado correto. O
// caminho da LG ja aprendeu isso do jeito caro — um arquivo anunciado como DV
// voltava HDR10 no videoInfo, e o selo tirado da AFIRMACAO DA FONTE mentia. Ler
// a afirmacao do addon aqui (dvPedido esta guardado logo abaixo) repetiria
// exatamente esse erro, agora sem nem ter o pipeline para desmentir.
int  video_tem_atmos(void)        { return 0; }
int  video_tem_dolby_vision(void) { return 0; }
const char *video_hdr(void)       { return "desconhecido"; }

int  video_largura(void)          { return vidW; }
int  video_altura(void)           { return vidH; }

// SEM EQUIVALENTE NESTE ALVO, e por isso responde 0 em vez de fingir.
//
// No webOS o app NEGOCIA o hdrType com o ACB e pode renegociar sem HDR; aqui
// quem decide o pipeline de HDR e o AVPlay do firmware, a partir do proprio
// fluxo — nao ha campo que este lado possa contradizer. A interface le
// video_pode_forcar_sdr justamente para nao oferecer um botao inerte.
// Ver a nota em video.h: as tres APIs foram testadas nesta TV e nenhuma
// recorta a fonte de video comum.
int  video_recorte_fonte(void) { return 0; }

int  video_pode_forcar_sdr(void) { return 0; }
void video_forcar_sdr(void) { }

// Guardado e NAO USADO no AVPlay: nao ha como pedir uma camada de Dolby Vision
// ao player. Fica registrado para nao parecer esquecimento, e para o dia em que
// a Samsung expuser algo equivalente.
// CABECALHOS DO ADDON NO ALVO TIZEN: AINDA NAO IMPLEMENTADO, DE PROPOSITO.
// Ver a nota longa no fim deste arquivo.
void video_definir_cabecalhos(const char *cabs) { (void)cabs; }

void video_definir_dv(int dv) { dvPedido = dv ? 1 : 0; (void)dvPedido; }
void video_definir_mp4(int ehMp4) { fonteMp4 = ehMp4; }

void video_escolher_audio(int i) {
  const VideoFaixa *f = video_audio(i);
  if (!temAvplay || !ativo || !f) return;
  // setSelectTrack quer o tipo em MAIUSCULA e o indice ABSOLUTO devolvido pelo
  // getTotalTrackInfo — nao a posicao na nossa lista filtrada. Por isso f->numero.
  avChamar("faixa", "AUDIO", f->numero, 0, 0, 0, NULL, 0);
  audioAtual = i;
}

void video_escolher_legenda(int i) {
  if (!temAvplay || !ativo) return;
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
  if (!temAvplay || !ativo || legAtual < 0) return 0;
  n = (int)avChamar("leg_texto", NULL, 0, 0, 0, 0, dst, tam) - 1;
  // Uma linha por processo quando o primeiro texto chega: e a prova, nos
  // registros, de que o firmware entrega a legenda embutida por callback.
  if (n > 0 && !avisou) {
    avisou = 1;
    printf("[video] legenda embutida: onsubtitlechange entregando texto (%d evento(s))\n", n);
    fflush(stdout);
  }
  return dst[0] != 0;
}

// SEM EQUIVALENTE UTIL: setExternalSubtitlePath so aceita CAMINHO LOCAL.
//
// O uMS da LG baixa a URL sozinho — o app so aponta o setSubtitleSource. O
// AVPlay nao: setExternalSubtitlePath quer um arquivo no sistema de arquivos do
// widget, e a legenda do OpenSubtitles chega pela rede (src/legenda.c). Gravar
// no MEMFS do Emscripten nao resolve, porque esse sistema de arquivos e uma
// invencao do WASM que o firmware nao enxerga; gravaria num caminho que o
// AVPlay nao consegue abrir, e o sintoma seria o silencio de sempre.
//
// Entao esta funcao FALHA de proposito, com log. O plano B ja existe e ja e
// usado: o app sabe desenhar legenda em GL por conta propria (src/legenda.c faz
// o download e o desenho, e VideoLegendaEstilo.familia foi criado justamente
// para esse overlay). Quem chama nao perde nada alem da sincronizacao feita
// pelo pipeline.
//
// Um caminho local so passaria a existir se o .wgt gravasse o .srt via
// tizen.filesystem numa pasta do app e passasse esse caminho aqui — e um
// pedido ao lado JS/config.xml, nao a este arquivo, e nao foi feito.
void video_legenda_externa(const char *url) {
  if (!url || !*url) return;
  printf("[video] legenda externa nao vai pelo AVPlay (so aceita caminho local); "
         "o overlay GL continua sendo o caminho; fonte id=%08x\n",
         video_id_fonte(url));
  fflush(stdout);
  marco("avplay: legenda externa recusada (URL de rede)");
}

// GUARDADO, nao aplicado ao player.
//
// Os cinco metodos de estilo do uMS (setSubtitleFontSize, charColor, bgOpacity,
// position, charEdgeType) nao tem par no AVPlay: a API de legenda dele e um
// caminho de arquivo e o evento onsubtitlechange. Esta copia ainda nao tem
// consumidor: player.c usa seu proprio legEstilo para o overlay EXTERNO.
// Legendas embutidas seguem o desenho nativo solicitado por leg_mudo=0.
void video_legenda_estilo(const VideoLegendaEstilo *e) {
  if (!e) return;
  estilo = *e;
  temEstilo = 1;
  (void)estilo; (void)temEstilo;
}

void video_encerrar(void) {
  if (!ligado) return;
  video_parar();
  if (temAvplay) AV0("encerrar");
  ligado = 0;
}

#endif  /* __EMSCRIPTEN__ */
