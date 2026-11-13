#include "dts/dts_video_diag.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
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
  d=nv_video_packet_scan(big,sizeof big,"hevc",2097152,256);
  assert(!d.clipped && d.scanned==sizeof big && d.nals==5 && d.rpu && d.irap);
  uint32_t fingerprint=nv_video_picture_hash(big,sizeof big);
  big[7000]=1;assert(nv_video_picture_hash(big,sizeof big)!=fingerprint);big[7000]=0;
  // Complete SPS/VUI and an RPU beyond the former 4 KiB prefix are observable.
  const unsigned char sps[]={66,1,1,0,0,3,0,0,3,0,0,3,0,0,3,0,0,3,0,0,160,3,192,128,16,228,217,69,127,194,38,161,34,1,48};
  memset(big,0,sizeof big);size_t at=5000;
  big[at++]=0;big[at++]=0;big[at++]=1;memcpy(big+at,sps,sizeof sps);at+=sizeof sps;
  big[at++]=0;big[at++]=0;big[at++]=1;big[at++]=124;big[at++]=1;
  d=nv_video_packet_scan(big,at,"hevc",2097152,256);
  assert(!d.clipped && d.rpu && d.sps_ok && d.format.width==1920 && d.format.height==1080);
  assert(d.format.depth==10 && d.format.primaries==9 && d.format.transfer==16 && d.format.matrix==9);
  // ASan: advertised size exceeds actual storage, but bounded inspection
  // cannot read beyond the allocated two MiB. No decode/copy allocation.
  unsigned char *cap=calloc(2097152,1);assert(cap);
  d=nv_video_packet_scan(cap,2097280,"hevc",2097152,256);
  assert(d.clipped && d.scanned==2097152);
  assert(nv_video_picture_hash(cap,2097280)==nv_video_picture_hash(cap,2097152));free(cap);
  for(unsigned i=1;i<=200;i++)assert(nv_video_picture_sample(i)==(i<=4 || i==24 || i==120));
  memset(big,0,sizeof big);for(size_t i=0;i<300*5;i+=5){big[i+2]=1;big[i+3]=124;big[i+4]=1;}
  d=nv_video_packet_scan(big,300*5,"hevc",2097152,256);assert(d.clipped && d.nals==256);
  puts("PASS video diagnostics: complete bounded samples, SPS colour/depth, later RPU, fingerprints, six-sample budget; bounded Annex-B inspection, IRAP/parameter/RPU/EL/header checks, H264, short samples and immutable borrowed bytes");
}
