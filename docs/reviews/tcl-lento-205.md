# TCL UI stalls: source/log investigation

Base: 1137c9f39 (agente/205-lento-tcl, integracao/2.0.5). Offline analysis of
`/Volumes/ExternalSSD/tmp/rev/logs-issue/tcl-lento-1915.txt`; no device measurements.

## Findings

`src/main.c:2170` times the entire `tex_bombear(3)` call as `bomb`.
That includes cache locks, pruning, GL allocation/pixel/mip uploads, sampling
warm-up, publication and surface disposal. The reported byte count includes
CPU mip levels; it is not bytes read from disk or a time limit.

The decisive records are log lines 1491 and 1515: one 393216-byte base texture
has `upload-total total_ms=8075 gl_ms=8075`, followed by `bomb=8074.5`.
`gl_ms` starts after the initial cache lock and excludes the publication lock.
Its scope includes glTexImage2D, CPU mip uploads, GL error queries, texture
parameters and gfx_tex_aquecer. Thus the 8 seconds are in this GL section,
not waiting for that image to decode. Decode publishes a completed surface;
file reads, reduction, palette and CPU mip construction run without the cache
mutex. Existing timing cannot distinguish the individual GL calls or OS
scheduling inside that interval. It cannot prove a disk/swap wait.

Concurrent TCL IME/Binder calls take 1644, 5435 and 7111 ms; system-wide
pressure is consistent with the owner's free RAM/swap evidence. GL residency
allocation/driver synchronization is a plausible cause, not a proven exact
kernel wait. The separate 639 ms decode is background CPU work. The sustained
30–45 FPS also occurs with zero pending textures: expensive draws/overdraw
(e.g. Detail 55.6 ms, fill 6.34x) are a separate problem. Neither is evidence
that HTTP read-ahead runs during Home rendering.

`git diff v2.0.8..HEAD -- src/tex_cache.c src/gfx.c src/main.c src/text.c`
is empty. Android source also has no changes against that tag. These render
paths predate today's merges; tag equality does not prove equal runtime state.

## Today's features

- PR #673 (merge 5610b3d05): moviebuffer.c is a no-op under NV_ANDROID.
  On supported native builds video.c creates it only when opening HTTP
  playback, after validating preset/protocol. dts_engine.c lazily mallocs the
  ring in read_fluxo_pending; close_fluxo frees it. video_parar invokes
  moviebuffer_stop, which cancels/joins workers and destroys the engine.
  No ring allocation on Home/Detail/Settings. Supported-platform startup can
  wait for a probe and cancellation join, but this is playback startup only.
- Music (merge 11f07f2a4): eight fixed album entries, about 83 KiB of metadata;
  Detail open starts an idempotent detached fetch, not an idle polling loop.
  Artwork uses the existing texture cache. mus_estado copies one album under
  a short mutex; worker parses outside that lock. Rapid navigation can leave
  multiple fetches in flight; no evidence this explains the logged GL stall.
- Live (440074537, merge 681d8487a): source-selection worker checks at most
  eight candidates; up to eight workers, 64 KiB maximum response each, cancellation
  checked and workers joined before publication. Not an idle Home task.
- ur673 (merge 493675b3e): native read-ahead/source compatibility follow-ups;
  same Android exclusion, no added render/texture changes.

## Patch and proof scope

The pump now defers selection/stripe validation when the shared cache mutex
is contended. A failed CPU mip allocation falls back to linear sampling,
not synchronous glGenerateMipmap on the render thread. Stage timing separates
pixel submission, mip/filter setup and warm-up on slow uploads.

The regression uses the real pump with a worker holding its mutex for 250 ms,
then models missing CPU mip levels. Before the patch it asserts after waiting
253 ms (rc 134); after the patch it returns in 0 ms and never generates GPU
mips in CPU mode (bash tests/tex_nonblocking.sh, rc 0).
`bash tools/build-local.sh` returns rc 0; existing ATOMIC_VAR_INIT warning only.
`git diff --check` returns rc 0.

This is a bounded fix for two avoidable blocking paths, not proof the TCL
8-second GL stall is eliminated. Publication still takes the shared cache
lock, and GLES calls remain synchronous. A hard guarantee against driver
stalls requires moving GL uploads to a shared-context worker with asynchronous
completion; that cross-platform lifecycle change is not implemented here.
New stage logs are needed to identify the exact remaining driver operation.
No push or deployment.

## P1 follow-up

Android now submits decoded artwork on one shared EGL context with a private
1x1 pbuffer. One ENVIANDO slot retains its surface and previous texture until
all base/mip submissions and glFinish complete on the worker. The frame pump
uses try-lock publication and skips renderer warm-up, whose mutable state
belongs to the drawing context. Large uploads use the same worker rather than
frame-thread stripes. Shutdown joins before freeing surfaces or contexts.
Other production platforms retain the existing pump. If shared-context startup
fails, Android retains the synchronous fallback; that case has no stall guarantee.

The injected 250 ms GL call blocks the committed baseline for 251 ms (assertion,
rc 134); the worker pump returns in 0 ms. The module regression passes (rc 0),
including queued retention, contended publication, missing CPU mips and a large
promotion retaining its old artwork until completion. Final local build and
C syntax/whitespace checks pass. These are host checks; Android EGL lifecycle,
device FPS, memory and visual behavior remain unverified. No push or deployment.
