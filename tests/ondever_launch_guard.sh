#!/bin/bash
# #765 parte 2: .wgt nao deve encerrar o app ao lançar Onde assistir (try/catch).
set -euo pipefail
cd "$(dirname "$0")/.."
grep -q 'tizen.application.launch' src/ondever.c
grep -q 'try {' src/ondever.c
grep -q 'Nenhum add-on de fontes instalado' src/addons.c
echo "PASS: ondever launch com try/catch; mensagem de fontes em addons.c"
