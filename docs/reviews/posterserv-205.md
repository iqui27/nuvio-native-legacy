# Custom poster coverage and template compatibility

Read-only public research, 2026-10-19. No live credentialed service calls.

## Coverage

Home portrait rows (including Top 10, collections and large styles) already converge on `arte_por_formato` through `arte_por_identidade`. Library, search portrait results and Discover/catalog/See all grids already call `posterprov_card_addon`.

Bypasses fixed:
- Home social-row title thumbnail.
- Collection movie parts: shared `extras_colecao_poster` accessor, including detail staircase and collection list/fallback cover.
- Explore mood grids, recommendations, trails and focused title: shared `cartaz` renderer.
- Detail person filmography cards.
- Post-play related movie cards.
- Discover/See all portrait information side panel.
- Landscape Home rows and Continue Watching (including episode still), catalog timeline/landscape rows, search best result: generic opt-in through `{shape}`. Templates without `{shape}` preserve their landscape art.

Collection-level artwork has no movie/series identity and remains collection artwork. Hero backgrounds, episode thumbnails outside Continue Watching, people portraits and static demo assets are not title poster cards. Existing addon-preference precedence and failure fallback remain.

## Public contracts

- [Posters+ upstream URL reference](https://github.com/UmbraProjects/PostersPlus/blob/dev/URL_REFERENCE.md), sections Client templates and Anime IDs: `/poster?tmdb_id={tmdb_id?}&imdb_id={imdb_id?}&stremio_id={id}&type={type}&shape={shape}`. Either numeric TMDB or `tt...` IMDb identifies a title. Type is movie/series; no `movie-`/`series-` prefix is needed in `tmdb_id`. Those prefixes belong to the existing RPDB preset. Query styling and literal keys pass through unchanged. `shape={shape}` was rejected by this engine, invalidating the entire copied template; now supported as poster/landscape, with independent cache/failure identities.
- [Posters+ deployment documentation](https://github.com/elfhosted/PostersPlus): WebP default, JPEG configurable with server `IMAGE_FORMAT=jpeg`; `ACCESS_KEY` becomes `access_key` in URLs if configured. TMDB/MDBList server keys or request keys supply enrichment. Debug output is JSON (`debug=1`), not an image; omit for poster URLs.
- [Pictorium hosted endpoint documentation](https://docs.elfhosted.com/app/pictorium/): `/api/poster/{type}/{tmdb_id}` or `/api/poster/{type}/{imdb_id}`, movie/series and bare numeric TMDB or IMDb `tt...`. Existing `{id}` can choose IMDb then numeric TMDB. Editor supplies style query parameters; hosted instances supply keys, public profiles require their own TMDB key.
- [Pictorium upstream endpoint](https://github.com/Eful97/Pictorium/blob/master/src/app/api/poster/%5Btype%5D/%5Bid%5D/route.ts): `fmt` or `format` chooses output. Use `?fmt=jpeg` (append with `&` when needed); WebP also supported. SVG is used inside server compositing, not the normal poster response. AVIF is explicit legacy `fmt=avif`; replace that with `fmt=jpeg` for this client.

Neither public poster contract requires a TVDB identity marker. TVDB keys support server-side artwork/ordering enrichment. No TVDB IDs are fabricated from TMDB IDs. No new named presets: preserve configurator/profile/instance-specific URLs in Custom template. No service-name branches or decoder dependencies added.

The binary download path already follows redirects (`src/rede.c`); texture decode already supports JPEG/PNG/WebP (`src/tex_cache.c`). No network/decoder change needed. This is offline URL/selector proof, not authenticated rendering or TV validation. Existing `{id}` semantics remain IMDb-or-TMDB; anime-only/raw namespaced metadata IDs remain unsupported by this engine.

## Verification

`bash tests/posterserv.sh`: failed before implementation (rc 134, actual extracted collection accessor returned original poster), passed after (rc 0). Also checks the exported optional-ID/shape/key template, independent shape URLs, disabled-provider fallback and landscape opt-in.

`bash tests/posterserv_build.sh`: rc 0; full host compile/link through `tools/mac.sh`, temporary output, no launch.
