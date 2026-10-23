# Deep links

Other apps, launchers, Google TV and `adb` can open content in Nuvio Legacy.
Parser: `src/deeplink.c` (test: `bash tests/deeplink.sh`).

| URL | Opens |
|---|---|
| `nuvio-legacy://meta/{movie\|series}/{id}` | title page |
| `nuvio-legacy://play/{movie\|series}/{id}` | title page, then Play |
| `nuvio-legacy://play/series/{id}/{season}/{episode}` | title page, Play on that episode |
| `nuvio-legacy://search?q=text` | Search screen with `text` |
| `stremio:///detail/{type}/{id}[/{videoId}]` | title page (`videoId` ignored) |
| `https://[www.\|m.]imdb.com/title/tt...` | title page |

`{id}` is an IMDb id (`tt0111161`), a TMDB id (`603`) or `tmdb:603`.
`type` is `movie` or `series` (`tv`/`show` accepted); it only matters for TMDB ids.
`q` is URL-encoded (`+` and `%XX`), cut at 95 bytes.

Rules: URLs over 512 bytes, unknown hosts/paths, non-numeric or out-of-range
season/episode, ids outside `tt\d+` / digits, empty `q` are ignored silently.
A link that arrives on the login or profile screen is kept until the app is in.
A link replaces whatever is open (player included); the newest link wins.
Play needs the title in the catalog (or already loaded): otherwise it only
opens the page. `stremio://` is also claimed by the Stremio app if installed.

## Android

Intent filters (VIEW, DEFAULT+BROWSABLE) live on `NuvioActivity` (not on the icon
`activity-alias`es, which are launcher-only). Cold start reads the intent in
`onCreate`, warm start in `onNewIntent` (`singleTask`); both call
`nativeDeepLink`, which posts to the C router.

```
adb shell am start -a android.intent.action.VIEW -d 'nuvio-legacy://meta/movie/tt0111161' space.nuvio.nativelegacy
adb shell am start -a android.intent.action.VIEW -d 'nuvio-legacy://play/series/tt0944947/1/1' space.nuvio.nativelegacy
adb shell am start -a android.intent.action.VIEW -d 'nuvio-legacy://search?q=the+matrix' space.nuvio.nativelegacy
```

## webOS / Tizen

TODO: not wired. webOS passes launch params (JSON, `relaunch` event / `webOSLaunch`)
to the page; the app should pass a `url` field to `deeplink_postar()`. The C side
is platform-neutral already.
