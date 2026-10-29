# Limited Xtream connections

The Xtream account row offers Auto, On and Off. The value lives in the existing
`xtream-p<N>.txt` account file; Xtream credentials/accounts currently have no
cloud-sync representation. Auto enables the policy after a valid account reply
reports a positive maximum of one connection, or active connections at the
maximum. The decision remains enabled for that account session. Unknown or zero
maximum does not imply a one-screen allowance. Off retains the existing path.

On Android, limited playback uses a shared HTTP permit instead of parallel range
downloads, seek caching or playlist revalidation. A new player waits for prior
release; its HTTP source also waits for the previous upstream close. Retired
loaders check cancellation after acquiring the permit. Account status is checked
once per player while holding the same permit, before reading media. Full-account
status or HTTP 403/458/509 stops failover and displays the translated line-in-use
message. The Live TV error card offers Try again, using its existing reload action.
An HTTP-200 placeholder without a full-account signal cannot be identified by
content alone.

Limited accounts skip guide previews, mini-player, preloads, retained playback,
source verification, diagnostic stream probes, chapter probes, subtitle prefetch,
subtitle synchronization reads and seek probes. Native subtitles remain available.
The existing 600 ms zap debounce selects the final channel.

## Offline evidence

Run `bash tests/conexlim.sh` from the repository root. It checks account policy,
account persistence/reset, stream URL boundaries and the production C zap debounce.
It compiles the real Android player against cached Android/Media3 dependencies,
then exercises the real LimitedDataSource against a localhost HTTP server. The
unrestricted factory must fail with two concurrent connections; the limited
factory must pass with one, A then C, delayed close, account refusal, HTTP
403/458/509, canceled loaders and successful retry. No dependencies are downloaded.

`bash tests/conexlim_shot.sh` renders the real Settings row and error card at
1920×1080 to `/Volumes/ExternalSSD/tmp/rev/conexlim/` using isolated test data.

## Remaining validation and scope

No TCL/device installation or provider access was performed. Actual Media3
teardown and adaptive streams still need device validation. The host HTTP test
covers the transport wrapper and production zap debounce separately, not the
complete SDL/JNI/ExoPlayer application loop.

The HTTP permit and account preflight currently exist only in the Android
backend. webOS, Tizen and HTML5 need equivalent close acknowledgements and
serialized HTTP ownership before the cross-platform single-connection guarantee
is complete. Their shared UI/probe guards alone do not prove that guarantee.
