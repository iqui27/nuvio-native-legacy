<picture>
  <source media="(max-width: 640px)" srcset="docs/assets/nuvio-readme-banner-mobile.png">
  <img src="docs/assets/nuvio-readme-banner.png" alt="Nuvio Native Legacy. A native TV client for LG webOS, Samsung Tizen and Android TV (experimental). Independent, unofficial fork." width="1600">
</picture>

<p align="center">
  <a href="https://github.com/iqui27/nuvio-native-legacy/releases/latest"><strong>Download latest release</strong></a>
  &nbsp;·&nbsp; <a href="INSTALL.md">Install guide</a>
  &nbsp;·&nbsp; <a href="#reporting-a-problem">Report a problem</a>
  &nbsp;·&nbsp; <a href="https://discord.gg/9NWr6SHyzJ">Discord</a>
  &nbsp;·&nbsp; <a href="https://www.patreon.com/cw/CraaazyDevs"><strong>Support on Patreon ♥</strong></a>
</p>

I wanted Nuvio on my 2019 LG OLED (C9, webOS 4) and the web app was too heavy
for it. So I rewrote the TV client in C, on top of SDL2 and GLES2, talking to
the TV's own video pipeline. On the C9 the home screen runs at **60 fps with
zero janks**. The same code also runs **natively on Samsung Tizen** (TVs from
2018 on), builds for Samsung as WebAssembly, and there's an experimental
**Hisense VIDAA** build.

This is an **unofficial fork**. It's not affiliated with NuvioMedia, and all the
credit for Nuvio itself goes to them. It uses the same account, addons and
settings as the official app: sign in once and your stuff is there.

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

Two builds, both on every release. Install either with
[Apps2Samsung](https://github.com/Apps2Samsung/Apps2Samsung), which signs the
package for your TV's DUID (packages ship **unsigned** on purpose: a
distributor certificate would lock the install to a fixed list of TVs).

#### Native `.tpk` (recommended if your TV is listed)

The same C app as on LG, running directly on the TV (OpenGL ES, no web engine
underneath). Pick the file for your TV:

| TV (year) | Tizen | File |
|---|---|---|
| 2018–2020 | 4.0 – 5.5 | `Nuvio-X.Y.Z-NuvioTpk40.tpk` |
| 2021 | 6.0 | `Nuvio-X.Y.Z-NuvioTpk60.tpk` |
| 2022–2023 | 6.5 – 7 | `Nuvio-X.Y.Z-NuvioTpk65.tpk` |
| 2024+ | 8 – 9 | `Nuvio-X.Y.Z-NuvioTpk.tpk` |

Not sure which Tizen you have? Apps2Samsung shows it when it connects.

- **Updates itself** (new in 1.5.4, so the first real run is the next
  release): when a new release is out, the app
  downloads the new native library, checks its SHA-256 against the release and
  switches to it on the next launch — no reinstall. If a release changes the
  app shell itself, the notes will say to reinstall. (Coming from a preview or
  test build? Install 1.5.4 once by hand.)
- On 2018–2020 sets the TV blocks native code loaded from the app folder, so
  the app loads it straight into memory instead. A normal (Public) certificate
  is enough; no Partner certificate needed.
- Settings › Advanced › **Visual effects**: *Automatic* measures the TV in the
  first seconds and turns off the heaviest effects if it can't keep up; you can
  force *Full* or *Light*.
- Samsung TVs don't support Dolby Vision; the player labels the source's HDR
  type instead (HDR10, HDR10+, HDR).

#### WebAssembly `.wgt`

`NuvioTV-X.Y.Z-tizen.wgt`, for Tizen **5.5 or newer**. The same app compiled to
WebAssembly, running in the TV's web engine. Samsung doesn't let a web app
install itself, so here the update card only tells you there's a new version.
Tizen 4 doesn't run the `.wgt` (#96) — use the native `NuvioTpk40` instead.

Follow the Samsung work in the
**[Samsung native project board](https://github.com/users/iqui27/projects/2)**
and [issue #137](https://github.com/iqui27/nuvio-native-legacy/issues/137).

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
| LG webOS 3.x | **Works**, reported by testers (webOS 3.4.3). Same package as everyone else. I don't have one. |
| LG webOS 2.x | Loads according to firmware symbol dumps. Never run. |
| Samsung Tizen 8/9 (native .tpk) | **Works**, confirmed on three 2023–2024 sets. |
| Samsung Tizen 6.5/7 (native .tpk) | Should work (same code as 6.0 and 8/9); few reports yet. |
| Samsung Tizen 6.0 (native .tpk) | **Works**, confirmed. |
| Samsung Tizen 4.0/5.0 (native .tpk) | **Works**, confirmed on two 4.0 and two 5.0 sets (2018–2019). |
| Samsung Tizen 5.5+ (WebAssembly) | **Works**, many users. |
| Samsung Tizen 4 (WebAssembly) | No (#96) — use the native `.tpk`. |
| Hisense VIDAA | Experimental, untested. |

On webOS 3, keep in mind that many of those sets can't decode H.265 or HDR, and
have little free RAM (around 300 MB), so pick H.264 sources when you can.

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

## Support development

<a href="https://www.patreon.com/cw/CraaazyDevs">
  <picture>
    <source media="(max-width: 640px)" srcset="docs/assets/patreon-support-banner-mobile.png">
    <img src="docs/assets/patreon-support-banner.png" alt="Support Nuvio Native Legacy by CraaazyDevs on Patreon. Built with care. Supported by the community. Support is optional." width="1600">
  </picture>
</a>

This project grew out of wanting a smooth, beautiful Nuvio experience on my LG C9. It now reaches more TVs, with help from people testing builds and sharing feedback.

If you’d like to support the time and care behind it, visit [CraaazyDevs on Patreon](https://www.patreon.com/cw/CraaazyDevs). Support is entirely optional. Bug reports, testing, and helping other users matter too.

This is an independent, unofficial project, not affiliated with NuvioMedia. Credit for Nuvio goes to its original creators.

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
bash tools/linux.sh            # build and run on Linux (UI only, no video)
bash tools/linux.sh --build    # compile the Linux desktop preview only
bash tools/linux.sh --preview  # browse the Linux UI without signing in
bash tools/arm.sh              # cross-compile for LG in Docker and deploy over ssh
bash tools/arm.sh --ipk        # also produce the .ipk
bash tools/tizen.sh            # build the Samsung target (WebAssembly)
bash tools/tizen-wgt.sh        # package it as an unsigned .wgt
bash tools/tpk.sh              # native Samsung: the four .tpk (Docker + .NET 8)
bash tools/release-samsung.sh  # every Samsung package for a release, checked
bash tools/hb-repo.sh <ipk> <dir>   # Homebrew Channel repo files for a release
```

On an x86_64 Linux host with Podman, build the SDK with its x86_64 toolchain
and select the runtime and packaging CLI explicitly:

```bash
podman build --platform linux/amd64 \
  --build-arg SDK_URL=https://github.com/openlgtv/buildroot-nc4/releases/download/webos-a38c582/arm-webos-linux-gnueabi_sdk-buildroot-x86_64.tar.gz \
  -t nuvio-webos-sdk tools/
NUVIO_CONTAINER_RUNTIME=podman NUVIO_BUILD_PLATFORM=linux/amd64 \
  NUVIO_ARES_PACKAGE=/path/to/ares-package bash tools/arm.sh --ipk --build
```

`--ipk --build` creates the package without deploying to a TV. The target
binary is ARMv7 regardless of the build host architecture.

webOS builds include DTS audio fallback by default. TVs that accept DTS keep
native playback; when the selected DTS track is unsupported, the app converts
its audio to stereo AAC and keeps video on the TV's hardware pipeline. See the
[DTS guide](docs/features/dts/README.md) for limits and verification, and
[DTS Debug build commands](tools/dts-pipeline/DEBUG.md) for a separate diagnostic
app that can be installed alongside the normal one.

For Linux UI previews, install the SDL2, SDL2_image, SDL2_ttf, GLES2, EGL and
zlib development packages, a C compiler and pkg-config. `--preview` skips login
and uses a separate data directory. Arrow keys and Enter navigate; Escape or
Backspace goes back. Video playback is unavailable in this desktop preview.

Builds read `local.properties` from the repository root or the legacy neighboring
web project; `NUVIO_PROPERTIES` selects another file.

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
  you it works. That's how webOS 3 support was checked: exactly one missing
  symbol (`SDL_CreateRGBSurfaceWithFormat`, SDL 2.0.5), replaced in
  `src/sdlcompat.h`, so the same package runs on webOS 3 and up.

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

Settings icons: [Lucide](https://lucide.dev), ISC license. The unmodified SVGs
and the license text are in `deploy/app/art/icones/lucide/`; the `aj_*.png` next
to them are rasterized by `tools/icones-lucide.sh`.

DTS-enabled builds use libraries from the [FFmpeg project](https://ffmpeg.org/)
under **LGPL 2.1 or later**. `tools/build-dts-ffmpeg.sh` records the exact release
source URL and checksum; `tools/arm.sh` includes its license and provenance in
`licenses/dts/` inside the app package. The
[DTS distribution notes](docs/features/dts/README.md#dependency-provenance-and-distribution)
describe the corresponding source and static relinking materials needed with
binary releases.

"Nuvio", the logo and the wordmark belong to the original authors. The GPL covers
the code, **not** the name or the branding. They appear here only to say which
project this one comes from.

Artwork fetched at runtime (TMDB, Trakt, Apple TV, addon CDNs) belongs to its
owners and isn't covered by this license.
