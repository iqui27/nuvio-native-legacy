# Source selection improvements

Date: 15 October 2026
Base: `origin/master` at `3c28c0d7` (includes #487, addon-declared audio/subtitle languages)

## 1. Code Geass S1E3: why Legacy picked the 2 Mbps WEB-DL

Before this change, the automatic "Best source" score (`pontos()` / `qualidade()` in `src/streams.c`) only used:

- resolution step (`degrauRes`, 10 000 per step),
- HDR / Dolby Vision level (`nivelHdr`),
- `altura / 10`, **+300 for MP4**, +200 for Atmos,
- penalties: over the quality cap or size range, uncached, P2P, plugin, StreamFit "heavy", no resolution.

Release type, bitrate, file size, video codec and audio format were **never part of the score**. The selo parser already detected `q-remux`, `q-webdl` etc., but only for display.

For the two Code Geass candidates (both cached on TorBox through AIOStreams, both 1080p SDR):

| Source | Base | MP4 | Total (old) |
|---|---:|---:|---:|
| Blu-ray REMUX, AVC, FLAC 2.0, 36 Mbps, 6.57 GB (`.mkv`) | 20 000 + 108 | 0 | **20 108** |
| WEB-DL, AVC, AAC, ~2 Mbps, 421 MB (Lilith-Raws ships `.mp4`) | 20 000 + 108 | +300 | **20 408** |

The WEB-DL won by the MP4 bonus alone. `tests/fontenota.c` reproduces this exact arithmetic before asserting the new result.

Two further factors can make the wrong pick sticky or more likely, and are confirmed by the code:

- **Auto memory (bingeGroup).** After the automatic choice plays, `fontepref_auto_guardar` stores its bingeGroup. On the next episode `escolherAuto` puts that group first, gated only by `stream_grupo_sem_perda`, which compared resolution and Dolby Vision. A WEB-DL chosen in episode 1 or 2 therefore came back in episode 3 even with a REMUX present. This also explains why the sheet showed the WEB-DL as both "Now playing" and "Best for this TV": `automaticaDaFolha()` returns the remembered source first.
- **Priority "Start fast"** subtracts 4 000 points per GB (capped at 40 000), so a 6.57 GB file loses ~24 000 points. In "Balanced", a REMUX measured as too heavy by StreamFit drops by 140 000. Neither applied to the screenshots as far as the code can tell, but the new diagnostic line logs both.

**Remaining uncertainty:** I don't have the device log from that playback, so I can't tell whether the WEB-DL was picked by score in episode 3 or carried over from an earlier episode by auto memory. Both paths are fixed. The original Nuvio screenshot only proves that the REMUX was listed. Original autoplay picks whatever AIOStreams lists first (see section 2), so it would have played the REMUX only if AIOStreams sorted it first.

## 2. Original Nuvio vs Legacy

**Original NuvioTV** (`StreamAutoPlaySelector.kt`) has **no quality scoring**:

- `orderAddonStreams`: groups that contain direct-debrid streams first, then the primary and backup addon, then installed-addon order, then plugins. Each addon's own stream order is kept.
- `selectAutoPlayStream`: source scope and allowlists filter the list. A persisted bingeGroup is played first if it is playable. Then `FIRST_STREAM` takes the first playable stream; `REGEX_MATCH` takes the first playable regex match. "Playable" excludes external links and debrid state `CHECKING`, `NOT_CACHED` and `UNKNOWN`.
- Any quality ordering comes from the addon itself, for example the AIOStreams sort.

**Legacy** (`fonteauto.c`, `fonteregra.c`, `streams.c`):

| | Original First Stream | Legacy "First in list" | Legacy "Best source" |
|---|---|---|---|
| Order used | addon order + each addon's stream order | same (`exib`: install order, then the addon's order) | score (`pontos`) |
| Remembered source | bingeGroup first | manual or auto memory first | manual or auto memory first |
| Allowlists / scope / regex | yes | yes (groups 0–3) | yes |
| Strict addon order | n/a | candidates within the cap, earliest addon first | earliest addon's best candidate |
| Tie-break addon order | n/a | n/a | same quality band (10 000) → earliest addon |
| Uncached / unplayable | excluded | excluded (`foraDoAuto`) | excluded |
| Quality cap / size range | n/a | moved to the end, never filtered | penalty, never filtered |
| Sources checked | 1 | 1 (`fonteauto_tentativas`) | serial or parallel (cached only) |

**Display order vs autoplay order.** The Sources sheet groups by resolution and HDR and sorts by size, but autoplay never uses that visual order. It uses `exib` (addon order) in First mode and the score in Best mode.

**Finding:** "First in list" already gives Mode A (AIOStreams order) in what it plays. One inconsistency was found and fixed: the "Best for this TV" badge and the sheet's first row always came from the score winner, even in First-in-List mode, so the sheet could highlight a source autoplay would not play. No new setting was needed for Mode A.

## 3. Files changed

| File | Change |
|---|---|
| `src/fontenota.h`, `src/fontenota.c` (new) | Pure evidence module: release type, credible bitrate, audio, codec and bit depth, language preference, compatibility (Hi10P, 4K upscale), in-band combination, safe log line. |
| `src/streams.c` | Addon-declared audio languages (`audioIdiomas`) feed the language preference; evidence cached per list index; added inside the resolution band in `qualidade()`; only language and compatibility in "Start fast"; `auto pick evidence` log line; auto memory yields to clearly stronger evidence; First-in-List badge matches what plays. |
| `src/ajustes.c`, `ajustes.h`, `ajustes_ux_*.inc` | One new local option, `AJ_FONTE_IDIOMA_AUDIO` ("Prefer audio language", off by default), appended to the end of the enum and the key table. |
| `src/idioma_tab.h`, `src/idioma_*.h` (28) | Label and help text, translated in every language table. |
| `tests/fontenota.c`, `tests/fontenota.sh` (new) | Fixture tests and a contract check on `streams.c`. |

## 4. How the improved algorithm works

`fontenota_avaliar()` reads only what the addon sent: name, description, file name, exact bytes (or the size in the text), and the runtime if `streams.c` already knows it from StreamFit metadata. Selo bits from the parser are a fallback when the stored text says nothing, because the addon's `title` survives only in the selos. Nothing is invented: a missing field contributes 0.

- **Bitrate** is either declared (`36.2 Mbps`, `2400 kbps`; `MB/s` is ignored as a download speed) or derived as size × 8 / runtime. Without a runtime, size is never turned into bitrate. A value above what the resolution allows is ignored; this also catches season-pack sizes. If the declared and derived values disagree by more than 2.5×, both are ignored.
- **Misleading names:** a "REMUX" whose credible bitrate is below a real remux (under 15 Mbps at 1080p, under 30 Mbps at 4K) gets no remux points.
- **Upscales:** a 4K labelled "upscale", or with a credible bitrate under 5 Mbps, drops one full resolution step and competes as 1080p.
- **Hi10P** (AVC 10-bit, common in older anime) is penalised on TV and Android, which have no hardware decoder for it.
- **Language** (only when the option is on): when the addon declares audio languages (`Stream.audioIdiomas`, #487), that list decides: the preferred language in it scores, and the release text is ignored, which is the same rule the language chip in the sheet uses. Without a declared list, the preferred audio language written in the text or as a flag scores more than "Dual/Multi audio" without named languages, and mentions near "sub" or "legenda" are treated as subtitles. A preference never filters anything.

`fontenota_somar()` adds the evidence and the language bonus **inside the current resolution band**, never reaching the next step, so a 1080p REMUX can't beat a 4K. Compatibility adjustments are then applied, and all existing penalties (cache, cap, plugin, StreamFit, P2P) still come after.

## 5. Settings

No existing setting changed meaning, and no existing text changed. Choosing the behaviour:

- **Settings › Playback › More options › Source choice › Automatic source**
  - **First in list** = Mode A: plays AIOStreams' order (plus allowlists, scope, regex, Strict or Tie-break addon order, cache and playability checks). The "Best for this TV" badge now shows that same source.
  - **Best source** = Mode B, using **"What Nuvio prioritises"**:
    - *Maximum quality*: resolution › HDR/DV › release evidence; no speed downgrade.
    - *Balanced* (default): the same evidence, plus the existing StreamFit downgrade when a source is measured as too heavy.
    - *Start fast*: unchanged (smaller and faster first), plus language and compatibility.
- **New: "Prefer audio language"** (below "What Nuvio prioritises"; default **Off**, local to the device). When on, it uses the existing **Languages › Audio language** setting to prefer dub or dual-audio releases at the same resolution. Track selection after the file opens is unchanged and still uses that same setting in `video.c`.

## 6. Ranking factors and priority (Best source)

From strongest to weakest:

1. Never auto-played: uncached, warning pages, decoder-blocked, dead links; allowlist and scope groups.
2. Penalties: over the cap or size range (sent to the end), P2P, plugin, StreamFit heavy (Balanced), no resolution.
3. Resolution step (10 000). A 4K upscale counts one step lower.
4. HDR / Dolby Vision level for this TV and setting (existing rules: Profile 5, MKV on LG, DV on/off).
5. Preferred audio language, if enabled: +2 000 explicit, +1 200 dual/multi, kept within the band.
6. Release type: REMUX +1 000, Blu-ray +550, WEB-DL +350, WEBRip +250, HDTV +100, CAM −3 000.
7. Credible bitrate relative to a good encode at that resolution: 0 to +300.
8. Audio: lossless +150, surround +100, basic +30. Atmos keeps its existing +200.
9. HEVC or AV1 10-bit: +50. There is no AVC-vs-HEVC bias.
10. MP4 +300 (existing; on LG the MP4-first rule still applies inside the band).
11. Compatibility: Hi10P −2 500.

Where scores tie, list order still decides, which is the addon's own order.

**Failover:** `falhaCandidatas` uses the same `pontos()`, so alternatives follow the same preferences. Its candidate filtering and manual-source handling are unchanged.

**Auto memory:** in Best source with Balanced or Maximum, a remembered auto group is skipped when the current winner at the same resolution leads by more than 300 evidence points (logged as `auto memory: … skipped`). Manual choices (`fontepref` manual) are never overridden.

**Logging:** each automatic pick now adds `[fonte] auto pick evidence: winner {tier=… kbps=…(declared|size/runtime) audio=… codec=… lang=… evidence=… adjust=… alerts=…} | runner-up {…}`. It contains only derived fields: no URLs, tokens or raw text. A test checks this with a TorBox-style URL containing a token.

## 7. Test results (WSL Ubuntu, gcc, SDL 2 built from source)

- `tests/fontenota.sh`: **pass**. It covers:
  - Code Geass (old vs new arithmetic, why the REMUX wins, size/runtime path, Start fast unchanged)
  - dual-audio vs Japanese-only, dub WEB-DL vs Japanese REMUX, Japanese-only still plays, "English Subs" is not audio, addon-declared audio list overrides the release text
  - 4K DV REMUX vs 1080p, 4K DV with no metadata, 1080p Blu-ray vs 4K upscale (by name and by 3 Mbps), genuine 4K WEB-DL
  - HEVC 10-bit vs AVC (+50 only), Hi10P, AVC REMUX vs HEVC encode
  - missing bitrate, missing size, fake REMUX, a 40 GB file with no metadata, implausible bitrate, bitrate conflict, `MB/s`
  - log safety and selo hints
  - uncached, unplayable, First in List, Strict, Tie-break, remembered source, failover queue order
- Also **pass**: `fonteregra`, `fonteregra_oficial`, `fonteantecipa`, `notasfontes`, `naovideo`, `streamfit`, `streamfit_passiva`, `stream_parser`, `stream_mp4`, `fonte_qualidade`, `fonte_lg_mp4`, `fonteescolha`, `fontefalha`, `fontepref`, `fontepref_auto`, `fontes_parcial` (expectation updated, see below), `fontecache`, `fontevolta`, `selospacote`.
- These fail **identically on unchanged `master`** (pre-existing, not caused by this change): `fonteauto` and `fonteparalela` (source-contract greps still look for the body of `stream_primeira_boa`, which now delegates to `stream_primeira_boa_sessao`), `fontepref_manual` (stale contract grep), `fontes_lista` (`conferirFolhaCheia: esc == 4`), `fonte_ordem_addon` and `fontecache_vod` (link errors on Linux), `fontefalha_playback` and `fontepref_file` (need SDL2_image/SDL2_ttf dev packages not present in this environment).

`fontes_parcial` changed expectation: its two 1080p "Rapido" fixtures are a WEB-DL and a BluRay. The test previously relied on the WEB-DL being the sheet's row 0. With release evidence the BluRay is row 0 ("Best for this TV"), which is the intended result, so the test now asserts that the BluRay wins and derives the focused card from the ranking. What it tests (focus stays on the same card when a slower addon arrives) is unchanged.

## 8. Regressions checked

- Position-preserving failover: same `falhaCandidatas` code; queue order tested.
- Dolby Vision and TrueHD: the HDR level and DV rules are untouched. TrueHD is only a +150 audio hint. The Samsung `.tpk` DTS/TrueHD penalty is unchanged.
- Manual source selection, the manual memory and the manual failover policy are untouched.
- LG MP4-first rule: untouched, still applies within the band.

Still needed on a device (real account and media): Code Geass S1E3 with "Best source" in both Balanced and Maximum, and once with "First in list", checking the `auto pick evidence` log line.
