# Triagem pós-v2.0.5 — 12/10/2026

Escopo: `agente/205-issues-triagem`, somente dados locais. 402 → 419 issues; 17 adicionadas. 419 entradas alteradas (inclui evidência, título, plataforma e última resposta). Nas 402 existentes: 37 mudanças de status, 101 de release, 77 de alvo e 4 de estado GitHub.

## Critério e limites

Conserto de produção citado em commit: associação pelo corpo completo e arquivos alterados, com alcance real via `git rev-list` em todas as tags estáveis locais. Release é a primeira versão numérica que contém o conserto; notas explícitas/semanticamente correspondentes também valem. Docs, testes isolados, merges só documentais e números de versão #203/#204 não são conserto. #203/#204 reais foram ligados aos commits de janela e ponteiro/trailer. Pré-releases diagnósticas não são correção estável.

O recorte de releases tem apenas 40 corpos; tags mais antigas continuam verificáveis pelo Git local. Ausência de commit citando #N não rebaixa lançamentos nem desfechos curados. Issues fechadas nunca ficam com status aberta; desfechos de suporte/desenho/escopo/duplicata/log são válidos. Comentário que apenas anuncia uma versão não basta. Lançada significa conserto publicado, não prova de resolução de todos os pedidos nem teste físico. Casos parciais ficam na lista (e).

Integração após v2.0.5: oito commits de painel, nenhum conserto elegível destas issues. Em-andamento exige commit de branch desde 28/09 (14 dias). Nenhuma atividade anterior a 07/09 no snapshot: **0 itens inativos há mais de 60 dias**; não confundir pedidos de log recentes com abandono.

Falha prévia: `python3 docs/issues/mapa.py --check` rc 1 (`MAPA.md fora de data`). Corrigida por regeneração. Nenhuma outra falha prévia de schema observada. Alvo 2.0.6 adicionado aos aceitos e ao roadmap; 2.0.5 marcada lançada. Decisões históricas do dono preservadas, sem reescrever aprovações.

## Contagens

| Status | Antes | Depois |
|---|---:|---:|
| aberta | 44 | 54 |
| consertada-nao-lancada | 26 | 0 |
| duplicada | 2 | 2 |
| em-andamento | 0 | 0 |
| fechada-sem-resposta | 6 | 6 |
| fora-do-escopo | 3 | 3 |
| lancada | 268 | 306 |
| por-desenho | 14 | 15 |
| precisa-log | 15 | 12 |
| respondida | 24 | 21 |

Total: 402 → 419. Abertas no GitHub: 111; fechadas: 308. Pendências antigas de alvo 2.0.5 redirecionadas: 37.

## Alterações

Formato: status/release/alvo/estado GitHub; alterações de evidência também aparecem mesmo sem mudança nesses quatro campos.

| # | De → para | Evidência |
|---|---|---|
| #1 | lancada/1.1.2/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #2 | lancada/2.0.0/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #3 | lancada/1.1.2/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #4 | lancada/1.1.0/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #5 | lancada/2.0.0/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #6 | lancada/1.1.0/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #7 | lancada/2.0.0/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #8 | lancada/1.0.7/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #9 | lancada/1.0.7/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #10 | lancada/1.0.21/—/fechada → lancada/1.0.7/—/fechada | 04c37f87 em v1.0.7 |
| #11 | lancada/1.0.31/—/fechada → lancada/1.0.8/—/fechada | 90743102 em v1.0.8 |
| #12 | lancada/1.0.43/—/fechada → lancada/1.0.10/—/fechada | 7716f148 em v1.0.10 |
| #13 | lancada/1.0.10/—/fechada → lancada/1.0.10/—/fechada | 7716f148 em v1.0.10 |
| #14 | lancada/1.0.16/—/fechada → lancada/1.0.10/—/fechada | 7716f148 em v1.0.10 |
| #15 | respondida/-/—/fechada → respondida/-/—/fechada | Resposta: plano de passar o trailer ao app de YouTube da TV; rawldon confirmou depois que toca (com atraso). Sem commit #15. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #16 | lancada/1.0.13/—/fechada → lancada/1.0.11/—/fechada | b28db878 em v1.0.11 |
| #17 | lancada/1.0.15/—/fechada → lancada/1.0.15/—/fechada | 4723f22f em v1.0.15 |
| #18 | lancada/1.4.2/—/fechada → lancada/1.0.11/—/fechada | b28db878 em v1.0.11 |
| #19 | lancada/1.0.23/—/fechada → lancada/1.0.11/—/fechada | b28db878 em v1.0.11 |
| #20 | respondida/-/—/fechada → respondida/-/—/fechada | "Fixed on master; next release" (08/09); nenhum commit cita #20. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #21 | lancada/1.5.2/—/fechada → lancada/1.0.24/—/fechada | 2de641c9 em v1.0.24 |
| #22 | lancada/1.0.30/—/fechada → lancada/1.0.22/—/fechada | 545dd122 em v1.0.22 |
| #23 | lancada/1.0.36/—/fechada → lancada/1.0.32/—/fechada | 7ef20234 em v1.0.32 |
| #24 | lancada/1.0.29/—/fechada → lancada/1.0.29/—/fechada | 8f1f7a97 em v1.0.29 |
| #25 | lancada/1.0.30/—/fechada → lancada/1.0.30/—/fechada | Sem commit #25; versão 1.0.30 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #26 | lancada/1.0.30/—/fechada → lancada/1.0.30/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #27 | lancada/1.6.5/—/fechada → lancada/1.6.5/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #28 | lancada/1.0.32/—/fechada → lancada/1.0.32/—/fechada | 2de0c2ec em v1.0.32 |
| #29 | lancada/1.0.44/—/fechada → lancada/1.0.34/—/fechada | f3d40b21 em v1.0.34 |
| #30 | lancada/1.0.34/—/fechada → lancada/1.0.31/—/fechada | bbfc4bd1 em v1.0.31 |
| #31 | lancada/1.0.43/—/fechada → lancada/1.0.31/—/fechada | bbfc4bd1 em v1.0.31 |
| #32 | lancada/1.0.35/—/fechada → lancada/1.0.35/—/fechada | Sem commit #32; versão 1.0.35 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #33 | lancada/1.0.41/—/fechada → lancada/1.0.39/—/fechada | 1444a771 em v1.0.39 |
| #34 | lancada/1.4.6/—/fechada → lancada/1.0.37/—/fechada | 0e43c9ad em v1.0.37 |
| #35 | lancada/1.0.38/—/fechada → lancada/1.0.38/—/fechada | 2d995944 em v1.0.38 |
| #36 | lancada/1.0.38/—/fechada → lancada/1.0.38/—/fechada | 87d986be em v1.0.38 |
| #37 | lancada/1.6.0/—/fechada → lancada/1.0.42/—/fechada | d7c0aefa em v1.0.42 |
| #38 | lancada/1.0.44/—/fechada → lancada/1.0.44/—/fechada | 24f8e451 em v1.0.44 |
| #39 | lancada/1.0.44/—/fechada → lancada/1.0.44/—/fechada | 24f8e451 em v1.0.44 |
| #40 | lancada/1.0.43/—/fechada → lancada/1.0.43/—/fechada | a3c48b28 em v1.0.43 |
| #41 | lancada/1.0.43/—/fechada → lancada/1.0.43/—/fechada | 1142ee84 em v1.0.43 |
| #42 | lancada/1.0.51/—/fechada → lancada/1.0.43/—/fechada | 1142ee84 em v1.0.43; 28f468ef em v1.0.43 |
| #43 | lancada/1.0.43/—/fechada → lancada/1.0.43/—/fechada | 31ad46da em v1.0.43 |
| #44 | lancada/1.0.44/—/fechada → lancada/1.0.44/—/fechada | 24f8e451 em v1.0.44 |
| #45 | lancada/1.6.5/—/fechada → lancada/1.0.44/—/fechada | 24f8e451 em v1.0.44 |
| #46 | lancada/1.0.45/—/fechada → lancada/1.0.45/—/fechada | 3267613a em v1.0.45 |
| #47 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #48 | lancada/1.7.4/—/fechada → lancada/1.7.4/—/fechada | e0596c71 em v1.7.4 |
| #49 | lancada/1.0.54/—/fechada → lancada/1.0.52/—/fechada | 6efca7e5 em v1.0.52 |
| #50 | lancada/1.0.51/—/fechada → lancada/1.0.51/—/fechada | Sem commit #50; versão 1.0.51 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #51 | lancada/1.0.51/—/fechada → lancada/1.0.51/—/fechada | Sem commit #51; versão 1.0.51 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #52 | lancada/1.0.51/—/fechada → lancada/1.0.51/—/fechada | Sem commit #52; versão 1.0.51 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #53 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #54 | lancada/1.0.53/—/fechada → lancada/1.0.53/—/fechada | 78793586 em v1.0.53 |
| #55 | lancada/1.3.2/—/fechada → lancada/1.0.55/—/fechada | 758aa5e3 em v1.0.55 |
| #56 | lancada/1.0.56/—/fechada → lancada/1.0.55/—/fechada | 1e6a770e em v1.0.55 |
| #57 | lancada/1.0.55/—/fechada → lancada/1.0.55/—/fechada | 1e6a770e em v1.0.55 |
| #58 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Salvos = união da lista Nuvio e Trakt watchlist, de propósito. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #59 | por-desenho/1.2.1 (LG); Samsung sem solução/—/fechada → por-desenho/1.2.1 (LG); Samsung sem solução/—/fechada | LG manda Referer desde 1.2.1; AVPlay da Samsung não permite header. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #60 | lancada/1.2.1/—/fechada → lancada/1.2.1/—/fechada | 699e244e em v1.2.1 |
| #61 | lancada/1.2.1/—/fechada → lancada/1.2.1/—/fechada | 699e244e em v1.2.1 |
| #62 | lancada/1.2.1/—/fechada → lancada/1.2.1/—/fechada | 699e244e em v1.2.1 |
| #63 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Tizen 5.0 sem WebAssembly threads; depois o .tpk 4/5 (1.6.0) resolveu por outro caminho. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #64 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #65 | lancada/1.2.1/—/fechada → lancada/1.2.1/—/fechada | 699e244e em v1.2.1 |
| #66 | lancada/1.3.0/—/fechada → lancada/1.3.0/—/fechada | 8e64134a em v1.3.0 |
| #67 | lancada/1.3.7/—/fechada → lancada/1.3.2/—/fechada | 8732c235 em v1.3.2 |
| #68 | lancada/1.3.2/—/fechada → lancada/1.3.2/—/fechada | a8970d44 em v1.3.2 |
| #69 | lancada/1.3.4/—/fechada → lancada/1.3.2/—/fechada | a8970d44 em v1.3.2 |
| #70 | lancada/1.3.2/—/fechada → lancada/1.3.2/—/fechada | 701f3db1 em v1.3.2 |
| #71 | lancada/1.3.2/—/fechada → lancada/1.3.2/—/fechada | 097ef52f em v1.3.2 |
| #72 | lancada/1.3.4/—/fechada → lancada/1.3.4/—/fechada | 2e60f9b8 em v1.3.4; 47c7b45b em v1.3.4; 60bcfc02 em v1.3.4 |
| #73 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #74 | lancada/1.3.4/—/fechada → lancada/1.3.4/—/fechada | f4e96196 em v1.3.4 |
| #75 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #76 | lancada/1.3.4/—/fechada → lancada/1.3.4/—/fechada | 91450a5f em v1.3.4 |
| #77 | lancada/1.3.5/—/fechada → lancada/1.3.5/—/fechada | 2764aa05 em v1.3.5 |
| #78 | lancada/1.3.4/—/fechada → lancada/1.3.4/—/fechada | 9e388586 em v1.3.4 |
| #79 | lancada/1.3.4/—/fechada → lancada/1.3.4/—/fechada | 9e388586 em v1.3.4 |
| #80 | lancada/1.3.4-comparacao1/—/fechada → lancada/1.3.5/—/fechada | b43ffef0 em v1.3.5 |
| #81 | respondida/1.3.7/—/fechada → respondida/1.3.7/—/fechada | v1.3.7 apenas registra o tempo de seek na Samsung (diagnóstico). Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #82 | lancada/1.3.11/—/fechada → lancada/1.3.9/—/fechada | b3b635d4 em v1.3.9 |
| #83 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #84 | lancada/1.4.7/—/fechada → lancada/1.3.8/—/fechada | 2883053e em v1.3.8 |
| #85 | lancada/1.3.10/—/fechada → lancada/1.3.10/—/fechada | 6f2eb5f7 em v1.3.10 |
| #86 | lancada/1.3.11/—/fechada → lancada/1.3.11/—/fechada | 58d491a7 em v1.3.11 |
| #87 | lancada/1.3.12/—/fechada → lancada/1.3.12/—/fechada | 6542d85e em v1.3.12 |
| #88 | lancada/1.4.1/—/fechada → lancada/1.4.1/—/fechada | d683d997 em v1.4.1 |
| #89 | lancada/1.3.12/—/fechada → lancada/1.3.12/—/fechada | 9cc8d044 em v1.3.12 |
| #90 | respondida/-/—/fechada → respondida/-/—/fechada | "Not in 1.4.2 yet — still on the list". Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #91 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #92 | lancada/1.5.0/—/fechada → lancada/1.4.2/—/fechada | 6a61e67e em v1.4.2 |
| #93 | lancada/1.3.12/—/fechada → lancada/1.3.12/—/fechada | 8882ce89 em v1.3.12 |
| #94 | lancada/1.5.1/—/fechada → lancada/1.3.12/—/fechada | ebdf2cd3 em v1.3.12 |
| #95 | lancada/1.4.6/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #96 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #97 | lancada/1.3.12/—/fechada → lancada/1.3.12/—/fechada | 2b94b895 em v1.3.12 |
| #98 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #99 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 5c4e7135 em v1.4.2; c8eb88dc em v1.4.2; cc990b4e em v1.4.2 |
| #100 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #101 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #102 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #103 | lancada/1.4/—/fechada → lancada/1.4/—/fechada | 876742e2 em v1.4 |
| #104 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 6a61e67e em v1.4.2; e7453e91 em v1.4.2 |
| #105 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 6a61e67e em v1.4.2 |
| #106 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 6a3f3a05 em v1.4.2; 6a61e67e em v1.4.2 |
| #107 | precisa-log/-/—/fechada → precisa-log/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #108 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | Sem commit #108; versão 1.4.2 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #109 | lancada/1.4.3/—/fechada → lancada/1.4.1/—/fechada | d683d997 em v1.4.1 |
| #110 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | a00f2e23 em v1.4.2; ae51d9b0 em v1.4.2 |
| #111 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 6a3f3a05 em v1.4.2 |
| #112 | lancada/1.4.2/—/fechada → lancada/1.4.2/—/fechada | 1c9f7c59 em v1.4.2 |
| #113 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | 10e4c3c0 em v1.4.3; 9cacb9c5 em v1.4.3 |
| #114 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | eedf7c81 em v1.4.3 |
| #115 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada | 89eab64c em v1.4.6; 8d36a1f2 em v1.4.6 |
| #116 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | 0fde2217 em v1.4.3 |
| #117 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | Sem commit #117; versão 1.4.3 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #118 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | 10e4c3c0 em v1.4.3; a994a0fc em v1.4.3 |
| #119 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada | Sem commit #119; versão 1.4.6 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #120 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | a8eb6296 em v1.4.3; d1427b55 em v1.4.3 |
| #121 | lancada/1.4.5/—/fechada → lancada/1.4.3/—/fechada | 9cc535e8 em v1.4.3; d1427b55 em v1.4.3 |
| #122 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | d1427b55 em v1.4.3; f4f05699 em v1.4.3 |
| #123 | lancada/1.4.3/—/fechada → lancada/1.4.3/—/fechada | 9293bf5b em v1.4.3; bd405a92 em v1.4.3 |
| #124 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | 72e3378f em v1.5.2 |
| #125 | lancada/1.4.4/—/fechada → lancada/1.4.4/—/fechada | Sem commit #125; versão 1.4.4 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #126 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada | 32a83118 em v1.4.6; 8874e60f em v1.4.6 |
| #127 | lancada/1.4.7/—/fechada → lancada/1.4.7/—/fechada | 563ccc81 em v1.4.7 |
| #128 | lancada/1.4.5/—/fechada → lancada/1.4.5/—/fechada | 6cce1253 em v1.4.5 |
| #129 | lancada/1.4.4/—/fechada → lancada/1.4.4/—/fechada | 0e6f0da6 em v1.4.4; 725a0cbc em v1.4.4; 94c59a3e em v1.4.4 |
| #130 | lancada/2.0.2/—/fechada → lancada/1.4.4/—/fechada | 7f66bfd3 em v1.4.4; 89c77368 em v1.4.4 |
| #131 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada | Sem commit #131; versão 1.4.6 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #132 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #133 | lancada/1.6.0/—/fechada → lancada/1.4.6/—/fechada | 91ceeeea em v1.4.6; f5a0017c em v1.4.6 |
| #134 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | release-notes.md v2.0.0: - **Plugins and P2P (experimental, off by default).** Nuvio scraper repositories as an extra source, and a built-in engine for torrents you pick by hand. |
| #135 | respondida/-/futuro/aberta → respondida/-/futuro/aberta | Porte experimental VIDAA; issue de chamada de testadores. Commits faacf493 (2.0.2) e feat/vidaa. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #136 | lancada/1.4.6/—/fechada → lancada/1.4.6/—/fechada | ea872c3b em v1.4.6 |
| #137 | lancada/1.5.4/—/fechada → lancada/1.5.4/—/fechada | 266dce13 em v1.5.4; 2f444a50 em v1.5.4; 4b2d3956 em v1.5.4 |
| #138 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | release-notes.md v1.6.0: / Samsung 2018–2020 / Tizen 4.0 / 5.0 / 5.5 / `Nuvio-1.6.0-NuvioTpk40.tpk` (native) / |
| #140 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #141 | lancada/1.5.3/—/fechada → lancada/1.5.0/—/fechada | 0a315f7f em v1.5.0; 68521ca4 em v1.5.0 |
| #142 | lancada/1.6.4/—/fechada → lancada/1.5.0/—/fechada | 86c5a1c9 em v1.5.0 |
| #144 | respondida/-/—/fechada → lancada/1.5.2/—/fechada | 876ddc79 em v1.5.2; 98160e65 em v1.5.2 |
| #145 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | c79995b6 em v1.6.0 |
| #146 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | release-notes.md v1.6.0: - **28 new languages**, and the interface language can follow your account, then the TV. Settings › Appearance. |
| #147 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | 378127d9 em v1.5.2; dc18b5e1 em v1.5.2 |
| #148 | precisa-log/-/—/fechada → precisa-log/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #149 | lancada/1.5.1/—/fechada → lancada/1.5.1/—/fechada | 400dd41d em v1.5.1; ea0a1373 em v1.5.1 |
| #150 | lancada/1.5.1/—/fechada → lancada/1.5.1/—/fechada | 035c538a em v1.5.1 |
| #151 | lancada/1.5.3/—/fechada → lancada/1.5.3/—/fechada | b274752b em v1.5.3 |
| #153 | lancada/1.5.1/—/fechada → lancada/1.5.1/—/fechada | e3ae1533 em v1.5.1 |
| #154 | por-desenho/-/—/fechada → por-desenho/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #155 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #156 | lancada/1.5.1/—/fechada → lancada/1.5.1/—/fechada | 96f5abe4 em v1.5.1 |
| #158 | lancada/2.0.2/—/fechada → lancada/1.5.2/—/fechada | 9a50dd55 em v1.5.2 |
| #159 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | 821b39f0 em v1.5.2 |
| #160 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | ebf1a825 em v1.5.2 |
| #161 | por-desenho/-/—/fechada → por-desenho/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #162 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | d4037a42 em v1.5.2 |
| #163 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | 426b0296 em v1.5.2; 6b05521d em v1.5.2 |
| #164 | lancada/1.5.2/—/fechada → lancada/1.5.2/—/fechada | cda85061 em v1.5.2 |
| #165 | lancada/1.7.0/—/fechada → lancada/1.5.4/—/fechada | c4e4d922 em v1.5.4 |
| #167 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #168 | fechada-sem-resposta/-/—/fechada → fechada-sem-resposta/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #169 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | release-notes.md v1.6.0: - **Sources**: AllDebrid, experimental P2P through a Stremio streaming server on your network, and a per-add-on speed test. |
| #170 | lancada/1.5.4/—/fechada → lancada/1.5.4/—/fechada | release-notes.md v1.5.4: - **Tizen 8/9**: the native app now opens from the TV's app menu, not only from Apps2Samsung (#170). |
| #171 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 775f9db9 em v2.0.0 |
| #172 | lancada/1.6.2/—/fechada → lancada/1.6.0/—/fechada | 414cd61e em v1.6.0; 4c2a6951 em v1.6.0; 65e3c8db em v1.6.0 |
| #173 | lancada/1.5.3/—/fechada → lancada/1.5.3/—/fechada | Sem commit #173; versão 1.5.3 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #174 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | 582bc2e6 em v1.6.0 |
| #175 | lancada/1.6.0/—/fechada → lancada/1.6.0/—/fechada | 582bc2e6 em v1.6.0 |
| #176 | lancada/1.7.0/—/fechada → lancada/1.6.0/—/fechada | 3e90ae7f em v1.6.0; 6def852c em v1.6.0; a6c318ef em v1.6.0 |
| #177 | lancada/1.6.0/—/fechada → lancada/1.5.4/—/fechada | 340e5791 em v1.5.4; 3f834522 em v1.5.4; release-notes.md v1.5.4: - **Next-episode card** (#177): the thumbnail respects "blur unwatched episodes". |
| #178 | lancada/1.6.3/—/fechada → lancada/1.5.4/—/fechada | 1c2de6b7 em v1.5.4; b904ea23 em v1.5.4; release-notes.md v1.5.4: - **Trailers with sound (Samsung native)**: fullscreen trailers try IMDb first, so Apple TV and Netflix movie trailers have audio (#178). |
| #179 | lancada/1.6.0/—/fechada → lancada/1.5.4/—/fechada | 3ae82b79 em v1.5.4; 3f834522 em v1.5.4; release-notes.md v1.5.4: - **Trakt "Now Watching"** (#179): Nuvio now tells Trakt when you start and pause, so progress and watched history sync like the official app. |
| #180 | lancada/1.6.0/—/fechada → lancada/1.5.4/—/fechada | 6f581853 em v1.5.4; c121f7d1 em v1.5.4; dde1abd2 em v1.5.4 |
| #181 | lancada/1.5.4/—/fechada → lancada/1.5.4/—/fechada | ae0ba290 em v1.5.4 |
| #182 | lancada/1.5.4/—/fechada → lancada/1.5.4/—/fechada | 3f834522 em v1.5.4; abc5408e em v1.5.4; release-notes.md v1.5.4: - **Addons not showing until reload** (#182): a slow addon (e.g. AIOStreams) is asked once more before the list appears. |
| #183 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #184 | lancada/1.6.4/—/fechada → lancada/1.6.1/—/fechada | 755a6dad em v1.6.1; release-notes.md v1.6.1: - **Samsung native** (#184): after an update downloads, the card offers "Restart now". |
| #185 | lancada/1.6.3/—/fechada → lancada/1.6.1/—/fechada | release-notes.md v1.6.1: - **Video edge** (#185): the video area lines up with the picture to the pixel. |
| #186 | lancada/1.6.1/—/fechada → lancada/1.6.1/—/fechada | 53d01871 em v1.6.1; release-notes.md v1.6.1: - **Symbols as boxes** (#186): the check mark in the audio/subtitle panel and symbols in source names no longer show as empty squares with LG Display or Droid Sans. |
| #187 | lancada/1.6.5/—/fechada → lancada/1.6.1/—/fechada | 3951e747 em v1.6.1; 5aebadf3 em v1.6.1; release-notes.md v1.6.1: - **Explore** (#187): stars open again when the TMDB integration is off, and holding OK no longer loops. The map follows your metadata language. |
| #188 | lancada/2.0.2/—/fechada → lancada/1.6.3/—/fechada | 5b0e4957 em v1.6.3; release-notes.md v1.6.3: - **Samsung native: home screen instead of video** (#188, #195): trailers and movies could show the Samsung home screen behind Nuvio, with only the sound playing. The trailer zoom sent the TV a picture area larger than the screen; it's off on the native app for now, and the picture only opens once the video is really playing. The native app for 2021 and newer also keeps its window opaque, so this is a new .tpk worth installing (the self-update brings only the first part). |
| #189 | respondida/-/—/fechada → respondida/-/—/fechada | "Vou tentar ajustes em breve", sem commit #189. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #190 | lancada/1.6.2/—/fechada → lancada/1.6.2/—/fechada | 000015d7 em v1.6.2; release-notes.md v1.6.2: - **Wrong show in the player** (#190): after Continue watching refreshed, the pause screen and episode list could show another title. |
| #191 | lancada/1.6.2/—/fechada → lancada/1.6.2/—/fechada | 2b99d09b em v1.6.2; release-notes.md v1.6.2: - **Samsung .wgt lag** (#191): text was being measured again every frame since 1.6.0; the web version is back to 1.5.3 speed. |
| #192 | por-desenho/-/—/fechada → por-desenho/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #193 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | release-notes.md v1.7.0: - **Samsung native: video behind the TV menu or a black screen** (#195, #203, #193). |
| #194 | lancada/1.6.4/—/fechada → lancada/1.6.2/—/fechada | 34db0d1a em v1.6.2; release-notes.md v1.6.2: - **Movie collections** (#194): films that belong to a saga now show a Collection section with the other parts. |
| #195 | lancada/2.0.2/—/fechada → lancada/1.6.3/—/fechada | 5b0e4957 em v1.6.3; 8316a07f em v1.6.3; release-notes.md v1.6.3: - **Samsung native: home screen instead of video** (#188, #195): trailers and movies could show the Samsung home screen behind Nuvio, with only the sound playing. The trailer zoom sent the TV a picture area larger than the screen; it's off on the native app for now, and the picture only opens once the video is really playing. The native app for 2021 and newer also keeps its window opaque, so this is a new .tpk worth installing (the self-update brings only the first part). |
| #196 | lancada/1.6.4/—/fechada → lancada/1.6.4/—/fechada | release-notes.md v1.6.4: - **Samsung native, "Not Available" on media keys** (#196). |
| #197 | lancada/1.6.4/—/fechada → lancada/1.6.4/—/fechada | b98ae904 em v1.6.4; f5813122 em v1.6.4; release-notes.md v1.6.4: - **Home rows** (#197): pressing an arrow repeatedly at the row limit no longer hides rows, and rows damaged by it are repaired once on start. |
| #198 | lancada/1.6.5/—/fechada → lancada/1.6.5/—/fechada | 46b1d1b7 em v1.6.5; f26efc21 em v1.6.5; release-notes.md v1.6.5: - **Colored stream badges** (#198): resolution, source, HDR/DV, audio, codec and services each get their own color, like the Nuvio badge pack. Settings › Playback › Colored badges. |
| #199 | lancada/1.7.0/—/fechada → lancada/1.6.5/—/fechada | 149f1235 em v1.6.5; release-notes.md v1.6.5: - **Up next from your account** (#199): without Trakt or Simkl, Continue watching now lists the next episode of the shows you watch, including ones not aired yet with their date (follows "Show unaired episodes"). The full watch history is synced, not just the first 900 entries. |
| #200 | lancada/1.7.0/—/fechada → lancada/1.6.5/—/fechada | f0735c7b em v1.6.5; release-notes.md v1.6.5: - **Custom posters from AIOMetadata** (#200): BetterPosters/PostersPlus show on every title, not just some. |
| #201 | lancada/2.0.1/—/fechada → lancada/1.7.0/—/fechada | aa43af06 em v1.7.0; b3991145 em v1.7.0; d93ea5a8 em v1.7.0 |
| #202 | lancada/2.0.2/—/fechada → lancada/1.7.0/—/fechada | c522e1f2 em v1.7.0; release-notes.md v1.7.0: - **Smaller fixes**: first row style under the Spotlight (#201), accent text in Settings (#202), MKV track names (#206), custom poster language (#200), LG trailers and pointer (#204), collapsed sidebar name (#210). |
| #203 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | 29342682 em v1.7.0 |
| #204 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | 1865cfa2 em v1.7.0; dcf0d8d7 em v1.7.0 |
| #205 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | dc9ca3bf em v1.7.0; release-notes.md v1.7.0: - **Resume from a row tile** (#208) and **disappearing Continue watching items** (#205). |
| #206 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | 94690ba3 em v1.7.0; release-notes.md v1.7.0: - **Smaller fixes**: first row style under the Spotlight (#201), accent text in Settings (#202), MKV track names (#206), custom poster language (#200), LG trailers and pointer (#204), collapsed sidebar name (#210). |
| #208 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | 39e4c794 em v1.7.0; release-notes.md v1.7.0: - **Resume from a row tile** (#208) and **disappearing Continue watching items** (#205). |
| #209 | lancada/1.7.1/—/fechada → lancada/1.7.0/—/fechada | 0da72c2a em v1.7.0; release-notes.md v1.7.0: - **Series in English with another language set** (#209). |
| #210 | lancada/1.7.0/—/fechada → lancada/1.7.0/—/fechada | 3e203fcf em v1.7.0; release-notes.md v1.7.0: - **Smaller fixes**: first row style under the Spotlight (#201), accent text in Settings (#202), MKV track names (#206), custom poster language (#200), LG trailers and pointer (#204), collapsed sidebar name (#210). |
| #211 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 937f8e6b em v2.0.2 |
| #212 | lancada/1.7.1/—/fechada → lancada/1.7.1/—/fechada | 24a14ed0 em v1.7.1; 5f5174ad em v1.7.1; 7a5896fa em v1.7.1 |
| #213 | lancada/1.7.1/—/fechada → lancada/1.7.1/—/fechada | 28064cae em v1.7.1; 5658d0b0 em v1.7.1; 7a5896fa em v1.7.1 |
| #214 | lancada/1.7.1/—/fechada → lancada/1.7.1/—/fechada | release-notes.md v1.7.1: - **Account outages** (#214, #215): temporary server errors preserve your session and the last successful account data for the current profile. Incomplete replies cannot replace a complete saved library. |
| #215 | lancada/2.0.0/—/fechada → lancada/1.7.1/—/fechada | 0ad0b807 em v1.7.1; 98ba4ac6 em v1.7.1; ce575c14 em v1.7.1 |
| #216 | lancada/1.7.2/—/fechada → lancada/1.7.2/—/fechada | 7924f0eb em v1.7.2; 80bb3dd2 em v1.7.2; 941ac3f0 em v1.7.2 |
| #217 | respondida/-/—/fechada → respondida/-/—/fechada | Contribuidor oferece PRs de build em Linux; resposta aceita. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #221 | lancada/2.0.0/—/fechada → lancada/1.7.2/—/fechada | 1e4cfa8e em v1.7.2; 98607591 em v1.7.2; e012f141 em v1.7.2 |
| #222 | lancada/1.7.4/—/fechada → lancada/1.7.4/—/fechada | 1ea1510b em v1.7.4 |
| #223 | lancada/2.0.0/—/fechada → lancada/1.7.2/—/fechada | 105e759b em v1.7.2; release-notes.md v1.7.2: - **Android**: the app no longer restarts when the TV changes display settings, and the black screen before the first frame is gone. This may help the black screen in #223. |
| #224 | lancada/1.7.2/—/fechada → lancada/1.7.2/—/fechada | 67401c73 em v1.7.2 |
| #225 | lancada/1.7.2/—/fechada → lancada/1.7.2/—/fechada | 67401c73 em v1.7.2 |
| #226 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - New app icon. A new What's New, with optional ways to support the project. |
| #227 | lancada/1.7.4/—/fechada → lancada/1.7.4/—/fechada | release-notes.md v1.7.4: - **Localized titles:** the written title appears when the logo is missing or confirmed to be in another language, without duplicating it during loading. |
| #228 | lancada/2.0.1/—/fechada → lancada/2.0.0/—/fechada | 1e03fc30 em v2.0.0 |
| #229 | lancada/1.7.4/—/fechada → lancada/1.7.4/—/fechada | release-notes.md v1.7.4: - **Subtitles and ratings:** Romanian `ron` support, more languages in All subtitles, and shared verified IMDb ratings across screens. |
| #231 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | d6495798 em v2.0.0; release-notes.md v2.0.0: - New option to leave Cinemeta out of search (#231). |
| #232 | lancada/2.0.0/—/fechada → lancada/1.7.4/—/fechada | 2495e39e em v1.7.4 |
| #233 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 3b625f58 em v2.0.0; release-notes.md v2.0.0: - Samsung: account catalogs and their order now follow the account (#233). |
| #234 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 652b759d em v2.0.0; b8a900f5 em v2.0.0 |
| #235 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 652b759d em v2.0.0; b8a900f5 em v2.0.0; release-notes.md v2.0.0: - Seek waits a moment before applying when Seekr thumbnails are on (#235). |
| #237 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 8f8be06a em v2.0.0; e5a848c3 em v2.0.0; release-notes.md v2.0.0: - Stalker/Xtream portal entry accepts "/" and "_" (#237). |
| #238 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 73f93686 em v2.0.0; d6495798 em v2.0.0; ebaef8ad em v2.0.0 |
| #239 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 6bb186ac em v2.0.0; d00870f0 em v2.0.0 |
| #240 | precisa-log/-/—/fechada → precisa-log/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #241 | lancada/2.0.2/—/fechada → lancada/2.0.0/—/fechada | 50ba9279 em v2.0.0; 50df3f42 em v2.0.0; 9a0cce0f em v2.0.0 |
| #242 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #243 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 8e7f4098 em v2.0.0; ccfe01a8 em v2.0.0; release-notes.md v2.0.0: - Continue watching: the same episode repeated, and "watched" not removing it (#244). The made-up "14" rating is gone (#243). |
| #244 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 8e7f4098 em v2.0.0; d6495798 em v2.0.0; release-notes.md v2.0.0: - Continue watching: the same episode repeated, and "watched" not removing it (#244). The made-up "14" rating is gone (#243). |
| #245 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 6bb186ac em v2.0.0; d00870f0 em v2.0.0 |
| #246 | precisa-log/2.0.3/2.0.3/aberta → precisa-log/2.0.3/2.0.6/aberta | NOTAS 2.0.3: "LG: resume no longer drops the source on a failed seek (#246)". Não confirmado que cobre o relato HEVC/anime; plataforma do relato desconhecida. ALVO 2.0.3 = SUSPEITA: o conserto está nas NOTAS mas não está confirmado que cobre o relato; falta log do autor. COMMITS vs v2.0.3 (09/10): a7f23592, a8048e0d, 023497dd estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor. Revisão 2026-10-12: Seek manual/HEVC não é corrigido pela proteção de retomada contra troca de fonte. |
| #247 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 6bb186ac em v2.0.0; a1cf2e46 em v2.0.0; d00870f0 em v2.0.0 |
| #248 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Resposta diz que é comportamento da plataforma Tizen. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #249 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | release-notes.md v2.0.0: - Next episode card stays over the full-screen video in series (#249). |
| #250 | respondida/2.1/2.1/aberta → respondida/2.1/2.1/aberta | Metadados em árabe na 2.0.1; menus em árabe planejados para 2.1 (segundo a resposta em #325). Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #252 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Opção já existe: Ajustes > Idiomas e legendas > Idioma do áudio. Alvo ja-lancada: a opção já existe. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #253 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | a1cf2e46 em v2.0.0 |
| #254 | respondida/2.0.0/—/fechada → respondida/2.0.0/—/fechada | Resposta: busca atualiza ao mudar add-ons na 2.0; a parte da Home não confirmada. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #255 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | 40bc9180 em v2.0.1; release-notes.md v2.0.1: - **Collections.** Accounts with more than 256 folders lost their last collections (#255). |
| #256 | respondida/-/—/fechada → lancada/2.0.5/—/fechada | 11791eda em v2.0.5 |
| #257 | respondida/-/—/fechada → respondida/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #258 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | a1cf2e46 em v2.0.0 |
| #260 | respondida/futuro/futuro/aberta → respondida/futuro/futuro/aberta | Layout RTL "planejado para depois"; sem branch. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #261 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | a1cf2e46 em v2.0.0 |
| #262 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | release-notes.md v2.0.0: - **Player.** Audio and Subtitles open from the corner island. Second subtitle track, subtitle AutoSync, a delay slider, and seeking that speeds up while you hold. |
| #263 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | 08a904d1 em v2.0.0 |
| #265 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - **LG:** visual effects lower themselves on slower TVs. |
| #266 | lancada/2.0.3/2.0.5/aberta → lancada/2.0.2/ja-lancada/aberta | a1d5e039 em v2.0.2; release-notes.md v2.0.2: - **Android 11:** a black screen at launch now shows a rescue screen, and the log is sent even without login (#266). |
| #267 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Resposta: CH+/CH- já atribuídos. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #269 | lancada/2.0.3/—/fechada → lancada/2.0.1/—/fechada | 34a1eae6 em v2.0.1; 47a3d55d em v2.0.1; ce1552cd em v2.0.1 |
| #270 | por-desenho/-/—/fechada → por-desenho/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #271 | lancada/2.0.0/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - **Crashes on LG and Android.** The app could close by itself when Home refreshed, most often while changing Settings. |
| #272 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | Sem commit #272; versão 2.0.1 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #274 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - **Arabic** text reads right to left with joined letters. |
| #275 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - **File size range** for automatic playback: Settings › Sources and add-ons › Maximum size and Minimum size. |
| #276 | lancada/2.0.0/—/fechada → lancada/2.0.0/—/fechada | release-notes.md v2.0.0: - **Plugins and P2P (experimental, off by default).** Nuvio scraper repositories as an extra source, and a built-in engine for torrents you pick by hand. |
| #277 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | release-notes.md v2.0.1: - **Add-ons behave like official Nuvio.** Slow add-ons get more time, setup kept in the link is no longer dropped, and subtitle add-ons like Subtitle Sync have time to answer. |
| #278 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | Sem commit #278; versão 2.0.1 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #279 | fora-do-escopo/-/—/fechada → fora-do-escopo/-/—/fechada | Tizen não tem API pública para trocar a taxa de atualização. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #280 | precisa-log/2.0.3/2.0.3/aberta → precisa-log/2.0.3/2.0.6/aberta | SUSPEITA: único commit que cita #280 é 3c51d5b7 (remontagens da descoberta, 2.0.3); a correção é genérica de fileiras. ALVO 2.0.3 = SUSPEITA: o conserto está nas NOTAS mas não está confirmado que cobre o relato; falta log do autor. COMMITS vs v2.0.3 (09/10): 3c51d5b7 estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor. Revisão 2026-10-12: Remontagem espaçada não comprova correção da contagem/configuração; mantenedor ainda pede log em 11/10. |
| #281 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | 3076c3a5 em v2.0.1; release-notes.md v2.0.1: - **Trailers on the hero and focused posters** now start on accounts with a single profile (#228). Samsung .tpk trailers re-apply their volume after starting (#281). |
| #282 | lancada/2.0.1/—/fechada → lancada/2.0.1/—/fechada | Sem commit #282; versão 2.0.1 citada na nossa resposta. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #283 | lancada/2.0.2/2.0.3/aberta → lancada/2.0.2/ja-lancada/aberta | b68bad96 em v2.0.2; release-notes.md v2.0.2: - **LG:** add-on live channels work (#283). CH+ opens Salvos, like on the other platforms. |
| #284 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | a63ec982 em v2.0.3; df492356 em v2.0.3; release-notes.md v2.0.3: - **Best for this TV** shows the resolution in the add-on's own order too, and an unknown resolution shows as "?" instead of 0p (#284). On LG, Dolby Vision in MKV only counts toward this ranking when that option is on. |
| #286 | lancada/2.0.3/2.0.5/aberta → lancada/2.0.2/ja-lancada/aberta | e03080dd em v2.0.2; release-notes.md v2.0.2: - **Samsung Tizen 4 with Mali-400:** the "Light" effects level now drops to minimal (#286). |
| #287 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 0702a143 em v2.0.2; release-notes.md v2.0.2: - **Player:** audio codec shown (#293), forced subtitles (#287), resolution in the sources list, Arabic subtitles (#273). |
| #288 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #289 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 3eb5aef5 em v2.0.2; release-notes.md v2.0.2: - Home trailer fade matches the still art (#290), plus fixes for #294, #295 and #289. |
| #290 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 4843e5b0 em v2.0.2; 66f19578 em v2.0.2; 75cee76f em v2.0.2 |
| #292 | por-desenho/-/nao vamos fazer/aberta → por-desenho/-/nao vamos fazer/aberta | Aviso nosso (virou issue a partir da discussão #291): resolução da interface em 4K deixa o Android lento. Alvo nao vamos fazer: é aviso/pergunta, não há mudança de código planejada (ver decisões). Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #293 | lancada/2.0.2/ja-lancada/aberta → lancada/2.0.2/ja-lancada/aberta | db1f5891 em v2.0.2; release-notes.md v2.0.2: - **Player:** audio codec shown (#293), forced subtitles (#287), resolution in the sources list, Arabic subtitles (#273). |
| #294 | lancada/2.0.3/—/fechada → lancada/2.0.2/—/fechada | b4186acd em v2.0.2; release-notes.md v2.0.2: - Home trailer fade matches the still art (#290), plus fixes for #294, #295 and #289. |
| #295 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 881f7cae em v2.0.2; release-notes.md v2.0.2: - Home trailer fade matches the still art (#290), plus fixes for #294, #295 and #289. |
| #296 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | release-notes.md v2.0.2: - **Auto-play rules like official Nuvio:** allowed add-ons and plugins, regex filter, and an instant-wait option. |
| #297 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 1b6a5439 em v2.0.2; 325f4e2b em v2.0.2; release-notes.md v2.0.2: - **P2P files over 2 GB** on Samsung and LG (#297). |
| #298 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | release-notes.md v2.0.2: - **Player:** audio codec shown (#293), forced subtitles (#287), resolution in the sources list, Arabic subtitles (#273). |
| #300 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | release-notes.md v2.0.2: - **Default aspect ratio** applied when playback starts (#300). |
| #302 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Rascunho de correção: o conserto do live TV NÃO está na 2.0.2 (resposta anterior dizia que sim). 0021b573 (2.0.2) é parcial. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. Log BKPH09 ainda não foi lido; sem conserto confirmado para este relato. Revisão 2026-10-12: Reconexão publicada, mas autor relata persistência em 2.0.4 em 10/10. |
| #303 | lancada/2.0.2/—/fechada → lancada/2.0.2/—/fechada | 1300a834 em v2.0.2; release-notes.md v2.0.2: - **Continue on profile picker** (#303). |
| #304 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #305 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 98bee79f em v2.0.3; release-notes.md v2.0.3: - **Player:** UP at the top of the controls hides them (#305); the Seekr time and preview follow the hold. |
| #306 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #307 | fora-do-escopo/-/nao vamos fazer/aberta → fora-do-escopo/-/nao vamos fazer/aberta | SUSPEITA de fora-do-escopo: pedido de .ipa, app é C/SDL para TVs. O dono decide. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #308 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 19ef3af8 em v2.0.3; 4151683c em v2.0.3; 6269f306 em v2.0.3 |
| #309 | por-desenho/-/—/fechada → por-desenho/-/—/fechada | Já existe: Fontes e add-ons > Texto da fonte. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #310 | aberta/2.0.2/2.0.5/aberta → lancada/2.0.2/ja-lancada/aberta | release-notes.md v2.0.2: - **Auto-play rules like official Nuvio:** allowed add-ons and plugins, regex filter, and an instant-wait option. |
| #311 | lancada/2.0.3/—/fechada → lancada/2.0.2/—/fechada | release-notes.md v2.0.2: - **Search:** a switch to turn Cinemeta results off (#311). |
| #312 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 317cb179 em v2.0.3; release-notes.md v2.0.3: - **Sync** summary is translated on the spot, and the Continue Watching source "Both" is now "All sources" (#312). With your own Seekr key there is no 50-lookup cap and no counter. |
| #313 | aberta/2.0.5 (agente/204-samsung-dts, só plano)/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #315 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #316 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #317 | lancada/2.0.3/—/fechada → lancada/2.0.2/—/fechada | 937f8e6b em v2.0.2; release-notes.md v2.0.2: - **LG:** startup breadcrumbs and a crash report even before the first frame (#317). |
| #318 | lancada/2.0.3/—/fechada → lancada/2.0.2/—/fechada | 7e96e7e7 em v2.0.2; release-notes.md v2.0.2: - **MStar Android boxes:** no more flicker from the video surface switching, and the remote stays bound to the app (#318). |
| #319 | lancada/2.0.3/—/fechada → lancada/2.0.2/—/fechada | c796ca7d em v2.0.2; release-notes.md v2.0.2: - **Home** drops rows of removed or disabled add-ons right away, and they no longer take Home slots (#319). |
| #320 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 22ecac46 em v2.0.3; 8ab3b78f em v2.0.3 |
| #321 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | b347004c em v2.0.3 |
| #322 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | b8d110de em v2.0.3 |
| #323 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 1b6c9a89 em v2.0.3; release-notes.md v2.0.3: - **Crashes from threads racing each other** (watched episodes, parallel source check, catalog episodes, sign-in token, collections) (#203, #323). |
| #324 | por-desenho/-/nao vamos fazer/aberta → por-desenho/-/nao vamos fazer/aberta | Relatório automático de triagem de logs (comentários nossos); não é bug. Alvo nao vamos fazer: não é bug nem pedido. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #325 | duplicada/2.1/—/fechada → duplicada/2.1/—/fechada | Duplicata de #250/#260. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #326 | consertada-nao-lancada/2.0.5 (agente/203-326, agente/203-331)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 1cc512dd em v2.0.5; 56868756 em v2.0.5; 95fc93c5 em v2.0.5 |
| #327 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 8ab1b19b em v2.0.3; d268b069 em v2.0.3; release-notes.md v2.0.3: - **Home:** switching profile no longer shows the previous profile's rows (#294); the hero no longer depends on a drawn row, and a catalog added from Not on Home is no longer swallowed by a collection (#327); rebuilds are spaced out so Home settles with many collections (#319, #280). |
| #328 | lancada/2.0.3/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | 7edab4b0 em v2.0.3; 9c5d0027 em v2.0.3; release-notes.md v2.0.3: - **Duplicate episodes** in the episode list (#328). |
| #329 | consertada-nao-lancada/2.0.5 (agente/203-331)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 4193781f em v2.0.5; 95fc93c5 em v2.0.5; a0a392fb em v2.0.5 |
| #330 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 2f047898 em v2.0.3; release-notes.md v2.0.3: - **Embedded subtitles:** large MKV track headers, gentler reads, no stale downloads (#308, #330); more add-on subtitles (#268); Arabic subtitles with real bold and sharp ASS in 4K (#335). |
| #331 | consertada-nao-lancada/2.0.5 (agente/203-331)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 11791eda em v2.0.5; 4193781f em v2.0.5; 4c17e680 em v2.0.5 |
| #332 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 35421d1a em v2.0.3; eabe3afd em v2.0.3; release-notes.md v2.0.3: - **Android:** start hang on Shield and other boxes, sign-in through the system network stack when libcurl fails (#266, #332); MStar flicker fixes and a crash report on Android 11 and older (#318). |
| #333 | duplicada/2.1/—/fechada → duplicada/2.1/—/fechada | Duplicata de #250 (Arabic). Comentário no commit c824675c (v1.0.1) é falso positivo. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #334 | consertada-nao-lancada/2.0.3/2.0.3/aberta → lancada/2.0.3/ja-lancada/aberta | c3e19d00 em v2.0.3; d3ca720b em v2.0.3; release-notes.md v2.0.3: - **P2P space limit** in the P2P settings (#334). Leftovers from a session that crashed are deleted at start. |
| #335 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 01016036 em v2.0.3; 3b93dfad em v2.0.3; 503ed5cc em v2.0.3 |
| #336 | por-desenho/-/—/fechada → por-desenho/-/—/fechada |  Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #337 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #338 | consertada-nao-lancada/2.0.5 (agente/203-338)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 74f781f3 em v2.0.5; 8c856535 em v2.0.5; 95fc93c5 em v2.0.5 |
| #339 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 30e13fa4 em v2.0.3; 7dc8cd71 em v2.0.3; b1b53000 em v2.0.3 |
| #340 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | bc63d8ca em v2.0.3; release-notes.md v2.0.3: - **Holding fast-forward** speeds up and crosses the whole file in about 10 s. A single press is still 10 s (#340). |
| #341 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | release-notes.md v2.0.3: - **Samsung:** Pause and subtitle list errors (#269); aspect/zoom button on Tizen 4/5 (#341, probable fix, not yet confirmed on a TV); Arabic subtitles when an older install left old fonts. When the TV hides an audio track it can't play (DTS, TrueHD), the other tracks keep the names from the file instead of a generic one. |
| #342 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #343 | respondida/futuro (anime-skip.com)/futuro/aberta → respondida/futuro (anime-skip.com)/—/fechada | AniSkip/TheIntroDB já na 2.0.1 (3a02817d); anime-skip.com "na atualização seguinte". Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #344 | lancada/2.0.4/—/fechada → lancada/2.0.4/—/fechada | 35a742ee em v2.0.4; 3e3044eb em v2.0.4; 8dd273f3 em v2.0.4 |
| #345 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #346 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #347 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #348 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #349 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #350 | lancada/2.0.3/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | 4b8ba24b em v2.0.3; release-notes.md v2.0.3: - **"View details"** in the Continue Watching hold menu, when OK on the card plays (#350).; release-notes.md v2.0.3: - **Live TV:** the reconnect watcher took the provider's end of stream for a manual pause and never reopened the channel. Manual pause is now tracked on its own (#302, #350). |
| #352 | consertada-nao-lancada/2.0.5 (agente/204-biblioteca-visto)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 402e2438 em v2.0.5; d4d72639 em v2.0.5; fab062b3 em v2.0.5 |
| #353 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #354 | fora-do-escopo/-/nao vamos fazer/aberta → fora-do-escopo/-/nao vamos fazer/aberta | SUSPEITA: versão desktop está fora do escopo (app é para TVs). Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #355 | consertada-nao-lancada/2.0.5 (agente/204-fonte-detalhes)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | ac055d54 em v2.0.5; b176eff5 em v2.0.5; c8dbfda5 em v2.0.5 |
| #356 | precisa-log/2.0.5/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | d47a78c8 em v2.0.3; release-notes.md v2.0.3: - **Up next** no longer disappears when the Nuvio catalog does not list the episode; Cinemeta is checked first (#356). |
| #357 | precisa-log/-/2.0.5/aberta → precisa-log/-/2.0.6/aberta | Zidoo Z9X 8K (Android 11) fecha ao abrir; Ugoos AM9 Pro. Autor comentou que não achou o aparelho no relatório #324 e não consegue enviar log. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #358 | consertada-nao-lancada/2.0.5 (agente/204-conta-catalogos)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 29c640b7 em v2.0.5; c4edb7c6 em v2.0.5; release-notes.md v2.0.5: - A new add-on catalog that no longer fits on Home goes to the queue instead of being switched off (#358), and a row switch now takes effect instead of flipping back. |
| #359 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 1860216b em v2.0.3; release-notes.md v2.0.3: - **Fixed sidebar:** the row editor (Reorder and enable rows) and the View all and collection grids start after it instead of under it (#359). |
| #360 | consertada-nao-lancada/2.0.5 (agente/204-sync-addons)/2.0.5/aberta → lancada/2.0.3/—/fechada | 46507ea5 em v2.0.3; release-notes.md v2.0.3: - **Add-on sync** merges your TV edits with the account instead of replacing it: an add-on installed on the phone stays, one removed on the phone does not come back. The merged list is never cut. After 3 refusals (4xx) the TV edit is dropped and the account list is applied (#360). |
| #361 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 2220b019 em v2.0.3; 9ac3531d em v2.0.3; release-notes.md v2.0.3: - **Posters with long image URLs** (over ~500 characters, such as rating-poster services) are no longer cut on the backdrop, the title page, Saved and the account library, so they load; a background or logo URL too long to keep is dropped with a log line instead of loading as a broken link (#361). |
| #362 | consertada-nao-lancada/2.0.5 (agente/204-colecao-vitrine)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 22320582 em v2.0.5; 491fcee3 em v2.0.5 |
| #363 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 54cf2fab em v2.0.3; 9bb93887 em v2.0.3; release-notes.md v2.0.3: - **Recommending a series** from a Continue Watching card works again (#363). |
| #364 | consertada-nao-lancada/2.0.5 (agente/204-fonte-recusada)/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 8f2c1701 em v2.0.5; bf666aa7 em v2.0.5; d78963ab em v2.0.5 |
| #365 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #366 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #367 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #368 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | dbec4f44 em v2.0.3; release-notes.md v2.0.3: - **Search** waits 300 ms after the last key before asking the add-ons, and keeps the previous rows until the first reply (#368). A search with no results clears the previous ones. |
| #369 | lancada/2.0.3/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | 5f04c36b em v2.0.3; 712557ca em v2.0.3; release-notes.md v2.0.3: - **Thai** is drawn with a bundled Noto Sans Thai font, in the interface and in subtitles (#369). |
| #370 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 2aee231b em v2.0.3; 56c9832e em v2.0.3; release-notes.md v2.0.3: - **Samsung .wgt:** Arabic subtitles use Noto Naskh (#370). Holding OK in the subtitle sheet makes one choice instead of reloading on every repeat (#370). |
| #371 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 2fa6e51a em v2.0.3; b39f4149 em v2.0.3; release-notes.md v2.0.3: - **Profile page:** Left at the edge opens the sidebar without closing the page, also on a new profile or one with only friends (#371). |
| #372 | lancada/2.0.4/—/fechada → lancada/2.0.4/—/fechada | 21077ab5 em v2.0.4; 8eb5ce90 em v2.0.4; c2112b61 em v2.0.4 |
| #373 | aberta/-/2.0.5/aberta → por-desenho/-/—/fechada | Revisão 2026-10-12: autor confirma UI resolvida em 2.0.4; mantenedor explica em 11/10 que Opening source é esperado por renovação dos links. Fechada no snapshot; sem commit de fix comprovado. |
| #374 | aberta/-/2.1/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #378 | consertada-nao-lancada/-/—/fechada → lancada/2.0.4/—/fechada | 04844fb9 em v2.0.4; 28aa5c4b em v2.0.4; 59733d54 em v2.0.4 |
| #379 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #382 | precisa-log/-/2.0.5/aberta → precisa-log/-/2.0.6/aberta | LG 50UT8050PSB, Nuvio 2.0.2 (.ipk). Relato: depois de ligar a TV a frio aparece "sem internet" mesmo com internet, some, volta várias vezes até parar de vez. O campo de log ficou sem código. SUSPEITA, sem prova: o aviso nasce do teste de rede do app (redesaude) logo após o boot, quando a rede da TV ainda sobe; não lido no código nem em log. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #383 | lancada/2.0.3/ja-lancada/aberta → lancada/2.0.5/ja-lancada/aberta | 3654b88e em v2.0.5; release-notes.md v2.0.5: - A subtitle you picked by hand survives into the next episode (#383); forced subtitles and unknown types keep their behaviour, and picking the active subtitle from outside the recommendations no longer lands on "Off". |
| #384 | lancada/2.0.3/—/fechada → lancada/2.0.3/—/fechada | 3e7bbb62 em v2.0.3; 4151683c em v2.0.3; 9ee093ac em v2.0.3 |
| #385 | respondida/2.0.3/2.0.5/aberta → lancada/2.0.3/ja-lancada/aberta | 4151683c em v2.0.3; 9ee093ac em v2.0.3; release-notes.md v2.0.3: - **Video server refusing connections** (#385). When the server of the video refuses a connection (seen on Samsung .tpk with TorBox links), the embedded subtitle reader, the track probe and the chapter reader stop and wait 10 to 60 s before trying that server again, with one connection at a time, instead of retrying several times while the video opens. It does not make a refused source play: if the server also refuses the TV player, the source still fails. |
| #386 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #387 | aberta/-/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 03213598 em v2.0.5; 0f98379f em v2.0.5; c82fa422 em v2.0.5 |
| #388 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #389 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #390 | consertada-nao-lancada/-/—/fechada → lancada/2.0.4/ja-lancada/aberta | a7bb794c em v2.0.4; a8fcd146 em v2.0.4; adfba576 em v2.0.4 |
| #391 | ausente → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #392 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.4/ja-lancada/aberta | 1d4b5e97 em v2.0.4; 2d3e472e em v2.0.4; 9023ebdb em v2.0.4 |
| #393 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #394 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #396 | ausente → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #397 | respondida/-/2.1/aberta → respondida/-/2.1/aberta | Pedido em quatro partes (formulário diz LG webOS / All; o texto fala de Android/Fire TV). Caminhos conferidos NO CÓDIGO (09/10), sem prova em TV. (a) HDR/Dolby Vision e passthrough: JÁ HÁ o que o app controla. Preferência de fonte em Ajustes › Reprodução › "Dolby Vision e HDR" (AJ_FONTE_HDR) e "Preferir som Dolby Atmos" (AJ_ATMOS, ajustes.c:4452-4453); "Dolby Vision em MKV (experimental)" só LG webOS 4+, desligado por padrão (AJ_DV_MKV, ajustes.c:5399): na 2.0.3 troca TrueHD por E-AC-3/AC-3 no idioma ouvido para manter o Dolby Vision (docs/releases/2.0.3/NOTAS.md:17). "Converter DTS para a TV ouvir" (AJ_DTS_AC3, ajustes.c:4464): só LG, converte o DTS em estéreo (AAC) ou 5.1 (Dolby Digital) via src/dts/; na Samsung o app só AVISA quando a TV recusa DTS/TrueHD (video_tpk.c:130-141, #313; esconder as faixas é 2.0.5, fase 1). LIMITADO PELA TV/PLATAFORMA, não dá para prometer: na LG e na Samsung quem decodifica HDR/DV e faz o passthrough é a TV (uMS / AVPlay / player .tpk), o app só escolhe a fonte e o caminho; no Android o Media3 usa o passthrough por AudioCapabilities (NvPlayer.kt:374-386; video_android.c:27) e com bitstream o app não mexe no áudio (sem reforço de volume, sem velocidade: video_android.c:260, 836-841). Não há "robustez garantida" a implementar sem um caso de TV com log. (b) Estilo da legenda na hora: JÁ EXISTE no player. Botão Legendas (PLR_CC, player.c:189) › aba "Estilo" (legendasui.c:824; faixas.c:1489, corpo faixas.c:533-600), com Tamanho (50% a 250%, de 10 em 10, padrão 120%, faixas.c:590), Fonte OpenSubtitles, Cor, Opacidade (100/75/50/25%), Fundo (Nenhum, Escuro 25/50/75/100%), Posição (1 a 8), Borda (Nenhuma/Contorno/Sombra), Atraso, Negrito (Ligado/Desligado) e Restaurar padrão; OK cicla o valor e aplica na hora. Ressalvas: com ASS desenhado pelo app, fonte/cor/fundo/posição/borda/negrito ficam "Preservado pelo ASS" (faixas.c:536-541) e o tamanho vira escala; o negrito vale só para legenda externa/OpenSubtitles, a faixa que a TV desenha segue o peso da TV (faixas.c:571-572). Os valores são cíclicos (sem slider). (c) Atraso: legenda JÁ EXISTE: Legendas › linha "Atraso" (LR_ATRASO, legendasui.c:671-676), régua de -10 s a +10 s, 0,1 s por toque e 0,5 s segurando a seta (legendasui.c:444-451); a legenda secundária tem o seu; e "Sincronização automática" (AutoSync, linha LR_SYNC) que acerta pelo áudio (Ajustes "Sincronizar pelo áudio", AJ_LEG_SYNC_AUDIO, só onde o player entrega PCM, hoje Android: audsync.h:1-6). ATRASO DO ÁUDIO NÃO EXISTE (sem backend nem linha: grep audio_atraso/audioDelay vazio). (d) Proporção JÁ EXISTE: botão Proporção (PLR_ASPECTO) ou tecla 0, 8 modos: Original, Recortar, Esticar, Zoom leve, Zoom cinema, Zoom ultra, Ajustar altura, Ajustar largura (player.c:877-881, player.h:218-226); padrão em Ajustes › Reprodução › "Proporção padrão" (AJ_PROPORCAO_PADRAO). Em TVs que não recortam a imagem os modos de zoom caem para Original (player.c:1173); o .wgt Samsung (AVPlay/WebAssembly) não tem o zoom que corta a barra preta. Velocidade JÁ EXISTE: folha de Áudio, linha "Velocidade" (faixas.c:1448; "Playback speed" no i18n), 0,75x 1x 1,25x 1,5x 1,75x 2x (velocidade.c:7), por vídeo; disponível na LG, no .tpk e no Android; AUSENTE na Samsung .wgt (AVPlay só tem trick play inteiro, video_tizen.c:1531-1539) e bloqueada no Android com áudio em passthrough ("Indisponível com áudio pelo receptor", faixas.c:1449, #202). Salto de QUADROS (frame step/skip) NÃO EXISTE (só saltos de 10/30/60/120 s); o pedido é ambíguo (passo de quadro ou pular quadros?). FALTA, sugestao: atraso do áudio na 2.1 (começar onde o backend deixa, Android/Media3; LG e Samsung podem não ter API, a confirmar); passo de quadro em futuro (precisa de pausa + avanço por quadro nos 4 backends, sem prova de que a TV deixa); faixa de velocidade mais larga (0,25x-3x) e slider de tamanho/opacidade da legenda em 2.0.5 se o dono quiser (pequeno). Alvo geral = o mais cedo entre as peças que faltam (atraso do áudio, 2.1). Origem: pedido do autor somprakashpathakoneplus-cell. Rascunho (EN): The subtitle style (size up to 250%, color, opacity, background, bold), the subtitle delay ruler (-10 s to +10 s) and AutoSync are already in the player under Subtitles, tab Style; aspect modes are on the Aspect ratio button (or key 0) and playback speed (0.75x-2x) is on the Audio sheet. Audio delay and frame stepping are not there yet and are on the roadmap, while HDR, Dolby Vision and passthrough depend on what each TV or player decodes, so we can only choose the source and path. Which device are you on, so we know which of these you are missing? Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #398 | ausente → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #399 | ausente → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #400 | consertada-nao-lancada/-/—/fechada → lancada/2.0.4/—/fechada | 3d9db2d8 em v2.0.4; a46e1a4c em v2.0.4; f60a4ba5 em v2.0.4 |
| #401 | respondida/-/2.0.5/aberta → respondida/-/2.0.6/aberta | SUSPEITA nova (09/10, relato do autor: 'Updating 1%..99%' lento, instalação nova da 2.0.2 em segundos): dados do app dentro da pasta do app (.nuvio 359 MB/3980 arquivos na C9, incluindo cache.suspeito-1853 de 195 MB nunca apagado). LG C5 webOS 26, 2.0.32 para 2.0.3 pelo Homebrew Channel, 15+ min. NAO e regressao de tamanho (ipk 2.0.3 61,1 MB/1218 arquivos; 2.0.2 60,7 MB/1215). MEDIDO no ipk 2.0.4 (58,3 MB, 1190 arquivos, 99,5 MB descomprimido): o binário nuvio-proto tem 55,3 MB e VEM COM debug_info e símbolos (file: not stripped), 30,8 MB de seções .debug*/.symtab/.strtab que gzip reduz a 10,5 MB; a arte (art/) tem 39 MB em 1142 arquivos, já comprimida (JPG/PNG/WebP). Estimativa (nao medida): strip tira ~10 MB do ipk (58,3 para ~48 MB). O Homebrew Channel baixa o ipk inteiro para /tmp, confere sha256 e chama appInstallService/dev/install (services/service.ts); a barra de download usa o content-length do servidor, nao o ipkSize (tools/hb-repo.sh ja grava ipkSize). Onde estao os 15 min (download do GitHub pela TV, gravacao em /tmp, ou appInstallService instalando 1190 arquivos) NAO foi medido: SUSPEITA. Pedir ao autor a duracao de cada fase (a barra mostra Downloading/Verifying/Installing). Alvo 2.0.5. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #402 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Namer03, S95C, .tpk 2.0.3. A imagem mostra, na aba NMR, o cabecalho do grupo "Other SDR" com fileiras cujo nome (formatter do addon) diz "FHD / REMUX / SDR" e "FHD / SDR"; so a ultima diz "N/A / SDR" (essa realmente nao declara resolucao). CAUSA (lida no codigo, sem teste ainda): o grupo vem de grupoRes (src/streams.c:2105) que usa s->altura ou os selos r-4k/r-1080/r-720; a altura sai de stream_parse.c:281-283, que so procura os numeros 2160/1440/1080/720/480 e 4k/uhd, e badges_detectar (badges.c:40-42) so tem 1080p/1080i/1080. A palavra FHD sozinha (comum em formatter de AIOStreams) nao casa, entao altura=0 e o grupo e Outras (GRUPO_NOME, streams.c:2072). O app ja conhece FHD para canal ao vivo (nv_res_do_texto, livetv_regras.h:38) mas nao usa no filme/serie. NAO e ordenacao (nivelHdr, MP4 primeiro da LG e perfil 5 nao entram: e a Samsung .tpk e o problema e o grupo, nao a ordem dentro dele). Nao e configuracao (fonteregra.h trata auto-play, nao grupos). Cuidado no conserto: nv_res_do_texto aceita "hd" solto como 720, o que casaria DTS-HD; copiar so fhd/fullhd/full hd. Candidato a 2.0.4 (muda so o parser, 2 linhas + teste), decisao do dono. SUSPEITA: o JSON bruto do autor pode ter a resolucao em outro campo; sem o log/JSON da fonte nao foi provado. Diagnóstico atualizado: torrents sem cache; sem conserto identificado. Revisão 2026-10-12: FHD corrigido parcialmente; autor reabriu com ordenação ainda errada em 2.0.4. |
| #403 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #404 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #405 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #406 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #407 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #409 | consertada-nao-lancada/2.0.4/—/fechada → lancada/2.0.4/—/fechada | 2051869e em v2.0.4; 5cd60adf em v2.0.4; 6d46e336 em v2.0.4 |
| #410 | consertada-nao-lancada/-/2.0.4/aberta → lancada/2.0.4/ja-lancada/aberta | 52dbde71 em v2.0.4; b32c9410 em v2.0.4; b8935ecf em v2.0.4 |
| #411 | consertada-nao-lancada/-/—/fechada → lancada/2.0.4/—/fechada | release-notes.md v2.0.4: - **Samsung Tizen 4 / 5: no more "Not Available" toast** on Play/Pause and other media keys (#411). |
| #412 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Log QHQRVE (S95C, .tpk): relógio do player congelado, UI com upd=120 ms por quadro, parar retorna 0 ms mas o player nativo segue tocando. Conserto em agente/204-tpkpreso412 (71c7d615): ASS deixa de segurar o quadro, recupera relógio congelado, parada confirmada antes de abrir outro player. Revisão Codex achou P1 (Voltar durante PrepareAsync encerraria o processo numa TV sã): rodada 2 em andamento. Aguarda log; sem conserto confirmado para este relato. Revisão 2026-10-12: Correção 2.0.4 não cobre a persistência após reinstalação; comentário de 11/10 descreve novo fix em progresso. |
| #413 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #414 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #415 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Diagnóstico via memlog; sem conserto confirmado. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #416 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 4b43829a em v2.0.5; 7f338254 em v2.0.5; 8b5ed6c0 em v2.0.5 |
| #417 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | e0bb1020 em v2.0.5; f219bc9d em v2.0.5; release-notes.md v2.0.5: - The title page shows one trailer card again (#417), and the screensaver waits while a trailer plays (#422). |
| #418 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #419 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 7f338254 em v2.0.5; 8b5ed6c0 em v2.0.5; release-notes.md v2.0.5: - Metadata loads again when the main TMDB address is blocked or times out (#416, #419), and posters with very long addresses load again (#433). |
| #420 | aberta/-/2.0.5/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #421 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | aced3443 em v2.0.5; c8e8d983 em v2.0.5; release-notes.md v2.0.5: - Embedded text subtitles are drawn by the app on LG and Samsung (#421); on LG their size matches the preview. |
| #422 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 65bb6e2c em v2.0.5; a408e51a em v2.0.5; release-notes.md v2.0.5: - The title page shows one trailer card again (#417), and the screensaver waits while a trailer plays (#422). |
| #423 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #424 | aberta/-/2.0.5/aberta → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #425 | aberta/-/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 2b4119d5 em v2.0.5; 2c571ab6 em v2.0.5; 8b7468b0 em v2.0.5 |
| #426 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 3ba2987a em v2.0.5; c4f344e8 em v2.0.5; release-notes.md v2.0.5: - Samsung aspect ratio: the picture stays where it belongs on Tizen 4 (#426, #341), an off-screen crop is refused instead of pushing the image away, and a refused or queued change gives the sound back and stays in its own video. |
| #427 | precisa-log/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 92ee6141 em v2.0.5 |
| #428 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Aguarda código/log para diagnóstico. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #429 | consertada-nao-lancada/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 2d42c8e0 em v2.0.5; b1da4311 em v2.0.5; release-notes.md v2.0.5: - The Serbo-Croatian code matches Croatian, Serbian and Bosnian (#429), and the exact language wins when a language is picked automatically. |
| #430 | aberta/-/2.1/aberta → lancada/2.0.5/ja-lancada/aberta | 551edb43 em v2.0.5; a9c97b26 em v2.0.5; release-notes.md v2.0.5: - Auto-skip of credits only fires with a real start and end marker (#329, #331) and rewinding never skips them again; intro markers also cover anime opened from IMDb/TMDB, in any season (#430). |
| #433 | precisa-log/2.0.5/2.0.5/aberta → lancada/2.0.5/ja-lancada/aberta | 7777e129 em v2.0.5; ea0c322a em v2.0.5; release-notes.md v2.0.5: - Metadata loads again when the main TMDB address is blocked or times out (#416, #419), and posters with very long addresses load again (#433). |
| #434 | aberta/-/2.1/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #436 | aberta/-/2.1/aberta → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #437 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #438 | aberta/-/futuro/aberta → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #439 | ausente → lancada/2.0.5/ja-lancada/aberta | 2f1e2de1 em v2.0.5; 6ad103f3 em v2.0.5; release-notes.md v2.0.5: - Subtitle sizes are the same everywhere and go up to 300% (#439, #440), and Arabic bold matches the regular size. |
| #440 | ausente → lancada/2.0.5/ja-lancada/aberta | 2f1e2de1 em v2.0.5; 6ad103f3 em v2.0.5; release-notes.md v2.0.5: - Subtitle sizes are the same everywhere and go up to 300% (#439, #440), and Arabic bold matches the regular size. |
| #450 | ausente → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #451 | ausente → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |
| #457 | ausente → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #458 | ausente → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #459 | ausente → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #460 | ausente → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #461 | ausente → aberta/-/2.1/aberta | sem conserto comprovado nos commits/notes locais |
| #462 | ausente → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #466 | ausente → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #467 | ausente → aberta/-/futuro/aberta | sem conserto comprovado nos commits/notes locais |
| #468 | ausente → aberta/-/2.0.6/aberta | sem conserto comprovado nos commits/notes locais |

## (a) Abertas mas com conserto lançado — 39

Conferir alcance antes de fechar; lançamento não prova resolução integral nem teste físico.

| # | Versão para fechamento |
|---|---|
| #266 | v2.0.2 |
| #283 | v2.0.2 |
| #286 | v2.0.2 |
| #293 | v2.0.2 |
| #310 | v2.0.2 |
| #326 | v2.0.5 |
| #328 | v2.0.3 |
| #329 | v2.0.5 |
| #331 | v2.0.5 |
| #334 | v2.0.3 |
| #338 | v2.0.5 |
| #341 | v2.0.3 |
| #350 | v2.0.3 |
| #352 | v2.0.5 |
| #355 | v2.0.5 |
| #356 | v2.0.3 |
| #358 | v2.0.5 |
| #362 | v2.0.5 |
| #364 | v2.0.5 |
| #369 | v2.0.3 |
| #383 | v2.0.5 |
| #385 | v2.0.3 |
| #387 | v2.0.5 |
| #390 | v2.0.4 |
| #392 | v2.0.4 |
| #410 | v2.0.4 |
| #416 | v2.0.5 |
| #417 | v2.0.5 |
| #419 | v2.0.5 |
| #421 | v2.0.5 |
| #422 | v2.0.5 |
| #425 | v2.0.5 |
| #426 | v2.0.5 |
| #427 | v2.0.5 |
| #429 | v2.0.5 |
| #430 | v2.0.5 |
| #433 | v2.0.5 |
| #439 | v2.0.5 |
| #440 | v2.0.5 |

## (b) Fechadas sem fix comprovado — 0

Exclui desfechos válidos de resolução e lançamentos curados; ausência de #ref não é prova de ausência de fix.

Nenhum item.

## (c) Duplicatas e candidatos — 6

| Origem | Principal | Evidência |
|---|---|---|
| #325 | duplicada/2.1/—/fechada → duplicada/2.1/—/fechada | Duplicata de #250/#260. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #333 | duplicada/2.1/—/fechada → duplicada/2.1/—/fechada | Duplicata de #250 (Arabic). Comentário no commit c824675c (v1.0.1) é falso positivo. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #330 | #412 | confirmada pelo autor de #412: substitui #330 com reprodução melhor |
| #419 | #416 | suspeita: próprio corpo de #419 aponta TMDB indisponível; sintomas diferentes |
| #391 | #372 | suspeita: episódios/temporadas incompletos; comparar Z4HDY2 com a numeração de #372 |
| #365 | #254 | suspeita: catálogos removidos persistem; #51 também relata isso, exigir reprodução atual |

## (d) Precisa informação do autor — 18

Avaliar o pedido antes de enviar: comentários são só os últimos três; o log não foi consultado nesta triagem.

| # | Falta |
|---|---|
| #288 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #315 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #357 | precisa-log/-/2.0.5/aberta → precisa-log/-/2.0.6/aberta | Zidoo Z9X 8K (Android 11) fecha ao abrir; Ugoos AM9 Pro. Autor comentou que não achou o aparelho no relatório #324 e não consegue enviar log. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #379 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #382 | precisa-log/-/2.0.5/aberta → precisa-log/-/2.0.6/aberta | LG 50UT8050PSB, Nuvio 2.0.2 (.ipk). Relato: depois de ligar a TV a frio aparece "sem internet" mesmo com internet, some, volta várias vezes até parar de vez. O campo de log ficou sem código. SUSPEITA, sem prova: o aviso nasce do teste de rede do app (redesaude) logo após o boot, quando a rede da TV ainda sobe; não lido no código nem em log. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #388 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #398 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #401 | respondida/-/2.0.5/aberta → respondida/-/2.0.6/aberta | SUSPEITA nova (09/10, relato do autor: 'Updating 1%..99%' lento, instalação nova da 2.0.2 em segundos): dados do app dentro da pasta do app (.nuvio 359 MB/3980 arquivos na C9, incluindo cache.suspeito-1853 de 195 MB nunca apagado). LG C5 webOS 26, 2.0.32 para 2.0.3 pelo Homebrew Channel, 15+ min. NAO e regressao de tamanho (ipk 2.0.3 61,1 MB/1218 arquivos; 2.0.2 60,7 MB/1215). MEDIDO no ipk 2.0.4 (58,3 MB, 1190 arquivos, 99,5 MB descomprimido): o binário nuvio-proto tem 55,3 MB e VEM COM debug_info e símbolos (file: not stripped), 30,8 MB de seções .debug*/.symtab/.strtab que gzip reduz a 10,5 MB; a arte (art/) tem 39 MB em 1142 arquivos, já comprimida (JPG/PNG/WebP). Estimativa (nao medida): strip tira ~10 MB do ipk (58,3 para ~48 MB). O Homebrew Channel baixa o ipk inteiro para /tmp, confere sha256 e chama appInstallService/dev/install (services/service.ts); a barra de download usa o content-length do servidor, nao o ipkSize (tools/hb-repo.sh ja grava ipkSize). Onde estao os 15 min (download do GitHub pela TV, gravacao em /tmp, ou appInstallService instalando 1190 arquivos) NAO foi medido: SUSPEITA. Pedir ao autor a duracao de cada fase (a barra mostra Downloading/Verifying/Installing). Alvo 2.0.5. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #404 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #407 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #414 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #415 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Diagnóstico via memlog; sem conserto confirmado. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #418 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #423 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #424 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #428 | precisa-log/2.0.5/2.0.5/aberta → precisa-log/2.0.5/2.0.6/aberta | Aguarda código/log para diagnóstico. Revisão 2026-10-12: status curado preservado; ausência de commit citando a issue não invalida o desfecho. |
| #451 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |
| #468 | sem código/log no snapshot; solicitar reprodução em 2.0.5 |

## (e) Decisões exclusivas do dono — 6

- **Prioridade/capacidade do patch:** Aprovar propostas 2.0.6 por plataforma; features médias em 2.1, porte PS5/VIDAA e integrações grandes em futuro. Alvos são propostas, não novos compromissos.
- **Escopo parcial de fechamento:** Manter/separar pendências em #310, #341, #352, #369, #390, #410 e #433. Primeira release é do primeiro conserto; há complementos posteriores.
- **AutoSync por plataforma:**  #374 pede sincronização pela linha clicada, ainda sem fix; #462 é diagnóstico de AutoSync Samsung com crash DND12K. Decidir critérios para integrar, sem chamar diagnóstico de fix.
- **Áudio Android:** #399: patch para travamento TrueHD vs seletor de decoder em 2.1; #397 e #313 dependem da capacidade dos backends.
- **Informativos e limites por desenho:** #396, #324, #292 e #427: decidir fechamento informativo, sem fingir implementação. Em #427, correção publicada é explicação do limite, não aumento de RAM.
- **Duplicatas e fechadas sem prova:** Confirmar os candidatos (c) e justificativas em (b); revisar só após reprodução, sem fechamento/reabertura em massa.

## Lançada sem commit citando a issue — 17

Status e release do HEAD restaurados; confirmação do dono pendente.

| # | Release preservada |
|---|---|
| #25 | 1.0.30 |
| #26 | 1.0.30 |
| #27 | 1.6.5 |
| #32 | 1.0.35 |
| #50 | 1.0.51 |
| #51 | 1.0.51 |
| #52 | 1.0.51 |
| #108 | 1.4.2 |
| #117 | 1.4.3 |
| #119 | 1.4.6 |
| #125 | 1.4.4 |
| #131 | 1.4.6 |
| #132 | 1.4.6 |
| #173 | 1.5.3 |
| #272 | 2.0.1 |
| #278 | 2.0.1 |
| #282 | 2.0.1 |

## Revisão precisa-log → lancada — 8

| # | Resultado | Motivo |
|---|---|---|
| #246 | precisa-log restaurada | Seek manual/HEVC não é corrigido pela proteção de retomada contra troca de fonte. |
| #280 | precisa-log restaurada | Remontagem espaçada não comprova correção da contagem/configuração; mantenedor ainda pede log em 11/10. |
| #302 | precisa-log restaurada | Reconexão publicada, mas autor relata persistência em 2.0.4 em 10/10. |
| #402 | precisa-log restaurada | FHD corrigido parcialmente; autor reabriu com ordenação ainda errada em 2.0.4. |
| #412 | precisa-log restaurada | Correção 2.0.4 não cobre a persistência após reinstalação; comentário de 11/10 descreve novo fix em progresso. |
| #356 | lancada mantida (2.0.3) | d47a78c8 consulta Cinemeta antes de descartar episódio ausente; cobre Up Next. |
| #427 | lancada mantida (2.0.5) | 92ee6141 mostra pedido salvo e limite aplicado; esclarece o teto, não aumenta RAM. |
| #433 | lancada mantida (2.0.5), parcial | 7777e129/ea0c322a aceitam URL até 2 KB e corrigem fallback horizontal; Continue Watching não comprovado. |

## Provas

- `python3 docs/issues/mapa.py`: rc 0.
- `python3 docs/issues/mapa.py --check`: rc 0.
- `sh tools/painel-issues/tests/publico.sh`: rc 0.
- `python3 .triagem/check-status-curados.py`: fechadas no GitHub com status aberta = **0**; lancada no HEAD e aberta agora = **0**; rc 0.
- Reconciliação: 419 números únicos, mesmos estados do snapshot, uma validação `triagem`; rc 0.

O teste público falhou inicialmente após marcar 2.0.5 lançada: ela aparecia nas colunas disponível e atual. Corrigido `tools/painel-issues/index.html` para escolher a próxima versão numérica não lançada do roadmap (2.0.6), sem duplicar a lançada. Teste existente ampliado para release publicada, fallback e ordenação numérica. Nenhuma publicação, commit ou acesso de rede.

## Novas issues (17) — triagem detalhada

Leitura offline do snapshot e do código; SUSPEITA não é reprodução. Alvos são propostas internas. [Rascunhos em inglês para o dono revisar](rascunhos/respostas-triagem-2.0.5.md): 39 lançadas (7 parciais), 18 pedidos de informação e 4 duplicatas/candidatas. Nenhuma resposta postada.

- **#391** — Samsung .tpk; bug; alvo 2.0.6; aberta: QN55Q60RAG, 2.0.3: só primeira temporada e episódios incompletos; log Z4HDY2 já fornecido. src/descoberta.c:parsearEpisodios, episodiosDoCatalogo e temporadasDosEps leem videos[], deduplicam (temporada, episódio), escolhem a lista de metadados e derivam as abas da lista publicada. A seleção descarta temporadas <= 0 e pode preferir outra lista quando a base parece lista de arquivos. SUSPEITA: cobertura/numeração ou escolha da lista de metadados; não prova duplicata de #372 nem resolução de #391. Há tratamento implementado, mas nenhum conserto estável especificamente comprovado para este relato. Próximo: Comparar em 2.0.5 a mesma série/add-on com a lista esperada, inspecionar Z4HDY2 quando autorizado e perguntar se coincide com #372; testar cobertura das temporadas e episódios.
- **#396** — all; meta; alvo futuro; respondida: Publicação informativa do mantenedor sobre C, Rust e roadmap; comentários pedem Discord. src/menu.c usa SDL/OpenGL e android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt contém o backend Media3: implementação C compartilhada com pontes de plataforma, não migração Rust. Não há bug reproduzível nem conserto a atribuir; o trecho do corpo é incompleto e não comprova entrega do piloto Rust. .triagem/release-notes.md v2.0.5 já anuncia o servidor comunitário Discord; o pedido de Discord tem evidência de atendimento, não de migração Rust. Próximo: Dono revisar resposta informativa e estado atual do Discord/roadmap; fechar como informativa se desejar, sem marcar migração Rust como lançada.
- **#398** — Samsung .tpk; bug; alvo 2.0.6; aberta: S95C, 2.0.3: transparência do contorno; pede opacidade em passos de 10 e redução de tamanho sem ciclo. Exemplos estão em issue externa não disponível offline; sem log. src/player.c:desenharLegendaPrincipal aplica alpha 1/.75/.5/.25 e passa borda ao PlainAssStyle; ASS autoral preserva estilo, texto simples recebe estilo do app. src/legendasui.c usa os mesmos quatro níveis. SUSPEITA: composição do contorno/alpha ou diferença de caminho ASS/texto, não comprovada. Tamanho já é limitado em 50–300% no código; isso não prova correção do contorno nem passos de 10% de opacidade. Próximo: Pedir exemplo visual local, tipo de legenda (embutida/add-on, SRT/ASS), ajustes e código de log em 2.0.5; reproduzir contorno. Separar refinamentos de controles/opacidade para 2.1.
- **#399** — Android; bug; alvo 2.0.6; aberta: Ugoos SK2 ligado por HDMI à Sony HT-CT790: AC3 toca, TrueHD 7.1 congela vídeo; pede preferir decoder do app. android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt:abrirMain usa DefaultRenderersFactory com EXTENSION_RENDERER_MODE_ON e fallback: plataforma/passthrough primeiro, FFmpeg para codec não suportado. android/app/build.gradle.kts já inclui decoder FFmpeg (inclui TrueHD); presença do decoder não prova fallback quando passthrough anuncia suporte e trava. SUSPEITA: capacidades HDMI/passthrough/AudioSink, sem reprodução. Seletor explícito não está neste caminho. Próximo: Pedir versão exata, Android, configuração de saída HDMI e log após congelar; comparar AC3/TrueHD da mesma mídia com passthrough desligado. Bug em 2.0.6; decisão do seletor em 2.1.
- **#439** — all; feature; alvo ja-lancada; lancada: Pede limite de legenda de 250% para 300%, sem plataforma indicada. Já lançado: src/ajustes.c:ajustes_leg2_tamanho chega a 300; src/player.c:desenharLegendaPrincipal limita pelo TXT_LEG_PCT_MAX (src/text.h), compartilhado com legenda secundária/preview. 6ad103f3 e merge 2f1e2de1 contidos em v2.0.5; .triagem/release-notes.md explicita #439/#440. Não depende só do comentário de intenção. Próximo: Dono revisar rascunho de fechamento citando 2.0.5; pedir reabertura se 300% não estiver disponível nessa versão.
- **#440** — all; bug; alvo ja-lancada; lancada: Relata texto menor ao ligar Bold; mantenedor identifica face árabe Noto Naskh Bold. src/text.c:corpoNegrito compara altura do glifo/ascensão, arabeBoldFace ajusta a face Bold à regular e fonteNegritoReal faz ajuste das famílias. Causa e compensação explícitas no código, não mera SUSPEITA. 6ad103f3 e 2f1e2de1 em v2.0.5 e nota de release #440 comprovam lançamento; não há prova física nesta triagem. Próximo: Dono revisar fechamento com 2.0.5; reabrir com idioma, fonte e exemplo se o tamanho ainda mudar.
- **#450** — all; feature; alvo futuro; aberta: Pedido de porte para PS5 com jailbreak; autor já usa webOS e aceita que é trabalho grande. src/menu.c:menu_desenhar depende de SDL/gfx; android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt é backend Media3 específico, e tizen-tpk contém empacotamento próprio. Reuso da UI não fornece backend/SDK/instalação PS5. Não há implementação PS5 comprovada nem bug atual. Próximo: Avaliar SDK/toolchain, vídeo, entrada e distribuição PS5 antes de qualquer plano; manter porte grande em futuro sem promessa.
- **#451** — Samsung .tpk; bug; alvo 2.0.6; aberta: QN74F Tizen 9, versão apenas Latest: Continue Watching tem uma série contra quatro em outros clientes; autor corrige título e confirma Nuvio sync, sem Trakt/Simkl. src/syncprog.c:syncprog_puxar já importa sync_pull_watch_progress do perfil ativo, normaliza milissegundos e descarta duração inválida; syncprog_aplicar mescla progresso e src/descoberta.c monta a união da conta/local. SUSPEITA: perfil, importação/normalização ou filtros de CW; não ausência geral de sync nem problema de legendas. Sem log nem conserto comprovado deste caso. Próximo: Pedir versão exata, perfil usado nos dois clientes, uma série/episódio ausente e sequência de sincronização; log após atualizar CW em 2.0.5. Conferir pull e filtros com dados sanitizados.
- **#457** — Android; feature; alvo 2.1; aberta: Pede selos DV/HDR10, tamanho, áudio e release group da fonte pré-resolvida na ficha. src/app.c usa stream_antecipada para iniciar a fonte; src/streams.c:montar e src/streams.h já mantêm metadados e exibem fontes/selos na folha. src/detail.c:detail_desenhar desenha ficha do título, não uma opção de resumo de fonte pré-resolvida com todos esses campos. Funcionalidade relacionada existe na folha, não prova entrega do pedido na ficha. Próximo: Definir quais campos confiáveis do Stream mostrar na ficha e estado sem fonte/sonda pendente; reaproveitar metadados/selos existentes sem buscar a mídia duas vezes.
- **#458** — all; feature; alvo 2.1; aberta: Relata fontes iguais entre add-ons, repetição no autoplay e aba All; pede deduplicação por prioridade e agregação por qualidade (corpo disponível é trecho). src/streams.c:mesmaFonte inclui provedor, URL, hash, arquivo e rótulo para manter foco entre atualizações, não dedup global; montar já organiza por qualidade ou preserva ordem do add-on. src/fontesmortas.c registra fontes mortas por provedor/identidade. SUSPEITA: tentativas repetidas entre provedores; não reproduzida. Agrupamento existe parcialmente, filtro transversal por prioridade não comprovado. Próximo: Criar caso local com dois provedores retornando mesmo URL/hash+fileIdx; definir identidade sem unir arquivos distintos e preservar headers/ordem. Separar dedup do redesenho de All.
- **#459** — LG; feature; alvo 2.1; aberta: LG 2.0.4, Simkl conectado diretamente: pede enviar progresso parcial ao pausar/sair para retomar em outros apps. src/simkl.c:simkl_ler_playback e simkl_continuar leem pausados; simkl_episodios_marcar/simkl_titulo_marcar enviam histórico de visto. src/player.c chama trakt_scrobble ao tocar/pausar; não há envio Simkl equivalente neste caminho. Leitura de playback e histórico existem, envio incremental Simkl não comprovado. Limite demonstrado do cliente, sem afirmar capacidade atual da API Simkl offline. Próximo: Validar contrato suportado para progresso parcial Simkl antes de implementar; especificar pause/stop e não marcar visto quando só pausou, com teste de retomada entre clientes.
- **#460** — Android; feature; alvo 2.1; aberta: Pede botões recarregar fonte e próximo episódio. src/player.c:player_proximo_episodio e player_janela_proximo já suportam próxima mídia e oferta no fim; player_pediu_recarregar/avAtivar (AV_B_RECARREGAR) atendem Live TV, não provam botão VOD recarregar. Episódios/posplay existem; botões VOD sempre acessíveis são ampliação de UI, relacionada a #403. Próximo: Conferir #403 e definir botões VOD com episódio disponível; reusar encaminhamento de próximo e reabrir a fonte sem perder posição/legenda, distinguindo recarregar Live TV.
- **#461** — Android; feature; alvo 2.1; aberta: Pede opção de toolbar no topo; mantenedor responde que hoje há menu lateral e relaciona #407. src/menu.c:desenhaBarra/geoEm implementam rail/painel lateral; src/detail.c roteia CIMA para área social e ESQUERDA para menu. Não há opção top toolbar comprovada; barra social no topo não é substituto da navegação principal. Sem bug ou causa a diagnosticar. Próximo: Dono definir navegação/foco do D-pad e preferência por layout, junto de #407; prototipar troca lateral/topo antes de mexer em todas as telas.
- **#462** — Samsung .tpk; meta; alvo 2.0.6; aberta: Chamada de teste v2.0.5-legref.1 para AutoSync Tizen 6+, não release estável; Q80A Tizen 6 relata sync, S95C Tizen 9 relata sucesso e crash DND12K com legenda add-on; PFQ7Y3 também informado. src/legref.c:legref_disponivel desliga Samsung salvo NV_LEGREF_TPK e sempre exclui NV_TPK40/web; src/legsync.c coordena coleta de referência/AutoSync. Manual delay existe; habilitação Samsung é canário. SUSPEITA: coleta/concorrência/render de legenda externa no crash; logs não consultados. Próximo: Analisar DND12K e comparar 60F3SE/6OF3SE, ZQSHBN e PFQ7Y3 quando logs estiverem disponíveis; reproduzir add-on vs embutida na mesma mídia. Estabilidade do canário em 2.0.6, dono decide habilitação estável; não fechar como fix lançado.
- **#466** — all; feature; alvo futuro; aberta: Pede resource watch_state para addons receberem eventos incrementais e delegarem tracking a vários provedores. src/addons.c interpreta resources de metadados/fontes/legendas; src/player.c emite eventos Trakt e progresso local/Nuvio via src/syncprog.c, sem despachante watch_state. Referências externas não consultadas (offline). Infraestrutura de tracking existe, contrato/autorização/entrega de eventos ao addon não estão implementados nesse caminho. Próximo: Obter contrato documentado em snapshot local; definir adesão por manifesto, dados mínimos, fila/retry e isolamento por perfil sem compartilhar credenciais. Planejar integração ampla em futuro.
- **#467** — all; feature; alvo futuro; aberta: Pede tracking AniList, sem plataforma. src/artereserva.c:anilist consulta arte GraphQL e src/descoberta.c reconhece IDs anilist; isso é metadado/arte, não autenticação ou envio de histórico. src/visto.c:visto_titulo/visto_episodios distribuem visto para provedores existentes, sem destino AniList. Tracking não implementado no caminho lido; sem bug a reproduzir. Próximo: Definir autenticação e mapeamento de anime/episódios AniList; comparar implementação direta com delegação watch_state (#466) antes de expandir destinos.
- **#468** — LG; bug; alvo 2.0.6; aberta: C5 webOS 26, 2.0.5: AIOMetadata via Jellyfin mostra cerca de quatro catálogos. src/jellyfin.c:jf_bibliotecas consulta UserViews; jf_ler_bibliotecas só aceita IDs hex limpos e CollectionType movies/tvshows. tarefaBibliotecas usa JF_FIL_MAX=6 (src/jellyfin.h) e pula biblioteca sem itens. Estes limites/filtros são comprovados no código, mas quatro visíveis não prova qual disparou. SUSPEITA: teto de seis, tipo/ID não convencional do servidor compatível ou fileira vazia. Nenhum conserto lançado para este relato. Próximo: Pedir log e nomes/quantidade dos catálogos esperados sem endereço/credenciais; obter fixture sanitizada de UserViews/Items para verificar teto, tipos, IDs e vazios com mais de seis bibliotecas.

### Provas da triagem detalhada

- `python3 docs/issues/mapa.py`: rc 0; `python3 docs/issues/mapa.py --check`: rc 0.
- `sh tools/painel-issues/tests/publico.sh`: rc 0 (`TUDO OK`).
- `grep -E "plataforma=\?|sem conserto comprovado nos commits/notes locais" /private/tmp/triagem-17-prova.txt`: rc 1 esperado, zero ocorrências nas 17; verificação Python rc 0.
- Demais 402 entradas preservadas; rascunhos: 39 + 18 + 4 = 61 textos (60 issues únicas, #419 possui alternativa de confirmação), sete parciais sinalizados. Sem URLs nos rascunhos.
- Sem rede, GitHub, commit ou postagem; memória M3 não acessada/persistida porque escopo não verificado e rede proibida.
