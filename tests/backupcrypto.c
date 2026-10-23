#include "backupcrypto.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static const unsigned char senha[] = "p\xc3\xa4ssw\xc3\xb6rd-Backup-123";
static const char contexto[] = "user-1:p7:settings";
static void rejeitar(const void *p, size_t n, const void *pw, size_t np, const char *ctx) {
  unsigned char *plain=(void *)1;size_t bytes=123;
  assert(!backupcrypto_abrir(p,n,pw,np,ctx,&plain,&bytes));
  assert(!plain && !bytes);
}
int main(int argc,char **argv) {
  const unsigned char original[]="local-provider-key=fixture-secret\n\0binary\xff";
  unsigned char *a=NULL,*b=NULL,*plain=NULL;size_t na=0,nb=0,np=0;
  assert(backupcrypto_disponivel());
  assert(backupcrypto_selar(original,sizeof original,senha,sizeof senha-1,contexto,&a,&na));
  assert(backupcrypto_selar(original,sizeof original,senha,sizeof senha-1,contexto,&b,&nb));
  assert(na==nb && memcmp(a+20,b+20,16) && memcmp(a+36,b+36,12));
  assert(backupcrypto_envelope(a,na,contexto));
  assert(backupcrypto_abrir(a,na,senha,sizeof senha-1,contexto,&plain,&np));
  assert(np==sizeof original && !memcmp(plain,original,np));backupcrypto_limpar(plain,np);free(plain);
  rejeitar(a,na,"wrong-password",14,contexto);
  rejeitar(a,na,senha,sizeof senha-1,"other-user:p7:settings");
  rejeitar(a,na,senha,sizeof senha-1,"user-1:p8:settings");
  rejeitar(a,na,senha,sizeof senha-1,"user-1:p7:font:abc:0");
  for(size_t i=0;i<54;i++)rejeitar(a,i,senha,sizeof senha-1,contexto);
  unsigned char *mut=malloc(na+1);assert(mut);memcpy(mut,a,na);mut[na]=0;
  rejeitar(mut,na+1,senha,sizeof senha-1,contexto);
  const size_t danos[]={0,16,20,36,48,50,54,na-17,na-1};
  for(size_t i=0;i<sizeof danos/sizeof *danos;i++) {
    memcpy(mut,a,na);mut[danos[i]]^=1;
    rejeitar(mut,na,senha,sizeof senha-1,contexto);
  }
  // Header metadata is authenticated along with encrypted account/settings data.
  memcpy(mut,a,na);memcpy(mut+20,b+20,28);rejeitar(mut,na,senha,sizeof senha-1,contexto);
  assert(!backupcrypto_selar(original,BACKUP_CRYPTO_MAX+1u,senha,sizeof senha-1,contexto,&plain,&np));
  assert(!plain && !np);
  if(argc==2) {
    FILE *f=fopen(argv[1],"wb");assert(f && fwrite(a,1,na,f)==na && !fclose(f));
  }
  free(mut);free(a);free(b);
  puts("PASS: AES-256-GCM roundtrip, independent salt/nonce, wrong password, account/profile/purpose binding, authenticated header/body/tag and strict bounds");
}
