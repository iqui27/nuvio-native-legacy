# Navigation P1 follow-up

## AIO paging, Detail server chips, and shared components

The expanded gate fails with production fixes removed (rc 1, 3,359 checks, 65 violations) and passes with the fixes restored (rc 0, 3,359 checks, zero violations). Both runs use identical fixtures.

| Regression | Before | After | Exact path |
|---|---:|---:|---|
| AIO results, 12 items | 28 | 0 | Title, Down through all 12 results, Up through all results; focused row must render |
| AIO evidence, 9 items | 25 | 0 | Evidence, Down through all 9 rows, Up through all rows; focused row must render |
| AIO catalogs, 6 items | 7 | 0 | Catalog 1, Down through catalog 6 without a footer detour, Up to catalog 1 |
| Detail server chips | 3 | 0 | Series with seasons and Jellyfin availability: Play, Down to Jellyfin, Up to Play; chip rectangle registered |
| Rating island / Detail | 1 | 0 | Open, Back: rendered transition lasts at least 150 ms |
| Player OSD | 1 | 0 | Open, Back: rendered transition lasts at least 150 ms |
| Sources from clock island | 0 | 0 | Real clock opening with `folhaRelogio=1`, 12 controls, geometry/reachability, open/Back motion, source-detail Back retains sheet/focus |
| Clock island modal | 0 | 0 | Three rendered buttons, geometry/reachability, open/Back motion; Back retains clock and emits no route request |

AIO reveals hidden adjacent rows before geometry can select a footer, preserving visible-target navigation. Scroll-window offsets within 0.001 of an integer are rounded because a settled spring can otherwise leave the focused last row outside the rendered window. Detail includes server chips in its rectangle registry and dispatches their focus through the existing callback. OSD Back retains its existing fade; Detail ratings keep drawing while the existing spring closes.

Dedicated fixtures compile the real clock-island, rating and player implementations. OSD transport buttons use geometry/reachability checks; seek-bar Up/Down behavior has explicit assertions because Left/Right seek rather than move focus. Rating tests also check star-row navigation and dismissal without persisting a rating. Sources renders through `ilha_desenhar` and checks `ilha_corpo_t`, rather than forcing the clock path off.

Final validation: `bash tests/nav21.sh`, `bash tools/build-local.sh`, `python3 tools/varredura-i18n.py`, and `git diff --check`, all rc 0. Build has only the existing `ATOMIC_VAR_INIT` warning; i18n has zero findings. Gate remains included in `tools/testa-tudo.sh`.

Evidence: `/Volumes/ExternalSSD/tmp/rev/navteste/p1-shared-before.log`, `p1-shared-before.md`, `p1-shared-final.log`, `p1-shared-build.log`, `p1-shared-i18n.log`, and `violations.md`. No push, deployment, or TV validation. Exhaustive acceptance of every original screen/subpage variant remains unestablished.

## Profile return and picker pagination follow-up

Both remaining P1s are fixed. `bash tests/nav21.sh` now passes **3,128 checks, zero violations**. The expanded baseline failed with 112 violations (rc 1); the final run passes (rc 0).

| Regression screen | Before | After |
|---|---:|---:|
| Router / Profile | 6 | 0 |
| Servers / users | 21 | 0 |
| Servers / discover | 18 | 0 |
| Servers / Plex servers | 18 | 0 |
| Servers / Connect servers | 18 | 0 |
| Servers / libraries | 18 | 0 |
| Servers / rows | 13 | 0 |

Profile regression: Up to Achievements, Spotlight Settings result, SDL Back through `app_evento`, advance `app_atualizar` for 1,500 ms. Assert Profile remains open with its previous focus and sidebar destination, accepts OK, and the next Back reaches Home. The offline router fixture supplies authenticated session/discovery state. Settings Back now honors the saved parent instead of reopening Settings search. Profile suspends without resetting focus or leaving an exit request; Back resumes it. As a separate negative control, reverting only Profile suspension/resumption reproduces the reported Home bounce and five failed assertions (rc 1); restoring it passes.

Picker regression: 4, 5 and 9 items on Users, Discover, Plex servers, Connect servers, Libraries and Rows. Traverse every item forward and backward using SDL arrows, assert focused items render, then assert Down from the last item reaches a footer. Paging reveals an adjacent hidden item before rectangle navigation can select a footer; visible targets retain geometry navigation.

Final checks: `bash tests/nav21.sh`, `bash tools/build-local.sh`, `python3 tools/varredura-i18n.py`, and `git diff --check` all rc 0. The build retains the existing `ATOMIC_VAR_INIT` warning; i18n reports zero findings.

Evidence: `/Volumes/ExternalSSD/tmp/rev/navteste/p1-return-paging-before.log`, `p1-return-paging-before.md`, `p1-profile-lifecycle-before.log`, `p1-profile-lifecycle-before.md`, `p1-return-paging-final.log`, `p1-return-paging-build.log`, and `p1-return-paging-i18n.log`. No push, deployment, or TV validation. Exhaustive coverage of every original owner-request transition remains unestablished.

## Earlier follow-up

The review regressions fail before the fixes and pass afterward under `bash tests/nav21.sh`. Final run: **2,741 checks, zero violations**. Existing geometry and reachability checks remain enabled.

| Review regression | Before | After | Exercised path |
|---|---:|---:|---|
| Router / Spotlight | 2 | 0 | Library, Spotlight Settings result, Back: Library and sidebar destination; retained Library focus asserted |
| Router / Settings guide | 3 | 0 | Settings guide Library action, Back: Settings, guide entry/button focus and sidebar destination |
| Sources provider state | 6 | 0 | Long provider list, tabs, Right / Left: clear server filter, scroll and velocity |
| Wizard authentication lifecycle | 18 | 0 | Authenticated provider, next page, Back, advance 750 ms: retain auth page and focus |
| Motion / Achievements families | 1 | 0 | Families, Back: animate return to grid |
| Motion / Home entry fixture | 1 | 0 | Reopen fixture: reset row entry lifecycle, measure row reveal |

The first expanded run reported 39 violations: the 31 above plus eight Library geometry/reachability failures caused by running router fixtures before the Library catalog fixture. The Library fixture now runs first, preserving its original dataset. This was a test-order correction, not a production navigation change.

Motion checks require both rendered frame changes and changes in the page/sheet transition value over at least 150 ms. A hard-cut negative control with continuously animated content must fail the motion predicate. Focus/content animation alone cannot pass. Internal open/Back checks cover every wizard page, AIO pages, Start steps, Settings Debrid/Filters/filter category/tour, Achievements detail, add-on connection, source detail, Juntos Sources and Filmography title options. Island-owned sheets render the clock island and measure its actual transition value.

Router tests compile the production router and invoke its Spotlight request and guide-action consumers. The guide fixture sends OK through the real Settings event handler. Exact destinations, sidebar destinations and focus are asserted separately. Production Back uses the same return handler.

Validation from the worktree root:

- `bash tests/nav21.sh` — rc 0; 2,741 checks, zero violations.
- `bash tools/build-local.sh` — rc 0; existing `ATOMIC_VAR_INIT` deprecation warning.
- `python3 tools/varredura-i18n.py` — rc 0; zero findings in all three categories.
- `git diff --check` — rc 0.

Artifacts: `/Volumes/ExternalSSD/tmp/rev/navteste/p1-before.log`, `p1-final.log`, `p1-build.log`, `p1-i18n.log`, `violations.md`, `targets.tsv`, `library.png`, `settings.png`.

This follow-up addresses the listed P1 review findings. It does not establish exhaustive acceptance of the original all-screen owner task or TV performance. P2 findings and target-device validation remain outside this follow-up. No push or deployment performed.

## Library D-pad follow-up

The same final fixtures fail against the original Library dispatcher (rc 1) and pass with the fix (rc 0):

| Screen/state | Before | After | Exact key path |
|---|---:|---:|---|
| Library / clock and sidebar requests | 3 | 0 | To watch, Up; To watch, Down, Down to Origins, Left, advance exit animation |
| Owned list / reorder | 3 | 0 | Open list with OK, Down, Right to Order, OK, title OK, Down; Up, Up, Left, Right at first title |
| Open list / directional controls, notes and Back | 0 | 0 | Every rendered control and arrow; Down, Right, Right to Notes, OK, Down, title OK, Back, Back, Back |

Geometry yields to a picked title and to Origins Left; arrows without a geometry neighbour reach existing clock/sidebar/scroll handlers. The generic gate treats Origins Left as a route outside its page registry, while explicit request assertions verify that route. Open-list fixtures now use the real OK handler to establish ownership, editing state and saved browser focus, with offline item/entry fixtures instead of manually setting `temAberta`.

Validation: `bash tests/nav21.sh` rc 0, 3,555 checks, zero violations; `bash tools/build-local.sh` rc 0; `python3 tools/varredura-i18n.py` rc 0; `git diff --check` rc 0. Build retains the existing `ATOMIC_VAR_INIT` deprecation warning.

Evidence under `/Volumes/ExternalSSD/tmp/rev/navteste/`: `p1-library-before.log`, `p1-library-before.md`, `p1-library-final.log`, `p1-library-build.log`, `library.png`, `settings.png`. Exhaustive original screen variants and physical-TV validation remain unestablished. No push or deployment performed.

## Screen handlers before geometry

Home geometry previously consumed Right before the hero handler: the added regression records focus `1003` instead of `0` and carousel intent `0` instead of `1`. The baseline fails four explicit hero checks. Home now handles the hero, cinema wake-up, friends row and horizontal row scrolling before geometry. Right requests the next hero title while retaining hero focus; Left returns to the previous title and requests the sidebar at the first title.

Ordering audit covered every geometry screen changed by this branch: Home, Detail, Library/Lists, Filmography/History, Settings, Sources, AIOStreams, server wizard, Juntos lobby, Achievements, Start and add-on guide. Detail now prioritizes server/version/friends controls, carousel expansion/title changes, episode season edges, progress notes and soundtrack paging. Filmography tabs/year scrolling, Sources provider switching and Settings tour traversal/editor keys also retain priority. Library reorder/sidebar guards, AIO/server paging and Achievements scrolling already precede geometry. Juntos geometry is restricted to the lobby; its other layers retain their handlers. Start and add-on guide have no competing carousel/scroll handlers.

Nav21 distinguishes semantic arrows from geometric focus edges. Explicit assertions cover Home carousel/row boundaries, Detail carousel, Filmography tab changes/year boundary, Sources provider switching and Settings tour row crossing. Filmography's static per-page graph cannot replay paths across page-changing tabs; its geometry edge checks remain, while static reachability replay is omitted for those pages. Exhaustive cross-page Filmography reachability is not established.

Final validation: `bash tests/nav21.sh` rc 0 (3,391 checks, zero violations), `bash tools/build-local.sh` rc 0 and `git diff --check` rc 0. One local build; existing `ATOMIC_VAR_INIT` warning only. Logs: `/Volumes/ExternalSSD/tmp/rev/navteste/p1-hero-before.log`, `p1-hero-final.log`, `p1-hero-build.log`; PNGs: `library.png`, `settings.png`. No push, deployment or physical-TV validation.
