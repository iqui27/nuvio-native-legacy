"""Review regressions at the application/UI boundaries (also runnable on HEAD)."""
import subprocess
import sys
from pathlib import Path


def source(path):
    if '--head' in sys.argv:
        return subprocess.check_output(['git', 'show', f'HEAD:{path}'], text=True)
    return Path(path).read_text()


ui = source('src/juntosui.c')
app = source('src/app.c')
player = source('src/player.c')
checks = {
    'P1: lobby exposes Join with code and opens the code editor':
        'b[n++] = BT_CODIGO;' in ui and
        'case BT_CODIGO: focoSala = foco;' in ui and
        'rotuloBotao(BT_CODIGO), iconeBotao(BT_CODIGO), focoDe(n)' in ui,
    'P1: browsing releases player input and presentation, Back restores lobby':
        'return aberto && !juntosui_escolhendo_filme();' in player and
        'if (!player_visivel()) return;\n  const CatItem *c = item();' in player and
        'if (player_visivel()) { player_evento(e); return; }' in app and
        'if (!player_visivel() || player_janela_animando' in app and
        'juntosui_voltar_filme()' in app and
        'detail_fechar_seco();\n    trocarTela(TELA_BUSCA);' in app,
    'P2: QR Back closes only QR; Sources and exit restore invoking focus':
        'if (ehVoltar(k)) { mostrarQr = qrPendente = 0;' in ui and
        'case BT_FONTES: focoSala = foco; mostrarQr = 0; pediuFontes = 1;' in ui and
        'camada = CAMADA_SALA; foco = focoSala;' in ui and
        '} else foco = focoSala;' in ui,
    'P2: Play compares full room movie/episode identity':
        'strcmp(salaJ->sala.ep, alvoJ)' in app and
        'idDoAlvo(tituloJ, alvoJ, sizeof alvoJ);' in app,
}
for name, passed in checks.items():
    print(('PASS: ' if passed else 'FAIL: ') + name)
assert all(checks.values()), 'Watch Together review regression'
