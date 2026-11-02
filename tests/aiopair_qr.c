// The actual shared QR texture path, with an offline GL sink.
#include "../src/celbotao.c"
#include <assert.h>
#undef glTexImage2D
int perftv_enabled,perftv_uploads;
static int uploads;
void glGenTextures(GLsizei n,GLuint *t) { assert(n==1); *t=1; }
void glBindTexture(GLenum target,GLuint t) { assert(t==1); }
void glPixelStorei(GLenum p,GLint value) { }
void glTexParameteri(GLenum target,GLenum p,GLint value) { }
void glTexImage2D(GLenum target,GLint level,GLint internal,GLsizei w,GLsizei h,GLint border,GLenum format,GLenum type,const GLvoid *data) {
  uploads++; assert(w==h && w>8 && format==GL_RGB);
  const unsigned char *pixels=data;
  for(int y=0;y<h;y++) for(int x=0;x<w;x++) if(x<4 || y<4 || x>=w-4 || y>=h-4)
    for(int c=0;c<3;c++) assert(pixels[(y*w+x)*3+c]==255);
  assert(pixels[(4*w+4)*3]==0); // Top-left finder starts after the quiet zone.
}
int main(void) {
  char url[QR_MAX_BYTES+2];
  strcpy(url,"https://pairing-fixture-with-long-public-hostname.example.invalid/aio/#");
  size_t prefix=strlen(url); memset(url+prefix,'a',32); url[prefix+32]=0;
  assert(strlen(url)>96 && strlen(url)<=QR_MAX_BYTES);
  assert(qrTextura(url) && uploads==1);
  assert(qrTextura(url) && uploads==1); // Full capability URL fits the cache.
  assert(!qrTextura("") && !qrTextura(NULL));
  memset(url,'x',sizeof url-1); url[sizeof url-1]=0;
  assert(!qrTextura(url) && uploads==1); // Failed generation cannot display an old QR.
  puts("aiopair QR: generation, four-module quiet zone, long URL cache and failure invalidation passed");
}
