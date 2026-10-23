# Trailer subtitles (plan, not built)

Request: Settings > Trailers > "Trailer subtitles" = Off (default) / Auto (UI language) / a language.

Status: only "Trailer audio" ships (`AJ_TRAILER_AUDIO`, `ajustes_trailer_audio()`, ranking in `src/extras.c`).
Subtitles need a new path; no caption code exists in the tree (`grep -ri "captionTracks\|timedtext" src` is empty).

## What is needed
1. Caption list: the YouTube player response (`captions.playerCaptionsTracklistRenderer.captionTracks[]`, each with
   `baseUrl`, `languageCode`, `kind` = `asr` for auto-generated). Where the trailer URL is resolved (trailerfonte.c /
   YouTube extractor) also keep this list. Only the extractor path sees it; the Samsung .wgt `iframe` player and any
   platform that plays through the YouTube embed cannot expose tracks, so the option must read "not available on this
   TV" there (no fake option).
2. Fetch: `baseUrl&fmt=vtt` (or `srv3`), pick `languageCode` == chosen language, prefer manual over `asr`; with no match,
   try `&tlang=<xx>` machine translation, else show none.
3. Render: feed the cues to the existing subtitle renderer used by the player (same path as external .vtt/.srt),
   with the user's subtitle style. The hero/extras trailer surface does not own a renderer today; it must be given one
   and be clocked by the trailer's playback position.
4. Setting: `AJ_TRAILER_LEGENDA` (append-only, local, key `trailerLegendaLocal`), values Off / Auto / list from
   `ling_opcao_*` (same list as `V_TRAUD`). i18n rows in all 28 tables.
5. Test: fixture `playerResponse` JSON with two tracks (pt-BR manual, en asr); assert pt-BR chosen for "pt", Off picks none.

## Risk
Extractor changes break often (YouTube side); keep the parse behind one function and fail silent (no subtitles).

## Built (webOS/emscripten YouTube embed)
No new setting: reuses the subtitle language (`ling_legenda()`). `trailer_js_legenda` in `src/trailer.c` holds the rule:
no captions when the trailer audio is dubbed in that language, none without a track in it, else that track.
Needs on-TV check: the tracklist reply (`info.tracklist`) from the embed/proxy page; without it, no captions.
