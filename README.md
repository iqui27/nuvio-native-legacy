# nuvio-native-legacy

A streaming app for LG webOS written in C99 against SDL2 and GLES2, instead of
JavaScript in the TV's browser. Built and measured on a 2019 OLED65C9
(webOS 4.10): **60.0 fps, 0 janks** on the home screen, worst frame 18-19 ms.
It also builds for Samsung Tizen, as WebAssembly inside a `.wgt`.

Video plays through the TV's own pipeline — LS2 to `com.webos.media` — on a
hardware plane behind the GL surface, not in a browser. On webOS 2 through 4 the
plane is held by `libAcbAPI`; LG removed that library in webOS 5, so newer sets
use SDL's exported-window API instead. The binary carries both paths and picks
at startup.

**[Download the .ipk](https://github.com/iqui27/nuvio-native-legacy/releases/latest)**
· [Install guide](INSTALL.md)

## Which build fits your TV

| | webOS 2.x | webOS 3.x | webOS 4.x | webOS 5+ | Samsung Tizen | Hisense VIDAA |
|---|---|---|---|---|---|---|
| **This one** (native C/SDL2) | loads, untested | **experimental build** | **yes**, measured | **reported working** | yes, as a `.wgt` | **experimental, not verified on a device** |
| [Web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) (JavaScript) | — | preview builds | yes | yes | — | — |

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

### webOS 3, and how far back this binary actually reaches

The table said "no" here for a long time, on the assumption that a 2016 TV would
need real porting work. It does not. There was **one symbol** in the way.

webosbrew publishes symbol dumps of retail firmware
([dev-toolbox-cli](https://github.com/webosbrew/dev-toolbox-cli), `common/data`).
Comparing the undefined symbols of the ARM binary against those dumps — using a
webOS 4.10 set as the baseline, because that is where the app demonstrably runs
— exactly one of 199 was missing on the old firmware:

```
webOS 2.2.3  (2015, W15M)   SDL_CreateRGBSurfaceWithFormat
webOS 3.4.0  (2016, W16N)   SDL_CreateRGBSurfaceWithFormat
webOS 3.9.2  (2017, W17H)   SDL_CreateRGBSurfaceWithFormat
webOS 4.10.0 (2019, W19P)   nothing
```

That function arrived in SDL 2.0.5. webOS 3.4 ships SDL 2.0.2, webOS 3.9 ships
2.0.4, and the 2019 set ships a 2.0.4 that LG evidently patched, because it
exports the symbol. `src/sdlcompat.h` does the same thing with
`SDL_PixelFormatEnumToMasks` + `SDL_CreateRGBSurface`, both present since SDL
2.0.0, and replaces the three call sites unconditionally — not behind a `dlsym`
— so the TV we can test on exercises exactly the code the TV we cannot test on
will run.

The video path needed nothing. It was the expected blocker, and it is not one:
`video_iniciar()` already picks between `libAcbAPI` and SDL's exported window
depending on which exists, the exported window is the webOS 5+ branch, and
webOS 3.4 ships `libAcbAPI.so.1.0.0` — the same path the C9 uses. One further
symbol, `AcbAPI_setMediaAudioData`, is absent on webOS 3.4.0 and had to become
optional instead of fatal; its only caller is a diagnostic `printf`.

Verified with webosbrew's own tool, not only a script of our own:

```
webosbrew-ipk-verify -r ">=3,<4"   All OK, exit 0
webosbrew-ipk-verify -r ">=2"      All OK, exit 0
webosbrew-ipk-verify               across all 14 bundled firmwares, only
                                   webOS 1.2 and 1.4 report anything
                                   (SDL_GL_GetDrawableSize, SDL_GetBasePath)
```

**A symbol existing is not the same as it working**, and webosbrew says plainly
that they have been bitten by treating it as proof. Nobody here owns a webOS 3
set — but somebody else does: a tester installed it on a **webOS 3.4.3** set,
reported that it runs, and the bugs he did find turned into four prereleases in
two days. So this is now at the same level of evidence as webOS 5+: it works on
someone else's TV, not on mine. Three things a symbol dump still cannot answer,
and a report of "it runs" does not settle either:

- **Codec.** The [moonlight-tv compatibility
  matrix](https://github.com/mariotaku/moonlight-tv/wiki/Compatibility-Status)
  marks webOS 3.x as no H.265 and no HDR. Most debrid content today is x265, so
  that may be the real ceiling on usefulness rather than anything in this code.
- **RAM.** 624 MB total on one of the reporting sets, around 300 MB free.
- **The uMediaServer payload.** This app talks Luna/JSON directly, which
  sidesteps the `libplayerAPIs` C++ ABI churn that forces webosbrew's samples
  into four build variants — but the shape of the `load` document may still
  differ by generation.

The build lives on the `webos3` branch and ships as a prerelease
([native-webos3-exp.4](https://github.com/iqui27/nuvio-native-legacy/releases/tag/native-webos3-exp.4)).
For a webOS 3 TV the
[web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos) is still the
safer choice — plain JavaScript, tuned for Chromium 53 and low RAM, with
preview builds people have actually run. Reports from either are welcome.

Both are unofficial and not affiliated with NuvioMedia.


### Hisense VIDAA

VIDAA apps are hosted URLs, not system packages. This port loads from
`https://nuvio-recomendacoes.henriquef29.workers.dev/tv/` and automatically picks
between two builds: **multithreaded** (mt) when the TV supports SharedArrayBuffer
and cross-origin isolation, with a **single-threaded fallback** (st) otherwise.

**Status:** experimental, not verified on a real device. Both builds start and
reach the home screen in desktop Chrome; nobody has run them on a VIDAA TV yet.
Reports welcome: if you accept automatic log upload when the app asks, the log
reaches us with your TV's browser version, the codecs it reports, and the
remote keys it did not recognize.

**Known limits:**

- Dolby Vision in MP4 is avoided (reported to crash VIDAA browser)
- AV1 video not decoded (browser limitation)
- HTTP video sources go through a relay for metadata, but HTTP video itself cannot
  play from an HTTPS page
- Audio/subtitle track switching depends on what the browser exposes
- YouTube trailers only work in single-threaded mode

**Install:** Open the TV browser and bookmark `https://nuvio-recomendacoes.henriquef29.workers.dev/tv/`,
or sideload with developer mode using `tools/vidaa-instalar/instalar.py` (DNS spoofing method).

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
- The Back button does not arrive at all until the surface asks for it. The
  webOS compositor swallows it and opens the app bar unless LG's SDL Wayland
  backend declares otherwise, through the `SDL_WEBOS_ACCESS_POLICY_KEYS_BACK`
  hint — which is read only when the window is *created*, so setting it later
  does nothing. With the hint on it arrives as webOS's own scancode 482, not as
  `SDLK_AC_BACK` and not as the 461 that web apps see. (This entry used to
  describe a FOCUS_LOST → FOCUS_GAINED pair. That was true of an older build and
  became actively wrong: `FOCUS_LOST` appears nowhere in `src/` today.)
- Firmware symbol dumps answer "will this load" without owning the TV. See the
  webOS 3 section above for the method and for what it does not prove.
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
