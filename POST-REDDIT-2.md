# Rascunho de post — NÃO PUBLICADO

Escrito para você revisar e postar. Eu não publico nada.

**Antes de postar, três coisas:**

1. **webOS 3.** Você me disse "agora funciona do webOS 3 até o último". Eu não
   escrevi assim. O que existe é: uma build que passa no verificador de firmware
   da webosbrew para webOS ≥ 2 e ≥ 3, publicada como pré-release, e **nenhuma
   pessoa confirmou numa TV webOS 3 de verdade** (a issue tracker não tem
   relato). O texto diz isso com essas palavras. Se alguém já te confirmou por
   fora, troque o parágrafo — mas "funciona" sem prova custa mais do que vale
   quando o primeiro relato é uma tela preta.
2. **Links fora do corpo**, no primeiro comentário — mesma razão do post
   anterior (filtro do Reddit). Lista no fim deste arquivo.
3. **Onde:** r/webos, r/LGOLED, r/Nuvio (se existir moderação que aceite). Ler a
   regra de autopromoção de cada um antes.

---

## Título (escolha um)

- Native C port of a webOS streaming app, 12 days later: Samsung Tizen, webOS 5+, an experimental webOS 3 build, and 54 releases of bug reports turned into fixes
- Update on the native webOS streaming app: now runs from webOS 5 back to 4, has a Tizen build, and a webOS 3 build nobody has tested yet
- What 236 commits and 57 issues taught me about writing a TV app in C (webOS 3 → webOS 5+, plus Samsung)

---

## Corpo

Twelve days ago I posted a native C/SDL2 port of a webOS streaming app, tested
on one rooted 2019 C9. Since then: 236 commits, 54 releases, 57 issues opened by
people here, 45 of them closed. This is what changed and what I learned. Links
in the first comment.

**Where it runs now**

- **webOS 4.x** — measured here, still the reference set. 60 fps on the home
  screen, worst frame under 20 ms.
- **webOS 5 and newer, including 2024 sets** — reported working by users. My
  own README said the opposite for a while and it was wrong: the video path
  used to need `libAcbAPI`, which LG removed in webOS 5, but the app already
  fell back to SDL's exported-window API. Nobody had updated the table.
- **webOS 3 (2016–2017)** — an experimental build exists. I compared the
  binary's undefined symbols against webosbrew's retail firmware dumps and
  exactly one of 199 was missing on webOS 3: `SDL_CreateRGBSurfaceWithFormat`,
  which arrived in SDL 2.0.5. A five-line shim built on two functions present
  since SDL 2.0.0 replaced it, one optional symbol in the audio path stopped
  being fatal, and `webosbrew-ipk-verify -r ">=3,<4"` says All OK. **Nobody on
  the project owns a webOS 3 TV and nobody has reported back yet.** "Passes the
  symbol check" is not "works"; if you have a C8/B7-era set, this build exists
  to find out.
- **Samsung Tizen** — the same C source compiled to WebAssembly inside a
  `.wgt`, with video through AVPlay. Tested by users on 2020+ sets (AU7000
  among them), which is where most of the performance work below came from.

**What was added**

- Multiple profiles with server-side PIN; a "who is watching" screen on
  launch, per session, not per install
- Trakt and Simkl linked from the TV (device-code flow); Continue Watching
  from either; watched state per episode kept on the TV
- A Saved list on the blue button, with a Social tab: recommend a title to a
  friend from the title page, add friends by code
- Live TV: a guide with EPG, channel PiP, sources chosen by probing the
  playlist instead of trusting the first URL
- Collections from the account (TMDB, Trakt lists, addon catalogues), with
  animated covers on focus
- A Home rows sheet: reorder, enable, disable, choose the card shape, see which
  addon each row comes from and what is queued when the row limit is hit
- Settings for everything the web app exposes, in the TV's own language,
  with an English table that went from 60% to 100% coverage over the issues
- TMDB and MDBList integrations (logos, cast, parental guides, ratings), a
  credits marker from TheIntroDB, "More like this" at the *end* of the film
- The app remembers the source you picked and uses the same one for the next
  episode — by the addon's own `bingeGroup` when it sends one, by provider +
  audio-track signature when it does not. The sources sheet now marks which
  one would be picked automatically.
- In-app update check; 4K UI as an option, off by default
- Focus is now a filled button in your accent colour instead of an outline —
  a request from a photo of my own TV that I agreed with once I saw it

**What I learned, in case you write one of these**

- *Artwork flicker was the eviction policy, not the network.* The texture cache
  was pinned at its 96 MB ceiling and the LRU had no idea which textures were
  on screen — the first card of the first row is always the least-recently-
  touched *visible* texture, so it was the first out, then re-decoded, then out
  again. Every focus move also loaded an 8 MB 1080p backdrop that pushed twelve
  posters out. Fix: never evict what was drawn in the last two frames; evict
  cold full-screen art and animation frames before any poster; keep the old
  texture while a bigger version decodes. Then size the budget from the TV's
  RAM at startup (48 MB on a 624 MB webOS 3 set, 128 MB on the C9, 192 MB on
  3 GB sets) instead of one number for everyone. Evictions of on-screen art
  went from dozens per second to zero.
- *`SDL_BlitScaled` is nearest-neighbour.* Its docs say otherwise. Downscaling
  a 1920 backdrop to 1280 dropped one column in three; the GPU then stretched
  it back with linear filtering, so the result was both aliased and blurry.
  That was the "washed out" report. Area-averaging by hand fixed it.
- *The GIF that "plays for a second then freezes" on Tizen.* SDL_image in
  Emscripten cannot decode GIF, the cache treated an undecodable file as
  corrupt and deleted it, the browser-side animator lost its file, the cache
  downloaded it again, and the 2 s / 10 s retry backoff set the rhythm of the
  flicker. The log showed it in three lines; my first two hypotheses, made
  without the log, were wrong.
- *A Trakt 403 was a missing User-Agent.* Not credentials. Two hours.
- *The hero backdrop that took 12 seconds* was queued FIFO behind seventeen
  thumbnails. Full-screen art now jumps the queue.
- *On a TV, `key.repeat` means nothing.* The firmware sends a held button as
  separate key-downs. Anything that toggles on key-down needs its own debounce.
- *The measure has to be built in.* The FPS line in the log now carries the
  texture cache's on-screen set, evictions of on-screen art, and process RSS,
  and the Settings screen shows a live graph of it. Every one of the fixes
  above was found by a number someone pasted from that log, not by guessing.

**Still true**

Only my own set is rooted; installs through Developer Mode or the Homebrew
Channel should work but whether an unrooted install gets LS2 access to
`com.webos.media` is still not measured by me — reports say yes, I have not
seen it. The `.ipk` is 35 MB, all of it bundled artwork so the home screen
has something before you sign in. GPLv3, unofficial, not affiliated with
NuvioMedia.

If you run it on something I do not have — webOS 3, webOS 5+, any Samsung —
the red button opens the log panel, and a photo of it is worth more than a
description.

---

## Primeiro comentário (links)

- Code: https://github.com/iqui27/nuvio-native-legacy
- Latest release (webOS 4/5+ `.ipk` and Tizen `.wgt`): https://github.com/iqui27/nuvio-native-legacy/releases/latest
- webOS 3 experimental build: https://github.com/iqui27/nuvio-native-legacy/releases/tag/native-webos3-exp.1  ← **se você publicar a 1.0.56 da webos3, troque a tag aqui**
- Install guide: https://github.com/iqui27/nuvio-native-legacy/blob/master/INSTALL.md
- Web (JavaScript) fork for anything else: https://github.com/iqui27/NuvioTVSmart-legacy-webos

## Números usados no texto, e de onde vieram

- 236 commits / 54 releases: `git log v1.0.2..HEAD`, `gh release list` (v1.0.2 → v1.0.56)
- 57 issues, 45 fechadas: `gh issue list --state all` em 16/09
- "60 fps, pior quadro < 20 ms": linha FPS do log da C9 hoje
- "dezenas por segundo → zero": `tex-despejos=46(q=…)` antes, `q=0` depois, mesmo log
- 35 MB: `space.nuvio.native.legacy_1.0.56_arm.ipk` gerado hoje
- webOS 3: `webosbrew-ipk-verify -S -d -r ">=3,<4"` All OK, exit 0, rodado hoje no 1.0.56 da branch webos3
