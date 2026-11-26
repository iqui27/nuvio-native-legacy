#!/usr/bin/env python3
"""Differential source ordering, without SDL/HTTP."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


source = (ROOT / "src/streams.c").read_text()
fixture = r'''
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define CAP 70032
static unsigned keys[CAP], *chave=keys;
static int order[CAP], *exib=order, expected[CAP], scratch[CAP];
typedef struct {long tamanhoMB;} Stream;
static Stream rows[CAP], *lista=rows;
@APPEND@
@SIZE@
static unsigned long sizeCalls;
static long countedSize(int i){sizeCalls++;return tamanhoOrdem(i);}
#define tamanhoOrdem countedSize
@SORT@
#undef tamanhoOrdem
static unsigned randomState=314159;
static unsigned nextRandom(void){randomState=randomState*1664525u+1013904223u;return randomState;}
static void legacyAppend(int before,int added){
 for(int k=0;k<added;k++){
  int i=before+k,pos=i;
  while(pos>0 && keys[expected[pos-1]]>keys[i]){expected[pos]=expected[pos-1];pos--;}
  expected[pos]=i;
 }
}
static void append(int before,int added,int rank){
 for(int i=0;i<added;i++)keys[before+i]=((unsigned)(rank<0?0:rank)<<16)|(unsigned)(i&0xffff);
 legacyAppend(before,added);ordemAcrescentar(before,added);
 assert(!memcmp(order,expected,(size_t)(before+added)*sizeof *order));
 for(int i=1;i<before+added;i++)assert(keys[order[i-1]]<=keys[order[i]]);
}
static void appendCases(void){
 append(0,262,4);append(262,1607,0);append(1869,25,2);
 append(1894,57,0);append(1951,0,1);append(1951,49,-1);
 for(int round=0;round<120;round++){
  int count=0;
  for(int batch=0;batch<12;batch++){
   int amount=(int)(nextRandom()%180);
   append(count,amount,(int)(nextRandom()%7));count+=amount;
  }
 }
 // Exercise the original 16-bit key limit and its wrapped fallback, too.
 append(0,65536,0);append(65536,1,0);
 append(0,65537,0);
 puts("PASS: late addon batches, duplicate keys, stable ties, empty batches and 65536-key boundary match legacy order");
}
static unsigned long legacySize(int *v,int count){
 unsigned long calls=0;
 for(int k=1;k<count;k++){
  int value=v[k],j=k-1;
  while(j>=0){calls+=2;if(tamanhoOrdem(v[j])>=tamanhoOrdem(value))break;v[j+1]=v[j];j--;}
  v[j+1]=value;
 }
 return calls;
}
static void sizes(void){
 unsigned long oldWork=0,newWork=0;
 for(int round=0;round<150;round++){
  int count=round==0?1950:(int)(nextRandom()%5001);
  for(int i=0;i<count;i++){
   order[i]=expected[i]=i;
   rows[i].tamanhoMB=round==0?i:(long)(nextRandom()%31)-3;
   if(round && i%19==0)rows[i].tamanhoMB=LONG_MAX;
  }
  unsigned long old=legacySize(expected,count);sizeCalls=0;
  ordenarTamanho(order,scratch,count);
  assert(!memcmp(order,expected,(size_t)count*sizeof *order));
  if(round==0){oldWork=old;newWork=sizeCalls;}
 }
 assert(newWork*20<oldWork);
 printf("PASS: descending sizes/unknowns/LONG_MAX preserve all ties; 1950-row adversarial size lookups %lu -> %lu\n",oldWork,newWork);
}
int main(void){appendCases();sizes();return 0;}
'''
for key, signature in [("APPEND", "static void ordemAcrescentar("),
                       ("SIZE", "static long tamanhoOrdem("),
                       ("SORT", "static void ordenarTamanho(")]:
    fixture = fixture.replace("@" + key + "@", function(source, signature))
with tempfile.TemporaryDirectory(prefix="nuvio-source-scroll-") as directory:
    temp = Path(directory)
    (temp / "test.c").write_text(fixture)
    flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(temp / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(temp / "test.c"),
                    "-lm", "-o", binary], check=True)
    subprocess.run([binary], check=True)
