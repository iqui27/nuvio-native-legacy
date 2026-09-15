# Guia de TV e EPG — como as peças se encaixam

O que o dono pediu: a lista de canais do addon virar um guia de TV de verdade —
canais por categoria, com o que está passando agora, o que vem a seguir,
descrição do canal e favoritos. Este arquivo explica de onde vem cada dado e
por que cada decisão é assim.

## Fontes

| Dado | De onde |
|------|---------|
| Canais, logo, descrição, categoria (`genre`) | catálogo do addon de canal (hoje o FrostView TV, `froststream-channels`), lido por inteiro com paginação `skip=N` — `guia.c`/`lerPagina` |
| O que está passando (agora/a seguir) | XMLTV do epgshare01 (`epg_ripper_BR1.xml.gz` + `_BR2.xml.gz`, ~3,5 dias de grade em português) — `epg.c` |
| Favoritos | `guia-fav.txt` na pasta de dados, um id por linha. Arquivo próprio porque `SalvoItem.id` tem 24 bytes e os ids do FrostView têm ~45 |

## O casamento nome → grade

O addon anuncia "Globo RJ HD", a grade conhece `globo.br`. O casamento é por
nome normalizado (minúscula, sem acento, sem sufixo de qualidade/país) com
três passos em `epg_match`: forma do id do canal da grade, nome normalizado e
a tabela `ALIAS` para os que divergem (RecordTV Paulista → Record TV,
Canal Sony → Sony Channel, H2 → History 2…).

Canais sem grade real — os "24h" de um filme só, cams de reality, feeds de
evento (DAZN, PPV) — não casam e se mostram como "AO VIVO", sem programa
inventado.

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

## Teste

```bash
bash tests/guiaepg.sh    # molde XMLTV: casamento, agora, próximos
bash tests/player.sh     # regressão do player, incl. zap por scancode 480/481
```
