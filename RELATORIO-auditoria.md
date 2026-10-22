# Auditoria da integração 2.0.5

Worktree: `/Volumes/ExternalSSD/nv-205-auditoria`.
Branch: `agente/205-auditoria`.
Base auditada: `integracao/2.0.5`, commit `121b2598`.
Comparação histórica: `master` em `581e8fb9`, mais `git log` por função/arquivo.
Modo: ponytail full; correções locais, sem push, TV ou publicação.

## 1. Estática 4 — escritores de `catalogo.c:itens`

**Classificação:** problema de sincronização anterior à 2.0.5; helper básico novo, já protegido.

Em `master:src/catalogo.c`, linhas 1237–1286, `cat_apontar_episodio`, `aplicarUm` e `cat_aplicar_progresso` já escrevem no vetor global sem `pubTrava`. O histórico inclui `39e4c794` (progresso em todas as cópias, #208), ancestral de master. Portanto, essas escritas não nasceram no merge de CW da 2.0.5.

`cat_completar_basico` foi acrescentado por `ab45a9ba` e já segura `pubTrava`. O detector não reconhece diretamente o nome `catTravar` na expressão de travas; seu alerta não significa que esse helper esteja literalmente sem mutex. Ainda assim, os escritores de progresso sem trava podem disputar o vetor com publicadores e enriquecedores que a seguram.

**Correção:** reutilizar `pubTrava` nas duas APIs de mutação e normalizar o índice depois de adquiri-la. O helper interno `apontarEpisodio` pressupõe a trava presa; `aplicarUm` chama esse helper, evitando adquirir o mesmo mutex novamente. A ordem continua `pubTrava -> epTrava`. Nenhuma trava foi acrescentada aos leitores/desenho por card; as novas aquisições pertencem a eventos de episódio e aplicação/gravação de progresso. Rede e gravação em disco continuam fora dessas seções.

Não houve alteração em `tools/auditoria/base.txt`. O ajuste protege essas mutações contra troca/publicação concorrente; não pretende eliminar todas as leituras sem trava já existentes no catálogo.

**Prova:** ampliação do teste existente `tests/catcorrida.c`, usando o gancho que publica outro bloco antes da aquisição. Com índice 39, o catálogo passa de 4 para 40 itens: a mutação deve atingir 39, sem usar a normalização anterior para 3. O teste falhou antes da correção (assert, saída 134) e passou depois com ASan/UBSan. `tests/retomar_copias.sh` também passou com ASan/UBSan, preservando propagação entre cópias, episódio e progresso remoto/local.

## 2. Estática 7 — pipe de `tests/colecao_exibicao.sh`

**Classificação:** defeito do teste, ausente em master. O script foi introduzido em `65e551e6` (layouts de coleção), dentro do intervalo `master..121b2598`; não é regressão de produto comprovadamente introduzida pelos merges finais da 2.0.5.

**Correção:** `set -euo pipefail`. O `|| true` foi limitado ao grupo do `grep`, para aceitar saída sem mensagens selecionadas sem esconder a falha do compilador.

**Prova:** execução real de `bash tests/colecao_exibicao.sh` passou com os seis layouts. Uma execução com `cc` temporário que sai 42 sem imprimir nada retornou exatamente 42 pelo script; o pipe não mascara a compilação.

## 3. Teste focado `cwretido` — símbolos ausentes

**Classificação:** stubs desatualizados após mudanças da Home no intervalo posterior a master. `ab45a9ba` acrescenta a pré-busca de informação/medições de tempo; `668f6298` acrescenta pedido antecipado do logo. O harness inclui `home.c` diretamente e precisa dos símbolos alcançados por `home_atualizar`.

**Reprodução:** `bash tests/cwretido.sh` falhou no link arm64 com `_marco`, `_tex_obter_larg` e `_tex_obter_logo_larg_qualquer` indefinidos.

**Correção:** três stubs em `tests/cwretido_home.c`, seguindo os já existentes em `tests/cwremover_home.c`. Texturas retornam zero; marca de tempo não faz nada. O produto não foi alterado para resolver o link.

**Prova:** o binário compilou e executou até `cwretido_home: tudo ok`, incluindo retenção na ilha/faixa, exclusão só do Continuar, retorno na frente, dispensar, republicação e troca de perfil.

## 4. Smoke A — Salvos não abre após Cima

**Classificação:** smoke desatualizado; não foi constatada regressão no painel de Salvos.

A captura anterior `A-salvos.png` mostra o modal “What's new in 2.0.5” por cima da página. `app_evento` entrega os eventos a `novcartao_evento` e retorna enquanto o modal está aberto. O caminho Cima da página para Salvos continua presente, inclusive em master.

O smoke marcava novidades como vistas somente extraindo nomes de `.c`/`.h`. Desde `d4195e39` (motor das novidades da 2.0.3), o nome está em `src/novidades/<versao>.inc`; a 2.0.5 usa `.arquivo = "novidades-205.txt"`. A cópia criada pelo smoke não recebia essa marca.

**Correção:** incluir `src/novidades/*.inc` na mesma extração existente. Nenhuma mudança no roteador ou no painel do produto; as marcas são gravadas somente na cópia de dados.

**Prova:** comparação executável do preparador em `121b2598` com o corrigido: antes não cria `novidades-205.txt`; depois cria com `1`, deixando a origem intacta. No app real, o smoke confirmou `[spainel] aberto: 129 salvos` e a série reaberta continuou mostrando 40 episódios.

## Ajuste do resultado da auditoria

O smoke antigo emitia incondicionalmente `NAO VERIFICADO` para abertura pela ilha/player, porque proíbe reprodução, e terminava com código 2 mesmo quando todos os checks executáveis passavam. Isso impedia `AUDITORIA: PASS` por definição.

Esse limite fixo passou a `INFO`, visível também no resumo: a abertura real pela ilha/player **não foi exercitada**; a estática 1 confere o pedido de episódios dessas entradas. Não se transformou esse limite em PASS. Casos executáveis indisponíveis (outro app aberto, falta de série/perfil/sessão) continuam `NAO VERIFICADO` e código 2. O contrato foi atualizado em `docs/auditoria-release.md`.

A execução intermediária confirmou todos os checks do smoke, mas foi descartada como validação final: o shell estava rodando enquanto o script foi editado e encontrou erro de leitura/sintaxe. O comando inteiro foi repetido após estabilizar os arquivos.

## Validação final

Comando, sem pular build ou smoke:

```bash
export TMPDIR=/Volumes/ExternalSSD/tmp
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
bash tools/auditoria-release.sh
```

**Resultado final: `AUDITORIA: PASS`, código de saída 0.** As sete verificações estáticas, os 15 testes focados, o build do Mac e os checks executáveis das rodadas A/B passaram. Salvos abriu com 129 linhas; a série mostrou 40 episódios nas duas aberturas; a troca 1 -> 2 produziu 12 itens de Continuar do novo perfil.

Log anterior: `/Volumes/ExternalSSD/tmp/auditoria-205-antes.log`.
Log final: `/Volumes/ExternalSSD/tmp/auditoria-205-depois.log`.
Logs detalhados: `/Volumes/ExternalSSD/tmp/nv-auditoria/`.
Capturas e logs do app: `/Volumes/ExternalSSD/tmp/nv-auditoria-smoke/`.

Validações adicionais: `SANITIZE=1 bash tests/catcorrida.sh`, `SANITIZE=1 bash tests/retomar_copias.sh`, `bash tests/colecao_exibicao.sh`, injeção de compilador com status 42, comparação do preparador de novidades, sintaxe shell/Python e `git diff --check`.
