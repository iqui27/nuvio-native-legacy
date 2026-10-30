# Desktop polish verification

Desktop-only changes are guarded by `NV_DESKTOP_UI`; Cocoa lives in
`src/desktop_mac.m` and is linked only by the Mac release packager.

## Behavior

- Desktop uses system arrow/hand cursors. Hover updates component focus and
  title previews while preserving page and row scroll; keyboard focus scrolls.
  Cards grow by 2.5% without a contour. Keyboard focus uses a thin accent ring.
  Desktop right-click opens the custom glass title popover; Escape goes back.
  Search keeps Backspace for deletion.
- Sidebar collapse uses an animated width and atomic `desktop-sidebar.txt`
  persistence. Button and Cmd/Ctrl+B toggle it. Collapse hides the sidebar
  entirely; the traffic lights and toggle remain. Left-edge hover or a swipe
  reveals a spring-animated overlay without changing the saved pin state.
  Search, Home, Library and TV Guide lead the menu;
  secondary destinations are grouped without a duplicate Library caption.
  Mac rows use an 18 pt icon in a fixed 24 pt column, 15 pt labels and even
  35 pt heights. Browse, You and Services headers precede their groups.
  Streaming services expand beneath their
  parent and use a bounded native scroll viewport on Mac.
- macOS uses a transparent unified title bar, NSGlassEffectView on macOS 26+
  with NSVisualEffectView Sidebar/BehindWindow fallback on older systems,
  system symbols and AppKit buttons. The GL sidebar is cleared to alpha zero.
  A scene copy under the glass preserves artwork beneath the alpha hole.
  It updates at most 10 Hz through a pixel pack buffer; an Apple GL fence
  prevents mapping unfinished GPU work. A GPU framebuffer blit flips the
  image, replacing the per-frame synchronous read and CPU scanline reversal.
  Native symbols share one blue tint. Pending keyboard/gamepad selection
  has an accent ring, distinct from the active destination. Destination and
  group labels refresh when the interface language changes.
  The titled window clips its content layer to rounded corners. Native labels
  avoid the straight-alpha GL text artifacts seen during the
  first transparent-window capture. The sidebar follows system appearance.
- Windows requests DWM system backdrop when available and uses the existing
  GL glass sidebar surface. Native Windows material was not visually verified.
- Native Cocoa precise scroll events retain pixel deltas before SDL normalizes
  them into wheel ticks. OS momentum is passed through; no app inertia is added
  to tagged Mac trackpad events. Notched wheels use app inertia. Left/right
  margin horizontal gestures open navigation/Saved, respect the system swipe
  preference, and do not capture gestures starting inside horizontal rows.
- Desktop person pages use the actual Filmography renderer and input handler.
  Filmography grid hit targets follow the visible free-scroll range and remain
  clipped beneath its filters. The offline proof opens the production person
  route with isolated person JSON, then scrolls beyond the initial viewport,
  hovers a card and opens its options.
- Home, Search, Library, Detail, collection Timeline and shared touch scroll
  consumers now accept continuous desktop input. Shared bounds support desktop
  rubber-band displacement and spring return. Existing phone/TV mode decisions
  remain separate from desktop geometry.
- Search uses SDL_TEXTINPUT, including composed Unicode, from any result focus,
  including desktop windows narrower than 1000 points.
  The title-bar field displays the query. No desktop Search keyboard or phone QR
  is drawn. Cmd/Ctrl+F opens Search; ordinary `f` no longer toggles fullscreen.
- Window-point layout keeps at least 1200 logical vertical units in shorter
  desktop windows; Retina drawable dimensions still drive rendering and text
  rasterization. Home hero fills the window height, extends beneath the sidebar,
  and scrolls away with its title and metadata. Library reserves the
  toolbar and separates tabs, source filters and grid. Detail information moves
  upward and reserves its source line. Its section layout invalidates on canvas
  resize so seasons follow metadata at both proof sizes; synopsis height never
  reserves less than the current text during a metadata change. Home metadata
  and GL sidebar labels use a 15-point minimum at the default desktop scale. Timeline restores clipping after card
  helpers and scales its sidebar offset correctly. Guide avoids traffic lights.
  Shared remote-control hint bars are suppressed on desktop.

Desktop Sports selects its own sidebar destination and uses compact Settings,
Leagues and Update icon actions in the page header. Child Detail/Filmography
overlays retain their parent destination. Library’s source label is a compact
pop-up that opens the existing animated source panel. Continue Watching adds
a title chevron (focuses the first resumable title) and hover ellipsis wired to
the desktop title popover. This compact cursor-anchored component preserves
existing TV actions and adds Play/Continue, Title, Trailer and Copy (IMDb link);
arrow keys, Enter, Escape, hover and outside click work inside window bounds.
The native account footer uses the active profile name,
with its avatar or initials. Proof data uses an isolated fictional profile.

## Reproduction

Capture safety: `tools/desktop-polish-capture.py` launches with `NV_CAPTURA=1`
and `SDL_MAC_BACKGROUND_APP=1`. SDL disables activation before initialization;
AppKit uses Accessory policy and `orderBack`, never a key window or Dock icon.
Capture disables swap-interval waiting: covered windows can receive no display
refresh signal, which otherwise stalls the first SDL swap. Normal desktop
windows keep vsync; the desktop frame limiter still caps capture rendering.
Capture windows start hidden before AppKit orders them behind other windows.
The helper never changes system appearance, focus, or cursor position. A private
`NV_CAPTURA_DIR` isolates commands and screenshots from any owner app using the
legacy `/tmp/nuvio-*` protocol. All
pointer, wheel and keyboard input enters through `SDL_PushEvent`. Page proofs
use in-process GL readback with the shared GL sidebar (`NV_CAPTURA_GL=1`).
Only the two native material/corner proofs use `screencapture -l ID -o -x` on
the covered window. Light/dark appearance is set on that window alone.
The GL sidebar transition loop records three opening and closing frames over
20 cycles. Native backdrop sampling flips rows with a GPU blit before publishing
a completed pixel pack buffer; all AppKit layer updates disable implicit animations. Every
launched process is terminated and reaped, including failure paths. Capture
commands and readback allow 90 seconds under shared-machine load. Do not use
system input synthesis or activate a capture window while the owner works.

From the worktree root:

```sh
bash tests/desktop-polish.sh
bash tests/desktop-build.sh
bash tests/desktop-packages.sh
python3 tools/desktop-polish-capture.py
python3 tools/desktop-polish-capture.py --native-only --reuse-transitions
# Focused refresh; start a fresh process per size for retained Filmography state.
python3 tools/desktop-polish-capture.py --affected-only --size 1280 --reuse-transitions
python3 tools/desktop-polish-capture.py --affected-only --size 1920 --gl-only --reuse-transitions
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
preference, mouse hover/click separation, bounce return, wide/narrow Unicode Search,
Filmography free scroll with hover and activation beyond the first viewport,
fullscreen translation and Mac/Windows/shared-TV syntax.

## Evidence

`/Volumes/ExternalSSD/tmp/rev/desktop/polish/` contains every requested screen at
1280x800 and 1920x1080: Home, expanded/collapsed sidebar, Streaming, Search typing,
Detail, Library, Lists, Filmography/Timeline before and after scroll, Settings,
Guide, Sports, Achievements and Player overlay. Native Aqua/DarkAqua window
captures test the real system material, traffic lights, and transparent pixels
at all four corners. Additional captures cover hero scroll at 0/50/100%,
cursor popovers, Detail scrolling, and the hidden sidebar with edge reveal. `compare-*.png`
places Home/sidebar/Streaming beside the supplied mocks. Native Mac proportions
and chrome follow the Apple TV reference where it conflicts with those mocks.

`frame-times.log` and `proof.json` record measured synthetic native-pixel
scroll input through the production pointer route, including frame intervals,
median work, p95 and maximum work. Consult the regenerated JSON for this build. This is not physical trackpad evidence
or a guarantee that every populated screen sustains 120 Hz.

The fixture uses an isolated data directory and local HTTP catalog. No fixture
catalog or credentials are added to the packages. Guide/Library/Lists and
Achievements include empty-account states. Player uses the Sintel trailer with
Fallout metadata, so the production duration-mismatch notice is expected in the
capture. Windows playback was exercised under Wine, not native Windows.

The covered Aqua and DarkAqua captures show distinct native materials and
artwork through the panel, with alpha zero at all four window corners.
Capture-only appearance names map to AppKit's named constants.

## Remaining limitations

- Final native light/dark recapture was rejected by `screencapture` with
  "could not create image from window" after covered-window rendering resumed.
  The retained native PNGs were refreshed earlier in this correction cycle;
  final GL screen proofs and both packages include the capture-vsync fix.
  Those retained native PNGs must not be treated as a fresh native runtime
  check of the final package.

- Transition frames use GL readback. Native frame sequences contain the alpha
  hole, not composited AppKit controls; they do not independently prove every
  intermediate glass frame is free of flicker.

- Physical trackpad feel, swipe preference changes on real hardware, native
  Windows Mica/acrylic appearance and TV/iOS device runtime were not exercised.
- Native Search remains a GL field backed by SDL_TEXTINPUT; native NSTextField
  editing, selection and clipboard shortcuts are not implemented.
- Full mouse audit of every nested modal and every populated account state is
  not covered by these screen captures.

Nothing was pushed, deployed, installed on devices or published.
