// Large imported release-group expressions, scoped modifiers and bounded memory.
#include "regexjs.h"
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void check(const char *pattern,const char *text,int expected) {
  char error[96];RegexJs *re=regexjs_compilar_badge(pattern,error,sizeof error);
  if(!re)fprintf(stderr,"compile: %s: %s\n",pattern,error);
  assert(re);assert(regexjs_testar(re,text,strlen(text))==expected);regexjs_liberar(re);
}
static void *test(void *unused) {
  (void)unused;char error[96];
  char *pattern=malloc(REGEXJS_BADGE_MAX+2);assert(pattern);
  size_t used=(size_t)sprintf(pattern,"^(?:");
  for(int i=0;i<8000;i++)used+=(size_t)sprintf(pattern+used,"%sRelease%05d",i?"|":"",i);
  used+=(size_t)sprintf(pattern+used,")$");assert(used>80000&&used<REGEXJS_BADGE_MAX);
  // Keep the existing program ceiling for normal autoplay rules.
  RegexJs *re=regexjs_compilar_web(pattern,error,sizeof error);assert(!re);
  re=regexjs_compilar_badge(pattern,error,sizeof error);assert(re);
  assert(regexjs_testar(re,"Release00000",12)&&regexjs_testar(re,"Release07999",12));
  assert(!regexjs_testar(re,"Release08000",12)&&!regexjs_testar(re,"Release07999x",13));
  assert(regexjs_memoria(re)>100000&&regexjs_memoria(re)<4*1024*1024);regexjs_liberar(re);
  memset(pattern,'a',REGEXJS_BADGE_MAX+1);pattern[REGEXJS_BADGE_MAX+1]=0;
  assert(!regexjs_compilar_badge(pattern,error,sizeof error));free(pattern);
  re=regexjs_compilar_badge("(?:a{1000}){1000}",error,sizeof error);assert(!re);
  check("(?i)^movie-(?-i:SiGMA)\\.mkv$","MOVIE-SiGMA.MKV",1);
  check("(?i)^movie-(?-i:SiGMA)\\.mkv$","MOVIE-SIGMA.MKV",0);
  check("(?i)^movie-(?-i:SiGMA)\\.mkv$","MOVIE-sigma.MKV",0);
  check("^(?i:[a-z]+)-(?-i:[A-Z]+)$","MoViE-SIGMA",1);
  check("^(?i:[a-z]+)-(?-i:[A-Z]+)$","MoViE-sigma",0);
  check("^(?i:a(?-i:b(?i:c))d)$","AbCD",1);
  check("^(?i:a(?-i:b(?i:c))d)$","ABCD",0);
  check("(?i)^(?-i:a*)B$","aaaB",1);check("(?i)^(?-i:a*)B$","AAAB",0);
  check("^(?s:a.*b)c$","a\nbc",1);check("^(?s:a.*b).$","a\nb\n",0);
  check("(?s)^(?-s:a.*b)c$","a\nbc",0);
  check("(?m:^a$)","x\na\ny",1);check("(?m)(?-m:^a$)","x\na\ny",0);
  check("(?i)(?<=(?-i:Ab))C","Abc",1);check("(?i)(?<=(?-i:Ab))C","aBc",0);
  check("^(?:a|ab)c$","abc",1);check("^(?:|ab)c$","abc",1);
  const char *bad[]={"(?-:x)","(?ii:x)","(?i-i:x)","(?s-:x)","(?q:x)","(?i:x"};
  for(size_t i=0;i<sizeof bad/sizeof *bad;i++){re=regexjs_compilar_badge(bad[i],error,sizeof error);assert(!re);}
  puts("PASS: 8000 alternatives, scoped i/m/s, nested/lookbehind/class/star modifiers, unchanged normal limit and bounded large programs");
  return NULL;
}
int main(void) {
  pthread_t thread;pthread_attr_t attributes;assert(!pthread_attr_init(&attributes));
#ifdef __SANITIZE_ADDRESS__
  size_t stack=1024*1024; // ASan adds large compiler frames; plain run checks the actual 64 KiB stack.
#else
  size_t stack=64*1024;
#endif
  assert(!pthread_attr_setstacksize(&attributes,stack));
  assert(!pthread_create(&thread,&attributes,test,NULL));pthread_attr_destroy(&attributes);
  assert(!pthread_join(thread,NULL));printf("badge regex stack: %zu KiB\n",stack/1024);
}
