#include "assrender.h"

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

#ifdef NV_ASS_LIBASS
#include <SDL2/SDL.h>
#include <ass/ass.h>
#include "gfx.h"

#define ASS_TEX_INICIAL 64

typedef struct {
  GLuint tex;
  int w, h;
} AssTex;

typedef struct {
  int x, y, w, h, stride, type;
  unsigned color;
  unsigned char *bitmap;
} AssCpuImage;

typedef struct {
  AssCpuImage *images;
  int count;
  unsigned generation, serial;
  size_t bytes;
  long long ms;          // instante (relogio do video) que este quadro mostra
  long long inicioMax;   // maior Start dos eventos vivos em ms; -1 sem evento
} AssCpuFrame;

static pthread_mutex_t assTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t assFilaTrava = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t assFilaCond = PTHREAD_COND_INITIALIZER;
static pthread_t assWorker;
static int assWorkerCriado, assTrackAtivo, assWorkerParar, assPedidoPendente;
static int assProntoValido, assAtualValido;
static double assPedidoMs;
static unsigned assPedidoGeracao, assPedidoSerial, assSerial;
static unsigned assEpoch;
static AssCpuFrame assPronto, assAtual;
static double assUltimoPedidoMs;
static unsigned assUltimoPedidoGeracao;
static int assTemUltimoPedido;
static int assEventos, assFontes, assResolucaoFonte;
static long long assCoberturaIni, assCoberturaFim;
static ASS_Library *assLib;
static ASS_Renderer *assRenderer;
static ASS_Track *assTrack;
static unsigned assTrackGeracao;
static AssTex *assTex;
static int assTexCap;
static int assTexResetar;
static int assFrameW, assFrameH;
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

static void ass_diag(const char *s) {
  pthread_mutex_lock(&assDiagTrava);
  snprintf(assDiag, sizeof assDiag, "%s", s ? s : "");
  pthread_mutex_unlock(&assDiagTrava);
}

static void ass_mensagem(int nivel, const char *fmt, va_list args, void *dados) {
  char linha[768];
  (void)dados;
  vsnprintf(linha, sizeof linha, fmt, args);
  if (strstr(linha, "fontselect:")) assResolucaoFonte++;
  /* So ate MSGL_INFO (4), o mesmo corte do callback padrao do libass (que
   * para em 5). Os niveis 6-7 sao depuracao por quadro e por glifo. */
  if (nivel > 4) return;
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
static char assFallbackFont[768];

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
static char assPastaFontes[640];
static void ass_fonte_da_pasta(const char *nome, const void *dados, size_t tam, void *u) {
  (void)u;
  ass_add_font(assLib, nome, (const char *)dados, (int)tam);
}
static void ass_carregar_pasta_locked(void) {
  int ignorados = 0, lidas;
  if (!assLib || !assPastaFontes[0]) return;
  lidas = assrender_ler_pasta_fontes(assPastaFontes, ass_fonte_da_pasta, NULL, &ignorados);
  fprintf(stderr, "[libass] pasta %s: %d fonte(s); %d arquivo(s) que nao sao fonte ignorado(s)\n",
          assPastaFontes, lidas, ignorados);
}

static void ass_aplicar_fontes_locked(void) {
  if (!assRenderer) return;
  ass_set_fonts(assRenderer, assFallbackFont[0] ? assFallbackFont : NULL, "Arial",
                ASS_FONTPROVIDER_AUTODETECT, NULL, 1);
}

static void ass_iniciar_locked(void) {
  char fontDir[640] = "";
  char fallbackFont[768] = "";
  struct timespec t0, t1;
  if (assLib) return;
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
    if (access("/usr/share/fonts", R_OK) == 0)
      snprintf(fontDir, sizeof fontDir, "%s", "/usr/share/fonts");
    if (access(fallbackFont, R_OK) != 0 && access("/usr/share/fonts/LG_Display-Regular.ttf", R_OK) == 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "/usr/share/fonts/LG_Display-Regular.ttf");
    if (access(fallbackFont, R_OK) != 0 && access("/usr/share/fonts/DroidSans.ttf", R_OK) == 0)
      snprintf(fallbackFont, sizeof fallbackFont, "%s", "/usr/share/fonts/DroidSans.ttf");
  }
#endif
  snprintf(assPastaFontes, sizeof assPastaFontes, "%s", fontDir);
  ass_carregar_pasta_locked();
  snprintf(assFallbackFont, sizeof assFallbackFont, "%s", fallbackFont);
  ass_aplicar_fontes_locked();
  assNFontesVistas = 0; assBytesFontesVistas = 0;
  ass_set_cache_limits(assRenderer, 0, 32);
  assFrameW = 1920; assFrameH = 1080;
  ass_set_frame_size(assRenderer, assFrameW, assFrameH);
  ass_set_storage_size(assRenderer, assFrameW, assFrameH);
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

static int ass_frame_copiar(ASS_Image *im, AssCpuFrame *out) {
  ASS_Image *p;
  int n = 0, i = 0;
  size_t total = 0;
  memset(out, 0, sizeof *out);
  for (p = im; p; p = p->next) {
    if (p->bitmap && p->w > 0 && p->h > 0 && p->stride >= p->w) n++;
    if (n > 4096) return 0;
  }
  if (n) {
    out->images = calloc((size_t)n, sizeof *out->images);
    if (!out->images) return 0;
    out->count = n;
  }
  for (p = im; p; p = p->next) {
    AssCpuImage *dst;
    size_t bytes;
    int y;
    if (!p->bitmap || p->w <= 0 || p->h <= 0 || p->stride < p->w) continue;
    bytes = (size_t)p->w * (size_t)p->h;
    /* Mantem o snapshot CPU de cada frame abaixo de 16 MiB. O quadro atual e
     * o pronto podem coexistir por um instante, portanto o teto total e 32 MiB. */
    if (bytes > 16u * 1024u * 1024u - total) {
      ass_frame_liberar(out); return 0;
    }
    dst = &out->images[i];
    dst->bitmap = malloc(bytes);
    if (!dst->bitmap) { ass_frame_liberar(out); return 0; }
    dst->x = p->dst_x; dst->y = p->dst_y;
    dst->w = p->w; dst->h = p->h; dst->stride = p->w;
    dst->type = p->type; dst->color = p->color;
    for (y = 0; y < p->h; y++)
      memcpy(dst->bitmap + (size_t)y * (size_t)p->w,
             p->bitmap + (size_t)y * (size_t)p->stride, (size_t)p->w);
    total += bytes; i++;
  }
  out->count = i; out->bytes = total;
  return 1;
}

static void *ass_worker_loop(void *unused) {
  (void)unused;
  for (;;) {
    AssCpuFrame frame;
    unsigned generation, serial, epoch;
    double ms;
    struct timespec a, b;
    ASS_Image *images = NULL;
    int changed = 0, pronto = 0, igual = 0;
    memset(&frame, 0, sizeof frame);
    pthread_mutex_lock(&assFilaTrava);
    while (!assWorkerParar && !assPedidoPendente)
      pthread_cond_wait(&assFilaCond, &assFilaTrava);
    if (assWorkerParar) { pthread_mutex_unlock(&assFilaTrava); break; }
    generation = assPedidoGeracao; serial = assPedidoSerial; epoch = assEpoch; ms = assPedidoMs;
    assPedidoPendente = 0;
    pthread_mutex_unlock(&assFilaTrava);

    clock_gettime(CLOCK_MONOTONIC, &a);
    pthread_mutex_lock(&assTrava);
    if (generation == __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) &&
        generation == assTrackGeracao &&
        assTrack && assRenderer) {
      long long t = (long long)llround(ms);
      images = ass_render_frame(assRenderer, assTrack, t, &changed);
      /* Igual ao publicado: nada a fazer, o quadro em tela continua certo. */
      pthread_mutex_lock(&assFilaTrava);
      igual = !changed && assProduziu && assProduzidoEpoch == epoch &&
              assProduzidoGeracao == generation && epoch == assEpoch;
      pthread_mutex_unlock(&assFilaTrava);
      if (!igual) {
        pronto = ass_frame_copiar(images, &frame);
        frame.ms = t; frame.inicioMax = -1;
        { int i;
          for (i = 0; i < assTrack->n_events; i++) {
            long long ini = assTrack->events[i].Start;
            if (ini <= t && ini + assTrack->events[i].Duration > t && ini > frame.inicioMax)
              frame.inicioMax = ini;
          } }
      }
    }
    pthread_mutex_unlock(&assTrava);
    clock_gettime(CLOCK_MONOTONIC, &b);

    pthread_mutex_lock(&assFilaTrava);
    { unsigned long long us = (unsigned long long)((b.tv_sec - a.tv_sec) * 1000000ll +
                              (b.tv_nsec - a.tv_nsec) / 1000ll);
      assMed.renders++; assMed.renderSomaUs += us;
      if (us > assMed.renderMaxUs) assMed.renderMaxUs = us;
      if (igual) assMed.iguais++;
      assUltimoRenderUs = us; }
    /* SEM o antigo "fabs(assPedidoMs - ms) <= 50": com o relogio interpolado o
     * pedido anda 16 ms por quadro, e um render de mais de 50 ms na C9 jogava
     * fora TODO quadro — a legenda nunca aparecia. Um quadro de 30 ms atras e
     * melhor que nenhum; o proximo pedido ja esta na fila. */
    if (pronto && !assWorkerParar && epoch == assEpoch &&
        generation == __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE) &&
        generation == assPedidoGeracao) {
      ass_frame_liberar(&assPronto);
      frame.generation = generation; frame.serial = serial;
      assPronto = frame; memset(&frame, 0, sizeof frame);
      assProntoValido = 1;
      assProduziu = 1; assProduzidoEpoch = epoch; assProduzidoGeracao = generation;
      assBytesQuadro = assPronto.bytes;
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

static int ass_carregar(const char *corpo, size_t tamanho, unsigned geracao, int manter) {
  ASS_Track *track;
  char *copia;
  if (!corpo || !tamanho) return 0;
  pthread_mutex_lock(&assTrava);
  if (geracao != __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE)) {
    pthread_mutex_unlock(&assTrava); return 0;
  }
  ass_iniciar_locked();
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
  if (geracao != __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE)) {
    ass_free_track(track);
    pthread_mutex_unlock(&assTrava); return 0;
  }
  if (assTrack) ass_free_track(assTrack);
  assTrack = track;
  assEventos = track->n_events;
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
  ass_diag("libass: ASS completo ativo");
  pthread_mutex_unlock(&assTrava);
  if (!ass_worker_iniciar()) {
    pthread_mutex_lock(&assTrava);
    if (assTrack == track) { ass_free_track(assTrack); assTrack = NULL; }
    assEventos = 0;
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
    ass_frame_liberar(&assPronto); assProntoValido = 0;
    ass_frame_liberar(&assAtual); assAtualValido = 0;
    assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
    assTexResetar = 1;
  }
  pthread_mutex_unlock(&assFilaTrava);
  return 1;
}

int assrender_carregar(const char *corpo, size_t tamanho, unsigned geracao) {
  return ass_carregar(corpo, tamanho, geracao, 0);
}

int assrender_atualizar(const char *corpo, size_t tamanho, unsigned geracao) {
  int mesma;
  pthread_mutex_lock(&assTrava);
  mesma = assTrack && assTrackGeracao == geracao;
  pthread_mutex_unlock(&assTrava);
  return ass_carregar(corpo, tamanho, geracao, mesma);
}

void assrender_limpar(void) {
  pthread_mutex_lock(&assTrava);
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  assEventos = 0;
  assCoberturaIni = assCoberturaFim = 0;
  __atomic_store_n(&assTrackGeracao, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 0, __ATOMIC_RELEASE);
  ass_diag("libass: sem faixa");
  pthread_mutex_unlock(&assTrava);
  pthread_mutex_lock(&assFilaTrava);
  assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
  ass_frame_liberar(&assPronto); assProntoValido = 0;
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

void assrender_limpar_fontes(void) {
  pthread_mutex_lock(&assTrava);
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  assEventos = 0; assFontes = 0; assResolucaoFonte = 0;
  assCoberturaIni = assCoberturaFim = 0;
  __atomic_store_n(&assTrackGeracao, 0, __ATOMIC_RELEASE);
  __atomic_store_n(&assTrackAtivo, 0, __ATOMIC_RELEASE);
  /* SEM ass_clear_fonts: ver a nota de assFontesVistas. As fontes da faixa
   * anterior ficam na biblioteca; so sao usadas se a proxima pedir o mesmo
   * nome. assFontes (diagnostico) volta a contar as desta faixa. */
  pthread_mutex_unlock(&assTrava);
  pthread_mutex_lock(&assFilaTrava);
  assPedidoPendente = 0; ++assEpoch; ++assSerial; assPedidoSerial = assSerial;
  ass_frame_liberar(&assPronto); assProntoValido = 0;
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

int assrender_adicionar_fonte(const char *nome, const void *dados, size_t tamanho) {
  if (!nome || !*nome || !dados || !tamanho || tamanho > (size_t)INT_MAX) return 0;
  pthread_mutex_lock(&assTrava);
  ass_iniciar_locked();
  if (assLib) {
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
        ass_carregar_pasta_locked();
        ass_aplicar_fontes_locked();
        assNFontesVistas = 0; assBytesFontesVistas = 0;
      }
      ass_add_font(assLib, nome, (const char *)dados, (int)tamanho);
      { AssFonteVista *v = &assFontesVistas[assNFontesVistas++];
        snprintf(v->nome, sizeof v->nome, "%s", nome);
        v->tam = tamanho; v->amostra = am; }
      assBytesFontesVistas += tamanho;
    }
    assFontes++;
  }
  { int ok = assLib != NULL; pthread_mutex_unlock(&assTrava); return ok; }
}

void assrender_definir_cor(int enabled, int r, int g, int b) {
  pthread_mutex_lock(&assFilaTrava);
  assCorAtiva = !!enabled;
  assCorR = r < 0 ? 0 : r > 255 ? 255 : r;
  assCorG = g < 0 ? 0 : g > 255 ? 255 : g;
  assCorB = b < 0 ? 0 : b > 255 ? 255 : b;
  pthread_mutex_unlock(&assFilaTrava);
}

void assrender_aplicar_invalidacao(void) {
  pthread_mutex_lock(&assFilaTrava);
  if (assTexResetar) {
    assTexResetar = 0;
    ass_apagar_texturas_locked();
  }
  pthread_mutex_unlock(&assFilaTrava);
}

static double ass_mono(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (double)ts.tv_sec + ts.tv_nsec / 1e9;
}

/* Medidas por quadro desenhado, sob assFilaTrava. `agora` e o relogio do
 * video em ms (o mesmo que foi pedido ao libass). */
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
       * relogio do video, e a diferenca. E a medida do "atrasada". */
      printf("[ass] fala %lld.%03lld apareceu em %lld.%03lld (%+lld ms)\n",
             inicioMax / 1000, inicioMax % 1000, agora / 1000, agora % 1000, atraso);
    }
    assMed.ultimoTextoMs = agora; assMed.vazios = 0;
  } else assMed.vazios++;
  assMed.textoAnterior = temTexto;
  if (m - assMed.ultimoRelatorio >= 10.0) {
    if (assMed.ultimoRelatorio > 0)
      printf("[ass] 10s: quadros=%ld trocas=%ld renders=%ld iguais=%ld render medio=%.1fms max=%.1fms "
             "piscas=%ld (vazios=%ld) falas=%ld atraso medio=%lldms max=%lldms eventos=%d\n",
             assMed.quadros, assMed.trocas, assMed.renders, assMed.iguais,
             assMed.renders ? assMed.renderSomaUs / 1000.0 / assMed.renders : 0.0,
             assMed.renderMaxUs / 1000.0, assMed.piscas, assMed.vaziosPisca, assMed.falas,
             assMed.falas ? assMed.atrasoSoma / assMed.falas : 0, assMed.atrasoMax, assEventos);
    fflush(stdout);
    assMed.quadros = assMed.trocas = assMed.renders = assMed.iguais = 0;
    assMed.piscas = assMed.vaziosPisca = assMed.falas = 0;
    assMed.renderSomaUs = assMed.renderMaxUs = 0;
    assMed.atrasoSoma = 0; assMed.atrasoMax = 0;
    assMed.ultimoRelatorio = m;
  }
}

int assrender_desenhar(double posSeg, int atrasoMs, float alpha,
                       float x, float y, float w, float h) {
  unsigned generation;
  int i, n = 0, reset, corChave, reenviar;
  long long agora;
  (void)w; (void)h;
  if (alpha <= 0.001f) return 0;
  generation = __atomic_load_n(&assGeracao, __ATOMIC_ACQUIRE);
  agora = (long long)llround(posSeg * 1000.0) + (long long)atrasoMs;
  pthread_mutex_lock(&assFilaTrava);
  if (!assWorkerCriado) { pthread_mutex_unlock(&assFilaTrava); return 0; }
  /* SALTO de verdade (seek): o quadro velho mostra outra cena e sai. ERA 200
   * ms — e o currentTime da C9 anda 197..247 ms por evento, 77 % acima de 200
   * (log de 23/09): a legenda era apagada a cada evento e piscava 2-4 vezes
   * por segundo. Com o relogio interpolado o passo e de um quadro; 1,5 s so
   * pega salto. Fora disso o quadro atual fica ate o proximo estar pronto. */
  if (assTemUltimoPedido && assUltimoPedidoGeracao == generation &&
      fabs((double)agora - assUltimoPedidoMs) > 1500.0) {
    ass_frame_liberar(&assPronto); assProntoValido = 0;
    ass_frame_liberar(&assAtual); assAtualValido = 0;
    ++assEpoch;
    ++assSerial;
  }
  if (!assTemUltimoPedido || assUltimoPedidoGeracao != generation ||
      fabs((double)agora - assUltimoPedidoMs) >= 5.0) {
    assUltimoPedidoMs = (double)agora;
    assUltimoPedidoGeracao = generation;
    assTemUltimoPedido = 1;
    assPedidoMs = (double)agora;
    assPedidoGeracao = generation;
    assPedidoSerial = ++assSerial;
    assPedidoPendente = 1;
    pthread_cond_signal(&assFilaCond);
  }

  if (assAtualValido && assAtual.generation != generation) {
    ass_frame_liberar(&assAtual); assAtualValido = 0;
  }
  if (assProntoValido) {
    if (assPronto.generation == generation) {
      ass_frame_liberar(&assAtual);
      assAtual = assPronto; memset(&assPronto, 0, sizeof assPronto);
      assAtualValido = 1;
      assMed.trocas++;
    } else ass_frame_liberar(&assPronto);
    assProntoValido = 0;
  }
  reset = assTexResetar; assTexResetar = 0;
  if (reset) ass_apagar_texturas_locked();
  if (!assAtualValido) {
    ass_medir_locked(agora, 0, -1);
    pthread_mutex_unlock(&assFilaTrava); return 0;
  }
  corChave = assCorAtiva ? (int)(((unsigned)assCorR << 16) | ((unsigned)assCorG << 8) | (unsigned)assCorB) : -1;
  reenviar = assAtual.serial != assTexSerial || corChave != assTexCorChave;
  for (i = 0; i < assAtual.count; i++) {
    const AssCpuImage *im = &assAtual.images[i];
    GLuint tex = reenviar ? ass_textura_locked(i, im)
                          : (i < assTexCap ? assTex[i].tex : 0);
    if (!tex) continue;
    /* libass trabalha no mesmo sistema de coordenadas do arquivo: o ponto
     * (0,0) e o canto superior esquerdo. A textura contem somente a caixa do
     * glyph e pode ser composta diretamente pelo shader de texto. */
    gfx_tex_aspect_atual = 0.0f;
    gfx_rect((GfxRect){ x + (float)im->x, y + (float)im->y,
                        (float)im->w, (float)im->h }, tex, GFX_TEXTO,
             0, 0, 0, 0, 1, 1, 1, alpha);
  }
  if (reenviar) { assTexSerial = assAtual.serial; assTexCorChave = corChave; }
  gfx_tex_aspect_atual = 0.0f;
  n = assAtual.count;
  ass_medir_locked(agora, n > 0, assAtual.inicioMax);
  pthread_mutex_unlock(&assFilaTrava);
  return n;
}

/* Quadro SINCRONO, sem GL e sem o worker: so para teste e diagnostico
 * (tests/ass_fontes.sh). Passa pelo mesmo ass_render_frame — e ali, no
 * ass_start_frame, que mora a asserção que derrubava a C9. */
int assrender_quadro_cpu(double posSeg) {
  int n = -1, mudou = 0;
  pthread_mutex_lock(&assTrava);
  if (assTrack && assRenderer) {
    ASS_Image *im = ass_render_frame(assRenderer, assTrack, (long long)llround(posSeg * 1000.0), &mudou);
    for (n = 0; im; im = im->next) n++;
  }
  pthread_mutex_unlock(&assTrava);
  return n;
}

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
  ass_iniciar_locked();
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
  ass_frame_liberar(&assPronto); assProntoValido = 0;
  ass_frame_liberar(&assAtual); assAtualValido = 0;
  assTemUltimoPedido = 0; assUltimoRenderUs = 0; assBytesQuadro = 0;
  assTexResetar = 1;
  pthread_mutex_unlock(&assFilaTrava);
}

#else

static char assDiag[96] = "libass: backend nao compilado";
static unsigned assGeracao;

int assrender_carregar(const char *corpo, size_t tamanho, unsigned geracao) { (void)corpo; (void)tamanho; (void)geracao; return 0; }
int assrender_atualizar(const char *corpo, size_t tamanho, unsigned geracao) { (void)corpo; (void)tamanho; (void)geracao; return 0; }
void assrender_limpar(void) { assGeracao++; }
void assrender_limpar_fontes(void) {}
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
int assrender_ativo(void) { return 0; }
int assrender_quadro_cpu(double posSeg) { (void)posSeg; return -1; }
const char *assrender_diagnostico(void) { return assDiag; }
void assrender_geracao(unsigned geracao) { assGeracao = geracao; }
void assrender_preaquecer(void) {}

#endif
