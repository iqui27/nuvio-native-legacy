# STATUS — Diário + Pegou no sono (feat/sp-diario)

API para as outras frentes: `API-diario.md` (estável desde 06/10; só acréscimos).

## Feito

- [x] `src/diario.h` + implementação `src/diario.c` — por perfil, fusão de fontes (6 h, p > t > c > l > m), rever, nota 1–10, flags, tags, consultas (título, episódio, período, lista/pessoa, filtro), ouvintes, arquivo `diario-p<N>.txt` com dono (conta) no cabeçalho, teto 20 mil. `tests/diario.sh`
- [x] Cache de nomes compartilhado (`src/diarionomes.c`)
- [x] Fontes (`src/diariofontes.c`): player (fim = 90%/créditos, seg, hora, cor, duração), conta (`contalib` vistos, "data aproximada"), reação dos créditos → nota 8/5/2 (`reacao.c`)
- [x] Trakt (`src/diariotrakt.c`): `watched/*` na 1ª volta + `history` do ano, depois incremental (cursor), 1 volta/6 h, 90 s após o arranque. `tests/diariotrakt.sh`
- [x] Perfil e Stats sem Trakt (`src/diarioperfil.c` + `app.c`): mês corrente, minutos só do que se sabe, sequência real, destaques com nome/capa. Estado vazio: "Seu Diário ainda está vazio". `tests/diarioperfil.sh`
- [x] Pegou no sono (`src/dormiu.c` regra + acompanhamento, `src/dormiuilha.c` pergunta na ilha ao abrir a página do título; Voltar = retomada no ponto; Não dormi = apaga e +10 min no limiar). `tests/dormiu.sh` (3 casos de ponta a ponta)
- [x] Cofre na nuvem (`src/diarionuvem.c`): perguntado 1 vez ao abrir Perfil e Stats; lotes de 100; restauração; 409 = desligado no servidor. `tests/diarionuvem.sh`, `tests/diarionuvem_servidor.sh` (contra wrangler dev --local)
- [x] Servidor: `migracao-011-diario.sql` (NÃO aplicada), `src/diario.js`, `src/letterboxd.js`; testes `teste-diario.mjs` (134) e `teste-letterboxd.mjs` (55)
- [x] Letterboxd: CSV de exportação no formato deles (servidor + `diario_csv_letterboxd` na TV), import do ZIP/CSV da própria pessoa (módulo JS, para a página do celular), link de review (só o link). Rascunho do pedido de API: `letterboxd-pedido-api.md` (não enviado)

## Em andamento / falta

- [ ] Ajustes › "Guardar diário na conta" (agente em paralelo)
- [ ] i18n das frases novas + capturas (`tests/diario_shot.sh`, NUVIO_SHOT_FONTE=3)
- Página do celular (import ZIP, review longa) é da onda 3: o módulo `letterboxd.js` já está pronto para ela.
- Tela "Diário" (lista por mês) é da onda 5 do plano.

## Precisa do dono

- Aplicar a migração 011 no D1 e publicar o Worker (nada foi publicado).
- Provar em TV: a pergunta do sono, a retomada e o Perfil sem Trakt.
- Enviar (ou não) o pedido de API do Letterboxd.
