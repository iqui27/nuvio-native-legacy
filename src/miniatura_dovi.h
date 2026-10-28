// Profile 5 thumbnail colour conversion. Operates on original decoded Y/U/V,
// never on SurfaceTexture RGB (which may already have clipped the Dolby signal).
// Uses FFmpeg's public per-frame Dolby metadata, including polynomial/MMR reshaping.
// Reference: libavutil/dovi_meta.h; libplacebo Dolby colour pipeline and HPE matrix.
#ifndef MINIATURA_DOVI_H
#define MINIATURA_DOVI_H
#include <libavutil/dovi_meta.h>
#include <math.h>
#include <string.h>
typedef struct {
  int count[3], method[3][8], order[3][8];
  float pivot[3][9], coef[3][8][3], constant[3][8], mmr[3][8][3][7];
  float nonlinear[9], offset[3], linear[9];
} MiniDovi;
static float mini_dv_clamp(float x) { return fminf(1, fmaxf(0,x)); }
static int mini_dv_prepare(MiniDovi *d, const AVDOVIRpuDataHeader *h,
                           const AVDOVIDataMapping *map, const AVDOVIColorMetadata *color) {
  if (!d || !h || !map || !color || h->bl_bit_depth<8 || h->bl_bit_depth>16 ||
      h->coef_log2_denom>32 || !h->disable_residual_flag) return 0;
  memset(d,0,sizeof *d);
  float ps=ldexpf(1,-h->bl_bit_depth), cs=ldexpf(1,-h->coef_log2_denom);
  for(int c=0;c<3;c++) {
    const AVDOVIReshapingCurve *r=&map->curves[c];
    if(r->num_pivots<2 || r->num_pivots>9) return 0;
    d->count[c]=r->num_pivots;
    for(int i=0;i<r->num_pivots;i++) {
      d->pivot[c][i]=r->pivots[i]*ps;
      if(i && r->pivots[i]<=r->pivots[i-1]) return 0;
    }
    for(int i=0;i<r->num_pivots-1;i++) {
      d->method[c][i]=r->mapping_idc[i];
      if(r->mapping_idc[i]==AV_DOVI_MAPPING_POLYNOMIAL) {
        if(r->poly_order[i]>2) return 0;
        for(int k=0;k<=r->poly_order[i];k++) d->coef[c][i][k]=r->poly_coef[i][k]*cs;
      } else if(r->mapping_idc[i]==AV_DOVI_MAPPING_MMR) {
        int n=r->mmr_order[i]; if(n<1 || n>3) return 0;
        d->order[c][i]=n; d->constant[c][i]=r->mmr_constant[i]*cs;
        for(int j=0;j<n;j++) for(int k=0;k<7;k++) d->mmr[c][i][j][k]=r->mmr_coef[i][j][k]*cs;
      } else return 0;
    }
  }
  for(int i=0;i<3;i++) {
    if(!color->ycc_to_rgb_offset[i].den) return 0;
    d->offset[i]=av_q2d(color->ycc_to_rgb_offset[i]);
  }
  // HPE LMS -> BT.2020 RGB, after the metadata's linear matrix.
  const float hpe[9]={3.06441879f,-2.16597676f,.10155818f,-.65612108f,1.78554118f,-.12943749f,.01736321f,-.04725154f,1.03004253f};
  for(int i=0;i<9;i++) {
    if(!color->ycc_to_rgb_matrix[i].den || !color->rgb_to_lms_matrix[i].den) return 0;
    d->nonlinear[i]=av_q2d(color->ycc_to_rgb_matrix[i]);
  }
  for(int i=0;i<3;i++) for(int j=0;j<3;j++)
    for(int k=0;k<3;k++) d->linear[i*3+j]+=hpe[i*3+k]*av_q2d(color->rgb_to_lms_matrix[k*3+j]);
  return 1;
}
static void mini_dv_reshape(const MiniDovi *d,const float input[3],float out[3]) {
  float v[3];
  for(int c=0;c<3;c++) v[c]=fminf(d->pivot[c][d->count[c]-1],fmaxf(d->pivot[c][0],input[c]));
  float cross[7]={v[0],v[1],v[2],v[0]*v[1],v[0]*v[2],v[1]*v[2],v[0]*v[1]*v[2]};
  for(int c=0;c<3;c++) {
    int i=0; while(i<d->count[c]-2 && v[c]>=d->pivot[c][i+1]) i++;
    float s;
    if(!d->method[c][i]) s=(d->coef[c][i][2]*v[c]+d->coef[c][i][1])*v[c]+d->coef[c][i][0];
    else {
      s=d->constant[c][i];
      for(int k=0;k<7;k++) {
        float p=cross[k];
        for(int j=0;j<d->order[c][i];j++) { s+=d->mmr[c][i][j][k]*p; p*=cross[k]; }
      }
    }
    out[c]=mini_dv_clamp(s);
  }
}
static float mini_dv_pq(float x) {
  float p=powf(mini_dv_clamp(x),1.f/78.84375f);
  return powf(fmaxf(p-.8359375f,0)/(18.8515625f-18.6875f*p),1.f/.1593017578125f)*100.f;
}
static float mini_dv_hable(float x) { return ((x*(.15f*x+.05f)+.004f)/(x*(.15f*x+.5f)+.06f))-.02f/.3f; }
static void mini_dv_rgb(const MiniDovi *d,const float signal[3],float out[3]) {
  float reshaped[3], cone[3], rgb[3]; mini_dv_reshape(d,signal,reshaped);
  for(int i=0;i<3;i++) {
    float x=0; for(int j=0;j<3;j++) x+=d->nonlinear[i*3+j]*(reshaped[j]-d->offset[j]);
    cone[i]=mini_dv_pq(x);
  }
  for(int i=0;i<3;i++) { rgb[i]=0; for(int j=0;j<3;j++) rgb[i]+=d->linear[i*3+j]*cone[j]; }
  const float to709[9]={1.6605f,-.5876f,-.0728f,-.1246f,1.1329f,-.0083f,-.0182f,-.1006f,1.1187f};
  float luminance=fmaxf(0,.2627f*rgb[0]+.6780f*rgb[1]+.0593f*rgb[2]);
  float scale=luminance>1e-8f ? mini_dv_hable(luminance)/(mini_dv_hable(10)*luminance) : 0;
  for(int i=0;i<3;i++) {
    float x=0; for(int j=0;j<3;j++) x+=to709[i*3+j]*rgb[j];
    out[i]=powf(mini_dv_clamp(x*scale),1.f/2.4f);
  }
}
// Explicit float transport, independent of C padding/endian-sensitive int fields.
static void mini_dv_pack(const MiniDovi *d,float out[1024]) {
  memset(out,0,1024*sizeof(float));
  memcpy(out,d->nonlinear,9*sizeof(float));memcpy(out+9,d->offset,3*sizeof(float));memcpy(out+12,d->linear,9*sizeof(float));
  for(int c=0;c<3;c++) {
    int b=32+c*240;out[b]=d->count[c];
    for(int i=0;i<d->count[c];i++)out[b+1+i]=d->pivot[c][i];
    for(int i=0;i<d->count[c]-1;i++) {
      int a=b+16+i*28;out[a]=d->method[c][i];out[a+1]=d->order[c][i];
      memcpy(out+a+2,d->coef[c][i],3*sizeof(float));out[a+5]=d->constant[c][i];
      memcpy(out+a+6,d->mmr[c][i],21*sizeof(float));
    }
  }
}

// Shared immutable tables: 80 KiB, initialized once by the caller.
#define MINI_DV_PQ_SIZE 16384
#define MINI_DV_GAMMA_SIZE 4096
typedef struct { float pq[MINI_DV_PQ_SIZE+1], gamma[MINI_DV_GAMMA_SIZE+1], invWhite; } MiniDoviLut;
static void mini_dv_lut_init(MiniDoviLut *t) {
  for(int i=0;i<=MINI_DV_PQ_SIZE;i++) t->pq[i]=mini_dv_pq((float)i/MINI_DV_PQ_SIZE);
  // Fourth-root coordinates concentrate samples near black without a huge table.
  for(int i=0;i<=MINI_DV_GAMMA_SIZE;i++) t->gamma[i]=powf((float)i/MINI_DV_GAMMA_SIZE,4.f/2.4f);
  t->invWhite=1/mini_dv_hable(10);
}
static float mini_dv_lookup(const float *table,int count,float x) {
  float p=mini_dv_clamp(x)*count;
  int i=(int)p; if(i>=count) return table[count];
  return table[i]+(table[i+1]-table[i])*(p-i);
}
static void mini_dv_rgb_fast(const MiniDovi *d,const MiniDoviLut *t,const float signal[3],float out[3]) {
  float reshaped[3], cone[3], rgb[3]; mini_dv_reshape(d,signal,reshaped);
  for(int i=0;i<3;i++) {
    float x=0; for(int j=0;j<3;j++) x+=d->nonlinear[i*3+j]*(reshaped[j]-d->offset[j]);
    cone[i]=mini_dv_lookup(t->pq,MINI_DV_PQ_SIZE,x);
  }
  for(int i=0;i<3;i++) { rgb[i]=0; for(int j=0;j<3;j++) rgb[i]+=d->linear[i*3+j]*cone[j]; }
  float lum=fmaxf(0,.2627f*rgb[0]+.6780f*rgb[1]+.0593f*rgb[2]);
  const float gamut[9]={1.6605f,-.5876f,-.0728f,-.1246f,1.1329f,-.0083f,-.0182f,-.1006f,1.1187f};
  float scale=lum>1e-8f ? mini_dv_hable(lum)*t->invWhite/lum : 0;
  for(int i=0;i<3;i++) {
    float x=0; for(int j=0;j<3;j++) x+=gamut[i*3+j]*rgb[j];
    float mapped=x*scale;
    // Matrix cancellation near black/gamut clipping amplifies tiny PQ table errors.
    // Keep the exact conversion there rather than trading colour accuracy for speed.
    if(mapped>-.002f && mapped<.01f && lum>1e-8f) { mini_dv_rgb(d,signal,out); return; }
    out[i]=mini_dv_lookup(t->gamma,MINI_DV_GAMMA_SIZE,sqrtf(sqrtf(mini_dv_clamp(mapped))));
  }
}
#endif
