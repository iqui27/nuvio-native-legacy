// WEBP ANIMADO pelo caminho do GIF (#141). O focusGif de algumas colecoes do
// bingecat e WebP ("RIFF"), e o log da Samsung dizia "o arquivo nao e GIF".
//
// O arquivo e montado AQUI, com o WebPAnimEncoder da libwebpmux: tres quadros
// de cor chapada e duracoes diferentes, para as assercoes serem sobre o que o
// decodificador devolve (cor de cada quadro, atraso, volta ao 0), e nao sobre
// um binario opaco no repositorio.
#include "gif.h"
#include "nvtmp.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <webp/encode.h>
#include <webp/mux.h>

#define W 64
#define H 32

static unsigned char *montar(size_t *n) {
  static const unsigned char cor[3][3] = { {255, 0, 0}, {0, 255, 0}, {0, 0, 255} };
  static const int dur[3] = { 50, 80, 120 };
  WebPAnimEncoderOptions eo;
  WebPAnimEncoder *enc;
  WebPConfig cfg;
  WebPPicture pic;
  WebPData dado;
  unsigned char *px = malloc(W * H * 4), *out;
  int t = 0, i, k;
  assert(px && WebPAnimEncoderOptionsInit(&eo) && WebPConfigInit(&cfg));
  cfg.lossless = 1;
  enc = WebPAnimEncoderNew(W, H, &eo);
  assert(enc);
  for (i = 0; i < 3; i++) {
    for (k = 0; k < W * H; k++) {
      px[k * 4] = cor[i][0]; px[k * 4 + 1] = cor[i][1]; px[k * 4 + 2] = cor[i][2]; px[k * 4 + 3] = 255;
    }
    assert(WebPPictureInit(&pic));
    pic.use_argb = 1; pic.width = W; pic.height = H;
    assert(WebPPictureImportRGBA(&pic, px, W * 4));
    assert(WebPAnimEncoderAdd(enc, &pic, t, &cfg));
    WebPPictureFree(&pic);
    t += dur[i];
  }
  assert(WebPAnimEncoderAdd(enc, NULL, t, NULL));
  WebPDataInit(&dado);
  assert(WebPAnimEncoderAssemble(enc, &dado));
  WebPAnimEncoderDelete(enc);
  free(px);
  out = malloc(dado.size);
  memcpy(out, dado.bytes, dado.size);
  *n = dado.size;
  WebPDataClear(&dado);
  return out;
}

// Cor dominante do pixel do meio da saida: 0 = vermelho, 1 = verde, 2 = azul.
static int corDe(const unsigned char *s, int sw, int sh) {
  const unsigned char *p = s + ((size_t)(sh / 2) * sw + sw / 2) * 4;
  if (p[0] > 200 && p[1] < 60 && p[2] < 60) return 0;
  if (p[1] > 200 && p[0] < 60 && p[2] < 60) return 1;
  if (p[2] > 200 && p[0] < 60 && p[1] < 60) return 2;
  return -1;
}

int main(void) {
  size_t n;
  unsigned char *b = montar(&n), *copia;
  GifDec *d;
  int y0, y1, k, tw, th;

  assert(gif_webp_suportado());
  assert(gif_webp_animado_bytes(b, n));
  assert(!gif_webp_animado_bytes((const unsigned char *)"GIF89a", 6));
  puts("ok  RIFF/VP8X com o bit de animacao e reconhecido");

  copia = malloc(n); memcpy(copia, b, n);
  d = gif_dec_abrir(copia, n, W / 2, H / 2);            // dono de `copia`
  assert(d);
  assert(gif_dec_quadros(d) == 3);
  gif_dec_tela(d, &tw, &th);
  assert(tw == W && th == H);
  assert(gif_dec_atraso(d, 0) == 50 && gif_dec_atraso(d, 1) == 80 && gif_dec_atraso(d, 2) == 120);
  puts("ok  3 quadros, tela 64x32, atrasos do demux (50/80/120 ms)");

  for (k = 0; k < 7; k++) {
    int q = gif_dec_proximo(d, &y0, &y1);
    assert(q == k % 3);
    assert(y0 == 0 && y1 == H / 2);
    assert(corDe(gif_dec_saida(d), W / 2, H / 2) == k % 3);
  }
  gif_dec_fechar(d);
  puts("ok  cada quadro sai com a cor dele, reduzido, e volta ao 0 no fim");

  { FILE *f = fopen(nvt("/nuvio-gifwebp.webp"), "wb");
    assert(f && fwrite(b, 1, n, f) == n); fclose(f);
    assert(gif_animado(nvt("/nuvio-gifwebp.webp")) == 1);
    puts("ok  gif_animado aceita o arquivo WebP animado"); }

  { GifFio *f;
    GifFaixa fx;
    int vistos = 0, tentativas = 0;
    copia = malloc(n); memcpy(copia, b, n);
    f = gif_fio_abrir(copia, n, W / 2, H / 2);
    assert(f && gif_fio_nominal(f) == 250);
    while (vistos < 4 && tentativas++ < 2000) {
      if (gif_fio_pegar(f, &fx)) {
        assert(fx.quadro == vistos % 3);
        vistos++;
        gif_fio_devolver(f);
      } else { struct timespec z = { 0, 1000000 }; nanosleep(&z, NULL); }
    }
    assert(vistos == 4);
    gif_fio_fechar(f);
    puts("ok  pelo fio de decode: volta de 250 ms, quadros em ordem"); }

  free(b);
  puts("gifwebp: tudo ok");
  return 0;
}
