#!/usr/bin/env python3
# #759(a): informarTela nao pode depender so do defaultDisplay. Quando a lista
# de tipos HDR vem vazia dele, o app consulta TODOS os displays do
# DisplayManager antes de declarar -1. O teste extrai o bloco de producao de
# NvPlayer.kt e reproduz a selecao de tipos sobre displays simulados.
import re, sys

FONTE = 'android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt'
src = open(FONTE, encoding='utf-8').read()
bloco = src[src.index('private fun informarTela'):src.index('private fun informarAudio')]

assert 'hdrCapabilities?.supportedHdrTypes' in bloco
consulta_dm = 'DISPLAY_SERVICE' in bloco and 'displays' in bloco
manda_menos1 = 'nativeTela(-1, -1)' in bloco
manda_1 = 'nativeTela(1, dv)' in bloco
print(f'consulta DisplayManager={consulta_dm} -1 no vazio={manda_menos1} 1 com tipos={manda_1}')

def tipos_efetivos(default_tipos, outros_displays):
    """Mesma regra da producao: default primeiro; se vazio, uniao dos demais."""
    if consulta_dm:
        if not default_tipos:
            unidos = []
            for t in outros_displays:
                unidos += t
            return sorted(set(unidos)) if unidos else []
        return default_tipos
    return default_tipos

CASOS = [
    # (default, outros, esperado)
    ([2, 4], [], [2, 4]),        # default ja responde: nao muda
    ([], [[3, 4]], [3, 4]),      # vazio no default, HDMI HDR pelo DM: agora conta
    ([], [[], []], []),          # tudo vazio: continua -1
]
rc = 0
for d, o, esp in CASOS:
    got = tipos_efetivos(d, o)
    ok = got == esp
    rc |= not ok
    print(f'{"OK " if ok else "FALHA"} default={d} outros={o} -> {got}')
sys.exit(rc or (not (consulta_dm and manda_menos1 and manda_1)))
