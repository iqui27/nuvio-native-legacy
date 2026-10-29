# TCL row-scroll memory regression

Range: c2e7d0ef9 (owner reports working) to e06c70bc8 (freeze).

The local mechanism check in `tests/tex_memory.py` finds no shared EGL upload context at c2e7d0ef9 or 9756ff9e9. It first appears at 44014cf78 and persists through 38e369735, d8b60bef5 and e06c70bc8. That commit submits artwork on a second shared context and calls glFinish before publication. Later fixes address budget starvation and startup failure, not driver contention. This is a source-level mechanism bisect, not a reproduction of the Mali driver stall.

The supplied log records system-wide low-memory killing, a 25,961 ms 3840x2160 decode, a 2,504.6 ms GL clear stall and 144 MB of cached GPU artwork. These support memory pressure plus GPU contention; they do not prove that a shared texture has two physical copies. The uploader allowed only one active upload, but retained decoded surfaces without a byte budget.

Changes:

- Remove shared upload context; retain mutex try-lock, CPU mip safeguards, upload stripes and budget-blocked selection.
- Android uses BitmapFactory bounds + inSampleSize for JPEG/PNG/WebP/GIF first frames. No full-size SDL fallback. Keep original dimensions for promotion, straight alpha for rendering, recycle the bitmap after copying pixels. Maximum decoded output is 2 Mi pixels; compressed inputs are capped at 8 MiB.
- Android automatic and maximum texture budgets: 32 MiB below 1500 MB/unknown RAM, 64 below 3000, 96 below 4000, 128 above. Existing cold-first eviction and trim handling remain; onLowMemory also forwards severe pressure.
- Reserve 48 MiB before Android decode. Active reservations plus retained decoded surfaces cannot exceed 64–96 MiB, scaled by texture budget. This conservative reservation covers compressed C/JNI input copies, bitmap/SDL output, reduction and mips at the enforced output limits. At most eight new requests are admitted concurrently. Compressed transfer bytes have an 8 MiB per-request ceiling; queued compressed responses wait when their total would exceed one quarter of the texture budget. Promotion admission charges both old and new GPU textures until publication.
- Android below 3 GB keeps first-frame artwork instead of starting the GIF/WebP animation worker.

Validation limits: host test instruments the actual native JPEG pixel allocation boundary for 3840x2160 -> 480x270 and checks Android MemTotal tiers. Android BitmapFactory/JNI compiles but is not executed on the host. Device scrolling, visual comparison, GPU residency and decoder internal allocations require subsequent TCL validation. No push or deployment.
