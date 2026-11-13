# Lote F integrado — perfil, controles e legendas

Pedido do dono: trazer o que falta do fork Matis94 para a integração de TV. Merge `26eec9dbbc52ef087e2faa2758a9187e23a29824`, candidato `1338812bec9891c9d62d25cc5bb8423515ee5b59`, base `e63757fd6712b802c03fadd3560bd6372e13fb59`. Cobre F17, F18, F19 e D12 do [plano](STATUS.md). Sem publicação de release, push, deploy de serviços, instalação ou escrita em conta. O hook documental existente atualiza painel e roadmap público automaticamente.

## Comportamento incorporado

- Lembrar último perfil é opt-in, desligado por padrão e local à TV. Perfil removido, com PIN ou bloqueado por horário não entra automaticamente; mudanças posteriores de sincronização reabrem o seletor quando necessário.
- Botões de intro, recap e créditos têm controles independentes, ligados por padrão e por perfil. Detecção e skip automático mantêm seu comportamento.
- Desligar avisos de atualização suprime card automático, anúncios e avisos já enfileirados; busca manual continua funcionando.
- Opacidade de legenda em passos de 5%, preservando os IDs antigos de 100/75/50/25%. Conversões entre posição na lista e valor salvo abrangem presets, revisão/undo, player, LG e mpv. Opacidade secundária migra chave antiga em configuração local, backup e arquivos individuais de perfil; chave nova prevalece em qualquer ordem, zero continua seguindo a principal.
- Strings novas traduzidas em todas as tabelas. Pequena correção no editor já existente de fonte da biblioteca evita ciclar para serviço desativado; três linhas usam ícones existentes de suas seções. Sem layout novo.

Commits principais: `8b0ab5919`, `f91dc1ee4`, `fdd4bf41d`, `ef6043075`, `42a2a2088`, `1d84ed871`. O último incorpora a correção de migração por perfil exportada do snapshot remoto; snapshot inicial não foi importado.

## Prova, revisão e gate

F1-profile foi bloqueio real: `perfilLerVetor` ignorava `legenda2OpacidadeLocal` em `ajustes-p<N>.txt`. Regressão com configuração descartável falhou no código anterior com rc 134, passou após a correção e cobre troca/edição/migração de perfil, valores antigos 0–4 e precedência da chave nova. Evidência privada: `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/f3-host-evidence.json` (inclui hashes), `f3-red-before.log` e `j-fix-f3-zima.log`.

Revisor `j-rev-f3-zima`; verificador distinto `j-verify-f3-zima`. F1-profile e F3 históricos foram retirados após conferir as correções. F2 sobre sincronização de legenda principal foi retirado por leitura independente: filtro local à TV já impedia escrita em conta/backup/perfis; não foi uma nova correção do produto.

SHA-256 do diff revisado: `af6a75ddb497e046c463de398043cbc8edb0c46d8230e323ec6b5ca90c559aaf`. Zima fez revisão/verificação por leitura, sem executar testes. Os relatórios atribuem explicitamente os testes anteriores ao host em `1d84ed871742108293e273567179da94d19b8dbb`. O gate da Central executou depois os oito comandos no candidato atual `1338812b…`: ajustes_ux_interacao, ajustes_escopo, update_manual, update_notice, skip_button_settings, perfilsel, i18n e build-local. Resultado 8/8, nenhum ignorado, timeout ou divergência; agreement verdadeiro. Digest de verificação do merge: `cdc300fc66fa5ae7511aece6797daa412643eb64787c47c8fe35193e59916340`. i18n automático do integrador passou.

## Pós-merge

HEAD `26eec9dbbc52ef087e2faa2758a9187e23a29824` idêntico antes/depois; TMPDIR próprio no SSD externo, `NUVIO_PROPERTIES=/dev/null`.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/ajustes_ux_interacao.sh` | 0 | `4b0bcc0eb583bcaa3536f2bfe1fe69a310b16a02f9acb59e685b39963dec1598` |
| `bash tests/update_manual.sh` | 0 | `b07196782d1217c7ea9c5d5213e3453bb0b9698e986c4f1a83794c9b21b8a650` |
| `bash tests/i18n.sh` | 0 | `a065f1ce412d73e023c75c1917a8cdfa753ba14f7ad18314cfd64f86ef182596` |

Manifesto `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/f-postmerge-evidence.json`; relatórios `j-rev-f3-zima.log` e `j-verify-f3-zima.log` no mesmo diretório. Livro de merges `/Volumes/ExternalSSD/tmp/rev/jobs.jsonl`.

## Notas P2 e limites

- F4 confirmado: opção de lembrar perfil ligada, último perfil removido e um único perfil desbloqueado restante ainda mostram seletor.
- F5 confirmado: arquivo de perfil anterior sem as novas chaves de skip pode herdar OFF do perfil ativo na primeira troca; depois o valor fica persistido. Recuperável pelos controles existentes. Não bloqueou o merge.
- Nenhuma prova física em TV, captura de UI ou ABI nesta rodada. Build/testes host não comprovam funcionamento em aparelho.
