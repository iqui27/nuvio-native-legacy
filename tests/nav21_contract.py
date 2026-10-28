"""Review regressions that runtime navigation alone cannot detect."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
baseline = '--baseline' in sys.argv

def source(path):
    if baseline:
        result = subprocess.run(['git', 'show', f'4e1e16d47:{path}'], cwd=root,
                                text=True, capture_output=True)
        return result.stdout if result.returncode == 0 else ''
    return (root / path).read_text()

panel = source('src/salvospainel.c')
social = source('src/social.c')
app = source('src/app.c')
checks = {
    'P1 composition: top tabs and localized selected destination title':
        'i18n(rotuloAba(aba))' in panel and 'i18n(rotuloAba(i))' in panel
        and '#define SP_ABAS_Y      (SP_Y + 40.0f)' in panel
        and '#define SP_TIT_Y       (SP_Y + 138.0f)' in panel,
    'P1 material: friend activity reuses shared sheet glass':
        'gfx_vidro_folha(il,SO_RAIO/il.h,1)' in social,
    'P1 production: blue panel and friend profiles retain their live owners':
        'spainel_evento(e)' in app and 'spainel_desenhar(agora)' in app
        and 'amigoperfil_abrir(pid)' in app and 'spainel_voltar_titulo()' in app,
    'P1 navigation: selecting the current sidebar destination clears panel ownership':
        'tituloDoPainel = 0;' in app.split('if (menu_escolheu()) {', 1)[1].split('\n  }', 1)[0],
    'P1 input: capture transitions use SDL events':
        bool(source('tests/nav21.sh'))
        and 'app_evento(&e)' in source('tests/nav21.c')
        and 'static void proof_focus(void) { selecionado=1; }' not in source('tests/social_tela_shot.c'),
}
for label, ok in checks.items():
    print(f'{"PASS" if ok else "FAIL"}: {label}')
raise SystemExit(0 if all(checks.values()) else 1)
