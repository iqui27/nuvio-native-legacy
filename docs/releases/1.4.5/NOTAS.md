TorBox P2P torrents play again, the home stops rebuilding itself from scratch, and seeking shows only the progress bar.

## Fixed

- **TorBox P2P / uncached torrents**: picking a torrent without a link in Sources did nothing and stayed on "loading". Now the app looks for a cached copy on every debrid service you have and, if there is none, asks TorBox to download it and shows the progress ("TorBox is downloading this torrent (37%)"). Pick it again when it's done. Automatic mode still plays only cached sources and puts uncached ones (⏳) last.
- **Home taking a long time to show your catalogs**: collections, catalog order or row settings arriving from the account in the middle of the home load threw away everything already fetched and started over (up to 85 s on some Samsung TVs). Now the rows are just rearranged, and the catalog cache is kept for the next start.
- **Samsung 1 GB**: animated GIFs (avatars, collection covers) no longer animate on 1 GB TVs, get a memory budget on 2 GB, and stop when they leave the screen. GIF avatars now show their picture when not focused.
- **LG animated WebP avatars** failed to load; they now show the first frame.
- **LG TVs under 1.2 GB** no longer get 4 network threads from the Diagnostic.
- Trakt/Simkl credential push rejected by the server is no longer retried on every token renewal.

## Changed

- **Seeking with the controls hidden** (#128): left/right shows only the progress bar and time. Down or OK still opens the full controls.

## Notes

- Samsung: the package is unsigned on purpose; sign it with your own certificate as before.
- If a TorBox torrent still doesn't start, send a log right after picking it: the lines starting with `[debrid]` say what TorBox answered.

## Packages
- `space.nuvio.native.legacy_1.4.5_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.4.5_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM.
- `NuvioTV-1.4.5-tizen.wgt` — Samsung Tizen (WASM), unsigned; sign it with your own certificate as before.
