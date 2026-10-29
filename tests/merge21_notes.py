#!/usr/bin/env python3
"""Focused regressions for the 2.1 merge notes; no network or deployment."""
import pathlib
import re
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class MergeNotes(unittest.TestCase):
    def test_translations(self):
        for path in sorted((ROOT / 'src').glob('idioma_*.h')):
            source = path.read_text()
            self.assertIn('"Trilha de filmes"', source, path.name)
            self.assertIn('"Trilha sonora"', source, path.name)
            self.assertNotRegex(source, r'"Trilha",')
        table = (ROOT / 'src/idioma_tab.h').read_text()
        for key, value in [('Completo', 'Full'), ('Dinâmica', 'Dynamic'),
                           ('Trilha de filmes', 'Trail'), ('Trilha sonora', 'Soundtrack')]:
            self.assertIn(f'{{ "{key}", "{value}" }}', table)
        self.assertIn('"Visual", "Trilha sonora",', (ROOT / 'src/reacao.c').read_text())
        self.assertIn('"Trilha de filmes"', (ROOT / 'src/biblioteca.c').read_text())

    def test_server_tabs(self):
        source = (ROOT / 'src/streams.c').read_text()
        function = source.split('static void atualizarProvedores(void) {', 1)[1].split('\nstatic int nFiltrados', 1)[0]
        program = '''#include <stdio.h>
#include <string.h>
#include <assert.h>
#define FONTEFALHA_CHAVE 96
#define ADDONS_MAX 12
#define ADDON_ABA_PRONTO 1
#define ORD(k) (k)
typedef struct { char nome[96], chave[96]; int ordem, estado; } AddonAba;
typedef AddonAba ProvAba;
static struct { char provedor[96], addonChave[96]; } lista[13];
static int n=13, filtro, nProvedores;
static char provedores[13][96], provChaves[13][96];
static unsigned char provEstado[13];
static int addons_parcial_abas(AddonAba *a, int n) { (void)a; (void)n; return 0; }
static int stream_ordem_addon(int i) { return i-1; }
static void atualizarProvedores(void) {''' + function + '''
int main(void) {
  for (int i=0; i<13; i++) {
    snprintf(lista[i].provedor, 96, "provider%d", i);
    snprintf(lista[i].addonChave, 96, "key%d", i);
  }
  atualizarProvedores();
  assert(nProvedores == 13);
  assert(!strcmp(provChaves[1], "key1"));
  assert(!strcmp(provChaves[12], "key12"));
}
'''
        with tempfile.TemporaryDirectory() as tmp:
            c = pathlib.Path(tmp) / 'tabs.c'
            c.write_text(program)
            subprocess.run(['cc', str(c), '-o', tmp + '/tabs'], check=True)
            subprocess.run([tmp + '/tabs'], check=True)
        addons = (ROOT / 'src/addons.c').read_text().split('static void progMarcarSrv(', 1)[1].split('\nstatic void ', 1)[0]
        self.assertIn('atomic_fetch_add(&progRev, 1)', addons)

    def test_worker_guard(self):
        config = (ROOT / 'servidor/recomendacoes/wrangler.toml').read_text()
        self.assertIn('command = "bash ../../tools/recomendacoes-guard.sh"', config)
        guard = ROOT / 'tools/recomendacoes-guard.sh'
        with tempfile.TemporaryDirectory() as tmp:
            upstream = pathlib.Path(tmp) / 'upstream'
            checkout = pathlib.Path(tmp) / 'checkout'
            def git(where, *args):
                return subprocess.run(['git', '-C', str(where), *args], check=True, capture_output=True)
            upstream.mkdir()
            git(upstream, 'init', '-b', 'master')
            git(upstream, 'config', 'user.email', 'test@example.org')
            git(upstream, 'config', 'user.name', 'Test')
            music = upstream / 'servidor/recomendacoes/src/musica.js'
            music.parent.mkdir(parents=True)
            music.write_text((ROOT / 'servidor/recomendacoes/src/musica.js').read_text())
            git(upstream, 'add', '.')
            git(upstream, 'commit', '-m', 'Initial worker')
            subprocess.run(['git', 'clone', '-q', str(upstream), str(checkout)], check=True)
            def run():
                return subprocess.run(['bash', str(guard)], cwd=checkout, capture_output=True).returncode
            self.assertEqual(run(), 0, 'current worker allowed')
            local_music = checkout / music.relative_to(upstream)
            original = local_music.read_text()
            local_music.write_text(original.replace('await deezer(', 'await removed('))
            self.assertNotEqual(run(), 0, 'missing Deezer fallback refused')
            local_music.write_text(original)
            (upstream / 'new.txt').write_text('new master commit')
            git(upstream, 'add', '.')
            git(upstream, 'commit', '-m', 'Advance master')
            self.assertNotEqual(run(), 0, 'lagging branch refused after refresh')
            git(checkout, 'merge', '--ff-only', 'origin/master')
            self.assertEqual(run(), 0, 'updated worker allowed')


if __name__ == '__main__':
    unittest.main()
