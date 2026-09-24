// O DETECTOR DE GIF ANIMADO, SEM DECODIFICAR PIXEL E SEM ARQUIVO NO REPOSITORIO.
//
// gif_animado() responde "este arquivo tem mais de um quadro?" caminhando pela
// estrutura de blocos do GIF, que e toda prefixada por tamanho. E a pergunta
// que separa a capa de colecao que precisa animar (#29) da que so precisa ser
// desenhada: no Tizen a resposta "sim" manda a capa para gif_textura(), e no
// webOS ela nao muda nada porque la nao ha decodificador de animacao.
//
// POR QUE OS GIFS SAO MONTADOS AQUI, BYTE A BYTE, e nao gravados em
// tests/fixtures: um GIF binario no repositorio nao diz o que esta sendo
// testado. Escrito assim, cada caso e legivel — da para ver que o quadro tem
// paleta local olhando o bit, e da para cortar o arquivo num ponto ESCOLHIDO em
// vez de num offset magico.
//
// O QUE ESTE TESTE PROVA:
//   1. um quadro nao e animacao; dois quadros sao;
//   2. extensao de controle grafico (o bloco que carrega o atraso entre
//      quadros, e que na pratica todo GIF animado tem) nao confunde a contagem;
//   3. paleta local — que muda o TAMANHO do descritor de imagem — tambem nao;
//   4. arquivo truncado em QUALQUER ponto termina, nao le fora do buffer (isto
//      so tem valor sob ASan: SANITIZE=1 bash tests/gif.sh) e nao inventa
//      animacao antes de ver o segundo quadro;
//   5. arquivo que nao e GIF, arquivo vazio e arquivo que nao existe devolvem
//      0 em vez de tentar interpretar o lixo;
//   6. o DECODIFICADOR (1.4.7) compoe cada quadro exatamente como o modelo do
//      GIF89a manda (LZW, paletas, entrelacamento, transparencia, descartes),
//      reduz por media de caixa, e aguenta arquivo cortado ou corrompido;
//   7. o FIO DE DECODE entrega faixas que, somadas em ordem, reconstroem a
//      saida, e fechar nao espera nem vaza.
//
//   bash tests/gif.sh
//   SANITIZE=1 bash tests/gif.sh
#include "../src/gif.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define CAMINHO "/tmp/nuvio-gif-teste.gif"

// --- montador ---------------------------------------------------------------
static unsigned char buf[8192];
static size_t nbuf;
// Para onde b1 escreve: `buf`, ou o buffer grande dos testes com pixel.
static unsigned char *gBuf = buf;
static size_t gCap = sizeof buf;
static int    nQuadros;           // quantos quadro() ja entraram no buffer
static size_t offSegundoQuadro;   // onde o separador 0x2C do 2o quadro caiu

static void b1(unsigned v) { assert(nbuf < gCap); gBuf[nbuf++] = (unsigned char)v; }
static void b2(unsigned v) { b1(v & 0xFF); b1((v >> 8) & 0xFF); }
static void bn(const char *s, size_t n) { for (size_t i = 0; i < n; i++) b1((unsigned char)s[i]); }

// Cabecalho + descritor de tela logica. `paleta` liga a paleta GLOBAL, que sao
// 3 * 2^(N+1) bytes logo depois do descritor — com N=0, seis bytes. O detector
// tem de pula-los para achar o primeiro bloco.
static void cabecalho(const char *assinatura, int paleta) {
  nbuf = 0; nQuadros = 0; offSegundoQuadro = 0;
  bn(assinatura, 6);
  b2(2); b2(2);                    // 2x2 pixels; o tamanho nao importa aqui
  b1(paleta ? 0x80 : 0x00);        // bit 7: ha paleta global, N = 0
  b1(0);                           // cor de fundo
  b1(0);                           // proporcao do pixel
  if (paleta) for (int i = 0; i < 6; i++) b1(0);
}

// Uma cadeia de sub-blocos com UM sub-bloco de `n` bytes, mais o terminador.
static void subBlocos(unsigned n) {
  b1(n);
  for (unsigned i = 0; i < n; i++) b1(0x00);
  b1(0x00);                        // tamanho zero fecha a cadeia
}

// Extensao de controle grafico: e ela que carrega o atraso entre quadros, e
// vem ANTES de cada imagem num GIF animado de verdade.
static void controleGrafico(void) {
  b1(0x21); b1(0xF9);
  b1(4); b1(0x00); b2(10); b1(0x00);   // sub-bloco de 4 bytes
  b1(0x00);
}

// Extensao de aplicacao NETSCAPE2.0 (o bloco de repeticao). Serve para provar a
// cadeia de sub-blocos com MAIS DE UM sub-bloco.
static void extensaoNetscape(void) {
  b1(0x21); b1(0xFF);
  b1(11); bn("NETSCAPE2.0", 11);
  b1(3); b1(1); b2(0);
  b1(0x00);
}

// Um quadro. `paletaLocal` liga a paleta LOCAL, que muda o tamanho do bloco:
// quem contar 9 bytes fixos de descritor e seguir em frente cai no meio da
// paleta e para de entender o arquivo.
static void quadro(int paletaLocal) {
  // Onde o SEGUNDO quadro comeca: o teste de truncagem usa este offset para
  // saber ate onde o arquivo ainda nao pode ser chamado de animado.
  if (nQuadros == 1) offSegundoQuadro = nbuf;
  nQuadros++;
  b1(0x2C);
  b2(0); b2(0); b2(2); b2(2);          // esquerda, topo, largura, altura
  b1(paletaLocal ? 0x80 : 0x00);       // bit 7: ha paleta local, N = 0
  if (paletaLocal) for (int i = 0; i < 6; i++) b1(0);
  b1(2);                               // tamanho minimo do codigo LZW
  subBlocos(3);                        // dados comprimidos, que ninguem le
}

static void fim(void) { b1(0x3B); }

static void gravarBruto(const unsigned char *p, size_t n) {
  FILE *f = fopen(CAMINHO, "wb");
  size_t esc;
  int fechou;
  assert(f);
  esc = n ? fwrite(p, 1, n, f) : 0;
  fechou = fclose(f);
  assert(esc == n && fechou == 0);
}

// Os primeiros `n` bytes do que foi montado. `n` igual a nbuf grava tudo, e
// zero grava um arquivo VAZIO — que e um caso de teste, nao um atalho.
static void gravar(size_t n) {
  assert(n <= nbuf);
  gravarBruto(buf, n);
}

static void gravarTudo(void) { gravarBruto(buf, nbuf); }

// --- decodificador (1.4.7) ---------------------------------------------------
//
// OS PIXELS AGORA SAO DAQUI, entao o teste precisa de GIFs com pixel de
// verdade. O codificador LZW abaixo e o do formato, sem atalho: dicionario que
// cresce, largura de codigo que sobe de minimo+1 ate 12 bits, CLEAR quando a
// tabela enche. E os quadros tem padroes que forcam os casos dificeis do
// decodificador: a sequencia KwKwK (corrida longa de um indice so), a subida
// de largura, a tabela cheia (ruido de 256 cores), paleta local,
// entrelacamento, transparencia e os descartes 1, 2 e 3.
//
// O RESULTADO E CONFERIDO CONTRA UM MODELO, e nao contra outro decodificador:
// o teste sabe os indices que escreveu, compoe a tela esperada pelas regras do
// GIF89a e compara pixel a pixel com gif_dec_composta.

static uint16_t filho[4096][256];   // codigo de (prefixo, simbolo), 0 = nao ha

static unsigned char lzwSaida[1 << 20];
static size_t lzwN;
static uint32_t lzwBits;
static int lzwNb;
static void lzwEmitir(int cod, int larg) {
  lzwBits |= (uint32_t)cod << lzwNb;
  lzwNb += larg;
  while (lzwNb >= 8) { assert(lzwN < sizeof lzwSaida); lzwSaida[lzwN++] = (unsigned char)(lzwBits & 0xFF); lzwBits >>= 8; lzwNb -= 8; }
}
// Codifica `n` indices e escreve no buf: tamanho minimo + sub-blocos.
static void lzwBloco(const unsigned char *idx, size_t n, int minimo) {
  int clear = 1 << minimo, eoi = clear + 1, prox = eoi + 1, larg = minimo + 1, atual;
  size_t i, p;
  lzwN = 0; lzwBits = 0; lzwNb = 0;
  memset(filho, 0, sizeof filho);
  lzwEmitir(clear, larg);
  atual = idx[0];
  for (i = 1; i < n; i++) {
    int s = idx[i];
    if (filho[atual][s]) { atual = filho[atual][s]; continue; }
    lzwEmitir(atual, larg);
    if (prox < 4096) {
      filho[atual][s] = (uint16_t)prox++;
      if (prox > (1 << larg) && larg < 12) larg++;
    } else {
      lzwEmitir(clear, larg);
      memset(filho, 0, sizeof filho);
      prox = eoi + 1; larg = minimo + 1;
    }
    atual = s;
  }
  lzwEmitir(atual, larg);
  // O decodificador cria uma entrada ao ler o ultimo codigo; se ela chega a
  // 2^larg, ele ja espera o END um bit mais largo.
  if (prox < 4096 && prox + 1 > (1 << larg) && larg < 12) larg++;
  lzwEmitir(eoi, larg);
  if (lzwNb) { lzwSaida[lzwN++] = (unsigned char)(lzwBits & 0xFF); lzwBits = 0; lzwNb = 0; }
  b1((unsigned)minimo);
  for (p = 0; p < lzwN; p += 255) {
    size_t k = lzwN - p < 255 ? lzwN - p : 255;
    b1((unsigned)k);
    for (i = 0; i < k; i++) b1(lzwSaida[p + i]);
  }
  b1(0);
}

static unsigned char grande[1 << 21];   // buffer dos GIFs com pixel
// O montador de cima escreve em `buf` (8 KB); os GIFs daqui sao maiores.
#define USAR_GRANDE() do { gBuf = grande; gCap = sizeof grande; } while (0)

typedef struct { int esq, topo, larg, alt, descarte, transp, entrel, palLocal; } Q;
#define MAXW 320
#define MAXH 200
static unsigned char idxDe[8][MAXW * MAXH];      // indices por quadro (ordem de linha real)
static uint32_t palG[256], palL[256];

static uint32_t rgba(int r, int g, int b) { return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | 0xFF000000u; }

// Monta um GIF de `nq` quadros em `grande`. Devolve o tamanho.
static size_t montarGif(int W, int H, int bitsPal, const Q *q, int nq) {
  int k, i, nPal = 1 << bitsPal;
  nbuf = 0;
  bn("GIF89a", 6); b2((unsigned)W); b2((unsigned)H);
  b1(0x80 | (unsigned)(bitsPal - 1)); b1(0); b1(0);
  for (i = 0; i < nPal; i++) { b1(palG[i] & 0xFF); b1((palG[i] >> 8) & 0xFF); b1((palG[i] >> 16) & 0xFF); }
  extensaoNetscape();
  for (k = 0; k < nq; k++) {
    unsigned char seq[MAXW * MAXH];
    int n = 0, y, x, linhas = q[k].alt;
    b1(0x21); b1(0xF9); b1(4);
    b1((unsigned)((q[k].descarte << 2) | (q[k].transp >= 0 ? 1 : 0)));
    b2(4); b1(q[k].transp >= 0 ? (unsigned)q[k].transp : 0); b1(0);
    b1(0x2C); b2((unsigned)q[k].esq); b2((unsigned)q[k].topo); b2((unsigned)q[k].larg); b2((unsigned)q[k].alt);
    b1((q[k].palLocal ? 0x80 | (unsigned)(bitsPal - 1) : 0) | (q[k].entrel ? 0x40 : 0));
    if (q[k].palLocal)
      for (i = 0; i < nPal; i++) { b1(palL[i] & 0xFF); b1((palL[i] >> 8) & 0xFF); b1((palL[i] >> 16) & 0xFF); }
    // Entrelacado: as linhas vao ao arquivo na ordem das quatro passadas.
    { static const int ini[4] = { 0, 4, 2, 1 }, passo[4] = { 8, 8, 4, 2 };
      int pas;
      if (q[k].entrel) {
        for (pas = 0; pas < 4; pas++)
          for (y = ini[pas]; y < linhas; y += passo[pas])
            for (x = 0; x < q[k].larg; x++) seq[n++] = idxDe[k][y * q[k].larg + x];
      } else {
        for (y = 0; y < linhas; y++)
          for (x = 0; x < q[k].larg; x++) seq[n++] = idxDe[k][y * q[k].larg + x];
      } }
    lzwBloco(seq, (size_t)n, bitsPal < 2 ? 2 : bitsPal);
  }
  b1(0x3B);
  return nbuf;
}

// O MODELO: a tela esperada depois de cada quadro, pelas regras do GIF89a.
static void modelo(int W, int H, const Q *q, int nq, int ate, uint32_t *tela) {
  static uint32_t salvo[MAXW * MAXH];
  int k, x, y;
  memset(tela, 0, (size_t)W * H * 4);
  for (k = 0; k <= ate && k < nq; k++) {
    if (k > 0) {
      const Q *a = &q[k - 1];
      if (a->descarte == 2)
        for (y = a->topo; y < a->topo + a->alt && y < H; y++)
          for (x = a->esq; x < a->esq + a->larg && x < W; x++) tela[y * W + x] = 0;
      if (a->descarte == 3)
        for (y = a->topo; y < a->topo + a->alt && y < H; y++)
          for (x = a->esq; x < a->esq + a->larg && x < W; x++) tela[y * W + x] = salvo[y * W + x];
    }
    if (q[k].descarte == 3) memcpy(salvo, tela, (size_t)W * H * 4);
    for (y = 0; y < q[k].alt; y++)
      for (x = 0; x < q[k].larg; x++) {
        int c = idxDe[k][y * q[k].larg + x], X = q[k].esq + x, Y = q[k].topo + y;
        if (X >= W || Y >= H || c == q[k].transp) continue;
        tela[Y * W + X] = q[k].palLocal ? palL[c] : palG[c];
      }
  }
}

static uint32_t esperado[MAXW * MAXH];

static void decodificador(void) {
  enum { W = 64, H = 40 };
  Q q[6];
  size_t n;
  int k, i, x, y;
  unsigned rnd = 12345;
  USAR_GRANDE();
  for (i = 0; i < 256; i++) { palG[i] = rgba(i, 255 - i, (i * 7) & 255); palL[i] = rgba((i * 3) & 255, i, 128); }

  // Quadro 0: tela inteira, faixas + ruido de 16 cores (codigos que crescem).
  q[0] = (Q){ 0, 0, W, H, 1, -1, 0, 0 };
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) {
    rnd = rnd * 1103515245u + 12345u;
    idxDe[0][y * W + x] = (unsigned char)(y < 10 ? 3 : ((rnd >> 16) & 15));
  }
  // Quadro 1: parcial, transparente no meio (indice 5), descarte 2.
  q[1] = (Q){ 10, 5, 20, 12, 2, 5, 0, 0 };
  for (i = 0; i < 20 * 12; i++) idxDe[1][i] = (unsigned char)((i % 20 > 5 && i % 20 < 14) ? 5 : 7 + i % 3);
  // Quadro 2: parcial, ENTRELACADO, paleta local, descarte 3.
  q[2] = (Q){ 30, 3, 25, 30, 3, -1, 1, 1 };
  for (i = 0; i < 25 * 30; i++) idxDe[2][i] = (unsigned char)(((i / 25) * 2 + (i % 25) / 8) & 15);
  // Quadro 3: corrida de um indice so (KwKwK) sobre o que o 2 restaurou.
  q[3] = (Q){ 0, 20, W, 10, 0, -1, 0, 0 };
  memset(idxDe[3], 9, W * 10);
  // Quadro 4: sai pela borda da tela (retangulo cortado), transparente 0.
  q[4] = (Q){ 50, 30, 30, 20, 1, 0, 0, 0 };
  for (i = 0; i < 30 * 20; i++) idxDe[4][i] = (unsigned char)(i % 4);
  n = montarGif(W, H, 4, q, 5);

  { GifDec *d;
    unsigned char *copia = (unsigned char *)malloc(n);
    int volta, y0, y1;
    memcpy(copia, grande, n);
    d = gif_dec_abrir(copia, n, W, H);
    assert(d && gif_dec_quadros(d) == 5);
    for (volta = 0; volta < 2; volta++)
      for (k = 0; k < 5; k++) {
        assert(gif_dec_proximo(d, &y0, &y1) == k);
        modelo(W, H, q, 5, k, esperado);
        assert(!memcmp(gif_dec_composta(d), esperado, (size_t)W * H * 4));
        // Saida do mesmo tamanho: e a propria tela.
        assert(!memcmp(gif_dec_saida(d), esperado, (size_t)W * H * 4));
        if (k == 0) assert(y0 == 0 && y1 == H);
        if (k == 1) assert(y0 == 5 && y1 == 17);           // so o retangulo dele
        if (k == 2) assert(y0 == 3 && y1 == 33);           // o dele mais o descarte 2 do 1
        if (k == 3) assert(y0 == 3 && y1 == 33);           // o dele (20-30) mais o 3 do 2 restaurado
        if (k == 4) assert(y0 == 30 && y1 == H);           // o 3 (descarte 0) nao mexe; cortado na borda
      }
    gif_dec_fechar(d);
    puts("ok  decodificador: LZW, paleta local, entrelacado, transparencia e descartes 1/2/3 batem com o modelo"); }

  // TABELA CHEIA: 320x200 de ruido em 256 cores estoura os 4096 codigos
  // varias vezes (CLEAR no meio do quadro).
  { Q r[2] = { { 0, 0, MAXW, MAXH, 0, -1, 0, 0 }, { 0, 0, MAXW, MAXH, 0, -1, 0, 0 } };
    GifDec *d;
    unsigned char *copia;
    int y0, y1;
    for (i = 0; i < MAXW * MAXH; i++) { rnd = rnd * 1103515245u + 12345u; idxDe[0][i] = (unsigned char)(rnd >> 16); idxDe[1][i] = (unsigned char)(i & 255); }
    n = montarGif(MAXW, MAXH, 8, r, 2);
    copia = (unsigned char *)malloc(n);
    memcpy(copia, grande, n);
    d = gif_dec_abrir(copia, n, MAXW, MAXH);
    assert(d);
    for (k = 0; k < 2; k++) {
      assert(gif_dec_proximo(d, &y0, &y1) == k);
      modelo(MAXW, MAXH, r, 2, k, esperado);
      assert(!memcmp(gif_dec_composta(d), esperado, (size_t)MAXW * MAXH * 4));
    }
    gif_dec_fechar(d);
    puts("ok  decodificador: 256 cores com a tabela de 4096 codigos enchendo e recomecando"); }

  // REDUCAO: 4x2 -> 2x1. Cada pixel da saida e a media de um bloco 2x2; bloco
  // meio transparente fica com alfa pela metade e a cor so dos opacos.
  { Q r[2] = { { 0, 0, 4, 2, 0, 0, 0, 0 }, { 0, 0, 1, 1, 0, -1, 0, 0 } };
    GifDec *d;
    unsigned char *copia;
    const unsigned char *o;
    int y0, y1, sw, sh;
    palG[0] = rgba(0, 0, 0); palG[1] = rgba(100, 0, 0); palG[2] = rgba(0, 200, 0); palG[3] = rgba(0, 0, 40);
    // linha 0: 1 1 | 2 0     linha 1: 1 1 | 0 3      (0 = transparente)
    { static const unsigned char px[8] = { 1, 1, 2, 0, 1, 1, 0, 3 }; memcpy(idxDe[0], px, 8); }
    idxDe[1][0] = 1;
    n = montarGif(4, 2, 2, r, 2);
    gif_tamanho_saida(4, 2, 2, &sw, &sh);
    assert(sw == 2 && sh == 1);
    gif_tamanho_saida(4, 2, 480, &sw, &sh);            // nunca amplia
    assert(sw == 4 && sh == 2);
    copia = (unsigned char *)malloc(n);
    memcpy(copia, grande, n);
    d = gif_dec_abrir(copia, n, 2, 1);
    assert(d);
    assert(gif_dec_proximo(d, &y0, &y1) == 0 && y0 == 0 && y1 == 1);
    o = gif_dec_saida(d);
    assert(o[0] == 100 && o[1] == 0 && o[2] == 0 && o[3] == 255);
    assert(o[4] == 0 && o[5] == 100 && o[6] == 20 && o[7] == 127);   // (0,200,0)+(0,0,40) em 2 de 4
    gif_dec_fechar(d);
    puts("ok  reducao por media de caixa, com alfa proporcional e cor so dos opacos"); }

  // ARQUIVO CORROMPIDO OU CORTADO EM QUALQUER PONTO: nao le fora (SANITIZE=1),
  // nao trava, e o que abre decodifica sem falhar.
  { Q r[2] = { { 0, 0, 16, 8, 2, 3, 1, 1 }, { 4, 2, 16, 8, 3, -1, 0, 0 } };
    size_t corte, inteiro;
    for (i = 0; i < 16 * 8; i++) { idxDe[0][i] = (unsigned char)(i % 5); idxDe[1][i] = (unsigned char)((i / 9) & 15); }
    inteiro = montarGif(16, 8, 4, r, 2);
    for (corte = 0; corte <= inteiro; corte++) {
      unsigned char *copia = (unsigned char *)malloc(corte ? corte : 1);
      GifDec *d;
      int y0, y1, v;
      memcpy(copia, grande, corte);
      d = gif_dec_abrir(copia, corte, 8, 4);
      if (!d) continue;                                  // abrir liberou `copia`
      for (v = 0; v < 5; v++) assert(gif_dec_proximo(d, &y0, &y1) >= 0 && y0 >= 0 && y1 <= 4);
      gif_dec_fechar(d);
    }
    // Bytes de LZW trocados por lixo: o quadro fica pela metade, sem crash.
    for (corte = 20; corte < inteiro; corte += 3) {
      unsigned char *copia = (unsigned char *)malloc(inteiro);
      GifDec *d;
      int y0, y1, v;
      memcpy(copia, grande, inteiro);
      copia[corte] ^= 0xA5;
      d = gif_dec_abrir(copia, inteiro, 16, 8);
      if (!d) continue;
      for (v = 0; v < 4; v++) assert(gif_dec_proximo(d, &y0, &y1) >= 0);
      gif_dec_fechar(d);
    }
    // Tela gigante (cabecalho mentiroso) nao chega ao malloc.
    { unsigned char *copia = (unsigned char *)malloc(inteiro);
      memcpy(copia, grande, inteiro);
      copia[6] = copia[7] = copia[8] = copia[9] = 0xFF;
      assert(gif_dec_abrir(copia, inteiro, 8, 4) == NULL); }
    assert(gif_dec_abrir(NULL, 0, 1, 1) == NULL);
    puts("ok  decodificador: cortado em qualquer ponto, LZW com lixo e tela gigante nao derrubam nada"); }
}

// O FIO: as faixas, somadas em ordem sobre o quadro anterior, reconstroem
// exatamente a saida do decodificador sincrono — em duas voltas, com o
// consumidor atrasando de proposito para a fila encher.
static void fio(void) {
  enum { W = 64, H = 40, SW = 21, SH = 13 };
  Q q[5];
  size_t n;
  int i, x, y, k;
  unsigned rnd = 777;
  static unsigned char recon[SW * SH * 4];
  USAR_GRANDE();
  for (i = 0; i < 256; i++) palG[i] = rgba(i, (i * 5) & 255, 255 - i);
  q[0] = (Q){ 0, 0, W, H, 1, -1, 0, 0 };
  for (y = 0; y < H; y++) for (x = 0; x < W; x++) { rnd = rnd * 1103515245u + 12345u; idxDe[0][y * W + x] = (unsigned char)((rnd >> 16) & 15); }
  q[1] = (Q){ 8, 30, 10, 6, 2, -1, 0, 0 };
  memset(idxDe[1], 4, 60);
  q[2] = (Q){ 40, 2, 20, 10, 3, 2, 0, 0 };
  for (i = 0; i < 200; i++) idxDe[2][i] = (unsigned char)(i % 3);
  q[3] = (Q){ 0, 0, 5, 5, 0, -1, 0, 0 };
  memset(idxDe[3], 11, 25);
  n = montarGif(W, H, 4, q, 4);

  { unsigned char *c1 = (unsigned char *)malloc(n), *c2 = (unsigned char *)malloc(n);
    GifDec *ref;
    GifFio *f;
    int passos = 0, sw, sh, nq;
    memcpy(c1, grande, n); memcpy(c2, grande, n);
    ref = gif_dec_abrir(c1, n, SW, SH);
    f = gif_fio_abrir(c2, n, SW, SH);
    assert(ref && f);
    gif_fio_tamanho(f, &sw, &sh, &nq);
    assert(sw == SW && sh == SH && nq == 4);
    assert(gif_fio_nominal(f) == 4 * 40);
    while (passos < 8) {
      GifFaixa fx;
      int y0, y1;
      struct timespec z = { 0, 2 * 1000 * 1000 };
      if (!gif_fio_pegar(f, &fx)) { nanosleep(&z, NULL); continue; }
      k = gif_dec_proximo(ref, &y0, &y1);
      assert(fx.quadro == k && fx.y0 == y0 && fx.y1 == y1 && fx.atraso == 40);
      if (fx.y1 > fx.y0) memcpy(recon + (size_t)fx.y0 * SW * 4, fx.px, (size_t)(fx.y1 - fx.y0) * SW * 4);
      gif_fio_devolver(f);
      assert(!memcmp(recon, gif_dec_saida(ref), sizeof recon));
      passos++;
      if (passos == 3) { struct timespec zz = { 0, 30 * 1000 * 1000 }; nanosleep(&zz, NULL); }
    }
    { int nq2; double ms; gif_fio_medida(f, &nq2, &ms); assert(nq2 >= 8 && ms >= 0.0); }
    gif_fio_fechar(f);
    gif_dec_fechar(ref); }

  // Fechar com a fila cheia e com o fio no meio do quadro: nao espera, nao
  // vaza, nao usa memoria solta (SANITIZE=1). Vinte vezes seguidas.
  for (i = 0; i < 20; i++) {
    unsigned char *c = (unsigned char *)malloc(n);
    GifFio *f;
    memcpy(c, grande, n);
    f = gif_fio_abrir(c, n, SW, SH);
    assert(f);
    if (i & 1) { struct timespec z = { 0, 1000 * 1000 }; nanosleep(&z, NULL); }
    gif_fio_fechar(f);
  }
  { struct timespec z = { 0, 50 * 1000 * 1000 }; nanosleep(&z, NULL); }  // os fios saem
  assert(gif_fio_abrir(NULL, 0, 1, 1) == NULL);
  puts("ok  fio de decode: faixas em ordem reconstroem a saida, fechar nao espera nem vaza");
}

// --- casos ------------------------------------------------------------------
int main(void) {
  // A) UM QUADRO NAO E ANIMACAO. E a capa parada, que e o caso comum: a maioria
  //    das pastas nao tem GIF de foco, e as que tem uma imagem so nao podem
  //    entrar no caminho de animacao — la o quadro seria recomposto a cada
  //    volta sem nada mudar na tela.
  cabecalho("GIF89a", 1);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  um quadro nao e animacao");

  // B) DOIS QUADROS SAO. O minimo que faz o detector dizer sim.
  cabecalho("GIF89a", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  dois quadros sao animacao");

  // C) GIF87a TAMBEM CONTA. O detector compara so "GIF8" de proposito: 87a e
  //    89a tem a mesma estrutura de blocos, e recusar o 87a rejeitaria arquivo
  //    valido por causa de um digito.
  cabecalho("GIF87a", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  GIF87a de dois quadros tambem e animacao");

  // D) EXTENSAO DE CONTROLE GRAFICO ANTES DE CADA QUADRO. E a forma real de um
  //    GIF animado — sem ela nao ha atraso entre quadros. A extensao NAO e
  //    quadro: contar blocos em vez de imagens daria 4 aqui.
  cabecalho("GIF89a", 1);
  extensaoNetscape();
  controleGrafico(); quadro(0);
  controleGrafico(); quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  extensao de controle grafico nao vira quadro");

  //    E o mesmo arquivo com UM quadro so continua nao sendo animacao: prova
  //    que o "sim" acima veio das imagens e nao das extensoes.
  cabecalho("GIF89a", 1);
  extensaoNetscape();
  controleGrafico(); quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  extensoes sozinhas nao fazem um quadro virar dois");

  // E) PALETA LOCAL. Ela fica ENTRE o descritor de imagem e os dados, e o
  //    tamanho dela sai dos bits do proprio descritor. Quem pular 9 bytes fixos
  //    cai dentro da paleta, le uma cor como se fosse separador de bloco e
  //    desiste — o GIF animado passaria por parado.
  cabecalho("GIF89a", 0);          // sem paleta global: so a local existe
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 1);
  puts("ok  paleta local nao desalinha a leitura dos blocos");

  // F) TRUNCADO EM QUALQUER PONTO. Um download interrompido, ou um corpo de
  //    erro do CDN gravado com o nome do arquivo, chega assim.
  //
  //    Sao tres afirmacoes numa varredura so:
  //      - TERMINA. Se o caminhamento entrasse em laco (um sub-bloco de tamanho
  //        zero que nao fechasse a cadeia, um `p` que nao avanca) o teste nao
  //        acabaria nunca, e isso e uma falha tao visivel quanto um assert.
  //      - NAO LE FORA. So vale rodando com SANITIZE=1; sem ASan a leitura
  //        invalida passaria despercebida.
  //      - NAO INVENTA. Enquanto o arquivo acaba antes do separador do segundo
  //        quadro, nao ha como haver dois quadros: a resposta tem de ser 0.
  cabecalho("GIF89a", 1);
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  controleGrafico(); quadro(1);
  fim();
  assert(offSegundoQuadro > 0 && offSegundoQuadro < nbuf);
  { size_t corte;
    size_t inteiro = nbuf;
    for (corte = 0; corte <= inteiro; corte++) {
      int r;
      gravar(corte);
      r = gif_animado(CAMINHO);
      assert(r == 0 || r == 1);
      if (corte <= offSegundoQuadro + 1) assert(r == 0);
    }
    // O arquivo inteiro, no fim da varredura, continua sendo animacao: a
    // varredura nao pode ter deixado estado para tras.
    gravarTudo();
    assert(gif_animado(CAMINHO) == 1); }
  puts("ok  truncado em qualquer ponto termina, nao le fora e nao inventa quadro");

  // G) SUB-BLOCO QUE PROMETE MAIS BYTES DO QUE O ARQUIVO TEM. Nao e truncagem
  //    acidental: e o campo de tamanho MENTINDO, que e como um arquivo
  //    corrompido derruba um leitor ingenuo. `p + len > n` e a guarda.
  cabecalho("GIF89a", 1);
  b1(0x2C);
  b2(0); b2(0); b2(2); b2(2);
  b1(0x00);
  b1(2);
  b1(0xFF);                        // 255 bytes de dados que nao existem
  b1(0x00); b1(0x00);
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  sub-bloco maior que o arquivo nao le fora do buffer");

  //    A mesma mentira dentro de uma EXTENSAO, que segue outro ramo do codigo.
  cabecalho("GIF89a", 1);
  b1(0x21); b1(0xF9);
  b1(0xFF);
  b1(0x00);
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  extensao com tamanho mentiroso nao le fora do buffer");

  // H) SEPARADOR DESCONHECIDO. Um byte que nao e 0x21, 0x2C nem 0x3B significa
  //    que a leitura se perdeu; seguir adiante seria ler ruido como estrutura.
  cabecalho("GIF89a", 1);
  quadro(0);
  b1(0x99);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  separador desconhecido para a leitura em vez de adivinhar");

  // I) NAO E GIF. O cache de disco aceita JPEG, PNG, GIF e WEBP no mesmo lugar,
  //    entao a maioria dos arquivos que chegam aqui NAO e GIF.
  { unsigned char jpeg[64];
    memset(jpeg, 0x20, sizeof jpeg);
    jpeg[0] = 0xFF; jpeg[1] = 0xD8; jpeg[2] = 0xFF; jpeg[3] = 0xE0;
    gravarBruto(jpeg, sizeof jpeg); }
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  arquivo que nao e GIF devolve 0");

  //    Inclusive um que COMECA parecido: "GIF7" nao e assinatura de GIF.
  cabecalho("GIF7xx", 1);
  quadro(0);
  quadro(0);
  fim();
  gravarTudo();
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  assinatura parecida nao passa por GIF");

  // J) ARQUIVO VAZIO, e o curto demais para ter cabecalho.
  gravarBruto((const unsigned char *)"", 0);
  assert(gif_animado(CAMINHO) == 0);
  gravarBruto((const unsigned char *)"GIF89a\0\0\0\0\0\0", 12);
  assert(gif_animado(CAMINHO) == 0);
  puts("ok  arquivo vazio e arquivo curto demais devolvem 0");

  // K) O QUE NEM CHEGA A SER ARQUIVO. gif_animado e chamada com o que o cache
  //    de disco devolver, e ele devolve NULL enquanto o download nao chegou.
  assert(gif_animado(NULL) == 0);
  assert(gif_animado("") == 0);
  assert(gif_animado("/tmp/nuvio-gif-que-nao-existe.gif") == 0);
  puts("ok  NULL, vazio e caminho inexistente devolvem 0");

  // L) FORA DO TIZEN NAO HA ANIMACAO, e isso e contrato e nao acidente: o
  //    SDL2_image da TV LG so exporta IMG_LoadGIF_RW, que devolve UM quadro.
  //    Ver o cabecalho de src/gif.h. Chamar mesmo assim tem de ser inofensivo.
  cabecalho("GIF89a", 1);
  quadro(0); quadro(0);
  fim();
  gravarTudo();
  assert(gif_textura(CAMINHO, 480) == 0);
  assert(gif_textura(NULL, 480) == 0);
  gif_parar();
  gif_parar();                     // duas vezes seguidas nao pode reclamar
  puts("ok  fora do Tizen gif_textura devolve 0 e gif_parar e inofensiva");

  // M) O MAPEAMENTO. Achar os quadros nao basta: o decodificador precisa do
  //    retangulo, do atraso, do descarte e da faixa de bytes de cada um.
  { GifQuadro q[8];
    int n;

    cabecalho("GIF89a", 1);
    controleGrafico(); quadro(0);
    controleGrafico(); quadro(1);
    fim();
    n = gif_mapear(buf, nbuf, q, 8);
    assert(n == 2);
    // O atraso sai do controle grafico: 10 centesimos sao 100 ms.
    assert(q[0].atraso == 100 && q[1].atraso == 100);
    assert(q[0].descarte == 0 && q[1].descarte == 0);
    assert(q[0].esq == 0 && q[0].topo == 0 && q[0].larg == 2 && q[0].alt == 2);
    // O SEGUNDO QUADRO TEM PALETA LOCAL, e ela esta DENTRO da faixa do quadro:
    // decodificar sem ela daria um quadro com as cores de outro.
    assert(q[1].fim - q[1].ini > q[0].fim - q[0].ini);
    assert(q[0].gceN == 8 && buf[q[0].gce] == 0x21 && buf[q[0].gce + 1] == 0xF9);
    puts("ok  mapeamento acha os quadros, o controle grafico e a paleta local");

    // Sem controle grafico nenhum o quadro ainda precisa de um passo: 100 ms,
    // que e o que o navegador usa quando o GIF nao diz nada.
    cabecalho("GIF89a", 1);
    quadro(0); quadro(0);
    fim();
    n = gif_mapear(buf, nbuf, q, 8);
    assert(n == 2 && q[0].atraso == 100 && q[0].descarte == 0);

    // METODO DE DESCARTE 2 (limpa a area do quadro antes do proximo) e atraso
    // de 3 centesimos. Sem ler o descarte, GIF de quadro parcial vira sujeira
    // acumulada na tela.
    cabecalho("GIF89a", 1);
    b1(0x21); b1(0xF9); b1(4); b1(2 << 2); b2(3); b1(0x00); b1(0x00);
    quadro(0);
    fim();
    n = gif_mapear(buf, nbuf, q, 8);
    assert(n == 1 && q[0].descarte == 2 && q[0].atraso == 30);

    // ATRASO DE 1 CENTESIMO VIRA 100 ms, que e o que todo navegador faz com os
    // GIF antigos gravados com 0 ou 1. Sem isto o quadro trocaria a cada volta
    // do laco de desenho.
    cabecalho("GIF89a", 1);
    b1(0x21); b1(0xF9); b1(4); b1(0x00); b2(1); b1(0x00); b1(0x00);
    quadro(0);
    fim();
    n = gif_mapear(buf, nbuf, q, 8);
    assert(n == 1 && q[0].atraso == 100);

    // O que nem chega a ser GIF nao vira quadro nenhum, e sem parametro
    // invalido derrubar nada.
    assert(gif_mapear(NULL, 0, q, 8) == 0);
    assert(gif_mapear(buf, nbuf, q, 0) == 0);
    assert(gif_mapear((const unsigned char *)"nao e gif de jeito nenhum", 25, q, 8) == 0);
    puts("ok  atraso, descarte e limites do mapeamento"); }

  // N) O ORCAMENTO POR RAM (24/09/2026). Os tres GIFs de avatar da TV de 1 GB
  //    que morria (registros 2340/2341/2351) e os de colecao ja vistos em 2 GB.
  { const size_t MB = 1024 * 1024;
    size_t g35 = gif_custo(35, 512, 512), g75 = gif_custo(75, 500, 375), g51 = gif_custo(51, 360, 360);
    assert(g35 == (size_t)35 * 512 * 512 * 4);
    assert(gif_custo(0, 512, 512) == 0 && gif_custo(3, 0, 10) == 0 && gif_custo(3, 10, -1) == 0);
    // 1 GB (o deviceMemory da Samsung dos registros) e menos: nao anima nada.
    assert(gif_orcamento_para(1.0) == 0);
    assert(gif_orcamento_para(0.5) == 0);
    assert(gif_orcamento_para(0.25) == 0);
    // 2 GB: os dois menores animam, o de 75 quadros fica parado; os de
    // colecao (21x498x448 do rawldon, 45x480x270) continuam animando.
    assert(gif_orcamento_para(2.0) == 48 * MB);
    assert(g35 <= gif_orcamento_para(2.0));
    assert(g51 <= gif_orcamento_para(2.0));
    assert(g75 > gif_orcamento_para(2.0));
    assert(gif_custo(21, 498, 448) <= gif_orcamento_para(2.0));
    assert(gif_custo(45, 480, 270) <= gif_orcamento_para(2.0));
    // Navegador que nao informa: o mesmo teto de 2 GB, nao "sem teto".
    assert(gif_orcamento_para(0.0) == 48 * MB);
    assert(gif_orcamento_para(-1.0) == 48 * MB);
    // 4 GB ou mais: o que sempre foi.
    assert(gif_orcamento_para(4.0) == GIF_SEM_TETO);
    assert(gif_orcamento_para(8.0) == GIF_SEM_TETO);
    // Fora do Tizen gif_pode_animar continua 0 e gif_ocioso e inofensiva.
    assert(gif_pode_animar() == 0);
    gif_ocioso(); gif_ocioso();
    puts("ok  orcamento por RAM: 1 GB parado, 2 GB ate 48 MB por volta, 4 GB sem teto"); }

  decodificador();
  fio();

  remove(CAMINHO);
  puts("gif: tudo ok");
  return 0;
}
