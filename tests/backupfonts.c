#define _POSIX_C_SOURCE 200809L
#include "backupfonts.h"
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static void nome(BackupFont *f,const void *data,size_t n) {
  uint64_t h=UINT64_C(14695981039346656037);const unsigned char *p=data;
  for(size_t i=0;i<n;i++)h=(h^p[i])*UINT64_C(1099511628211);
  snprintf(f->id,sizeof f->id,"%016llx.ttf",(unsigned long long)h);f->n=n;f->chunks=(int)((n+BACKUP_FONT_CHUNK-1)/BACKUP_FONT_CHUNK);
  for(int i=0;i<f->chunks;i++)snprintf(f->remoto[i],200,"Fixture_%d",i);
}
static int stages(int root) {
  int fd=openat(root,".",O_RDONLY|O_DIRECTORY);assert(fd>=0);DIR *d=fdopendir(fd);assert(d);int n=0;struct dirent *e;
  while((e=readdir(d)))if(!strncmp(e->d_name,".nuvio-backup-fonts-",19))n++;
  closedir(d);return n;
}
int main(int argc,char **argv) {
  assert(argc==2);int root=open(argv[1],O_RDONLY|O_DIRECTORY);assert(root>=0);
  unsigned char *data=malloc(BACKUP_FONT_BYTES);assert(data);
  for(size_t i=0;i<BACKUP_FONT_BYTES;i++)data[i]=(unsigned char)(i*17u+3u);
  BackupFontManifest m={.n=1};nome(&m.font[0],data,BACKUP_FONT_BYTES);
  char *manifest=NULL;size_t nm;assert(backupfonts_manifest_exportar(&m,&manifest,&nm));
  BackupFontManifest decoded;assert(backupfonts_manifest_ler(manifest,nm,&decoded) && decoded.n==1 && decoded.font[0].n==BACKUP_FONT_BYTES);
  assert(!backupfonts_manifest_ler(manifest,nm-1,&decoded));free(manifest);
  assert(!backupfonts_id("../../sessions.txt") && !backupfonts_id("000000000000000g.ttf"));
  char *badBefore=NULL,*badAfter=NULL;
  assert(!backupfonts_plano("NUVIO-FONT-FILES 1\nfont 0000000000000001.ttf 4294967297\n","NUVIO-FONT-FILES 1\nstage aB12cd\n",&badBefore,&badAfter));
  BackupFontStage *stage=backupfonts_stage(argv[1]);assert(stage && stages(root)==1);
  assert(!backupfonts_stage_chunk(stage,&m.font[0],1,data,BACKUP_FONT_CHUNK));
  for(int j=0;j<BACKUP_FONT_CHUNKS;j++) {
    assert(!backupfonts_stage_chunk(stage,&m.font[0],j,data+(size_t)j*BACKUP_FONT_CHUNK,BACKUP_FONT_CHUNK-1));
    assert(backupfonts_stage_chunk(stage,&m.font[0],j,data+(size_t)j*BACKUP_FONT_CHUNK,BACKUP_FONT_CHUNK));
    assert(!backupfonts_stage_chunk(stage,&m.font[0],j,data+(size_t)j*BACKUP_FONT_CHUNK,BACKUP_FONT_CHUNK));
  }
  assert(backupfonts_stage_completo(stage,&m));
  assert(!backupfonts_stage_chunk(stage,&m.font[0],0,data,BACKUP_FONT_CHUNK));
  char *native=NULL,*before=NULL,*rollback=NULL,*after=NULL;size_t nn,nb;
  assert(backupfonts_stage_plano(stage,&native,&nn));
  assert(backupfonts_disco_ler(root,&before,&nb));
  assert(backupfonts_plano(before,native,&rollback,&after));
  assert(backupfonts_iguais(before,rollback));
  assert(backupfonts_disco_gravar(root,after,strlen(after)));
  BackupFontManifest existing;assert(backupfonts_enumerar(argv[1],&existing) && existing.n==1);
  // Immutable installation is idempotent and private; it never overwrites an
  // existing font that happens to have the same ID but different bytes.
  assert(backupfonts_disco_gravar(root,after,strlen(after)));
  int fonts=openat(root,"fontes-importadas",O_RDONLY|O_DIRECTORY);assert(fonts>=0);
  struct stat st;assert(!fstatat(fonts,m.font[0].id,&st,0) && (st.st_mode&0777)==0600);
  for(int j=0;j<4;j++) {
    unsigned char *chunk=NULL;size_t nc;
    assert(backupfonts_chunk_ler(argv[1],&m.font[0],j,&chunk,&nc) && nc==BACKUP_FONT_CHUNK && !memcmp(chunk,data+(size_t)j*BACKUP_FONT_CHUNK,nc));free(chunk);
  }
  // A before image removes only files introduced by this restore, preserving
  // the old library. It is sufficient for crash recovery without old font bytes.
  assert(backupfonts_disco_gravar(root,rollback,strlen(rollback)));
  assert(backupfonts_enumerar(argv[1],&existing) && !existing.n);
  int partial=openat(fonts,m.font[0].id,O_WRONLY|O_CREAT|O_EXCL,0600);assert(partial>=0 && !close(partial));
  assert(backupfonts_disco_gravar(root,rollback,strlen(rollback))); /* crash before the first byte */
  assert(backupfonts_enumerar(argv[1],&existing) && !existing.n);
  backupfonts_soltar(stage);assert(!stages(root));
  stage=backupfonts_stage(argv[1]);assert(stage);
  assert(backupfonts_stage_chunk(stage,&m.font[0],0,data,BACKUP_FONT_CHUNK));
  assert(!backupfonts_stage_completo(stage,&m));backupfonts_soltar(stage);assert(!stages(root));
  assert(!symlinkat("/etc/passwd",fonts,m.font[0].id));
  assert(!backupfonts_enumerar(argv[1],&existing));
  assert(!backupfonts_disco_gravar(root,rollback,strlen(rollback)));unlinkat(fonts,m.font[0].id,0);
  free(native);free(before);free(rollback);free(after);free(data);close(fonts);close(root);
  puts("PASS: bounded 32 MB font chunks, exact framing/order, fingerprint validation, private immutable install, rollback, cancellation cleanup and symlink rejection (font-face admission tested separately)");
}

int backup_test_linkat(int a,const char *b,int c,const char *d,int e) { (void)a;(void)b;(void)c;(void)d;(void)e;assert(!"Android does not permit hard links");return -1; }
