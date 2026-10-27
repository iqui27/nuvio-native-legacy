# Sports #619 in guide v5

Goal: retain every community PR #619 capability inside the existing guide v5 visual system.

- Merge pr/619 to preserve Grant Barker's original commits. Resolve app, Sources, guide and language conflicts by retaining both branches' behavior.
- Reuse its bounded catalog, eight-league cache, combined team favorites, profile settings, venue artwork, logo handling and EPG matching. Expose the existing card and Sources helpers to v5 instead of duplicating matching.
- Keep v5 header, sidebar, controls, text scale, focus and transitions. Sports home opens News, today's/live games, Upcoming, Results, sport/league browsing and Favorites. Lists page through all entries; Back restores their opener. Settings expose sport/league visibility and image quality.
- Request news metadata through the existing bounded ESPN worker. Article details remain metadata; only matched configured IPTV channels enter Sources/playback.
- Verify original Sports suites plus a focused v5 integration gate and guide gates. Capture home, league, matched Sources and Favorites using offline fixtures. Run i18n scan and one final local build (900-second timeout). Never push, deploy or stash.
