# ENTREGA — PR #381 após o merge da integração 2.0.5

Branch `agente/205-prghbarker`, worktree `/Volumes/ExternalSSD/nv-205-prghbarker`. Continuação a partir do merge já concluído `9b02841b` (pais `aae376a9` e `86613a07`). Sem push, stash, publicação ou instalação em TV. O PR #431 continua revertido.

## Uma implementação por comportamento

| Comportamento | Escolha final e motivo |
| --- | --- |
| Ajuste de créditos | Mantido `AJ_PULAR_CREDITOS` / `pularCreditosLocal` da integração, desligado de fábrica, local ao aparelho e independente do perfil. Eliminados `AJ_AUTO_CREDITOS`, `ajustes_auto_creditos` e a persistência por perfil de `autoCreditosLocal` do PR. Não há ajuste novo para acrescentar ao enum. |
| Pulo automático | Mantido o único ponto de disparo em `player_atualizar`, chamando `intro_auto_pular`, `intro_auto_ja` e `intro_auto_marcar` da integração (#329/#331, `a3cd9757`). `creditosauto.h` agora guarda somente confirmação de seek e troca de fonte; não decide nem memoriza outro pulo. |
| Validação dos créditos | Mantida `intro_janela_ok` da integração, compartilhada com o caminho manual. Incorporada a leitura estrita de limites numéricos explícitos do PR, na mesma seleção de trecho em `intro.c`. Marcadores abertos, estimativas e vizinhos não autorizam pulo automático. Capítulos do próprio arquivo continuam prioritários; AniSkip com limites explícitos continua aceito. |
| Cena depois dos créditos | O automático usa o fim dos créditos, sem emendar a prévia; o manual mantém a regra existente de emendar créditos/prévia. Mantida a política da integração: o fim deve estar pelo menos 5 s antes do fim da mídia; créditos até o fim ficam para Next. |
| Memória do pulo e recuo | Mantida a memória única por tipo de trecho em `intro.c`. Acrescentada a proteção do PR para recuo antes da chegada tardia de marcadores, incluindo cada deslocamento de `salto_quadro` e o ponteiro. O gesto usa `recuarAutomatico`, sem alterar o caminho existente de pause/seek. |
| Next | Mantida `posplay_contagem` da integração: automático desligado por padrão, 1–60 s configuráveis, padrão 5 s; marcador real permite contagem após 4 s de cartão. Eliminada a contagem fixa do PR. Acrescentado o cancelamento da contagem e do pedido automático pendente ao recuar, preservando OK manual; seek pendente suspende a mesma contagem até progresso/fim real. |
| Preparo e janela de Next | Mantidos pré-carga, duração estável, janela do player, identidade do episódio e posicionamento das legendas da integração. Não há segundo painel nem caminho de abertura do próximo. |
| Samsung WGT | Mantido `EST_BUFPCT` da integração na posição original; acrescentado `EST_FIM` do PR no fim do vetor. C/JS usam dez doubles (80 bytes); seek limpa o fim anterior. |

Comparação automatizada com o segundo pai de `9b02841b`: os 279 índices existentes e os arquivos `src/ajustes.c`, `src/ajustes.h`, `src/ajustes_ux_padrao.inc` e `src/ajustes_ux_tela.inc` são idênticos à integração. `tests/ajustes_padroes.c` e `tests/ajustes_ux_dados.c` já foram atualizados no merge para testar o padrão desligado, escopo local, ausência de `autoCreditosLocal`, opção única na tela e cauda até `AJ_PROXIMO_CONTAGEM`. `tests/ajustes_creditos.c` verifica persistência no aparelho e isolamento de conta/perfil.

## Conflitos do merge já concluído

O commit `9b02841b` registra conflitos em `src/ajustes.c`, `src/ajustes.h`, `src/ajustes_ux_padrao.inc`, `src/ajustes_ux_tela.inc`, `tests/ajustes_ux_dados.c`, `src/posplay.c`, `src/video_tizen.c` e nas 29 tabelas `src/idioma_tab.h` / `src/idioma_*.h`.

A resolução conserva os ajustes e a contagem da integração e aplica as proteções complementares do PR ao fluxo único. Nos idiomas, conserva a união das chaves dos dois pais e as traduções existentes da integração. As chaves antigas do PR permanecem nas tabelas por essa união; elas não criam opções nem lógica no app. `python3 tools/idiomas.py --sincronizar` não deixou alterações pendentes. A comparação das 29 tabelas confirmou a união e preservação das traduções da integração.

## Verificação desta continuação

Todos os 23 testes/execuções, build e verificações de idiomas abaixo terminaram com **rc 0** nesta árvore, sem correção adicional de produção necessária.

| Comando | Rc |
| --- | --- |
| `NUVIO_MAC_SO_COMPILAR=1 NUVIO_MAC_BIN=<pasta de evidências>/nuvio-mac bash tools/mac.sh` | 0 |
| `bash tests/ajustes_padroes.sh` | 0 |
| `bash tests/ajustes_ux_dados.sh` | 0 |
| `bash tests/ajustes_ux_interacao.sh` | 0 |
| `bash tests/ajustes_creditos.sh` | 0 |
| `bash tests/ajustes_perfil.sh` | 0 |
| `bash tests/player_retido.sh` | 0 |
| `bash tests/player_visto_trailer.sh` | 0 |
| `bash tests/player_fonte_duracao.sh` | 0 |
| `bash tests/abrecine.sh` | 0 |
| `bash tests/intro.sh` | 0 |
| `bash tests/posplay.sh` | 0 |
| `bash tests/posplay_recuo.sh` | 0 |
| `bash tests/creditosauto.sh` | 0 |
| `bash tests/creditos_limitados.sh` | 0 |
| `bash tests/player_recuo.sh` | 0 |
| `node tests/tizen-avplay-contract.cjs` | 0 |
| `SANITIZE=1 bash tests/creditosauto.sh` | 0 |
| `SANITIZE=1 bash tests/creditos_limitados.sh` | 0 |
| `bash tests/proximo.sh` | 0 |
| `bash tests/credfonte.sh` | 0 |
| `bash tests/player_android_retomada.sh` | 0 |
| `bash tests/player_prefs.sh` | 0 |
| `bash tests/fontecache_vod.sh` | 0 |
| `python3 tools/idiomas.py --sincronizar` | 0 |
| `python3 tools/idiomas.py` | 0 |
| `python3 tools/varredura-i18n.py` | 0 |
| Comparação dos índices, vetores, fluxo único e união de idiomas | 0 |
| `git diff --check` | 0 |

Evidências resumidas e logs dos créditos/Next/recuo em `docs/reviews/ghbarker-205-merge-evidencias/`. Logs completos e binário macOS em `/var/folders/tb/3tdgxb_151x63831hfnszg1n9cg9nz/T/nv-205-postmerge-ij1acs7g/`. O build usa configuração local incompleta de servidor e emite o aviso correspondente; é build de verificação desktop, não pacote de release. A validação de idiomas conferiu 5478 entradas × 28 idiomas; a varredura teve zero ocorrências nas três categorias.

`player_retido.sh` passou na primeira execução desta continuação; a intermitência registrada na revisão anterior não apareceu aqui, mas isso não demonstra sua eliminação. Não foram feitos build completo WGT/Android nesta continuação nem reprodução física em TV; Samsung e Android foram validados pelos respectivos harnesses.

O merge `9b02841b` foi preservado, sem reescrever histórico ou repetir um merge já integrado. Esta continuação apenas registra a validação e esclarece que a revisão anterior descreve uma árvore antiga; o comentário já pendente em `tests/ajustes_creditos.sh` foi incluído para refletir o escopo único por aparelho. O arquivo local não versionado `j-381b.log`, encontrado no início, foi preservado fora do commit.
