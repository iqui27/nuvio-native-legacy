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
#include "gfx.h"
#include "bidi.h"
#include "assperf.h"
#include "assfios.h"
#if !defined(__EMSCRIPTEN__)
#include <sys/resource.h>
#include <sys/syscall.h>
#endif

#define ASS_TEX_INICIAL 64

typedef struct {
  GLuint tex;
  int w, h;
} AssTex;


typedef struct {
  int x, y, w, h, stride, type;
  unsigned color;
  unsigned char *bitmap;
  unsigned long long chave;   // assperf: conteudo (bitmap + w + h); 0 = nao calculada
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
#define ASS_FILA_BYTES (24u * 1024u * 1024u)
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

static void ass_mensagem(int nivel, const char *fmt, va_list args, void *dados) {
  char linha[768];
  (void)dados;
  vsnprintf(linha, sizeof linha, fmt, args);
  if (strstr(linha, "fontselect:")) assResolucaoFonte++;
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
}

// Explicit selection changes only. Routine packet delivery keeps the original
// font cache path; a full current batch lets us restore attachment precedence.
static void ass_fontes_reconstruir_locked(const unsigned char *selecionada, size_t n) {
  ass_clear_fonts(assLib);
  if (selecionada) ass_add_font(assLib, assFonteLocal, (const char *)selecionada, (int)n);
  assFonteRegistrada = selecionada != NULL;
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
  }
  ass_carregar_pasta_locked();
  snprintf(assFallbackFont, sizeof assFallbackFont, "%s", fallbackFont);
  ass_aplicar_fontes_locked();
  assNFontesVistas = 0; assBytesFontesVistas = 0;
  ass_set_cache_limits(assRenderer, 0, 32);
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


static int ass_frame_copiar(ASS_Image *im, AssCpuFrame *out, int hashear) {
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
    /* Texture cache key (new engine). Alpha 8-bit is tinted at draw time:
     * the same bitmap serves any colour. */
    dst->chave = hashear ? ass_chave_bitmap(dst->bitmap, p->w, p->h, 0u) : 0;
    total += bytes; i++;
  }
  out->count = i; out->bytes = total;
  return 1;
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
      if (p->overrides & PLAIN_ASS_BORDER) {
        s->BorderStyle = 1;
        s->Outline = p->border == 1 ? 2 : 0;
        s->Shadow = p->border == 2 ? 4 : 0;
      }
      if (p->overrides & PLAIN_ASS_BG) {
        s->BorderStyle = p->background ? 4 : 1;
        s->BackColour = p->background ? (unsigned)(255 - p->background * 255 * 16 / 100) : 46u;
      }
      // BackColour carries shadow RGB unless it is a background box.
      if ((p->overrides & PLAIN_ASS_OUTLINE) && s->BorderStyle == 1)
        s->BackColour = ((uint32_t)p->outlineRgb << 8) | (s->BackColour & 255u);
      if (p->overrides & PLAIN_ASS_POS) s->MarginV = p->marginV * assTrack->PlayResY / 1080;
      if (p->overrides & PLAIN_ASS_BOLD) s->Bold = p->bold;
    }
    /* Flush cached event layout after an explicit style change or reset. */
    ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_MARGINS);
    ass_set_selective_style_override_enabled(assRenderer, 0);
    plainApl = *p; plainAplValido = 1;
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
  s->Outline = p->border == 1 ? 2 : 0;
  s->Shadow = p->border == 2 ? 4 : 0;
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
  plainApl = *p; plainAplValido = 1;
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
#if defined(NV_TPK) && defined(ASS_NV_API)
  { static int feito;
    if (!feito) {
      /* The blur helper threads are born inside libass and inherit the
       * caller's name: "nv-assblur" tells them apart from the UI. */
      char nomeAnt[16];
      feito = 1;
      ass_nv_set_engine(assRenderer, 1);
      assfios_nome_trocar("nv-assblur", nomeAnt);
      ass_nv_set_threads(ASSPERF_FIOS);
      assfios_nome_voltar(nomeAnt);
    } }
  /* Dialogue opacity (Subtitles > Style): only lines without \pos, \move,
   * \clip, \org or a drawing; the signs keep the author's alpha. */
  ass_nv_set_dialogue_alpha(assRenderer, simples ? 255 : (assperf_get(AP_ASSOPAC) * 255 + 50) / 100);
#endif
}


/* Evento que muda SOZINHO entre Start e End: \t, \move, \fad/\fade,
 * karaoke (\k, \kf, \ko, \K) ou Effect (Banner/Scroll). */
static int ass_evento_anima(const ASS_Event *e) {
  const char *p;
  if (e->Effect && e->Effect[0]) return 1;
  if (!e->Text) return 0;
  for (p = strchr(e->Text, '\\'); p; p = strchr(p + 1, '\\'))
    if (!strncmp(p, "\\t(", 3) || !strncmp(p, "\\move", 5) || !strncmp(p, "\\fad", 4) || p[1] == 'k' || p[1] == 'K')
      return 1;
  return 0;
}

static double ass_ms_entre(const struct timespec *a, const struct timespec *b) {
  return (double)(b->tv_sec - a->tv_sec) * 1000.0 + (double)(b->tv_nsec - a->tv_nsec) / 1e6;
}

static void ass_pod_devolver_assTrava(void);
static double ass_fps(void);
static long long ass_grade_seguinte(long long t);
static long long ass_grade_desde(long long ms);
static void *ass_worker_loop(void *unused) {
  (void)unused;
  assfios_nomear("nv-assrender");
  for (;;) {
    AssCpuFrame frame;
    unsigned generation, serial, epoch;
    double ms, esc, resol;
    float ox, oy;
    int lw, lh, vw, vh, plainValid, k;
    PlainAssStyle plain;
    struct timespec a, b, c0;
    ASS_Image *images = NULL;
    int changed = 0, pronto = 0, igual = 0, seq = 0;
    long long validoDe = LLONG_MAX, validoAte = LLONG_MAX;
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
      if (assperf_pesado()) prox = alvo;
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
    ox = layPedX; oy = layPedY; lw = layPedW; lh = layPedH; k = layPedK;
    /* assperf: resolucao do render. O quadro sai `resol` do tamanho do alvo e
     * o desenho amplia (esc = k x resol pixels por unidade). 1 = como antes. */
    resol = assperf_escala_quadro();   // Auto: 75 % while heavy (assperf.c)
    lw = (int)lround((double)lw * k * resol); lh = (int)lround((double)lh * k * resol);   /* libass renderiza em pixels do alvo, nao em unidades de layout */
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
        PlainAssStyle ps = plain;
        if (ass_estilo_autor_locked()) ps.overrides = 0;   // estilos do autor de volta
        ass_plain_style_locked(&ps);
      }
      if (ass_estilo_autor_locked())
        ass_set_selective_style_override_enabled(assRenderer, ASS_OVERRIDE_BIT_SELECTIVE_FONT_SCALE);
      ass_plain_reflow_locked(t);
      ass_pod_devolver_assTrava();   // seek para tras alem da poda: as falas voltam antes do render
      images = ass_render_frame(assRenderer, assTrack, t, &changed);
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
          if (ev->Start < t || ev->Start >= t + 1500) continue;   /* text length only inside the window */
          n = ev->Text ? (long)strlen(ev->Text) : 0;
          if (ev->Start < t + 1000) { nIni++; chars += n; }
          if (ev->Start >= t + 500) { nIniC++; charsC += n; }
        }
        for (im = images; im; im = im->next) nImg++;
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
        pronto = ass_frame_copiar(images, &frame, ASS_NOVO);
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
    }
    pthread_mutex_unlock(&assTrava);
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
#define ASS_TC_TETO 768
#define ASS_TC_BYTES (24u * 1024u * 1024u)
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

static size_t ass_tc_tam(int w, int h) { return (size_t)w * (size_t)h; }

static void ass_tc_apagar(void) {
  int i;
  for (i = 0; i < assTcN; i++)
    if (assTc[i].tex) { gfx_tex_esquecer(assTc[i].tex); glDeleteTextures(1, &assTc[i].tex); }
  free(assTc); assTc = NULL; assTcN = assTcCap = 0; assTcBytes = 0;
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
  return 0;   // tudo em uso agora (raro): a imagem fica de fora deste quadro
}

/* Sobe (se o quadro e novo) e desenha assMain. Fio GL. */
static int ass_main_desenhar(float x, float y, float alpha, int corAtiva, int cr, int cg, int cb,
                             int quadroNovo) {
  int i;
  float kk = assMain.k > 1 ? (float)assMain.k : 1.0f, esc = assMain.esc > 0.01f ? assMain.esc : kk;
  assGlQuadro++;
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
  for (i = 0; i < assMain.count; i++) {
    const AssCpuImage *im = &assMain.images[i];
    GLuint tex = assMainTex[i];
    unsigned c = im->color;
    float dx, dy, r, g, b, op;
    if (!tex) continue;
    gfx_tex_aspect_atual = 0.0f;
    /* Origem arredondada ao pixel do ALVO (kk); a imagem em pixels do
     * quadro do libass (esc = kk x resolucao), ampliada quando esc < kk. */
    dx = roundf((x + assMain.ox) * kk) / kk + (float)im->x / esc;
    dy = roundf((y + assMain.oy) * kk) / kk + (float)im->y / esc;
    r = (float)((c >> 24) & 255u) / 255.0f; g = (float)((c >> 16) & 255u) / 255.0f;
    b = (float)((c >> 8) & 255u) / 255.0f; op = (float)(255u - (c & 255u)) / 255.0f;
    if (corAtiva && im->type == IMAGE_TYPE_CHARACTER) { r = cr / 255.0f; g = cg / 255.0f; b = cb / 255.0f; }
    if (gama != 1.0 || contraste != 1.0) {
      r = (float)pow(r * contraste, gama); g = (float)pow(g * contraste, gama); b = (float)pow(b * contraste, gama);
    }
    gfx_rect((GfxRect){ dx, dy, (float)im->w / esc, (float)im->h / esc }, tex, GFX_MARCA,
             0, 0, 0, 0, r, g, b, alpha * op);
  }
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
    assEventos = assTrack->n_events;
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
  { int antes = assTrack->n_events, k;
    for (k = 0; k < nNovas; k++) {
      int n0 = assTrack->n_events;
      ass_process_data(assTrack, (char *)(corpo + novasO[k]), novasN[k]);
      if (assTrack->n_events > n0 && ass_inc_ev_por(assTrack->n_events)) {
        /* ReadOrder = ordem de chegada. Antes ficava 0 em todas e o libass
         * empatava pela posicao no vetor: igual, mas uma fala devolvida pela
         * poda ia para o fim e mudava quem sobe na colisao. */
        while (incEvN < assTrack->n_events) { assTrack->events[incEvN].ReadOrder = ++assLeituraSeq; incEv[incEvN++] = novasH[k]; }
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
  assEventos = total;
  assCoberturaIni = LLONG_MAX; assCoberturaFim = LLONG_MIN;
  { int k;
    for (k = 0; k < total; k++) {
      long long ini = assTrack->events[k].Start, fimE = ini + assTrack->events[k].Duration;
      if (ini < assCoberturaIni) assCoberturaIni = ini;
      if (fimE > assCoberturaFim) assCoberturaFim = fimE;
    } }
  incPronto = !desalinhou && incEvN == assTrack->n_events;
  pthread_mutex_unlock(&assTrava);
  if (aceitas || removidos || reconstruir) {
    pthread_mutex_lock(&assFilaTrava);
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
  if (assTrack) ass_free_track(assTrack);
  assTrack = track;
  ass_trilha_tailandes(track);
  plainTimingValid = 0;
  /* Authored ASS retains its own margins after a converted plain track. */
  ass_set_selective_style_override_enabled(assRenderer, 0);
  __atomic_store_n(&assTextoSimples, simples, __ATOMIC_RELEASE);
  plainAplValido = 0;
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
  ass_diag(simples ? "libass: texto RTL simples ativo" : "libass: ASS completo ativo");
  pthread_mutex_unlock(&assTrava);
  if (!ass_worker_iniciar()) {
    pthread_mutex_lock(&assTrava);
    if (assTrack == track) { ass_authored_styles_free(); ass_free_track(assTrack); assTrack = NULL; }
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
int assrender_fonte_local_ativa(const char *caminho) {
  pthread_mutex_lock(&assTrava);
  int ativa = caminho && caminho[0] && assFonteRegistrada && !strcmp(assFonteLocal, caminho);
  pthread_mutex_unlock(&assTrava);
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
  pthread_mutex_lock(&assFilaTrava);
  if (!plainPedValido || memcmp(style, &plainPed, sizeof *style)) {
    plainPed = *style; plainPedValido = 1;
    ++assEpoch; assTemUltimoPedido = 0;
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
  pthread_mutex_lock(&assTrava);
  ass_authored_styles_free();
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  assEventos = 0;
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
  if (assTrack) { ass_free_track(assTrack); assTrack = NULL; }
  assEventos = 0; assFontes = 0; assResolucaoFonte = 0;
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
  pthread_mutex_lock(&assFilaTrava);
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



/* Grade dos quadros (assrender_definir_fps). So o fio GL. */
static double assFps;   /* GL escreve, o worker le: __atomic */
void assrender_definir_fps(double fps) {
  double v = fps > 1.0 && fps < 200.0 ? fps : 0.0;
  __atomic_store(&assFps, &v, __ATOMIC_RELAXED);
}
static double ass_fps(void) { double v; __atomic_load(&assFps, &v, __ATOMIC_RELAXED); return v; }
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
  }
  /* Hold until due: com render ahead o quadro feito para daqui a 100 ms
   * espera a sua vez na fila. */
  segurar = adiante > 0;
  /* Quadros INTEIROS a frente: 100 ms = 2,4 quadros caia na borda e o pedido
   * pulava entre +2 e +3. */
  { double fps = ass_fps();
    if (fps > 0 && adiante > 0 && !assperf_pesado()) {
      long long q = llround((double)adiante * fps / 1000.0);
      if (q < 1) q = 1;
      /* Cena leve com quadros caros (mascaras do polegar: 50-65 ms no log 15):
       * 3 quadros a frente, senao dois seguidos chegam um quadro tarde. */
      { double pico; __atomic_load(&assRenderPico, &pico, __ATOMIC_RELAXED);
        if (pico > 40.0 && q < 3) q = 3; }
      pedido = ass_na_grade(agora + llround((double)q * 1000.0 / fps));
    } else pedido = ass_na_grade(agora + adiante); }
  pthread_mutex_lock(&assFilaTrava);
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
    pthread_mutex_lock(&assFilaTrava);
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
    pthread_mutex_lock(&assFilaTrava);
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
    ASS_Image *im = ass_render_frame(assRenderer, assTrack, (long long)llround(posSeg * 1000.0), &mudou);
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
