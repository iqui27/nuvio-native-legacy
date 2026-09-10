// AINDA NAO LIGADO A NADA. O detector abaixo esta pronto e testavel; o que
// falta e o lado de quem CHAMA: ler `focusGifUrl` das pastas que vem da conta
// (colecoes.c so le esse campo no caminho do PACOTE) e pedir o quadro aqui
// quando o cartaz estiver em foco. Ver gif.h para o levantamento de por que o
// webOS nao anima e o Tizen anima.
//
// Este arquivo entra na arvore agora para o cabecalho nao ficar orfao: gif.h
// foi commitado antes dele, por descuido meu, num commit que era do #30.
#include "gif.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ---------------------------------------------------------------- detector
//
// CONTA OS QUADROS SEM DECODIFICAR NENHUM. Todo bloco do GIF e prefixado por
// tamanho, entao da para caminhar pela estrutura pulando os dados comprimidos
// sem tocar no LZW. E o que separa "GIF parado" de "GIF animado" — a capa de
// colecao que vem do pacote e sempre um JPEG, a da conta pode ser qualquer
// coisa, e so a segunda precisa de tratamento.
//
// Nao ha limite de tamanho aqui de proposito: a funcao le o arquivo inteiro
// uma vez, na abertura do painel, e o maior GIF de capa medido tem dezenas de
// KB. Se um dia isso mudar, o teto entra em quem chama.

// Pula uma cadeia de sub-blocos: cada um comeca com o proprio tamanho, e a
// cadeia acaba num tamanho zero. Devolve a posicao depois do terminador, ou 0
// quando o arquivo acaba no meio (arquivo truncado nao vira laco infinito).
static size_t pularSubBlocos(const unsigned char *b, size_t n, size_t p) {
  while (p < n) {
    size_t len = b[p++];
    if (!len) return p;
    if (p + len > n) return 0;
    p += len;
  }
  return 0;
}

#ifdef __EMSCRIPTEN__
int gif_pode_animar(void) { return 1; }
#else
int gif_pode_animar(void) { return 0; }
#endif

int gif_animado(const char *caminho) {
  FILE *f;
  unsigned char *b;
  long tam;
  size_t p, n;
  int quadros = 0;
  if (!caminho || !caminho[0]) return 0;
  f = fopen(caminho, "rb");
  if (!f) return 0;
  fseek(f, 0, SEEK_END);
  tam = ftell(f);
  rewind(f);
  if (tam < 14) { fclose(f); return 0; }
  b = (unsigned char *)malloc((size_t)tam);
  if (!b) { fclose(f); return 0; }
  n = fread(b, 1, (size_t)tam, f);
  fclose(f);
  if (n < 14 || memcmp(b, "GIF8", 4)) { free(b); return 0; }

  // Descritor de tela logica: 7 bytes, e o bit 7 do quinto diz se ha paleta
  // global — que ocupa 3 * 2^(N+1) bytes logo depois.
  p = 6 + 7;
  if (b[10] & 0x80) p += (size_t)3 << ((b[10] & 7) + 1);

  while (p < n) {
    unsigned char sep = b[p++];
    if (sep == 0x3B) break;                       // fim do arquivo
    if (sep == 0x21) {                            // extensao: rotulo + blocos
      if (p >= n) break;
      p++;
      p = pularSubBlocos(b, n, p);
      if (!p) break;
      continue;
    }
    if (sep != 0x2C) break;                       // byte inesperado: desiste
    // Descritor de imagem: 9 bytes; o ultimo diz se ha paleta local.
    if (p + 9 > n) break;
    { unsigned char campos = b[p + 8];
      p += 9;
      if (campos & 0x80) p += (size_t)3 << ((campos & 7) + 1); }
    if (p >= n) break;
    p++;                                          // tamanho minimo do codigo
    p = pularSubBlocos(b, n, p);
    quadros++;
    // DOIS JA RESPONDEM A PERGUNTA. Varrer um GIF de duzentos quadros ate o fim
    // so para dizer "sim" seria trabalho jogado fora.
    if (quadros > 1) break;
    if (!p) break;
  }
  free(b);
  return quadros > 1;
}

// ---------------------------------------------------------------- animacao
#ifdef __EMSCRIPTEN__

// QUEM ANIMA E O NAVEGADOR. Uma <img> com o GIF anda sozinha; o Chromium conta
// o tempo, compoe os quadros e trata disposal — tudo que um decodificador
// proprio teria de refazer. Aqui so pedimos "o que esta valendo agora",
// desenhando a <img> num canvas e lendo os pixels.
//
// O arquivo vive no MEMFS/IDBFS e nao tem URL: por isso o blob. Ele e criado
// UMA vez por caminho e fica preso enquanto aquele cartaz estiver em foco —
// recriar por quadro reiniciaria a animacao no primeiro quadro, para sempre.
EM_JS(void, gif_js_preparar, (const char *cam, const unsigned char *dados, int n), {
  var caminho = UTF8ToString(cam);
  if (Module.nvGif && Module.nvGif.caminho === caminho) return;
  if (Module.nvGif && Module.nvGif.url) URL.revokeObjectURL(Module.nvGif.url);
  var bytes = HEAPU8.slice(dados, dados + n);
  var url = URL.createObjectURL(new Blob([bytes], { type: 'image/gif' }));
  var img = new Image();
  img.src = url;
  Module.nvGif = { caminho: caminho, url: url, img: img, cv: null, cx: null };
});

// Devolve 0 enquanto a <img> nao carregou; depois disso escreve os pixels em
// `destino` (RGBA, larg*alt*4) e devolve 1.
EM_JS(int, gif_js_quadro, (unsigned char *destino, int larg, int alt), {
  var g = Module.nvGif;
  if (!g || !g.img || !g.img.complete || !g.img.naturalWidth) return 0;
  if (!g.cv || g.cv.width !== larg || g.cv.height !== alt) {
    g.cv = document.createElement('canvas');
    g.cv.width = larg; g.cv.height = alt;
    g.cx = g.cv.getContext('2d');
  }
  g.cx.clearRect(0, 0, larg, alt);
  g.cx.drawImage(g.img, 0, 0, larg, alt);
  var d = g.cx.getImageData(0, 0, larg, alt).data;
  HEAPU8.set(d, destino);
  return 1;
});

EM_JS(void, gif_js_soltar, (), {
  if (Module.nvGif && Module.nvGif.url) URL.revokeObjectURL(Module.nvGif.url);
  Module.nvGif = null;
});

EM_JS(int, gif_js_largura, (), {
  var g = Module.nvGif;
  return (g && g.img && g.img.naturalWidth) ? g.img.naturalWidth : 0;
});
EM_JS(int, gif_js_altura, (), {
  var g = Module.nvGif;
  return (g && g.img && g.img.naturalHeight) ? g.img.naturalHeight : 0;
});

static GLuint tex;
static char   preso[512];
static int    texW, texH;

GLuint gif_textura(const char *caminho, int largAlvo) {
  int w, h;
  unsigned char *px;
  if (!caminho || !caminho[0] || largAlvo < 8) return 0;

  if (strcmp(preso, caminho)) {
    FILE *f = fopen(caminho, "rb");
    long tam;
    unsigned char *b;
    if (!f) return 0;
    fseek(f, 0, SEEK_END); tam = ftell(f); rewind(f);
    if (tam < 14) { fclose(f); return 0; }
    b = (unsigned char *)malloc((size_t)tam);
    if (!b) { fclose(f); return 0; }
    if (fread(b, 1, (size_t)tam, f) != (size_t)tam) { free(b); fclose(f); return 0; }
    fclose(f);
    gif_js_preparar(caminho, b, (int)tam);
    free(b);
    snprintf(preso, sizeof preso, "%s", caminho);
    texW = texH = 0;
  }

  { int nw = gif_js_largura(), nh = gif_js_altura();
    if (nw < 1 || nh < 1) return 0;              // ainda carregando
    w = largAlvo;
    h = (int)((double)nh * largAlvo / nw + 0.5);
    if (h < 1) h = 1; }

  px = (unsigned char *)malloc((size_t)w * (size_t)h * 4);
  if (!px) return 0;
  if (!gif_js_quadro(px, w, h)) { free(px); return 0; }

  if (!tex) {
    glGenTextures(1, &tex);
    if (!tex) { free(px); return 0; }
  }
  glBindTexture(GL_TEXTURE_2D, tex);
  if (w != texW || h != texH) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
    texW = w; texH = h;
  } else {
    // MESMO TAMANHO = MESMA TEXTURA, so o conteudo troca. glTexImage2D a cada
    // quadro realoca no driver; glTexSubImage2D escreve por cima.
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, px);
  }
  free(px);
  return tex;
}

void gif_parar(void) {
  if (!preso[0]) return;
  gif_js_soltar();
  preso[0] = 0;
  texW = texH = 0;
}

#else   /* webOS e Mac: sem animacao. Ver o cabecalho de gif.h. */

GLuint gif_textura(const char *caminho, int largAlvo) {
  (void)caminho; (void)largAlvo;
  return 0;
}
void gif_parar(void) {}

#endif
