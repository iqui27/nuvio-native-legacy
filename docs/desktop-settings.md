# Desktop settings visibility

Desktop builds (`NV_DESKTOP_UI`) hide TV-only rows through `ajOpcaoDisponivel`
(`src/ajustes_desktop.inc`). Layouts and stored values are untouched; TV builds
never compile the list. `tests/desktop_settings.c` checks the table.

| Setting | Desktop | Reason |
|---|---|---|
| Interface mode (TV/Mobile) | hidden | window width picks the layout |
| Keyboard (on-screen keyboard choice) | hidden | desktop types on the physical keyboard |
| Interface resolution | hidden | window/Retina size decides |
| Enlarge interface, upscaling algorithm, sharpness | hidden | TV panel upscaling |
| Fast horizontal navigation | hidden | D-pad auto-repeat |
| Remote tips | hidden | no remote |
| Screensaver, screensaver style, screensaver font | hidden, forced off | the computer has its own screen saver |
| DTS converted to | hidden | LG-only conversion; mpv decodes DTS |
| Dolby Vision in MKV, DV profile 7 conversion | hidden | LG/TV conversion paths |
| Disk seek cache | hidden | TV player cache; mpv has its own |
| Buffer size | hidden | webOS read-ahead; mpv keeps its own cache |
| Top menu, sidebar fixed/collapsed, modern sidebar, sidebar blur | hidden | desktop uses the native sidebar |
| App icon | hidden | launcher icon swap exists on Android/webOS only |
| Microphone button, tunneling, AFR, HDR10+ strip, Samsung trailer zoom, audio-sync by PCM, Android TV home | already hidden off-platform | compile-time platform gates |
| Playback, languages/subtitles, sources/add-ons, Live TV, Home content, appearance (colour, glass, font, animations, clock), title page, posters/trailers, account, visual effects, image quality, memory, diagnostics, speed test, about/help, parental | kept | apply on a computer |

Wording: "This TV" reads "This device"; "Limits of this TV", "Not available on
this TV", "Saved on this TV" and "Press OK to connect" map to existing neutral keys
(`src/idioma.c`).

## Desktop density and shortcuts

Settings draw at `NV_DESKTOP_AJUSTES_DENSIDADE` (src/layout.h) while
`layout_desktop_ativo()` (window at or above `NV_DESKTOP_BREAKPOINT`): the
settings canvas grows by 1/0.78, so rows and text land near Mac sizes (~13 pt
text, ~30 pt rows) instead of 10-foot sizes. The same gate widens the category
column (470) and the inspector (560), lets "What changes" use up to 7 lines and
hides descriptions that name a TV platform or the remote (`ajDesktopTextoTv`).
Below the breakpoint every value is the TV one. Cmd+, opens Settings through
the guarded handler in `app_evento` (ignored with the player or keyboard open;
closes the title page first). `tests/desktop_settings_r13.sh` covers it.
