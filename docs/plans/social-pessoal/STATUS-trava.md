# STATUS — Trava de série (Onda 1, mesma conta/TV)

Branch `feat/sp-trava` (worktree `/Volumes/ExternalSSD/nv-sp-trava`). Plano: seção 5 e 10.2 de
[social-pessoal-plano.md](social-pessoal-plano.md). Nada publicado, nada aplicado no D1, nada instalado em TV.

## 1. A conta escreve "visto" em OUTRO perfil? (pergunta do risco 16, antes do Juntos)

**Conclusão: sim, pelo que o código e o contrato do servidor mostram. Não medido com escrita** (proibido escrever na conta do dono).

Provas (só leitura):

1. **Assinatura da RPC**, lida do OpenAPI público do PostgREST (`GET <supabase>/rest/v1/` com a chave anon, a mesma embutida no app; nenhum dado de usuário):
   `sync_push_watched_items(p_items, p_origin_client_id, p_profile_id)` e `sync_delete_watched_items(p_keys, p_origin_client_id, p_profile_id)`.
   Não há parâmetro de PIN, de "perfil desbloqueado" ou de sessão de perfil. O token é o JWT da **conta** (um `sub` para todos os perfis).
2. **O PIN é só uma porta no cliente.** `verify_profile_pin(p_pin, p_profile_id)` devolve um booleano/`{unlocked}`; nenhum token sai dele. O app oficial (`NuvioTVOS/.../ProfileRepository.kt`) guarda o PIN verificado localmente (`ProfilePinCache`) e depois chama as RPCs só com `p_profile_id`. O nosso (`perfis.c`) faz o mesmo.
3. **Nosso app já escreve em perfil que não é o ativo hoje.** O jornal `contapend.c` envia cada gesto "no perfil onde foi feito", mesmo depois de trocar de perfil (`enviarJa`, comentário "O ativo pode ter mudado depois; a marca é de quem a fez"). Esse caminho está em produção desde o jornal.
4. **Efeito nos apps oficiais**: eles leem por `sync_pull_watched_items_delta` com cursor de eventos (`watched_item_events`), então a marca aparece no outro perfil como se tivesse sido feita lá. `p_origin_client_id` é o nosso id de cliente, como em todo push.

O que falta para virar "medido": o dono, numa conta de teste ou na dele, criar uma trava Juntos, terminar um episódio e conferir no log `[sync] vistos perfil N: marcar 1 na conta -> ok (HTTP 200)` para o perfil do outro membro, e o check aparecer no app oficial nesse perfil.

**Plano B (implementado como reserva automática, não como caminho principal):** se a conta recusar a escrita do outro perfil (HTTP ≠ 2xx), a entrada fica pendente no jornal (como qualquer falha) e o log diz `vistos perfil N ... falhou`; a trava continua funcionando porque a posição de cada membro também anda **na própria trava** (arquivo local + `POST /v1/trava/passo` com a pessoa de cada membro). Ou seja, o "visto para todos" da trava não depende da conta: só o check nos apps oficiais depende. Para desligar a escrita na conta sem novo build não há chave; se o dono medir recusa, o ajuste é uma linha em `travarede.c` (`TRAVA_JUNTOS_NA_CONTA`).

## 2. Feito

- [x] `src/trava.h/.c`: modelo, regra pura (limite, gate de spoiler, frases), arquivo `travas-<conta>.txt`, fila de operações, fusão com o servidor por perfil, "alcançou você".
- [x] Servidor: `migracao-012-trava.sql` (NÃO aplicada) + `src/trava.js` (`GET /v1/travas` com ETag, criar/modo/pin/pausar/membros/sair/apagar/passo; só perfis da mesma conta) + `travaRev` em `/v1/rec`.
- [x] `src/travarede.c`: fila → servidor num fio, leitura quando `travaRev` muda, reconciliação de hora em hora pelos vistos da conta de cada membro (`sync_pull_watched_items`, só leitura), ouvinte de `visto_episodios`.
- [x] **Juntos**: marca o episódio para os outros membros na conta (`contapend_episodios_perfil`, jornal) e no Trakt do membro **só se** o token dele está nesta TV (`traktauth_token_de`); ilha "Marcado para Jerry e Sandra" com Desfazer; com `diario.h` no build, grava a vez no diário dos membros (`DIARIO_F_JUNTOS`, `comPerfis`) via `__has_include`.
- [x] `src/travaui.c`: modal Travar/Gerenciar (pessoas com SEM PIN, modo, chave de PIN, linha de estado viva, Pausar/Desfazer), folha de espera (miniatura borrada, rostos, "Esperando…", Assistir mesmo assim com PIN de QUALQUER outro membro com PIN, num fio), Perfil › página 2 "Travas de série".
- [x] Ganchos: cadeado na linha de ações da série (`detail.c`), cartão de episódio travado (borrado + rostos + "Esperando…"), gate antes de tocar (`app.c`), próximo episódio no fim (`posplay.c` → ilha "Você alcançou…"), ilha "X alcançou você", BAIXO no Perfil.
- [x] i18n: 54 chaves novas nas 29 tabelas (`tools/idiomas.py` ok).
- [x] Ícones `aj_lock`/`aj_lock-open` (Lucide 1.48.0, SVG transcrito; `tools/icones-lucide.sh`).

## 3. Testes

- `tests/trava.sh` (regra, virada de temporada, especiais, rever, Juntos/pausada, frases, arquivo, servidor, alcance).
- `node --test servidor/recomendacoes/teste-trava.mjs` (5) + os outros `teste-*.mjs`; `wrangler dev --local` com `preparar-local.sh` + `teste.sh` (42 ok) e curl em `/v1/trava*`.
- Capturas `tests/trava_shot.sh` (fonte 3): modal, espera, PIN, perfil.
- Build Mac sem avisos; `-DNV_TPK` sintaxe ok. `tests/visto.sh`, `contapend.sh`, `traktauth.sh` ok.

## 4. Falta / precisa do dono

- **Deploy**: aplicar `migracao-012-trava.sql` no D1 e depois `wrangler deploy` (sem isso a trava funciona só local nesta TV).
- **Medir na TV/conta** a escrita do Juntos em outro perfil (seção 1).
- Fora desta entrega: T3 (ordem do "Continuar"), linha "te esperando" na escolha de perfil, chip no hero, amigos (Onda 4), regra de perfil infantil (perfis.c não expõe "infantil").
- Diário: integração automática quando `diario.h` entrar no mesmo build; a entrada do perfil ATIVO com `comPerfis` fica com o diário (player).
