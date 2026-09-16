# Rascunho do segundo post — NÃO PUBLICADO

Escrito para você revisar e postar. Eu não publico nada.

**As imagens estão em `~/Desktop/nuvio-post-2/`** (36 PNG 1920×1080, interface em
inglês, capturadas do próprio app — build do Mac com a sua conta, mais um recorte
da C9; as da parte Social vêm do teste com dados de mentira, porque a sua conta
ainda não tem recomendação real). A lista com legenda está no fim; a ordem sugerida para a galeria é a
numeração dos arquivos. O Reddit aceita até 20 imagens numa galeria: a seleção
de 14 marcada com ★ é a que eu usaria.

**Antes de postar:**

1. **webOS 3.** Você me disse que está testado e funcionando. Escrevi "tested
   by users on webOS 3 sets" e não "measured by me", porque a única TV deste
   bancada é a C9. Se foi você mesmo numa webOS 3, troque para primeira pessoa.
2. **Links fora do corpo**, no primeiro comentário — mesma razão do post
   anterior (filtro do Reddit). Lista no fim.
3. **Publique antes** a webos3 1.0.56 e as duas variantes alto-cache
   (`.ipk` e `.wgt`), senão o post aponta para o que não existe.
4. **Onde:** r/webos, r/LGOLED, r/Nuvio. Ler a regra de autopromoção de cada um.

---

## Título (escolha um)

- Native C port of a webOS streaming app, twelve days later: webOS 3 through the 2024 sets, Samsung Tizen, one build that sizes itself to the TV — and 54 releases of your bug reports turned into fixes
- Update on the native (C/SDL2) webOS streaming app: runs from webOS 3 to webOS 6+, has a Samsung build, and remembers which source you picked
- What 236 commits and 57 issues taught me about writing a TV app in C

---

## Corpo

Twelve days ago I posted a native C/SDL2 port of a webOS streaming app, tested
on one rooted 2019 C9. Since then: 236 commits, 54 releases, 57 issues opened
by people here and 45 of them closed. This is what changed, with screenshots,
and what I learned along the way. Links in the first comment.

**Where it runs now — one build, every generation**

- **webOS 3 (2016–2017)** — tested by users and working. There was no porting to
  do: I compared the binary's undefined symbols against webosbrew's retail
  firmware dumps and exactly *one* of 199 was missing on webOS 3
  (`SDL_CreateRGBSurfaceWithFormat`, SDL 2.0.5). A five-line shim built on two
  calls present since SDL 2.0.0 replaced it, one optional audio symbol stopped
  being fatal, and `webosbrew-ipk-verify -r ">=3,<4"` says All OK.
- **webOS 4** — measured here, still the reference set: 60 fps, worst frame
  under 20 ms.
- **webOS 5, 6 and newer, including the 2024 sets** — working, reported by
  users. My own README said the opposite for a while and it was wrong: the
  video path used to need `libAcbAPI`, which LG removed in webOS 5, but the app
  already fell back to SDL's exported-window API. Nobody had updated the table.
- **Samsung Tizen** — the same C compiled to WebAssembly inside a `.wgt`, video
  through AVPlay. Users on 2020+ sets (AU7000 among them) supplied most of the
  performance reports below.

It is the same `.ipk` for every LG generation. The one thing that differs
between a 2016 set with 624 MB of RAM and a 2024 set with 3 GB — how much
artwork to keep in memory — is decided at startup from the TV's own RAM
(48 MB on the smallest, 128 MB on the C9, 192 MB on 3 GB sets; on Samsung
from `navigator.deviceMemory`, 96 MB when the browser does not say). For
people who want to push it, there is a separate **high-cache** build for both
platforms with the ceiling fixed at 300 MB. Settings shows a live graph of
what the cache is doing, so you can see whether your set needs it.

**What was added**

- Multiple profiles with server-side PIN and a "who is watching" screen at
  launch, asked once per session, not once per install *(17)*
- Trakt and Simkl linked from the TV itself through their device-code flows;
  Continue Watching from either; a **Profile & Stats** page — hours watched,
  activity rhythm, most watched, genres — from your Trakt history *(06)*
- A **Saved** list on the blue button, with a **Social** tab: recommend a title
  to a friend from its page, add friends by code, see what they sent you *(15, 16)*
- **Live TV**: a guide with EPG for 768 channels in 35 categories, channel PiP,
  and sources chosen by probing the playlist instead of trusting the first URL
  *(03)*
- **Collections** from your account — streaming services, genres, themes,
  film series — with animated covers on focus *(18, 19)*
- A **Home rows** sheet: reorder, enable, disable, choose the card shape, see
  which add-on each row comes from and what is queued behind the row limit
  *(09)*
- Add-ons managed on the TV, with what each one actually provides read from
  its manifest rather than assumed *(11)*
- Every setting the web app exposes, in the TV's language; the English table
  went from partial to complete across the issues *(07, 08)*
- TMDB and MDBList integration: logos, cast with photos, parental guides,
  ratings, Trakt comments on the title page; a credits marker from TheIntroDB;
  "More like this" at the *end* of the film, not the start *(12)*
- The app **remembers the source you picked** and uses the same one for the
  next episode — by the add-on's own `bingeGroup` when it sends one, by
  provider + audio-track signature when it does not — and the sources sheet
  now marks which one would be picked automatically *(14)*
- In-app update check with the release notes, and on rooted LG sets it
  installs the update itself
- Focus is now a filled button in your accent colour instead of an outline —
  a request from a photo of my own TV that I agreed with once I saw it *(02, 07)*
- A watched check on the poster, sized like the reference at last *(23)*
- First-time cards that explain a feature the first time you meet it *(21, 22)*

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
  numbers are on the Settings page *(10)*.
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
unrooted install myself. The `.ipk` is 35 MB, all bundled artwork so the home
screen has something before you sign in. GPLv3, unofficial, not affiliated
with NuvioMedia.

If you run it on something I do not have — webOS 3, webOS 6, any Samsung —
the red button opens the log panel, and a photo of it is worth more than a
description.

---

## Primeiro comentário (links)

- Code: https://github.com/iqui27/nuvio-native-legacy
- Latest release — LG `.ipk` (webOS 3 to 6+) and Samsung `.wgt`: https://github.com/iqui27/nuvio-native-legacy/releases/latest
- High-cache builds (300 MB, for sets with RAM to spare): same release page, files with `altocache` in the name
- Install guide: https://github.com/iqui27/nuvio-native-legacy/blob/master/INSTALL.md
- Web (JavaScript) fork, for anything else: https://github.com/iqui27/NuvioTVSmart-legacy-webos

## Imagens (`~/Desktop/nuvio-post-2/`) e legendas

★ = as 14 que eu poria na galeria, nesta ordem (o Reddit aceita 20).

| arquivo | legenda sugerida |
|---|---|
| ★ 01b-home-rows.png | Home: hero, rows, focused card grows — no ring |
| 01-home.png | Home top: Continue Watching and friends on Trakt |
| ★ 02-menu.png | Side menu, focus as a filled button in the accent colour |
| ★ 03c-guide-focus.png | TV Guide: 768 channels in 35 categories, EPG on now / up next for the focused channel |
| 03-guide.png | Guide at the top: category sections |
| 03b-guide-epg.png | Guide further down, programme bars |
| 04-search.png | Search with the on-screen keyboard |
| 05-library.png | Library: Saved and Collection tabs, type/sort filters |
| ★ 06-profile.png | Profile & Stats from Trakt: hours, rhythm, most watched, genres |
| 07-settings.png | Settings: focused row filled, categories on the left |
| 08-settings-categories.png | Settings with focus on the category column |
| ★ 09-home-rows.png | Home rows sheet: order, enable, card shape, where each row comes from |
| 09b-home-rows-out.png | Rows not on Home, grouped by add-on |
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

Nota sobre 13-player.png: o vídeo vive num plano de hardware atrás do GL e não
sai em captura nenhuma (nem na TV); a imagem mostra só os controles, e no Mac
não há pipeline de vídeo, por isso o aviso "Could not open the source" no meio.
Se quiser mostrar reprodução, é foto da TV.

Não capturado: o cartão de atualização em inglês (o do Mac fica marcado "Depois"
e não volta; só tenho a versão em português da TV), e o PiP de canal (precisa de
vídeo). Se quiser os dois, é tirar na TV.

## Números usados no texto, e de onde vieram

- 236 commits / 54 releases: `git log v1.0.2..HEAD`, `gh release list` (v1.0.2 → v1.0.56)
- 57 issues, 45 fechadas: `gh issue list --state all` em 16/09
- 768 canais / 35 categorias: `[guia] 768 canais em 35 categorias` no log do Mac hoje
- "60 fps, pior quadro < 20 ms": linha FPS do log da C9 hoje
- "dezenas por segundo → zero": `tex-despejos=46(q=…)` antes, `q=0` depois, mesmo log
- 48/128/192 MB: orcamentoMB() em src/tex_cache.c; 624 MB é o relato de RAM de uma webOS 3 no README
- 35 MB: `space.nuvio.native.legacy_1.0.56_arm.ipk` gerado hoje
- webOS 3: `webosbrew-ipk-verify -S -d -r ">=3,<4"` All OK, exit 0, no 1.0.56 da branch webos3 hoje
