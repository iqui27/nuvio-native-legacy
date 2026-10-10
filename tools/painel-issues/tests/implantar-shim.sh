#!/usr/bin/env bash
# Simula api/implantar-remoto.sh com um "docker" e um "curl" falsos (sem docker, sem rede, sem ssh).
set -euo pipefail
AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REMOTO="$AQUI/../api/implantar-remoto.sh"
# O bloco REMOTO do publicar.sh: testado direto, com o mesmo docker/curl falsos.
REMOTO_PUB="$(awk "/<<'REMOTO'/{f=1;next} /^REMOTO\$/{f=0} f" "$AQUI/../publicar.sh")"
[ -n "$REMOTO_PUB" ] || { echo "publicar.sh: bloco REMOTO nao encontrado"; exit 1; }
W="$(mktemp -d)"; trap 'rm -rf "$W"' EXIT
FALHAS=0
ok()  { echo "PASS $1"; }
bad() { echo "FAIL $1"; FALHAS=$((FALHAS+1)); }
SH="$W/bin"; mkdir -p "$SH"
export ST="$W/st"

cat > "$SH/sleep" <<'S'
#!/bin/sh
exit 0
S
cat > "$SH/docker" <<'S'
#!/usr/bin/env bash
# estado: $ST/c/<nome> com "status porta"
C="$ST/c"; mkdir -p "$C"
cat >/dev/null   # se o script deixar o docker ler o stdin, o resto do script some
st()   { cut -d' ' -f1 "$C/$1"; }
porta(){ cut -d' ' -f2 "$C/$1"; }
cmd="$1"; shift
case "$cmd" in
  build) [ -z "${FALHA_BUILD:-}" ] ;;
  network) [ "$1" = create ] && touch "$ST/net" && exit 0; [ -f "$ST/net" ] ;;
  ps) unset s; todos=0; ex=0
      for a in "$@"; do [ "$a" = -a ] && todos=1; [ "$a" = status=exited ] && ex=1; done
      for f in "$C"/*; do [ -e "$f" ] || continue; n="$(basename "$f")"; s="$(cut -d' ' -f1 "$f")"
        if [ $ex = 1 ]; then [ "$s" = exited ] && echo "$n"
        elif [ $todos = 1 ]; then echo "$n"
        else [ "$s" = running ] && echo "$n"; fi
        s=""
      done; true ;;
  run) nome=""; p=""; rm=0
       while [ $# -gt 0 ]; do case "$1" in --name) nome="$2"; shift;; -p) p="$2"; shift;; --rm) rm=1;; esac; shift; done
       [ $rm = 1 ] && exit 0
       [ -e "$C/$nome" ] && { echo "docker shim: nome em uso: $nome" >&2; exit 125; }
       porta8094=""; case "$p" in 8094:*) porta8094=1;; esac
       if [ -n "$porta8094" ]; then
         [ -n "${FALHA_PORTA:-}" ] && exit 125
         for f in "$C"/*; do [ -e "$f" ] && [ "$(st "$(basename "$f")")" = running ] && [ "$(porta "$(basename "$f")")" = 8094 ] && exit 125; done
         case "${FALHA_MATA:-}" in
           term) echo "created -" > "$C/$nome"; kill -TERM "$PPID"; exit 0 ;;
           kill) echo "created -" > "$C/$nome"; kill -KILL "$PPID"; exit 0 ;;
         esac
         echo "running 8094" > "$C/$nome"
       elif [ -n "$p" ]; then echo "running tmp" > "$C/$nome"
       else echo "running -" > "$C/$nome"; fi ;;
  stop)  echo "exited $(porta "$1")" > "$C/$1" ;;
  start) if [ "$(porta "$1")" = 8094 ]; then
           for f in "$C"/*; do n="$(basename "$f")"; [ "$n" != "$1" ] && [ "$(st "$n")" = running ] && [ "$(porta "$n")" = 8094 ] && exit 1; done
         fi; echo "running $(porta "$1")" > "$C/$1" ;;
  rm) for a in "$@"; do case "$a" in -f) ;; *) rm -f "$C/$a";; esac; done ;;
  exec) [ -z "${FALHA_API:-}" ] ;;
  port) echo "127.0.0.1:45678" ;;
  *) echo "docker shim: $cmd?" >&2; exit 99 ;;
esac
S
cat > "$SH/curl" <<'S'
#!/usr/bin/env bash
url="${@: -1}"; C="$ST/c"
rodando() { for f in "$C"/*; do [ -e "$f" ] || continue; n="$(basename "$f")"; [ "$(cut -d' ' -f1 "$f")" = running ] || continue
  case "$n" in "$1"*) [ -z "${2:-}" ] || [ "$(cut -d' ' -f2 "$f")" = "$2" ] && echo "$n";; esac; done; }
api_up() { rodando nuvio-painel-api- | grep . >/dev/null; }
case "$url" in
  *:45678/*) [ -z "${FALHA_TEMP:-}" ] || exit 22; ;;
  *:8094/*)
    ng=""; for n in $(rodando nuvio-painel 8094); do case "$n" in nuvio-painel-api*) ;; *) ng="$n";; esac; done
    [ -n "$ng" ] || exit 22
    [ -n "${FALHA_POS:-}" ] && case "$ng" in *"$TS"*) exit 22;; esac ;;
  *) exit 22 ;;
esac
case "$url" in
  */api/saude) case "${ng:-versionado}" in nuvio-painel) exit 22;; esac   # nginx legado nao tem /api
               api_up || exit 22; echo '{"ok": true}';;
esac
S
cat > "$SH/ss" <<'S'
#!/bin/sh
[ -n "${PORTA_OCUPADA:-}" ] && echo "LISTEN 0 128 0.0.0.0:8094 0.0.0.0:*"   # ss de verdade: nada escutando
exit 0
S
chmod +x "$SH"/*

novo_cenario() { # recria base e estado: nginx legado servindo na 8094
  rm -rf "$W/base" "$ST"; mkdir -p "$W/base/html" "$W/base/api" "$ST/c"
  echo '{"decisoes":[{"id":"dec-a"}]}' > "$W/base/html/data.json"
  echo '{"versao":1,"respostas":{"dec-a":{"historico":[]}}}' > "$W/base/respostas.json"
  echo "running 8094" > "$ST/c/nuvio-painel"
}
roda() { # TS [VAR=val...] -> rc
  local ts="$1"; shift
  ( export PATH="$SH:$PATH" TS="$ts"; for kv in "$@"; do export "$kv"; done
    export SUF=7; bash -s -- "$W/base" < "$REMOTO" ) >"$W/out" 2>&1 && return 0 || return $?
}
estado() { cat "$ST/c/$1" 2>/dev/null | cut -d' ' -f1 || true; }
tem() { [ -e "$ST/c/$1" ]; }
espera() { if [ "$2" = "$3" ]; then ok "$1"; else bad "$1 (esperado $2, veio $3)"; fi; }
sem_novos() { ! ls "$ST/c" | grep -- "-$1-7$" >/dev/null; }
# O bloco REMOTO do publicar.sh, com o mesmo docker/curl falsos. Vai por arquivo e
# nao por stdin porque o docker falso le o stdin (igual o ssh bash -s real).
publica() {
  printf '%s\n' "$REMOTO_PUB" > "$W/pub-remoto.sh"
  ( export PATH="$SH:$PATH"; bash "$W/pub-remoto.sh" nuvio-painel 8094 "$W/base/html" ) >"$W/pub" 2>&1
}

echo "== sucesso (com migracao de respostas.json)"
novo_cenario; rc=0; roda 20260101000001 || rc=$?
espera "deploy ok rc" 0 "$rc"
espera "novo nginx rodando" running "$(estado nuvio-painel-20260101000001-7)"
espera "antigo parado, nao removido" exited "$(estado nuvio-painel)"
[ -f "$W/base/respostas/respostas.json" ] && [ -f "$W/base/respostas.json" ] && ok "respostas.json copiado e antigo mantido" || bad "migracao"
[ "$(cat "$W/base/.nginx-bom")" = nuvio-painel-20260101000001-7 ] && ok "marcador do ultimo bom" || bad "marcador"
echo "== segundo deploy: antigos versionados so parados"
rc=0; roda 20260101000002 || rc=$?
espera "2o deploy rc" 0 "$rc"
espera "1a versao parada" exited "$(estado nuvio-painel-20260101000001-7)"
espera "1a API parada" exited "$(estado nuvio-painel-api-20260101000001-7)"
espera "2a versao rodando" running "$(estado nuvio-painel-20260101000002-7)"
echo "== --limpar sem --sim: so lista, nao apaga"
limpar() { ( export PATH="$SH:$PATH" TS=x; bash -s -- "$W/base" limpar "${1:-}" < "$REMOTO" ) >"$W/out" 2>&1; }
rc=0; limpar || rc=$?
espera "limpar rc" 0 "$rc"
grep -q "removeria:.*nuvio-painel-20260101000001-7" "$W/out" && ok "lista o que removeria" || bad "nao listou o que removeria: $(cat "$W/out")"
grep "^--limpar removeria:" "$W/out" | grep -qv "20260101000002-7" && ok "o ultimo bom nao aparece na lista" || bad "ultimo bom na lista: $(grep '^--limpar removeria:' "$W/out")"
tem nuvio-painel-20260101000001-7 && ok "sem --sim nada foi apagado" || bad "apagou sem --sim"
tem nuvio-painel-api-20260101000001-7 && ok "API antiga intacta sem --sim" || bad "apagou API sem --sim"

echo "== --limpar --sim: remove so os parados velhos; legado e ultimo bom ficam"
rc=0; limpar sim || rc=$?
espera "limpar --sim rc" 0 "$rc"
tem nuvio-painel && ok "legado preservado" || bad "legado removido"
tem nuvio-painel-20260101000001-7 && bad "1a versao parada deveria sair" || ok "versao antiga removida"
tem nuvio-painel-api-20260101000001-7 && bad "1a API parada deveria sair" || ok "API antiga removida"
espera "atual intacto" running "$(estado nuvio-painel-20260101000002-7)"
espera "API atual intacta" running "$(estado nuvio-painel-api-20260101000002-7)"

echo "== publicar.sh com o deploy versionado no ar: so sincroniza, nao religa o legado"
rc=0; publica || rc=$?
espera "publicar rc" 0 "$rc"
espera "legado segue parado" exited "$(estado nuvio-painel)"
espera "versionado segue rodando" running "$(estado nuvio-painel-20260101000002-7)"
grep -q "so os arquivos foram sincronizados" "$W/pub" && ok "fala que so sincronizou" || bad "mensagem: $(cat "$W/pub")"

echo "== publicar.sh sem nada servindo: religa o legado parado"
echo "exited 8094" > "$ST/c/nuvio-painel-20260101000002-7"
echo "exited -" > "$ST/c/nuvio-painel-api-20260101000002-7"
rc=0; publica || rc=$?
espera "publicar rc" 0 "$rc"
espera "legado religado" running "$(estado nuvio-painel)"
grep -q "estava parado: iniciado" "$W/pub" && ok "mensagem do legado religado" || bad "mensagem: $(cat "$W/pub")"

echo "== publicar.sh sem container nenhum e porta livre: cria o legado"
novo_cenario; rm "$ST/c/nuvio-painel"
rc=0; publica || rc=$?
espera "publicar rc" 0 "$rc"
espera "legado criado na 8094" running "$(estado nuvio-painel)"
grep -q "criado na porta" "$W/pub" && ok "mensagem do container criado" || bad "mensagem: $(cat "$W/pub")"

echo "== publicar.sh sem container e com a porta ocupada por outro: aborta"
novo_cenario; rm "$ST/c/nuvio-painel"; echo "exited 8094" > "$ST/c/nuvio-painel-outro-1"
export PORTA_OCUPADA=1; rc=0; publica || rc=$?; unset PORTA_OCUPADA
[ "$rc" != 0 ] && grep -q "porta 8094 ja em uso" "$W/pub" && ok "abortou sem criar nada (rc=$rc)" || bad "deveria abortar (rc=$rc): $(cat "$W/pub")"
tem nuvio-painel && bad "criou container com a porta ocupada" || ok "nenhum container criado"

echo "== falha no build: nada tocado"
novo_cenario; rc=0; roda 20260101000003 FALHA_BUILD=1 || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "build falhou mas rc=0"
espera "antigo segue rodando" running "$(estado nuvio-painel)"
sem_novos 20260101000003 && ok "nenhum container novo" || bad "sobrou container novo"

echo "== API nao fica pronta"
novo_cenario; rc=0; roda 20260101000004 FALHA_API=1 || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo segue rodando" running "$(estado nuvio-painel)"
sem_novos 20260101000004 && ok "novos removidos" || bad "novos sobraram"

echo "== nginx falha na porta temporaria"
novo_cenario; rc=0; roda 20260101000005 FALHA_TEMP=1 || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo nunca parou" running "$(estado nuvio-painel)"
sem_novos 20260101000005 && ok "novos removidos" || bad "novos sobraram"

echo "== porta 8094 ocupada na troca"
novo_cenario; rc=0; roda 20260101000006 FALHA_PORTA=1 || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo religado" running "$(estado nuvio-painel)"
sem_novos 20260101000006 && ok "novos removidos" || bad "novos sobraram"

echo "== falha depois da troca (health check)"
novo_cenario; rc=0; roda 20260101000007 FALHA_POS=1 || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo religado" running "$(estado nuvio-painel)"
sem_novos 20260101000007 && ok "novos removidos" || bad "novos sobraram"
grep -q "restaurado: 8094 servindo" "$W/out" && ok "imprime o estado restaurado" || bad "sem mensagem de restauracao"

echo "== interrompido (SIGTERM) logo apos parar o antigo"
novo_cenario; rc=0; roda 20260101000008 FALHA_MATA=term || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo religado pelo trap" running "$(estado nuvio-painel)"
sem_novos 20260101000008 && ok "novos removidos" || bad "novos sobraram"

echo "== morto sem trap (SIGKILL) e reexecucao"
novo_cenario; rc=0; roda 20260101000009 FALHA_MATA=kill || rc=$?
[ "$rc" != 0 ] && ok "rc != 0 ($rc)" || bad "rc=0"
espera "antigo ficou parado" exited "$(estado nuvio-painel)"
rc=0; roda 20260101000010 || rc=$?
espera "reexecucao rc" 0 "$rc"
grep -q "recuperado: nuvio-painel religado" "$W/out" && ok "religou o ultimo bom antes de tudo" || bad "nao recuperou primeiro"
espera "novo servindo" running "$(estado nuvio-painel-20260101000010-7)"

echo "== trava: segunda execucao sai na hora, trava velha e retomada"
novo_cenario; mkdir "$W/base/.deploy.lock"; sleep 30 & DONO=$!; echo $DONO > "$W/base/.deploy.lock/pid"
rc=0; roda 20260101000011 || rc=$?; kill $DONO 2>/dev/null || true
[ "$rc" != 0 ] && grep -q "outra execucao" "$W/out" && ok "segunda execucao recusada (rc=$rc)" || bad "trava nao recusou"
espera "nada tocado" running "$(estado nuvio-painel)"; [ -d "$W/base/.deploy.lock" ] && ok "trava do dono preservada" || bad "apagou trava alheia"
echo 999999 > "$W/base/.deploy.lock/pid"; rc=0; roda 20260101000012 || rc=$?
espera "trava velha retomada, deploy ok" 0 "$rc"; [ ! -d "$W/base/.deploy.lock" ] && ok "trava liberada ao fim" || bad "trava sobrou"

echo "== recuperacao confere /api/saude: API parada e religada antes de tudo"
echo "exited -" > "$ST/c/nuvio-painel-api-20260101000012-7"
rc=0; roda 20260101000013 FALHA_BUILD=1 || rc=$?
espera "API do ultimo bom religada" running "$(estado nuvio-painel-api-20260101000012-7)"
grep -q "recuperado: nuvio-painel-api-20260101000012-7" "$W/out" && ok "mensagem de recuperacao da API" || bad "sem recuperacao da API"
espera "nginx bom seguiu rodando" running "$(estado nuvio-painel-20260101000012-7)"

echo "== falha limpa so o que esta execucao criou"
novo_cenario; echo "running -" > "$ST/c/nuvio-painel-api-20260101000020-7"   # de outra execucao, mesmo nome-base
rc=0; roda 20260101000014 FALHA_POS=1 || rc=$?
tem nuvio-painel-api-20260101000020-7 && ok "container alheio preservado" || bad "apagou container alheio"

[ "$FALHAS" = 0 ] && { echo "TUDO OK"; exit 0; } || { echo "$FALHAS falha(s)"; exit 1; }
