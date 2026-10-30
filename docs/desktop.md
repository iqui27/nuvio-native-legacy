# Nuvio 2.1 desktop packages

From the repository root on macOS:

```sh
brew install sevenzip mingw-w64 mpv
NUVIO_PROPERTIES=/absolute/path/to/local.properties bash tools/mac-pacote.sh
NUVIO_PROPERTIES=/absolute/path/to/local.properties bash tools/windows.sh
```

Both scripts require the same core server settings as TV releases through
`tools/env.sh`. Nothing reads account credentials from the developer profile.
`local.properties` must stay outside Git. Temporary macro headers are private,
removed on exit, and never included in either archive. Desktop version is 2.1;
TV version metadata and TV build scripts are unchanged.

Outputs default to `/Volumes/ExternalSSD/tmp/rev/desktop/`: `Nuvio.app`,
`Nuvio-mac-arm64.dmg` (or `Nuvio-mac-universal.dmg` when all dependencies have both slices),
`Nuvio-win64.zip`, and `windows-dlls.txt`.
Override `NUVIO_DESKTOP_OUT` and optionally `NUVIO_DESKTOP_CACHE` to change those
locations. SDL2, SDL_image and SDL_ttf binaries come from official libsdl.org
releases, verified against pinned SHA-256 digests before extraction. macOS uses
universal frameworks with relative `@rpath` references. Windows includes the
upstream curl.se libcurl DLL and zlib, also checksum verified, plus MinGW's
`libwinpthread-1.dll` required by GCC's compiler TLS runtime. Application threads
use SDL. All runtime libraries accompany the package. Third-party licenses accompany the packages.
The distribution art is staged through the same allowlist as the TV packages;
personal catalog, cache, account and settings files are excluded.

The Mac application's architectures and minimum macOS version follow the complete
Homebrew libmpv dependency closure, recorded in `mac-mpv-deps.json`. A normal
Apple silicon Homebrew installation produces an arm64 application. The package
script never advertises an Intel slice without matching playback libraries.
It is ad-hoc signed, not notarized: Gatekeeper may require Finder's Open command.
Drag the application from the disk image to Applications. On Windows x64, extract
the entire ZIP before opening `Nuvio.exe`; keep DLLs, fonts and art alongside it.
Windows requires an OpenGL 2.1 driver. No installer or administrator rights are
needed.

Data defaults to `~/Library/Application Support/Nuvio/` on Mac and
`%APPDATA%\Nuvio\` on Windows. `NUVIO_DADOS` explicitly overrides it, including
for isolated smoke tests; packages never use the developer's `~/.nuvio` by default.

Keyboard: arrows navigate, Enter selects, Escape/Backspace go Back. Backspace
still edits text when the system text input is open. F toggles fullscreen;
Cmd+Ctrl+F also works on Mac while typing. Mouse uses the existing pointer/wheel
handling. SDL-compatible gamepads use D-pad, A to select and B/Back to go Back;
controllers can be connected or disconnected while running.

## Desktop playback and layout

Desktop releases use libmpv's OpenGL render API in the SDL context. Video fills
the existing alpha holes beneath player controls and trailer overlays. Seeking,
audio track changes, embedded subtitles and external subtitle files use the
existing player interface. HDR sources may be tone-mapped to SDR.

macOS packages include Homebrew libmpv and its recursive non-system dylib closure,
relinked to paths inside `Contents/Frameworks` and signed before the application.
Windows packages include the checksum-verified development build from
`shinchiro/mpv-winbuild-cmake` and its `mpv-2.dll`. The TV, iOS and Android playback
paths retain their platform implementations; local UI-only builds retain their
simulation backend.

The default window is 1280×800 and resizable. Wide windows use a persistent
sidebar and toolbar search; narrow windows use the existing touch portrait
layout. Keyboard and SDL gamepad navigation remain available alongside mouse
hover, click and wheel input. No control-hint bar is shown.

Windows omits the TV phone-entry and remote log HTTP servers and media relays.
System libjpeg/libwebp acceleration is optional; SDL_image provides fallback
image loading. P2P native engines are not bundled.

For deterministic screenshots, `NUVIO_UI_PREVIEW=1` opens the existing offline
home preview without account synchronization. `python3 tools/desktop-capture.py`
creates isolated fixture data outside the application, captures wide/narrow
windows and compares the wide window with the approved mock. This mode is
opt-in and does not include a catalog or account data in release packages.

For a deliberately unconfigured diagnostic build only, set
`NUVIO_DESKTOP_ENV_MODE=--allow-unconfigured`. Its login screen reports missing
server configuration. This mode is not a configured release and cannot verify
account login. Rebuild with the owner's properties before distribution.
