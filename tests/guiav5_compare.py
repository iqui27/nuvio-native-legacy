"""Build mock | native comparisons from real app pixels using ImageMagick."""
from pathlib import Path
import subprocess
import sys
out = Path(sys.argv[1] if len(sys.argv)>1 else '/Volumes/ExternalSSD/tmp/rev/guiav5-png')
mock = Path('/Volumes/ExternalSSD/tmp/rev/guiatv-estado/v5')
for path in sorted(out.glob('*-app.png')):
    name = path.name.removesuffix('-app.png')
    info = subprocess.check_output(['magick', str(path), '-format', '%w %h %[fx:standard_deviation]', 'info:'],text=True).split()
    mobile=name.startswith('celular-')
    valid_size=(info[0]=='390' and int(info[1])>=844) if mobile else info[:2]==['1920','1080']
    assert valid_size and float(info[2])>.05, f'Blank or wrong-size image: {path}'
    if mobile and info[1]!='844':print(f'GAP: {name} height {info[1]} vs mock 844; full form preserved')
    subprocess.run(['magick',str(mock/f'{name}.png'),str(path),'+append',str(out/f'{name}-compare.png')],check=True)
print('PASS: native screenshots nonblank; mock | app comparisons written')
