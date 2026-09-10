# nuvio-native-legacy

A streaming app for LG webOS written in C99 against SDL2 and GLES2, instead of
JavaScript in the TV's browser. Built and measured on a 2019 OLED65C9
(webOS 4.10): **60.0 fps, 0 janks** on the home screen, worst frame 18-19 ms.
It also builds for Samsung Tizen, as WebAssembly inside a `.wgt`.

Video plays through the TV's own pipeline — LS2 to `com.webos.media` — on a
hardware plane behind the GL surface, not in a browser. On webOS 4 the plane is
held by `libAcbAPI`; LG removed that library in webOS 5, so newer sets use SDL's
exported-window API instead.

**[Download the .ipk](https://github.com/iqui27/nuvio-native-legacy/releases/latest)**
· [Install guide](INSTALL.md)

## Which build fits your TV

| | webOS 3 | webOS 4.x | webOS 5+ | Samsung Tizen |
|---|---|---|---|---|
| **This one** (native C/SDL2) | no | **yes**, measured | **reported working** | yes, as a `.wgt` |
| [Web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) (JavaScript) | preview builds | yes | yes | — |

**This table used to say webOS 5+ did not work, and that was wrong.** The video
path once depended on `libAcbAPI`, which LG removed in webOS 5; since then the
app falls back to SDL's exported-window API and plays fine without it. Nobody
updated the table, so it told people the opposite of the truth for a while.
Corrected after a report from a 2024 LG B4 where playback works
([#26](https://github.com/iqui27/nuvio-native-legacy/issues/26)).

The honest status per row: webOS 4 is **measured** here on a C9. webOS 5+ is
**reported** by users, not verified by us — there is no such set on this bench.
One piece of that path is optional: if the TV does not expose the source-crop
call, video plays but the zoom/aspect modes do nothing. The app says which case
it is in, in its log.

webOS 3 is not targeted at all. For those TVs the
[web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) is the one to
use — plain JavaScript, tuned for Chromium 53 and low RAM, and it carries
preview builds for webOS 3 (C8, B7).

Both are unofficial and not affiliated with NuvioMedia.

## What works

- Sign in on the TV with a QR code; the session survives reboots
- Multiple profiles, PIN checked server-side
- Addons, layout settings, TMDB key and watch progress come from the account
- Trakt and Simkl linked from the TV itself, through their device-code flows
- Continue Watching, library, search, collections, director pages, settings

## What is not verified

**Only tested on a rooted C9.** It should install through Developer Mode or the
Homebrew Channel with no root, but the app needs LS2 access to
`com.webos.media`, and whether a non-rooted install grants that has not been
measured. If it does not, expect the UI to run and video to be a black screen.
[INSTALL.md](INSTALL.md) explains the reasoning and what evidence there is.
Reports from non-rooted TVs are welcome.

The `.ipk` is **~47 MB**, most of it bundled artwork so the home screen has
something to show before you sign in. It used to be 175 MB and to carry the
packager's own catalogue and addon keys; the packaging script now excludes every
personal file and verifies the exclusion after building, deleting the package if
one appears.

## Building

```bash
bash tools/mac.sh              # build and run on macOS (UI only, no video)
bash tools/arm.sh              # cross-compile in Docker, deploy over ssh
bash tools/arm.sh --ipk        # also produce the .ipk
bash tools/tizen.sh            # build the Samsung target (WebAssembly)
bash tools/tizen-wgt.sh        # package it as an unsigned .wgt
```

The `.wgt` ships unsigned: installing it needs your own Samsung certificate,
created in Tizen Studio against your TV's DUID. `tools/tizen-wgt.sh` prints the
steps when it finishes.

Server URLs and client ids are **not in the source**. They travel from a
`local.properties` file to the compiler command line through `tools/env.sh`; the
build refuses to package a credential file and deletes the `.ipk` if one appears
(`tools/testa-ipk.sh` verifies it by extracting the `ar` archive — `tar tzf` on
an `.ipk` lists three names and passes even when a secret is inside).

## Notes for anyone porting to webOS

Written down because each one cost a day:

- The video plane sits *behind* the GL surface, revealed through an alpha hole.
  `glReadPixels` and the TV's capture service never see it — a screenshot during
  playback is fully black. That is the model, not a bug.
- `StarfishMediaAPIs` calls `exit(0)` when the process does not match `exeName`
  in its LS2 role. Not a crash, and the journal stays silent. Talking to
  `com.webos.media` over LS2 directly is what works.
- When SAM launches a native app, stdout and stderr go to `/dev/null`. Every
  printf is discarded until you `freopen` a log file.
- The Back button never arrives as a key: it appears as FOCUS_LOST →
  FOCUS_GAINED within a few ms. A real exit is FOCUS_LOST with no return.
- Do not link libcurl. The SDK ships `.so.4`, the TV has `.so.5`, and the binary
  will not start. `dlopen` at runtime, trying both.

## Documentation

Most design documents are in Portuguese, since they were written for the author:
[PORT-LEGACY.md](PORT-LEGACY.md), [PLANO-CONTA-SYNC.md](PLANO-CONTA-SYNC.md),
[MEDIDAS-WEB.md](MEDIDAS-WEB.md), [FERRAMENTAS.md](FERRAMENTAS.md).
[INSTALL.md](INSTALL.md) and this file are in English.

## License

[GNU General Public License v3.0](LICENSE).

GPLv3 is not an arbitrary pick. The C in `src/` is written from scratch, but the
interface icons in `deploy/app/art/icones/` are rasterised from the SVGs of the
upstream web app — seven of them still ship as `.svg` — and that app is GPLv3,
as is the [web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) this
project grew alongside. Derived assets carry the licence of what they derive
from, so the whole thing goes out under the same terms the rest of the ecosystem
uses.

"Nuvio", the logo and the wordmark belong to the original authors. The GPL
covers the code, **not** the name or the branding — they appear here only to
identify the project this one is derived from. Unofficial and unaffiliated.

Artwork fetched at runtime (TMDB, Trakt, add-on CDNs) belongs to whoever owns
it and is not covered by this licence.
