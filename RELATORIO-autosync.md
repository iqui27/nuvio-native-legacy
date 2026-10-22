# Autosync de legenda na Samsung — correção local

Branch: `agente/205-autosync-ref`. Worktree: `/Volumes/ExternalSSD/nv-205-autosync`. Base: `integracao/2.0.5`, merge-base `13e4489b103a3609f7ca245c0ae75414a9eb9d6e`.

Nenhum push, instalação em TV ou publicação. Os logs originais foram apenas lidos localmente; URLs de addon/debrid não foram copiadas para este relatório, testes ou commits.

## 1. ASS embutida com referência vazia

### Evidência e causa

Em `72470.txt`:

- Linha 515: a faixa 5 tem 15.907 blocos; mkvass trabalha somente na janela do playhead.
- Linha 590: o coletor encerra com 87/15.907 blocos e grava sidecar parcial.
- Linha 619: `reference unavailable reason=budget track=5 events=0/0 requests=3 bytes=2253612`.
- Linhas 620–621: autosync encerra sem referência e sem trocar a externa.

O código do legref limita os pontos a 7.999. Sua capacidade cresce até 8.192; o próximo crescimento recusa o índice antes da leitura dos blocos. Portanto, a cardinalidade de 15.907 pontos já basta para explicar `budget` sem eventos. Os três pedidos são compatíveis com cabeçalho e leitura do índice. O log não demonstra que clusters HEVC grandes sejam a causa, e não foi necessário aumentar o orçamento nem varrer o vídeo.

### Mudança

`mkvass.c` conserva um snapshot imutável dos eventos locais de ASS cujo índice tem pelo menos 8.000 pontos. A publicação acontece no worker, com progresso e intervalo entre snapshots, e também ao encerrar a coleta. O snapshot sobrevive à escolha da legenda externa, que para o mkvass. Outra fonte/faixa invalida o estado anterior.

A referência é a maior sequência contígua de pontos efetivamente colhidos, com limites conservadores nos tempos dos pontos vizinhos. Pontos não colhidos, corpo/índice truncado, varredura, texto simples e faixas forced/letreiro/comentário/codificadas não passam por esse caminho. O parser também precisa concluir sem truncamento. A identidade contém somente o TrackNumber.

`legref.c` consulta uma fonte local registrada pelo player antes de qualquer Range. O coletor continua independente do overlay; não chama mkvass diretamente nem troca legenda. Sessão e faixas excluídas são respeitadas. O log distingue `reference window` de `reference complete`.

`LEGENDA_DOC_JANELA` indica que a completude se refere somente ao intervalo declarado. A engine compara a externa dentro desse intervalo, com margem de 10 segundos e busca de atraso limitada a ±10 segundos. Mantém os portões de confiança, repetição, letreiros, quantidade de falas e cobertura; janela com menos de 80 segundos não corrige. Só permite offset puro, sem extrapolar fps, cortes ou âncoras a partir de uma janela. Faixas menores e referências completas continuam pelo caminho anterior.

### FAIL → PASS

Teste: `bash tests/mkvass_referencia.sh`.

- FAIL registrado antes da implementação: assert `72470: 87/15907 blocos locais devem servir como janela de referencia`.
- PASS: fixture local com TrackNumber 5, índice de 15.907 pontos e 87 eventos contíguos entrega referência não vazia à engine e aceita atraso conhecido de +2.500 ms.
- Resultado observado: `reference window reason=ok track=5 events=87/87 requests=0 bytes=0`.
- Proteções exercitadas: outra URL, faixa excluída, buraco no índice, janela curta, fonte inválida, corpo truncado, geração obsoleta, faixa abaixo de 8.000 pontos e externa com eventos distantes fora da janela.
- O mesmo teste passou com `SANITIZE=1` (ASan/UBSan).

A fixture usa tempos/textos sintéticos com estrutura indexada equivalente; não é reprodução binária do arquivo MTBB. Os 87 blocos reais podem incluir camadas repetidas, letreiros e poucas falas. O reaproveitamento não garante que a engine aceite esse arquivo real; recusar uma janela insuficiente é intencional.

## 2. Queda da conexão / retries

### Investigação

A hipótese de disputa de conexões é plausível em geral, mas não foi provada por estes logs:

- `72470.txt:590`: mkvass já estava encerrando e gravando o parcial.
- `72470.txt:619`: legref já encerrou a leitura.
- `72470.txt:737`: consulta de posição do player levou 5.000 ms.
- `72470.txt:741`: relógio em 224,255 segundos não andava há 15 segundos; recuperação foi acionada.
- `72470.txt:743`: erro `0xfffffffe`, tentativa 1/3, espera de 3 segundos.
- `72470.txt:750`: reconexão para 224 segundos.
- `72470.txt:762–765`: parada não confirmou, player ficou indisponível e veio `0xfffffffd`, tentativa 2/3.
- `72470.txt:770`: saída do player.

Não há captura de conexões simultâneas, resposta do servidor associada à queda ou comparação A/B sem coletores. A sequência documenta travamento do backend e falha da recuperação; não atribui a causa ao legref. `72480.txt` começa com um retry remanescente e depois mostra fontes só com PGS; `72510.txt` confirma `no_text_track`; `72518.txt` só traz partida e não acrescenta prova causal.

### Mitigação mínima e FAIL → PASS

Foi usada a mesma fonte local do item 1: quando uma janela local da ASS enorme está disponível, legref não abre conexão auxiliar para reler a faixa. Não foi alterada a política de retry de vídeo nem adicionada uma serialização global sem prova de necessidade. Os tetos de rede existentes permanecem: 12 MiB por leitura, 24 MiB por sessão, limite de pedidos e pausas por seek/buffer.

Teste-alvo do item 2, isolando a ligação da fonte local:

- FAIL: `TESTE_SEM_FONTE_LOCAL=1 bash tests/mkvass_referencia.sh` abre um pedido e falha no assert `nao abrir conexao extra para a mesma faixa local`.
- PASS: `bash tests/mkvass_referencia.sh` entrega a referência com zero chamadas ao leitor, zero pedidos e zero bytes, inclusive com leitor de rede que recusaria qualquer pedido.

Essa prova cobre a remoção da leitura duplicada, não a eliminação dos erros de vídeo. Se o snapshot ainda não estiver disponível ou não contiver eventos utilizáveis, o legref conserva seu fallback limitado de rede.

## 3. Arquivo só com PGS, sem referência por áudio

### UI antes

`legsync_visao` mapeava `LEGREF_SEM_FAIXA` para motivo genérico. A apresentação detalhada dizia que não havia texto indexado, mas a linha automática de `legsync_texto_simples` priorizava `autoFase == 3` e mostrava somente “Não deu para sincronizar”. A pílula final informava legenda aplicada/não sincronizada, sem causa.

### UI depois

`LEGREF_SEM_FAIXA` ganha motivo de UI específico. A linha automática e o estado detalhado mostram:

> Sem legenda de texto neste arquivo para usar como referência

A pílula mantém seu comportamento. Não há troca automática de legenda nem alteração do atraso manual quando falta referência. O motivo também pode abranger arquivos sem faixa de texto elegível, por exemplo somente forced; não afirma capacidade de áudio.

A chave foi incluída na tabela inglês e nas 28 tabelas irmãs, em ordem de bytes UTF-8. Sérvio segue o alfabeto latino adotado pela tabela existente.

### FAIL → PASS

Teste: `AUTOSYNC_PGS=1 bash tests/legsync.sh`.

A fixture preserva a estrutura EBML/Tracks de MKV real e substitui o CodecID de texto por PGS. Não pretende validar decodificação de imagens: testa a classificação da referência e o texto publicado pelo provedor da UI.

- FAIL anterior: `reference unavailable reason=no_text_track`, plano termina em fase 3 e o assert da mensagem específica falha.
- PASS: provedor da linha de sync e apresentação manual retornam a frase acima; atraso manual de 320 ms permanece 320 ms; nenhuma troca ocorre.

## Validação executada

Todas as verificações abaixo passaram:

- `bash tests/legsync.sh`: suíte de sessão/engine/UI e regressões TCL 1, 2 e 3.
- `bash tests/legref.sh`: 26 casos, mais HTTP real local, redirect, recusa de Range e cabeçalhos.
- `bash tests/audsync.sh` e `bash tests/autosync.sh`.
- `bash tests/mkvass.sh`: suíte real de extração, fontes, cache, varredura, prebusca e falhas HTTP; validação `ass_tempos` também passou.
- `bash tests/mkvass_alem_teto.sh`, `mkvass_grande.sh`, `mkvass_limites.sh`, `mkvass_pool_obsoleto.sh`, `mkvass_quadro.sh`, `mkvass_recusa.sh`, `mkvass_requests.sh`, `mkvass_texto.sh`, `mkvass_tracks_grande.sh` e `mkvass_url.sh`.
- `SANITIZE=1 bash tests/mkvass_referencia.sh`.
- `AUTOSYNC_PGS=1 bash tests/legsync.sh`.
- `bash tests/i18n.sh`: 5.558 entradas × 28 tabelas, busca/ordem/formatos e varredura sem problemas.
- `python3 tools/varredura-i18n.py`: zero ocorrências nas três categorias.
- `bash tools/build-local.sh`: build de verificação local com credenciais vazias.
- `git diff --check`.

A primeira execução de `mkvass_texto.sh` revelou um stub de teste já incompleto: `faixas_texto.c` não definia `legsync_manual_seq`, utilizado por faixas.c. Foi acrescentado apenas o stub constante; a suíte então passou. Há warnings preexistentes de inicializador/variáveis nos testes, sem falha de compilação.

Os scripts `*_shot.sh` são capturas com janela GL e olho humano, explicitamente fora das suítes; não foram executados. Não houve prova visual em TV, novo log Samsung, reprodução do fluxo do servidor real nem teste de throughput no aparelho. O build local não equivale a build/execução Tizen. Esses limites impedem afirmar que a queda real foi resolvida ou que a legenda MTBB real será aceita.

## Rodada 2

### Integração e escopo

Primeiro foi executado `git merge integracao/2.0.5`, sem conflitos, gerando `c1b1f067` com pais `7fa87017` e `5efd0be4`. As mudanças dos dois lados foram preservadas; nenhuma mudança da integração foi revertida. Esta rodada corrige os dois P1 de `/Volumes/ExternalSSD/tmp/rev/REV-autosync.md`, reutilizando as provas `prova-troca.c` e `prova-janela.c` nas suítes existentes em `tests/`.

### P1: recorte insuficiente autorizava troca

`autosync_alinhar` valida primeiro a externa inteira. Para referência com `LEGENDA_DOC_JANELA`, prepara depois o recorte com margem de 10 segundos. Se somente esse recorte não sustenta a comparação, marca `referenciaInvalida`, chegando ao portão existente `autoFalhou(semReferencia)` do legsync. Falhas globais da externa continuam distintas; a política anterior para uma externa globalmente insuficiente permanece coberta pela regressão TCL 2.

FAIL → PASS em `bash tests/legsync.sh`:

- Antes da correção, a externa correta `emb.srt` tinha 113 falas, a referência local em `[0, 120]` tinha 20, e a margem deixava somente 18 na externa. A engine recusava com `sparse_dialogue`, o legsync anunciava “referencia recusou; troca uma vez” e o assert `trocas == 0` falhava com `trocas=1`.
- Depois, a mesma regressão encerra com `trocas=0`, fase automática 3, sem retorno à original nem troca sinalizada, offset automático zero e atraso manual de 320 ms preservado.
- `tests/autosync.c` também verifica recorte com poucas falas, span curto e nenhuma interseção: os três recusam com `referenciaInvalida` e offset zero. Uma externa globalmente esparsa continua recusando sem marcar falha da referência.

### P1: modo CD1/CD2 escapava do raio local

A ampliação parcial CD1/CD2 é desativada para `LEGENDA_DOC_JANELA`. Antes do aceite, o offset refinado em segundos é comparado ao raio configurado, limitado a 10.000 ms; não depende apenas do arredondamento do offset aplicado. A verificação ocorre depois do alinhamento/refino e exige também um único trecho com escala 1.

FAIL → PASS em `bash tests/autosync.sh`:

- A prova da janela `[1000, 1120]`, 40 falas concentradas e duas nas bordas foi incorporada ao teste. Antes, o worker aceitava e aplicava `30000 ms`, fazendo falhar o assert de recusa/offset zero.
- Depois, os modos rápido e completo recusam essa mesma entrada com `low_confidence`, `offsetMs=0` e `autosync_offset_ms=0`.
- Um teste adicional exercita o portão final: +10.000 ms é aceito; +10.100 ms chega ao refino com cobertura/confiança altas, mas é recusado com `region_disagreement` e offset aplicado zero.

### Validação desta rodada

Todos os comandos abaixo terminaram com código 0:

| Comando | Resultado |
| --- | --- |
| `bash tests/mkvass_referencia.sh` | ASS 87/15.907, +2.500 ms, zero Ranges auxiliares; proteções aprovadas |
| `SANITIZE=1 bash tests/mkvass_referencia.sh` | Mesmos resultados, sem diagnóstico ASan/UBSan |
| `bash tests/legsync.sh` | 21 casos, incluindo a nova regressão; TCL 1, 2 e 3 aprovados |
| `AUTOSYNC_PGS=1 bash tests/legsync.sh` | Mensagem específica, nenhuma troca, atraso manual preservado |
| `bash tests/legref.sh` | 26 casos e HTTP local: completo, redirect, recusa de Range e cabeçalhos |
| `bash tests/autosync.sh` | 572 verificações de corpus/estado/parser |
| `bash tests/audsync.sh` | 23 casos |
| `bash tests/i18n.sh` | 5.556 entradas × 28 idiomas após o merge, busca real e varredura aprovadas |
| `git diff --check` | Sem erro de whitespace |
| `git merge-base --is-ancestor integracao/2.0.5 HEAD` | Integração incorporada ao histórico |

Logs FAIL → PASS e das suítes: `/Volumes/ExternalSSD/tmp/rev/rodada2-*.log`. A primeira tentativa do novo teste legsync terminou sem plano automático; o harness foi corrigido para habilitar o modo automático de teste. O FAIL funcional registrado acima foi então reproduzido antes de alterar a engine. Permanecem warnings preexistentes nos harnesses; nenhuma falha de compilação.

Sem push, TV ou publicação. As provas são locais e sintéticas; esta rodada não acrescenta evidência sobre a queda da conexão real ou a aceitação da ASS MTBB real.
