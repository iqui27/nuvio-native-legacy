# Reddit reply draft: hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (2026-10-09)

Not posted. For the owner to review.

---

Thanks a lot for such a detailed review, it helped a lot. I went through all of it against the code. Short answers, in your order. Please update to 2.0.3 first (it is out now); a good part of this is already in it.

1. Home rows: the 40-row limit is a deliberate memory cap on every TV. We have a version that lifts it on Android only (up to 200), planned for 2.0.4. On the catalog order, I need a log to see what the app got from your account.
2. Continue "Episode thumbnail" showing the backdrop: fixed in 2.0.3.
3. Continue with source Nuvio showing 5-6 and then 1: 2.0.3 changed how the "up next" is filled from the account, but each source also has a cap of 12 items, so I can't say yet. Log please.
4. Old watched items with Trakt + Simkl and source Nuvio: 2.0.3 drops episodes already watched elsewhere and series idle for 60+ days. If you still see them on 2.0.3, log please.
5. Upcoming seasons showing once and vanishing: we have a lead (the row is built before the account's watched list arrives), not confirmed. Log from 2.0.3 please.
6. Library 213 (Trakt) and 483 (Simkl): 2.0.3 reads the Trakt watchlist page by page, but caps it at 400 (Simkl at 300). A higher cap for Android is being considered for 2.0.4.
7. Watched badge on series: you are right. A series only gets the badge after you open its page. Marking it from the poster list is planned for 2.0.4.
8. Trailers starting small and then filling the screen: I could not find a cause yet; planned to be looked at for 2.0.4, a log helps.
9. Hero trailer sound: with the focus on the hero the trailer uses the "hero sound" setting; a trailer started while a poster is focused always plays muted. Whether that should follow the setting too is open for 2.0.4.
10. Hero trailer plays only once: this is on purpose (one trailer per title per session, so it does not loop every time you come back). We are discussing a switch for it in 2.0.4.
11. Dark posters / shine animation: there is no separate switch today. "Reduced animations" turns the shine off, and the depth effect can be turned off in Settings. A proper shine toggle and a brightness option are planned for 2.0.4.
12. Badge manifest URL: I need the URL (or the kind of JSON it serves) and a log. The app also only reads the account's badges when the account has them in its settings blob.
13. One Jellyfin / one Emby server: a limit for now (one of each per profile). Several servers is on the idea list for 2.1.
14. Jellyfin/Emby in the source lists (allowed add-ons, order): they are not part of those lists today, their sources always come first. On the idea list for 2.1; tell me what order you would like.
15. Watched movies with Resume instead of Play: fixed in 2.0.3.
16. Subtitle style only for the second subtitle: the main one is styled in the player sheet, not in Settings; a Settings entry is planned for 2.0.4. The automatic sync (other than audio) only works with external subtitles that have an embedded text track to compare with, not embedded subtitles. Which subtitles were you using?
17. One Xtream account: a limit today (one per profile). On the idea list for 2.1.
18. IPTV VOD: not supported, the app only reads live channels from Xtream. Not planned yet.
19. Live TV only UK/US: before 2.0.3 the guide cut at 900 channels and 48 categories on every TV, which can leave out whole countries. 2.0.3 raises it on Android to 3000 channels and 128 categories and shows "Showing N of M channels". Please update and tell me if it persists, with a log.
20. Live TV fps 24-50 (33, 34, 48): I need a log taken while a live channel plays, with the performance meter on.
21. Channels flashing green: log please, and tell me which channel it happens on.

For items 3, 4, 5, 19, 20 and 21, please send a log: Settings > About > Send log, then post the code here. Thanks again!
