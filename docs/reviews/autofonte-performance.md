# Automatic source selection latency

The partial-list decision gate waited for the configured deadline even after every add-on capable of beating the current source had answered. This affected sources below the quality ceiling, and add-on-order sources held by an earlier installed fallback add-on.

The fix releases the winner when the minimum possible group of every pending add-on is strictly worse than the winner's group. Group order precedes score, resolution ceiling and add-on order in `fonteauto_fila_o`. Therefore no pending result can displace this winner, regardless of the priority mode. No requests, timers, caches or threads were added.

Explicit wait-all, pending remembered choices, earlier/equal groups, exclusions and the existing retry limits retain their gates. With no configured group precedence, all sources share group zero and this optimization does not apply. The separate provider/add-on failover policy still waits for full collection in `app.c`; its ordering differs and cannot safely reuse this shortcut.

## Deterministic measurement

Run `bash tests/fonteauto.sh`. Fake add-ons use a virtual clock in 10 ms steps, with fixed reply delays of 400, 800 and 12000 ms. The first two add-ons belong to group zero; the last belongs to fallback group eight. None of the first two sources qualifies for the existing early quality-ceiling shortcut. The configured deadline is 5000 ms.

The test drives the real decision and queue functions. Scores supplied for the four modes represent the output of `streams.c`; the test does not benchmark its scoring or perform HTTP requests. Each mode's selected source is checked against the complete-list result. A second case gives every add-on the same group and replies within 1000 ms, checking that later better candidates still win. Existing tests cover remembered choices, strict add-on ordering, exclusions and failed-source retries.

| Priority | Pick before | Pick after | Selected source before/after |
| --- | ---: | ---: | --- |
| Balance | 5000 ms | 800 ms | B / B |
| Maximum quality | 5000 ms | 800 ms | A / A |
| Start fast | 5000 ms | 800 ms | B / B |
| Add-on order | 5000 ms | 800 ms | A / A |

Selection saves 4200 ms (84%). With every add-on answering within 1000 ms, selection improves from 1000 to 800 ms with the same winner. Explicit wait-all remains 12000 ms.

For a synthetic probe cost of 100 ms and decoder cost of 700 ms, Play-to-first-frame is 5800 ms before and 1600 ms after (72% lower). Those two downstream costs are fixed model inputs, not device measurements. The new `[player] play->first-frame` log records actual opening-to-playing latency on the existing first-frame signal, separately from `video_pronto()`, plus load-to-first-frame latency. Existing decision and verification/debrid logs provide the preceding stages. No TV playback or live account measurement was performed.

## Existing stage costs

- Collection already publishes each add-on's result incrementally. A VOD request round allows 30 seconds per add-on; a second chance can add another 30 seconds. An unknown manifest may first take up to 12 seconds. A decision normally has its own 5-second deadline; explicit wait-all can pay the full collection tail.
- Direct candidate probes allow 10 seconds; HLS playlist validation can add 8 seconds. The cached prefix already probes up to three candidates concurrently, preserving queue order. Native torrents and candidates outside that prefix remain sequential to avoid unnecessary debrid side effects.
- Native debrid API requests allow 15 seconds each. Resolution can require multiple requests and polling; Real-Debrid polls up to three times with one-second waits. This change does not speculate on additional torrents.
- Failed probes are excluded from the current list. Playback fallback remains bounded by the configured retry count; opening deadlines are 8 seconds without data and 30 seconds with data. Remembered choices and prepared-source reuse already exist.

The target regression failed with eight timing assertions before the production change and passed afterward, retaining the existing source-order and retry assertions.
