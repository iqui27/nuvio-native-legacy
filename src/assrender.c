#if defined(NV_TPK) && defined(__linux__) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE   // pthread_getattr_np (the worker's stack in the crash log)
#endif
#include "assrender.h"
#include "legfontes.h"

#include <pthread.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include <dirent.h>
#include <sys/stat.h>

/* A PASTA DE FONTES DO SISTEMA NAO E SO DE FONTES. Na C9, /usr/share/fonts
 * traz tabelas do firmware (arib_mrg_v5-10.bin e parecidas) ao lado das .ttf.
 * ass_set_fonts_dir entregava TUDO ao libass como fonte em memoria, e cada
 * arquivo que nao e fonte virava "[libass] Error opening memory font" no log
 * — alem de ser lido inteiro para a memoria por nada. Aqui a pasta e lida
 * pelo app, com o mesmo criterio do libass (arquivo regular, sem ponto no
 * comeco, sem descer em subpasta), mas so passa o que tem assinatura de fonte
 * TrueType/OpenType: 00 01 00 00, "OTTO", "true", "typ1" ou colecao "ttcf". */
int assrender_bytes_sao_fonte(const void *dados, size_t n) {
  const unsigned char *p = (const unsigned char *)dados;
  if (!p || n < 4) return 0;
  return (p[0] == 0 && p[1] == 1 && p[2] == 0 && p[3] == 0) ||
         !memcmp(p, "OTTO", 4) || !memcmp(p, "true", 4) ||
         !memcmp(p, "typ1", 4) || !memcmp(p, "ttcf", 4);
}

int assrender_ler_pasta_fontes(const char *dir,
                               void (*cb)(const char *nome, const void *dados,
                                          size_t tam, void *u),
                               void *u, int *ignorados) {
  DIR *d;
  struct dirent *e;
  int lidas = 0, fora = 0;
  if (ignorados) *ignorados = 0;
  if (!dir || !*dir || !(d = opendir(dir))) return 0;
  while ((e = readdir(d)) != NULL) {
    char caminho[1024];
    struct stat st;
    unsigned char cab[4];
    FILE *f;
    char *buf;
    if (e->d_name[0] == '.') continue;
    snprintf(caminho, sizeof caminho, "%s/%s", dir, e->d_name);
    if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size < 4 ||
        (unsigned long long)st.st_size > (unsigned long long)INT_MAX) continue;
    f = fopen(caminho, "rb");
    if (!f) continue;
    if (fread(cab, 1, 4, f) != 4 || !assrender_bytes_sao_fonte(cab, 4)) {
      fclose(f); fora++; continue;
    }
    buf = malloc((size_t)st.st_size);
    if (buf && fseek(f, 0, SEEK_SET) == 0 &&
        fread(buf, 1, (size_t)st.st_size, f) == (size_t)st.st_size) {
      if (cb) cb(e->d_name, buf, (size_t)st.st_size, u);
      lidas++;
    }
    free(buf);
    fclose(f);
  }
  closedir(d);
  if (ignorados) *ignorados = fora;
  return lidas;
}

/* Pasta das fontes do app que o libass le (Noto Naskh incluso). `wasm` e o
 * __EMSCRIPTEN__: la SDL_GetBasePath devolve "/" e as fontes vivem em /app/fonts. */
void assrender_pasta_fontes_app(const char *base, int wasm, char *out, size_t n) {
  if (n == 0) return;
  out[0] = 0;
  if (wasm) { snprintf(out, n, "%s", "/app/fonts"); return; }
  if (base) snprintf(out, n, "%sfonts", base);
  if (access(out, R_OK)) snprintf(out, n, "%s", "deploy/app/fonts");
}

/* #369: o libass nao cai para "qualquer fonte com o glifo" quando as fontes sao as
 * da memoria (sem fontconfig: Android, .tpk, wasm): o tailandes de uma trilha
 * ASS com estilo "Arial"/Inter saia em .notdef mesmo com o NotoSansThai ja
 * entregue. Cada corrida de tailandes (U+0E00-0E7F, E0 B8/B9 xx) fora de chaves
 * de override ganha {\fnNoto Sans Thai} ... {\fn}, o mesmo expediente do texto
 * simples arabe (plainass.c). So corre se a fonte chegou ao libass. */
static int ass_byte_tailandes(const unsigned char *p) {
  return p[0] == 0xE0 && (p[1] == 0xB8 || p[1] == 0xB9) && (p[2] & 0xC0) == 0x80;
}
char *assrender_marcar_tailandes(const char *txt) {
  static const char ini[] = "{\\fnNoto Sans Thai}", fim[] = "{\\fn}";
  size_t n = strlen(txt), cap = n + 1, i, o = 0;
  int achou = 0, chaves = 0, em = 0;
  char *out;
  for (i = 0; i + 2 < n; i++)
    if (ass_byte_tailandes((const unsigned char *)txt + i)) { achou = 1; cap += sizeof ini + sizeof fim; i += 2; }
  if (!achou) return NULL;
  if (!(out = (char *)malloc(cap))) return NULL;
  for (i = 0; i < n;) {
    if (txt[i] == '{') chaves = 1;
    if (txt[i] == '}') chaves = 0;
    if (!chaves && i + 2 < n && ass_byte_tailandes((const unsigned char *)txt + i)) {
      if (!em) { memcpy(out + o, ini, sizeof ini - 1); o += sizeof ini - 1; em = 1; }
      memcpy(out + o, txt + i, 3); o += 3; i += 3; continue;
    }
    if (em) { memcpy(out + o, fim, sizeof fim - 1); o += sizeof fim - 1; em = 0; }
    out[o++] = txt[i++];
  }
  if (em) { memcpy(out + o, fim, sizeof fim - 1); o += sizeof fim - 1; }
  out[o] = 0;
  return out;
}

#ifdef NV_ASS_LIBASS
#include <SDL2/SDL.h>
#include <ass/ass.h>
#include "asssinais.h"
/* Split passes (Auto 50/75, busy scenes) and the signs & songs flag: the .tpk's patched
 * libass only (tools/tpk/libass-nv, 0004). Elsewhere one pass, as before. */
#if defined(NV_TPK) && defined(ASS_NV_SPLIT)
#define ASS_DIVIDIR 1
#else
#define ASS_DIVIDIR 0
#endif
#include "gfx.h"
#include "bidi.h"
#include "assperf.h"
#include "assfios.h"
#ifdef NV_TPK
#include "video.h"    // video_tpk_leituras ("[ass] 10s tv")
#include "memlog.h"
#include "mkvfix.h"   // mkvfix_medidas ("[mkvfix] 10s")
#endif
#if !defined(__EMSCRIPTEN__)
#include <sys/resource.h>
#include <sys/syscall.h>
#include <signal.h>
#endif

#ifdef NV_TPK
/* How many CPUs this process may actually run on, not how many the TV has:
 * a Tizen 4/5 sandbox starts the app inside a cpuset pinned to one CPU while
 * sysconf still reports all of them. */
static int ass_usable_cpus(void) {
#if defined(__linux__) && defined(SYS_sched_getaffinity)
  unsigned long mask[16];
  memset(mask, 0, sizeof mask);
  long r = syscall(SYS_sched_getaffinity, 0, sizeof mask, mask);
  if (r > 0) {
    int n = 0;
    size_t words = (size_t)r / sizeof mask[0];
    if (words > sizeof mask / sizeof mask[0]) words = sizeof mask / sizeof mask[0];
    for (size_t i = 0; i < words; i++) n += __builtin_popcountl(mask[i]);
    if (n > 0) return n;
  }
#endif
  long onln = sysconf(_SC_NPROCESSORS_ONLN);
  return onln > 0 ? (int)onln : 1;
}
#endif

#define ASS_TEX_INICIAL 64

typedef struct {
  GLuint tex;
  int w, h;
} AssTex;


typedef struct {
  int x, y, w, h, stride, type, rgba;
  unsigned color;
  unsigned char *bitmap;
  unsigned long long chave;   // assperf: conteudo (bitmap + w + h); 0 = nao calculada
  float esc;                  // the soft (blur) pass: its own pixels per layout unit; 0 = the frame's
  float subX, subY;           // fractional offset in this image's pixel space (rigid cache)
} AssCpuImage;

typedef struct {
  AssCpuImage *images;
  int count;
  unsigned generation, serial;
  size_t bytes;
  long long ms;          // instante (relogio do video) que este quadro mostra
  long long inicioMax;   // maior Start dos eventos vivos em ms; -1 sem evento
  float ox, oy;          // canto da area do video na tela quando foi renderizado
  int k;                 // pixels do alvo por unidade de layout: o quadro foi renderizado k vezes maior (#335)
  float esc;             // assperf: pixels do quadro por unidade de layout (k x resolucao); 0 = k
} AssCpuFrame;

static pthread_mutex_t assTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t assFilaTrava = PTHREAD_MUTEX_INITIALIZER;
/* GL thread locks (log 14, 23/11/2026): the GL thread must never wait for a
 * render. Every lock it takes per frame goes through here; a wait is counted
 * for "[ass] 10s draw: ... lock waits N (max X ms)", so a regression shows. */
static long glTravaEsperas;          // __atomic
static long long glTravaMaxUs;       // __atomic
static void ass_gl_trava(pthread_mutex_t *m) {
  struct timespec t0, t1;
  long long us, mx;
  if (pthread_mutex_trylock(m) == 0) return;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  pthread_mutex_lock(m);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  us = (long long)(t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_nsec - t0.tv_nsec) / 1000;
  __atomic_add_fetch(&glTravaEsperas, 1, __ATOMIC_RELAXED);
  mx = __atomic_load_n(&glTravaMaxUs, __ATOMIC_RELAXED);
  while (us > mx && !__atomic_compare_exchange_n(&glTravaMaxUs, &mx, us, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
}
#ifdef NV_TPK
static long long assProxInicio = LLONG_MAX;   // worker: proxima fala depois do ultimo render (ms)
static Uint32 assBordaQuerTick;                // fio GL: ultimo quadro em que o libass desenhou / ia desenhar
#endif
static pthread_cond_t assFilaCond = PTHREAD_COND_INITIALIZER;
static pthread_t assWorker;
static int assWorkerCriado, assTrackAtivo, assWorkerParar, assPedidoPendente;
static int assTextoSimples;
static PlainAssStyle plainPed, plainApl;
static int plainPedValido, plainAplValido;
static ASS_Style *authoredStyles;
static int authoredStyleCount;

static void ass_authored_styles_free(void) {
  for (int i = 0; i < authoredStyleCount; i++) free(authoredStyles[i].FontName);
  free(authoredStyles); authoredStyles = NULL; authoredStyleCount = 0;
}
/* Protected by assTrava. Re-evaluate active cues only at a timing boundary
 * or after seeking, not on every video frame. */
static int plainTimingValid;
static long long plainLastTime, plainNextBoundary;
static uint64_t plainActiveSet;
static int assAtualValido;
static double assPedidoMs;
static unsigned assPedidoGeracao, assPedidoSerial, assSerial;
static unsigned assEpoch;
/* FILA DOS QUADROS PRONTOS (assperf, render ahead). Era um quadro so
 * (assPronto): o novo apagava o anterior, e o desenho mostrava o que chegasse.
 * Com "Hold until due" o quadro renderizado para daqui a 80 ms espera a sua
 * vez; enquanto isso o worker ja faz os seguintes, entao cabe mais de um.
 * Sem hold a fila tem teto 1 e e o comportamento de sempre. Sob assFilaTrava,
 * em ordem de pedido (ms crescente dentro de um epoch). */
#define ASS_FILA_MAX 4
/* 22/11: 64 MB (was 24; RAM has headroom, 4K-UI flattened frames). */
#define ASS_FILA_BYTES (64u * 1024u * 1024u)
static AssCpuFrame assFila[ASS_FILA_MAX];
static int assFilaN;
static int assFilaTeto = 1;      // 1 = como antes; ASS_FILA_MAX com hold
static AssCpuFrame assAtual;
/* assperf "Render only on changes": o ultimo render vale para [de, ate) — nenhum
 * evento comeca, acaba ou anima nesse trecho. Sob assFilaTrava. */
static long long assValidoDe = LLONG_MAX, assValidoAte = LLONG_MIN;
static double assUltimoVistoMs;      // o ultimo instante que o desenho pediu OU pulou
static unsigned assUltimoVistoGeracao;
static unsigned assValidoEpoch, assValidoGeracao;
static double assUltimoPedidoMs;
static unsigned assUltimoPedidoGeracao;
static int assTemUltimoPedido;
/* QUADRO A QUADRO (dono 26/10, letreiros que seguem o dedo dela aos 19:00).
 * O worker renderizava so o ULTIMO pedido: com um render de mais de um quadro
 * de video (42 ms a 24 fps) o pedido ja tinha andado e o quadro do meio nunca
 * existia — o letreiro do quadro anterior ficava 1-3 quadros na tela (no
 * chapeu / em cima dos dedos). Agora, com a fila do Render ahead, o worker faz
 * TODO quadro da grade, do ultimo que fez ate o pedido; pula so os que ja
 * passaram da tela. Sob assFilaTrava. */
static long long assSeqUltMs = LLONG_MIN;      // o ultimo quadro que o worker fez
static unsigned assSeqEpoch, assSeqGeracao;
static long long assAgoraUiMs = LLONG_MIN;     // o quadro em tela agora (desenho)
static unsigned assAgoraUiGeracao;
static int assPedidoForcado;                   // documento/tela mudou: nada de pular pela validade
static double assRenderPico;                   // ms: pico recente do render (cai 5 % por render), __atomic
static int assAdianteMax;                      // ms: largest render-ahead asked in the 10 s window ("[ass] 10s tv"), __atomic
static int assEventos, assFontes, assResolucaoFonte;
static long long assCoberturaIni, assCoberturaFim;
static ASS_Library *assLib;
static ASS_Renderer *assRenderer;
/* CUSTOM AUTO (owner 19/11/2026): the soft pass (blurred borders, glows,
 * drawings) at the blur resolution, on the same track; created at the first
 * busy frame that needs it, never on the other modes. assRenderer then draws
 * the sharp pass at the text resolution. Under assTrava. */
static ASS_Renderer *assRendererB;
static int layBW, layBH;
/* Split stats for the "[ass] 10s tv" line (worker only): frames split, the
 * two passes' time, images each pass gave. */
static struct { long quadros; double msA, msB, maxA, maxB; long imgsA, imgsB; long umaPassada; } assDiv;
/* A frame of more images than this is overloaded: flattened even when split
 * (ass_frame_flatten), and the next one renders in one pass (below). */
#define ASS_FLAT_SOBRECARGA 300
/* Overload frames in one pass (log 14, 23/11/2026). Fate/strange Fake's OP
 * (~1000-1200 active events, 600-1800 images a frame) costs ~240 ms split
 * (text pass 68-76 + blur pass 97-108 ms) and ~170 ms in one pass at the blur
 * resolution (what Low auto does). After a frame over ASS_FLAT_SOBRECARGA
 * images, the next one renders in one pass at the blur resolution; decided
 * per frame like the flatten, so a frame under the limit puts the next one
 * back on the normal split. Normal busy scenes (Kaguya: 12-57 images) never
 * get here. The image count is only known after rendering, hence the last
 * frame's. */
static int assUltimoImgs;   // worker writes, __atomic: images of the last published frame (before the flatten)
static inline void ass_ultimo_imgs_guardar(int n) { __atomic_store_n(&assUltimoImgs, n, __ATOMIC_RELAXED); }
static inline int ass_uma_passada_sobrecarga(int dividir, double resolB) {
  return dividir && resolB > 0.0 && __atomic_load_n(&assUltimoImgs, __ATOMIC_RELAXED) > ASS_FLAT_SOBRECARGA;
}
static ASS_Track *assTrack;
static unsigned assTrackMut;   // bumped wherever assTrack's events change (the time index rebuilds)
static unsigned assTrackGeracao;
static AssTex *assTex;
static int assTexCap;
static int assTexResetar;
static int assFrameW, assFrameH;
/* AREA DO VIDEO. O libass desenhava num quadro 1920x1080 fixo, como se o video
 * ocupasse a tela inteira: num 4:3 uma placa em \pos(64,48) caia em x=195,
 * DENTRO da barra preta (o video comeca em x=240); num 2.39:1 caia na barra de
 * cima e 33 % maior. O quadro do libass agora e o retangulo em que o video
 * aparece, e o storage e o tamanho do quadro decodificado — o que o mpv faz e
 * o que o ass.js do app web faz com resampling "video_height".
 * O fio grafico PEDE (layPed*, sob assFilaTrava) e o worker APLICA antes do
 * proximo render (layApl*, sob assTrava): o fio grafico nunca espera um
 * ass_render_frame para mudar o layout. */
static float layPedX, layPedY;
static int layPedW = 1920, layPedH = 1080, layPedVW = 1920, layPedVH = 1080;
static double layPedEscala = 1.0;
static int layPedK = 1;
static int layAplW, layAplH, layAplVW, layAplVH;
static double layAplEscala;
static unsigned assGeracao;
static int assCorAtiva, assCorR, assCorG, assCorB;
static unsigned long long assUltimoRenderUs;
static size_t assBytesQuadro;
/* Quadro PUBLICADO por ultimo (epoch + geracao): com o `changed` do libass em
 * zero, o quadro novo e identico a ele e nao precisa ser copiado, publicado
 * nem reenviado a GPU. */
static unsigned assProduzidoEpoch, assProduzidoGeracao;
static int assProduziu;
/* Textura: serial do quadro cujas imagens estao nas texturas e a cor usada.
 * Reenviar tudo a cada quadro (o que se fazia) custa a conversao RGBA de
 * todos os glifos 60 vezes por segundo. */
static unsigned assTexSerial;
static int assTexCorChave = -1;
/* Medidas (#92): o dono ve "atrasada e piscando"; o log precisa dizer quanto. */
static struct {
  long quadros, trocas, renders, iguais, piscas, vaziosPisca, falas;
  unsigned long long renderSomaUs, renderMaxUs;
  long long atrasoSoma, atrasoMax;
  int textoAnterior, vazios;
  long long ultimoTextoMs, ultimoInicioLogado;
  double ultimoRelatorio;
} assMed;
static pthread_mutex_t assDiagTrava = PTHREAD_MUTEX_INITIALIZER;
static char assDiag[160] = "libass pronto";
static int assGlifoMsgs;   // assperf: mensagens de glifo ausente desta faixa (sob assTrava)

static void ass_diag(const char *s) {
  pthread_mutex_lock(&assDiagTrava);
  snprintf(assDiag, sizeof assDiag, "%s", s ? s : "");
  pthread_mutex_unlock(&assDiagTrava);
}

/* LATE ATTACHED FONTS (owner 18/11, log 10). libass keeps the face it picked
 * for a (family, bold, italic) in its font cache: a family looked up before the
 * MKV's fonts arrived stays on the fallback (Arial) even after they are added.
 * Fallbacks since the last ass_set_fonts + an attached font added since then
 * -> the next load rebuilds the selector (ass_aplicar_fontes_locked), once. */
static int assFallbacks;      // "Using default font" since the last ass_set_fonts (atomic)
static int assFonteNovaTarde; // an attached font was added since the last ass_set_fonts (assTrava)

static void ass_mensagem(int nivel, const char *fmt, va_list args, void *dados) {
  char linha[768];
  (void)dados;
  vsnprintf(linha, sizeof linha, fmt, args);
  if (strstr(linha, "fontselect:")) assResolucaoFonte++;
  /* libass 0.17.5 (the .tpk's) says "fontselect: Using default font: (...)";
   * older ones "Using default font family". Log 19 (owner 24/11/2026, Kaguya):
   * only the old wording was matched, so the late-font rebuild never ran and
   * fallen-back families stayed on the app's font until the app restarted. */
  if (strstr(linha, "Using default font")) __atomic_add_fetch(&assFallbacks, 1, __ATOMIC_RELAXED);
  /* So ate MSGL_INFO (4), o mesmo corte do callback padrao do libass (que
   * para em 5). Os niveis 6-7 sao depuracao por quadro e por glifo. */
  if (nivel > 4) return;
#ifdef NV_TPK
  /* Clean log: the "Glyph ... not found / selecting one more font / Trying
   * all charmaps" loop is three lines PER GLYPH (60+ in the 23/10 log). The
   * first 6 per track stay. */
  if ((strstr(linha, "Glyph 0x") || strstr(linha, "already is available")) && ++assGlifoMsgs > 6) return;
#endif
  fprintf(stderr, "[libass] %s\n", linha);
}

/* FONTES ANEXADAS JA ENTREGUES AO libass (#92, queda na C9 em 22/09/2026).
 *
 * O libass NAO aceita esvaziar a lista de fontes e seguir renderizando.
 * ass_start_frame guarda quantas fontes ja passou ao fontselect
 * (num_emfonts) e, quando a biblioteca muda de tamanho, afirma que so CRESCEU:
 *   assert(library->num_fontdata > num_emfonts)
 * ass_clear_fonts zera num_fontdata e deixa num_emfonts como estava; a proxima
 * fonte anexada (ou nenhuma) deixa a conta menor, e o quadro seguinte ABORTA o
 * app. MEDIDO: tests/ass_fontes.sh reproduz com o libass 0.17.5 do Homebrew, e
 * o log da C9 morre em "ass_render.c:3069: ass_start_frame: Assertion" logo
 * depois da SEGUNDA entrega do mkvass — que e quem chamava
 * assrender_limpar_fontes a cada lote.
 *
 * O remedio "limpar e chamar ass_set_fonts de novo" zera a conta, mas o
 * fontselect novo rele a pasta de fontes: na C9 e /usr/share/fonts, 106 MB e
 * 29 arquivos carregados na memoria — a cada lote do mkvass e a cada troca de
 * faixa. Entao a lista SO CRESCE: cada fonte entra uma vez (nome + tamanho +
 * amostra do conteudo) e fica para as proximas faixas. So quando o acumulado
 * passa do teto e que a biblioteca e reiniciada por inteiro, com ass_set_fonts
 * logo depois do ass_clear_fonts — o caro fica no caso raro. */
typedef struct { char nome[96]; size_t tam; unsigned long long amostra; } AssFonteVista;
#define ASS_FONTES_MAX 192
#define ASS_FONTES_TETO_BYTES (96u * 1024u * 1024u)
static AssFonteVista assFontesVistas[ASS_FONTES_MAX];
static int assNFontesVistas;
static size_t assBytesFontesVistas;
static char assFallbackFont[768], assFonteLocal[768];
static int assFonteRegistrada;
/* Copy of assFonteLocal / assFonteRegistrada for the GL thread (log 14,
 * 23/11/2026): assrender_fonte_local_ativa took assTrava every UI frame and
 * waited for the whole libass render (150-430 ms on Fate). Written under
 * assTrava after every change; its own lock is never held during a render. */
static pthread_mutex_t assFonteEspelhoTrava = PTHREAD_MUTEX_INITIALIZER;
static char assFonteEspelho[768];
static int assFonteEspelhoOk;
static void ass_fonte_espelho_locked(void) {
  pthread_mutex_lock(&assFonteEspelhoTrava);
  snprintf(assFonteEspelho, sizeof assFonteEspelho, "%s", assFonteLocal);
  assFonteEspelhoOk = assFonteRegistrada;
  pthread_mutex_unlock(&assFonteEspelhoTrava);
}
#define ASS_ATUAIS_MAX 32
#define ASS_ATUAIS_BYTES (32u * 1024u * 1024u)
typedef struct { char nome[256]; unsigned char *dados; size_t tam; } AssFonteAtual;
static AssFonteAtual assFontesAtuais[ASS_ATUAIS_MAX];
static int assNFontesAtuais, assAtuaisCompletas = 1;
static size_t assBytesAtuais;
static void ass_atuais_limpar_locked(void) {
  for (int i = 0; i < assNFontesAtuais; i++) free(assFontesAtuais[i].dados);
  memset(assFontesAtuais, 0, sizeof assFontesAtuais);
  assNFontesAtuais = 0; assBytesAtuais = 0; assAtuaisCompletas = 1;
}
static void ass_atual_guardar_locked(const char *nome, const void *dados, size_t n) {
  if (!assAtuaisCompletas) return;
  for (int i = 0; i < assNFontesAtuais; i++) {
    const AssFonteAtual *f = &assFontesAtuais[i];
    if (f->tam == n && !strcmp(f->nome, nome) && !memcmp(f->dados, dados, n)) return;
  }
  unsigned char *copia = NULL;
  if (strlen(nome) < sizeof assFontesAtuais[0].nome && assNFontesAtuais < ASS_ATUAIS_MAX &&
      n <= ASS_ATUAIS_BYTES - assBytesAtuais) copia = malloc(n);
  if (!copia) {
    ass_atuais_limpar_locked(); assAtuaisCompletas = 0;
    return; /* Keep the original renderer path; never replay a partial batch. */
  }
  AssFonteAtual *f = &assFontesAtuais[assNFontesAtuais++];
  memcpy(copia, dados, n); f->dados = copia; f->tam = n;
  snprintf(f->nome, sizeof f->nome, "%s", nome); assBytesAtuais += n;
}
static unsigned char *ass_fonte_local_ler(const char *path, size_t *n) {
  struct stat st; *n = 0;
  if (lstat(path, &st) || !S_ISREG(st.st_mode) || st.st_size < 12 ||
      (unsigned long long)st.st_size > LEGFONTE_MAX_BYTES) return NULL;
  FILE *f = fopen(path, "rb"); if (!f) return NULL;
  unsigned char *b = malloc((size_t)st.st_size);
  if (b && fread(b, 1, (size_t)st.st_size, f) == (size_t)st.st_size &&
      fgetc(f) == EOF && !ferror(f) && assrender_bytes_sao_fonte(b, (size_t)st.st_size)) *n = (size_t)st.st_size;
  fclose(f);
  if (!*n) { free(b); b = NULL; }
  return b;
}

static unsigned long long ass_amostra_fonte(const unsigned char *p, size_t n) {
  unsigned long long h = 1469598103934665603ULL;
  size_t i, pedaco = n < 65536u ? n : 65536u;
  for (i = 0; i < pedaco; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  for (i = n - pedaco; i < n; i++) { h ^= p[i]; h *= 1099511628211ULL; }
  return h;
}

/* Pasta das fontes do sistema, lida por assrender_ler_pasta_fontes em vez de
 * ass_set_fonts_dir. Guardada porque ass_clear_fonts (teto de fontes anexadas)
 * tira estas junto, e elas precisam voltar antes do ass_set_fonts seguinte. */
static char assPastaFontes[640], assPastaFontesApp[640];
static int assUsaSistema;
static int assVistoNaskh;          /* NotoNaskhArabic-Regular chegou ao libass nesta carga */
static int assNaskhBold;           /* NotoNaskhArabic-Bold chegou ao libass nesta carga (#335) */
static int assVistoThai;           /* NotoSansThai-Regular chegou ao libass nesta carga (#369) */
static char assPastaExtra[640];    /* NV_TPK: pasta de fontes baixadas (dados/fontes) */
static void ass_fonte_da_pasta(const char *nome, const void *dados, size_t tam, void *u) {
  (void)u;
  if (!strcmp(nome, "NotoNaskhArabic-Regular.ttf")) assVistoNaskh = 1;
  if (!strcmp(nome, "NotoNaskhArabic-Bold.ttf")) assNaskhBold = 1;
  if (!strcmp(nome, "NotoSansThai-Regular.ttf")) assVistoThai = 1;
  /* The UI subset shares the family name but lacks mixed-script glyphs.
   * Plain subtitles use the verified complete face; leave UI selection intact. */
  if (!strcmp(nome, "NotoNaskhArabic-Subset.ttf")) return;
  ass_add_font(assLib, nome, (const char *)dados, (int)tam);
}
#ifdef NV_TPK
static int assNaskhFalhou;         /* a completa nao veio: vale o recorte da interface */
static void ass_subset_naskh_locked(void) {
  /* Sem a completa (sem rede, tag ausente) o recorte da interface ainda tem as
   * letras: sem juncao, mas legiveis, em vez de quadrados. */
  char r[700]; FILE *f; long n = 0; char *b;
  if (!assLib || !assPastaFontesApp[0]) return;
  snprintf(r, sizeof r, "%s/NotoNaskhArabic-Subset.ttf", assPastaFontesApp);
  if (!(f = fopen(r, "rb"))) return;
  fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
  if (n > 4 && n < (1L << 22) && (b = malloc((size_t)n))) {
    if (fread(b, 1, (size_t)n, f) == (size_t)n) {
      ass_add_font(assLib, "NotoNaskhArabic-Subset.ttf", b, (int)n);
      fprintf(stderr, "[libass] sem NotoNaskhArabic-Regular: usando o recorte da interface\n");
    }
    free(b);
  }
  fclose(f);
}
#endif
static void ass_carregar_pasta_locked(void) {
  int ignorados = 0, lidas;
  if (!assLib || !assPastaFontes[0]) return;
  assNaskhBold = 0;
  assVistoThai = 0;
  lidas = assUsaSistema ? assrender_ler_pasta_fontes(assPastaFontes, ass_fonte_da_pasta, NULL, &ignorados) : 0;
  assVistoNaskh = 0;
  if (assPastaFontesApp[0] && (!assUsaSistema || strcmp(assPastaFontesApp, assPastaFontes)))
    assrender_ler_pasta_fontes(assPastaFontesApp, ass_fonte_da_pasta, NULL, NULL);
#ifdef NV_TPK
  /* O res/ do .tpk e o da INSTALACAO (a auto-atualizacao troca so a .so), entao
   * o NotoNaskhArabic-Regular.ttf, novo na 2.0.2, falta em quem atualizou por
   * dentro do app: o libass nao achava "Noto Naskh Arabic" e caia na Inter, que
   * nao tem arabe (quadradinhos). A copia baixada da tag fica em dados/fontes. */
  if (assPastaExtra[0]) assrender_ler_pasta_fontes(assPastaExtra, ass_fonte_da_pasta, NULL, NULL);
  if (!assVistoNaskh && assNaskhFalhou) ass_subset_naskh_locked();
#endif
  plainass_arabic_bold = assNaskhBold;
  fprintf(stderr, "[libass] pasta %s: %d fonte(s); %d arquivo(s) que nao sao fonte ignorado(s)\n",
          assPastaFontes, lidas, ignorados);
}

static void ass_aplicar_fontes_locked(void) {
  if (!assRenderer) return;
  ass_set_fonts(assRenderer, assFallbackFont[0] ? assFallbackFont : NULL, "Arial",
                ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
  if (assRendererB)
    ass_set_fonts(assRendererB, assFallbackFont[0] ? assFallbackFont : NULL, "Arial",
                  ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
  __atomic_store_n(&assFallbacks, 0, __ATOMIC_RELAXED);
  assFonteNovaTarde = 0;
}

// Explicit selection changes only. Routine packet delivery keeps the original
// font cache path; a full current batch lets us restore attachment precedence.
static void ass_fontes_reconstruir_locked(const unsigned char *selecionada, size_t n) {
  ass_clear_fonts(assLib);
  if (selecionada) ass_add_font(assLib, assFonteLocal, (const char *)selecionada, (int)n);
  assFonteRegistrada = selecionada != NULL;
  ass_fonte_espelho_locked();
  ass_carregar_pasta_locked();
  assNFontesVistas = 0; assBytesFontesVistas = 0;
  for (int i = 0; i < assNFontesAtuais; i++) {
    const AssFonteAtual *f = &assFontesAtuais[i];
    ass_add_font(assLib, f->nome, (const char *)f->dados, (int)f->tam);
    AssFonteVista *v = &assFontesVistas[assNFontesVistas++];
    snprintf(v->nome, sizeof v->nome, "%.95s", f->nome);
    v->tam = f->tam; v->amostra = ass_amostra_fonte(f->dados, f->tam);
    assBytesFontesVistas += f->tam;
  }
  ass_aplicar_fontes_locked();
}

static void ass_iniciar_locked(int sistema) {
  char fontDir[640] = "";
  char fallbackFont[768] = "";
  struct timespec t0, t1;
  if (assLib) {
    if (sistema && !assUsaSistema) {
      assUsaSistema = 1; ass_carregar_pasta_locked(); ass_aplicar_fontes_locked();
    }
    return;
  }
  assUsaSistema = sistema;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  assLib = ass_library_init();
  if (!assLib) { ass_diag("libass: falha ao iniciar biblioteca"); return; }
  ass_set_message_cb(assLib, ass_mensagem, NULL);
  ass_set_extract_fonts(assLib, 1);
  assRenderer = ass_renderer_init(assLib);
  if (!assRenderer) {
    ass_library_done(assLib); assLib = NULL;
    ass_diag("libass: falha ao iniciar renderer"); return;
  }
  ass_set_shaper(assRenderer, ASS_SHAPING_COMPLEX);
  /* Fontes anexadas continuam tendo prioridade. A pasta da plataforma ajuda
   * o libass a resolver familias instaladas; Inter, distribuida com o app,
   * evita texto vazio quando a familia pedida nao existe. */
#ifdef __EMSCRIPTEN__
  snprintf(fontDir, sizeof fontDir, "%s", "/usr/share/fonts");
  snprintf(fallbackFont, sizeof fallbackFont, "%s", "/app/fonts/InterDisplay-Regular.ttf");
  if (access(fontDir, R_OK) != 0) snprintf(fontDir, sizeof fontDir, "%s", "/app/fonts");
#else
  {
    char *base = SDL_GetBasePath();
    if (base) {
      snprintf(fontDir, sizeof fontDir, "%sfonts", base);
      snprintf(fallbackFont, sizeof fallbackFont, "%sfonts/InterDisplay-Regular.ttf", base);
      SDL_free(base);
    }
    if (access(fontDir, R_OK) != 0)
      snprintf(fontDir, sizeof fontDir, "%s", "deploy/app/fonts");
    if (access(fallbackFont, R_OK) != 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "deploy/app/fonts/InterDisplay-Regular.ttf");
#ifdef NV_ANDROID
    /* Android: a pasta de fontes e a que o NuvioActivity extraiu (<arte>/../fonts,
     * ja achada acima). Nao ha /usr/share/fonts; o sistema entra so como
     * ultimo recurso para a fonte de reserva. */
    {
      const char *env = getenv("NUVIO_ARTE");
      if (env && env[0]) {
        char r[512];
        snprintf(r, sizeof r, "%s/../fonts", env);
        if (access(r, R_OK) == 0) {
          snprintf(fontDir, sizeof fontDir, "%s", r);
          snprintf(fallbackFont, sizeof fallbackFont, "%s/InterDisplay-Regular.ttf", r);
        }
      }
    }
    if (access(fallbackFont, R_OK) != 0 && access("/system/fonts/Roboto-Regular.ttf", R_OK) == 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "/system/fonts/Roboto-Regular.ttf");
#elif defined(NV_TPK)
    /* .tpk (Samsung): SDL_GetBasePath e o diretorio do host .NET, nao o do
     * pacote. O host passa a arte em NUVIO_TPK_ARTE (src/tpk.c) e fonts/ fica
     * ao lado de art/ (a mesma conta do txt_iniciar). Sem fontconfig e sem
     * pasta de sistema: as fontes sao as do app, entregues ao libass em memoria. */
    {
      const char *env = getenv("NUVIO_TPK_ARTE");
      if (env && env[0]) {
        char r[640];
        snprintf(r, sizeof r, "%s/../fonts", env);
        if (access(r, R_OK) == 0) {
          snprintf(fontDir, sizeof fontDir, "%s", r);
          snprintf(fallbackFont, sizeof fallbackFont, "%s/InterDisplay-Regular.ttf", r);
        }
      }
    }
#else
    if (access("/usr/share/fonts", R_OK) == 0)
      snprintf(fontDir, sizeof fontDir, "%s", "/usr/share/fonts");
    if (access(fallbackFont, R_OK) != 0 && access("/usr/share/fonts/LG_Display-Regular.ttf", R_OK) == 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "/usr/share/fonts/LG_Display-Regular.ttf");
    if (access(fallbackFont, R_OK) != 0 && access("/usr/share/fonts/DroidSans.ttf", R_OK) == 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "/usr/share/fonts/DroidSans.ttf");
#endif
  }
#endif
  { char *base = SDL_GetBasePath();
#ifdef __EMSCRIPTEN__
    assrender_pasta_fontes_app(base, 1, assPastaFontesApp, sizeof assPastaFontesApp);
#else
    assrender_pasta_fontes_app(base, 0, assPastaFontesApp, sizeof assPastaFontesApp);
#endif
    if (base) SDL_free(base);
#ifdef NV_TPK
    if (fontDir[0] && strstr(fontDir, "/../fonts")) snprintf(assPastaFontesApp, sizeof assPastaFontesApp, "%s", fontDir);
#endif
  }
  snprintf(assPastaFontes, sizeof assPastaFontes, "%s", fontDir);
  if (assFonteLocal[0]) {
    size_t n; unsigned char *b = ass_fonte_local_ler(assFonteLocal, &n);
    if (b) { ass_add_font(assLib, assFonteLocal, (const char *)b, (int)n); assFonteRegistrada = 1; }
    else assFonteRegistrada = 0;
    free(b);
    ass_fonte_espelho_locked();
  }
  ass_carregar_pasta_locked();
  snprintf(assFallbackFont, sizeof assFallbackFont, "%s", fallbackFont);
  ass_aplicar_fontes_locked();
  assNFontesVistas = 0; assBytesFontesVistas = 0;
  ass_set_cache_limits(assRenderer, 0, 96);   /* 22/11: RAM has headroom (owner); was 32 */
  assFrameW = 1920; assFrameH = 1080;
  ass_set_frame_size(assRenderer, assFrameW, assFrameH);
  ass_set_storage_size(assRenderer, assFrameW, assFrameH);
  ass_set_font_scale(assRenderer, 1.0);
  ass_set_hinting(assRenderer, ASS_HINTING_LIGHT);   /* explicito: nao depender do padrao da versao */
  layAplW = layAplVW = assFrameW; layAplH = layAplVH = assFrameH; layAplEscala = 1.0;
  clock_gettime(CLOCK_MONOTONIC, &t1);
  /* Uma vez por sessao; na C9 a pasta tem 106 MB de fontes. */
  printf("[ass] libass iniciado em %ld ms (fontes de %s)\n",
         (long)((t1.tv_sec - t0.tv_sec) * 1000L + (t1.tv_nsec - t0.tv_nsec) / 1000000L),
         fontDir[0] ? fontDir : "-");
  fflush(stdout);
}

static void ass_frame_liberar(AssCpuFrame *frame) {
  int i;
  for (i = 0; i < frame->count; i++) free(frame->images[i].bitmap);
  free(frame->images);
  memset(frame, 0, sizeof *frame);
}

/* Fila dos prontos (ver assFila). Todas sob assFilaTrava. */
static void ass_fila_limpar_locked(void) {
  int i;
  for (i = 0; i < assFilaN; i++) ass_frame_liberar(&assFila[i]);
  assFilaN = 0;
}
static size_t ass_fila_bytes_locked(void) {
  size_t b = 0; int i;
  for (i = 0; i < assFilaN; i++) b += assFila[i].bytes;
  return b;
}
static void ass_fila_tirar_locked(int i, AssCpuFrame *saida) {
  if (i < 0 || i >= assFilaN) return;
  if (saida) *saida = assFila[i]; else ass_frame_liberar(&assFila[i]);
  memmove(&assFila[i], &assFila[i + 1], (size_t)(assFilaN - i - 1) * sizeof assFila[0]);
  assFilaN--;
  memset(&assFila[assFilaN], 0, sizeof assFila[0]);
}
/* Publica (worker). Teto 1 = substitui, como o assPronto antigo; com mais,
 * a fila cheia perde o mais velho. */
static void ass_fila_publicar_locked(AssCpuFrame *f) {
  int teto = assFilaTeto < 1 ? 1 : assFilaTeto > ASS_FILA_MAX ? ASS_FILA_MAX : assFilaTeto, i, pos;
  /* EM ORDEM DE TEMPO (log 13): um lote novo do mkvass faz o worker refazer
   * quadros que ja estavam na fila; o refeito ia para o fim e o desenho (o
   * ultimo devido) mostrava o quadro anterior por um quadro de video. O mesmo
   * instante substitui; o resto entra no lugar dele. */
  for (i = 0; i < assFilaN; i++)
    if (assFila[i].ms == f->ms && assFila[i].generation == f->generation) { ass_fila_tirar_locked(i, NULL); break; }
  while (assFilaN >= teto) ass_fila_tirar_locked(0, NULL);
  /* Quadros enormes (letreiro com blur de tela cheia): a fila nao guarda mais
   * que ASS_FILA_BYTES; sai o mais velho. */
  while (assFilaN && ass_fila_bytes_locked() + f->bytes > ASS_FILA_BYTES) ass_fila_tirar_locked(0, NULL);
  for (pos = assFilaN; pos > 0 && assFila[pos - 1].ms > f->ms; pos--) {}
  memmove(&assFila[pos + 1], &assFila[pos], (size_t)(assFilaN - pos) * sizeof assFila[0]);
  assFila[pos] = *f;
  assFilaN++;
  memset(f, 0, sizeof *f);
}

/* assperf (texcache): chave do CONTEUDO de uma imagem, para a textura de um
 * letreiro que nao mudou nao subir de novo. Palavra de 4 bytes por vez (o
 * ARMv7 da TV), linha a linha do bitmap ja compacto. Nunca 0. */
static unsigned long long ass_chave_bitmap(const unsigned char *b, int w, int h, unsigned extra) {
  unsigned long long hsh = 0x9E3779B97F4A7C15ULL ^ ((unsigned long long)(unsigned)w << 32) ^ (unsigned)h ^
                           ((unsigned long long)extra << 17);
  size_t n = (size_t)w * (size_t)h, i = 0;
  for (; i + 4 <= n; i += 4) {
    uint32_t v; memcpy(&v, b + i, 4);
    hsh = (hsh ^ v) * 0x100000001B3ULL;
    hsh ^= hsh >> 29;
  }
  for (; i < n; i++) hsh = (hsh ^ b[i]) * 0x100000001B3ULL;
  return hsh ? hsh : 1;
}


/* Split frames (owner 19/11/2026): the sharp pass `a` and the soft pass `b`
 * merged in libass's stacking order (ass_nv_image_key, libass-nv 0004: both
 * lists are already in key order; ties keep `a` first). b's images carry
 * their own scale (escB pixels per layout unit). b NULL = one pass. */
static int ass_frame_copiar2(ASS_Image *a, ASS_Image *b, float escB, AssCpuFrame *out, int hashear) {
  ASS_Image *p, *q;
  int n = 0, i = 0;
  size_t total = 0;
  memset(out, 0, sizeof *out);
  for (p = a; p; p = p->next) {
    if (p->bitmap && p->w > 0 && p->h > 0 && p->stride >= p->w) n++;
    if (n > 4096) return 0;
  }
  for (p = b; p; p = p->next) {
    if (p->bitmap && p->w > 0 && p->h > 0 && p->stride >= p->w) n++;
    if (n > 4096) return 0;
  }
  if (n) {
    out->images = calloc((size_t)n, sizeof *out->images);
    if (!out->images) return 0;
    out->count = n;
  }
  p = a; q = b;
  while (p || q) {
    AssCpuImage *dst;
    ASS_Image *im;
    size_t bytes;
    int y, doB;
#if ASS_DIVIDIR
    doB = q && (!p || ass_nv_image_key(q) < ass_nv_image_key(p));
#else
    doB = !p;
#endif
    im = doB ? q : p;
    if (doB) q = q->next; else p = p->next;
    if (!im->bitmap || im->w <= 0 || im->h <= 0 || im->stride < im->w) continue;
    if (i >= n) break;
    bytes = (size_t)im->w * (size_t)im->h;
    /* Mantem o snapshot CPU de cada frame abaixo de 16 MiB. O quadro atual e
     * o pronto podem coexistir por um instante, portanto o teto total e 32 MiB. */
    if (bytes > 16u * 1024u * 1024u - total) {
      ass_frame_liberar(out); return 0;
    }
    dst = &out->images[i];
    dst->bitmap = malloc(bytes);
    if (!dst->bitmap) { ass_frame_liberar(out); return 0; }
    dst->x = im->dst_x; dst->y = im->dst_y;
    dst->w = im->w; dst->h = im->h; dst->stride = im->w;
    dst->type = im->type; dst->color = im->color;
    dst->esc = doB ? escB : 0.0f;
    for (y = 0; y < im->h; y++)
      memcpy(dst->bitmap + (size_t)y * (size_t)im->w,
             im->bitmap + (size_t)y * (size_t)im->stride, (size_t)im->w);
    /* Texture cache key (new engine). Alpha 8-bit is tinted at draw time:
     * the same bitmap serves any colour. */
    dst->chave = hashear ? ass_chave_bitmap(dst->bitmap, im->w, im->h, 0u) : 0;
    total += bytes; i++;
  }
  out->count = i; out->bytes = total;
  return 1;
}

static int ass_frame_copiar(ASS_Image *im, AssCpuFrame *out, int hashear) {
  return ass_frame_copiar2(im, NULL, 0.0f, out, hashear);
}

static void ass_trilha_tailandes(ASS_Track *t) {
  int i;
  if (!assVistoThai) return;
  for (i = 0; i < t->n_events; i++) {
    char *novo;
    if (!t->events[i].Text || !(novo = assrender_marcar_tailandes(t->events[i].Text))) continue;
    free(t->events[i].Text); t->events[i].Text = novo;
  }
}

/* The user's Border on a libass Style (owner 14/11/2026, ring 16/11/2026).
 * Outline (1) is Stremio's look: add-on subtitles drawn with 8 CSS shadows
 * in a ring, `±0.15rem ±0.15rem 0.15rem rgb(34,34,34)` (text.c sombraSuave):
 * 2.25 px out at 1080p, blurred by a Gaussian of sigma 1.125 px (a CSS blur
 * radius r is sigma r/2), whatever the subtitle size. libass's nearest is an
 * opaque 2.25 px border in the border colour; libass blurs only the BORDER
 * when there is one, so the fill stays sharp. libass sigma =
 * \blur x 2/sqrt(ln 256). The subtitle fades as one (ass_desenhar_grupo).
 * Shadow (2) and None (0) are as they were. `rgb` = the border colour. */
static void ass_borda_estilo(ASS_Style *s, const PlainAssStyle *p, int resY, double fontSize, uint32_t rgb) {
  s->Blur = 0.0;
  if (p->border == 1) {
    /* Stremio's ring (text.c sombraSuave, owner 16/11/2026): 8 shadows
     * 2.25 px out at 1080p, Gaussian sigma 1.125 px, whatever the size.
     * libass's nearest is a 2.25 px border blurred by that sigma. */
    double k = resY > 0 ? resY / 1080.0 : 1.0;
    (void)fontSize;
    s->Outline = 2.25 * k;
    s->Shadow = 0.0;
    s->Blur = 1.125 * k / (2.0 / sqrt(log(256.0)));
    s->OutlineColour = rgb << 8;   // opaque border = the ring
    return;
  }
  s->Outline = 0;
  s->Shadow = p->border == 2 ? 4 : 0;
}

static unsigned assTrackStyleVer;

static void ass_plain_style_locked(const PlainAssStyle *p) {
  ASS_Style *s;
  if (!assTrack || !assTrack->n_styles) return;
  if (plainAplValido && !memcmp(p, &plainApl, sizeof *p)) return;
  if (!assTextoSimples) {
    if (!authoredStyles) {
      authoredStyles = calloc(assTrack->n_styles, sizeof *authoredStyles);
      if (!authoredStyles) return;
      for (int i = 0; i < assTrack->n_styles; i++) {
        authoredStyles[i] = assTrack->styles[i];
        authoredStyles[i].FontName = strdup(assTrack->styles[i].FontName);
        if (!authoredStyles[i].FontName) { ass_authored_styles_free(); return; }
        authoredStyleCount++;
      }
    }
    for (int i = 0; i < authoredStyleCount; i++) {
      ASS_Style *original = &authoredStyles[i];
      char *font = strdup(p->overrides & PLAIN_ASS_FONT ? p->font : original->FontName);
      if (!font) return;
      s = &assTrack->styles[i];
      free(s->FontName); *s = *original; s->FontName = font;
      if (p->overrides & PLAIN_ASS_COLOR) s->PrimaryColour = (uint32_t)p->rgb << 8;
      if (p->overrides & PLAIN_ASS_OUTLINE) s->OutlineColour = (uint32_t)p->outlineRgb << 8;
      if (p->overrides & PLAIN_ASS_BORDER) s->BorderStyle = 1;
      if (p->overrides & PLAIN_ASS_BG) {
        s->BorderStyle = p->background ? 4 : 1;
        s->BackColour = p->background ? (unsigned)(255 - p->background * 255 * 16 / 100) : 46u;
      }
      // BackColour carries shadow RGB unless it is a background box.
      if ((p->overrides & PLAIN_ASS_OUTLINE) && s->BorderStyle == 1)
        s->BackColour = ((uint32_t)p->outlineRgb << 8) | (s->BackColour & 255u);
      /* Last: Outline's ring takes the border colour (the chosen one, or
       * the author's OutlineColour). */
      if (p->overrides & PLAIN_ASS_BORDER)
        ass_borda_estilo(s, p, assTrack->PlayResY, s->FontSize, s->OutlineColour >> 8);
      if (p->overrides & PLAIN_ASS_POS) s->MarginV = p->marginV * assTrack->PlayResY / 1080;
      if (p->overrides & PLAIN_ASS_BOLD) s->Bold = p->bold;
    }
    /* Flush cached event layout after an explicit style change or reset. */
    ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_MARGINS);
    ass_set_selective_style_override_enabled(assRenderer, 0);
    plainApl = *p; plainAplValido = 1; assTrackStyleVer++;
    return;
  }
  s = &assTrack->styles[assTrack->default_style];
  if (!s->FontName || strcmp(s->FontName, p->font)) {
    char *name = strdup(p->font);
    if (!name) return;
    free(s->FontName); s->FontName = name;
  }
  s->FontSize = p->size;
  s->PrimaryColour = s->SecondaryColour = ((uint32_t)p->rgb << 8);
  s->OutlineColour = (uint32_t)p->outlineRgb << 8;
  // BorderStyle=4 uses BackColour for the existing black background box.
  // Without that box, BackColour is the shadow, at its original opacity.
  s->BackColour = p->background ? (unsigned)(255 - (p->background * 255 * 16 / 100))
                   : (p->border == 2 ? (uint32_t)p->outlineRgb << 8 : 0u) | 46u;
  s->Bold = p->bold ? 1 : 0;
  s->BorderStyle = p->background ? 4 : 1;
  ass_borda_estilo(s, p, assTrack->PlayResY, p->size, (uint32_t)p->outlineRgb & 0xFFFFFFu);
  s->MarginV = p->marginV;
  s->MarginL = s->MarginR = 130;
  /* Direct style edits leave libass's per-event collision position cached.
   * A cue first rendered above player controls can stay raised after they
   * close. Apply margins through the public renderer setter and invalidate
   * layout even on older libass (0.17.1 doesn't invalidate in the setter).
   * This runs only when the user's/plain player style changes, not per frame. */
  ass_set_selective_style_override_enabled(assRenderer, 0);
  ass_set_selective_style_override(assRenderer, s);
  ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_MARGINS);
  plainApl = *p; plainAplValido = 1; assTrackStyleVer++;
}

#define ASS_RIGID_CACHE_MAX 1024   /* 22/11: was 384 (RAM has headroom) */
#define ASS_RIGID_FRAME_MAX 128
#define ASS_RIGID_MISS_MAX 8
#define ASS_RIGID_BYTES_MAX (96u * 1024u * 1024u)   /* 22/11: was 32 MB */

typedef struct {
  int x, y, w, h, stride;
  int isB;
  uint32_t color;
  int type;
  float esc;
  unsigned long long chave;
  unsigned long long lowKey;
  unsigned char *bitmap;
} AssRigidImg;

typedef struct {
  uint64_t sig;
  unsigned generation, styleVer;
  int lw, lh, lwB, lhB, vw, vh;
  int playResX, playResY;
  int dividir;
  double esc;
  double refPx, refPy;
  unsigned tick;
  int count;
  size_t bytes;
  AssRigidImg *imgs;
} AssRigidEntry;

typedef struct {
  uint64_t sig;
  double px, py;
  int layer, readOrder, mode;
} AssRigidHit;

typedef struct {
  ASS_Event ev;
  uint64_t sig;
  double px, py;
  int layer, readOrder, mode;
  long long t;
} AssRigidMiss;

typedef struct {
  ASS_Track *snapTrack;
  ASS_Event *snapEvents;
  int snapCap;
  ASS_Track *rigidTrack;
  ASS_Event rigidEvent;
  unsigned snapGen, snapStyleVer;
  AssRigidHit hits[ASS_RIGID_FRAME_MAX];
  int hitN;
  AssRigidMiss misses[ASS_RIGID_MISS_MAX];
  int missN;
} AssRigidCtx;

static AssRigidEntry assRigid[ASS_RIGID_CACHE_MAX];
static size_t assRigidBytes;
static unsigned assRigidTick;
static long assRigidHits, assRigidMisses;
static pthread_mutex_t assRigidTrava = PTHREAD_MUTEX_INITIALIZER;
#define ASS_RIGID_SEEN_MAX 512
static uint64_t assRigidSeen[ASS_RIGID_SEEN_MAX];
static uint64_t assRigidBanned[ASS_RIGID_SEEN_MAX];
static long long assRigidRetry[ASS_RIGID_SEEN_MAX];
static AssRigidCtx assRigidCtx;

static void ass_rigid_entry_free(AssRigidEntry *e) {
  int i;
  if (!e->imgs) return;
  for (i = 0; i < e->count; i++) free(e->imgs[i].bitmap);
  free(e->imgs);
  if (assRigidBytes >= e->bytes) assRigidBytes -= e->bytes;
  else assRigidBytes = 0;
  memset(e, 0, sizeof *e);
}

static void ass_rigid_clear(void) {
  int i;
  pthread_mutex_lock(&assRigidTrava);
  for (i = 0; i < ASS_RIGID_CACHE_MAX; i++) ass_rigid_entry_free(&assRigid[i]);
  assRigidBytes = 0;
  memset(assRigidSeen, 0, sizeof assRigidSeen);
  memset(assRigidBanned, 0, sizeof assRigidBanned);
  memset(assRigidRetry, 0, sizeof assRigidRetry);
  pthread_mutex_unlock(&assRigidTrava);
}

static int ass_rigid_should_cache(uint64_t sig, long long nowMs) {
  unsigned slot = (unsigned)((sig ^ (sig >> 32)) & (ASS_RIGID_SEEN_MAX - 1));
  int hit = 0;
  pthread_mutex_lock(&assRigidTrava);
  if (assRigidBanned[slot] == sig) {
    pthread_mutex_unlock(&assRigidTrava);
    return 0;
  }
  if (assRigidSeen[slot] == sig) {
    if (assRigidRetry[slot] > nowMs && assRigidRetry[slot] - nowMs < 2000) {
      pthread_mutex_unlock(&assRigidTrava);
      return 0;
    }
    assRigidRetry[slot] = 0;
    hit = 1;
  } else {
    assRigidSeen[slot] = sig;
    assRigidRetry[slot] = 0;
  }
  pthread_mutex_unlock(&assRigidTrava);
  return hit;
}

static void ass_rigid_defer(uint64_t sig, long long retryAfterMs) {
  unsigned slot = (unsigned)((sig ^ (sig >> 32)) & (ASS_RIGID_SEEN_MAX - 1));
  pthread_mutex_lock(&assRigidTrava);
  if (assRigidBanned[slot] != sig) {
    assRigidSeen[slot] = sig;
    assRigidRetry[slot] = retryAfterMs;
  }
  pthread_mutex_unlock(&assRigidTrava);
}

static void ass_rigid_ban(uint64_t sig) {
  unsigned slot = (unsigned)((sig ^ (sig >> 32)) & (ASS_RIGID_SEEN_MAX - 1));
  pthread_mutex_lock(&assRigidTrava);
  assRigidBanned[slot] = sig;
  assRigidRetry[slot] = 0;
  pthread_mutex_unlock(&assRigidTrava);
}

static const AssRigidEntry *ass_rigid_lookup_locked(uint64_t sig, unsigned gen, unsigned styleVer,
                                                    int lw, int lh, int lwB, int lhB,
                                                    int vw, int vh, int dividir, double esc) {
  int i;
  for (i = 0; i < ASS_RIGID_CACHE_MAX; i++) {
    AssRigidEntry *e = &assRigid[i];
    if (e->sig == sig && e->imgs && e->generation == gen && e->styleVer == styleVer &&
        e->lw == lw && e->lh == lh && e->lwB == lwB && e->lhB == lhB &&
        e->vw == vw && e->vh == vh && e->dividir == dividir && fabs(e->esc - esc) < 1e-4) {
      e->tick = ++assRigidTick;
      return e;
    }
  }
  return NULL;
}

static int ass_rigid_has(uint64_t sig, unsigned gen, unsigned styleVer,
                         int lw, int lh, int lwB, int lhB,
                         int vw, int vh, int dividir, double esc) {
  int found;
  pthread_mutex_lock(&assRigidTrava);
  found = ass_rigid_lookup_locked(sig, gen, styleVer, lw, lh, lwB, lhB, vw, vh, dividir, esc) != NULL;
  if (found) assRigidHits++;
  pthread_mutex_unlock(&assRigidTrava);
  return found;
}

static void ass_rigid_store(uint64_t sig, unsigned gen, unsigned styleVer,
                            int lw, int lh, int lwB, int lhB,
                            int vw, int vh, int playResX, int playResY,
                            int dividir, double esc,
                            double refPx, double refPy,
                            const ASS_Image *headA, const ASS_Image *headB, float escB) {
  const ASS_Image *cur;
  AssRigidImg *imgs = NULL;
  int count = 0, idx = 0, i, slot = -1;
  size_t bytes = 0;
  for (cur = headA; cur; cur = cur->next) if (cur->w > 0 && cur->h > 0 && cur->bitmap) count++;
  for (cur = headB; cur; cur = cur->next) if (cur->w > 0 && cur->h > 0 && cur->bitmap) count++;
  if (count <= 0 || count > 64) return;
  imgs = (AssRigidImg *)calloc((size_t)count, sizeof *imgs);
  if (!imgs) return;
  for (i = 0; i < 2; i++) {
    float passEsc = i == 0 ? 0.0f : escB;
    for (cur = i == 0 ? headA : headB; cur; cur = cur->next) {
      size_t sz;
      if (cur->w <= 0 || cur->h <= 0 || !cur->bitmap) continue;
      sz = (size_t)cur->w * (size_t)cur->h;
      if (bytes + sz > 4u * 1024u * 1024u) {
        int j;
        for (j = 0; j < idx; j++) free(imgs[j].bitmap);
        free(imgs);
        return;
      }
      imgs[idx].bitmap = (unsigned char *)malloc(sz);
      if (!imgs[idx].bitmap) {
        int j;
        for (j = 0; j < idx; j++) free(imgs[j].bitmap);
        free(imgs);
        return;
      }
      { int y;
        for (y = 0; y < cur->h; y++)
          memcpy(imgs[idx].bitmap + (size_t)y * (size_t)cur->w,
                 cur->bitmap + (size_t)y * (size_t)cur->stride, (size_t)cur->w); }
      imgs[idx].w = cur->w;
      imgs[idx].h = cur->h;
      imgs[idx].stride = cur->w;
      imgs[idx].x = cur->dst_x;
      imgs[idx].y = cur->dst_y;
      imgs[idx].isB = i;
      imgs[idx].color = cur->color;
      imgs[idx].type = cur->type;
      imgs[idx].esc = passEsc;
      imgs[idx].chave = ass_chave_bitmap(imgs[idx].bitmap, cur->w, cur->h, cur->w);
#if ASS_DIVIDIR
      imgs[idx].lowKey = ass_nv_image_key(cur) & 0xFFFFULL;
#else
      imgs[idx].lowKey = 0ULL;
#endif
      bytes += sz;
      idx++;
    }
  }
  pthread_mutex_lock(&assRigidTrava);
  if (ass_rigid_lookup_locked(sig, gen, styleVer, lw, lh, lwB, lhB, vw, vh, dividir, esc)) {
    pthread_mutex_unlock(&assRigidTrava);
    for (i = 0; i < idx; i++) free(imgs[i].bitmap);
    free(imgs);
    return;
  }
  while (assRigidBytes + bytes > ASS_RIGID_BYTES_MAX) {
    int oldest = -1;
    for (i = 0; i < ASS_RIGID_CACHE_MAX; i++)
      if (assRigid[i].imgs && (oldest < 0 || assRigid[i].tick < assRigid[oldest].tick)) oldest = i;
    if (oldest < 0) break;
    ass_rigid_entry_free(&assRigid[oldest]);
  }
  for (i = 0; i < ASS_RIGID_CACHE_MAX; i++) {
    if (!assRigid[i].imgs) { slot = i; break; }
    if (slot < 0 || assRigid[i].tick < assRigid[slot].tick) slot = i;
  }
  if (slot >= 0) {
    AssRigidEntry *e = &assRigid[slot];
    ass_rigid_entry_free(e);
    e->sig = sig; e->generation = gen; e->styleVer = styleVer;
    e->lw = lw; e->lh = lh; e->lwB = lwB; e->lhB = lhB;
    e->vw = vw; e->vh = vh; e->playResX = playResX; e->playResY = playResY;
    e->dividir = dividir; e->esc = esc;
    e->refPx = refPx; e->refPy = refPy;
    e->tick = ++assRigidTick;
    e->count = idx; e->bytes = bytes; e->imgs = imgs;
    assRigidBytes += bytes;
    assRigidMisses++;
  } else {
    for (i = 0; i < idx; i++) free(imgs[i].bitmap);
    free(imgs);
  }
  pthread_mutex_unlock(&assRigidTrava);
}

static void ass_plain_reflow_locked(long long t) {
  long long next = LLONG_MAX;
  uint64_t active = UINT64_C(1469598103934665603);
  if (!assTextoSimples || !assTrack || !plainAplValido) return;
  if (plainTimingValid && t >= plainLastTime && t < plainNextBoundary) {
    plainLastTime = t;
    return;
  }
  for (int i = 0; i < assTrack->n_events; i++) {
    ASS_Event *e = &assTrack->events[i];
    long long end = e->Start + e->Duration;
    if (e->Start > t && e->Start < next) next = e->Start;
    if (end > t && end < next) next = end;
    if (e->Start <= t && end > t)
      active = (active ^ (uint64_t)(i + 1)) * UINT64_C(1099511628211);
  }
  if (plainTimingValid && active != plainActiveSet) {
    /* libass keeps a collided cue's old vertical shift even after the
     * other cues end. Plain captions must reflow from the current bottom,
     * like the existing text overlay; authored ASS keeps its semantics. */
    ass_set_selective_style_override_enabled(assRenderer, 0);
    ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_MARGINS);
  }
  plainTimingValid = 1; plainLastTime = t;
  plainNextBoundary = next; plainActiveSet = active;
}

/* Styled ASS on the .tpk is drawn as the author made it (colours, border,
 * box, font of the Style sheet do NOT apply), and the size only scales the
 * dialogue (ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE, libass's and mpv's default).
 * Plain text follows the sheet as always. Under assTrava. */
static int ass_estilo_autor_locked(void) {
  return ASS_NOVO && !__atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
}

/* The .tpk's libass (tools/tpk/libass-nv): NEON engine, 3 blur threads and
 * the dialogue opacity. Under assTrava, on the worker, when the options
 * change (assperf_versao). Elsewhere nothing. */
static unsigned assApVersao = ~0u;
static int assApSimples = -1;
static void ass_aplicar_assperf_locked(void) {
  unsigned v = assperf_versao();
  int simples = __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
  if (!assRenderer || (v == assApVersao && simples == assApSimples)) return;
  assApVersao = v; assApSimples = simples;
  /* Dialogue size / opacity change what a cached layer looks like: new
   * cache key (22/11/2026). */
  { static int tam = -1, opac = -1;
    int t2 = assperf_get(AP_ASSTAM), o2 = assperf_get(AP_ASSOPAC);
    if (t2 != tam || o2 != opac) {
      if (tam != -1) assTrackStyleVer++;
      tam = t2; opac = o2;
    } }
  /* Hinting (22/11/2026): none for authored ASS, like libass's and mpv's
   * default. Light hinting sent every new glyph size through FreeType's
   * autohinter, and frame-by-frame \fs/\fscx animation makes new sizes every
   * frame. Plain text (SRT) keeps light. Only on a change: it clears caches. */
  { static int hint = -1;
    int h2 = simples ? ASS_HINTING_LIGHT : ASS_HINTING_NONE;
    if (h2 != hint) {
      if (hint != -1) assTrackStyleVer++;   /* cached rigid layers were drawn with the old hinting */
      hint = h2;
      ass_set_hinting(assRenderer, (ASS_Hinting)h2);
#if ASS_DIVIDIR
      if (assRendererB) ass_set_hinting(assRendererB, (ASS_Hinting)h2);
#endif
      printf("[ass] hinting %s (%s)\n", h2 == ASS_HINTING_NONE ? "none" : "light", simples ? "plain text" : "authored ASS");
    } }
#if defined(NV_TPK) && defined(ASS_NV_API)
  { static int feito;
    if (!feito) {
      /* The blur helper threads are born inside libass and inherit the
       * caller's name: "nv-assblur" tells them apart from the UI. */
      char nomeAnt[16];
      feito = 1;
      ass_nv_set_engine(assRenderer, 1);
      assfios_nome_trocar("nv-assblur", nomeAnt);
      int usable = ass_usable_cpus();
#ifdef NV_TPK40
      ass_nv_set_threads(usable <= 1 ? 1 : 2);
#else
      ass_nv_set_threads(usable <= 1 ? 1 : ASSPERF_FIOS);
#endif
      assfios_nome_voltar(nomeAnt);
      /* WHAT THIS TV IS, once per session (libass on the Tizen 4/5 .tpk, 09/11):
       * the engine's values are the S95C's on every .tpk, so a 2018-2020 TV's
       * "[ass] 10s" lines are read against its cores and memory. */
      printf("[ass] tv: cores=%ld online of %ld (usable=%d) | RAM=%ldMB total, %ldMB free | "
             "bitmap engine=%s, blur threads=%d | render ahead=%dms, lines dropped after=%lds, "
             "attached fonts up to=%luMB, relay park=%ldMB\n",
             sysconf(_SC_NPROCESSORS_ONLN), sysconf(_SC_NPROCESSORS_CONF), usable,
             memlog_total_mb(), memlog_livre_mb(),
             ass_nv_get_engine(assRenderer) ? "NEON" : "C", ass_nv_get_threads(),
             (int)ASSPERF_ADIANTE_MS, (long)(ASSPERF_PODA_MS / 1000L),
             (unsigned long)(ASSPERF_FONTES_TETO >> 20), (long)(ASSPERF_PARQUE >> 20));
      fflush(stdout);
    } }
  /* Dialogue opacity (Subtitles > Style): only lines without \pos, \move,
   * \clip, \org or a drawing; the signs keep the author's alpha. */
  ass_nv_set_dialogue_alpha(assRenderer, simples ? 255 : (assperf_get(AP_ASSOPAC) * 255 + 50) / 100);
#if ASS_DIVIDIR
  if (assRendererB) ass_nv_set_dialogue_alpha(assRendererB, simples ? 255 : (assperf_get(AP_ASSOPAC) * 255 + 50) / 100);
#endif
#endif
}

/* ===== EVENT TIME INDEX (22/11/2026) =========================================
 * Every subtitle frame used to walk the WHOLE event list several times (the
 * snapshot, the Signs & Songs marking, the busy-scene forecast), and strlen
 * every event starting in the next 2.2 s. The track keeps 2 min behind and up
 * to 3 min ahead: tens of thousands of frame-by-frame events in a busy ED. Now
 * the events are indexed by Start once per change of the track (a batch from
 * mkvass, a prune, a seek back), text lengths are kept with prefix sums, and a
 * frame touches only the active events plus the forecast window. Long events
 * (> ASS_IDX_CURTO ms: songs, whole-scene signs) sit in their own short list
 * so the backward reach for active events stays the longest SHORT event.
 * Worker thread, under assTrava. */
#define ASS_IDX_CURTO 2000LL
static int *idxOrd, idxN = -1, idxCap;       // event indices sorted by (Start, index)
static long long *idxIni, *idxPre;           // Start of idxOrd[i]; prefix sums of text length (N+1)
static int *idxLongo, idxLongoN, idxLongoCap;
static long long idxCurtoMax;
static int *idxLen, idxLenCap;               // text length per event index
static int *idxCand, idxCandCap;
static unsigned idxMut = ~0u;
static const ASS_Track *idxTrack;
static const ASS_Event *idxEvs;
static long idxReconstrucoes;
static int assIdxAtivosUlt;   // active events of the last snapshot (under assTrava)
static int assPlayResYVisto = 1080;   // worker only: the script's PlayResY seen last (clean blur grid)
typedef struct { const char *txt; long long ini; int ro, len; } AssLenEnt;
static AssLenEnt *idxMapa; static int idxMapaCap;   // previous lengths by (Text, Start, ReadOrder)
static const ASS_Event *idxCmpEv;
static int ass_idx_cmp(const void *a, const void *b) {
  int ia = *(const int *)a, ib = *(const int *)b;
  long long sa = idxCmpEv[ia].Start, sb = idxCmpEv[ib].Start;
  return sa < sb ? -1 : sa > sb ? 1 : ia - ib;
}
static unsigned ass_idx_h(const char *txt, long long ini, int ro) {
  uint64_t h = (uint64_t)(uintptr_t)txt * UINT64_C(0x9E3779B97F4A7C15) ^ (uint64_t)ini * UINT64_C(0xC2B2AE3D27D4EB4F) ^ (uint64_t)(unsigned)ro;
  return (unsigned)(h ^ (h >> 31));
}
static int ass_idx_cresce(int **p, int *cap, int n) {
  if (n <= *cap) return 1;
  { int nc = *cap ? *cap : 1024; int *nv;
    while (nc < n) nc *= 2;
    nv = (int *)realloc(*p, (size_t)nc * sizeof *nv);
    if (!nv) return 0;
    *p = nv; *cap = nc; }
  return 1;
}
/* idxOrd[0..n) = 0..n-1 sorted by (Start, index): natural merge sort. 0 = no
 * memory (the caller uses qsort). */
static int *idxTmp, idxTmpCap;
static int ass_idx_ordenar_runs(int n) {
  const ASS_Event *ev = idxCmpEv;
  int *a, *b, *t, larg, i;
  int *ini, nRuns = 0;
  if (n < 2) return 1;
  if (!ass_idx_cresce(&idxTmp, &idxTmpCap, 2 * n + 2)) return 0;
  /* Run starts in idxTmp[n+1..]: a new run wherever Start goes down. */
  ini = idxTmp + n + 1;
  ini[nRuns++] = 0;
  for (i = 1; i < n; i++) if (ev[i].Start < ev[i - 1].Start) ini[nRuns++] = i;
  if (nRuns == 1) return 1;                 // already in order (idxOrd is 0..n-1)
  if (nRuns > 64) return 0;                 // shuffled: qsort is as good
  ini[nRuns] = n;
  a = idxOrd; b = idxTmp;
  for (larg = 1; larg < nRuns; larg *= 2) {
    int r;
    for (r = 0; r < nRuns; r += 2 * larg) {
      int lo = ini[r], mid = ini[r + larg < nRuns ? r + larg : nRuns], hi = ini[r + 2 * larg < nRuns ? r + 2 * larg : nRuns];
      int x = lo, y = mid, k = lo;
      while (x < mid && y < hi) {
        const ASS_Event *ex = &ev[a[x]], *ey = &ev[a[y]];
        if (ex->Start < ey->Start || (ex->Start == ey->Start && a[x] < a[y])) b[k++] = a[x++];
        else b[k++] = a[y++];
      }
      while (x < mid) b[k++] = a[x++];
      while (y < hi) b[k++] = a[y++];
    }
    t = a; a = b; b = t;
  }
  if (a != idxOrd) memcpy(idxOrd, a, (size_t)n * sizeof *idxOrd);
  return 1;
}
/* 1 = the index matches assTrack. 0 = no memory: callers fall back to a walk. */
static int ass_idx_garantir(void) {
  int n, i, mc;
  AssLenEnt *novo;
  if (!assTrack) return 0;
  n = assTrack->n_events;
  if (idxTrack == assTrack && idxEvs == assTrack->events && idxN == n &&
      idxMut == __atomic_load_n(&assTrackMut, __ATOMIC_ACQUIRE)) return 1;
  idxN = -1;
  if (!ass_idx_cresce(&idxOrd, &idxCap, n + 1) || !ass_idx_cresce(&idxLen, &idxLenCap, n + 1) ||
      !ass_idx_cresce(&idxLongo, &idxLongoCap, n + 1)) return 0;
  { long long *a = (long long *)realloc(idxIni, (size_t)(n + 1) * sizeof *a);
    if (!a) return 0;
    idxIni = a;
    a = (long long *)realloc(idxPre, (size_t)(n + 2) * sizeof *a);
    if (!a) return 0;
    idxPre = a; }
  /* Lengths: reuse the previous rebuild's (same Text pointer, Start and
   * ReadOrder), strlen only what is new. */
  for (mc = 1024; mc < 2 * n + 2; mc *= 2) {}
  novo = (AssLenEnt *)calloc((size_t)mc, sizeof *novo);
  if (!novo) return 0;
  for (i = 0; i < n; i++) {
    const ASS_Event *e = &assTrack->events[i];
    int len = -1;
    unsigned h;
    if (!e->Text) { idxLen[i] = 0; continue; }
    if (idxMapa) {
      for (h = ass_idx_h(e->Text, e->Start, e->ReadOrder) & (unsigned)(idxMapaCap - 1); idxMapa[h].txt;
           h = (h + 1) & (unsigned)(idxMapaCap - 1))
        if (idxMapa[h].txt == e->Text && idxMapa[h].ini == e->Start && idxMapa[h].ro == e->ReadOrder) { len = idxMapa[h].len; break; }
    }
    if (len < 0) len = (int)strlen(e->Text);
    idxLen[i] = len;
    for (h = ass_idx_h(e->Text, e->Start, e->ReadOrder) & (unsigned)(mc - 1); novo[h].txt; h = (h + 1) & (unsigned)(mc - 1)) {}
    novo[h].txt = e->Text; novo[h].ini = e->Start; novo[h].ro = e->ReadOrder; novo[h].len = len;
  }
  free(idxMapa); idxMapa = novo; idxMapaCap = mc;
  idxLongoN = 0; idxCurtoMax = 0;
  for (i = 0; i < n; i++) {
    const ASS_Event *e = &assTrack->events[i];
    idxOrd[i] = i;
    if (e->Duration > ASS_IDX_CURTO) idxLongo[idxLongoN++] = i;
    else if (e->Duration > idxCurtoMax) idxCurtoMax = e->Duration;
  }
  idxCmpEv = assTrack->events;
  /* Owner 22/11/2026 (log 1): rebuilds after a seek cost 12-18 ms per frame.
   * The track is a few runs already in Start order (each mkvass batch is), so
   * a natural merge sort (runs merged pairwise) replaces qsort. */
  if (!ass_idx_ordenar_runs(n)) qsort(idxOrd, (size_t)n, sizeof *idxOrd, ass_idx_cmp);
  idxPre[0] = 0;
  for (i = 0; i < n; i++) {
    idxIni[i] = assTrack->events[idxOrd[i]].Start;
    idxPre[i + 1] = idxPre[i] + idxLen[idxOrd[i]];
  }
  idxTrack = assTrack; idxEvs = assTrack->events; idxN = n;
  idxMut = __atomic_load_n(&assTrackMut, __ATOMIC_ACQUIRE);
  idxReconstrucoes++;
  return 1;
}
static int ass_idx_lb(long long v) {   // first i with idxIni[i] >= v
  int lo = 0, hi = idxN;
  while (lo < hi) { int m = (lo + hi) / 2; if (idxIni[m] < v) lo = m + 1; else hi = m; }
  return lo;
}
/* Events starting in [de, ate): count and text length. */
static void ass_idx_janela(long long de, long long ate, int *n, long *chars) {
  int a = ass_idx_lb(de), b = ass_idx_lb(ate);
  if (b < a) b = a;
  *n = b - a; *chars = (long)(idxPre[b] - idxPre[a]);
}
/* First Start after t (LLONG_MAX: none). */
static long long ass_idx_prox(long long t) {
  int i = ass_idx_lb(t + 1);
  return i < idxN ? idxIni[i] : LLONG_MAX;
}
static int ass_int_cmp(const void *a, const void *b) { int x = *(const int *)a, y = *(const int *)b; return x - y; }
/* Events with Start <= ate and End > t, in track order (as the old walk). */
static int ass_idx_candidatos(long long t, long long ate, const int **out) {
  int i, n = 0, a, b;
  const ASS_Event *ev = assTrack->events;
  a = ass_idx_lb(t - idxCurtoMax);
  b = ass_idx_lb(ate + 1);
  if (!ass_idx_cresce(&idxCand, &idxCandCap, (b > a ? b - a : 0) + idxLongoN + 1)) { *out = NULL; return -1; }
  for (i = a; i < b; i++) {
    const ASS_Event *e = &ev[idxOrd[i]];
    if (e->Duration > ASS_IDX_CURTO) continue;   // in idxLongo
    if (e->Start + e->Duration > t) idxCand[n++] = idxOrd[i];
  }
  for (i = 0; i < idxLongoN; i++) {
    const ASS_Event *e = &ev[idxLongo[i]];
    if (e->Start <= ate && e->Start + e->Duration > t) idxCand[n++] = idxLongo[i];
  }
  qsort(idxCand, (size_t)n, sizeof *idxCand, ass_int_cmp);
  *out = idxCand;
  return n;
}

#if ASS_DIVIDIR
/* Split frames: the soft-pass renderer, set up like assRenderer (fonts,
 * shaper, hinting, engine, dialogue opacity); a smaller bitmap cache, it
 * holds only blurred parts at the blur resolution. Under assTrava. */
static void ass_b_criar_locked(void) {
  if (assRendererB || !assLib) return;
  assRendererB = ass_renderer_init(assLib);
  if (!assRendererB) { fprintf(stderr, "[ass] custom auto: second renderer failed, one pass\n"); return; }
  ass_set_shaper(assRendererB, ASS_SHAPING_COMPLEX);
  ass_set_fonts(assRendererB, assFallbackFont[0] ? assFallbackFont : NULL, "Arial",
                ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
  ass_set_cache_limits(assRendererB, 0, 64);   /* 22/11: was 24 */
  ass_set_hinting(assRendererB, ASS_HINTING_NONE);   /* authored ASS only (22/11) */
  ass_set_font_scale(assRendererB, 1.0);
#ifdef ASS_NV_API
  ass_nv_set_engine(assRendererB, 1);
  ass_nv_set_dialogue_alpha(assRendererB, __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE) ? 255 :
                            (assperf_get(AP_ASSOPAC) * 255 + 50) / 100);
#endif
  ass_nv_set_split(assRendererB, 2, 1);
  layBW = layBH = 0;
  printf("[ass] custom auto: second renderer (soft pass) created\n");
  fflush(stdout);
}

/* SIGNS & SONGS (asssinais.h; owner 19/11/2026: libass's rule first, until
 * we know, then the partner decides alone). The lines on screen at t:
 *   nv_sign = 1   a sign (Signs & Songs track, or the line is in the partner):
 *                 no dialogue size, opacity or skipping;
 *   nv_sign = -1  dialogue (the partner is known there and lacks it), even
 *                 with \pos or a drawing (libass-nv 0004);
 *   other (<= -2) not known yet: libass's own rule. Holds -(version + 2):
 *                 looked at again only after the partner grew. */

static void ass_marcar_sinais_locked(unsigned g, long long t) {
  int i, todos = asssinais_trilha_sinais(g), par;
  int marca;
  if (!assTrack) return;
  par = !todos && asssinais_ativo(g);
  if (!todos && !par) return;
  marca = -(int)(asssinais_versao() & 0x3FFFFFFFu) - 2;
  { const int *cand = NULL;
    int nc = ass_idx_garantir() ? ass_idx_candidatos(t, t + 1000, &cand) : -1, ci;
  for (ci = 0; ci < (nc >= 0 ? nc : assTrack->n_events); ci++) {
    ASS_Event *ev = &assTrack->events[nc >= 0 ? cand[ci] : ci];
    int r;
    if (ev->nv_sign > 0 || ev->nv_sign == -1 || ev->Start > t + 1000 || ev->Start + ev->Duration <= t) continue;
    if (todos) { ev->nv_sign = 1; continue; }
    if (ev->nv_sign == marca) continue;
    r = asssinais_decidir(g, ev->Start, asssinais_hash(ev->Text));
    ev->nv_sign = r > 0 ? 1 : r == 0 ? -1 : marca;
  } }
  (void)i;
}
#endif


static void ass_rigid_free_ctx_locked(void) {
  int i, n = assRigidCtx.snapTrack ? assRigidCtx.snapTrack->n_events : 0;
  /* libass keeps per-event collision state in render_priv and frees it in
   * ass_free_event. These copies never go through that, so release it here.
   * Only the slots in use: past n_events the array is uninitialised. */
  for (i = 0; i < n; i++) free(assRigidCtx.snapEvents[i].render_priv);
  free(assRigidCtx.snapEvents); assRigidCtx.snapEvents = NULL; assRigidCtx.snapCap = 0;
  free(assRigidCtx.rigidEvent.render_priv);
  assRigidCtx.rigidEvent.render_priv = NULL;
  if (assRigidCtx.snapTrack) {
    assRigidCtx.snapTrack->events = NULL; assRigidCtx.snapTrack->n_events = 0; assRigidCtx.snapTrack->max_events = 0;
    ass_free_track(assRigidCtx.snapTrack); assRigidCtx.snapTrack = NULL;
  }
  if (assRigidCtx.rigidTrack) {
    assRigidCtx.rigidTrack->events = NULL; assRigidCtx.rigidTrack->n_events = 0; assRigidCtx.rigidTrack->max_events = 0;
    ass_free_track(assRigidCtx.rigidTrack); assRigidCtx.rigidTrack = NULL;
  }
  assRigidCtx.snapGen = 0; assRigidCtx.snapStyleVer = 0;
  assRigidCtx.hitN = 0; assRigidCtx.missN = 0;
}

static int ass_track_copy_header_locked(ASS_Track *dst, const ASS_Track *src) {
  int i;
  while (dst->n_styles > 0) {
    dst->n_styles--;
    ass_free_style(dst, dst->n_styles);
  }
  dst->PlayResX = src->PlayResX;
  dst->PlayResY = src->PlayResY;
  dst->Timer = src->Timer;
  dst->WrapStyle = src->WrapStyle;
  dst->ScaledBorderAndShadow = src->ScaledBorderAndShadow;
  dst->Kerning = src->Kerning;
  dst->YCbCrMatrix = src->YCbCrMatrix;
  dst->default_style = src->default_style;
  dst->track_type = src->track_type;
  free(dst->Language);
  dst->Language = src->Language ? strdup(src->Language) : NULL;
  for (i = 0; i < src->n_styles; i++) {
    int sid = ass_alloc_style(dst);
    const ASS_Style *ss = &src->styles[i];
    ASS_Style *ds;
    if (sid < 0) return 0;
    ds = &dst->styles[sid];
    *ds = *ss;
    ds->Name = ss->Name ? strdup(ss->Name) : NULL;
    ds->FontName = ss->FontName ? strdup(ss->FontName) : NULL;
  }
  return 1;
}

static int ass_rigid_ensure_locked(AssRigidCtx *w) {
  if (!assLib) return 0;
  if (!w->snapTrack) {
    w->snapTrack = ass_new_track(assLib);
    if (!w->snapTrack) return 0;
    w->snapGen = 0;
  }
  if (!w->rigidTrack) {
    w->rigidTrack = ass_new_track(assLib);
    if (!w->rigidTrack) return 0;
  }
  return 1;
}

/* Evento que muda SOZINHO entre Start e End: \t, \move, \fad/\fade,
 * karaoke (\k, \kf, \ko, \K) ou Effect (Banner/Scroll). */
static int ass_evento_anima(const ASS_Event *e) {
  const char *p;
  if (e->Effect && (!strncasecmp(e->Effect, "Banner", 6) || !strncasecmp(e->Effect, "Scroll", 6)))
    return 1;
  if (!e->Text) return 0;
  for (p = strchr(e->Text, '\\'); p; p = strchr(p + 1, '\\'))
    if (!strncmp(p, "\\t(", 3) || !strncmp(p, "\\move", 5) || !strncmp(p, "\\fad", 4) || p[1] == 'k' || p[1] == 'K')
      return 1;
  return 0;
}

static uint64_t ass_fnv_bytes(uint64_t h, const char *p, size_t n) {
  size_t i;
  for (i = 0; i < n; i++) h = (h ^ (unsigned char)p[i]) * UINT64_C(1099511628211);
  return h;
}

static uint64_t ass_fnv_i64(uint64_t h, long long v) {
  uint64_t u = (uint64_t)v;
  int i;
  for (i = 0; i < 8; i++) { h = (h ^ (u & 0xFFu)) * UINT64_C(1099511628211); u >>= 8; }
  return h;
}

static int ass_evento_anima_nao_move(const ASS_Event *e) {
  const char *p;
  if (e->Effect && (!strncasecmp(e->Effect, "Banner", 6) || !strncasecmp(e->Effect, "Scroll", 6)))
    return 1;
  if (!e->Text) return 0;
  for (p = strchr(e->Text, '\\'); p; p = strchr(p + 1, '\\'))
    if (!strncmp(p, "\\t(", 3) || !strncmp(p, "\\fad", 4) || p[1] == 'k' || p[1] == 'K')
      return 1;
  return 0;
}

/* Detect rigid/static layers eligible for bitmap caching:
 * mode 0: short-duration (<=150ms) non-clipped rigid \pos(x,y) layer
 * mode 1: static positioned layer (>150ms, no animation, may be clipped)
 * mode 2: non-clipped rigid \move(x1,y1,x2,y2[,t1,t2]) without \org/\frx/\fry. */
/* Known dialogue (nv_sign -1, the Signs & Songs comparison): it gets the
 * dialogue size/opacity, so it never reuses a bitmap cached while it was drawn as a sign (22/11/2026). */
#ifdef ASS_NV_API
#define ASS_EV_DIALOGO_CONHECIDO(ev) ((ev)->nv_sign == -1)
#else
#define ASS_EV_DIALOGO_CONHECIDO(ev) 0
#endif
typedef struct { double x1, y1, x2, y2, t1, t2; } AssMove;
static int ass_rigid_parse_cru(const ASS_Event *e, int playResX, int playResY,
                               uint64_t *outSig, double *outPx, double *outPy, int *outMode, AssMove *mv) {
  const char *txt = e->Text, *p, *posEnd = NULL, *orgEnd = NULL, *moveEnd = NULL;
  double px = 0.0, py = 0.0, ox = 0.0, oy = 0.0;
  int hasPos = 0, hasOrg = 0, hasMove = 0, hasRot = 0, has3d = 0, hasClip = 0;
  uint64_t h = UINT64_C(1469598103934665603);
  if (!txt || !txt[0] || e->Duration <= 0 || playResX <= 0 || playResY <= 0) return 0;
  if (ass_evento_anima_nao_move(e)) return 0;

  h = ass_fnv_i64(h, e->Style);
  h = ass_fnv_i64(h, e->Layer);
  h = ass_fnv_i64(h, e->MarginL);
  h = ass_fnv_i64(h, e->MarginR);
  h = ass_fnv_i64(h, e->MarginV);
#ifdef ASS_NV_API
  /* Authored tracks only, so this is the .tpk's libass: a stock header (webOS,
   * the .wgt, the host tests) has no nv_sign. */
  h = ass_fnv_i64(h, e->nv_sign > 0 ? 1 : 0);
#endif

  if (e->Duration > 150 && !strstr(txt, "\\move(")) {
    if (!strstr(txt, "\\pos(")) return 0;
    h = ass_fnv_i64(h, 0x55);
    h = ass_fnv_bytes(h, txt, strlen(txt));
    *outSig = h ? h : 1;
    *outPx = 0.0; *outPy = 0.0;
    *outMode = 1;
    return 1;
  }

  for (p = strchr(txt, '\\'); p; p = strchr(p + 1, '\\')) {
    if (!strncmp(p, "\\clip", 5) || !strncmp(p, "\\iclip", 6)) { hasClip = 1; break; }
    if (!strncmp(p, "\\frx", 4) || !strncmp(p, "\\fry", 4)) has3d = 1;
    if (!strncmp(p, "\\fr", 3) || !strncmp(p, "\\fa", 3)) hasRot = 1;
    if (!hasPos && !strncmp(p, "\\pos(", 5)) {
      char *end1 = NULL, *end2 = NULL;
      px = strtod(p + 5, &end1);
      if (end1 && *end1 == ',') py = strtod(end1 + 1, &end2);
      if (end2 && *end2 == ')') { hasPos = 1; posEnd = end2 + 1; }
    } else if (!hasMove && !strncmp(p, "\\move(", 6)) {
      char *e1 = NULL, *e2 = NULL, *e3 = NULL, *e4 = NULL, *e5 = NULL, *e6 = NULL;
      double x1 = strtod(p + 6, &e1), y1 = 0, x2 = 0, y2 = 0;
      if (e1 && *e1 == ',') y1 = strtod(e1 + 1, &e2);
      if (e2 && *e2 == ',') x2 = strtod(e2 + 1, &e3);
      if (e3 && *e3 == ',') y2 = strtod(e3 + 1, &e4);
      if (e4 && (*e4 == ')' || *e4 == ',')) {
        double t1 = 0.0, t2 = (double)e->Duration;
        const char *endP = NULL;
        if (*e4 == ')') endP = e4;
        else {
          t1 = strtod(e4 + 1, &e5);
          if (e5 && *e5 == ',') t2 = strtod(e5 + 1, &e6);
          if (e6 && *e6 == ')') endP = e6;
        }
        if (endP) {
          /* The position along the move is worked out per frame by the
           * caller (the parse itself is cached, 22/11/2026). */
          mv->x1 = x1; mv->y1 = y1; mv->x2 = x2; mv->y2 = y2; mv->t1 = t1; mv->t2 = t2;
          hasMove = 1; moveEnd = endP + 1;
        }
      }
    } else if (!hasOrg && !strncmp(p, "\\org(", 5)) {
      char *end1 = NULL, *end2 = NULL;
      ox = strtod(p + 5, &end1);
      if (end1 && *end1 == ',') oy = strtod(end1 + 1, &end2);
      if (end2 && *end2 == ')') { hasOrg = 1; orgEnd = end2 + 1; }
    }
  }
  if (hasClip) return 0;
  if (hasMove) {
    if (hasPos || hasOrg || has3d) return 0;
    h = ass_fnv_i64(h, 0x77);
  } else {
    if (!hasPos || e->Duration > 150) return 0;
    if (hasRot && !hasOrg) { ox = px; oy = py; }
    if (hasOrg) {
      h = ass_fnv_i64(h, llround(ox - px));
      h = ass_fnv_i64(h, llround(oy - py));
    }
  }
  p = txt;
  while (*p) {
    if (hasPos && !strncmp(p, "\\pos(", 5) && posEnd) {
      h = ass_fnv_bytes(h, "\\pos()", 6);
      p = posEnd;
      continue;
    }
    if (hasMove && !strncmp(p, "\\move(", 6) && moveEnd) {
      h = ass_fnv_bytes(h, "\\move()", 7);
      p = moveEnd;
      continue;
    }
    if (hasOrg && !strncmp(p, "\\org(", 5) && orgEnd) {
      h = ass_fnv_bytes(h, "\\org()", 6);
      p = orgEnd;
      continue;
    }
    h = (h ^ (unsigned char)*p) * UINT64_C(1099511628211);
    p++;
  }
  *outSig = h ? h : 1;
  *outPx = px; *outPy = py;
  *outMode = hasMove ? 2 : 0;
  return 1;
}

/* The rigid signature, cached per event (22/11/2026): the raw parse hashes
 * the whole Text (vector drawings are long) and used to run for every active
 * event on every frame. Everything in it is fixed for an event except the
 * position along a \move, which is worked out here per frame. Key: the Text
 * pointer plus the fields the signature reads, so a reused pointer of another
 * event never matches. Worker, under assTrava. */
typedef struct {
  const char *txt; long long ini, dur; int ro, layer, style, ml, mr, mvv, sinal;
  int ok, mode; uint64_t sig; double px, py; AssMove mv;
} AssSigEnt;
#define ASS_SIG_CAP 16384
static AssSigEnt *assSig; static int assSigN;
static long assSigAcertos, assSigCalculos;
static int ass_rigid_parse(const ASS_Event *e, long long t, int playResX, int playResY,
                           uint64_t *outSig, double *outPx, double *outPy, int *outMode) {
  AssSigEnt *s;
  unsigned h;
  int sinal = 0;
#ifdef ASS_NV_API
  sinal = e->nv_sign > 0 ? 1 : 0;
#endif
  if (!e->Text) return 0;
  if (!assSig) { assSig = (AssSigEnt *)calloc(ASS_SIG_CAP, sizeof *assSig); assSigN = 0; }
  if (!assSig) return 0;   /* no memory: no rigid caching (rendered normally) */
  { uint64_t k = (uint64_t)(uintptr_t)e->Text * UINT64_C(0x9E3779B97F4A7C15) ^ (uint64_t)e->Start * UINT64_C(0xC2B2AE3D27D4EB4F) ^
                 (uint64_t)(unsigned)e->ReadOrder ^ ((uint64_t)(unsigned)e->Layer << 32) ^ (uint64_t)sinal << 63;
    h = (unsigned)(k ^ (k >> 29)) & (ASS_SIG_CAP - 1); }
  for (;; h = (h + 1) & (ASS_SIG_CAP - 1)) {
    s = &assSig[h];
    if (!s->txt) break;
    if (s->txt == e->Text && s->ini == e->Start && s->dur == e->Duration && s->ro == e->ReadOrder &&
        s->layer == e->Layer && s->style == e->Style && s->ml == e->MarginL && s->mr == e->MarginR &&
        s->mvv == e->MarginV && s->sinal == sinal) {
      assSigAcertos++;
      goto achou;
    }
  }
  /* Full: start over (the busy ED keeps a few thousand live events). */
  if (assSigN >= ASS_SIG_CAP * 3 / 4) {
    memset(assSig, 0, ASS_SIG_CAP * sizeof *assSig); assSigN = 0;
    { uint64_t k = (uint64_t)(uintptr_t)e->Text * UINT64_C(0x9E3779B97F4A7C15) ^ (uint64_t)e->Start * UINT64_C(0xC2B2AE3D27D4EB4F) ^
                   (uint64_t)(unsigned)e->ReadOrder ^ ((uint64_t)(unsigned)e->Layer << 32) ^ (uint64_t)sinal << 63;
      h = (unsigned)(k ^ (k >> 29)) & (ASS_SIG_CAP - 1); }
    s = &assSig[h];
  }
  memset(s, 0, sizeof *s);
  s->txt = e->Text; s->ini = e->Start; s->dur = e->Duration; s->ro = e->ReadOrder; s->layer = e->Layer;
  s->style = e->Style; s->ml = e->MarginL; s->mr = e->MarginR; s->mvv = e->MarginV; s->sinal = sinal;
  s->ok = ass_rigid_parse_cru(e, playResX, playResY, &s->sig, &s->px, &s->py, &s->mode, &s->mv);
  assSigN++; assSigCalculos++;
achou:
  if (!s->ok) return 0;
  *outSig = s->sig; *outMode = s->mode; *outPx = s->px; *outPy = s->py;
  if (s->mode != 2) return 1;
  { const AssMove *m = &s->mv;
    double rel = (double)(t - e->Start), px, py;
    if (rel <= m->t1 || m->t2 <= m->t1) { px = m->x1; py = m->y1; }
    else if (rel >= m->t2) { px = m->x2; py = m->y2; }
    else { double u = (rel - m->t1) / (m->t2 - m->t1); px = m->x1 + (m->x2 - m->x1) * u; py = m->y1 + (m->y2 - m->y1) * u; }
    *outPx = px; *outPy = py; }
  return 1;
}

static int ass_snap_build_locked(AssRigidCtx *w, unsigned generation, long long t,
                                 int lw, int lh, int lwB, int lhB, int vw, int vh,
                                 int dividir, double esc,
                                 long long *validoDe, long long *validoAte, long long *prox,
                                 int *nIni, long *chars, int *nIniC, long *charsC,
                                 long long *inicioMax) {
  int i, activeN = 0, anima = 0;
  *validoDe = t; *validoAte = LLONG_MAX; *prox = LLONG_MAX;
  *nIni = *nIniC = 0; *chars = *charsC = 0; *inicioMax = -1;
  w->hitN = 0; w->missN = 0;
  if (!assTrack || !w->snapTrack || !w->rigidTrack) return 0;
  if (w->snapGen != generation || w->snapStyleVer != assTrackStyleVer ||
      w->snapTrack->n_styles != assTrack->n_styles) {
    if (!ass_track_copy_header_locked(w->snapTrack, assTrack)) return 0;
    if (!ass_track_copy_header_locked(w->rigidTrack, assTrack)) return 0;
    w->snapGen = generation;
    w->snapStyleVer = assTrackStyleVer;
  }
  /* The time index (22/11/2026): only the active events and the forecast
   * window, not the whole track. Without memory for it: the old full walk. */
  const int *cand = NULL;
  int nCand = ass_idx_garantir() ? ass_idx_candidatos(t, t, &cand) : -1, ci;
  if (nCand >= 0) {
    *prox = ass_idx_prox(t);
    *validoAte = *prox;
    if (ASS_NOVO) {
      ass_idx_janela(t, t + 1000, nIni, chars);
      ass_idx_janela(t + 500, t + 2200, nIniC, charsC);
    }
  }
  for (ci = 0; ci < (nCand >= 0 ? nCand : assTrack->n_events); ci++) {
    i = nCand >= 0 ? cand[ci] : ci;
    const ASS_Event *ev = &assTrack->events[i];
    long long ini = ev->Start, fimE = ini + ev->Duration;
    if (nCand < 0) {
      if (ini > t && ini < *prox) *prox = ini;
      if (ASS_NOVO && ini >= t && ini < t + 2200) {
        long n = ev->Text ? (long)strlen(ev->Text) : 0;
        if (ini < t + 1000) { (*nIni)++; *chars += n; }
        if (ini >= t + 500) { (*nIniC)++; *charsC += n; }
      }
    }
    if (ini > t) {
      if (ini < *validoAte) *validoAte = ini;
    } else if (fimE > t) {
      uint64_t rSig = 0;
      int rMode = 0;
      double rPx = 0.0, rPy = 0.0;
      if (fimE < *validoAte) *validoAte = fimE;
      if (!anima && ass_evento_anima(ev)) anima = 1;
      if (ini > *inicioMax) *inicioMax = ini;
      if (ASS_NOVO && w->hitN < ASS_RIGID_FRAME_MAX && !ASS_EV_DIALOGO_CONHECIDO(ev) &&
          ass_rigid_parse(ev, t, assTrack->PlayResX, assTrack->PlayResY,
                          &rSig, &rPx, &rPy, &rMode)) {
        if (ass_rigid_has(rSig, generation, assTrackStyleVer, lw, lh, lwB, lhB, vw, vh, dividir, esc)) {
          AssRigidHit *hit = &w->hits[w->hitN++];
          hit->sig = rSig; hit->px = rPx; hit->py = rPy;
          hit->layer = ev->Layer; hit->readOrder = ev->ReadOrder; hit->mode = rMode;
          continue;
        }
        int inView = rMode != 2 || (rPx >= 80.0 && rPx <= (double)assTrack->PlayResX - 80.0 &&
                                    rPy >= 40.0 && rPy <= (double)assTrack->PlayResY - 40.0);
        if (inView && w->missN < ASS_RIGID_MISS_MAX && ass_rigid_should_cache(rSig, t)) {
          AssRigidMiss *ms = &w->misses[w->missN++];
          ms->ev = *ev; ms->ev.render_priv = NULL;
          ms->sig = rSig; ms->px = rPx; ms->py = rPy;
          ms->layer = ev->Layer; ms->readOrder = ev->ReadOrder; ms->mode = rMode;
          ms->t = t;
          continue;
        }
      }
      if (activeN >= w->snapCap) {
        int nc = w->snapCap ? w->snapCap * 2 : 256;
        ASS_Event *ne = (ASS_Event *)realloc(w->snapEvents, (size_t)nc * sizeof *ne);
        if (!ne) return 0;
        /* The new tail must start zeroed: the release path walks these. */
        memset(ne + w->snapCap, 0, (size_t)(nc - w->snapCap) * sizeof *ne);
        w->snapEvents = ne; w->snapCap = nc;
      }
      /* libass reuses render_priv only while render_id matches, and that id is
       * per renderer, so a slot reused for a different event would keep the
       * old event's collision box. Clear it as before, but release it first:
       * the copy is never handed back to libass for freeing, so the old one
       * would be unreachable. */
      free(w->snapEvents[activeN].render_priv);
      w->snapEvents[activeN] = *ev;
      w->snapEvents[activeN].render_priv = NULL;
      activeN++;
    }
  }
  if (anima) *validoAte = t;
  assIdxAtivosUlt = activeN + w->hitN + w->missN;
  w->snapTrack->events = w->snapEvents;
  w->snapTrack->n_events = activeN;
  w->snapTrack->max_events = w->snapCap;
  return 1;
}

static void ass_rigid_render_misses(AssRigidCtx *w, unsigned generation,
                                    int lw, int lh, int lwB, int lhB,
                                    int vw, int vh, int dividir, double esc, float escB) {
  int i;
  for (i = 0; i < w->missN; i++) {
    AssRigidMiss *m = &w->misses[i];
    if (!ass_rigid_has(m->sig, generation, w->snapStyleVer, lw, lh, lwB, lhB, vw, vh, dividir, esc)) {
      int chA = 0, chB = 0;
      ASS_Image *imA = NULL, *imB = NULL;
      w->rigidEvent = m->ev;
      /* A different event every miss, so nothing can be carried over: libass
       * will give this copy its own collision state, released at the end of
       * the loop below. */
      w->rigidEvent.render_priv = NULL;
      w->rigidTrack->events = &w->rigidEvent;
      w->rigidTrack->n_events = 1;
      w->rigidTrack->max_events = 1;
      imA = ass_render_frame(assRenderer, w->rigidTrack, m->t, &chA);
#if ASS_DIVIDIR
      if (dividir && assRendererB)
        imB = ass_render_frame(assRendererB, w->rigidTrack, m->t, &chB);
#else
      (void)chB;
#endif
      int touched = 0, spans = 0, mx = lw / 32, my = lh / 24;
      if (mx < 2) mx = 2;
      if (my < 2) my = 2;
      if (m->mode != 1 && lw > 4 && lh > 4) {
        const ASS_Image *p;
        for (p = imA; p; p = p->next) {
          if (p->w <= 0 || p->h <= 0) continue;
          if (p->dst_x <= mx || p->dst_x + p->w >= lw - mx ||
              p->dst_y <= my || p->dst_y + p->h >= lh - my)
            touched = 1;
          if (p->w >= lw - 4 || p->h >= lh - 4) spans = 1;
        }
        if (dividir && lwB > 4 && lhB > 4) {
          int mxB = lwB / 32, myB = lhB / 24;
          if (mxB < 2) mxB = 2;
          if (myB < 2) myB = 2;
          for (p = imB; p; p = p->next) {
            if (p->w <= 0 || p->h <= 0) continue;
            if (p->dst_x <= mxB || p->dst_x + p->w >= lwB - mxB ||
                p->dst_y <= myB || p->dst_y + p->h >= lhB - myB)
              touched = 1;
            if (p->w >= lwB - 4 || p->h >= lhB - 4) spans = 1;
          }
        }
      }
      if (m->mode != 1 && (touched || (!imA && !imB))) {
        if (spans) ass_rigid_ban(m->sig);
        else ass_rigid_defer(m->sig, m->t + 160);
        if (w->snapTrack->n_events >= w->snapCap) {
          int nc = w->snapCap ? w->snapCap * 2 : 256;
          ASS_Event *ne = (ASS_Event *)realloc(w->snapEvents, (size_t)nc * sizeof *ne);
          if (ne) { memset(ne + w->snapCap, 0, (size_t)(nc - w->snapCap) * sizeof *ne);
                    w->snapEvents = ne; w->snapCap = nc; w->snapTrack->events = ne; w->snapTrack->max_events = nc; }
        }
        if (w->snapTrack->n_events < w->snapCap) {
          free(w->snapEvents[w->snapTrack->n_events].render_priv);
          w->snapEvents[w->snapTrack->n_events] = m->ev;
          w->snapEvents[w->snapTrack->n_events++].render_priv = NULL;
        }
        continue;
      }
      ass_rigid_store(m->sig, generation, w->snapStyleVer, lw, lh, lwB, lhB, vw, vh,
                      w->rigidTrack->PlayResX, w->rigidTrack->PlayResY,
                      dividir, esc, m->px, m->py, imA, imB, escB);
    }
    if (w->hitN < ASS_RIGID_FRAME_MAX) {
      AssRigidHit *hit = &w->hits[w->hitN++];
      hit->sig = m->sig; hit->px = m->px; hit->py = m->py;
      hit->layer = m->layer; hit->readOrder = m->readOrder; hit->mode = m->mode;
    }
  }
  w->rigidTrack->events = NULL;
  w->rigidTrack->n_events = 0;
  w->rigidTrack->max_events = 0;
  free(w->rigidEvent.render_priv);
  w->rigidEvent.render_priv = NULL;
}

typedef struct {
  unsigned long long stackKey;
  int seq;
  int fromRigid;
  const ASS_Image *im;
  float esc;
  const AssRigidImg *rim;
  double dx, dy;
} AssMergeItem;

static unsigned long long ass_event_stack_key(int layer, int readOrder) {
  long long l = (long long)layer + 32768;
  l = l < 0 ? 0 : l > 0xFFFF ? 0xFFFF : l;
  return ((unsigned long long)l << 48) | ((unsigned long long)((unsigned)readOrder ^ 0x80000000u) << 16);
}

static int ass_merge_cmp(const void *pa, const void *pb) {
  const AssMergeItem *a = (const AssMergeItem *)pa;
  const AssMergeItem *b = (const AssMergeItem *)pb;
  if (a->stackKey != b->stackKey) return a->stackKey < b->stackKey ? -1 : 1;
  return a->seq < b->seq ? -1 : (a->seq > b->seq ? 1 : 0);
}

static int ass_frame_merge_rigid(AssRigidCtx *w, unsigned generation,
                                 int lw, int lh, int lwB, int lhB,
                                 int vw, int vh, int dividir, double esc,
                                 ASS_Image *headA, ASS_Image *headB,
                                 float escB, AssCpuFrame *dst, int *outTotalImgs) {
  int count = 0, idx = 0, i, j, seq = 0;
  size_t totalBytes = 0;
  const ASS_Image *cur;
  AssMergeItem *items = NULL;
  if (w->hitN <= 0) {
    for (cur = headA; cur; cur = cur->next) count++;
    for (cur = headB; cur; cur = cur->next) count++;
    *outTotalImgs = count;
    return dividir ? ass_frame_copiar2(headA, headB, escB, dst, ASS_NOVO)
                   : ass_frame_copiar(headA, dst, ASS_NOVO);
  }
  ass_frame_liberar(dst);
  for (cur = headA; cur; cur = cur->next) if (cur->w > 0 && cur->h > 0 && cur->bitmap) count++;
  for (cur = headB; cur; cur = cur->next) if (cur->w > 0 && cur->h > 0 && cur->bitmap) count++;
  pthread_mutex_lock(&assRigidTrava);
  for (i = 0; i < w->hitN; i++) {
    const AssRigidEntry *e = ass_rigid_lookup_locked(w->hits[i].sig, generation, w->snapStyleVer,
                                                     lw, lh, lwB, lhB, vw, vh, dividir, esc);
    if (e) count += e->count;
  }
  *outTotalImgs = count;
  if (count <= 0) { pthread_mutex_unlock(&assRigidTrava); return 1; }
  items = (AssMergeItem *)malloc((size_t)count * sizeof *items);
  if (!items) { pthread_mutex_unlock(&assRigidTrava); return 0; }
  for (i = 0; i < 2; i++) {
    float passEsc = i == 0 ? 0.0f : escB;
    for (cur = i == 0 ? headA : headB; cur; cur = cur->next) {
      if (cur->w <= 0 || cur->h <= 0 || !cur->bitmap) continue;
#if ASS_DIVIDIR
      items[idx].stackKey = ass_nv_image_key(cur);
#else
      items[idx].stackKey = 0ULL;
#endif
      items[idx].seq = seq++;
      items[idx].fromRigid = 0; items[idx].im = cur; items[idx].esc = passEsc;
      items[idx].rim = NULL; items[idx].dx = 0.0; items[idx].dy = 0.0;
      idx++;
    }
  }
  for (i = 0; i < w->hitN; i++) {
    unsigned long long evKey = ass_event_stack_key(w->hits[i].layer, w->hits[i].readOrder);
    const AssRigidEntry *e = ass_rigid_lookup_locked(w->hits[i].sig, generation, w->snapStyleVer,
                                                     lw, lh, lwB, lhB, vw, vh, dividir, esc);
    if (!e) continue;
    double sxA = e->playResX > 0 ? (double)lw / (double)e->playResX : 1.0;
    double syA = e->playResY > 0 ? (double)lh / (double)e->playResY : 1.0;
    double sxB = (dividir && lwB > 0 && e->playResX > 0) ? (double)lwB / (double)e->playResX : sxA;
    double syB = (dividir && lhB > 0 && e->playResY > 0) ? (double)lhB / (double)e->playResY : syA;
    for (j = 0; j < e->count; j++) {
      double sx = e->imgs[j].isB ? sxB : sxA;
      double sy = e->imgs[j].isB ? syB : syA;
      items[idx].stackKey = evKey | e->imgs[j].lowKey;
      items[idx].seq = seq++;
      items[idx].fromRigid = 1;
      items[idx].im = NULL; items[idx].esc = e->imgs[j].esc;
      items[idx].rim = &e->imgs[j];
      items[idx].dx = w->hits[i].mode == 1 ? 0.0 : (w->hits[i].px - e->refPx) * sx;
      items[idx].dy = w->hits[i].mode == 1 ? 0.0 : (w->hits[i].py - e->refPy) * sy;
      idx++;
    }
  }
  qsort(items, (size_t)idx, sizeof *items, ass_merge_cmp);
  dst->images = (AssCpuImage *)calloc((size_t)idx, sizeof *dst->images);
  if (!dst->images) { free(items); pthread_mutex_unlock(&assRigidTrava); return 0; }
  dst->count = idx;
  for (i = 0; i < idx; i++) {
    AssCpuImage *out = &dst->images[i];
    if (!items[i].fromRigid) {
      const ASS_Image *im = items[i].im;
      size_t sz = (size_t)im->w * (size_t)im->h;
      if (totalBytes + sz > 32u * 1024u * 1024u) {
        free(items); pthread_mutex_unlock(&assRigidTrava); ass_frame_liberar(dst); return 0;
      }
      out->bitmap = (unsigned char *)malloc(sz);
      if (!out->bitmap) {
        free(items); pthread_mutex_unlock(&assRigidTrava); ass_frame_liberar(dst); return 0;
      }
      { int y;
        for (y = 0; y < im->h; y++)
          memcpy(out->bitmap + (size_t)y * (size_t)im->w,
                 im->bitmap + (size_t)y * (size_t)im->stride, (size_t)im->w); }
      out->w = im->w; out->h = im->h; out->stride = im->w;
      out->x = im->dst_x; out->y = im->dst_y;
      out->subX = 0.0f; out->subY = 0.0f;
      out->color = im->color; out->type = im->type; out->esc = items[i].esc;
      out->chave = 0;   /* after the flatten decision: ass_frame_chaves (22/11/2026) */
      totalBytes += sz;
    } else {
      const AssRigidImg *rim = items[i].rim;
      size_t sz = (size_t)rim->w * (size_t)rim->h;
      if (totalBytes + sz > 32u * 1024u * 1024u) {
        free(items); pthread_mutex_unlock(&assRigidTrava); ass_frame_liberar(dst); return 0;
      }
      out->bitmap = (unsigned char *)malloc(sz);
      if (!out->bitmap) {
        free(items); pthread_mutex_unlock(&assRigidTrava); ass_frame_liberar(dst); return 0;
      }
      memcpy(out->bitmap, rim->bitmap, sz);
      out->w = rim->w; out->h = rim->h; out->stride = rim->w;
      { int ix = (int)llround(items[i].dx), iy = (int)llround(items[i].dy);
        out->x = rim->x + ix;
        out->y = rim->y + iy;
        out->subX = (float)(items[i].dx - (double)ix);
        out->subY = (float)(items[i].dy - (double)iy); }
      out->color = rim->color; out->type = rim->type; out->esc = rim->esc;
      out->chave = rim->chave;
      totalBytes += sz;
    }
  }
  dst->bytes = totalBytes;
  free(items);
  pthread_mutex_unlock(&assRigidTrava);
  return 1;
}

static double ass_ms_entre(const struct timespec *a, const struct timespec *b) {
  return (double)(b->tv_sec - a->tv_sec) * 1000.0 + (double)(b->tv_nsec - a->tv_nsec) / 1e6;
}

static void ass_pod_devolver_assTrava(void);
static double ass_fps(void);
static long long ass_grade_seguinte(long long t);
static int ass_so_o_mais_novo(void);
static long long ass_grade_desde(long long ms);
#if defined(NV_TPK) && defined(__linux__)
// glibc declares this only under _GNU_SOURCE, and the define at the top of
// this file comes too late in the .tpk build: its -include src/tpk.h has
// already pulled in the system headers. Without a declaration the call was
// an implicit one (a warning on the SDK's compiler, an error on gcc 14).
extern int pthread_getattr_np(pthread_t fio, pthread_attr_t *attr);
#endif
#if ASS_NOVO
static void ass_cor_final(const AssCpuImage *im, int corAtiva, int cr, int cg, int cb,
                          double gama, double contraste, float *r, float *g, float *b, float *op);

/* Flatten high-overdraw or high-quad-count frames into one straight-RGBA image
 * on the worker so the GL thread draws 1 quad instead of 17-60+ quads.
 * `corAtiva` is the viewer's dialogue colour override: when it is on, this
 * hands the frame back untouched, because that colour is applied per image at
 * draw time and a flattened frame could not follow it changing.
 *
 * Split frames (a soft pass at its own scale, im->esc) are never flattened:
 * they go per image through the texture cache (owner 23/11/2026, LOG 3: the
 * per-pass layers cost more than they saved). Rigid-cache images with a
 * sub-pixel offset (subX/subY) are resampled bilinearly (ratio 1), so a
 * flattened frame keeps their exact position like the per-image path's
 * GL_LINEAR quads (owner 23/11/2026: Exact, not whole pixel). Returns 1 when
 * the frame was flattened. */
static long assFlatQuadros, assFlatFeitos;   // [ass] cost (under assFilaTrava when read)
static int ass_flat_reamostra(const AssCpuImage *im, float escF, float *r, float *ox, float *oy) {
  float e = im->esc > 0.01f ? im->esc : escF;
  *r = escF / e;
  *ox = ((float)im->x + im->subX) * *r;
  *oy = ((float)im->y + im->subY) * *r;
  return fabsf(*r - 1.0f) >= 0.001f || fabsf(im->subX) >= 0.001f || fabsf(im->subY) >= 0.001f;
}
static inline void ass_flat_px(unsigned char *dst, unsigned lr, unsigned lg, unsigned lb, unsigned sa) {
  unsigned inv = 255u - sa;
  unsigned r = lr * sa + (unsigned)dst[0] * inv + 128u;
  unsigned g = lg * sa + (unsigned)dst[1] * inv + 128u;
  unsigned b = lb * sa + (unsigned)dst[2] * inv + 128u;
  unsigned a = (unsigned)dst[3] * inv + 128u;
  dst[0] = (unsigned char)((r + (r >> 8)) >> 8);
  dst[1] = (unsigned char)((g + (g >> 8)) >> 8);
  dst[2] = (unsigned char)((b + (b >> 8)) >> 8);
  dst[3] = (unsigned char)(sa + ((a + (a >> 8)) >> 8));
}
/* Composites the images idx[0..n-1] of `f` (in that order) into one
 * straight-RGBA canvas on the pixel grid of scale `escG` (gW x gH). Images of
 * another scale, or with a sub-pixel offset, are resampled bilinearly (like
 * the GL_LINEAR quad). Returns the canvas (NULL = nothing drawn / no memory)
 * and its box on that grid. */
static unsigned char *ass_flat_compor(const AssCpuFrame *f, const int *idx, int n, float escG, int gW, int gH,
                                      double gama, double contraste, int *oX, int *oY, int *oW, int *oH) {
  static uint32_t invAlpha[256];
  static int invInit;
  int k, minX = gW, minY = gH, maxX = 0, maxY = 0, outW, outH;
  size_t outBytes;
  unsigned char *rgba;
  for (k = 0; k < n; k++) {
    const AssCpuImage *im = &f->images[idx[k]];
    int x0, y0, x1, y1;
    float r, fx, fy;
    if (ass_flat_reamostra(im, escG, &r, &fx, &fy)) {
      x0 = (int)floorf(fx) - 1; y0 = (int)floorf(fy) - 1;
      x1 = (int)ceilf(fx + (float)im->w * r) + 1; y1 = (int)ceilf(fy + (float)im->h * r) + 1;
    } else { x0 = im->x; y0 = im->y; x1 = im->x + im->w; y1 = im->y + im->h; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > gW) x1 = gW;
    if (y1 > gH) y1 = gH;
    if (x0 < x1 && y0 < y1) {
      if (x0 < minX) minX = x0;
      if (y0 < minY) minY = y0;
      if (x1 > maxX) maxX = x1;
      if (y1 > maxY) maxY = y1;
    }
  }
  outW = maxX - minX; outH = maxY - minY;
  if (outW <= 0 || outH <= 0) return NULL;
  outBytes = (size_t)outW * (size_t)outH * 4u;
  if (outBytes > 3840u * 2160u * 4u) return NULL;   /* 4K UI (k = 2): the text pass can be 3840x2160 */
  if (!invInit) {
    for (int a = 1; a < 255; a++)
      invAlpha[a] = (255u * 65536u + (unsigned)a / 2u) / (unsigned)a;
    invInit = 1;
  }
  rgba = (unsigned char *)calloc(outBytes, 1);
  if (!rgba) return NULL;
  for (k = 0; k < n; k++) {
    const AssCpuImage *im = &f->images[idx[k]];
    float rf, gf, bf, opf, r, fx, fy;
    unsigned lr, lg, lb, lop;
    int y;
    ass_cor_final(im, 0, 0, 0, 0, gama, contraste, &rf, &gf, &bf, &opf);
    lr = (unsigned)lroundf(rf * 255.0f);
    lg = (unsigned)lroundf(gf * 255.0f);
    lb = (unsigned)lroundf(bf * 255.0f);
    lop = (unsigned)lroundf(opf * 255.0f);
    if (lop == 0) continue;
    if (ass_flat_reamostra(im, escG, &r, &fx, &fy)) {
      /* Bilinear, like the GL_LINEAR quad: destination pixel centre (u+0.5)
       * maps to source (u+0.5-fx)/r - 0.5. Outside the bitmap = 0. */
      int x0 = (int)floorf(fx) - 1, y0 = (int)floorf(fy) - 1;
      int x1 = (int)ceilf(fx + (float)im->w * r) + 1, y1 = (int)ceilf(fy + (float)im->h * r) + 1;
      int w, u;
      int *cx; unsigned short *cw;
      if (x0 < minX) x0 = minX;
      if (y0 < minY) y0 = minY;
      if (x1 > maxX) x1 = maxX;
      if (y1 > maxY) y1 = maxY;
      if (x0 >= x1 || y0 >= y1) continue;
      w = x1 - x0;
      cx = (int *)malloc((size_t)w * sizeof *cx);
      cw = (unsigned short *)malloc((size_t)w * sizeof *cw);
      if (!cx || !cw) { free(cx); free(cw); continue; }
      for (u = 0; u < w; u++) {
        float sx = ((float)(x0 + u) + 0.5f - fx) / r - 0.5f;
        int ix = (int)floorf(sx);
        cx[u] = ix; cw[u] = (unsigned short)lroundf((sx - (float)ix) * 256.0f);
      }
      for (y = y0; y < y1; y++) {
        float sy = ((float)y + 0.5f - fy) / r - 0.5f;
        int iy = (int)floorf(sy);
        unsigned wy = (unsigned)lroundf((sy - (float)iy) * 256.0f);
        const unsigned char *l0 = iy >= 0 && iy < im->h ? im->bitmap + (size_t)iy * (size_t)im->stride : NULL;
        const unsigned char *l1 = iy + 1 >= 0 && iy + 1 < im->h ? im->bitmap + (size_t)(iy + 1) * (size_t)im->stride : NULL;
        unsigned char *dst = rgba + ((size_t)(y - minY) * (size_t)outW + (size_t)(x0 - minX)) * 4u;
        if (!l0 && !l1) continue;
        for (u = 0; u < w; u++, dst += 4) {
          int ix = cx[u];
          unsigned wx = cw[u], p00 = 0, p01 = 0, p10 = 0, p11 = 0, v, sa;
          if (ix + 1 < 0 || ix >= im->w) continue;
          if (ix >= 0) { if (l0) p00 = l0[ix]; if (l1) p10 = l1[ix]; }
          if (ix + 1 < im->w) { if (l0) p01 = l0[ix + 1]; if (l1) p11 = l1[ix + 1]; }
          v = ((p00 * (256u - wx) + p01 * wx) * (256u - wy) + (p10 * (256u - wx) + p11 * wx) * wy + 32768u) >> 16;
          if (v == 0u) continue;
          if (lop == 255u) sa = v;
          else { unsigned st = v * lop + 128u; sa = (st + (st >> 8)) >> 8; }
          if (sa == 0u) continue;
          if (sa == 255u) { dst[0] = (unsigned char)lr; dst[1] = (unsigned char)lg; dst[2] = (unsigned char)lb; dst[3] = 255u; }
          else ass_flat_px(dst, lr, lg, lb, sa);
        }
      }
      free(cx); free(cw);
      continue;
    }
    { int x0 = im->x < minX ? minX : im->x;
      int y0 = im->y < minY ? minY : im->y;
      int x1 = im->x + im->w > maxX ? maxX : im->x + im->w;
      int y1 = im->y + im->h > maxY ? maxY : im->y + im->h;
      if (x0 >= x1 || y0 >= y1) continue;
      for (y = y0; y < y1; y++) {
        const unsigned char *src = im->bitmap + (size_t)(y - im->y) * (size_t)im->stride + (size_t)(x0 - im->x);
        unsigned char *dst = rgba + ((size_t)(y - minY) * (size_t)outW + (size_t)(x0 - minX)) * 4u;
        int w = x1 - x0, x;
        for (x = 0; x < w; x++, dst += 4) {
          unsigned sv = src[x], sa;
          if (sv == 0u) continue;
          if (lop == 255u) sa = sv;
          else { unsigned st = sv * lop + 128u; sa = (st + (st >> 8)) >> 8; }
          if (sa == 0u) continue;
          if (sa == 255u) { dst[0] = (unsigned char)lr; dst[1] = (unsigned char)lg; dst[2] = (unsigned char)lb; dst[3] = 255u; }
          else ass_flat_px(dst, lr, lg, lb, sa);
        }
      } }
  }
  /* Unpremultiply for straight-RGBA GFX_TEXTO blending via reciprocal table. */
  { size_t p, nPx = (size_t)outW * (size_t)outH;
    unsigned char *px = rgba;
    for (p = 0; p < nPx; p++, px += 4) {
      unsigned a = px[3];
      if (a > 0u && a < 255u) {
        uint32_t inv = invAlpha[a];
        unsigned r = ((uint32_t)px[0] * inv + 32768u) >> 16;
        unsigned g = ((uint32_t)px[1] * inv + 32768u) >> 16;
        unsigned b = ((uint32_t)px[2] * inv + 32768u) >> 16;
        px[0] = (unsigned char)(r > 255u ? 255u : r);
        px[1] = (unsigned char)(g > 255u ? 255u : g);
        px[2] = (unsigned char)(b > 255u ? 255u : b);
      }
    } }
  *oX = minX; *oY = minY; *oW = outW; *oH = outH;
  return rgba;
}

/* Overload (owner 23/11/2026, log 12, Fate/strange Fake Kaleido-subs): ~600
 * tiny images per frame filled the 2048-texture table and froze the GL thread
 * 300-450 ms per frame uploading them. A frame with more images than this is
 * flattened even when it is split: the soft pass is resampled into the text
 * pass's grid (ass_flat_compor already does that, exact positions). Normal
 * busy scenes (Kaguya ED: 12-57 images) never reach it, so their per-image
 * path is unchanged. */
static long assSobreQuadros, assSobreImgsSoma, assSobreImgsMax;   // [ass] cost (under assFilaTrava when read)
static long assTcFora;   // images left out: every texture slot was drawn in the last frames (GL thread, __atomic)
static int ass_frame_flatten(AssCpuFrame *f, int frameW, int frameH, int corAtiva) {
  int i, minX, minY, maxX, maxY, outW, outH, sobrecarga;
  size_t sumArea = 0, outBytes;
  double gama, contraste;
  float escF;
  AssCpuImage plano;
  if (!f || f->count <= 16 || !f->images || frameW <= 0 || frameH <= 0) return 0;
  sobrecarga = f->count > ASS_FLAT_SOBRECARGA;
  escF = f->esc > 0.01f ? f->esc : 1.0f;
  minX = frameW; minY = frameH; maxX = 0; maxY = 0;
  for (i = 0; i < f->count; i++) {
    const AssCpuImage *im = &f->images[i];
    int x0, y0, x1, y1;
    float r, fx, fy;
    if (im->rgba || !im->bitmap || im->w <= 0 || im->h <= 0) return 0;
    if (corAtiva && im->type == IMAGE_TYPE_CHARACTER) return 0;
    /* Split frames: per image, never flattened, unless overloaded. */
    if (!sobrecarga && im->esc > 0.01f && fabsf(im->esc - escF) >= 0.01f) return 0;
    if (ass_flat_reamostra(im, escF, &r, &fx, &fy)) {
      x0 = (int)floorf(fx) - 1; y0 = (int)floorf(fy) - 1;
      x1 = (int)ceilf(fx + (float)im->w * r) + 1; y1 = (int)ceilf(fy + (float)im->h * r) + 1;
      sumArea += (size_t)((float)im->w * r * (float)im->h * r);
    } else {
      x0 = im->x; y0 = im->y; x1 = im->x + im->w; y1 = im->y + im->h;
      sumArea += (size_t)im->w * (size_t)im->h;
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > frameW) x1 = frameW;
    if (y1 > frameH) y1 = frameH;
    if (x0 < x1 && y0 < y1) {
      if (x0 < minX) minX = x0;
      if (y0 < minY) minY = y0;
      if (x1 > maxX) maxX = x1;
      if (y1 > maxY) maxY = y1;
    }
  }
  outW = maxX - minX; outH = maxY - minY;
  if (outW <= 0 || outH <= 0) return 0;
  outBytes = (size_t)outW * (size_t)outH * 4u;
  if (outBytes > 3840u * 2160u * 4u) return 0;
  int highOverdraw = f->count > 24 && sumArea > (size_t)frameW * (size_t)frameH;
  int smallBatch = f->count > 16 && outBytes <= 512u * 1024u;
  if (!highOverdraw && !smallBatch && !sobrecarga) return 0;
  gama = assperf_gama(); contraste = assperf_contraste();
  memset(&plano, 0, sizeof plano);
  { int *idx = (int *)malloc((size_t)f->count * sizeof *idx);
    if (!idx) return 0;
    for (i = 0; i < f->count; i++) idx[i] = i;
    plano.bitmap = ass_flat_compor(f, idx, f->count, escF, frameW, frameH, gama, contraste,
                                   &plano.x, &plano.y, &plano.w, &plano.h);
    free(idx);
    if (!plano.bitmap) return 0;
    plano.stride = plano.w * 4; plano.rgba = 1; }
  for (i = 0; i < f->count; i++) free(f->images[i].bitmap);
  f->images[0] = plano; f->count = 1; f->bytes = (size_t)plano.w * (size_t)plano.h * 4u;
  { AssCpuImage *novo = (AssCpuImage *)realloc(f->images, sizeof *novo);
    if (novo) f->images = novo; }
  return sobrecarga ? 2 : 1;
}

/* Texture keys only for frames that stay per image (22/11/2026): a flattened
 * frame never uses them, and hashing every bitmap byte of a busy frame on the
 * 32-bit ARM worker was pure waste. */
static void ass_frame_chaves(AssCpuFrame *f) {
  int i;
  for (i = 0; i < f->count; i++) {
    AssCpuImage *im = &f->images[i];
    if (!im->chave && !im->rgba && im->bitmap && im->w > 0 && im->h > 0)
      im->chave = ass_chave_bitmap(im->bitmap, im->w, im->h, 0u);
  }
}
#endif

/* [ass] cost (22/11/2026): what each stage of a subtitle frame costs the
 * worker, so the next log shows where the time goes (scan of the event list,
 * libass pass A / B, rigid misses, copy/merge, flatten, texture keys). Under
 * assFilaTrava. */
enum { CU_SCAN, CU_A, CU_B, CU_MISS, CU_COPIA, CU_FLAT, CU_CHAVE, CU_N };
static const char *const cuNome[CU_N] = { "scan", "libassA", "libassB", "misses", "copy", "flat", "keys" };
static double cuSoma[CU_N], cuMax[CU_N];
static long cuQuadros, cuEventos, cuAtivos;
static double ass_ms_desde(const struct timespec *t0) {
  struct timespec t1; clock_gettime(CLOCK_MONOTONIC, &t1);
  return ass_ms_entre(t0, &t1);
}


/* [ass] strips (22/11/2026, the owner's gradient phone screen): horizontal
 * lines on a gradient built from thin stacked strips, depending on the
 * resolution. Busy frames, at most once per 10 s: per pass the thin images
 * (h <= 3 px, w >= 64 px), the pass scale, the grid (frame px at 100 %, k,
 * PlayResY, screen px per script px), and up to 3 active events whose rect
 * \clip or \p drawing is <= 4 script px tall. Worker, under assTrava. */
static double ass_tira_altura(const char *txt, int *clipRet, int *clipVet, int *desenho, double *blur) {
  const char *p;
  double alt = -1.0;
  *clipRet = *clipVet = *desenho = 0; *blur = 0.0;
  if (!txt) return -1.0;
  for (p = strchr(txt, '\\'); p; p = strchr(p + 1, '\\')) {
    if (!strncmp(p, "\\clip(", 6) || !strncmp(p, "\\iclip(", 7)) {
      double a, b, c, d;
      const char *q = strchr(p, '(') + 1;
      if (sscanf(q, "%lf,%lf,%lf,%lf", &a, &b, &c, &d) == 4) { *clipRet = 1; alt = fabs(d - b); }
      else *clipVet = 1;
    } else if (!strncmp(p, "\\blur", 5)) *blur = atof(p + 5);
    else if (!strncmp(p, "\\be", 3) && *blur == 0.0) *blur = atof(p + 3);
    else if (p[1] == 'p' && p[2] >= '1' && p[2] <= '9') *desenho = p[2] - '0';
  }
  if (*desenho && alt < 0) {   /* the drawing's y range (text after the last override block) */
    const char *d = strrchr(txt, '}');
    double ymin = 1e18, ymax = -1e18, v;
    int i = 0, n;
    d = d ? d + 1 : txt;
    while (*d) {
      if ((*d >= '0' && *d <= '9') || *d == '-' || *d == '.') {
        if (sscanf(d, "%lf%n", &v, &n) == 1) {
          if (i % 2 == 1) { if (v < ymin) ymin = v; if (v > ymax) ymax = v; }
          i++; d += n; continue;
        }
      }
      d++;
    }
    if (ymax >= ymin) alt = (ymax - ymin) / (double)(1 << (*desenho - 1));
  }
  return alt;
}
static void ass_diag_tiras(const ASS_Image *a, const ASS_Image *b, long long t, double resol, double resolB,
                           int altPx, int k) {
  static double ult;
  struct timespec ts; double agora;
  int nA = 0, nB = 0, pry, i, mostrados = 0;
  const ASS_Image *im;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  agora = (double)ts.tv_sec + ts.tv_nsec / 1e9;
  if (ult > 0 && agora - ult < 10.0) return;
  for (im = a; im; im = im->next) if (im->h <= 3 && im->w >= 64) nA++;
  for (im = b; im; im = im->next) if (im->h <= 3 && im->w >= 64) nB++;
  ult = agora;
  pry = assPlayResYVisto > 0 ? assPlayResYVisto : 288;
  printf("[ass] strips: t=%lld ms | sharp pass %.0f%% (%.2f px per script px): %d thin images | soft pass %s%.0f%% (%.2f): %d thin | "
         "frame %d px at 100%%, k=%d, PlayResY %d\n",
         t, resol * 100.0, resol * altPx / pry, nA, resolB > 0 ? "" : "= sharp ", (resolB > 0 ? resolB : resol) * 100.0,
         (resolB > 0 ? resolB : resol) * altPx / pry, nB, altPx, k, pry);
  if (assTrack && ass_idx_garantir()) {
    const int *cand = NULL;
    int nc = ass_idx_candidatos(t, t, &cand);
    for (i = 0; i < nc && mostrados < 3; i++) {
      const ASS_Event *e = &assTrack->events[cand[i]];
      int cr, cv, dz; double bl, alt = ass_tira_altura(e->Text, &cr, &cv, &dz, &bl);
      if (alt < 0 || alt > 4.0) continue;
      mostrados++;
      printf("[ass] strips: event layer %d style %s | box %.2f script px | clip %s | drawing %s | blur %.2f | %.120s\n",
             e->Layer, e->Style >= 0 && e->Style < assTrack->n_styles && assTrack->styles[e->Style].Name ? assTrack->styles[e->Style].Name : "?",
             alt, cr ? "rect" : cv ? "vector" : "none", dz ? "yes" : "no", bl, e->Text ? e->Text : "");
    }
  }
  fflush(stdout);
}

static void *ass_worker_loop(void *unused) {
  (void)unused;
  assfios_nomear("nv-assrender");
#if defined(NV_TPK) && defined(__linux__)
  /* Crash capture (owner 14/11: a Persian subtitle kills this thread on its
   * first frame and the reporter wrote nothing). glibc gives a thread no
   * alternate signal stack, so a fault on a full stack could not run
   * queda.c's handler (SA_ONSTACK). This one can. The stack's bounds go to
   * the log: the crash line's sp says whether the stack ran out. */
  { static char *alt;
    pthread_attr_t at;
    if (!alt) alt = malloc(64 * 1024);
    if (alt) {
      stack_t ss;
      ss.ss_sp = alt; ss.ss_size = 64 * 1024; ss.ss_flags = 0;
      if (sigaltstack(&ss, NULL) != 0) printf("[ass] worker: sigaltstack failed\n");
    }
    if (pthread_getattr_np(pthread_self(), &at) == 0) {
      void *base = NULL; size_t tam = 0, guarda = 0;
      pthread_attr_getstack(&at, &base, &tam);
      pthread_attr_getguardsize(&at, &guarda);
      printf("[ass] worker stack: %zu KB at %p-%p (guard %zu KB), signal stack 64 KB %s\n",
             tam / 1024, base, (void *)((char *)base + tam), guarda / 1024, alt ? "on" : "off");
      pthread_attr_destroy(&at);
    }
    fflush(stdout); }
#endif
#ifdef NV_TPK40
  /* TIZEN 4/5 (owner, 16/11/2026, after the UE55RU7170 log of 10/11: frames
   * of 65-107 ms in a sign-heavy scene): the TV's player comes first. This
   * worker and the blur helpers it starts (they inherit it) run at nice 10. */
  if (ass_usable_cpus() > 1) {
    if (setpriority(PRIO_PROCESS, 0, 10) == 0)
      printf("[ass] tizen 4/5: subtitle worker and blur threads at low priority (nice 10)\n");
    else
      printf("[ass] tizen 4/5: could not lower the subtitle threads' priority\n");
  } else {
    printf("[ass] tizen 4/5: 1-core sandbox, subtitle worker at normal priority (nice 0)\n");
  }
  fflush(stdout);
#endif
  for (;;) {
    AssCpuFrame frame;
    unsigned generation, serial, epoch;
    double ms, esc, resol, resolB = 0.0;
    int dividir = 0, lwB = 0, lhB = 0;
    ASS_Image *imagesB = NULL;
    double msPassA = 0.0, msPassB = 0.0;
    int divOk = 0, umaPassada = 0;
    long divImgsA = 0, divImgsB = 0;
    float ox, oy;
    int lw, lh, vw, vh, plainValid, k;
    PlainAssStyle plain;
    struct timespec a, b, c0;
    ASS_Image *images = NULL;
    int changed = 0, pronto = 0, igual = 0, seq = 0;
    long long validoDe = LLONG_MAX, validoAte = LLONG_MAX;
    double cu[CU_N] = {0};
    struct timespec cuT;
    int cuEv = 0, cuAt = 0;
    (void)cuEv; (void)cuAt;
    memset(&frame, 0, sizeof frame);
    pthread_mutex_lock(&assFilaTrava);
    while (!assWorkerParar && !assPedidoPendente)
      pthread_cond_wait(&assFilaCond, &assFilaTrava);
    if (assWorkerParar) { pthread_mutex_unlock(&assFilaTrava); break; }
    generation = assPedidoGeracao; serial = assPedidoSerial; epoch = assEpoch; ms = assPedidoMs;
    /* Quadro a quadro (ver assSeqUltMs): o proximo da grade depois do ultimo
     * feito, nunca um que ja saiu da tela, ate o pedido. */
    if (assFilaTeto > 1 && ass_fps() > 0 && assAgoraUiGeracao == generation && assAgoraUiMs != LLONG_MIN) {
      long long alvo = (long long)llround(ms), agoraUi = assAgoraUiMs, prox = alvo;
      int continua = assSeqEpoch == epoch && assSeqGeracao == generation && assSeqUltMs != LLONG_MIN;
      /* Cena pesada (dono 26/10): como o Render ahead de 100 ms era antes — so o
       * pedido mais novo, sem os quadros do meio. A leve faz todos. */
      /* Every frame (owner 22/11/2026): On, or Auto while the render peak
       * fits, busy scenes render every grid frame like quiet ones. */
      if (ass_so_o_mais_novo()) prox = alvo;
      /* Salto: o quadro de agora primeiro. Documento novo (lote do mkvass): do
       * PROXIMO quadro, o de agora ja esta na tela. */
      else if (!continua) prox = agoraUi;
      else if (assPedidoForcado) prox = ass_grade_seguinte(agoraUi);
      else if (assSeqUltMs < alvo) {
        prox = ass_grade_seguinte(assSeqUltMs);
        /* Nada muda ate assValidoAte (Render only on changes): os quadros do
         * meio seriam iguais ao ultimo. */
        if (assValidoEpoch == epoch && assValidoGeracao == generation &&
            assValidoDe == assSeqUltMs && assValidoAte > prox && assValidoAte != LLONG_MAX)
          prox = assValidoAte >= alvo ? alvo : ass_grade_desde(assValidoAte);
        if (prox < agoraUi) prox = agoraUi;   // frames already off the screen (the worker fell behind)
      }
      if (prox < alvo) { ms = (double)prox; seq = 1; }
    }
    serial = ++assSerial;   // cada quadro publicado e outro (as texturas sobem de novo)
    assPedidoForcado = 0;
    int baseW = layPedW, baseH = layPedH;
    ox = layPedX; oy = layPedY; lw = baseW; lh = baseH; k = layPedK;
    /* assperf: resolucao do render. O quadro sai `resol` do tamanho do alvo e
     * o desenho amplia (esc = k x resol pixels por unidade). 1 = como antes. */
    /* Auto 50 / Auto 75 (owner 19/11/2026): busy frames in two passes, the
     * text and the blur at their own resolutions (assperf.h; libass-nv 0004);
     * quiet ones at 100 %, one pass. */
    /* Clean blur (22/11/2026): the grid is the text pass's frame height at
     * 100 % (layout x k: 2 on a 4K UI) and the script's PlayResY (read under
     * assTrava on an earlier frame; the worker is the only writer). */
    assperf_grade_definir(baseH * k, assPlayResYVisto);
    dividir = assperf_resolucao_quadro(&resol, &resolB);
#if !ASS_DIVIDIR
    dividir = 0; resolB = 0.0;
#endif
    if (dividir) {
      lwB = (int)lround((double)baseW * k * resolB); lhB = (int)lround((double)baseH * k * resolB);
      if (lwB < 16) lwB = 16;
      if (lhB < 16) lhB = 16;
    }
    lw = (int)lround((double)baseW * k * resol); lh = (int)lround((double)baseH * k * resol);   /* libass renderiza em pixels do alvo, nao em unidades de layout */
    if (lw < 16) lw = 16;
    if (lh < 16) lh = 16;
    vw = layPedVW; vh = layPedVH; esc = layPedEscala;
    plain = plainPed; plainValid = plainPedValido;
    assPedidoPendente = 0;
    pthread_mutex_unlock(&assFilaTrava);

    clock_gettime(CLOCK_MONOTONIC, &a);
    pthread_mutex_lock(&assTrava);
    clock_gettime(CLOCK_MONOTONIC, &c0);
    if (generation == __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) &&
        generation == assTrackGeracao &&
        assTrack && assRenderer) {
      long long t = (long long)llround(ms);
      /* The script's height as libass uses it: a script without PlayResY gets
       * libass's default on its first render (on the snapshot copy when the
       * rigid path renders that). */
      int pryAgora = assTrack->PlayResY > 0 ? assTrack->PlayResY :
                     assRigidCtx.snapTrack && assRigidCtx.snapTrack->PlayResY > 0 ? assRigidCtx.snapTrack->PlayResY : 0;
      if (pryAgora > 0 && pryAgora != assPlayResYVisto) {
        /* A new script size: the clean values change, take them this frame. */
        assPlayResYVisto = pryAgora;
        assperf_grade_definir(baseH * k, assPlayResYVisto);
        dividir = assperf_resolucao_quadro(&resol, &resolB);
#if !ASS_DIVIDIR
        dividir = 0; resolB = 0.0;
#endif
        if (dividir) {
          lwB = (int)lround((double)baseW * k * resolB); lhB = (int)lround((double)baseH * k * resolB);
          if (lwB < 16) lwB = 16;
          if (lhB < 16) lhB = 16;
        }
        lw = (int)lround((double)baseW * k * resol); lh = (int)lround((double)baseH * k * resol);
        if (lw < 16) lw = 16;
        if (lh < 16) lh = 16;
      }
      clock_gettime(CLOCK_MONOTONIC, &cuT);
      if (ASS_NOVO && ass_estilo_autor_locked() && !assperf_pesado()) {
        int nP = 0, nCP = 0;
        long cP = 0, cCP = 0;
        if (ass_idx_garantir()) {   // the time index (22/11/2026)
          ass_idx_janela(t, t + 1000, &nP, &cP);
          ass_idx_janela(t + 500, t + 2200, &nCP, &cCP);
        } else
        for (int pi = 0; pi < assTrack->n_events; pi++) {
          const ASS_Event *pev = &assTrack->events[pi];
          if (pev->Start < t || pev->Start >= t + 2200) continue;
          long plen = pev->Text ? (long)strlen(pev->Text) : 0;
          if (pev->Start < t + 1000) { nP++; cP += plen; }
          if (pev->Start >= t + 500) { nCP++; cCP += plen; }
        }
        if (nP >= 300 || cP >= 400000L || nCP >= 300 || cCP >= 400000L) {
          assperf_previsao(nP, cP, 25, nCP, cCP);
          dividir = assperf_resolucao_quadro(&resol, &resolB);
#if !ASS_DIVIDIR
          dividir = 0; resolB = 0.0;
#endif
          if (dividir) {
            lwB = (int)lround((double)baseW * k * resolB); lhB = (int)lround((double)baseH * k * resolB);
            if (lwB < 16) lwB = 16;
            if (lhB < 16) lhB = 16;
          }
          lw = (int)lround((double)baseW * k * resol); lh = (int)lround((double)baseH * k * resol);
          if (lw < 16) lw = 16;
          if (lh < 16) lh = 16;
        }
      }
      cu[CU_SCAN] += ass_ms_desde(&cuT);
#if ASS_DIVIDIR && ASS_NOVO
      /* The last frame was overloaded: this one in one pass, at the blur
       * resolution (see assUltimoImgs). */
      if (ass_uma_passada_sobrecarga(dividir, resolB) && ass_estilo_autor_locked()) {
        resol = resolB; dividir = 0; resolB = 0.0; umaPassada = 1;
        lw = (int)lround((double)baseW * k * resol); lh = (int)lround((double)baseH * k * resol);
        if (lw < 16) lw = 16;
        if (lh < 16) lh = 16;
      }
#endif
      ass_aplicar_assperf_locked();
      if (lw != layAplW || lh != layAplH) {
        ass_set_frame_size(assRenderer, lw, lh);
        assFrameW = layAplW = lw; assFrameH = layAplH = lh;
      }
      if (vw != layAplVW || vh != layAplVH) {
        ass_set_storage_size(assRenderer, vw, vh);
        layAplVW = vw; layAplVH = vh;
      }
      if (esc != layAplEscala) {
        ass_set_font_scale(assRenderer, esc);
        layAplEscala = esc;
      }
      if (plainValid) {
        PlainAssStyle ps = ass_estilo_autor_locked() ? (PlainAssStyle){0} : plain;
        ass_plain_style_locked(&ps);
      }
      if (ass_estilo_autor_locked())
        ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE);
      ass_plain_reflow_locked(t);
      ass_pod_devolver_assTrava();   // seek para tras alem da poda: as falas voltam antes do render
#if ASS_DIVIDIR
      /* Plain text (SRT drawn by libass) is never split, never has signs. */
      if (!ass_estilo_autor_locked()) dividir = 0;
      if (dividir && !assRendererB) ass_b_criar_locked();
      if (!assRendererB) dividir = 0;
      ass_nv_set_split(assRenderer, dividir ? 1 : 0, 0);
      if (dividir) {
        if (lwB != layBW || lhB != layBH) { ass_set_frame_size(assRendererB, lwB, lhB); layBW = lwB; layBH = lhB; }
        ass_set_storage_size(assRendererB, vw, vh);
        ass_set_font_scale(assRendererB, esc);
        ass_set_selective_style_override_enabled(assRendererB, ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE);
      }
      clock_gettime(CLOCK_MONOTONIC, &cuT);
      if (ass_estilo_autor_locked()) ass_marcar_sinais_locked(generation, t);
      cu[CU_SCAN] += ass_ms_desde(&cuT);
#endif
      int useRigid = ASS_NOVO && !__atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
      long long prox = LLONG_MAX, inicioMax = -1;
      int nIni = 0, nIniC = 0, nImg = 0, snapOk = 0;
      long chars = 0, charsC = 0;
      clock_gettime(CLOCK_MONOTONIC, &cuT);
      if (useRigid && ass_rigid_ensure_locked(&assRigidCtx)) {
        snapOk = ass_snap_build_locked(&assRigidCtx, generation, t, lw, lh, lwB, lhB, vw, vh, dividir, esc,
                                       &validoDe, &validoAte, &prox, &nIni, &chars, &nIniC, &charsC, &inicioMax);
      }
      cu[CU_SCAN] += ass_ms_desde(&cuT);
      cuEv = assTrack->n_events; cuAt = assIdxAtivosUlt;
#ifdef NV_TPK
      /* Crash capture (owner 14/11): what libass is asked to draw, once per
       * track and per style change, flushed BEFORE the render. A crash right
       * after this line is inside libass on that frame. */
      { static unsigned gerLog = ~0u; static PlainAssStyle psLog; static int psLogOk;
        int simples = __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
        int novo = generation != gerLog || (simples && plainValid && (!psLogOk || memcmp(&plain, &psLog, sizeof plain)));
        if (novo && assTrack->n_styles > 0) {
          const ASS_Style *st = &assTrack->styles[assTrack->default_style];
          gerLog = generation;
          if (plainValid) { psLog = plain; psLogOk = 1; }
          printf("[ass] render first frame: t=%lld ms, %s, events %d, frame %dx%d storage %dx%d scale %.3f | "
                 "style %s %.1f bold %d BorderStyle %d Outline %.2f Shadow %.2f Blur %.3f ScaleX %.0f | engine %d, %d blur thread(s)\n",
                 t, simples ? "plain RTL text" : "authored ASS", assTrack->n_events,
                 lw, lh, vw, vh, esc, st->FontName ? st->FontName : "-", st->FontSize, st->Bold, st->BorderStyle,
                 st->Outline, st->Shadow, st->Blur, st->ScaleX * 100.0,
#ifdef ASS_NV_API
                 ass_nv_get_engine(assRenderer), ass_nv_get_threads()
#else
                 -1, -1
#endif
                 );
          fflush(stdout);
        } }
#endif
      if (snapOk) {
        struct timespec pa, pb;
        if (assRigidCtx.missN > 0) {
          clock_gettime(CLOCK_MONOTONIC, &cuT);
          ass_rigid_render_misses(&assRigidCtx, generation, lw, lh, lwB, lhB,
                                  vw, vh, dividir, esc, (float)((double)k * resolB));
          cu[CU_MISS] = ass_ms_desde(&cuT);
        }
        clock_gettime(CLOCK_MONOTONIC, &pa);
        images = ass_render_frame(assRenderer, assRigidCtx.snapTrack, t, &changed);
        clock_gettime(CLOCK_MONOTONIC, &pb);
        msPassA = ass_ms_entre(&pa, &pb);
#if ASS_DIVIDIR
        if (dividir) {
          int changedB = 0;
          imagesB = ass_render_frame(assRendererB, assRigidCtx.snapTrack, t, &changedB);
          clock_gettime(CLOCK_MONOTONIC, &pa);
          msPassB = ass_ms_entre(&pb, &pa);
          if (changedB > changed) changed = changedB;
        }
#endif
#ifdef NV_TPK
        __atomic_store_n(&assProxInicio, prox, __ATOMIC_RELAXED);
#endif
        clock_gettime(CLOCK_MONOTONIC, &cuT);
        pronto = ass_frame_merge_rigid(&assRigidCtx, generation, lw, lh, lwB, lhB, vw, vh,
                                       dividir, esc, images, imagesB,
                                       (float)((double)k * resolB), &frame, &nImg);
        cu[CU_COPIA] = ass_ms_desde(&cuT);
        if (ASS_NOVO) assperf_previsao(nIni, chars, nImg, nIniC, charsC);
        if (dividir && pronto) {
          const ASS_Image *im;
          divImgsA = divImgsB = 0;
          for (im = images; im; im = im->next) divImgsA++;
          for (im = imagesB; im; im = im->next) divImgsB++;
          divOk = 1;
        }
        frame.ms = t; frame.inicioMax = inicioMax;
        frame.ox = ox; frame.oy = oy; frame.k = k;
        frame.esc = (float)((double)k * resol);
        goto rigid_done;
      }
      { struct timespec pa, pb;
        clock_gettime(CLOCK_MONOTONIC, &pa);
        images = ass_render_frame(assRenderer, assTrack, t, &changed);
        clock_gettime(CLOCK_MONOTONIC, &pb);
        msPassA = ass_ms_entre(&pa, &pb);
#if ASS_DIVIDIR
        if (dividir) {
          int changedB = 0;
          imagesB = ass_render_frame(assRendererB, assTrack, t, &changedB);
          clock_gettime(CLOCK_MONOTONIC, &pa);
          msPassB = ass_ms_entre(&pb, &pa);
          if (changedB > changed) changed = changedB;
        }
#endif
      }
#ifdef NV_TPK
      /* Borda das legendas (tpkborda.c): o proximo inicio de fala depois de t,
       * para o premult ligar ~200 ms antes de a linha aparecer. */
      { long long prox = LLONG_MAX;
        int i;
        for (i = 0; i < assTrack->n_events; i++)
          if (assTrack->events[i].Start > t && assTrack->events[i].Start < prox) prox = assTrack->events[i].Start;
        __atomic_store_n(&assProxInicio, prox, __ATOMIC_RELAXED); }
#endif
      /* CENA PESADA PREVISTA (dono 26/10): o que comeca no proximo segundo ja
       * esta na faixa (a colheita vai a frente). No .mks do dono: celular
       * (22:13) 377-1228 falas e 130-440 mil caracteres por segundo; desenhos
       * de 22:19 1,6 milhao; dedos (19:05) uma rajada de 132 / 176 mil; fala
       * comum 0-1. Mais o numero de imagens em tela (cena "simples"). */
      /* Also the second starting 0.5 s ahead: render resolution Auto drops to
       * 75 % that much before a busy scene, so its full redraw happens just
       * before the scene instead of in its first frames (owner 27/10). */
      if (ASS_NOVO) { int i, nIni = 0, nImg = 0, nIniC = 0; long chars = 0, charsC = 0; const ASS_Image *im;
        for (i = 0; i < assTrack->n_events; i++) {
          const ASS_Event *ev = &assTrack->events[i];
          long n;
          if (ev->Start < t || ev->Start >= t + 2200) continue;   /* text length only inside the window */
          n = ev->Text ? (long)strlen(ev->Text) : 0;
          if (ev->Start < t + 1000) { nIni++; chars += n; }
          if (ev->Start >= t + 500) { nIniC++; charsC += n; }
        }
        for (im = images; im; im = im->next) nImg++;
        for (im = imagesB; im; im = im->next) nImg++;
        assperf_previsao(nIni, chars, nImg, nIniC, charsC); }
      /* assperf: ate quando este quadro continua certo (ver assValidoAte). */
      if (ASS_NOVO) {
        int i, anima = 0;
        for (i = 0; i < assTrack->n_events && !anima; i++) {
          const ASS_Event *ev = &assTrack->events[i];
          long long ini = ev->Start, fimE = ev->Start + ev->Duration;
          if (ini > t) { if (ini < validoAte) validoAte = ini; }
          else if (fimE > t) { if (fimE < validoAte) validoAte = fimE; if (ass_evento_anima(ev)) anima = 1; }
        }
        validoDe = t;
        if (anima) validoAte = t;   // animando: cada quadro e outro
      }
      /* Igual ao publicado: nada a fazer, o quadro em tela continua certo. */
      pthread_mutex_lock(&assFilaTrava);
      igual = !changed && assProduziu && assProduzidoEpoch == epoch &&
              assProduzidoGeracao == generation && epoch == assEpoch;
      pthread_mutex_unlock(&assFilaTrava);
      if (!igual) {
        clock_gettime(CLOCK_MONOTONIC, &cuT);
        /* Texture keys after the flatten decision (22/11/2026): ass_frame_chaves. */
        pronto = dividir ? ass_frame_copiar2(images, imagesB, (float)((double)k * resolB), &frame, 0)
                         : ass_frame_copiar(images, &frame, 0);
        cu[CU_COPIA] = ass_ms_desde(&cuT);
        if (dividir && pronto) {
          const ASS_Image *im;
          divImgsA = divImgsB = 0;
          for (im = images; im; im = im->next) divImgsA++;
          for (im = imagesB; im; im = im->next) divImgsB++;
          divOk = 1;
        }
        frame.ms = t; frame.inicioMax = -1;
        frame.ox = ox; frame.oy = oy; frame.k = k;
        frame.esc = (float)((double)k * resol);
        { int i;
          for (i = 0; i < assTrack->n_events; i++) {
            long long ini = assTrack->events[i].Start;
            if (ini <= t && ini + assTrack->events[i].Duration > t && ini > frame.inicioMax)
              frame.inicioMax = ini;
          } }
      }
      rigid_done:;
      if (ASS_NOVO && pronto && assperf_pesado()) ass_diag_tiras(images, imagesB, t, resol, dividir ? resolB : 0.0, baseH * k, k);
    }
    pthread_mutex_unlock(&assTrava);
#if ASS_NOVO
    /* Read atomically: the GL thread writes these when the viewer picks a color. */
    int flatR = -1, imgsAntes = 0;
    if (pronto) {
      clock_gettime(CLOCK_MONOTONIC, &cuT);
      frame.esc = frame.esc > 0.01f ? frame.esc : (float)((double)k * resol);
      imgsAntes = frame.count;
      ass_ultimo_imgs_guardar(imgsAntes);
      flatR = ass_frame_flatten(&frame, lw, lh, __atomic_load_n(&assCorAtiva, __ATOMIC_ACQUIRE));
      cu[CU_FLAT] = ass_ms_desde(&cuT);
      clock_gettime(CLOCK_MONOTONIC, &cuT);
      if (!flatR) ass_frame_chaves(&frame);
      cu[CU_CHAVE] = ass_ms_desde(&cuT);
    }
#endif
    clock_gettime(CLOCK_MONOTONIC, &b);
    if (ASS_NOVO && pronto) assperf_escala_custo(ass_ms_entre(&c0, &b));

    pthread_mutex_lock(&assFilaTrava);
    { unsigned long long us = (unsigned long long)((b.tv_sec - a.tv_sec) * 1000000ll +
                              (b.tv_nsec - a.tv_nsec) / 1000ll);
      { double puro = ass_ms_entre(&c0, &b), pico;   // render without the wait for the lock
        __atomic_load(&assRenderPico, &pico, __ATOMIC_RELAXED);
        pico = puro > pico ? puro : pico * 0.95;
        __atomic_store(&assRenderPico, &pico, __ATOMIC_RELAXED); }
      assMed.renders++; assMed.renderSomaUs += us;
      if (divOk) {   /* Split frame: under assFilaTrava, like the log that reads it */
        assDiv.quadros++; assDiv.msA += msPassA; assDiv.msB += msPassB;
        if (msPassA > assDiv.maxA) assDiv.maxA = msPassA;
        if (msPassB > assDiv.maxB) assDiv.maxB = msPassB;
        assDiv.imgsA += divImgsA; assDiv.imgsB += divImgsB;
      }
      if (umaPassada && pronto) assDiv.umaPassada++;
      if (us > assMed.renderMaxUs) assMed.renderMaxUs = us;
      if (igual) assMed.iguais++;
#if ASS_NOVO
      if (pronto) {   /* [ass] cost */
        int ci;
        cu[CU_A] = msPassA; cu[CU_B] = msPassB;
        for (ci = 0; ci < CU_N; ci++) { cuSoma[ci] += cu[ci]; if (cu[ci] > cuMax[ci]) cuMax[ci] = cu[ci]; }
        cuQuadros++; cuEventos = cuEv; cuAtivos += cuAt;
        if (frame.count > 16 || flatR > 0) assFlatQuadros++;
        if (flatR > 0) assFlatFeitos++;
        if (flatR == 2) {
          assSobreQuadros++; assSobreImgsSoma += imgsAntes;
          if (imgsAntes > assSobreImgsMax) assSobreImgsMax = imgsAntes;
        }
      }
#endif
      assUltimoRenderUs = us; }
    /* SEM o antigo "fabs(assPedidoMs - ms) <= 50": com o relogio interpolado o
     * pedido anda 16 ms por quadro, e um render de mais de 50 ms na C9 jogava
     * fora TODO quadro — a legenda nunca aparecia. Um quadro de 30 ms atras e
     * melhor que nenhum; o proximo pedido ja esta na fila. */
    if (pronto && !assWorkerParar && epoch == assEpoch &&
        generation == __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) &&
        generation == assPedidoGeracao) {
      frame.generation = generation; frame.serial = serial;
      assBytesQuadro = frame.bytes;
      ass_fila_publicar_locked(&frame);
      assProduziu = 1; assProduzidoEpoch = epoch; assProduzidoGeracao = generation;
    }
    if (validoDe != LLONG_MAX && !assWorkerParar && epoch == assEpoch &&
        generation == __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) && (pronto || igual)) {
      assValidoDe = validoDe; assValidoAte = validoAte;
      assValidoEpoch = epoch; assValidoGeracao = generation;
    } else if (!igual && !pronto) {
      assValidoDe = LLONG_MAX; assValidoAte = LLONG_MIN;
    }
    if (epoch == assEpoch) {
      assSeqUltMs = (long long)llround(ms); assSeqEpoch = epoch; assSeqGeracao = generation;
      /* Ainda falta ate o pedido: o proximo quadro ja, sem esperar o desenho
       * (com a fila cheia espera: o desenho pede de novo quando um sair). */
      if (seq && !assWorkerParar && !assPedidoPendente && generation == assPedidoGeracao &&
          assSeqUltMs < (long long)llround(assPedidoMs) && assFilaN < assFilaTeto)
        assPedidoPendente = 1;
    }
    ass_frame_liberar(&frame);
    pthread_mutex_unlock(&assFilaTrava);
  }
  return NULL;
}

static int ass_worker_iniciar(void) {
  int ok = 1;
  pthread_mutex_lock(&assFilaTrava);
  if (!assWorkerCriado) {
    assWorkerParar = 0;
    if (pthread_create(&assWorker, NULL, ass_worker_loop, NULL) == 0) {
      pthread_detach(assWorker);
      __atomic_store_n(&assWorkerCriado, 1, __ATOMIC_RELEASE);
    } else ok = 0;
  }
  pthread_mutex_unlock(&assFilaTrava);
  return ok;
}

static void ass_apagar_texturas_locked(void) {
  int i;
  for (i = 0; i < assTexCap; i++) {
    if (assTex[i].tex) {
      gfx_tex_esquecer(assTex[i].tex);
      glDeleteTextures(1, &assTex[i].tex);
      assTex[i].tex = 0;
    }
    assTex[i].w = assTex[i].h = 0;
  }
  free(assTex); assTex = NULL; assTexCap = 0;
  assTexSerial = 0;
}

static int ass_reservar_texturas_locked(int slot) {
  int nc, i;
  AssTex *nv;
  if (slot < assTexCap) return 1;
  nc = assTexCap ? assTexCap : ASS_TEX_INICIAL;
  while (nc <= slot) {
    if (nc > 4096) { nc = slot + 1; break; }
    nc *= 2;
  }
  nv = realloc(assTex, (size_t)nc * sizeof *nv);
  if (!nv) return 0;
  for (i = assTexCap; i < nc; i++) { nv[i].tex = 0; nv[i].w = nv[i].h = 0; }
  assTex = nv; assTexCap = nc;
  return 1;
}

static GLuint ass_textura_locked(int slot, const AssCpuImage *im) {
  int x, y;
  unsigned char *rgba;
  unsigned color = im->color;
  unsigned r = (color >> 24) & 255u;
  unsigned g = (color >> 16) & 255u;
  unsigned b = (color >> 8) & 255u;
  unsigned opacidade = 255u - (color & 255u);
  size_t bytes = (size_t)im->w * (size_t)im->h * 4u;

  if (assCorAtiva && im->type == IMAGE_TYPE_CHARACTER) {
    r = (unsigned)assCorR; g = (unsigned)assCorG; b = (unsigned)assCorB;
  }

  if (slot < 0 || !im->bitmap || !im->w || !im->h ||
      !ass_reservar_texturas_locked(slot)) return 0;
  if (!assTex[slot].tex) glGenTextures(1, &assTex[slot].tex);
  rgba = (unsigned char *)malloc(bytes);
  if (!rgba) return 0;
  for (y = 0; y < im->h; y++) {
    const unsigned char *src = im->bitmap + (size_t)y * (size_t)im->stride;
    for (x = 0; x < im->w; x++) {
      unsigned a = ((unsigned)src[x] * opacidade + 127u) / 255u;
      unsigned char *dst = rgba + ((size_t)y * (size_t)im->w + (size_t)x) * 4u;
      dst[0] = (unsigned char)r;
      dst[1] = (unsigned char)g;
      dst[2] = (unsigned char)b;
      dst[3] = (unsigned char)a;
    }
  }
  glBindTexture(GL_TEXTURE_2D, assTex[slot].tex);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, im->w, im->h, 0,
               GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  gfx_tex_esquecer(0);
  free(rgba);
  assTex[slot].w = im->w; assTex[slot].h = im->h;
  return assTex[slot].tex;
}

/* ===== MATRIZ YCbCr DAS CORES DO ASS (16/11/2026) =============================
 * Uma cor do script foi escolhida olhando o video decodificado com a matriz do
 * "YCbCr Matrix:" do script (o VSFilter decodificava tudo em BT.601). A TV
 * decodifica o video com a matriz DELE (BT.709 num 1080p). Sem conversao, um
 * letreiro que casava com a parede do video sai com outro tom em cima dela.
 * Como o mpv (sd_ass.c, mangle_colors) e o JASSUB: a cor do script volta ao
 * YCbCr com a matriz e a faixa do script e sai em RGB com as do video.
 *   - script sem cabecalho (YCBCR_DEFAULT): BT.601 TV, a regra do VSFilter;
 *   - "None" ou desconhecida: nada muda;
 *   - video RGB, BT.2020 ou HDR: nada muda (a TV mapeia o HDR por conta dela);
 *   - matriz do video nao declarada: BT.709 de 720 linhas para cima, senao
 *     BT.601 (a regra do mpv). Faixa nao declarada: TV.
 * A cor que a pessoa escolheu para o dialogo e o texto simples nao convertem. */
static int assTrackMatriz = -1;   // ASS_YCbCrMatrix da faixa; -1 = sem faixa / texto simples
static int assVidMatriz, assVidFaixa, assVidHdr, assVidAlt;

void assrender_definir_cor_video(int matriz, int faixa, int hdr, int altura) {
  __atomic_store_n(&assVidMatriz, matriz, __ATOMIC_RELAXED);
  __atomic_store_n(&assVidFaixa, faixa, __ATOMIC_RELAXED);
  __atomic_store_n(&assVidHdr, hdr, __ATOMIC_RELAXED);
  __atomic_store_n(&assVidAlt, altura, __ATOMIC_RELAXED);
}

typedef struct { double kr, kb; int pc; } AssCsp;

static int ass_csp_script(int m, AssCsp *c) {
  c->pc = 0;
  switch (m) {
    case YCBCR_DEFAULT: case YCBCR_BT601_TV: c->kr = 0.299; c->kb = 0.114; return 1;
    case YCBCR_BT601_PC: c->kr = 0.299; c->kb = 0.114; c->pc = 1; return 1;
    case YCBCR_BT709_TV: c->kr = 0.2126; c->kb = 0.0722; return 1;
    case YCBCR_BT709_PC: c->kr = 0.2126; c->kb = 0.0722; c->pc = 1; return 1;
    case YCBCR_SMPTE240M_TV: c->kr = 0.212; c->kb = 0.087; return 1;
    case YCBCR_SMPTE240M_PC: c->kr = 0.212; c->kb = 0.087; c->pc = 1; return 1;
    case YCBCR_FCC_TV: c->kr = 0.30; c->kb = 0.11; return 1;
    case YCBCR_FCC_PC: c->kr = 0.30; c->kb = 0.11; c->pc = 1; return 1;
    default: return 0;   // UNKNOWN, NONE
  }
}

/* Matriz do video (ISO/IEC 23001-8, a mesma numeracao do MKV). */
static int ass_csp_video(int m, int faixa, int hdr, int altura, AssCsp *c) {
  if (hdr) return 0;
  if (m == 0 || m == 2 || m == 3) m = altura >= 720 || altura <= 0 ? 1 : 6;   // nao declarada
  c->pc = faixa == 2;
  switch (m) {
    case 1: c->kr = 0.2126; c->kb = 0.0722; return 1;
    case 4: c->kr = 0.30; c->kb = 0.11; return 1;
    case 5: case 6: c->kr = 0.299; c->kb = 0.114; return 1;
    case 7: c->kr = 0.212; c->kb = 0.087; return 1;
    default: return 0;   // BT.2020 e o resto
  }
}

/* RGB 0..1 do script -> RGB 0..1 como a TV mostraria o mesmo YCbCr. */
static void ass_csp_converter(const AssCsp *s, const AssCsp *v, double rgb[3]) {
  double y = s->kr * rgb[0] + (1.0 - s->kr - s->kb) * rgb[1] + s->kb * rgb[2];
  double pb = (rgb[2] - y) / (2.0 * (1.0 - s->kb)), pr = (rgb[0] - y) / (2.0 * (1.0 - s->kr));
  double yd, cb, cr, g;
  int k;
  /* Codigos de 8 bits divididos por 255, como no arquivo. */
  if (s->pc) { yd = y; cb = 128.0 / 255.0 + pb; cr = 128.0 / 255.0 + pr; }
  else { yd = (16.0 + 219.0 * y) / 255.0; cb = (128.0 + 224.0 * pb) / 255.0; cr = (128.0 + 224.0 * pr) / 255.0; }
  if (v->pc) { y = yd; pb = cb - 128.0 / 255.0; pr = cr - 128.0 / 255.0; }
  else { y = (yd * 255.0 - 16.0) / 219.0; pb = (cb * 255.0 - 128.0) / 224.0; pr = (cr * 255.0 - 128.0) / 224.0; }
  rgb[0] = y + 2.0 * (1.0 - v->kr) * pr;
  rgb[2] = y + 2.0 * (1.0 - v->kb) * pb;
  g = (y - v->kr * rgb[0] - v->kb * rgb[2]) / (1.0 - v->kr - v->kb);
  rgb[1] = g;
  for (k = 0; k < 3; k++) rgb[k] = rgb[k] < 0.0 ? 0.0 : rgb[k] > 1.0 ? 1.0 : rgb[k];
}

/* Converte `v` (RGB 0..1 de uma imagem do libass) quando o script e o video
 * diferem. Fio GL; le so atomicos. */
static __attribute__((unused)) void ass_cor_matriz(double v[3]) {
  AssCsp s, d;
  int tm = __atomic_load_n(&assTrackMatriz, __ATOMIC_ACQUIRE);
  if (tm < 0 || !ass_csp_script(tm, &s)) return;
  if (!ass_csp_video(__atomic_load_n(&assVidMatriz, __ATOMIC_RELAXED),
                     __atomic_load_n(&assVidFaixa, __ATOMIC_RELAXED),
                     __atomic_load_n(&assVidHdr, __ATOMIC_RELAXED),
                     __atomic_load_n(&assVidAlt, __ATOMIC_RELAXED), &d)) return;
  if (s.kr == d.kr && s.kb == d.kb && s.pc == d.pc) return;
  ass_csp_converter(&s, &d, v);
}

#if ASS_NOVO   /* only built where it runs (.tpk): off it, nothing of this is used */
/* ===== O CAMINHO NOVO DO DESENHO (fio GL, ASS_NOVO) =========================
 *
 * O caminho antigo (acima, assAtual + assTex) e o de fora do .tpk. Este:
 *   - Alpha 8-bit: a textura e o bitmap do libass como veio (GL_ALPHA, 1 byte
 *     por pixel, sem a conversao RGBA por pixel na CPU); a cor e a opacidade
 *     da imagem vao no desenho (GFX_MARCA: forma do alfa, cor de uCor).
 *   - Cache de texturas pela CHAVE do conteudo: um letreiro que nao mudou nao
 *     sobe de novo. Uma textura so e regravada depois de ASS_TC_RECICLA
 *     quadros sem ser desenhada (a GPU ainda pode le-la).
 *   - Upload fora da trava: o quadro em tela (assMain) e do fio GL; o upload
 *     corre sem assFilaTrava, que o worker precisa para pegar o proximo pedido.
 * Quem invalida de outro fio (troca de faixa, seek, geracao) liga
 * assTexResetar, como sempre; o fio GL entao solta assMain e as texturas. */
typedef struct {
  GLuint tex;
  int w, h;
  unsigned long long chave;
  unsigned uso;          // assGlQuadro do ultimo desenho
} AssTc;
/* 22/11: 2048 / 64 MB (was 768 / 24 MB: the busy ED hit it). Caches row Small
 * (owner 23/11/2026, KeijoMika's ass-minimal): 768 / 24 MB again; textures
 * past the cap leave through the usual recycling. */
#define ASS_TC_TETO 2048
#define ASS_TC_BYTES (64u * 1024u * 1024u)
#define ASS_TC_RECICLA 2u
static AssTc *assTc;
static int assTcN, assTcCap;
static size_t assTcBytes;
static unsigned assGlQuadro = 1000;
static AssCpuFrame assMain;      // o quadro em tela no caminho novo (so o fio GL toca)
static int assMainValido;
static GLuint *assMainTex;       // textura de cada imagem do assMain
static int *assMainIdx;          // e o indice dela em assTc
static int assMainTexCap, assMainEnviado;
static int assMainResetar;       // assrender_aplicar_invalidacao consumiu um assTexResetar
#define ASS_FLAT_RING 3
/* A flattened frame is one RGBA image (split frames stay per image since
 * 23/11/2026): one texture per ring slot. */
#define ASS_FLAT_CAMADAS 1
static GLuint assFlatTex[ASS_FLAT_RING][ASS_FLAT_CAMADAS];
static int assFlatW[ASS_FLAT_RING][ASS_FLAT_CAMADAS], assFlatH[ASS_FLAT_RING][ASS_FLAT_CAMADAS];
static int assFlatCur;

static int ass_flat_bucket(int v) {
  if (v <= 16) return 16;
  if (v <= 64) return (v + 15) & ~15;
  if (v <= 256) return (v + 31) & ~31;
  return (v + 63) & ~63;
}

static size_t ass_tc_tam(int w, int h) { return (size_t)w * (size_t)h; }

static void ass_tc_apagar(void) {
  int i;
  for (i = 0; i < assTcN; i++)
    if (assTc[i].tex) { gfx_tex_esquecer(assTc[i].tex); glDeleteTextures(1, &assTc[i].tex); }
  free(assTc); assTc = NULL; assTcN = assTcCap = 0; assTcBytes = 0;
  for (i = 0; i < ASS_FLAT_RING; i++) for (int c = 0; c < ASS_FLAT_CAMADAS; c++) {
    if (assFlatTex[i][c]) { gfx_tex_esquecer(assFlatTex[i][c]); glDeleteTextures(1, &assFlatTex[i][c]); assFlatTex[i][c] = 0; }
    assFlatW[i][c] = assFlatH[i][c] = 0;
  }
  assFlatCur = 0;
  assMainEnviado = 0;
}

static void ass_main_soltar(void) {
  ass_frame_liberar(&assMain); assMainValido = 0; assMainEnviado = 0;
}

/* Sobe `im` em `tex`. `novo` = a textura nao tem armazenamento desse tamanho
 * (glTexImage2D); senao glTexSubImage2D, sem realocar. */
static void ass_tc_subir(GLuint tex, const AssCpuImage *im, int novo) {
  glBindTexture(GL_TEXTURE_2D, tex);
  if (novo) {
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, im->w, im->h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, im->bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  } else {
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im->w, im->h, GL_ALPHA, GL_UNSIGNED_BYTE, im->bitmap);
  }
}

/* Textura para a imagem pelo CONTEUDO. */
static GLuint ass_tc_obter(const AssCpuImage *im, unsigned long long chave, int *idx) {
  int i, livre = -1, velha = -1;
  *idx = -1;
  for (i = 0; i < assTcN; i++) {
    AssTc *t = &assTc[i];
    if (t->chave == chave && t->w == im->w && t->h == im->h && t->tex) {
      t->uso = assGlQuadro; *idx = i;
      return t->tex;
    }
  }
  for (i = 0; i < assTcN; i++) {
    AssTc *t = &assTc[i];
    if (assGlQuadro - t->uso < ASS_TC_RECICLA) continue;   // desenhada ha pouco: a GPU ainda pode le-la
    if (t->w == im->w && t->h == im->h) { livre = i; break; }
    if (velha < 0 || t->uso < assTc[velha].uso) velha = i;
  }
  if (livre >= 0) {   // mesmo tamanho: regrava sem realocar
    AssTc *t = &assTc[livre];
    ass_tc_subir(t->tex, im, 0);
    t->chave = chave; t->uso = assGlQuadro; *idx = livre;
    return t->tex;
  }
  if (assTcN < ASS_TC_TETO && assTcBytes + ass_tc_tam(im->w, im->h) <= ASS_TC_BYTES) {
    AssTc *t;
    if (assTcN == assTcCap) {
      int nc = assTcCap ? assTcCap * 2 : 64;
      AssTc *nv = (AssTc *)realloc(assTc, (size_t)nc * sizeof *nv);
      if (!nv) return 0;
      assTc = nv; assTcCap = nc;
    }
    t = &assTc[assTcN];
    memset(t, 0, sizeof *t);
    glGenTextures(1, &t->tex);
    if (!t->tex) return 0;
    *idx = assTcN++;
    ass_tc_subir(t->tex, im, 1);
    t->w = im->w; t->h = im->h; t->chave = chave; t->uso = assGlQuadro;
    assTcBytes += ass_tc_tam(im->w, im->h);
    return t->tex;
  }
  if (velha >= 0) {   // cheio: a mais antiga fora de uso muda de tamanho
    AssTc *t = &assTc[velha];
    assTcBytes -= ass_tc_tam(t->w, t->h);
    ass_tc_subir(t->tex, im, 1);
    t->w = im->w; t->h = im->h; t->chave = chave; t->uso = assGlQuadro;
    assTcBytes += ass_tc_tam(im->w, im->h);
    *idx = velha;
    return t->tex;
  }
  __atomic_add_fetch(&assTcFora, 1L, __ATOMIC_RELAXED);   // [ass] cost: images left out
  return 0;   // tudo em uso agora (raro): a imagem fica de fora deste quadro
}

/* A cor de uma imagem como sai na tela: cor da legenda (ou a da pessoa no
 * dialogo), ganho do contraste e curva de gama da TV (AP_CONTRASTE, AP_GAMA),
 * na mesma ordem da TV. O ganho satura em 1 antes da curva, como o video. */
static void ass_cor_final(const AssCpuImage *im, int corAtiva, int cr, int cg, int cb,
                          double gama, double contraste, float *r, float *g, float *b, float *op) {
  unsigned c = im->color;
  double v[3];
  int k;
  v[0] = (double)((c >> 24) & 255u) / 255.0; v[1] = (double)((c >> 16) & 255u) / 255.0;
  v[2] = (double)((c >> 8) & 255u) / 255.0; *op = (float)(255u - (c & 255u)) / 255.0f;
  if (corAtiva && im->type == IMAGE_TYPE_CHARACTER) { v[0] = cr / 255.0; v[1] = cg / 255.0; v[2] = cb / 255.0; }
  else ass_cor_matriz(v);   // a cor da pessoa nao veio do script: fica como ela escolheu
  if (gama != 1.0 || contraste != 1.0)
    for (k = 0; k < 3; k++) {
      double t = v[k] * contraste;
      v[k] = pow(t < 0.0 ? 0.0 : t > 1.0 ? 1.0 : t, gama);
    }
  *r = (float)v[0]; *g = (float)v[1]; *b = (float)v[2];
}

/* Sobe (se o quadro e novo) e desenha assMain. Fio GL. */
/* "[ass] 10s tv" (Tizen 4-6 playback damage, 16/11): what drawing the subtitles
 * costs the GL thread per frame (uploads of new frames + the draw calls). */
static long long glDesUs, glDesMaxUs;   // __atomic: the GL thread adds, the 10 s report takes
static long glDesN, glDesNovos;
static int ass_main_desenhar_corpo(float x, float y, float alpha, int corAtiva, int cr, int cg, int cb,
                                   int quadroNovo);
static int ass_main_desenhar(float x, float y, float alpha, int corAtiva, int cr, int cg, int cb,
                             int quadroNovo) {
  struct timespec t0, t1;
  long long us, mx;
  int r;
  clock_gettime(CLOCK_MONOTONIC, &t0);
  r = ass_main_desenhar_corpo(x, y, alpha, corAtiva, cr, cg, cb, quadroNovo);
  clock_gettime(CLOCK_MONOTONIC, &t1);
  us = (long long)(t1.tv_sec - t0.tv_sec) * 1000000LL + (t1.tv_nsec - t0.tv_nsec) / 1000;
  __atomic_add_fetch(&glDesUs, us, __ATOMIC_RELAXED);
  __atomic_add_fetch(&glDesN, 1, __ATOMIC_RELAXED);
  if (quadroNovo) __atomic_add_fetch(&glDesNovos, 1, __ATOMIC_RELAXED);
  mx = __atomic_load_n(&glDesMaxUs, __ATOMIC_RELAXED);
  while (us > mx && !__atomic_compare_exchange_n(&glDesMaxUs, &mx, us, 1, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) {}
  return r;
}
static int ass_main_desenhar_corpo(float x, float y, float alpha, int corAtiva, int cr, int cg, int cb,
                                   int quadroNovo) {
  int i;
  float kk = assMain.k > 1 ? (float)assMain.k : 1.0f, esc = assMain.esc > 0.01f ? assMain.esc : kk;
  assGlQuadro++;
  /* Flattened frame: 1 RGBA image. */
  if (assMain.count >= 1 && assMain.count <= ASS_FLAT_CAMADAS && assMain.images[0].rgba) {
    int c, nC = assMain.count;
    if (quadroNovo || !assMainEnviado) {
      struct timespec t0, t1;
      clock_gettime(CLOCK_MONOTONIC, &t0);
      if (quadroNovo) assFlatCur = (assFlatCur + 1) % ASS_FLAT_RING;
      int s = assFlatCur;
      glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
      for (c = 0; c < nC; c++) {
        const AssCpuImage *im = &assMain.images[c];
        if (!im->rgba || !im->bitmap) continue;
        if (!assFlatTex[s][c]) glGenTextures(1, &assFlatTex[s][c]);
        if (!assFlatTex[s][c]) continue;
        int bw = ass_flat_bucket(im->w), bh = ass_flat_bucket(im->h);
        if (bw < assFlatW[s][c]) bw = assFlatW[s][c];
        if (bh < assFlatH[s][c]) bh = assFlatH[s][c];
        glBindTexture(GL_TEXTURE_2D, assFlatTex[s][c]);
        if (bw != assFlatW[s][c] || bh != assFlatH[s][c]) {
          glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bw, bh, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
          glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
          assFlatW[s][c] = bw; assFlatH[s][c] = bh;
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im->w, im->h, GL_RGBA, GL_UNSIGNED_BYTE, im->bitmap);
      }
      gfx_tex_esquecer(0);
      clock_gettime(CLOCK_MONOTONIC, &t1);
      assMainEnviado = 1;
      if (quadroNovo) assperf_carga_gl(ass_ms_entre(&t0, &t1));
    }
    int s = assFlatCur;
    gfx_tex_aspect_atual = 0.0f;
    for (c = 0; c < nC; c++) {
      const AssCpuImage *im = &assMain.images[c];
      float e = im->esc > 0.01f ? im->esc : esc;   // a soft layer: its own pass's scale
      if (!im->rgba || !assFlatTex[s][c] || assFlatW[s][c] <= 0 || assFlatH[s][c] <= 0) continue;
      float dx = roundf((x + assMain.ox) * kk) / kk + (float)im->x / e;
      float dy = roundf((y + assMain.oy) * kk) / kk + (float)im->y / e;
      float parx = (float)im->w / (float)assFlatW[s][c];
      float pary = (float)im->h / (float)assFlatH[s][c];
      int minDim = assFlatW[s][c] < assFlatH[s][c] ? assFlatW[s][c] : assFlatH[s][c];
      float inset = 0.5f / (float)minDim;
      gfx_rect((GfxRect){ dx, dy, (float)im->w / e, (float)im->h / e }, assFlatTex[s][c], GFX_TEXTO,
               0, parx, pary, inset, 1, 1, 1, alpha);
    }
    return 1;
  }
  if (quadroNovo || !assMainEnviado) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    if (assMain.count > assMainTexCap) {
      GLuint *nv = (GLuint *)realloc(assMainTex, (size_t)assMain.count * sizeof *nv);
      int *ni;
      if (!nv) return 0;
      assMainTex = nv;
      ni = (int *)realloc(assMainIdx, (size_t)assMain.count * sizeof *ni);
      if (!ni) return 0;
      assMainIdx = ni;
      assMainTexCap = assMain.count;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (i = 0; i < assMain.count; i++) {
      const AssCpuImage *im = &assMain.images[i];
      unsigned long long ch = im->chave ? im->chave : ass_chave_bitmap(im->bitmap, im->w, im->h, 0u);
      assMainTex[i] = ass_tc_obter(im, ch, &assMainIdx[i]);
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gfx_tex_esquecer(0);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    assMainEnviado = 1;
    /* Busy-scene load (picture sync, Auto): what a new frame cost here. */
    if (quadroNovo) assperf_carga_gl(ass_ms_entre(&t0, &t1));
  } else {
    /* Quadro repetido: as texturas dele continuam "em uso" (a reciclagem
     * conta quadros GL sem desenho). */
    for (i = 0; i < assMain.count; i++) {
      if (assMainIdx[i] >= 0 && assMainIdx[i] < assTcN) assTc[assMainIdx[i]].uso = assGlQuadro;
    }
  }
  /* Calibracao para casar com o video (AP_CONTRASTE, AP_GAMA): a imagem da TV
   * so processa o plano de video, nunca a nossa camada. Na mesma ordem da TV:
   * o ganho do contraste antes, a curva de gama depois. */
  const double gama = assperf_gama(), contraste = assperf_contraste();
  /* Plain text fades as ONE picture (owner 14/11/2026, ass_borda_estilo). */
  static GfxGrupo grupo;
  const float alphaGrupo = alpha;
  if (__atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE) && alpha < 0.999f && assMain.count > 0) {
    float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
    for (i = 0; i < assMain.count; i++) {
      const AssCpuImage *im = &assMain.images[i];
      float dx = (x + assMain.ox) + (float)im->x / esc, dy = (y + assMain.oy) + (float)im->y / esc;
      if (!assMainTex[i]) continue;
      if (dx < x0) x0 = dx;
      if (dy < y0) y0 = dy;
      if (dx + (float)im->w / esc > x1) x1 = dx + (float)im->w / esc;
      if (dy + (float)im->h / esc > y1) y1 = dy + (float)im->h / esc;
    }
    if (x1 > x0 && y1 > y0 && gfx_grupo_comecar(&grupo, (GfxRect){ x0 - 2.0f, y0 - 2.0f, x1 - x0 + 4.0f, y1 - y0 + 4.0f }, alpha))
      alpha = 1.0f;
  }
  for (i = 0; i < assMain.count; i++) {
    const AssCpuImage *im = &assMain.images[i];
    GLuint tex = assMainTex[i];
    float dx, dy, r, g, b, op;
    if (!tex) continue;
    gfx_tex_aspect_atual = 0.0f;
    /* Origem arredondada ao pixel do ALVO (kk); a imagem em pixels do
     * quadro do libass (esc = kk x resolucao), ampliada quando esc < kk.
     * Split frames: the soft pass's images have their own scale. */
    { float e = im->esc > 0.01f ? im->esc : esc;
      dx = roundf((x + assMain.ox) * kk) / kk + ((float)im->x + im->subX) / e;
      dy = roundf((y + assMain.oy) * kk) / kk + ((float)im->y + im->subY) / e;
      ass_cor_final(im, corAtiva, cr, cg, cb, gama, contraste, &r, &g, &b, &op);
      gfx_rect((GfxRect){ dx, dy, (float)im->w / e, (float)im->h / e }, tex, GFX_MARCA,
               0, 0, 0, 0, r, g, b, alpha * op); }
  }
  gfx_grupo_terminar(&grupo, alphaGrupo);
  gfx_tex_aspect_atual = 0.0f;
  return assMain.count;
}
#endif   /* ASS_NOVO: o caminho novo do desenho */

/* ===== assperf: CARGA INCREMENTAL ("Batch loading = New lines only") =======
 *
 * Cada lote do mkvass entregava o documento INTEIRO (milhares de linhas de
 * letreiro quadro a quadro) e ass_carregar o re-lia com ass_read_memory sob
 * assTrava: o worker ficava parado a carga toda, 58 vezes em 20 s no log.
 * Aqui o documento e varrido FORA da trava (so o hash de cada linha) e so as
 * linhas que ainda nao estao na faixa do libass entram, com ass_process_data,
 * sob a trava por microssegundos.
 *
 * As contas sao de MULTICONJUNTO: uma linha que aparece duas vezes no
 * documento (letreiro em duas camadas iguais) entra duas vezes. incEv[i] e o
 * hash da linha de que veio o evento i da faixa, alinhado com
 * assTrack->events. "Drop past events": sai da faixa o evento que ja acabou
 * ha mais de N antes do playhead E que o documento nao traz mais (o mkvass o
 * despejou da janela). Se o documento ainda o traz, fica — o seek para tras
 * continua achando a fala.
 *
 * Trava: incTrava antes de assTrava, sempre. O cabecalho do documento (tudo
 * antes da primeira "Dialogue:") muda = faixa outra: volta a carga cheia. */
typedef struct { unsigned long long *k; int *v; unsigned *g; int cap, n; unsigned gen; } AssMapa;
static pthread_mutex_t incTrava = PTHREAD_MUTEX_INITIALIZER;
static int incPronto;
static unsigned incGeracao;
static unsigned long long incCab;
static unsigned long long *incEv;
static int incEvN, incEvCap;
static AssMapa incAdd, incDoc;
static int assLeituraSeq;   // ReadOrder das falas da carga incremental (sob assTrava)

static unsigned long long ass_hash_bytes(const char *p, size_t n) {
  unsigned long long h = 0xCBF29CE484222325ULL ^ (unsigned long long)n;
  size_t i = 0;
  for (; i + 4 <= n; i += 4) { uint32_t v; memcpy(&v, p + i, 4); h = (h ^ v) * 0x100000001B3ULL; h ^= h >> 31; }
  for (; i < n; i++) h = (h ^ (unsigned char)p[i]) * 0x100000001B3ULL;
  return h ? h : 1;
}

static void mapa_limpar(AssMapa *m) { m->gen++; m->n = 0; if (!m->gen) { m->gen = 1; if (m->g) memset(m->g, 0, (size_t)m->cap * sizeof *m->g); } }

static int mapa_crescer(AssMapa *m) {
  int nc = m->cap ? m->cap * 2 : 4096, i;
  AssMapa novo; memset(&novo, 0, sizeof novo);
  novo.k = (unsigned long long *)calloc((size_t)nc, sizeof *novo.k);
  novo.v = (int *)calloc((size_t)nc, sizeof *novo.v);
  novo.g = (unsigned *)calloc((size_t)nc, sizeof *novo.g);
  if (!novo.k || !novo.v || !novo.g) { free(novo.k); free(novo.v); free(novo.g); return 0; }
  novo.cap = nc; novo.gen = m->gen ? m->gen : 1;
  for (i = 0; i < m->cap; i++) {
    if (m->g[i] != m->gen) continue;
    { unsigned j = (unsigned)(m->k[i] ^ (m->k[i] >> 32)) & (unsigned)(nc - 1);
      while (novo.g[j] == novo.gen) j = (j + 1) & (unsigned)(nc - 1);
      novo.k[j] = m->k[i]; novo.v[j] = m->v[i]; novo.g[j] = novo.gen; novo.n++; }
  }
  free(m->k); free(m->v); free(m->g);
  *m = novo;
  return 1;
}

/* Ponteiro para a contagem de `k` (criada com 0). NULL so sem memoria. */
static int *mapa_conta(AssMapa *m, unsigned long long k) {
  unsigned j;
  if (!m->gen) m->gen = 1;
  if ((m->n + 1) * 10 >= m->cap * 7 && !mapa_crescer(m)) return NULL;
  j = (unsigned)(k ^ (k >> 32)) & (unsigned)(m->cap - 1);
  while (m->g[j] == m->gen) {
    if (m->k[j] == k) return &m->v[j];
    j = (j + 1) & (unsigned)(m->cap - 1);
  }
  m->g[j] = m->gen; m->k[j] = k; m->v[j] = 0; m->n++;
  return &m->v[j];
}

static void ass_pod_soltar(void);
static void ass_inc_esquecer(void) {
  pthread_mutex_lock(&incTrava);
  incPronto = 0; incEvN = 0; incCab = 0;
  ass_pod_soltar();
  pthread_mutex_unlock(&incTrava);
}

/* Bytes do cabecalho: tudo antes da primeira linha "Dialogue:". */
static size_t ass_inc_cabecalho(const char *c, size_t n) {
  size_t i = 0;
  while (i < n) {
    if (n - i >= 9 && !memcmp(c + i, "Dialogue:", 9)) return i;
    { const char *nl = (const char *)memchr(c + i, '\n', n - i);
      if (!nl) break;
      i = (size_t)(nl - c) + 1; }
  }
  return n;
}

static void ass_tailandes_de(ASS_Track *t, int de) {
  int i;
  if (!assVistoThai) return;
  for (i = de < 0 ? 0 : de; i < t->n_events; i++) {
    char *novo;
    if (!t->events[i].Text || !(novo = assrender_marcar_tailandes(t->events[i].Text))) continue;
    free(t->events[i].Text); t->events[i].Text = novo;
  }
}

static int ass_inc_ev_por(int n) {
  if (n <= incEvCap) return 1;
  { int nc = incEvCap ? incEvCap : 1024;
    unsigned long long *nv;
    while (nc < n) nc *= 2;
    nv = (unsigned long long *)realloc(incEv, (size_t)nc * sizeof *nv);
    if (!nv) return 0;
    incEv = nv; incEvCap = nc; }
  return 1;
}

static int ass_carregar(const char *corpo, size_t tamanho, unsigned geracao, int manter, int simples);

/* "Drop past events" + seek para tras (log de 26/10): a poda liberava as
 * falas e elas so voltariam numa entrega nova do mkvass; com tudo ja colhido
 * ele nao entrega, e a legenda sumia ate desligar e religar. Agora a poda so
 * TIRA as falas da faixa do libass (o custo de varrer) e as guarda aqui; um
 * pedido antes de assPodaLimite (seek para tras) devolve todas (o worker, na
 * hora do proximo render). Sob incTrava. Faixa nova: soltas. */
static ASS_Event *assPod;
static unsigned long long *assPodH;
static int assPodN, assPodCap;
static long long assPodaLimite = LLONG_MIN;   // ate onde a poda tirou falas (ms)
static int assPodaVoltar;                     // desenho -> worker: devolver as falas podadas
#define ASS_POD_MAX 4000   // ~2-3 MB de texto no pior caso; as mais antigas saem
static int ass_pod_guardar(const ASS_Event *e, unsigned long long h) {
  if (assPodN >= ASS_POD_MAX) {
    free(assPod[0].Name); free(assPod[0].Effect); free(assPod[0].Text); free(assPod[0].render_priv);
    memmove(assPod, assPod + 1, (size_t)(assPodN - 1) * sizeof *assPod);
    memmove(assPodH, assPodH + 1, (size_t)(assPodN - 1) * sizeof *assPodH);
    assPodN--;
  }
  if (assPodN == assPodCap) {
    int nc = assPodCap ? assPodCap * 2 : 1024;
    ASS_Event *ne = (ASS_Event *)realloc(assPod, (size_t)nc * sizeof *ne);
    unsigned long long *nh;
    if (!ne) return 0;
    assPod = ne;
    nh = (unsigned long long *)realloc(assPodH, (size_t)nc * sizeof *nh);
    if (!nh) return 0;
    assPodH = nh; assPodCap = nc;
  }
  assPod[assPodN] = *e; assPodH[assPodN] = h; assPodN++;
  return 1;
}
/* Worker, sob assTrava: devolve as falas podadas a faixa (pedido do desenho).
 * incTrava vem antes de assTrava no lote: aqui so tenta; ocupada, fica para o
 * proximo render. */
static int *mapa_conta(AssMapa *m, unsigned long long k);
static int ass_inc_ev_por(int n);
static void ass_pod_devolver_assTrava(void) {
  int i, devolvidas = 0, repetidas = 0;
  if (!__atomic_load_n(&assPodaVoltar, __ATOMIC_ACQUIRE) || !assTrack) return;
  if (pthread_mutex_trylock(&incTrava)) return;
  if (incEvN == assTrack->n_events) {
    /* Quantas copias de cada fala a faixa deve ter: as do documento atual ou
     * as que a poda tirou, o que for maior. Se a colheita ja devolveu a fala
     * (seek para tras com a janela nova entregue antes deste render), a copia
     * guardada sairia DOBRADA — e a de cima e empurrada pela colisao. */
    AssMapa podC; memset(&podC, 0, sizeof podC);
    for (i = 0; i < assPodN; i++) { int *c = mapa_conta(&podC, assPodH[i]); if (c) (*c)++; }
    for (i = 0; i < assPodN; i++) {
      int id, *a = mapa_conta(&incAdd, assPodH[i]), *d = mapa_conta(&incDoc, assPodH[i]), *c = mapa_conta(&podC, assPodH[i]);
      int quer = d && c ? (*d > *c ? *d : *c) : 1;
      if (a && *a >= quer) {
        free(assPod[i].Name); free(assPod[i].Effect); free(assPod[i].Text); free(assPod[i].render_priv);
        repetidas++; devolvidas++;
        continue;
      }
      id = ass_alloc_event(assTrack);
      if (id < 0 || !ass_inc_ev_por(assTrack->n_events)) { if (id >= 0) assTrack->n_events--; break; }
      /* ReadOrder e a ordem de chegada (ver ass_inc_lote): volta para o lugar
       * de antes na ordem do libass (camada, ReadOrder), nao para o fim. */
      assTrack->events[id] = assPod[i];
      incEv[incEvN++] = assPodH[i];
      if (a) (*a)++;
      devolvidas++;
    }
    free(podC.k); free(podC.v); free(podC.g);
    if (devolvidas < assPodN) memmove(assPod, assPod + devolvidas, (size_t)(assPodN - devolvidas) * sizeof *assPod),
                              memmove(assPodH, assPodH + devolvidas, (size_t)(assPodN - devolvidas) * sizeof *assPodH);
    assPodN -= devolvidas;
    assEventos = assTrack->n_events; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
    for (i = assTrack->n_events - (devolvidas - repetidas); i < assTrack->n_events; i++)
      if (assTrack->events[i].Start < assCoberturaIni) assCoberturaIni = assTrack->events[i].Start;
  }
  if (!assPodN) __atomic_store_n(&assPodaLimite, LLONG_MIN, __ATOMIC_RELEASE);
  __atomic_store_n(&assPodaVoltar, 0, __ATOMIC_RELEASE);
  pthread_mutex_unlock(&incTrava);
}

static void ass_pod_soltar(void) {   // sob incTrava
  int i;
  for (i = 0; i < assPodN; i++) { free(assPod[i].Name); free(assPod[i].Effect); free(assPod[i].Text); free(assPod[i].render_priv); }
  assPodN = 0;
  __atomic_store_n(&assPodaLimite, LLONG_MIN, __ATOMIC_RELEASE);
}

/* Um lote do mkvass. 1 = aplicado; 0 = geracao velha (descartado).
 * Cabecalho diferente ou faixa nao pronta: a carga cheia de sempre. */
static int ass_inc_lote(const char *corpo, size_t tamanho, unsigned geracao) {
  size_t cabN, i;
  unsigned long long cab;
  size_t *novasO = NULL; int *novasN = NULL; unsigned long long *novasH = NULL;
  int nNovas = 0, capNovas = 0, reconstruir, removidos = 0, aceitas = 0, total = 0, ok = 0, desalinhou = 0;
  cabN = ass_inc_cabecalho(corpo, tamanho);
  cab = ass_hash_bytes(corpo, cabN);
  pthread_mutex_lock(&incTrava);
  if (incGeracao != geracao || incCab != cab) {
    pthread_mutex_unlock(&incTrava);
    return ass_carregar(corpo, tamanho, geracao, 1, 0);
  }
  reconstruir = !incPronto;
  mapa_limpar(&incDoc);
  if (reconstruir) mapa_limpar(&incAdd);
  /* Varredura: so hashes, sem trava do libass. */
  for (i = cabN; i < tamanho;) {
    const char *l = corpo + i, *nl = (const char *)memchr(l, '\n', tamanho - i);
    size_t n = nl ? (size_t)(nl - l) : tamanho - i;
    i += n + (nl ? 1 : 0);
    if (n > 0 && l[n - 1] == '\r') n--;
    if (n < 10 || memcmp(l, "Dialogue:", 9)) continue;
    { unsigned long long h = ass_hash_bytes(l, n);
      int *d = mapa_conta(&incDoc, h), *a = mapa_conta(&incAdd, h);
      if (!d || !a) goto falta;
      (*d)++;
      if (*d <= *a) continue;
      (*a)++;
      if (nNovas == capNovas) {
        int nc = capNovas ? capNovas * 2 : 256;
        size_t *no = (size_t *)realloc(novasO, (size_t)nc * sizeof *no);
        int *nn; unsigned long long *nh;
        if (!no) goto falta;
        novasO = no;
        nn = (int *)realloc(novasN, (size_t)nc * sizeof *nn);
        if (!nn) goto falta;
        novasN = nn;
        nh = (unsigned long long *)realloc(novasH, (size_t)nc * sizeof *nh);
        if (!nh) goto falta;
        novasH = nh; capNovas = nc;
      }
      novasO[nNovas] = (size_t)(l - corpo); novasN[nNovas] = (int)n; novasH[nNovas] = h; nNovas++; }
  }
  pthread_mutex_lock(&assTrava);
  if (geracao != __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) || !assTrack || assTrackGeracao != geracao ||
      __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE) || !assLib) {
    pthread_mutex_unlock(&assTrava);
    incPronto = 0;
    goto fim;
  }
  if (reconstruir) { ass_flush_events(assTrack); incEvN = 0; ass_pod_soltar(); }
  if (incEvN != assTrack->n_events) {
    /* Desalinhou (nao deveria): refaz na proxima entrega. */
    pthread_mutex_unlock(&assTrava);
    incPronto = 0;
    goto fim;
  }
  long long minNewStart = LLONG_MAX;
  { int antes = assTrack->n_events, k;
    for (k = 0; k < nNovas; k++) {
      int n0 = assTrack->n_events;
      ass_process_data(assTrack, (char *)(corpo + novasO[k]), novasN[k]);
      if (assTrack->n_events > n0 && ass_inc_ev_por(assTrack->n_events)) {
        /* ReadOrder = ordem de chegada. Antes ficava 0 em todas e o libass
         * empatava pela posicao no vetor: igual, mas uma fala devolvida pela
         * poda ia para o fim e mudava quem sobe na colisao. */
        while (incEvN < assTrack->n_events) {
          if (assTrack->events[incEvN].Start < minNewStart)
            minNewStart = assTrack->events[incEvN].Start;
          assTrack->events[incEvN].ReadOrder = ++assLeituraSeq;
          incEv[incEvN++] = novasH[k];
        }
        aceitas++;
      } else if (assTrack->n_events != incEvN) {
        /* Sem memoria para o hash: perde o alinhamento, refaz depois. */
        desalinhou = 1;
      }
    }
    ass_tailandes_de(assTrack, antes); }
  /* Drop past events. O playhead e o ultimo pedido do desenho. */
  { long poda = ASSPERF_PODA_MS;
    if (poda > 0 && !desalinhou && incEvN == assTrack->n_events) {
      long long playhead, limite;
      int k, w = 0;
      pthread_mutex_lock(&assFilaTrava); playhead = (long long)assPedidoMs; pthread_mutex_unlock(&assFilaTrava);
      limite = playhead - poda;
      for (k = 0; k < assTrack->n_events; k++) {
        ASS_Event *e = &assTrack->events[k];
        int *a = mapa_conta(&incAdd, incEv[k]), *d = mapa_conta(&incDoc, incEv[k]);
        if (a && d && *a > *d && e->Start + e->Duration < limite) {
          if (!ass_pod_guardar(e, incEv[k])) ass_free_event(assTrack, k);   // sem memoria: some, como antes
          (*a)--; removidos++;
          continue;
        }
        if (w != k) { assTrack->events[w] = assTrack->events[k]; incEv[w] = incEv[k]; }
        w++;
      }
      assTrack->n_events = w; incEvN = w;
      if (removidos && limite > __atomic_load_n(&assPodaLimite, __ATOMIC_ACQUIRE))
        __atomic_store_n(&assPodaLimite, limite, __ATOMIC_RELEASE);
    } }
  total = assTrack->n_events;
  assEventos = total; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
  assCoberturaIni = LLONG_MAX; assCoberturaFim = LLONG_MIN;
  { int k;
    for (k = 0; k < total; k++) {
      long long ini = assTrack->events[k].Start, fimE = ini + assTrack->events[k].Duration;
      if (ini < assCoberturaIni) assCoberturaIni = ini;
      if (fimE > assCoberturaFim) assCoberturaFim = fimE;
    } }
  incPronto = !desalinhou && incEvN == assTrack->n_events;
  pthread_mutex_unlock(&assTrava);
  if (reconstruir || aceitas) {
    pthread_mutex_lock(&assFilaTrava);
    if (reconstruir || minNewStart <= (long long)assPedidoMs + 100)
      assTemUltimoPedido = 0;   // o worker ve o documento novo mesmo pausado
    pthread_mutex_unlock(&assFilaTrava);
  }
  ok = 1;
  goto fim;
falta:
  incPronto = 0;   // sem memoria no meio: a proxima entrega reconstroi
fim:
  pthread_mutex_unlock(&incTrava);
  free(novasO); free(novasN); free(novasH);
  return ok;
}

static int ass_carregar(const char *corpo, size_t tamanho, unsigned geracao, int manter, int simples) {
  ASS_Track *track;
  char *copia;
  if (!corpo || !tamanho) return 0;
  { /* assperf: a carga cheia redefine a base da carga incremental. */
    size_t cabN = ass_inc_cabecalho(corpo, tamanho);
    pthread_mutex_lock(&incTrava);
    incPronto = 0; incEvN = 0; incGeracao = geracao; incCab = simples ? 0 : ass_hash_bytes(corpo, cabN);
    ass_pod_soltar();   // as podadas eram da faixa que sai
    pthread_mutex_unlock(&incTrava); }
  pthread_mutex_lock(&assTrava);
  if (geracao != __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE)) {
    pthread_mutex_unlock(&assTrava); return 0;
  }
  ass_iniciar_locked(!simples);
  if (!assLib || !assRenderer) { pthread_mutex_unlock(&assTrava); return 0; }
  copia = (char *)malloc(tamanho + 1u);
  if (!copia) { ass_diag("libass: memoria insuficiente para documento"); pthread_mutex_unlock(&assTrava); return 0; }
  memcpy(copia, corpo, tamanho); copia[tamanho] = 0;
  track = ass_read_memory(assLib, copia, tamanho, "UTF-8");
  free(copia);
  if (!track || track->n_events <= 0) {
    if (track) ass_free_track(track);
    ass_diag("libass: documento ASS invalido ou incompleto");
    pthread_mutex_unlock(&assTrava); return 0;
  }
  if (simples) {
    /* Converted plain text follows Unicode layout, not VSFilter's legacy
     * per-style-run layout. Keep authored ASS compatibility unchanged. */
    ass_track_set_feature(track, ASS_FEATURE_WHOLE_TEXT_LAYOUT, 1);
    ass_track_set_feature(track, ASS_FEATURE_BIDI_BRACKETS, 1);
  }
  if (geracao != __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE)) {
    ass_free_track(track);
    pthread_mutex_unlock(&assTrava); return 0;
  }
  ass_authored_styles_free();
  ass_rigid_free_ctx_locked();
  ass_rigid_clear();
  if (assTrack) ass_free_track(assTrack);
  assTrack = track;
  assTrackStyleVer++;
  { int fb = __atomic_load_n(&assFallbacks, __ATOMIC_RELAXED);
    if (assFonteNovaTarde && fb > 0) {
      struct timespec a, b;
      clock_gettime(CLOCK_MONOTONIC, &a);
      ass_aplicar_fontes_locked();
      clock_gettime(CLOCK_MONOTONIC, &b);
      fprintf(stderr, "[ass] attached fonts arrived after %d fallback font pick(s): font choice redone in %ld ms\n", fb,
              (long)((b.tv_sec - a.tv_sec) * 1000L + (b.tv_nsec - a.tv_nsec) / 1000000L));
    } }
  ass_trilha_tailandes(track);
  plainTimingValid = 0;
  /* Authored ASS retains its own margins after a converted plain track. */
  ass_set_selective_style_override_enabled(assRenderer, 0);
  __atomic_store_n(&assTextoSimples, simples, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackMatriz, simples ? -1 : (int)track->YCbCrMatrix, __ATOMIC_RELEASE);
  plainAplValido = 0;
  assEventos = track->n_events; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
  assResolucaoFonte = 0;
  assCoberturaIni = LLONG_MAX; assCoberturaFim = LLONG_MIN;
  { int i;
    for (i = 0; i < track->n_events; i++) {
      long long ini = track->events[i].Start;
      long long fim = ini + track->events[i].Duration;
      if (ini < assCoberturaIni) assCoberturaIni = ini;
      if (fim > assCoberturaFim) assCoberturaFim = fim;
    } }
  __atomic_store_n(&assTrackGeracao, geracao, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 1, __ATOMIC_RELEASE);
  ass_diag(simples ? "libass: texto RTL simples ativo" : "libass: ASS completo ativo");
  pthread_mutex_unlock(&assTrava);
  if (!ass_worker_iniciar()) {
    pthread_mutex_lock(&assTrava);
    if (assTrack == track) { ass_authored_styles_free(); ass_rigid_free_ctx_locked(); ass_rigid_clear(); ass_free_track(assTrack); assTrack = NULL; }
    assEventos = 0; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
    __atomic_store_n(&assTrackAtivo, 0, __ATOMIC_RELEASE);
    ass_diag("libass: worker de render indisponivel");
    pthread_mutex_unlock(&assTrava);
    return 0;
  }
  pthread_mutex_lock(&assFilaTrava);
  if (manter) {
    /* Mesma faixa, mais eventos (o mkvass entregou um lote). O quadro em tela
     * CONTINUA: apagar aqui era o outro "pisca", um por lote colhido. So o
     * pedido e refeito, para o worker ver o documento novo mesmo pausado. */
    assTemUltimoPedido = 0;
  } else {
    assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
    ass_fila_limpar_locked();
    ass_frame_liberar(&assAtual); assAtualValido = 0;
    assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
    assTexResetar = 1;
  }
  pthread_mutex_unlock(&assFilaTrava);
  return 1;
}

int assrender_carregar(const char *corpo, size_t tamanho, unsigned geracao) {
  pthread_mutex_lock(&assTrava);
  assGlifoMsgs = 0;
  pthread_mutex_unlock(&assTrava);
  return ass_carregar(corpo, tamanho, geracao, 0, 0);
}

#ifdef NV_TPK
#include <sys/stat.h>
#include "dados.h"
#include "rede.h"
#ifndef NV_VERSAO
#define NV_VERSAO "dev"
#endif
/* Garante a fonte arabe completa para o texto simples RTL (ver ass_carregar_pasta_locked).
 * Chamada fora de assTrava, no fio que baixou a legenda: ~175 KB, uma vez por instalacao. */
static void ass_garantir_naskh(void) {
  static int pronto, tentativas;
  const char *arte = getenv("NUVIO_TPK_ARTE"), *v;
  char caminho[700], pasta[640], url[320], tmp[720];
  char *buf; long n = 0; FILE *f;
  if (pronto || tentativas >= 1) return;
  if (arte && arte[0]) {
    snprintf(caminho, sizeof caminho, "%s/../fonts/NotoNaskhArabic-Regular.ttf", arte);
    if (access(caminho, R_OK) == 0) { pronto = 1; return; }
  }
  if (!dados_dir() || !dados_dir()[0]) return;
  if (!dados_caminho(pasta, sizeof pasta, "fontes")) goto falhou;
  snprintf(caminho, sizeof caminho, "%s/NotoNaskhArabic-Regular.ttf", pasta);
  if (access(caminho, R_OK) != 0) {
    if (NV_VERSAO[0] < '0' || NV_VERSAO[0] > '9') { pronto = 1; return; }
    for (v = NV_VERSAO; *v; v++) if (!((*v >= '0' && *v <= '9') || *v == '.')) { pronto = 1; return; }
    tentativas++;
    mkdir(pasta, 0755);
    snprintf(url, sizeof url, "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/v%s/deploy/app/fonts/NotoNaskhArabic-Regular.ttf", NV_VERSAO);
    buf = rede_baixar_bin(url, 20, &n);
    if (!buf || n < 100000 || n > 400000 || !assrender_bytes_sao_fonte((const unsigned char *)buf, 4)) {
      fprintf(stderr, "[libass] NotoNaskhArabic-Regular nao veio da tag v%s (%ld bytes)\n", NV_VERSAO, buf ? n : -1L);
      free(buf); goto falhou;
    }
    snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
    f = fopen(tmp, "wb");
    if (!f || fwrite(buf, 1, (size_t)n, f) != (size_t)n) { if (f) fclose(f); free(buf); remove(tmp); goto falhou; }
    fclose(f); free(buf);
    if (rename(tmp, caminho) != 0) { remove(tmp); goto falhou; }
    fprintf(stderr, "[libass] NotoNaskhArabic-Regular baixada da tag v%s (%ld bytes)\n", NV_VERSAO, n);
    /* Negrito real (#335), de cortesia: sem ele o libass cai no "sem negrito"
     * para trilhas com arabe, que e o comportamento antigo seguro. */
    snprintf(url, sizeof url, "https://raw.githubusercontent.com/iqui27/nuvio-native-legacy/v%s/deploy/app/fonts/NotoNaskhArabic-Bold.ttf", NV_VERSAO);
    snprintf(caminho, sizeof caminho, "%s/NotoNaskhArabic-Bold.ttf", pasta);
    buf = rede_baixar_bin(url, 20, &n);
    if (buf && n > 100000 && n < 400000 && assrender_bytes_sao_fonte((const unsigned char *)buf, 4)) {
      snprintf(tmp, sizeof tmp, "%s.tmp", caminho);
      f = fopen(tmp, "wb");
      if (f && fwrite(buf, 1, (size_t)n, f) == (size_t)n) { fclose(f); if (rename(tmp, caminho)) remove(tmp); }
      else { if (f) fclose(f); remove(tmp); }
    }
    free(buf);
  }
  pronto = 1;
  pthread_mutex_lock(&assTrava);
  snprintf(assPastaExtra, sizeof assPastaExtra, "%s", pasta);
  if (assLib) {
    /* A biblioteca ja subiu sem ela: so esta pasta entra (nao duplica as outras
     * fontes) e o seletor e refeito com a lista completa. */
    assrender_ler_pasta_fontes(assPastaExtra, ass_fonte_da_pasta, NULL, NULL);
    ass_aplicar_fontes_locked();
  }
  pthread_mutex_unlock(&assTrava);
  return;
falhou:
  pthread_mutex_lock(&assTrava);
  assNaskhFalhou = 1;
  if (assLib) { ass_subset_naskh_locked(); ass_aplicar_fontes_locked(); }
  pthread_mutex_unlock(&assTrava);
}
#endif

int assrender_fonte_local_transacao(const char *caminho, int (*confirmar)(void *), void *u) {
  if (!caminho) caminho = "";
  if (strlen(caminho) >= sizeof assFonteLocal) return 0;
  pthread_mutex_lock(&assTrava);
  int mudou = strcmp(assFonteLocal, caminho) != 0 || (assLib && caminho[0] && !assFonteRegistrada);
  if (mudou && assLib && !assAtuaisCompletas) { pthread_mutex_unlock(&assTrava); return 0; }
  size_t n = 0; unsigned char *b = NULL;
  if (mudou && caminho[0]) {
    b = ass_fonte_local_ler(caminho, &n);
    if (!b) { pthread_mutex_unlock(&assTrava); return 0; }
  }
  if (confirmar && !confirmar(u)) { free(b); pthread_mutex_unlock(&assTrava); return 0; }
  if (mudou) {
    snprintf(assFonteLocal, sizeof assFonteLocal, "%s", caminho);
    if (assLib) ass_fontes_reconstruir_locked(b, n);
    else assFonteRegistrada = 0;
    ass_fonte_espelho_locked();
  }
  free(b);
  pthread_mutex_unlock(&assTrava);
  if (mudou) {
    pthread_mutex_lock(&assFilaTrava);
    assPedidoPendente = 0; ++assEpoch; assTemUltimoPedido = 0;
    ass_fila_limpar_locked();
    ass_frame_liberar(&assAtual); assAtualValido = 0; assTexResetar = 1;
    pthread_mutex_unlock(&assFilaTrava);
  }
  return 1;
}
/* Every UI frame (player.c): the copy, never assTrava (see assFonteEspelho). */
int assrender_fonte_local_ativa(const char *caminho) {
  int ativa;
  if (!caminho || !caminho[0]) return 0;
  ass_gl_trava(&assFonteEspelhoTrava);
  ativa = assFonteEspelhoOk && !strcmp(assFonteEspelho, caminho);
  pthread_mutex_unlock(&assFonteEspelhoTrava);
  return ativa;
}

int assrender_carregar_texto(const char *corpo, size_t tamanho, unsigned geracao) {
#ifdef NV_TPK
  ass_garantir_naskh();
#endif
  return ass_carregar(corpo, tamanho, geracao, 0, 1);
}

int assrender_texto_simples(void) {
  return assrender_ativo() && __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
}

void assrender_definir_texto_estilo(const PlainAssStyle *style) {
  if (!style) return;
  ass_gl_trava(&assFilaTrava);
  if (!plainPedValido || memcmp(style, &plainPed, sizeof *style)) {
    plainPed = *style; plainPedValido = 1;
    if (!ASS_NOVO || __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE)) {
      ++assEpoch; assTemUltimoPedido = 0;
    }
  }
  pthread_mutex_unlock(&assFilaTrava);
}

int assrender_atualizar(const char *corpo, size_t tamanho, unsigned geracao) {
  int mesma, simples;
  pthread_mutex_lock(&assTrava);
  mesma = assTrack && assTrackGeracao == geracao;
  simples = __atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE);
  pthread_mutex_unlock(&assTrava);
  /* assperf: so as linhas novas do lote (ver ass_inc_lote). */
  if (ASS_NOVO && mesma && !simples && corpo && tamanho)
    return ass_inc_lote(corpo, tamanho, geracao);
  return ass_carregar(corpo, tamanho, geracao, mesma, 0);
}

void assrender_limpar(void) {
  ass_inc_esquecer();
  assperf_escala_reset();   // Render resolution Auto: a new track starts at 100 %
  ass_ultimo_imgs_guardar(0);   // a new track starts on the normal split
  pthread_mutex_lock(&assTrava);
  ass_authored_styles_free();
  ass_rigid_free_ctx_locked();
  ass_rigid_clear();
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  __atomic_store_n(&assTrackMatriz, -1, __ATOMIC_RELEASE);
  assEventos = 0; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE);
  assCoberturaIni = assCoberturaFim = 0;
  __atomic_store_n(&assTrackGeracao, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 0, __ATOMIC_RELEASE);
  ass_diag("libass: sem faixa");
  pthread_mutex_unlock(&assTrava);
  pthread_mutex_lock(&assFilaTrava);
  assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
  ass_fila_limpar_locked();
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

void assrender_limpar_fontes(void) {
  ass_inc_esquecer();
  pthread_mutex_lock(&assTrava);
  ass_atuais_limpar_locked();
  ass_authored_styles_free();
  ass_rigid_free_ctx_locked();
  ass_rigid_clear();
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  __atomic_store_n(&assTrackMatriz, -1, __ATOMIC_RELEASE);
  assEventos = 0; __atomic_add_fetch(&assTrackMut, 1, __ATOMIC_RELEASE); assFontes = 0; assResolucaoFonte = 0;
  assCoberturaIni = assCoberturaFim = 0;
  __atomic_store_n(&assTrackGeracao, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 0, __ATOMIC_RELEASE);
  if (assLib && assFonteLocal[0]) {
    size_t n; unsigned char *b = ass_fonte_local_ler(assFonteLocal, &n);
    ass_fontes_reconstruir_locked(b, n); free(b);
  }
  /* SEM ass_clear_fonts: ver a nota de assFontesVistas. As fontes da faixa
   * anterior ficam na biblioteca; so sao usadas se a proxima pedir o mesmo
   * nome. assFontes (diagnostico) volta a contar as desta faixa. */
  pthread_mutex_unlock(&assTrava);
  pthread_mutex_lock(&assFilaTrava);
  assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
  ass_fila_limpar_locked();
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

// #415: no aviso de memoria, esvazia os caches do libass — o loop
// "Glyph not found -> selecting one more font" encheu ~190 MB na G4 sem
// que gpu-cache ou texturas mostrassem. ass_set_cache_limits NAO serve:
// 0 vira o default e o corte so acontece preguiçosamente no proximo
// ass_render_frame. O que despeja na hora e o ass_reconfigure dentro de
// ass_set_fonts (bitmap/composite/outline/font/metrics + fontselect
// refeito, o que tambem encerra o loop de fallback). Custa uma releitura
// da pasta de fontes; aceitavel porque so roda no aviso.
void assrender_aliviar(void) {
  pthread_mutex_lock(&assTrava);
  if (assRenderer) ass_aplicar_fontes_locked();
  pthread_mutex_unlock(&assTrava);
}

int assrender_adicionar_fonte(const char *nome, const void *dados, size_t tamanho) {
  if (!nome || !*nome || !dados || !tamanho || tamanho > (size_t)INT_MAX) return 0;
  pthread_mutex_lock(&assTrava);
  ass_iniciar_locked(1);
  if (assLib) {
    ass_atual_guardar_locked(nome, dados, tamanho);
    unsigned long long am = ass_amostra_fonte((const unsigned char *)dados, tamanho);
    int i, visto = 0;
    for (i = 0; i < assNFontesVistas; i++)
      if (assFontesVistas[i].tam == tamanho && assFontesVistas[i].amostra == am &&
          !strncmp(assFontesVistas[i].nome, nome, sizeof assFontesVistas[i].nome - 1)) { visto = 1; break; }
    if (!visto) {
      /* TETO: episodios em sequencia com fontes DIFERENTES fariam a lista
       * crescer sem fim. Passou dele, reinicia tudo — e ai sim ass_set_fonts
       * logo apos o ass_clear_fonts, que e o par que mantem a conta do
       * ass_start_frame coerente. Nao ha faixa sendo desenhada com fonte
       * velha: adicionar fonte so acontece antes de carregar o documento. */
      if (assNFontesVistas >= ASS_FONTES_MAX ||
          assBytesFontesVistas + tamanho > ASS_FONTES_TETO_BYTES) {
        ass_clear_fonts(assLib);
        // ass_clear_fonts removes every memory face, including the imported
        // device-local selection. Restore it before attachments and system
        // fonts so the selection keeps the same precedence after rollover.
        if (assFonteLocal[0]) {
          size_t localN = 0;
          unsigned char *local = ass_fonte_local_ler(assFonteLocal, &localN);
          if (local) ass_add_font(assLib, assFonteLocal, (const char *)local, (int)localN);
          assFonteRegistrada = local != NULL;
          free(local);
        } else assFonteRegistrada = 0;
        ass_fonte_espelho_locked();
        ass_carregar_pasta_locked();
        ass_aplicar_fontes_locked();
        assNFontesVistas = 0; assBytesFontesVistas = 0;
      }
      ass_add_font(assLib, nome, (const char *)dados, (int)tamanho);
      assFonteNovaTarde = 1;
      { AssFonteVista *v = &assFontesVistas[assNFontesVistas++];
        snprintf(v->nome, sizeof v->nome, "%s", nome);
        v->tam = tamanho; v->amostra = am; }
      assBytesFontesVistas += tamanho;
    }
    assFontes++;
  }
  { int ok = assLib != NULL; pthread_mutex_unlock(&assTrava); return ok; }
}

/* #335: o layout do app e sempre 1920x1080. Com a interface em 4K o alvo tem
 * 3840x2160, e um quadro do libass feito em 1920x1080 era esticado 2x com filtro
 * linear: a legenda ASS saia mole. Agora o libass renderiza no tamanho REAL do
 * alvo (k = pixels por unidade, 1 ou 2) e o quadro volta a ser desenhado 1:1.
 * Quem desenha informa (player.c); sem informar, k = 1. */
static int layPxUnid = 1;
void assrender_pixels_por_unidade(float f) { layPxUnid = f >= 1.5f ? 2 : 1; }

void assrender_definir_layout(float x, float y, float w, float h,
                              int videoW, int videoH, double escalaFonte) {
  int lw = (int)lroundf(w), lh = (int)lroundf(h), k;
  if (lw < 16 || lh < 16) return;
  if (videoW < 2 || videoH < 2) { videoW = lw; videoH = lh; }
  if (!(escalaFonte > 0.1 && escalaFonte < 4.0)) escalaFonte = 1.0;
  k = layPxUnid;
  ass_gl_trava(&assFilaTrava);
  if (lw != layPedW || lh != layPedH || videoW != layPedVW || videoH != layPedVH ||
      k != layPedK || escalaFonte != layPedEscala || fabsf(x - layPedX) > 0.5f || fabsf(y - layPedY) > 0.5f) {
    layPedX = x; layPedY = y; layPedW = lw; layPedH = lh;
    layPedVW = videoW; layPedVH = videoH; layPedEscala = escalaFonte; layPedK = k;
    /* O quadro em tela fica ate o novo chegar (sem piscar), mas nada
     * renderizado com o layout velho pode ser publicado depois disto, e o
     * proximo desenhar pede um render mesmo com o video pausado. */
    ++assEpoch;
    assTemUltimoPedido = 0;
  }
  pthread_mutex_unlock(&assFilaTrava);
}

void assrender_definir_cor(int enabled, int r, int g, int b) {
  ass_gl_trava(&assFilaTrava);
  assCorAtiva = !!enabled;
  assCorR = r < 0 ? 0 : r > 255 ? 255 : r;
  assCorG = g < 0 ? 0 : g > 255 ? 255 : g;
  assCorB = b < 0 ? 0 : b > 255 ? 255 : b;
  pthread_mutex_unlock(&assFilaTrava);
}

void assrender_aplicar_invalidacao(void) {
  ass_gl_trava(&assFilaTrava);
  if (assTexResetar) {
    assTexResetar = 0;
    ass_apagar_texturas_locked();
#if ASS_NOVO
    assMainResetar = 1;   // o caminho novo tambem solta o quadro e as texturas dele
#endif
  }
  pthread_mutex_unlock(&assFilaTrava);
}

static double ass_mono(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Medidas por quadro desenhado, sob assFilaTrava. `agora` e o relogio do
 * video em ms (o mesmo que foi pedido ao libass). */
#if defined(NV_TPK) && ASS_NOVO && defined(__linux__)
/* CPU EVERY 10 s (Tizen 4-6 playback damage, 16/11/2026): the whole TV (from
 * /proc/stat) and this app's threads (/proc/self/task/<tid>/stat), as % of one
 * core over the window. The TV's own player runs in other processes: what the
 * TV is busy with beyond this app shows as "TV" minus "app". */
#define CPU_FIOS 160
static struct { int tid; unsigned long long t; } cpuAnt[CPU_FIOS];
static int cpuAntN;
static unsigned long long cpuTvOcupado, cpuTvTotal;
static double cpuEm;

static int cpu_ler(const char *arq, char *b, size_t n) {
  FILE *f = fopen(arq, "r");
  size_t k;
  if (!f) return 0;
  k = fread(b, 1, n - 1, f);
  fclose(f);
  b[k] = 0;
  return (int)k;
}

static void cpu_10s(double agora) {
  typedef struct { int tid; unsigned long long t; char nome[20]; double pct; } Fio;
  static Fio f[CPU_FIOS];
  int n = 0, i, j;
  char b[512], linha[700];
  size_t o = 0;
  double dt = cpuEm > 0 ? agora - cpuEm : 0.0, tck = (double)sysconf(_SC_CLK_TCK), app = 0, tv = -1;
  unsigned long long u = 0, ni = 0, sy = 0, id = 0, io = 0, irq = 0, sirq = 0, st = 0;
  DIR *d;
  struct dirent *e;
  if (tck <= 0) tck = 100;
  if (cpu_ler("/proc/stat", b, sizeof b) &&
      sscanf(b, "cpu %llu %llu %llu %llu %llu %llu %llu %llu", &u, &ni, &sy, &id, &io, &irq, &sirq, &st) >= 4) {
    unsigned long long ocup = u + ni + sy + irq + sirq + st, tot = ocup + id + io;
    if (cpuTvTotal && tot > cpuTvTotal)
      tv = 100.0 * (double)(ocup - cpuTvOcupado) / (double)(tot - cpuTvTotal) * (double)sysconf(_SC_NPROCESSORS_ONLN);
    cpuTvOcupado = ocup; cpuTvTotal = tot;
  }
  d = opendir("/proc/self/task");
  while (d && (e = readdir(d)) != NULL && n < CPU_FIOS) {
    char arq[64], *p, *q;
    unsigned long long ut = 0, stt = 0;
    int tid = atoi(e->d_name);
    if (tid <= 0) continue;
    snprintf(arq, sizeof arq, "/proc/self/task/%d/stat", tid);
    if (!cpu_ler(arq, b, sizeof b) || !(p = strchr(b, '(')) || !(q = strrchr(b, ')'))) continue;
    f[n].tid = tid;
    snprintf(f[n].nome, sizeof f[n].nome, "%.*s", (int)(q - p - 1 < 15 ? q - p - 1 : 15), p + 1);
    /* after ")": state(3) ppid ... utime is field 14, stime 15 */
    if (sscanf(q + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %llu %llu", &ut, &stt) != 2) continue;
    f[n].t = ut + stt;
    f[n].pct = -1;
    for (j = 0; j < cpuAntN; j++)
      if (cpuAnt[j].tid == tid) {
        if (dt > 0 && f[n].t >= cpuAnt[j].t) f[n].pct = 100.0 * (double)(f[n].t - cpuAnt[j].t) / tck / dt;
        break;
      }
    if (f[n].pct < 0) f[n].pct = 0;   /* new thread: counted from the next window */
    app += f[n].pct;
    n++;
  }
  if (d) closedir(d);
  for (i = 0; i < n; i++) { cpuAnt[i].tid = f[i].tid; cpuAnt[i].t = f[i].t; }
  cpuAntN = n;
  cpuEm = agora;
  if (dt <= 0) return;
  /* busiest first, same names summed ("nv-assblur" x3) */
  for (i = 0; i < n; i++)
    for (j = i + 1; j < n; j++)
      if (!strcmp(f[i].nome, f[j].nome)) { f[i].pct += f[j].pct; f[j].pct = -1; }
  for (i = 0; i < 8; i++) {
    int m = -1;
    for (j = 0; j < n; j++) if (f[j].pct >= 0.5 && (m < 0 || f[j].pct > f[m].pct)) m = j;
    if (m < 0) break;
    o += (size_t)snprintf(linha + o, sizeof linha - o, "%s%s %.0f%%", i ? ", " : "", f[m].nome, f[m].pct);
    f[m].pct = -1;
    if (o >= sizeof linha - 40) break;
  }
  printf("[ass] 10s cpu: TV busy=%.0f%% of %ld cores | app=%.0f%% (%d threads) | %s\n",
         tv, sysconf(_SC_NPROCESSORS_ONLN) * 100L, app, n, o ? linha : "-");
}
#endif

static void ass_medir_locked(long long agora, int temTexto, long long inicioMax) {
  double m = ass_mono();
  assMed.quadros++;
  if (temTexto) {
    if (!assMed.textoAnterior && assMed.ultimoTextoMs &&
        agora - assMed.ultimoTextoMs < 300 && agora >= assMed.ultimoTextoMs) {
      /* Texto -> vazio -> texto em menos de 300 ms: e o "pisca". Uma fala
       * que termina e outra que comeca nao passam por aqui, a menos que o
       * proprio arquivo tenha esse buraco (raro: fansub cola as falas). */
      assMed.piscas++; assMed.vaziosPisca += assMed.vazios;
    }
    if (inicioMax >= 0 && inicioMax > assMed.ultimoInicioLogado) {
      long long atraso = agora - inicioMax;
      assMed.ultimoInicioLogado = inicioMax;
      assMed.falas++; assMed.atrasoSoma += atraso;
      if (atraso > assMed.atrasoMax) assMed.atrasoMax = atraso;
      /* Uma linha por fala que entra: Start do evento, quando apareceu no
       * relogio do video, e a diferenca. E a medida do "atrasada". Not on the
       * .tpk: frame-by-frame signs start hundreds of lines a second. */
#ifndef NV_TPK
      printf("[ass] fala %lld.%03lld apareceu em %lld.%03lld (%+lld ms)\n",
             inicioMax / 1000, inicioMax % 1000, agora / 1000, agora % 1000, atraso);
#endif
    }
    assMed.ultimoTextoMs = agora; assMed.vazios = 0;
  } else assMed.vazios++;
  assMed.textoAnterior = temTexto;
  if (m - assMed.ultimoRelatorio >= 10.0) {
    if (assMed.ultimoRelatorio > 0) {
      long rHits = 0, rMiss = 0;
      pthread_mutex_lock(&assRigidTrava);
      rHits = assRigidHits; rMiss = assRigidMisses;
      assRigidHits = assRigidMisses = 0;
      pthread_mutex_unlock(&assRigidTrava);
      printf("[ass] 10s: quadros=%ld trocas=%ld renders=%ld iguais=%ld render medio=%.1fms max=%.1fms "
             "piscas=%ld (vazios=%ld) falas=%ld atraso medio=%lldms max=%lldms eventos=%d rigid=%ld/%ld\n",
             assMed.quadros, assMed.trocas, assMed.renders, assMed.iguais,
             assMed.renders ? assMed.renderSomaUs / 1000.0 / assMed.renders : 0.0,
             assMed.renderMaxUs / 1000.0, assMed.piscas, assMed.vaziosPisca, assMed.falas,
             assMed.falas ? assMed.atrasoSoma / assMed.falas : 0, assMed.atrasoMax, assEventos, rHits, rMiss);
#if ASS_NOVO
      /* Where a subtitle frame's worker time goes (22/11/2026): avg/max ms per
       * stage, the event list size vs the events a frame touched (time index),
       * the flatten (busy frames flattened; split ones stay per image) and
       * the signature cache. */
      if (cuQuadros > 0) {
        char b[512]; int o = 0, ci;
        for (ci = 0; ci < CU_N; ci++)
          o += snprintf(b + o, sizeof b - (size_t)o, "%s %s=%.1f/%.1f", ci ? "" : "", cuNome[ci],
                        cuSoma[ci] / (double)cuQuadros, cuMax[ci]);
        printf("[ass] cost (ms avg/max per frame, %ld frames):%s | events=%ld active avg=%.0f index rebuilds=%ld | "
               "flat=%ld of %ld busy frames | overload=%ld of %ld frames (images avg/max %ld/%ld) | "
               "images left out=%ld | sig cache hits=%ld parses=%ld\n",
               cuQuadros, b, cuEventos, (double)cuAtivos / (double)cuQuadros, idxReconstrucoes,
               assFlatFeitos, assFlatQuadros, assSobreQuadros, cuQuadros,
               assSobreQuadros ? assSobreImgsSoma / assSobreQuadros : 0L, assSobreImgsMax,
               __atomic_exchange_n(&assTcFora, 0L, __ATOMIC_RELAXED), assSigAcertos, assSigCalculos);
      }
      memset(cuSoma, 0, sizeof cuSoma); memset(cuMax, 0, sizeof cuMax);
      cuQuadros = 0; cuAtivos = 0; idxReconstrucoes = 0;
      assFlatFeitos = assFlatQuadros = 0; assSigAcertos = assSigCalculos = 0;
      assSobreQuadros = assSobreImgsSoma = assSobreImgsMax = 0;
#endif
    }
#if defined(NV_TPK) && ASS_NOVO
    /* The TV's side of the same window (libass on the Tizen 4/5 .tpk, 09/11):
     * how often the position arrives and what a query costs (the 40 ms clock
     * thread gives ~25/s; without it, 4/s), free memory, and what the engine
     * holds. The counters are taken every window so the next one starts clean. */
    { int n, fora, maxMs;
      long somaMs;
      double dt = assMed.ultimoRelatorio > 0 ? m - assMed.ultimoRelatorio : 0.0;
      video_tpk_leituras(&n, &fora, &somaMs, &maxMs);
      if (dt > 0.0)
        printf("[ass] 10s tv: position readings=%d (%.1f/s, %d dropped) query avg=%.1fms max=%dms | "
               "free RAM=%ldMB | queue=%d frames %.1fMB | textures=%d %.1fMB | scale=%d%% busy=%d "
               "(now %s) | ahead up to %dms (Tizen 4-6 %s)\n",
               n, n / dt, fora, n ? (double)somaMs / n : 0.0, maxMs, memlog_livre_mb(),
               assFilaN, ass_fila_bytes_locked() / 1048576.0, assTcN, assTcBytes / 1048576.0,
               (int)(assperf_escala() * 100.0 + 0.5), assperf_pesado(),
               !assperf_pesado() ? "quiet" : ass_so_o_mais_novo() ? "newest only" : "every frame",
               __atomic_exchange_n(&assAdianteMax, 0, __ATOMIC_RELAXED), assperf_tv_lenta() ? "yes" : "no");
      /* Render resolution (owner 19/11/2026): the mode, the scales in use
       * now, and what each pass cost on the split (busy) frames. */
      { char modo[200];
        double bl = assperf_escala_blur();
        assperf_modo_texto(modo, (int)sizeof modo);
        printf("[ass] 10s res: mode=%s | now text %d%% blur %s%d%% | "
               "split frames=%ld sharp avg=%.1fms max=%.1fms (%ld imgs) soft avg=%.1fms max=%.1fms (%ld imgs) | "
               "split skipped (overload) %ld of %ld frames\n",
               modo, (int)(assperf_escala() * 100.0 + 0.5),
               bl > 0.0 ? "" : "=", (int)((bl > 0.0 ? bl : assperf_escala()) * 100.0 + 0.5),
               assDiv.quadros, assDiv.quadros ? assDiv.msA / assDiv.quadros : 0.0, assDiv.maxA, assDiv.imgsA,
               assDiv.quadros ? assDiv.msB / assDiv.quadros : 0.0, assDiv.maxB, assDiv.imgsB,
               assDiv.umaPassada, assDiv.quadros + assDiv.umaPassada);
        memset(&assDiv, 0, sizeof assDiv); }
      /* Signs & Songs matching (asssinais.h): how the lines were decided, and
       * the dialogue settings they protect the signs from. */
      { AssSinaisEst se;
        asssinais_estatisticas(&se);
        if (se.linhas || se.consultas || se.guardadas)
          printf("[ass] 10s signs: partner lines read=%d held=%d | lookups=%ld: sign by partner=%ld, "
                 "dialogue by partner=%ld, waiting (libass rule)=%ld | known stretches=%d, newest %.1f-%.1f s | "
                 "dialogue size=%d%% opacity=%d%%\n",
                 se.linhas, se.guardadas, se.consultas, se.sinais, se.dialogos, se.esperando,
                 se.trechos, se.conhecidoIni / 1000.0, se.conhecidoFim / 1000.0,
                 assperf_get(AP_ASSTAM), assperf_get(AP_ASSOPAC)); }
      /* Drawing the subtitles on the GL thread, and the local server feeding
       * the TV (Tizen 4-6 playback damage, 16/11): every window, also paused. */
      { long long us = __atomic_exchange_n(&glDesUs, 0, __ATOMIC_RELAXED),
                  mx = __atomic_exchange_n(&glDesMaxUs, 0, __ATOMIC_RELAXED);
        long gn = __atomic_exchange_n(&glDesN, 0, __ATOMIC_RELAXED), gnov = __atomic_exchange_n(&glDesNovos, 0, __ATOMIC_RELAXED);
        MkvfixMedidas md;
        mkvfix_medidas(&md);
        if (dt > 0.0) {
          long esp = __atomic_exchange_n(&glTravaEsperas, 0, __ATOMIC_RELAXED);
          long long espMx = __atomic_exchange_n(&glTravaMaxUs, 0, __ATOMIC_RELAXED);
          printf("[ass] 10s draw: GL frames with subtitles=%ld (new %ld) avg=%.2fms max=%.1fms | lock waits %ld (max %.1f ms)\n",
                 gn, gnov, gn ? us / 1000.0 / gn : 0.0, mx / 1000.0, esp, espMx / 1000.0);
          printf("[mkvfix] 10s: mode=%s | to the TV %.2f MB/s (%.1f Mbit/s), blocked sending %lldms, waiting on the CDN %lldms, "
                 "connections=%d | Void: %ld block(s) %.1fKB, resync %ld, lost %ld, undecided %ld\n",
                 mkvfix_modo_ass_atual() == MKVFIX_ASS_ANULAR ? "Void" : "Hidden",
                 md.bytes / 1048576.0 / dt, md.bytes * 8.0 / 1e6 / dt, md.envioMs, md.cdnMs, md.conexoes,
                 md.anulados, md.bytesAnulados / 1024.0, md.sincronias, md.perdas, md.indecisos);
        } }
#if defined(__linux__)
      cpu_10s(m);
#endif
    }
#endif
    fflush(stdout);
    assMed.quadros = assMed.trocas = assMed.renders = assMed.iguais = 0;
    assMed.piscas = assMed.vaziosPisca = assMed.falas = 0;
    assMed.renderSomaUs = assMed.renderMaxUs = 0;
    assMed.atrasoSoma = 0; assMed.atrasoMax = 0;
    assMed.ultimoRelatorio = m;
  }
}



/* Grade dos quadros (assrender_definir_fps). So o fio GL. */
static double assFps;   /* GL escreve, o worker le: __atomic */
void assrender_definir_fps(double fps) {
  double v = fps > 1.0 && fps < 200.0 ? fps : 0.0;
  __atomic_store(&assFps, &v, __ATOMIC_RELAXED);
}
static double ass_fps(void) { double v; __atomic_load(&assFps, &v, __ATOMIC_RELAXED); return v; }
/* Busy scene: only the newest frame is rendered (owner 23/11/2026: Every
 * frame Off, the row is gone). */
static int ass_so_o_mais_novo(void) { return assperf_pesado(); }
static long long ass_na_grade(long long ms) {
  /* O meio do quadro que contem `ms`. Folga de 0,02 quadro (<1 ms: o PTS
   * arredondado ao ms). Era 0,1 (4 ms): o letreiro trocava um tico ANTES do
   * video (dono 26/10, polegar fantasma acima dos dedos aos 19:00). */
  double fps = ass_fps();
  if (fps <= 0 || ms <= 0) return ms;
  return llround((floor((double)ms * fps / 1000.0 + 0.02) + 0.5) * 1000.0 / fps);
}
/* O meio do quadro seguinte ao do meio `t`. */
static long long ass_grade_seguinte(long long t) {
  double fps = ass_fps();
  if (fps <= 0) return t + 1;
  return ass_na_grade(t + (long long)llround(1000.0 / fps));
}
/* O primeiro meio de quadro >= ms (onde uma mudanca em `ms` ja aparece). */
static long long ass_grade_desde(long long ms) {
  long long g = ass_na_grade(ms);
  return g < ms ? ass_grade_seguinte(g) : g;
}

/* Render ahead: estado do fio GL. */
static long long assUltimoAgora = LLONG_MIN;
static int assParadoN;                // chamadas seguidas com o mesmo `agora`

int assrender_desenhar(double posSeg, int atrasoMs, float alpha,
                       float x, float y, float w, float h) {
  unsigned generation;
  int i, n = 0, reset, corChave, reenviar, segurar, saltou = 0, quadroNovo = 0;
  int corAtiva, cr, cg, cb, adiante = 0;
  long long agora, pedido;
  AssCpuFrame novo;
  (void)w; (void)h;
  if (alpha <= 0.001f) return 0;
  memset(&novo, 0, sizeof novo);
  generation = __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE);
  agora = ass_na_grade((long long)llround(posSeg * 1000.0) + (long long)atrasoMs);
  /* Parado (pausa, buffering, relogio sem amostra nova): o mesmo instante
   * 3 vezes seguidas. Ai nao se adianta nada: a pausa mostra o quadro exato. */
  if (agora == assUltimoAgora) { if (assParadoN < 1000) assParadoN++; } else assParadoN = 0;
  assUltimoAgora = agora;
  if (ASS_NOVO && assParadoN < 3) {
    /* Cena pesada por segundo (assperf.c): tocando, cada quadro de UI e um
     * tique de trabalho 0 — uma cena parada que toca conta como leve; pausado, nao. */
    assperf_carga_gl(0.0);
    adiante = ASSPERF_ADIANTE_MS;
    /* BUSY SCENE (owner 17/11/2026 on Tizen 4-6; 19/11/2026 every .tpk): a
     * heavy frame on Tizen 6 takes 118-192 ms (Kaguya inbox), so a frame asked
     * 100 ms ahead came out late and the signs trailed by up to 0.5 s. Ask far
     * enough ahead for the recent render peak to finish in time (+ one frame),
     * 100..600 ms: the busy worker renders only the newest request, so signs
     * update less often but each one is on time. Hold-until-due keeps it off
     * the screen early. On a fast TV (peak ~40 ms) this stays at 100 ms. */
    if (ass_so_o_mais_novo()) {
      double pico, fps = ass_fps();
      __atomic_load(&assRenderPico, &pico, __ATOMIC_RELAXED);
      adiante = (int)lround(pico * 1.25 + (fps > 0 ? 1000.0 / fps : 42.0));
      if (adiante < ASSPERF_ADIANTE_MS) adiante = ASSPERF_ADIANTE_MS;
      if (adiante > 600) adiante = 600;
    }
    { int mx = __atomic_load_n(&assAdianteMax, __ATOMIC_RELAXED);
      if (adiante > mx) __atomic_store_n(&assAdianteMax, adiante, __ATOMIC_RELAXED); }
  }
  /* Hold until due: com render ahead o quadro feito para daqui a 100 ms
   * espera a sua vez na fila. */
  segurar = adiante > 0;
  /* Quadros INTEIROS a frente: 100 ms = 2,4 quadros caia na borda e o pedido
   * pulava entre +2 e +3. */
  { double fps = ass_fps();
    if (fps > 0 && adiante > 0 && !ass_so_o_mais_novo()) {
      long long q = llround((double)adiante * fps / 1000.0);
      if (q < 1) q = 1;
      /* Cena leve com quadros caros (mascaras do polegar: 50-65 ms no log 15):
       * 3 quadros a frente, senao dois seguidos chegam um quadro tarde. */
      { double pico; __atomic_load(&assRenderPico, &pico, __ATOMIC_RELAXED);
        if (pico > 40.0 && q < 3) q = 3; }
      pedido = ass_na_grade(agora + llround((double)q * 1000.0 / fps));
    } else pedido = ass_na_grade(agora + adiante); }
  ass_gl_trava(&assFilaTrava);
  if (!assWorkerCriado) { pthread_mutex_unlock(&assFilaTrava); return 0; }
  { /* An option changed (Subtitles > Style): the frame on
     * screen may be valid for seconds (render only on changes); ask again. */
    static unsigned versaoVista;
    unsigned v = assperf_versao();
    if (v != versaoVista) { versaoVista = v; ++assEpoch; assTemUltimoPedido = 0; } }
  assFilaTeto = segurar ? ASS_FILA_MAX : 1;
  assAgoraUiMs = agora; assAgoraUiGeracao = generation;
  /* SALTO de verdade (seek): o quadro velho mostra outra cena e sai. ERA 200
   * ms — e o currentTime da C9 anda 197..247 ms por evento, 77 % acima de 200
   * (log de 23/09): a legenda era apagada a cada evento e piscava 2-4 vezes
   * por segundo. Com o relogio interpolado o passo e de um quadro; 1,5 s so
   * pega salto. Fora disso o quadro atual fica ate o proximo estar pronto. */
  /* Com "Render only on changes" o ultimo PEDIDO pode ser de segundos atras
   * (a fala parada nao pede nada): o salto e medido contra o ultimo instante
   * VISTO, senao uma fala longa virava "seek" e piscava. */
  if (assTemUltimoPedido && assUltimoPedidoGeracao == generation &&
      fabs((double)pedido - (ASS_NOVO && assUltimoVistoGeracao == generation
                             ? assUltimoVistoMs : assUltimoPedidoMs)) > 1500.0) {
    ass_fila_limpar_locked();
    ass_frame_liberar(&assAtual); assAtualValido = 0;
    ++assEpoch;
    ++assSerial;
    saltou = 1;
  }
  /* Seek para tras alem da poda: as falas de la voltam pelo mkvass. */
  { long long lim = __atomic_load_n(&assPodaLimite, __ATOMIC_ACQUIRE);
    if (lim != LLONG_MIN && agora < lim + 500 && !__atomic_load_n(&assPodaVoltar, __ATOMIC_ACQUIRE)) {
      __atomic_store_n(&assPodaVoltar, 1, __ATOMIC_RELEASE);
      assTemUltimoPedido = 0;   // o worker renderiza de novo (ja com as falas de volta)
    } }
  /* Com "Hold until due" a fila cheia (ou grande demais) para os pedidos ate
   * um quadro sair: sem isso o worker renderizaria sempre mais adiante e o
   * quadro "devido" nunca chegaria. */
  /* assperf "Render only on changes": o ultimo render ainda vale para este
   * instante (nada comeca, acaba ou anima ate la) — o worker nao e chamado. */
  { int vale = ASS_NOVO && assTemUltimoPedido && assUltimoPedidoGeracao == generation &&
               assValidoEpoch == assEpoch && assValidoGeracao == generation &&
               pedido >= assValidoDe && pedido < assValidoAte;
  if (!vale && (!assTemUltimoPedido || assUltimoPedidoGeracao != generation ||
       fabs((double)pedido - assUltimoPedidoMs) >= 5.0) &&
      (!segurar || (assFilaN < ASS_FILA_MAX && ass_fila_bytes_locked() < ASS_FILA_BYTES))) {
    if (!assTemUltimoPedido || assUltimoPedidoGeracao != generation) assPedidoForcado = 1;
    assUltimoPedidoMs = (double)pedido;
    assUltimoPedidoGeracao = generation;
    assTemUltimoPedido = 1;
    assPedidoMs = (double)pedido;
    assPedidoGeracao = generation;
    assPedidoSerial = ++assSerial;
    assPedidoPendente = 1;
    pthread_cond_signal(&assFilaCond);
  } }

  if (assAtualValido && assAtual.generation != generation) {
    ass_frame_liberar(&assAtual); assAtualValido = 0;
  }
  /* O quadro da fila que entra: o mais novo ja devido (os anteriores a ele
   * saem). Sem hold, qualquer um esta devido — o de sempre. */
  { int devido = -1;
    for (i = 0; i < assFilaN; i++) {
      if (assFila[i].generation != generation) { ass_fila_tirar_locked(i, NULL); i--; continue; }
      if (!segurar || assFila[i].ms <= agora + 8) {
        devido = i;
      }
    }
    if (devido >= 0) {
      for (i = 0; i < devido; i++) ass_fila_tirar_locked(0, NULL);
      ass_fila_tirar_locked(0, &novo);
      quadroNovo = 1;
      assMed.trocas++;
    } }
  reset = assTexResetar; assTexResetar = 0;
  corAtiva = assCorAtiva; cr = assCorR; cg = assCorG; cb = assCorB;   // read under the lock (new path)
  assUltimoVistoMs = (double)pedido; assUltimoVistoGeracao = generation;
#if !ASS_NOVO
  (void)corAtiva; (void)cr; (void)cg; (void)cb; (void)saltou;
#endif

  if (!ASS_NOVO) {
    /* ---- o caminho de sempre (fora do .tpk) ---- */
    if (quadroNovo) {
      ass_frame_liberar(&assAtual);
      assAtual = novo; memset(&novo, 0, sizeof novo);
      assAtualValido = 1;
    }
    if (reset) ass_apagar_texturas_locked();
    if (!assAtualValido) {
      ass_medir_locked(agora, 0, -1);
      pthread_mutex_unlock(&assFilaTrava);
      return 0;
    }
    corChave = assCorAtiva ? (int)(((unsigned)assCorR << 16) | ((unsigned)assCorG << 8) | (unsigned)assCorB) : -1;
    reenviar = assAtual.serial != assTexSerial || corChave != assTexCorChave;
    /* Plain text fades as ONE picture (owner 14/11/2026, ass_borda_estilo). */
    static GfxGrupo grupoVelho;
    const float alphaGrupo = alpha;
    if (__atomic_load_n(&assTextoSimples, __ATOMIC_ACQUIRE) && alpha < 0.999f && assAtual.count > 0) {
      float kk = assAtual.k > 1 ? (float)assAtual.k : 1.0f, esc = assAtual.esc > 0.01f ? assAtual.esc : kk;
      float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
      for (i = 0; i < assAtual.count; i++) {
        const AssCpuImage *im = &assAtual.images[i];
        float dx = (x + assAtual.ox) + (float)im->x / esc, dy = (y + assAtual.oy) + (float)im->y / esc;
        if (dx < x0) x0 = dx;
        if (dy < y0) y0 = dy;
        if (dx + (float)im->w / esc > x1) x1 = dx + (float)im->w / esc;
        if (dy + (float)im->h / esc > y1) y1 = dy + (float)im->h / esc;
      }
      if (x1 > x0 && y1 > y0 && gfx_grupo_comecar(&grupoVelho, (GfxRect){ x0 - 2.0f, y0 - 2.0f, x1 - x0 + 4.0f, y1 - y0 + 4.0f }, alpha))
        alpha = 1.0f;
    }
    for (i = 0; i < assAtual.count; i++) {
      const AssCpuImage *im = &assAtual.images[i];
      GLuint tex = reenviar ? ass_textura_locked(i, im)
                            : (i < assTexCap ? assTex[i].tex : 0);
      if (!tex) continue;
      /* libass trabalha no mesmo sistema de coordenadas do arquivo: o ponto
       * (0,0) e o canto superior esquerdo. A textura contem somente a caixa do
       * glyph e pode ser composta diretamente pelo shader de texto. */
      gfx_tex_aspect_atual = 0.0f;
      { float kk = assAtual.k > 1 ? (float)assAtual.k : 1.0f;
        float esc = assAtual.esc > 0.01f ? assAtual.esc : kk;
        /* Origem arredondada ao pixel do alvo: 1:1, sem meio pixel de filtro. */
        float dx = roundf((x + assAtual.ox) * kk) / kk + (float)im->x / esc;
        float dy = roundf((y + assAtual.oy) * kk) / kk + (float)im->y / esc;
        gfx_rect((GfxRect){ dx, dy, (float)im->w / esc, (float)im->h / esc }, tex, GFX_TEXTO,
                 0, 0, 0, 0, 1, 1, 1, alpha); }
    }
    gfx_grupo_terminar(&grupoVelho, alphaGrupo);
    if (reenviar) { assTexSerial = assAtual.serial; assTexCorChave = corChave; }
    gfx_tex_aspect_atual = 0.0f;
    n = assAtual.count;
    ass_medir_locked(agora, n > 0, assAtual.inicioMax);
    pthread_mutex_unlock(&assFilaTrava);
  }
#if ASS_NOVO
  else {
    /* ---- o caminho novo: upload e desenho fora da trava ---- */
    long long inicioMax;
    int temTexto;
    pthread_mutex_unlock(&assFilaTrava);
    if (reset || saltou || assMainResetar) {
      ass_main_soltar();
      ass_tc_apagar();
      assMainResetar = 0;
    }
    if (assMainValido && assMain.generation != generation) ass_main_soltar();
    if (quadroNovo) {
      ass_frame_liberar(&assMain);
      assMain = novo; memset(&novo, 0, sizeof novo);
      assMainValido = 1;
    }
    if (assMainValido) n = ass_main_desenhar(x, y, alpha, corAtiva, cr, cg, cb, quadroNovo);
    temTexto = assMainValido && n > 0;
    inicioMax = assMainValido ? assMain.inicioMax : -1;
    ass_gl_trava(&assFilaTrava);
    ass_medir_locked(agora, temTexto, inicioMax);
    pthread_mutex_unlock(&assFilaTrava);
  }
#endif
  ass_frame_liberar(&novo);
#ifdef NV_TPK
  /* Borda das legendas: o libass quer o premult se desenhou agora, se ha um
   * quadro com imagem na fila (render ahead) ou se uma fala comeca em ate
   * 200 ms. tpkborda_quadro decide (com 1 s de folga para desligar). */
  { int fila = 0, k;
    long long prox = __atomic_load_n(&assProxInicio, __ATOMIC_RELAXED);
    ass_gl_trava(&assFilaTrava);
    for (k = 0; k < assFilaN && !fila; k++) fila = assFila[k].count > 0;
    pthread_mutex_unlock(&assFilaTrava);
    if (n > 0 || fila || (prox != LLONG_MAX && prox >= agora && prox - agora <= 200)) {
      Uint32 tk = SDL_GetTicks();
      __atomic_store_n(&assBordaQuerTick, tk ? tk : 1u, __ATOMIC_RELEASE);
    } }
#endif
  return n;
}

/* Quadro SINCRONO, sem GL e sem o worker: so para teste e diagnostico
 * (tests/ass_fontes.sh). Passa pelo mesmo ass_render_frame — e ali, no
 * ass_start_frame, que mora a asserção que derrubava a C9. */
int assrender_quadro_cpu(double posSeg) {
  int n = -1, mudou = 0;
  pthread_mutex_lock(&assTrava);
  if (assTrack && assRenderer) {
    ASS_Image *im;
#if ASS_DIVIDIR
    ass_nv_set_split(assRenderer, 0, 0);   /* the whole frame here; the worker sets its own each frame */
#endif
    im = ass_render_frame(assRenderer, assTrack, (long long)llround(posSeg * 1000.0), &mudou);
    for (n = 0; im; im = im->next) n++;
  }
  pthread_mutex_unlock(&assTrava);
  return n;
}

#ifdef NV_TPK
/* tpkborda.c: 1 = o libass desenhou (ou ia desenhar) no ultimo segundo e a
 * faixa continua ativa. */
int assrender_borda_quer(void) {
  Uint32 tk = __atomic_load_n(&assBordaQuerTick, __ATOMIC_ACQUIRE);
  if (!tk || !assrender_ativo()) return 0;
  return (Uint32)(SDL_GetTicks() - tk) < 1000u;
}
#endif
int assrender_ativo(void) {
  unsigned geracao = __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE);
  int ativo = __atomic_load_n(&assTrackAtivo, __ATOMIC_ACQUIRE) &&
              __atomic_load_n(&assTrackGeracao, __ATOMIC_ACQUIRE) == geracao &&
              __atomic_load_n(&assWorkerCriado, __ATOMIC_ACQUIRE);
  return ativo;
}

const char *assrender_diagnostico(void) {
  static char texto[256];
  char base[sizeof assDiag];
  unsigned long long renderUs;
  size_t memoria;
  int eventos, fontes, resolucoes;
  long long coberturaIni, coberturaFim;
  pthread_mutex_lock(&assDiagTrava);
  snprintf(base, sizeof base, "%s", assDiag);
  pthread_mutex_unlock(&assDiagTrava);
  pthread_mutex_lock(&assFilaTrava);
  renderUs = assUltimoRenderUs; memoria = assBytesQuadro;
  pthread_mutex_unlock(&assFilaTrava);
  pthread_mutex_lock(&assTrava);
  eventos = assEventos; fontes = assFontes; resolucoes = assResolucaoFonte;
  coberturaIni = assCoberturaIni; coberturaFim = assCoberturaFim;
  pthread_mutex_unlock(&assTrava);
  snprintf(texto, sizeof texto,
           "%s; eventos=%d fontes=%d fontselect=%d cobertura=%lld-%lldms render=%lluus quadro=%zuB",
           base, eventos, fontes, resolucoes,
           coberturaIni == LLONG_MAX ? 0 : coberturaIni,
           coberturaFim == LLONG_MIN ? 0 : coberturaFim, renderUs, memoria);
  return texto;
}

static void *ass_preaquecer_fio(void *u) {
  (void)u;
  pthread_mutex_lock(&assTrava);
  ass_iniciar_locked(1);
  pthread_mutex_unlock(&assTrava);
  return NULL;
}

void assrender_preaquecer(void) {
  static int pedido;
  pthread_t t;
  if (__atomic_exchange_n(&pedido, 1, __ATOMIC_ACQ_REL)) return;
  if (pthread_create(&t, NULL, ass_preaquecer_fio, NULL) == 0) pthread_detach(t);
  else __atomic_store_n(&pedido, 0, __ATOMIC_RELEASE);
}

void assrender_geracao(unsigned geracao) {
  __atomic_store_n(&assGeracao, geracao, __ATOMIC_RELEASE);
  pthread_mutex_lock(&assFilaTrava);
  assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
  ass_fila_limpar_locked();
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

#else

static char assDiag[96] = "libass: backend nao compilado";
static unsigned assGeracao;

int assrender_carregar_texto(const char *corpo, size_t tamanho, unsigned geracao) { (void)corpo; (void)tamanho; (void)geracao; return 0; }
int assrender_texto_simples(void) { return 0; }
int assrender_fonte_local_transacao(const char *caminho, int (*confirmar)(void *), void *u) {
  (void)caminho; return confirmar ? confirmar(u) : 1;
}
int assrender_fonte_local_ativa(const char *caminho) { (void)caminho; return 0; }
void assrender_definir_texto_estilo(const PlainAssStyle *style) { (void)style; }
int assrender_carregar(const char *corpo, size_t tamanho, unsigned geracao) { (void)corpo; (void)tamanho; (void)geracao; return 0; }
int assrender_atualizar(const char *corpo, size_t tamanho, unsigned geracao) { (void)corpo; (void)tamanho; (void)geracao; return 0; }
void assrender_limpar(void) { assGeracao++; }
void assrender_definir_fps(double fps) { (void)fps; }
void assrender_definir_cor_video(int matriz, int faixa, int hdr, int altura) { (void)matriz; (void)faixa; (void)hdr; (void)altura; }
void assrender_limpar_fontes(void) {}
void assrender_aliviar(void) {}
int assrender_adicionar_fonte(const char *nome, const void *dados, size_t tamanho) {
  (void)nome; (void)dados; (void)tamanho; return 0;
}
void assrender_definir_cor(int enabled, int r, int g, int b) {
  (void)enabled; (void)r; (void)g; (void)b;
}
void assrender_aplicar_invalidacao(void) {}
int assrender_desenhar(double posSeg, int atrasoMs, float alpha,
                       float x, float y, float w, float h) {
  (void)posSeg; (void)atrasoMs; (void)alpha; (void)x; (void)y; (void)w; (void)h; return 0;
}
void assrender_definir_layout(float x, float y, float w, float h,
                              int videoW, int videoH, double escalaFonte) {
  (void)x; (void)y; (void)w; (void)h; (void)videoW; (void)videoH; (void)escalaFonte;
}
void assrender_pixels_por_unidade(float f) { (void)f; }
int assrender_ativo(void) { return 0; }
#ifdef NV_TPK
int assrender_borda_quer(void) { return 0; }
#endif
int assrender_quadro_cpu(double posSeg) { (void)posSeg; return -1; }
const char *assrender_diagnostico(void) { return assDiag; }
void assrender_geracao(unsigned geracao) { assGeracao = geracao; }
void assrender_preaquecer(void) {}

#endif
