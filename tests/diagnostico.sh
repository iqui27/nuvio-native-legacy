#!/bin/bash
# DIAGNOSTICO E OTIMIZACAO: tabela por aparelho, regra de restaurar, fluxo da
# aplicacao automatica contra o tex_cache real e as chaves de traducao da tela.
#
# A terceira parte existe porque tests/i18n.sh nao ve as frases desta tela: elas
# chegam em i18n() por variavel (painelTitulo, metrica, BOTAO_ROTULO...), e a
# varredura so le literal dentro de txt_*. Foi assim que "Modo", "Artes" e
# "Resultado geral" apareceram em portugues com o app em ingles (C9, 22/09).
set -eu
cd "$(dirname "$0")/.."

cc tests/diagnostico.c src/perfiltv.c -Isrc -o /tmp/nuvio-diagnostico \
  -O1 -g -Wall -Wextra
/tmp/nuvio-diagnostico

sources=()
for source in src/*.c; do
  case "$source" in src/main.c|src/diagnostico.c) continue;; esac
  sources+=("$source")
done
# -DNV_DADOS_TRAVA_TESTE: a trava de arquivos do Tizen, que no Mac e no-op.
# Trava dupla no mesmo fio aborta (issue #113: congelava a Samsung).
cc "${sources[@]}" tests/diagnostico_fluxo.c -Isrc -o /tmp/nuvio-diagnostico-fluxo \
  -DNV_DADOS_TRAVA_TESTE -O1 -g -I/opt/homebrew/include -I/opt/homebrew/include/SDL2 \
  -L/opt/homebrew/lib -lSDL2 -lSDL2_image -lSDL2_ttf -lz -framework OpenGL \
  -Wno-deprecated-declarations -Wno-macro-redefined
/tmp/nuvio-diagnostico-fluxo | grep '^ok'

python3 - <<'EOF'
import re, importlib.util, sys
spec = importlib.util.spec_from_file_location('v', 'tools/varredura-i18n.py')
v = importlib.util.module_from_spec(spec); spec.loader.exec_module(v)
chaves = v.chaves_da_tabela()
# Iguais nos dois idiomas: unidade, sigla, nome proprio.
IGUAIS = {"MB", "px", "TMDB", "Trakt", "Metahub", "Logo", "dev", "manifest.json"}
faltam = []
for arq in ("src/diagnostico.c", "src/perfiltv.c"):
    txt = open(arq, encoding="utf-8").read()
    txt = re.sub(r"//[^\n]*", "", txt)
    txt = re.sub(r"/\*.*?\*/", "", txt, flags=re.S)
    for n, linha in enumerate(txt.split("\n"), 1):
        # Log, relatorio e arquivo ficam em portugues/ASCII de proposito.
        if re.search(r"\bprintf|ACRESCENTA|dados_(gravar|ler|apagar)|#include|"
                     r"SDL_CreateThread|snprintf\(ck|campo\(|memcmp|strstr", linha):
            continue
        for m in re.finditer(r'"((?:[^"\\]|\\.)*)"', linha):
            s = v.decodificar(m.group(1))
            if s in IGUAIS or s in chaves: continue
            if re.fullmatch(r"[a-z0-9_.:]*", s): continue          # identificador
            if not re.search(r"[A-Za-zÀ-ú]{2}", s): continue       # so formato
            if re.fullmatch(r"[%\d. →×·a-z/MB]*", s): continue      # "%ld / %ld MB"
            if s.startswith("versao="): continue                    # arquivo em disco
            faltam.append(f"{arq}:{n}: {s!r}")
if faltam:
    print("FALTA CHAVE em idioma_tab.h:"); print("\n".join(faltam)); sys.exit(1)
print("ok  toda frase da tela de diagnostico tem chave")
EOF
echo "diagnostico: tudo ok"
