# Functional tour completion

Merged integracao/2.1 before finishing the tour. Settings v4 now has a dedicated level-zero tour entry, preserving the chapter guide under Help. The tour includes subtitle sync (audio on Android, automatic sync elsewhere), second subtitles, Seekr, clock alerts, source selection, DTS conversion, profiles, live TV, public profiles, data saving, rest screen, Watch Together, Dolby Vision and the previous appearance/server cards.

Settings cards focus the v4 destination and open its draft editor without saving. Profiles open the v4 profile controls; Xtream opens the provider manager. Watch Together has no settings toggle: its translated instructions lead through Library to a title's Watch Together action. Unavailable platform/service settings remain visibly disabled. Preview scenes contain illustrations rather than setting switches; only the focused scene animates.

Validation: tests/ajustes_tour.sh failed on missing required cards (134), then passed (0). Final build-local.sh, varredura-i18n.py, idiomas.py and git diff --check passed. Independent reviews found route mismatches, untranslated illustration labels and clipped quality labels; all were corrected. Final host captures show the corrected labels and focused destinations.

Visual artifacts: /Volumes/ExternalSSD/tmp/rev/ajtour-png/{solid,glass}-*.png and trio-{solid,glass}-*.png, pt-BR with NUVIO_SHOT_FONTE=3. Every final capture and trio was opened. Tour references png-v2/04.png; entry/list/editor comparisons use approved Settings v4 references. The tour shows actual card/new counts instead of the mock's example enabled/release counts, uses real platform-specific sync copy, and separates DTS and Dolby Vision. Later tour rows have no separate approved mockup; trios use the tour's first-page layout reference. Existing v4 list/editor differences are not changed by this tour patch.

No push or deployment. Physical-TV performance, remote usability and live service acceptance were not tested on the host.
