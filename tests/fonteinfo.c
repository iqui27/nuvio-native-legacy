#include "fonteinfo.h"
#include "streams.h"
#include "badges.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static int tem(const FonteInfo *f,const char *s) {
  for(int i=0;i<f->nTextos;i++) if(!strcmp(f->texto[i],s)) return 1;
  return 0;
}
int main(void) {
  Stream s = {0}; FonteInfo f;
  // Missing metadata never becomes SDR, stereo, an invented size or provider.
  snprintf(s.url,sizeof s.url,"https://secret.invalid/4K-HDR10-7.1/file?key=private");
  snprintf(s.provedor,sizeof s.provedor,"4K Atmos");
  fonteinfo_montar(&s,&f); assert(!f.selos && !f.nTextos);
  s.altura = 2160;
  s.badges = badges_detectar("BluRay HDR10 TrueHD Atmos 7.1 Netflix");
  s.filtro.valores[FONTEFILTRO_AUDIO] = fontefiltro_valores(FONTEFILTRO_AUDIO,"TrueHD Atmos");
  s.filtro.valores[FONTEFILTRO_CANAIS] = fontefiltro_valores(FONTEFILTRO_CANAIS,"7.1");
  snprintf(s.filtro.grupo,sizeof s.filtro.grupo,"MgB");
  s.tamanhoBytes = UINT64_C(13851269530);
  fonteinfo_montar(&s,&f);
  assert(f.selos & badges_bit("r-4k"));
  assert(f.selos & badges_bit("q-bluray"));
  assert(f.selos & badges_bit("v-hdr10"));
  assert(f.selos & badges_bit("a-truehd"));
  assert(f.selos & badges_bit("a-atmos"));
  assert(f.selos & badges_bit("c-71"));
  assert(!(f.selos & badges_bit("p-netflix")));
  assert(tem(&f,"MgB") && tem(&f,"12.9 GB"));
  fonteinfo_guardar(&s,"tt123:1:1",4,18,2);
  assert(fonteinfo_ler("tt123:1:1",4,18,&f));
  s.tamanhoBytes = UINT64_C(2147483648);
  snprintf(s.filtro.grupo,sizeof s.filtro.grupo,"NewGroup");
  fonteinfo_guardar(&s,"tt123:1:1",4,18,2);
  assert(fonteinfo_ler("tt123:1:1",4,18,&f));
  assert(tem(&f,"2.0 GB") && tem(&f,"NewGroup") && !tem(&f,"MgB"));
  assert(!fonteinfo_ler("tt123:1:2",4,18,&f) && !f.selos && !f.nTextos);
  assert(!fonteinfo_ler("tt999:1:1",4,18,&f));
  assert(!fonteinfo_ler("tt123:1:1",5,18,&f));
  assert(!fonteinfo_ler("tt123:1:1",4,19,&f));
  // Failover replaces the winner even within the same title/list/profile.
  Stream next = {0}; next.altura=1080;
  next.filtro.valores[FONTEFILTRO_AUDIO] = fontefiltro_valores(FONTEFILTRO_AUDIO,"AAC");
  next.filtro.valores[FONTEFILTRO_CANAIS] = fontefiltro_valores(FONTEFILTRO_CANAIS,"2.0");
  fonteinfo_guardar(&next,"tt123:1:1",4,18,3);
  assert(fonteinfo_ler("tt123:1:1",4,18,&f));
  assert((f.selos & badges_bit("r-1080")) && !(f.selos & badges_bit("r-4k")));
  assert(tem(&f,"AAC") && tem(&f,"2.0") && !tem(&f,"MgB"));
  fonteinfo_limpar(); assert(!fonteinfo_ler("tt123:1:1",4,18,&f));
  fonteinfo_guardar(NULL,"tt123",4,18,3); assert(!fonteinfo_ler("tt123",4,18,&f));
  s.badges=badges_bit("a-atmos-dv"); s.altura=0;
  memset(&s.filtro,0,sizeof s.filtro);s.tamanhoBytes=0;
  fonteinfo_montar(&s,&f);
  assert((f.selos & badges_bit("v-dv")) && (f.selos & badges_bit("a-atmos")));
  assert(!(f.selos & badges_bit("a-atmos-dv")));
  puts("fonteinfo: metadata-only fields, no credentials, actual winner, episode/profile/generation isolation PASS");
}
