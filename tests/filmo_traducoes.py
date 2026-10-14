import subprocess,re,json
from pathlib import Path
diff=subprocess.check_output(['git','show','861c05ca','--','src/idioma_fr.h'],text=True)
pairs=[json.loads('['+line[5:-2]+']') for line in diff.splitlines() if line.startswith('+  T(')]
for path in Path('src').glob('idioma_*.h'):
 if path.name=='idioma_tab.h':continue
 table={}
 for line in path.read_text().splitlines():
  if not line.strip().startswith('T('):continue
  try:key,value=json.loads('['+line.strip()[2:-2]+']');table[key]=value
  except json.JSONDecodeError:continue
 for key,english in pairs:
  value=table[key]
  assert value!=english or key in ['Família · Wikidata','Participação'],(path,key,value)
  assert sorted(re.findall(r'%[ds]',value))==sorted(re.findall(r'%[ds]',english)),(path,key,value)
assert 'Apparitions masquées' in Path('src/idioma_fr.h').read_text()
print('28 filmography translations and placeholders: PASS')
