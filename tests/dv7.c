#include "../src/dts/dv7.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static size_t read_nal(const char *dir,const char *name,uint8_t *p) {
  char path[2048]; snprintf(path,sizeof path,"%s/%s",dir,name);
  FILE *f=fopen(path,"rb"); assert(f);
  size_t n=fread(p,1,8192,f); assert(n>2 && feof(f)); fclose(f); return n;
}
int main(int argc,char **argv) {
  assert(argc==2);
  assert(nv_dv7_selected(7,0,1) && !nv_dv7_selected(5,0,1) && !nv_dv7_selected(8,0,1));
  assert(!nv_dv7_selected(7,1,0) && nv_dv7_selected(5,1,0) && nv_dv7_selected(8,1,0));
  assert(nv_dv7_profile(7,1,1,1));
  assert(nv_dv7_profile(7,0,1,1));
  assert(!nv_dv7_profile(7,1,0,1));
  assert(!nv_dv7_profile(7,1,1,0));
  assert(nv_dv7_profile(5,0,1,1) && nv_dv7_profile(8,0,1,1));
  assert(!nv_dv7_profile(8,1,1,1) && !nv_dv7_profile(4,0,1,1));
  for(int kind=0;kind<2;kind++) {
    uint8_t input[32768],output[32768],nal[8192],expected[8192];
    /* Parameter set + base-layer VCL bytes must be copied unchanged. */
    const uint8_t base[]={0,0,0,1,0x40,1,0xaa,0xbb,0,0,0,1,0x02,1,0xcc,0xdd};
    const uint8_t el[]={0,0,1,0x7e,1,0xaa,0,0,0,1,0x02,9,0xbb};
    size_t n=sizeof base,out=0;
    memcpy(input,base,n); memcpy(input+n,el,sizeof el); n+=sizeof el;
    size_t len=read_nal(argv[1],kind?"fel-input.nal":"mel-input.nal",nal);
    size_t want=read_nal(argv[1],kind?"fel-output.nal":"mel-output.nal",expected);
    memcpy(input+n,"\0\0\1",3); n+=3; memcpy(input+n,nal,len); n+=len;
    assert(!nv_dv7_packet(input,n,output,sizeof output,&out));
    assert(out==sizeof base+4+want);
    assert(!memcmp(output,base,sizeof base));
    assert(!memcmp(output+sizeof base,"\0\0\0\1",4));
    assert(!memcmp(output+sizeof base+4,expected,want));
    assert(nv_dv7_packet(input,n,output,8,&out)<0 && !out);
    /* Bad metadata fails closed, so the player can return to HDR10. */
    input[n-2]^=0x5a;
    assert(nv_dv7_packet(input,n,output,sizeof output,&out)<0 && !out);
    assert(!nv_dv7_packet(el,sizeof el,output,sizeof output,&out) && !out);
  }
  uint8_t out[64],bad[]={0,0,0,1,0x7c,0,0x19}; size_t n;
  assert(nv_dv7_packet(bad,sizeof bad,out,sizeof out,&n)<0);
  for(size_t cut=0;cut<sizeof bad;cut++) assert(nv_dv7_packet(bad,cut,out,sizeof out,&n)<0);
  puts("DV7 MEL/FEL: RPU conversion, unchanged base layer, EL removal and invalid-input fallback passed.");
}
