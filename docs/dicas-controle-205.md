# Remote tips, 2.0.5

Four pages appear after dismissing the current What's New card, or the 2.0 guide on a fresh installation. The outgoing card gets 360 ms to fade. OK, Back or Done finishes and records `dicas-controle-205.txt` in the app data directory. Replays of What's New respect that marker; Settings > About and help > Remote tips always reopens the pages. A future content revision should use a new marker, independent of routine app updates.

The modal owns keys, pointer targets and touch scrolling, including its closing fade. TV pages use Left/Right; touch uses page dots and horizontal swipes. Reduced animations disable movement and the key pulse. The background uses the shared snapshot, then a private 96-pixel-wide miniature cached per opening. The extra snapshot step allows Settings help previews to draw their own miniatures without nesting `gfx_mini` targets. No assets or dependencies are added.

Verified input sources:

- Android TV and native Tizen: `main.c:remapCanal`, `NuvioActivity.kt` and `tpkteclas.c` map CH+ to the Saved/Alerts panel and CH- to Spotlight outside live TV.
- webOS: `app.c` handles CH+ for the panel. Spotlight accepts Yellow or holding Search in the menu; CH- is not mapped to Spotlight in this build.
- Tizen WGT and native Tizen 4–5 (`NV_TPK40`): `tools/tizen-shell.html` and `main.c` deliver CH+ and Yellow. The shell does not register CH-, so the tips teach Yellow.
- Remote Control Center: `main.c:teclaCh`, `central.c` and `chsegura.h` handle held CH+ outside live TV.
- Poster menu: `home.c`, `ctxmenu.c` and `ponteiro.c` handle held OK and touch-and-hold.
- Touch: `ilha.c:pontRelogio` opens Saved/Alerts, `menu.c` handles held Search, and `app.c:sidebarToqueRegistrar` opens navigation from the left edge. There is no existing long-clock gesture for Control Center, so the touch pages do not claim one.

Checks from the worktree root:

```sh
bash tests/dicascontrole.sh shot /Volumes/ExternalSSD/tmp/rev/dicascontrole
bash tests/novidades_cartao.sh
bash tests/i18n.sh
python3 tools/varredura-i18n.py
```

The tips harness compiles the app modules and syntax-checks `main.c`, exercises first/repeat in separate processes, uses the actual Settings action and app input router, measures all four pages in all 30 languages, and captures Portuguese/German/Japanese in four device modes plus reduced motion in touch landscape. It uses the existing hidden SDL/GL FBO screenshot pattern. Physical devices and deployment are outside this job.
