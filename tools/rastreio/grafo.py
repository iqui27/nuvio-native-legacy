#!/usr/bin/env python3
"""Grafo issue<->codigo do Nuvio nativo (so stdlib; NAO usa o graphify: ele
precisa de LLM para docs e aqui o que importa e o elo issue->commit->funcao->teste).

  python3 tools/rastreio/grafo.py [construir]        # grava graphify-out/issues/grafo.json + RELATORIO.md
  python3 tools/rastreio/grafo.py depende --arquivo src/x.c | --funcao nome
  python3 tools/rastreio/grafo.py sem-codigo         # issues abertas sem codigo tocado
  python3 tools/rastreio/grafo.py sem-teste          # consertada/lancada sem teste

Nos: issue, commit, file, function, test. Arestas: conserta (issue->commit),
toca (commit->file/function), compila (test->file), chama (test->function),
inclui (file->file), prova (issue->test: teste adicionado/alterado no commit de
conserto, ou "teste (#N)" no assunto) e declara (issue->test: campo "teste" do
mapa.json). Mesma HEAD => mesma saida (tudo ordenado, sem datas).
"""
import json, os, re, subprocess, sys, collections

RAIZ = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True, cwd=os.path.dirname(os.path.abspath(__file__))).strip()
SAIDA = os.path.join(RAIZ, 'graphify-out', 'issues')
GRAFO = os.path.join(SAIDA, 'grafo.json')
CONSERTADAS = ('consertada-nao-lancada', 'lancada')

def git(*a, check=True):
    r = subprocess.run(['git', '-C', RAIZ, *a], capture_output=True, text=True, errors='replace')
    if check and r.returncode: raise RuntimeError('git %s: %s' % (' '.join(a), r.stderr.strip()))
    return r.stdout

# ---- indice de funcoes C ----------------------------------------------------
NAO_FUNC = {'if', 'for', 'while', 'switch', 'return', 'sizeof', 'else', 'do'}
def _fecha_chave(t, p):
    """offset do '}' que fecha o '{' em t[p], ignorando comentarios, strings e chars."""
    d = 0; n = len(t); i = p
    while i < n:
        c = t[i]
        if c == '/' and t.startswith('//', i):
            j = t.find('\n', i); i = n if j < 0 else j; continue
        if c == '/' and t.startswith('/*', i):
            j = t.find('*/', i + 2); i = n if j < 0 else j + 2; continue
        if c in '"\'':
            i += 1
            while i < n and t[i] != c:
                i += 2 if t[i] == '\\' else 1
            i += 1; continue
        if c == '{': d += 1
        elif c == '}':
            d -= 1
            if d == 0: return i
        i += 1
    return n - 1
def indice_funcoes(texto):
    """[(nome, ini, fim)] 1-based: definicao que comeca na coluna 0 e termina no '}' que
    fecha o '{' (profundidade de chaves, nao '}' na coluna 0: funcao de uma linha nao engole as vizinhas)."""
    L = texto.split('\n'); n = len(L); out = []
    off = [0]
    for l in L: off.append(off[-1] + len(l) + 1)
    i = 0
    while i < n:
        l = L[i]
        if l and (l[0].isalpha() or l[0] == '_') and not l.startswith(('typedef', 'extern')):
            sig = ''; j = i; achou = False
            while j < n and j < i + 10:
                sig += L[j] + ' '
                k = min([p for p in (L[j].find('{'), L[j].find(';')) if p >= 0] or [-1])
                if k >= 0: break
                j += 1
            c = sig.find('{'); p = sig.find(';')
            if c >= 0 and (p < 0 or c < p):
                m = re.match(r'^[^(=]*?(\w+)\s*\(', sig[:c])
                if m and m.group(1) not in NAO_FUNC:
                    # offset do '{' = inicio da linha i + posicao em sig (linhas unidas por ' ' == '\n' em tamanho)
                    fim = _fecha_chave(texto, off[i] + c)
                    e = max(j, next((q for q in range(i, n) if off[q + 1] > fim), n - 1))
                    out.append((m.group(1), i + 1, e + 1)); i = e + 1; continue
        i += 1
    return out

_cache = {}
def funcoes_em(ref, caminho):
    k = (ref, caminho)
    if k not in _cache:
        t = git('show', '%s:%s' % (ref, caminho), check=False)
        _cache[k] = indice_funcoes(t) if t else []
    return _cache[k]

# ---- testes -----------------------------------------------------------------
INC = re.compile(r'^\s*#\s*include\s+"([^"]+)"', re.M)
def le(p):
    try: return open(os.path.join(RAIZ, p), errors='replace').read()
    except OSError: return ''
def resolve_inc(de, nome):
    for base in (os.path.dirname(de), 'src', 'tests', ''):
        c = os.path.normpath(os.path.join(base, nome))
        if os.path.exists(os.path.join(RAIZ, c)): return c
    return None
def fecho_inc(arqs):
    """arquivos + tudo que eles #include (src/tests), transitivo."""
    vis = set(); pend = list(arqs)
    while pend:
        a = pend.pop()
        if a in vis: continue
        vis.add(a)
        if a.endswith(('.c', '.h')):
            for m in INC.findall(le(a)):
                r = resolve_inc(a, m)
                if r: pend.append(r)
    return vis

TOK = re.compile(r'(?:src|tests|deploy|tools)/[\w./+-]+\.(?:c|h|py|cjs|mjs|js|sh)\b')
def deps_teste(sh):
    """arquivos que o teste usa: tokens do .sh + do script auxiliar + #includes."""
    base = set(); texto = le(sh)
    for t in TOK.findall(texto):
        if os.path.exists(os.path.join(RAIZ, t)): base.add(t)
    for t in list(base):  # script auxiliar (py/cjs/mjs/js em tests/) olha src/ por dentro
        if t.startswith('tests/') and t.endswith(('.py', '.cjs', '.mjs', '.js')):
            for u in TOK.findall(le(t)):
                if os.path.exists(os.path.join(RAIZ, u)): base.add(u)
    return base

def entrada_hash():
    """Hash do que o grafo le: HEAD, diff/untracked de src+tests+tools+deploy e os campos do mapa.json
    que alimentam o grafo (sem teste/sem_teste, que sao SAIDA do backfill)."""
    import hashlib
    h = hashlib.sha1()
    h.update(git('rev-parse', 'HEAD').encode())
    h.update(git('diff', 'HEAD', '--', 'src', 'tests', 'tools', 'deploy').encode())
    h.update(git('ls-files', '-o', '--exclude-standard', 'src', 'tests').encode())
    for f in git('ls-files', '-o', '--exclude-standard', 'src', 'tests').split('\n'):
        if f: h.update(le(f).encode(errors='replace'))
    m = json.load(open(os.path.join(RAIZ, 'docs/issues/mapa.json')))['itens']
    h.update(json.dumps([[r['numero'], r['status'], r['estado_github'], r['conserto'], r.get('alvo'), r['titulo']] for r in m], sort_keys=True).encode())
    return h.hexdigest()

def construir():
    mapa = json.load(open(os.path.join(RAIZ, 'docs/issues/mapa.json')))['itens']
    head = git('rev-parse', 'HEAD').strip()
    nos = {}; ar = set()
    def no(i, **kw):
        nos.setdefault(i, {'id': i, **kw})
    # indice de funcoes de HEAD
    srcs = sorted(f for f in git('ls-files', 'src').split('\n') if f.endswith('.c'))
    fun_head = {f: indice_funcoes(le(f)) for f in srcs}
    nome2arq = collections.defaultdict(set)
    for f, fs in fun_head.items():
        for (n, _, _) in fs: nome2arq[n].add(f)
    # inclusoes src
    for f in sorted(git('ls-files', 'src').split('\n')):
        if f.endswith(('.c', '.h')):
            no(f, tipo='file')
            for m in INC.findall(le(f)):
                r = resolve_inc(f, m)
                if r: no(r, tipo='file'); ar.add((f, r, 'inclui'))
    # testes
    testes = sorted(t for t in git('ls-files', 'tests').split('\n') if t.endswith('.sh') and t.count('/') == 1)
    for t in testes:
        no(t, tipo='test')
        usados = deps_teste(t)
        fecho = fecho_inc(usados)
        for u in sorted(usados):
            if u.startswith('src/'):
                no(u, tipo='file'); ar.add((t, u, 'compila'))
        compilados = {u for u in fecho if u.startswith('src/') and u.endswith('.c')}
        for u in sorted(fecho):  # headers/.c puxados por include (inclusive de tests/*.c)
            if u.startswith('src/') and (t, u, 'compila') not in ar: no(u, tipo='file'); ar.add((t, u, 'inclui-indireto'))
        # chamadas: identificadores nos .c do teste que sao funcoes de src compilados
        nomes = set()
        for u in fecho:
            if u.startswith('tests/') and u.endswith('.c'):
                nomes |= set(re.findall(r'\b(\w+)\s*\(', le(u)))
        for nm in sorted(nomes):
            for f in sorted(nome2arq.get(nm, ())):
                if f in compilados or f in usados:
                    fid = f + '::' + nm; no(fid, tipo='function', arquivo=f, nome=nm); ar.add((t, fid, 'chama'))
    # issues / commits
    base_issue = {}
    por_assunto = collections.defaultdict(set)  # issue -> commits "teste (...#N...)"
    for linha in git('log', '--format=%H%x09%s', 'HEAD').split('\n'):
        if '\t' not in linha: continue
        h, s = linha.split('\t', 1)
        if re.match(r'^teste\b', s, re.I):
            for n in re.findall(r'#(\d+)', s):
                if n not in ('203', '204'): por_assunto[int(n)].add(h)
    resolvidos = {}
    def resolve(h):
        if h not in resolvidos:
            r = subprocess.run(['git', '-C', RAIZ, 'rev-parse', '--verify', '-q', h + '^{commit}'], capture_output=True, text=True)
            resolvidos[h] = r.stdout.strip() if r.returncode == 0 else None
        return resolvidos[h]
    commits_issue = {}
    for r in mapa:
        n = r['numero']; no('#%d' % n, tipo='issue', numero=n, titulo=r['titulo'], status=r['status'], estado=r['estado_github'], alvo=r.get('alvo'))
        hs = set()
        for c in r['conserto']:
            m = re.match(r'^([0-9a-f]{7,40})\b', c)
            if m:
                full = resolve(m.group(1))
                if full: hs.add(full)
        tc = set(por_assunto.get(n, ()))
        commits_issue[n] = (hs, tc)
        for t in r.get('teste', []) or []:
            if os.path.exists(os.path.join(RAIZ, t)):
                no(t, tipo='test'); ar.add(('#%d' % n, t, 'declara'))
    # commits
    info = {}
    todos = sorted({h for hs, tc in commits_issue.values() for h in hs | tc})
    for h in todos:
        pais = git('rev-list', '--parents', '-n1', h).split()[1:]
        assunto = git('log', '-1', '--format=%s', h).strip()
        no('c:' + h[:8], tipo='commit', hash=h[:8], assunto=assunto, merge=len(pais) > 1)
        if len(pais) != 1: info[h] = None; continue  # merge/raiz: sem diff (o conserto real e o commit filho)
        st = {}
        for l in git('diff', '--name-status', '--no-renames', pais[0], h, '--', 'src', 'tests', 'deploy', 'tools').split('\n'):
            if '\t' in l: s, f = l.split('\t', 1); st[f] = s
        hunks = collections.defaultdict(list)
        atual = None
        for l in git('diff', '--unified=0', '--no-renames', pais[0], h, '--', 'src').split('\n'):
            if l.startswith('+++ b/'): atual = l[6:]
            elif l.startswith('@@') and atual:
                m = re.match(r'@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@', l)
                a = int(m.group(1)); q = int(m.group(2)) if m.group(2) is not None else 1
                hunks[atual].append((a, a + max(q, 1) - 1))
        info[h] = st
        for f in sorted(st):
            no(f, tipo='test' if f.startswith('tests/') and f.endswith('.sh') else 'file')
            ar.add(('c:' + h[:8], f, 'toca'))
        for f in sorted(hunks):
            if not f.endswith('.c') and not f.endswith('.h'): continue
            idx = funcoes_em(h, f) if f.endswith('.c') else []
            for (a, b) in hunks[f]:
                for (nm, i0, i1) in idx:
                    if a <= i1 and b >= i0:
                        fid = f + '::' + nm; no(fid, tipo='function', arquivo=f, nome=nm); ar.add(('c:' + h[:8], fid, 'toca'))
    # fecha testes tocados por .c/.py/.cjs -> .sh que os usa
    usa = collections.defaultdict(set)
    for (a, b, k) in ar:
        if k == 'compila' or k == 'inclui-indireto': pass
    aux2sh = collections.defaultdict(set)
    for t in testes:
        for u in deps_teste(t):
            if u.startswith('tests/'): aux2sh[u].add(t)
    # issue -> commits, issue -> prova. Um teste so PROVA a issue se (a) foi adicionado/alterado num commit
    # do conserto (ou "teste (#N)") E (b) o .sh compila/usa pelo menos um arquivo que o conserto tocou.
    cobre_cache = {}
    def cobre(t):
        # o que o teste usa DE FATO: tokens do .sh/aux + o que os tests/*.c dele incluem (um nivel,
        # sem o fecho transitivo: senao todo teste que inclui js.h "cobre" 40 consertos)
        if t not in cobre_cache:
            base = deps_teste(t); c = set(base)
            for u in base:
                if u.startswith('tests/') and u.endswith(('.c', '.h')):
                    for m in INC.findall(le(u)):
                        r = resolve_inc(u, m)
                        if r: c.add(r)
            for u in list(c):  # .h <-> .c par
                if u.endswith('.h'): c.add(u[:-2] + '.c')
                if u.endswith('.c'): c.add(u[:-2] + '.h')
            cobre_cache[t] = c
        return cobre_cache[t]
    for r in mapa:
        n = r['numero']; hs, tc = commits_issue[n]
        for h in sorted(hs | tc):
            if h in hs: ar.add(('#%d' % n, 'c:' + h[:8], 'conserta'))
            st = info.get(h)
            if not st: continue
            tf = {f: s for f, s in st.items() if f.startswith('tests/')}
            tocados = {f for f in st if not f.startswith('tests/')}  # do MESMO commit
            if len(tocados) > 40: continue  # commit em massa (i18n, icones) nao prova issue
            if len(tf) > 12: continue  # varredura em massa nao prova issue nenhuma
            for f, s in tf.items():
                alvo = [f] if f.endswith('.sh') else sorted(aux2sh.get(f, ()))
                if h in tc and not alvo: continue
                for t in alvo:
                    if os.path.exists(os.path.join(RAIZ, t)) and (cobre(t) & tocados):  # teste apagado/renomeado depois nao prova nada
                        forca = 'adicionado' if s == 'A' else 'alterado'
                        if h in tc: forca = 'commit-teste'
                        no(t, tipo='test'); ar.add(('#%d' % n, t, 'prova:' + forca))
    os.makedirs(SAIDA, exist_ok=True)
    out = {'head': head, 'entrada': entrada_hash(), 'nos': [nos[k] for k in sorted(nos)], 'arestas': [{'de': a, 'para': b, 'tipo': k} for (a, b, k) in sorted(ar)]}
    json.dump(out, open(GRAFO, 'w'), ensure_ascii=False, indent=1, sort_keys=True); open(GRAFO, 'a').write('\n')
    relatorio(out)
    return out

# ---- consultas --------------------------------------------------------------
class G:
    def __init__(self, d):
        self.d = d; self.no = {n['id']: n for n in d['nos']}
        self.sai = collections.defaultdict(list); self.ent = collections.defaultdict(list)
        for e in d['arestas']:
            self.sai[e['de']].append((e['tipo'], e['para'])); self.ent[e['para']].append((e['tipo'], e['de']))
    def tipo(self, i): return self.no.get(i, {}).get('tipo')
    def issues(self): return sorted((n for n in self.d['nos'] if n['tipo'] == 'issue'), key=lambda n: n['numero'])
    def commits_de(self, iss): return [p for (k, p) in self.sai[iss] if k == 'conserta']
    def provas(self, iss, declaradas=True):
        return sorted({p for (k, p) in self.sai[iss] if k.startswith('prova:') or (declaradas and k == 'declara')})
    def provas_graf(self, iss):
        return sorted({p for (k, p) in self.sai[iss] if k.startswith('prova:')})
    def issues_de_commits(self, cs):
        return {i for c in cs for (k, i) in self.ent[c] if k == 'conserta'}
    def tocam(self, alvo):
        return [c for (k, c) in self.ent[alvo] if k == 'toca']
    def headers_dependentes(self, f):
        """f + quem o inclui (transitivo, so src)."""
        vis = {f}; pend = [f]
        while pend:
            a = pend.pop()
            for (k, d) in self.ent[a]:
                if k == 'inclui' and d not in vis: vis.add(d); pend.append(d)
        return vis

def _issues_do_arquivo(g, f):
    cs = set(g.tocam(f))
    for nid, n in g.no.items():
        if n['tipo'] == 'function' and n.get('arquivo') == f: cs |= set(g.tocam(nid))
    return g.issues_de_commits(cs)
def depende_arquivo(g, f, com_includers=True):
    """(issues, testes). Issues: as que tocaram f e, se f e header, as que tocaram os arquivos que o
    incluem (transitivo). Testes: os que compilam f ou quem inclui f. Retorna tambem a origem em g.ultima."""
    fs = g.headers_dependentes(f) if com_includers else {f}
    testes = sorted({t for x in fs for (k, t) in g.ent[x] if k in ('compila', 'inclui-indireto') and g.tipo(t) == 'test'})
    diretas = _issues_do_arquivo(g, f)
    iss = set(diretas)
    for x in fs - {f}: iss |= _issues_do_arquivo(g, x)
    g.ultima = {'diretas': len(diretas), 'via_include': len(iss - diretas), 'includers': len(fs) - 1}
    return iss, testes
def depende_funcao(g, nome):
    ids = [i for i, n in g.no.items() if n['tipo'] == 'function' and n['nome'] == nome]
    cs = {c for i in ids for c in g.tocam(i)}
    iss = g.issues_de_commits(cs)
    testes = sorted({t for i in ids for (k, t) in g.ent[i] if k == 'chama'})
    return iss, testes, sorted(ids)
def mostra(g, iss, testes, extra=''):
    print('issues (%d):' % len(iss))
    for i in sorted(iss, key=lambda x: g.no[x]['numero']):
        n = g.no[i]; print('  %s [%s] %s -> %s' % (i, n['status'], n['titulo'][:60], ', '.join(g.provas(i)) or '(sem teste)'))
    print('testes (%d):' % len(testes))
    for t in testes: print('  ' + t)

def sem_codigo(g):
    out = []
    for n in g.issues():
        if n['estado'] != 'aberta': continue
        cs = g.commits_de(n['id'])
        tocou = any(k == 'toca' and g.tipo(p) in ('file', 'function') and p.startswith('src/') for c in cs for (k, p) in g.sai[c])
        if not tocou: out.append(n)
    return out
def sem_teste(g):
    return [n for n in g.issues() if n['status'] in CONSERTADAS and not g.provas(n['id'])]

def relatorio(d):
    g = G(d); c = collections.Counter(n['tipo'] for n in d['nos']); e = collections.Counter(x['tipo'].split(':')[0] for x in d['arestas'])
    ic = [n for n in g.issues() if n['status'] in CONSERTADAS]
    com = [n for n in ic if g.provas(n['id'])]
    L = ['# Grafo issue<->codigo', '', 'HEAD `%s`. Gerado por `tools/rastreio/grafo.py` (stdlib; nao e o graphify).' % d['head'][:8], '',
         'Nos: ' + ', '.join('%s %d' % kv for kv in sorted(c.items())) + '.', 'Arestas: ' + ', '.join('%s %d' % kv for kv in sorted(e.items())) + '.', '',
         'Issues consertada-nao-lancada/lancada: %d; com teste (grafo ou declarado): %d; sem: %d.' % (len(ic), len(com), len(ic) - len(com)), '',
         'Issues abertas sem codigo tocado em src/: %d.' % len(sem_codigo(g)), '',
         'Limites: merges nao entram no diff (o conserto e o commit filho); funcao = definicao na coluna 0 terminada por "{"; teste -> funcao so por chamada direta em tests/*.c.', '']
    open(os.path.join(SAIDA, 'RELATORIO.md'), 'w').write('\n'.join(L))

def carrega():
    """Grafo em cache SO se a entrada (HEAD, arvore, mapa) nao mudou; senao reconstroi."""
    if os.path.exists(GRAFO):
        d = json.load(open(GRAFO))
        if d.get('entrada') == entrada_hash(): return d
        print('grafo: cache velho (head %s), reconstruindo...' % d.get('head', '?')[:8], file=sys.stderr, flush=True)
    return construir()

def main():
    a = sys.argv[1:]; cmd = a[0] if a else 'construir'
    if cmd == 'construir':
        d = construir(); print('ok: %d nos, %d arestas -> %s' % (len(d['nos']), len(d['arestas']), os.path.relpath(GRAFO, RAIZ))); return
    g = G(carrega())
    if cmd == 'depende':
        if '--arquivo' in a:
            f = a[a.index('--arquivo') + 1]; iss, t = depende_arquivo(g, f)
            # provas das issues tambem contam como testes que dependem
            mostra(g, iss, sorted(set(t)))
        elif '--funcao' in a:
            nm = a[a.index('--funcao') + 1]; iss, t, ids = depende_funcao(g, nm)
            print('definicoes: ' + (', '.join(ids) or '(nenhuma)')); mostra(g, iss, t)
        else: sys.exit('uso: depende --arquivo F | --funcao N')
    elif cmd == 'sem-codigo':
        for n in sem_codigo(g): print('#%d [%s/%s] %s' % (n['numero'], n['status'], n['alvo'], n['titulo'][:70]))
    elif cmd == 'sem-teste':
        for n in sem_teste(g): print('#%d [%s] %s' % (n['numero'], n['status'], n['titulo'][:70]))
    else: sys.exit('comando desconhecido: ' + cmd)
if __name__ == '__main__': main()
