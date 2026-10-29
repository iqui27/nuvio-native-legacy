"""Run the production Android fallback dispatch with a failed platform codec.

Host SDL_image decodes real PNGs; this does not emulate BitmapFactory/JNI.
"""
import pathlib
import struct
import subprocess
import tempfile
import zlib

source = pathlib.Path('src/tex_cache.c').read_text()
# Also supports the pre-fix inline dispatch, so the regression fails before fixes.
if 'static SDL_Surface *decodeArteMem(' in source:
    dispatch = source[source.index('static int fallbackCabe('):source.index('static int threadDecode(')]
else:
    start = source.index('      bruta = jpeg_rapido_carregar_mem(bruto')
    end = source.index('      free(bruto);', start)
    mem = source[start:end]
    start = source.index('    bruta = jpeg_rapido_carregar(caminho, limite')
    end = source.index('\n    }\n    tLoad', start)
    local = source[start:end]
    dispatch = '''
static SDL_Surface *decodeArteMem(const unsigned char *bruto, size_t nBruto,
                                  int limite, int *ow, int *oh) {
  int srcW = 0, srcH = 0; SDL_Surface *bruta = NULL;
''' + mem + '''
  *ow = srcW; *oh = srcH; return bruta;
}
static SDL_Surface *decodeArteArquivo(const char *caminho, int limite, int *ow, int *oh) {
  int srcW = 0, srcH = 0; SDL_Surface *bruta = NULL;
''' + local + '\n  *ow = srcW; *oh = srcH; return bruta;\n}\n'

harness = r'''
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define NV_ANDROID
static int loads, unknown;
static unsigned dim(const unsigned char *p) {
  return (unsigned)p[0]<<24 | (unsigned)p[1]<<16 | (unsigned)p[2]<<8 | p[3];
}
/* Bounds survive a codec rejection or allocation failure; no pixels allocated. */
static SDL_Surface *jpeg_rapido_carregar_mem(const unsigned char *b, size_t n,
                                             int limit, int *w, int *h) {
  (void)limit; *w = *h = 0;
  if (!unknown && n >= 24) { *w = dim(b+16); *h = dim(b+20); }
  return NULL;
}
static SDL_Surface *jpeg_rapido_carregar(const char *path, int limit, int *w, int *h) {
  unsigned char b[24]; FILE *f = fopen(path, "rb"); assert(f);
  size_t n = fread(b, 1, sizeof b, f); fclose(f);
  return jpeg_rapido_carregar_mem(b, n, limit, w, h);
}
static SDL_Surface *loadMem(SDL_RWops *rw, int close) {
  loads++; return IMG_Load_RW(rw, close);
}
static SDL_Surface *loadFile(const char *path) { loads++; return IMG_Load(path); }
#define IMG_Load_RW loadMem
#define IMG_Load loadFile
#define webp_carregar_larg_mem(b,n,l,w,h) NULL
#define webp_carregar_larg(p,l,w,h) NULL
#define gifFotoMem(b,n,l,w,h) NULL
#define gifFotoArquivo(p,l,w,h) NULL
'''
harness += dispatch + r'''
int main(int argc, char **argv) {
  assert(argc == 3);
  FILE *f = fopen(argv[1], "rb"); assert(f);
  fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
  unsigned char *b = malloc(n); assert(b && fread(b, 1, n, f) == (size_t)n); fclose(f);
  int allowed = atoi(argv[2]), w, h;
  for (int local = 0; local < 2; local++) {
    loads = 0;
    SDL_Surface *s = local ? decodeArteArquivo(argv[1], 1920, &w, &h)
                          : decodeArteMem(b, n, 1920, &w, &h);
    assert(loads == allowed);
    assert((s != NULL) == allowed);
    if (s) { assert(s->w == w && s->h == h); SDL_FreeSurface(s); }
  }
  unknown = 1; loads = 0;
  assert(!decodeArteMem(b, n, 1920, &w, &h) && loads == 0);
  loads = 0;
  assert(!decodeArteArquivo(argv[1], 1920, &w, &h) && loads == 0);
  free(b); return 0;
}
'''

def png(w, h):
    def chunk(kind, data):
        return struct.pack('>I', len(data)) + kind + data + struct.pack('>I', zlib.crc32(kind + data))
    compressor = zlib.compressobj()
    row = bytes(1 + w * 4)
    data = b''.join(compressor.compress(row) for _ in range(h)) + compressor.flush()
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) + chunk(b'IDAT', data) + chunk(b'IEND', b'')

with tempfile.TemporaryDirectory(prefix='nv-fallback-') as tmp:
    root = pathlib.Path(tmp)
    c = root / 'fallback.c'
    c.write_text(harness)
    binary = root / 'fallback'
    subprocess.run(['cc', str(c), '-I/opt/homebrew/include', '-L/opt/homebrew/lib',
                    '-lSDL2', '-lSDL2_image', '-o', str(binary)], check=True)
    for w, h, allowed in [(8192, 8192, 0), (64, 40000, 0), (1024, 2049, 0),
                           (1024, 2048, 1), (32, 32, 1)]:
        path = root / 'art.png'
        path.write_bytes(png(w, h))
        subprocess.run([str(binary), str(path), str(allowed)], check=True)
        print(f'PASS: Android fallback dispatch local/buffer {w}x{h}, allowed={allowed}')
