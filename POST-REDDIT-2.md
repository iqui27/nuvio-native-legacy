# Rascunho do segundo post — NÃO PUBLICADO

Escrito para você revisar e postar. Eu não publico nada.

**As imagens estão em `~/Desktop/nuvio-post-2/`** (51 PNG: 30 em 1920×1080, 20 em
3840×2160 e um recorte de 540×220; interface em inglês, capturadas do próprio
app — build do Mac com a sua conta, mais as duas da home e o recorte, que vieram
da C9; as da parte Social vêm do teste com dados de mentira, porque a sua conta
ainda não tem recomendação real). A lista com legenda está no fim; a ordem sugerida para a galeria é a
numeração dos arquivos. O Reddit aceita até 20 imagens numa galeria: a seleção
marcada com ★ é a que eu usaria — hoje ela tem 24, ver a nota na tabela.

**Antes de postar:**

1. **webOS 3 — está confirmado, e o texto agora diz isso.** Você me passou que
   o **Mane155 testou numa webOS 3.4.3 e está tudo certo**. Essa confirmação
   veio por fora do GitHub (não há comentário dele em issue nenhuma — procurei),
   então quem sustenta a frase é você. Escrevi como relato de testador, em
   primeira pessoa sua. O que existe público e ancora a frase: os defeitos que
   ele achou viraram **quatro pré-releases em dois dias** (exp.1 a exp.4), e a
   exp.4 cita o aparelho dele.
   **O ponto do link resolveu-se:** não há mais download separado de webOS 3, é
   um `.ipk` só de 2016 a 2024, e o primeiro comentário perdeu o segundo link.
   **O que NÃO se resolveu é a evidência**, e o texto separa as duas coisas: o
   `webosbrew-ipk-verify` contra os dumps de firmware retail prova que o binário
   **carrega** naquele firmware (todo símbolo que ele usa existe lá), não que
   alguém assistiu algo numa TV de 2016. Quem sustenta "funciona" continua sendo
   o testador, e ele rodou as 1.0.x, não a 1.1. Se alguém perguntar no post,
   essa é a resposta honesta.
1b. **A v1.1.0 está publicada**, com os quatro pacotes:
   `space.nuvio.native.legacy_1.1.0_arm.ipk`, o `-highcache.ipk`,
   `NuvioTV-1.1.0-tizen.wgt` e `NuvioTV-1.1.0-highcache-tizen.wgt`. A seção
   "What landed in 1.1" descreve exatamente essa build, então o post já pode
   sair. Abra o link de "Latest release" do primeiro comentário antes de postar
   e confira que os quatro estão lá — é o único jeito de o post não apontar para
   o que ninguém consegue instalar.
1c. **O portal IPTV é o item mais arriscado do bloco da 1.1: eu nunca o testei
   contra um servidor de verdade**, não tenho um. O texto diz isso com todas as
   letras, e essa frase não é humildade — é o que separa "anunciei uma
   funcionalidade" de "anunciei uma funcionalidade que não funciona". Se você
   conseguir testar com um portal antes de postar, troca o parágrafo. Se não,
   ou deixa a frase como está ou tira o item.
1d. **Sobre anunciar o portal IPTV em público, que é decisão sua e não minha.**
   Eu te disse antes de implementar que não faria: portal Stalker com MAC
   forjado é, na prática esmagadora, revenda de assinatura pirata, e o post
   passa a descrever o app também como cliente de IPTV — é assim que ele vai
   ser lido em r/webos e r/LGOLED, e é assim que a LG leria. Você decidiu
   fazer, está feito e entra na 1.1. Mas a decisão de **anunciar** é separada
   da de construir, e essa ainda está aberta. Tirar o item do post não desfaz
   nada do código.
2. **Links fora do corpo**, no primeiro comentário — mesma razão do post
   anterior (filtro do Reddit). Lista no fim.
2b. **Imagem embaixo de cada trecho**: dá, mas só no editor novo do Reddit
   ("Rich Text", não Markdown). Escreva o parágrafo, tecle Enter e arraste o
   PNG ali — cada imagem vira um bloco entre os parágrafos. Os marcadores
   `[IMAGEM: arquivo.png]` no corpo abaixo dizem onde cada uma entra; apague o
   marcador depois de soltar a imagem. Se o subreddit só aceitar post de texto
   simples, o plano B é a galeria com as ★ e as legendas da tabela do fim.
3. **O corpo do post cita três downloads** — o `.ipk`, o `.wgt` e as variantes
   alto-cache — e os quatro arquivos da 1.1.0 cobrem os três. Não há mais um
   quinto para webOS 3 (nota 1).
4. **Onde:** r/webos, r/LGOLED, r/Nuvio. Ler a regra de autopromoção de cada um.

---

## Título (escolha um)

- Native C port of a webOS streaming app, twelve days later: 2016 sets through the 2024 ones, Samsung Tizen, and a build that sizes its own memory to the TV it lands on
- Update on the native (C/SDL2) webOS streaming app: a 2016 set is running it, there is a Samsung build, and it remembers which source you picked
- What 280 commits and 58 issues taught me about writing a TV app in C

---

## Corpo

Twelve days ago I posted a native C/SDL2 port of a webOS streaming app, tested
on one rooted 2019 C9. Since then: 291 commits, 57 releases, 59 issues opened
by people here and 55 of them closed. This is what changed, with screenshots,
and what I learned along the way. Links in the first comment.

**Where it runs now — one build, every generation**

- **webOS 3 (2016–2017)** — working, confirmed on a real set. There was almost
  no porting to do: I compared the binary's undefined symbols against
  webosbrew's retail firmware dumps and exactly *one* of 199 was missing on
  webOS 3 (`SDL_CreateRGBSurfaceWithFormat`, SDL 2.0.5). A five-line shim built
  on two calls present since SDL 2.0.0 replaced it, and one optional audio
  symbol stopped being fatal. But passing a symbol check is not the same as
  working, and I do not own a 2016 set — so this is here because a tester put
  it on a **webOS 3.4.3** set, reported back that it runs, and the bugs he did
  find turned into four builds in two days. As of 1.1 it is no longer a separate
  download: the same `.ipk` installs from 2016 to 2024. What I can prove about
  that package is that it *loads* — `webosbrew-ipk-verify -S -d -r ">=3,<4"`
  says All OK against the retail firmware dumps, meaning every symbol it uses
  exists on that firmware. What it does not prove is that anyone watched
  something on a 2016 set, and the tester's report is from the 1.0.x builds, not
  this one.
- **webOS 4** — measured here, still the reference set: 60 fps, worst frame
  under 20 ms.
- **webOS 5, 6 and newer, including the 2024 sets** — working, reported by
  users. My own README said the opposite for a while and it was wrong: the
  video path used to need `libAcbAPI`, which LG removed in webOS 5, but the app
  already fell back to SDL's exported-window API. Nobody had updated the table.
- **Samsung Tizen** — the same C compiled to WebAssembly inside a `.wgt`, video
  through AVPlay. Users on 2020+ sets (AU7000 among them) supplied most of the
  performance reports below.

[IMAGEM: 01b-home-rows.png]

One binary, every LG generation from webOS 3 up. The one thing that has to
differ between a 2016 set with 624 MB of RAM and a 2024 set with 3 GB is how
much artwork it is allowed to keep decoded in memory, and **the app decides
that at startup by reading the TV's own RAM** instead of shipping one number
and hoping:

```
< 800 MB   →  48 MB    a 2016 set: one report of 624 MB TOTAL
< 3 GB     →  128 MB   the C9, measured: home sits at 44–50 MB hot, RSS 255–278
>= 3 GB    →  192 MB   C1/C2/C3 and newer
```

On Samsung there is no `/proc/meminfo` to read, so it asks the browser
(`navigator.deviceMemory`) and falls back to 96 MB when the browser will not
say — which is the number the app always used. **Those Tizen tiers are not
measured**: there is no Samsung set on this bench, and I would rather write
that than pretend.

Two numbers make the difference concrete. A poster decoded at the 320 px cap
costs **363 KB**; the same poster in a list at 128 px costs **48 KB** — 7.6×
less. A 1080p backdrop is **8.3 MB**, the same art at 1280 is **3.7 MB**. On a
48 MB budget, one careless full-screen decode is a sixth of everything you
have.

For sets with RAM to spare there is a separate **high-cache** build for both
platforms with the ceiling fixed at 300 MB. It is a separate download on
purpose: a bigger cache on a set that cannot afford it does not run slower,
it gets killed. Settings shows a live graph of what the cache is actually
doing — used against ceiling, on-screen set, evictions, coloured by pressure —
so you can tell whether your set needs the other build instead of guessing.

[IMAGEM: 10-settings-images.png]

**What was added**

- Multiple profiles with server-side PIN and a "who is watching" screen at
  launch, asked once per session, not once per install

[IMAGEM: 17-profiles.png]
- Trakt and Simkl linked from the TV itself through their device-code flows;
  Continue Watching from either; a **Profile & Stats** page — hours watched,
  activity rhythm, most watched, genres — from your Trakt history

[IMAGEM: 06-profile.png]
- A **Saved** list on the blue button, with a **Social** tab: recommend a title
  to a friend from its page, add friends by code, see what they sent you

[IMAGEM: 16-social-mock.png]
[IMAGEM: 16c-recommend-what-to-say.png]
- **Live TV**: a guide with EPG for 768 channels in 35 categories, channel PiP,
  and sources chosen by probing the playlist instead of trusting the first URL.
  The focused channel takes the colour of its own logo background.

[IMAGEM: 03c-guide-focus.png]
- **Collections** from your account — streaming services, genres, themes,
  film series — with animated covers on focus

[IMAGEM: 18-collections.png]
- A **Home rows** sheet: reorder, enable, disable, choose the card shape, see
  which add-on each row comes from and what is queued behind the row limit

[IMAGEM: 09-home-rows.png]
- Add-ons managed on the TV, with what each one actually provides read from
  its manifest rather than assumed

[IMAGEM: 11-addons.png]
- Every setting the web app exposes, in the TV's language; the English table
  went from partial to complete across the issues

[IMAGEM: 07-settings.png]
- TMDB and MDBList integration: logos, cast with photos, parental guides,
  ratings, Trakt comments on the title page; a credits marker from TheIntroDB;
  "More like this" at the *end* of the film, not the start

[IMAGEM: 12-detail.png]
[IMAGEM: 12c-detail-down2.png]
- The app **remembers the source you picked** and uses the same one for the
  next episode — by the add-on's own `bingeGroup` when it sends one, by
  provider + audio-track signature when it does not — and the sources sheet
  now marks which one would be picked automatically

[IMAGEM: 14-sources.png]
- In-app update check with the release notes, and on LG sets with the Homebrew
  Channel it installs the update itself

[IMAGEM: 20-update.png]
- Focus is now a filled button in your accent colour instead of an outline —
  a request from a photo of my own TV that I agreed with once I saw it

[IMAGEM: 02-menu.png]
- A watched check on the poster, sized like the reference at last

[IMAGEM: 23-watched-badge.png]

- First-time cards that explain a feature the first time you meet it

[IMAGEM: 21-intro-guide.png]

**What landed in 1.1**

*(Everything in this block is version 1.1, which is on the release page now —
one `.ipk` for every LG generation and one `.wgt` for Samsung, plus the
high-cache variant of each. One item in it — the IPTV portal — has never talked
to a real server, and says so where it appears.)*

- **The Home opens on the hero, and the hero is a row now.** Left and right walk
  the first ten titles, with a **3 / 10** counter next to the title and a button
  that opens the title's page. Press down and the rows come back exactly where
  they have always been, with the block of text travelling down with them on the
  same spring. The automatic rotation used to walk the whole catalogue — 281
  titles on my account. Nobody could tell while nothing was counting; it becomes
  a lie the moment a "3 / 281" appears next to it. Both walk the same ten now.

[IMAGEM: 01-home.png]
- **What the hero shows is a setting.** It is the first line of the Home rows
  sheet (Settings → Home → Reorder), with left and right choosing between
  Automatic, Random from the catalog, and each row that is on the Home — pick a
  row and the hero shows that row's titles. It is per profile. A row that stops
  existing does not clear the choice, because the add-on may come back: the line
  says the row is unavailable and the hero falls back to automatic.

[IMAGEM: 09c-rows-spotlight.png]
- **The "Card" and "Size" columns of that same sheet draw the shape.** Six card
  names and three size names, and nothing said what any of them do — two of the
  six are the same landscape art at different sizes. There is now a strip of
  silhouettes at the measurements the code actually uses (212×322, 568×320,
  480×270, 360×203), at one scale and on one baseline, which is what lets you
  compare heights at all. Only the selected one is named, with a sentence
  underneath saying what it is.

[IMAGEM: 09d-rows-card-shapes.png]
- **Full-screen art asks for the full-size file.** The hero draws 1920 px wide
  and was being handed the URL sized for a card — TMDB's backdrop path arrives
  as w1280, so it was blown up 1.5×, which is what "the hero looks pixelated"
  actually was: the file is whole, it is just small for where it is being used.
  Measured with curl: TMDB w1280 (1280×720) has an `original` at 3840×2160;
  Trakt's `/medium/` (1280×720, 70 KB) has a `/full/` at 1920×1080, 155 KB;
  metahub's background is already 1920×1080, and medium, big, large and original
  are the same file byte for byte. When a title has no backdrop at all but has
  an IMDb id, that metahub URL can be built without asking anyone — the
  alternative there was a stretched poster, which is not lighter, only uglier.
- **A hero that is an episode shows that episode's still.** A "Continue
  watching" entry for a series is an episode, and the series art is the same for
  all ten seasons. The URL is deterministic and costs no lookup —
  `episodes.metahub.space/<tt>/<season>/<episode>/original.jpg`, measured at
  1920×1080. Not every episode has one; the TV log was already full of 404s from
  it, so a miss falls back to the title's art, once, without flickering. The
  Home screenshot above is one of these: that background is the still from S1
  E1, not the series backdrop.
- **Three picture settings: Low, Default and High.** What changes is the decode
  ceiling — how much source pixel a piece of artwork carries for the same
  drawing on screen — and the ceiling for full-screen art: 1280 on Low, 1920 on
  the other two. Measured here: the same cards that decode at 352/576/832/1056
  on Default go to 448/704/1056/1344 on High.

[IMAGEM: 07c-settings-image-quality.png]
- **A schedule.** Every show you follow, on a vertical time axis with today
  anchored at the top: weekday, day numeral, how long the wait is, and what the
  episode actually is. TMDB already returns the next air date, the episode
  synopsis, whether it is a season premiere or finale, the runtime and the
  network in the body the app was *already* downloading — so the whole screen
  costs **zero extra requests**. Shows that ended or were cancelled fall into a
  dashed tail instead of being given a fake date.

[IMAGEM: 25-agenda.png]
- **Reminders, and an honest sentence about them.** Neither webOS nor Tizen
  will wake a closed app, and this one has no background service. So the button
  says what actually happens: the reminder is marked, and the card appears the
  next time you open the app on that day. No push, and no pretending.

[IMAGEM: 25b-agenda-reminder.png]
- **Lists in the Library** — your Trakt lists, Simkl's five tracking states,
  your Nuvio collections, and a search over Trakt's public lists that works
  **without a Trakt account** because that endpoint only needs the app's client
  id. Any list can be pinned to the Library or added to the Home as a row, and
  a Trakt list added to the Home reuses the row machinery that already existed
  — no new network path. Card grid or compact list, remembered per profile.

[IMAGEM: 26-library-lists.png]
[IMAGEM: 26b-library-list-view.png]
- **Audience charts on a series page**, each with a label that says what it is
  not: a per-episode rating arc ("Trakt ratings — **not IMDb**"), a retention
  curve ("people who marked this episode, over people who marked E1 — **not
  general audience**") with the axis cropped because on a 0–100% axis every
  show is a flat line at the ceiling, and a four-axis fingerprint per episode.
  Retention is not clamped at 100%: one season here has an E2 with *more*
  markers than its E1, and the chart shows 100.3% rather than lying.

[IMAGEM: 28-series-charts.png]
[IMAGEM: 28b-series-fingerprint.png]
- **Quotes and a production sheet.** Wikidata gives the IMDb id → Wikiquote
  page link in one query, and the same query returns budget, box office,
  filming locations, source work and awards. Measured coverage, because it
  matters: **11 of 14 films** had usable quotes, **2 of 12 series** — most
  series pages are indexes, not dialogue. The sheet is labelled "fields from
  Wikidata, not trivia written by us", because IMDb-style trivia has no API and
  inventing it would be worse than not having it.

[IMAGEM: 29-quotes.png]
- **In-app self-update with a real progress bar** on LG sets with the Homebrew
  Channel, plus a button in Settings for people who dismissed the card.

[IMAGEM: 20-update.png]
- **A Social tab that asks before it shows you to anyone.** First time in, the
  whole tab is one question — "appear to other people?", with **No** as the
  first focusable answer — and it spells out that other people would see your
  name and photo and *not* what you watch, saved or recommended. The default
  in the database is invisible; nobody already registered became visible
  because the column exists.

[IMAGEM: 27-social-consent.png]
[IMAGEM: 27b-social-suggestions.png]

- **You can choose the source now.** A tester made the argument better than I
  would have: two sources for the same title differ in resolution, video codec
  and audio track, and that choice belongs to whoever is watching. Resume
  already opened the source sheet; Play never did. It is now a setting, off by
  default, because being asked on *every* playback is its own kind of tiring.
  Live channels stay out of it — a sheet between one zap and the next is the
  opposite of what live TV wants.

[IMAGEM: 08-settings-categories.png]
- **Switching profiles actually switches Continue Watching.** It did not. The
  row is rebuilt by one function called from exactly two places, one of them
  guarded by "did the sync bring anything new?". Returning to an already-synced
  profile brings no news, so the guard was never true and the previous
  profile's titles stayed on screen. Reported by a tester who thought he was
  describing a sync bug; it was a refresh that never fired.
- **A live channel that freezes now gets replaced.** The watchdog only ever
  noticed a source that failed to *open* — its deadline stops counting the
  moment playback starts. A source that opens and then stops delivering raises
  neither an error nor a pause, so the picture froze and nothing noticed. The
  signal was already arriving and being written to a log line nobody reads: the
  player pipeline reports buffering start and end. Twelve seconds of that on a
  live channel and it moves on.
- **Three things that were wrong on screen.** Two Settings options — "Corner
  radius" and "Memory used by images" — belonged to no category, so nothing drew
  them, while they still took focus and still drew their own help panel on the
  right. That is what the photo of my TV shows: a panel on the right and no row
  beside it on the left. The size of each category was written by hand and had
  drifted from the enum by one; it is read from the enum now. Second, drawing
  the value of an option whose value fell outside its own list read past the end
  of that list and crashed — the screenshot harness found that one. Third, more
  Portuguese leaked into the English interface: the month and the genre on the
  Profile page, the type label that comes from Trakt ("Programa de TV · 2025 ·
  51 min", under the hero) and two of the D-pad hints in Settings. Same cause
  every time — a string composed from a function's return value never matches a
  translation key, and the check that catches these only sees literals.
- **IPTV portals (Stalker/Ministra), and a plain warning about it.** Several
  people asked. It speaks the set-top-box protocol directly — MAC handshake,
  session token, channel list paged by genre — and the channels land in the
  same TV guide as everything else, with the same EPG matching and the same
  CH+/- zapping. Two details decided the design: the playback link a portal
  hands out is good for minutes, so it is never stored anywhere and every
  playback asks for a new one; and the MAC is a credential, so it lives in a
  per-profile file, is masked on screen, never reaches a log, and is erased
  when you sign out. **I have not tested it against a real portal — I do not
  have one.** It compiles and the protocol is implemented end to end, and that
  is all I can honestly claim until someone runs it.

*One thing I tried and threw away:* linking a famous line to its timestamp so
you could jump straight to that moment. The pieces exist — the app already
parses subtitles into cue/timecode pairs — so I measured it: matching
Wikiquote lines against the English subtitle hit **13% to 56%** depending on
the film, and a timecode is only valid for the exact release that subtitle was
timed to. A button that lands in the wrong scene half the time is worse than
no button.

**What I learned, in case you write one of these**

- *Artwork flicker was the eviction policy, not the network.* The texture cache
  was pinned at its ceiling and its LRU had no idea which textures were on
  screen — the first card of the first row is always the least-recently-
  touched *visible* texture, so it was the first out, then re-decoded, then
  out again. Every focus move also loaded an 8 MB 1080p backdrop that pushed
  twelve posters out. Fix: never evict what was drawn in the last two frames;
  evict cold full-screen art and animation frames before any poster; keep the
  old texture while a bigger version decodes; size the budget from the RAM.
  Evictions of on-screen art went from dozens per second to zero, and the
  graph turns green instead of red.

[IMAGEM: 10c-settings-images-pressure.png]
- *`SDL_BlitScaled` is nearest-neighbour.* Its docs say otherwise. Downscaling
  a 1920 backdrop to 1280 dropped one column in three; the GPU then stretched
  it back with linear filtering, so the result was both aliased and blurry.
  That was the "washed out" report. Area-averaging by hand fixed it.
- *The GIF that "plays for a second then freezes" on Tizen.* SDL_image in
  Emscripten cannot decode GIF; the cache treated an undecodable file as
  corrupt and deleted it; the browser-side animator lost its file; the cache
  downloaded it again; and the 2 s / 10 s retry backoff set the rhythm of the
  flicker. The log showed it in three lines. My first two hypotheses, made
  without the log, were wrong.
- *A Trakt 403 was a missing User-Agent header.* Not credentials.
- *The hero backdrop that took 12 seconds* was queued FIFO behind seventeen
  thumbnails. Full-screen art now jumps the queue.
- *On a TV, `key.repeat` means nothing.* The firmware sends a held button as
  separate key-downs. Anything that toggles on key-down needs its own debounce.
- *Measure inside the app.* The FPS line in the log carries the on-screen
  texture set, evictions, process RSS; the red button opens the log panel.
  Every fix above started from a number someone pasted from that panel.

**Still true**

Only my own set is rooted. Installs through Developer Mode or the Homebrew
Channel are reported working, including video, but I have not measured an
unrooted install myself. The `.ipk` is 35 MB and the `.wgt` 23 MB, nearly all of
it bundled artwork so the home
screen has something before you sign in. It is called **Nuvio** on the TV now —
it used to install as "Nuvio Legacy Native" on webOS — and the id inside the
package did not change, so 1.1 updates over what you already have. GPLv3,
unofficial, not affiliated with NuvioMedia.

If you run it on something I do not have — webOS 3, webOS 6, any Samsung —
the red button opens the log panel, and a photo of it is worth more than a
description.

---

## Primeiro comentário (links)

- Code: https://github.com/iqui27/nuvio-native-legacy
- Latest release — one LG `.ipk` (webOS 3 through the 2024 sets) and one Samsung `.wgt`: https://github.com/iqui27/nuvio-native-legacy/releases/latest
- High-cache builds (300 MB ceiling, for sets with RAM to spare): same release page, the two files with `highcache` in the name
- Install guide: https://github.com/iqui27/nuvio-native-legacy/blob/master/INSTALL.md
- Web (JavaScript) fork, for anything else: https://github.com/iqui27/NuvioTVSmart-legacy-webos

## Imagens (`~/Desktop/nuvio-post-2/`) e legendas

★ = as que eu poria na galeria, nesta ordem. **Atenção: são 24 e o Reddit aceita
20** — o texto dizia "14" mas a tabela já vinha com 20 marcadas antes de eu
mexer, e eu marquei mais quatro (01-home, 07c, 09c, 09d), que são a cara da 1.1.
Quatro têm de sair e a escolha é sua; se for para eu chutar, tiraria
12c-detail-down2, 15-saved, 16-social-mock e 23-watched-badge — as quatro que
menos dependem de estar em tamanho grande.

| arquivo | legenda sugerida |
|---|---|
| ★ 01-home.png | Home as it opens: the spotlight in focus, "3 / 10", "View title", rows pushed down — and the art is that episode's still (C9) |
| ★ 01b-home-rows.png | The same Home after pressing down: rows back where they always were, focused card grows — no ring (C9) |
| ★ 02-menu.png | Side menu, focus as a filled button in the accent colour |
| ★ 03c-guide-focus.png | TV Guide: 768 channels in 35 categories, EPG on now / up next for the focused channel |
| 03-guide.png | Guide at the top: category sections |
| 03b-guide-epg.png | Guide further down, programme bars |
| 04-search.png | Search with the on-screen keyboard |
| 05-library.png | Library: Saved and Collection tabs, type/sort filters |
| ★ 06-profile.png | Profile & Stats from Trakt: hours, rhythm, most watched, genres |
| 07-settings.png | Settings: focused row filled, categories on the left |
| ★ 07c-settings-image-quality.png | Settings → Posters and cards: "Image quality — Default", with what the three levels do on the right |
| 08-settings-categories.png | Settings with focus on the category column — "Choose the source on Play" is the last row of Playback |
| ★ 09-home-rows.png | Home rows sheet: order, enable, card shape, where each row comes from |
| 09b-home-rows-out.png | Rows not on Home, grouped by add-on |
| ★ 09c-rows-spotlight.png | "Spotlight on top" is the first line of the sheet: ← → change what the Home spotlight shows |
| ★ 09d-rows-card-shapes.png | The Card column in focus, with the silhouettes at the bottom right: each card shape at its real measurements |
| ★ 10-settings-images.png | Image cache panel: used vs ceiling, on-screen set, 2-minute graph in pressure colour (green) |
| 10c-settings-images-pressure.png | The same panel with the cache pinned at the ceiling (amber → red) — what the old builds looked like all the time |
| 11-addons.png | Add-ons: what each provides, read from its manifest |
| ★ 12-detail.png | Title page: logo, actions, metadata badges |
| 12b-detail-down.png | Seasons and episodes with Trakt ratings |
| ★ 12c-detail-down2.png | Cast & crew, Trakt comments (TV Show / Episode) |
| 13-player.png | Player controls (video plane not capturable — see note) |
| ★ 14-sources.png | Sources sheet: provider filters, badges, "Now playing" / automatic pick |
| ★ 15-saved.png | Saved panel on the blue button |
| ★ 16-social-mock.png | Social tab: what friends sent you, who and when, their line (mock data) |
| 16b-recommend-to-whom.png | Recommend a title: pick the friend |
| 16c-recommend-what-to-say.png | Recommend a title: pick a line |
| 16d-social-arrival-card.png | The card that shows up when a recommendation arrives |
| ★ 17-profiles.png | Who is watching — per session |
| ★ 18-collections.png | Collections: streaming services with animated covers |
| 18b-collections-genres.png | Collections: genres and themes |
| 19-collection-open.png | A collection opened: catalogues by service |
| 21-intro-guide.png | First-time card: TV Guide |
| 22-intro-social.png | First-time card: the Social tab |
| ★ 23-watched-badge.png | Watched check on the poster (crop from the C9) |
| 24-context-menu.png | Long-press menu on a card |
| ★ 25-agenda.png | Schedule: every show you follow on a time axis, today anchored, dated tail for the ones that ended |
| 25b-agenda-reminder.png | The reminder button on a title — only the clock, green when armed |
| ★ 26-library-lists.png | Library → Lists: your Trakt lists, Simkl states, Nuvio collections |
| 26b-library-list-view.png | The same library as a compact list instead of a card grid |
| 26c-library-trakt-search.png | Searching Trakt's public lists — works without a Trakt account |
| ★ 27-social-consent.png | The first thing the Social tab asks, with "No" as the first answer |
| 27b-social-suggestions.png | Suggestions, each saying where it came from |
| ★ 28-series-charts.png | Per-episode rating arc and the retention curve, both labelled for what they are not |
| 28b-series-fingerprint.png | Four-axis fingerprint of one episode |
| ★ 29-quotes.png | Quotes from Wikiquote beside the Wikidata production sheet |
| ★ 20-update.png | The update card, with the progress bar during a self-install |

Nota sobre 13-player.png: o vídeo vive num plano de hardware atrás do GL e não
sai em captura nenhuma (nem na TV); a imagem mostra só os controles, e no Mac
não há pipeline de vídeo, por isso o aviso "Could not open the source" no meio.
Se quiser mostrar reprodução, é foto da TV.

**AS IMAGENS DAS FEATURES NOVAS JÁ ESTÃO EM `~/Desktop/nuvio-post-2/`**:
25, 25b, 26, 26b, 26c, 28, 28b e 29 — 1920×1080, interface em inglês, geradas
pelos harnesses de captura do repositório.

As duas que faltavam já estão na pasta: **27 e 27b (Social)**, de 17/09 11:34 —
depois que o botão de "aparecer para outras pessoas" virou interruptor — e
**20-update.png**, de 17/09 08:53.

**Duas coisas que a geração destas capturas descobriu**, e as duas viraram
conserto no app, não só na foto:

1. Os harnesses escreviam a interface em **português** e não havia como trocar
   sem recompilar. Agora `NUVIO_SHOT_EN=1` troca para inglês — recompilar para
   mudar de língua é o tipo de atrito que faz alguém publicar a captura errada.
2. A nota do IMDb saía com **vírgula decimal cravada** nos três lugares que a
   desenham (`biblioteca.c`, `detail.c`, `recomenda.c`). Em inglês "8,4" não
   lê como um número com uma casa: lê como milhar interrompido. Agora segue o
   idioma. Só apareceu porque o álbum foi gerado em inglês.

O conteúdo de ensaio dos harnesses (nomes de lista, títulos de filme, sinopse)
também passou para inglês. Não é interface: é DADO, e numa lista pública do
Trakt o nome vem de quem a criou. Com a interface em inglês e o conteúdo em
português a captura parece defeito para quem lê o álbum.

**AS CAPTURAS DA 1.1 ENTRARAM.** `01-home.png` e `01b-home-rows.png` foram
refeitas na C9 com a build final e são o par do gesto (destaque em foco → baixo,
fileiras no lugar); `09c` e `09d` são a folha de fileiras; `07c` é o "Image
quality". A escolha de fonte já aparecia em `07-settings.png` e
`08-settings-categories.png`.

**O que continua sem foto:**

- **Ajustes › Conta com o portal IPTV** — as três linhas, com o portal e o MAC
  já mascarados. **Confira a máscara na própria captura antes de postar**: a
  tela mostra só os dois últimos octetos, mas se a captura sair de uma TV com
  um portal real configurado, é o endereço dele que aparece ali.
- O guia com canais de portal misturados aos dos addons, se você chegar a
  configurar um.
- A arte de tela cheia em resolução nova: só vale como par antes/depois, e o
  "antes" não existe mais no binário. (O still do episódio já está mostrado —
  é o fundo de `01-home.png`.)

Três coisas da 1.1 não rendem captura — troca de perfil, travamento de canal e o
segfault de Ajustes são defeitos que sumiram, e a única imagem possível seria a
do defeito.

Também não capturado: o PiP de canal (precisa de vídeo, que não existe no
build do Mac).

## Números usados no texto, e de onde vieram

- 291 commits / 57 releases: `git rev-list --count v1.0.2..HEAD` = 291 e `gh release list` = 57 linhas, ja com a v1.1.0 (cinco delas sao pre-release, as exp. de webOS 3). Eram 280/56 no rascunho de ontem
- 59 issues, 55 fechadas, 4 abertas: `gh issue list --state all|closed|open` em 17/09, depois da v1.1.0. Eram 58/55/3 no rascunho
- 363 KB / 48 KB por arte (cap de 320 px contra 128 px) e 8,3 MB / 3,7 MB (backdrop 1920 contra 1280): medidos por `tex_estatisticas` e pelas contas em src/tex_cache.c
- Degraus 48/128/192 MB: orcamentoMB() em src/tex_cache.c, com o comentario dizendo qual foi medido e qual foi escolhido
- Degraus do Tizen: NAO medidos, nao ha Samsung aqui — esta escrito assim no post
- Cobertura do Wikiquote (11/14 filmes, 2/12 series) e casamento fala↔legenda (13% a 56%): medidos em titulos reais em 16/09
- webOS 3 em aparelho: confirmacao do Mane155 numa 3.4.3, passada por VOCE — nao ha comentario dele em issue deste repo (procurei em todas). O que e publico: as quatro pre-releases exp.1..exp.4 em 15-16/09, e o corpo da exp.4 citando o defeito do segundo perfil no aparelho dele.
- 768 canais / 35 categorias: `[guia] 768 canais em 35 categorias` no log do Mac hoje
- "60 fps, pior quadro < 20 ms": linha FPS do log da C9 hoje
- "dezenas por segundo → zero": `tex-despejos=46(q=…)` antes, `q=0` depois, mesmo log
- 48/128/192 MB: orcamentoMB() em src/tex_cache.c; 624 MB é o relato de RAM de uma webOS 3 no README
- 35 MB / 23 MB: os quatro pacotes da 1.1.0, medidos em 17/09 — `space.nuvio.native.legacy_1.1.0_arm.ipk` 36.519.936 bytes e o `-highcache.ipk` 36.519.820 (~34,8 MB cada); `NuvioTV-1.1.0-tizen.wgt` 23.640.074 bytes e o `-highcache-tizen.wgt` 23.639.603 (~22,5 MB cada). O corpo arredonda para cima, que e o que a pessoa ve na pagina da release
- webOS 3: `webosbrew-ipk-verify -S -d -r ">=3,<4"` All OK contra os dumps de firmware retail da webosbrew, no `space.nuvio.native.legacy_1.1.0_arm.ipk`. Isso prova que CARREGA (todo simbolo usado existe naquele firmware), nao que alguem assistiu algo numa TV de 2016 — o relato do testador e das 1.0.x. O post diz as duas coisas separadas
- Pacotes da 1.1.0: `space.nuvio.native.legacy_1.1.0_arm.ipk`, `...-highcache.ipk`, `NuvioTV-1.1.0-tizen.wgt`, `NuvioTV-1.1.0-highcache-tizen.wgt`. Os dois `.wgt` estao na raiz do repo, de hoje 11:52 e 12:24
- Nome "Nuvio" na TV e id inalterado: `deploy/app/appinfo.json` — `"title": "Nuvio"`, `"id": "space.nuvio.native.legacy"`, `"version": "1.1.0"`
- Escolha manual de fonte, troca de perfil e travamento de canal: commits 9d06940, 1adc7fa e 6dc4ece na master, 17/09 — compilam no Mac e no ARM. Entram na 1.1, publicada hoje
- Destaque como fileira, "3 / 10" e os 281 titulos do catalogo: commit 5941ddb, 17/09; `HOME_HERO_LISTA 10` em src/home.c
- Destaque configuravel na folha de fileiras (Automatico / Aleatorio / uma fileira) e as medidas das silhuetas 212x322, 568x320, 480x270, 360x203: commit 9e4d9bb, 17/09 — as medidas sao as de home.c
- Arte de tela cheia: medidas com curl em 17/09, anotadas em src/artehero.c — TMDB w1280 1280x720 -> original 3840x2160; Trakt /medium/ 1280x720 70 KB -> /full/ 1920x1080 155 KB; metahub background 1920x1080 e medium/big/large/original byte a byte iguais
- Still do episodio em 1920x1080: `episodes.metahub.space/<tt>/<T>/<E>/original.jpg`, medido em 17/09 (w780 da 780x439, w1280 da 1280x720). Nem todo episodio tem: o 404 ja aparecia no log da TV
- Tetos de decodificacao 352/576/832/1056 (Padrao) contra 448/704/1056/1344 (Alta): medidos no Mac nas mesmas telas, commit 28918f5 — a razao 1,28 e a das folgas 1,25 e 1,60
- Duas opcoes de Ajustes sem categoria, segfault do valor fora da lista e os vazamentos de portugues: commits af57a7b, a9b251f, d4e01b3, bce8e6f, 81216de e 28918f5, 17/09. As duas opcoes sem categoria vieram de uma FOTO da TV, nao de teste
- Portal IPTV (Stalker): commit fbf8027, 17/09. Entra na 1.1. Protocolo implementado do handshake ao create_link; compila nos dois alvos; a suite de testes do repo passa. NUNCA foi exercitado contra um portal real — nao ha um aqui. Isso esta escrito no corpo do post, e tem de continuar escrito.
