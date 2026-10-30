# Frost flicker investigation

Worktree baseline: e79037376, branch agente/21-21-frostpisca.
Evidence directory: `/Volumes/ExternalSSD/tmp/rev/frostpisca/`.

No production fix is claimed. The reported Frost flicker has not been reproduced
on the host. A failing-before/passing-after regression remains outstanding.
The final diagnostic run still caught a Glass-off Home difference (maxdelta=99,
4441 channel samples), so the whole matrix is not a passing acceptance gate.
Real-time artwork/fade completion can still enter a frozen-clock measurement;
this harness needs stronger settling before it can serve as a reliable gate.

`bash tests/frost_stability.sh` renders the real Home, Detail and Library screens
at 1920x1080 with offline artwork. It checks 12 consecutive frames for each of
three renderer effect levels, Glass on/off, before and after a focus event.
The glass quality setting is forced to High. The effect levels are full, light
and minimal; they are not three distinct Frost quality settings in this revision.
The harness advances the production update functions while settling focus, then
holds the test clock/state during pixel comparison. The acceptance threshold is
at most two LSBs and one hundred changed channel samples across eleven full-HD
frame comparisons; exact matches are reported as maxdelta=0 and changed=0. It alternates cache reuse
with explicit glass invalidation to compare cached and recaptured pixels.
This checks settled rendering, not every intermediate frame during navigation.
Four-frame strips are written for full effects with glass enabled.

Earlier live-clock runs crossed Home hero rotation and unfinished transitions,
including with Glass off. Those failures are not evidence of a Frost defect.
The `before` and `after` directories contain investigation captures of the same
production renderer, not a demonstrated visual fix.

Bounded SSH reads saved `lg.log` and `lg-start.log` from the specified jail log
without recursively traversing the jail. The running package identifies itself
as version 2.0.8, `Nuvio Legacy (b4869f28 high cache)`, on Mali-G71. This does not
confirm the reported 2.1 installation. The sampled interval reports zero texture
evictions. Bounded ADB logcat is saved in `tcl.log`; the explicit serial was
`192.168.1.34:45067` because two connections refer to the same television.
No device state, installation or application data was changed.

Compile commands used external-SSD TMPDIR and ccache with the requested
NUVIO_PROPERTIES path. Host and webOS high-cache compilation succeeded. Android's
first attempt stopped before native compilation because the default dependency
cache lacked libcurl.so. An isolated SSD cache reused the preserved native
libraries from `/Volumes/ExternalSSD/nuvio-build-170-preservado/android/jnilibs`.
The recovered Android debug build succeeded for both ABIs (Gradle BUILD
SUCCESSFUL, 41 tasks). No package was installed.
The reproducible per-target entry point is `bash tests/frost_build.sh host`,
`android` or `webos`; it never deploys.

Remaining work: reproduce the affected device/build and exact settings, capture
actual flicker, isolate its cause, implement a minimal fix, and demonstrate a
real failing-before/passing-after test including moving-focus image continuity.
