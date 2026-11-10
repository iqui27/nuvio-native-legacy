/* Host test of the production loader; synthetic pixels, no app/device access. */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static size_t copied;
static int refuse_malloc;
static void *count_copy(void *to, const void *from, size_t n) {
  copied += n;
  return memcpy(to, from, n);
}
static void *test_malloc(size_t n) { return refuse_malloc ? NULL : malloc(n); }
#undef memcpy
#define memcpy count_copy
#define malloc test_malloc
#ifndef WEBP_OUTPUT_SOURCE
#define WEBP_OUTPUT_SOURCE "../src/webp.c"
#endif
#include WEBP_OUTPUT_SOURCE
#undef malloc
#undef memcpy
#include <webp/decode.h>
#include <webp/encode.h>
#include <stddef.h>
#include <time.h>
#include <assert.h>
#include <dlfcn.h>

_Static_assert(sizeof(WpConfig) == sizeof(WebPDecoderConfig), "config ABI");
_Static_assert(sizeof(WpBuffer) == sizeof(WebPDecBuffer), "buffer ABI");
_Static_assert(offsetof(WpConfig, output) == offsetof(WebPDecoderConfig, output), "output ABI");
_Static_assert(offsetof(WpConfig, options) == offsetof(WebPDecoderConfig, options), "options ABI");
_Static_assert(offsetof(WpBuffer, u.RGBA) == offsetof(WebPDecBuffer, u.RGBA), "RGBA ABI");
_Static_assert(offsetof(WpBuffer, private_memory) == offsetof(WebPDecBuffer, private_memory), "private ABI");

/* Independent official-header reference, reused from the reviewed copy harness. */
static SDL_Surface *reference(const uint8_t *d, size_t n, int tw) {
  WebPDecoderConfig c;
  int w, h, th;
  assert(WebPGetInfo(d, n, &w, &h));
  if (tw > w) tw = w;
  th = (int)(((long)h * tw + w / 2) / w);
  if (th < 1) th = 1;
  SDL_Surface *s = nv_superficie(0, tw, th, 32, SDL_PIXELFORMAT_ABGR8888);
  assert(s && WebPInitDecoderConfig(&c));
  c.options.use_scaling = tw != w;
  c.options.scaled_width = tw;
  c.options.scaled_height = th;
  c.output.colorspace = MODE_RGBA;
  c.output.is_external_memory = 1;
  c.output.u.RGBA.rgba = s->pixels;
  c.output.u.RGBA.stride = s->pitch;
  c.output.u.RGBA.size = (size_t)s->pitch * th;
  int status = WebPDecode(d, n, &c);
  WebPFreeDecBuffer(&c.output);
  assert(status == VP8_STATUS_OK);
  return s;
}
static void equal(SDL_Surface *s, SDL_Surface *r) {
  assert(s && r && s->w == r->w && s->h == r->h);
  assert(s->format->format == SDL_PIXELFORMAT_ABGR8888);
  for (int y = 0; y < s->h; ++y)
    assert(!memcmp((char *)s->pixels + (size_t)y * s->pitch,
                   (char *)r->pixels + (size_t)y * r->pitch, (size_t)s->w * 4));
}
static uint64_t now_ns(void) {
  struct timespec t;
  assert(!clock_gettime(CLOCK_MONOTONIC, &t));
  return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec;
}
static int reject_init(WpConfig *c, int abi) { (void)c; (void)abi; return 0; }
static int reject_decode(const uint8_t *d, size_t n, WpConfig *c) {
  (void)d; (void)n; (void)c; return 3;
}
static int observed_external;
static FnDecode real_decode;
static int observe_decode(const uint8_t *d, size_t n, WpConfig *c) {
  observed_external = c->output.is_external_memory;
  return real_decode(d, n, c);
}

int main(void) {
  const struct { const char *name; int w, h, target, lossless, alpha; } cases[] = {
    {"lossy-4k",3840,2160,1920,0,0}, {"lossless",1920,1080,960,1,0},
    {"alpha",1280,720,640,1,1}, {"odd",1001,501,301,0,1},
    {"one-row",71,1,35,1,1}, {"unscaled",64,64,320,1,1}
  };
  int expected = getenv("EXPECT_TRANSFERS") ? atoi(getenv("EXPECT_TRANSFERS")) : 1;
  assert(expected == 1 || expected == 2);
  printf("libwebp=%#x SDL=%d.%d.%d expected-scaled-transfers=%d\n",
         WebPGetDecoderVersion(), SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL, expected);
  pthread_once(&abrirUmaVez, abrir);
  Dl_info a, b;
  assert(pDecode && dladdr((void *)pDecode, &a) && dladdr((void *)WebPDecode, &b));
  assert(a.dli_fbase == b.dli_fbase);
  printf("SAME-IMAGE %s\n", a.dli_fname);
  real_decode = pDecode;
  for (size_t k = 0; k < sizeof cases / sizeof cases[0]; ++k) {
    int w = cases[k].w, h = cases[k].h, target = cases[k].target;
    uint8_t *rgba = malloc((size_t)w * h * 4), *enc = NULL;
    assert(rgba);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) {
      uint8_t *p = rgba + ((size_t)y * w + x) * 4;
      p[0] = (uint8_t)(x * 255 / (w > 1 ? w - 1 : 1));
      p[1] = (uint8_t)(y * 255 / (h > 1 ? h - 1 : 1));
      p[2] = (uint8_t)((x ^ y) * 7);
      p[3] = cases[k].alpha ? (uint8_t)((x + y) * 3) : 255;
    }
    size_t n = cases[k].lossless ? WebPEncodeLosslessRGBA(rgba,w,h,w*4,&enc)
                                : WebPEncodeRGBA(rgba,w,h,w*4,90,&enc);
    free(rgba);
    assert(n && enc);
    SDL_Surface *r = reference(enc, n, target);
    int ow, oh;
    copied = 0;
    observed_external = -1;
    pDecode = observe_decode;
    SDL_Surface *s = webp_carregar_larg_mem(enc,n,target,&ow,&oh);
    pDecode = real_decode;
    equal(s, r);
    assert(ow == w && oh == h);
    size_t output = (size_t)r->w * r->h * 4;
    printf("copies %s bytes=%zu output=%zu external=%d\n", cases[k].name, copied, output, observed_external);
    fflush(stdout);
    assert(copied == output * (w > target ? expected : 1));
    if (w > target) assert(observed_external == (expected == 1));
    SDL_FreeSurface(s);
    /* Two warmups; seven timed production calls. Encoding/parity/free excluded. */
    for (int i = -2; i < 7; ++i) {
      uint64_t t = now_ns();
      s = webp_carregar_larg_mem(enc,n,target,&ow,&oh);
      uint64_t elapsed = now_ns() - t;
      assert(s);
      if (i >= 0) printf("sample,%s,%d,%llu\n",cases[k].name,i,(unsigned long long)elapsed);
      SDL_FreeSurface(s);
    }
    s = webp_carregar_larg_mem(enc,n/2,target,&ow,&oh);
    assert(!s);
    /* Missing/refused advanced API, failed decode, and output allocation failure
       must all retain the existing full-size fallback and original pixels. */
    if (w > target) {
      SDL_Surface *full = reference(enc,n,w);
      FnCfgInit init = pCfgInit;
      for (int mode = 0; mode < 4; ++mode) {
        if (mode == 0) pCfgInit = NULL;
        if (mode == 1) pCfgInit = reject_init;
        if (mode == 2) pDecode = reject_decode;
        if (mode == 3) refuse_malloc = 1;
        s = webp_carregar_larg_mem(enc,n,target,&ow,&oh);
        refuse_malloc = 0; pCfgInit = init; pDecode = real_decode;
        equal(s,full);
        assert(ow == w && oh == h);
        SDL_FreeSurface(s);
      }
      SDL_FreeSurface(full);
    }
    SDL_FreeSurface(r); WebPFree(enc);
    printf("PASS %s parity/truncation/fallback\n",cases[k].name);
  }
  puts("RESULT PASS");
  return 0;
}
