# Pesquisa: plx-native — o que o player e a UI deles fazem de melhor

**Pedido do dono:** "veja se o player tem algo melhor que o nosso e as otimizações de interface
porque é bem fluido".

**Método.** Clone read-only em `/Volumes/ExternalSSD/tmp/pesquisa/plx-native`
(`--depth 1`, commit `9d2777fe4d1fd9cb9cd9eb08503bc780e8a52c17`). Nada foi compilado nem executado
(nem o código deles, nem scripts deles). Nenhuma mudança em `src/`. Comparado contra o nosso
`src/player.c`, `src/video.c`, `src/legenda*.c`, `src/tex_cache.c`, `src/main.c`, `src/app.c`,
`src/anim.h`, backends (`video.c` LG, `video_tpk.c`, `video_tizen.c`, Android).

**O que eles são.** Cliente Plex para webOS, quase tudo em Rust desenhando direto no GPU a 60 fps,
vídeo no decoder da TV (Starfish/ACB — mesmo caminho do nosso `src/video.c:46+`), ffmpeg carregado
por dynlib só para demux/legendas (`rust-modules/media/src/ff.rs`). Não há navegador.

## Licença

`LICENSE:1-3` = GPLv3; `LICENSING.md:3-5` = **GPL-3.0-or-later** (eles mantêm um grant MIT
histórico em `licenses/PlxNative-historical-MIT.txt`). Nosso `LICENSE` também é GPLv3 →
**compatível**: código deles pode entrar num projeto GPLv3 desde que sob GPL, com copyright
deles preservado. Constantes medidas, tabelas e ideias não são copyrightáveis. Não copiar nada de
`THIRD-PARTY-NOTICES.md` (dependências deles) nem tocar em `vendor/`.

## Top 10 por valor/esforço

| # | O quê | Custo | Risco em TV fraca |
|---|-------|-------|-------------------|
| 1 | Lookahead de pôsteres em repouso (esquentar 3 linhas além da janela) | baixo-médio | baixo |
| 2 | Cenas de regressão de FPS **no aparelho**, com gate | médio | nenhum (medição) |
| 3 | Orçamento de upload por classe com pior caso medido + frame SOLO | baixo | baixo |
| 4 | Recusa/pausa da arte **no ponto do pedido** (antes do fetch) | médio | médio (tile escuro se exagerar) |
| 5 | `warm_tex`: aquecer textura nova offscreen antes do 1º uso | baixo-médio | baixo |
| 6 | Legenda de imagem (PGS/VobSub/DVB) por decode software | médio-alto | médio (memória) |
| 7 | Pre-fetch de legendas **sem parar o vídeo** (side reader) | médio | baixo |
| 8 | StallGuard de HLS: abortar cedo a stream perdida | médio | baixo |
| 9 | Press-dip no OK + commit no key-up (estilo tvOS) | médio | médio (muda timing da ação) |
| 10 | Heartbeat medindo o intervalo real de swap (fora do CPU work) | baixo | nenhum |

## Detalhe

### 1. Lookahead de pôsteres em repouso — o que mais explica a fluidez deles
- **Eles:** `rust-modules/screens/src/library/lookahead.rs:1-27` (headline), `:37`
  (`DEFAULT_ROWS = 3`), `:44` (`SETTLE_FRAMES = 2`). Só com a grade **parada** e a fonte ociosa
  (`tex::source_idle`), esquenta no máximo 1 carta por vez além da janela pintada, na direção do
  último movimento; conjunto limitado a `AHEAD_SET_MAX = 30` (`rust-modules/ui/src/tex.rs:293`).
  Medido no aparelho (mock de 1000 filmes): **28.969 → 6.668** "placeholder tile-frames" em
  `library-burst` só com o lookahead.
- **Por que é mais suave:** a rolagem curta revela cartazes já prontos; sem isso cada linha nova
  começa o fetch só quando é desenhada pela 1ª vez → 2+ quadros de cinza por carta
  (o mesmo sintoma que o nosso "pisca", `src/tex_cache.c:511-517`).
- **Nós:** não temos lookahead. O pedido nasce do desenho e o critério "quente"
  (`src/tex_cache.c:511-517`) só serve à política de expulsão; a fila atende tudo
  (`src/tex_cache.c:410-439`). Onde encaixa: `src/tex_cache.c` (novo pedido em repouso) + as grades
  que já medem a janela (`src/biblioteca.c:2387`, `src/detail.c:6453-6455` já poda fora da tela).
- **Custo:** baixo-médio (um passo por quadro,condição de repouso = alvo de scroll parado ~2 quadros).
- **Risco:** baixo — só enfileira trabalho quando nada mais está pendente.
- **Licença:** GPL-3.0-or-later, ok.

### 2. Cenas de regressão de FPS no aparelho
- **Eles:** `tests/run.py:5932` (`run_fps_scene`), `:6270-6286` (suíte com tiers), cenas tipo
  `fps:library-scroll`/`home-grid`/`search-type` catalogadas em `tests/manifest.json`, servidor
  sintético `tests/mock_fps.py:1` (`run.py --fps --mock`), gate offline de contadores
  `tests/poster_gate.py:1-25`, gatilho por frame `plxnative-framedrop`
  (`rust-modules/src/dev.rs:127`), e a tabela de decisões medida no TV em
  `rust-modules/src/app/adapters/poster.rs:1230-1236`.
- **Por que vale:** toda decisão de fluidez deles tem número de aparelho antes de virar default
  (drops/fps medianos). É o mecanismo que **mantém** a UI fluida, não um truque pontual.
- **Nós:** temos telemetria por fase (`src/main.c:1591-1613`), log de jank
  (`src/main.c:2148-2175`) e relógio de GPU (`src/main.c:1416`), mas **nenhuma cena scriptada com
  gate** — regressão só aparece no relato do dono.
- **Custo:** médio (cenas de input scriptado + mock server; dá pra começar host-only).
- **Risco:** nenhum.
- **Licença:** GPL ok; convenção de teste deles serve de modelo, código nosso.

### 3. Orçamento de upload por classe, pior caso medido, frame SOLO
- **Eles:** `rust-modules/ui/src/frame/budget.rs:11` (`PREPARE_MAX_US = 5000`),
  `:18` (`SOLO_MAX_US = 8000`), `:56-77` (`Class::Poster` = 2500 µs pior caso medido no TV,
  cota 3/quadro; `Class::Residency` = 6000 µs → **SOLO**: roda sozinha no quadro, recusando as
  outras takes).
- **Por que é mais suave:** o relógio abre o quadro **antes** de cada take; uma arte grande nunca
  entra em cima de 3 cartazes — o quadro longo é longo **sozinho**.
- **Nós:** orçamento plano de 4 ms e `tex_bombear(3)` (`src/tex_cache.c:61-62`,
  `src/main.c:1900`), sem classes; upload em faixas p/ >2 MB já existe e ajuda
  (`src/tex_cache.c:3990-4008`). Falta: preço por classe e a regra SOLO.
- **Custo:** baixo (contagem + um switch de classe no `tex_bombear`).
- **Risco:** baixo.
- **Licença:** GPL ok (adaptação).

### 4. Recusa no ponto do pedido
- **Eles:** `rust-modules/src/app/adapters/poster.rs:1230-1236` — A/B medido: recusar **no
  pedido** derruba drops de 90/97 → **21/23** e sobe mediana 55 → **60 fps**; "rate-limit depois do
  decode" e "1 em voo" foram **piores que nada** ("pacing não torna o trabalho acessível; o pedido
  é o último ponto onde ainda dá pra recusar de graça"). Classificação de velocidade da carta em
  `rust-modules/ui/src/card_motion.rs:28` (`MOVING_SPEED = 120 px/s`), `:201` (`declines_request`)
  — hoje só como gatilho de dev, porque a recusa fixa deixava tiles escuros.
- **Nós:** nenhuma recusa por movimento; tudo que é desenhado é pedido e processado
  (`src/tex_cache.c:410-439`). Urgente=2 só prioriza, não recusa.
- **Custo:** médio. **Risco:** médio — seguir **o default final deles** (lookahead, item 1) e deixar
  a recusa atrás de um gatilho medido; recusa fixa = tiles escuros (medido por eles).
- **Licença:** GPL ok.

### 5. `warm_tex` (aquecimento offscreen)
- **Eles:** `rust-modules/gfx/src/gfx.rs:2330` (`warm_tex`), alvo em `:2372`; medido em
  `budget.rs:1-8`: sem o warm, o 1º draw do quadro seguinte pagava a espera do back buffer —
  **116 ms** no draw seguinte num upload de 1280×720; com warm offstream o prepare caiu de
  p90 12.9 → 6.1 ms.
- **Por que é mais suave:** a espera do buffer nova sai do caminho do desenho (fica na janela de
  prepare, que tem orçamento) em vez de virar pico no meio da fase de desenho.
- **Nós:** publicamos no `tex_bombear` (`src/main.c:1900`) e desenhamos logo atrás
  (`src/main.c:1984`); a espera aparece misturada no `swap` (nossa própria nota
  `src/main.c:1596`) ou — pior — no `des`.
- **Custo:** baixo-médio (1 draw minúsculo num FBO pequeno ao publicar).
- **Risco:** baixo. **Licença:** GPL ok.

### 6. Legenda de imagem (PGS/VobSub/DVB)
- **Eles:** decodificam por software com ffmpeg — declarações em
  `rust-modules/media/src/ff.rs:411-425` (`avcodec_decode_subtitle2` p/ "PGS/VobSub/DVB →
  paletted bitmap"), teste `ff_image_subtitle_tests.rs`.
- **Nós:** **escondemos** essas faixas — `src/faixasmkv.c:52-56` (função `legendaImagem`),
  `:110` e `:138` filtram; o MKV com 1 faixa PGS vai sem legenda nenhuma (medido por nós,
  p2p-20). Falta de recurso, não de fluidez — mas é a maior diferença de player.
- **Custo:** médio-alto (já temos ffmpeg no build DTS, `src/dts/dts_engine.c:34-38`; o overlay de
  bitmap já existe no caminho do libass, `src/assrender.c:114+`).
- **Risco:** médio — PGS 1080p ≈ 8 MB por quadro de legenda; só ao escolher a faixa, com teto.
- **Licença:** GPL ok.

### 7. Pre-fetch de legenda sem parar o vídeo
- **Eles:** "side reader" — 2º demux lendo só pacotes de legenda do Part original enquanto o vídeo
  toca; janela de 12 s na frente do playhead, park de 250 ms
  (`rust-modules/media/src/ff_subside.rs:1-27`).
- **Nós:** o pre-fetch ASS **para o vídeo** — `src/player.c:1689-1696` chama `video_parar()` e fica
  em "abrindo fonte" até achar a legenda (nota em `src/player.c:326-330`).
- **Por que é melhor:** começa a tocar na hora; a legenda chega no máximo 12 s depois, no pior caso,
  e nunca trava o início.
- **Custo:** médio (o `mkvass` já tem modo "com vídeo"; falta o pacing + o teto à frente).
- **Risco:** baixo. **Licença:** GPL ok.

### 8. StallGuard de HLS (TV ao vivo)
- **Eles:** `rust-modules/media/src/ff.rs:1563-1691` (`StallGuard::arm/should_abort`) + teste
  `ff_stall_guard_tests.rs:1-30`: previsão de reserva vs taxa atingida, com proteção contra o
  prefixo JIT do servidor (214 KB em 258 ms não prova que o resto não chega) — aborta cedo a
  stream perdida, sem spinner eterno.
- **Nós:** ao vivo com .m3u8/.ts e retry reativo — `src/app.c:1088-1228` e a máquina de
  reconexão `src/video_reconexao.h:1-26` (3/10/25 s, até 3 tentativas). Reage **depois** do
  travamento; não prevê.
- **Custo:** médio (só o caminho HLS ao vivo; o teste deles é spec valiosa).
- **Risco:** baixo. **Licença:** GPL ok.

### 9. Press-dip + commit no key-up
- **Eles:** `rust-modules/ui/src/press.rs:1-45`, `:37` (`DIP = 0.918`), `:78` (`LONG_MS = 500`),
  `:255` (`held_ms`): OK-**down** afunda a carta, OK-**up** solta com overshoot e **então** comita a
  ação (o bounce aparece antes do commit); press preso resolvido por heartbeat (key-up caido).
- **Por que parece mais fluido:** feedback imediato no key-down e a ação visível depois do pop,
  além de long-press medido de verdade (context menu).
- **Nós:** ação no key-down com só o pop de foco; não achei dip de pressão em `src/`
  (`grep press/dip` só comentários). Mapeia no dispatch de OK (`src/app.c` evento, `src/main.c:264`
  `ok→SDLK_RETURN`) + escala por carta nos desenhos.
- **Custo:** médio (estado de pressão propagado ao desenho). **Risco:** médio — muda o timing da
  ativação em todas as telas; testar teclados sem key-up (já temos a defesa de "repeat do firmware",
  `src/app.c:2644-2651`).
- **Licença:** GPL ok.

### 10. Heartbeat: intervalo real de swap medido
- **Eles:** `rust-modules/base/src/diag/heartbeat.rs:744` — mede o intervalo **real** entre swaps,
  incluindo o tempo fora do trabalho de CPU (compositor/driver entram na conta), e carrega
  `fps=`/`loop=`/`rate_stats`; `FRAMEDROP` por quadro sob gatilho (`budget.rs:42`).
- **Nós:** telemetria de fases por CPU (`src/main.c:1591-1613`) é boa, mas o `swap` mistura espera
  de vsync + dívida de GPU (nossa nota em `src/main.c:1596`) e o compositor não aparece em lugar
  nenhum — "33 ms" no nosso log não distingue 17 de 30 ms de GPU sem o `gputempo`
  (`src/main.c:2165+`), que só existe onde a extensão existe.
- **Custo:** baixo (carimbo de tempo entre swaps consecutivos + contagem). **Risco:** nenhum.
- **Licença:** GPL ok.

## Não portar (medido por eles e recusado)

- **Damage tracking / `EGL_KHR_partial_update`:** teto medido enorme (480×270 de dano:
  `GPU_ACTIVE` 7.531.684 → 542.012), mas a extensão não é anunciada pelo driver, o painel do
  compositor não é endereçável de dentro do processo e o veredito deles é **"do not ship"**
  (`docs/egl-partial-update-and-damage.md:339`,
  `docs/perf-damage-tracking-verdict.md` cabeçalho). Não gasta tempo.
- **Recusa fixa em carta em movimento:** já descrita no item 4 — default final deles é lookahead.

## Onde já estamos iguais ou à frente (não faz)

- Mola de 2ª ordem criticamente amortecida com closed form e retarget (`src/anim.h:58-76`) —
  equivalente ao `Spring` deles; política de movimento reduzido centralizada (`src/anim.h:12-22`).
- Culling de cartas fora da tela (`src/detail.c:6453-6455`) e clip por scissor (`src/gfx.c:1821`).
- Orçamento + upload em faixas + urgente + pirâmide de mip na CPU (`src/tex_cache.c:61-62`,
  `3990-4008`, `419-439`, `2333-2372`) — rumo já certo; faltam as classes do item 3.
- Snapshot é FBO de GPU, não readback (`src/gfx.c:3266-3305`).
- Decodificação por hardware via ACB/Starfish no LG (`src/video.c:46+`) — mesmo caminho de
  `src/starfish.c` deles; reconexão/seek-retry/DV-retry nossos não têm equivalente visível lá.
- Cache de disco com teto proporcional + reserva (`src/tex_cache.c:338-348`) e TTL negativo 24 h
  (`src/tex_cache.c:1488`) vs 128 MiB/8192 arquivos deles (`docs/image-cache.md:19`) — equivalentes;
  **eles ganham** em stale-while-revalidate com worker de refresh em repouso
  (`docs/image-cache.md:28`) e teto de bytes na RAM/GPU (`render_set.rs:46` 64 MiB,
  `tex.rs:182` 44 MiB) — nós temos slots, não bytes (honorável menção, custo baixo).
- libass dos dois (`src/assrender.c:114+` vs `src/ass.c`) — paridade; eles aplicam teto de
  memória/ eventos (`src/ass.c:14-24`), nós não (honorável menção).

## Verificação

Sem mudanças em `src/`; relatório é documentação. Testes do repo no fim:
`bash tests/reconexao.sh` (rc 0) e `bash tests/vazao.sh` (rc 0).
