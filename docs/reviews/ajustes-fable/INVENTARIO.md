# Inventário de Ajustes (gerado do código, integracao/2.1 @ 0bbce98ed)

Fonte: `src/ajustes.c` (enum `OpcaoId`, tabela `OPCOES[]`) e o catálogo de apresentação `src/ajustes_ux_tela.inc` (`TELA[]`). Regenerar com `python3 docs/reviews/ajustes-fable/mockup/inventario.py` quando o enum mudar.

- Opções no enum: **426**
- No catálogo TELA (o que a tela mostra): **401** em 11 seções
- Fora do catálogo (vivem dentro de editores próprios): listadas no fim
- `*` = só em algumas plataformas (`#ifdef` no catálogo); **[técnica]** = fica atrás de "Mais opções"

Checklist: cada linha abaixo tem de continuar alcançável na nova tela, com explicação e com o mesmo efeito. Marcar na revisão final.

## Reprodução (87) — Qualidade, som e o que o player faz · ícone `aj_circle-play`

### Imagem e som

- [ ] `AJ_QUALIDADE` · Qualidade máxima · escolha
- [ ] `AJ_FONTE_HDR` · HDR e Dolby Vision · escolha
- [ ] `AJ_DV` · Dolby Vision · escolha
- [ ] `AJ_ATMOS` · Dolby Atmos · escolha
- [ ] `AJ_AUDIO_OFFSET` · Ajuste do áudio (ms) · número *
- [ ] `AJ_AUDIO_NORMALIZAR` · Normalizar volume · escolha

### Escolha da fonte

- [ ] `AJ_FONTE_MANUAL` · Escolher a fonte ao reproduzir · escolha
- [ ] `AJ_FONTE_PRIORIDADE` · A escolha automática prioriza · escolha
- [ ] `AJ_FONTE_IDIOMA_AUDIO` · Preferir o idioma do áudio · escolha
- [ ] `AJ_FONTE_PRAZO` · Espera pelos add-ons · escolha
- [ ] `AJ_FONTE_REPOR` · Outra fonte se falhar · escolha
- [ ] `AJ_TAM_MAX` · Tamanho máximo · escolha
- [ ] `AJ_FONTE_GRUPO` · Preferir o mesmo grupo de fonte · escolha
- [ ] `AJ_FONTE_GRUPO_SESSOES` · Reusar o grupo entre sessões · escolha
- [ ] `AJ_LINK_REUSAR` · Reusar o último link · escolha
- [ ] `AJ_LINK_PRAZO` · Validade do último link · escolha

### Durante e depois

- [ ] `AJ_PAUSA_OVERLAY` · Painel ao pausar · escolha
- [ ] `AJ_PAUSA_COMENTARIOS` · Comentários do Trakt ao pausar · escolha
- [ ] `AJ_PAUSA_NOTAS` · Notas ao pausar · escolha
- [ ] `AJ_PAUSA_ELENCO` · Elenco ao pausar · escolha
- [ ] `AJ_TELA_ABERTURA` · Tela de abertura do vídeo · escolha
- [ ] `AJ_ABERTURA_ADDON` · Mostrar addon na abertura · escolha
- [ ] `AJ_ABERTURA_FUNDO` · Fundo da tela de abertura · escolha
- [ ] `AJ_ABERTURA_ESTILO` · Estilo da tela de abertura · escolha
- [ ] `AJ_ESTAT_TAMANHO` · Tamanho das estatísticas ao vivo · escolha
- [ ] `AJ_ESTAT_LADO` · Lado das estatísticas · escolha
- [ ] `AJ_PLR_CLASSIF` · Classificação no player · escolha
- [ ] `AJ_PULAR_ABERTURA` · Pular a abertura automaticamente · escolha
- [ ] `AJ_PULAR_RESUMO` · Pular o resumo automaticamente · escolha
- [ ] `AJ_PULAR_CREDITOS` · Pular os créditos automaticamente · escolha
- [ ] `AJ_PROXIMO_AUTO` · Tocar o próximo episódio automaticamente · escolha
- [ ] `AJ_PROXIMO_CONTAGEM` · Contagem do próximo episódio · número
- [ ] `AJ_REACAO_CREDITOS` · Perguntar o que achou nos créditos · escolha
- [ ] `AJ_CW_CONCLUIDO` · Percentual assistido · número
- [ ] `AJ_CW_MINIMO` · Mínimo para entrar no Continuar · número
- [ ] `AJ_PLR_FORMATO` · Formato no player · escolha
- [ ] `AJ_PLR_FORMATO_POS` · Posição do formato no player · escolha
- [ ] `AJ_PLR_CODEC` · Mostrar o codec do áudio · escolha
- [ ] `AJ_SAIDA_PLAYER` · Ao sair do player · escolha
- [ ] `AJ_PROPORCAO_PADRAO` · Proporção padrão · escolha
- [ ] `AJ_BRILHO_PLAYER` · Brilho da interface no player · escolha
- [ ] `AJ_HDR_UI` · Brilho da interface em HDR · escolha

### Miniatura ao avançar

- [ ] `AJ_SEEKR_LIGADO` · Miniaturas na barra de tempo · escolha
- [ ] `AJ_SEEKR_CHAVE` · Chave do Seekr · ação
- [ ] `AJ_SEEKR_TESTAR` · Testar chave do Seekr · ação
- [ ] `AJ_SEEKR_FITA` · Fita de miniaturas · escolha

### Escolha da fonte [técnica]

- [ ] `AJ_FONTE_REGEX` · Coincidência por regex · escolha
- [ ] `AJ_FONTE_REGEX_PADRAO` · Padrão da regex · ação
- [ ] `AJ_FONTE_REGEX_MODELO` · Modelo de regex · escolha
- [ ] `AJ_FONTE_ESCOPO` · Fontes do automático · escolha
- [ ] `AJ_FONTE_ADDONS_PERM` · Add-ons permitidos · ação
- [ ] `AJ_FONTE_PLUGINS_PERM` · Plugins permitidos · ação
- [ ] `AJ_FONTE_ORDEM` · Ordem dos add-ons · ação
- [ ] `AJ_FONTE_ORDEM_USO` · Usar a ordem · escolha
- [ ] `AJ_FONTE_OUTROS` · Usar os outros se não houver · escolha
- [ ] `AJ_FONTE_AUTO` · Fonte automática · escolha
- [ ] `AJ_FONTE_TOCAR_CONFERINDO` · Tocar enquanto confere a fonte · escolha
- [ ] `AJ_FONTE_AQUECER` · Aquecer conexões ao abrir o título · escolha
- [ ] `AJ_FONTE_CONFERIR_VARIAS` · Conferir várias fontes ao mesmo tempo · escolha
- [ ] `AJ_FONTE_PREPARAR` · Preparar a fonte ao abrir o título · escolha
- [ ] `AJ_FONTE_PRECARREGAR` · Pré-carregar vídeo · escolha *
- [ ] `AJ_TAM_MIN` · Tamanho mínimo · escolha
- [ ] `AJ_FONTE_TEXTO` · Texto das fontes · escolha
- [ ] `AJ_FONTE_ORDEM_ADDON` · Ordem das fontes · escolha
- [ ] `AJ_SELOS_CORES` · Selos coloridos · escolha
- [ ] `AJ_SELOS_PACOTE` · Pacote de selos · escolha
- [ ] `AJ_SELOS_PACOTE_ADD` · Adicionar pacote de selos · ação
- [ ] `AJ_SELOS_PACOTE_REM` · Remover pacote · ação

### Som e vídeo desta TV [técnica]

- [ ] `AJ_DTS_AC3` · DTS convertido em · escolha
- [ ] `AJ_HDR10P_STRIP` · Tirar HDR10+ do vídeo · escolha *
- [ ] `AJ_AFR` · Taxa de atualização da tela · escolha *
- [ ] `AJ_TUNNELING` · Reprodução em túnel · escolha *
- [ ] `AJ_DV_MKV` · Dolby Vision em MKV (experimental) · escolha *
- [ ] `AJ_DV7` · Dolby Vision P7 para P8.1 (experimental) · escolha *
- [ ] `AJ_CACHE_SEEK` · Cache de seek em disco · escolha
- [ ] `AJ_MANTER_VIDEO` · Manter o vídeo pronto ao sair · escolha
- [ ] `AJ_RETOMADA_MORNA` · Retomada rápida ao voltar · escolha
- [ ] `AJ_MANTER_PRAZO` · Tempo com o vídeo pronto · escolha
- [ ] `AJ_SEEKR_AJUSTE` · Sincronia da miniatura · número
- [ ] `AJ_BUFFER` · Tamanho do buffer · escolha *
- [ ] `AJ_PLAYER_EXTERNO` · Player · escolha *
- [ ] `AJ_PLAYER_ANIME` · Player para anime · escolha *

### P2P [técnica]

- [ ] `AJ_P2P_LIGADO` · Servidor P2P (experimental) · escolha *
- [ ] `AJ_P2P_URL` · Endereço do servidor P2P · ação *
- [ ] `AJ_P2P_TESTAR` · Testar servidor P2P · ação *
- [ ] `AJ_P2P_LIMITE` · Limite de espaço do P2P · escolha *
- [ ] `AJ_P2P_DESTINO` · Cache do P2P · escolha *

## Idiomas e legendas (35) — Idioma da interface, áudio, legenda e metadados · ícone `aj_languages`

### Idiomas

- [ ] `AJ_IDIOMA` · Idioma do app · escolha
- [ ] `AJ_AUD_LINGUA` · Idioma do áudio · escolha
- [ ] `AJ_AUD_ORIGINAL` · Preferir faixa original · escolha · **sem página Pencil hoje (cai em Outros ajustes)**
- [ ] `AJ_AUD_LINGUA2` · Segundo idioma do áudio · escolha
- [ ] `AJ_LEG_LINGUA` · Idioma da legenda · escolha
- [ ] `AJ_LEG_FORCADA` · Legenda forçada automática quando o áudio for no seu idioma · escolha
- [ ] `AJ_LEG_MOSTRAR` · Mostrar no player · escolha
- [ ] `AJ_LEG_SYNC_AUTO` · Sincronia automática da legenda · escolha
- [ ] `AJ_LEG_SYNC_AUDIO` · Sincronia por áudio · escolha *

### Legenda principal

- [ ] `AJ_LEG1_TAMANHO` · Tamanho · número
- [ ] `AJ_LEG1_FONTE` · Fonte da legenda · escolha
- [ ] `AJ_LEG1_COR` · Cor · escolha
- [ ] `AJ_LEG1_OPACIDADE` · Opacidade · escolha
- [ ] `AJ_LEG1_FUNDO` · Fundo · escolha
- [ ] `AJ_LEG1_POSICAO` · Posição · escolha
- [ ] `AJ_LEG1_BORDA` · Borda · escolha
- [ ] `AJ_LEG1_COR_BORDA` · Cor da borda · escolha
- [ ] `AJ_LEG1_NEGRITO` · Negrito · escolha
- [ ] `AJ_LEG1_ATRASO` · Atraso · número
- [ ] `AJ_LEG1_RESTAURAR` · Restaurar padrão · ação

### Segunda legenda

- [ ] `AJ_LEG_LINGUA2` · Idioma da legenda secundária · escolha
- [ ] `AJ_LEG2_TAMANHO` · Tamanho · escolha
- [ ] `AJ_LEG2_FONTE` · Fonte da legenda · escolha
- [ ] `AJ_LEG2_COR` · Cor · escolha
- [ ] `AJ_LEG2_OPACIDADE` · Opacidade · escolha
- [ ] `AJ_LEG2_FUNDO` · Fundo · escolha
- [ ] `AJ_LEG2_POSICAO` · Posição · escolha
- [ ] `AJ_LEG2_BORDA` · Borda · escolha
- [ ] `AJ_LEG2_COR_BORDA` · Cor da borda · escolha
- [ ] `AJ_LEG2_NEGRITO` · Negrito · escolha
- [ ] `AJ_LEG2_ATRASO` · Atraso · número
- [ ] `AJ_LEG2_RESTAURAR` · Restaurar padrão · ação
- [ ] `AJ_LEG2_POS` · Posição da segunda legenda · escolha
- [ ] `AJ_LEG_CONTORNO_COR` · Cor do contorno da legenda · escolha

### Idiomas [técnica]

- [ ] `AJ_TMDB_IDIOMA` · Idioma dos metadados · escolha

## Fontes e addons (34) — Addons, debrid e servidores pessoais · ícone `aj_puzzle`

### Addons

- [ ] `AJ_ADDONS` · Addons · ação
- [ ] `AJ_ADDONS_LIGADO` · Addons · escolha · **sem página Pencil hoje (cai em Outros ajustes)**
- [ ] `AJ_DEBRID_LIGADO` · Debrid · escolha · **sem página Pencil hoje (cai em Outros ajustes)**
- [ ] `AJ_LEG_ESTILO_LIGADO` · Legendas com estilo · escolha · **sem página Pencil hoje (cai em Outros ajustes)**
- [ ] `AJ_PLUGINS` · Plugins · ação
- [ ] `AJ_ADDONS_PRINCIPAL` · Usar os addons do perfil principal · escolha
- [ ] `AJ_BUSCA_CINEMETA` · Buscar no Cinemeta · escolha
- [ ] `AJ_BUSCA_NUVIO` · Resultados do catálogo do Nuvio na busca · escolha
- [ ] `AJ_BUSCA_ORIGEM` · Mostrar a fonte nos resultados da busca · escolha

### Falha na reprodução

- [ ] `AJ_FILTROS_FONTES` · Filtros das fontes · ação
- [ ] `AJ_FALHA_PROVEDORES` · Trocar provedor se falhar · escolha
- [ ] `AJ_FALHA_PROVEDOR_ORDEM` · Ordem e limites dos provedores · ação
- [ ] `AJ_FALHA_ADDONS` · Trocar add-on se falhar · escolha
- [ ] `AJ_FALHA_ADDON_ORDEM` · Ordem e limites dos add-ons · ação
- [ ] `AJ_FALHA_ADDON_LIMITE` · Links neste add-on · número

### Debrid

- [ ] `AJ_PROVEDORES_ESCOPO` · Configuração dos provedores · escolha
- [ ] `AJ_DEBRID_RD` · Chave do Real-Debrid · ação
- [ ] `AJ_DEBRID_TB` · Chave do TorBox · ação
- [ ] `AJ_DEBRID_PM` · Chave do Premiumize · ação
- [ ] `AJ_DEBRID_AD` · Chave do AllDebrid · ação
- [ ] `AJ_DEBRID_AD_TESTAR` · Testar chave do AllDebrid · ação
- [ ] `AJ_DEBRID_RD_TESTAR` · Testar chave do Real-Debrid · ação
- [ ] `AJ_DEBRID_TB_TESTAR` · Testar chave do TorBox · ação
- [ ] `AJ_DEBRID_PM_TESTAR` · Testar chave do Premiumize · ação

### Servidores pessoais

- [ ] `AJ_JF_LIGADO` · Servidores pessoais (experimental) · escolha
- [ ] `AJ_JF_SERVIDOR` · Endereço do Jellyfin · ação
- [ ] `AJ_JF_ENTRAR` · Entrar no Jellyfin · ação
- [ ] `AJ_JF_SAIR` · Sair do Jellyfin · ação
- [ ] `AJ_EM_SERVIDOR` · Endereço do Emby · ação
- [ ] `AJ_EM_ENTRAR` · Entrar no Emby · ação
- [ ] `AJ_EM_SAIR` · Sair do Emby · ação
- [ ] `AJ_PX_ENTRAR` · Entrar no Plex · ação
- [ ] `AJ_PX_SERVIDOR` · Servidor do Plex · ação
- [ ] `AJ_PX_SAIR` · Sair do Plex · ação

## TV ao vivo (45) — Provedor e solução de problemas ao assistir canais · ícone `aj_tv-minimal-play`

### Contas de TV ao vivo

- [ ] `AJ_IPTV_ORDEM` · Ordem · ação
- [ ] `AJ_IPTV_CONTA` · Conta de TV ao vivo · escolha
- [ ] `AJ_IPTV_NOME` · Nome da conta · ação
- [ ] `AJ_IPTV_ATIVA` · Conta ativada · escolha
- [ ] `AJ_IPTV_LISTA_STALKER` · Stalker · escolha
- [ ] `AJ_IPTV_ADD_STALKER` · Adicionar portal Stalker · ação
- [ ] `AJ_IPTV_LISTA_XTREAM` · Xtream · escolha
- [ ] `AJ_IPTV_ADD_XTREAM` · Adicionar conta Xtream · ação
- [ ] `AJ_IPTV_LISTA_M3U` · M3U · escolha
- [ ] `AJ_IPTV_ADD_M3U` · Adicionar lista M3U · ação
- [ ] `AJ_IPTV_REMOVER` · Remover conta de TV ao vivo · ação
- [ ] `AJ_IPTV_ACOES` · Conta de TV ao vivo · escolha
- [ ] `AJ_IPTV_EXCLUIR` · Remover conta de TV ao vivo · escolha
- [ ] `AJ_XTREAM_SERVIDOR` · Servidor Xtream · ação
- [ ] `AJ_XTREAM_USUARIO` · Usuário Xtream · ação
- [ ] `AJ_XTREAM_SENHA` · Senha Xtream · ação
- [ ] `AJ_XTREAM_CONTA` · Conta Xtream · leitura
- [ ] `AJ_XTREAM_CONEXOES` · Conta com limite de conexões · escolha
- [ ] `AJ_XTREAM_LIMPAR` · Remover o Xtream · ação
- [ ] `AJ_STALKER_PORTAL` · Portal Stalker (MAC) · ação
- [ ] `AJ_STALKER_MAC` · MAC do portal · ação
- [ ] `AJ_STALKER_LIMPAR` · Remover o portal Stalker · ação
- [ ] `AJ_M3U_URL` · URL da lista M3U · ação
- [ ] `AJ_M3U_EPG` · URL da programação XMLTV · ação
- [ ] `AJ_M3U_LIMPAR` · Remover lista M3U · ação
- [ ] `AJ_IPTV_SALVAR` · Salvar · ação
- [ ] `AJ_IPTV_CANCELAR` · Cancelar · ação

### Guia de TV

- [ ] `AJ_GUIA_ORDEM` · Ordenação · escolha
- [ ] `AJ_GUIA_FONTE` · Fontes do Guia de TV · escolha
- [ ] `AJ_GUIA_RECARREGAR` · Atualizar canais e programação · ação
- [ ] `AJ_EPG_PAIS` · Grade de programação · escolha
- [ ] `AJ_EPG_MEMORIA` · Memória da programação · escolha
- [ ] `AJ_GUIA_CATEGORIAS` · Ocultar categorias · escolha
- [ ] `AJ_URL_LISTA` · Listas e guias por URL · escolha
- [ ] `AJ_URL_M3U_ADD` · Adicionar lista M3U · ação
- [ ] `AJ_URL_XMLTV_ADD` · Adicionar guia XMLTV · ação
- [ ] `AJ_URL_REM` · Remover a escolhida · ação
- [ ] `AJ_LIVETV_SUGERIR` · Sugerir categorias dos canais · escolha
- [ ] `AJ_GUIA_PREVIA_GRANDE` · Prévia grande no Guia TV · escolha

### Se o canal não abre

- [ ] `AJ_LIVETV_DIAG` · Diagnóstico da Live TV · ação
- [ ] `AJ_LIVETV_RES` · Resolução principal · escolha
- [ ] `AJ_LIVETV_ESPERA` · Espera para abrir o canal · escolha

### Se o canal não abre [técnica]

- [ ] `AJ_LIVETV_FORMATO` · Formato do Xtream · escolha
- [ ] `AJ_LIVETV_MODO` · Modo do player da Live TV · escolha
- [ ] `AJ_LIVETV_PROXY` · Proxy de TS da Live TV · escolha

## Tela inicial (53) — Estilo, fileiras, barra lateral e Continuar assistindo · ícone `aj_layout-dashboard`

### Estilo

- [ ] `AJ_HOME_LAYOUT` · Layout da home · escolha
- [ ] `AJ_LANDSCAPE` · Pôsteres horizontais · escolha
- [ ] `AJ_HERO` · Mostrar destaque · escolha
- [ ] `AJ_HERO_CHEIO` · Fundo em tela cheia · escolha
- [ ] `AJ_FUNDO_DIN` · Fundo da Dinâmica · escolha
- [ ] `AJ_FUNDO_DIN_COR` · Cor do fundo · escolha
- [ ] `AJ_HERO_CATALOGOS` · Catálogos do destaque · ação
- [ ] `AJ_HERO_GENERO` · Mostrar gênero no destaque · escolha
- [ ] `AJ_HERO_SINOPSE` · Mostrar sinopse no destaque · escolha
- [ ] `AJ_HERO_NOTAS` · Mostrar notas no destaque · escolha
- [ ] `AJ_HERO_ANO` · Mostrar ano no destaque · escolha
- [ ] `AJ_HERO_DURACAO` · Mostrar duração no destaque · escolha
- [ ] `AJ_HERO_PASTA_DETALHES` · Mostrar detalhes da pasta no destaque · escolha
- [ ] `AJ_HERO_TAMANHO` · Tamanho do destaque · escolha
- [ ] `AJ_HERO_SINOPSE_TAM` · Tamanho da sinopse · escolha

### Fileiras

- [ ] `AJ_FIL_ORDEM` · Ordenar e ativar fileiras · ação
- [ ] `AJ_FIL_LIMITE` · Limite de fileiras · número
- [ ] `AJ_ITENS_FILEIRA` · Itens por fileira · escolha
- [ ] `AJ_ESPACO_FILEIRAS` · Espaço entre fileiras · número
- [ ] `AJ_ESPACO_TITULOS` · Espaço entre títulos · número
- [ ] `AJ_OCULTAR_NLANC` · Ocultar não lançados · escolha
- [ ] `AJ_NOTAS_HOME` · Avaliações gerais · escolha
- [ ] `AJ_SELO_VISTO` · Selo de assistido nos pôsteres · escolha
- [ ] `AJ_ABAS_ESTILO` · Estilo das abas · escolha
- [ ] `AJ_OCULTAR_FILMES_VISTOS` · Ocultar filmes assistidos · escolha
- [ ] `AJ_BIB_HOME` · Fileira da Biblioteca · escolha
- [ ] `AJ_AMIGOS_ESTILO` · Estilo da fileira de amigos · escolha
- [ ] `AJ_CURADORIA_URL` · Endereço da curadoria · ação

### Coleções

- [ ] `AJ_COLECAO_EXIBICAO` · Exibição padrão das coleções · escolha

### Continuar assistindo

- [ ] `AJ_CW_LIGADO` · Mostrar \ · escolha
- [ ] `AJ_CW_OK` · OK no card · escolha
- [ ] `AJ_CW_ESTILO` · Estilo do \ · escolha
- [ ] `AJ_CW_THUMB` · Miniatura do episódio · escolha
- [ ] `AJ_CW_BLUR_PROX` · Desfocar próximo episódio · escolha
- [ ] `AJ_CW_ORDEM` · Ordenação · escolha
- [ ] `AJ_CW_NAO_EXIBIDOS` · Mostrar episódios não exibidos · escolha
- [ ] `AJ_CW_RETIDO_TAMBEM` · Também em Continuar assistindo · escolha

### Barra lateral

- [ ] `AJ_MENU_TOPO` · Posição do menu · escolha
- [ ] `AJ_RAIL_MODERNA` · Barra lateral moderna · escolha
- [ ] `AJ_RAIL` · Barra lateral · escolha
- [ ] `AJ_MENU_EXPLORAR` · Explorar na barra lateral · escolha
- [ ] `AJ_MENU_GUIA` · Guia TV na barra lateral · escolha
- [ ] `AJ_MENU_AGENDA` · Agenda na barra lateral · escolha
- [ ] `AJ_MENU_PERFIL` · Perfil e Stats na barra lateral · escolha

### Escolha de perfil

- [ ] `AJ_PS_FUNDO` · Fundo da escolha de perfil · escolha
- [ ] `AJ_PS_CONTINUAR` · Continuar na escolha de perfil · escolha

### Estilo [técnica]

- [ ] `AJ_HERO_TRANSICAO` · Transição do destaque · escolha

### Fileiras [técnica]

- [ ] `AJ_ROTULOS` · Rótulos nos pôsteres · escolha
- [ ] `AJ_NOME_ADDON` · Nome do addon no catálogo · escolha
- [ ] `AJ_SUFIXO_TIPO` · Tipo de conteúdo · escolha

### Continuar assistindo [técnica]

- [ ] `AJ_CW_FURTHEST` · Próximo do episódio mais alto · escolha

### Barra lateral [técnica]

- [ ] `AJ_RAIL_BLUR` · Desfoque da barra moderna · escolha
- [ ] `AJ_MIC_BUSCA` · Botão de microfone na busca · escolha *

## Aparência (30) — Cor, transparência, fonte, animações e relógio · ícone `aj_palette`

### (sem grupo)

- [ ] `AJ_INTERFACE_MODO` · Modo da interface · escolha *
- [ ] `AJ_TECLADO_ANDROID` · Teclado · escolha *

### Cores e vidro

- [ ] `AJ_TEMA` · Cor de destaque · escolha
- [ ] `AJ_ARTE_TOM` · Tom da arte · escolha
- [ ] `AJ_FUNDO` · Fundo · escolha
- [ ] `AJ_COR_LOGO` · Cor da logo · escolha
- [ ] `AJ_VIDRO` · Interface de vidro · escolha
- [ ] `AJ_VIDRO_CONTORNO` · Contorno do vidro · escolha
- [ ] `AJ_FOCO_ACENTO` · Foco na cor de destaque · escolha
- [ ] `AJ_SUAVIZAR_DEGRADES` · Suavizar degradês · escolha

### Fileiras

- [ ] `AJ_TITULO_FIL` · Título das fileiras · escolha
- [ ] `AJ_PALCO_FUNDO` · Fundo do Palco · escolha

### Logo e abertura

- [ ] `AJ_LOGO_APP` · Logo do app · escolha
- [ ] `AJ_ABERTURA` · Abertura do app · escolha
- [ ] `AJ_ICONE_APP` · Ícone do app · escolha

### Texto e tamanho

- [ ] `AJ_FONTE_UI` · Fonte da interface · escolha
- [ ] `AJ_TAMANHO_UI` · Tamanho da interface · escolha
- [ ] `AJ_ESCALA_TEXTO` · Escala de texto e componentes · escolha
- [ ] `AJ_DET_ESPACO` · Espaçamento das fileiras · escolha
- [ ] `AJ_TAMANHO_AJUSTES` · Tamanho dos ajustes · escolha
- [ ] `AJ_LAYOUT_AJUSTES` · Layout dos Ajustes · escolha

### Movimento e relógio

- [ ] `AJ_ANIM` · Animações · escolha
- [ ] `AJ_RELOGIO` · Relógio na tela · escolha
- [ ] `AJ_RELOGIO_12H` · Formato do relógio · escolha

### Tela de descanso

- [ ] `AJ_ESMAECER` · Tela de descanso · escolha
- [ ] `AJ_DESCANSO_ESTILO` · Estilo do descanso · escolha
- [ ] `AJ_DESCANSO_FONTE` · Títulos da vitrine · escolha

### Cores e vidro [técnica]

- [ ] `AJ_VIDRO_OPAC` · Opacidade do vidro · escolha
- [ ] `AJ_VIDRO_FOSCO` · Vidro fosco · escolha
- [ ] `AJ_VIDRO_QUALIDADE` · Qualidade do vidro · escolha

## Página do título (36) — O que aparece, notas e dados do título · ícone `aj_file-text`

### O que aparece

- [ ] `AJ_DET_VEU` · Escurecimento do fundo · número
- [ ] `AJ_DET_DATA_CHEIA` · Data de lançamento completa · escolha
- [ ] `AJ_DET_BLUR_NAO_VISTOS` · Sem spoilers · escolha
- [ ] `AJ_EP_TAMANHO` · Tamanho dos cards de episódio · escolha
- [ ] `AJ_DET_TOPO` · Topo da página · escolha
- [ ] `AJ_DET_ELENCO_VISTO` · Onde você já viu o elenco · escolha

### Notas

- [ ] `AJ_NT_IMDB` · IMDb · escolha
- [ ] `AJ_NT_TOMATES` · Rotten Tomatoes (crítica) · escolha
- [ ] `AJ_NT_AUDIENCIA` · Popcornmeter (público) · escolha
- [ ] `AJ_NT_META` · Metacritic (crítica) · escolha
- [ ] `AJ_NT_METAUSER` · Metacritic (usuários) · escolha
- [ ] `AJ_NT_TRAKT` · Trakt · escolha
- [ ] `AJ_NT_TMDB` · TMDB · escolha
- [ ] `AJ_NT_LETTER` · Letterboxd · escolha
- [ ] `AJ_NT_MAL` · MyAnimeList · escolha
- [ ] `AJ_NT_EBERT` · Roger Ebert (crítica) · escolha
- [ ] `AJ_NT_SCORE` · Nota do MDBList · escolha
- [ ] `AJ_NT_ORDEM` · Ordem das notas na linha do título · ação
- [ ] `AJ_NT_EP` · Nota nos episódios · escolha
- [ ] `AJ_MDB_CHAVE` · Chave do MDBList · leitura

### Dados do título

- [ ] `AJ_TMDB_LIGADO` · TMDB · escolha

### Notas [técnica]

- [ ] `AJ_MDB_LIGADO` · MDBList · escolha

### Dados do título [técnica]

- [ ] `AJ_TMDB_ARTE` · Arte localizada · escolha
- [ ] `AJ_TMDB_BASICO` · Título e sinopse · escolha
- [ ] `AJ_TMDB_FICHA` · Ficha técnica · escolha
- [ ] `AJ_TMDB_DATAS` · Datas de lançamento · escolha
- [ ] `AJ_TMDB_ELENCO` · Elenco e equipe · escolha
- [ ] `AJ_TMDB_PROD` · Produtoras · escolha
- [ ] `AJ_TMDB_REDES` · Redes e estúdios · escolha
- [ ] `AJ_TMDB_EPS` · Episódios · escolha
- [ ] `AJ_TMDB_TRAILERS` · Trailers · escolha
- [ ] `AJ_TMDB_MAIS` · \ · escolha
- [ ] `AJ_TMDB_COL` · Coleções e sagas · escolha
- [ ] `AJ_TMDB_CW` · Enriquecer \ · escolha
- [ ] `AJ_DET_META_EXT` · Priorizar metadados externos · escolha
- [ ] `AJ_DET_SO_CINEMETA` · Usar sempre o catálogo do Nuvio · escolha

## Cartazes e trailers (36) — Cartaz em foco, de onde vem a arte e trailers · ícone `aj_clapperboard`

### Cartaz em foco

- [ ] `AJ_EXPANDIR` · Expandir pôster ao focar · escolha
- [ ] `AJ_EXPANDIR_ATRASO` · Atraso da expansão · número
- [ ] `AJ_BORDA_FOCO` · Borda no cartaz em foco · escolha
- [ ] `AJ_MENU_CARTAZ` · Menu de contexto enriquecido · escolha

### Forma

- [ ] `AJ_LARGURA_DP` · Largura do item · número
- [ ] `AJ_RAIO_DP` · Arredondamento · número
- [ ] `AJ_PROF` · Efeito de profundidade · escolha

### De onde vem a arte

- [ ] `AJ_HERO_FUNDO` · Background do hero · escolha
- [ ] `AJ_HERO_ARTE_DIF` · Destaque com outra arte · escolha
- [ ] `AJ_POSTER_PROV` · Pôsteres personalizados · escolha
- [ ] `AJ_FANART_CHAVE` · Chave do fanart.tv · ação

### Trailers

- [ ] `AJ_HERO_TRAILER` · Trailer no destaque · escolha
- [ ] `AJ_FOCO_TRAILER` · Trailer do cartaz em foco · escolha
- [ ] `AJ_DET_TRAILER_AUTO` · Trailer automático · escolha
- [ ] `AJ_DET_TRAILER` · Botão de trailer · escolha

### Som e imagem

- [ ] `AJ_HERO_TRAILER_SOM` · Som do trailer · escolha
- [ ] `AJ_TRAILER_FONTE` · Fonte do trailer · escolha
- [ ] `AJ_TRAILER_AUDIO` · Áudio do trailer · escolha *
- [ ] `AJ_TRAILER_ZOOM_TPK` · Zoom no trailer e no player (experimental) · escolha *
- [ ] `AJ_LOGO_TRAILER` · Esconder logo durante o trailer · escolha

### Cartaz em foco [técnica]

- [ ] `AJ_GRAD_CLASSICO` · Gradiente de foco clássico · escolha
- [ ] `AJ_MURAL_TITULOS` · Títulos no Mural · escolha

### Forma [técnica]

- [ ] `AJ_PROF_BRILHO` · Reflexo · número

### De onde vem a arte [técnica]

- [ ] `AJ_ADDON_POSTER` · Pôsteres do addon · escolha
- [ ] `AJ_ADDON_FUNDO` · Fundo do destaque do addon · escolha
- [ ] `AJ_ADDON_LOGO` · Logo do addon · escolha
- [ ] `AJ_COL_ARTE_CONTA` · Arte das pastas da conta · escolha
- [ ] `AJ_POSTER_INST` · Endereço do SpatialPosters · ação
- [ ] `AJ_POSTER_TOKEN` · Token do SpatialPosters · ação
- [ ] `AJ_POSTER_EXTRA` · Parâmetros do SpatialPosters · ação
- [ ] `AJ_POSTER_CHAVE` · Chave do RPDB · ação
- [ ] `AJ_POSTER_MODELO` · Modelo de URL dos pôsteres · ação
- [ ] `AJ_POSTER_TESTAR` · Testar pôsteres · ação

### Trailers [técnica]

- [ ] `AJ_HERO_TRAILER_ESPERA` · Espera do trailer no destaque · número
- [ ] `AJ_TRAILER_QUAL` · Qualidade do trailer · escolha
- [ ] `AJ_TRAILER_ASPECTO` · Proporção do trailer · escolha

## Conta e serviços (19) — Perfil, sincronização, Trakt, Simkl e avisos · ícone `aj_user-round`

### Perfil

- [ ] `AJ_PERFIL_ATIVO` · Perfil · leitura
- [ ] `AJ_SYNC` · Sincronização (LegaSync) · leitura
- [ ] `AJ_PERFIL_PESQ` · Perfil pesquisável · escolha
- [ ] `AJ_PERFIL_EDITAR` · Meu perfil público · ação
- [ ] `AJ_PERFIL_INFANTIL` · Perfil infantil · escolha
- [ ] `AJ_SAIR` · Sair da conta · ação

### Trakt e Simkl

- [ ] `AJ_TRAKT` · Trakt · ação
- [ ] `AJ_SIMKL` · Simkl · ação
- [ ] `AJ_SALVOS_DEST` · Fonte da biblioteca · escolha
- [ ] `AJ_CW_FONTE` · Fonte do \ · escolha
- [ ] `AJ_CW_PROXIMO` · Próximo episódio no Continuar · escolha
- [ ] `AJ_HIST_CONTA` · Enviar histórico para a conta Nuvio · escolha
- [ ] `AJ_SOCIAL` · Recursos sociais · escolha
- [ ] `AJ_DISCORD` · Discord · ação
- [ ] `AJ_DIARIO_NUVEM` · Guardar diário na conta · escolha

### Notificações

- [ ] `AJ_ENQUETES` · Receber enquetes · escolha

### Conquistas

- [ ] `AJ_CONQ_LIGADO` · Conquistas · escolha
- [ ] `AJ_CONQ_AVISO` · Avisar depois dos créditos · escolha
- [ ] `AJ_CONQ_DICAS` · Dicas das conquistas secretas · escolha

## Esta TV (14) — Resolução, imagens, efeitos e diagnóstico · ícone `aj_monitor-cog`

### (sem grupo)

- [ ] `AJ_RESOLUCAO` · Resolução da interface · escolha
- [ ] `AJ_GPU_EFEITOS` · Efeitos visuais · escolha
- [ ] `AJ_AMPLIAR_UI` · Ampliar interface · escolha
- [ ] `AJ_AMPLIACAO_ALGO` · Algoritmo de ampliação · escolha
- [ ] `AJ_AMPLIACAO_NITIDEZ` · Nitidez · número
- [ ] `AJ_HDR_INTERFACE` · Interface em HDR (teste) · escolha
- [ ] `AJ_QUALIDADE_IMG` · Qualidade da imagem · escolha
- [ ] `AJ_NAV_RAPIDA` · Navegação horizontal rápida · escolha
- [ ] `AJ_MEDIDOR` · Medidor de desempenho · escolha
- [ ] `AJ_DIAGNOSTICO` · Diagnóstico e otimização · ação
- [ ] `AJ_VELOCIDADE` · Teste de velocidade · ação
- [ ] `AJ_ESPACO` · Memória usada por imagens · leitura
- [ ] `AJ_LIMPAR_CACHE` · Limpar cache · ação *

### Memória [técnica]

- [ ] `AJ_TEX_MB` · Memória para imagens · escolha

## Sobre e ajuda (12) — Guia de uso, versão, atualização e registros · ícone `aj_info`

### (sem grupo)

- [ ] `AJ_GUIA` · Guia de uso · ação
- [ ] `AJ_NOVIDADES20` · Novidades 2.0 · ação
- [ ] `AJ_DICASCONTROLE` · Dicas do controle · ação
- [ ] `AJ_APOIAR` · Apoiar o projeto · ação
- [ ] `AJ_AVISO_LEGAL` · Aviso legal · ação
- [ ] `AJ_VERSAO_I` · Versão · leitura
- [ ] `AJ_ATUALIZAR` · Atualizar o aplicativo · ação
- [ ] `AJ_VER_REGISTRO` · Ver o registro na tela · ação
- [ ] `AJ_ENVIAR_LOG` · Enviar registro · ação
- [ ] `AJ_ENVIO_AUTO` · Enviar registros sozinho · escolha
- [ ] `AJ_LOG_REDE` · Registro na rede local · escolha
- [ ] `AJ_BAIXAR_REGISTRO` · Baixar registro · ação *

## Fora do catálogo TELA (dentro de editores/folhas próprias)

- [ ] `AJ_DESCOBRIR` · Local do Descobrir · escolha
- [ ] `AJ_PROF_BORDA` · Brilho da borda · número · editor de Efeito de profundidade
- [ ] `AJ_PROF_COBERTURA` · Cobertura da borda · número · editor de Efeito de profundidade
- [ ] `AJ_PROF_POSTERS` · Profundidade nos pôsteres · escolha · editor de Efeito de profundidade
- [ ] `AJ_PROF_CW` · Profundidade no \ · escolha · editor de Efeito de profundidade
- [ ] `AJ_PROF_EPS` · Profundidade nos episódios · escolha · editor de Efeito de profundidade
- [ ] `AJ_PROF_ELENCO` · Profundidade no elenco · escolha · editor de Efeito de profundidade
- [ ] `AJ_PROF_TRAILERS` · Profundidade nos trailers · escolha · editor de Efeito de profundidade
- [ ] `AJ_MDB_TRAKT` · Notas do Trakt · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_IMDB` · Notas do IMDb · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_TMDB` · Notas do TMDB · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_LETTER` · Notas do Letterboxd · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_TOMATES` · Notas do Rotten Tomatoes · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_AUDIENCIA` · Nota da audiência · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_META` · Notas do Metacritic · escolha · editor das fontes do MDBList
- [ ] `AJ_MDB_MAL` · Notas do MyAnimeList · escolha · editor das fontes do MDBList
- [ ] `AJ_DET_TRAILER_SOM` · Som do trailer na página do título · escolha
- [ ] `AJ_RELOGIO_POS` · Posição do relógio · escolha
- [ ] `AJ_AVANCADAS` · Mostrar opções avançadas · escolha
- [ ] `AJ_FALHA_RD_LIMITE` · Links no Real-Debrid · número · folha Ordem e limites dos provedores
- [ ] `AJ_FALHA_TB_LIMITE` · Links no TorBox · número · folha Ordem e limites dos provedores
- [ ] `AJ_FALHA_PM_LIMITE` · Links no Premiumize · número · folha Ordem e limites dos provedores
- [ ] `AJ_FALHA_AD_LIMITE` · Links no AllDebrid · número · folha Ordem e limites dos provedores
- [ ] `AJ_FILTRO_TAM_MAX` · Tamanho máximo da fonte · número · folha Filtros das fontes
- [ ] `AJ_FILTRO_GRUPOS` · Grupos de lançamento excluídos · ação · folha Filtros das fontes

## Rotas que não são opções (têm de continuar existindo)

- [ ] Busca de ajustes (Spotlight modo Ajustes, `ajustes_buscar`)
- [ ] Predefinições / presets (`ajustes_pen_presets.inc`)
- [ ] Diferente do padrão: revisão e desfazer (`aj4Review`)
- [ ] O que o Nuvio faz? (tour, `ajustes_ux_tour.inc`)
- [ ] Guia de uso (`ajustes_ux_guia.inc`) e Novidades 2.0
- [ ] Dicas do controle
- [ ] Apoiar o projeto (QRs, `ajustes_ux_apoio.inc`)
- [ ] Aviso legal e créditos
- [ ] Perfil: editor, PIN, administrador, bloqueio, perfil infantil, limites (`ajustes_ux_perfil.inc`, `ajustes_pen_parental.inc`)
- [ ] Controle parental: regras, filmes permitidos, tempo de tela, classificação máxima
- [ ] Filtros das fontes (folha `desenhaFolhaFiltros`)
- [ ] Ordem e limites dos provedores / add-ons
- [ ] Fileiras: ordenar e ativar, catálogos do destaque (`fileiras.c`)
- [ ] Contas de TV ao vivo: formulários Xtream/Stalker/M3U/URL (`iptvForm`)
- [ ] Servidores pessoais: Jellyfin/Emby/Plex entrar e sair
- [ ] Debrid: chaves RD/TB/PM/AD e teste
- [ ] Seekr: chave e teste
- [ ] Pôsteres personalizados: provedor, instância, token, chave, modelo, teste
- [ ] Diagnóstico, teste de velocidade, registro na tela, enviar registro, limpar cache
- [ ] Assistir Juntos (abre a folha do `juntosui`)
- [ ] Sincronização (`ajustes_sync.inc`) e dispositivo (`ajustes_device.inc`)
- [ ] Atualizar o aplicativo
- [ ] Toggles novos da branch `agente/21-21-fonteincompat` (esconder fontes incompatíveis: DV na Samsung, áudio sem suporte) — ainda sem commits na branch em 27/10; reconciliar no merge
