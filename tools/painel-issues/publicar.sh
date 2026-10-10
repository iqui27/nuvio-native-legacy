#!/usr/bin/env bash
# Publica o painel no ZimaOS (LAN/Tailscale). Uso: publicar.sh [--dry-run] [worktree]
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
HOST="zimaos-lan"
DEST="/DATA/AppData/nuvio-painel/html"
NOME="nuvio-painel"
PORTA=8094
DRY=0
REPO="/Volumes/ExternalSSD/nv-2031-int"

for arg in "$@"; do
  case "$arg" in
    --dry-run) DRY=1 ;;
    -h|--help) echo "uso: $0 [--dry-run] [worktree]"; exit 0 ;;
    *) REPO="$arg" ;;
  esac
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# Respostas do dono: sem a API implantada o ssh/cat falha, e publicar continua.
if [ "$DRY" = 1 ]; then
  echo "[dry-run] faria: respostas.sh baixar $REPO (tolerando falha)"
elif ! "$AQUI/respostas.sh" baixar "$REPO"; then
  echo "AVISO: nao consegui baixar as respostas do dono (API ainda nao implantada?); publicando sem elas" >&2
fi

python3 "$AQUI/gerar.py" --repo "$REPO" --out "$TMP"

if [ "$DRY" = 1 ]; then
  echo "[dry-run] nenhum ssh sera feito. Faria:"
  echo "  ssh $HOST mkdir -p $DEST"
  echo "  rsync -az --delete $TMP/ $HOST:$DEST/   (arquivos: $(ls "$TMP" | tr '\n' ' '))"
  echo "  ssh $HOST: se a $PORTA ja responde (/index.html), so sincroniza os arquivos e nao toca em container"
  echo "  senao (primeira publicacao, ainda sem API):"
  echo "     se '$NOME' existe em 'docker ps -a' e parado: 'docker start $NOME'"
  echo "     se nao existe: confere a $PORTA livre (ss -ltn / docker ps); aborta se ocupada;"
  echo "     docker run -d --name $NOME --restart unless-stopped -p $PORTA:80 -v $DEST:/usr/share/nginx/html:ro nginx:alpine"
  echo "  (nenhum outro container e tocado)"
else
  ssh "$HOST" "mkdir -p '$DEST'"
  chmod 755 "$TMP"; chmod 644 "$TMP"/*
  rsync -az --delete "$TMP"/ "$HOST:$DEST/"
  # O openrsync do macOS ignora --chmod no diretorio raiz: garante no destino.
  ssh "$HOST" "chmod 755 '$DEST' && chmod -R a+rX '$DEST'"
  ssh "$HOST" bash -s -- "$NOME" "$PORTA" "$DEST" <<'REMOTO'
set -euo pipefail
nome="$1"; porta="$2"; dest="$3"
# Deploy versionado no ar (api/implantar.sh): a porta ja responde, entao os arquivos
# sincronizados acima chegam pelo bind mount. Nao religa o legado nem cria container.
if curl -fsS --max-time 5 -o /dev/null "http://127.0.0.1:$porta/index.html"; then
  echo "porta $porta ja servindo: so os arquivos foram sincronizados (nenhum container tocado)"
  exit 0
fi
# grep sem -q de proposito: o -q sai na primeira linha e mata o docker com SIGPIPE,
# o que com pipefail faria este if parecer "container nao existe".
if docker ps -a --format '{{.Names}}' | grep -x "$nome" >/dev/null; then
  if ! docker ps --format '{{.Names}}' | grep -x "$nome" >/dev/null; then
    docker start "$nome" >/dev/null
    echo "container $nome estava parado: iniciado"
  else
    echo "container $nome ja rodando: so os arquivos foram sincronizados"
  fi
else
  if ss -ltn 2>/dev/null | awk '{print $4}' | grep -Eq "[:.]${porta}$" \
     || docker ps --format '{{.Ports}}' | grep -Eq "[:.]${porta}->"; then
    echo "ERRO: porta $porta ja em uso no ZimaOS; nada foi criado" >&2
    exit 1
  fi
  docker run -d --name "$nome" --restart unless-stopped -p "$porta":80 \
    -v "$dest":/usr/share/nginx/html:ro nginx:alpine >/dev/null
  echo "container $nome criado na porta $porta"
fi
REMOTO
fi

echo
echo "Painel: http://192.168.1.20:$PORTA (LAN)"
echo "Fora de casa: use o hostname/IP Tailscale do ZimaOS na porta $PORTA. Nao expor publicamente."
