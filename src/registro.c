#include "app_id.h"
#include "registro.h"
#include "dados.h"
#include "gfx.h"
#include "text.h"
#include "idioma.h"
#include "idiomacod.h"
#include "layout.h"
#include "teclavoltar.h"
#include "ajustes.h"
#include "avisos.h"
#include "redesaude.h"
#include "tex_cache.h"
#define NV_ESCALA_TELA   // o arquivo inteiro mede pela tela virtual (escala.h)
#include "escala.h"
#include "ponteiro.h"
#include "rolagemtoque.h"
#include "telefonecartao.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <sys/stat.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif

// TECLA VERMELHA NO webOS: 486, CONFERIDO no header do SDK que compila o alvo
// ARM (SDL_webOS.h: SDL_WEBOS_SCANCODE_RED 486, na mesma tabela de onde
// layout.h tirou BACK=482 e BLUE=489). Nao esta em layout.h porque este arquivo
// nao e dono dela.
//
// O que NAO foi conferido: se a tecla chega ao app no aparelho. O BACK precisou
// de um hint proprio na criacao da janela (SDL_WEBOS_ACCESS_POLICY_KEYS_BACK);
// para as coloridas nao existe hint equivalente. Se o vermelho nao abrir o
// painel na TV da LG, a causa mais provavel e essa e nao o numero — e por isso
// existe Ajustes > Sobre e ajuda > "Ver o registro na tela" (registro_abrir).
#define REG_SCANCODE_VERMELHA 486

// A MESMA tecla precisa existir sem controle de TV: no Mac nao ha vermelha, e
// no Tizen a vermelha e traduzida pelo shell (keyCode 403 -> F9). F9 e nao uma
// letra: o painel e roteado ANTES de todas as telas, e uma letra abriria o
// painel no meio de quem estivesse digitando na busca.
#define REG_TECLA_TECLADO SDLK_F9

// Quantos bytes do FIM do arquivo sao lidos, e quantas linhas ficam guardadas.
// O que interessa e sempre o fim. As linhas que nao cabem nas 200 ainda entram
// nas CONTAGENS das areas ("Tudo 214"): o painel mostra 200 das ultimas N.
//
// REG_COL e 512 e nao a largura que CABE na tela: guardar a linha inteira e o
// que permite ao ehFalha achar o "<<< SEM PERSISTENCIA" que vive no FIM da
// linha de FPS, e ao detalhe mostrar a linha toda.
#define REG_JANELA 40960
#define REG_MAX    200
#define REG_COL    512

// AO VIVO (dono, 03/10): com o painel aberto o arquivo e relido a cada
// segundo — so quando o tamanho mudou. Rolar para cima PAUSA (a lista para de
// andar debaixo do foco) e conta as linhas novas; descer ate o fim volta a
// seguir.
#define REG_RELER_MS 1000u

static int aberto;
static int aviso;
static int legal; // 1: aviso curto; 2: leitura em Ajustes
static int legalVisto;
static int avisoDecidido;
// A soltura do MESMO toque que abriu/fechou algo aqui. ARMADILHA JA PAGA NESTE
// REPO: o KEYUP de um toque que fechou uma camada vaza para a tela de tras
// (issue #8). Toda vez que este modulo age num KEYDOWN ele guarda a tecla e
// engole o KEYUP. SYM E SCANCODE: as coloridas do webOS chegam por SCANCODE.
static SDL_Keycode soltarSym;
static int soltarScan = -1;
static void engolirSoltura(const SDL_Event *e) {
  soltarSym  = e->key.keysym.sym;
  soltarScan = e->key.keysym.scancode;
}

// --- AS LINHAS ---------------------------------------------------------------
// Areas: a etiqueta entre colchetes do printf ([rede], [video]...) agrupada em
// cinco. Toda linha cai em exatamente uma (o que nao se reconhece vai para
// Sistema), entao as contagens das cinco somam o "Tudo".
enum { RG_TUDO = 0, RG_PROB, RG_VIDEO, RG_REDE, RG_FONTES, RG_IMAGENS, RG_SISTEMA, RG_N };
typedef struct {
  char t[REG_COL];          // a linha inteira, com as URLs cortadas no host
  short msg;                // onde o texto comeca, depois de "[area] "
  char area[24];            // "video", "quadros" (a linha FPS=), "" sem etiqueta
  unsigned char sev;        // 0 normal, 1 aviso, 2 erro
  unsigned char grupo;      // RG_VIDEO..RG_SISTEMA
  unsigned char fps;        // linha FPS= de main.c
} RegLinha;
static RegLinha lin[REG_MAX];
static int nLin;            // linhas guardadas (as ultimas REG_MAX)
static int nLidas;          // linhas na janela lida (>= nLin)
static int cont[RG_N];      // por area, sobre a janela lida
static int nErros, nAvisos;
static int semFonte;        // 1 = nao existe de onde ler nesta compilacao
static long marcaLida;      // tamanho do arquivo (ou n de linhas no wasm) na ultima leitura
static Uint32 ultLeitura;

static char cru[REG_JANELA + 1];

const char *registro_arquivo(void) {
#ifdef __EMSCRIPTEN__
  // Nao ha arquivo util no navegador: o "sistema de arquivos" e MEMFS, morre a
  // cada recarga, e o log ja esta no console. NULL faz main.c pular o freopen.
  return NULL;
#else
  static char caminho[256];
  static int resolvido;
  if (!resolvido) {
    const char *env = getenv("NUVIO_LOG");
    resolvido = 1;
    if (env && env[0]) snprintf(caminho, sizeof caminho, "%s", env);
#if defined(__APPLE__) || defined(NV_LINUX_DESKTOP)
    // No Mac o log vai para o TERMINAL e continua assim. Para exercitar o
    // painel na previa:  NUVIO_LOG=/tmp/nuvio.log bash tools/mac.sh
    else caminho[0] = 0;
#elif defined(NV_ANDROID)
    else snprintf(caminho, sizeof caminho, "%s/nuvio.log",
                  getenv("NUVIO_DADOS") ? getenv("NUVIO_DADOS") : ".");
#else
#ifdef NV_DTS_DEBUG
    else snprintf(caminho, sizeof caminho, "/tmp/" NV_APP_ID ".log");
#else
    else snprintf(caminho, sizeof caminho, "/tmp/nuvio.log");
#endif
#endif
  }
  return caminho[0] ? caminho : NULL;
#endif
}

// Corta a URL no HOST. Ver a nota de privacidade em registro.h: a chave do
// debrid viaja no CAMINHO da URL, e este painel aparece na TV da sala. So
// mascara quando o caminho tem 4 bytes ou mais (o tamanho de "/…" em UTF-8):
// a substituicao nunca cresce a linha.
static void mascarar(char *s) {
  char *p = s;
  while ((p = strstr(p, "://")) != NULL) {
    char *h = p + 3, *fim;
    while (*h && *h != '/' && *h != '?' && *h != ' ') h++;
    if (*h != '/' && *h != '?') { p = h; continue; }
    fim = h;
    while (*fim && *fim != ' ') fim++;
    if (fim - h < 4) { p = fim; continue; }
    memmove(h + 4, fim, strlen(fim) + 1);
    memcpy(h, "/\xe2\x80\xa6", 4);
    p = h + 4;
  }
}

// Corta lixo de UTF-8 no FIM: snprintf trunca por BYTE, e uma sequencia
// invalida faz o SDL_ttf devolver linha VAZIA.
static void aparaUtf8(char *s) {
  size_t n = strlen(s), i;
  unsigned char c;
  int quer;
  if (!n) return;
  i = n;
  while (i > 0 && ((unsigned char)s[i - 1] & 0xC0) == 0x80 && n - i < 3) i--;
  if (i == 0) return;
  c = (unsigned char)s[i - 1];
  quer = c < 0x80 ? 1 : (c & 0xE0) == 0xC0 ? 2 : (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
  if ((int)(n - i + 1) < quer) s[i - 1] = 0;
}

// ERRO: o que o painel ja pintava de vermelho, sem mudar (heuristica, nao
// classificacao: o C nao sabe qual printf era erro; "Homem de Ferro" sai
// vermelho. Falso positivo custa uma linha colorida; falso negativo, o
// defeito passar batido numa foto da tela).
static int ehFalha(const char *s) {
  return strstr(s, "<<<") || strstr(s, "FALH") || strstr(s, "falh") ||
         strstr(s, "erro") || strstr(s, "Erro") || strstr(s, "ERRO");
}
// "HTTP 4xx" ou "HTTP 5xx" em qualquer ponto da linha; devolve o codigo.
static int httpErro(const char *s) {
  const char *p = s;
  while ((p = strstr(p, "HTTP ")) != NULL) {
    p += 5;
    if ((p[0] == '4' || p[0] == '5') && p[1] >= '0' && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' &&
        !(p[3] >= '0' && p[3] <= '9')) return (p[0] - '0') * 100 + (p[1] - '0') * 10 + (p[2] - '0');
  }
  return 0;
}
// AVISO (ambar, decisao do dono de 03/10): o servidor ou a conexao reclamou,
// mas o app seguiu. A lista e curta de proposito: cada palavra e uma que os
// printf do app usam para isso.
static int ehAviso(const char *s) {
  return strstr(s, "sem resposta") || strstr(s, "caiu") || strstr(s, "corte") ||
         strstr(s, "nao se despediu") ||
         strstr(s, "recusou") || httpErro(s);
}

static const char *const AREA_VIDEO[] = {
  "video", "player", "mkv", "mkvass", "legenda", "legendas", "ass", "serieaud",
  "posplay", "pg", "seekr", "trailer", "4k", "vazao", "proxy-ts", "livetv",
  "livetv-diag", "xtream", "xtepg", "stalker", "epg", "guia", "faixas", NULL };
static const char *const AREA_REDE[] = {
  "rede", "addons", "sync", "avisos", "trakt", "simkl", "recomenda", "nuvem",
  "atualizacao", "noticia", "noticias", "tmdb", "sessao", "login", "celular",
  "telemetria", "diagnostico", NULL };
static const char *const AREA_FONTES[] = {
  "fonte", "voltafonte", "fontecache", "fontes", "debrid", "desc", "p2p",
  "streams", "extras", NULL };
static const char *const AREA_IMAGENS[] = {
  "tex", "tex-trace", "tex-nitidez", "arte", "poster", "webp", "jpeg", "gif",
  "hero", "cor", "desfoque", "logo", NULL };
static int naLista(const char *const *l, const char *a) {
  for (; *l; l++) if (!strcmp(*l, a)) return 1;
  return 0;
}
static int grupoDe(const char *a) {
  if (naLista(AREA_VIDEO, a)) return RG_VIDEO;
  if (naLista(AREA_REDE, a)) return RG_REDE;
  if (naLista(AREA_FONTES, a)) return RG_FONTES;
  if (naLista(AREA_IMAGENS, a)) return RG_IMAGENS;
  return RG_SISTEMA;
}

// Etiqueta, gravidade e area de uma linha ja mascarada.
static void classificar(RegLinha *L) {
  const char *s = L->t;
  L->msg = 0; L->area[0] = 0; L->fps = 0;
  if (s[0] == '[') {
    const char *f = strchr(s, ']');
    if (f && f - s - 1 > 0 && f - s - 1 < (long)sizeof L->area) {
      memcpy(L->area, s + 1, (size_t)(f - s - 1));
      L->area[f - s - 1] = 0;
      f++;
      while (*f == ' ') f++;
      L->msg = (short)(f - s);
    }
  } else if (!strncmp(s, "FPS=", 4)) {
    snprintf(L->area, sizeof L->area, "quadros");
    L->fps = 1;
  }
  L->sev = ehFalha(s) ? 2 : ehAviso(s) ? 1 : 0;
  L->grupo = (unsigned char)(L->fps ? RG_SISTEMA : grupoDe(L->area));
}

// Quebra o bloco lido em linhas e guarda as ultimas REG_MAX. `parcial` = a
// primeira linha pode estar cortada no meio (o arquivo foi lido do fim) e e
// descartada — meia linha de log confunde mais do que ajuda.
static void fatiar(char *texto, int parcial) {
  static char *ptr[REG_JANELA / 2];
  int n = 0, i, k;
  char *p = texto, *q;
  nLin = 0; nLidas = 0; nErros = nAvisos = 0;
  memset(cont, 0, sizeof cont);
  if (parcial) {
    q = strchr(p, '\n');
    if (!q) return;
    p = q + 1;
  }
  while (*p && n < (int)(sizeof ptr / sizeof *ptr)) {
    q = strchr(p, '\n');
    if (q) *q = 0;
    { char *fim = p + strlen(p);
      while (fim > p && (fim[-1] == '\r' || fim[-1] == ' ')) *--fim = 0; }
    if (*p) ptr[n++] = p;
    if (!q) break;
    p = q + 1;
  }
  nLidas = n;
  k = n > REG_MAX ? n - REG_MAX : 0;
  for (i = 0; i < n; i++) {
    static RegLinha tmp;
    RegLinha *L = i >= k ? &lin[i - k] : &tmp;
    snprintf(L->t, REG_COL, "%s", ptr[i]);
    aparaUtf8(L->t);
    mascarar(L->t);
    classificar(L);
    cont[L->grupo]++;
    if (L->sev) { cont[RG_PROB]++; if (L->sev == 2) nErros++; else nAvisos++; }
  }
  nLin = n - k;
  cont[RG_TUDO] = nLidas;
}

#ifdef __EMSCRIPTEN__
// As linhas que o shell (tools/tizen-shell.html) guarda em texto puro, a mesma
// lista que alimenta o painel DOM. -1 = a lista nem existe (shell antigo), 0 =
// existe e esta vazia: o painel tem frases diferentes para cada um.
EM_JS(int, nv_registro_js, (char *dst, int tam, int *total), {
  try {
    var a = (typeof window !== "undefined") ? window.__nvLinhas : null;
    if (!a) return -1;
    HEAP32[total >> 2] = a.length;
    if (!a.length) return 0;
    var t = a.slice(-260).join("\n");
    if (t.length > tam - 8) t = t.slice(-(tam - 8));
    stringToUTF8(t, dst, tam);
    return 1;
  } catch (e) { return 0; }
});
#endif

// Tamanho atual da fonte (bytes do arquivo, ou linhas no wasm); -1 sem fonte.
static long marcaAtual(void) {
#ifdef __EMSCRIPTEN__
  int total = 0;
  if (nv_registro_js(cru, 2, &total) < 0) return -1;
  return total;
#else
  struct stat st;
  const char *c = registro_arquivo();
  if (!c || stat(c, &st) != 0) return -1;
  return (long)st.st_size;
#endif
}

#ifdef REGISTRO_TESTE
static const char *regTesteTexto;   // linhas fixas do harness (tests/registro_shot.c)
static int regTesteSemFonte;
#endif

static void carregar(void) {
  nLin = 0; nLidas = 0; semFonte = 0;
  ultLeitura = SDL_GetTicks();
  memset(cont, 0, sizeof cont);
#ifdef REGISTRO_TESTE
  if (regTesteSemFonte) { semFonte = 1; return; }
  if (regTesteTexto) {
    snprintf(cru, sizeof cru, "%s", regTesteTexto);
    fatiar(cru, 0);
    marcaLida = (long)strlen(regTesteTexto);
    return;
  }
#endif
#ifdef __EMSCRIPTEN__
  { int r, total = 0;
    cru[0] = 0;
    r = nv_registro_js(cru, (int)sizeof cru, &total);
    marcaLida = total;
    if (r <= 0) { semFonte = (r < 0); return; }
    fatiar(cru, 0); }
#else
  { const char *caminho = registro_arquivo();
    FILE *f;
    long tam, comeco;
    size_t n;
    if (!caminho) { semFonte = 1; return; }
    f = fopen(caminho, "rb");
    if (!f) { semFonte = 1; return; }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); semFonte = 1; return; }
    tam = ftell(f);
    if (tam < 0) { fclose(f); semFonte = 1; return; }
    comeco = tam > (long)REG_JANELA ? tam - (long)REG_JANELA : 0;
    fseek(f, comeco, SEEK_SET);
    n = fread(cru, 1, REG_JANELA, f);
    cru[n] = 0;
    fclose(f);
    marcaLida = tam;
    fatiar(cru, comeco > 0); }
#endif
}

// --- O ESTADO DO PAINEL ------------------------------------------------------
static int area;            // RG_* da aba
static int segEd;           // escolhendo a area (o segmentado no acento)
static int pausado;         // ↑ pausou: a lista nao anda
static int foco = -1;       // linha em foco (indice em vis), pausado
static int detalhe;         // a linha em foco aberta por inteiro
static int focoEnviar;      // foco no "Enviar agora" do inspetor
static int novas;           // linhas que chegaram desde a pausa
static long marcaPausa;
static int focoEtapa;       // Sistema com rastro de etapas: a etapa em foco

// A lista filtrada: indice em lin e se e linha de CONTEXTO (apagada, na aba
// Problemas: a linha de antes de cada problema).
static int vis[REG_MAX * 2];
static unsigned char visCtx[REG_MAX * 2];
static int nVis;
#ifdef NV_TOUCH_UI
static ToqueRolagem toqueRegistro, toqueEtapas, toqueAreas;
static float toqueRegistroOffset, toqueEtapasOffset, toqueAreasOffset;
static ToqueRolagem *toqueRegAtiva;
#endif
static void filtrar(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueRegistro); toquerol_limpar(&toqueEtapas);
#endif
  int i;
  nVis = 0;
  for (i = 0; i < nLin; i++) {
    const RegLinha *L = &lin[i];
    if (area == RG_TUDO) { vis[nVis] = i; visCtx[nVis++] = 0; continue; }
    if (area == RG_PROB) {
      if (!L->sev) continue;
      if (i > 0 && !lin[i - 1].sev && !lin[i - 1].fps && (!nVis || vis[nVis - 1] != i - 1)) {
        vis[nVis] = i - 1; visCtx[nVis++] = 1;
      }
      vis[nVis] = i; visCtx[nVis++] = 0;
      continue;
    }
    if (L->grupo == area) { vis[nVis] = i; visCtx[nVis++] = 0; }
  }
}

static void recarregar(void) {
  carregar();
  filtrar();
  if (foco >= nVis) foco = nVis - 1;
}

static void pausar(int focoEm) {
  pausado = 1;
  novas = 0;
  marcaPausa = marcaLida;
  foco = focoEm < 0 ? 0 : focoEm >= nVis ? nVis - 1 : focoEm;
  detalhe = 1;
}
static void seguir(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueRegistro);
#endif
  pausado = 0; foco = -1; detalhe = 0; novas = 0;
  recarregar();
}
#ifdef NV_TOUCH_UI
static int toqueRegistroRolar(const PonteiroRolagem *e) {
  if (e->fase == PONT_ROL_INICIO) {
    toqueRegAtiva = NULL;
    if (telefoneui_ativo() && toquerol_evento(&toqueAreas, e)) { toqueRegAtiva = &toqueAreas; return 1; }
    if (toquerol_evento(&toqueEtapas, e)) toqueRegAtiva = &toqueEtapas;
    else if (toquerol_evento(&toqueRegistro, e)) toqueRegAtiva = &toqueRegistro;
    if (toqueRegAtiva) { pausado = 1; marcaPausa = marcaLida; return 1; }
    return 0;
  }
  return toqueRegAtiva ? toquerol_evento(toqueRegAtiva, e) : 0;
}
static void toqueRegLinha(int i, int b) {
  (void)b;
  if (i < 0 || i >= nVis) return;
  pausar(i); detalhe = 0; segEd = focoEnviar = 0;
}
static void toqueRegArea(int i, int b) {
  (void)b;
  if (i < 0 || i >= RG_N) return;
  area = i; segEd = 1; focoEnviar = 0; seguir();
}
static void toqueRegSeguir(int a, int b) { (void)a; (void)b; seguir(); }
static void toqueRegEnviar(int a, int b) { (void)a; (void)b; segEd = 0; focoEnviar = 1; }
static void toqueRegEtapa(int a, int b) { (void)b; focoEtapa = a; segEd = focoEnviar = 0; }
#endif

// Chamado por quadro com o painel aberto: rele ao vivo, ou conta as novas.
static void passoAoVivo(void) {
  Uint32 agora = SDL_GetTicks();
  long m;
  if (agora - ultLeitura < REG_RELER_MS) return;
  ultLeitura = agora;
#ifdef REGISTRO_TESTE
  if (regTesteTexto || regTesteSemFonte) return;
#endif
  fflush(stdout);
  m = marcaAtual();
  if (m == marcaLida) return;
  if (!pausado) { recarregar(); return; }
  // Pausado: conta as linhas que chegaram depois da pausa, sem mexer na lista.
#ifdef __EMSCRIPTEN__
  novas = (int)(m - marcaPausa);
#else
  { const char *c = registro_arquivo();
    FILE *f = c ? fopen(c, "rb") : NULL;
    if (f) {
      static char buf[REG_JANELA];
      long de = marcaPausa > m - (long)REG_JANELA ? marcaPausa : m - (long)REG_JANELA;
      size_t n, k;
      int q = 0;
      fseek(f, de, SEEK_SET);
      n = fread(buf, 1, sizeof buf, f);
      fclose(f);
      for (k = 0; k < n; k++) if (buf[k] == '\n') q++;
      novas = q;
    } }
#endif
  if (novas < 0) novas = 0;
}

int registro_aberto(void) { return aberto; }

static void abrir(void) {
#ifdef NV_TOUCH_UI
  toquerol_limpar(&toqueAreas); toqueAreasOffset = 0;
#endif
  aberto = 1;
  area = RG_TUDO; segEd = 0; pausado = 0; foco = -1; detalhe = 0; focoEnviar = 0;
  novas = 0; focoEtapa = 0;
  fflush(stdout);   // o que este fio ja imprimiu entra no arquivo antes da leitura
  recarregar();
}
// "NOVO" na linha de Ajustes ate a primeira abertura (a marca e um arquivo).
static int jaAberto = -1;
int registro_ja_aberto(void) {
  if (jaAberto < 0) { char *s = dados_ler("registro-aberto.txt"); jaAberto = s != NULL; free(s); }
  return jaAberto;
}
void registro_abrir(void) {
  if (aberto) return;
  abrir();
  if (!registro_ja_aberto()) { jaAberto = 1; dados_gravar("registro-aberto.txt", "1\n"); }
}

static void fecharAviso(void) {
  if (!aviso) return;
  aviso = 0;
  // Grava DEPOIS de ter sido visto: se o app morrer com o cartao na tela, a
  // pessoa ve o aviso de novo. Sem pasta gravavel e no-op e o aviso volta.
  dados_gravar("aviso-log.txt", "1\n");
}

void registro_aviso_primeira_vez(void) {
  char *s;
  if (avisoDecidido) return;
  avisoDecidido = 1;
  s = dados_ler("aviso-log.txt");
  if (s) { free(s); return; }
  aviso = 1;
}

void registro_legal_abrir(void) { legal = 2; }
int registro_legal_aberto(void) { return legal; }
void registro_aviso_legal(void) {
  char *s;
  if (legalVisto) return;
  s = dados_ler("aviso-legal.txt");
  if (s) { free(s); legalVisto = 1; return; }
  legalVisto = 1;
  dados_gravar("aviso-legal.txt", "1\n");
  legal = 1;
}

static int ehAlternar(const SDL_Event *e) {
  if (e->key.repeat) return 0;
  if (e->key.keysym.sym == REG_TECLA_TECLADO) return 1;
  // A AZUL (489) fica de fora de proposito: e o atalho do perfil em app.c.
  if (e->key.keysym.scancode == REG_SCANCODE_VERMELHA) return 1;
  return 0;
}

static int ehOk(SDL_Keycode k) { return k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE; }

static int etapasN(void);   // registro_ilha.inc
static int temEtapas(void) { return area == RG_SISTEMA && etapasN() > 0; }

static void trocarArea(int d) {
  int a = area + d;
  if (a < 0 || a >= RG_N) return;
  area = a; focoEtapa = 0;
  if (pausado) { pausado = 0; foco = -1; detalhe = 0; }
  filtrar();
}

int registro_envio_aberto(void);
int registro_envio_evento(const SDL_Event *e);

int registro_evento(const SDL_Event *e) {
#ifdef NV_TOUCH_UI
  if (toquerol_navegacao(e)) {
    if (toquerol_livre(&toqueRegistro) && nVis > 0) {
      foco = (int)(toqueRegistroOffset / 32.0f);
      if (foco >= nVis) foco = nVis - 1;
      segEd = 0;
    }
    toquerol_limpar(&toqueRegistro); toquerol_limpar(&toqueEtapas); toquerol_limpar(&toqueAreas);
  }
#endif
  int ehTecla = (e->type == SDL_KEYDOWN || e->type == SDL_KEYUP || e->type == SDL_TEXTINPUT);

  if (e->type == SDL_KEYUP &&
      ((soltarSym && e->key.keysym.sym == soltarSym) ||
       (soltarScan >= 0 && (int)e->key.keysym.scancode == soltarScan))) {
    soltarSym = 0;
    soltarScan = -1;
    return 1;
  }

  if (legal) {
    if (e->type == SDL_KEYDOWN && (ehOk(e->key.keysym.sym) || nv_tecla_voltar(e))) {
      legal = 0; engolirSoltura(e);
    }
    return ehTecla;
  }

  if (e->type == SDL_KEYDOWN && ehAlternar(e)) {
    fecharAviso();   // quem faz o gesto nao precisa mais do aviso que o ensina
    if (aberto) aberto = 0;
    else abrir();
    engolirSoltura(e);
    return 1;
  }

  if (!aberto && aviso) {
    // CARTAO, nao tela: QUALQUER tecla o dispensa, e a tecla morre aqui.
    if (e->type == SDL_KEYDOWN) { fecharAviso(); engolirSoltura(e); return 1; }
    return ehTecla;
  }

  if (!aberto) return 0;

  // Daqui para baixo o painel e MODAL: nenhuma tecla chega a tela de tras.
  if (e->type == SDL_KEYDOWN) {
    SDL_Keycode k = e->key.keysym.sym;
    if (nv_tecla_voltar(e)) { aberto = 0; engolirSoltura(e); return 1; }
    if (semFonte || !nLin) {
      if (ehOk(k)) { recarregar(); engolirSoltura(e); }
      return 1;
    }
    if (focoEnviar) {
      if (k == SDLK_LEFT || k == SDLK_UP) { focoEnviar = 0; segEd = 1; }
      else if (ehOk(k)) {
        aberto = 0;
        registro_envio_abrir();
        engolirSoltura(e);
      }
      return 1;
    }
    if (segEd) {
      if (k == SDLK_LEFT) trocarArea(-1);
      else if (k == SDLK_RIGHT) {
        if (area == RG_N - 1) { segEd = 0; focoEnviar = 1; }
        else trocarArea(1);
      }
      else if (ehOk(k) || k == SDLK_DOWN) { segEd = 0; engolirSoltura(e); }
      return 1;
    }
    if (temEtapas()) {
      int n = etapasN();
      if (k == SDLK_UP) { if (focoEtapa > 0) focoEtapa--; else segEd = 1; }
      else if (k == SDLK_DOWN) { if (focoEtapa < n - 1) focoEtapa++; }
      else if (k == SDLK_LEFT) { segEd = 1; trocarArea(-1); }
      else if (k == SDLK_RIGHT) focoEnviar = 1;
      return 1;
    }
    if (!pausado) {
      if (k == SDLK_UP || ehOk(k)) { if (nVis) pausar(nVis - 1); if (ehOk(k)) engolirSoltura(e); }
      else if (k == SDLK_LEFT) { segEd = 1; trocarArea(-1); }
      else if (k == SDLK_RIGHT) {
        if (area == RG_N - 1) focoEnviar = 1;
        else { segEd = 1; trocarArea(1); }
      }
      return 1;
    }
    // Pausado: ↑ ↓ andam de linha, ← → de pagina, OK abre/fecha o detalhe, e
    // descer alem da ultima volta a seguir o fim.
    if (k == SDLK_UP) { if (foco > 0) foco--; else { segEd = 1; } }
    else if (k == SDLK_DOWN) { if (foco < nVis - 1) foco++; else seguir(); }
    else if (k == SDLK_LEFT || k == SDLK_PAGEUP) { foco -= 16; if (foco < 0) foco = 0; }
    else if (k == SDLK_RIGHT || k == SDLK_PAGEDOWN) { foco += 16; if (foco >= nVis) seguir(); }
    else if (ehOk(k)) { detalhe = !detalhe; engolirSoltura(e); }
    return 1;
  }
  return ehTecla;
}

#include "registro_ilha.inc"
#include "registro_envio.inc"

// Leitura legal com as mesmas pecas de texto longo do painel de registro.
static void legalPonteiro(int a, int b) { (void)a; (void)b; }
static void desenhaLegal(void) {
  const char *texto = legal == 1
    ? "Nuvio Legacy é um player: addons e listas são de terceiros e você é responsável pelo que adiciona.\nAjustes › Sobre e ajuda › Aviso legal."
    : "Nuvio Legacy é apenas um player. Não hospeda, armazena, indexa, envia nem distribui conteúdo.\nNão vem com fontes: tudo que toca vem de addons, listas, servidores e contas de terceiros que você escolhe.\nVocê é responsável pelo que adiciona e assiste e por ter direito de acesso onde mora.\nO projeto não controla, recomenda nem verifica addons, listas ou serviços e não fornece links para conteúdo sem licença.\nAvisos de direitos autorais devem ir a quem hospeda o conteúdo.\nNão é afiliado ao Stremio, ao Nuvio oficial, a serviços de streaming ou debrid, nem a fabricantes de TV.";
  float w = fminf(1500, NV_VTELA_W - 96), x = (NV_VTELA_W - w) * 0.5f;
  float h = rgBlocoAltura(TXT_AJ_TEXTO, texto, w - 88, 30, 24) + 200;
  float y = (NV_VTELA_H - h) * 0.5f, by, bw;
  TxtLinha titulo = rgT(TXT_LOG_T34, "Aviso legal");
  rgIlha((GfxRect){ x, y, w, h }, 36, 1, 0.055f, 0.059f, 0.071f);
  txt_desenhar(titulo, x + 44, y + 36);
  rgBloco(TXT_AJ_TEXTO, texto, x + 44, y + 96, w - 88, 30, 0.85f, 24);
  by = y + h - 84;
  bw = rgBotao("OK", NULL, x + 44, by, 1, 1);
  ponteiro_alvo(x + 44, by, bw, 56, legalPonteiro, NULL, 0, 0);
}

static void registro_desenharCorpo_(void);
// Camada ampliada (escala.h): o corpo desenha na tela virtual.
void registro_desenhar(void) {
  ESCALA_INI();
  registro_desenharCorpo_();
  ESCALA_FIM();
}
static void registro_desenharCorpo_(void) {
#ifdef NV_TOUCH_UI
  toqueRegistro.offset = toqueEtapas.offset = toqueAreas.offset = NULL;
#endif
  if (!aberto && !aviso && !legal) return;
  // RECORTE DESLIGADO ANTES DE DESENHAR: se uma tela esquecer o recorte ligado,
  // o unico sintoma seria o painel de DIAGNOSTICO nao aparecer.
  gfx_sem_recorte();
  // PONTEIRO (#99): o aviso e um cartao que qualquer tecla dispensa — camada
  // sem alvos, o clique e o OK (registro_evento o fecha) e nao vaza para a
  // tela de tras. O painel aberto ja e camada em app.c.
  if (legal) { ponteiro_camada(); desenhaLegal(); return; }
  if (!aberto && aviso) ponteiro_camada();
  if (aberto) { passoAoVivo(); desenhaPainel(); }
  else desenhaAviso();
}

#ifdef REGISTRO_TESTE
void registro_teste_texto(const char *t) { regTesteTexto = t; regTesteSemFonte = 0; }
void registro_teste_sem_fonte(int s) { regTesteSemFonte = s; }
void registro_teste_estado(int a, int ed, int pausa, int focoLinha, int det, int nov) {
  area = a; segEd = ed; focoEnviar = 0; focoEtapa = 0; filtrar();
  if (pausa) { pausado = 1; foco = focoLinha < 0 ? nVis + focoLinha : focoLinha; detalhe = det; novas = nov; }
  else { pausado = 0; foco = -1; detalhe = 0; novas = 0; }
}
int registro_teste_achar(const char *trecho, int ultima) {
  int i, r = -1;
  for (i = 0; i < nVis; i++) if (strstr(lin[vis[i]].t, trecho)) { r = i; if (!ultima) break; }
  return r;
}
void registro_teste_aviso(int a) { aviso = a; }
void registro_teste_foco_etapa(int f) { focoEtapa = f; }
#endif
