# Seekr 2.0.5 — chave própria

Worktree: `/Volumes/ExternalSSD/nv-205-seekr`

Branch: `agente/205-seekr-chave-propria`

Base informada: `integracao/2.0.5`

HEAD inicial: `40a2f768`

## Resultado

O Seekr continua **desligado por padrão** para quem nunca alterou a opção. O padrão já era `1` (Desligado) em `src/ajustes_ux_padrao.inc`; foi preservado e ganhou cobertura de regressão.

Cada pessoa precisa fornecer sua própria chave, guardada localmente em `seekr.txt` e exibida mascarada. Nenhuma chave de pacote é usada como fallback. Sem chave, os valores e as ajudas de ligar, chave e testar orientam:

> Use sua própria chave do Seekr, gratuita em seekr.tv.

Tentar ligar sem chave mostra esse aviso, não grava a ativação e não inicia validação ou consulta. Uma preferência antiga ligada permanece salva, mas os controles mostram o estado efetivo desligado enquanto não houver chave. Inserir uma chave própria permite o uso; removê-la desativa o uso efetivo, sem restaurar uma chave embutida.

Foram removidos os caminhos e os textos de ajustes referentes à chave padrão e ao teto de 50 consultas. Com chave própria, continuam valendo os limites do plano no Seekr. A infraestrutura antiga de quota e seus testes foram preservados: não houve refatoração desse subsistema.

## Builds e empacotamento

- `tools/env.sh` não lê mais `SEEKR_API_KEY`: produz sempre `NV_SEEKR_API_KEY` vazio tanto nos `-D` quanto no `--env-file`. A macro foi mantida por compatibilidade com os consumidores.
- Conferidos `tools/arm.sh`, `tools/tpk.sh`, `tools/tpk/x64.sh`, `tools/android.sh` e o CMake Android: os caminhos normais consomem a configuração de `env.sh`, sem outra leitura de chave Seekr.
- Conferidos os consumidores desktop e web/Tizen, incluindo `tools/mac.sh`, `tools/linux.sh`, `tools/tizen.sh`, `tools/teste-avplay.sh` e a verificação de proveniência de `tools/tizen-wgt.sh`.
- O código de ajustes ignora inclusive uma macro antiga preenchida, caso alguém use configuração de compilação externa ou obsoleta.
- Identificada e fechada também a inclusão possível de `seekr.txt` na arte ARM: `tools/arm.sh` e `tools/testa-ipk.sh` agora excluem `seekr.txt` e `seekr.txt.tmp`. Android, TPK e web/Tizen usam `tools/tizen-art.sh`, que seleciona assets e rejeita arquivos pessoais `.txt`/`.tmp`.

Não foram gerados pacotes de distribuição nem executadas builds completas dos SDKs ARM, Tizen ou Android. A conferência desses caminhos foi por leitura do fluxo e teste da configuração compartilhada; não é uma inspeção de artefatos finais dessas plataformas.

## Prova FAIL → PASS

1. `bash tests/seekr_pacote.sh` falhou antes da correção com `FAIL: chave do pacote entrou nos -D`, usando somente uma sentinela fictícia em propriedades e ambiente. Depois passou nos dois formatos de saída. A cobertura adicional acusou a ausência de `seekr.txt` na exclusão do IPK e passou após corrigir as listas.
2. `bash tests/seekrquota_settings.sh` falhou antes da correção em `!efetiva[0]`: o módulo real de ajustes recuperava a chave de pacote fictícia. Depois passou cobrindo padrão desligado, preferência antiga ligada sem chave, aviso no interruptor, controles visíveis, nenhuma validação sem chave, chave própria, máscara, persistência, falhas de escrita/remoção e ausência do contador fixo.
3. `tests/ajustes_ux_interacao.c` ganhou um cenário com o módulo e a persistência reais: sem chave, aviso e arquivo inalterado; com chave própria, ativação e recarga; remoção da chave desativa o uso efetivo.
4. `tests/seekrquota_pipeline.c` passou também com uma checagem explícita de que validação e pedido sem chave não chegam à rede. O cenário existente de chave própria continua obtendo miniaturas mesmo após a quota local antiga ter chegado a 50, sem consumir essa quota.

## Validação executada

| Comando | Resultado |
| --- | --- |
| `bash tests/seekr_pacote.sh` | PASS |
| `bash tests/seekr.sh` | PASS |
| `bash tests/seekr_estados.sh` | PASS |
| `bash tests/seekr_recorte.sh` | PASS |
| `bash tests/seekretry.sh` | PASS |
| `bash tests/seekrquota.sh` | PASS |
| `bash tests/seekrquota_pipeline.sh` | PASS |
| `bash tests/seekrquota_settings.sh` | PASS |
| `bash tests/player_seekr_status.sh` | PASS |
| `bash tests/seekr_vivo.sh`, sem chave pessoal no ambiente | PASS; API real recusou a chave fictícia, sem uso de chave do dono |
| `bash tests/seekr_shot.sh`, sem chave pessoal no ambiente | Compilou e encerrou com código 0; captura pulada pelo próprio harness por falta de chave |
| `bash tests/player_seekr_status_shot.sh /tmp/nuvio-seekr-205-status` | PASS; GL real, materiais sólido e vidro, sem consultas |
| `bash tests/ajustes_ux_dados.sh` | PASS |
| `bash tests/ajustes_ux_interacao.sh` | PASS, inclusive ativação/persistência com chave fictícia |
| `bash tests/i18n.sh` | PASS; 5.555 entradas, inglês e 28 tabelas irmãs |
| `python3 tools/varredura-i18n.py` | PASS; zero nas três categorias |
| `bash -n tools/env.sh tools/arm.sh tools/testa-ipk.sh tests/seekr_pacote.sh` | PASS |
| `git diff --check` | PASS |

A nova mensagem foi traduzida em todas as tabelas, em ordem de bytes UTF-8. Três entradas mortas sobre chave de pacote e limite de 50 foram removidas de todas elas. O validador detectou uma inserção inicialmente fora de ordem; a posição foi corrigida e a suíte de i18n foi executada novamente com sucesso.

## Limitação preexistente

A checagem extra `bash tests/ajustes_padroes.sh` falha em `tests/ajustes_padroes.c:149`, na asserção `AJ_N == AJ_PROXIMO_CONTAGEM + 1`. O enum já inclui `AJ_INTERFACE_MODO` depois dessa opção: a comparação real é `280 == 279`.

A mesma asserção foi reproduzida compilando o `ajustes.c` e os includes `.inc` do HEAD inicial, em uma pasta temporária. Portanto, a falha antecede esta alteração. O teste não foi alterado fora do escopo; o padrão específico do Seekr está coberto pelos testes que passaram.

Não houve teste positivo com credencial real de pessoa. A validação positiva, a consulta e a persistência usam chaves fictícias e rede simulada; somente o teste de recusa usou a API real.

## Commits

- `3660dc62` — `fix(seekr): remove package key from builds`
- `3864a4b5` — `fix(seekr): require personal key before enabling`
- O relatório é entregue em um terceiro commit de documentação.

Sem push, instalação em TV ou publicação. Nenhuma chave real foi impressa.
