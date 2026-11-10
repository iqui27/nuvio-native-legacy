#!/bin/bash
# tools/mac.sh incremental objects, on a synthetic fixture copy (never the app,
# never real local.properties, never ~/.nuvio). MAC_SH=<script> tests another copy.
set -euo pipefail
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/nuvio mac objects.XXXXXX")
trap 'rm -rf "$work"' EXIT
root="$work/repo dir"; log="$work/cc.log"; bin="$work/out dir/nuvio mac"
mkdir -p "$root/tools" "$root/src/dts" "$work/bin" "$work/home" "$work/tmp dir" "$work/out dir"
cp "${MAC_SH:-tools/mac.sh}" "$root/tools/mac.sh"; cp tools/native-objects.mk "$root/tools/"
echo 1.2.3 > "$work/ver"
cat > "$root/tools/env.sh" <<EOF
#!/bin/bash
printf -- '-DNV_VERSAO=\\\\"%s\\\\" -DNV_SIMKL_APP=\\\\"Nuvio\\\\"' "\$(cat "$work/ver")"
EOF
cat > "$root/tools/rust-mkvcore.sh" <<'EOF'
#!/bin/bash
set -e; cd "$(dirname "$0")/.."; mkdir -p "rust out"
echo 'const char *rust_fn(void) { return "rust"; }' > "rust out/r.c"
/usr/bin/cc -c "rust out/r.c" -o "rust out/r.o"; /usr/bin/ar rcs "rust out/libmkvcore.a" "rust out/r.o"
echo "$(pwd)/rust out/libmkvcore.a"
EOF
cat > "$work/bin/cc" <<'EOF'
#!/bin/bash
c=0; for a in "$@"; do case $a in *.c) echo "C $a :: $*" >> "$LOG"; c=1;; esac; done
[ $c = 1 ] || echo "L $*" >> "$LOG"
exec /usr/bin/cc "$@"
EOF
chmod +x "$root/tools/"*.sh "$work/bin/cc"
echo 'int h_val(void);' > "$root/src/h.h"
cat > "$root/src/main.c" <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include "h.h"
const char *rust_fn(void);
int main(void) {
  if (getenv("RAN")) fclose(fopen(getenv("RAN"), "w"));
#ifdef NV_RUST_MKVCORE
  printf("%s %s %s %d\n", NV_VERSAO, NV_SIMKL_APP, rust_fn(), h_val());
#else
  printf("%s %s c %d\n", NV_VERSAO, NV_SIMKL_APP, h_val());
#endif
  return 0;
}
EOF
echo '#include "h.h"
int h_val(void) { return 7; }' > "$root/src/a.c"
echo 'int b_marker(void) { return 1; }' > "$root/src/b.c"
echo 'int d_marker(void) { return 2; }' > "$root/src/dts/d.c"

fail=0
check() { if eval "$2"; then echo "PASS $1"; else echo "FAIL $1"; fail=1; fi; }
run() { : > "$log"; env -i HOME="$work/home" NUVIO_DADOS="$work/dados" TMPDIR="$work/tmp dir" \
  PATH="$work/bin:/usr/bin:/bin" LOG="$log" RAN="$work/ran" NUVIO_MAC_SO_COMPILAR=1 \
  NUVIO_MAC_BIN="$bin" "$@" bash "$root/tools/mac.sh" >"$work/out" 2>&1 && rc=0 || rc=$?; }
compiled() { grep -c '^C ' "$log" || true; }
later() { sleep 1; touch "$@"; }

run; check "clean build rc0, compiles all 4" '[ "$rc" = 0 ] && [ "$(compiled)" = 4 ]'
check "Rust1 compile flag on every TU" '[ "$(grep -c "^C .*-DNV_RUST_MKVCORE" "$log")" = 4 ]'
check "Rust1 links archive" 'grep -q "^L .*rust out/libmkvcore.a" "$log"'
check "embedded settings" '[ "$("$bin")" = "1.2.3 Nuvio rust 7" ]'
check "b object linked" 'nm "$bin" | grep -q b_marker'
run; check "no-op compiles zero" '[ "$(compiled)" = 0 ]'
later "$root/src/b.c"; run; check "touched .c compiles one" '[ "$(compiled)" = 1 ] && grep -q "^C src/b.c" "$log"'
later "$root/src/h.h"; run; check "header compiles dependents only" '[ "$(compiled)" = 2 ] && ! grep -qE "^C src/(b|dts/d).c" "$log"'
run NUVIO_RUST_MKVCORE=0; check "Rust0 separate cache" '[ "$(compiled)" = 4 ] && ! grep -q -e NV_RUST_MKVCORE -e libmkvcore "$log"'
check "Rust0 binary" '[ "$("$bin")" = "1.2.3 Nuvio c 7" ]'
run; check "Rust1 cache kept" '[ "$(compiled)" = 0 ] && grep -q libmkvcore "$log"'
echo 9.9.9 > "$work/ver"; run; check "changed flags isolated cache" '[ "$(compiled)" = 4 ] && [ "$("$bin")" = "9.9.9 Nuvio rust 7" ]'
echo 1.2.3 > "$work/ver"; run; check "original flags cache kept" '[ "$(compiled)" = 0 ]'
rm "$root/src/b.c"; run; check "deleted source not linked" '! nm "$bin" | grep -q b_marker'
echo 'syntax error' >> "$root/src/dts/d.c"; run
check "compiler failure nonzero" '[ "$rc" != 0 ]'
check "compile-only never launched" '[ ! -e "$work/ran" ]'
check "fresh HOME/NUVIO_DADOS untouched" '[ -z "$(ls -A "$work/home")" ] && [ ! -e "$work/dados" ]'
exit $fail
