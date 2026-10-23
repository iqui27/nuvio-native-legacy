# Painel Nuvio: integração GitHub

## Entrega e fronteira

Plano para o agente de backend; nenhuma implementação de servidor nesta rodada.
`index.html` é um protótipo local: seletor Visitante / Contribuidor / Dono, ações
em memória, indicação permanente de simulação. Login não autentica, notificações
não são enviadas e recarregar a página descarta alterações. A leitura legada de
`data.json` e `api/respostas` continua; envios legados estão desativados nesta prévia.
Não conectar esse seletor à produção. Ausência de API deve manter a identificação
“demonstração”; falha temporária da API real deve bloquear escrita, nunca converter
uma ação real em sucesso mock.

## Perfis e fluxos

| Perfil | Fluxo |
|---|---|
| Visitante | Consulta issues, status públicos, responsáveis e candidatas; abre a issue no GitHub. “Quero pegar” abre revisão do efeito e login GitHub. OAuth cancelado retorna ao cartão sem atribuição. |
| Contribuidor | Após OAuth, volta à mesma issue, revisa identidade e confirma. Exibir “Sincronizando” até o GitHub confirmar. Sucesso mostra badge `@login` e link do comentário. Pode devolver sua atribuição com confirmação e motivo opcional; não pode tomar issue ocupada, fechada ou alterar release/status do dono. |
| Dono | Acesso por identidade GitHub autorizada no servidor. Abre drawer por issue, pesquisa SHA em `meta.commits`, anexa vários commits, redige contexto público e nota privada separadamente, escolhe candidatura e status. Salvar revisa efeitos públicos antes de confirmar. Pode retirar responsável com motivo, reabrir/fechar e promover candidata após os gates. |

No protótipo, salvar drawer confirma apenas a simulação. No produto, mostrar diff
antes/depois e preview literal do comentário público. Cancelar não altera nada.
Um commit anexado é referência de trabalho, não prova de correção ou validação em TV.
Candidatura não é escopo aprovado, milestone não é publicação, fechar não é lançar.

## Estados independentes

- Sessão: visitante, autenticando, contribuidor, dono, expirada. Papel vem do servidor.
- Atribuição: disponível, solicitando, sincronizando, atribuída, devolvendo, erro.
  Disputa simultânea: apenas um vencedor; o segundo recebe 409 e vê o responsável atual.
- Trabalho: `planned`, `in_progress`, `blocked`, `needs_test`, `released`.
  No legado: `aberta`, `em-andamento`, `precisa-log`, `precisa-teste`, `lancada`.
- GitHub: `open` / `closed` e `state_reason` independentes do trabalho.
- Release: sem candidata, candidata a versão, incluída no escopo, lançada ou retirada.
- Operação: `pending`, `applied`, `partial`, `failed`. Notificação:
  `pending`, `delivered`, `failed`; falha de notificação não desfaz atribuição aplicada.

`released` só com release publicada, referência incluída e gates de validação
satisfeitos por plataforma. O servidor rejeita salto inválido com 422. Reabrir
retira `released` e candidatura anterior só quando explicitamente solicitado.

## Escritas no GitHub

O servidor é o único escritor, com repo fixo e permissões mínimas. Revalidar issue,
permissões e SHA antes de cada operação. Preservar labels e assignees que não são
controlados pelo painel. Adotar no máximo um responsável do fluxo; se já houver
qualquer assignee, não sobrescrever silenciosamente.

| Ação | Assignee | Labels gerenciadas | Milestone | Comentário público |
|---|---|---|---|---|
| Assumir | Adicionar login autenticado e elegível | `status:in-progress` substitui outra `status:*` do painel | Preservar | Claim |
| Devolver / retirar | Remover somente responsável do fluxo | `status:planned` | Preservar | Release assignment |
| Anexar/remover commits, contexto | Preservar | Preservar | Preservar | Work update com lista final e contexto público |
| Nota privada | Nenhuma escrita | Nenhuma | Nenhuma | Nunca |
| Candidatar | Preservar | Trocar apenas `candidate:*` gerenciada por `candidate:2.0.6` | Preservar | Candidate |
| Retirar candidatura | Preservar | Remover candidata gerenciada | Preservar | Candidate removed |
| Incluir no escopo | Preservar | Remover candidata | Definir milestone existente da versão | Scope confirmed |
| Retirar do escopo | Preservar | Preservar | Remover somente milestone gerenciada, com confirmação | Scope removed |
| Alterar trabalho | Preservar | Uma de `status:planned`, `status:in-progress`, `status:blocked`, `status:needs-test`, `status:released` | Preservar | Status update |
| Fechar / reabrir | Preservar | Não inferir lançamento | Preservar | State update; definir `state` e `state_reason` explicitamente |

Candidata usa **label**, milestone fica reservado ao escopo confirmado. Não criar
milestone implicitamente ao salvar drawer. Versões selecionáveis vêm do backend;
`2.0.6` é exemplo no mock. Alterações externas via GitHub devem refletir no painel
por webhook assinado, com reconciliação periódica e timestamp de última sincronização.

### Templates em inglês

Campos entre chaves são substituições validadas, texto escapado como Markdown.
Não interpolar nota privada, decisão, validação ou identidade interna.

```text
Claim:
@{contributor} has taken this issue through the Nuvio contributor dashboard.
Status: In progress. @{owner}, a contributor is now working on this issue.

Release assignment:
@{contributor} is no longer assigned to this issue.
Status: Planned. Public reason: {reason_or_not_provided}.

Work update:
Work references updated by @{actor}.
Commits: {canonical_commit_urls_or_none}
Context: {public_context_or_not_provided}
These references do not confirm a release or device validation.

Candidate:
Proposed for {version}. This is a release candidate, not a delivery commitment.

Candidate removed:
This issue is no longer a candidate for {version}.

Scope confirmed:
Included in the planned scope of {version}. Release publication is still pending.

Scope removed:
Removed from the planned scope of {version}.

Status update:
Status changed from {old_public_status} to {new_public_status} by @{actor}.
{public_context_if_explicitly_provided}

State update:
Issue {closed_or_reopened} by @{actor}. Reason: {public_reason}.
Closing an issue does not by itself confirm a shipped fix.
```

Cada operação usa marcador oculto `<!-- nuvio-operation:{operation_id} -->` para
reconciliação de comentários sem duplicação. Um save com vários campos gera um
único comentário composto apenas pelas seções modificadas. Mudança só de nota
interna não produz comentário. Remover commit do painel não apaga comentário antigo:
publicar a lista atualizada, mantendo trilha histórica.

## Notificações ao dono

Assumir/devolver, alterações por contribuidor e falhas de sincronização entram em
uma caixa interna de eventos (issue, ator público, ação, horário, operation_id,
URL GitHub, estado). Uma menção `@{owner}` no comentário de atribuição usa as
notificações nativas do GitHub; entrega externa depende das preferências do dono.
Não declarar email entregue só porque o comentário foi criado. Caixa interna é o
registro durável; webhook confirma estado remoto, não leitura humana. Deduplicar
por operation_id; reprocessar falhas com backoff e permitir retry manual pelo dono.
Eventos privados nunca vão em menção/comentário público. Slack/Discord/email
adicionais não estão autorizados nem são necessários para a primeira integração.

## Autenticação e segredos

OAuth GitHub para contribuidores, autorização code flow no servidor, `state` de
uso único e proteção contra login CSRF. Registrar callback exato; return path só
relativo e permitido. Sessão opaca em cookie Secure, HttpOnly, SameSite=Lax, com
rotação no login e revogação no logout. Token OAuth nunca em localStorage, URL,
HTML, logs ou payload público. Solicitar somente identidade pública necessária;
nenhum escopo de escrita do repo ao contribuidor. Servidor verifica login e
possibilidade de atribuição antes de usar a credencial de escrita.

Dono identificado por allowlist de IDs GitHub imutáveis no servidor, nunca por
seletor, querystring ou nome enviado pelo cliente. Credencial de escrita: **Vaultwarden
item Nuvio painel GitHub owner token** (nome proposto, existência não confirmada).
Configuração OAuth: **Vaultwarden item Nuvio painel GitHub OAuth** (nome proposto).
Token do dono fica somente no servidor e limitado ao repositório e operações
necessárias. CLI Vaultwarden bloqueada pelo sandbox ao consultar status nesta rodada;
nenhum segredo foi lido. O agente backend deve confirmar itens e permissões.

Todas as mutações validam Origin, sessão, CSRF e autorização por objeto. Rate limit
por usuário/IP; limite de corpo e textos; SHA deve existir no repo configurado.
Não aceitar repo arbitrário, actor, assignee de terceiros ou owner enviado no body.
Webhooks verificam assinatura, delivery ID e replay; nunca confiar só no payload.

## Contrato HTTP proposto — `/api/v1`

JSON UTF-8. GET público cacheável somente com projeção pública. Respostas privadas
`Cache-Control: no-store`, sem cache compartilhado. Datas ISO 8601 UTC, IDs string,
issue number inteiro positivo, SHA canônico completo. Não usar os endpoints novos
até o backend estar pronto; `api/respostas` legado não é autorização de dono.

| Método e endpoint | Entrada | Retorno / autorização |
|---|---|---|
| GET `/session` | — | `{user:null}` ou `{user:{id,login,role},csrfToken}` |
| GET `/auth/github/start?returnTo=/issues/426` | caminho permitido | 302 OAuth; guardar intenção sem executar claim |
| GET `/auth/github/callback?code=…&state=…` | callback | criar sessão; redirect interno para confirmação |
| POST `/auth/logout` | `{}` | 204 e revogar sessão |
| GET `/issues?status=…&candidate=…&cursor=…&q=…` | filtros públicos, limit ≤ 100 | `{items:[PublicIssue],nextCursor,syncedAt}` |
| GET `/issues/{number}` | — | `PublicIssue`, ETag |
| GET `/releases` | — | `{items:[{version,milestoneNumber,state,url}]}` |
| GET `/owner/meta/commits?q={prefix}&cursor=…` | prefixo hexadecimal ≥ 7 | `{commits:{shortSha:fullSha},nextCursor}`; dono, repo fixo |
| POST `/issues/{number}/claim` | `{expectedVersion:12}` | 202 `Operation`; usuário autenticado |
| POST `/issues/{number}/unclaim` | `{expectedVersion:13,publicReason:"…"}` | 202; responsável atual ou dono |
| GET `/owner/issues/{number}` | — | `{issue:PublicIssue,internal:{note,decisions,validations},version}`; dono |
| PATCH `/owner/issues/{number}` | exemplo abaixo | 202; dono, rascunho atômico local + sincronização remota |
| POST `/owner/issues/{number}/scope` | `{expectedVersion:14,version:"2.0.6",milestoneNumber:26}` ou `version:null,milestoneNumber:null` | 202; dono; confirmar diff |
| POST `/owner/issues/{number}/state` | `{expectedVersion:15,state:"closed",reason:"completed",publicReason:"…"}` | 202; dono; reason `completed`/`not_planned`, null ao reabrir |
| GET `/operations/{id}` | — | Operation; ator da operação ou dono, sem dados privados a contribuidor |
| POST `/owner/operations/{id}/retry` | `{}` | 202 mesma operação; dono |
| GET `/owner/notifications?cursor=…` | — | `{items:[{id,issue,actor,action,operationId,createdAt,readAt,url}],nextCursor}` |
| PATCH `/owner/notifications/{id}` | `{read:true}` | 200 evento |
| POST `/webhooks/github` | payload GitHub assinado | 202 deduplicado; sem sessão browser |

```json
{
  "expectedVersion": 13,
  "commits": ["0123456789abcdef0123456789abcdef01234567"],
  "publicContext": "Fix reference; TV testing is still pending.",
  "candidateVersion": "2.0.6",
  "status": "needs_test",
  "internalNote": "Private owner note"
}
```

PATCH: campo ausente preserva; `commits:[]`, `candidateVersion:null` e texto vazio
limpam explicitamente. Até 20 SHAs distintos; texto até 2000 caracteres. O exemplo
SHA é ilustrativo; backend resolve/exige commit real. `released` exige adicional
`releaseEvidence:{releaseId,commitSha}` e gates internos existentes; 422 se ausentes.
Nunca derivar contexto público da nota privada. A atualização interna e a outbox
são transacionais, resposta registra snapshot pretendido. Não prometer atomicidade
entre múltiplas chamadas GitHub: estado `partial` informa etapas confirmadas.

```json
{
  "id": "op_123",
  "issueNumber": 426,
  "state": "pending",
  "version": 14,
  "steps": {"assignment":"pending","labels":"pending","comment":"pending"},
  "notification": "pending",
  "commentUrl": null,
  "error": null
}
```

Todas as mutações levam `X-CSRF-Token` e `Idempotency-Key` (UUID por intenção,
reutilizado em retry). Mesma chave+body retorna mesma operação, chave com outro
body retorna 409. `expectedVersion` implementa concorrência otimista. Serializar
por issue e reler GitHub antes da escrita; se a situação remota mudou, reconciliar
e pedir nova confirmação. Em timeout após escrita, buscar marcador/estado antes
de tentar novamente. Não desfazer edição alheia para “rollback”.

```json
{
  "error": {
    "code": "ISSUE_TAKEN",
    "message": "This issue is already assigned.",
    "retryable": false,
    "details": {"assignee":"octocat","currentVersion":14},
    "requestId": "req_123"
  }
}
```

Erros: 400 `INVALID_PAYLOAD`; 401 `LOGIN_REQUIRED`/`SESSION_EXPIRED`; 403
`FORBIDDEN`/`CSRF_INVALID`; 404 `NOT_FOUND`; 409 `ISSUE_TAKEN`, `VERSION_CONFLICT`,
`IDEMPOTENCY_CONFLICT`; 422 `ISSUE_CLOSED`, `ASSIGNEE_INELIGIBLE`, `UNKNOWN_COMMIT`,
`UNKNOWN_RELEASE`, `INVALID_TRANSITION`, `RELEASE_EVIDENCE_REQUIRED`; 429
`RATE_LIMITED` com Retry-After; 502 `GITHUB_ERROR`; 503 `SYNC_UNAVAILABLE`.
Mensagem segura e requestId, nunca headers/tokens ou erros brutos do provedor.
Preservar rascunho no erro; não mostrar sucesso até operação `applied`.

## Projeção pública e privacidade

`PublicIssue` é allowlist: `{number,title,url,platform,githubState,status,assignees,
candidateVersion,milestone,releaseVersion,publicContext,commits,version,syncedAt}`.
`commits` contém apenas referências deliberadamente publicadas e URLs do repo.
Decisões, recomendações, validações detalhadas, notas do dono, propostas de resposta,
"aprovada pelo dono", "responder (sem resposta)", logs privados, auditoria e estado
operacional de notificações são exclusivos de `/owner/*`. Busca pública também
não indexa notas internas. Publicar resultado de teste requer resumo público
separado, explícito e revisado; não exportar automaticamente auditoria.

**Bloqueio de publicação atual:** `gerar.py` ainda exporta o mapa completo em
`data.json`. A tela oculta áreas internas no modo visitante e não usa esses textos
nos cartões públicos, mas isso NÃO é controle de acesso. Backend/coordenador deve
substituir o export por projeção allowlist, retirar o mapa privado da pasta pública,
proteger endpoints legados, eliminar seletor de papel e revisar cache antes de
qualquer publicação. Este protótipo só serve para revisão local autorizada.

## Verificação e passagem para backend

- Provas locais: `node tools/painel-issues/tests/painel-js.mjs` e
  `python3 tools/painel-issues/gerar.py --out /Volumes/ExternalSSD/tmp/painel-site`.
- Check adicional de mock descrito em `verificaFluxoMock()` no HTML: estado novo,
  campos privados fora do cartão visitante, candidate/assignee e restauração.
- Backend deve provar: OAuth cancelado/expirado, CSRF, dono falso, notas ausentes
  no JSON público, claim duplo concorrente, assignee inelegível, webhook duplicado,
  timeout após escrita, label externa preservada, nota privada sem comentário,
  SHA desconhecido, candidatura removida, release sem prova e retry sem duplicatas.
- Revisão visual pendente: não havia navegador disponível na sessão. Conferir
  desktop/mobile, modal com Escape e foco nativo, teclado,
  filtros, confirmação de claim, drawer, candidatura, devolução e troca de perfil.
- Arte: `deploy/app/art/02.jpg` e `05.jpg`, embutidas no HTML porque o gerador
  atual só copia HTML/CSS/JS e fontes. Lucide local (user-round, link, settings-2),
  sem dependências/CDN. Acento VIOLET da paleta `src/ajustes.c`.

## Perguntas ao dono antes da integração real

1. Qual login/ID GitHub recebe as menções e qual allowlist identifica os donos?
2. Confirmar candidatura por label e milestone apenas após inclusão no escopo?
3. Confirmar nomes dos itens Vaultwarden propostos e quais versões aceitam candidatas?
