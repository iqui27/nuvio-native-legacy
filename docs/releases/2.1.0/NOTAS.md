# Nuvio Legacy 2.1.0 — draft

Draft based on local `v2.0.8..HEAD` history. Release scope, issue attribution, device validation and artifacts require review before publication.

## Highlights

- **Watch Together:** a lobby-first flow with invitations, shared source selection, readiness and cinema start (`077db1ce7`, `035955c15`, `e36bb3eb1`).
- **Diary and achievements:** per-profile viewing history, sleep recovery and achievement trails (`bf3dab6b6`, `ea16b2445`, `ad4772184`).
- **Library and profiles:** redesigned library views and per-profile controls (`e28a6df66`, `a52915384`).
- **AIOStreams:** a dedicated dashboard, configuration access, catalog inventory and diagnostics (`130cf8044`, `45d9801a0`, `e2de8a9d2`).
- **Settings Palco:** redesigned blades for navigating settings (`bb31fa345`).
- **Multiview:** several channels on one screen in a 16:9 grid — the main tile plays live video while the other tiles show static channel previews; the picker lists each source's channels with per-source and per-account limits (`fc92c0b91`, `a76364a22`).

## New

- Watch Together connects the title page, player and island, with lobby invitations and shared sources (`a194cf307`, `035955c15`).
- The diary tracks viewing per profile, supports Trakt history import and offers optional private account storage (`bf3dab6b6`, `7b0955bee`, `5b70a63fc`).
- Achievements include local trails and history-based progress (`3ff9488b6`, `ad4772184`).
- Library includes compact title lists and server library and subtitle support (`e28a6df66`, `0f4d7774d`).
- AIOStreams adds dashboard sections, catalog inventory, configuration capabilities and diagnostics; phone pairing is available for setup (`7f101143e`, `45d9801a0`).
- Sports sections were integrated into guide v5 (PR #619, `07f75d823`).

## Improved

- Profile plugin inheritance and PIN enforcement survive profile deletion (`6001ff5f4`).
- Watch Together handles saved-source rooms, credential refresh and stale playback errors (`d8ed10bc9`, `a92671f50`, `0d0f2bb00`).
- Subtitle controls support millisecond adjustment and preserve selected tracks across Samsung seeks (`f4e95bc28`, `fa3c6a936`).
- AIOStreams Jellyfin Quick Connect accepts longer secrets (PR #726, `b60f48248`).

## Fixed

- **#697:** Next Episode keeps episode-bearing anime IDs associated with their own title (`de0db2a20`).
- **#681:** the last subtitle row remains selectable when add-ons return the same file (`a6271d24b`).
- **#708:** custom posters apply to Spotlight rows and list covers (`fd438a50b`, `bc6ed43c2`).
- **#700:** collection genre pages fetch titles for the selected genre (`bff0cbc4b`).
- **#650, #687:** localized YouTube trailer launch on native targets, including Samsung .tpk via the YouTube app (`729c81b37`, `950aca515`).
- **#692:** subtitle add-ons that were not probed stay in episode searches, so the Subtitle Sync add-on keeps contributing results (`cf40fa40f`, covered by `tests/addons_legendas.sh` against the real manifest).
- **#706:** the webOS screensaver request is answered during playback via `protegerScreensaver`/`responseScreenSaverRequest`, so the TV screensaver does not fire mid-session (LG webOS; `e7f76855d`, `11b687b96`; covered by `tests/lsregistro.sh`).

## Platforms and files

| Platform | Relevant files or changes |
| --- | --- |
| LG webOS | Shared features and legacy SDL compatibility (`a877c0f7c`, `e79037376`). |
| Samsung Tizen native | Localized trailer launch and subtitle track persistence (`729c81b37`, `fa3c6a936`). |
| Samsung Tizen browser | AIOStreams identified stream requests (`5798edb37`). |
| Android / Google TV | Watch Together preload remains paused (`214789fc2`). |
| Shared | Diary, achievements, Library, profiles, AIOStreams and Settings changes in `src/`. |

## Known issues

- Review the older 2.0.8 device-validation follow-ups and verify release artifacts on target devices before publication.

## Community

Thank you to everyone who contributed code, reported problems and tested builds. Local git authors include Grant Barker, Henrique Rocha, I0-oX, KeijoMika, Mateusz, Vidhin05, ghbarker and rauldzmartin; verify identities and contribution attribution before publication. Git also records automated contributors Claude Code, Claude Fable 5.1, Claude Opus 5.5, Claude Sonnet 5.5, Copilot, nuvio-agente and pi.

Thank you to reporters of #650, #681, #687, #692, #697, #700, #706 and #708. Local older notes reviewed do not establish reporter identities for these reports.

## Pull requests

Local history records merges for #619, #627, #633, #654, #669, #673, #685, #686, #688 and #726. Verify each contribution and its author before publication; a test merge of #573 also appears in history and needs separate verification.

## Issues closed or addressed

See **Fixed** for commit-level evidence, including #692 and #706.
