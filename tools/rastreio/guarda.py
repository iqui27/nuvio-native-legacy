#!/usr/bin/env python3
"""Guarda de regressao: testes-trava das issues que dependem do que mudou.
  guarda.sh <base>   arquivos mudados desde <base> (ate a arvore de trabalho) -> issues -> testes
  guarda.sh --tudo   todos os testes-trava (campo "teste" do mapa.json)
Status por teste: ok | pulado (SKIP) | falha-conhecida | QUEBROU | TEMPO (timeout) | AUSENTE.
Falha o guarda (exit 1): QUEBROU, TEMPO, AUSENTE, e falha conhecida com rc/assinatura diferente.
pulado nao falha mas e contado. Issues sem teste aparecem em "sem teste" (nao falham).
Grava graphify-out/issues/guarda-ultimo.json (contrato no README)."""
import json, os, sys, subprocess, signal, time, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import grafo
R = grafo.RAIZ; AQUI = os.path.dirname(os.path.abspath(__file__))
TMO = int(os.environ.get('GUARDA_TIMEOUT', '90'))
MAXT = int(os.environ.get('GUARDA_MAX_TESTES', '0'))  # 0 = sem teto; se > 0 corta e AVISA o que ficou de fora
LOGS = os.path.join(grafo.SAIDA, 'guarda-logs')

def linhas(nome):
    p = os.path.join(AQUI, nome)
    return [l.rstrip('\n') for l in open(p) if l.strip() and not l.lstrip().startswith('#')] if os.path.exists(p) else []
def conhecidas():
    out = {}
    for l in linhas('falhas-conhecidas.txt'):
        p = [x.strip() for x in l.split('|', 3)]
        if len(p) < 3 or not p[1].lstrip('-').isdigit() or not p[2] or '*' in p[0] or '?' in p[0]:
            sys.exit('falhas-conhecidas.txt: linha invalida (nome exato, rc, assinatura): ' + l)
        out[p[0]] = (int(p[1]), p[2])
    return out
def timeouts():
    out = {}
    for l in linhas('tempos.txt'):
        p = [x.strip() for x in l.split('|')]
        out[p[0]] = int(p[1])
    return out

def roda(t, tmo):
    """(rc, saida, segundos). rc 124 = timeout (mata o grupo todo), 127 = teste ausente."""
    if not os.path.isfile(os.path.join(R, t)): return 127, '', 0.0
    ini = time.time()
    pr = subprocess.Popen(['bash', t], cwd=R, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, errors='replace', start_new_session=True)
    try: out, _ = pr.communicate(timeout=tmo); rc = pr.returncode
    except subprocess.TimeoutExpired:
        os.killpg(pr.pid, signal.SIGKILL); out, _ = pr.communicate(); rc = 124
    return rc, out or '', time.time() - ini

def classifica(t, rc, out, conh):
    """-> (status, detalhe)"""
    if rc == 127: return 'AUSENTE', 'teste nao existe'
    if rc == 124: return 'TEMPO', 'estourou o timeout'
    if t in conh:
        rc0, sig = conh[t]
        if rc == 0: return 'corrigido', 'passou: tire de falhas-conhecidas.txt'
        if rc == rc0 and sig in out: return 'falha-conhecida', 'rc=%d' % rc
        return 'QUEBROU', 'falha conhecida mudou (rc=%d, esperado %d%s)' % (rc, rc0, '' if sig in out else ', assinatura ausente')
    if rc != 0: return 'QUEBROU', 'rc=%d' % rc
    if any(l.startswith('SKIP') for l in out.split('\n')): return 'pulado', 'SKIP na saida'
    return 'ok', ''
FALHAM = ('QUEBROU', 'TEMPO', 'AUSENTE')

def selecao(g, trava, base):
    mudados = [f for f in subprocess.check_output(['git', '-C', R, 'diff', '--name-only', base], text=True).split('\n') if f]
    issues = set(); testes_extra = set(); n_dir = n_inc = 0
    for f in mudados:
        if f.startswith('tests/'):
            st = f.rsplit('.', 1)[0] + '.sh'
            issues |= {n for n, ts in trava.items() if st in ts or f in ts}
        elif f in g.no:
            iss, ts = grafo.depende_arquivo(g, f)
            n_dir += g.ultima['diretas']; n_inc += g.ultima['via_include']
            issues |= {g.no[i]['numero'] for i in iss}
            testes_extra |= set(ts)
    return mudados, issues, testes_extra, (n_dir, n_inc)

def main():
    a = sys.argv[1:]
    if not a: sys.exit('uso: guarda.sh <base> | --tudo')
    mapa = json.load(open(os.path.join(R, 'docs/issues/mapa.json')))['itens']
    trava = {r['numero']: r['teste'] for r in mapa if r.get('teste')}
    locks = {t for ts in trava.values() for t in ts}
    g = grafo.G(grafo.carrega())
    sem = []
    if a[0] == '--tudo':
        alvo = dict(trava); mudados = []; extra = set(); cont = (0, 0)
    else:
        mudados, issues, extra, cont = selecao(g, trava, a[0])
        alvo = {n: trava[n] for n in sorted(issues) if n in trava}
        por = {r['numero']: r for r in mapa}
        sem = sorted(n for n in issues if n not in trava and por.get(n, {}).get('status') in grafo.CONSERTADAS)
    # testes: os das issues selecionadas + lock tests que compilam o arquivo mudado (ou quem o inclui)
    ordem = sorted({t for ts in alvo.values() for t in ts} | (extra & locks))
    print('base %s; arquivos mudados %d; issues %d (diretas %d, via include %d, sem teste %d); testes %d (+%d por compilar o arquivo mudado)' % (
        a[0], len(mudados), len(alvo) + len(sem), cont[0], cont[1], len(sem), len(ordem), len(extra & locks - {t for ts in alvo.values() for t in ts})), flush=True)
    if MAXT and len(ordem) > MAXT:
        print('AVISO: %d testes acima do teto GUARDA_MAX_TESTES=%d; NAO rodam: %s' % (len(ordem) - MAXT, MAXT, ' '.join(ordem[MAXT:])), flush=True)
        ordem = ordem[:MAXT]
    conh = conhecidas(); tmo = timeouts(); os.makedirs(LOGS, exist_ok=True)
    res = {}
    for t in ordem:
        rc, out, seg = roda(t, tmo.get(t, TMO))
        st, det = classifica(t, rc, out, conh)
        open(os.path.join(LOGS, os.path.basename(t)[:-3] + '.log'), 'w').write(out)
        res[t] = {'rc': rc, 'status': st, 'detalhe': det, 'segundos': round(seg, 1)}
        print('  %-16s %s (%.0f s) %s' % (st, t, seg, det), flush=True)
    quebrou = 0; out = []
    for n, ts in sorted(alvo.items()):
        ruins = [t for t in ts if t in res and res[t]['status'] in FALHAM]
        novo = [t for t in ts if t not in res]
        if ruins:
            quebrou += 1; print('#%d QUEBROU (%s)' % (n, ', '.join('%s rc=%d %s' % (t, res[t]['rc'], res[t]['status']) for t in ruins)))
        else:
            ex = [t for t in ts if t in res and res[t]['status'] in ('pulado', 'falha-conhecida', 'corrigido')]
            print('#%d ok%s' % (n, (' (' + ', '.join('%s %s' % (t, res[t]['status']) for t in ex) + ')') if ex else '') + (' [nao rodou: %s]' % ' '.join(novo) if novo else ''))
        out.append({'issue': n, 'resultado': 'quebrou' if ruins else 'ok', 'testes': {t: res[t]['status'] for t in ts if t in res}})
    if sem: print('\nSEM TESTE (%d, nao falham o guarda): %s' % (len(sem), ' '.join('#%d' % n for n in sem)))
    cont_st = collections.Counter(v['status'] for v in res.values())
    # testes fora de qualquer issue selecionada (so por compilar o arquivo) tambem falham o guarda
    soltos = [t for t in ordem if res[t]['status'] in FALHAM and not any(t in ts for ts in alvo.values())]
    for t in soltos: quebrou += 1; print('(solto) QUEBROU %s %s' % (t, res[t]['detalhe']))
    print('\nPor status: ' + ', '.join('%s %d' % kv for kv in sorted(cont_st.items())) + '; tempo total %.0f s' % sum(v['segundos'] for v in res.values()))
    json.dump({'head': grafo.git('rev-parse', 'HEAD').strip(), 'grafo_head': g.d['head'], 'grafo_entrada': g.d['entrada'], 'base': a[0],
               'quebrou': quebrou, 'sem_teste': sem, 'contagem': dict(cont_st), 'testes': res, 'issues': out},
              open(os.path.join(grafo.SAIDA, 'guarda-ultimo.json'), 'w'), indent=1, sort_keys=True)
    print('GUARDA: %s (%d quebrada(s))' % ('FAIL' if quebrou else 'PASS', quebrou))
    sys.exit(1 if quebrou else 0)
if __name__ == "__main__": main()
