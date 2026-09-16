# Rascunho do segundo post — NÃO PUBLICADO

Escrito para você revisar e postar. Eu não publico nada.

**As imagens estão em `~/Desktop/nuvio-post-2/`** (36 PNG 1920×1080, interface em
inglês, capturadas do próprio app — build do Mac com a sua conta, mais um recorte
da C9; as da parte Social vêm do teste com dados de mentira, porque a sua conta
ainda não tem recomendação real). A lista com legenda está no fim; a ordem sugerida para a galeria é a
numeração dos arquivos. O Reddit aceita até 20 imagens numa galeria: a seleção
de 14 marcada com ★ é a que eu usaria.

**Antes de postar:**

1. **webOS 3 — está confirmado, e o texto agora diz isso.** Você me passou que
   o **Mane155 testou numa webOS 3.4.3 e está tudo certo**. Essa confirmação
   veio por fora do GitHub (não há comentário dele em issue nenhuma — procurei),
   então quem sustenta a frase é você. Escrevi como relato de testador, em
   primeira pessoa sua. O que existe público e ancora a frase: os defeitos que
   ele achou viraram **quatro pré-releases em dois dias** (exp.1 a exp.4), e a
   exp.4 cita o aparelho dele.
   **O único ponto que sobrou, e é de link, não de funcionamento:** a build de
   webOS 3 ainda é uma **pré-release de outra branch** (`native-webos3-exp.4`).
   Quem clicar na release principal baixa um `.ipk` que não é o dela. Ou você
   funde a branch e publica um pacote só antes de postar, ou mantém os dois
   links separados como estão no primeiro comentário. Isso é o que eu vou
   resolver com o merge da webos3.
1b. **O que deste post ainda NÃO existe numa release.** A seção
   "What landed since" descreve trabalho que está na árvore e **não foi
   publicado**. Ou você publica a 1.0.57 antes de postar, ou apaga a seção.
   Anunciar o que ninguém consegue instalar é o jeito mais rápido de queimar
   um post nesses subreddits.
2. **Links fora do corpo**, no primeiro comentário — mesma razão do post
   anterior (filtro do Reddit). Lista no fim.
2b. **Imagem embaixo de cada trecho**: dá, mas só no editor novo do Reddit
   ("Rich Text", não Markdown). Escreva o parágrafo, tecle Enter e arraste o
   PNG ali — cada imagem vira um bloco entre os parágrafos. Os marcadores
   `[IMAGEM: arquivo.png]` no corpo abaixo dizem onde cada uma entra; apague o
   marcador depois de soltar a imagem. Se o subreddit só aceitar post de texto
   simples, o plano B é a galeria com as ★ e as legendas da tabela do fim.
3. **Publique antes** a webos3 1.0.56 e as duas variantes alto-cache
   (`.ipk` e `.wgt`), senão o post aponta para o que não existe.
4. **Onde:** r/webos, r/LGOLED, r/Nuvio. Ler a regra de autopromoção de cada um.

---

## Título (escolha um)

- Native C port of a webOS streaming app, twelve days later: 2016 sets through the 2024 ones, Samsung Tizen, and a build that sizes its own memory to the TV it lands on
- Update on the native (C/SDL2) webOS streaming app: a 2016 set is running it, there is a Samsung build, and it remembers which source you picked
- What 251 commits and 57 issues taught me about writing a TV app in C

---

## Corpo

Twelve days ago I posted a native C/SDL2 port of a webOS streaming app, tested
on one rooted 2019 C9. Since then: 251 commits, 56 releases, 57 issues opened
by people here and 55 of them closed — two still open. This is what changed, with screenshots,
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
  find turned into four builds in two days. It ships as a separate build for
  now, from its own branch.
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

One binary, every LG generation from webOS 4 up. The one thing that has to
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

[IMAGEM: 20-update.png]  ← FALTA CAPTURAR: ver a nota no fim
- Focus is now a filled button in your accent colour instead of an outline —
  a request from a photo of my own TV that I agreed with once I saw it

[IMAGEM: 02-menu.png]
- A watched check on the poster, sized like the reference at last

[IMAGEM: 23-watched-badge.png]

- First-time cards that explain a feature the first time you meet it

[IMAGEM: 21-intro-guide.png]

**What landed since — NOT in a release yet, see note 1b**

*(Everything above is installable today. Everything in this block is in the
tree and goes out with the next release. Delete this whole section if you post
before it ships.)*

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
unrooted install myself. The `.ipk` is 35 MB, all bundled artwork so the home
screen has something before you sign in. GPLv3, unofficial, not affiliated
with NuvioMedia.

If you run it on something I do not have — webOS 3, webOS 6, any Samsung —
the red button opens the log panel, and a photo of it is worth more than a
description.

---

## Primeiro comentário (links)

- Code: https://github.com/iqui27/nuvio-native-legacy
- Latest release — LG `.ipk` (webOS 4, 5, 6 and the 2024 sets) and Samsung `.wgt`: https://github.com/iqui27/nuvio-native-legacy/releases/latest
- **webOS 3 (2016/2017 sets) is a separate experimental build**, from its own branch: https://github.com/iqui27/nuvio-native-legacy/releases/tag/native-webos3-exp.4
- High-cache builds (300 MB ceiling, for sets with RAM to spare): same release page, files with `altocache` in the name
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

**AS 11 IMAGENS DAS FEATURES NOVAS AINDA NÃO EXISTEM** (25, 25b, 26, 26b, 26c,
27, 27b, 28, 28b, 29 — e a 20). Os marcadores já estão no corpo e as legendas
na tabela; falta rodar as capturas. Cada tela nova tem harness próprio, então
é mecânico:

```
tests/agenda_shot.sh        agenda (25, 25b)
tests/biblioteca_shot.sh    listas (26, 26b, 26c)
tests/social_shot.sh        consentimento e sugestões (27, 27b)
tests/serieaud_shot.sh      gráficos (28, 28b)
tests/seriefrases_shot.sh   frases e ficha (29)
```

**Não capturei ainda de propósito, e o motivo é tempo perdido, não preguiça:**
dois agentes estão mudando exatamente duas dessas telas neste momento — o
ícone do lembrete e o pôster da Agenda numa, o painel de frases na outra.
Capturar agora é jogar fora.

**Duas coisas a acertar na hora de capturar**, porque as capturas dos testes
não servem como estão: elas saem com a **interface em português** e algumas
são recorte, e o resto do álbum é 1920×1080 em inglês. O harness escreve um
`ajustes.txt` dentro de `NUVIO_DADOS` — é ali que se troca o idioma. Capturar
em inglês, tela cheia, e gravar direto em `~/Desktop/nuvio-post-2/`.

**Falta uma imagem antiga: `20-update.png`, o cartão de atualização.** Ele só abre
quando há release mais nova que a instalada, e no Mac eu não consegui fazer o
cartão abrir mesmo com a checagem dando `instalada 1.0.50, no GitHub 1.0.56 --
NOVA` e o arquivo `atualizacao-vista.txt` apagado — alguma guarda da home não
deixou. **Na TV ele aparece** (foi de lá que veio a versão em português), e a
TV está com a interface em inglês agora, então é uma captura de 30 s quando
ela estiver livre: abrir o app, o cartão sobe sozinho na home, tecla vermelha
não, só a captura.

Também não capturado: o PiP de canal (precisa de vídeo, que não existe no
build do Mac).

## Números usados no texto, e de onde vieram

- 251 commits / 56 releases: `git log v1.0.2..HEAD`, `gh release list` (v1.0.2 → v1.0.56), conferido 16/09 ao fim do dia
- 57 issues, 55 fechadas, 2 abertas: `gh issue list --state open|closed|all` em 16/09
- 363 KB / 48 KB por arte (cap de 320 px contra 128 px) e 8,3 MB / 3,7 MB (backdrop 1920 contra 1280): medidos por `tex_estatisticas` e pelas contas em src/tex_cache.c
- Degraus 48/128/192 MB: orcamentoMB() em src/tex_cache.c, com o comentario dizendo qual foi medido e qual foi escolhido
- Degraus do Tizen: NAO medidos, nao ha Samsung aqui — esta escrito assim no post
- Cobertura do Wikiquote (11/14 filmes, 2/12 series) e casamento fala↔legenda (13% a 56%): medidos em titulos reais em 16/09
- webOS 3 em aparelho: confirmacao do Mane155 numa 3.4.3, passada por VOCE — nao ha comentario dele em issue deste repo (procurei em todas). O que e publico: as quatro pre-releases exp.1..exp.4 em 15-16/09, e o corpo da exp.4 citando o defeito do segundo perfil no aparelho dele.
- 768 canais / 35 categorias: `[guia] 768 canais em 35 categorias` no log do Mac hoje
- "60 fps, pior quadro < 20 ms": linha FPS do log da C9 hoje
- "dezenas por segundo → zero": `tex-despejos=46(q=…)` antes, `q=0` depois, mesmo log
- 48/128/192 MB: orcamentoMB() em src/tex_cache.c; 624 MB é o relato de RAM de uma webOS 3 no README
- 35 MB: `space.nuvio.native.legacy_1.0.56_arm.ipk` gerado hoje
- webOS 3: `webosbrew-ipk-verify -S -d -r ">=3,<4"` All OK, exit 0, no 1.0.56 da branch webos3 hoje
