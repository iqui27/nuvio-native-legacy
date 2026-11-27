#!/usr/bin/env python3
"""Real language matching: legacy equivalence, bounded cache and invalidation."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "src/streams.c").read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    if ";" in source[start:opening]:
        start = source.index(signature, start + len(signature))
        opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


matching = function("static int audioDeclaradoEm(")
old = matching.replace("static int audioDeclaradoEm(", "static int legacyAudio(")
begin = old.index("    // Parser hints")
end = old.index("    if (!cod && faixaIdiomaEtiqueta", begin)
old = old[:begin] + "    const char *cod = faixaIdiomaDoNome(raw);\n" + old[end:]
start = source.index("// Raw indices survive addon appends.")
cache = source[start:source.index("static const char *ajudaFiltroAudio(", start)]
fixture = r'''
#include "linguas.h"
#include <assert.h>
#include <ctype.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
typedef struct {char audioIdiomas[512];} Stream;
static Stream *lista;static int n,aberta;static unsigned listaGeracao=1;static _Atomic unsigned audioFiltroRevisao;
static const char *preference="pl";
static const char *idiomaFiltroAudio(void){return preference;}
static int evaluations,failAlloc;
static void *controlledRealloc(void *p,size_t bytes){return failAlloc?NULL:realloc(p,bytes);}
@NAME@
@TAG@
@MATCH@
@OLD@
@UNCACHED@
static int counted(const Stream *s,const char *p){evaluations++;return temAudioPreferidoSemCache(s,p);}
#define temAudioPreferidoSemCache counted
#define realloc controlledRealloc
@CACHE@
#undef realloc
#undef temAudioPreferidoSemCache
static Stream row(const char *text){Stream s;memset(&s,0,sizeof s);snprintf(s.audioIdiomas,sizeof s.audioIdiomas,"%s",text);return s;}
int main(void){
 const char *tokens[]={"pl","PL","pol","pl-PL","en","eng","pt-BR","pt_PT","pob","por","Português (PT)",
 "Portuguese (Brazil)","Polish","Polski","🇵🇱","French","fr","fre","Türkçe","tr","tur","Thai","th",
 "English, Polish, German","unknown","",",,", "eng,pol,spa,fr,de,ita,tr,pt-BR,ces,sv,ru", "Subtitles: Polish"};
 const char *prefs[]={"pl","en","fr","tr","pt","pt-BR","pt-PT","th","ja","","*"};
 for(unsigned i=0;i<sizeof tokens/sizeof *tokens;i++)for(unsigned k=0;k<sizeof prefs/sizeof *prefs;k++) {
  Stream s=row(tokens[i]);
  assert(audioDeclaradoEm(&s,prefs[k])==legacyAudio(&s,prefs[k]));
 }
 Stream full;memset(full.audioIdiomas,'a',sizeof full.audioIdiomas);
 assert(!audioDeclaradoEm(&full,"pl")); // deliberately unterminated field
 lista=calloc(2400,sizeof *lista);assert(lista);n=1950;aberta=1;
 for(int i=0;i<2400;i++)lista[i]=row(i%2?tokens[i%(sizeof tokens/sizeof *tokens)]:"");
 for(int pass=0;pass<20;pass++)for(int i=0;i<n;i++)
  assert(temAudioPreferido(&lista[i])==temAudioPreferidoSemCache(&lista[i],preference));
 assert(evaluations==1950 && audioFiltroCache.cap==1950);
 n=2000;
 for(int i=0;i<n;i++)assert(temAudioPreferido(&lista[i])==temAudioPreferidoSemCache(&lista[i],preference));
 assert(evaluations==2000); // append retains the first 1950 results
 preference="en";
 for(int i=0;i<n;i++)assert(temAudioPreferido(&lista[i])==temAudioPreferidoSemCache(&lista[i],preference));
 assert(evaluations==4000);
 listaGeracao++;strcpy(lista[0].audioIdiomas,"English");
 assert(temAudioPreferido(&lista[0]) && evaluations==4001);
 // A renewed source keeps its index and list generation; the revision drops the old answer.
 strcpy(lista[0].audioIdiomas,"Polish");audioFiltroRevisao++;
 assert(!temAudioPreferido(&lista[0]) && evaluations==4002);
 evaluations--;
 Stream external=row("Polish");
 assert(!temAudioPreferido(&external) && evaluations==4002);
 aberta=0;strcpy(lista[0].audioIdiomas,"Polish");
 assert(!temAudioPreferido(&lista[0]) && evaluations==4003); // temporary/closed lists stay live
 audioFiltroDescartar();assert(!audioFiltroCache.match && !audioFiltroCache.cap);
 aberta=1;failAlloc=1;preference="pl";
 assert(temAudioPreferido(&lista[0]) && !audioFiltroCache.match);
 failAlloc=0;assert(temAudioPreferido(&lista[0]));
 n=1;listaGeracao++;strcpy(lista[0].audioIdiomas,"English");assert(!temAudioPreferido(&lista[0]));
 preference="";assert(temAudioPreferido(&lista[0])); // "All" matches every row
 audioFiltroDescartar();free(lista);lista=NULL;n=0;
 puts("PASS: legacy language/alias matching; 39000 repeated checks need only 1950 evaluations; append, replace, renewal, preference, external rows, allocation failure and cleanup");
}
'''
for marker, code in {
    "NAME": function("static const char *faixaIdiomaDoNome("),
    "TAG": function("static size_t faixaIdiomaEtiqueta("), "MATCH": matching, "OLD": old,
    "UNCACHED": function("static int temAudioPreferidoSemCache("), "CACHE": cache,
}.items():
    fixture = fixture.replace("@" + marker + "@", code)
with tempfile.TemporaryDirectory(prefix="nuvio-source-audio-cache-") as directory:
    path = Path(directory) / "test.c"
    path.write_text(fixture)
    flags = ["-std=gnu11", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-Isrc"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(Path(directory) / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, str(path), "src/linguas.c", "-o", binary], check=True, cwd=ROOT)
    subprocess.run([binary], check=True)
