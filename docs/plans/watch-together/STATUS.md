# Watch Together — status

Branch `feat/watch-together` (worktree `/Volumes/ExternalSSD/nv-watch-together`). Nada publicado, nada aplicado no D1 real, nada instalado em TV.

## Marcos

| # | Marco | Estado | Como foi testado |
|---|---|---|---|
| 1 | Servidor: DO `Sala` + bilhete + convites + protocolo + relógio | feito | `servidor/juntos/teste-logica.mjs` (16 casos, relógio falso); `servidor/juntos/teste-juntos.mjs` (workerd real via `wrangler dev`, 3 pares Node com atraso 20–300 ms ± 80 ms e relógios errados em +7 s/−1 h, websockets reais); `servidor/recomendacoes/teste-sala.mjs` (7 casos, worker inteiro + SQLite); `wrangler deploy --dry-run` dos dois |
| 2 | Motor de sync no cliente (C) | feito | `tests/juntossync.sh` (relógio NTP, faixas A/B, proteções, mensagens); `tests/juntos_sim.sh` (4 motores reais contra sala falsa, rede ruim, 10 min simulados; 20 sementes dentro do critério) |

### Marco 2 — o que saiu

- `src/juntossync.[ch]` (puro): relógio estilo NTP (8 amostras, menor RTT, EMA 0,3, salto > 250 ms direto), posição alvo `pos0 + (t − t0)`, interpolação da amostra do backend, decisão a cada 500 ms: zona morta, **velocidade 0,95x/1,05x** (nível A), **seek** com latência medida, **segurar** (nível B adiantado: pausa e solta quando o relógio da sala chega), margem de keyframe aprendida, cooldown 3 s, "sync frouxo" por 2 min depois de 3 seeks/min, prova de velocidade (dois empurrões que andaram a 1x → nível B).
- `src/juntosproto.[ch]`: mensagens do websocket (7.3), impressão do release sem URL.
- `src/juntosmotor.[ch]`: o membro inteiro sem SDL/rede/UI (player e transporte por ponteiro): rajada de pings, play agendado com latência de play medida, entrar no meio, pausa otimista, rajada do D-pad (só o último seek, 700 ms depois), eco do próprio comando antes de corrigir, parada de posição como "carregando" (o .wgt não tem sinal de buffering), fila de eventos para a UI.
- Servidor: `prep` (latência seek+play da TV mais lenta) entra na folga do play agendado.

Resultado do simulador (semente fixa; 20 sementes rodadas): Android A p95 15 ms, LG A 45 ms (posição lida a cada 200 ms, relógio +1 h), .wgt B 614 ms (keyframe de 2 s, sem sinal de buffering, rede 150–300 ms ± 80), .tpk que diz A mas não anda caiu para B sozinho (20 ms). Pior alinhamento depois de seek: 2,6 s (≤ 9,2 s em todas as sementes; o resto do nível A fecha por velocidade).

## Próximo

3. Impressão do release + veredito mesma/parecida/diferente/nenhuma pelo pipeline de fontes.
4. UI + ligação no app (`juntos.c`: HTTP, websocket, `video_*`).

## Riscos / pendências do dono

- Deploy (nada feito): migração 010 → `tools/juntos-publicar.sh --segredo` → `tools/juntos-publicar.sh` → deploy do `nuvio-recomendacoes` **de master**.
- Os limiares (6.5) são do simulador; os definitivos saem da TV (Android primeiro, depois LG).
