# Watch Together — status

Branch `feat/watch-together` (worktree `/Volumes/ExternalSSD/nv-watch-together`). Nada publicado, nada aplicado no D1 real, nada instalado em TV, nenhum push.

## Marcos

| # | Marco | Estado | Como foi testado |
|---|---|---|---|
| 1 | Servidor: DO `Sala` + bilhete + convites + protocolo + relógio | feito | `servidor/juntos/teste-logica.mjs` (17 casos, relógio falso); `servidor/juntos/teste-juntos.mjs` (workerd real via `wrangler dev`, 3 pares Node com atraso 20–300 ms ± 80 ms e relógios errados); `servidor/recomendacoes/teste-sala.mjs` (8 casos, worker inteiro + SQLite); `wrangler deploy --dry-run` dos dois |
| 2 | Motor de sync no cliente (C) | feito | `tests/juntossync.sh`; `tests/juntos_sim.sh` (4 motores reais, rede ruim, 10 min simulados: Android A p95 15 ms, LG A 45 ms, .wgt B 614 ms com keyframe de 2 s, .tpk que não anda cai sozinho para B) |
| 3 | Impressão do release + veredito | feito | `tests/juntosfonte.sh` (hash+idx, bytes+nome, bingeGroup+altura, reserva por altura, duração/PAL/clipe de aviso; prova que nada de URL sai) |
| 4 | UI + ligação no app | feito (MVP) | Build do Mac sem warning novo; `tests/juntos_e2e.sh` (dois motores C pelo websocket do app contra o `nuvio-juntos` local: diferença de 0–2 ms depois de começar, pausa, seek e play); `tests/juntos_shot.sh` (capturas de cada camada); build Android (`tools/android.sh`) ok |
| 5 | Celular por QR (chat + controle) | feito | Página aberta no navegador em tamanho de celular contra `wrangler dev`: entrar pelo código, chat, Começar, relógio andando, reconexão pela chave de sessão; `teste-juntos.mjs` cobre código de uso único e CSP |
| 6 | Infantil, feed "assistiram juntos", sala de 5 | feito no servidor; TV parcial | `teste-sala.mjs` (regras do infantil, feed agrupado e alcance 0 fora); `SALA_MAX=5` coberto em `teste-logica`/`teste-juntos` |

## O que precisa do dono

1. **Deploy** (nesta ordem): migração `servidor/recomendacoes/migracao-010-juntos.sql` no D1 → `tools/juntos-publicar.sh --segredo` (grava o mesmo `SALA_SEGREDO` nos dois Workers) → `tools/juntos-publicar.sh` (testa, publica e confere `/saude`) → deploy do `nuvio-recomendacoes` **a partir de master** (depois do merge; `JUNTOS_URL` já está no `[vars]`). Sem o segredo as rotas `/v1/sala*` respondem 503 e a TV esconde o recurso.
2. **Kids profile on TV**: `perfilinfantil.h` now stores the per-profile flag and `juntos.c` sends `infantil:1` when active (commit `ddc55394c`). Server rules and TV classification behavior still need device validation.
3. **Prova em TV**: Android primeiro (par TCL + outra TV/box, ou TCL contra o Mac), depois LG + Android. Conferir no log as linhas `[juntos]`; os limiares de 6.5 são do simulador.

## Client contract validation (2.1)

- `bash tests/video_juntos.sh`: FAIL (missing sync multiplier, rc=134) to PASS (rc=0). Covers native sample age/equal samples, personal speed isolation, actual client adapter and retry exclusions, protocol/clock, four-engine jitter simulation, source fingerprint/duration checks, Android JNI dispatch, .tpk host dispatch, focused LG native samples, AVPlay bridge, and local Worker WebSocket end-to-end. Local peers differed by 3 ms after start and 0 ms after seek/play.
- `bash tools/build-local.sh`: rc=0. No target deployment or remote service changes.
- `bash tests/juntos_shot.sh /Volumes/ExternalSSD/tmp/rev/watchtogether21-png/watchtogether`: rc=0. Seven hidden-window PNGs, each 1920×1080, named `watchtogether-01-convite.png` through `watchtogether-07-player-reacoes.png` in that directory. Capture fixture now keeps the invitation at full canvas size. Production screen layout is unchanged.
- `python3 tools/varredura-i18n.py`: rc=0, all three groups zero. No translation table reordering or new keys; account summary reuses labels already present in every table. Scanner exclusions document protocol values, filenames, genre aliases and diagnostics that never render as labels.
- The full LG pause fixture has an unrelated existing screensaver assertion failure after the sync assertions. The focused suite uses `NV_JUNTOS_TEST=1` to run only this task's LG assertions.
- Remaining validation: native target builds and physical TV timing, buffering, passthrough and reconnect behavior. Simulator measurements do not replace device proof.

## Lacunas conhecidas (MVP)

- O veredito da fonte aparece **depois** de entrar (no cartão da sala e em aviso), não no modal do convite: calcular antes exigiria buscar as fontes de outro título enquanto a pessoa assiste outra coisa.
- Entrar só pela página do título (circular "Assistir Juntos") ou pelo convite; não há atalho na Central nem "Chamar alguém" de dentro do player, nem o cartão "Voltar para a sessão" na ilha depois de sair do player. "Minimizar" não está na pergunta de sair.
- O cartão do próximo episódio continua local em cada TV; quem pede primeiro leva a sala (o servidor ignora o repetido).
- O feed agrupa "assistiram juntos" por título + hora, não pelo id da sala.
- Native sample timestamps and the separate sync multiplier are now implemented: `video_pos_amostra` on LG/Android/.tpk/.wgt, `video_taxa_sync` in thousandths, and `video_taxa_efetiva` for native dispatch. The room uses the same 64-bit monotonic clock; personal speed stays at 1x during a room and corrections clear on exit.
- Room-selected sources now arm the existing VOD watchdog. Retries prefer the room fingerprint and exclude failed candidates; a changed source gets a fresh duration verdict. `juntosrede` remains the existing `discordws` transport plus the ticket/backoff logic in `juntos.c`; no duplicate WebSocket implementation was introduced.
- Check de compilação ARM do LG (`tools/arm.sh --build`) não rodado.
- `tests/discord_runtime.sh` já falha em master (link de `discord_tls.c` sem `redeurl.c`); não é deste trabalho.
