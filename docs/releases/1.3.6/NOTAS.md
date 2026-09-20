Two main-thread savings that the 1.3.5 logs pointed at. The new `longtask` line said the stalls on Samsung are the app's own script, not the TV's runtime — so this release cuts what the app itself does on that thread.

## Performance

- **Samsung: a third less WebAssembly code.** The frame loop yields to the browser through Asyncify, and until now Asyncify instrumented *every* function that could be on the stack at that moment — that is, the whole app: every draw, decode and network function carried stack-unwind code. Instrumentation is now limited to the frame loop itself. The `.wasm` went from 4.17 MB to 2.95 MB (−29%); less code to compile at start and less to run on every call.
- **Translation lookup cached.** With the interface in English, every text line drawn ran a binary search over ~1500 entries, every frame. A hash cache turns that into one pass over the string. Measured on the desktop profiler over 35 s of home browsing: 268 ms → 44 ms of main-thread time; the app's non-idle main-thread time dropped from 4.7 s to 2.6 s in the same run. On a TV CPU the same work costs several times more.
- Function names are kept in the `.wasm` so profiles (the TV's Web Inspector or a desktop Chromium) name what is running instead of `wasm-function[729]`.

## Logs

- LG: the automatic upload runs every 5 minutes instead of every minute (an LG session with the player open fills the 200 KB in under a minute). Samsung keeps 1 minute.

## Notes

- Both Samsung settings stay at the rc1 values; the AU7000 reporter confirms 1.3.5 smooth. The logs from this release show, per session, whether the long tasks shrank.
- No functional changes.

## Packages
- `space.nuvio.native.legacy_1.3.6_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.6_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM (capped by the TV's RAM).
- `NuvioTV-1.3.6-tizen.wgt` — Samsung Tizen (WASM).
