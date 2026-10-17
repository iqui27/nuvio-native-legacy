# Issue #512 identity fixture

`cinemeta_kaiju_reduced.json` contains four unchanged episode objects from
the public Cinemeta response for Kaiju No. 8 (`tt21975436`). The enclosing
metadata is reduced to the fields used by the existing enrichment route.
`provenance.json` records the public URL, capture time, original response hash,
and selected episode IDs. No authenticated tracking account was accessed.

The C test generates a **synthetic** Trakt playback response using the
issue's reported continuous-numbering pattern: S01E14 is the episode which
the catalog calls S02E02. The episode TVDB ID is 11209906 in both responses.
This is a controlled example of the reported mismatch, not a claim that a
live Trakt account currently returns this numbering.

`catalog_duplicate_episode_id.json` deliberately corrupts S02E01 to carry
the same TVDB ID as S02E02. That fixture must not produce an arbitrary match.

The regression exercises the real `trakt_continuar` response parser, normal
metadata enrichment, playback-record removal, and `prox_seguinte`. Network
functions alone are replaced. Its intended RED on unchanged upstream is:

    mode=mapped actual=S01E14 expected=S02E02

The ordinary-numbering, missing-ID, duplicate-ID, and unavailable-metadata
controls preserve the existing pair. In particular, this test does not ask
`prox_ancora` to reinterpret any S01 episode by its absolute position.

Run with `bash tests/trakt_episode_identity_512.sh`; use `SANITIZE=1` for
ASan/UBSan. Tests are kept in a separate commit from eventual product changes.

Official Trakt contracts checked at commit
`643dfe4ae9b3c6c49657f8fd930a633d3d2a32c4`:

- `projects/api/src/contracts/scrobble/schema/request/episodeScrobbleRequestSchema.ts`
  accepts `{ "episode": { "ids": ... }, "progress": ... }`.
- `projects/api/src/contracts/_internal/request/idsRequestSchema.ts`
  accepts a positive episode `tvdb` ID as the required episode identity.
- `projects/api/src/contracts/sync/schema/response/episodeProgressResponseSchema.ts`
  embeds `episodeResponseSchema`, whose `ids` include optional `tvdb`.

Public source: <https://github.com/trakt/trakt-api/tree/643dfe4ae9b3c6c49657f8fd930a633d3d2a32c4/projects/api/src/contracts>
