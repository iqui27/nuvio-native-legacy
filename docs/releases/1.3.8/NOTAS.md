One change, on Samsung only: animated GIF artwork is now decoded and composed in the decode Worker instead of on the main thread (#84).

## Samsung

- **GIF artwork off the main thread** (#84). Until 1.3.7 every GIF frame was an `<img>` decoded on the main thread plus a canvas draw there — on an AU7000 that is ~15 decodes per second on the same thread that draws the interface, measured as 29–30 FPS with 20 janks while a GIF cover was on screen. The frames are now sent once to the app's decode Worker, which decodes each one, applies the GIF disposal rules and returns an ImageBitmap already scaled to the card; the main thread only uploads it to the GPU. If the Worker is not available the old path is used.
- Nothing else changed since 1.3.7, on purpose: if the GIF is smooth now, this is what did it; if it is not, the log says so without another variable in the way.

## Packages
- `space.nuvio.native.legacy_1.3.8_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.8_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM (capped by the TV's RAM).
- `NuvioTV-1.3.8-tizen.wgt` — Samsung Tizen (WASM).
