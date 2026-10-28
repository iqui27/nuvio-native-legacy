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

## 4. Arquitetura da implementação (uma geração só)

- Novo `src/ajustes_tri*.inc` (estado + eventos + desenho) incluído por `ajustes.c` no lugar de `ajustes_ux_v4*.inc` e `ajustes_pen*.inc`. Lê **só** `TELA[]` (catálogo), `OPCOES[]`, `ajudaOpcao`, `uxValorAtual/uxValorPadrao/uxEscopoValor`, `ajPrevia`, `ajVisualCena` — nenhuma lista nova de opções.
- Reaproveita os componentes 2.0.8: `ajIlha`, `ajFocoLinha`, `ajNeutro`, `ajPilulaAcento`, `ajDicasLinha`, `plrui_kicker`, `plrui_seg`, `botao_pilula`, `TXT_ILHA_*`, ícones `aj_*` (Lucide).
- Mantém intactos: editores/folhas existentes (perfil, parental, PIN, IPTV, filtros, ordem de provedores, fileiras, presets, review de diferenças, busca/Spotlight, tour, guia, apoio, aviso legal, sync, device). Eles abrem por cima da ilha, como hoje; o `ajPenFind`/`focarOpcao` vira "abrir seção + rolar até a linha".
- Remove: `ajustes_ux_v2.inc`, `ajustes_ux_v4.inc`, `ajustes_ux_v4_desenho.inc`, `ajustes_ux_v4_mapa.inc`, `ajustes_pen*.inc` (as frases de ajuda curtas dos `AjPenRow` migram para uma tabela `AJ_FRASE[op]` reutilizada na linha). As cenas `ajustes_ux_cenas*`, `previa`, `visual`, `secoes` ficam (são a ilha 3).
- Strings novas em `src/idioma_tab.h` (ordem de bytes) + `tools/idiomas.py --sincronizar`.
- Provas: `tests/nav21_settings.c` reescrito para as regras da §3.1; `tests/ajustes_pen_shot.sh` → `ajustes_tri_shot.sh` com um quadro por seção, "Mais opções" aberto, seletor, número, vidro e sólido; `ajustes_opus`, `ajustes_padroes`, `nav21_live`, `ajustes_ux_dados`, `ajustes_busca`, `ajustes_perfil` verdes.

## 5. Estado

- [x] Inventário (426 opções, 11 seções) — INVENTARIO.md
- [x] Auditoria UX
- [x] Mockup (4 quadros + 2 antes/depois) — **aguardando OK do dono na PNG antes de implementar**
- [ ] Implementação
- [ ] Provas (nav, shots, i18n, testes existentes)
- [ ] Entrega à central (branch + commit)
