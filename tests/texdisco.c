#include <stdio.h>
#include <errno.h>
#include <assert.h>
#include <sys/statvfs.h>
#include "artehero.h"
#include "../src/sdlcompat.h"
static int falhaEscrita, falhaFechar, downloads;
static SDL_mutex *writeRaceMtx;
static SDL_cond *writeRaceCond;
static int writeRaceHold, writeRaceBlocked, writeRaceRelease;
static size_t escritaTeste(const void *p, size_t a, size_t b, FILE *f) {
  if (falhaEscrita) { int e = falhaEscrita; falhaEscrita = 0; errno = e; return 0; }
  if (writeRaceMtx) {
    SDL_LockMutex(writeRaceMtx);
    if (writeRaceHold) {
      writeRaceBlocked = 1;
      SDL_CondSignal(writeRaceCond);
      while (!writeRaceRelease) SDL_CondWait(writeRaceCond, writeRaceMtx);
    }
    SDL_UnlockMutex(writeRaceMtx);
  }
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
static int appleConsultas;
static int appleTeste(const char *id, const char *titulo, int ano, int serie, char *dst, size_t n) {
  assert(!strcmp(id, "tt3567288") && !strcmp(titulo, "Silo") && ano == 2015 && !serie);
  appleConsultas++;
  snprintf(dst, n, "https://is1-ssl.mzstatic.com/image/thumb/correta/{w}x{h}.{f}");
  return 1;
}
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
  snprintf(tmp, sizeof tmp, "%s-pacote/icone.png", dir);
  assert(!noCache(tmp));
  snprintf(tmp, sizeof tmp, "%s/00000000.jpg", dir);
  assert(noCache(tmp));
  puts("ok: caminho com prefixo parecido nao pertence a pasta de cache");
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
  // A versao anterior persistiu a arte errada sob a URL sem versao.
  {
    const char *antiga = "https://nuvio.invalid/arte/apple/1920/tt3567288/m/2015/Silo";
    CatItem c = {0};
    char velha[600]; long n; int foi = 0, antes;
    char *bytes = redeTeste("https://teste/errada.jpg", 0, &n);
    nomeDeCache(antiga, velha, sizeof velha);
    assert(gravarArquivo(velha, ".parcial", (unsigned char *)bytes, n)); free(bytes);
    antes = downloads;
    assert(garantirLocal(antiga, dst, sizeof dst, &foi, NULL) && !foi);
    snprintf(c.imdb, sizeof c.imdb, "tt3567288");
    snprintf(c.titulo, sizeof c.titulo, "Silo");
    snprintf(c.meta, sizeof c.meta, "2015");
    snprintf(c.tipo, sizeof c.tipo, "movie");
    arte_fonte_definir_apple(appleTeste);
    artehero_qualidade(1);
    assert(garantirLocal(artehero_url_destaque(&c, ARTEHERO_APPLE, 0), dst, sizeof dst, &foi, NULL));
    assert(foi && appleConsultas == 1 && downloads == antes + 1);
    assert(strcmp(dst, velha) && access(velha, F_OK) == 0);
    assert(garantirLocal(artehero_url_card_fonte(&c, ARTEHERO_APPLE, 0), dst, sizeof dst, &foi, NULL) && !foi);
    assert(garantirLocal("https://teste/primeira.jpg", dst, sizeof dst, &foi, NULL) && !foi);
    assert(downloads == antes + 1 && appleConsultas == 1);
    arte_fonte_definir_apple(NULL);
    puts("ok: Apple ignora cache antigo, preserva arquivo e reutiliza cache novo e de outras fontes");
  }
  /* Simula o item entregue para decode: nao pode ser podado nesse intervalo. */
  mtx=SDL_CreateMutex(); assert(mtx); nMax=1;
  snprintf(itens[0].caminho,sizeof itens[0].caminho,"https://teste/em-uso.jpg");
  itens[0].estado=PENDENTE; nomeDeCache(itens[0].caminho,dst,sizeof dst);
  assert(discoProtegido(dst,NULL)); itens[0].estado=PRONTO; assert(!discoProtegido(dst,NULL));
  SDL_DestroyMutex(mtx); mtx=NULL;
  puts("ok: protege arquivos entre rede e decode");
  // O contador e lido pela thread de desenho a cada relatorio. Segurar a
  // trava da fila de gravacao (discoMtx deixou de existir em 22/09/2026) deve
  // continuar permitindo a leitura da ultima amostra.
  cacheDiscoBytes = 123456;
  publicarCacheDisco();
  assert(pthread_mutex_lock(&gravMtx) == 0);
  assert(tex_cache_disco_bytes() == 123456);
  assert(pthread_mutex_unlock(&gravMtx) == 0);
  puts("ok: getter de cache nao espera a trava de gravacao");
  // A assinatura passa no download, mas o JPEG esta truncado. O decode
  // rejeita os bytes enquanto o gravador ainda esta escrevendo o w780;
  // nem o arquivo original nem a fila podem esconder essa variante ruim.
  {
    const char *original = "https://image.tmdb.org/t/p/original/corrompida.jpg";
    const char *variant = "https://image.tmdb.org/t/p/w780/corrompida.jpg";
    SDL_Thread *decoder;
    int before = downloads, k, foi = 0;
    char originalPath[600], marker[600];
    FILE *f;
    snprintf(marker, sizeof marker, "%s/.limpo-143", dir);
    f = fopen(marker, "w"); assert(f); fputs("1\n", f); assert(!fclose(f));
    writeRaceMtx = SDL_CreateMutex(); writeRaceCond = SDL_CreateCond();
    assert(writeRaceMtx && writeRaceCond);
    SDL_LockMutex(writeRaceMtx); writeRaceHold = 1; SDL_UnlockMutex(writeRaceMtx);
    mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
    condDec = SDL_CreateCond(); condLivre = SDL_CreateCond();
    assert(mtx && cond && condDec && condLivre);
    memset(itens, 0, sizeof itens); nMax = 1; rodando = 1;
    itens[0].estado = PENDENTE; itens[0].limite = 704;
    snprintf(itens[0].caminho, sizeof itens[0].caminho, "%s", original);
    nomeDeCache(original, originalPath, sizeof originalPath);
    f = fopen(originalPath, "wb"); assert(f); fputs("original-preservada", f); assert(!fclose(f));
    tex_cache_dir(dir);
    assert(baixarParaItem(0, original, dst, sizeof dst, &foi, NULL) == 1);
    assert(foi && downloads == before + 1 && itens[0].limiteTamanho == 704);
    nomeDeCache(variant, tmp, sizeof tmp); assert(!strcmp(tmp, dst));
    SDL_LockMutex(writeRaceMtx);
    while (!writeRaceBlocked) SDL_CondWait(writeRaceCond, writeRaceMtx);
    SDL_UnlockMutex(writeRaceMtx);
    // Uma copia ainda na fila tambem deve ser descartada junto da ativa.
    assert(enfileirarGravacao(dst, itens[0].bruto, itens[0].nBruto));
    SDL_LockMutex(mtx); paraDecode(0); SDL_UnlockMutex(mtx);
    decoder = SDL_CreateThread(threadDecode, "tex-corrupt-variant", NULL); assert(decoder);
    for (k = 0; k < 400; k++) {
      int failed;
      SDL_LockMutex(mtx); failed = itens[0].estado == FALHOU; SDL_UnlockMutex(mtx);
      if (failed) break;
      SDL_Delay(5);
    }
    assert(k < 400);
    SDL_LockMutex(mtx); rodando = 0; SDL_CondSignal(condDec); SDL_UnlockMutex(mtx);
    SDL_WaitThread(decoder, NULL);
    { unsigned char *pending = NULL; long n = 0;
      assert(!gravacaoPendente(dst, &pending, &n) && !pending); }
    SDL_LockMutex(writeRaceMtx); writeRaceRelease = 1; SDL_CondSignal(writeRaceCond); SDL_UnlockMutex(writeRaceMtx);
    tex_cache_esperar_gravacoes();
    assert(access(dst, F_OK) != 0);
    assert(access(originalPath, F_OK) == 0);
    SDL_LockMutex(mtx); itens[0].estado = PENDENTE; SDL_UnlockMutex(mtx);
    assert(baixarParaItem(0, original, dst, sizeof dst, &foi, NULL) == 1);
    assert(foi && downloads == before + 2);
    soltarBruto(&itens[0]); tex_cache_esperar_gravacoes();
    SDL_DestroyCond(condLivre); SDL_DestroyCond(condDec); SDL_DestroyCond(cond); SDL_DestroyMutex(mtx);
    condLivre = condDec = cond = NULL; mtx = NULL;
    // O gravador fica ocioso ate o processo sair; mantenha sua barreira viva.
    puts("ok: decode ruim invalida a variante exata, inclusive gravacao em curso, e retenta pela rede");
    unlink(marker);
  }
  DIR *d=opendir(dir); struct dirent *e;
  while ((e=readdir(d))) { if(e->d_name[0]=='.')continue; snprintf(tmp,sizeof tmp,"%s/%s",dir,e->d_name); unlink(tmp); }
  closedir(d); assert(!rmdir(dir));
  return 0;
}
