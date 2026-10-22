"""Execute production routing prefixes with playback/UI boundaries stubbed."""
from pathlib import Path
import re
import subprocess
import tempfile

source = Path('src/app.c').read_text()
manual = source.split('static void tocarFonteEscolhida(int fonte, int manual) {', 1)[1].split('\n  {', 1)[0]
picker = re.split(r'episodios_escolheu\(&t,&e\)\)? && player_aberto\(\)\) \{', source, maxsplit=1)[1]
picker = picker.split('diagnostico_cancelar_vazao();', 1)[0]
next_ep = source.split('player_pediu_proximo(&t,&e) && player_aberto()) {', 1)[1].split('diagnostico_cancelar_vazao();', 1)[0]
program = r'''
#include <assert.h>
static int opened, blocked = 1, playing, aguardandoFonte, folhaParaTocar;
static int player_aberto(void) { return playing; }
static int player_indice(void) { return 7; }
static int detail_indice(void) { return 7; }
static int player_episodio_atual(int *t, int *e) { *t=2; *e=5; return 1; }
static int detail_ep_foco(int *t, int *e) { *t=2; *e=5; return 1; }
static int travaui_abrir_espera(int idx, int t, int e) {
  assert(idx == 7 && t == 2 && e == 5); return blocked;
}
static int episodioNaSala(int idx, int t, int e) { return 0; }
static void manual_source(void) {
''' + manual + r'''
  opened++;
}
static void picker(void) { int t=2,e=5, retomar=0, autorizado=-1;
''' + picker + r'''
  opened++;
  semEscolhaEpisodio:;
}
static void next_episode(void) { int t=2,e=5;
''' + next_ep + r'''
  opened++;
  semProximo:;
}
int main(void) {
  manual_source(); assert(opened == 0);
  playing=1; manual_source(); picker(); next_episode(); assert(opened == 0);
  blocked=0; manual_source(); picker(); next_episode(); assert(opened == 3);
}
'''
with tempfile.TemporaryDirectory() as directory:
    c = Path(directory) / 'routing.c'
    exe = Path(directory) / 'routing'
    c.write_text(program)
    subprocess.run(['cc', str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('playback gate routing: ok')
