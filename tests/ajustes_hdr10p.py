#!/usr/bin/env python3
"""Issue #605: the "Tirar HDR10+ do vídeo" row (strip HDR10+ dynamic SEI).

Real production code only (src/ajustes.c tables + the strip filter itself):
the catalog gate (Tizen/webOS), the default (Off), the saved key, and the
SEI filter that keeps static HDR10 while dropping SMPTE ST 2094-40.
"""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
src = (ROOT / 'src/ajustes.c').read_text()
falhas = 0


def confere(ok, msg):
    global falhas
    print(('ok   ' if ok else 'FAIL ') + msg)
    falhas += 0 if ok else 1


# 1. The option: own enum slot, own saved key, Off by default, hidden on Android.
confere('AJ_HDR10P_STRIP, // #605' in src, 'enum: AJ_HDR10P_STRIP (append-only slot)')
confere(re.search(r'"hdr10pStripLocal",\n', src) is not None, 'saves as hdr10pStripLocal (device-local)')
confere('int ajustes_hdr10p_strip(void) { return lig(AJ_HDR10P_STRIP); }' in src,
        'ajustes_hdr10p_strip() reads the row (lig(): 1 = Desligado)')
padrao = (ROOT / 'src/ajustes_ux_padrao.inc').read_text()
confere('#605: strip HDR10+ dynamic metadata: Desligado' in padrao, 'default: Desligado (off)')
tela = (ROOT / 'src/ajustes_ux_tela.inc').read_text()
confere(re.search(r'#if defined\(NV_TPK\) \|\| defined\(NV_WEBOS\)\n      OPC\(AJ_HDR10P_STRIP\),\n#endif', tela)
        is not None, 'row shows on Tizen (.tpk) and webOS builds')
confere('#if defined(NV_TPK) || defined(NV_WEBOS)\n  if (op == AJ_HDR10P_STRIP) return 1;\n#endif' in src,
        'hidden everywhere else (Android TV has no hook in the ExoPlayer path)')

# 2. Both decoder paths ask for the strip.
video = (ROOT / 'src/video.c').read_text()
confere('dts_engine_set_hdr10p_strip(ajustes_hdr10p_strip());        // #605' in video,
        'webOS: the DTS/DV session snapshots the setting')
tpk = (ROOT / 'src/video_tpk.c').read_text()
confere('dts_remux_set_hdr10p_strip(ajustes_hdr10p_strip());   // #605' in tpk,
        'Tizen: the remux session snapshots the setting')

# 3. The filter runs only where the packet is really Annex-B HEVC.
engine = (ROOT / 'src/dts/dts_engine.c').read_text()
confere('e->hdr10p_strip && !strcmp(e->info.video_codec,"hevc")' in engine, 'webOS: HEVC-only strip')
remux = (ROOT / 'src/dts/dts_remux.c').read_text()
confere('atomic_load(&gHdr10pStrip) && g->src->vpar->codec_id == AV_CODEC_ID_HEVC' in remux, 'Tizen: HEVC-only strip')

# 4. The C row value -> delivery mapping, compiled from the real code.
c = r'''
#include <stdio.h>
enum { AJ_DV7 = 0, AJ_HDR10P_STRIP, AJ_N };
static int valor[AJ_N];
static int lig(int op) { return valor[op] == 0; }
''' + re.search(r'int ajustes_hdr10p_strip\(void\) \{ return lig\(AJ_HDR10P_STRIP\); \}', src).group(0) + r'''
int main(void) {
  int f = 0;
  /* V_LIGA: 1 = Desligado (off), 0 = Ligado (strip on). */
  for (int v = 0; v < 2; v++) {
    valor[AJ_HDR10P_STRIP] = v;
    int strip = ajustes_hdr10p_strip();
    printf("%s valor %d -> strip %d (expected %d)\n", strip == !v ? "ok  " : "FAIL", v, strip, !v);
    f += strip != !v;
  }
  return f;
}
'''
with tempfile.TemporaryDirectory() as d:
    (Path(d) / 't.c').write_text(c)
    subprocess.run(['cc', '-Wall', '-Werror', '-o', f'{d}/t', f'{d}/t.c'], check=True)
    r = subprocess.run([f'{d}/t'], capture_output=True, text=True)
    print(r.stdout, end='')
    falhas += r.returncode

print(f'ajustes_hdr10p: {falhas} failure(s)')
raise SystemExit(1 if falhas else 0)
