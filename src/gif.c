// GIF animado das capas de colecao (#29) e das fotos de perfil (#45).
// Ver gif.h para onde anima e por que.
#include "gif.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

// ---------------------------------------------------------------- estrutura
//
// CAMINHA PELO ARQUIVO SEM DECODIFICAR PIXEL NENHUM. Todo bloco do GIF e
// prefixado por tamanho, entao da para achar cada quadro pulando os dados
// comprimidos sem tocar no LZW. Isso serve a duas perguntas:
//   - "isto e animacao ou uma imagem parada?" (gif_animado)
//   - "onde comeca e acaba cada quadro?"      (gif_mapear, que o decodificador usa)
//
// Nao ha limite de tamanho aqui de proposito: a funcao le o arquivo inteiro
// uma vez, na abertura do painel, e o maior GIF de capa medido tem ~1 MB. Se
// um dia isso mudar, o teto entra em quem chama.

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

// Cabecalho + descritor de tela logica + paleta global, em bytes. 0 se o
// arquivo nem chega a ser um GIF.
static size_t cabecalhoN(const unsigned char *b, size_t n) {
  size_t p;
  if (n < 14 || memcmp(b, "GIF8", 4)) return 0;
  // 6 da assinatura + 7 do descritor de tela; o bit 7 do quinto byte do
  // descritor diz se ha paleta global, que ocupa 3 * 2^(N+1) bytes depois.
  p = 13;
  if (b[10] & 0x80) p += (size_t)3 << ((b[10] & 7) + 1);
  return p <= n ? p : 0;
}

// Le o arquivo inteiro. Devolve NULL (e nao deixa lixo) em qualquer falha.
static unsigned char *lerTudo(const char *caminho, size_t *n) {
  FILE *f;
  long tam;
  unsigned char *b;
  size_t lidos;
  *n = 0;
  if (!caminho || !caminho[0]) return NULL;
  f = fopen(caminho, "rb");
  if (!f) return NULL;
  fseek(f, 0, SEEK_END);
  tam = ftell(f);
  rewind(f);
  if (tam < 14) { fclose(f); return NULL; }
  b = (unsigned char *)malloc((size_t)tam);
  if (!b) { fclose(f); return NULL; }
  lidos = fread(b, 1, (size_t)tam, f);
  fclose(f);
  if (lidos != (size_t)tam) { free(b); return NULL; }
  *n = lidos;
  return b;
}

// ---------------------------------------------------------------- orcamento
//
// POR QUE HA UM (24/09/2026). Registros 2340/2341/2351 (Tizen 6, deviceMemory
// 1): cada GIF de avatar que comecava a animar custava um quadro de 2,3 s
// (`des=2353`, `des=2342`, `des=2387`) e o Worker nao dava conta do ritmo ("deu
// a volta nos 35 quadros em 4119 ms", um GIF de 1050 ms). Numa TV que ja passa
// segundos parada com a propria home (`swap=` de 3 a 6 s sem GIF nenhum), a
// animacao e trabalho que ela nao tem de onde tirar. Nao esta provado que o
// GIF derrubou a pagina: as tres sessoes morreram na home, 150 a 420 s depois
// do ultimo GIF. O que esta medido e o custo acima.
//
// A MEDIDA E O QUE O GIF DECODIFICA POR VOLTA, quadros x tela logica x 4. Nao e
// memoria presa (o decodificador guarda um quadro composto por vez, ver
// "decodificador" abaixo), e o trabalho de decode por volta. A tabela ficou
// como estava na 1.4.6 quando o decode passou a ser nativo (1.4.7): o custo
// por quadro caiu, mas nao ha medida em TV de 2 GB que justifique mexer nela.
//
//   RAM <= 1 GB       0   nao anima: fica o primeiro quadro, parado
//   1 GB < RAM < 4   48 MB  35x512x512 (37 MB) e 51x360x360 (26 MB) animam;
//                          75x500x375 (56 MB) fica parado
//   RAM >= 4 GB     sem teto (o que sempre foi)
//   sem deviceMemory 48 MB  Chromium que nao informa e o mais velho
//
// Os 48 MB sao CHUTE pelos GIFs dos registros, nao medida de limite: nenhum
// aparelho de 2 GB aqui. Os GIFs de colecao vistos (21x498x448 = 19 MB,
// 45x480x270 = 23 MB) continuam animando em 2 GB.
size_t gif_custo(int quadros, int telaW, int telaH) {
  if (quadros < 1 || telaW < 1 || telaH < 1) return 0;
  return (size_t)quadros * (size_t)telaW * (size_t)telaH * 4u;
}

size_t gif_orcamento_para(double memGB) {
  if (memGB <= 0.0) return (size_t)48 * 1024 * 1024;
  if (memGB <= 1.0) return 0;
  if (memGB < 4.0) return (size_t)48 * 1024 * 1024;
  return GIF_SEM_TETO;
}

#ifdef __EMSCRIPTEN__
EM_JS(double, gif_js_memoria_gb, (), {
  try {
    var m = (typeof navigator !== 'undefined') ? navigator.deviceMemory : 0;
    return (typeof m === 'number' && m > 0) ? m : 0;
  } catch (e) { return 0; }
});

// Decidido uma vez: a RAM nao muda com o app aberto.
static size_t orcamento(void) {
  static int lido;
  static size_t orc;
  if (!lido) {
    double gb = gif_js_memoria_gb();
    lido = 1;
    orc = gif_orcamento_para(gb);
    if (orc == GIF_SEM_TETO)
      printf("[gif] orcamento de animacao: sem teto (deviceMemory=%g GB)\n", gb);
    else if (!orc)
      printf("[gif] orcamento de animacao: nenhum (deviceMemory=%g GB): GIF fica no primeiro quadro\n", gb);
    else
      printf("[gif] orcamento de animacao: %u MB por GIF (deviceMemory=%g GB)\n",
             (unsigned)(orc / (1024 * 1024)), gb);
    fflush(stdout);
  }
  return orc;
}

int gif_pode_animar(void) { return orcamento() != 0; }
#else
int gif_pode_animar(void) { return 0; }
#endif

int gif_mapear(const unsigned char *b, size_t n, GifQuadro *q, int max) {
  size_t p, gce = 0, gceN = 0;
  int achados = 0;
  if (!b || !q || max < 1) return 0;
  p = cabecalhoN(b, n);
  if (!p) return 0;

  while (p < n && achados < max) {
    unsigned char sep = b[p];
    if (sep == 0x3B) break;                       // fim do arquivo
    if (sep == 0x21) {                            // extensao: rotulo + blocos
      size_t ini = p, fimExt;
      if (p + 2 > n) break;
      fimExt = pularSubBlocos(b, n, p + 2);
      if (!fimExt) break;
      // O CONTROLE GRAFICO VALE PARA O PROXIMO QUADRO, e e dele que saem o
      // atraso e o metodo de descarte. Guardado inteiro (e nao so
      // interpretado) porque o decodificador le dele a cor transparente.
      if (b[p + 1] == 0xF9) { gce = ini; gceN = fimExt - ini; }
      p = fimExt;
      continue;
    }
    if (sep != 0x2C) break;                       // byte inesperado: desiste
    if (p + 10 > n) break;                        // separador + 9 do descritor
    {
      GifQuadro *d = &q[achados];
      unsigned char campos = b[p + 9];
      size_t dados = p + 10;
      d->esq  = b[p + 1] | (b[p + 2] << 8);
      d->topo = b[p + 3] | (b[p + 4] << 8);
      d->larg = b[p + 5] | (b[p + 6] << 8);
      d->alt  = b[p + 7] | (b[p + 8] << 8);
      if (campos & 0x80) dados += (size_t)3 << ((campos & 7) + 1);  // paleta local
      if (dados >= n) break;
      dados = pularSubBlocos(b, n, dados + 1);    // +1: tamanho minimo do codigo
      if (!dados) break;                          // cadeia nao fechou: nao e quadro
      d->ini = p; d->fim = dados;
      d->gce = gce; d->gceN = gceN;
      d->atraso = 100;
      d->descarte = 0;
      if (gceN >= 6) {
        d->descarte = (b[gce + 3] >> 2) & 7;
        d->atraso = (b[gce + 4] | (b[gce + 5] << 8)) * 10;
        // ATRASO DE 0 OU 1 CENTESIMO VIRA 100 ms. E o que todo navegador faz
        // com os GIF antigos que gravavam 0; sem isto um GIF "de 0 ms"
        // tentaria trocar de quadro a cada volta do laco.
        if (d->atraso < 20) d->atraso = 100;
      }
      gce = 0; gceN = 0;
      achados++;
      p = dados;
    }
  }
  return achados;
}

int gif_animado(const char *caminho) {
  GifQuadro q[2];
  size_t n = 0;
  unsigned char *b = lerTudo(caminho, &n);
  int r;
  if (!b) return 0;
  // DOIS JA RESPONDEM A PERGUNTA. Varrer um GIF de duzentos quadros ate o fim
  // so para dizer "sim" seria trabalho jogado fora.
  r = gif_mapear(b, n, q, 2) > 1;
  free(b);
  return r;
}


// ---------------------------------------------------------------- decodificador
//
// POR QUE NATIVO (#84, 1.4.7). Ate a 1.4.6 cada quadro era remontado como um
// GIF de um quadro so e entregue ao NAVEGADOR para decodificar (Worker com
// createImageBitmap, ou <img>): copia para o Worker, decode assincrono,
// composicao e reducao no OffscreenCanvas, ImageBitmap de volta ao fio
// principal, texImage2D dele. Na AU7000 do rawldon (Tizen 6, 2 GB), da 1.4.1
// a 1.4.6: "deu a volta nos 198 quadros em 27604 ms", ~7 quadros por segundo.
//
// O QUE ESTA MEDIDO E O QUE NAO ESTA (tests/gifbanco-tizen.sh, Chrome do Mac,
// GIFs de 35x512x512, 75x500x375 e 198x250x250):
//   - com o passo de 67 ms de quem chamava, a 1.4.6 dava a volta em 2,9 s,
//     6,2 s e 16,2 s para 1,05 s, 3 s e 9,9 s nominais. O passo prendia o GIF
//     a 15 fps e ainda perdia fase a cada troca.
//   - sem o passo, a mesma 1.4.6 acompanha o nominal NO MAC: o Worker entrega
//     um quadro em 0,6 a 2 ms. O decode deste arquivo (wasm) leva 0,9 a 3,8 ms.
//     Ou seja: no Mac o navegador nao e mais lento que isto.
//   - NA TV nao se sabe qual pedaco do ida-e-volta custa os ~140 ms por quadro
//     (Chromium 76: OffscreenCanvas, ImageBitmap para WebGL, postMessage com o
//     fio principal ocupado). O caminho nativo tira TODOS eles do caminho, e o
//     custo que sobra e CPU pura, previsivel e medido aqui por quadro (o log
//     "decode X ms por quadro").
//
// AQUI O GIF INTEIRO E C: LZW, paleta, transparencia, entrelacamento, descarte
// e a reducao ao tamanho do card, num pthread (gif_fio_*). O fio principal so
// faz glTexSubImage2D das LINHAS QUE MUDARAM. O navegador nao entra.
//
// Escrito aqui, e nao importado: sao poucas centenas de linhas sobre o
// gif_mapear que ja existia, e o formato e fechado (GIF89a, CompuServe 1990).
// Nenhum codigo de terceiros, nenhuma licenca nova.
//
// MEMORIA: um quadro composto por vez (tela logica x 4), mais a copia da area
// que o descarte 3 restaura (so se algum quadro usa), mais a saida reduzida.
// Os quadros continuam comprimidos no proprio arquivo, que fica na memoria.

typedef struct {
  const unsigned char *b;
  size_t n, p;            // fim da faixa do quadro; proximo byte
  unsigned sub;           // bytes que faltam no sub-bloco corrente
  int fim;                // a cadeia acabou (terminador ou fim da faixa)
  uint32_t bits;
  int nbits;
} Leitor;

struct GifDec {
  unsigned char *b;
  size_t n;
  GifQuadro *q;
  int nq, telaW, telaH, saidaW, saidaH;
  int prox;               // proximo quadro a decodificar
  int anterior;           // ultimo quadro desenhado (-1 = nenhum)
  uint32_t *tela;         // RGBA composto, telaW*telaH
  uint32_t *salvo;        // copia para o descarte 3, telaW*telaH (ou NULL)
  unsigned char *idx;     // indices de um quadro, larg*alt
  size_t idxCap;
  uint32_t *saida;        // RGBA reduzido, saidaW*saidaH
  int *colX, *linY;       // onde comeca cada coluna/linha da saida na tela
  // Tabela do LZW: 4096 codigos de ate 12 bits (ver lzw()).
  uint32_t pos[4096];
  uint16_t compr[4096];
};

static int lerByte(Leitor *l) {
  if (l->fim) return -1;
  while (!l->sub) {
    if (l->p >= l->n) { l->fim = 1; return -1; }
    l->sub = l->b[l->p++];
    if (!l->sub) { l->fim = 1; return -1; }
  }
  if (l->p >= l->n) { l->fim = 1; return -1; }
  l->sub--;
  return l->b[l->p++];
}

// LZW do GIF: codigos de largura variavel (minimo+1 ate 12 bits), bit menos
// significativo primeiro, com CLEAR e END. Escreve ate `total` indices em
// `dst` e devolve quantos escreveu. Codigo fora da tabela (arquivo corrompido)
// PARA o quadro ali: o resto dele nao desenha, como no Chromium.
//
// A TABELA GUARDA ONDE CADA STRING JA ESTA NA SAIDA, e nao a cadeia de
// prefixos: toda entrada nova e "a string do codigo anterior + o primeiro
// simbolo do atual", e essa sequencia acabou de ser escrita em `dst` a partir
// de onde o anterior foi escrito. Emitir um codigo vira copiar `compr` bytes
// de tras — como no LZ77 —, em vez de andar a cadeia um byte por vez. No caso
// KwKwK (o codigo pedido e o que esta sendo criado) a copia sobrepoe o
// destino em um byte, e por isso ela e feita em ordem, byte a byte.
static size_t lzw(GifDec *d, Leitor *l, int minimo, unsigned char *dst, size_t total) {
  int clear, eoi, prox, larg, anterior = -1;
  size_t pos = 0, posAnt = 0;
  uint32_t mascara;
  if (minimo < 1 || minimo > 11) return 0;
  clear = 1 << minimo; eoi = clear + 1; prox = eoi + 1; larg = minimo + 1;
  mascara = (1u << larg) - 1;
  while (pos < total) {
    int cod;
    while (l->nbits < larg) {
      int c;
      if (l->sub && l->p < l->n) { l->sub--; c = l->b[l->p++]; }
      else if ((c = lerByte(l)) < 0) return pos;
      l->bits |= (uint32_t)c << l->nbits;
      l->nbits += 8;
    }
    cod = (int)(l->bits & mascara);
    l->bits >>= larg;
    l->nbits -= larg;
    if (cod < clear) {                        // raiz: um simbolo so
      if (anterior >= 0 && prox < 4096) {
        d->pos[prox] = (uint32_t)posAnt;
        d->compr[prox] = (uint16_t)(d->compr[anterior] + 1);
        if (++prox == (1 << larg) && larg < 12) { larg++; mascara = (1u << larg) - 1; }
      }
      d->compr[cod] = 1;
      posAnt = pos;
      dst[pos++] = (uint8_t)cod;
      anterior = cod;
      continue;
    }
    if (cod == clear) {
      prox = eoi + 1; larg = minimo + 1; mascara = (1u << larg) - 1; anterior = -1;
      continue;
    }
    if (cod == eoi || anterior < 0 || cod > prox || (cod == prox && prox >= 4096)) break;
    if (prox < 4096) {
      d->pos[prox] = (uint32_t)posAnt;
      d->compr[prox] = (uint16_t)(d->compr[anterior] + 1);
      if (++prox == (1 << larg) && larg < 12) { larg++; mascara = (1u << larg) - 1; }
    }
    { size_t n = d->compr[cod], de = d->pos[cod], k;
      if (n > total - pos) n = total - pos;
      if (de + n <= pos) memcpy(dst + pos, dst + de, n);
      else for (k = 0; k < n; k++) dst[pos + k] = dst[de + k];
      posAnt = pos;
      pos += n;
    }
    anterior = cod;
  }
  return pos;
}

// Linha real da n-esima linha decodificada de um quadro entrelacado: passadas
// de 8 em 8 (a partir de 0), de 8 em 8 (de 4), de 4 em 4 (de 2), de 2 em 2 (de 1).
static int linhaEntrelacada(int n, int alt) {
  int p1 = (alt + 7) / 8, p2 = (alt + 3) / 8, p3 = (alt + 1) / 4;
  if (n < p1) return n * 8;
  n -= p1;
  if (n < p2) return 4 + n * 8;
  n -= p2;
  if (n < p3) return 2 + n * 4;
  n -= p3;
  return 1 + n * 2;
}

static void limparArea(uint32_t *px, int W, int x0, int y0, int x1, int y1) {
  int y;
  for (y = y0; y < y1; y++) memset(px + (size_t)y * W + x0, 0, (size_t)(x1 - x0) * 4);
}
static void copiarArea(uint32_t *dst, const uint32_t *src, int W, int x0, int y0, int x1, int y1) {
  int y;
  for (y = y0; y < y1; y++)
    memcpy(dst + (size_t)y * W + x0, src + (size_t)y * W + x0, (size_t)(x1 - x0) * 4);
}

// O retangulo do quadro, cortado a tela logica. 0 quando nao sobra nada.
static int areaDe(const GifDec *d, const GifQuadro *q, int *x0, int *y0, int *x1, int *y1) {
  *x0 = q->esq < d->telaW ? q->esq : d->telaW;
  *y0 = q->topo < d->telaH ? q->topo : d->telaH;
  *x1 = q->esq + q->larg < d->telaW ? q->esq + q->larg : d->telaW;
  *y1 = q->topo + q->alt < d->telaH ? q->topo + q->alt : d->telaH;
  return *x1 > *x0 && *y1 > *y0;
}

// MEDIA DE CAIXA: cada pixel da saida e a media dos pixels da tela que ele
// cobre. A cor e a media so dos OPACOS (no GIF o alfa e 0 ou 255, e somar o
// preto dos transparentes escureceria a borda); o alfa e a fracao opaca.
// Recalcula so o retangulo [ox0,ox1) x [oy0,oy1) da saida.
static void reduzir(GifDec *d, int ox0, int oy0, int ox1, int oy1) {
  int ox, oy;
  if (d->saidaW == d->telaW && d->saidaH == d->telaH) {
    copiarArea(d->saida, d->tela, d->telaW, ox0, oy0, ox1, oy1);
    return;
  }
  for (oy = oy0; oy < oy1; oy++) {
    int sy0 = d->linY[oy], sy1 = d->linY[oy + 1];
    uint32_t *o = d->saida + (size_t)oy * d->saidaW;
    for (ox = ox0; ox < ox1; ox++) {
      int sx0 = d->colX[ox], sx1 = d->colX[ox + 1], sx, sy;
      unsigned r = 0, g = 0, bl = 0, cnt = 0, area = (unsigned)((sx1 - sx0) * (sy1 - sy0));
      for (sy = sy0; sy < sy1; sy++) {
        const uint32_t *s = d->tela + (size_t)sy * d->telaW;
        for (sx = sx0; sx < sx1; sx++) {
          uint32_t c = s[sx];
          if (c >> 24) { r += c & 0xFF; g += (c >> 8) & 0xFF; bl += (c >> 16) & 0xFF; cnt++; }
        }
      }
      if (!cnt) { o[ox] = 0; continue; }
      o[ox] = (r / cnt) | ((g / cnt) << 8) | ((bl / cnt) << 16) |
              ((cnt == area ? 255u : cnt * 255u / area) << 24);
    }
  }
}

void gif_tamanho_saida(int telaW, int telaH, int largAlvo, int *saidaW, int *saidaH) {
  int w = largAlvo, h;
  if (telaW < 1 || telaH < 1) { *saidaW = *saidaH = 0; return; }
  if (w < 1 || w > telaW) w = telaW;
  h = (int)((double)telaH * w / telaW + 0.5);
  if (h < 1) h = 1;
  if (h > telaH) h = telaH;
  *saidaW = w; *saidaH = h;
}

void gif_dec_fechar(GifDec *d) {
  if (!d) return;
  free(d->b); free(d->q); free(d->tela); free(d->salvo); free(d->idx);
  free(d->saida); free(d->colX); free(d->linY);
  free(d);
}

// Teto de quadros mapeados. Um GifQuadro sao ~70 bytes: 4096 sao 280 KB, e so
// durante a abertura (depois encolhe para os que existem).
#define NV_GIF_MAX_Q 4096

GifDec *gif_dec_abrir(unsigned char *b, size_t n, int saidaW, int saidaH) {
  GifDec *d;
  GifQuadro *q;
  int nq, telaW, telaH, i, precisaSalvo = 0;
  if (!b) return NULL;
  if (n < 14 || memcmp(b, "GIF8", 4)) { free(b); return NULL; }
  telaW = b[6] | (b[7] << 8);
  telaH = b[8] | (b[9] << 8);
  // A TELA TEM TETO: o app roda com ABORTING_MALLOC no Tizen, e um cabecalho
  // de 65535x65535 pediria 17 GB. Nenhuma capa ou avatar chega perto.
  if (telaW < 1 || telaH < 1 || (long)telaW * telaH > GIF_TELA_MAX ||
      saidaW < 1 || saidaH < 1 || saidaW > telaW || saidaH > telaH) { free(b); return NULL; }
  q = (GifQuadro *)malloc(sizeof *q * NV_GIF_MAX_Q);
  if (!q) { free(b); return NULL; }
  nq = gif_mapear(b, n, q, NV_GIF_MAX_Q);
  if (nq < 2) { free(q); free(b); return NULL; }
  { GifQuadro *menor = (GifQuadro *)realloc(q, sizeof *q * (size_t)nq); if (menor) q = menor; }
  d = (GifDec *)calloc(1, sizeof *d);
  if (!d) { free(q); free(b); return NULL; }
  d->b = b; d->n = n; d->q = q; d->nq = nq;
  d->telaW = telaW; d->telaH = telaH; d->saidaW = saidaW; d->saidaH = saidaH;
  d->anterior = -1;
  for (i = 0; i < nq; i++) if (q[i].descarte == 3) precisaSalvo = 1;
  d->tela = (uint32_t *)calloc((size_t)telaW * telaH, 4);
  d->salvo = precisaSalvo ? (uint32_t *)calloc((size_t)telaW * telaH, 4) : NULL;
  d->saida = (uint32_t *)calloc((size_t)saidaW * saidaH, 4);
  d->colX = (int *)malloc(sizeof(int) * (size_t)(saidaW + 1));
  d->linY = (int *)malloc(sizeof(int) * (size_t)(saidaH + 1));
  if (!d->tela || !d->saida || !d->colX || !d->linY || (precisaSalvo && !d->salvo)) {
    gif_dec_fechar(d);
    return NULL;
  }
  for (i = 0; i <= saidaW; i++) d->colX[i] = (int)((long)i * telaW / saidaW);
  for (i = 0; i <= saidaH; i++) d->linY[i] = (int)((long)i * telaH / saidaH);
  return d;
}

int gif_dec_quadros(const GifDec *d) { return d ? d->nq : 0; }
void gif_dec_tela(const GifDec *d, int *w, int *h) { *w = d ? d->telaW : 0; *h = d ? d->telaH : 0; }
int gif_dec_atraso(const GifDec *d, int i) { return d && i >= 0 && i < d->nq ? d->q[i].atraso : 100; }
const unsigned char *gif_dec_saida(const GifDec *d) { return d ? (const unsigned char *)d->saida : NULL; }
const unsigned char *gif_dec_composta(const GifDec *d) { return d ? (const unsigned char *)d->tela : NULL; }

int gif_dec_proximo(GifDec *d, int *y0s, int *y1s) {
  const GifQuadro *q;
  const unsigned char *b, *cores;
  size_t p, total, feitos = 0;
  uint32_t pal[256];
  int i, k, palN = 0, transp = -1, entrel, larg, alt, campos;
  int x0, y0, x1, y1, dx0 = 0, dy0 = 0, dx1 = 0, dy1 = 0, tem;
  if (!d || !y0s || !y1s) return -1;
  i = d->prox;
  q = &d->q[i];
  b = d->b;
  tem = areaDe(d, q, &x0, &y0, &x1, &y1);

  // O QUE MUDA NA TELA: a area deste quadro mais a que o descarte do anterior
  // mexe. O quadro 0 (tambem na volta do laco) comeca da tela limpa.
  if (i == 0 || d->anterior < 0) {
    memset(d->tela, 0, (size_t)d->telaW * d->telaH * 4);
    dx1 = d->telaW; dy1 = d->telaH;
  } else {
    const GifQuadro *a = &d->q[d->anterior];
    int ax0, ay0, ax1, ay1;
    if (tem) { dx0 = x0; dy0 = y0; dx1 = x1; dy1 = y1; }
    if ((a->descarte == 2 || (a->descarte == 3 && d->salvo)) &&
        areaDe(d, a, &ax0, &ay0, &ax1, &ay1)) {
      if (a->descarte == 2) limparArea(d->tela, d->telaW, ax0, ay0, ax1, ay1);
      else copiarArea(d->tela, d->salvo, d->telaW, ax0, ay0, ax1, ay1);
      if (!tem) { dx0 = ax0; dy0 = ay0; dx1 = ax1; dy1 = ay1; }
      else {
        if (ax0 < dx0) dx0 = ax0;
        if (ay0 < dy0) dy0 = ay0;
        if (ax1 > dx1) dx1 = ax1;
        if (ay1 > dy1) dy1 = ay1;
      }
    }
  }
  if (q->descarte == 3 && d->salvo && tem) copiarArea(d->salvo, d->tela, d->telaW, x0, y0, x1, y1);

  // PALETA: a local do quadro, senao a global. Arquivo sem nenhuma, ou indice
  // alem da paleta, nao desenha. gif_mapear ja conferiu que a local cabe.
  campos = b[q->ini + 9];
  entrel = (campos & 0x40) != 0;
  p = q->ini + 10;
  if (campos & 0x80) {
    palN = 1 << ((campos & 7) + 1);
    cores = b + p;
    p += (size_t)palN * 3;
  } else {
    if (b[10] & 0x80) palN = 1 << ((b[10] & 7) + 1);
    cores = b + 13;
  }
  for (k = 0; k < palN; k++)
    pal[k] = cores[k * 3] | ((uint32_t)cores[k * 3 + 1] << 8) |
             ((uint32_t)cores[k * 3 + 2] << 16) | 0xFF000000u;
  if (q->gceN >= 7 && (b[q->gce + 3] & 1)) transp = b[q->gce + 6];

  larg = q->larg; alt = q->alt;
  total = (size_t)larg * (size_t)alt;
  if (tem && total && p < q->fim) {
    if (total > d->idxCap) {
      unsigned char *novo = (unsigned char *)realloc(d->idx, total);
      if (novo) { d->idx = novo; d->idxCap = total; }
    }
    if (total <= d->idxCap) {
      Leitor l;
      memset(&l, 0, sizeof l);
      l.b = b; l.n = q->fim; l.p = p + 1;
      feitos = lzw(d, &l, b[p], d->idx, total);
    }
  }
  // Compoe as linhas que o LZW entregou (a ultima pode vir pela metade).
  if (feitos) {
    int n, linhas = (int)((feitos + (size_t)larg - 1) / (size_t)larg);
    for (n = 0; n < linhas; n++) {
      int y = q->topo + (entrel ? linhaEntrelacada(n, alt) : n), x, xmax = larg;
      const unsigned char *s = d->idx + (size_t)n * larg;
      uint32_t *o;
      if (y < y0 || y >= y1) continue;
      o = d->tela + (size_t)y * d->telaW;
      if ((size_t)(n + 1) * larg > feitos) xmax = (int)(feitos - (size_t)n * larg);
      if (q->esq + xmax > x1) xmax = x1 - q->esq;
      for (x = 0; x < xmax; x++) {
        int c = s[x];
        if (c == transp || c >= palN) continue;
        o[q->esq + x] = pal[c];
      }
    }
  }
  d->anterior = i;
  d->prox = (i + 1) % d->nq;

  // Linhas (e colunas) da SAIDA que a area suja toca.
  if (dx1 <= dx0 || dy1 <= dy0) { *y0s = *y1s = 0; return i; }
  { int oy0 = 0, oy1 = d->saidaH, ox0 = 0, ox1 = d->saidaW;
    while (oy0 < d->saidaH && d->linY[oy0 + 1] <= dy0) oy0++;
    while (oy1 > oy0 && d->linY[oy1 - 1] >= dy1) oy1--;
    while (ox0 < d->saidaW && d->colX[ox0 + 1] <= dx0) ox0++;
    while (ox1 > ox0 && d->colX[ox1 - 1] >= dx1) ox1--;
    reduzir(d, ox0, oy0, ox1, oy1);
    *y0s = oy0; *y1s = oy1; }
  return i;
}

// ---------------------------------------------------------------- fio de decode
//
// UM PTHREAD POR GIF ANIMANDO (e so um GIF anima por vez: o do foco). Ele
// decodifica GIF_FILA quadros a frente e dorme quando a fila enche; o fio
// principal pega uma faixa pronta sem esperar nada. Cada posicao da fila tem
// o proprio buffer, e a posse passa por um atomico (`cheio`): 0 = do fio de
// decode, 1 = de quem consome. Nenhuma trava, nenhum Atomics.wait no fio
// principal (no Tizen isso congela a pagina).
//
// QUEM LIBERA E QUEM SAI POR ULTIMO (`refs`): gif_fio_fechar nao espera o fio
// terminar — so avisa. O fio termina o quadro que estiver fazendo e solta tudo.

struct GifFio {
  GifDec *dec;
  int saidaW, saidaH, nq, nominal;
  unsigned char *faixa[GIF_FILA];
  int quadro[GIF_FILA], atraso[GIF_FILA], y0[GIF_FILA], y1[GIF_FILA];
  atomic_int cheio[GIF_FILA];
  atomic_int parar;
  atomic_int refs;
  int leit;                       // so o consumidor mexe
  atomic_int medQuadros;
  atomic_llong medUs;
};

static double agoraMs(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec * 1000.0 + t.tv_nsec / 1e6;
}

static void soltarFio(GifFio *f) {
  int i;
  if (atomic_fetch_sub(&f->refs, 1) != 1) return;
  gif_dec_fechar(f->dec);
  for (i = 0; i < GIF_FILA; i++) free(f->faixa[i]);
  free(f);
}

static void *fioDecode(void *arg) {
  GifFio *f = (GifFio *)arg;
  int escr = 0;
  size_t linha = (size_t)f->saidaW * 4;
  while (!atomic_load(&f->parar)) {
    int y0, y1, k;
    double t0;
    if (atomic_load_explicit(&f->cheio[escr], memory_order_acquire)) {
      // Fila cheia: o quadro mais proximo so vence daqui a um atraso inteiro
      // (20 ms no minimo), entao 4 ms de cochilo nao atrasam nada.
      struct timespec z = { 0, 4 * 1000 * 1000 };
      nanosleep(&z, NULL);
      continue;
    }
    t0 = agoraMs();
    k = gif_dec_proximo(f->dec, &y0, &y1);
    if (k < 0) break;
    if (y1 > y0)
      memcpy(f->faixa[escr], gif_dec_saida(f->dec) + (size_t)y0 * linha, (size_t)(y1 - y0) * linha);
    f->quadro[escr] = k;
    f->atraso[escr] = gif_dec_atraso(f->dec, k);
    f->y0[escr] = y0;
    f->y1[escr] = y1;
    atomic_fetch_add(&f->medQuadros, 1);
    atomic_fetch_add(&f->medUs, (long long)((agoraMs() - t0) * 1000.0));
    atomic_store_explicit(&f->cheio[escr], 1, memory_order_release);
    escr = (escr + 1) % GIF_FILA;
  }
  soltarFio(f);
  return NULL;
}

GifFio *gif_fio_abrir(unsigned char *b, size_t n, int saidaW, int saidaH) {
  GifFio *f;
  pthread_t t;
  int i;
  GifDec *d = gif_dec_abrir(b, n, saidaW, saidaH);
  if (!d) return NULL;
  f = (GifFio *)calloc(1, sizeof *f);
  if (!f) { gif_dec_fechar(d); return NULL; }
  f->dec = d;
  f->saidaW = saidaW; f->saidaH = saidaH; f->nq = gif_dec_quadros(d);
  for (i = 0; i < f->nq; i++) f->nominal += gif_dec_atraso(d, i);
  atomic_init(&f->refs, 2);
  atomic_init(&f->parar, 0);
  for (i = 0; i < GIF_FILA; i++) {
    atomic_init(&f->cheio[i], 0);
    f->faixa[i] = (unsigned char *)malloc((size_t)saidaW * saidaH * 4);
    if (!f->faixa[i]) break;
  }
  if (i < GIF_FILA || pthread_create(&t, NULL, fioDecode, f) != 0) {
    atomic_store(&f->refs, 1);
    soltarFio(f);
    return NULL;
  }
  pthread_detach(t);
  return f;
}

void gif_fio_tamanho(const GifFio *f, int *saidaW, int *saidaH, int *quadros) {
  if (saidaW) *saidaW = f ? f->saidaW : 0;
  if (saidaH) *saidaH = f ? f->saidaH : 0;
  if (quadros) *quadros = f ? f->nq : 0;
}

int gif_fio_nominal(const GifFio *f) { return f ? f->nominal : 0; }

int gif_fio_pegar(GifFio *f, GifFaixa *s) {
  int i;
  if (!f || !s) return 0;
  i = f->leit;
  if (!atomic_load_explicit(&f->cheio[i], memory_order_acquire)) return 0;
  s->quadro = f->quadro[i];
  s->atraso = f->atraso[i];
  s->y0 = f->y0[i];
  s->y1 = f->y1[i];
  s->px = f->faixa[i];
  return 1;
}

void gif_fio_devolver(GifFio *f) {
  if (!f) return;
  atomic_store_explicit(&f->cheio[f->leit], 0, memory_order_release);
  f->leit = (f->leit + 1) % GIF_FILA;
}

void gif_fio_fechar(GifFio *f) {
  if (!f) return;
  atomic_store(&f->parar, 1);
  soltarFio(f);
}

void gif_fio_medida(const GifFio *f, int *quadros, double *ms) {
  if (quadros) *quadros = f ? atomic_load(&((GifFio *)f)->medQuadros) : 0;
  if (ms) *ms = f ? (double)atomic_load(&((GifFio *)f)->medUs) / 1000.0 : 0.0;
}

// ---------------------------------------------------------------- animacao
#ifdef __EMSCRIPTEN__

// QUEM CONTA O TEMPO E O APP, E NAO O NAVEGADOR (#49): o relogio e daqui, e o
// quadro so troca quando o `atraso` do proprio GIF vence. Desde a 1.4.7 quem
// decodifica tambem e o app (ver "por que nativo" acima).
//
// A ARTE NAO FREIA MAIS O GIF. A 1.4.6 segurava a troca em >= 400 ms enquanto
// havia arte na fila do cache nas TVs < 4 GB, porque cada quadro era um decode
// do NAVEGADOR disputando o fio principal e o Worker de imagem com as artes
// (FPS da tela de perfis de 32-36 para 21-24 com o GIF animando). Agora o
// decode e um pthread do app e o fio principal so faz o glTexSubImage2D das
// linhas que mudaram. Na bancada (tests/gifbanco-tizen.sh, Chrome do Mac) o
// fio principal gasta 0,09 a 0,22 ms por quadro do GIF, sem nenhum quadro de
// tela acima de 50 ms por causa dele; a regra saiu com a causa dela.
// NAO MEDIDO: o que o pthread de decode (1 a 4 ms por quadro no Mac, varias
// vezes isso na CPU da TV) tira das artes nos outros nucleos. Se a TV mostrar
// arte atrasando com GIF animando, e aqui que se volta.

static GLuint tex;
static char   preso[512];
static int    texW, texH;
static GifFio *fio;
static int    fioW, fioH, fioQ, telaW, telaH, nominal;
static int    mostrou;            // a textura ja tem um quadro deste GIF
static double proxTroca;          // instante (ms) em que o quadro na tela vence
// A VOLTA COMPLETA, uma linha de log so: e o que diz se o GIF anda no ritmo
// dele (o #84 e exatamente essa linha, "198 quadros em 27604 ms").
static double inicioVolta;
static int    contouVolta;
// O GIF preso passou do orcamento: gif_textura devolve 0 sem reler o arquivo
// ate o caminho mudar (ou gif_parar/gif_ocioso soltar).
static int    recusado;
// Ultima vez que alguem pediu gif_textura (ms). E o que gif_ocioso olha.
static double ultimoUso;
#ifndef NV_GIF_OCIOSO_MS
#define NV_GIF_OCIOSO_MS 1500
#endif
// Atraso maior que isto (o fio de decode nao acompanhou o GIF) nao se paga
// correndo atras: o relogio realinha no agora.
#define NV_GIF_REALINHA_MS 250.0

static void soltarFioAtual(void) {
  if (fio) gif_fio_fechar(fio);
  fio = NULL;
  mostrou = 0;
}

// Abre o fio de decode para `caminho` num card de `largAlvo`. Deixa `fio`
// NULL quando o GIF nao anima (e `recusado` quando e pelo orcamento).
static void abrir(const char *caminho, int largAlvo) {
  size_t n = 0, custo, orc;
  unsigned char *b = lerTudo(caminho, &n);
  GifQuadro q0[2];
  if (!b) return;
  if (gif_mapear(b, n, q0, 2) < 2) { free(b); return; }
  telaW = b[6] | (b[7] << 8);
  telaH = b[8] | (b[9] << 8);
  gif_tamanho_saida(telaW, telaH, largAlvo, &fioW, &fioH);
  fio = gif_fio_abrir(b, n, fioW, fioH);      // dono de `b` daqui em diante
  if (!fio) {
    printf("[gif] %dx%d: decodificador nao abriu (tela acima de %d px ou sem memoria): fica a foto\n",
           telaW, telaH, GIF_TELA_MAX);
    fflush(stdout);
    recusado = 1;
    return;
  }
  gif_fio_tamanho(fio, NULL, NULL, &fioQ);
  // O ORCAMENTO (1.4.6) continua valendo: recusado, fica a foto parada.
  custo = gif_custo(fioQ, telaW, telaH);
  orc = orcamento();
  if (orc != GIF_SEM_TETO && custo > orc) {
    printf("[gif] %d quadros %dx%d = %u MB por volta, acima do orcamento de %u MB: fica o primeiro quadro\n",
           fioQ, telaW, telaH, (unsigned)(custo / (1024 * 1024)), (unsigned)(orc / (1024 * 1024)));
    fflush(stdout);
    soltarFioAtual();
    recusado = 1;
    return;
  }
  nominal = gif_fio_nominal(fio);
  texW = texH = 0;
  proxTroca = 0.0;
  inicioVolta = 0.0;
  contouVolta = 0;
  printf("[gif] %d quadros %dx%d -> %dx%d, %d ms por volta: decode nativo em fio proprio\n",
         fioQ, telaW, telaH, fioW, fioH, nominal);
  fflush(stdout);
}

static void subir(const GifFaixa *f) {
  glBindTexture(GL_TEXTURE_2D, tex);
  if (texW != fioW || texH != fioH) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fioW, fioH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    texW = fioW; texH = fioH;
  }
  // SO AS LINHAS QUE MUDARAM. Linhas inteiras sao contiguas no buffer, entao
  // uma faixa e um glTexSubImage2D so (WebGL 1 nao tem UNPACK_ROW_LENGTH).
  if (f->y1 > f->y0)
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, f->y0, fioW, f->y1 - f->y0,
                    GL_RGBA, GL_UNSIGNED_BYTE, f->px);
}

GLuint gif_textura(const char *caminho, int largAlvo) {
  double agora;
  int k;
  if (!caminho || !caminho[0] || largAlvo < 8) return 0;
  agora = emscripten_get_now();
  ultimoUso = agora;
  if (!orcamento()) return 0;

  if (strcmp(preso, caminho)) {
    soltarFioAtual();
    recusado = 0;
    snprintf(preso, sizeof preso, "%s", caminho);
    abrir(caminho, largAlvo);
  } else if (fio && largAlvo > fioW + fioW / 4 && fioW < telaW) {
    // O CARD CRESCEU (o foco amplia o avatar): a saida pequena ficaria
    // borrada. Reabre maior; ate o primeiro quadro novo quem chama segura a
    // textura que ja tem.
    soltarFioAtual();
    abrir(caminho, largAlvo);
  }
  if (recusado || !fio) return 0;
  if (!tex) {
    glGenTextures(1, &tex);
    if (!tex) return 0;
  }

  // Sobe o que venceu. Mais de uma faixa por chamada quando quem desenha
  // atrasou (TV a 20 fps, GIF a 30): as faixas somam, nenhuma pode ser pulada.
  for (k = 0; k < GIF_FILA; k++) {
    GifFaixa f;
    if (mostrou && agora < proxTroca) break;
    if (!gif_fio_pegar(fio, &f)) break;
    subir(&f);
    gif_fio_devolver(fio);
    if (!mostrou) {
      mostrou = 1;
      proxTroca = agora + f.atraso;
      inicioVolta = agora;
      continue;
    }
    // Relogio ancorado no vencimento ANTERIOR, nao em `agora`: chegar 20 ms
    // atrasado num quadro nao empurra todos os seguintes.
    proxTroca += f.atraso;
    if (f.quadro == 0 && !contouVolta) {
      int nq = 0; double ms = 0.0;
      contouVolta = 1;
      gif_fio_medida(fio, &nq, &ms);
      printf("[gif] deu a volta nos %d quadros em %.0f ms (nominal %d ms; decode %.1f ms por quadro)\n",
             fioQ, agora - inicioVolta, nominal, nq ? ms / nq : 0.0);
      fflush(stdout);
    }
  }
  if (mostrou && agora - proxTroca > NV_GIF_REALINHA_MS) proxTroca = agora;
  return mostrou ? tex : 0;
}

void gif_parar(void) {
  if (!preso[0]) return;
  soltarFioAtual();
  preso[0] = 0;
  texW = texH = 0;
  proxTroca = 0.0;
  inicioVolta = 0.0;
  contouVolta = 0;
  recusado = 0;
}

// A textura `tex` NAO sai: e pequena (o tamanho do card) e quem chama guarda o
// nome dela entre amostras; apaga-la deixaria esse nome apontando para nada.
void gif_ocioso(void) {
  double agora;
  if (!preso[0]) return;
  agora = emscripten_get_now();
  if (agora - ultimoUso < NV_GIF_OCIOSO_MS) return;
  if (!recusado) {
    printf("[gif] ninguem desenha o GIF ha %.0f ms: animacao solta\n", agora - ultimoUso);
    fflush(stdout);
  }
  gif_parar();
}

#else   /* webOS e Mac: sem animacao. Ver o cabecalho de gif.h. */

GLuint gif_textura(const char *caminho, int largAlvo) {
  (void)caminho; (void)largAlvo;
  return 0;
}
void gif_parar(void) {}
void gif_ocioso(void) {}

#endif
