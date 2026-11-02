The Filmography Back change introduces a navigation regression when the screen is opened with focus on its hero actions.

Review comment:

- [P2] Let Back exit from the hero action row — /Volumes/ExternalSSD/nv-21-21-suiteassert2/src/filmo.c:195-195
  When Filmography opens for a new person, `filmo_abrir()` sets `zona` to 0, the hero action row. This condition treats that row like content, so pressing Back immediately after opening moves focus down to the tabs instead of returning to the previous screen. Restrict the tab-row redirect to content zones so Back still exits from the hero row.

## Follow-up (P2 fix)

- **Change:** In `src/filmo.c`, Back handler keeps sheet dismiss first (`camada`), then redirects to the selected tab only when `zona >= 2`; `zona == 0` (hero) and `zona == 1` (tabs) set `saindo = 1`.
- **Tests:** `tests/nav21_filmo.c` — hero open + Back expects exit; content (`zona == 4`) + Back expects tab focus, second Back expects exit.
- **Baseline:** pre-fix branch snapshot → `bash tests/nav21.sh` rc=1, 3632 checks/152 violations; History, Filmography, Filmography/about, and Filmography/achievements each had 1.
- **After fix:** `bash tests/nav21.sh` rc=1, 3637 checks/148 violations; Filmography, /about, and /achievements had 0 per job output. Integration HEAD baseline (`af596b224`) aborts earlier at `nav21_library.c:51` (rc=134), before reaching Filmography. Remaining 148 findings are not individually claimed as pre-existing.
- **Remaining limits:** no device test or build in this branch pass.
