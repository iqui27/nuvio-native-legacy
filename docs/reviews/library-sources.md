# Library Sources regression

Baseline: `8480546ae`, compared with the working Lists picker from `873e8b98b`.

## Failures recorded before changes

- Back closes the picker only in Lists. In Library it starts leaving the screen, with the picker still open.
- Selecting a Library account or add-on filters without closing the picker or returning focus to Sources. Server selection has the same inconsistency.
- Watched excludes the Sources button and cycles only the four account origins through a separate chip; add-ons and servers cannot be selected there.
- Library draws its own picker with T/S/N/A letter badges. Lists uses Lucide icons and provider marks.
- The Library panel has its own fixed 700-pixel height and footer, independent of the working Lists picker viewport.
- Watchlist membership is restricted to the Settings save destination before the source filter runs. All and other providers can therefore omit saved titles.
- Account counts deduplicate before considering each provider. A second catalog copy with another provider membership is lost from that provider's count.
- Geometric navigation runs ahead of Library source handlers, unlike Lists; rendered focus targets can override the source row's intended arrow handling.

`tests/biblioteca_sources.sh` includes the production screen, draws it with offline fixtures, sends remote keys, and captures full spring frames. It covers All, Trakt, Simkl, Nuvio account, AIOMetadata, Plex, Jellyfin, and Emby across the four Library states, plus Lists Back behavior. The final test reports 68 failed checks against the baseline and zero against the fix. Baseline failures and PNGs are saved under `/Volumes/ExternalSSD/tmp/rev/bibfonte/`.

## Result

Library and Lists share one picker renderer, source identity/marks, spring, clipping, selection return path, and Back handling. All four Library tabs expose it. The large source title uses `TXT_ILHA_TITULO`, reserves the clock width, animates on change, and leaves synchronization on the subtitle row. Account and server counts deduplicate per provider. The Library All filter includes all saved sources independently of the Settings write destination; detail/context save membership still follows Settings.

Verified: `bash tests/biblioteca_sources.sh`, `bash tests/listasux.sh`, `bash tests/biblioteca_filtro.sh`, `bash tests/bibfonte.sh`, and `bash tools/build-local.sh` pass. The save-path fixture now explicitly selects its write provider, while All-provider behavior is covered by the new screen test. Before/after PNGs include closed/open Library, selected Trakt, selected Lists source, and Watched filtered to Emby.

No new translation keys were needed. `python3 tools/idiomas.py --sincronizar` completed without translation changes.

The extra `tests/servidores_library.sh` suite passes Library ownership/provider filtering, then fails linking its unrelated stream-source fixture because it lacks dependencies such as `streamfit_particionar`, `txt_addon_altura`, and `txt_bloco`. The new native screen test covers all three server providers.

The requested `/Volumes/ExternalSSD/tmp/rev/DESIGN.md` was absent. The existing design-system copy at `/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy/DESIGN.md` supplies the official typography, island material, focus, and animation rules.
