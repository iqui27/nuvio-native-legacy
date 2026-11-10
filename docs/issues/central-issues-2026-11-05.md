# Issues central — 2026-11-05, batches 1–6

Repository: iqui27/nuvio-native-legacy. Integration baseline inspected: `b22f4c5176ac50a49fd8efd122656e9c7ad34645` in `/Volumes/ExternalSSD/nv-21-int`. Published release checked: `v2.0.8`, published 2026-10-21. This reconciliation changed only its own documentation. No integration/map edits, product changes, push, release, installation or deployment.

## Verified results

- Initial live scope: **127 open issues**. The **613 historical backlog** figure includes closed history and is not the response queue.
- **68 public replies**, each preceded by a fresh target check. Final authenticated GitHub inventory verified every returned comment URL and body, allowing only boundary whitespace differences.
- **5 verified closures**, leaving **122 live open issues**. No new open issue outside the original inventory appeared in the final fetch.
- **59 reviewed without another public comment**: explicit issue-specific decision, evidence, responsible central and next step recorded. These are not resolved issues. Avoiding duplicate advice or empty acknowledgments is deliberate.
- **0 initial issues without an explicit reconciliation disposition.** Product investigation, implementation, release and device acceptance remain outstanding where specified.
- Reporter followups reviewed: #754 confirmed TPK40; #706 distinguished rooted CX with original screensaver from separately confirmed Glasshouse cases; #687 acknowledged the native trailer-language limitation. No repeated acknowledgment posted.

## Closures and their limits

| Issue | Evidence and public action |
|---|---|
| [649](https://github.com/iqui27/nuvio-native-legacy/issues/649#issuecomment-6659637350) | Completed: published v2.0.8 Android Lite assets. ARM64 48,372,435 bytes; ARMv7 47,778,615 bytes; full APK 67,967,288 bytes. This does not fulfill Tizen Lite request #663. |
| [621](https://github.com/iqui27/nuvio-native-legacy/issues/621#issuecomment-6659695564) | Completed: reporter explicitly confirmed GIF distortion fixed after 2.0.8 retest request. No independent physical test claimed; #660 animation lifecycle remains separate. |
| [716](https://github.com/iqui27/nuvio-native-legacy/issues/716#issuecomment-6659856493) | Duplicate of #710: same reporter, TV/version and subtitle description. Preserved #716 log and #710 network-warning subcase. Playback is not fixed. |
| [744](https://github.com/iqui27/nuvio-native-legacy/issues/744#issuecomment-6660022640) | Duplicate of #692: same reporter, Subtitle Sync and webOS 3/C1 setup. Both differing log references remain preserved. Add-on compatibility remains unresolved. |
| [683](https://github.com/iqui27/nuvio-native-legacy/issues/683#issuecomment-6660024792) | Completed on reporter's explicit statement that the black screen disappeared after re-downloading. No code fix or general startup acceptance claimed; recurrence can reopen this issue. |

## Public reply records

Exact bodies and evidence are in `central-respostas-2026-11-05.json` (18), `central-respostas-lote3-2026-11-05.json` (11), `central-respostas-lote4-2026-11-05.json` (13), `central-respostas-lote5-2026-11-05.json` (14), and `central-respostas-lote6-2026-11-05.json` (12). These files contain **already published comments**, not an unprocessed posting queue. All 68 URLs and verification records are in `central-ledger-2026-11-05.json`.

Replies match each discussion's language and distinguish published, integrated, host-tested, physical-device-tested and reporter-confirmed evidence. Diagnostic requests ask only for missing actionable details, excluding private media/configuration URLs and credentials. Existing supplied log codes are retained in GitHub rather than recopied into this report.

Examples of preserved scope:

- [659](https://github.com/iqui27/nuvio-native-legacy/issues/659#issuecomment-6660033648): Continue Watching = Aura remains unimplemented. Shared final independent static boundary review reports no new code or runtime proof. Catalog overrides alone do not fulfill the fixed-row requirement or global-default fallback.
- [677](https://github.com/iqui27/nuvio-native-legacy/issues/677#issuecomment-6660026889): `bc76197cc38c2690182bfe4208ecb31227194f7a` is an ancestor of integration, not v2.0.8. LG row-rebuild focus acceptance remains pending; no test package/date promised.
- [662](https://github.com/iqui27/nuvio-native-legacy/issues/662#issuecomment-6660026236): same-author body repeats #661, but title adds flickering. Asked whether distinct; did not close away that symptom.
- [634](https://github.com/iqui27/nuvio-native-legacy/issues/634#issuecomment-6660065135): reporter's same-account Fire TV comparison contradicts treating add-on language advice as an established solution.
- [650](https://github.com/iqui27/nuvio-native-legacy/issues/650#issuecomment-6660070527): native trailer-language support remains missing, related to #687 without asserting cross-device acceptance.
- [396](https://github.com/iqui27/nuvio-native-legacy/issues/396#issuecomment-6660032781): answered outstanding Discord requests using the permanent public invite verified in README; kept architecture/roadmap thread open.

## Explicit decisions without repeated comments

`central-dispositions-2026-11-05.json` holds all 59 issue-specific dispositions, also copied into the ledger. Counts:

- 28 feature requests with sufficient scope to assess against existing work; no invented schedule or implementation claim.
- 18 reports whose supplied logs/reproductions require investigation by the responsible central. More generic questions would not substitute for reading that evidence.
- 6 previously answered but unvalidated discussions.
- 3 discussions already awaiting an answer to a relevant question.
- 1 insufficient integration reference, 1 partially answered multi-scope report, 1 explicitly reopened regression, 1 maintained log-triage thread.

Concrete retained fixtures include #714 cold Simkl titles dropped for missing title object, #698 Mobland S2E6 with **Nuvio Sync only**, #707 reporter's retraction of the router-based resolution, #570 source switch substituting a previously watched movie and losing resume, #524 Slime/Chasing Mavericks identity, and #694 native-player TLS versus curl/relay A/B. None is classified fixed from a general metadata/network/subtitle change.

## Attributable proposals for Central TV, sole map writer

1. Mirror the five closure bases and all 68 comment URLs from the ledger. Preserve reporter-confirmed resolution versus published artifact versus duplicate.
2. #754: live reporter confirmed `Nuvio-2.0.8-NuvioTpk40.tpk` on NU7100/Tizen 4; the WGT form value was wrong. Mirroring on second launch remains unresolved.
3. #706: CX reporter uses original LG screensaver on a rooted TV. Other reporters' Glasshouse resolution does not settle this case. Unreleased screensaver-registration retry needs prolonged-playback plus pause/stop acceptance.
4. #756: production Jellyfin row order must survive process restart, same-profile reload, delayed server rows and final Home rebuild. Generic reorder tests with simulated save do not prove this.
5. #765: no-source-addon detection and Where to Watch exit remain distinct; external-app/store transition is not automatically a crash.
6. #687/#650: `AJ_TRAILER_AUDIO` hidden under `#ifndef __EMSCRIPTEN__` in released and integration code. Language/provider changes do not prove a usable native selector.
7. #549: `4baf783fa` is in v2.0.8 but clarified end-of-row See All tile placement is unproved. #661 all-row merge `a175353ca` is integrated, not in v2.0.8; original AU7000/Captain Collection test pending. Keep #711 centering and #662 flicker separate.
8. #659: final shared `issue659-fixed-active.md` reports fixed Continue Watching composition/menu boundary, no new commit or runtime proof. Preserve full request, including CW = Aura.
9. #694: reporter's same-file native TLS versus PC-relay A/B is useful; missing firmware root is still an inference. Inspect proposed patch and native-player path without weakening TLS verification or publishing relay advice as a fix.
10. Reuse existing logs and work for #714, #681, #684, #728 and remaining log-investigation dispositions. No duplicate implementation jobs launched by this central.

Android evidence requests were already routed for #759 Shield sources/timeouts and #740 C835 speed-change interruption. The ledger additionally preserves Android cases #698, #626, #641 and #646 and other platform-specific scopes. No result from another central is presumed complete without attributable evidence.

## Restart discipline

Read the ledger before any write. Pending comment/closure states require live reconciliation first. Posted URLs prevent resend; reviewed no-comment decisions prevent empty acknowledgments. Fresh comments can change the disposition and must be read before another outward action. Historical catalog classifications remain labeled as historical; this reconciliation does not turn them into runtime validation. Reopen the workflow when reporter answers or owning-central evidence arrives.
