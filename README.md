# nuvio-native-legacy

I wanted Nuvio on my 2019 LG OLED (C9, webOS 4) and the web app was too heavy
for it. So I rewrote the TV client in C, on top of SDL2 and GLES2, talking to
the TV's own video pipeline. On the C9 the home screen runs at **60 fps with
zero janks**. The same code also builds for **Samsung Tizen** as WebAssembly,
and there's an experimental **Hisense VIDAA** build.

This is an **unofficial fork**. It's not affiliated with NuvioMedia, and all the
credit for Nuvio itself goes to them. It uses the same account, addons and
settings as the official app: sign in once and your stuff is there.

**[Latest release](https://github.com/iqui27/nuvio-native-legacy/releases/latest)**
· [Install guide](INSTALL.md)
· [Report a problem](#reporting-a-problem)

---

## Install

### LG webOS

The easiest way is the **Homebrew Channel**. Add this repository in the
channel's settings and the app shows up in the list, with updates:

```
https://github.com/iqui27/nuvio-native-legacy/releases/latest/download/repo.json
```

Or grab the `.ipk` from the [releases page](https://github.com/iqui27/nuvio-native-legacy/releases/latest).
Each release has two packages:

- `space.nuvio.native.legacy_X.Y.Z_arm.ipk`: the normal one. The texture budget
  follows the TV's RAM.
- `space.nuvio.native.legacy_X.Y.Z_arm-highcache.ipk`: a 300 MB texture cache,
  for TVs with 2 GB of RAM or more.

After the first install you don't need the PC anymore. When a new version is
out, the app shows a card with the release notes and **Update now**, and it
installs through the Homebrew Channel right on the TV.

Developer Mode (`ares-install`) works too; the steps are in [INSTALL.md](INSTALL.md).

### Samsung Tizen

Download `NuvioTV-X.Y.Z-tizen.wgt` from the release. It ships **unsigned** on
purpose: a distributor certificate locks the install to a fixed list of TVs, so
you sign it with your own certificate (Tizen Studio, against your TV's DUID)
and install it in Developer Mode. Samsung doesn't let an app install itself,
so on Samsung the update card only tells you there's a new version.

Tizen **5.5 or newer**. Tizen 4 (2018 sets) doesn't run it yet (#96).

### Hisense VIDAA (experimental)

VIDAA apps are hosted pages, not packages. Bookmark
`https://nuvio-recomendacoes.henriquef29.workers.dev/tv/` in the TV browser, or
sideload it in developer mode with `tools/vidaa-instalar/instalar.py` (see
`tools/vidaa-instalar/LEIAME.md`). It picks a multithreaded build when the
browser supports SharedArrayBuffer and falls back to single-threaded otherwise.
**Nobody has run it on a real VIDAA TV yet**, so reports are very welcome.
Known limits: no Dolby Vision in MP4, no AV1, no plain-HTTP video from the
HTTPS page, and YouTube trailers only in the single-threaded build.

---

## Which TVs

| TV | Status |
|---|---|
| LG webOS 4.x | **Measured** on my C9. This is where I develop. |
| LG webOS 5+ | **Works**, reported by users (2020 CX up to 2024 B4). I don't have one. |
| LG webOS 3.x | **Experimental** build on the `webos3` branch, [prerelease](https://github.com/iqui27/nuvio-native-legacy/releases/tag/native-webos3-exp.4). A tester on webOS 3.4.3 got it running. |
| LG webOS 2.x | Loads according to firmware symbol dumps. Never run. |
| Samsung Tizen 5.5+ | **Works**, many users. |
| Samsung Tizen 4 | Not yet (#96). |
| Hisense VIDAA | Experimental, untested. |

For webOS 3 the [web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos)
is still the safer choice: plain JavaScript, tuned for old Chromium and low RAM.

About root on LG: the app itself doesn't run as root, and installing through
the Homebrew Channel doesn't need it. What I haven't been able to measure is
video on a TV **without** root, because the app talks straight to the TV's media
service. If you have a non-rooted TV and video is a black screen while the UI
works, please tell me.

---

## What it does

**Account and profiles**
- QR sign-in on the TV; the session survives reboots
- Profiles with PIN; each profile has its own addons, home order, Continue
  Watching and progress
- Trakt and Simkl linked from the TV, **per profile**
- Addons, home catalog order, collections, library, watched items and settings
  come from your Nuvio account

**Home**
- Hero with trailers (Apple TV first, then YouTube), logos and backdrops from
  several sources (TMDB, metahub, Apple TV, fanart.tv, anime sources)
- Continue Watching, Trakt rows, collections, director pages, an Explore page
- Opens from a local cache in about a second, then updates from the network
- Magic Remote pointer on LG

**Playback**
- The TV's own video pipeline: HDR10, Dolby Vision on MP4, Atmos passthrough
  where the TV supports it
- Sources from your addons, with debrid (Real-Debrid, TorBox, Premiumize).
  Uncached TorBox torrents can be sent to download from the Sources sheet
- Automatic source: **Best source** or **First in list** (like Nuvio's auto-play
  first source), checked one at a time, with a limited number of retries
- **ASS/SSA subtitles** embedded in MKV (anime) drawn by the app with libass, read
  in the background over HTTP ranges while the video plays
- Addon subtitles, preferred subtitle/audio language that turns on by itself,
  subtitle style
- Up Next / More like this at the credits (MKV chapters and TheIntroDB), skip
  intro, resume
- Seek shows only the progress bar when the controls are hidden

**Also**
- Live TV (Xtream and Stalker portals) with a guide
- Blur unwatched episodes (spoiler protection)
- Recommend a title to a friend
- A **Diagnostic** that measures your TV and tunes the app for it (below)

What it doesn't do yet: Nuvio **plugins** (JavaScript scrapers). This app runs
Stremio addons, but it has no JavaScript engine for plugins (#134).

---

## The Diagnostic, and why I ask you to run it

Almost everything that made this app faster came from real logs, and I only own
one TV. So there's a **Diagnostic** in **Settings › Diagnostics › Diagnostics
and optimization**. It takes about a minute. It measures your TV (addons,
sources, every artwork source, memory, frame times), picks the best settings
for it, checks before and after, and keeps the new profile only if it's clearly
better than network noise.

Then it sends an anonymous report: timings and sizes. **No passwords, tokens,
addon URLs or personal lists.** Each report tells me the right memory budget,
network threads and image sizes for another TV model.

---

## Reporting a problem

1. Right after the problem happens, send the log from the app: **Settings ›
   About › Send log** (on LG you can also open the log panel from the remote).
2. Open an [issue](https://github.com/iqui27/nuvio-native-legacy/issues) with your
   TV model, the app version and what you did.

The log never carries credentials. Logs are kept for 30 days and are only used
to fix bugs. You can also turn on automatic log sending in Settings.

---

## Building

```bash
bash tools/mac.sh              # build and run on macOS (UI only, no video)
bash tools/arm.sh              # cross-compile for LG in Docker and deploy over ssh
bash tools/arm.sh --ipk        # also produce the .ipk
bash tools/tizen.sh            # build the Samsung target (WebAssembly)
bash tools/tizen-wgt.sh        # package it as an unsigned .wgt
bash tools/hb-repo.sh <ipk> <dir>   # Homebrew Channel repo files for a release
```

Tests are shell scripts in `tests/` (`bash tests/<name>.sh`); most of them build
a piece of `src/` on the host with the network or the TV faked.

Server URLs and client ids are **not in the source**. They come from a
`local.properties` file through `tools/env.sh`. The build refuses to package any
credential file and deletes the `.ipk` if one shows up (`tools/testa-ipk.sh`
checks by extracting the `ar` archive).

---

## Things I learned porting to webOS

Each one of these cost me at least a day:

- The video plane sits **behind** the GL surface and shows through an alpha
  hole. `glReadPixels` and the TV's screenshot never see it, so a capture during
  playback is black. That's the design, not a bug.
- `StarfishMediaAPIs` calls `exit(0)` when the process doesn't match `exeName` in
  its LS2 role. Silent, no crash. Talking to `com.webos.media` over LS2 directly
  is what works.
- webOS 5 removed `libAcbAPI`. The app carries both paths: ACB on webOS 2–4,
  SDL's exported window on 5+, picked at startup.
- When SAM launches a native app, stdout and stderr go to `/dev/null`. Nothing you
  print survives until you `freopen` a log file.
- The Back button doesn't arrive until the window asks for it, through the
  `SDL_WEBOS_ACCESS_POLICY_KEYS_BACK` hint, and only if it's set **before** the
  window is created. It then arrives as scancode 482.
- Don't link libcurl. The SDK has `.so.4`, the TV has `.so.5`, and the binary won't
  start. `dlopen` it at runtime and try both.
- The TV's libcurl (7.53) is old: reusing connections helps a lot, but only after
  a transfer finished cleanly.
- Firmware symbol dumps from [webosbrew](https://github.com/webosbrew/dev-toolbox-cli)
  tell you whether a binary will **load** on a TV you don't own. They don't tell
  you it works. That's how the webOS 3 build was checked: exactly one missing
  symbol (`SDL_CreateRGBSurfaceWithFormat`, SDL 2.0.5), replaced in
  `src/sdlcompat.h`.

And on Samsung, where the app is WebAssembly in the TV's browser: one busy main
thread and the whole app waits. Most of the Samsung speedups came from moving
work off it (image decoding in a Worker, no synchronous storage writes, no
per-frame file checks).

---

## Documentation

Most design notes are in Portuguese, because I wrote them for myself:
[PORT-LEGACY.md](PORT-LEGACY.md), [PLANO-CONTA-SYNC.md](PLANO-CONTA-SYNC.md),
[MEDIDAS-WEB.md](MEDIDAS-WEB.md), [FERRAMENTAS.md](FERRAMENTAS.md).
Release notes live in `docs/releases/`. [INSTALL.md](INSTALL.md) and this file are
in English.

## License

[GNU General Public License v3.0](LICENSE).

The C in `src/` is written from scratch, but the interface icons in
`deploy/app/art/icones/` come from the SVGs of the upstream web app, which is
GPLv3, as is the [web fork](https://github.com/iqui27/NuvioTVSmart-legacy-webos)
this grew alongside. So the whole thing goes out under the same terms.

"Nuvio", the logo and the wordmark belong to the original authors. The GPL covers
the code, **not** the name or the branding. They appear here only to say which
project this one comes from.

Artwork fetched at runtime (TMDB, Trakt, Apple TV, addon CDNs) belongs to its
owners and isn't covered by this license.
