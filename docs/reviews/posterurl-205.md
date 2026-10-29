# Custom poster URL investigation (#205)

Custom templates were limited to 400 input bytes and 511 generated URL bytes, although catalog posters and the texture cache already accept 2047 bytes. A 716-byte Posters Plus style fixture was truncated to 400 bytes by the settings keyboard and could not retain its complete configuration. This is a confirmed local defect; the Discord report does not provide enough information to establish that it is the user's exact failure.

The fix reuses `NV_ARTE_URL_MAX` for provider templates, generated URLs, keyboard storage and native text bridges. Settings persistence already sizes its buffers from the provider constants. Expansion still rejects addresses that exceed the shared limit. Size notices use the new limit in all translations. The phone webpage retains its existing 1024-character input limit; the reproduced fixture fits it.

## Upstream comparison

Reference: [NuvioMobile CustomPosterUrlResolver.kt](https://github.com/NuvioMedia/NuvioMobile/blob/229c083cf3db7bab6fda284484f87439eb4e0eb0/composeApp/src/commonMain/kotlin/com/nuvio/app/core/poster/CustomPosterUrlResolver.kt), and `CustomPosterUrlRepository.kt` at the same revision. The repository stores the trimmed pattern without a 400-byte limit.

- Native Legacy already supports `{imdb}`, `{imdb_id}`, `{tmdb}`, `{tmdb_id}`, `{id}`, `{type}` and `{tipo_tmdb}`. Required missing IDs cause fallback. Literal API keys and existing percent-encoded query values are preserved.
- Mobile additionally supports `{id_type}`, `{typed_id}`, `{shape}`, other ID namespaces, optional placeholders (`{tmdb_id?}`), alternatives (`{imdb_id|tmdb_id}`), encoded braces/pipes, and automatic ID-path fallback for ratingposterdb.com, aioratings.com, top-posters.com and btttr.cc. Those remain compatibility gaps outside this focused length fix. Mobile's `{id}` preserves a namespaced meta ID; Legacy prefers IMDb, otherwise the numeric TMDB ID.
- `{season}` is not supported by the inspected upstream resolver either. No season placeholder was added.
- Native downloading already follows redirects (`src/rede.c`), and texture validation recognizes JPEG, PNG, GIF and WebP signatures, with WebP decoding fallback (`src/tex_cache.c`). No evidence justified transport or codec changes.

## Verification

`bash tests/poster_modelo_longo.sh`: rc 1 before the fix, rc 0 after. The 716-byte `.invalid` fixture uses fake keys; no poster service is contacted. Checks cover input, save/reload, placeholder expansion, key case, encoded values, trailing options, card URL, provider recognition, native event queue, TV text bridge and URL expansion limits.

`bash tools/build-local.sh`: rc 0. Existing `ATOMIC_VAR_INIT` deprecation warning.

`python3 tools/varredura-i18n.py`: rc 0; all three categories empty.

`git diff --check`: rc 0.

Additional `bash tests/posterprov.sh` is blocked by existing test stubs: the pure provider executable imports `jellyfin_stub.inc`, whose weak external functions fail to link on macOS; bypassing that unused include allowed the provider assertions to pass, but the addon fixture then reported duplicate session wrapper definitions. The include was restored. Only the outdated 512-byte expectation was adjusted for the new shared limit. Stub repairs are pending.

No device run, service request, screenshot, installation, deployment or push. Full provider parity and the exact user template/platform remain unverified.
