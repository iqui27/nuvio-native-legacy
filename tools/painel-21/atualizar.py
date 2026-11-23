#!/usr/bin/env python3
"""Validate or atomically apply VPS reports to the release's single JSON source."""
import argparse
import copy
import datetime as dt
import json
import os
from pathlib import Path
import re
import tempfile

STATES = {'A_FAZER', 'IMPLEMENTANDO', 'REVISAO', 'CORRECAO', 'APROVADA',
          'LOTE', 'PR_ABERTA', 'FEITO', 'BLOQUEADA', 'POR_ULTIMO'}
DEFAULT = Path(__file__).with_name('release.json')


def validate(data):
    ids = [item['id'] for item in data['items']]
    if len(ids) != len(set(ids)):
        raise ValueError('IDs duplicados')
    for item in data['items']:
        if item['status'] not in STATES:
            raise ValueError(f"Estado inválido: {item['id']}")
        for dep in item.get('dependencies', []):
            if dep not in ids or dep == item['id']:
                raise ValueError(f"Dependência inválida: {item['id']} → {dep}")
    # All-dependencies closing tasks are implicit and excluded from DAG edges.
    graph = {i['id']: i.get('dependencies', []) for i in data['items']}
    visiting, visited = set(), set()

    def visit(key):
        if key in visiting:
            raise ValueError('Ciclo de dependências: ' + key)
        if key in visited:
            return
        visiting.add(key)
        for dep in graph[key]:
            visit(dep)
        visiting.remove(key)
        visited.add(key)

    for key in graph:
        visit(key)
    for gate in data.get('gates', []):
        if any(key not in ids for key in gate.get('items', [])):
            raise ValueError('Pendência aponta para item inexistente')
    return data


def item_id(raw):
    raw = raw.strip()
    if re.fullmatch(r'(?i)farol\s*#?\s*\d+', raw):
        return 'farol-' + re.search(r'\d+', raw)[0]
    if re.fullmatch(r'(?i)(?:pr|#)\s*#?\s*\d+', raw):
        return 'pr-' + re.search(r'\d+', raw)[0]
    return raw


def resolve_targets(result, raw, ref=''):
    key = item_id(raw)
    key = {'perfil-semlogin': 'perfil-sem-login', 'nomefonte': 'arquivo-desktop',
           'mac-ajustes': 'mac-desktop'}.get(key, key)
    exact = [i for i in result['items'] if i['id'] == key]
    batch = [b for b in result['batches'] if key == 'lote-' + b['id']]
    if exact or batch:
        return exact or batch
    # A shared review unit (D1, 762+720) intentionally updates its issue cards together.
    unit = re.sub(r'\s*\(.*\)$', '', raw).strip()
    shared = [i for i in result['items'] if i.get('reviewUnit') == unit]
    if shared and not re.fullmatch(r'\d+', unit):
        return shared
    if re.fullmatch(r'\d+(?:\s*\(teste\))?', raw):
        number = int(re.search(r'\d+', raw)[0])
        matches = [i for i in result['items'] if i.get('number') == number and i['group'] not in ('diagnostico', 'gosto')]
        if len(matches) == 1:
            return matches
        branch = ref.rsplit('@', 1)[0]
        exact_branch = [i for i in matches if i.get('branch') == branch]
        if len(exact_branch) == 1:
            return exact_branch
        if re.search(r'(?:^|[-/])pr' + str(number) + r'(?:$|[-/])', branch, re.I):
            port = [i for i in matches if i['id'] == 'pr-' + str(number)]
            if len(port) == 1:
                return port
    raise ValueError('Item desconhecido ou ambíguo (use farol-N, issue-N, pr-N ou lote-21d): ' + raw)


def apply_reports(data, lines):
    result = copy.deepcopy(data)
    now = dt.datetime.now(dt.timezone.utc).isoformat(timespec='seconds')
    for line in lines:
        if not line.strip():
            continue
        match = re.fullmatch(r'(?:(\d{4}-\d{2}-\d{2} \d{2}:\d{2})\s+)?\[([A-Z_]+)\]\s*(.+?)\s*·\s*(.*?)\s*·\s*(.*)', line.strip())
        if not match or match[2] not in STATES:
            raise ValueError('Relato inválido: ' + line)
        timestamp, status, raw, ref, note = match.groups()
        reported_at = dt.datetime.strptime(timestamp, '%Y-%m-%d %H:%M').replace(tzinfo=dt.timezone.utc).isoformat() if timestamp else now
        targets = resolve_targets(result, raw, ref)
        reference = {}
        if ref not in ('', '—', '-', 'não verificado'):
            if '@' not in ref:
                if '/' not in ref or re.search(r'\s', ref):
                    raise ValueError('Referência deve ser branch@sha ou branch sem commit: ' + ref)
                reference = dict(branch=ref, sha=None)
            else:
                branch, sha = ref.rsplit('@', 1)
                if not branch or not re.fullmatch('[0-9a-fA-F]{7,40}', sha):
                    raise ValueError('Referência inválida: ' + ref)
                reference = dict(branch=branch, sha=sha)
        for target in targets:
            previous = target['status']
            if timestamp and target.get('reportedAt') and reported_at < target['reportedAt']:
                raise ValueError('Relato anterior ao último estado: ' + target['id'])
            if target.get('note') and target['note'] != note:
                target.setdefault('previousNotes', []).insert(0, target['note'])
            target.update(reference)
            target.update(status=status, note=note, updatedAt=now, reportedAt=reported_at, source='Relato VPS', verified=False)
            if 'sem commit' in note.lower():
                target['commitRole'] = 'base'
            elif reference:
                target.pop('commitRole', None)
            # A new report/reference is not an ancestry verification of that new commit.
            if reference:
                target['integrationVerified'] = False
            result['events'].insert(0, {'at': reported_at, 'appliedAt': now, 'item': target['id'], 'from': previous, 'to': status, 'report': line})
    result['meta']['updatedAt'] = now
    return validate(result)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('reports', nargs='*', help='Uma linha por relato; aceita vários argumentos')
    parser.add_argument('--file', type=Path, default=DEFAULT)
    parser.add_argument('--stdin', action='store_true')
    parser.add_argument('--check', action='store_true', help='Só validar; não escrever')
    args = parser.parse_args()
    try:
        data = validate(json.loads(args.file.read_text()))
        reports = args.reports
        if args.stdin:
            import sys
            reports += sys.stdin.read().splitlines()
        if reports:
            data = apply_reports(data, reports)
        if reports and not args.check:
            # Same-directory replace: readers see either the old or entire new file.
            name = None
            try:
                with tempfile.NamedTemporaryFile('w', dir=args.file.parent, delete=False, encoding='utf-8') as tmp:
                    name = tmp.name
                    json.dump(data, tmp, ensure_ascii=False, indent=2)
                    tmp.write('\n')
                    tmp.flush()
                    os.fsync(tmp.fileno())
                os.replace(name, args.file)
            finally:
                if name and os.path.exists(name):
                    os.unlink(name)
        print(f"OK · {len(data['items'])} itens · {len(reports)} relatos" + (' · simulação' if args.check else ''))
    except (ValueError, KeyError, OSError) as exc:
        parser.exit(1, f'Erro: {exc}\n')


if __name__ == '__main__':
    main()
