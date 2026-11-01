# Release 2.1 Library shot gate

All 11 original failures were stale test choreography or its downstream effects. No app regression was reproduced; no application or layout code changed. All original assertions remain enabled.

Provenance: `dcb1da63b22002018a8a0d3c798bf1f7c85167a6` introduced the 6B list page with wrapped actions and Continuar; `bdcbae393` merged the lists UX with review approval and owner approval on PNGs dated October 28. `d34e0a548690e44cff87e535ba16146b76622e70` changed Library arrows to follow rendered control geometry. The old test still assumed one horizontal action row. The corrected test uses explicit arrows through the existing wrapped controls and renders one frame after leaving Notes so hit targets reflect the shorter inactive label.

| Original failing assertion | Classification and correction |
| --- | --- |
| D-pad reaches Order | Stale: Pin → Home → Order requires two Right presses in the rendered layout. |
| Order activates and focuses items | Downstream: the old route selected the wrong target; correct Order route restores this assertion. |
| OK grabs title 0 | Downstream: Order had never activated. |
| Arrow moves grabbed title to 1 | Downstream: no title had been grabbed. |
| Title 0 appears at position 1 | Downstream: no reorder had occurred. |
| Saving without an account reports failure | Downstream: unchanged order caused no save attempt. The restored reorder exercises the original failure message assertion. |
| D-pad reaches Share | Stale: return from title 1 with Up twice, then Down to Trail and Left to Share. Repeated Up would continue through Continuar. |
| Public list opens QR | Downstream: the old route never selected Share. |
| Notes activates | Stale: from Share, Up reaches Movies; Right three times reaches Notes. |
| OK on title opens note keyboard | Downstream: Notes had never activated. |
| Description opens keyboard | Stale: render after exiting Notes, then Up from the first item and Left twice from Notes to Description. |

Link failures: both isolated discovery tests now link the real `src/metaep.c`. `cateps.sh` also links the existing `tests/discovery_boundary_stub.c` for unrelated rewatch and hunt boundaries, matching `posterprov.sh`. No replacement metadata parser or new stubs were added.

Final validation, with `TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp` exported:

- `bash tests/biblioteca_shot.sh /Volumes/ExternalSSD/tmp/jobtmp/bibshot-final`: rc 0, all original list-action assertions pass, cold-cache second run passes.
- `bash tests/posterprov.sh`: rc 0.
- `bash tests/cateps.sh`: rc 0.
- `bash tools/build-local.sh`: rc 0; existing atomic initializer deprecation warning.
- `git diff --check`: rc 0.

The shot harness produced BMP files. A diagnostic BMP converted to PNG was black in this GL environment; these captures are not visual proof. No PNG is submitted. Visual/layout verification is outside this test-only change. No push, deployment, installation, or stash performed.
