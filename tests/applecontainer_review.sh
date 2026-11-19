#!/bin/bash
# Regression coverage for the Apple Container review. No daemon or credentials needed.
set -uo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
failed=0
check() {
  if "$@"; then echo "PASS: $*"; else echo "FAIL: $*"; failed=1; fi
}

# Reject unsupported ARM32 VM requests at the runtime boundary.
cat > "$T/container" <<'SH'
#!/bin/bash
printf '%s\n' "$@" > "$CTR_LOG"
for arg in "$@"; do
  [ "$arg" != linux/arm/v5 ] || exit 42
done
[ "$1 $2" != 'image inspect' ] || exit 1
[ "$1" != build ] || exit 43
SH
chmod +x "$T/container"
export PATH="$T:$PATH" NUVIO_CTR=container CTR_LOG="$T/args"
# Run the actual build entry point; stop at image build before downloads/secrets.
bash tools/tpk.sh > "$T/build.log" 2>&1
rc=$?
if [ "$rc" = 43 ]; then echo 'PASS: Samsung build selects supported SDK host';
else echo "FAIL: Samsung SDK host selection rc=$rc (expected 43)"; failed=1; fi

# Compare arguments, not flattened strings: spaces and glob characters must survive.
check bash -c 'source tools/ctr.sh; ctr_run -v "/LG WEB:/LG WEB" -w "/LG WEB/*" image true'
printf '%s\n' run --rm -v '/LG WEB:/LG WEB' -w '/LG WEB/*' image true > "$T/expected"
check diff -u "$T/expected" "$T/args"

# Keep the existing Docker SDK path on Linux.
cat > "$T/docker" <<'SH'
#!/bin/bash
printf '%s\n' "$@" > "$CTR_LOG"
SH
chmod +x "$T/docker"
check env NUVIO_CTR=docker bash -c 'source tools/ctr.sh; if [ -f tools/tpk/sdk.sh ]; then source tools/tpk/sdk.sh; fi; ctr_run --platform "${TPK_PLATFORM:-linux/arm/v5}" "${TPK_IMAGE:-nuvio-tpk-sdk}" true'
printf '%s\n' run --platform linux/arm/v5 nuvio-tpk-sdk true > "$T/expected"
check diff -u "$T/expected" "$T/args"

# Scan controlled scripts through the real removal gate, including compound commands.
mkdir -p "$T/scan/tests" "$T/scan/tools"
cp tests/sem_docker.sh "$T/scan/tests/"
for code in 'docker run image true' 'if ! docker info; then exit 1; fi' \
            'test -f image || docker build .' \
            'CONTAINER_RUNTIME=${CONTAINER_RUNTIME:-docker}; "$CONTAINER_RUNTIME" run image true'; do
  printf '%s\n' "$code" > "$T/scan/tools/local.sh"
  if bash "$T/scan/tests/sem_docker.sh" > "$T/scan.log" 2>&1; then
    echo "FAIL: Docker gate accepted $code"; failed=1
  else
    echo "PASS: Docker gate rejects $code"
  fi
done
printf '%s\n' '# docker run example' 'echo "docker build example"' 'ctr_run image true' > "$T/scan/tools/local.sh"
check bash "$T/scan/tests/sem_docker.sh"
check bash tests/ctr_platform.sh
check bash tests/sem_docker.sh
for script in tools/ctr.sh tools/tpk.sh tools/tpk/sdk.sh tools/tpk/deps.sh \
              tools/tpk-testa.sh tools/tizen-uep-spike.sh tools/tizen-memfd-spike.sh \
              tools/tizen-tpk-spike.sh tools/build-dts-ffmpeg.sh tests/tpk_sdk_arm32.sh \
              tests/tpk40_tls.sh tests/mkvass_grande.sh; do
  check bash -n "$script"
done
check git diff --check
exit "$failed"
