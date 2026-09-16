# Guia de TV e EPG — como as peças se encaixam

O que o dono pediu: a lista de canais do addon virar um guia de TV de verdade —
canais por categoria, com o que está passando agora, o que vem a seguir,
descrição do canal e favoritos. Este arquivo explica de onde vem cada dado e
por que cada decisão é assim.

## Fontes

| Dado | De onde |
|------|---------|
| Canais, logo, descrição, categoria (`genre`) | catálogos de **qualquer addon de canais** — `ehCanal()` aceita `channel`, `tv`, `channels`, `live` e `iptv`; metas cujo `type` não é desses são ignorados. Lidos por inteiro com paginação `skip=N` — `guia.c`/`lerPagina`. As fontes vêm das fileiras montadas E da sonda de manifestos (`sondaManifestos`), que varre `catalogs[]` de cada addon ativo — a home tem teto de 16 fileiras, então um catálogo de canal fora do corte não pode deixar o guia sem dados |
| O que está passando (agora/a seguir) | XMLTV do epgshare01: `epg_ripper_BR1/BR2` (Brasil) mais `PT1`/`MX1`/`AR1` (Portugal, México, Argentina) para canais de addons de outras regiões. US/UK ficam de fora — ~60 MB de XML descomprimido por ganho raro — `epg.c` |
| Favoritos | `guia-fav.txt` na pasta de dados, um id por linha. Arquivo próprio porque `SalvoItem.id` tem 24 bytes e os ids do FrostView têm ~45 |

## O casamento nome → grade

O addon anuncia "Globo RJ HD", a grade conhece `globo.br`. O casamento é por
nome normalizado (minúscula, sem acento, sem sufixo de qualidade/país) em
passos em `epg_match`: exato, `ALIAS` (RecordTV Paulista → Record TV, H2 →
History 2, TV União → Record TV…), prefixo, substring de chave mais longa
única ("TV Cidade - RecordTV" → `recordtv`), e primeiro token para afiliadas
("SBT Thathi Vale" → `sbt`). Entradas regionais no XMLTV
(`São.Paulo/SP..Cartoonito.br`) têm a chave extraída depois do `..` —
`wChavePorId` —, senão o prefixo da cidade vira parte do nome e nada casa.
Duplicatas do mesmo canal na grade (várias regiões) não contam como
ambiguidade.

Medido no catálogo real do FrostView: **~40% dos 768 canais casam** (311);
o teto da fonte BR é esse — o resto é loop "24h", cam de reality, feed de
evento (DAZN, PPV) ou canal sem cobertura XMLTV, e se mostra como "AO VIVO"
sem programa inventado.

## Atualização

O XML fica em cache na pasta de dados e é rebaixado quando passa de 12 h. O
download e o parse rodam num fio próprio; o fio de desenho só publica o
resultado pronto (`epg_passo`), nunca lê o staging.

## Controle remoto

- Guia e overlay: segurar ↑/↓ por ~600 ms entra no modo pula-categoria; 2 s
  parado volta ao normal. OK longo = favorito.
- Player com canal no ar: BAIXO ou botão AZUL abrem o overlay do guia;
  CH+/CH− (scancodes 480/481 do `SDL_webOS.h`) zapeiam na ordem do guia —
  `NV_SCANCODE_CH_UP`/`NV_SCANCODE_CH_DOWN` em `layout.h`, pedidos
  `player_pediu_zap`/`player_pediu_guia` consumidos em `app.c`.
- No Tizen a casca entrega CH+ como a tecla `s` (tizen-shell.html); por isso
  `s` também abre/fecha o overlay quando um canal está no ar.

## Identidade do canal no player

O player guarda só um índice no vetor do catálogo, e a descoberta republica
esse vetor inteiro a cada fileira que responde — inclusive durante a
reprodução. Sem proteção, o índice passa a apontar para outro item e a sessão
deixa de ser "canal": o Baixo volta a só acordar os controles e o overlay
nunca abre. Por isso `player_abrir` congela `canalSessao`, `idCanal` e
`tituloCanal` na abertura (`player_id_canal`), e `tocarCanal` ainda chama
`player_marcar_canal` depois — a janela entre `cat_acrescentar` e
`player_abrir` pode atravessar uma republicação. A repetição de KEYDOWN da
tecla que abre o overlay (azul/`s`) é absorvida por `G_OVERLAY_REP_MS`.

## Teste

```bash
bash tests/guiaepg.sh    # molde XMLTV: casamento, agora, próximos
bash tests/player.sh     # regressão do player, incl. zap por scancode 480/481
```
