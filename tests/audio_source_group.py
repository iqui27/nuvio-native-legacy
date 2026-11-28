#!/usr/bin/env python3
"""Remembered audio sees the selected source before synchronous load callbacks."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
source = (root / "src/app.c").read_text()


def function(signature):
    start = source.index(signature)
    opening = source.index("{", start)
    # Skip a forward declaration when present.
    if ";" in source[start:opening]:
        start = source.index(signature, start + len(signature))
        opening = source.index("{", start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


fixture = r'''
#include "audioescolha.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
typedef struct { char url[128], bingeGroup[128]; } Stream;
static Stream current = {.url="https://example.invalid/episode"}, voltaFonte;
static int selected, fonteVODRetentou, vodRenovou;
static int stream_atual(void) { return selected; }
static const Stream *stream_item(int i) { return i==0 ? &current : NULL; }
static void provaZerar(void) {}
static AudioCand tracks[] = {
 {"pl","AC3","Polish AC3 2.0",2,0,1,0},
 {"pl","AC3","Polish AC3 2.0",2,0,1,1}
};
static void player_definir_fonte(const char *url) {
 assert(url && *url);
 assert(audioesc_escolher(tracks,2,NULL)==1);
}
@GROUP@
@OPEN@
static void remember(const char *group) {
 audioesc_fonte_binge(group);
 audioesc_lembrar_faixa("pl","AC3","Polish AC3 2.0",2,1);
 audioesc_fonte_binge("previous-episode");
}
int main(void) {
 audioesc_titulo("tt123");
 strcpy(current.bingeGroup,"manual-release");
 remember(current.bingeGroup);
 definirFonteVOD(current.url);
 selected=-1;
 strcpy(voltaFonte.url,"https://example.invalid/saved");
 strcpy(voltaFonte.bingeGroup,"saved-release");
 remember(voltaFonte.bingeGroup);
 definirFonteVOD(voltaFonte.url);
 strcpy(voltaFonte.bingeGroup,"another-saved-release");
 remember(voltaFonte.bingeGroup);
 definirFonteVOD(voltaFonte.url);
 puts("PASS: manual and consecutive saved sources publish audio group before load");
}
'''
fixture = fixture.replace("@GROUP@", function("static void bingeDaFonteAtual("))
fixture = fixture.replace("@OPEN@", function("static void definirFonteVOD("))
with tempfile.TemporaryDirectory(prefix="nuvio-audio-group-") as directory:
    temp = Path(directory)
    (temp / "test.c").write_text(fixture)
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror", "-g"]
    if os.environ.get("SANITIZE") == "1":
        flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    binary = str(temp / "test")
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-I" + str(root / "src"),
                    str(temp / "test.c"), str(root / "src/audioescolha.c"),
                    str(root / "src/audioinfo.c"), str(root / "src/linguas.c"),
                    "-o", binary], check=True)
    subprocess.run([binary], check=True)
