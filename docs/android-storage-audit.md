# Android storage audit

Paths below are relative to Android `filesDir` or `cacheDir`. Default budget:
80 MiB shared regenerable LRU + 60 MiB P2P + two 2 MiB logs + at most
6 MiB guide spools = 150 MiB. Explicit seek-cache and P2P size overrides remain
user choices. Packaged resources and durable user data are outside this budget.

| Writer | Before (filesDir unless noted) | After |
| --- | --- | --- |
| `tex_cache.c`, native `cachearte.c` | `dados/cache`; up to 512 MiB, proportional LRU, pinned images exempt | `cacheDir/regenerable/images`; 64 MiB local ceiling, mandatory shared 80 MiB LRU without pin exemption |
| `trailerapple.c`, `trailerimdb.c` | `dados/trailer`; 20 MiB periodic oldest-file pruning | `cacheDir/regenerable/trailer`; shared 80 MiB LRU |
| `mkvass.c` subtitles/font sidecars | `dados/mkvass-*`; 20 MiB prefix pruning | `cacheDir/regenerable/mkvass-*`; shared LRU |
| `assrender.c` downloaded Arabic fonts | `dados/fontes`; no aggregate cap | `cacheDir/regenerable/fontes`; shared LRU; bundled fonts stay installed |
| `epg.c` | `dados/epg-*`; TTL, network response ceiling, no aggregate disk cap | `cacheDir/regenerable/epg-*`; shared LRU |
| `guia.c` Xtream/Stalker/M3U channel inventories | `dados/ltv-*`; per-load bounds, no aggregate disk cap | `cacheDir/regenerable/ltv-*`; shared LRU; anonymous guide spools bounded to 2 + 4 MiB |
| `m3u.c` playlist response | `dados/m3u-cache-*`; TTL, no aggregate cap | `cacheDir/regenerable/m3u-cache-*`; shared LRU |
| `plugins.c` repository manifests and JS | `dados/plugins-c-*`; TTL, no aggregate cap | `cacheDir/regenerable/plugins-c-*`; shared LRU; installed repository/configuration state stays in `dados/plugins-*` |
| `catalogo.c` | `dados/catalogo-*.bin`; no aggregate cap | `cacheDir/regenerable/catalogo-*`; shared LRU |
| `descoberta.c`, `fichameta.c` | `dados/meta-*`, `ficha-*`, `loc-texto.txt`; slot/response bounds | `cacheDir/regenerable`; shared LRU |
| `intro.c` anime index | `dados/anime-fribb.*`; one index + retry, no aggregate cap | `cacheDir/regenerable`; shared LRU |
| `serieaud.c`, `seriefrases.c`, `noticias.c`, `mapa.c`, `recomenda.c`, `corviva.c`, `negcache.inc`, `tendencia.c` | audience/quotes/news/explore/feed/color/API/trend response files under `dados`; mixed TTL, item limits, or periodic pruning | explicitly identified response names under `cacheDir/regenerable`; shared LRU |
| `atualizacao.c` APK downloads | `dados/atualizacao`; two-package retention | `cacheDir/regenerable/atualizacao`; shared LRU; FileProvider updated |
| `p2pmotor.c` / engine | `dados/p2p`; default 1536 MiB hard ceiling + rolling window; stop cleanup | `cacheDir/engine/p2p`; default 60 MiB hard / 28 MiB soft; existing stop-after-writers cleanup retained; oversized streams without a viable window refused |
| `android.c`, `main.c`, `ArranqueVigia.kt` logs | `dados/nuvio.log`, previous log; previous >2 MiB removed, current unbounded | `cacheDir/logs`; current truncates at 2 MiB; previous <=2 MiB; crash capture adds at most 256 KiB before process exit |
| Kotlin `CacheMidia.kt` | `cacheDir/nv-seek`; Media3 LRU 256/512/1024 MiB, disabled by default | same explicit opt-in sizes; clear action stops player, drops retained session and removes spans after delayed release |
| `dados.c` durable state, `xtream.c`, account/provider modules | `dados`: profiles, sessions, settings, accounts, progress, add-ons, preferences, journals, quotas | retained; never classified by the cache clear allowlist |
| Activity resource extraction | `filesDir/res`: packaged art/fonts copied once per installation | retained as installed resources, not a downloaded response cache |

Android linker wraps native `fclose` and `rename`, covering direct stdio writers
as well as atomic writes without changing other platform behavior. Cache reads
touch file mtime for LRU. Completed writes enforce the shared ceiling, including abandoned temporary
files; in-flight writes may transiently exceed it until rename/close. Active guide spools are
bounded separately. P2P's existing watchdog checks the hard limit every two
seconds, so its ceiling also has an in-flight overshoot window.

Startup discards only known obsolete response files from `dados`, old engine
files and old logs. It does not migrate durable state. Clear-cache deletes the
regenerable tree's contents without following symlinks, retains directories used
by live workers, stops P2P, drops Media3 seek spans and clears logs. Downloads
already in flight may repopulate the cache afterward.

Validation: `bash tests/android_storage.sh` covers over-budget LRU eviction,
nested caches, symlinks, and preservation of profile/account/progress/add-on
sentinels. `python3 tools/varredura-i18n.py` checks the new settings strings.
Android TV device/UI validation was not run; no deployment or push.
