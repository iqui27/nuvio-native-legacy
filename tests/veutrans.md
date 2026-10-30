# Home-to-Detail veil continuity

The Dynamic carousel used `GFX_JANELA` with the older page vignette and the current background colour. Landscape Detail switched to a different curve, forced a black scrim, and bypassed the configured Frost/ambient background. The texture was the same; its composition changed at the renderer boundary.

The fix shares the page-scrim function between both shaders and routes settled Detail through the existing background-mode handling. Both paths honour the same vignette strength. Missing artwork keeps an accent-tinted waiting layer, then uses the existing 220 ms reveal to blend into the actual backdrop and scrim. Spring timings are unchanged.

`bash tests/veutrans.sh` renders actual Home selection and Detail backgrounds at 60 Hz, using local artwork. It captures 100 consecutive frames for each of the three layout enum values, with Glass off, Frost, and Glass. Dynamic crosses its real renderer boundary between frames 55 and 56. Modern and Standard/lateral use the same Detail renderer throughout growth. The historical Apple TV label refers to Dynamic; the repository now exposes Modern (0), Standard (1), and Dynamic (2), all covered.

The capture isolates the backdrop and veil from labels and focus animations. It also renders adjacent states immediately around the Dynamic boundary, then injects missing texture readiness and captures every frame of delayed artwork arrival. The isolated capture flushes deferred Frost/ambient drawing as production content does. Pixel checks use ImageMagick and Python's standard library.

Evidence is stored outside Git at `/Volumes/ExternalSSD/tmp/rev/veutrans/`:

- `before/` and `after/`: all consecutive PNG frames, CSV spring states, endpoint BMPs, and capture logs.
- `red.log`: original-code regression failures. Dynamic endpoint mean RGB difference: 27.459/255 with Glass off/Glass; 24.611/255 with Frost.
- `after.log`: fixed hand-off checks. Endpoint means: 0.103/255 with Glass off/Glass; 0.465/255 with Frost. Every-frame sample step: at most 1/255.
- `veil-handoff.jpg`: six adjacent frames per row, before/after for each layout, width 2196 px.
- `late-final.log` and `build-final.log`: delayed-art checks and host/Android build output.

Commands, run from the worktree root:

```sh
bash tests/veutrans.sh
bash tests/veutrans_late.sh
bash tests/veutrans_check.sh
bash tests/veutrans_montage.sh
bash tests/veutrans_build.sh
```

The build wrapper runs `tools/build-local.sh`, removes Android compressed-assets intermediates, then runs the requested Android command with the external cache, properties file, temporary directory, and 1800-second timeout. It never installs or deploys. Captures and shader execution are from desktop OpenGL; no physical-TV validation was performed.
