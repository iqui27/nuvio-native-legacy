#define _POSIX_C_SOURCE 200809L
#include "miniatura_dovi.h"
#include <time.h>
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
static void near(float a,float b) { assert(isfinite(a) && fabsf(a-b)<.0002f); }
int main(void) {
  setbuf(stdout,NULL);
  AVDOVIRpuDataHeader h={0}; AVDOVIDataMapping m={0}; AVDOVIColorMetadata c={0}; MiniDovi d;
  h.bl_bit_depth=10; h.coef_log2_denom=12; h.disable_residual_flag=1;
  for(int k=0;k<3;k++) {
    m.curves[k].num_pivots=2; m.curves[k].pivots[1]=1023;
    m.curves[k].poly_order[0]=1; m.curves[k].poly_coef[0][1]=4096;
    c.ycc_to_rgb_offset[k]=(AVRational){0,1};
    for(int j=0;j<3;j++) {
      c.ycc_to_rgb_matrix[k*3+j]=(AVRational){k==j,1};
      c.rgb_to_lms_matrix[k*3+j]=(AVRational){k==j,1};
    }
  }
  assert(mini_dv_prepare(&d,&h,&m,&c));
  float in[3]={.2f,.4f,.6f},out[3]; mini_dv_reshape(&d,in,out);
  for(int k=0;k<3;k++) near(in[k],out[k]);
  // MMR must read the original three components, not previously reshaped output.
  m.curves[0].poly_coef[0][0]=409;
  m.curves[1].mapping_idc[0]=AV_DOVI_MAPPING_MMR; m.curves[1].mmr_order[0]=3;
  m.curves[1].mmr_constant[0]=1024;
  m.curves[1].mmr_coef[0][0][3]=4096; // Y*U
  m.curves[1].mmr_coef[0][1][0]=2048; // .5 Y^2
  m.curves[1].mmr_coef[0][2][6]=4096; // (Y*U*V)^3
  assert(mini_dv_prepare(&d,&h,&m,&c)); mini_dv_reshape(&d,in,out);
  near(out[1],.25f+.2f*.4f+.5f*.2f*.2f+powf(.2f*.4f*.6f,3));
  near(mini_dv_pq(0),0); near(mini_dv_pq(1),100);
  mini_dv_rgb(&d,in,out); for(int k=0;k<3;k++) assert(isfinite(out[k]) && out[k]>=0 && out[k]<=1);
  float packed[1024]; mini_dv_pack(&d,packed);
  near(packed[0],d.nonlinear[0]); near(packed[12],d.linear[0]);
  near(packed[32],2); near(packed[272],2); near(packed[512],2);
  near(packed[272+16],1); near(packed[272+17],3);
  near(packed[272+16+6+3],1); // MMR order-one Y*U coefficient
  // Sweep RGB results against the original exact-pow conversion, including near-black.
  MiniDoviLut *lut=malloc(sizeof *lut); assert(lut); mini_dv_lut_init(lut);
  float worst=0, worstV[3]={0}, worstE[3]={0}, worstF[3]={0}; unsigned rng=12345;
  for(int n=0;n<100000;n++) {
    float v[3],exact[3],fast[3];
    for(int k=0;k<3;k++) { rng=rng*1664525u+1013904223u; v[k]=(rng>>8)/16777215.f; }
    if(n<1000) v[0]=v[1]=v[2]=n/1000000.f;
    mini_dv_rgb(&d,v,exact); mini_dv_rgb_fast(&d,lut,v,fast);
    for(int k=0;k<3;k++) { float e=fabsf(exact[k]-fast[k]); if(e>worst) { worst=e; memcpy(worstV,v,sizeof v); memcpy(worstE,exact,sizeof exact); memcpy(worstF,fast,sizeof fast); } assert(isfinite(fast[k])); }
  }
  printf("LUT maximum channel error over 100000 colours: %.5f / 255\n",worst*255);
  printf("Worst signal %g %g %g exact %g %g %g fast %g %g %g\n",worstV[0],worstV[1],worstV[2],worstE[0],worstE[1],worstE[2],worstF[0],worstF[1],worstF[2]);
  assert(worst<.5f/255);
  volatile float checksum=0; struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC,&ts); double t0=ts.tv_sec+ts.tv_nsec/1e9;
  for(int n=0;n<100000;n++) { float v[3]={.1f+(n%900)*.001f,.3f,.6f}; mini_dv_rgb(&d,v,out); checksum+=out[0]; }
  clock_gettime(CLOCK_MONOTONIC,&ts); double t1=ts.tv_sec+ts.tv_nsec/1e9;
  for(int n=0;n<100000;n++) { float v[3]={.1f+(n%900)*.001f,.3f,.6f}; mini_dv_rgb_fast(&d,lut,v,out); checksum+=out[0]; }
  clock_gettime(CLOCK_MONOTONIC,&ts); double t2=ts.tv_sec+ts.tv_nsec/1e9;
  printf("Host microbenchmark exact %.1fms LUT %.1fms (not a Shield benchmark), checksum %.2f\n",(t1-t0)*1000,(t2-t1)*1000,(double)checksum);
  free(lut);
  h.disable_residual_flag=0; assert(!mini_dv_prepare(&d,&h,&m,&c)); h.disable_residual_flag=1;
  m.curves[0].num_pivots=10; assert(!mini_dv_prepare(&d,&h,&m,&c)); m.curves[0].num_pivots=2;
  m.curves[1].mmr_order[0]=4; assert(!mini_dv_prepare(&d,&h,&m,&c)); m.curves[1].mmr_order[0]=3;
  c.ycc_to_rgb_matrix[0].den=0; assert(!mini_dv_prepare(&d,&h,&m,&c));
  puts("Dolby metadata bounds, polynomial/MMR, PQ and RGB tests passed");
}
