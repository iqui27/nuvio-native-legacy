#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/statvfs.h>
static int falhaEscrita, falhaFechar, downloads;
static size_t escritaTeste(const void *p, size_t a, size_t b, FILE *f) {
  if (falhaEscrita) { int e = falhaEscrita; falhaEscrita = 0; errno = e; return 0; }
  return fwrite(p, a, b, f);
}
static int fecharTeste(FILE *f) {
  int r = fclose(f);
  if (falhaFechar) { falhaFechar = 0; errno = ENOSPC; return EOF; }
  return r;
}
#define fwrite escritaTeste
#define fclose fecharTeste
#define rede_baixar_bin redeTeste
#include "../src/tex_cache.c"
#undef fwrite
#undef fclose
#undef rede_baixar_bin
char *redeTeste(const char *url, int timeout, long *n) {
  unsigned char *b = calloc(1, 1024);
  (void)url; (void)timeout;
  if (strstr(url, "nao-webp")) {
    memcpy(b, "RIFF", 4); memcpy(b + 8, "NOPE", 4);
  } else { b[0] = 0xff; b[1] = 0xd8; }
  *n = 1024; downloads++; return (char *)b;
}
static void criar(const char *dir, const char *nome, time_t uso) {
  char p[1024]; struct utimbuf t = {uso, uso};
  snprintf(p, sizeof p, "%s/%s", dir, nome);
  FILE *f = fopen(p, "wb"); assert(f);
  assert(ftruncate(fileno(f), 1024) == 0); assert(!fclose(f)); assert(!utime(p, &t));
}
static int existe(const char *dir, const char *nome) {
  char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, nome); return access(p, F_OK) == 0;
}
static int protegido(const char *p, void *ctx) { (void)ctx; return strstr(p, "00000000.jpg") != NULL; }
int main(void) {
  char dir[] = "/tmp/nuvio-texdisco-XXXXXX", nome[32], dst[600], tmp[640];
  long total = 700 * 1024;
  assert(mkdtemp(dir));
  assert(NV_CACHE_DISCO_MAX <= 512L * 1024 * 1024);
  for (int i=0; i<700; i++) { snprintf(nome,sizeof nome,"%08x.jpg",i); criar(dir,nome,1000+i); }
  criar(dir,"ajustes.txt",1); criar(dir,"0000ffff.jpg.parcial",1);
  snprintf(tmp,sizeof tmp,"%s/0000eeee.jpg",dir); assert(!symlink("ajustes.txt",tmp));
  assert(nv_cache_podar(dir,&total,0,100*1024,0,0,protegido,NULL)==625);
  assert(total==75*1024 && existe(dir,"00000000.jpg") && existe(dir,"000002bb.jpg"));
  assert(!existe(dir,"00000001.jpg") && existe(dir,"ajustes.txt") && existe(dir,"0000ffff.jpg.parcial"));
  struct stat st; assert(!lstat(tmp,&st) && S_ISLNK(st.st_mode));
  puts("ok: mais de 512 arquivos, antigos primeiro, protege em uso/dados/parciais/symlinks");
  struct statvfs fs; assert(!statvfs(dir,&fs));
  uint64_t reserva=(uint64_t)fs.f_bavail*fs.f_frsize+4096;
  assert(nv_cache_podar(dir,&total,0,512*1024,reserva,0,protegido,NULL)>=4);
  puts("ok: espaco livre dispara poda mesmo abaixo do teto");
  snprintf(dirCache,sizeof dirCache,"%s",dir); cacheDiscoBytes=total;
  falhaEscrita=ENOSPC;
  assert(garantirLocal("https://teste/primeira.jpg",dst,sizeof dst,NULL,NULL));
  assert(downloads==1 && !stat(dst,&st) && st.st_size==1024);
  assert(garantirLocal("https://teste/primeira.jpg",dst,sizeof dst,NULL,NULL) && downloads==1);
  snprintf(tmp,sizeof tmp,"%s.parcial",dst); assert(access(tmp,F_OK)!=0);
  puts("ok: ENOSPC retenta sem outro download; arquivo completo e cache reutilizado");
  falhaFechar=1;
  assert(garantirLocal("https://teste/segunda.jpg",dst,sizeof dst,NULL,NULL) && downloads==2);
  puts("ok: ENOSPC no fclose tambem recupera");
  falhaEscrita=EACCES;
  assert(!garantirLocal("https://teste/terceira.jpg",dst,sizeof dst,NULL,NULL) && downloads==3);
  assert(access(dst,F_OK)!=0); snprintf(tmp,sizeof tmp,"%s.parcial",dst); assert(access(tmp,F_OK)!=0);
  puts("ok: erro de permissao nao promove arquivo nem apaga cache para retentar");
  assert(!garantirLocal("https://teste/nao-webp.bin",dst,sizeof dst,NULL,NULL));
  assert(downloads==4 && access(dst,F_OK)!=0);
  puts("ok: RIFF que nao declara WEBP nao entra no cache");
  /* Simula o item entregue para decode: nao pode ser podado nesse intervalo. */
  mtx=SDL_CreateMutex(); assert(mtx); nMax=1;
  snprintf(itens[0].caminho,sizeof itens[0].caminho,"https://teste/em-uso.jpg");
  itens[0].estado=PENDENTE; nomeDeCache(itens[0].caminho,dst,sizeof dst);
  assert(discoProtegido(dst,NULL)); itens[0].estado=PRONTO; assert(!discoProtegido(dst,NULL));
  SDL_DestroyMutex(mtx); mtx=NULL;
  puts("ok: protege arquivos entre rede e decode");
  // O contador e lido pela thread de desenho a cada relatorio. Segurar o
  // mutex que envolve a varredura de disco deve continuar permitindo a leitura
  // da ultima amostra; a versao anterior esperava aqui e travava o quadro.
  cacheDiscoBytes = 123456;
  publicarCacheDisco();
  assert(pthread_mutex_lock(&discoMtx) == 0);
  assert(tex_cache_disco_bytes() == 123456);
  assert(pthread_mutex_unlock(&discoMtx) == 0);
  puts("ok: getter de cache nao espera discoMtx");
  DIR *d=opendir(dir); struct dirent *e;
  while ((e=readdir(d))) { if(e->d_name[0]=='.')continue; snprintf(tmp,sizeof tmp,"%s/%s",dir,e->d_name); unlink(tmp); }
  closedir(d); assert(!rmdir(dir));
  return 0;
}
