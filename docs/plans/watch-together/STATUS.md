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
2. **Perfil infantil na TV**: o cliente não tem marca de perfil infantil nem limite de classificação (`perfis.h`/`parental.h` não têm). O servidor já aplica todas as regras quando recebe `infantil:1`; falta decidir de onde a TV tira essa marca. Hoje ela manda sempre 0.
3. **Prova em TV**: Android primeiro (par TCL + outra TV/box, ou TCL contra o Mac), depois LG + Android. Conferir no log as linhas `[juntos]`; os limiares de 6.5 são do simulador.

## Lacunas conhecidas (MVP)

- O veredito da fonte aparece **depois** de entrar (no cartão da sala e em aviso), não no modal do convite: calcular antes exigiria buscar as fontes de outro título enquanto a pessoa assiste outra coisa.
- Entrar só pela página do título (circular "Assistir Juntos") ou pelo convite; não há atalho na Central nem "Chamar alguém" de dentro do player, nem o cartão "Voltar para a sessão" na ilha depois de sair do player. "Minimizar" não está na pergunta de sair.
- O cartão do próximo episódio continua local em cada TV; quem pede primeiro leva a sala (o servidor ignora o repetido).
- O feed agrupa "assistiram juntos" por título + hora, não pelo id da sala.
- `video_pos_amostra`/`video_taxa_sync` do plano não foram criados: o motor usa `video_pos` (com o instante em que o valor mudou) e `video_velocidade` (a da pessoa fica 1x durante a sala).
- Check de compilação ARM do LG (`tools/arm.sh --build`) não rodado.
- `tests/discord_runtime.sh` já falha em master (link de `discord_tls.c` sem `redeurl.c`); não é deste trabalho.
