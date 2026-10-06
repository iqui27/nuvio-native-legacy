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

- [x] `src/trava.h/.c`: modelo, regra pura (limite, gate de spoiler, frases), arquivo `travas-<conta>.txt`, fila de operações, fusão com o servidor, "alcançou você".
- [x] `tests/trava.sh`: regra, virada de temporada, especiais, rever, Juntos/pausada, frases, arquivo, servidor, alcance (também com ASan/UBSan).

## 3. Falta / próximos passos

(atualizado a cada commit)
