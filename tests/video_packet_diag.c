#include "dts/dts_video_diag.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  unsigned char p[]={0,0,0,1,64,1,0,0,1,66,1,0,0,1,68,1,0,0,1,42,1,0,0,1,124,1};
  unsigned char saved[sizeof p];memcpy(saved,p,sizeof p);
  NvVideoPacketDiag d=nv_video_packet_diag(p,sizeof p,"hevc");
  assert(d.nals==5 && d.vps && d.sps && d.pps && d.irap && d.rpu && d.vcl==21 && !d.el && !d.invalid);
  assert(!memcmp(p,saved,sizeof p));
  p[5]=9;d=nv_video_packet_diag(p,sizeof p,"hevc");assert(d.el);
  p[5]=0;d=nv_video_packet_diag(p,sizeof p,"hevc");assert(d.invalid);
  unsigned char avc[]={0,0,1,0x67,1,0,0,1,0x68,1,0,0,1,0x65,1};
  d=nv_video_packet_diag(avc,sizeof avc,"h264");assert(d.sps && d.pps && d.irap && d.vcl==5);
  for(size_t i=0;i<sizeof p;i++)(void)nv_video_packet_diag(p,i,"hevc");
  unsigned char big[8192]={0};memcpy(big+5000,saved,sizeof saved);
  d=nv_video_packet_diag(big,sizeof big,"hevc");assert(d.clipped && d.scanned==4096 && !d.nals);
  d=nv_video_packet_diag(NULL,0,"hevc");assert(!d.nals);
  puts("PASS video diagnostics: bounded Annex-B inspection, IRAP/parameter/RPU/EL/header checks, H264, short samples and immutable borrowed bytes");
}
