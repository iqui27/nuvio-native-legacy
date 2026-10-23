#define _POSIX_C_SOURCE 200809L
#include "drivevinculo.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char **argv) {
  assert(argc==2);const char *dir=argv[1],*client="fixture.apps.googleusercontent.com";
  assert(!drivevinculo_ler(dir,"user-1",7,client));
  assert(drivevinculo_gravar(dir,"user-1",7,client,"refresh/+&="));
  char *s=drivevinculo_ler(dir,"user-1",7,client);assert(s && !strcmp(s,"refresh/+&="));free(s);
  assert(!drivevinculo_ler(dir,"user-1",8,client));
  assert(!drivevinculo_ler(dir,"user-2",7,client));
  assert(!drivevinculo_ler(dir,"user-1",7,"other.apps.googleusercontent.com"));
  assert(!drivevinculo_gravar(dir,"../escape",7,client,"refresh"));
  assert(!drivevinculo_gravar(dir,"user-1",0,client,"refresh"));
  assert(!drivevinculo_gravar(dir,"user-1",7,client,"bad\nvalue"));
  char path[4096],target[4096];snprintf(path,sizeof path,"%s/drive-auth-user-1-p7.json",dir);
  struct stat st;assert(!stat(path,&st) && (st.st_mode&0777)==0600);
  assert(drivevinculo_gravar(dir,"user-1",7,client,"rotated"));
  s=drivevinculo_ler(dir,"user-1",7,client);assert(s && !strcmp(s,"rotated"));free(s);
  assert(drivevinculo_gravar(dir,"user-1",7,client,""));
  assert(!drivevinculo_ler(dir,"user-1",7,client));
  assert(drivevinculo_gravar(dir,"user-1",7,client,""));
  snprintf(target,sizeof target,"%s/untouched.txt",dir);FILE *f=fopen(target,"w");assert(f);fputs("sentinel",f);fclose(f);
  assert(!symlink(target,path));assert(!drivevinculo_ler(dir,"user-1",7,client));
  assert(!drivevinculo_gravar(dir,"user-1",7,client,"replacement"));
  f=fopen(target,"r");assert(f);char b[16];assert(fgets(b,sizeof b,f) && !strcmp(b,"sentinel"));fclose(f);
  unlink(path);unlink(target);
  puts("PASS: Drive connection restart, profile/account/client separation, rotation, disconnect, private mode and symlink protection");
}
