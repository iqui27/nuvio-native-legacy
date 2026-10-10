// Bootstrap: janela, contexto GL, loop e telemetria. Toda a UI vive nos modulos.
#include "app_id.h"
#include "arranque.h"
#include "streams.h"
#ifdef NV_DTS_DEBUG
#include "dts/dts_engine.h"
#include "dts/dts_pipeline.h"
#endif
#include <SDL2/SDL.h>
#include "tpkteclas.h"
#include "central.h"
#include "sdlcompat.h"
#include <SDL2/SDL_image.h>
#include "gl_compat.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/heap.h>
#include <malloc.h>
#endif
// Alvos SEM webOS. O Mac ja pulava estes trechos por um ifndef __APPLE__;
// o alvo Tizen (WASM) precisa pular exatamente os mesmos. Nomear a condicao
// evita ter de lembrar de dois simbolos em cada ponto - sem isto o primeiro
// build para o navegador ainda tentava abrir libwayland-client.so.0.
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP) || defined(__EMSCRIPTEN__) || defined(NV_TPK) || defined(NV_ANDROID)
#define NV_SEM_WEBOS 1
#endif
#ifdef NV_ANDROID
#include "android.h"
#endif
#include <stdio.h>
#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
// SIGTERM tratado pelo app: webOS e desktop (Mac/Linux). No .tpk o host .NET e
// dono do sinal, no Android o sistema mata sem SIGTERM util e no Tizen/WASM nao
// ha sinal. Os desktops entram para o handler ser exercitavel por teste no Mac.
#if !defined(__EMSCRIPTEN__) && !defined(NV_TPK) && !defined(NV_ANDROID)
#define NV_SINAL_TERMINAR 1
#include <signal.h>
#endif
#include <unistd.h>   // dup2 (o stderr no mesmo descritor do log)
#include "gfx.h"
#include "fundo.h"
#include "gpunivel.h"
#include "gputempo.h"
#include "text.h"
#include "marco.h"
#include "memlog.h"
#include "rede.h"
#include "tex_cache.h"
#include "webp.h"
#include "cachearte.h"
#include "artehero.h"
#include "arteescolha.h"
#include "artereserva.h"
#include "trailerapple.h"
#include "home.h"
#include "homeestado.h"
#include "text.h"
#include "detail.h"
#include "dados.h"
#include "negcache.h"
#include "p2pmotor.h"
#include <pthread.h>
#include <stdatomic.h>
#include <time.h>
#include "nuvem.h"
#ifdef NV_ANDROID
#include <sys/system_properties.h>
#endif
#include "sessao.h"
#include "perfis.h"
#include "sync.h"
#include "traktauth.h"
#include "simklauth.h"
#include "app.h"
#include "registro.h"
#include "desempenho.h"
#include "atualizacao.h"
#include "avisos.h"
#include "seguro.h"
#include "video.h"
#include "addons.h"
#include "ajustes.h"
#include "perfiltv.h"
#include "corviva.h"
#include "catalogo.h"
#include "iconeapp.h"
#include "descoberta.h"
#include "trakt.h"
#include "player.h"
#include "ilha.h"
#include "resolucao.h"
#include "trailer.h"
#include "ponteiro.h"
#include "entrada_texto.h"
#include "gif.h"
#include "idioma.h"
#include "idiomaauto.h"
#include "abertura.h"
#include "logoapp.h"
// O idioma AUTOMATICO da interface mudou depois do arranque (a conta chegou, ou
// a TV respondeu o locale). Titulos e generos das fileiras saem no idioma novo,
// e a pessoa fica sabendo por que a tela trocou sozinha — uma vez por idioma
// (o aviso tem um id por codigo e avisos-vistos.txt lembra). O texto ja sai no
// idioma novo: i18n le ajustes_idioma().
static void aoMudarIdiomaAuto(const char *codigo, int fonte, int notificar) {
  desc_repetir();
  if (!notificar) return;
  avisos_idioma_definido(codigo,
      fonte == IDA_SISTEMA
        ? i18n("Idioma definido pelo sistema da TV · mudar em Ajustes")
        : i18n("Idioma definido pela sua conta · mudar em Ajustes"));
}

#ifndef NV_SEM_WEBOS
#include <dlfcn.h>
#include <SDL2/SDL_syswm.h>
#endif

#ifdef NV_SINAL_TERMINAR
// SIGTERM (deploy, `kill`, o SAM fechando o app) PRECISA PASSAR PELA SAIDA
// NORMAL. O handler antigo fechava video e log e dava _exit(0) direto: nunca
// chegava a avisos_encerrar (apaga a marca de sessao viva) nem a
// seguro_encerrar (fecha a sessao no diario), e a sessao seguinte logava "nao
// se despediu"/"anterior caiu" e contava queda rapida no modo seguro — a cada
// deploy. Esses dois gravam arquivo, formatam texto e alocam: nada disso e
// seguro dentro de um handler. Entao ele so ergue uma flag (sig_atomic_t) e o
// laco principal, que ja checa app_quer_sair, sai e roda o encerramento de
// sempre. O alarm e a rede de seguranca: se o laco estiver travado e nao
// reagir em 4 s, o SIGALRM encerra o processo. So o que e async-signal-safe: o
// handler antigo chamava trailer_fechar/video_encerrar/ajustes_log_vazar_tudo
// (travas, free, join) e, se o laco estava preso DENTRO de uma delas, ou a
// saida normal ja corria em outro ponto, reentrava na mesma trava e nunca
// chegava ao _exit (so o SIGKILL tirava). Quando o laco sai de verdade o alarm
// e rearmado com folga para a saida normal (join do fioFonte, p2pmotor_saida) e
// cancelado no fim dela: antes o alarm(4) cortava a saida lenta pela metade.
#define NV_SAIDA_FOLGA_S 25
static volatile sig_atomic_t sinalTerminou = 0;
static void aoAlarmeTerminar(int sig) {
  static const char msg[] = "[main] SIGALRM: saida travada, _exit\n";
  (void)sig;
  (void)!write(2, msg, sizeof msg - 1);
  _exit(0);
}
// So o PRIMEIRO sinal arma o alarm(4). Um 2o SIGTERM (deploy repetindo o kill,
// SAM insistindo) chegava durante a saida normal, ja com alarm(NV_SAIDA_FOLGA_S),
// e o rearmava em 4 s: cortava p2pmotor_saida/ajustes_log_vazar_tudo/corviva.
static void aoSinalTerminar(int sig) {
  (void)sig;
  if (sinalTerminou) return;
  sinalTerminou = 1;
  alarm(4);
}
#endif
#include "layout.h"
#include "plugins.h"
#include "plex.h"
#include "esmaecer.h"
#include "descanso.h"

// RSS DO PROCESSO, em MB, lido de /proc/self/statm. E o numero que responde
// "da para subir o orcamento de texturas?" — o teto de 96 MB foi escolhido
// com a memoria de 2019 na cabeca e nunca foi conferido contra o que o
// processo ocupa de verdade na TV (a LG tem 2,2 GB e 860 MB livres). Zero
// onde nao ha /proc (Emscripten tem o [mem] proprio).
static double rssMB(void) {
#ifdef __EMSCRIPTEN__
  return 0.0;
#else
  FILE *f = fopen("/proc/self/statm", "r");
  long paginas = 0, res = 0;
  if (!f) return 0.0;
  if (fscanf(f, "%ld %ld", &paginas, &res) != 2) res = 0;
  fclose(f);
  return (double)res * 4096.0 / 1048576.0;
#endif
}

// Captura de tela sob demanda. O framebuffer da TV nao pode ser lido nem como
// root ("Operation not permitted") e o servico de captura da LG responde erro,
// entao a unica forma de ver o que o app desenha e o proprio app se fotografar.
// Sem isso, cada ajuste visual depende de alguem apontar um celular para a TV.
//
// Protocolo: alguem cria /tmp/nuvio-shot-req; no proximo quadro o app grava
// /tmp/nuvio-shot.png e apaga o pedido.

// CH+/CH- (F7/F8) e o que eles viram fora do zap. Funcao, e nao trecho do laco,
// porque a tecla injetada da Samsung no simulador (/tmp/nuvio-key "XF86...")
// passa por aqui tambem SEM a fila do SDL: o SDL do Mac (sdl2-compat 2.32,
// SDL3 por baixo) zera no SDL_PushEvent o scancode 489 da AZUL (medido: 489
// entra, 0 sai). No .tpk o SDL e o 2.30.9 de verdade e nao mexe nele.
static void remapCanal(SDL_Event *e) {
#if defined(NV_ANDROID) || defined(NV_TPK) || defined(__APPLE__)
  // (Samsung .tpk: tpkteclas.c entrega XF86RaiseChannel/LowerChannel como F7/F8.
  // No Mac vale tambem, com o comportamento da Samsung: e o simulador dela.)
  // CH+/CH- NO ANDROID. O NuvioActivity entrega CH+ como F7 e CH- como F8.
  // Com canal na tela (guia, canal ao vivo, canal no canto) sao CH+/CH- de
  // verdade, com os scancodes do webOS que guia.c, player.c e app.c ja
  // tratam. Fora disso fazem o papel das teclas que o controle Android nao
  // tem: CH+ = AZUL (Salvos), CH- = Spotlight (F5, SPOT_TECLA_ABRIR). O
  // registro foi para a tecla Info (NuvioActivity: KEYCODE_INFO -> F9).
  if ((e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) &&
      (e->key.keysym.sym == SDLK_F7 || e->key.keysym.sym == SDLK_F8)) {
    int sobe = e->key.keysym.sym == SDLK_F7;
    if (app_zap_ativo()) {
      e->key.keysym.scancode = (SDL_Scancode)(sobe ? NV_SCANCODE_CH_UP : NV_SCANCODE_CH_DOWN);
      e->key.keysym.sym = sobe ? SDLK_PAGEUP : SDLK_PAGEDOWN;
    } else {
#if defined(NV_TPK) || defined(__APPLE__)
      // Na Samsung o CH+ vai com o scancode da AZUL de verdade: como a
      // letra "s" ele era recusado com campo de texto ativo e o Spotlight
      // o escrevia (relato de 05/10/2026).
      e->key.keysym.scancode = sobe ? (SDL_Scancode)NV_SCANCODE_BLUE : SDL_SCANCODE_F5;
#else
      e->key.keysym.scancode = sobe ? SDL_SCANCODE_S : SDL_SCANCODE_F5;
#endif
      e->key.keysym.sym = sobe ? SDLK_s : SDLK_F5;
    }
  }
#elif defined(__EMSCRIPTEN__)
  // .wgt: tizen-shell.html entrega o CH+ (427) como F7, com keydown e keyup,
  // para a Central de controle saber quanto ele ficou embaixo. Fora disso ele
  // e o "s" de sempre (Salvos).
  if ((e->type == SDL_KEYDOWN || e->type == SDL_KEYUP) && e->key.keysym.sym == SDLK_F7) {
    e->key.keysym.sym = SDLK_s;
    e->key.keysym.scancode = SDL_SCANCODE_S;
  }
#else
  (void)e;
#endif
}

// O CH+ CRU, antes de remapCanal: F7 onde o host o entrega assim (Android,
// .tpk, .wgt, Mac) e o scancode do LG. Segurado abre a Central de controle
// (central.h); o toque curto volta por entregarCh.
static int teclaCh(const SDL_Event *e) {
  if (e->type != SDL_KEYDOWN && e->type != SDL_KEYUP) return 0;
#if defined(NV_ANDROID) || defined(NV_TPK) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
  if (e->key.keysym.sym == SDLK_F7) return 1;
#endif
  return e->key.keysym.scancode == (SDL_Scancode)NV_SCANCODE_CH_UP;
}
static void entregarCh(SDL_Event *e) {
  remapCanal(e);
  app_evento(e);
}

// Teclas injetadas por arquivo, para conferir a UI sem alguem no sofa com o
// controle: escreva "down", "ok", "back"... em /tmp/nuvio-key e o app processa
// como se viesse do D-pad. Uma tecla por linha, o arquivo e consumido.
static SDL_Keycode codigoDaTecla(const char *nome) {
  if (!strcmp(nome, "up"))    return SDLK_UP;
  if (!strcmp(nome, "down"))  return SDLK_DOWN;
  if (!strcmp(nome, "left"))  return SDLK_LEFT;
  if (!strcmp(nome, "right")) return SDLK_RIGHT;
  if (!strcmp(nome, "ok"))    return SDLK_RETURN;
  if (!strcmp(nome, "back"))  return SDLK_AC_BACK;
  // "log" abre/fecha o painel de registro na tela. Sem isto, exercitar o painel
  // exigia a tecla vermelha de um controle de TV — e no Mac ela nao existe.
  if (!strcmp(nome, "log"))   return SDLK_F9;
  // "azul" abre o painel de Salvos, pelo mesmo motivo do "log" logo acima: a
  // tecla de verdade e a AZUL do controle da TV (NV_SCANCODE_BLUE, 489), e ela
  // nao existe em teclado nenhum. app.c ja aceita o S como equivalente dela; e
  // esse S que sai daqui, entao o caminho exercitado e o mesmo.
  if (!strcmp(nome, "azul"))  return SDLK_s;
  // "zero" cicla o modo de aspecto no player (tecla 0 do controle).
  if (!strcmp(nome, "zero"))  return SDLK_0;
  // "guia" abre o Guia de TV de qualquer lugar (F10, roteado em app.c). Ver la
  // por que uma porta direta vale mais que navegar ate ele por setas.
  if (!strcmp(nome, "guia"))  return SDLK_F10;
  // "verde" abre o diagnostico da Live TV no guia: o `d` e o equivalente de
  // teclado do VERDE (G_SCANCODE_GREEN em guia.c).
  if (!strcmp(nome, "verde")) return SDLK_d;
  // "spotlight" abre a caixa de busca por cima da tela (a AMARELA), "voz" e o
  // botao de microfone (F6: abre e, onde ha ditado, ja comeca). spotlight.h.
  if (!strcmp(nome, "spotlight")) return SDLK_F5;
  if (!strcmp(nome, "voz"))   return SDLK_F6;
  return 0;
}

// O arquivo e CONSUMIDO truncando, nunca apagando: /tmp tem sticky bit e os
// arquivos sao criados por root, entao o app (uid 5410) nao consegue remove-los.
// Enquanto isso nao foi visto, cada pedido era reprocessado a cada quadro —
// uma unica tecla "down" virava centenas e o foco corria ate o fim da pagina.
static void consome(const char *caminho) {
  FILE *f = fopen(caminho, "w");
  if (f) fclose(f);
}

// O pedido e NOVO? Guarda contra o arquivo que nao da para consumir.
//
// `consome` esvazia abrindo com "w" em vez de apagar, justamente por causa do
// sticky bit do /tmp. Mas isso tambem falha quando o arquivo pertence a OUTRO
// usuario: um pedido criado por ssh como root fica 644, e o app (uid 5152) nao
// pode nem apagar nem truncar. O pedido entao vale para sempre.
//
// MEDIDO na TV do dono, e fui eu que causei: um /tmp/nuvio-shot-req esquecido
// como root fez o app capturar a tela inteira (glReadPixels de 1920x1080 mais
// 8 MB gravados) EM TODO QUADRO por horas — `aux` foi de 0,0 para 100,7 ms e o
// app caiu de 60 para 9 fps. O sintoma que chegou foi "a interface ta lerda".
//
// Comparar a data de modificacao resolve sem depender de escrita: um pedido que
// nao mudou desde o ultimo atendimento nao e um pedido novo.
// A data so e consultada quando o consumo FALHA, e nao sempre: ela tem
// resolucao de um segundo, e duas rajadas de tecla no mesmo segundo seriam
// tratadas como a mesma. No caminho normal (arquivo do proprio app) o consumo
// funciona e nada disto entra em jogo.
static int pedidoNovo(const char *caminho, time_t *bloqueado) {
  struct stat st;
#ifdef __EMSCRIPTEN__
  // NAO HA /tmp/nuvio-* NO TIZEN — ninguem injeta tecla nem pede captura por
  // arquivo la — e cada stat que falha custa um FS.ErrnoError do Emscripten
  // (Error com pilha capturada + varredura de ERRNO_CODES), no fio principal,
  // tres vezes por quadro. MEDIDO no Chrome com a build do Tizen: 8,3% do
  // fio principal em ErrnoError, mais que todo o WASM junto (20/09, #72).
  (void)caminho; (void)bloqueado; (void)st;
  return 0;
#else
  if (stat(caminho, &st) != 0 || st.st_size <= 0) return 0;
  // Pedido que ja foi atendido e nao pode ser esvaziado: ignora enquanto nao
  // mudar. Sem isto ele vale para sempre e o trabalho e refeito por quadro.
  if (*bloqueado && st.st_mtime == *bloqueado) return 0;
  *bloqueado = 0;
  return 1;
#endif
}

// Esvazia e confere. Devolve 0 quando NAO conseguiu — dono diferente, sticky
// bit — e nesse caso marca o pedido para ser ignorado ate a data mudar.
static int consomeOuBloqueia(const char *caminho, time_t *bloqueado) {
  struct stat st;
  consome(caminho);
  if (stat(caminho, &st) == 0 && st.st_size > 0) {
    *bloqueado = st.st_mtime;
    printf("[main] %s nao pode ser consumido (dono diferente); ignorando\n",
           caminho);
    fflush(stdout);
    return 0;
  }
  return 1;
}
// stat() e nao fopen+fseek: esta sondagem roda para TRES arquivos em TODO
// quadro, e cada fopen paga alocacao de FILE e dois syscalls a mais so para
// descobrir o tamanho. O stat responde a mesma pergunta com um syscall.
static long tamanhoDe(const char *caminho) {
  struct stat st;
  if (stat(caminho, &st) != 0) return -1;
  return (long)st.st_size;
}

// KEYUP adiado de uma tecla segurada.
static Uint32 soltarEm = 0;
static SDL_Keycode soltarTecla = 0;

// BUTTONUP adiado de um "clicar:x,y:hold".
static Uint32 soltarMouseEm = 0;
static SDL_Event soltarMouse;

static void teclasInjetadas(void (*entregar)(const SDL_Event *)) {
  if (soltarMouseEm && SDL_GetTicks() >= soltarMouseEm) {
    soltarMouseEm = 0;
    ponteiro_evento(&soltarMouse, entregar);
  }
  if (soltarEm && SDL_GetTicks() >= soltarEm) {
    SDL_Event up; SDL_zero(up);
    up.type = SDL_KEYUP; up.key.keysym.sym = soltarTecla;
    entregar(&up);
    soltarEm = 0;
  }
  static time_t bloqueadoKey;
  if (!pedidoNovo("/tmp/nuvio-key", &bloqueadoKey)) return;
  FILE *f = fopen("/tmp/nuvio-key", "r");
  if (!f) return;
  // A tecla injetada tambem conta como gente no controle: sem isto a tela de
  // descanso entrava 2 min depois da ultima tecla DE VERDADE, no meio de uma
  // medida feita so por este arquivo. Nao engole a tecla (consumivel = 0).
  esmaecer_entrada(SDL_GetTicks(), 0);
  char linha[32];
  while (fgets(linha, sizeof linha, f)) {
    char *fim = linha + strlen(linha);
    while (fim > linha && (fim[-1] == '\n' || fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0;
    // "ok:hold" simula a pressao longa: o KEYUP dela fica agendado para depois
    // do limiar, em vez de vir junto. Sem isso nao da para exercitar por aqui
    // nada que dependa de segurar o botao.
    int segurar = 0;
    char *dp = strchr(linha, ':');
    if (dp && !strcmp(dp + 1, "hold")) { *dp = 0; segurar = 1; }
    // "abrir:tt0121955" abre o titulo direto (app.c). Porta de teste, como
    // "guia".
    if (dp && !strncmp(linha, "abrir:", 6)) { app_abrir_titulo(dp + 1); continue; }
    // "ime:abrir", "ime:voz", "ime:fechar": teclado do sistema direto
    // (entrada_texto.h), para medir na TV sem depender da tela que o liga.
    if (dp && !strncmp(linha, "ime:", 4)) {
      if (!strcmp(dp + 1, "fechar")) texto_sistema_fechar();
      else texto_sistema_abrir("", !strcmp(dp + 1, "voz"));
      printf("[texto] porta de teste: %s (disponivel=%d)\n", dp + 1, texto_sistema_disponivel());
      fflush(stdout);
      continue;
    }
    // "texto:matrix" digita letra por letra (a-z, 0-9; "_" e espaco), como o
    // teclado fisico: e o que o Spotlight e a Busca aceitam fora da grade.
    if (dp && !strncmp(linha, "texto:", 6)) {
      const char *c;
      for (c = dp + 1; *c; c++) {
        SDL_Event t; SDL_zero(t);
        if (!((*c >= 'a' && *c <= 'z') || (*c >= '0' && *c <= '9') || *c == '_')) continue;
        t.type = SDL_KEYDOWN; t.key.keysym.sym = *c == '_' ? SDLK_SPACE : (SDL_Keycode)*c;
        entregar(&t);
        t.type = SDL_KEYUP; entregar(&t);
      }
      continue;
    }
    // "mover:960,540" e "clicar:960,540" (e "clicar:960,540:hold") fazem o
    // papel do Magic Remote (issue #99), em coordenadas da janela — que na TV
    // sao as do layout. Passam por ponteiro_evento como um evento de mouse de
    // verdade, entao o hit-test e o mesmo do controle.
    if (dp && (!strncmp(linha, "mover:", 6) || !strncmp(linha, "clicar:", 7))) {
      int mx = 0, my = 0, clicar = linha[0] == 'c';
      if (sscanf(dp + 1, "%d,%d", &mx, &my) == 2) {
        SDL_Event m; SDL_zero(m);
        m.type = SDL_MOUSEMOTION; m.motion.x = mx; m.motion.y = my;
        ponteiro_evento(&m, entregar);
        if (clicar) {
          SDL_zero(m);
          m.type = SDL_MOUSEBUTTONDOWN; m.button.button = SDL_BUTTON_LEFT;
          m.button.x = mx; m.button.y = my;
          ponteiro_evento(&m, entregar);
          m.type = SDL_MOUSEBUTTONUP;
          if (strstr(dp + 1, ":hold")) { soltarMouseEm = SDL_GetTicks() + NV_HOLD_MS + 120; soltarMouse = m; }
          else ponteiro_evento(&m, entregar);
        }
      }
      continue;
    }

    // "tocar:960,540" e "arrastar:x0,y0,x1,y1" fazem o papel do DEDO (#216),
    // em coordenadas do layout: SDL_FINGER* normalizados, pelo mesmo
    // ponteiro_evento do toque de verdade.
    if (dp && (!strncmp(linha, "tocar:", 6) || !strncmp(linha, "arrastar:", 9))) {
      float x0 = 0, y0 = 0, x1, y1;
      int n = sscanf(dp + 1, "%f,%f,%f,%f", &x0, &y0, &x1, &y1), passos, i;
      if (n < 2) continue;
      if (n < 4) { x1 = x0; y1 = y0; }
      passos = n < 4 ? 0 : 12;
      { SDL_Event t; SDL_zero(t);
        t.tfinger.touchId = 1; t.tfinger.fingerId = 1;
        t.type = SDL_FINGERDOWN; t.tfinger.x = x0 / NV_TELA_W; t.tfinger.y = y0 / NV_TELA_H;
        ponteiro_evento(&t, entregar);
        for (i = 1; i <= passos; i++) {
          t.type = SDL_FINGERMOTION;
          t.tfinger.x = (x0 + (x1 - x0) * i / passos) / NV_TELA_W;
          t.tfinger.y = (y0 + (y1 - y0) * i / passos) / NV_TELA_H;
          ponteiro_evento(&t, entregar);
        }
        t.type = SDL_FINGERUP; t.tfinger.x = x1 / NV_TELA_W; t.tfinger.y = y1 / NV_TELA_H;
        ponteiro_evento(&t, entregar); }
      continue;
    }

    // NOME DE TECLA DA SAMSUNG ("XF86Blue", "XF86RaiseChannel", "XF86ChannelGuide"...):
    // a mesma tabela do .tpk (tpkteclas.c) e o mesmo remapeamento de CH+/CH-
    // do laco principal (remapCanal). E o simulador da Samsung no Mac.
    if (!strncmp(linha, "XF86", 4)) {
      SDL_Event t;
      if (tpkteclas_evento(linha, 1, &t)) {
        remapCanal(&t);
        printf("[tecla] injetada %s -> sym=%d scancode=%d\n", linha,
               (int)t.key.keysym.sym, (int)t.key.keysym.scancode);
        fflush(stdout);
        entregar(&t);
      }
      if (tpkteclas_evento(linha, 0, &t)) { remapCanal(&t); entregar(&t); }
      continue;
    }
    SDL_Keycode k = codigoDaTecla(linha);
    if (!k) continue;
    SDL_Event e; SDL_zero(e);
    e.type = SDL_KEYDOWN; e.key.keysym.sym = k;
    entregar(&e);

    // O par KEYUP existe porque parte da interface so decide quando a tecla
    // SOBE — o toque curto contra a pressao longa do OK, por exemplo. Mandar
    // so o KEYDOWN deixava essas acoes mudas.
    if (segurar) { soltarEm = SDL_GetTicks() + NV_HOLD_MS + 120; soltarTecla = k; }
    else { e.type = SDL_KEYUP; entregar(&e); }
  }
  fclose(f);
  consomeOuBloqueia("/tmp/nuvio-key", &bloqueadoKey);
}

// Tamanho do buffer de onde a captura le. Definido no arranque, junto com o
// viewport.
static int capW = (int)NV_TELA_W, capH = (int)NV_TELA_H;

// PORTA DE TESTE DO MOTOR P2P: "p2p:<infoHash>" no pedido de video resolve o
// torrent pelo motor embutido (num fio: metadados, pares, primeiros bytes) e
// toca a URL local, cronometrando do pedido ate a URL. "-" para o video e
// solta o motor (app.c o para como se o player tivesse fechado). So torrent
// legal: Big Buck Bunny (dd8255ecdc7ca55fb0bbf81323d87062db1f6d1c), Sintel.
static char p2pTesteUrl[600];
static _Atomic int p2pTesteEstado;   // 0 nada, 1 resolvendo, 2 pronto (url ou erro)
static char p2pTesteHash[48];
static struct timespec p2pTesteT0;
static void *p2pTesteFio(void *x) {
  int e;
  (void)x;
  e = p2pmotor_resolver(p2pTesteHash, -1, "", 0, 0, p2pTesteUrl, sizeof p2pTesteUrl);
  if (e) printf("[p2p-teste] erro %d\n", e);
  atomic_store(&p2pTesteEstado, 2);
  return NULL;
}
static void p2pTesteBombear(void) {
  struct timespec a;
  if (atomic_load(&p2pTesteEstado) != 2) return;
  atomic_store(&p2pTesteEstado, 0);
  clock_gettime(CLOCK_MONOTONIC, &a);
  printf("[p2p-teste] url em %.1f s\n", (double)(a.tv_sec - p2pTesteT0.tv_sec) +
         (double)(a.tv_nsec - p2pTesteT0.tv_nsec) / 1e9);
  fflush(stdout);
  if (p2pTesteUrl[0]) { video_tocar(p2pTesteUrl); video_janela(0, 0, 1920, 1080); }
  else p2pmotor_segurar(0);
}
// No Android nao ha /tmp: o pedido mora na pasta de dados (adb run-as).
static const char *pedidoVideo(void) {
#ifdef NV_ANDROID
  static char p[600];
  if (!p[0]) snprintf(p, sizeof p, "%s/nuvio-video", dados_dir());
  return p;
#else
  return "/tmp/nuvio-video";
#endif
}

// Mesmo protocolo das outras ferramentas: escreva uma URL em /tmp/nuvio-video e
// o app toca. E o unico jeito de testar reproducao sem alguem no sofa — e o
// video nao pode ser conferido por captura, porque vive em outro plano.
//
// A SONDAGEM SAI DO FIO PRINCIPAL (05/10/2026). No Android o pedido mora em
// /data (775f9db9), e o stat() dele rodava em TODO quadro dentro do `aux` — que
// e so isto e a captura. A TCL do dono, ao abrir um 4K Dolby Vision, mostrou
// `aux=21569.1` e `aux=147.2` no [quadro]: o unico syscall que toca disco nesse
// trecho e esse stat. Acho (sem prova ainda) que o /data parou sob escrita
// pesada (cache de seek do F07 gravando o 4K, ou o p2p) e o lookup esperou a
// fila do eMMC. Seja qual for o motivo do disco, o fio de desenho nao pode
// esperar por ele: um fio olha o arquivo a cada 250 ms e o quadro so le duas
// variaveis atomicas. A linha "[aux] stat ... levou" mostra quando o disco
// para — era o que faltava para provar o congelamento.
#ifndef __EMSCRIPTEN__
static _Atomic long pedVidTam;      // tamanho visto pelo fio (<=0: nada)
static _Atomic long pedVidMtime;    // st_mtime visto pelo fio
static _Atomic int pedVidFioOk;     // 1 = o fio esta de pe
static void *pedVidFio(void *x) {
  (void)x;
  for (;;) {
    struct stat st;
    struct timespec a, b;
    double ms;
    int ok;
    clock_gettime(CLOCK_MONOTONIC, &a);
    ok = stat(pedidoVideo(), &st) == 0;
    clock_gettime(CLOCK_MONOTONIC, &b);
    ms = (double)(b.tv_sec - a.tv_sec) * 1000.0 + (double)(b.tv_nsec - a.tv_nsec) / 1e6;
    if (ms > 100.0) {
      printf("[aux] stat do pedido de video levou %.0f ms (disco parado; fora do fio principal)\n", ms);
      fflush(stdout);
    }
    atomic_store(&pedVidMtime, ok ? (long)st.st_mtime : 0L);
    atomic_store(&pedVidTam, ok ? (long)st.st_size : -1L);
    { struct timespec z = { 0, 250000000L }; nanosleep(&z, NULL); }
  }
  return NULL;
}
#endif
static void videoSeSolicitado(void) {
  static time_t bloqueado;
  char url[1024];
  FILE *f;
  p2pTesteBombear();
#ifndef __EMSCRIPTEN__
  { static int tentou;
    if (!tentou) {
      pthread_t t;
      tentou = 1;
      pedidoVideo();   // monta o caminho aqui, antes do fio ler
      atomic_store(&pedVidTam, -1L);
      if (pthread_create(&t, NULL, pedVidFio, NULL) == 0) { pthread_detach(t); atomic_store(&pedVidFioOk, 1); }
    }
    if (atomic_load(&pedVidFioOk)) {
      // Mesma regra de pedidoNovo, com o que o fio viu.
      if (atomic_load(&pedVidTam) <= 0) return;
      if (bloqueado && (time_t)atomic_load(&pedVidMtime) == bloqueado) return;
      bloqueado = 0;
      atomic_store(&pedVidTam, -1L);   // atendido; o fio volta a olhar em 250 ms
    } else if (!pedidoNovo(pedidoVideo(), &bloqueado)) return;
  }
#else
  if (!pedidoNovo(pedidoVideo(), &bloqueado)) return;
#endif
  f = fopen(pedidoVideo(), "r");
  if (!f) return;
  if (fgets(url, sizeof url, f)) {
    char *fim = url + strlen(url);
    while (fim > url && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    { char pub[120]; printf("[video] pedido: %s\n", rede_url_publica(url, pub, sizeof pub)); }
    fflush(stdout);
    if (url[0] == '-') { video_parar(); p2pmotor_segurar(0); }
    else if (!strncmp(url, "p2p:", 4) && p2pmotor_disponivel() && atomic_load(&p2pTesteEstado) == 0) {
      pthread_t t;
      snprintf(p2pTesteHash, sizeof p2pTesteHash, "%s", url + 4);
      clock_gettime(CLOCK_MONOTONIC, &p2pTesteT0);
      p2pmotor_segurar(1);
      atomic_store(&p2pTesteEstado, 1);
      if (pthread_create(&t, NULL, p2pTesteFio, NULL) == 0) pthread_detach(t);
      else { atomic_store(&p2pTesteEstado, 0); p2pmotor_segurar(0); }
    }
    else { video_tocar(url); video_janela(0, 0, 1920, 1080); }
  }
  fclose(f);
  consomeOuBloqueia(pedidoVideo(), &bloqueado);
}

static void capturaSeSolicitado(void) {
  static time_t bloqueado;
  if (!pedidoNovo("/tmp/nuvio-shot-req", &bloqueado)) return;
  consomeOuBloqueia("/tmp/nuvio-shot-req", &bloqueado);

  // Le o DRAWABLE inteiro, nao 1920x1080 fixo: em tela retina o buffer e maior
  // que a janela, e ler o tamanho da janela captura so um quarto da imagem.
  int w = capW, h = capH;
  size_t n = (size_t)w * h * 4;
  unsigned char *px = malloc(n);
  if (!px) return;
  glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);

  // BMP escrito a mao, em UM fwrite. SDL_SaveBMP converte pixel a pixel quando
  // as mascaras nao batem com o formato nativo, e nesta CPU isso leva segundos:
  // o arquivo ficava incompleto quando eu ia le-lo. Aqui a unica conversao e a
  // troca R<->B, feita no proprio buffer.
  for (size_t i = 0; i < n; i += 4) { unsigned char t2 = px[i]; px[i] = px[i+2]; px[i+2] = t2; }

  unsigned int tam = 54 + (unsigned int)n;
  unsigned char cab[54] = {0};
  cab[0] = 'B'; cab[1] = 'M';
  cab[2] = tam & 255; cab[3] = (tam >> 8) & 255; cab[4] = (tam >> 16) & 255; cab[5] = (tam >> 24) & 255;
  cab[10] = 54; cab[14] = 40;
  cab[18] = w & 255; cab[19] = (w >> 8) & 255;
  // altura POSITIVA = linhas de baixo para cima, que e exatamente a ordem em
  // que o glReadPixels devolve. Assim nao ha inversao a fazer.
  cab[22] = h & 255; cab[23] = (h >> 8) & 255;
  cab[26] = 1; cab[28] = 32;
  cab[34] = n & 255; cab[35] = (n >> 8) & 255; cab[36] = (n >> 16) & 255; cab[37] = (n >> 24) & 255;

  // grava num temporario e so entao renomeia: quem le nunca pega arquivo pela metade
  FILE *f = fopen("/tmp/.nuvio-shot.tmp", "wb");
  if (f) {
    fwrite(cab, 1, 54, f);
    fwrite(px, 1, n, f);
    fclose(f);
    rename("/tmp/.nuvio-shot.tmp", "/tmp/nuvio-shot.bmp");
    printf("captura: /tmp/nuvio-shot.bmp (%u bytes)\n", tam);
  }
  free(px);
}

#ifdef __EMSCRIPTEN__
// Cede o controle ao navegador uma vez por quadro, ESPERANDO O rAF.
//
// A primeira versao usava emscripten_sleep(0), que vira setTimeout(0). MEDIDO
// no Chrome: 0,5 FPS, com o proprio app relatando "pior=0.0ms" — o trabalho de
// desenhar custava zero e o tempo inteiro era espera. Motivo: setTimeout numa
// aba em segundo plano e estrangulado para uma chamada por segundo, e mesmo em
// primeiro plano ele nao tem relacao nenhuma com o vsync.
//
// requestAnimationFrame e o unico relogio que o compositor do navegador
// respeita. EM_ASYNC_JS suspende a funcao C (via ASYNCIFY) ate a promessa
// resolver, entao o `while` do main continua sendo o laco do app — nada de
// partir o corpo do quadro num callback.
EM_ASYNC_JS(void, nv_ceder_quadro, (), {
  await new Promise(function (r) { requestAnimationFrame(r); });
});
#endif

#ifdef NV_ANDROID
#define NV_ETAPA(n) android_etapa(n)
#else
#define NV_ETAPA(n) ((void)0)
#endif
static int nvPrimeiroQuadroFeito;
#ifdef __EMSCRIPTEN__
static int window_primeiro_quadro_feito(void) { return nvPrimeiroQuadroFeito; }
#endif
// Para a descoberta (descoberta.h): com conta, a primeira volta espera os
// addons do perfil em vez de montar a Home com a lista vazia.
static int esperaAddonsDaConta(void) {
  if (!sessao_logada() || sync_estado() == SYNC_FALHOU) return 0;
  if (perfis_precisa_escolher()) return 1;
  return sync_perfil_pronto() ? 0 : 2;   // ciclo terminado E aplicado para o perfil ativo
}


// #317: caminho de arranque defensivo (LG/Tizen/Linux; Android tem o seu).
#if !defined(NV_ANDROID) && !defined(__APPLE__) && !defined(__EMSCRIPTEN__)
#define NV_ARRANQUE_REDE 1
#else
#define NV_ARRANQUE_REDE 0
#endif
#if NV_ARRANQUE_REDE
#include <dlfcn.h>   // nv_log_egl; no .tpk (NV_SEM_WEBOS) o include de cima nao vale
// Configs EGL cada vez mais simples: sem profundidade/stencil/MSAA, depois RGB565.
static void nv_attrs_simples(int nivel) {
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 0);
  SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 0);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
  if (nivel == 0) {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8); SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8); SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  } else {
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5); SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5); SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
  }
}
static const char *const nv_nomes_simples[] = { "RGBA8888 sem depth/stencil/MSAA", "RGB565 sem alfa" };
// Janela NULL -> tenta de novo com configs simples. Devolve a janela (ou NULL).
static SDL_Window *nv_janela_simples(SDL_Window *w, const char *t, int pw, int ph, Uint32 fl) {
  for (int i = 0; !w && i < 2; i++) {
    printf("[arranque] janela: tentando %s\n", nv_nomes_simples[i]); fflush(stdout);
    nv_attrs_simples(i);
    w = SDL_CreateWindow(t, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, pw, ph, fl);
    if (!w) { printf("[arranque] janela falhou: %s\n", SDL_GetError()); fflush(stdout); }
  }
  return w;
}
// Contexto falhou: recria janela+contexto com configs simples.
static SDL_GLContext nv_contexto_simples(SDL_Window **w, const char *t, int pw, int ph, Uint32 fl) {
  for (int i = 0; i < 2; i++) {
    printf("[arranque] contexto: tentando %s\n", nv_nomes_simples[i]); fflush(stdout);
    if (*w) { SDL_DestroyWindow(*w); *w = NULL; }
    nv_attrs_simples(i);
    *w = SDL_CreateWindow(t, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, pw, ph, fl);
    SDL_GLContext c = *w ? SDL_GL_CreateContext(*w) : NULL;
    if (c) { printf("[arranque] contexto GL ok com %s\n", nv_nomes_simples[i]); fflush(stdout); return c; }
    printf("[arranque] tentativa falhou: %s\n", SDL_GetError()); fflush(stdout);
  }
  return NULL;
}
// EGL vendor/version via dlsym (sem ligar libEGL no binario: NEEDED nao muda).
static void nv_log_egl(void) {
  typedef void *(*cur_t)(void);
  typedef const char *(*q_t)(void *, int);
  void *h = dlopen("libEGL.so.1", RTLD_NOW);
  if (!h) h = dlopen("libEGL.so", RTLD_NOW);
  cur_t cur = h ? (cur_t)dlsym(h, "eglGetCurrentDisplay") : NULL;
  q_t q = h ? (q_t)dlsym(h, "eglQueryString") : NULL;
  void *d = cur ? cur() : NULL;
  const char *ev = (d && q) ? q(d, 0x3053) : NULL, *ee = (d && q) ? q(d, 0x3054) : NULL;
  printf("[arranque] EGL vendor=%s version=%s | GL vendor=%s renderer=%s version=%s\n",
         ev ? ev : "?", ee ? ee : "?", (const char *)glGetString(GL_VENDOR),
         (const char *)glGetString(GL_RENDERER), (const char *)glGetString(GL_VERSION));
  fflush(stdout);
}
#endif
int main(int argc, char **argv) {
  // NUMERO COM PONTO, SEMPRE. O host .NET do .tpk poe o processo no locale do
  // idioma da TV, e em alemao/portugues/russo o printf("%.2f") sai "0,50" e o
  // strtod para na virgula. Os registros 9866-9920 (Tizen 9, "FPS=51,2")
  // mostram o efeito: todo /scrobble do Trakt levou HTTP 500 porque o JSON
  // saia com "progress":0,50. Nenhum texto do app depende do locale do C —
  // o idioma da interface e o i18n proprio —, entao o numerico e o do C.
  setlocale(LC_NUMERIC, "C");
  // Sem a identidade do app, o SDL do webOS registra a surface como "(null)" e
  // o compositor NAO exibe a janela — o app roda a 60fps desenhando para
  // ninguem. Medido: "Invalid appId specified OR Unsupported Application Type".
#ifndef NV_SEM_WEBOS
#ifdef NV_DTS_DEBUG
  setenv("APPID", NV_APP_ID, 1);
  setenv("LS2_APPID", NV_APP_ID, 1);
#else
  setenv("APPID", NV_APP_ID, 0);
  setenv("LS2_APPID", NV_APP_ID, 0);
#endif
  setenv("SDL_VIDEODRIVER", "wayland", 0);
#endif
  // Lancado pelo SAM, stdout e stderr vao para /dev/null — toda a telemetria
  // (FPS, texturas, teclas) estava sendo descartada em silencio. Log em arquivo
  // e a unica forma de ler qualquer coisa de um app nativo em execucao normal.
  //
  // O CAMINHO SAI DE registro.c e nao esta escrito aqui. E o mesmo arquivo que
  // o painel de log na tela le quando abre; com o nome repetido nos dois lados,
  // trocar um e esquecer o outro daria um painel vazio sem nenhuma pista do
  // motivo. NULL = esta compilacao nao redireciona nada (o Mac, onde o log vai
  // para o terminal, e o alvo Tizen, onde nao ha arquivo util) — que e
  // exatamente o que o antigo #ifndef NV_SEM_WEBOS ja fazia.
#ifdef NV_SINAL_TERMINAR
  signal(SIGTERM, aoSinalTerminar);
  signal(SIGALRM, aoAlarmeTerminar);
#endif
  { const char *log = registro_arquivo();
    // O LOG DA SESSAO ANTERIOR SOBREVIVE UMA VOLTA: renomeado antes de o novo
    // truncar o arquivo. E ele que "Enviar registro" manda quando a sessao
    // anterior morreu sem se despedir (avisos.h). Custa um rename; o arquivo
    // e do proprio app, entao o sticky bit do /tmp nao atrapalha.
    //
    // stderr e o MESMO descritor do stdout (dup2), nao um segundo freopen. Com
    // "w" num e "a" no outro, cada um tinha o seu deslocamento: o stdout
    // escrevia por cima do que o stderr acabara de anexar. Medido na C9 em
    // 23/09: nenhuma linha de stderr sobrevivia no arquivo (o diagnostico do
    // libass, "[legenda] libass: ...", nunca aparecia) e sobravam ~1600 linhas
    // vazias — os restos dos textos sobrescritos.
#if defined(NV_TPK) || defined(NV_ANDROID)
    if (log) { rename(log, getenv("NUVIO_LOG_ANTERIOR"));
#else
    if (log) { rename(log,
#ifdef NV_DTS_DEBUG
        "/tmp/" NV_APP_ID "-anterior.log"
#else
        "/tmp/nuvio-anterior.log"
#endif
      );
#endif
               if (freopen(log, "w", stdout)) { fflush(stderr); dup2(fileno(stdout), fileno(stderr)); } } }
  setvbuf(stdout, NULL, _IOLBF, 0);
  arranque_etapa("main");
  arranque_relatar();
#ifdef NV_DTS_DEBUG
  printf("[dts-debug] appId=%s engine=%d nativeAdapter=%d; diagnostic build, TV validation pending\n",
         NV_APP_ID, dts_engine_available(), dts_pipeline_available(0));
#endif
#ifdef NV_ANDROID
  android_iniciar();   // [tv] no log + espelho no logcat (android.c)
#endif
  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);

  // O SAM lanca o app passando o JSON de launch como argv[1], entao so tratamos
  // argv[1] como caminho quando NAO for JSON.
  char dirBuf[512];
  const char *dirArte = NULL;
  if (argc > 1 && argv[1][0] != '{') dirArte = argv[1];
#ifdef NV_ANDROID
  // Sem argv util no Android: o NuvioActivity exporta NUVIO_ARTE antes do SDL.
  if (!dirArte && getenv("NUVIO_ARTE") && getenv("NUVIO_ARTE")[0]) dirArte = getenv("NUVIO_ARTE");
#endif
  if (!dirArte) {
    char *base = SDL_GetBasePath();
    if (base) { snprintf(dirBuf, sizeof dirBuf, "%sart", base); SDL_free(base); dirArte = dirBuf; }
    else dirArte = "/tmp/art";
  }

  // O compositor do webOS engole o BACK e abre a barra de apps — a menos que a
  // surface declare que o app quer a tecla. Quem faz essa declaracao e o
  // backend Wayland do SDL da LG, atraves deste hint, e ele so e lido na
  // CRIACAO da janela: setar depois nao adianta.
  //
  // Com o hint ligado, o Back chega como um scancode proprio do webOS (482), e
  // nao como SDLK_AC_BACK nem como o 461 dos apps web. Foi por isso que o
  // registro de todos os eventos SDL nao mostrava nada: a tecla nunca era
  // entregue, e o codigo que ela usa tambem nao era o que eu procurava.
  SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");

#ifdef __EMSCRIPTEN__
  // SDL_Delay NUNCA via emscripten_sleep. Com -sASYNCIFY o SDL2 troca o
  // SDL_Delay por emscripten_sleep, que so existe no fio principal; os nossos
  // SDL_Delay estao todos em fios de trabalho (descoberta, streams,
  // recomenda, atualizacao) e, com ASYNCIFY_ONLY (tizen.sh), um deles aborta
  // o worker com "invalid state: 1". Com o hint em 0 o SDL_Delay vira o
  // nanosleep dos pthreads, que e o que sempre se quis ali. O laco de quadro
  // nao usa SDL_Delay: cede pelo rAF (nv_ceder_quadro).
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
  NV_ETAPA("SDL_Init");
  arranque_etapa("SDL_Init");
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
  IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);
  // Antes de qualquer fio: ver nv_blindar_formatos em sdlcompat.h (issue #65).
  nv_blindar_formatos();

#ifdef __APPLE__
  // Perfil de compatibilidade: e o unico do macOS que ainda aceita GLSL 1.20 e
  // as funcoes fixas que o GLES2 tem como core.
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI;
#else
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
#ifdef NV_LINUX_DESKTOP
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif
  // Canal alpha no framebuffer. Sem ele a superficie nao tem como ficar
  // transparente, e o plano de video do aparelho — que fica ATRAS da janela e
  // so aparece pelo alpha — nunca poderia ser revelado.
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
  // E 8 bits por cor, pedidos e nao herdados: o padrao do SDL e 3/3/2 minimo,
  // e o EGL que listar RGB565 antes de 8888 entregaria 32 niveis por canal —
  // degrau de 8/255, que dither nenhum esconde. Na C9 o EGL ja dava 8888
  // (log: "framebuffer R8 G8 B8 A8"); o pedido e para as outras LGs.
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
#if defined(__EMSCRIPTEN__) || defined(NV_LINUX_DESKTOP)
  // Desktop preview is windowed; browser fullscreen requires a user gesture.
  // O navegador da TV ja entrega a pagina em tela cheia; pedir FULLSCREEN
  // aqui exigiria um gesto do usuario e falharia em silencio.
  Uint32 flags = SDL_WINDOW_OPENGL;
#else
  Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN;
#endif
#endif
  // 4K NAO E POSSIVEL NESTE APARELHO — MEDIDO, nao presumido.
  //
  // A TV e 4K, e a ideia (do dono) era renderizar em 3840x2160 e desenhar tudo
  // em dobro: o texto pararia de ser rasterizado a 1080p e ampliado pelo
  // painel, que e o borrao que aparece ao lado do app web.
  //
  // Foram tentados os dois caminhos, na TV, com o contador de quadro do proprio
  // app gravando em /tmp/nuvio-fps.txt:
  //   1. SDL_CreateWindow com 3840x2160  -> drawable=1920x1080
  //   2. appinfo.json "resolution": "3840x2160" -> drawable=1920x1080
  // O compositor do webOS 4.10 fixa a superficie do app nativo em 1080p e
  // ignora os dois pedidos, em silencio. Nao ha o que otimizar aqui: a saida
  // seria o painel receber 1080p e ampliar, que e o que ja acontece.
  //
  // Base para comparacao futura, medida nesta tela (home, sem rolar):
  //   drawable=1920x1080 FPS=50.0 pior=21ms janks=0
  //
  // txt_iniciar continua recebendo a escala do drawable: no aparelho ela e 1 e
  // nao muda nada, no Mac (retina) ela e 2 e a previa deixa de mentir.
  // BUILD DE MEDICAO, LIGADA SO POR BANDEIRA DE COMPILACAO (issue #28).
  //
  // A medicao acima e de 2019, numa C9. O relator tem um B4 de 2024, com um
  // compositor muito mais novo, e nao ha nenhum desses aqui para testar. Em vez
  // de adivinhar ou de embutir um ajuste que ninguem sabe se funciona, existe
  // esta bandeira: `NUVIO_EXTRA_CFLAGS=-DNV_PEDIR_4K bash tools/arm.sh --ipk`
  // gera um .ipk que PEDE 3840x2160 e imprime o que recebeu na linha
  // `janela=... drawable=...` que ja existe logo abaixo.
  //
  // Se o drawable voltar 3840x2160, o resto do app ja acompanha: txt_iniciar e
  // tex_escala recebem dw/NV_TELA_W, que e o mesmo caminho pelo qual a previa
  // no Mac (retina) desenha em 2x. Se voltar 1920x1080, a resposta e a mesma da
  // C9 e nao ha o que fazer neste lado.
  int pedeW, pedeH, pediu4k = 0, autoPediu = 0, autoEst = 0;

  // OS DADOS ANTES DA JANELA, e so por causa desta escolha.
  //
  // dados_iniciar ficava perto de app_iniciar, bem depois daqui. Mas o tamanho
  // da superficie e decidido AGORA, uma vez, e nao ha como redimensiona-la
  // depois — entao o ajuste precisa estar legivel antes. Ela nao depende de
  // SDL: mexe em getenv/fopen/mkdir, e no Emscripten monta o IDBFS. `dirArte`
  // ja esta resolvido desde o topo do main.
  NV_ETAPA("dados_iniciar");
  dados_iniciar(dirArte);
  negcache_disco(dados_ler, dados_gravar_leve);   // 404 de API de metadados lembrado entre arranques
  #ifdef NV_LEVE
  printf("[leve] build de diagnostico: pool de fios 12, sem canal de avisos, sem recomendacoes, sem sync periodico, sem GIF de foco\n");
#endif
  avisos_iniciar();   // le a marca da sessao anterior e grava a desta
  // #334: pasta do motor P2P que sobrou de uma sessao que caiu (TV desligada
  // com o filme tocando). So stat aqui; apagar e em fio solto, sem abrir o motor.
  p2pmotor_limpar_sobra();
  // E OS AJUSTES LOGO ATRAS, pelo mesmo motivo: ajustes_4k() le `valor[]`, que
  // so sai do padrao depois desta chamada. Sem ela a opcao existia na tela,
  // gravava no arquivo e nao fazia efeito nenhum — o pior tipo de ajuste.
  //
  // A chamada de sempre, la embaixo, FICA: ela roda depois de addons_carregar
  // e e a que estabelece o idioma e o espelho do limite de fileiras. Reler o
  // mesmo arquivo duas vezes e barato e deixa aquele bloco intacto.
  ajustes_dir(dados_dir()[0] ? dados_dir() : dirArte);
  // MODO SEGURO, LOGO DEPOIS DE LER OS AJUSTES e antes de qualquer coisa que os
  // use para decidir peso: a superficie 4K (logo abaixo) e o teto de fileiras
  // sao lidos daqui. Se a sessao anterior caiu logo depois de uma mudanca
  // arriscada, ela e desfeita agora; ver seguro.h. O veredito de queda vem de
  // avisos_iniciar (acima), que ja descontou a despedida do Tizen.
  ajustes_seguro_iniciar(avisos_sessao_anterior_caiu());
  // MESMA PASTA DO ajustes_dir logo acima, e pelo mesmo motivo: sem isto
  // art/player.txt (estilo de legenda, aspecto) gravava na pasta de ARTE, nao
  // na de DADOS — e so a de dados sobrevive a TV matando o processo (issue
  // #42, "Settings are not getting saved").
  player_dir(dados_dir()[0] ? dados_dir() : dirArte);

  // 4K SO ONDE A TV DEIXA, E SO SE PEDIREM.
  //
  // Medido nos dois extremos: uma C9 de 2019 (webOS 4.10) IGNORA o pedido em
  // silencio e devolve 1920x1080, tanto por SDL_CreateWindow quanto por
  // `resolution` no appinfo.json; um B4 de 2024 CONCEDE — o relator do #28
  // mediu `drawable=3840x2160` e disse que a fluidez nao mudou, com a
  // interface ja desenhada em quatro vezes os pixels.
  //
  // Por isso e ajuste e nao padrao: onde a TV concede, quadruplicar o
  // preenchimento e decisao de quem esta olhando, nao minha. Onde ela nao
  // concede, ligar nao faz mal nenhum — volta 1080p e o log diz isso.
  //
  // NV_PEDIR_4K continua existindo para a build de medicao, que precisa pedir
  // sem depender de ajuste gravado.
  { int quer4k = ajustes_4k();
    // 4K THAT DID NOT HOLD on this TV in an earlier session (resolucao.h):
    // start at 1080p. Picking 4K again in Settings deletes the file.
    if (quer4k) {
      char *recuo = dados_ler(RES_ARQ_RECUO);
      if (recuo) {
        free(recuo); quer4k = 0;
        printf("[4k] esta TV nao aguentou 4K numa sessao anterior: comeca em 1080p "
               "(escolher 4K de novo em Ajustes tenta outra vez)\n");
      }
    }
    pediu4k = quer4k;
    // AUTOMATIC (resolucao.h): ask for the 4K surface only on a 4K display, and
    // only while the verdict for this TV + this app version is "probe" or "4K".
    if (ajustes_res_auto() && !quer4k) {
      char *mem = dados_ler(RES_ARQ_AUTO);
      int est = res_auto_ler(mem, NV_VERSAO);
      SDL_DisplayMode dm; int tela4k = SDL_GetDesktopDisplayMode(0, &dm) == 0 && dm.w >= 3840;
      free(mem);
      autoEst = est;
      if (est != RES_AUTO_1080 && tela4k) {
        quer4k = 1; autoPediu = 1;
        printf("[4k] automatico: %s\n", est == RES_AUTO_4K ? "4K aprovado nesta TV, pedindo 3840x2160"
                                                          : "tela 4K: sondando a GPU em 4K");
      } else {
        printf("[4k] automatico: 1080p (%s)\n", est == RES_AUTO_1080 ? "decidido antes nesta TV" : "tela nao e 4K");
      }
    }
#ifdef NV_PEDIR_4K
    quer4k = 1;
    printf("[4k] build de medicao: pedindo 3840x2160\n");
#endif
    pedeW = quer4k ? 3840 : (int)NV_TELA_W;
    pedeH = quer4k ? 2160 : (int)NV_TELA_H;
    if (quer4k) { printf("[4k] pedindo %dx%d — a linha `janela=` abaixo diz o "
                         "que a TV concedeu\n", pedeW, pedeH); fflush(stdout); } }
  SDL_Window *win;
#ifdef NV_ANDROID
  // No Android a janela do SDL tem o tamanho da SUPERFICIE: pedir 3840x2160 ao
  // SDL_CreateWindow nao muda nada. Ver android_pedir_superficie.
  if (pedeW > (int)NV_TELA_W) android_pedir_superficie(pedeW, pedeH);
#endif
  const char *windowTitle = "Nuvio";
#ifdef NV_LINUX_DESKTOP
  windowTitle = "Nuvio - Linux UI preview";
#endif
  NV_ETAPA("SDL_CreateWindow");
  arranque_etapa("SDL_CreateWindow");
  win = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     pedeW, pedeH, flags);
#if NV_ARRANQUE_REDE
  // #317: sem janela nao seguimos as cegas. Registra o erro do SDL e tenta
  // atributos GL cada vez mais simples (sem profundidade/stencil/MSAA, depois
  // RGB565 sem alfa), como o Android faz abaixo.
  if (!win) { printf("[arranque] SDL_CreateWindow falhou: %s\n", SDL_GetError()); fflush(stdout); }
  win = nv_janela_simples(win, windowTitle, pedeW, pedeH, flags);
#endif
#ifdef NV_ANDROID
  // CONFIG EGL DE RESERVA (#266). No Android o SDL_CreateWindow ja escolhe a
  // config EGL e cria a superficie; um driver que recusa RGBA8888 (+ a
  // profundidade 16 padrao do SDL) devolvia "janela: ..." e o app fechava sem
  // dizer nada na tela. Tenta configs cada vez mais simples, cada tentativa no
  // log. Sem alfa o video por baixo nao aparece, mas a interface abre e o
  // registro sai — melhor que nada.
  { static const struct { int r, g, b, a, prof; const char *nome; } reserva[] = {
      { 8, 8, 8, 8, 0, "RGBA8888 sem profundidade" },
      { 8, 8, 8, 0, 0, "RGB888 sem alfa" },
      { 5, 6, 5, 0, 0, "RGB565" },
    };
    size_t i;
    for (i = 0; !win && i < sizeof reserva / sizeof reserva[0]; i++) {
      printf("[android] janela falhou (%s); tentando EGL %s\n", SDL_GetError(), reserva[i].nome);
      fflush(stdout);
      SDL_GL_SetAttribute(SDL_GL_RED_SIZE, reserva[i].r);
      SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, reserva[i].g);
      SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, reserva[i].b);
      SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, reserva[i].a);
      SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, reserva[i].prof);
      SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
      win = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             pedeW, pedeH, flags);
    }
    if (win && i) { printf("[android] janela com EGL de reserva: %s\n", reserva[i - 1].nome); fflush(stdout); }
  }
#endif
  if (!win) {
    printf("janela: %s\n", SDL_GetError()); fflush(stdout);
    arranque_etapa("FALHA: sem janela SDL");
    SDL_Quit();
    return 1;
  }
  // CURSOR DO SISTEMA LIGADO NO webOS, DESLIGADO NO RESTO (issue #99).
  //
  // No webOS o SDL_ShowCursor(SDL_DISABLE) NAO SO ESCONDE a seta do Magic
  // Remote: o SDL da LG (2.0.5-webos da C9) para de entregar SDL_MOUSEMOTION e
  // SDL_MOUSEBUTTON. MEDIDO na C9 em 23/09 injetando o ponteiro no evdev do
  // controle: desligado, chegavam o 484 (cursor apareceu), o ENTER da janela e
  // a rodinha — e nenhum movimento; ligado, MOUSEMOTION em coordenadas 1920x1080
  // e MOUSEBUTTONDOWN/UP botao 1 no clique. Por isso aqui ele fica ligado, e a
  // seta desenhada e a do proprio sistema (ponteiro.c nao desenha outra).
  // No Mac o cursor do sistema continua escondido e o app desenha o seu.
#ifdef NV_SEM_WEBOS
  SDL_ShowCursor(SDL_DISABLE);
#else
  SDL_ShowCursor(SDL_ENABLE);
#endif
#ifndef NV_SEM_WEBOS
  // Declara a superficie NAO-opaca. Por padrao o compositor trata a janela como
  // opaca e descarta o canal alpha inteiro — o furo do gfx_furo existiria no
  // framebuffer e mesmo assim nada apareceria atras dele.
  //
  // Duas armadilhas medidas neste aparelho, ambas silenciosas:
  // 1. o SDL daqui escreve um SDL_SysWMinfo MAIOR que o header declara, entao a
  //    struct vai num buffer folgado e nao numa variavel do tamanho "certo";
  // 2. o `version` tem de vir de SDL_GetVersion(); preenchido a mao o SDL
  //    recusa em silencio e a unica pista e a tela preta.
  {
    static char infoBuf[512];
    SDL_SysWMinfo *info = (SDL_SysWMinfo *)infoBuf;
    SDL_GetVersion(&info->version);
    // O SDL do aparelho pode ser bem mais velho que o header (TV: 2.0.4,
    // header 2.30). So confiamos no layout manual se o runtime e wayland e
    // novo o bastante para ter o subsistema (>= 2.0.2).
    printf("[arranque] SDL runtime %d.%d.%d\n", info->version.major, info->version.minor, info->version.patch);
    arranque_etapa("SDL_GetWindowWMInfo");
    if (SDL_GetWindowWMInfo(win, info)) {
      // O SDL_config.h do SDK vem com SDL_VIDEO_DRIVER_WAYLAND desligado, entao
      // o campo info.wl nem existe no header — mas o SDL do aparelho E wayland.
      // Ler por deslocamento evita depender de um header que descreve outra
      // compilacao: version ocupa 3 bytes (alinhado a 4), subsystem vem em 4, e
      // a uniao comeca em 8. Para wayland ela e {display, surface, ...}.
      int sub = *(int *)(infoBuf + 4);
      void **campos = (void **)(infoBuf + 8);
      void *sup = campos[1];
      void *wl = dlopen("libwayland-client.so.0", RTLD_NOW);
      void (*marshal)(void *, unsigned, ...) =
          wl ? (void (*)(void *, unsigned, ...))dlsym(wl, "wl_proxy_marshal") : NULL;
      printf("syswm sub=%d display=%p surface=%p\n", sub, campos[0], sup);
      int verOk = info->version.major > 2 || (info->version.major == 2 &&
                  (info->version.minor > 0 || info->version.patch >= 2));
      // Opcode 4 de wl_surface e set_opaque_region; NULL = "nada e opaco".
      // Sem commit de proposito: o commit vem do proximo SwapWindow.
      if (sub != (int)SDL_SYSWM_WAYLAND || !verOk) {
        printf("[arranque] syswm nao e wayland confiavel (sub=%d, SDL %d.%d.%d): pulando set_opaque_region\n",
               sub, info->version.major, info->version.minor, info->version.patch);
      } else {
        arranque_etapa("wayland set_opaque_region");
      if (marshal && sup) { marshal(sup, 4, NULL); printf("superficie nao-opaca\n"); }
      else printf("sem wayland: video nao vai aparecer\n");
      }
    }
  }
#endif
  NV_ETAPA("SDL_GL_CreateContext");
  arranque_etapa("SDL_GL_CreateContext");
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
#if NV_ARRANQUE_REDE
  if (!ctx) {
    printf("[arranque] SDL_GL_CreateContext falhou: %s\n", SDL_GetError()); fflush(stdout);
    ctx = nv_contexto_simples(&win, windowTitle, pedeW, pedeH, flags);
    if (!ctx) {
      printf("[arranque] sem contexto GL em nenhuma config: %s\n", SDL_GetError()); fflush(stdout);
      arranque_etapa("FALHA: sem contexto GL");
      if (win) SDL_DestroyWindow(win);
      SDL_Quit();
      return 2;
    }
  }
  nv_log_egl();
#endif
#ifdef NV_LINUX_DESKTOP
  if (!ctx) {
    printf("[linux] sem contexto GLES2: %s\n", SDL_GetError());
    SDL_DestroyWindow(win); SDL_Quit(); return 2;
  }
#endif
#ifdef NV_TPK
  // Sem GL (Tizen 4/5 sem superficie) nao ha o que desenhar; sai e o host
  // mostra o motivo (nv_tpk_erro).
  if (!ctx) { printf("[tpk] sem contexto GL, saindo\n"); SDL_Quit(); return 2; }
#endif
#ifdef NV_ANDROID
  if (!ctx) {
    // A janela ja existe com a config escolhida; recriar janela e contexto
    // numa config mais simples (o contexto ES2 e o mesmo pedido em todas).
    printf("[android] sem contexto GL: %s; tentando RGB565 sem profundidade\n", SDL_GetError());
    fflush(stdout);
    SDL_DestroyWindow(win);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 6);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
    win = SDL_CreateWindow(windowTitle, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           pedeW, pedeH, flags);
    ctx = win ? SDL_GL_CreateContext(win) : NULL;
  }
  if (!ctx) { printf("[android] sem contexto GL: %s\n", SDL_GetError()); SDL_Quit(); return 2; }
#endif
  // O cursor do Magic Remote e desenhado pelo app (ponteiro.c). Depois do
  // contexto: o log de arranque dele le a janela corrente.
  ponteiro_iniciar();
#ifdef __APPLE__
  // Sem vsync no Mac. O SDL2 do Homebrew virou uma camada sobre o SDL3
  // (sdl2-compat), e nela o SwapWindow fica preso esperando um sinal de vsync
  // que nunca chega quando a janela nao esta em primeiro plano — o app trava no
  // primeiro quadro. No aparelho o SDL2 e o de verdade e o vsync fica ligado,
  // que e o que mantem os 60fps estaveis la.
  SDL_GL_SetSwapInterval(0);
#else
  SDL_GL_SetSwapInterval(1);
#endif
  // O tamanho REAL do buffer importa mais que o tamanho pedido: esta TV e 4K, e
  // se o compositor entregar uma superficie 3840x2160 cada camada de tela cheia
  // custa quatro vezes o que a conta de 1080p diz.
  int dw = 0, dh = 0, jw = 0, jh = 0;
  SDL_GL_GetDrawableSize(win, &dw, &dh);
  SDL_GetWindowSize(win, &jw, &jh);
  printf("GPU: %s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
  printf("janela=%dx%d drawable=%dx%d\n", jw, jh, dw, dh);
  // O plano de video e posicionado em pixels da superficie, o layout em 1920x1080
  // (#176: com drawable 3840x2160 o video ocupava um quarto da tela).
  video_escala_definir(dw, dh);
  // Pedir SDL_GL_ALPHA_SIZE nao garante receber: o EGL escolhe a config mais
  // proxima e pode entregar 0 bits de alpha em silencio. Com 0 aqui, o furo da
  // superficie e impossivel e o plano de video NUNCA vai aparecer, por mais
  // certo que esteja o lado do ACB.
  { int a = -1, r = -1, g = -1, b = -1;
    SDL_GL_GetAttribute(SDL_GL_ALPHA_SIZE, &a);
    SDL_GL_GetAttribute(SDL_GL_RED_SIZE, &r);
    SDL_GL_GetAttribute(SDL_GL_GREEN_SIZE, &g);
    SDL_GL_GetAttribute(SDL_GL_BLUE_SIZE, &b);
    printf("framebuffer R%d G%d B%d A%d%s\n", r, g, b, a,
           a > 0 ? "" : "  <<< SEM ALPHA: video nao tem como aparecer");
    // O que o GL diz do alvo LIGADO, e nao o que o SDL leu da config EGL: sao
    // fontes diferentes, e so esta enxerga um drawable que o driver rebaixou.
    { GLint br = -1, bg = -1, bb = -1, ba = -1;
      glGetIntegerv(GL_RED_BITS, &br);   glGetIntegerv(GL_GREEN_BITS, &bg);
      glGetIntegerv(GL_BLUE_BITS, &bb);  glGetIntegerv(GL_ALPHA_BITS, &ba);
      printf("framebuffer GL R%d G%d B%d A%d%s\n", br, bg, bb, ba,
             (br > 0 && br < 8) ? "  <<< MENOS DE 8 BITS: degrade vai sair em faixas" : ""); } }

  // MARCOS FINOS DO ARRANQUE. Na TV Samsung o log parava exatamente na linha
  // "framebuffer ..." acima e nada mais saia — sem erro, sem excecao. Entre
  // aquele printf e o proximo havia quatro passos, todos triviais, e adivinhar
  // qual custaria uma ida a TV por tentativa. Estes marcos custam uma linha
  // cada e respondem de primeira.
  printf("[arranque] viewport\n"); fflush(stdout);
  NV_ETAPA("primeiro clear");

  // Em tela retina o drawable e maior que a janela; sem ajustar o viewport, o
  // desenho ocupa um quarto da tela.
  SDL_GL_GetDrawableSize(win, &dw, &dh);
  glViewport(0, 0, dw, dh);
  gfx_tamanho_alvo(dw, dh);
  capW = dw; capH = dh;
#ifdef NV_ANDROID
  // QUADRO DE ESPERA NA COR DA SPLASH. A SurfaceView do SDL fura a janela: com
  // ela no ar, o fundo da janela (drawable/abertura.xml) some atras do furo e o
  // que aparece e PRETO ate o primeiro SwapWindow — medido no emulador Android
  // TV, ~700 ms entre a splash do sistema e a abertura; a TCL da #223 levou
  // 2,8 s para o primeiro quadro. Um clear + swap aqui, antes dos shaders, troca
  // esse preto pela cor lisa da abertura (#0E0F12, a mesma de values/cores.xml). So no
  // Android: na LG o sistema segura o splash.png ate o primeiro quadro, e uma
  // cor lisa aqui apagaria a marca.
  glClearColor(14.0f / 255.0f, 15.0f / 255.0f, 18.0f / 255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  SDL_GL_SwapWindow(win);
  // TESTE DO VIGIA (#266): `run-as ... touch files/dados/teste-travar-arranque`
  // prende o main() aqui, como numa TV que nao passa da tela preta.
  { char f[600];
    snprintf(f, sizeof f, "%s/teste-travar-arranque", dados_dir());
    if (dados_dir()[0] && access(f, F_OK) == 0) {
      printf("[android] teste: arranque preso de proposito (%s)\n", f); fflush(stdout);
      android_etapa("teste-travar-arranque");
      for (;;) SDL_Delay(1000);
    } }
#endif

  // O relogio dos marcos comeca AQUI e nao no topo do main: o que vem antes e
  // parse de argumento e SDL_Init, que nao dependem de nada nosso.
  printf("[arranque] marco_iniciar\n"); fflush(stdout);
  marco_iniciar();
  printf("[arranque] rede_preparar\n"); fflush(stdout);
  NV_ETAPA("rede_preparar");
  // ANTES de tex_iniciar e de app_iniciar, que sao quem cria os fios de rede.
  // Discord alone uses the bundled Mozilla roots on native TV builds.
  char discordCa[4096];
  snprintf(discordCa, sizeof discordCa, "%s/discord-ca.pem", dirArte);
#if (defined(__APPLE__) || defined(NV_LINUX_DESKTOP)) && !defined(NV_ANDROID) && !defined(NV_TPK)
  FILE *discordRoots = fopen(discordCa, "rb");
  if (discordRoots) fclose(discordRoots);
  else discordCa[0] = 0; // Desktop development can use system trust.
#endif
#ifdef NV_TPK
  // #290: the .tpk in-app update replaces only libnuvio.so; res/art is the
  // one from the installed package, and discord-ca.pem only ships since
  // 1.7.4. A CAINFO pointing at a missing file makes curl fail every verified
  // https request at once (CURLE_SSL_CACERT_BADFILE). Without the bundle,
  // fall back to the TV's own trust store.
  if (access(discordCa, R_OK) != 0) {
    printf("[rede] %s missing from the package: using the TV's CA store\n", discordCa);
    fflush(stdout);
    discordCa[0] = 0;
  }
#endif
  rede_discord_ca(discordCa);
  rede_preparar();
  arranque_enviar();   // #317: se a sessao anterior morreu no arranque, conta isso ao servidor (1x)
#if defined(NV_TPK) || defined(__EMSCRIPTEN__)
  // SAMSUNG (.tpk e .wgt): nunca Dolby Vision (regra do dono; o Player do Tizen falha no
  // Prepare, 42% dos picks DV nos logs 2.0.2). HDR10/HDR10+ por modelo nao tem
  // API antes de tocar: -1 = desconhecido, nao penaliza. Android informa a
  // tela pelo NvPlayer (nativeTela); webOS nao tem consulta antes do pipeline.
  stream_definir_tela(-1, 0);
  printf("[video] samsung tela hdr=-1 dv=0\n");
#endif
  // NIVEL DE GPU (gpunivel.h): le GL_*, marca a GPU fraca no perfil e decide
  // o nivel de partida ANTES de tex_iniciar, que tira o perfil do aparelho.
#ifdef NV_ANDROID
  // #266: garante o contexto GL corrente neste fio antes das consultas (o
  // primeiro SwapWindow ja saiu acima).
  NV_ETAPA("GL MakeCurrent");
  if (SDL_GL_GetCurrentContext() != ctx) SDL_GL_MakeCurrent(win, ctx);
#endif
  gpun_iniciar(dw, dh);
  { const char *gr = (const char *)glGetString(GL_RENDERER);
    if (ptv_gpu_utgard(gr)) ajustes_trailer_hero_vetar(gr); }
  gputempo_iniciar();   // GPU clock per frame: Android diagnostic, opt-in (gputempo.h)
  int gpuPref = ajustes_gpu_efeitos();
  if (gpuPref) gpun_preferencia(gpuPref);
  if (ajustes_720p()) gpun_forcar_720();
#ifdef NV_TPK
  video_tpk_zoom_roi_definir(ajustes_trailer_zoom_tpk());
#endif
  printf("[arranque] gfx_iniciar (compila os shaders)\n"); fflush(stdout);
  marco("gfx_iniciar");
  NV_ETAPA("gfx_iniciar (shaders)");
  arranque_etapa("gfx_iniciar");
  if (!gfx_iniciar()) { printf("[arranque] gfx_iniciar FALHOU\n"); fflush(stdout); return 1; }
  printf("[arranque] gfx_iniciar ok\n"); fflush(stdout);
  // A marca da abertura (#213), ANTES do primeiro quadro: ele ja nasce com ela,
  // no lugar em que o splash do sistema a deixou.
  logoapp_iniciar(dirArte);
  abertura_iniciar(dirArte);
#ifdef __EMSCRIPTEN__
  // O ARRANQUE CEDE AO NAVEGADOR EM DOIS PONTOS (24/09/2026). Do topo do main
  // ate o primeiro quadro era UMA tarefa so do fio principal: 1,0 a 3,4 s nos
  // registros da Samsung (`[t] ... primeiro quadro na tela` = 1067, 1455,
  // 2561, 3253, 3448 ms; `longtask-max=2755`/`4325 ms` no primeiro relatorio).
  // A maior parte e a compilacao dos shaders (gfx_iniciar: 0,7 a 2,0 s na TV
  // de 1 GB) e o resto e app_iniciar + a montagem do primeiro quadro. Ceder um
  // rAF aqui e depois de app_iniciar nao encurta nada disso; parte a tarefa
  // em tres, e o navegador (e o vigia de pagina que nao responde, se a TV tiver
  // um) ve a pagina respirar no meio. O canvas ainda nao tem nada desenhado.
  nv_ceder_quadro();
#endif
  // fonts/ fica ao lado de art/: derruba o ultimo componente do caminho da arte
  char dirRec[512];
  snprintf(dirRec, sizeof dirRec, "%s", dirArte);
  char *barra = strrchr(dirRec, '/');
  if (barra) *barra = 0;
  NV_ETAPA("txt_iniciar");
  txt_iniciar(dirRec, (float)dw / NV_TELA_W);
  // A MESMA escala vai para o cache de texturas: e ela que decide o teto de
  // decodificacao de cada arte a partir da largura com que o card a desenha.
  // Sem isto todo card decodificava com o teto unico de 640 e o cache batia no
  // orcamento com ~40 texturas.
  tex_escala((float)dw / NV_TELA_W);
  marco("fontes+tex prontos");
  // 192 slots, nao 96. O teto de slots so faz sentido junto com o tamanho de
  // cada textura: com o teto unico de 640 cada uma custava 2,4 MB e 96 slots ja
  // estouravam o orcamento de 96 MB (medido: `texturas=40 pend=32 92.3MB` com a
  // home rolando — o cache despejava o que ainda estava na tela). Com o teto
  // por uso a mesma arte custa ~500 KB na TV, e 192 slots cabem com folga.
  //
  // Isso tambem dobra o teto de itens EM VOO, que e nMax/3 em slotLivre: a
  // fileira que entra na tela pede tudo de uma vez em vez de pedir aos poucos.
#ifdef __EMSCRIPTEN__
  // O DECODIFICADOR DO NAVEGADOR NASCE AQUI, e nao no primeiro pedido: criar
  // Worker e canal e trabalho do fio principal, e no primeiro pedido ele ja
  // podia estar preso numa tarefa longa (ver o canal direto em webp.c).
  navegador_iniciar();
#endif
  NV_ETAPA("tex_iniciar");
  tex_iniciar(192);
  { int mb = 0; long mem = 0;
    tex_orcamento_info(&mb, &mem, NULL, NULL);
    gpun_log_perfil(mem, mb, tex_fios_rede(), tex_teto_heroi()); }
  // A POLITICA DE ARTE PERGUNTA AO CACHE o que ja falhou: e assim que ela sabe
  // passar do metahub (1920, barato) para a reserva do TMDB sem pedir duas
  // vezes a mesma arte que nao existe. Ver artehero.h.
  artehero_definir_falhou(tex_falhou);
  // "Destaque com outra arte" confere se a outra arte nao e o MESMO arquivo
  // do card (mesma url real ou mesmos bytes) — artereserva.h.
  artehero_definir_igual(arte_mesma_imagem);
  artehero_definir_resolvida(arte_fonte_resolvida);
  // A arte escolhida a mao (#142): lida do disco na primeira consulta.
  artehero_definir_escolha(arteesc_fundo, arteesc_logo);
  // Fonte "Apple TV" do destaque: a arte-chave que a busca do trailer traz.
  arte_fonte_definir_apple(trailerapple_arte);
  // A conta vem ANTES da UI: app_iniciar decide entre abrir na home e abrir no
  // login, e para decidir ele precisa saber se ha sessao gravada. (dados_iniciar
  // ja rodou la em cima, antes da janela — ver a nota do 4K.)
  NV_ETAPA("conta");
  nuvem_configurar(dirArte);
  sessao_iniciar();
  perfis_carregar_ativo();
  // Vinculos feitos NESTA TV. Vem antes de trakt_carregar (que le o arquivo do
  // pacote) para o vinculo do usuario ganhar do arquivo de quem montou — e num
  // pacote distribuivel esse arquivo nem existe.
  // POR PERFIL: o do perfil gravado, que perfis_carregar_ativo acabou de ler.
  // Se a tela de escolha trocar o perfil, app.c chama traktauth_trocar_perfil.
  traktauth_carregar_perfil(perfis_ativo());
  simklauth_carregar_perfil(perfis_ativo());
  NV_ETAPA("app_iniciar");
  arranque_etapa("app_iniciar");
  if (!app_iniciar(dirArte)) return 1;
  arranque_etapa("app_iniciar ok");
  NV_ETAPA("addons/catalogo");
  // Cor viva: a paleta da ultima cena volta ANTES do primeiro quadro, entao
  // quem usa o tema dinamico ja abre o app na cor do ultimo titulo.
  corviva_carregar();
#ifdef __EMSCRIPTEN__
  nv_ceder_quadro();   // o segundo ponto: ver a nota logo depois de gfx_iniciar
#endif
  // Progresso e dado DO USUARIO: sai da pasta do pacote, que e a mesma para
  // todo mundo que usar o aparelho, e passa para a pasta da instalacao.
  if (dados_dir()[0]) cat_dir_gravacao(dados_dir());

  // DUAS PASTAS, e nao uma.
  //
  // `dirArte` e o PACOTE: so-leitura por definicao. No webOS isso era teorico
  // (o .ipk instalado e gravavel em modo desenvolvedor) e por isso tudo cabia
  // numa variavel so. No alvo Tizen deixou de ser: o .wgt e so-leitura de
  // verdade, e no build de hoje /app/art vem de --preload-file, ou seja MEMFS,
  // que e APAGADO a cada recarga. Gravar la nao falha — e o pior dos dois
  // mundos, porque some sem erro.
  //
  // `dirDados` e a pasta descoberta por dados_iniciar: /nuvio (IDBFS) no Tizen,
  // ~/.nuvio ou /media/developer/temp/nuvio nos outros. Quando nenhuma serve, a
  // propria dados_iniciar ja escolheu dirArte como ultimo recurso e as duas
  // voltam a coincidir — que e exatamente o comportamento de hoje.
  const char *dirDados = dados_dir()[0] ? dados_dir() : dirArte;

  // A configuracao de addons mora junto da arte. Ausente, o app segue com a
  // lista de exemplo — nunca fica sem nada para mostrar. addons.c olha a pasta
  // gravavel primeiro: a lista da CONTA e guardada la e sobrevive a recarga.
  addons_carregar(dirArte);
  // Plugins Nuvio (F09, desligados por padrao): estado da conta+perfil e a
  // ligacao como mais uma origem da busca de fontes.
  plugins_iniciar();
  plugins_ligar_aos_addons();
  plex_ligar_aos_addons();   // Plex: server file as one more source on matching titles
  // Ajustes tambem sao do USUARIO, nao do pacote.
  ajustes_dir(dirDados);
  // Icone do app (apoiadores): a arte vem do pacote; o alias do launcher do
  // Android e a abertura do .wgt seguem o que ficou gravado (idempotente).
  iconeapp_iniciar(dirArte);
  iconeapp_aplicar_plataforma();
  ajustes_idioma_auto_iniciar(aoMudarIdiomaAuto);
  // A estrutura persistida só pode ser comparada após carregar a configuração.
  homeestado_iniciar();
  { // Nativo conserva arte comprimida na pasta gravavel, sujeita a poda LRU.
    // Tizen inicializa aqui o IndexedDB separado de imagens, lido sob demanda;
    // a pasta IDBFS continua disponivel para o caminho legado de GIFs.
    char c[600];
    snprintf(c, sizeof c, "%s/cache", dirDados);
    tex_cache_dir(c); }
  // Os icones da interface saem de art/icones (SVG do app web rasterizados).
  gfx_icones_dir(dirArte);
  // Catalogo da rede. O do pacote ja esta carregado e continua na tela ate a
  // resposta chegar — abrir vazio enquanto busca seria pior que mostrar o de
  // ontem por dois segundos.
  trakt_carregar(dirArte);
  desc_tmdb(dirArte);
  desc_espera_addons_definir(esperaAddonsDaConta);
  desc_iniciar();
  // RESOLUCAO DE LAYOUT, e nao metade: o snapshot e o fundo parado atras do
  // painel de Salvos (app.c), e a esquerda dele e uma faixa de 1120 px da home
  // a 42 % de brilho — na metade, o texto das fileiras amolecia no instante em
  // que a copia entrava no lugar da home desenhada. RGB, ~6 MB.
  gfx_snap_iniciar((int)NV_TELA_W, (int)NV_TELA_H);
  // Alvo minusculo de proposito: e ele esticado que vira o desfoque do fundo.
  // 480x270: com o gaussiano de duas passadas, o que importa nao e o alvo ser
  // minusculo (isso e que produzia blocos ao esticar) e sim o desfoque ser de
  // verdade. Esticado 4x, nenhuma borda de texel aparece.
  gfx_borrao_iniciar(480, 270);
  NV_ETAPA("primeiro quadro");

  Uint32 ultRelato = SDL_GetTicks();
  double txtMsQuadro = 0, piorTxtMs = 0;
  static int rastroQuadros;
  static double gpuFillSoma, gpuFillPico;   // [gpu-modos] fill: media e pico da janela
  static int gpuFillN;
  double fPrep = 0, fGlClr = 0;
  int    txtNQuadro = 0, piorTxtN = 0;
  int quadros = 0, janks = 0; double pior = 0;

  // TELEMETRIA POR FASE. O quadro pior custava 22ms num alvo de 20ms e nao
  // havia como saber ONDE. Os relogios sao de CPU (SDL_GetPerformanceCounter)
  // e NAO ha glFinish em lugar nenhum: glFinish esconde o jank, porque
  // distribui o custo de GPU igualmente por todos os quadros em vez de deixar
  // o atraso aparecer onde ele nasce. Aqui, `des` e o custo de SUBMETER o
  // desenho (CPU) e `swap` absorve a espera do vsync MAIS o que a GPU ainda
  // devia — um quadro pesado de GPU aparece como swap grande, um quadro pesado
  // de CPU aparece na fase que o causou.
  double perFreq = (double)SDL_GetPerformanceFrequency();
  Uint64 ultQuadro = SDL_GetPerformanceCounter();
  double fEv=0, fBomb=0, fUpd=0, fDes=0, fSwap=0, fAux=0, fClr=0;
  int fUplN=0, pUplN=0; long fUplB=0, pUplB=0;
  double pEv=0, pBomb=0, pUpd=0, pDes=0, pSwap=0, pAux=0, pClr=0;
  double pPrep=0, pGlClr=0;
  // Dentro de `des`: quanto e travessia de GL e quanto e busca no cache.
  double fFill=0, pFill=0; int fNCheio=0, pNCheio=0;
  double fGfxMs=0, fTexMs=0, fOutMs=0; int fNRect=0, fNProg=0, fNBind=0, fNBusca=0, fNOut=0;
  double pGfxMs=0, pTexMs=0, pOutMs=0; int pNRect=0, pNProg=0, pNBind=0, pNBusca=0, pNOut=0;
#define NV_T0() (SDL_GetPerformanceCounter())
#define NV_DT(a) ((SDL_GetPerformanceCounter() - (a)) * 1000.0 / perFreq)

#ifdef __EMSCRIPTEN__
  // DE QUEM E A TAREFA LONGA (23/09/2026). O [navegador] diz que o fio
  // principal parou 1 a 31 s (Samsung 1.4.1), mas nao se foi o NOSSO quadro
  // ou o que roda entre dois quadros (chamadas proxiadas dos pthreads, IDBFS,
  // timers do shell, o proprio navegador). Com o FPS abaixo de 7 nem o
  // [quadro] aparecia (o `quadros > 20` abaixo), e era exatamente quando as
  // tarefas eram maiores. c-max: o maior trecho de C sem ceder (do retorno de
  // nv_ceder_quadro ate a proxima chamada: uma tarefa do navegador inteira e
  // nossa). fora-max: a maior espera dentro de nv_ceder_quadro (o que o
  // navegador fez no meio). longtask-max ~ fora-max com c-max pequeno = nao
  // e codigo do laco.
  Uint64 fimCeder = SDL_GetPerformanceCounter();
  double cMaxMs = 0, foraMaxMs = 0;
#endif
  while (!app_quer_sair()
#ifdef NV_SINAL_TERMINAR
         && !sinalTerminou
#endif
         ) {
    SDL_Event e;
    Uint64 tEv = NV_T0();
    // VIRADA DE QUADRO DO CATALOGO, antes de qualquer tela tocar em cat_item():
    // aqui nenhum ponteiro de item do quadro anterior esta mais na mao, entao
    // os blocos trocados fora durante ele podem morrer. Ver cat_quadro.
    cat_quadro();
#ifdef __EMSCRIPTEN__
    // FASE ATUAL para o observer de longtask do shell (window.__nvFase): a
    // tarefa longa e atribuida a quem estava em cena quando ela foi vista.
    // So cruza para o JS quando a fase muda (poucas vezes por sessao).
    { static const char *faseAnt;
      static int jaPrimeiro;
      const char *fase;
      if (!jaPrimeiro && window_primeiro_quadro_feito()) jaPrimeiro = 1;
      fase = !jaPrimeiro ? "startup" : (player_aberto() ? "player"
           : (desc_montando() ? "catalog" : "home"));
      if (fase != faseAnt) { faseAnt = fase; EM_ASM({ window.__nvFase = UTF8ToString($0); }, fase); }
    }
#endif
    // Enquanto o detalhe existe ele fica com o teclado inteiro: a home
    // continua desenhada por baixo, mas nao deve reagir ao D-pad.
    while (SDL_PollEvent(&e)) {
      ponteiro_diag(&e);
      // PROTECAO DE OLED (esmaecer.h): toda acao da pessoa acorda a tela, e a
      // tecla que acorda so acorda — nao age.
      if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP || e.type == SDL_MOUSEMOTION ||
          e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_MOUSEBUTTONUP ||
          e.type == SDL_MOUSEWHEEL || e.type == SDL_FINGERDOWN) {
        int age = e.type == SDL_KEYDOWN || e.type == SDL_MOUSEBUTTONDOWN || e.type == SDL_FINGERDOWN;
        // OK na vitrine da tela de descanso abre o titulo que esta nela
        // (descanso.h). A tecla continua engolida: quem abre e o app.c.
        if (e.type == SDL_KEYDOWN && esmaecer_descanso() &&
            (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_KP_ENTER))
          descanso_pedir_abrir();
        if (esmaecer_entrada(SDL_GetTicks(), age)) continue;
      }
      // Teclado do sistema (entrada_texto.h): ve o texto ANTES de qualquer tela.
      texto_sistema_observar(&e);
      if (e.type == SDL_WINDOWEVENT) {
        // Ultimo sinal de vida na marca de sessao (avisos_sinal): e o que diz,
        // na abertura seguinte, se a sessao que "nao se despediu" tinha ido
        // para segundo plano antes de morrer.
        const char *ev = NULL;
        switch (e.window.event) {
          case SDL_WINDOWEVENT_HIDDEN:       ev = "oculto"; break;
          case SDL_WINDOWEVENT_SHOWN:        ev = "visivel"; break;
          case SDL_WINDOWEVENT_MINIMIZED:    ev = "minimizado"; break;
          case SDL_WINDOWEVENT_FOCUS_LOST:   ev = "foco-perdido"; break;
          case SDL_WINDOWEVENT_FOCUS_GAINED: ev = "foco-voltou"; break;
          default: break;
        }
        if (ev) avisos_sinal(ev, (float)rssMB());
        // VOLTOU DO SEGUNDO PLANO: a reconsulta de versao vencida espera uns
        // segundos em vez de sair junto com tudo o que acorda (atualizacao.h).
        if (e.window.event == SDL_WINDOWEVENT_SHOWN ||
            e.window.event == SDL_WINDOWEVENT_FOCUS_GAINED ||
            e.window.event == SDL_WINDOWEVENT_RESTORED)
          atualizacao_retomou();
        continue;
      }
      if (e.type == SDL_APP_DIDENTERFOREGROUND) atualizacao_retomou();
      // PONTEIRO (Magic Remote, issue #99): mouse, rodinha e os avisos 484/485
      // de cursor do webOS ficam la; o que ele traduz em tecla chega a
      // app_evento como se viesse do D-pad.
      if (ponteiro_evento(&e, app_evento)) continue;
      // O BACK do webOS chega com scancode proprio (482), nao como AC_BACK, e
      // com KEYDOWN e KEYUP quase juntos — so o KEYDOWN conta. Isto ja tinha
      // sido resolvido uma vez e voltou a quebrar quando limpei os remendos
      // antigos: o tratamento saiu junto.
#if defined(__EMSCRIPTEN__) || defined(NV_LINUX_DESKTOP)
      // TIZEN: o Return do controle Samsung e o keyCode 10009 (XF86Back), que
      // nao existe na tabela do SDL. tizen-shell.html o traduz em Escape, e
      // AQUI o Escape vira AC_BACK — o mesmo codigo que o webOS entrega.
      //
      // Normalizar no ponto unico, e nao tela a tela, porque o defeito
      // apareceu justamente onde faltava: a filmografia so tratava AC_BACK
      // (detail.c), entao no Tizen o voltar nao voltava dali. Corrigir so
      // aquela tela deixaria a proxima com a mesma armadilha. Conferido antes:
      // nenhuma tela do app trata ESCAPE sem tratar AC_BACK junto, entao a
      // conversao nao tira nada de ninguem.
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
        e.key.keysym.sym = SDLK_AC_BACK;
#ifdef NV_LINUX_DESKTOP
      if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_BACKSPACE)
        e.key.keysym.sym = SDLK_AC_BACK;
#endif
#endif
      // CH+ segurado x tocado (central.h). Com canal na tela (zap) ele segue
      // direto, sem atraso.
      if (teclaCh(&e) && central_tecla(&e, !app_zap_ativo() && app_central_pode())) continue;
      remapCanal(&e);
      // TECLA DESCONHECIDA, UMA LINHA CADA, UMA VEZ SO.
      //
      // Este app aprendeu na mao qual scancode e cada tecla do controle: o Back
      // e 482, as coloridas sao 486-489 (RED, GREEN, YELLOW, BLUE no
      // SDL_webOS.h). Descobrir isso exigiu ler o cabecalho do SDK, e as teclas
      // que o firmware entrega DE FATO ainda nao estao provadas no aparelho —
      // o Back precisou de um hint proprio para nao ser engolido pelo
      // compositor e nao existe hint equivalente para as coloridas.
      //
      // Com esta linha, apertar uma tecla e ler o log responde a pergunta, em
      // vez de exigir uma build instrumentada de proposito. Cada scancode sai
      // UMA vez por sessao: um controle de TV repete a tecla sozinho e um log
      // por evento afogaria o resto.
      if (e.type == SDL_KEYDOWN) abertura_tecla();   // a pessoa quer entrar: a marca sai
      if (e.type == SDL_KEYDOWN) {
        static unsigned char visto[512];
        SDL_Scancode sc = e.key.keysym.scancode;
        if (sc < 512 && !visto[sc]) {
          visto[sc] = 1;
          printf("[tecla] scancode=%d sym=%d (%s)\n", (int)sc,
                 (int)e.key.keysym.sym, SDL_GetKeyName(e.key.keysym.sym));
          fflush(stdout);
        }
      }
      if (e.type == SDL_KEYDOWN && e.key.keysym.scancode == NV_SCANCODE_BACK) {
        SDL_Event back; SDL_zero(back);
        back.type = SDL_KEYDOWN;
        back.key.keysym.sym = SDLK_AC_BACK;
        app_evento(&back);
        continue;
      }
      app_evento(&e);
    }
    central_tecla_quadro(SDL_GetTicks(), entregarCh);
    teclasInjetadas(app_evento);
    texto_sistema_quadro();
    fEv = NV_DT(tEv);

    Uint32 agora = SDL_GetTicks();
    // dt VEM DO RELOGIO DE ALTA RESOLUCAO, nao de SDL_GetTicks.
    //
    // SDL_GetTicks conta em MILISSEGUNDOS INTEIROS. No Mac o app roda sem vsync
    // a ~1300 fps, entao quase todo quadro dura menos de 1 ms e a subtracao dava
    // ZERO — e o piso `if (dt <= 0) dt = 1/60` entregava 16,7 ms SINTETICOS para
    // um quadro de 0,8 ms de relogio real. Toda animacao avancava ~20x mais
    // rapido que o relogio: medido, um fade de 330 ms terminava em 92 ms.
    //
    // Na TV o vsync escondia o defeito (dt real, sempre >= 20 ms), mas o efeito
    // pratico era pior que um bug de Mac: QUALQUER calibracao de animacao feita
    // na previa perseguia um numero que a TV nunca ia reproduzir, e a medida de
    // pior quadro no Mac tambem saia distorcida.
    //
    // O clamp continua, mas so como TETO: voltar de suspensao entrega um dt de
    // varios segundos e uma animacao daria um salto. Piso nao existe mais —
    // quadro curto tem de ser um dt curto.
    Uint64 cQuadro = SDL_GetPerformanceCounter();
    double dtms = (double)(cQuadro - ultQuadro) * 1000.0 / perFreq;
    ultQuadro = cQuadro;
    if (dtms < 0.0) dtms = 0.0;
    // O TETO E DA ANIMACAO, NAO DA MEDICAO, e confundir os dois cegou o
    // diagnostico do issue #33. `dtms` era grampeado em 100 ms ANTES de virar
    // `pior`, entao todo quadro de 100 ms para cima virava exatamente
    // "pior=100.0ms". O log do relator tinha essa linha tres vezes e ela nao
    // queria dizer "cem milissegundos": queria dizer "cem ou mais, nao sei
    // quanto". Com FPS=9.6 na mesma janela — 104 ms de media — havia quadro
    // muito acima disso, e o numero que existia para revelar isso escondia.
    //
    // O teto continua onde sempre precisou estar: voltar de suspensao entrega
    // um dt de varios segundos e a animacao daria um salto.
    double dtAnim = dtms > 100.0 ? 100.0 : dtms;
    float dt = (float)(dtAnim / 1000.0);
    // Quadro de mais de 1 s entra no pior MESMO nos primeiros 20 da janela:
    // com FPS de 0,1 a janela inteira tem 1 quadro, e o pior saia 0.0.
    if (quadros > 20 || dtms > 1000.0) {
      if (dtms > pior) { pior = dtms; piorTxtMs = txtMsQuadro; piorTxtN = txtNQuadro;
                         pEv=fEv; pBomb=fBomb; pUpd=fUpd; pDes=fDes; pSwap=fSwap; pAux=fAux; pClr=fClr; pPrep=fPrep; pGlClr=fGlClr;
                         pUplN=fUplN; pUplB=fUplB;
                         pGfxMs=fGfxMs; pTexMs=fTexMs; pNRect=fNRect; pNProg=fNProg;
                         pNBind=fNBind; pNBusca=fNBusca; pOutMs=fOutMs; pNOut=fNOut; pFill=fFill; pNCheio=fNCheio; }
      if (dtms > 33.0) janks++;
    }
    // RASTRO POR QUADRO, so com /tmp/nuvio-quadros presente (conferido no
    // relatorio de 3 s): cada quadro acima de 25 ms sai com a reparticao, os
    // uploads e o texto rasterizado. O [quadro] de 3 s mostra UM pior por
    // janela; para achar o que causa cada tranco da navegacao e preciso ver
    // todos, na ordem, ao lado das teclas.
    if (rastroQuadros && dtms > 25.0)
      printf("[qd] %.1fms ev=%.1f bomb=%.1f(%d tex %.1fMB) upd=%.1f clr=%.1f des=%.1f aux=%.1f swap=%.1f"
             " [prep=%.1f glclear=%.1f] txt=%.1fms/%d rects=%d fill=%.2f assados=%d gpu~=%.1f %s\n", dtms, fEv, fBomb, fUplN, fUplB / 1048576.0,
             fUpd, fClr, fDes, fAux, fSwap, fPrep, fGlClr, txtMsQuadro, txtNQuadro, fNRect, fFill, gfx_n_assados, gputempo_ultimo(), home_rastro_foco());
    if (rastroQuadros && dtms > 25.0) {
      int k; printf("[qd-fill]");
      for (k = 0; k < GFX_NMODOS; k++) if (gfx_fill_modo[k] > 0.02) printf(" %d=%.2f", k, gfx_fill_modo[k]);
      printf("\n");
    }
#if defined(NV_TPK) || defined(NV_ANDROID) || defined(NV_WEBOS)
    // NIVEL DE GPU ADAPTATIVO (gpunivel.h): o quadro que acabou, repartido em
    // ESPERA (clr + swap: o driver devolvendo buffer, a GPU atrasada) e CPU.
    // "Cheia" = artes na tela (o conjunto quente do cache), conferido a cada
    // meio segundo: a home vazia roda a 60 e nao diz nada sobre a GPU.
    { static Uint32 cheiaEm; static int cheia;
      if (agora - cheiaEm >= 500u) {
        int it = 0, pe = 0, qu = 0; long b = 0, bq = 0;
        tex_estatisticas(&it, &pe, &b, &qu, &bq);
        cheia = qu >= 8; cheiaEm = agora; }
      gpun_medir(dtms, fClr + fSwap, fEv + fBomb + fUpd + fDes + fAux, app_na_home(), cheia); }
#endif
    // zera os contadores do quadro que comeca agora; o que foi medido acima
    // pertence ao quadro anterior, que e o que acabou de custar dtms
    txtMsQuadro = txt_ms; txtNQuadro = txt_rasterizadas;
    txt_ms = 0.0; txt_rasterizadas = 0;

    // TRES por quadro. O limite de 1 vinha de quando TODA arte era decodificada
    // com o teto unico de 640: cada glTexImage2D custava ~2 MB e dois no mesmo
    // quadro passavam de 20 ms, aparecendo como tranco ao entrar numa fileira.
    //
    // Esse argumento caiu junto com o teto unico: agora cada arte e decodificada
    // pela largura com que e desenhada (tex_obter_larg), e na TV um poster sai a
    // ~500 KB em vez de 2,4 MB. Tres envios pequenos somam menos que o UNICO
    // envio grande de antes, e a fileira que entra na tela deixa de aparecer aos
    // pedacos.
    Uint64 t0 = NV_T0();
    tex_upl_n = 0; tex_upl_bytes = 0;
    gputempo_quadro_inicio();   // the GPU clock brackets uploads + draw
    tex_bombear(3);
    fBomb = NV_DT(t0);
    fUplN = tex_upl_n; fUplB = tex_upl_bytes;
    t0 = NV_T0();
    app_atualizar(dt, agora);
    // COR VIVA: UMA vez por quadro, antes do desenho. Consome o pedido que o
    // desenho do quadro anterior fez (corviva_definir) e anda a transicao; o
    // desenho deste quadro so le o resultado (ajustes_acento, NV_COR_FUNDO_*).
    // PROTECAO DE OLED: "parado" = nenhuma tecla E nenhum video tocando de
    // verdade (pausado conta como parado).
    esmaecer_escolha(ajustes_esmaecer());
    // Com o player aberto (filme pausado) fica so o escurecer de antes: a
    // vitrine pediria texturas de tela cheia com o decodificador ocupado.
    esmaecer_estilo(player_aberto() ? ESM_ESTILO_ESCURECER : ajustes_descanso_estilo());
    esmaecer_quadro(agora, dt,
                    (player_aberto() || player_mini_ativo()) && player_com_video() &&
                    !player_pausado() && !player_carregando());
    // TELA DE DESCANSO: vitrine/relogio por cima do preto (descanso.h).
    descanso_quadro(agora, dt, esmaecer_descanso(),
                    ajustes_descanso_estilo(), ajustes_descanso_fonte());
    { int escuro;
      // Escuro e sem filme: solta o "manter tela ligada" (so o Android segura
      // fora do player; na LG o app ja o libera sem filme, ver video.c).
      if (esmaecer_mudou_escuro(&escuro)) {
#ifdef NV_ANDROID
        if (escuro) SDL_EnableScreenSaver(); else SDL_DisableScreenSaver();
#endif
        printf("[esmaecer] %s\n", escuro ? "tela quase apagada (estagio 2)" : "acordou");
        fflush(stdout);
      } }
    ajustes_log_vazar();
    ajustes_idioma_auto_tick();   // locale da TV (webOS): chega de um fio
    corviva_quadro(dt, ajustes_cor_viva(), ajustes_cor_logo(),
                   ajustes_animacoes_reduzidas());
    ajustes_textura_quadro();   // Textura: a do titulo em cena vai ao gfx
    fUpd = NV_DT(t0);

    // RECORTE DESLIGADO ANTES DO CLEAR. glClear respeita o scissor test: se
    // qualquer tela terminar o quadro com um recorte ativo, o clear seguinte
    // limpa SO aquele retangulo e o resto da tela guarda o quadro anterior.
    // Hoje todos os chamadores equilibram recorte/sem_recorte, mas isso e uma
    // invariante que ninguem verifica — e o sintoma seria justamente uma faixa
    // com conteudo velho, dificil de atribuir a causa. Uma chamada por quadro.
    t0 = NV_T0();
    gfx_novo_quadro();
    // Amostra: os desenhos grandes de UM quadro a cada 30 (meio segundo).
    gfx_rastro_grandes = rastroQuadros && (quadros % 30) == 0;
    if (gfx_rastro_grandes) printf("[qd-rect] --- quadro\n");
    tex_novo_quadro();
    gfx_sem_recorte();
    fPrep = NV_DT(t0);
    gfx_ambiente_preparar();
    fundo_fosco_quadro();   // vidro fosco: a arte borrada do titulo em cena
    fPrep = NV_DT(t0) - fPrep;
    // Nivel 3 (so com "720p" escolhido) ou 4K que nao aguentou: o quadro
    // inteiro vai para o alvo interno (o clear abaixo ja limpa ele);
    // gpun_quadro_fim amplia para a janela.
    // Mudou "Efeitos visuais" nos Ajustes: aplica no proximo quadro.
    if (ajustes_gpu_efeitos() != gpuPref) { gpuPref = ajustes_gpu_efeitos(); gpun_preferencia(gpuPref); }
#ifdef NV_TPK
    video_tpk_zoom_roi_definir(ajustes_trailer_zoom_tpk());   // #241: Ajustes > Trailers
#endif
    gpun_quadro_inicio();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    if (!nvPrimeiroQuadroFeito) arranque_etapa("primeiro glClear");
    fGlClr = NV_DT(t0);
    glClear(GL_COLOR_BUFFER_BIT);
    fGlClr = NV_DT(t0) - fGlClr;
    // "Dinâmica imersiva": a luz da arte POR BAIXO de toda tela, logo depois do
    // clear — e o que as rampas do destaque e do detalhe deixam aparecer quando
    // se apagam em alfa (uVaza). Uma passada de tela cheia com 4 luzes; nos
    // outros temas a forca e 0 e isto nao desenha nada.
    gfx_ambiente(1.0f);
    fClr = NV_DT(t0);
    t0 = NV_T0();
    txt_novo_quadro();
    ponteiro_quadro(agora);
    app_desenhar(agora);
    // A abertura (#213) cobre os primeiros quadros, com a home montando por baixo.
    if (abertura_ativa()) {
      int pend = 0;
      tex_estatisticas(NULL, &pend, NULL, NULL, NULL);
      abertura_fundo_fica(app_no_login());
      abertura_desenhar(agora, dt, pend);
    }
    esmaecer_desenhar(agora);   // o veu da protecao de OLED, UMA passada no fim do quadro
    ponteiro_desenhar();
    // GIF QUE NINGUEM DESENHOU ha 1,5 s sai da memoria (tela de perfis
    // fechada, foco fora do cartaz). Ver gif_ocioso em gif.h.
    gif_ocioso();
    gfx_ambiente_descarregar();   // quadro sem desenho por cima: a luz ainda sai
    gpun_quadro_fim();
    fDes = NV_DT(t0);
    fGfxMs = gfx_ms_rect; fTexMs = tex_ms_busca;
    fNRect = gfx_n_rect; fNProg = gfx_n_prog; fNBind = gfx_n_bind; fNBusca = tex_n_busca;
    fOutMs = gfx_ms_outros; fNOut = gfx_n_outros;
    fFill = gfx_fill; fNCheio = gfx_n_cheio;
    // Media e pico do preenchimento real na janela de 3 s ([gpu-modos] fill).
    gpuFillSoma += gfx_fill_gpu; gpuFillN++;
    if (gfx_fill_gpu > gpuFillPico) gpuFillPico = gfx_fill_gpu;
    t0 = NV_T0();
    videoSeSolicitado();
    capturaSeSolicitado();
    fAux = NV_DT(t0);
    gputempo_quadro_fim();
    t0 = NV_T0();
    if (!nvPrimeiroQuadroFeito) arranque_etapa("primeiro SwapWindow");
    SDL_GL_SwapWindow(win);
#ifdef NV_ANDROID
    android_quadro();   // todo quadro apresentado, de qualquer tela (vigia #266)
#endif
#ifdef __EMSCRIPTEN__
    { Uint64 c0 = SDL_GetPerformanceCounter();
      double c = (double)(c0 - fimCeder) * 1000.0 / perFreq, fora;
      if (c > cMaxMs) cMaxMs = c;
      nv_ceder_quadro();
      fimCeder = SDL_GetPerformanceCounter();
      fora = (double)(fimCeder - c0) * 1000.0 / perFreq;
      if (fora > foraMaxMs) foraMaxMs = fora; }
    // Oferece ao IDBFS a chance de descarregar. Quase todo quadro isso e a
    // leitura de duas bandeiras e um return: quem decide SE e quando descarregar
    // e a politica em dados.c, porque descarregar a cada escrita era o que
    // produzia os picos de 100 ms.
    dados_sincronizar();
#endif
    fSwap = NV_DT(t0);
    // PRIMEIRO PIXEL. E o numero que responde "quanto tempo ate a TV mostrar
    // alguma coisa", que nenhuma metrica de quadro dava.
    //
    // Bandeira PROPRIA e nao `if (!quadros)`: `quadros` zera a cada relatorio
    // de 3 s, entao aquilo carimbaria "primeiro quadro" tres vezes por minuto.
    { static int jaCarimbou;
      if (!jaCarimbou) { jaCarimbou = 1; nvPrimeiroQuadroFeito = 1; arranque_etapa("quadro-1"); marco("primeiro quadro na tela");
        NV_ETAPA("pronto");
#ifdef __EMSCRIPTEN__
        // Chegou: zera o contador de arranques falhados (tizen-shell.html).
        EM_ASM({ try { localStorage.setItem('nv-boot-falhas', '0'); } catch (e) {} });
#endif
      } }
    quadros++;

    if (agora - ultRelato >= 3000) {
      int itens, pend, quentes; long bytes, bytesQ;
      char memoria[96];
      memlog_amostra(memoria, sizeof memoria);
      NvCacheArteStats cacheArteStats;
      tex_estatisticas(&itens, &pend, &bytes, &quentes, &bytesQ);
      memset(&cacheArteStats, 0, sizeof cacheArteStats);
      cachearte_estatisticas_pedir();
      cachearte_estatisticas(&cacheArteStats);
      // `idbfs=N/X.Xms` e a descarga para o IndexedDB: quantas e o custo SINCRONO
      // da pior. Sem estes dois numeros nao ha como distinguir "o pico sumiu" de
      // "o pico mudou de fase" — foi essa descarga que produziu os 100 ms.
      //
      // `tela=Q/XMB` e o conjunto QUENTE do cache de texturas — o que foi
      // desenhado nos dois ultimos quadros — e `tex-despejos=N(q=M)` quantas
      // texturas o cache jogou fora, e quantas dessas estavam na tela. Os dois
      // numeros faltavam: `despejos=` sempre foi o cache de TEXTO, e o log da
      // LG de 16/09 mostrava o cache de texturas encostado em 95.9 de 96 MB sem
      // dizer se ele girava. Se `tela` passa do orcamento, nenhum ajuste de
      // fila resolve — e o teto.
      // Vigia do locale (ver o setlocale no comeco de main): se o host trocou
      // o numerico depois, o JSON com decimal voltaria a sair com virgula.
      { struct lconv *lc = localeconv();
        if (lc && lc->decimal_point && strcmp(lc->decimal_point, ".")) {
          printf("[locale] numerico trocado por fora (\"%s\"): de volta ao C\n", lc->decimal_point);
          setlocale(LC_NUMERIC, "C");
        } }
      printf("FPS=%.1f pior=%.1fms janks=%d | pior-quadro: texto %.1fms em %d linhas"
             " | gpu-cache=%d %.1fMB tela=%d/%.1fMB fila-tex=%d tex-despejos=%d(q=%d) disco-direto=%d neg-arte=%d"
             " | despejos=%d | cache-arte=%ld/%ldB hit=%ld miss=%ld grav=%ld err=%ld essenciais=%ld/%ld"
             " | fs-backend=%s idbfs=%s sync=%s ok=%d err=%d pend=%d custo=%d/%.1fms recovery=%d"
             " | cache-disco=%.1fMB | rss=%.0fMB%s%s\n",
             quadros * 1000.0 / (double)(agora - ultRelato), pior, janks,
             piorTxtMs, piorTxtN, itens, bytes / 1048576.0,
             quentes, bytesQ / 1048576.0, pend, tex_despejos, tex_despejos_quentes,
             tex_disco_direto, tex_negativas_poupadas(),
             txt_despejos,
             cacheArteStats.itens, cacheArteStats.bytes,
             cacheArteStats.hits, cacheArteStats.misses, cacheArteStats.gravacoes, cacheArteStats.falhas,
             cacheArteStats.essenciais, cacheArteStats.essenciais_esperados,
             dados_persistente() ? "on" : "off",
#ifdef __EMSCRIPTEN__
             dados_persistente() ? "mounted" : "unmounted",
#else
             "native",
#endif
             dados_sync_pendente() ? "pending" : (dados_sync_em_recuo() ? "retry" : "idle"),
             dados_sync_sucessos, dados_sync_falhas, dados_sync_pendente(),
             dados_desc_n, dados_desc_ms, dados_modo_recuperacao(),
             tex_cache_disco_bytes() / 1048576.0,
             rssMB(),
             memoria,
             dados_persistente() ? "" : "  <<< SEM PERSISTENCIA");
      // O medidor de desempenho na tela (desempenho.h) le os mesmos numeros.
      desempenho_amostra((float)(quadros * 1000.0 / (double)(agora - ultRelato)), (float)pior, janks,
                         quentes, (float)(bytesQ / 1048576.0), pend, tex_despejos, tex_despejos_quentes,
                         (float)rssMB());
      avisos_sinal(NULL, (float)rssMB());   // batida: no maximo 1 a cada 60 s
      seguro_batida(SDL_GetTicks() / 1000); // confirma mudancas arriscadas depois de 3 min
      corviva_gravar_se_preciso(0);          // corviva.txt: no maximo 1 a cada 20 s
      dados_sync_sucessos = 0;
      dados_sync_falhas = 0;
#ifdef __EMSCRIPTEN__
      // Heap linear, nao RAM total do processo: GPU e memoria JS ficam fora.
      // uordblks inclui pilhas dos pthreads e dados alocados pelo malloc.
      { struct mallinfo mi = mallinfo();
        printf("[mem] WASM=%.1f MiB malloc=%.1f MiB livre-no-heap=%.1f MiB\n",
               emscripten_get_heap_size() / 1048576.0,
               mi.uordblks / 1048576.0, mi.fordblks / 1048576.0); }
      // O NAVEGADOR, visto de fora do WASM (20/09/2026, #72, registros 10 e
      // 11): `swap` de 3-11 s com pend=0 e `[hero] app parado` por 27 s
      // dizem que o fio principal ficou fora do nosso codigo — mas nao se
      // foi o runtime da TV que parou ou se e algo que ainda fazemos. Isto
      // separa: heap JS (performance.memory), o maior buraco entre dois rAF
      // medidos por um laco JS proprio (tizen-shell.html), quantos passaram
      // de 500 ms, e se a pagina esteve escondida.
      { int jsMB = 0, jsLimMB = 0, rafMax = 0, rafLentos = 0, escondida = 0;
        jsMB = EM_ASM_INT({ try { return (performance.memory.usedJSHeapSize / 1048576) | 0; } catch (e) { return -1; } });
        jsLimMB = EM_ASM_INT({ try { return (performance.memory.jsHeapSizeLimit / 1048576) | 0; } catch (e) { return -1; } });
        rafMax = EM_ASM_INT({ var r = window.__nvRaf; if (!r) return -1; var m = r.max | 0; r.max = 0; return m; });
        rafLentos = EM_ASM_INT({ var r = window.__nvRaf; if (!r) return -1; var n = r.lentos | 0; r.lentos = 0; return n; });
        escondida = EM_ASM_INT({ return document.hidden ? 1 : 0; });
        { static char quem[64];
          int longMax, longN, longSoma;
          longMax  = EM_ASM_INT({ var L = window.__nvLong; return L ? (L.max | 0) : -1; });
          longN    = EM_ASM_INT({ var L = window.__nvLong; return L ? (L.n | 0) : -1; });
          longSoma = EM_ASM_INT({ var L = window.__nvLong; return L ? (L.soma | 0) : -1; });
          EM_ASM({ var L = window.__nvLong; if (!L) return;
                   var q = (L.quem || "-") + "@" + (L.fase || "?"); var i = 0;
                   for (; i < q.length && i < 62; i++) HEAPU8[$0 + i] = q.charCodeAt(i) & 127;
                   HEAPU8[$0 + i] = 0;
                   L.max = 0; L.n = 0; L.soma = 0; L.quem = ""; L.fase = ""; }, quem);
          printf("[navegador] js=%d/%d MiB raf-max=%d ms raf-lentos=%d escondida=%d"
                 " longtask-max=%d ms n=%d soma=%d ms quem=%s c-max=%d ms fora-max=%d ms\n",
                 jsMB, jsLimMB, rafMax, rafLentos, escondida, longMax, longN, longSoma, quem,
                 (int)cMaxMs, (int)foraMaxMs);
          cMaxMs = 0; foraMaxMs = 0; } }
#endif
      // A REPARTICAO DO PIOR QUADRO, NA TELA E NAO SO NO ARQUIVO.
      //
      // Ela ja existia, mas so em /tmp/nuvio-fps.txt — e quem relata desempenho
      // no Samsung ve o painel de log do proprio app, nunca esse arquivo. O
      // resultado foi o #33: tres linhas de "pior=100.0ms" sem NADA que
      // dissesse em que fase o tempo foi gasto, e duas causas candidatas com
      // conserto diferente. So sai quando o quadro passou de jank, para nao
      // dobrar o log em uso normal.
      //
      // `bomb` e a subida de textura para a GPU; `upl` diz se foi UMA arte
      // grande ou muitas pequenas, que e a diferenca entre partir o upload e
      // reduzir o orcamento.
      // GPU time of the window, by the GPU's own clock (gputempo.h). Only where
      // the extension exists; the line is what tells a 17 ms frame from a 30 ms
      // one when both show as "33" to the CPU.
      { double gMed = 0, gPior, gUlt, gP90 = gputempo_p90(); int gN = gputempo_colher(&gMed, &gPior, &gUlt);
        if (gN > 0) printf("[gpu-tempo] med=%.1fms pior=%.1fms ult=%.1fms n=%d\n", gMed, gPior, gUlt, gN);
        // AUTOMATIC 4K (resolucao.h): probe, then watch. Same validity rule as below.
        if (autoPediu) {
          static ResAuto ra; static int iniciou, avisouSem;
          if (!iniciou) { iniciou = 1; ra.estado = autoEst == RES_AUTO_4K ? RES_AUTO_4K : RES_AUTO_SONDAR; }
          if (dw <= (int)NV_TELA_W) {
            if (!avisouSem) {
              avisouSem = 1;
              printf("[4k] automatico: 1080p (a TV nao concedeu 4K)\n");
              dados_gravar(RES_ARQ_AUTO, "1080 " NV_VERSAO "\n");
            }
          } else if (!gpun_alvo_1080_ativo() && ra.estado != RES_AUTO_1080) {
            double fpsJan = quadros * 1000.0 / (double)(agora - ultRelato);
            int valida = SDL_GetTicks() > 8000u && !abertura_ativa() &&
                         !player_aberto() && !player_mini_ativo();
            double p90 = gN > 0 ? gP90 : 0.0;
            int r = res_auto_amostra(&ra, fpsJan, p90, valida);
            if (r == RES_AUTO_APROVOU) {
              printf("[4k] automatico: 4K (gpu p90=%.1f ms) fps=%.1f\n", ra.pior, fpsJan);
              dados_gravar(RES_ARQ_AUTO, "4k " NV_VERSAO "\n");
            } else if (r == RES_AUTO_REBAIXOU) {
              printf("[4k] automatico: 1080p (gpu p90=%.1f ms) fps=%.1f\n", p90, fpsJan);
              gpun_alvo_1080();
              dados_gravar(RES_ARQ_AUTO, "1080 " NV_VERSAO "\n");
              if (autoEst == RES_AUTO_4K)
                ilha_avisar("res-4k-recuo", ILHA_INFO, NULL,
                            i18n("Interface em 1080p: esta TV não aguenta 4K"), 8000u, 0);
            }
          }
        }
        // 4K WATCH (resolucao.h): the person picked 4K and the TV granted it.
        // Only the interface counts: no player, no opening, 10 s of warm-up.
        if (pediu4k && dw > (int)NV_TELA_W && !gpun_alvo_1080_ativo()) {
          static ResVigia vigia4k;
          double fpsJan = quadros * 1000.0 / (double)(agora - ultRelato);
          int valida = SDL_GetTicks() > 10000u && !abertura_ativa() &&
                       !player_aberto() && !player_mini_ativo();
          if (res_vigia_amostra(&vigia4k, fpsJan, gN > 0 ? gMed : 0.0, valida)) {
            printf("[4k] recuo: gpu=%.1fms fps=%.1f em %d relatorios seguidos -> 1080p nesta "
                   "sessao e nas proximas\n", gN > 0 ? gMed : 0.0, fpsJan, RES_4K_SEGUIDAS);
            gpun_alvo_1080();
            dados_gravar(RES_ARQ_RECUO, "1\n");
            ilha_avisar("res-4k-recuo", ILHA_INFO, NULL,
                        i18n("Interface em 1080p: esta TV não aguenta 4K"), 8000u, 0);
          }
        } }
      if (pior > 33.0) {
        printf("[quadro] pior=%.1fms | ev=%.1f bomb=%.1f(%d tex, %.1fMB)"
               " upd=%.1f clr=%.1f des=%.1f aux=%.1f swap=%.1f\n",
               pior, pEv, pBomb, pUplN, pUplB / 1048576.0,
               pUpd, pClr, pDes, pAux, pSwap);
        // 2.0.1: `clr` de 6 e 14 s no Android ao abrir video (Expressluck, TCL
        // A14) e o numero junta seis passos. Aberto em partes so quando pesa:
        // prep = luz ambiente + vidro fosco, gl = o glClear (primeira chamada
        // GL depois do swap: e onde o driver espera o buffer da janela).
        if (pClr > 100.0)
          printf("[quadro-clr] clr=%.1f prep=%.1f gl=%.1f resto=%.1f\n",
                 pClr, pPrep, pGlClr, pClr - pPrep - pGlClr);
        // O `des` DO PIOR QUADRO REPARTIDO, NO LOG. Ate aqui so ia para
        // /tmp/nuvio-fps.txt, que no Android nao existe: a TCL do dono
        // (04/10/2026) mostrava `des=30..45 ms` com texto 0 e upload 0, e nada
        // dizia se era CPU no gfx_rect, busca de textura, FBO/desfoque (`out`)
        // ou preenchimento. Mesma condicao do [quadro]: so com jank.
        // (os relogios de gfx_rect e da busca de textura so existem com
        // -DNV_PERF_FINO; as contagens e o `out` existem sempre)
        printf("[quadro-des] rects=%d(p%d,b%d) out=%.1fms/%d fill=%.2fx cheias=%d texto=%.1fms/%d\n",
               pNRect, pNProg, pNBind, pOutMs, pNOut, pFill, pNCheio, piorTxtMs, piorTxtN);
      }
      // Instrumento de campo (gfx.h, gfx_modos_desligados): a lista em
      // /tmp/nuvio-gfx-off desliga modos de desenho; e o preenchimento do
      // ultimo quadro por modo, para saber quem pesa sem recompilar.
      { FILE *fq = fopen("/tmp/nuvio-quadros", "r"); rastroQuadros = fq != NULL; if (fq) fclose(fq); }
      { FILE *fo = fopen("/tmp/nuvio-gfx-off", "r");
        unsigned long long m = 0; int n;
        if (fo) { while (fscanf(fo, "%d", &n) == 1) if (n >= 0 && n < GFX_NMODOS) m |= 1ull << n; fclose(fo); }
#ifdef NV_ANDROID
        // No Android nao ha /tmp: `adb shell setprop debug.nuvio.gfxoff "33 1"`
        // faz o mesmo (lista de modos, separados por espaco; "" religa).
        { char pv[PROP_VALUE_MAX] = "";
          if (__system_property_get("debug.nuvio.gfxoff", pv) > 0) {
            const char *q = pv;
            while (*q) { char *fim; long v = strtol(q, &fim, 10); if (fim == q) break;
                         if (v >= 0 && v < GFX_NMODOS) m |= 1ull << v; q = fim; }
          } }
#endif
        if (m != gfx_modos_desligados) { printf("[gpu-modos] desligados=%llx\n", m); gfx_modos_desligados = m; }
        // Field instruments on Android, where there is no /tmp (all read once per
        // 3 s report, all no-ops when the property is unset or "0"):
        //   debug.nuvio.quadros 1  -> per-frame trace, same as /tmp/nuvio-quadros
        //   debug.nuvio.fill 1     -> the "[gpu-modos] fill" line every report
        //   debug.nuvio.gpunivel N -> force GPU level N (0-3) for A/B measurement
        //   debug.nuvio.gputempo 1 -> GPU timer per frame (read at startup, gputempo.c)
        int forcaFill = 0;
#ifdef NV_ANDROID
        { char pv[PROP_VALUE_MAX] = "";
          if (__system_property_get("debug.nuvio.quadros", pv) > 0 && pv[0] && pv[0] != '0') rastroQuadros = 1;
          pv[0] = 0;
          forcaFill = __system_property_get("debug.nuvio.fill", pv) > 0 && pv[0] && pv[0] != '0';
          pv[0] = 0;
          if (__system_property_get("debug.nuvio.gpunivel", pv) > 0 && pv[0] >= '0' && pv[0] <= '3' &&
              pv[0] - '0' != gpun_nivel())
            gpun_definir_nivel(pv[0] - '0'); }
#endif
        // NO CAMPO, SEM ARQUIVO: interface lenta (FPS < 50 com mais de 9
        // texturas na tela) solta a mesma linha, no maximo uma vez a cada 30 s,
        // com layout, tema e vidro. Registros 10063-10235 (LG webOS 5): 60 fps
        // na 1.5.4 e 41 na 1.6.0 com a mesma configuracao, e o log nao dizia
        // que desenho pesava.
        static Uint32 ultModos;
        double fpsAgora = quadros * 1000.0 / (double)(agora - ultRelato + 1);
        // 45 e nao 50: TV Samsung roda a 50 Hz e o .tpk marca 49,9 fps em
        // repouso; com 50 a linha saia para 12 de 12 pessoas na 1.6.1 e
        // afogava os casos lentos de verdade.
        int lenta = fpsAgora < 45.0 && quentes >= 10 && (Uint32)(agora - ultModos) >= 30000u;
        // UMA LINHA POR SESSAO (e outra se a pessoa mudar algo): sem ela o vidro
        // so aparece nas sessoes lentas, e nao ha como saber quantas sessoes com
        // vidro ligado andam bem. Com ela a proporcao tem denominador.
        { static int sLay = -1, sCor = -1, sVid = -1;
          int lay = ajustes_home_layout(), cor = ajustes_cor_viva(), vid = ajustes_vidro();
          if (lay != sLay || cor != sCor || vid != sVid) {
            sLay = lay; sCor = cor; sVid = vid;
            printf("[gpu-modos] sessao: layout=%d cor-viva=%d vidro=%d\n", lay, cor, vid);
          } }
        if (lenta) { ultModos = agora;
          printf("[gpu-modos] lento: fps=%.1f layout=%d cor-viva=%d vidro=%d tela=%s\n",
                 fpsAgora, ajustes_home_layout(), ajustes_cor_viva(), ajustes_vidro(),
                 app_tela_nome()); }
        if (fo || lenta || forcaFill || getenv("NUVIO_FILL_MODOS")) {
          int k; printf("[gpu-modos] fill: forca=%.2f |", nv_ambiente_forca);
          for (k = 0; k < GFX_NMODOS; k++) if (gfx_fill_modo_ult[k] > 0.02) printf(" %d=%.2f", k, gfx_fill_modo_ult[k]);
          // gpu = o que a GPU pinta de fato (gfx.h, gfx_fill_gpu); mist = a parte com mistura
          printf(" | gpu=%.2f mist=%.2f med=%.2f pico=%.2f\n", gfx_fill_gpu_ult, gfx_fill_gpu_mist_ult,
                 gpuFillN ? gpuFillSoma / gpuFillN : 0.0, gpuFillPico);
        }
        gpuFillSoma = 0.0; gpuFillPico = 0.0; gpuFillN = 0; }
#ifdef NV_TPK
      // Quanto de tela o pior quadro pintou (gfx_fill, em telas 1920x1080) e
      // em que nivel de GPU (gpunivel.h): e o que separa "a GPU nao da conta
      // deste quadro" de "este quadro pinta mais que o normal".
      printf("[gpu] nivel=%d fill-pior=%.2fx cheias=%d rects=%d\n",
             gpun_nivel(), pFill, pNCheio, pNRect);
#endif
      fflush(stdout);
      // A MESMA linha vai para um arquivo. No aparelho a saida padrao do app
      // lancado pelo applicationManager nao chega a lugar nenhum que se possa
      // ler, e rodar o binario a mao nao funciona (sem a identidade do app o
      // compositor recusa a superficie e ele morre em silencio). Sem isto nao
      // ha como MEDIR quadro no aparelho — so olhar e achar.
      { FILE *fp = fopen("/tmp/nuvio-fps.txt", "w");
        if (fp) {
          fprintf(fp, "drawable=%dx%d FPS=%.1f pior=%.1fms janks=%d"
                  " texto=%.1fms/%d texturas=%d %.1fMB"
                  " | pior: ev=%.1f bomb=%.1f upd=%.1f clr=%.1f des=%.1f aux=%.1f swap=%.1f"
                  " | des: gfx=%.1f/%d(p%d,b%d) tex=%.2f/%d out=%.1f/%d fill=%.2fx(cheias=%d)"
                  " | despejos=%d\n",
                  dw, dh,
                  quadros * 1000.0 / (double)(agora - ultRelato), pior, janks,
                  piorTxtMs, piorTxtN, itens, bytes / 1048576.0,
                  pEv, pBomb, pUpd, pClr, pDes, pAux, pSwap,
                  pGfxMs, pNRect, pNProg, pNBind, pTexMs, pNBusca, pOutMs, pNOut, pFill, pNCheio,
                  txt_despejos);
          fclose(fp);
        } }
      quadros = 0; ultRelato = agora; pior = 0; janks = 0; piorTxtMs = 0; piorTxtN = 0;
      txt_despejos = 0;
      tex_despejos = 0; tex_despejos_quentes = 0;
      dados_desc_zerar();
      pEv=pBomb=pUpd=pDes=pSwap=pAux=pClr=0;
      pUplN=0; pUplB=0;
      pGfxMs=pTexMs=pOutMs=0; pNRect=pNProg=pNBind=pNBusca=pNOut=0; pFill=0; pNCheio=0;
    }
  }

#ifdef NV_SINAL_TERMINAR
  if (sinalTerminou) alarm(NV_SAIDA_FOLGA_S);   // saida normal em curso: mais folga que os 4 s do laco
#endif
  gfx_borrao_encerrar();
  gfx_snap_encerrar();
  app_encerrar();
  // Motor P2P embutido: cancela e espera o destroy por ate P2PM_SAIDA_MS; o
  // cache so e apagado depois do destroy (ou no proximo inicio). No-op sem motor.
  p2pmotor_saida();
  ajustes_log_vazar_tudo();
  corviva_gravar_se_preciso(1);
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
#ifdef NV_SINAL_TERMINAR
  alarm(0);   // a saida normal terminou: nada de SIGALRM depois dela
#endif
#ifdef __EMSCRIPTEN__
  // O SDL_Quit apaga TODOS os hints (SDL_ClearHints, SDL.c do port 2.32.10),
  // inclusive o de ASYNCIFY acima — e os fios de trabalho continuam vivos
  // ate a pagina fechar. O primeiro SDL_Delay de um deles depois disto via
  // emscripten_sleep e abortava: "Aborted(invalid state: 1)" logo depois do
  // "fim" em todo log de saida da Samsung 1.5.0. Repor o hint aqui fecha isso.
  SDL_SetHint(SDL_HINT_EMSCRIPTEN_ASYNCIFY, "0");
#endif
  printf("fim\n");
#ifdef __EMSCRIPTEN__
  // SAIR DE VERDADE NO TIZEN. Aqui o laco de quadro acabou e o main devolve,
  // mas com EXIT_RUNTIME=0 a pagina CONTINUA no ar — com o canvas parado no
  // ultimo quadro e nada respondendo. Do sofa isso e indistinguivel de um
  // travamento, e era literalmente o relato: "no Tizen, Voltar na home trava;
  // na LG fecha o aplicativo".
  //
  // O .wgt fecha pela API do proprio Tizen. Fora dela (Chrome de bancada) o
  // objeto nao existe, e o catch deixa a pagina como estava — que la e o
  // comportamento util.
  //
  // E NAO NA HORA (issue #120): o que app_encerrar gravou ou apagou (a marca de
  // sessao viva, os avisos vistos) ainda esta so no MEMFS. dados.c descarrega
  // para o IndexedDB e fecha no callback.
  dados_descarregar_e_sair();
#endif
  return 0;
}
