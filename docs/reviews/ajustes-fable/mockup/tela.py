import re,json,sys
txt=open('src/ajustes_ux_tela.inc',encoding='utf-8').read()
inv={o['id']:o for o in json.load(open(sys.argv[1]))}
m=re.search(r'static const Item TELA\[\] = \{(.*?)\n\};',txt,re.S)
secs=[];cur=None;rot=None;mais=False;cond=[]
seen=set()
for line in m.group(1).splitlines():
    s=line.strip()
    if s.startswith('#if'): cond.append(s); continue
    if s.startswith('#endif'): cond.pop() if cond else None; continue
    if s.startswith('#else'): cond[-1]='!'+cond[-1] if cond else None; continue
    code=s.split('//')[0]
    for kind,arg in re.findall(r'\b(SEC|ROT|OPC|MAIS)\(([^)]*)\)',code):
        if kind=='SEC':
            a=re.findall(r'"([^"]*)"',arg); cur={'sec':a[0],'sub':a[1],'icone':a[2] if len(a)>2 else '','grupos':[]}; secs.append(cur); mais=False; rot=None
        elif kind=='MAIS': mais=True
        elif kind=='ROT':
            rot={'rot':re.findall(r'"([^"]*)"',arg)[0],'avancado':mais,'ops':[]}; cur['grupos'].append(rot)
        elif kind=='OPC':
            if rot is None: rot={'rot':'','avancado':mais,'ops':[]}; cur['grupos'].append(rot)
            o=inv.get(arg,{'tipo':'?','rotulo':'?'})
            seen.add(arg)
            rot['ops'].append({'id':arg,'tipo':o['tipo'],'rotulo':o['rotulo'],'cond':' '.join(cond)})
json.dump(secs,open(sys.argv[2],'w'),ensure_ascii=False,indent=1)
n=0
for s in secs:
    k=sum(len(g['ops']) for g in s['grupos']); n+=k
    print(f"== {s['sec']} ({k}) — {s['sub']} [{s['icone']}]")
    for g in s['grupos']:
        print(f"   {'[AVANÇ] ' if g['avancado'] else ''}{g['rot']} ({len(g['ops'])}): "+', '.join(o['id'][3:]+('*' if o['cond'] else '')+(':'+o['tipo'][0] if o['tipo']!='ESC' else '') for o in g['ops']))
print('TOTAL em TELA:',n,'de',len(inv))
print('FORA da TELA:',' '.join(i for i in inv if i not in seen))
