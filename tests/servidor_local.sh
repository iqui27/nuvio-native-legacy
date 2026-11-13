# Test-only tooling for the local Workers. Source from tests/*.sh.
# Resolves an ALREADY INSTALLED Wrangler 4.x, builds an isolated scratch
# environment (own HOME/XDG, synthetic empty env file, temporary --persist-to),
# so `wrangler dev` runs fully on loopback without touching ~/.wrangler,
# .dev.vars, the project's default .wrangler/state, or any account.
# No npm/npx resolution, no downloads, no symlinks, no installs.

# Wrangler binary: explicit override wins (NUVIO_WRANGLER), then a project-local
# node_modules, then every existing ~/.npm/_npx cache dir whose package.json
# pins wrangler 4.x. Fails (nonzero) when nothing usable is installed: callers
# must NOT fall back to `npx wrangler@4` (registry resolution) and must not
# silently skip.
nv_wrangler_bin() {
  if [ -n "${NUVIO_WRANGLER:-}" ] && [ -x "${NUVIO_WRANGLER:-}" ]; then
    printf '%s\n' "$NUVIO_WRANGLER"; return 0
  fi
  local c
  for c in "$PWD/node_modules/.bin/wrangler" \
           "$PWD/servidor/juntos/node_modules/.bin/wrangler"; do
    [ -x "$c" ] && { printf '%s\n' "$c"; return 0; }
  done
  local h="${NUVIO_NPX_CACHE:-${HOME}/.npm/_npx}" d
  [ -d "$h" ] || return 1
  for d in "$h"/*/node_modules/wrangler; do
    [ -d "$d" ] || continue
    if grep -q '"version"[^0-9]*4\.' "$d/package.json" 2>/dev/null \
       && [ -x "${d%/wrangler}/.bin/wrangler" ]; then
      printf '%s\n' "${d%/wrangler}/.bin/wrangler"; return 0
    fi
  done
  return 1
}

# Isolated process env for every wrangler invocation. Arg1 = scratch dir the
# caller owns (mktemp -d). Child HOME/XDG point inside it so ~/.wrangler and
# real dotfiles are invisible; metrics and .dev.vars discovery are off; Cloud
# credential vars are blanked so nothing can be picked up from the shell env.
nv_isolate_env() {
  export HOME="$1"
  export XDG_CONFIG_HOME="$1/xdg-config"
  export XDG_CACHE_HOME="$1/xdg-cache"
  export XDG_STATE_HOME="$1/xdg-state"
  export WRANGLER_SEND_METRICS=false
  export CLOUDFLARE_API_TOKEN=
  export CLOUDFLARE_ACCOUNT_ID=
  export CLOUDFLARE_EMAIL=
  export CLOUDFLARE_API_KEY=
  export NUVIO_DEV_ENV_FILE="$1/empty.env"
  : > "$NUVIO_DEV_ENV_FILE"
}

# Generated dev config for servidor/recomendacoes: the real wrangler.toml minus
# the sections that must not run in a test:
#   [build]     -> runs tools/recomendacoes-guard.sh, which does `git fetch`
#   [[services]]-> remote service binding to nuvio-tv (unreachable locally and
#                 never exercised by the tests)
#   [triggers]  -> cron retention sweep (kept as empty list)
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

# Launch `wrangler dev` in the background on a distinct loopback port.
# Args: <config.toml> <workdir-log> ; echoes "<port> <pid>".
# Caller must have run nv_isolate_env first; NV_W is required.
# The spawned $pid is the `.bin/wrangler` NODE process; `nv_dev_stop` TERM /
# KILL it and walks its own children, so workerd/esbuild cannot survive as
# orphans (the real wrangler CLI is a CHILD of $pid, not a parent).
nv_dev_start() {
  local cfg="$1" logdir="$2" port pid
  [ -x "${NV_W:-}" ] || { echo "NV_W unset (run NV_W=$(nv_wrangler_bin))" >&2; return 1; }
  local p
  for p in $(seq $((8600 + RANDOM % 90)) 8999); do
    if ! nc -z 127.0.0.1 "$p" 2>/dev/null; then port=$p; break; fi
done
  [ -n "${port:-}" ] || { echo "no free loopback port" >&2; return 1; }
  "$NV_W" dev --local --config "$cfg" --env-file "$NUVIO_DEV_ENV_FILE" \
    --port "$port" --ip 127.0.0.1 --persist-to "$logdir/persist" \
    > "$logdir/dev.log" 2>&1 &
  pid=$!
  local i return_ready=
  for i in $(seq 1 160); do
    kill -0 "$pid" 2>/dev/null || { cat "$logdir/dev.log"; echo "wrangler dev died" >&2; return 1; }
    curl -sf -m 2 "http://127.0.0.1:$port/v1/saude" >/dev/null 2>&1 && break
    curl -sf -m 2 "http://127.0.0.1:$port/saude" >/dev/null 2>&1 && break
    sleep 0.25
  done
  curl -sf -m 2 "http://127.0.0.1:$port/v1/saude" >/dev/null 2>&1 && return_ready=1
  curl -sf -m 2 "http://127.0.0.1:$port/saude" >/dev/null 2>&1 && return_ready=1
  [ "${return_ready:-}" = 1 ] || { cat "$logdir/dev.log" >&2; kill "$pid" 2>/dev/null; echo "wrangler dev not ready" >&2; return 1; }
  printf '%s %s\n' "$port" "$pid"
}

# Kill the dev server returned by nv_dev_start AND the whole subtree below it:
# .bin/wrangler -> node cli.js -> workerd x2 / esbuild. A bare `kill $DEV`
# orphaned workerd on every earlier run. Usage: nv_dev_stop "$DEV".
nv_dev_stop() {
  local pid="$1" kids k i
  [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null || return 0
  kids=$(pgrep -P "$pid" 2>/dev/null) || true
  for k in $kids; do nv_dev_stop "$k" || true; done
  kill -TERM "$pid" 2>/dev/null || true
  for i in $(seq 1 20); do kill -0 "$pid" 2>/dev/null || return 0; sleep 0.1; done
  kill -9 "$pid" 2>/dev/null || true
}

# `wrangler d1 execute <db> --local` against a given config+persist dir.
# Args: <config.toml> <persist-dir> then extra args (--file f | --command sql).
# The persist dir MUST match the one given to nv_dev_start (same SQLite file).
nv_d1_exec() {
  local cfg="$1" persist="$2"; shift 2
  "$NV_W" d1 execute nuvio-recomendacoes --local --config "$cfg" \
    --persist-to "$persist" "$@"
}
