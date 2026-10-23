#!/bin/bash
# Contrato do #411: o host 4/5 reserva a mesma lista do 6+, sem perder eventos.
# Nao prova keygrab nem o desaparecimento do aviso na TV.
set -eu
cd "$(dirname "$0")/.."
python3 - <<'PY'
import re
from pathlib import Path

legacy = Path('tizen-tpk/NuvioTpk40/Program40.cs').read_text()
modern = Path('tizen-tpk/Program.cs').read_text()

def keys(source):
    match = re.search(r'static readonly string\[\] TeclasMidia\s*=\s*\{(.*?)\};', source, re.S)
    assert match, 'FALHA: host sem lista TeclasMidia'
    return re.findall(r'"([^"]+)"', re.sub(r'//[^\n]*', '', match[1]))

old, new = keys(legacy), keys(modern)
assert old and len(old) == len(set(old)), 'FALHA: lista vazia ou duplicada'
assert set(old) == set(new), f'FALHA: faltam {set(new)-set(old)}, sobram {set(old)-set(new)}'
assert {'XF86AudioPlayPause', 'XF86PlayBack'} <= set(old), 'FALHA: aliases Play/Pause ausentes'

update = legacy.split('protected override bool OnUpdate()', 1)[1].split('protected override void OnKeyEvent', 1)[0]
assert re.search(r'if\s*\(!teclasReservadas\)\s*\{\s*teclasReservadas\s*=\s*true;\s*ReservaTeclasMidia\(\);', update), 'FALHA: reserva deve ocorrer uma vez'
assert update.index('if (!rodando)') < update.index('ReservaTeclasMidia();') < update.index('NvLib.Quadro()'), 'FALHA: reserva fora do arranque da janela GL'

handler = legacy.split('protected override void OnKeyEvent(GLKey k)', 1)[1].split('protected override void OnPause()', 1)[0]
assert 'if (rodando) NvLib.Tecla(k.KeyPressedName, k.State == GLKey.StateType.Down ? 1 : 0);' in handler, 'FALHA: perdeu encaminhamento down/up'
assert 'IntPtr pTec = resolve("nv_tpk_tecla");' in legacy, 'FALHA: perdeu destino nativo'
assert 'const int TOPMOST = 2;' in legacy, 'FALHA: modo incorreto'
for backend in ('ecore_wl', 'ecore_wl2'):
    assert f'{backend}_window_keygrab_set(w, k, 0, 0, 0, TOPMOST)' in legacy, f'FALHA: reserva ausente em {backend}'
for library in ('libecore_wayland.so.1', 'libecore_wl2.so.1'):
    assert re.search(r'\[DllImport\("' + re.escape(library) + r'"\)\] static extern byte \w+_keygrab_set', legacy), f'FALHA: ABI Eina_Bool/soname de {library}'
print(f'PASS: {len(old)} teclas iguais ao 6+, reserva unica TOPMOST wl/wl2, encaminhamento down/up preservado')
PY
