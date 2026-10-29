# Live TV integration for 2.0.7

Baseline: `f246d8e919a4763ce467f5d3ecbaa363f20e8512`.
Merged branches: `feat/guia-tv`, then `agente/guiaunicode`, both with merge commits.

Screenshots use `tests/guia_shot.sh` with `NUVIO_SHOT_VIDRO=1` and generated EPG fixtures. Program times follow the capture clock. The baseline contains eight channels; the merged fixture contains ten channels across two sources, including separate sports groups. The hero, preview, and grid retain integration geometry and glass rendering. Source chips occupy previously empty hero space.

- `before.png`: baseline guide with current program.
- `after.png`: merged guide with source filters and provider channel numbers.
- `after-sources.png`: selected source, health, and capacity notice.
- `after-groups.png`: per-source group picker.

## Release notes

- Browse Live TV by source or favorites, with separate groups for each provider.
- Load larger channel catalogs within device limits and choose which groups to include.
- Keep provider group order or sort groups alphabetically per source.
- See source health, channel counts, capacity notices, and cached channel lists during outages.
- Add and remove M3U playlists and XMLTV guide URLs separately for each profile.
- Play M3U channels with their required User-Agent and Referer headers.
- Load large XMLTV guides in chunks and reuse cached schedules for faster reopening.
- Match schedules by provider ID, channel name, and aliases, or choose a channel's guide manually.
- Correct guide time offsets separately for each source.
- Tune by channel number and return to the previous channel from Live TV controls.
- Show consistent current and upcoming programs in the guide and player OSD.
- Recover supported Android live streams that fall behind the live window.
- Decode escaped Unicode in channel categories correctly.
- Translate the new Live TV controls into English and 28 additional languages.

Integration preserves profile ownership, load cancellation, declared add-on catalog types, complete CNCVerse channel IDs, stream headers, and bounded EPG search pagination.
