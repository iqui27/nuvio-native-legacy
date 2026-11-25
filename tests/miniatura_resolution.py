"""Exercise the production CPU shrinker at every supported output resolution."""
from pathlib import Path
import subprocess
import tempfile

source = Path("src/miniatura_motor.c").read_text()
start = source.index("static inline float clamp01")
end = source.index("\nstatic int codificar", start)
production = source[start:end]
prefix = r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#define MINI_W 320
#define AV_PIX_FMT_FLAG_RGB 1
#define AVCOL_RANGE_JPEG 1
#define AVCOL_SPC_BT2020_NCL 1
#define AVCOL_SPC_BT2020_CL 2
#define AVCOL_SPC_BT470BG 3
#define AVCOL_SPC_SMPTE170M 4
#define AVCOL_SPC_UNSPECIFIED 5
#define AVCOL_TRC_SMPTE2084 1
#define AVCOL_TRC_UNSPECIFIED 2
#define AVCOL_TRC_ARIB_STD_B67 3
enum AVPixelFormat { AV_PIX_FMT_YUVJ420P };
typedef struct { int num,den; } AVRational;
typedef struct {
 int width,height,format,color_range,colorspace,color_trc,linesize[3];
 uint8_t *data[3]; AVRational sample_aspect_ratio;
} AVFrame;
typedef struct { int flags,nb_components,log2_chroma_w,log2_chroma_h; struct { int depth; } comp[3]; } AVPixFmtDescriptor;
typedef struct {
 struct { int larguraSaida; } cfg; AVFrame *quadro,*saida;
 struct { AVRational sample_aspect_ratio; } *st;
 int hdrDica; float lutPq[1024],lutGama[4096];
} MiniMotor;
static const AVPixFmtDescriptor *av_pix_fmt_desc_get(enum AVPixelFormat f) {
 (void)f; static const AVPixFmtDescriptor d={.nb_components=3,.log2_chroma_w=1,.log2_chroma_h=1,.comp={{8},{8},{8}}}; return &d;
}
static void prepararLuts(MiniMotor *m,int pq) { (void)m;(void)pq; }
static void av_frame_unref(AVFrame *f) { for(int p=0;p<3;p++){free(f->data[p]);f->data[p]=NULL;} }
static int av_frame_get_buffer(AVFrame *f,int alignment) {
 (void)alignment;
 for(int p=0;p<3;p++) {
  int w=p?f->width/2:f->width,h=p?f->height/2:f->height;
  f->linesize[p]=w;f->data[p]=malloc((size_t)w*h);assert(f->data[p]);
 }
 return 0;
}
'''
checks = r'''
int main(void) {
 for(int mode=0;mode<2;mode++) for(int i=0;i<4;i++) {
  int widths[]={0,320,480,640};
  AVFrame in={.width=64,.height=mode?48:36,.color_range=AVCOL_RANGE_JPEG,.sample_aspect_ratio={1,1}},out={0};
  av_frame_get_buffer(&in,32);
  memset(in.data[0],100,(size_t)in.width*in.height);
  memset(in.data[1],128,(size_t)in.width*in.height/4);
  memset(in.data[2],128,(size_t)in.width*in.height/4);
  MiniMotor m={.quadro=&in,.saida=&out};m.cfg.larguraSaida=widths[i];
  assert(!reduzir(&m));
  assert(out.width==(widths[i]?widths[i]:320) && out.height==out.width*9/16);
  assert(out.data[0][(out.height/2)*out.linesize[0]+out.width/2]>80);
  if(mode) assert(out.data[0][0]==0); // pillarbox for 4:3 source
  av_frame_unref(&in);av_frame_unref(&out);
 }
 puts("PASS: production CPU output at Auto/180p/270p/360p and 4:3 letterboxing");
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / "resolution.c").write_text(prefix + production + checks)
    subprocess.run(["cc", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    str(path / "resolution.c"), "-lm", "-o", str(path / "resolution")], check=True)
    subprocess.run([str(path / "resolution")], check=True, timeout=30)
