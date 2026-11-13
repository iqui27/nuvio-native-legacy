# Lote H integrado — episódios e progresso local

Pedido do dono: “vamos trazer o que falta do fork pra nossa integração”. TV apenas.

Merge `8a16234bc2ac98de5c3a258c7592342151c56507`, candidato `8b739fd87ee4796c6d79edb83f2f155d7bb42255`, base `70f5ec284b2bef7670b9dab04802565dbaf3ffa3`. Cobre F14, F15 e F20 do [plano](STATUS.md). Sem push, publicação de release, deploy do Worker de recomendações/logs, instalação ou acesso a contas. O hook documental existente atualiza painel e roadmap público; isso não publica serviços nem novas builds.

## Comportamento incorporado

- Nomes de arquivo/release em episódios cedem lugar a metadados ou fallback localizado, preservando IDs, temporada e número do episódio.
- Temporadas disponíveis são atualizadas com limite `CAT_TEMP_MAX`; especiais (temporada zero) ficam por último.
- “Continuar assistindo” publica o progresso local do perfil antes de esperar o reconciliador remoto, inclusive quando a fileira anterior já tinha conteúdo. Geração e travas impedem publicação de outro perfil e deixam a reconciliação final prevalecer. Ausência local não remove entradas conhecidas apenas pelo cache; itens ocultos/removidos continuam filtrados.
- Consentimento manual de fontes e comportamento das issues 639/787 preservados.

Commits de implementação: `cb40e8e3c`, `4f9cfd1ab`, `cebba6101`. A correção de dependência moveu helpers puros para `catalogo.c/.h`, evitando referência de catálogo à camada de descoberta. O candidato anterior quebrava oito harnesses de catálogo por símbolo indefinido; a correção recuperou esses testes sem stubs artificiais.

## Revisão, verificação e gate

Revisor: `Claude Code review job j-rev-h2`. Verificador distinto: `j-verify-h2-zima`. Sem P0/P1 pendente. O achado histórico de dependência foi retirado após examinar a correção e sua evidência.

SHA-256 do diff: `020af46e86d0dd8e6cade103687babf96ad0b229fa7899859b9091cdc5c5b0d1`. Atualizar o candidato com a integração preservou byte a byte o patch antes revisado em `597d0cf1f21548de9d718a51d3844c980e2ecf6c`; identidade registrada em `h-current-base-pin.json`.

Zima fez verificação por leitura; não executou os testes. Seu relatório identifica as execuções históricas como evidência do revisor anterior. A Central executou depois os 15 comandos sobre o candidato atual `8b739fd8…`: `central.py verifica` retornou gate `passed`, agreement verdadeiro, 15/15 aprovados, nenhum ignorado, timeout ou divergência. O integrador conferiu base, HEAD, limpeza e identidades antes do merge; i18n automático passou.

Digest de verificação do merge: `f0d8402a3a68a5fe2e461772d476db024ce0c941563542d21bfe15c7f4266ff7`. Relatórios privados em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/j-rev-h2.log` e `j-verify-h2-zima.log`; eventos no livro `/Volumes/ExternalSSD/tmp/rev/jobs.jsonl`.

## Testes pós-merge

Executados no merge `8a16234bc2ac98de5c3a258c7592342151c56507`, com HEAD idêntico antes e depois. TMPDIR exclusivo no SSD externo; `NUVIO_PROPERTIES=/dev/null`.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/cwprov.sh` | 0 | `abc0cb2ee8f8eb0d175f559caab22a1bad2c63565678e4a091e92aa5f7ca8512` |
| `bash tests/episodiosdup.sh` | 0 | `6179777b582fbf70fb6f19d8856f7929a6a17077f1e8185e0af0b05933de462e` |
| `bash tests/i18n.sh` | 0 | `07c16d17d396124e3a114322c0ccda63d0990cd2145e8f9fb383b60ebff0a14c` |

Manifesto: `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/h-postmerge-evidence.json`; logs individuais no mesmo diretório, prefixo `h-postmerge-`.

## Notas P2 e limites

- R1 confirmado: detecção de título genérico pode abranger títulos reais como “Part 2”, “Chapter 3”, “Day 5”, um número isolado ou “Part2Finale”.
- R2 confirmado: metadados do doador Cinemeta podem usar numeração diferente da origem.
- R3 inconclusivo: placeholder chinês com espaços após troca de idioma; pacote do verificador não incluía a tabela zh. O mecanismo foi inspecionado, sem confirmação desse caso.
- R4 confirmado/intencional: publicação local inicial pode mostrar arte ainda sem enriquecimento remoto.
- P2 não bloqueou o merge. Testes host não comprovam navegação física, performance de TV, ARM/ABI ou instalação. Nenhuma alegação de validação em aparelho.
