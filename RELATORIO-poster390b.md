# Relatório — poster390b / issue #390

Worktree: `/Volumes/ExternalSSD/nv-205-poster390b`.
Branch: `agente/205-poster390b`. Base informada: `integracao/2.0.5`.
Escopo: modelo próprio de URL de pôster no Samsung QN74F, Tizen 9. O limite `PP_MODELO_MAX = 401` já existente foi mantido.

## O que mudou

1. **Aliases de marcador** — `src/posterprov.c` aceita `{imdb_id}` como `{imdb}`, `{tmdb_id}` como `{tmdb}` e `{id}` como IMDb válido (`tt…`) se disponível, senão TMDB numérico. A validação, montagem e cálculo de pior caso reconhecem os três aliases: 23 bytes para `{imdb_id}`/`{id}` e 19 para `{tmdb_id}`. O comentário público em `src/posterprov.h` e a mensagem `PST_MODELO_RUIM` foram atualizados. A tabela mestra de i18n e as 28 tabelas irmãs receberam os aliases, preservando a ordem de bytes e as traduções existentes.
2. **Aviso imediato** — todas as recusas de `pstDefinir` convergem para `uxNotificar(pstTexto(AJ_POSTER_TESTAR))`. É o aviso existente de Ajustes, com duração de 4,5 segundos; a indicação na linha Testar também continua disponível. Teclado da TV e texto recebido do celular passam pelo mesmo setter. Não há tela ou mecanismo de notificação novo.
3. **Log redigido** — cada recusa de `pstDefinir` imprime uma linha com motivo, comprimento do texto recebido e URL redigida por `posterprov_redigir`. O sucesso imprime `[poster] modelo salvo (n=<len>)`. As linhas são descarregadas com `fflush(stdout)`. Entradas sem esquema HTTP(S) são omitidas antes da redação, pois o redator existente devolveria uma chave nua inteira. Caminho, query e userinfo não aparecem no log de recusa.

Exemplo de recusa:

```text
[poster] modelo recusado: sintaxe invalida (n=34) https://meu.servidor.invalid/...
```

## Testes e códigos de retorno

`tests/poster_modelo_longo.c` foi estendido sem adicionar dependências. O gancho de teste agora recebe texto pelo filtro real de sistema/celular, conclui o teclado com evento OK e consome o resultado em `ajustes_atualizar`, antes de reler `posteres.txt`.

| Verificação | Antes da correção correspondente | Depois |
| --- | --- | --- |
| Aliases: gravar/reler, montar URL correta, preferência de IMDb, fallback TMDB, dado ausente e pior caso de 511/512 bytes | FAIL, rc=1, 13 falhas | PASS, rc=0 |
| Aviso após Done/OK: recusa por sintaxe e recusa por tamanho | FAIL, rc=1, 2 falhas | PASS, rc=0 |
| Captura de stdout: uma linha por recusa, todos os motivos, chave no caminho/query/userinfo, entradas sem esquema, recusa por tamanho e log de sucesso | FAIL, rc=1, 9 falhas | PASS, rc=0 |

| Comando executado | Resultado |
| --- | --- |
| `bash tests/poster_modelo_longo.sh` | PASS, rc=0; também conserva os casos de 392/400 caracteres e a leitura de modelos de versões anteriores |
| `bash tests/posterprov.sh` | PASS, rc=0; inclui `posterprov` e `posteraddon` |
| `bash tests/poster_url_longa.sh` | PASS, rc=0; URLs de 600, 1173 e 2000 bytes, persistência e transporte simulado |
| `bash tests/i18n.sh` | PASS, rc=0; 5557 entradas × 28 tabelas irmãs, ordem conferida e zero ocorrências nas três varreduras |
| `git diff --check` | PASS, rc=0 |

Os scripts de `posterprov` e URL longa emitiram avisos de compilação em código fora do diff, sem impedir a execução. Os logs das execuções ficaram em `/tmp/poster390b-*.log`; são evidências temporárias locais.

## Commits

Exatamente um commit por item:

| Item | Commit | Mensagem |
| --- | --- | --- |
| 1 | `1d6215d8` | `Aceita aliases de identificador no modelo de poster (#390)` |
| 2 | `6e2d9f0a` | `Mostra a recusa do modelo de poster ao concluir o teclado (#390)` |
| 3 | `HEAD` na entrega | `Registra o resultado do modelo de poster sem expor a chave (#390)` |

Este relatório integra o terceiro commit. Seu identificador é indicado por `HEAD` para evitar registrar no próprio conteúdo um hash que mudaria ao incluí-lo; `git log -3 --oneline` mostra os três hashes finais.

## Limites da validação

Validação local no macOS, sem janela e com transporte simulado onde previsto pelos testes existentes. O teste confirma o estado do aviso usado pelo desenho de Ajustes; não foi feita inspeção visual em TV nem teste do transporte real entre celular e TV. Não houve push, instalação na TV ou publicação.
