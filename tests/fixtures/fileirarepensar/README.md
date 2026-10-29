# Top Rated exploration fixture

Frozen public Cinemeta metadata and MetaHub artwork, downloaded for the native design studies. `catalog.json` records each IMDb identity and the local artwork paths; `catalog.inc` feeds the same values into `CatItem` without a network dependency.

Source metadata: `https://v3-cinemeta.strem.io/meta/series/<IMDb ID>.json`. Artwork: `https://images.metahub.space/{poster/small,background/medium,logo/medium}/<IMDb ID>/img`.

The first four titles and their order match the owner's screenshot: The Sopranos, The Wonderfools, Chernobyl, The Good Bad Mother. The remaining eight titles fill the original 12-item row; their order is a fixture, since the screenshot does not show them. The Sopranos synopsis is overridden in the harness with the wording visible in the reference.

These assets are review fixtures, not additions to the production artwork bundle. Some MetaHub responses use WebP bytes despite a JPG URL; SDL_image detects the actual format.
