import copy
import json
from pathlib import Path
import unittest

from atualizar import apply_reports, validate


class ReportsTest(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(Path(__file__).with_name('release.json').read_text())

    def test_real_inventory_has_unique_items_and_no_dependency_cycles(self):
        validate(self.data)
        farol = [i for i in self.data['items'] if i['group'] in ('diagnostico', 'gosto')]
        self.assertEqual(56, len(farol))
        self.assertNotIn('farol-42', [i['id'] for i in farol])
        self.assertEqual(1, len([i for i in self.data['items'] if i['id'] == 'issue-731']))

    def test_report_updates_reference_and_preserves_unverified_text(self):
        result = apply_reports(self.data, ['[REVISAO] Farol 6 · agente/f6@abcdef123 · não verificado · aguardando TV'])
        i = next(i for i in result['items'] if i['id'] == 'farol-6')
        self.assertEqual('REVISAO', i['status'])
        self.assertEqual('agente/f6', i['branch'])
        self.assertEqual('abcdef123', i['sha'])
        self.assertEqual('não verificado · aguardando TV', i['note'])
        self.assertFalse(i['verified'])
        self.assertEqual('REVISAO', result['events'][0]['to'])
        self.assertEqual(self.data['gates'], result['gates'])

    def test_invalid_second_report_does_not_mutate_input(self):
        before = copy.deepcopy(self.data)
        with self.assertRaises(ValueError):
            apply_reports(self.data, ['[FEITO] Farol 6 · — · entregue', '[FEITO] desconhecido · — · erro'])
        self.assertEqual(before, self.data)

    def test_interrupted_attempt_preserves_the_delivered_commit_role(self):
        base = apply_reports(self.data, ['[IMPLEMENTANDO] farol-6 · agente/f6@aaaaaaa · base de trabalho; sem commit ainda'])
        self.assertEqual('base', next(i for i in base['items'] if i['id'] == 'farol-6')['commitRole'])
        base = apply_reports(base, ['[IMPLEMENTANDO] farol-6 · agente/f6@aaaaaaa · tentativa interrompida sem commit'])
        self.assertEqual('base', next(i for i in base['items'] if i['id'] == 'farol-6')['commitRole'])
        delivered = apply_reports(base, ['[REVISAO] farol-6 · agente/f6@bbbbbbb · implementação entregue'])
        result = apply_reports(delivered, ['[CORRECAO] farol-6 · agente/f6@bbbbbbb · nova tentativa interrompida sem commit; relançando'])
        item = next(i for i in result['items'] if i['id'] == 'farol-6')
        self.assertEqual('bbbbbbb', item['sha'])
        self.assertNotEqual('base', item.get('commitRole'))
        self.assertFalse(item['verified'])

    def test_ambiguous_number_requires_explicit_identity(self):
        with self.assertRaises(ValueError):
            apply_reports(self.data, ['[FEITO] 763 · — · não adivinhar issue ou porte'])
        result = apply_reports(self.data, ['[REVISAO] pr763 · agente/pr763@abcdef123 · nova revisão'])
        self.assertEqual('REVISAO', next(i['status'] for i in result['items'] if i['id'] == 'pr-763'))

    def test_shared_review_unit_updates_all_linked_issues(self):
        result = apply_reports(self.data, ['[REVISAO] D1 · — · revisar como unidade'])
        items = [i for i in result['items'] if i.get('reviewUnit') == 'D1']
        self.assertEqual(8, len(items))
        self.assertTrue(all(i['status'] == 'REVISAO' for i in items))

    def test_batch_is_not_a_merge_and_does_not_close_tv_gates(self):
        result = apply_reports(self.data, ['[PR_ABERTA] lote-21d · lote/21d@bbf45a751 · PR aberta'])
        self.assertEqual('PR_ABERTA', next(b['status'] for b in result['batches'] if b['id'] == '21d'))
        self.assertEqual(self.data['items'], result['items'])
        self.assertEqual(self.data['gates'], result['gates'])
        self.assertEqual(self.data['meta']['integration'], result['meta']['integration'])

    def test_bad_references_and_statuses_fail(self):
        for line in ['[FEITO] Farol 6 · sha-solto · inválido', '[FEITO] Farol 6 · branch@ZZZZZZZ · inválido', '[OK] Farol 6 · — · inválido']:
            with self.subTest(line=line), self.assertRaises(ValueError):
                apply_reports(self.data, [line])

    def test_timestamp_alias_branch_only_and_ordering(self):
        # Keep the ordering fixture independent of later production reports.
        for item in self.data['items']:
            if item['id'] in ('perfil-sem-login', 'issue-778'):
                item['reportedAt'] = '2026-11-23T03:44:00+00:00'
        result = apply_reports(self.data, ['2026-11-23 03:45 [LOTE] perfil-semlogin · agente/perfil@abcdef123 · em lote',
                                           '2026-11-23 03:45 [BLOQUEADA] 778 · agente/21z-778 · log pendente'])
        i = next(i for i in result['items'] if i['id'] == 'issue-778')
        self.assertEqual('agente/21z-778', i['branch'])
        self.assertIsNone(i['sha'])
        self.assertEqual('2026-11-23T03:45:00+00:00', i['reportedAt'])
        with self.assertRaises(ValueError):
            apply_reports(result, ['2026-11-23 03:44 [FEITO] 778 · — · antigo'])

    def test_branch_disambiguates_port_and_github_state_survives_priority(self):
        result = apply_reports(self.data, ['[APROVADA] 749 · agente/21z-pr749@abcdef123 · porte aprovado',
                                          '[POR_ULTIMO] 830 · — · após os lotes VPS'])
        self.assertEqual('APROVADA', next(i['status'] for i in result['items'] if i['id'] == 'pr-749'))
        pr = next(i for i in result['items'] if i['id'] == 'pr-830')
        self.assertEqual('POR_ULTIMO', pr['status'])
        self.assertEqual('open', pr['githubState'])
        self.assertTrue(pr['previousNotes'])

    def test_cycles_and_missing_dependencies_fail_validation(self):
        for dep in ['farol-1', 'farol-999']:
            broken = copy.deepcopy(self.data)
            broken['items'][0]['dependencies'] = [dep]
            with self.assertRaises(ValueError):
                validate(broken)


if __name__ == '__main__':
    unittest.main()
