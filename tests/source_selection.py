#!/usr/bin/env python3
"""Differential release scoring and source-sheet candidate cache invalidation."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    if ";" in source[start:opening]:
        return function(source[start + len(signature):], signature)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


note = (ROOT / "src/fontenota.c").read_text()
legacy = note.replace(function(note, "static const char *palavraEm("), r'''
static const char *palavraEm(const char *t, const char *de, const char *termo) {
  size_t n = strlen(termo);
  for (const char *q = de; *q; q++) {
    if (strncasecmp(q, termo, n)) continue;
    if (q > t && prende((unsigned char)q[-1])) continue;
    if (prende((unsigned char)q[n])) continue;
    return q;
  }
  return NULL;
}''').replace(function(note, "static int contem("), r'''
static int contem(const char *t, const char *termo) {
  size_t n = strlen(termo);
  for (const char *q = t; *q; q++) if (!strncasecmp(q, termo, n)) return 1;
  return 0;
}''')
legacy = legacy.replace(function(legacy, "static const char *termoEm("), "")
exports = re.findall(r"^(?:void|int|long) (fontenota_\w+)\(", note, re.M)
counter = r'''
#include <stddef.h>
#include <strings.h>
static unsigned long compares;
unsigned long @PREFIX@_comparisons(void) { unsigned long n=compares; compares=0; return n; }
static int counted(const char *a,const char *b,size_t n) { compares++;return strncasecmp(a,b,n); }
#define strncasecmp counted
'''
fixture = r'''
#include "fontenota.h"
#include <assert.h>
#include <locale.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
void legacy_fontenota_avaliar(const FonteNotaEntrada *,FonteNota *);
int legacy_fontenota_idioma(const char *,const char *);
int legacy_fontenota_bitrate_declarado(const char *);
long legacy_fontenota_somar(long,const FonteNota *,int);
unsigned long fast_comparisons(void),legacy_comparisons(void);
static unsigned rng=314159;
static unsigned randomNext(void) {rng=rng*1664525u+1013904223u;return rng;}
static const char *tokens[]={"Lupin.S04E01", "Chicago.Fire", "The.Lord.of.the.Rings", "4K", "2160p", "1080p",
 "BluRay", "REMUX", "WEB-DL", "WEBRip", "HDCAM", "DTS-HD MA", "DTS:X", "TrueHD", "DDP5.1", "HEVC",
 "Hi10P", "H.264", "10bit", "upscaled", "AV1", "Polish", "Polski", "LEKTOR", "FRENCH", "español",
 "English Subs", "ENG-SUB", "🇵🇱", "🇬🇧", "Dual Audio", "MULTI", "37.5 Mbps", "48 MB/s", ".mkv", "Cloud"};
static void check(FonteNotaEntrada *e) {
 FonteNota a,b;fontenota_avaliar(e,&a);legacy_fontenota_avaliar(e,&b);
 assert(!memcmp(&a,&b,sizeof a));
 for(int mode=0;mode<2;mode++)assert(fontenota_somar(45000,&a,mode)==legacy_fontenota_somar(45000,&b,mode));
 assert(fontenota_idioma(e->descricao,e->idiomaPref)==legacy_fontenota_idioma(e->descricao,e->idiomaPref));
 assert(fontenota_bitrate_declarado(e->descricao)==legacy_fontenota_bitrate_declarado(e->descricao));
}
int main(void) {
 const char *languages[]={"pl","en","pt-BR","fr","es","ja",""};
 const char *locales[]={"C","C.UTF-8"};
 char description[2048],filename[1024];
 for(unsigned loc=0;loc<sizeof locales/sizeof *locales;loc++) {
  if(!setlocale(LC_CTYPE,locales[loc]))continue;
  for(int sample=0;sample<2400;sample++) {
   size_t used=0;description[0]=0;
   for(int i=0;i<35;i++) {
    const char *t=tokens[randomNext()%(sizeof tokens/sizeof *tokens)];
    int written=snprintf(description+used,sizeof description-used,"%s%s",i%5==0?"\n":i%3==0?".":" ",t);
    assert(written>0 && (size_t)written<sizeof description-used);used+=(size_t)written;
   }
   snprintf(filename,sizeof filename,"%.*s.mkv",(int)sizeof filename-5,description);
   FonteNotaEntrada e={.rotulo="[TB ⚡] ST Store 4K",.descricao=description,.arquivo=filename,
    .altura=sample%4==0?2160:sample%4==1?1080:sample%4==2?720:0,
    .bytes=sample%3?UINT64_C(80000000000):0,.tamanhoMB=sample%3?0:8500,
    .duracaoSeg=sample%4?7200:0,.idiomaPref=languages[sample%7],.semHi10p=sample%2,
    .tierDica=sample%6,.audioDica=sample%4,.codecDica=sample%4,
    .temIdiomaDeclarado=sample%3==0,.idiomaDeclarado=sample%2?FN_IDIOMA_EXPLICITO:0};
   check(&e);
  }
 }
 // End-of-string/short UTF-8, NULL inputs and exact bitrate byte/bit spelling.
 for(int i=0;i<64;i++) {
  memset(description,'a',sizeof description);description[i]=0;
  FonteNotaEntrada e={.descricao=description,.idiomaPref="pl"};check(&e);
 }
 FonteNotaEntrada empty={0};check(&empty);
 fast_comparisons();legacy_comparisons();
 FonteNotaEntrada e={.rotulo="[TB ⚡] ST Store 4K",.arquivo="The.Lord.of.the.Rings.The.Fellowship.of.the.Ring.2001.Extended.INTERNAL.UHD.BluRay.2160p.TrueHD.Atmos.7.1.DV.HDR10.HEVC.REMUX-seedpool.mkv",
  .descricao="🎬 The.Lord.of.the.Rings.The.Fellowship.of.the.Ring.2001.Extended.INTERNAL.UHD.BluRay.2160p.TrueHD.Atmos.7.1.DV.HDR10.HEVC.REMUX-seedpool.mkv\n📀 REMUX\n🎨 HEVC HDR DV\n🎧 Atmos TrueHD 7.1\n🗣️🌍 🇵🇱 🇬🇧 MULTI\n💾 155.48 GB Personal Cloud",
  .altura=2160,.bytes=UINT64_C(155480000000),.duracaoSeg=12500,.idiomaPref="pl",.semHi10p=1};
 for(int i=0;i<1950;i++)check(&e);
 unsigned long fast=fast_comparisons(),old=legacy_comparisons();
 assert(fast*5<old);
 printf("PASS: 4800 mixed release descriptions match legacy scores/flags/languages; 1950-row comparisons %lu -> %lu\n",old,fast);
}
'''

source = (ROOT / "src/streams.c").read_text()
body = function(source, "static void folhaDados(void)")
getters = sorted(set(re.findall(r"\b(ajustes_\w+)\(\)", body)) - {"ajustes_qualidade"})
cache = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
enum {NV_SRC_STATE,NV_SRC_FILTER};
typedef struct {int mode;char addons[128];} FonteFalhaConfig;
static unsigned folhaDadosVersao,folhaRevisao,rules,dead,tabs;
static int n=1949,listaGeracao=1,filtro,soMp4,soCache,soDub,atual,preferida,canalFolha,pedidoVod,fitN;
static int decoder4k[4],decoderFalhaAltura[4],settings[64],nProvedores=3;
static int folhaAuto,folhaContagens[13],folhaOcultos,folhaBloqueada,folhaVisiveis,autoCalls,filterCalls;
static int lista[2000],where;static char alvoPedido[64],audio[96],quality[32];
static FonteFalhaConfig failConfig;
static uint64_t sourceperf_begin(void){return 0;}
static void sourceperf_end(int s,uint64_t t){(void)s;(void)t;}
static unsigned fonteregra_versao(void){return rules;}
static unsigned fontesmortas_versao(void){return dead;}
static unsigned addons_parcial_rev(void){return tabs;}
static const char *idiomaFiltroAudio(void){return audio;}
static const char *ajustes_qualidade(void){return quality;}
static void fontefalha_config(FonteFalhaConfig *c){*c=failConfig;}
static int ondever_n(const char *s){(void)s;return where;}
static void atualizarProvedores(void){}
static int passaChips(int i){filterCalls++;return !soDub || i%2==0;}
static int doProvedor(const int *s,int p){(void)s;(void)p;return 1;}
static int stream_filtro_permite(const int *s){(void)s;return 1;}
static int chipsOk(int i){return !soDub || i%2==0;}
static int stream_filtro_bloqueou(void){return 0;}
static int filtrosVisiveisCalcular(void){return 1;}
static int automaticoDaFolhaModoCalcular(void){autoCalls++;return 123;}
@GETTERS@
@BODY@
static void fresh(void){int before=autoCalls;folhaDados();assert(autoCalls==before+1 && folhaAuto==123);}
int main(void){
 fresh();int before=autoCalls;
 for(int i=0;i<120;i++)folhaDados();
 assert(autoCalls==before);
 for(int i=0;i<50;i++) {
  filtro=i%3;soDub=i%2;soMp4=(i/2)%2;soCache=(i/3)%2;tabs++;
  int passes=filterCalls;folhaDados();assert(autoCalls==before && filterCalls>passes);
  assert(folhaContagens[0]==(soDub?(n+1)/2:n));
 }
 folhaRevisao++;fresh();n++;fresh();listaGeracao++;fresh();rules++;fresh();dead++;fresh();
 fitN++;fresh();atual++;fresh();preferida++;fresh();canalFolha++;fresh();pedidoVod++;fresh();
 decoder4k[0]++;fresh();decoderFalhaAltura[3]++;fresh();failConfig.mode++;fresh();
 strcpy(audio,"pl");fresh();strcpy(quality,"1080p");fresh();where++;fresh();
 for(int i=0;i<@COUNT@;i++){settings[i]++;fresh();}
 puts("PASS: tabs/chips/addon completion reuse sheet candidate; source/rule/filter/decoder/preferences still invalidate it");
}
'''
cache = cache.replace("@GETTERS@", "\n".join(f"static int {name}(void){{return settings[{i}];}}"
                                                   for i, name in enumerate(getters)))
cache = cache.replace("@BODY@", body).replace("@COUNT@", str(len(getters)))
with tempfile.TemporaryDirectory(prefix="nuvio-source-selection-") as directory:
    temp = Path(directory)
    flags = ["-std=c11", "-O2", "-g", "-Wall", "-Wextra", "-Isrc"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    files = []
    for prefix, text in [("fast", note), ("legacy", legacy)]:
        renames = "" if prefix == "fast" else "\n".join(f"#define {name} legacy_{name}" for name in exports)
        path = temp / (prefix + ".c")
        path.write_text(counter.replace("@PREFIX@", prefix) + renames + "\n" + text)
        files.append(str(path))
    test = temp / "test.c"
    test.write_text(fixture)
    binary = str(temp / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, *files, str(test), "src/linguas.c", "-o", binary],
                   check=True, cwd=ROOT)
    subprocess.run([binary], check=True)
    test.write_text(cache)
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(test), "-o", binary], check=True, cwd=ROOT)
    subprocess.run([binary], check=True)
