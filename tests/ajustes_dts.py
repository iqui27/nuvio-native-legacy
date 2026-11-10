#!/usr/bin/env python3
"""Check the DTS settings catalog and real settings persistence/sync paths."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]

# Availability is controlled by the UI; the catalog contains one row on each platform.
for macro in [None, 'NV_WEBOS', 'NV_TPK', 'NV_ANDROID']:
    command = [os.environ.get('CC', 'cc'), '-E', '-P', '-x', 'c']
    if macro:
        command.append('-D' + macro)
    command.append(str(ROOT / 'src/ajustes_ux_tela.inc'))
    catalog = subprocess.check_output(command, text=True)
    assert catalog.count('OPC(AJ_DTS_AC3)') == 1, macro

# This compiles the real settings implementation, including device/account sync.
subprocess.run(['bash', str(ROOT / 'tests/ajustes_ux_dados.sh')], cwd=ROOT, check=True)
print('ajustes_dts: catalog and settings integration passed')
