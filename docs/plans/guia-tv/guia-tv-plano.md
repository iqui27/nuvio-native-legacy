# Guia de TV / Live TV profissional — plano

Data: 06/10/2026 · Base lida: `/Volumes/ExternalSSD/nv-202-amigosdup` (master depois da v2.0.1, `23498abd`) · Nenhum repositório foi alterado.
Pesquisa com links: `guia-tv-pesquisa.md` (partes: `guia-tv-pesquisa-apps.md`, `guia-tv-pesquisa-tecnica.md`).

Pedido do dono: *"Vamos focar no guia de TV também, deixar ele mais confiável, com separação de addon também. Faça uma pesquisa sobre tudo que é útil, que outros usam, usabilidade e praticidade e organização. Profissional."*

---

## 0. Resumo executivo

1. **O guia de hoje é bom de olhar e frágil por dentro.** A tela (herói com preview 16:9, modo Lista com régua de tempo, mini guia sobre o vídeo, OSD próprio, lembretes, diagnóstico da Live TV) já está no nível dos guias FAST. O problema é o modelo de dados: **uma lista única de 900 canais e 48 categorias para todas as fontes juntas**, categorias fundidas por nome e ordenadas em ordem alfabética. Nos logs da 2.0.x, **18 das 23 pessoas com Xtream (78%) têm a lista cortada**: a lista real tem mediana de 16,5 mil canais (máx. 57 mil) e o guia mostra ~5,5% dela, sem avisar na tela.
2. **"Separação de addon" é o pedido central e hoje não existe.** O painel "Addons" só liga e desliga fontes. A proposta: **Fonte** vira o primeiro nível de organização (abas/chips "Todas · Favoritos · <addon A> · <addon B> · Xtream · Stalker"), cada fonte com seus grupos, sua ordem, seu EPG, sua saúde e seu cache. É o modelo do Kodi (uma instância por fonte) com a conveniência do TiviMate e do Channels (vista "Todas" mesclada e listas próprias).
3. **Confiabilidade do EPG:** o parser lê o XML inteiro na memória (teto de 32 MB de rede / 128 MB de XML; 12/48 MB no .wgt). XMLTV de provedor grande é descartado ("maior que o teto, ignorada", #158): **42% das tentativas** nos logs 2.0.x. A troca é um **parser em stream** (expat ou um parser próprio por blocos) que filtra pelos canais da lista enquanto lê, com cache binário compacto por fonte. Fim do teto por tamanho de arquivo, e o teto passa a ser de memória retida.
4. **Casamento de EPG em cascata, explicável e corrigível:** `epg_channel_id`/`tvg-id` exato → nome normalizado → apelidos → fuzzy, com o resultado guardado por canal e **correção manual** ("Escolher grade deste canal"), como no TiviMate e no Kodi.
5. **Fuso:** guardar sempre epoch UTC e aplicar **offset por fonte e por canal** (−12 a +14 h), além do `server_info.timezone` do Xtream para catch-up.
6. **Reprodução ao vivo:** o caminho por plataforma já existe (proxy TS no webOS, formatos m3u8/ts do Xtream, watchdog por fonte). Falta torná-lo **automático e memorizado por fonte e por canal** (o "Modo do player A/B/C" e "Formato do Xtream" saem dos Ajustes comuns e viram resultado do diagnóstico guardado por fonte), e tratar `BEHIND_LIVE_WINDOW` no Android.
7. **Recursos que os profissionais têm e nós não:** último canal (recall), digitar número no controle, catch-up/arquivo (Xtream `tv_archive`), listas próprias e reordenação, esconder canal/grupo, PIN parental por grupo, M3U+XMLTV por URL, busca de programas no EPG inteiro, "Agora" (parede do que está no ar), guia de vários dias.
8. **Faseamento:** MVP de confiabilidade e separação por fonte, já com M3U e XMLTV por URL (~5 semanas de agente), v1.5 com organização (listas, recall, número, M3U) (~3 semanas), v2 com catch-up, multi-EPG avançado e "Agora" (~4 semanas). Gravação fica fora (inviável e arriscada nas TVs).

---

## 1. Estado atual (mapeado no código e nos logs)

### 1.1 Módulos

| Arquivo | Linhas | Papel |
|---|---|---|
| `src/guia.c` / `guia.h` | 4.319 | Tudo do guia: carga de canais (addons Stremio `channel/tv/channels/live/iptv`, Stalker, Xtream), cache em disco, favoritos, modos Cartões/Lista, mini guia (overlay), gaveta de categorias, painel de addons, busca, preview, zapping, lembretes no guia |
| `src/epg.c` / `epg.h` | 1.316 | XMLTV do epgshare01 por país (BR1, BR2, PT1, MX1, AR1 por padrão; países escolhíveis) + "sexta fonte" = `xmltv.php` do Xtream; casamento por nome normalizado + apelidos + prefixo; cache .gz 12 h |
| `src/xtepg.c` | 261 | `get_short_epg` canal a canal para Xtream (192 canais LRU, 12 programas, validade 30 min, recuo em 429) |
| `src/xtream.c` | 699 | `player_api.php`: categorias, canais, conta (`user_info`), formatos, URL, grade curta |
| `src/stalker.c` | — | Portal MAC (create_link com renovação) |
| `src/aovivo.c`, `aovivoui.c` | 77 + 431 | Zapping com debounce (600 ms), agora/a seguir, OSD do canal, banner, cartão de erro |
| `src/livetvdiag.c`, `livetv_regras.h` | 1.080 + 217 | Diagnóstico (rede, conta, HLS vs TS, modos A/B/C do player, recomendação aplicável) |
| `src/proxyts.c` | ~700 | Proxy local de TS (só webOS; desligado em .wgt, .tpk e Android) |
| `src/guialembrete.c`, `lembrete.*` | 169 | Lembrete de programa (cartão "começa agora") |
| `app.c` (trechos) | — | Resolução de fonte de canal Xtream (variantes FHD/HD/SD × m3u8/ts × proxy), watchdog "fonte morta → próxima", erro com motivo da conta |
| Testes | — | `tests/guiaepg`, `xtepg`, `xtream`, `canalfila`, `aovivo`, `livetv_regras`, `guia_shot`, `aovivo_shot`, `livetvdiag_shot`, `fluidez_perf_guia`… |

### 1.2 Como os canais entram

- **Addons Stremio**: catálogos do tipo `channel/tv/channels/live/iptv`, achados nas fileiras da home e nos manifestos (`addons_catalogos_canal`). Paginação `skip=100`. Prazo 15 s por página. Nenhum EPG vem do addon: o app **não lê `meta.videos` com `startTime/endTime` nem `behaviorHints.epgProvider`**, que é o jeito Stremio de publicar grade.
- **Stalker** e **Xtream**: entram depois dos addons, no mesmo vetor.
- **Não há** M3U por URL nem XMLTV por URL configuráveis pelo usuário (só o `xmltv.php` implícito do Xtream).

### 1.3 Limites estruturais (a causa de boa parte da "falta de confiança")

| Limite | Valor | Efeito |
|---|---|---|
| `G_MAX_CANAL` | 900 canais **somando todas as fontes** | Xtream grande é cortado ("lista cortada: X de Y"); cota por catálogo com piso de 150 |
| `G_MAX_CAT` | 48 categorias **somando todas as fontes** | Canal cuja categoria passa da 48ª **é descartado em silêncio** (`sCatDe` devolve −1 e o canal não entra). Xtream tem `XT_MAX_CAT 256` |
| Categorias | fundidas **por nome** entre fontes, ordem **alfabética** | "Esportes" do addon A e do Xtream viram uma fileira só; não há como ver "só o meu Xtream" |
| `GCanal` | ~2,4 KB × 900 × 3 vetores ≈ 6,5 MB estáticos | Memória fixa mesmo para quem não usa Live TV |
| EPG | arena 16 MB, 400 mil eventos, 2.400 canais; XML inteiro em memória | XMLTV do provedor acima de 32 MB (rede) é ignorado |
| Janela do modo Lista | 120 min visíveis, até +3 h | A grade cobre 3,3–3,8 dias, mas a tela não passa de 3 h; não há "amanhã" |
| Atualização | lista: cache + rede a cada abertura; EPG 12 h | Sem atualização em fundo programada; sem "última atualização" visível por fonte |
| Teclas | Azul (mini guia), Amarelo (modo), Verde (diagnóstico), CH± (salta categoria) | Controles Samsung novos não têm botões coloridos (#267); Tizen não repassa Amarelo nem CH− |
| Player | `0` = proporção; sem dígitos para canal; sem "último canal" | Faltam os dois atalhos mais usados de TV paga |

### 1.4 Issues

- **#158** (aberta, 29 comentários, LG C4 webOS 11.2, Xtream romeno). Linha do tempo: crash ao ir para tela cheia (1.5.2) → canais não tocam e guia vazio → EPG do provedor lido e casado por `epg_channel_id` (1.5.3) → XMLTV do provedor **maior que o teto e ignorado** → grade curta `get_short_epg` (xtepg) → conta só TS e o decoder da C4 **não começa a decodificar** apesar de 8–36 s em buffer → diagnóstico com modos A/B/C → proxy TS local (1.7.0) → canal pausa depois de segundos porque uma segunda conexão derrubava o fluxo principal (corrigido em 1.7.1) → "funcionando". Lições: (a) cada provedor e cada TV pedem um modo diferente, e só o diagnóstico descobriu; (b) contas Xtream com 1 conexão quebram com qualquer sonda paralela; (c) EPG grande é a regra, não a exceção; (d) foram ~10 builds de ida e volta com o usuário.
- **#112** (Samsung, canais IPTV não tocam): tipo `tv` vs `channel` e Xtream via proxy no Samsung.
- **#237**: campo de endereço Stalker/Xtream não aceitava `/` e `_` (corrigido em 2.0).
- **#267**: controle Samsung novo sem botão azul; resposta: CH± já atribuídos. Confirma que o guia não pode depender de teclas coloridas.
- **#187**: o "Guide" do título é outra coisa (TMDB), mas mostra que "Guide" é ambíguo na interface em inglês.

### 1.5 Números dos logs 2.0.x (D1, somente leitura)

Consulta somente leitura na tabela `registro` (ids 36002–43526, versões `2.0.0`/`2.0.1`, 06–07/10/2026). Contagem por **log enviado** (um log = uma sessão enviada; a mesma pessoa pode mandar vários). Os logs são cortados em ~200 KB, então tudo abaixo é **piso**, não teto. Pessoas contadas por hash, nada pessoal guardado.

| Medida | Valor |
|---|---|
| Logs 2.0.x | 5.991 (android 1.881 · webOS 2.018 · tizen-tpk 1.648 · tizen .wgt 213) |
| Logs com o guia em uso (`[guia]`) | 544 (≈ 9%), de **108 pessoas** (android 48 · tpk 33 · webOS 26 · .wgt 3) |
| Logs com Xtream | 118, de **23 pessoas** (nenhuma no .tpk) |
| **Lista Xtream cortada** ("lista cortada: X de Y") | **86 logs, 18 das 23 pessoas com Xtream (78%)**. Tamanho real da lista: **mediana 16.462 canais, máximo 57.302**; o guia mostra **mediana de 5,5%** da lista |
| Listas do guia publicadas no teto (≥ 890 canais) | 89 de 391 publicações, 27 pessoas |
| Listas no teto de categorias (48) | 20 publicações, 5 pessoas (canais das categorias além da 48ª somem em silêncio) |
| Grade do provedor (`xmltv.php`) | 117 tentativas: **56 ok · 49 maior que o teto, ignorada (42%) · 10 sem resposta · 2 sem corpo** |
| Grade curta Xtream (`get_short_epg`) | 224 pedidos: **21 com programa (9%)**, 155 sem programa, 48 falhas; 2 logs com HTTP 429 |
| Grade epgshare01 publicada | 497 publicações; mediana 797 canais / 90 mil programas; cobre **+3,8 dias** (mediana); 2 com zero programa |
| Países da grade | 304 com o padrão BR,PT,MX,AR; o resto ES, IT, FR, UK, NL, DE, RO… (o app já escolhe pelo país das listas) |
| **Fonte de canal morta** (`[guia] fonte N morta`) | **152 logs, 34 pessoas**; 99 logs tentaram a próxima fonte |
| Causas nas fontes mortas (top) | sem erro do player + decoder anunciado e buffer 0 (34) · Android `2004` HTTP inválido (31) · webOS `203 AV Type Not Founded` (20) · Android `3003` contêiner não suportado (17) · `100 Playing error` (14) · `300 Network Error` (10) · Samsung `0xfe6c0026/31/23` (14) · Android `1002` = **BEHIND_LIVE_WINDOW** (3) |
| Conta Xtream (`user_info`) | 85 ativas m3u8+ts · 11 **só TS** · 6 sem resposta · 5 HTTP 513 · 4 HTTP 403 com página HTML · 4 auth=0 |
| Lista Xtream com problema | 5 respostas vazias · 3 páginas HTML · 6 "servidor não respondeu" · 4 credencial recusada |
| Diagnóstico da Live TV | 127 logs (26 pessoas); das 40 recomendações, **20 não mediram nada** (0/0 canais testados); onde mediu: HLS tocou 34/41, TS 0/6 |
| Guia usado pelo preview / busca | 198 logs com "canal no ar foi para o preview"; só 5 com busca no guia |
| Rede vazia (lista do cache mantida) | 7 logs (todos .tpk) |

Leitura:
1. **O maior problema de confiabilidade é o corte de lista**: quase toda pessoa com Xtream vê ~5% dos canais e não sabe. Somado ao teto de 48 categorias, explica "não acho meu canal" sem nenhum erro aparente.
2. **O EPG do provedor falha em 42% das vezes por tamanho**, e a grade curta só acha programa em 9% dos pedidos (painéis respondem vazio). O caminho que funciona (XMLTV do provedor) é justamente o que o teto derruba.
3. **Abrir canal falha por formato/decoder, não por rede**: os erros dominantes são de contêiner/tipo (`203`, `3003`, `2004`, decoder mudo com buffer 0). Isso pede escolha de formato/modo **memorizada por fonte**, não mais tempo de espera.
4. **O diagnóstico existe mas metade das execuções não testa nada** (fileira sem canais testáveis ou saída antes de medir): precisa escolher os canais sozinho.
5. Não há linha de log com a **taxa de casamento de EPG** nem resumo por fonte; o plano inclui isso (M6) para medir o efeito das mudanças.

---

## 2. O que os profissionais fazem (resumo da pesquisa)

Matriz completa e fontes em `guia-tv-pesquisa.md`. O que importa para nós:

| Recurso | TiviMate | Kodi IPTV Simple | Channels DVR | YouTube TV / Sling | FAST (Pluto, TV Plus, LG Channels) | **Nuvio hoje** |
|---|---|---|---|---|---|---|
| Grade de tempo | sim | sim | sim | sim | sim | sim (2 h, até +3 h) |
| Mini guia sobre o vídeo | sim | parcial | sim (filtrável) | sim (Baixo 2×) | parcial | sim (faixa) |
| Separação por fonte | parcial (mescla, filtra por playlist no Premium) | **sim (instância por fonte)** | parcial (regras por fonte) | n/a | n/a | **não** (só liga/desliga) |
| Grupos (ocultar, renomear, reordenar) | sim | parcial | sim (Collections) | parcial | sim | não (alfabético fixo) |
| Favoritos / listas próprias | sim | sim | sim | sim | sim | só Favoritos |
| Número de canal / digitar número | sim | sim (`tvg-chno`) | sim | não | não (Pluto decidiu não ter) | posição na lista; sem dígitos |
| Último canal (recall) | sim | sim | sim (Jump to Last) | sim | parcial | **não** |
| Busca de canal + programa | sim | parcial | sim | sim | sim | sim (canais + programas da grade carregada) |
| Catch-up (`tv_archive`) | Premium | sim | não | restart | não | **não** (só "voltar ao vivo" na janela DVR do HLS) |
| Lembretes | n.v. | parcial | n.v. | sim | n.v. | sim |
| Gravação | Premium | via PVR | sim | sim (nuvem) | não | não (fora do escopo) |
| EPG multi-fonte + match tvg-id → nome | sim (1ª fonte vence) | sim (3 passes) | sim | n/a | n/a | parcial (fontes fixas por país + Xtream; match por nome) |
| Offset de fuso por fonte / canal | sim / sim | sim (−12/+14 h) | n.v. | n/a | n/a | **não** |
| Atualização do EPG | 4–24 h, "ao abrir" | ao abrir | servidor | servidor | servidor | 12 h, só com o guia aberto |
| PIN parental por grupo/canal | sim | sim | sim | n/a | n/a | não |
| Saúde da fonte / diagnóstico | parcial | log | sim (servidor) | n/a | n/a | **sim** (diferencial nosso) |

Padrões consagrados que adotamos: favoritos como primeira aba; mini guia sobre o vídeo; "Agora" como parede de cartões além da grade; filtros "grudam" entre sessões; recall com uma tecla; digitar número com prévia; troca rápida de fonte; editar o EPG ID de um canal; offset por fonte; atualizar em fundo e mostrar "atualizado há X". Reclamações a evitar: "No information" por tvg-id divergente; horário deslocado 1 h ou 5–8 h; reimportar EPG grande a cada abertura; travar com playlist grande em TV de pouca RAM; editor de canais só fora da TV; recursos essenciais escondidos.

---

## 3. Experiência alvo (tela a tela, linguagem Glass UI)

Princípios: (1) **a fonte é visível sempre** — todo canal mostra de onde vem; (2) **nada depende de botão colorido** — toda ação tem caminho por D-pad + OK + menu de opções (segurar OK), as cores são atalhos; (3) **o que falhou é dito no lugar onde falta** (fileira, canal, célula de programa), nunca só no log; (4) **o que já carregou aparece na hora** (cache), e a atualização acontece em fundo, com selo "atualizado há 2 h".

### 3.1 Barra de fontes (novo, topo do guia)

Chips de vidro (Frost) logo abaixo do cabeçalho, no lugar da linha de modos:

`★ Favoritos · Todas · Meu Futebol · Pluto TV · Fenix · Xtream (Provedor X) · Stalker · + Adicionar`

- Cada chip: logo/ícone da fonte, nome, contagem de canais e um **ponto de saúde** (verde ok, âmbar degradado/EPG velho, vermelho fora). Foco no chip abre um cartão pequeno: "412 canais · 18 grupos · grade em 61% · atualizado há 2 h · Atualizar · Diagnóstico · Ajustes da fonte".
- **Todas** mescla, mas cada linha de canal carrega uma etiqueta discreta da fonte (14 px, translúcida). Em Todas, grupos de mesmo nome vindos de fontes diferentes ficam separados e aparecem como "Esportes · Xtream" (decisão 2). A ordem dos grupos é a do provedor, com a opção "A–Z" em cada fonte (decisão 3).
- O guia abre sempre em **Todas** (decisão 1). A última fonte escolhida vale só durante a sessão.
- CH± dentro do guia: troca de grupo (como hoje). **Segurar Esquerda no primeiro canal** ou subir até a barra: troca de fonte.

### 3.2 Guia em grade (o modo Lista vira o padrão)

```
┌ cabeçalho: Guia de TV · 21:42 (ilha do relógio) ─────────────────────────────┐
│ [★ Favoritos][Todas][Meu Futebol][Pluto][Xtream ●]          [Buscar][Opções] │
├ herói: logo, nome, nº, fonte · programa no ar (barra) · a seguir │ preview 16:9│
├ régua: Hoje ▸ 21:30 │ 22:00 │ 22:30 │ 23:00          [◂ Ontem | Hoje | Amanhã ▸]│
│ grupos (coluna de 260 px, recolhível) │ 101 ▣ SporTV    │ Futebol ███ │ Jornal ██ │
│  Esportes  ▸                          │ 102 ▣ ESPN      │ NBA ███████ │           │
│  Notícias                             │ 103 ▣ Premiere  │ (sem grade — ao vivo)   │
└ dica: OK assistir · Segure OK opções · ◂▸ horário · CH± grupo ─────────────────┘
```

- **Foco por programa** (não só por canal): Direita anda célula a célula; OK num programa futuro abre a ficha com "Lembrar", "Assistir canal agora" e, quando houver arquivo, "Assistir desde o início"; OK no programa atual toca.
- **Dias**: régua com "Hoje/Amanhã", janela navegável até o fim da grade real (`epg_janela_total`), e para trás até o limite do catch-up da fonte. Salto de 6 h com CH± quando o foco está na régua.
- **Coluna de grupos** à esquerda (recolhível), como Channels/TiviMate: substitui a gaveta escondida atrás de "segurar ↑/↓" (que fica como atalho).
- **Células sem grade**: "Ao vivo · sem programação" com textura distinta. Grade que acabou: "Programação até 23:00" (já existe a semântica em `epg_janela`).
- **Modo Cartões** continua como "Agora" (ver 3.4), não como segundo modo da grade.

### 3.3 Mini guia / OSD do canal (sobre o vídeo)

- **Baixo** com o OSD fechado abre a faixa (hoje é o Azul): o canal atual e 2 de cada lado, agora/a seguir. Azul segue como atalho. **Baixo duas vezes** abre o guia completo com o canal no preview (padrão Sling).
- Faixa ganha **filtro de fonte/grupo** no topo ("Xtream · Esportes ▾"), para zapear dentro do grupo, como no Channels.
- **Recall**: tecla "Voltar" rápida dentro de 1 s depois de um zap volta ao canal anterior; botão "Último canal" no OSD; e a tecla **"0"** quando não há número sendo digitado. Isso vale só na Live TV: a proporção da Live TV vai para o OSD, e no VOD o "0" continua trocando a proporção (decisão 5). Duplo OK também, como Channels.
- **Digitar número**: dígitos abrem um selo "1 0 2 ▸ ESPN" com prévia e confirmam em 1,5 s ou com OK. Numeração por fonte (`tvg-chno`, `num` do Xtream ou a posição). Com uma fonte filtrada, o número leva ao canal dela. Em "Todas", o número é procurado nos Favoritos e depois nas fontes, na ordem da barra. Se o número for ambíguo, a tela mostra "fonte · número" (decisão 7).
- **Info do programa**: OK no título do agora abre o painel da ilha (já existe `aovivo_info_corpo`) com descrição, gênero, classificação, "começou há 12 min", e "Assistir desde o início" se houver arquivo.
- **Erro do canal**: cartão atual (Recarregar, Fonte, Guia) ganha "Próxima variante (HD/SD)", "Pular para o próximo canal" e a frase da causa conhecida (conta, formato, provedor 404/458, decoder).

### 3.4 "Agora" (parede do que está no ar)

Fileiras de cartões (o modo Cartões atual) por grupo, ordenadas por "assistidos recentemente" → favoritos → grupos. Cada cartão: logo, título do programa, barra de progresso, selo da fonte. É a porta de entrada da Live TV na home ("Ao vivo agora") e a primeira tela para quem não é de grade (padrão Channels "On Now", Pluto, Google TV Live).

### 3.5 Lista de canais e organização (por fonte)

Tela "Gerenciar canais" (Opções ▸ Organizar), uma por fonte e uma para Favoritos/listas:

- Grupos: **mostrar/ocultar, renomear, reordenar** (ordem do provedor por padrão, não alfabética; alfabética como opção).
- Canais: ocultar, mover, renumerar, marcar favorito, "Escolher grade (EPG) deste canal" (lista de candidatos da grade com o casamento atual destacado), "Ajustar horário deste canal (±h)".
- **Listas próprias** ("Futebol", "Crianças"): aparecem como chips ao lado de Favoritos; um canal pode estar em várias. Guardadas por perfil e sincronizadas com a conta (sync já existe para addons).
- Edição por D-pad: segurar OK num canal → "Mover" → ↑/↓ → OK. Nada exige celular ou web (reclamação recorrente do YouTube TV).
- Variantes FHD/HD/SD do mesmo canal (já detectadas por `nv_nome_base`): **agrupar numa linha só** com a resolução escolhida automaticamente e as outras como fontes alternativas. Isso reduz listas Xtream em 30–50% de linhas.

### 3.6 Busca

Uma caixa, três seções de resultado: **Canais** (por nome, número, fonte), **No ar agora**, **Mais tarde** (programas da grade inteira, todos os dias carregados), cada resultado com o selo da fonte. Busca de programa usa um índice de títulos montado no parse (só títulos dos canais da lista). Também alimenta o Spotlight (já existe `guia_buscar_canais`).

### 3.7 Ajustes ▸ Live TV (reorganizados)

- **Fontes**: lista com as de addon (vêm sozinhas) e as adicionadas à mão: Xtream, Stalker, **M3U por URL**, **XMLTV por URL**. Por fonte: nome de exibição, ligar/desligar, atualizar a cada (6/12/24 h, ao abrir), offset de horário, EPG a usar (automático, provedor, URL própria, epgshare01 país), modo do player (Automático guardado pelo diagnóstico), formato preferido (Automático/HLS/TS), user-agent (avançado), PIN.
- **Grade (EPG)**: países do epgshare01 (já existe), dias a guardar (1/3/7), "Atualizar agora", e o relatório de casamento ("812 de 1.204 canais com grade").
- **Reprodução**: resolução principal (já existe), espera, proxy (só LG), "Ir para o canal ao abrir a Live TV" (último canal).
- **Controle parental**: PIN para grupos/canais/fontes (reaproveita a infraestrutura de perfis/PIN se houver).
- **Diagnóstico da Live TV** (já existe): passa a rodar **por fonte** e a gravar a recomendação na fonte, em vez de mexer em ajustes globais.
- Os itens técnicos "Modo do player A/B/C" e "Formato do Xtream" saem da tela principal para "Avançado" da fonte.

### 3.8 Estados vazios e erros

| Situação | Hoje | Alvo |
|---|---|---|
| Nenhuma fonte | mensagem + painel de addons recomendados | Assistente: "Adicionar addon de canais · Xtream · M3U · Stalker" |
| Fonte fora do ar | lista do cache fica (bom), aviso no cabeçalho | Chip vermelho + "Fora do ar desde 21:10 · mostrando a lista de ontem · Tentar de novo" |
| Lista cortada | log "lista cortada" | Não existe mais corte silencioso: aviso "Mostrando 3.000 de 12.400 canais: escolha os grupos" e seleção de grupos na primeira vez |
| Sem grade no canal | "AO VIVO" | "Sem programação · Escolher grade" (atalho para o casamento manual) |
| Conta Xtream vencida/telas cheias | cartão de erro com motivo (bom) | Igual + selo no chip da fonte antes de tentar tocar |

---

## 4. Arquitetura de confiabilidade

### 4.1 Modelo de dados por fonte

```
LtvFonte  { id estável (hash da base/servidor), tipo (addon|xtream|stalker|m3u),
            nome, estado (ok|degradada|fora|desligada), ultimaLista, ultimaGrade,
            erroUltimo, ajustes (offset, formato, modo player, ua, pin, refresh) }
LtvGrupo  { fonte, idOriginal, nome, ordemOriginal, oculto, ordemUsuario }
LtvCanal  { fonte, idOriginal, nome, logo, grupo, numero (chno|num|posição),
            epgId (tvg-id/epg_channel_id), arquivoDias, variantes[], base addon }
```

- **Armazenamento compacto**: strings numa arena por fonte (não `char[600]` fixos por canal). Um canal médio cabe em ~200–300 B. 10 mil canais ≈ 3 MB; com teto por plataforma (4.2).
- **Cache por fonte** (`ltv-<fonte>-p<perfil>.bin`, mesma disciplina de versão/tamanho do `guia-cache.bin` atual): uma fonte que falha não invalida as outras; cada uma tem seu "atualizado há".
- **Ids estáveis** `fonte:idOriginal` para favoritos, listas, lembretes e progresso; a URL com credencial continua fora (regra atual de `xtream.h`).
- Separar `guia.c` em módulos: `ltv_fontes.c` (registro, saúde, cache), `ltv_lista.c` (carga por tipo), `ltv_org.c` (ocultos, ordem, listas, números), `epg_*` (4.3), e `guia.c` só desenho e foco. Hoje são 4.319 linhas num arquivo.

### 4.2 Tetos por plataforma (memória limitada, nunca corte silencioso)

| Plataforma | Canais retidos | EPG retido | Observação |
|---|---|---|---|
| webOS (C9/C4, 1,5–3 GB) | 30.000 | 400 mil programas / 24 MB arena | |
| Tizen .tpk (heap app ~256 MiB citado no código) | 20.000 | 300 mil / 16 MB | medir no aparelho |
| Tizen .wgt (emscripten) | 8.000 | 150 mil / 8 MB | heap WASM |
| Android TV (1–2 GB) | 30.000 | 400 mil / 24 MB | |
| TV de 2016/1 GB (`perfiltv`) | 5.000 | 100 mil / 6 MB | escolhido por `perfiltv.c` |

Quando a fonte tem mais do que cabe: **o usuário escolhe os grupos** (padrão TiviMate "selecionar grupos ao importar"); o app nunca descarta categoria em silêncio. Com ~250 B por canal na arena, 30 mil canais ≈ 7,5 MB (contra ~6,5 MB fixos hoje para 900). Os números são proposta inicial; o MVP inclui uma medição de memória por fonte no log (`[ltv] fonte X: N canais, M KB`).

### 4.3 EPG: busca, parse, cache, casamento

1. **Fontes de grade por fonte de canal**, em ordem de prioridade (primeira que tiver programa vence, como no TiviMate):
   - Xtream: `xmltv.php` do provedor → `get_short_epg` sob demanda (xtepg, já existe) para canal visível sem grade.
   - M3U: `url-tvg`/`x-tvg-url` do cabeçalho → XMLTV por URL do usuário.
   - Addon Stremio: **`meta.videos` com `startTime/endTime` e `behaviorHints.epgProvider`** (padrão do SDK de EPG do Stremio) → epgshare01 por país.
   - Global: epgshare01 por país (atual) e XMLTV por URL.
2. **Parse em stream**: ler o gzip em blocos (zlib já está no projeto) e alimentar um parser incremental (expat, ou o tokenizador atual adaptado a blocos com estado). Durante a leitura: (a) os `<channel>` montam o índice; (b) cada `<programme>` só é retido se o canal estiver no conjunto de interesse (ids e nomes normalizados da lista) e dentro da janela [−passado da fonte, +N dias]. Assim um XMLTV de 500 MB passa com memória de pico de poucos MB. Teto de tempo (por exemplo 5 min) e de bytes lidos (300 MB) para não prender o fio.
3. **Cache compacto**: o resultado (eventos + arena de títulos e descrições curtas) vai para `epg-<fonte>.bin`, versionado. Abrir o app lê o binário em milissegundos, sem reabrir XML. O .gz bruto deixa de ser guardado.
4. **Atualização em fundo**: agendada por fonte (padrão 12 h; mínimo 2 h para não ser bloqueado; "ao abrir" opcional), também com o guia fechado (hoje só roda enquanto o guia/overlay está vivo). `ETag`/`Last-Modified` quando o servidor manda. Falha mantém a grade anterior e marca "degradada".
5. **Casamento em cascata, guardado e explicável**:
   1. `epgId` exato (`tvg-id`, `epg_channel_id`), sem caixa (opção do Kodi);
   2. `tvg-name`/nome exato normalizado;
   3. nome sem prefixo de país/pacote (`epg_sem_prefixo` já existe) e sem marcas de resolução (`nv_nome_base` já existe);
   4. apelidos (tabela atual) + regras de afiliada (atuais);
   5. fuzzy com limiar (tokens em comum, distância pequena), **só sugerido**, nunca aplicado se ambíguo;
   6. **manual**: escolha do usuário, guardada por `fonte:canal` e sincronizada.
   O resultado fica num mapa persistido (`epgmap-<fonte>.bin`) com o método que casou, para o relatório "812 de 1.204 com grade (640 por id, 150 por nome, 22 manual)".
6. **Fuso**: tudo em epoch UTC (já é assim no `xmltvTempo`). Offset por fonte e por canal aplicado na leitura. Hora exibida pelo fuso da TV (respeitando o ajuste 12/24 h da #263). Xtream: guardar `server_info.timezone`/`timestamp_now` para calcular o desvio do painel (usado no catch-up, que espera hora local do painel).
7. **Grade curta Xtream (xtepg)**: manter, com prioridade para o canal em foco e o canal no ar, respeitar `max_connections` (nunca pedir em paralelo com o vídeo numa conta de 1 tela; #158 mostrou que uma segunda conexão derruba o fluxo).

### 4.4 Saúde por fonte e diagnóstico

- Estado por fonte calculado de sinais que já existem: lista (HTTP, página HTML, auth, tempo), conta Xtream (`status`, `exp_date`, `active_cons/max_connections`), grade (idade, % de canais com programa, cobertura em dias), reprodução (taxa de canais que tocaram nas últimas N tentativas, tempo até o primeiro quadro, formato que funcionou).
- Visível no chip da fonte e numa tela "Saúde das fontes" (evolução do `livetvdiag`): cada linha com causa provável e ação ("Renovar assinatura", "Trocar para TS", "Grade velha · Atualizar").
- Diagnóstico por fonte grava `formato`, `modo do player`, `espera` e `resolução` **na fonte** (hoje vão para ajustes globais e valem para todas as fontes).
- Log: uma linha resumo por fonte por sessão (`[ltv] fonte=<hash> tipo=xtream canais=… grupos=… grade=…% idade=…h tocou=…/… formato=…`), sem URL nem credencial. Isso torna a medição dos próximos planos uma consulta só.

### 4.5 Prazos e tentativas

| Operação | Hoje | Proposta |
|---|---|---|
| Página de catálogo de canal (addon) | 15 s | 10 s conexão / 20 s leitura, 1 nova tentativa com recuo; páginas em paralelo entre fontes (não dentro da mesma) |
| Manifesto | 15 s (lido da descoberta quando há) | igual |
| Lista Xtream | 20 s login / 60 s lista | igual; com `Retry-After` respeitado (já há para 429) |
| XMLTV | 60 s, inteiro | stream com prazo de inatividade 20 s e teto total 5 min |
| `get_short_epg` | série, 400 ms–3 s entre pedidos | igual, mais pausa total enquanto o vídeo toca em conta de 1 tela |
| Abrir canal | watchdog por fonte (15 s Xtream; 25/45 s opção) | prazo aprendido por fonte (mediana do primeiro quadro × 2, entre 8 e 30 s) |

---

## 5. Confiabilidade da reprodução ao vivo

- **Ordem de tentativa por canal**, guardada por fonte e por canal: variante preferida (resolução) × formato que funcionou por último × proxy (LG). Hoje a ordem é recalculada por sessão (`xtream_formato_funcionou` vale só na sessão): **persistir por fonte**.
- **webOS**: proxy TS local como padrão para Xtream TS (já é a correção de #158); manter o modo do player (A/B/C) como escolha automática por fonte; um único fluxo por conta (nada de sondas paralelas enquanto toca).
- **Tizen .tpk / .wgt**: AVPlay com buffer mínimo 4 s (doc Samsung); confirmar TS ao vivo no .tpk (marcado como não verificado na pesquisa); `setTimeoutForBuffering` alinhado ao prazo aprendido.
- **Android (Media3)**: TS por `ProgressiveMediaSource`/`DefaultMediaSourceFactory` com tipo forçado quando a extensão mente (o endpoint `.ts` que devolve playlist, registro 14565, já foi visto); tratar `ERROR_CODE_BEHIND_LIVE_WINDOW` com `seekToDefaultPosition()` + `prepare()` (hoje o código não trata).
- **Fallback**: fonte morta → próxima variante/formato (já existe) → próximo canal do mesmo grupo **só se o usuário pedir** (botão no cartão) → mensagem com causa.
- **Zapping**: debounce de 600 ms hoje; reduzir para ~300 ms quando a tecla é única (o acúmulo continua para toques múltiplos), mostrar o último quadro/logo do canal enquanto abre, e pré-resolver a URL (sem abrir conexão) dos vizinhos ± 1 para Xtream e M3U, que não custam rede. Pré-abrir fluxo vizinho fica fora por causa do limite de conexões.
- **Catch-up / timeshift (v2)**: Xtream `tv_archive=1` + `tv_archive_duration` → URL `/timeshift/U/P/<min>/<YYYY-MM-DD:HH-MM>/<id>.ts` (hora do painel); M3U `catchup`, `catchup-source`, `catchup-days` (modos default/append/shift/flussonic/xc). Na grade: programas passados com ícone de "arquivo", OK = assistir desde o início.
- **Gravação**: fora. Exige armazenamento gravável grande e processo em fundo que as TVs não garantem; nenhum ganho proporcional.

---

## 6. Faseamento

Estimativas em dias de agente (um agente Sonnet focado, com testes de unidade no Mac e uma captura por plataforma); somam revisão e build, não teste longo em TV (preferência do dono: só testar o que mudou).

### MVP — "confiável e separado por fonte" (≈ 22–26 dias)

| # | Entrega | Dias |
|---|---|---|
| M1 | Modelo por fonte (`LtvFonte/Grupo/Canal`), arena de strings, cache por fonte, ids estáveis com migração de favoritos/lembretes | 4 |
| M2 | Fim do corte silencioso: tetos por plataforma, seleção de grupos quando passa do teto, categorias por fonte na ordem do provedor (alfabético opcional) | 2 |
| M3 | Barra de fontes (chips com saúde) + "Todas" com selo de fonte + filtro guardado | 3 |
| M4 | Parse XMLTV em stream com filtro por canais da lista + cache binário de EPG por fonte (fim do teto de 32 MB) | 4 |
| M5 | Casamento em cascata persistido + relatório de cobertura + "Escolher grade deste canal" | 2 |
| M6 | Offset de horário por fonte + linha `[ltv]` de resumo por fonte no log | 1 |
| M7 | Teclas sem cor: Baixo abre o mini guia (Azul continua), segurar OK abre opções, "0" vira último canal só na Live TV e a proporção da Live TV vai para o OSD | 2 |
| M8 | Diagnóstico escolhe sozinho até 6 canais da fonte (metade das execuções hoje não testa nada) e grava formato/modo na fonte; Android trata `BEHIND_LIVE_WINDOW` (erro 1002 visto nos logs) | 2 |
| M9 | Fontes M3U por URL (parser `#EXTINF`: `tvg-id`, `tvg-name`, `tvg-logo`, `group-title`, `tvg-chno`, `url-tvg`/`x-tvg-url`, `catchup*` guardado para a v2; cabeçalhos `\|user-agent=`) e XMLTV por URL, por fonte, no mesmo pipeline em stream; cadastro em Ajustes ▸ Live TV ▸ Fontes; credencial fora de log | 4 |

Critério de pronto: uma M3U de 20 mil linhas com o seu `url-tvg` carrega com grade; Xtream de 16 mil canais (a mediana medida)/200 grupos carrega sem perder grupo; XMLTV de 100 MB+ entra numa C9 sem fechar o app; cada fonte mostra saúde e "atualizado há"; mudar de fonte não recarrega as outras.

### v1.5 — organização e praticidade (≈ 11–14 dias)

Ocultar/renomear/reordenar grupos e canais; listas próprias com sync; digitar número (regras da decisão 7); variantes FHD/HD/SD numa linha; busca com "No ar agora / Mais tarde"; atualização em fundo programada; foco por programa e navegação por dias na grade.

### v2 — profissional completo (≈ 15–20 dias)

Catch-up (Xtream e M3U, só onde o provedor oferece) com "Assistir desde o início"; EPG dos addons Stremio (`meta.videos`/`epgProvider`); "Agora" como entrada na home; PIN parental por fonte/grupo/canal; tela "Saúde das fontes"; offset por canal; fuzzy sugerido; prazo de abertura aprendido; zapping com pré-resolução de vizinhos e último quadro.

---

## 7. Riscos

1. **Migração de dados** (favoritos, lembretes, cache) dos ids atuais para `fonte:id`. Mitigação: tabela de tradução na primeira carga e manter leitura do formato antigo por duas versões.
2. **Memória nas TVs antigas**: o parser em stream reduz o pico, mas a lista grande cresce. Mitigação: tetos de 4.2 via `perfiltv`, medição no log antes de subir os tetos.
3. **Limite de conexões do provedor**: qualquer sonda, pré-carga ou grade curta paralela ao vídeo derruba o fluxo (#158). Mitigação: um "orçamento de conexões" por fonte Xtream (`max_connections` − 1 para o vídeo).
4. **Fuso e catch-up**: hora do painel diferente da TV; erro de 1 h no horário de verão. Mitigação: usar `timestamp_now` do servidor para calcular o desvio e teste com painel em fuso diferente.
5. **Escopo de UI**: barra de fontes + grupos + grade por programa mexem no foco inteiro do guia. Mitigação: entregar o modelo (M1/M2) antes da tela, com o guia atual lendo do modelo novo.
6. **Teclas por plataforma**: Tizen não repassa Amarelo/CH−; controles sem cores (#267). Mitigação: nenhuma ação exclusiva de tecla colorida (princípio 2).
7. **Legal/loja**: M3U por URL deixa o app mais parecido com "player de IPTV". O app não fornece listas; textos e loja devem dizer isso (mesma postura de hoje com Xtream/Stalker).
8. **Dependência do epgshare01**: fonte gratuita pode mudar ou cair. Mitigação: XMLTV por URL configurável e cache que sobrevive a falhas.

---

## 8. Decisões do dono (06/10/2026)

As perguntas 1, 2 e 4 foram respondidas pelo dono. Nas demais ele disse "faça o melhor", e o que está abaixo é a escolha tomada.

1. **O guia abre em "Todas"**: todas as fontes, com o selo da fonte em cada canal. O último filtro de fonte usado fica guardado só durante a sessão, não entre aberturas do app.
2. **Grupos de mesmo nome vindos de fontes diferentes ficam SEPARADOS.** Em "Todas" aparecem como "Esportes · <fonte>". A opção de fundir grupos sai do plano.
3. **Ordem dos grupos**: a do provedor por padrão, com uma opção "A–Z" em cada fonte.
4. **M3U e XMLTV por URL entram no MVP** (item M9).
5. **Baixo também abre o mini guia**, e o Azul continua funcionando. **Só na Live TV**, o "0" vira "último canal" (recall), e a proporção da Live TV passa para um botão do OSD. No VOD o "0" continua trocando a proporção.
6. **Catch-up (`tv_archive`) na v2**, só onde o provedor oferece.
7. **Numeração**:
   - Cada fonte mantém os números do provedor (`tvg-chno`, `num` do Xtream ou a posição).
   - Com uma fonte filtrada, digitar um número leva ao canal daquela fonte.
   - Em "Todas", o número é procurado primeiro nos Favoritos e depois nas fontes, na ordem da barra.
   - Quando o mesmo número existe em mais de uma fonte, a tela mostra "fonte · número".
   - A numeração global por faixas, proposta antes, sai do plano.
