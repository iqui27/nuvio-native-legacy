#!/usr/bin/env python3
import re
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

with tempfile.TemporaryDirectory() as tmp:
    source = Path(tmp) / 'size.c'
    binary = Path(tmp) / 'size'
    source.write_text('#include <stdio.h>\n#include "catalogo.h"\nint main(void){printf("%zu",sizeof(CatItem));}\n')
    subprocess.run(['cc', '-Isrc', str(source), '-o', str(binary)], cwd=ROOT, check=True)
    item_size = int(subprocess.check_output([str(binary)], text=True))

assert item_size * 120 > 2 * 1024 * 1024
assert item_size * 48 > 256 * 1024

checks = {
    'src/listas.c': r'CatItem\s+lote\s*\[LST_ITENS_MAX\]',
    'src/descoberta.c': r'CatItem\s+lote\s*\[48\]',
}
for name, pattern in checks.items():
    assert not re.search(pattern, (ROOT / name).read_text()), f'{name} keeps a large CatItem batch on a worker stack'

print(f'ok: large CatItem worker batches use heap storage (sizeof={item_size})')
