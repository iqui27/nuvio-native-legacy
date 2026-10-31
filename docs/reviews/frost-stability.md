# Frost High regional source flicker

Baseline: `9b3b276ac`, after merging `integracao/2.1` into
`agente/21-21-frostpisca`. Evidence:
`/Volumes/ExternalSSD/tmp/rev/frostpisca/`.

## Root cause and fix

`vidroFundoFechar` captures and blurs the previous frame's glass regions.
A new glass outside that coverage falls through `vidroFonte` to a slot capture
of the live framebuffer. Foreground has already drawn at that point. The next
frame includes the new region in the backdrop capture and excludes foreground.
The glass therefore changes source in one frame although its content and
geometry are stationary. This is reproducible independently of texture uploads,
dither, the governor, or blur resolution changes.

High now retains a full RGB copy when capturing the backdrop. A new-region
slot blurs that retained copy. Modal veils still capture the live framebuffer;
their scratch copy is separate, so they cannot overwrite the saved backdrop.
The existing regional blur passes remain regional. Automatic and Performance
keep their prior paths.

This adds one full-size RGB texture (about 5.93 MiB at 1080p, 23.73 MiB at 4K)
and changes High's backdrop copy from regional to full-screen when the
backdrop needs refreshing. Static frames still reuse the cache. Desktop
measurements below do not establish the cost on TV tile-based GPUs; hardware
performance validation remains required.

## Reproduction and evidence

`bash tests/frost_flicker.sh verified` compiles the existing offline GL fixture
and runs hidden windows. No visible desktop window or OS input is used; keys
travel through `SDL_PushEvent`. Processes exit and destroy their SDL windows.
This uses the production TV layout and shared GL renderer on the desktop,
not the native AppKit sidebar.

Each layout records 60 consecutive frames, paced at 60 Hz, for static Home,
hero navigation, row navigation, poster reuploads and Detail entry. PNG encoding
happens after recording; eight consecutive frames from each sequence are saved.
GPU-complete render timings include `glFinish`, exclude readback, pacing and
PNG encoding. Runtime may miss the 60 Hz deadline on individual upload frames;
60 Hz is the requested cadence, not a measured guarantee.

The fixed-content reproduction first settles a small glass region, invalidates
and rebuilds its regional cache, then adds a distant glass over a green
foreground rectangle. Both rectangles and the neutral backdrop remain fixed.
Frame-to-frame integer luminance uses `(54R + 183G + 19B + 128) / 256`.
The sampled interior is x=1250..1449, y=400..599. Threshold: 2/255.

* Before: max luminance jump 18/255; 40,000 pixels exceed threshold.
* After: max luminance jump 0/255; zero pixels exceed threshold.
* Each comparison spans 60 frames (59 differences).
* The same failure/passing result occurs with all three layout configurations.

`before-current/layout0-region-strip.png` and
`after-current/layout0-region-strip.png` show eight consecutive frames.
`before-after-8-frames.png` stacks before above after. The first before panel
is green; the next is neutral. Every after panel is neutral.

The live Home captures intentionally contain moving art and changing text.
Whole-screen pixel differences there are not classified as flicker. Earlier
attempts to use entire supposedly quiet horizontal bands produced false
positives from real hero transitions and were discarded. The isolated
stationary-glass check supplies the quantitative regression proof.

Current 2.1 has Modern=0, Standard/Classic=1 and Dynamic=2. Apple TV was renamed
to Dynamic and is not a fourth distinct layout. Likewise glass quality choices
are Automatic=0, Performance=1 and High=2; there are no separately named Low
and Medium settings. Automatic/Performance controls are included in the final
check. Full/Light/Minimal are separate renderer-effects choices, not glass
quality names.

## Desktop timings

Matched initial before/after runs, mean GPU-complete milliseconds per frame:

| Layout | Static before / after | Hero before / after | Posters before / after | Detail before / after |
| --- | --- | --- | --- | --- |
| Modern | 1.724 / 1.804 | 1.868 / 1.886 | 1.889 / 1.800 | 2.194 / 2.055 |
| Classic | 2.048 / 1.280 | 2.226 / 1.624 | 2.086 / 1.346 | 2.558 / 1.706 |
| Dynamic | 0.945 / 1.597 | 1.241 / 1.650 | 1.673 / 1.290 | 4.525 / 2.609 |

These are short host samples, with asynchronous loading and other machine
activity. Some after samples are slower, so a claim of proven zero performance
regression would be unsupported. The Dynamic before row sample included a
49.513 ms/frame outlier, versus 1.389 after; it is not treated as a reliable
speedup. Logs preserve all measured scenarios. The final `verified` run also
includes the Automatic/Performance controls and local fixture logos to reduce
metadata fetching.

## Limits

The regional source pop is reproduced and fixed. Attribution of every flicker
seen by the tester is not established. Real TV GPU timing, exact tester scene,
all moving frosted-region quantitative comparisons, and proof of no TV
performance regression remain pending. No device was installed or modified;
no push or stash was used.
