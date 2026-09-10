// Bootstrap: janela, contexto GL, loop e telemetria. Toda a UI vive nos modulos.
#include <SDL2/SDL.h>
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
#if defined(__APPLE__) || defined(__EMSCRIPTEN__)
#define NV_SEM_WEBOS 1
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "gfx.h"
#include "text.h"
#include "marco.h"
#include "rede.h"
#include "tex_cache.h"
#include "home.h"
#include "text.h"
#include "detail.h"
#include "dados.h"
#include "nuvem.h"
#include "sessao.h"
#include "perfis.h"
#include "sync.h"
#include "traktauth.h"
#include "simklauth.h"
#include "app.h"
#include "registro.h"
#include "video.h"
#include "addons.h"
#include "ajustes.h"
#include "descoberta.h"
#include "trakt.h"
#include "player.h"
#ifndef NV_SEM_WEBOS
#include <dlfcn.h>
#include <SDL2/SDL_syswm.h>
#endif
#include "layout.h"

// Captura de tela sob demanda. O framebuffer da TV nao pode ser lido nem como
// root ("Operation not permitted") e o servico de captura da LG responde erro,
// entao a unica forma de ver o que o app desenha e o proprio app se fotografar.
// Sem isso, cada ajuste visual depende de alguem apontar um celular para a TV.
//
// Protocolo: alguem cria /tmp/nuvio-shot-req; no proximo quadro o app grava
// /tmp/nuvio-shot.png e apaga o pedido.
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
  if (stat(caminho, &st) != 0 || st.st_size <= 0) return 0;
  // Pedido que ja foi atendido e nao pode ser esvaziado: ignora enquanto nao
  // mudar. Sem isto ele vale para sempre e o trabalho e refeito por quadro.
  if (*bloqueado && st.st_mtime == *bloqueado) return 0;
  *bloqueado = 0;
  return 1;
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

static void teclasInjetadas(void (*entregar)(const SDL_Event *)) {
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

// Mesmo protocolo das outras ferramentas: escreva uma URL em /tmp/nuvio-video e
// o app toca. E o unico jeito de testar reproducao sem alguem no sofa — e o
// video nao pode ser conferido por captura, porque vive em outro plano.
static void videoSeSolicitado(void) {
  static time_t bloqueado;
  char url[1024];
  FILE *f;
  if (!pedidoNovo("/tmp/nuvio-video", &bloqueado)) return;
  f = fopen("/tmp/nuvio-video", "r");
  if (!f) return;
  if (fgets(url, sizeof url, f)) {
    char *fim = url + strlen(url);
    while (fim > url && (fim[-1] == '\n' || fim[-1] == '\r')) *--fim = 0;
    printf("[video] pedido: %s\n", url);
    fflush(stdout);
    if (url[0] == '-') video_parar();
    else { video_tocar(url); video_janela(0, 0, 1920, 1080); }
  }
  fclose(f);
  consomeOuBloqueia("/tmp/nuvio-video", &bloqueado);
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

int main(int argc, char **argv) {
  // Sem a identidade do app, o SDL do webOS registra a surface como "(null)" e
  // o compositor NAO exibe a janela — o app roda a 60fps desenhando para
  // ninguem. Medido: "Invalid appId specified OR Unsupported Application Type".
#ifndef NV_SEM_WEBOS
  setenv("APPID", "space.nuvio.native.legacy", 0);
  setenv("LS2_APPID", "space.nuvio.native.legacy", 0);
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
  { const char *log = registro_arquivo();
    if (log) { freopen(log, "w", stdout); freopen(log, "a", stderr); } }
  setvbuf(stdout, NULL, _IOLBF, 0);
  if (!getenv("XDG_RUNTIME_DIR")) setenv("XDG_RUNTIME_DIR", "/tmp/xdg", 1);

  // O SAM lanca o app passando o JSON de launch como argv[1], entao so tratamos
  // argv[1] como caminho quando NAO for JSON.
  char dirBuf[512];
  const char *dirArte = NULL;
  if (argc > 1 && argv[1][0] != '{') dirArte = argv[1];
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

  if (SDL_Init(SDL_INIT_VIDEO) != 0) { printf("SDL_Init: %s\n", SDL_GetError()); return 1; }
  IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

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
  // Canal alpha no framebuffer. Sem ele a superficie nao tem como ficar
  // transparente, e o plano de video do aparelho — que fica ATRAS da janela e
  // so aparece pelo alpha — nunca poderia ser revelado.
  SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
#ifdef __EMSCRIPTEN__
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
  int pedeW, pedeH;

  // OS DADOS ANTES DA JANELA, e so por causa desta escolha.
  //
  // dados_iniciar ficava perto de app_iniciar, bem depois daqui. Mas o tamanho
  // da superficie e decidido AGORA, uma vez, e nao ha como redimensiona-la
  // depois — entao o ajuste precisa estar legivel antes. Ela nao depende de
  // SDL: mexe em getenv/fopen/mkdir, e no Emscripten monta o IDBFS. `dirArte`
  // ja esta resolvido desde o topo do main.
  dados_iniciar(dirArte);
  // E OS AJUSTES LOGO ATRAS, pelo mesmo motivo: ajustes_4k() le `valor[]`, que
  // so sai do padrao depois desta chamada. Sem ela a opcao existia na tela,
  // gravava no arquivo e nao fazia efeito nenhum — o pior tipo de ajuste.
  //
  // A chamada de sempre, la embaixo, FICA: ela roda depois de addons_carregar
  // e e a que estabelece o idioma e o espelho do limite de fileiras. Reler o
  // mesmo arquivo duas vezes e barato e deixa aquele bloco intacto.
  ajustes_dir(dados_dir()[0] ? dados_dir() : dirArte);

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
#ifdef NV_PEDIR_4K
    quer4k = 1;
    printf("[4k] build de medicao: pedindo 3840x2160\n");
#endif
    pedeW = quer4k ? 3840 : (int)NV_TELA_W;
    pedeH = quer4k ? 2160 : (int)NV_TELA_H;
    if (quer4k) { printf("[4k] pedindo %dx%d — a linha `janela=` abaixo diz o "
                         "que a TV concedeu\n", pedeW, pedeH); fflush(stdout); } }
  SDL_Window *win;
  win = SDL_CreateWindow("Nuvio", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED,
                                     pedeW, pedeH, flags);
  if (!win) { printf("janela: %s\n", SDL_GetError()); return 1; }
  // App de TV nao tem ponteiro: o cursor por cima da interface polui a leitura
  // e some sozinho no aparelho, mas nao no Mac.
  SDL_ShowCursor(SDL_DISABLE);
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
      // Opcode 4 de wl_surface e set_opaque_region; NULL = "nada e opaco".
      // Sem commit de proposito: o commit vem do proximo SwapWindow.
      if (marshal && sup) { marshal(sup, 4, NULL); printf("superficie nao-opaca\n"); }
      else printf("sem wayland: video nao vai aparecer\n");
    }
  }
#endif
  SDL_GLContext ctx = SDL_GL_CreateContext(win);
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
           a > 0 ? "" : "  <<< SEM ALPHA: video nao tem como aparecer"); }

  // MARCOS FINOS DO ARRANQUE. Na TV Samsung o log parava exatamente na linha
  // "framebuffer ..." acima e nada mais saia — sem erro, sem excecao. Entre
  // aquele printf e o proximo havia quatro passos, todos triviais, e adivinhar
  // qual custaria uma ida a TV por tentativa. Estes marcos custam uma linha
  // cada e respondem de primeira.
  printf("[arranque] viewport\n"); fflush(stdout);

  // Em tela retina o drawable e maior que a janela; sem ajustar o viewport, o
  // desenho ocupa um quarto da tela.
  SDL_GL_GetDrawableSize(win, &dw, &dh);
  glViewport(0, 0, dw, dh);
  gfx_tamanho_alvo(dw, dh);
  capW = dw; capH = dh;

  // O relogio dos marcos comeca AQUI e nao no topo do main: o que vem antes e
  // parse de argumento e SDL_Init, que nao dependem de nada nosso.
  printf("[arranque] marco_iniciar\n"); fflush(stdout);
  marco_iniciar();
  printf("[arranque] rede_preparar\n"); fflush(stdout);
  // ANTES de tex_iniciar e de app_iniciar, que sao quem cria os fios de rede.
  rede_preparar();
  printf("[arranque] gfx_iniciar (compila os shaders)\n"); fflush(stdout);
  marco("gfx_iniciar");
  if (!gfx_iniciar()) { printf("[arranque] gfx_iniciar FALHOU\n"); fflush(stdout); return 1; }
  printf("[arranque] gfx_iniciar ok\n"); fflush(stdout);
  // fonts/ fica ao lado de art/: derruba o ultimo componente do caminho da arte
  char dirRec[512];
  snprintf(dirRec, sizeof dirRec, "%s", dirArte);
  char *barra = strrchr(dirRec, '/');
  if (barra) *barra = 0;
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
  tex_iniciar(192);
  // A conta vem ANTES da UI: app_iniciar decide entre abrir na home e abrir no
  // login, e para decidir ele precisa saber se ha sessao gravada. (dados_iniciar
  // ja rodou la em cima, antes da janela — ver a nota do 4K.)
  nuvem_configurar(dirArte);
  sessao_iniciar();
  perfis_carregar_ativo();
  // Vinculos feitos NESTA TV. Vem antes de trakt_carregar (que le o arquivo do
  // pacote) para o vinculo do usuario ganhar do arquivo de quem montou — e num
  // pacote distribuivel esse arquivo nem existe.
  traktauth_carregar();
  simklauth_carregar();
  if (!app_iniciar(dirArte)) return 1;
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
  // Ajustes tambem sao do USUARIO, nao do pacote.
  ajustes_dir(dirDados);
  { // As imagens vindas de URL vao para a pasta GRAVAVEL, e nao para o lado da
    // arte do pacote. Uma vez baixadas valem para sempre (arte de filme nao
    // muda), e "para sempre" no Tizen quer dizer IDBFS: em /app/art elas
    // morriam na recarga e a home rebaixava tudo a cada arranque.
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
  desc_iniciar();
  // Metade da resolucao: o snapshot so aparece escurecido e nas bordas.
  int temSnap = gfx_snap_iniciar((int)NV_TELA_W / 2, (int)NV_TELA_H / 2);
  int snapValido = 0;
  // Alvo minusculo de proposito: e ele esticado que vira o desfoque do fundo.
  // 480x270: com o gaussiano de duas passadas, o que importa nao e o alvo ser
  // minusculo (isso e que produzia blocos ao esticar) e sim o desfoque ser de
  // verdade. Esticado 4x, nenhuma borda de texel aparece.
  gfx_borrao_iniciar(480, 270);

  Uint32 ultRelato = SDL_GetTicks();
  double txtMsQuadro = 0, piorTxtMs = 0;
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
  // Dentro de `des`: quanto e travessia de GL e quanto e busca no cache.
  double fFill=0, pFill=0; int fNCheio=0, pNCheio=0;
  double fGfxMs=0, fTexMs=0, fOutMs=0; int fNRect=0, fNProg=0, fNBind=0, fNBusca=0, fNOut=0;
  double pGfxMs=0, pTexMs=0, pOutMs=0; int pNRect=0, pNProg=0, pNBind=0, pNBusca=0, pNOut=0;
#define NV_T0() (SDL_GetPerformanceCounter())
#define NV_DT(a) ((SDL_GetPerformanceCounter() - (a)) * 1000.0 / perFreq)

  while (!app_quer_sair()) {
    SDL_Event e;
    Uint64 tEv = NV_T0();
    // Enquanto o detalhe existe ele fica com o teclado inteiro: a home
    // continua desenhada por baixo, mas nao deve reagir ao D-pad.
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_WINDOWEVENT) continue;
      // O BACK do webOS chega com scancode proprio (482), nao como AC_BACK, e
      // com KEYDOWN e KEYUP quase juntos — so o KEYDOWN conta. Isto ja tinha
      // sido resolvido uma vez e voltou a quebrar quando limpei os remendos
      // antigos: o tratamento saiu junto.
#ifdef __EMSCRIPTEN__
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
#endif
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
    teclasInjetadas(app_evento);
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
    if (quadros > 20) {
      if (dtms > pior) { pior = dtms; piorTxtMs = txtMsQuadro; piorTxtN = txtNQuadro;
                         pEv=fEv; pBomb=fBomb; pUpd=fUpd; pDes=fDes; pSwap=fSwap; pAux=fAux; pClr=fClr;
                         pUplN=fUplN; pUplB=fUplB;
                         pGfxMs=fGfxMs; pTexMs=fTexMs; pNRect=fNRect; pNProg=fNProg;
                         pNBind=fNBind; pNBusca=fNBusca; pOutMs=fOutMs; pNOut=fNOut; pFill=fFill; pNCheio=fNCheio; }
      if (dtms > 33.0) janks++;
    }
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
    tex_bombear(3);
    fBomb = NV_DT(t0);
    fUplN = tex_upl_n; fUplB = tex_upl_bytes;
    t0 = NV_T0();
    app_atualizar(dt, agora);
    fUpd = NV_DT(t0);

    // RECORTE DESLIGADO ANTES DO CLEAR. glClear respeita o scissor test: se
    // qualquer tela terminar o quadro com um recorte ativo, o clear seguinte
    // limpa SO aquele retangulo e o resto da tela guarda o quadro anterior.
    // Hoje todos os chamadores equilibram recorte/sem_recorte, mas isso e uma
    // invariante que ninguem verifica — e o sintoma seria justamente uma faixa
    // com conteudo velho, dificil de atribuir a causa. Uma chamada por quadro.
    t0 = NV_T0();
    gfx_novo_quadro();
    tex_novo_quadro();
    gfx_sem_recorte();
    glClearColor(NV_COR_FUNDO_R, NV_COR_FUNDO_G, NV_COR_FUNDO_B, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    fClr = NV_DT(t0);
    t0 = NV_T0();
    txt_novo_quadro();
    app_desenhar(agora);
    fDes = NV_DT(t0);
    fGfxMs = gfx_ms_rect; fTexMs = tex_ms_busca;
    fNRect = gfx_n_rect; fNProg = gfx_n_prog; fNBind = gfx_n_bind; fNBusca = tex_n_busca;
    fOutMs = gfx_ms_outros; fNOut = gfx_n_outros;
    fFill = gfx_fill; fNCheio = gfx_n_cheio;
    t0 = NV_T0();
    videoSeSolicitado();
    capturaSeSolicitado();
    fAux = NV_DT(t0);
    t0 = NV_T0();
    SDL_GL_SwapWindow(win);
#ifdef __EMSCRIPTEN__
    nv_ceder_quadro();
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
      if (!jaCarimbou) { jaCarimbou = 1; marco("primeiro quadro na tela"); } }
    quadros++;

    if (agora - ultRelato >= 3000) {
      int itens, pend; long bytes;
      tex_estatisticas(&itens, &pend, &bytes);
      // `idbfs=N/X.Xms` e a descarga para o IndexedDB: quantas e o custo SINCRONO
      // da pior. Sem estes dois numeros nao ha como distinguir "o pico sumiu" de
      // "o pico mudou de fase" — foi essa descarga que produziu os 100 ms.
      printf("FPS=%.1f pior=%.1fms janks=%d | pior-quadro: texto %.1fms em %d linhas"
             " | texturas=%d pend=%d %.1fMB | despejos=%d | idbfs=%d/%.1fms | cache-disco=%.1fMB%s\n",
             quadros * 1000.0 / (double)(agora - ultRelato), pior, janks,
             piorTxtMs, piorTxtN, itens, pend, bytes / 1048576.0, txt_despejos,
             dados_desc_n, dados_desc_ms,
             tex_cache_disco_bytes() / 1048576.0,
             dados_persistente() ? "" : "  <<< SEM PERSISTENCIA");
#ifdef __EMSCRIPTEN__
      // Heap linear, nao RAM total do processo: GPU e memoria JS ficam fora.
      // uordblks inclui pilhas dos pthreads e dados alocados pelo malloc.
      { struct mallinfo mi = mallinfo();
        printf("[mem] WASM=%.1f MiB malloc=%.1f MiB livre-no-heap=%.1f MiB\n",
               emscripten_get_heap_size() / 1048576.0,
               mi.uordblks / 1048576.0, mi.fordblks / 1048576.0); }
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
      if (pior > 33.0) {
        printf("[quadro] pior=%.1fms | ev=%.1f bomb=%.1f(%d tex, %.1fMB)"
               " upd=%.1f clr=%.1f des=%.1f aux=%.1f swap=%.1f\n",
               pior, pEv, pBomb, pUplN, pUplB / 1048576.0,
               pUpd, pClr, pDes, pAux, pSwap);
      }
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
      dados_desc_zerar();
      pEv=pBomb=pUpd=pDes=pSwap=pAux=pClr=0;
      pUplN=0; pUplB=0;
      pGfxMs=pTexMs=pOutMs=0; pNRect=pNProg=pNBind=pNBusca=pNOut=0; pFill=0; pNCheio=0;
    }
  }

  gfx_borrao_encerrar();
  gfx_snap_encerrar();
  app_encerrar();
  tex_encerrar();
  txt_encerrar();
  gfx_encerrar();
  SDL_GL_DeleteContext(ctx);
  SDL_DestroyWindow(win);
  IMG_Quit();
  SDL_Quit();
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
  EM_ASM({
    try { tizen.application.getCurrentApplication().exit(); } catch (e) {}
  });
#endif
  return 0;
}
