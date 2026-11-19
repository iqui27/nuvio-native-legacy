# Test-only tooling for the local Workers. Source from tests/*.sh.
# Resolves an ALREADY INSTALLED Wrangler 4.x, builds an isolated scratch
# environment (own HOME/XDG, synthetic empty env file, temporary --persist-to),
# so `wrangler dev` runs on loopback without touching ~/.wrangler, .dev.vars,
# the project's .wrangler (state or cache), or any account. Default Wrangler/
# Miniflare network fetches are switched off in nv_isolate_env. No npm/npx
# resolution, no downloads, no installs.

# Wrangler binary, EXACT version only (default 4.149.0, the toolchain the
# offline/egress behaviour was verified against; NUVIO_WRANGLER_VERSION is the
# separate, explicit override). Order: NUVIO_WRANGLER, project node_modules,
# existing ~/.npm/_npx cache dirs. Fails (nonzero) when nothing matches:
# callers must NOT fall back to `npx wrangler@4` and must not silently skip.
nv_wrangler_ok() {
  python3 -c 'import json, sys; sys.exit(json.load(open(sys.argv[1])).get("version") != sys.argv[2])' \
    "$(dirname "$1")/../wrangler/package.json" "${NUVIO_WRANGLER_VERSION:-4.149.0}" 2>/dev/null
}
nv_wrangler_bin() {
  local c
  if [ -n "${NUVIO_WRANGLER:-}" ]; then
    [ -x "$NUVIO_WRANGLER" ] && nv_wrangler_ok "$NUVIO_WRANGLER" \
      && { printf '%s\n' "$NUVIO_WRANGLER"; return 0; }
    return 1
  fi
  for c in "$PWD/node_modules/.bin/wrangler" \
           "$PWD/servidor/juntos/node_modules/.bin/wrangler" \
           "${NUVIO_NPX_CACHE:-${HOME}/.npm/_npx}"/*/node_modules/.bin/wrangler; do
    [ -x "$c" ] && nv_wrangler_ok "$c" && { printf '%s\n' "$c"; return 0; }
  done
  return 1
}

# Isolated process env for every wrangler invocation. Arg1 = scratch dir the
# caller owns (mktemp -d). HOME/XDG/TMPDIR and every Wrangler/Miniflare cache
# point inside it. Default network fetches are OFF: WRANGLER_HIDE_BANNER skips
# the npm registry update check (it runs from the banner of `dev` and
# `d1 execute`), CLOUDFLARE_CF_FETCH_ENABLED=false stops Miniflare fetching
# workers.cloudflare.com/cf.json into the project .wrangler/cache. `--local`
# alone is NOT egress-proof. Cloud credentials, legacy CF_* names,
# CLOUDFLARE_INCLUDE_PROCESS_ENV and proxy vars are unset (never read).
nv_isolate_env() {
  export HOME="$1"
  export XDG_CONFIG_HOME="$1/xdg-config"
  export XDG_CACHE_HOME="$1/xdg-cache"
  export XDG_STATE_HOME="$1/xdg-state"
  export TMPDIR="$1/tmp"
  mkdir -p "$TMPDIR"
  export WRANGLER_SEND_METRICS=false WRANGLER_HIDE_BANNER=true
  export CLOUDFLARE_CF_FETCH_ENABLED=false
  export WRANGLER_CACHE_DIR="$1/wrangler-cache" MINIFLARE_CACHE_DIR="$1/mf-cache"
  export WRANGLER_LOG_PATH="$1/wrangler-logs"
  unset CLOUDFLARE_API_TOKEN CLOUDFLARE_ACCOUNT_ID CLOUDFLARE_EMAIL \
    CLOUDFLARE_API_KEY CF_API_TOKEN CF_ACCOUNT_ID CF_API_KEY CF_EMAIL \
    CLOUDFLARE_INCLUDE_PROCESS_ENV CLOUDFLARE_ENV CLOUDFLARE_CF_FETCH_PATH \
    HTTP_PROXY HTTPS_PROXY ALL_PROXY NO_PROXY http_proxy https_proxy \
    all_proxy no_proxy
  export NUVIO_DEV_ENV_FILE="$1/empty.env"
  : > "$NUVIO_DEV_ENV_FILE"
}

# Generated dev config for servidor/recomendacoes: the real wrangler.toml minus
# the sections that must not run in a test:
#   [build]     -> runs tools/recomendacoes-guard.sh, which does `git fetch`
#   [[services]]-> remote service binding to nuvio-tv (unreachable locally and
#                 never exercised by the tests)
#   [triggers]  -> cron retention sweep (section dropped)
# JWKS/JUNTOS URLs are rewritten to a closed loopback address so any code path
# that would fetch them fails instantly instead of touching the network.
# Arg1 = output path; the D1 name/id are kept identical so `d1 execute --local
# --persist-to` and `wrangler dev --persist-to` share one SQLite file.
nv_rec_test_config() {
  local src=servidor/recomendacoes/wrangler.toml out="$1" line skip=0
  : > "$out"
  while IFS= read -r line; do
    case "$line" in
      "[build]"|"[[services]]"|"[triggers]") skip=1; continue ;;
      "["*) skip=0 ;;
    esac
    [ "$skip" = 1 ] && continue
    case "$line" in
      NUVIO_JWKS_URL*) line='NUVIO_JWKS_URL = "http://127.0.0.1:1/auth/v1/.well-known/jwks.json"' ;;
      JUNTOS_URL*)     line='JUNTOS_URL = "http://127.0.0.1:1"' ;;
    esac
    printf '%s\n' "$line" >> "$out"
  done < "$src"
  # Absolute main: the config lives in scratch, the source does not.
  sed -i '' "s|^main = .*|main = \"${PWD}/servidor/recomendacoes/src/index.js\"|" "$out"
}

# Generated dev config for servidor/juntos: same file with an absolute main,
# so the project root Wrangler uses for .wrangler/tmp is the scratch dir.
nv_juntos_test_config() {
  sed "s|^main = .*|main = \"${PWD}/servidor/juntos/src/index.js\"|" \
    servidor/juntos/wrangler.toml > "$1"
}

# Launch `wrangler dev` in the background on a loopback port.
# Args: <config.toml> <workdir> <health-path> [extra dev args...]; sets
# NV_DEV_PORT and NV_DEV_PID (no subshell: the caller's EXIT trap must already
# be installed and reference "$NV_DEV_PID"). dev.log goes to <workdir>.
# Ready = OUR pid alive AND our dev.log says "Ready on http://127.0.0.1:<port>"
# AND the health path answers. A busy port (TOCTOU with another worker) makes
# our wrangler die or never log Ready: that is an honest failure, never a pass
# against someone else's server. On failure the whole owned tree is stopped
# and dev.log goes to stderr.
nv_dev_start() {
  local cfg="$1" w="$2" health="$3" i
  shift 3
  NV_DEV_PID=""
  [ -x "${NV_W:-}" ] || { echo "NV_W unset" >&2; return 1; }
  NV_DEV_PORT=""
  for i in $(seq $((8600 + RANDOM % 300)) 8999); do
    nc -z 127.0.0.1 "$i" 2>/dev/null || { NV_DEV_PORT=$i; break; }
  done
  [ -n "$NV_DEV_PORT" ] || { echo "no free loopback port" >&2; return 1; }
  python3 -c 'import os, sys; os.setsid(); os.execv(sys.argv[1], sys.argv[1:])' \
    "$NV_W" dev --local --config "$cfg" --env-file "$NUVIO_DEV_ENV_FILE" \
    --port "$NV_DEV_PORT" --ip 127.0.0.1 --persist-to "$w/persist" "$@" \
    > "$w/dev.log" 2>&1 &
  NV_DEV_PID=$!
  for i in $(seq 1 160); do
    kill -0 "$NV_DEV_PID" 2>/dev/null || break
    if grep -q "Ready on http://127.0.0.1:$NV_DEV_PORT" "$w/dev.log" 2>/dev/null \
       && curl -sf -m 2 "http://127.0.0.1:$NV_DEV_PORT$health" >/dev/null 2>&1; then
      kill -0 "$NV_DEV_PID" 2>/dev/null && return 0
    fi
    sleep 0.25
  done
  cat "$w/dev.log" >&2
  echo "wrangler dev not ready on our port $NV_DEV_PORT" >&2
  nv_dev_stop "$NV_DEV_PID"
  NV_DEV_PID=""
  return 1
}

# Stop the server from nv_dev_start and ONLY its own subtree
# (.bin/wrangler -> node cli.js -> workerd / esbuild). Snapshot descendants,
# The launcher creates a new session, so its process group remains addressable
# even if the parent dies first. TERM the parent for graceful disposal, then
# TERM/KILL the owned group and retained descendants. No name-based killing.
nv_dev_tree() {
  local k
  for k in $(pgrep -P "$1" 2>/dev/null || true); do echo "$k"; nv_dev_tree "$k"; done
}
nv_dev_stop() {
  local pid="${1:-}" tree k i
  [ -n "$pid" ] || return 0
  tree=$(nv_dev_tree "$pid")
  kill -TERM "$pid" 2>/dev/null || true
  for i in $(seq 1 30); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
  for k in $tree; do tree="$tree $(nv_dev_tree "$k")"; done
  kill -TERM -- "-$pid" 2>/dev/null || true
  kill -TERM $tree 2>/dev/null || true
  sleep 0.3
  kill -9 -- "-$pid" 2>/dev/null || true
  kill -9 "$pid" $tree 2>/dev/null || true
  return 0
}

# `wrangler d1 execute <db> --local` against a given config+persist dir.
# Args: <config.toml> <persist-dir> then extra args (--file f | --command sql).
# The persist dir MUST match the one given to nv_dev_start (same SQLite file).
nv_d1_exec() {
  local cfg="$1" persist="$2"; shift 2
  "$NV_W" d1 execute nuvio-recomendacoes --local --config "$cfg" \
    --persist-to "$persist" "$@"
}
