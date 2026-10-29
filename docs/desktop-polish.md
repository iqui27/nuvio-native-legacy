# Desktop polish verification

Desktop-only changes are guarded by `NV_DESKTOP_UI`; Cocoa lives in
`src/desktop_mac.m` and is linked only by the Mac release packager.

## Behavior

- Desktop uses system arrow/hand cursors. Hover highlights targets without
  invoking their keyboard-focus callbacks; clicks retain production activation.
  Right-click and Escape retain Back. Search keeps Backspace for deletion.
- Sidebar collapse uses an animated width and atomic `desktop-sidebar.txt`
  persistence. Button and Cmd/Ctrl+B toggle it. Collapsed width leaves space for
  the Mac traffic lights. Search, Home, Library and TV Guide lead the menu;
  secondary destinations are grouped. Streaming services expand beneath their
  parent and use a bounded native scroll viewport on Mac.
- macOS uses a transparent unified title bar, real Sidebar NSVisualEffectView,
  system symbols and AppKit buttons. The GL sidebar is cleared to alpha zero.
  Native labels avoid the straight-alpha GL text artifacts seen during the
  first transparent-window capture. The sidebar follows system appearance.
- Windows requests DWM system backdrop when available and retains the solid
  GL sidebar fallback. Native Windows material was not visually verified.
- Native Cocoa precise scroll events retain pixel deltas before SDL normalizes
  them into wheel ticks. OS momentum is passed through; no app inertia is added
  to tagged Mac trackpad events. Notched wheels use app inertia. Left/right
  margin horizontal gestures open navigation/Saved, respect the system swipe
  preference, and do not capture gestures starting inside horizontal rows.
- Home, Search, Library, Detail, collection Timeline and shared touch scroll
  consumers now accept continuous desktop input. Shared bounds support desktop
  rubber-band displacement and spring return. Existing phone/TV mode decisions
  remain separate from desktop geometry.
- Search uses SDL_TEXTINPUT, including composed Unicode, from any result focus.
  The title-bar field displays the query. No desktop Search keyboard or phone QR
  is drawn. Cmd/Ctrl+F opens Search; ordinary `f` no longer toggles fullscreen.
- Window-point layout keeps at least 1200 logical vertical units in shorter
  desktop windows; Retina drawable dimensions still drive rendering and text
  rasterization. Home hero height follows window height. Library reserves the
  toolbar and separates tabs, source filters and grid. Detail information moves
  upward and reserves its source line. Timeline restores clipping after card
  helpers and scales its sidebar offset correctly. Guide avoids traffic lights.
  Shared remote-control hint bars are suppressed on desktop.

## Reproduction

From the worktree root:

```sh
bash tests/desktop-polish.sh
bash tests/desktop-build.sh
bash tests/desktop-packages.sh
python3 tools/desktop-polish-capture.py
python3 tools/desktop-polish-proof.py
python3 tools/desktop-proof.py
python3 tools/desktop-proof.py --mount-only
python3 tools/desktop-wine.py
```

The full build gate passed local Mac and Android arm64/armv7; subsequent
platform-specific visual fixes were packaged by the desktop-only build.
The packages are `/Volumes/ExternalSSD/tmp/rev/desktop/Nuvio-mac-arm64.dmg`
and `Nuvio-win64.zip`. Mac minimum OS remains 27.0, inherited from the bundled
Homebrew mpv dependency closure. Packages are ad-hoc signed, not notarized.

`desktop-polish.sh` covers collapse reload, menu routes, streaming ordering,
fractional wheel input, no Mac double momentum, both edges, swipe-disabled
preference, mouse hover/click separation, bounce return, Unicode Search,
fullscreen translation and Mac/Windows/shared-TV syntax.

## Evidence

`/Volumes/ExternalSSD/tmp/rev/desktop/polish/` contains every requested screen at
1280x800 and 1920x1080: Home, expanded/collapsed sidebar, Streaming, Search typing,
Detail, Library, Lists, Filmography/Timeline before and after scroll, Settings,
Guide, Sports, Achievements and Player overlay. Native Aqua/DarkAqua window
captures show the real system material and traffic lights. `compare-*.png`
places Home/sidebar/Streaming beside the supplied mocks. Native Mac proportions
and chrome follow the Apple TV reference where it conflicts with those mocks.

`frame-times.log` and `proof.json` record 159 frames during synthetic native-pixel
scroll input through the production pointer route: frame interval median 8 ms,
work median 8 ms, p95 9 ms, maximum 12 ms. This is not physical trackpad evidence
or a guarantee that every populated screen sustains 120 Hz.

The fixture uses an isolated data directory and local HTTP catalog. No fixture
catalog or credentials are added to the packages. Guide/Library/Lists and
Achievements include empty-account states. Player uses the Sintel trailer with
Fallout metadata, so the production duration-mismatch notice is expected in the
capture. Windows playback was exercised under Wine, not native Windows.

## Remaining limitations

- Physical trackpad feel, swipe preference changes on real hardware, native
  Windows Mica/acrylic appearance and TV/iOS device runtime were not exercised.
- Continue Watching keeps the existing Nuvio card metadata/controls. Dedicated
  Apple TV-style hover ellipsis and row-title chevrons were not added.
- Profile uses the existing account avatar rather than a new initials renderer.
- Native Search remains a GL field backed by SDL_TEXTINPUT; native NSTextField
  editing, selection and clipboard shortcuts are not implemented.
- Full mouse audit of every nested modal and every populated account state is
  not covered by these screen captures.

Nothing was pushed, deployed, installed on devices or published.
