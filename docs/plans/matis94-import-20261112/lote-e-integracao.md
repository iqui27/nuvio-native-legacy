# Lote E integrado — lista de fontes, formatter e selos

Pedido do dono: importar o que falta do fork Matis94 para a integração de TV. Merge `1ac01b8b5b10afa8c36696378cb06c8eae045690`, candidato `f571a7415e17505deaa547cac79e37df7f3fd929`, base `e6da65314883f88ea701b9fd98178e6b50f205f7`. Código incorporado; aceite físico, ABI e desempenho em TV permanecem pendentes. Sem push, release, instalação, escrita em conta ou serviço novo. Hook documental existente atualiza painel e roadmap público automaticamente.

## Comportamento incorporado

- Regex com modificadores locais por grupo, restauração de flags, alternativas balanceadas e limites próprios do compilador de regras de selos.
- Cópias de dados para worker de selos com trava, publicação protegida por geração/versão/índice, limites de vizinhos e largura estável enquanto texturas carregam.
- Formatter limitado, culling de blocos fora da área visível, estilos importados, parser de emoji/bandeiras/DUAL/MULTI e labels somente quando o add-on não declarou faixas de áudio.
- Escolha local Topo/Embaixo para selos de Do add-on, Topo por padrão; opção acrescentada depois dos IDs integrados de A/F/H/G nas tabelas paralelas. Reutiliza componentes e ícones existentes.
- Nome UTF-8 completo em linhas, sem a animação antiga de marquee. Identidade e cópia preservam o nome real; deduplicação Cloud só oculta o nome quando a descrição realmente é desenhada inteira, sem elipse ou elementos que a substituam.
- Fallback de VSync restrito à espera da fonte no caminho webOS; sem alegação de ganho de FPS host.
- Teste de backup preserva explicitamente as sete opções locais dos lotes F/E: upload não as inclui, importação ignora variantes legadas camel/snake/shared/device e reabrir mantém escolhas desta TV. Política de produto não foi relaxada para satisfazer o teste.

## Revisão e verificação

Revisor independente `j-rev-e3-zima`; verificador distinto `j-verify-e3-zima`, cobertura de todos os 57 arquivos concluída. Sem P0/P1 atual confirmado. Dois P1 históricos retirados após confirmação independente: identidade do nome perdida na deduplicação e teste de renderização referindo funções removidas. Revisores fizeram inspeção de fonte, sem executar testes ou builds; resultados host atribuídos à Central e aos HEADs reais.

Diff SHA-256 `c1af2821a75ff35d1e875bced997d1160467ac0c19e7c5f38b050154b9312eb0`.

Doze checks host passaram no candidato exato, em `e10-host-evidence.json` e `e11-host-evidence.json`: ajustes_nuvem, skip_button_settings, ajustes_ux_interacao, fontes_lote_e, i18n, build-local, fontenome, fontes_formatter, fonte_addon_texto_shot, glyphrender, regexjs_scoped e selospacote. O teste de texto usa SDL/GL oculto; não estabelece aparência ou aceite na TV.

Primeiro gate executou seis comandos e todos passaram, mas recusou a estrutura dos metadados: o relatório colocou reviewer/verifier/complete no topo, enquanto o contrato exige esses campos dentro de review. Central preservou o relatório original e copiou os três valores existentes para o campo esperado, sem mudar achados, vereditos, pins ou testes; proveniência registrada em `j-verify-e3-normalized.log`. Novo gate passou: 6 declarados/executados/obrigatórios/passados, zero falhas, skips, timeouts ou divergências, agreement verdadeiro. Digest `e8c7e13d866589dc892331d32bf317e8f9322863a300d79dfad747da06a919e8`. Checagem i18n do integrador passou.

## Pós-merge

Três comandos passaram no merge exato; HEAD `1ac01b8b5b10afa8c36696378cb06c8eae045690` idêntico antes/depois, TMPDIR no SSD e `NUVIO_PROPERTIES=/dev/null`.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/fontes_lote_e.sh` | 0 | `f590bd6c159240c2a07756866e68bde1af24ee7cb32420f0368f56ef1c2f7576` |
| `bash tests/ajustes_nuvem.sh` | 0 | `8d28f637b4939a350208dc0a76b9205b9102c6f2d25d08f8caa0be0c9c0218e8` |
| `bash tests/i18n.sh` | 0 | `aeaa32eb5c0497980f5df77f877c67ff0ba6418e0f3ad4325ba64c001528e10e` |

## Notas P2 e limites

- E-ARQ-I18N: trecho de nome que coincida com uma chave como Multi/Dublado pode ser traduzido, pois o wrapper usa funções de texto com i18n.
- E-EMPTY-BADGE-STRIP: cartão Do add-on sem selos correspondentes ou ainda em processamento reserva 36 px vazios para manter geometria estável.
- E-ARQ-TALL-ROW: nome perto de 1023 bytes ou fallback de descrição perto de 2047 bytes pode produzir linha mais alta que a lista, sem rolagem interna no modo padrão. Título/extremos ficam cortados; nomes usuais de 100–200 caracteres não têm esse problema.
- E-ARQ-CTRL-KEY: tab/newline no nome diferencia a chave bruta da medição e a cópia sanitizada do desenho, causando recomputação do cache e possível diferença de altura.
- Build host define NV_SEM_WEBOS: ramo de VSync webOS foi inspecionado, sem compilação ARM/webOS comprovada nesta rodada. Sem prova de ABI, FPS, TV ou aprovação visual física. Nenhuma issue encerrada por este merge.

Evidência privada em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`: `j-rev-e3-zima.log`, `j-verify-e3-zima.log`, `j-verify-e3-normalized.log`, `e-final-gate.log`, `e-final-gate-normalized.log`, `e-integration.log`, manifests host e pós-merge.
