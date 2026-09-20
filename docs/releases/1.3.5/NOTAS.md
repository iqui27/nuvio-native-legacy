A Samsung release: the two settings that went from the test build to 1.3.4 are back to the values that were smooth on the AU7000, the app now heals itself when it cannot start, and — if you allow it — sends its log by itself so the remaining freezes can be found from the log instead of guessed. Also fixes the row list on LG when an account declares hundreds of catalogs.

## Samsung

- **Back to the rc1 values** (#80: "now it's back to freezing"). Between the rc1 and 1.3.4 only two things changed on Samsung: add-on queries went from two at a time to four, and GIF artwork decoded three frames ahead instead of one. Both are back to the rc1 values. Which of the two brought the freezing back is not known yet; the log line below is what will tell.
- **Long tasks with attribution.** Every 3 s the `[navegador]` log line now says the longest task the browser's main thread ran and who ran it (script, layout, GC…). In the logs received so far (Tizen 6.0 and 9.0 TVs with 1 GB) the main thread stops for 1–3 s while the app itself is idle; this line separates "our code" from "the TV's runtime".
- **Send logs automatically** (Settings › About). Off by default. On Samsung a card asks once on the first start; OK turns it on. When on, the app uploads the previous session's log when it opens (that is the one from a freeze) and this session's every minute — no passwords or keys, only what the app did and how long it took. Turn it off any time.
- **Self-healing start** (#77: could not get into the app, even after reinstalling). Two starts in a row that never draw a frame wipe the app's IndexedDB before mounting it — the only thing that survives a reinstall. You will have to log in again, but the app opens.
- The in-page log is written to storage every 30 s instead of every 10 s (a 200 KB synchronous write on the TV's main thread).

## Fixed

- **Rows missing from the reorder list, "Continue watching" and "Friends" pushed to the bottom** (LG, an account whose add-on declares 600+ catalogs). The known-rows table was full of rows that could not be evicted, so rows on the home never entered the list, and the app's own rows had been evicted and re-added at the end. App rows and the top of the home are never evicted now; the table grew from 320 to 768; a last-resort pass evicts a declared-but-not-shown row.
- The stream match on Resume, the Trakt comments selector, 64 seasons, the open card on the home, accent-colored buttons and the rest of 1.3.4 are unchanged.

## Notes

- The automatic upload goes to the same service as the Send log button and keeps logs for 30 days.
- If you are on 1.3.4 and want to help: update, accept the card, use the app normally for a day.

## Packages
- `space.nuvio.native.legacy_1.3.5_arm.ipk` — LG webOS (arm). Default texture budget.
- `space.nuvio.native.legacy_1.3.5_arm-highcache.ipk` — LG webOS (arm), 300 MB texture cache for TVs with ≥2 GB RAM (capped by the TV's RAM).
- `NuvioTV-1.3.5-tizen.wgt` — Samsung Tizen (WASM).
