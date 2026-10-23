# Prepare USB drive (Settings > P2P)

Issue #205. Goal: user plugs a drive, taps one button, drive is ready and fast enough for the P2P cache.
Status: spec only. The only code in this job is the read-only probe `src/usbsonda.c` (fs type + ro flag).

## Findings: how the app sees USB today

`src/p2pmotor.c` `usbEscanear` scans only `/tmp/usb` (webOS), `/media/usb`, and `/opt/storage/usb` (`NV_TPK`).
It creates `<mount>/p2p` and proves a write. Reasons for rejection are logged by `agente/205-usbmotivo` (do not duplicate).
Android is not scanned at all: there is no `/storage/<uuid>` base. That is a gap, separate from formatting.

## Per platform

Sources: LG support (FAT32/NTFS; exFAT only on newer models), lgwebos.com wish list "Reading exFAT formatted USB",
Samsung Filesystem API (`removable_*`, privilege `externalstorage`), Stack Overflow 30875722 (Tizen `removable2/sda1`),
Android `StorageManager.java` (`format`/`partitionPrivate` are `@SystemApi`), Sony Android TV support 00152574
("Erase & format as removable storage"), Android `Settings.ACTION_MEMORY_CARD_SETTINGS`.

| | webOS (.ipk) | Tizen (.tpk) | Android TV (.apk) |
|---|---|---|---|
| Mount | `/tmp/usb/sdXN` | `removable2/sdXN` (web API label); native path `/opt/storage/usb/...` is what the code already assumes, unverified on device | `/storage/<UUID>` (public volume); app-writable only in `Android/data/<pkg>` via `getExternalFilesDirs()` or SAF |
| FAT32 | rw | rw | rw (portable) |
| exFAT | model-dependent; LG docs say FAT32/NTFS, newer firmware lists exFAT | rw on recent TVs; older: unverified | rw (portable) |
| NTFS | usually ro or unsupported by the app's view; not safe to assume rw | commonly ro | ro or unsupported (varies by OEM) |
| ext4 | not supported for plug-in drives | no | only as "internal storage" adoption (device-formatted) |
| App can format? | **No.** Jailed app, no format Luna API found in public docs. | **No.** Filesystem API has list/resolve/read/write, no format; `externalstorage` privilege is access only. | **No direct call.** `StorageManager.format`/`partitionPrivate` are `@SystemApi`. Can open the system storage screen (`Settings.ACTION_MEMORY_CARD_SETTINGS`, falls back to `ACTION_INTERNAL_STORAGE_SETTINGS`); user picks "Erase & format as removable storage". TV OEM screens vary, intent may be missing. |

Honest summary: **no platform lets us format silently.** Android can hand off to the system screen; webOS and Tizen need
a PC (or the TV's own USB tools, if the model has them) and we show short instructions.
Unverified items (webOS exFAT per model, Tizen native mount path, NTFS rw) must be confirmed with the probe logs from real TVs before the UI promises anything.

## Limits that matter for the cache

- FAT32: max file 4 GiB minus 1 byte. P2P cache pieces/files larger than that fail. Cache must cap file size on FAT32 (or reject FAT32 for torrents with big single files).
- exFAT: no practical limit; best default for a new format (cluster 128 KiB for 32 GB+ drives is fine).
- NTFS/ext4 on TV: do not rely on rw; probe `ro` flag.
- Cheap drives: sequential write often 5-15 MB/s, random much worse. A 4K stream plus P2P writes needs roughly 3-6 MB/s sustained plus headroom. Test at runtime, do not guess.
- Drives with a single huge partition can be slow to mount; no action needed.

## "Prepare drive" flow (Settings > P2P > Pendrive USB)

1. **Detect.** Scan as today. Show: drive name, size, free space, fs type (from probe), ro/rw. No drive: "Plug in a USB drive."
2. **Verdict** from fs + ro + free space:
   - ro -> "Drive is read-only. Cannot be used." + fix steps (step 5).
   - FAT32 -> "Works, files over 4 GB will not fit. exFAT is better." (offer step 5).
   - free < 8 GB -> "Too small for a good cache."
3. **Speed test** (user taps "Test drive"): write 32 MB to `<mount>/p2p/.nuvio-speedtest` in 1 MiB blocks with `fsync`, read it back, delete it (also on error/cancel, and sweep stale file at start). Show MB/s. Thresholds: >= 15 good, 6-15 ok, < 6 "too slow for 4K".
4. **Create folder.** `<mount>/p2p` + `.nuvio` owner marker (existing `usbMarcarDono`). "Drive is ready" with a one-line summary (fs, free, speed).
5. **Format or instructions** (only when verdict is not OK, or user taps "Format drive"):
   - Android: show destructive dialog (below); on confirm open `ACTION_MEMORY_CARD_SETTINGS` (fallback `ACTION_INTERNAL_STORAGE_SETTINGS`). The system screen does the format. App never formats.
   - webOS / Tizen: no format button. Show: "Format on a PC: exFAT (or FAT32 if the drive is under 32 GB). Name it NUVIO. Plug back in." with a short per-OS hint (Windows: right-click > Format; macOS: Disk Utility > ExFAT). Warn that formatting erases everything.
   - **Destructive confirmation (mandatory, before any hand-off to a format screen):** modal text `Erase "<label>" (<size, e.g. 32 GB>)? ALL files on this drive will be permanently erased.` Default focus on Cancel; confirm button reads "Erase drive". Never reuse a stale label/size: re-read from the probe at confirm time and abort if the drive changed.
6. Re-detect after return/replug and show the verdict again.

## Tasks (each small, each with a test)

1. **Probe** `usbsonda_montagem` (done here): fs type + ro flag from a /proc/mounts-format file. Test `tests/usbsonda.sh`.
2. **Drive info**: size/free via `statvfs` + probe -> struct; verdict function pure (fs, ro, free) -> enum. Test: table of cases.
3. **Speed test** function (path, bytes) -> MB/s, cleans temp file on all paths. Test: temp dir, file gone after, error path on ro dir.
4. **FAT32 cap**: P2P cache refuses/limits file > 4 GiB-1 on vfat. Test: fixture mount says vfat -> limit applied.
5. **Android scan base**: add `/storage/<UUID>` volumes (via JNI `getExternalFilesDirs`) to `usbEscanear`. Test: injected base like `usbExtra`.
6. **Settings UI**: "Prepare drive" row, verdict text, results screen, strings in all `idioma_*.h`. Test: UI string/key presence test per existing pattern.
7. **Android format hand-off**: destructive dialog + intent launch with fallback. Test: dialog text builder includes label and size and the word "erased"; intent chosen by availability (pure function).
8. **webOS/Tizen instructions screen**: static text, per-OS hint. Test: text builder.
9. **Device validation** (needs TVs): log fs/ro/mount path from real TCL (webOS test unit), Samsung, Android TV; resolve the "unverified" cells above. Evidence into the map.

## Not doing

Formatting from native code (mkfs, ioctl, Luna, Tizen). Auto-format without confirmation. Adoptable ("internal") storage conversion on Android: it ties the drive to one TV and is the system's choice, not ours.
