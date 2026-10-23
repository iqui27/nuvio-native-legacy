#!/bin/bash
# tools/ctr.sh must keep the caller's requested platform (SDK selection is separate),
# default to docker off macOS (Linux CI) and pass docker flags through untouched.
# Also tools/tizen-tpk-spike.sh must find tools/ctr.sh.
cd "$(dirname "$0")/.."
T="$(mktemp -d)"; trap 'rm -rf "$T"' EXIT
for c in container docker; do printf '#!/bin/sh\necho "%s $*" >> "%s/log"\n' "$c" "$T" > "$T/$c"; done
printf '#!/bin/sh\necho "${FAKE_UNAME:-Darwin}"\n' > "$T/uname"
chmod +x "$T"/*
falha=0
caso() { # name expected-log-line env...
  local nome="$1" esperado="$2"; shift 2
  : > "$T/log"
  env -u NUVIO_CTR PATH="$T:$PATH" "$@" bash -c 'source tools/ctr.sh; ctr_run --rm --platform linux/arm/v5 --user 1:1 -v /a:/b:ro img true' >/dev/null 2>&1
  if grep -qxF -- "$esperado" "$T/log"; then echo "OK   $nome"; else echo "FAIL $nome: $(tr '\n' '|' < "$T/log")"; falha=1; fi
}
caso "helper preserves explicit platform" "container run --rm --platform linux/arm/v5 -v /a:/b:ro img true"
caso "docker passthrough" "docker run --rm --platform linux/arm/v5 --user 1:1 -v /a:/b:ro img true" NUVIO_CTR=docker
caso "linux defaults to docker" "docker run --rm --platform linux/arm/v5 --user 1:1 -v /a:/b:ro img true" FAKE_UNAME=Linux

saida="$(TIZEN_SDK="$T/nada" bash tools/tizen-tpk-spike.sh 2>&1)"
if grep -q 'Falta NativeToolchain' <<<"$saida"; then echo "OK   spike sources ctr.sh"; else echo "FAIL spike: $saida"; falha=1; fi
exit $falha
