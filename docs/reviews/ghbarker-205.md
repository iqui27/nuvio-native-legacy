# ENTREGA — ghbarker na integração 2.0.5

Branch: `agente/205-prghbarker`. Base anterior aos dois merges: `33d939d1`.

Este documento registra a revisão anterior ao merge `9b02841b` da integração 2.0.5. A decisão final de unificação, os conflitos e a verificação depois desse merge estão em [ghbarker-205-merge.md](ghbarker-205-merge.md). As referências a `AJ_AUTO_CREDITOS`, preferência por perfil e ausência de `a3cd9757` abaixo descrevem somente aquela árvore anterior.

**Resultado: #431 não entra; #381 entra com ressalvas.** Nenhum push, comentário no GitHub, stash ou instalação em TV foi feito. As cabeças foram conferidas com fetch: `pr-431=7437d872`, `pr-381=8d6ed977`; ambas de ghbarker. Os merges originais `e1b09420` e `57b3e231` permanecem no histórico.

## Escopo da revisão

Foram consultadas as descrições por `gh pr view --json` e os comandos pedidos `gh pr view N -R iqui27/nuvio-native-legacy --comments`, todos com rc 0. Os dois PRs não têm comentários, reviews ou comentários inline. Também foi consultado o #351, citado pelo #381, inclusive a resposta do dono pedindo créditos com limites explícitos e cancelamento no recuo sem rerotear seek/pause.

Foram inspecionados os intervalos pedidos `7437d872^1..e1b09420` e `8d6ed977^1..57b3e231`. Eles incluem trabalho anterior da integração: respectivamente 296 e 945 arquivos. Para atribuir achados aos PRs, a revisão usou também os deltas efetivos dos merges: `e1b09420^1..e1b09420` (130 arquivos) e `57b3e231^1..57b3e231` (57 arquivos), além dos commits originais do autor.

## PR #431 — não entra

O APK padrão passa a definir `NV_TOUCH_UI`, mas vários caminhos tratam essa capacidade compilada como se fosse o modo Mobile escolhido pela pessoa. Corrigir isso exige revisar o isolamento em diversas telas e validar a experiência com controle remoto. Não é um patch pequeno.

Os locais abaixo são do estado **`e1b09420`**, antes do revert:

| Gravidade | Local | Achado |
| --- | --- | --- |
| Bloqueio | `android/app/src/main/cpp/CMakeLists.txt:81`, `src/agendaui.c:196` | Com `NV_TOUCH_UI` no APK normal, `agColunaUnica()` retorna verdadeiro em 1920×1080 porque 16:9 >= 1,7, mesmo com modo TV salvo. A Agenda passa a usar a disposição do telefone. Falta a condição de modo Mobile. |
| Bloqueio | `src/busca.c:1185`, `src/busca.c:1459` | O passo das colunas e o desenho do cabeçalho dependem de `NV_TOUCH_UI`; no segundo caso o caminho Android deixa de usar o kicker da TV mesmo quando Mobile está desligado. |
| Bloqueio | `src/ajustes_ux_ilha.inc:744` | Os botões de toque substituem as dicas do controle quando `ajPontEditor` está ativo, sem testar Mobile. O modo TV também entra no retorno antecipado. |
| Build | `tools/android/ffmpeg-android.sh:77` | Array `ASM=()` com `set -u` quebra no Bash 3.2 do macOS: `ASM[@]: unbound variable`. Build pedido terminou rc 1. A expansão protegida foi experimentada localmente e permitiu avançar, mas não foi mantida depois de retirar o PR. |
| Build | `src/faixas.c:1731` | Após contornar o array vazio, o build arm64 falha: `ponteiroAtrasoPasso` é usado mas não existe. O build com os dois PRs terminou rc 1. |
| i18n | `src/pipintro.c:188`, `src/pipintro.c:190`, `src/pipintro.c:192`, `src/ctxmenu.c:1571` | Quatro strings novas sem chave: instruções da miniatura/Guia e “Toque para aplicar”. A varredura inicial terminou rc 1. Foram experimentadas entradas com fallback inglês, mas a alteração saiu junto com o PR. |
| Testes | `tests/android_window_layout.py:18` | O teste existente extrai apenas `aplicarJanela`, que agora depende de `larguraLogica`/`alturaLogica`; a fixture JVM não as contém. Rc 1 por referências não resolvidas. |

O enum foi corretamente anexado depois de `AJ_TELA_ABERTURA`; o modo nasce TV e está excluído da sincronização/perfil. Esses pontos não compensam as regressões acima.

**Ação:** `c187ef53`, revert do merge com parent 1. O merge já estava concluído; não havia merge em andamento para `git merge --abort`. Os conflitos foram resolvidos mantendo o #381. O delta final de `android/` e das ferramentas Android contra `33d939d1` é zero. A autoria permanece no histórico, embora os efeitos do #431 estejam fora da árvore final.

**Para o dono:** decidir quando retomar a funcionalidade; exigir isolamento explícito por modo nas telas, restaurar os testes de modo TV/Mobile e validar em Android TV antes da entrada. Não foi feita a correção transversal. LG e Samsung voltam ao código anterior ao #431; não houve teste físico dessas TVs.

**Rascunho em inglês, não publicado:**

> I reviewed this against our 2.0.5 integration. We cannot include it yet: enabling NV_TOUCH_UI in the standard APK also changes TV-mode Agenda, Search and Settings behavior without checking the saved Mobile mode. The macOS build first fails on an empty Bash array, then on the missing ponteiroAtrasoPasso callback; four UI strings also need translation keys. Please isolate the runtime Mobile paths and restore TV-mode regression coverage before rebasing. Your original commit and authorship are preserved in our review history.

## PR #381 — entra com ressalvas

Mantido pelo merge `57b3e231`, com correções próprias separadas. Não se copiou o código do autor para um commit nosso.

| Local final | Achado e ação |
| --- | --- |
| `src/player.c:3367` | O recuo contínuo da 2.0.5 é aplicado por `salto_quadro`; o PR só registrava o primeiro toque de 10 s. Marcadores recebidos depois podiam fazer o trecho atravessado na rajada ser pulado novamente. `717d3f59` registra cada deslocamento negativo com a função já existente. `tests/player_recuo.sh` falhou rc 134 antes e passou rc 0 depois. |
| `src/intro.c:712` | A lista de limites explícitos ignorava `intro_janela_ok`, aceitando outro corte/meio do episódio que o botão manual da integração recusava. `ac19eb04` reutiliza essa validação sob a trava existente. O teste de janela falhou rc 134 antes e passou rc 0 depois, também com ASan/UBSan. |
| `src/ajustes_ux_visual.inc:144` | A nova opção não tinha ícone no mapa explícito; `ajustes_ux_interacao.sh` falhava rc 134. `c4e40c05` reutiliza `aj_circle-play`. |
| `tests/ajustes_ux_dados.c:678` | A asserção de cauda ainda exigia `AJ_N` logo após `AJ_TELA_ABERTURA`; falha rc 134. Atualizada para a opção acrescentada, preservando a ordem anterior. |
| `tests/ajustes_creditos.c:64`, `tests/creditos_limitados.c:36`, `tests/posplay_recuo.c:1` | Fixtures não acompanhavam APIs atuais de idiomas/log/rede/preparo do próximo. Antes havia erros de link e inclusão SDL. Foram adaptadas; o teste de posplay reutiliza o roteiro app-linked existente e elimina a duplicação de tipos SDL/desenho. |

**Comportamento mantido:** opção desligada por padrão, local ao perfil neste aparelho, fora do blob da conta; início e fim explícitos; uma tentativa por intervalo; troca de fonte desativa o automático nessa reprodução; recuo cancela o Next automático, preservando o OK manual; Samsung passa o fim nativo pelo contrato de nove doubles. Os 272 índices anteriores do enum foram comparados e preservados; `AJ_AUTO_CREDITOS` é a única nova entrada, após `AJ_TELA_ABERTURA`.

### Créditos próprios #329/#331, sem duplicação

O commit próprio `a3cd9757` existe em outro branch, mas **não é ancestral deste branch**. A busca em `player.c`, `intro.c` e `posplay.c` da base encontrou o botão manual, as regras de janela/créditos e o cartão Next; não havia um segundo pulo automático de créditos aqui. Ficou uma única implementação automática: a do #381, aproveitando a validação da integração.

Há diferenças a resolver **antes de trazer futuramente aquele outro branch**:

- A implementação própria usa `ajustes_pular_creditos` e `intro_auto_*`; não mesclar esse segundo fluxo/ajuste em paralelo com `ajustes_auto_creditos`/`CreditosAuto`.
- O #381 aceita fim explícito exatamente na duração da mídia, esperando o fim nativo antes do Next; `a3cd9757` exige pelo menos 5 s restantes e deixa os créditos até o fim para o Next. A política de unificação cabe ao dono. A árvore atual mantém a política descrita no #381.
- O #381 mantém preferência por perfil/aparelho; o outro caminho tem escopo diferente. Não foi importado código do outro branch.

A ressalva operacional é a ausência de reprodução física LG/Samsung/Android TV. O contrato Samsung foi executado no harness, não numa TV. As quatro strings do #381 estão na tabela mestra e nas 28 tabelas irmãs com fallback inglês; tradução nativa completa fica pendente.

**Rascunho em inglês, não publicado:**

> Integrated through your original merge, with small follow-up commits for our 2.0.5 code. I fixed continuous held-key rewind tracking and reused our existing credits-window validator so automatic skips reject markers from another cut. I also updated the settings icon and fixtures for current APIs. Focused credits/profile/Next tests, i18n checks, the Samsung bridge contract and Android ARM64/ARMv7 debug/release builds pass. Physical TV playback remains unverified. Our separate credits branch is not merged here; we retained one implementation and documented the policy differences for its later integration.

## Verificação e códigos de saída

Evidências em `docs/reviews/ghbarker-205-evidencias/`. Linhas de compilação com credenciais locais foram removidas dos logs arquivados.

| Comando/check | Rc final | Observação |
| --- | --- | --- |
| `bash tests/ajustes_padroes.sh` | 0 | Obrigatório. |
| `bash tests/ajustes_ux_dados.sh` | 0 | Obrigatório; cauda append-only corrigida. |
| `bash tests/ajustes_ux_interacao.sh` | 0 | Ícone corrigido. |
| `bash tests/ajustes_perfil.sh` | 0 | Persistência por perfil. |
| `bash tests/ajustes_creditos.sh` | 0 | Padrão desligado, isolamento da conta, perfil antigo/novo, logout. |
| `bash tests/player_retido.sh` | 134 / 0 | Obrigatório: uma execução falhou intermitentemente na linha 118, caso de clipe curto; repetição passou. Também passou com `player.c` de `33d939d1` e restante da árvore atual. Isso não prova a origem da intermitência; não foi alterada a lógica de retenção. |
| `bash tests/abrecine.sh` | 0 | Obrigatório. |
| `bash tests/creditos_limitados.sh` | 0 | Parser, publicação concorrente e validação da janela. |
| `SANITIZE=1 bash tests/creditos_limitados.sh` | 0 | ASan/UBSan. |
| `bash tests/creditosauto.sh` | 0 | Limites, cenas, recuo, fonte e repetição. |
| `SANITIZE=1 bash tests/creditosauto.sh` | 0 | ASan/UBSan. |
| `bash tests/intro.sh` | 0 | Marcadores existentes/AniSkip/fixtures. |
| `bash tests/player_recuo.sh` | 0 | Regressão do recuo contínuo. |
| `bash tests/posplay_recuo.sh` | 0 | Executa também o roteiro existente de `tests/posplay.c`; Next, cancelamento, amostra velha e OK manual. |
| `bash tests/player_android_retomada.sh` | 0 | Retomada Android, episódio, perfil e retido. |
| `bash tests/player_visto_trailer.sh` | 0 | Preserva correção da integração contra vistos falsos. |
| `node tests/tizen-avplay-contract.cjs` | 0 | Fim nativo, seek e contrato C/JS Samsung. |
| `python3 tools/varredura-i18n.py` | 0 | Nenhum literal novo sem chave. |
| `python3 tools/idiomas.py --sincronizar` | 0 | Executado; sem mudança pendente. |
| `python3 tools/idiomas.py` | 0 | 5404 entradas × 28 idiomas. |
| Verificação dos índices do enum | 0 | 272 entradas antigas preservadas. |
| `git diff --check` | 0 | Sem erro de whitespace. |
| Build Android pedido, árvore final | 0 | Debug e release, arm64-v8a e armeabi-v7a, motor P2P nas duas arquiteturas. |

O build final executou exatamente:

```sh
TMPDIR=/Volumes/ExternalSSD/tmp \
NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties" \
NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/nv-203-shield-cache \
bash tools/android.sh
```

Artefatos locais: `build/android/Nuvio-2.0.4-android-debug.apk` e `build/android/Nuvio-2.0.4-android.apk`. O nome ainda é 2.0.4 porque é a versão atual em `deploy/app/appinfo.json`; não foi alterada a versão para esta revisão. Nada instalado.

Antes do revert do #431 também passaram `ponteiro.sh`, `ponteiro_toque.sh`, `ponteiro_telas.sh`, `perfilsel_toque.sh`, `video_android_janela.sh` e `video_android_retomada.sh` (rc 0). Isso não cobria os caminhos Android compilados com `NV_TOUCH_UI`; por isso não foi usado como prova de ausência de regressão no modo TV.

## Commits próprios

- `c187ef53`: retira os efeitos do #431, preservando merges/autoria e #381.
- `c4e40c05`: ícone e adaptação dos testes do #381.
- `717d3f59`: recuo contínuo completo, com teste vermelho/verde.
- `ac19eb04`: validação da janela já existente, com teste vermelho/verde e sanitizadores.

Limites: sem suíte completa de todas as telas, sem teste físico em TV e sem investigação conclusiva da intermitência de `player_retido.sh`. O #431 exige trabalho transversal antes de reconsiderar a entrada; o #381 fica pronto para revisão do dono com essas ressalvas.
