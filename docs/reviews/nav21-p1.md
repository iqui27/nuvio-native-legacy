# Navigation P1 follow-up

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
