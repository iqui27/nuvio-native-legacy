# Extract the production "Assistir do comeco" block and its helpers from
# src/detail.c, verbatim, for tests/expandebotao_detalhe.c.
import pathlib, sys

s = pathlib.Path(sys.argv[1]).read_text()
h = pathlib.Path(sys.argv[2]).read_text().splitlines()
out = pathlib.Path(sys.argv[3])


def fn(head):
    i = s.index(head)
    return s[i:s.index('\n}\n', i) + 3]


start = s.index('    if (detalheRetrato() && temInicio()) {')
end = s.index('    if (detalheRetrato() && temLembrar() && !acoesAgrupadas()) {', start)
(out / 'expandebotao_bloco.h').write_text(
    '\n'.join(l for l in h if l.startswith('#define NV_DETW')) + '\n')
(out / 'expandebotao_bloco.c').write_text(
    fn('static int acoesAgrupadas(void) {') + fn('static float larguraSecundario(') +
    fn('static void desenhaSecundario(') +
    'static void bloco(void) { float a = 1, bx = 0, cyBtn = 100; int nb = 1;\n' +
    s[start:end] + '  (void)bx; (void)nb;\n}\n')
