#!/bin/bash
# O overlay de texto simples do .tpk (#269) so funciona se mkvass_aceitar_texto(1)
# for chamada. video_iniciar() nao roda no .tpk (so o ramo da LG de video.c a
# chama), entao a chamada tem de estar em abrirSessao(), que roda a cada fonte.
# Em 2.0.1 a flag ficava em 0: 125 logs de 13 pessoas com "no-go: faixa nao e
# ASS" em faixa S_TEXT/UTF8 e nenhum "e texto simples".
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re,sys
s=open('src/video_tpk.c').read()
m=re.search(r'static int abrirSessao\(void\) \{(.*?)\n\}\n',s,re.S)
if not m or 'mkvass_aceitar_texto(1)' not in m.group(1):
    print('FALHA: abrirSessao() nao liga mkvass_aceitar_texto(1)'); sys.exit(1)
print('PASS: abrirSessao() liga o texto simples do overlay')
PY
