# Nuvio Legacy 2.0.7 — resumo (pt)

Cobre v2.0.6..HEAD. Lista completa de commits em `NOTAS.md`. Discord: https://discord.gg/9NWr6SHyzJ

## Player

| Mudança | Ref |
|---|---|
| Tela de abertura cinematográfica (arte nítida, véu meio, anel sob o logo) | 718e61abb |
| Opção de estilo de abertura: Centro ou Canto | 922500ef4 |
| Abertura Dolby Vision Cinema reusa a abertura normal | d89784e82, 8f788b25b |
| Vidro translúcido sobre vídeo ao vivo; tinta mais densa; vidro esfumaçado nos overlays | 46e0bd5bd, 6f1c61cb8, db42a0a55, 6974f7293 |
| Aviso quando a fonte ignora Range e não dá para buscar tempo | d034960e8, #557 |
| Recarregar vai para Fontes; novo "Tocar do início" | 6a6d5b71b, PR #574 |
| Botão de episódio anterior removido da barra de série | 4e6b592d8 |
| Trilha de áudio escolhida lembrada para a série toda (inclui MKV convertido) | 0c2a1ba2c, 3486c616b, PR #550 |
| Configuração de tamanho do buffer: Auto/64/128/256 MiB, limitada pela RAM livre | 42b3718cf, 62421bfdc |
| Segundo "tentar" não inicia player interno com modal de player externo aberto | a04f2a820 |
| Trailer: áudio dublado/original, legendas YouTube, fallback Apple/IMDb | b43655122, c831a592f, e23f0476d, 1797b5306, 5f4caef1c |
| Trailer IMDb começa em 720p em Samsung ≤2 GB (Auto) | b89ddc139, #584 |
| Contorno de legenda sólido 8 direções; opacidade aplicada uma vez | 6a76a46aa, d0f6f5986 |
| Árabe em Noto Naskh Bold em toda a UI e legendas simples | 458828591 |
| GIFs pesados tocam em câmera lenta | b2aa59ada, #589 |

## Home

| Mudança | Ref |
|---|---|
| Continue Watching até 40 itens sem travar o Home | cee137de7, 4cbf5ad5c, 696a3a6ee, PR #538 |
| Estilo "Split hero" (com ano/nota sob os pôsteres) | 8655956cb, 669fa7b5c, f8815746a |
| Estilos de título de fileira: Clássico, Ilha, Editorial, Destaque | 331746034 |
| Fileira "Big poster" (Pequeno/Médio/Grande) | e50a38ef6 |
| Fundo fixo Cinza/Cor no layout Dinâmico; Imersivo mais calmo | 6da1fc418, e91329dd2 |
| Picker de estilo de fileira anima ao abrir/fechar | 92656f2ae, 84239e65a, 86c763a2d, 40b7d8e15 |
| Trailer do card focado desacoplado do hero; fade do buraco do trailer | a6472f80d, 524de66ac |
| Sem piscar de artwork; mipmaps mantidos na arte do hero | 54242990a, 98bc829e1 |
| Home segue reordenar/desativar e novas fontes sem reiniciar | c47a40788, #468 |
| Linhas fantasma de add-ons removidos; catálogos não declarados sem vaga | 455135df2, 26dbd9a2a, PR #543, #545, #504 |
| Relógio e ilha de retomar acessíveis com Cima | a7d8b1f58, PR #547 |
| Amigos: estilos polidos com foco do Home; último painel no scroll toque | 8806e6aff, d95bd32e1, e27514693 |
| Refresh de catálogo após Add explícito; todas as linhas varridas | 975638744, 53b050753 |

## Detalhe

| Mudança | Ref |
|---|---|
| Comentários Trakt no vidro compartilhado, desenhados uma vez por frame, cartões baratos | a363d821c, 3aec9a8c0, bb9f2abc9, d4317aaa4 |
| Elenco, recomendações, trailers e menu Mais no cartão de vidro | e047eecee, 9ebdc9a3b |
| Coleções carregadas mostram frost; legendas sólidas | 0b52916d0, 937a69475 |
| Menu do título com frost sobre a arte; opção Poster menu | a2ea074c7 |
| Foco de episódio em placa de vidro; scroll título→episódios mais barato | df06809f2 |
| Menos camadas full-screen sobre o trailer ao vivo | 5ed6ddfaa |
| Arte do detalhe reaproveitada ao abrir vídeo | 61d73a5bf, PR #541 |
| Especiais (temporada 0) listados e tocáveis | 8dff0e2a9, PR #569, #556 |
| Falsas temporadas de 1 episódio ignoradas (The Blacklist) | 94d16f7de, 288e531ee |
| Comentários da série restaurados ao abrir | 5195cd14f |
| Retrato do elenco do add-on mantido se TMDB falhar | 773c576f7, a0e225043, PR #540 |
| Tooltip de foco tirado do botão shuffle | c9c2d3f15 |
| Variante Dolby Vision selecionada mantida na saída; zonas do card detalhado | ef33525a8, 4542aeffe, b59987511, 8f788b25b |
| Troca de arte em vidro, não gradiente opaco | 26dbd9a2a |

## Vidro / visual

| Mudança | Ref |
|---|---|
| Frost em quick menu, guia parental, player, Statistics | 3982ef84b, 46e0bd5bd |
| Topo translúcido com frost desligado; sidebar frost; ilhas; pílulas de acento | b5007ae85, 7bf4caac9, 90b0f9cd7, e3eb56d39 |
| Configuração de qualidade do vidro + governador automático | 30a932909, 0fb1a0b1c |
| Frost lê backdrop pré-borrado, sem captura de tela | ab02d4aaa, e9c809475 |
| Uma captura por frame (Android/.tpk); reuso em frame estático | 0c77460c1, d98d72026, 5536ee8da |
| Cópia/blur só das regiões lidas; alvos limitados a 1080p | 3dc70083c, 93b3d69ec, 17717812d, e70adbf7a |
| Sem blur full-screen quando vidro desligado | 033c3fa70, 0723edb59 |
| Controles segmentados, abas de biblioteca/catálogo, modal Saved em frost | 796a3441f, 21696fd8c, fb4af3ee5 |
| Tamanho de texto e componentes 85–130% (padrão 100%) | 406359d3a, bab9c56e3, e7fa6d67c, 3f8d304b7, 1eb28a937 |
| Escala em Busca, View all TV, teclado, Ranking/Saga, colunas em px reais | f04e6de29, 5ff1d7ef3, e24e614a2, 9dbe1a156, 8b1f605d0 |
| Arte de coleção/episódio mantém dimensões | 79a2ec3e0, 531a36f6e, 93fb92118 |
| Qualidade Alta de imagem busca no tamanho do desenho; backdrop de recomendações | ff1cd6dcd, 44bf4884a, 946ad1819, PR #546 |

## Fontes / add-ons

| Mudança | Ref |
|---|---|
| Fontes sumindo: hostname antes de exigir P2P; legenda assumida até o manifesto responder | cbcf467f5, 27988615e, #552 |
| URL direta toca sem P2P; notWebReady não esconde | d25a3e513, PR #559 |
| PenguPlay: streams http notWebReady oferecidos com P2P off | 6d524f947, c32a87bbf, 68cf2c63b, #565 |
| Busca: série de título exato não some atrás do limite | 2e5b2ffde, e0ddeca5f, #555 |
| Episódios preenchidos do catálogo para ids tmdb: sem IMDb | d479a8463 |
| Busca abre o título selecionado; falha de voz reportada | a2fd7e41d |
| Jellyfin: todas as views de servidores com muitas bibliotecas | b0e429762, #468 |
| Live TV CNCVerse: ids longos | 630292d7e |
| Deep links `nuvio-legacy://`, `stremio://`, imdb.com | 8f38bb943, 7cf145dca, a3eb1e19a, f44e64863, #539 |
| Perfil: sessão velha recuperada; PIN abre sessão | 1c9043ed0, a2e9daf91, #551, PR #554 |
| Nota IMDb 0–10 em vez de % | 03a144294, 0d998e33a, #590 |
| Ver tudo (Showcase / resultados por catálogo) | 4baf783fa, PR #568, PR #563 |
| Identidade da série na busca/reprodução | cdf5bdd88, PR #561 |

## Android

| Mudança | Ref |
|---|---|
| Player não pausa ao mudar volume | 4a0b3408d, #581 |
| Player externo (TV) + separado para anime | f954a272c, 059f4d4f3, 276207fac, #501 |
| Modo TV/Mobile automático no APK padrão; ajustes mobile | c305e6f7e, 4bd7a33cc, acc47411f, 29a516106, 7163e053c, PR #431 |
| Remux grande abre mais rápido (índice final paralelo, redirecionamento lembrado) | cc61bfdd8, 8876c13c6, 809bd7c96, PR #572 |
| Destino lembrado que serve outro arquivo não é confiado | f6618db6b |
| Buffers de preload em MiB; dica de log no canvas Mobile | 35a944d9f, 510007bdd |
| Cache P2P em USB: linha, detalhes, volumes via getExternalFilesDirs, FAT32 | 9b4f37efb, b090905dc, a633888a3, 51daddd84 |

## Samsung .tpk

| Mudança | Ref |
|---|---|
| Tela preta no start: IME adiado, falha exposta | cc49341e6, #558 |
| Raízes USBDrive* + privilégio externalstorage | 31a62b273 |
| Uma captura de backdrop por frame no .tpk | d98d72026 |
| Trailer IMDb 720p em ≤2 GB | b89ddc139, #584 |

## LG webOS

| Mudança | Ref |
|---|---|
| Baixa memória (#415): descarte de buffers CPU, piso de cache ao abrir player | bcdd3eb04 |
| Teto de texturas por MemAvailable; retry travado após erro de pipeline | f77eeeef7 |
| Piso de reprodução por MemTotal; alívio total no aviso | 87977f537, 370769c09 |
| Flush real do cache libass, piso PiP, teto de reprodução por memória | 8feec1a91 |

## Trakt / Simkl

| Mudança | Ref |
|---|---|
| Scrobble com identidade exata do episódio | ff1f1aa25, PR #560 |
| Estado assistido persiste entre reinícios e pulls velhos | 66a608ef7, b074d9fd5, #548 |
| Retries revalidados; escritas serializadas; overrides de episódio reaplicados | 8330673a8, 2d04fbecc, 6db247078 |
| Simkl: anime entra nas listas de séries, Continue Watching, Plan to Watch | f5dab2f8a |

## Testes

| Mudança | Ref |
|---|---|
| Provas de escala de texto, pixels e artwork (determinísticas) | 4bfafc814, 15903519f, d6b0c2363, 915e360a4, 207e0a283 |
| Harness de flicker (pisca3, ANGLE GLES2) e FPS de detalhe | 01603fd5b, 491f8ce0e, 9356bc16c |
| Cobertura PenguPlay, metadados, CW, remoção de add-on, relógio | c32a87bbf, 8c99638f7, a06447082, bd6e61119, d570cb3c1 |
| Cobertura Reload/Restart e identidade de título | 075fec9a4, 2537db6a0 |
| Links de fixtures em Mach-O (stubs fracos) | 5b37cecd8, 5a926a863, c76ed4632 |

## Pendente

- Scroll título↔episódios ainda lento em algumas TVs; em medição.
- Testes `legenda_ass` / `mkvass` sendo consertados.

## Issues

#415 #468 #501 #504 #505 #528 #539 #545 #548 #551 #552 #555 #556 #557 #558 #565 #581 #584 #589 #590 (e #401 #407 #429 #434 #502 — ver NOTAS.md).

## PRs da comunidade

@ghbarker #431 #538 #540 #541 #543 #546 #547 #560 #561 #563 #568 #569 #571 #574 · @Tolu-Walop-E #550 #554 #572 · @mark-major #559 · @Namer03 #567.
