# Nuvio on iPhone Duo (iOS 27.2) and Mac: mockups

`gen.py` builds one HTML page per flow in `out/` and renders JPGs (<= 2000 px)
to `/Volumes/ExternalSSD/tmp/rev/dono-envio/ios-*.jpg` with Playwright (system
Chrome). Fonts are the embedded Inter Display; art, posters and Lucide icons
come from `deploy/app/art`.

    /Library/Frameworks/Python.framework/Versions/3.13/bin/python3 design/ios-duo/gen.py

Pages: Home, Search, Detail, Sources, Player (closed + book, tabletop + flat),
TV Guide, Library, Sign-in, Profiles, Settings, Mac. Each page shows the outer
display (466x678 pt, compact width, vertical bar) next to the inner display
(951x669 pt, regular width; fold at 475.5) in the pose the flow needs.
Device numbers are mockup constants only; code reads size class, safe area and
ReservedRegion at runtime. Plan: `docs/reviews/ios/PLANO.md`.
