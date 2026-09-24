#!/bin/bash
# REPOSITORIO DO HOMEBREW CHANNEL (webOS), gerado a cada release.
#
# O Homebrew Channel aceita repositorios extras: uma URL que devolve um JSON
# com `packages[]`, cada um com o manifesto do app (id, versao, ipkUrl,
# sha256). Mesmo formato do repositorio oficial (repo.webosbrew.org/api/apps.json).
#
# SEM SERVIDOR: os dois arquivos vao ANEXADOS a release, e a URL que a pessoa
# cadastra no canal e a de "ultima release" do GitHub, que redireciona sempre
# para a mais nova:
#   https://github.com/iqui27/nuvio-native-legacy/releases/latest/download/repo.json
#
# Uso: bash tools/hb-repo.sh <pacote.ipk> <pasta-de-saida>
# Gera <saida>/webosbrew.manifest.json e <saida>/repo.json. Publica-los e
# trabalho do `gh release create/upload` da receita de release.
set -euo pipefail
cd "$(dirname "$0")/.."

IPK="${1:?uso: hb-repo.sh <pacote.ipk> <saida>}"
SAIDA="${2:?uso: hb-repo.sh <pacote.ipk> <saida>}"
REPO="iqui27/nuvio-native-legacy"
mkdir -p "$SAIDA"

python3 - "$IPK" "$SAIDA" "$REPO" <<'EOF'
import hashlib, json, os, sys
ipk, saida, repo = sys.argv[1], sys.argv[2], sys.argv[3]
app = json.load(open("deploy/app/appinfo.json"))
versao = app["version"]
nome = os.path.basename(ipk)
# O nome do anexo tem de ser o do arquivo publicado: e dele que sai a ipkUrl.
esperado = f"{app['id']}_{versao}_arm.ipk"
if nome != esperado:
    sys.exit(f"hb-repo.sh: esperava {esperado}, recebi {nome}")
h = hashlib.sha256(open(ipk, "rb").read()).hexdigest()
base = f"https://github.com/{repo}/releases/download/v{versao}"
icone = f"https://raw.githubusercontent.com/{repo}/v{versao}/deploy/app/icon-large.png"
manifesto = {
    "id": app["id"],
    "version": versao,
    "type": "native",
    "title": "Nuvio (native legacy)",
    "appDescription": "Unofficial native Nuvio client for LG webOS",
    "iconUri": icone,
    "sourceUrl": f"https://github.com/{repo}",
    # O app roda sem root (uid do jail); o que depende de root e so o video
    # em algumas TVs, e isso o canal nao tem como saber. Ver INSTALL.md.
    "rootRequired": False,
    "ipkUrl": f"{base}/{nome}",
    "ipkHash": {"sha256": h},
    "ipkSize": os.path.getsize(ipk),
}
indice = {
    "paging": {"page": 1, "count": 1, "maxPage": 1, "itemsTotal": 1,
               "prevUrl": None, "nextUrl": None},
    "packages": [{
        "id": app["id"],
        "title": manifesto["title"],
        "iconUri": icone,
        "manifestUrl": f"{base}/webosbrew.manifest.json",
        "manifest": manifesto,
        "pool": "main",
        "shortDescription": manifesto["appDescription"],
    }],
}
json.dump(manifesto, open(os.path.join(saida, "webosbrew.manifest.json"), "w"), indent=2)
json.dump(indice, open(os.path.join(saida, "repo.json"), "w"), indent=2)
print(f"hb-repo.sh: {versao} sha256={h[:12]}... -> {saida}/repo.json")
EOF
