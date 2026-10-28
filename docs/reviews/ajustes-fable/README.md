# Ajustes 2.1 — redesign completo (agente/21-21-ajustesfable)

Pedido do dono (27/10/2026): Ajustes bonitos, com a nossa identidade, fáceis de usar, **sem perder nenhuma função**, cada opção explicada, ícones SVG, trabalho profissional. Este documento é o pacote de decisão: inventário, auditoria, proposta e o que a implementação vai fazer. O mockup está em `mockup/` (PNG 1920×1080, gerado por `gen.py`); as JPGs para o dono em `/Volumes/ExternalSSD/tmp/rev/dono-envio/ajfable-*.jpg`.

## 1. O que existe hoje (medido no código, não de cabeça)

| | |
|---|---|
| Opções no enum `OpcaoId` | **426** (o rodapé da tela atual diz 420) |
| No catálogo de apresentação `TELA[]` | **401** em **11 seções** e 58 grupos; 25 vivem em editores próprios (profundidade, MDBList, limites de falha, filtros) |
| Técnicas (atrás de "Mais opções") | 136 |
| Por tipo | 318 escolhas, 26 números, 74 ações, 8 leituras |
| Com frase de ajuda própria (`ajudaOpcaoBase`) | quase todas; o resto cai no texto genérico "Use as setas laterais…" que a nova tela **não** vai mostrar |
| Prévias com arte real (`ajPrevia`, `ajVisualCena`) | 11 cenas de categoria + ~20 prévias de opção, já desenhadas no material da ilha |
| Gerações de UI empilhadas em `ajustes.c` | 4: `ajustes_ux_*` (2.0.3, três colunas), `ajustes_ux_v2.inc`, `ajustes_ux_v4*.inc` (home com cartões) e `ajustes_pen*.inc` (Pencil, páginas fixas 2×7). Total **34 773 linhas** de `.inc` + 11 946 de `ajustes.c` |

Inventário completo, linha a linha, com checkbox: [INVENTARIO.md](INVENTARIO.md). Gerado por `mockup/inventario.py` + `mockup/tela.py`; regenerar quando o enum mudar.

## 2. Auditoria UX — por que "fica cada vez pior"

Lentes: 10-foot UI (leitura a 3 m), D-pad (custo em teclas e previsibilidade), arquitetura da informação (achar e entender), copy (o que a frase promete).

### 2.1 Arquitetura da informação
- **Três taxonomias ao mesmo tempo.** O catálogo `TELA[]` tem 11 seções aprovadas pelo dono em 07/10 (Assistir / Telas / Conta e sistema). A home Pencil mostra **8** cartões com outros nomes ("Minha tela", "Perfil e serviços", "Desempenho"). O mapa v4 tem 44 grupos com nomes diferentes dos `ROT` do catálogo. A busca devolve um caminho, a tela mostra outro.
- **Páginas fixas de 2×7 linhas.** O Pencil corta cada seção em páginas "Reprodução (1/4)… (4/4)" porque o layout aprovado tinha 7 linhas por coluna. Resultado: coluna da esquerda com 2 linhas e um buraco de 400 px (ver antes/depois 2), grupos partidos ("Destaque (1/2)", "(2/2)"), e **o que não coube caiu em "Outros ajustes"** — uma lista cega de 14 por página com ícone genérico e sem agrupamento (hoje ≥ 80 opções estão lá, porque o Pencil só mapeou 342 das 426).
- **Avançado = estado global escondido.** "Simples/Avançado" no topo muda o que cada página contém; a pessoa não sabe o que o botão escondeu. O catálogo já tem a resposta certa (07/10): "Mais opções · N" no fim da lista, abre no lugar.

### 2.2 D-pad
- Esquerda/Direita **viram página** dentro da seção e também trocam de coluna. Na última linha da coluna 1, Direita pula para a coluna 2; na coluna 2, Direita vai para a próxima página. Duas ações diferentes na mesma tecla, sem indicação.
- Voltar às vezes sobe um nível, às vezes volta à página irmã (`ajPenBack` com `fallback` e histórico de 128). Teste `nav21` cobre, mas a regra não é explicável a uma pessoa.
- Linhas de 76 px em ilhas de 7 linhas: 14 opções por tela com 1/3 da tela vazia.

### 2.3 Leitura a 3 m
- Valor atual em 16–17 px cinza à direita; a frase de ajuda em 14–16 px. Abaixo do mínimo de 19 px que o DESIGN.md usa para "sub" e abaixo do que a Home usa.
- O painel de explicação da 2.0.3 (que o dono pediu: "entender tudo o que cada coisa faz") **sumiu** no Pencil: a ajuda virou uma linha de 16 px debaixo do título, cortada por reticências.
- Prévias com arte real existem no código e não aparecem em nenhuma página Pencil.

### 2.4 Copy
- Rótulos repetidos sem contexto ("Tamanho", "Cor", "Fundo" aparecem 2× na mesma página; "Restaurar padrão" 2×).
- Ajuda que repete o rótulo ("Fundo da abertura — Arte de fundo da tela de abertura").
- Texto genérico de fallback promete "setas laterais" que não fazem isso no Pencil.

## 3. Proposta: "três ilhas"

Um só layout para tudo, no material da ilha (vidro ou sólido, mesmas formas):

```
[relógio]                                          [Buscar] [Predefinições] [Diferente do padrão · 3] [O que o Nuvio faz?]
[rail]  ┌─ Seções (400) ─┐ ┌─ Opções da seção (760) ────────────┐ ┌─ Explicação (480) ──────┐
        │ ASSISTIR        │ │ Reprodução   46 opções · 41 técnicas│ │ [prévia 16:9 arte real] │
        │ ● Reprodução 87 │ │ IMAGEM E SOM ──────────────────────│ │ Título da opção         │
        │   Idiomas…   35 │ │ ⟳ Qualidade máxima   Automática ›  │ │ Seção › grupo           │
        │   …             │ │ ✦ HDR e Dolby Vision HDR10 e DV ›  │ │ O QUE FAZ: frase longa  │
        │ TELAS           │ │ ⚡ Dolby Vision            [on]    │ │ Agora / Padrão / Vale p/│
        │   …             │ │ ESCOLHA DA FONTE ──────────────────│ │ dica da tecla           │
        │ CONTA E SISTEMA │ │ …                                  │ │                         │
        │   …             │ │ [Mais opções · 41 técnicas  ⌄]     │ │                         │
        └─────────────────┘ └────────────────────────────────────┘ └─────────────────────────┘
OK abrir · ← voltar · ↕ navegar · → próxima ilha                              Reprodução · Imagem e som · 1 de 46
```

- **Ilha 1 — Seções**: as 11 do catálogo `TELA[]`, nos três blocos aprovados (Assistir / Telas / Conta e sistema), ícone Lucide + nome + contagem. A seção aberta leva o ponto de acento; a focada, a superfície clara.
- **Ilha 2 — Opções**: a lista inteira da seção, com os grupos `ROT` como cabeçalho (kicker + fio), rolagem contínua com mola. Linha: ícone, título 22/600, frase 16 (o `pen->help` ou a 1.ª frase da ajuda), valor 20/500 ou toggle; "alterado" em chip de acento quando difere do padrão. Técnicas ficam depois de **"Mais opções · N técnicas"**, que OK abre no lugar (grupos técnicos com kicker no acento). Sem páginas, sem "Outros ajustes".
- **Ilha 3 — Explicação**: nunca recebe foco sozinha. Mostra a prévia (as cenas `ajPrevia`/`ajVisualCena` que já existem, com arte real), título, caminho, "O que faz" (a ajuda completa, 19 px), **Agora / Padrão / Vale para** (escopo conta × só esta TV, que o código já sabe por `uxEscopoValor`), e a dica da tecla. Com a seção focada mostra o resumo da seção. **Quando OK abre uma escolha, a ilha 3 vira o seletor** (lista de valores, atual com check, explicação do valor focado); OK aplica, Voltar cancela e devolve o foco à linha. Números viram régua na mesma ilha. Ações (chaves, entrar, testar, ordenar) abrem as folhas/formulários que já existem, por cima, como hoje.
- Cabeçalho: kicker + "Ajustes" + chips de ferramenta (Buscar, Predefinições, Diferente do padrão · N, O que o Nuvio faz?). Subir da primeira seção foca os chips.

### 3.1 D-pad (regra inteira cabe numa frase por tecla)
| Tecla | Faz |
|---|---|
| ↑ ↓ | linha anterior / seguinte na ilha atual (seções, opções, ou valores do seletor). Do topo das opções, ↑ vai aos chips do cabeçalho. |
| → | da ilha 1 para a 2 (entra na seção, foco na 1.ª linha). Na ilha 2 não faz nada (nunca muda valor sem OK). |
| ← | da ilha 2 para a 1, lembrando a linha. Na ilha 1 fecha Ajustes. |
| OK | ilha 1: entra na seção. Ilha 2: toggle alterna e grava; escolha/número abrem o seletor na ilha 3; ação abre a folha. "Mais opções" abre/fecha. |
| Voltar | sobe um nível e devolve o foco a quem chamou: seletor → linha; opções → seção; seções → sai. |

Nada de página irmã, nada de tecla com dois sentidos. Valor só muda com OK (decisão da revisão de 02/10, mantida).

### 3.2 Movimento
Ilha com a mola oficial (`anim_mola`/`mov_mola_assenta`); foco de linha interpola a superfície; troca de seção faz a ilha 2 deslizar (velha sai, nova entra, 220 ms como `ajCenaTroca`); prévia da ilha 3 faz o fade já existente. Nada é cortado por TV fraca: o nível automático de GPU cuida.

## 4. Arquitetura da implementação (o que foi feito)

- **Casca nova** `src/ajustes_tri.inc` (estado + eventos) e `src/ajustes_tri_desenho.inc` (desenho), incluídas por `ajustes.c` onde v4 e Pencil entravam. Leem **só** o núcleo 2.0.3 que já lia o catálogo: `TELA[]`, `focar`/`focavel`/`focarSecao`, `maisAberto`, `ajDesenharLinha`/`ajDesenharGrupo`/`ajDesenharMais`, `ajDesenharInspetor` (prévias reais), `aj2InspetorDiferencas`, o editor `ux*` e os modais. Nenhuma lista nova de opções: o que está em `TELA[]` aparece, inclusive o que o Pencil jogava em "Outros ajustes".
- **Menu lateral nunca imitado**: o app desenha o menu real por cima (`menu.c`) em qualquer layout; a tela só reserva espaço. `triX0` = `ajustes_conteudo_x()` quando há rail fixa (Padrão, Moderna com a pílula flutuante), 48 sem rail (recolhida, menu no topo, Dinâmica/Apple TV, onde a pílula do canto já diz "Ajustes" e o título grande some, como em `perfil.c`). As capturas com `NUVIO_RAIL=fixa|moderna|dinamica|recolhida` provam os quatro casos.
- **Geometria** redirecionada: `aj2ListaR`/`aj2InspR`/`aj2EditorR`/`aj2TemInsp` agora devolvem as ilhas 2 e 3 (`triListaR`, `triInspMiolo`, `triInspR`); `AJ_TOPO`/`AJ_BASE` idem. "Lista" em Aparência › Texto e tamanho continua valendo: sem ilha 3, a lista toma a largura e a ajuda abre embaixo da linha.
- **Editores compactos** na ilha 3 (`triEditorEscolha`, `triEditorNumero`): kicker, título, "Vale para · Padrão", lista de valores com o salvo marcado, a ajuda da opção e "Restaurar padrão". Os editores que precisam de largura (categorias do guia, pendrive do P2P, limite de fileiras com a ordem da Home, contas de TV ao vivo) abrem o editor 2.0.3 com a ilha 3 alargada para 760.
- **Rotas utilitárias na ilha 2** (chips do cabeçalho): Buscar = Spotlight em modo Ajustes (como 2.0.3); Predefinições (4 prontas + até 8 salvas por perfil, `ajustes_presets.inc`); Diferente do padrão (lista 2.0.3 + "Restaurar tudo"/"Desfazer"); Prévia da mudança (revisão em lote `rv*`: nada grava sem o OK em "Aplicar"); O que o Nuvio faz? (tour). **Controle parental** virou a 12.ª entrada da ilha 1 (`ajustes_parental.inc`: PIN do perfil, travar, remover, tempo de tela, classificação, busca, compras, filmes, perfil-alvo).
- **D-pad semântico**: `triSetaSemantica` tira as setas das ilhas do vizinho geométrico (`navigationMove`), que continua servindo folhas, filtros e tour. Valor só muda com OK; toggles viram interruptor (52×30) no acento; a estrela de "padrão de fábrica" e os selos "ESTE PERFIL/ESTA TV/AVANÇADO" saíram da linha (o inspetor diz "Vale para" e "Recomendado").
- **Movimento por ilha**, sem fade da tela inteira: lista entra em 220 ms ao trocar de seção (`paginaA`), a ilha 2 acende com o foco (`triListaF`), o editor entra deslizando (`triEditorF`), as rotas utilitárias idem (`triUtilA`). `ajustes_teste_transicao`/`ajustes_teste_movimento` expõem isso ao gate.
- **Removido**: `ajustes_ux_v4.inc`, `ajustes_ux_v4_desenho.inc`, `ajustes_ux_v4_mapa.inc`, os 15 `ajustes_pen*.inc`, `tools/import-ajustes-pen.py`, `tests/ajustes_v4.c`, `ajustes_pen_shot.sh`, `ajustes_v4_shot.sh`. `ajustes_pen_saved.inc` → `ajustes_presets.inc`; `ajustes_pen_parental.inc` → `ajustes_parental.inc`. **Fica para uma segunda poda**: `ajustes_ux_v2.inc` (grade/menu 2.0.3, hoje sem chamador, mas dona de `aj2DicasLinha`, `aj2CorMedidas`, `aj2Mola` e cia) e o ramo morto de `uxEvento` depois de `return triEvento(k)`.
- **i18n**: 25 strings novas em `src/idioma_tab.h` (ordem de bytes) e traduzidas nas 29 irmãs; `tools/idiomas.py` continua com os 42 problemas pré-existentes de cirílico em `idioma_sr.h` (iguais em `integracao/2.1`).
- **Retrato (celular em pé)**: não há ilha 1; as seções viram uma **faixa horizontal** de pílulas sob o cabeçalho (`triFaixaSecoes`, 64 px, rola até a pílula em foco por `triSecRolX`), e as ilhas 2/3 começam depois dela (`triIlhaTopo`). D-pad na faixa: ← → trocam de seção, ↓ ou OK entram, ↑ vai aos chips; ↑ da 1.ª linha da lista volta à faixa. Na tela estreita os quatro chips do cabeçalho ficam só com o ícone (o rótulo aparece no chip em foco) em vez de sumir.
- **Ponteiro/toque nas rotas utilitárias**: linhas de presets, prévia e parental (`triPontUtilLinha`), pílulas de ação (`triPontUtilAcao`) e as escolhas do parental (`triPontParentalValor`) registram `ponteiro_alvo` quando `ajPontUtil` (rota entrou, sem editor/modal); a lista Diferente do padrão idem (`ajPontDif`). Como em toda linha da tela, um clique foca e o ponteiro entrega o OK (ponteiro.c).
- **Rolagem das rotas**: o desenho de presets/prévia/parental anota onde a linha em foco está (sem a rolagem) e deixa `triUtilRolAlvo`; `triAtualizar` leva `triUtilRol` até lá por mola (`triUtilRolar`). Linhas parentais têm 72 px (título + frase) e a lista rola.
- **Toque no celular** (modo Mobile, `ajTelaCheia`): a faixa de seções é uma região de rolagem **horizontal** (`AJT_FAIXA`, `ajToqueRegistrarEixo` com eixo X: as regiões do `ajToqueRolar` agora têm eixo) e as listas de presets/prévia/parental e Diferente do padrão são regiões verticais (`AJT_UTIL`, `AJT_DIFS`). Enquanto o dedo manda (`ajToqueLivre`) a mola do foco não puxa; a primeira tecla devolve a lista ao foco (`ajToqueLimpar`).
- **Diferente do padrão**: o renderizador 2.0.3 (`aj2DesenharDiferencas`) deixou de supor dez linhas: a janela vai de `AJ_TOPO` a `triUtilBase()` (acima das pílulas Restaurar/Desfazer) e a linha em foco fica visível pela mesma mola `triUtilRol` das outras rotas (as linhas fora da janela só contam altura).
- **Escolha parental sem ilha 3** ("Lista", retrato): toma o lugar da lista da rota na ilha 2 (não por cima: o vidro deixava as linhas aparecerem); com ilha 3, abre nela. Nos dois casos `ponteiro_camada()` tira as linhas de baixo do toque, e as setas são dela (`triSetaSemantica` inclui `ajPenParentEditor`).
- **Provas**: `tests/ajustes_tri.c` (rotas, alcance de toda opção, revisão/desfazer, presets, parental), `tests/ajustes_opus.c` portado item a item, `tests/nav21_settings.c` reescrito (ilhas semânticas + movimento), `tests/ajustes_tri_review.c` (fontes reais, texto cortado), `tests/ajustes_tri_shot.sh` (14 quadros + prova de movimento), `tests/ajustes_tri_p1.c` (janela GL oculta, build de toque: faixa do retrato, alvos das rotas, rolagem, escolha parental; `--baseline <commit>` prova que falhava antes; PNGs com um diretório como argumento), `ajustes_tour`/`ajustes_ux_interacao`/`ajustes_escopo` ajustados.

## 5. Estado

- [x] Inventário (426 opções, 11 seções) — INVENTARIO.md
- [x] Auditoria UX
- [x] Mockup (4 quadros + 2 antes/depois) — aprovado pelo dono em 27/10 ("Aprovado, implementa")
- [x] Implementação (três ilhas, editores compactos, rotas, parental, remoção v4/Pencil)
- [x] i18n (25 strings, 29 idiomas)
- [x] Provas: ajustes_tri, ajustes_opus (28/28), padroes, tour, ux_interacao, escopo, toque_modo, categorias_ui, tri_review, ux_dados, busca, perfil, secoes, lista_cabecalho, tela_cobre_enum, nav21_live, sync, texto, dependencia, fonte, iptv, seguro, trakt610, tri_shot; nav21: 3586 checks, 0 violações
- [x] Build Android: build/android/Nuvio-2.0.8-android.apk (73 MB), lite armv7/arm64 — tools/android.sh rc=0 em 28/10
- [x] Entrega à central: branch agente/21-21-ajustesfable, commit 66a9a16fa (+ este doc)
- [x] Re-revisão da central (28/10): 2 P1 — faixa/listas sem rolagem por toque no celular; Diferente do padrão com janela de dez linhas (foco cortado/sobre as pílulas a 100%). Corrigidos; `tests/ajustes_tri_p1.sh --baseline a46ab766e` = 6 FAIL, depois 0 (cenários `toque` e `difJanela`, este a 100% via `ajustes_teste_escala`).
- [x] Revisão da central (28/10): 4 P1 — retrato sem navegação de seções, rotas utilitárias sem alvo de ponteiro, listas das rotas sem rolagem, escolha parental invisível sem ilha 3. Corrigidos com `tests/ajustes_tri_p1.sh` (15 FAIL no baseline 96d834d06, 0 depois). P2 anotados, não feitos: o chip "Diferente do padrão" conta com `AJ_N`/`triMutavel` e a lista com `AJ_N_TELA`; desempenho em TV fraca não medido.
- [x] Merge de integracao/2.1 (fonteincompat + guiapolir): os 6 toggles novos já estavam em TELA[] e aparecem nas ilhas; ganharam ícone e os do guia ganharam ajuda (30 idiomas). Suíte verde de novo, nav21 0 violações, Android rc=0. `ajustes_ux_dados` já falha em integracao/2.1 (uxValorPadrao dos AJ_FONTE_OCULTAR_* devolve 0/1 e valorPadrao é 2) — herança, não desta branch.

## 6. Desvios do mockup, de propósito

- Linhas de 60 px com a frase curta só na linha em foco (2.0.3), não 76 px com frase em todas: cabem 9 opções por tela em vez de 7, e a ilha 3 já explica tudo.
- O "alterado" é o ponto discreto depois do rótulo (2.0.3), não um chip no acento: o chip "Diferente do padrão · N" do cabeçalho já conta.
- O kicker do cabeçalho é "Nuvio · <perfil>" (o nome da TV não existe no app).
