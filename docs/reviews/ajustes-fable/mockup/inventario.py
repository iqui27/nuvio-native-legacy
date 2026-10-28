import re,sys,json,glob
src=open('src/ajustes.c',encoding='utf-8').read()
# enum
m=re.search(r'typedef enum \{(.*?)\} OpcaoId;',src,re.S)
ids=[]
for line in m.group(1).splitlines():
    for t in re.findall(r'\b(AJ_[A-Z0-9_]+)\b',line.split('//')[0]):
        if t!='AJ_N': ids.append(t)
# OPCOES table
m=re.search(r'static const Opcao OPCOES\[AJ_N\] = \{(.*?)\n\};',src,re.S)
rows=[]
for line in m.group(1).splitlines():
    code=line.split('//')[0].strip()
    mm=re.match(r'(ESC|NUM|LER|ACAO)\((".*?")',code)
    if mm: rows.append((mm.group(1),json.loads(mm.group(2).replace('\\"','"')) if False else mm.group(2).strip('"').replace('\\"','"')))
print(len(ids),len(rows),file=sys.stderr)
# v4 map
mapa=open('src/ajustes_ux_v4_mapa.inc',encoding='utf-8').read()
grupos=re.findall(r'"([^"]+)",',mapa.split('enum')[0])
def parse(fn):
    body=re.search(fn+r'\(int op\) \{(.*?)\n\}',mapa,re.S).group(1)
    d={}
    for cases,ret in re.findall(r'((?:case AJ_[A-Z0-9_]+:\s*)+)\s*return (-?\d+);',body):
        for c in re.findall(r'AJ_[A-Z0-9_]+',cases): d[c]=int(ret)
    return d
sec=parse('aj4MapaSecao');grp=parse('aj4MapaGrupo')
SEC=['Reprodução','Áudio e legendas','Fontes e addons','TV ao vivo','Tela inicial','Visual','Perfil e serviços','Sobre e desempenho']
# pen pages
pen={}
for f in glob.glob('src/ajustes_pen*.inc'):
    for t in re.findall(r'\{(AJ_[A-Z0-9_]+),"([^"]*)","([^"]*)","([^"]*)",(\d)\}',open(f,encoding='utf-8').read()):
        pen.setdefault(t[0],[]).append((f.split('/')[-1],t[1],t[2],t[3]))
out=[]
for i,idn in enumerate(ids):
    tipo,rot=rows[i] if i<len(rows) else ('?','?')
    p=pen.get(idn)
    out.append(dict(i=i,id=idn,tipo=tipo,rotulo=rot,secao=SEC[sec[idn]] if idn in sec else '-',grupo=grupos[grp[idn]] if idn in grp else '-',pen=p[0][1] if p else '',pen_ajuda=p[0][2] if p else '',pen_icone=p[0][3] if p else ''))
json.dump(out,open(sys.argv[1],'w',encoding='utf-8'),ensure_ascii=False,indent=0)
import collections
print('sem secao v4:',[o['id'] for o in out if o['secao']=='-'],file=sys.stderr)
print('sem pagina pen:',len([o for o in out if not o['pen']]),file=sys.stderr)
print(collections.Counter(o['tipo'] for o in out),file=sys.stderr)
