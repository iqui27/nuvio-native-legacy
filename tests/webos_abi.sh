#!/usr/bin/env bash
# No cross compiler needed: exercise the readelf boundary with symbol fixtures.
set -euo pipefail
cd "$(dirname "$0")/.."
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
cat > "$T/readelf" <<'SH'
#!/usr/bin/env bash
cat "$ABI_FIXTURE"
SH
chmod +x "$T/readelf"
export READELF="$T/readelf" ABI_FIXTURE="$T/symbols"
header="Symbol table '.dynsym' contains 3 entries:"
printf '%s\n' "$header" ' 1: 0 0 FUNC GLOBAL DEFAULT UND SDL_GetTicks' \
  ' 2: 0 0 FUNC GLOBAL DEFAULT UND memcpy@GLIBC_2.19 (2)' \
  ' 3: 0 0 FUNC GLOBAL DEFAULT 12 SDL_GetTicks64' > "$ABI_FIXTURE"
bash tools/webos-abi-check.sh fixture.elf
reject() {
  printf '%s\n' "$header" " 1: 0 0 FUNC GLOBAL DEFAULT UND $1" > "$ABI_FIXTURE"
  if bash tools/webos-abi-check.sh fixture.elf > "$T/out" 2>&1; then
    echo "Guard accepted forbidden import: $1" >&2; exit 1
  fi
  grep -F "$2" "$T/out" >/dev/null
}
reject SDL_GetTicks64 'outside SDL 2.0.4 baseline'
reject SDL_CreateRGBSurfaceWithFormat 'outside SDL 2.0.4 baseline'
reject SDL_RWseek 'outside SDL 2.0.4 baseline'
reject memcpy@GLIBC_2.20 'exceeds GLIBC_2.19'
reject memcpy@GLIBC_2.100 'exceeds GLIBC_2.19'
reject __libc_dlopen_mode@GLIBC_PRIVATE 'private glibc ABI'
printf 'no symbols\n' > "$ABI_FIXTURE"
if bash tools/webos-abi-check.sh fixture.elf > "$T/out" 2>&1; then exit 1; fi
grep -F 'missing dynamic symbol table' "$T/out" >/dev/null
export READELF=false
if bash tools/webos-abi-check.sh missing.elf > "$T/out" 2>&1; then exit 1; fi
echo 'webOS ABI guard: positive, negative, numeric version and reader failure checks passed'
