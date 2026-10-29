from pathlib import Path
import subprocess,sys
out=Path(sys.argv[1]);mocks=Path('/Volumes/ExternalSSD/tmp/addon-guia-mock')
files=sorted(out.glob('*-app.png'));assert len(files)==168,len(files)
for file in files:
    screen=file.stem.removesuffix('-app').rsplit('-',2)[0]
    mock=mocks/(screen+'.png')
    for p in (file,mock):
        size=subprocess.check_output(['magick','identify','-format','%wx%h',str(p)],text=True)
        assert size=='1920x1080',(p,size)
    std=float(subprocess.check_output(['magick',str(file),'-crop','1120x620+400+180','-format','%[fx:standard_deviation]','info:'],text=True))
    assert std>0.04,(file,std)
    pair=out/(file.stem.removesuffix('-app')+'-side-by-side.png')
    subprocess.run(['magick','-limit','thread','1',str(mock),str(file),'+append',
                    '-define','png:compression-level=1',str(pair)],check=True)
print('168 native PNGs: six scales, both materials, dimensions/content and side-by-side OK')
