**Set it up on your phone, the TV follows**

You don't configure add-ons on the TV. AIOManager, Xperience and the Nuvio Configurator write to your Nuvio account, and our TV app pulls everything from that account: add-ons, collections, Home rows and stream badges.

**How**

1. Sign in on the TV with your Nuvio account.
2. On your phone, use one of:
   - AIOManager — install/reorder/enable add-ons per profile
   - Xperience — collections, Home layouts, badge packages ("Push to Nuvio")
   - Nuvio Configurator (third-party) — generate nuvio_collections.json to import into your account
3. Open the app on the TV. It syncs within ~5 minutes, or go to Settings → Sources and add-ons → Sync add-ons to pull the list right away.

**What syncs today**

- Add-ons: install, order and enabled state, per profile (up to 128)
- Collections: same schema as the official app, art included
- Home: catalog order from the account
- Stream badges: up to 3 packages from the account
- Service keys (debrid, TMDB, MDBList, Trakt): picked up from the account

**Two things to know**

- The add-on list is one-way: account → TV. Toggling an add-on on the TV only lasts until the next sync. Make permanent changes on the phone.
- Add-on configuration URLs are secrets (they can contain your debrid key or a password). Never post a full config URL, UUID or token here — name the add-on and show only the host.

**Subtitles:** we recommend SubMaker — one add-on aggregating OpenSubtitles, SubDL, SubSource and Wyzie, with AI translation. The TV sends the file name, size and OpenSubtitles hash, so hash-matched results work.

Some paths above are implemented but not yet confirmed end-to-end on a fresh test account; they're marked in the full guide (docs/guia/phone-setup.md). Feedback welcome.
