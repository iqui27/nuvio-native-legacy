# Mapa vivo das issues

Base: `21077ab5` (integracao/2.0.3.1, que sai como 2.0.4; a 2.0.3 está na tag v2.0.3, 8ed4517c). Atualizado em 2026-10-09. 379 issues (abertas e fechadas) de iqui27/nuvio-native-legacy.

## Como atualizar

1. Chegou issue nova, ou um conserto entrou numa branch/tag: edite **uma** entrada em `docs/issues/mapa.json` (procure por `"numero": N`) e rode `python3 docs/issues/mapa.py`, que valida e reescreve este arquivo. Sem o script, edite a linha equivalente aqui.
2. Campos: `numero`, `titulo`, `plataforma` (LG, Samsung .tpk, Samsung .wgt, Android, all, `?`), `tipo` (bug, feature, question, meta), `status`, `release` (2.0.2 ou tag antiga, 2.0.3 = integracao/2.0.3, `2.0.4` = hotfix em integracao/2.0.3.1, `2.0.5 (branch)`, 2.1/2.2/futuro, `-`), `conserto` (hashes curtos com a ref entre parênteses), `ultima_resposta` (`nos` = o último comentário é nosso, `data`, `ultimo_comentario_por`, `ultima_nossa`), `proximo_passo`, `notas`, `alvo` (só issues ABERTAS, obrigatório: `2.0.3`, `2.0.4`, `2.0.5`, `2.1`, `2.2`, `futuro`, `nao vamos fazer`, ou `ja-lancada` quando já saiu e só falta fechar). `python3 docs/issues/mapa.py --check` valida e confere se o MAPA.md está em dia, sem escrever; o gerador sai com erro se uma issue aberta não tiver alvo. O roadmap e as decisões ficam em `roadmap` e `decisoes` no json; decisão tem `id` (OBRIGATÓRIO e estável, `dec-<assunto>` em minúsculas/dígitos/hifens: é o que liga a resposta do dono no painel à decisão; nunca mude nem reutilize, e o id que sai de vez vai para `decisoes_ids_retirados`), `estado` (`pendente` ou `decidida`) e, decidida, `decisao`, `data` e `quem`. Campo opcional `release` diz a versão afetada quando ela não dá para deduzir do alvo das issues ligadas. `aplicada_em` (AAAA-MM-DD) é preenchido pela coordenação depois de aplicar a resposta; a decisão sai da fila de pendentes do painel.
3. Vocabulário de `status`: `aberta`, `respondida`, `consertada-nao-lancada`, `lancada`, `por-desenho`, `fora-do-escopo`, `precisa-log`, `duplicada`. Extra: `fechada-sem-resposta` (issue fechada sem nenhum comentário).
4. Regra de honestidade: só vale `consertada-nao-lancada`/`lancada` com commit na ref. "Lançado" = commit contido numa tag `v*`. Sem commit, escreva "suspeita" ou "sem commit" em `conserto`/`notas`. "Lançada" em issue antiga sem commit com `#N` quer dizer: a nossa resposta cita uma versão que existe como tag (ver nota na linha).
5. Atenção: mensagens de commit com `(#203)` / `(#204)` falam da VERSÃO 2.0.3 / 2.0.4 (o plano que hoje é a 2.0.5; a linha 2.0.3.1 é que saiu como 2.0.4), não das issues #203/#204. Esses dois números foram ignorados na busca por commits.
6. Auditoria: TODA validação (teste FAIL->PASS, revisão de Codex/OpenCode/ultrareview, teste na TV, leitura de log) entra em `validacoes` no json, com `data` (AAAA-MM-DD [HH:MM]), `quem` (pessoa ou agente e modelo), `o_que`, `issues` (lista, pode ser vazia), `ref` (commit, branch ou build testado), `resultado` (`passou`, `falhou`, `inconclusivo`, `nao-reproduziu`, `pendente`) e `evidencia` (arquivo, log, PR ou id do registro). Uma validação que falhou fica registrada mesmo depois do conserto: a seguinte aponta o conserto. O gerador recusa entrada sem esses campos.
7. Próximo passo "postar correção": há rascunhos em `/Volumes/ExternalSSD/tmp/203-respostas-correcao.md` (#302 #350 #286 #312 #368 #360); o dono decide. Correção conhecida: "Dolby Vision in MKV" é DESLIGADO por padrão (a resposta do #312 disse ligado).

## Roadmap

Alvo de cada issue aberta e os recursos por versão. Alvos são decisão de planejamento, não promessa pública; os pontos marcados SUSPEITA não têm confirmação.

### 2.0.3 - lançada 09/10 (v2.0.3, 8ed4517c)

Publicada na tag v2.0.3 (commit 8ed4517c, 09/10/2026). Lista completa em docs/releases/2.0.3/NOTAS.md. Em 09/10 os consertos com todos os commits contidos na tag passaram a status lancada; o que ficou como estava tem commits fora da tag ou conserto só suspeito (ver notas de cada issue). Inclui #361 (URL de cartaz longa).


### 2.0.4 - hotfix (antes chamado 2.0.3.1; sai como 2.0.4 porque webOS e Tizen exigem x.y.z) em revisao no PR #408 (so revisao); tudo abaixo integrado em integracao/2.0.3.1 menos o Dolby Vision perfil 5, sem tag

Hotfix sobre a 2.0.3 (já publicada na tag v2.0.3) com as causas achadas em 09/10.

- **Temporadas: abas da lista publicada (#372 parte 1)** (#372): INTEGRADO em integracao/2.0.3.1 (merge 21077ab5; 038a7dc7, c2112b61, a120b67c, 167310e6, 8eb5ce90), sem tag v*. CAUSA PROVADA: regressão 9677065b (v2.0.1, catálogo do Nuvio primeiro para a lista de episódios; o TMDB junta anime longo, Bleach {1:366,2:50} contra S1-16 no Cinemeta) e defeito antigo das abas de temporada vindas do corpo do Nuvio mesmo quando a lista do add-on vence (descoberta.c ~7069-7087, detail.c ~742-765). Log BZB857 (2.0.3, webOS). A parte 2 (numeração) foi para a 2.0.5. #328 saiu daqui: vai para a 2.0.5. Origem: logs dos autores, 09/10; decisão do dono 09/10. Branches: `agente/2031-temporadas (integrada)`.
- **Queda ao pesquisar na TV ao vivo (#344), primeira rodada** (#344): INTEGRADO em integracao/2.0.3.1 (merge c9af64b1; teste 9d78c75a, conserto 8dd273f3), sem tag v*. CAUSA PROVADA por tombstone simbolizado (log AX1R49): leitura fora do limite em buscaFazer (src/guia.c ~3965, ps[24] com epg_faixa/xtepg_faixa devolvendo o total), desde a v1.7.0, todas as plataformas com o guia. Origem: log do autor, 09/10. Branches: `agente/2031-guiabusca (integrada)`.
- **Ordem da Home ao trocar de perfil (#392, #294)** (#392, #294): INTEGRADO em integracao/2.0.3.1 (merge bdd49eb6). CAUSA PROVADA pelo log VV8JG2 (.tpk Tizen 6.0, v2.0.3): o build da Home registra os catálogos de add-ons do perfil anterior no arquivo de ordem do perfil novo e expulsa fileiras. Teste bb583b60, conserto 9023ebdb. Origem: log do autor, 09/10. Branches: `agente/2031-ordemperfil`.
- **Queda do webOS 3 na sonda do adaptador DTS**: INTEGRADO em integracao/2.0.3.1 (merge 09ce19a0): a sonda do adaptador DTS responde uma vez por processo (00b4a937) e a libplayerAPIs fica na memória (RTLD_NODELETE, 65491e7b); a explicação do mecanismo é SUSPEITA, não certeza (1d7264c1). Sem issue no mapa. Origem: investigação 09/10. Branches: `agente/2031-webos3`.
- **Fontes com FHD sem numero caem em Outras (#402)** (#402): INTEGRADO em integracao/2.0.3.1 (merge ca8f29b4; teste tests/fhd402.sh, revisado por Codex e OpenCode). CAUSA lida no codigo: stream_parse.c:281-283 e badges.c:40-42 nao reconhecem FHD/FULLHD sem numero, entao altura=0 e o grupo vira Outras (streams.c:2105). Conserto de 2 linhas mais teste; candidato a entrar na 2.0.4 se o dono quiser. Nao e a ordenacao nem uma configuracao. Origem: agente 09/10.
- **Legendas "Da conta" e modelo de URL do poster (#378, #390)**: INTEGRADO em integracao/2.0.3.1 (merges 28aa5c4b e beab28ec), cada um com teste que falhava antes do conserto.
- **Dolby Vision perfil 5 na LG: decisão pendente (HDR10 ou MP4 primeiro)**: Pendente: duas branches de alternativas; a decisão espera o teste do dono na C9. Entra no hotfix só se for escolhida. Origem: dono, 09/10; decisão pendente. Branches: `agente/2031-dvp5-hdr10`, `agente/2031-dvp5-mp4`.

### 2.0.5 - planejada; já tem branches agente/203-3xx e agente/204-*, nada integrado

Pequenos recursos de player e Biblioteca que já têm código fora da 2.0.3, mais triagem dos bugs sem conserto (pedir log).

- **Próximo episódio automático após 5 s** (#256, #331, #326, #329): Estende a contagem estilo Netflix do #331 (agente/203-331) e o pedido #256; é uma opção no mesmo card, então vai junto. Sem branch próprio para os 5 s: SUSPEITA de que baste um valor padrão.
- **Estatísticas do player e tamanho do buffer** (#338): Código pronto em agente/203-338; a resposta já prometeu "next build".
- **Biblioteca: indicadores de visto, esconder assistidos, vitrine de coleção, detalhes da fonte** (#352, #366, #362, #355): Pequenos, cada um em seu branch agente/204-*; não dependem do redesign.
- **Catálogos da conta, sync de add-ons, fonte recusada, P2P por formato** (#358, #365, #360, #364, #349): Branches agente/204-conta-catalogos, 204-sync-addons, 204-fonte-recusada prontos ou quase.
- **Samsung: DTS/TrueHD escondido (fase 1)** (#313): Plano e spike em agente/204-samsung-dts (docs/plans); fase 1 já planejada para a 2.0.5.
- **Android: fileiras ilimitadas na Home, armazenamento rotativo do P2P** (#334): agente/204-fileiras-android e agente/203-334-janela (janela de streaming do P2P) existem e ficaram fora da 2.0.3 por risco.
- **Triagem de bugs sem conserto e dois pedidos pequenos (#306, #337)** (#288, #315, #316, #344, #345, #346, #353, #357, #367, #372, #373, #378, #379, #306, #337, #385, #308): Todos aguardam log do autor; entram na 2.0.5 se o log aparecer a tempo, senão escorregam.
- **Binge group: lembrar também a fonte que o automático tocou (#310)** (#310): Pedido (a) do #310. A revisão de código mostrou que a escolha manual de outro episódio já aplica o binge group; a lacuna era só a fonte escolhida à mão ser lembrada. O dono aprovou em 09/10 ("sim vamos adicionar na 2.0.4"; a 2.0.4 planejada foi renumerada para 2.0.5 em 09/10) lembrar também o binge group da fonte que o automático tocou. Código em agente/204-binge (29d2c248, 02753401, 6345e9ff), não integrado. Origem: dono 09/10. Branches: `agente/204-binge`.
- **Trailer vazando para a sessão do episódio no .tpk (#385)** (#385): SUSPEITA, sem conserto. No log do #385 (.tpk, 2.0.2), "tpk evento 1 (129036)" vem sempre depois de "[trailer] ... no fundo com som" e aparece até sem episódio aberto; no log 1 o evento de duração do trailer chegou depois de "abrir: url ao pipeline" e o posplay o leu como duração do episódio ("duracao suspeita 129s"). Conferir se os eventos do player do trailer (duração, fim, erro) são descartados ao abrir o episódio. Separado da recusa de conexão do TorBox, que foi bloqueio do nó/IP (conserto da pausa na 2.0.3). Origem: logs do #385, 09/10.
- **Numeração de temporadas Cinemeta + camada de tradução (#372 parte 2)** (#372, #328): Opção (b) escolhida pelo dono: lista de episódios na numeração Cinemeta/TVDB mais uma camada de tradução por número absoluto do episódio para o progresso/scrobble do Trakt, a conta Nuvio, o Simkl, os ids de "a seguir"/Continuar assistindo e as marcas de visto (vistonao/vistoep). A parte 1 (abas de temporada) sai na 2.0.4. #328 acompanha: mesma raiz suspeita. Origem: dono 09/10.
- **CI com ASan/TSan nos testes de host (+ ASan do app inteiro por candidata a release, helpers de buffer limitado)**: Alternativa sem Rust do estudo: SANITIZE=1 e SANITIZE=thread nos 152 + 29 testes que já aceitam, falhando o PR; roteiro fixo com ASan no app inteiro por release candidate; helpers de buffer limitado (nv_cpy, nv_slice). Cobre os bugs de memória do ranking a custo de horas, sem tocar em alvo de TV. Referência: docs/plans/rust-piloto.md ("alternativas sem Rust", seção 5). Origem: dono 09/10. Branches: `docs/plans/rust-piloto.md`.
- **Biblioteca: aba Salvos seguir o destino Simkl (sugestao)** (#393): #393; só se o autor confirmar que o que falta é isso (Listas › Simkl e "Onde o + salva" já existem). Sugestão do agente, não aprovado pelo dono. Origem: agente 09/10.
- **Binario do LG sem simbolos de depuracao: ipk menor (#401)** (#401): MEDIDO no ipk 2.0.4: 58,3 MB, binario 55,3 MB com debug_info/.symtab (30,8 MB de secoes de depuracao, 10,5 MB comprimidas), arte 39 MB ja comprimida. Strip no link do tools/arm.sh deve baixar o ipk para ~48 MB (estimativa, nao medida) e guardar o binario com simbolos como anexo da release para simbolizar queda. Nao prova que seja a causa dos 15 min do #401 (SUSPEITA: download da TV ou instalacao de 1190 arquivos); pedir ao autor o tempo por fase. Fontes opcionais (Roboto, Montserrat, Inter, ate ~4 MB descomprimidos) poderiam baixar sob demanda como a Naskh (assrender.c ~870), sem promessa. Origem: agente 09/10.
- **Botao Proximo episodio na fileira de controles do player (#403)** (#403): Hoje so o cartao na janela de creditos/fim (dispensa gruda) e a lista pelo botao Episodios. Sugestao: acao de proximo episodio na fileira (ou atalho) para serie, reaproveitando player_proximo_episodio e o pedido de app.c:4424. Vai junto com o proximo episodio automatico. Sugestao do agente, nao aprovado pelo dono. Origem: agente 09/10.
- **Dados do app fora da pasta do app (LG) e limpeza de sobras** (#401): Medido na C9 (09/10): .nuvio dentro da pasta do app com 359 MB / 3980 arquivos (cache de imagens 111 MB, cache.suspeito-1853 195 MB nunca apagado, trailers 28 MB, fontes de legenda ASS repetidas por faixa ~7,5 MB cada). SUSPEITA de ser o que deixa a atualização pelo Homebrew lenta (#401): instalação nova é rápida, atualização por cima leva 15 min. Fazer: (1) cache de imagens, trailers e fontes da legenda fora da pasta do app; (2) apagar cache.suspeito-* antigos; (3) não guardar cópia das fontes por faixa. Aprovado pelo dono 09/10.
- **Novos de 09/10: ordem do add-on, busca rapida, notas em bloco, menu do topo (#400, #405, #406, #407)**: Triagem 09/10 sem conserto: #400 pedido (opcao de manter a ordem do add-on), #405 bug com log ABB125, #406 pedido, #407 bug sem log, todos do Samsung menos o #400.

### 2.1 - aprovada pelo dono em 06/10/2026 (e 07/10 para Ajustes); sem data

Os seis recursos aprovados pelo dono, mais perfis com o pacote de UX dos Ajustes. Os itens marcados "suspeita" foram colocados aqui pelo agente anterior e NÃO estão aprovados.

- **Watch Together (Assistir Juntos)**: Aprovado pelo dono para a 2.1. Sala compartilhada entre aparelhos; o branch já carrega a marca de perfil infantil enviada para a sala. Origem: dono 06/10. Branches: `feat/watch-together`.
- **Diário + sono (Diário + sono)**: Aprovado pelo dono para a 2.1. Origem: dono 06/10. Branches: `feat/sp-diario`.
- **Trava (Lock)**: Aprovado pelo dono para a 2.1. Origem: dono 06/10. Branches: `feat/sp-trava`.
- **Caça / Conquistas (Hunt/Achievements)**: Aprovado pelo dono para a 2.1. Origem: dono 06/10. Branches: `feat/sp-conquistas`.
- **Redesign da Biblioteca (Biblioteca)**: Branch agente/202-biblioteca (procedência por título, aba Listas); o plano de dados está em docs/plans/biblioteca-ilimitada.md (branch agente/biblioteca-ilimitada-plano) e diz "não na 2.0.3". Mexe em memória e navegação; não cabe num ponto de versão. Origem: dono 06/10. Branches: `agente/202-biblioteca`, `agente/biblioteca-ilimitada-plano (docs/plans/biblioteca-ilimitada.md)`.
- **Coleções + fileiras**: Aprovado pelo dono para a 2.1. Os pequenos ajustes de vitrine (#362) continuam na 2.0.5 em agente/204-colecao-vitrine e não dependem disto. Origem: dono 06/10. Branches: `feat/colecoes-fileiras`, `agente/201-colecoes`.
- **Perfis + tour, "Começar", "Resolver um problema" e busca com i18n nos Ajustes**: Decisão de UX dos Ajustes de 07/10: esses quatro itens saem na 2.1 junto com perfis. Branch de cada um: não identificado (suspeita: parte do trabalho está em feat/ajustes-ux e agente/204-perfil, não confirmado). Origem: dono 07/10.
- **Limites maiores de Trakt/Biblioteca (sem teto)**: Parte do plano biblioteca-ilimitada, que o dono aprovou só como redesign; tirar os tetos do Trakt como item próprio não foi citado por ele. A 2.0.3 só pagina a watchlist do Trakt até 400 itens e mantém os tetos de hoje (96ccdab1). Tirar o teto exige o índice leve + janela de CatItem do plano biblioteca-ilimitada, que é parte do redesign. Origem: suspeita (sugestão do agente, não aprovado).
- **Remoção de barras pretas embutidas no quadro (blackbar/crop)** (#341): Recurso novo, sem issue nem branch (relacionados: #341 aspecto/zoom e #241 barras do trailer, esse já tratado). Precisa detectar a barra no quadro em cada pipeline (LG, .tpk, Android). SUSPEITA de 2.1; pode virar 2.0.5 se for só um recorte manual por título. Origem: suspeita (sugestão do agente, não aprovado).
- **Dolby Vision em MKV na Samsung**: A 2.0.3 só traz DV em MKV na LG (opcional, desligado) e a Samsung nunca escolhe DV. Na Samsung falta prova de que o decodificador aceita; precisa de spike por modelo antes de prometer. SUSPEITA de 2.1. Origem: suspeita (sugestão do agente, não aprovado).
- **Menus em árabe e auto sync de legenda por linha** (#250, #333, #374): SUSPEITA do agente, fora do roadmap do dono: o árabe nos menus foi prometido na resposta do #325 e o auto sync tem pesquisa em legenda-sync-lg-samsung-pesquisa.md, mas nenhum dos dois consta da decisão de 06/10. Pode sair da 2.1 para futuro se o dono preferir. Origem: suspeita (sugestão do agente, não aprovado).
- **Plano de refatoração geral**: docs/plans/refatoracao-geral.md (branch agente/refatoracao-plano, 0dc06334): 23 mudanças pequenas, cada uma com porteiro de teste. Fazer depois da 2.0.3 sair e fora da 2.0.5, para não misturar refatoração com os recursos acima. Itens 01-04 (apagar morto, deduplicar Biblioteca) vão melhor junto do redesign da Biblioteca. Ordem com o Rust (dono 09/10): o porte Rust de fonteparalela+streams vem depois do piloto e deve ser coordenado com os itens do plano que tocam streams.c. Origem: suspeita (sugestão do agente, não aprovado). Branches: `agente/refatoracao-plano`.
- **Auto-play: reusar último link com validade e regex separada por tipo (filmes / séries / anime)** (#310): Pedidos (b) e (c) do #310. (b) não há cache de link hoje (fontepref guarda só a identidade, 180 dias; fontecache é prefetch de 30 s): guardar URL de debrid pede arquivo, validade nos Ajustes e queda para a busca. (c) hoje há uma regex e um modo só (fonteregra.c:15, ajustes.c:1301, uma chave na conta): por tipo pede três regex, três modos, blob novo e uma definição de anime. Médio e grande, por isso 2.1 e não 2.0.5. Sem aprovação do dono ainda. Origem: suspeita (sugestão do agente, não aprovado).
- **Piloto Rust 'mkvcore' (no_std, atrás de -DNV_RUST_MKVCORE, com fallback C) — piloto de toolchain**: Experimento de toolchain nos 5 alvos com flag por alvo e fallback C (reversível trocando uma variável de build); flag desligada por padrão no primeiro candidato. Estimativa de 3 a 4 semanas, SUSPEITA. Referência: docs/plans/rust-piloto.md (seção 4). Origem: dono 09/10. Branches: `docs/plans/rust-piloto.md`.
- **Portar fonteparalela+streams e o publicador do catálogo (se o piloto passar)**: Só depois do piloto mkvcore passar os guards nos 5 alvos; é onde está o ganho real de Rust segundo o estudo, mas o custo é de meses (SUSPEITA). Coordenar com o plano de refatoração geral, que também mexe em streams.c. Referência: docs/plans/rust-piloto.md (seção 6). Origem: dono 09/10. Branches: `docs/plans/rust-piloto.md`.
- **Atraso do áudio no player (sugestao)** (#397): Pedido no #397; não existe hoje (só o atraso de legenda). Sugestão do agente, não aprovado pelo dono: começar onde o backend deixa (Android/Media3); LG e Samsung podem não ter API. Origem: agente 09/10.

### 2.2 - aprovada pelo dono em 06/10/2026; sem data

Servidores locais, guia de TV e música.

- **Servidores locais (Jellyfin / Plex / Emby)**: Aprovado pelo dono para a 2.2; ele tem um servidor real para testar. Origem: dono 06/10. Branches: `f11-jellyfin`, `feat/emby-plex`.
- **Guia de TV (guia pro)**: Aprovado pelo dono para a 2.2. Existem várias linhas de trabalho: guia-tv, guia-pro, guia-pro-2, lembrete e a UI w18; qual é a mais recente fica a conferir. Origem: dono 06/10. Branches: `feat/guia-tv`, `feat/guia-pro`, `feat/guia-pro-2`, `feat/guia-lembrete`, `ui/w18-guia-tv`.
- **Música (álbum da trilha sonora)**: Aprovado pelo dono para a 2.2: álbum da trilha, só link do Spotify, prévia para testar na TV e no celular, crowdsourcing no Android permitido, Apple Insight via uts-api timed-metadata. Branch: nenhum encontrado. Origem: dono 06/10.

### futuro - sem data nem dono

Pedidos que fazem sentido mas não têm plano.

- **Porte VIDAA, layout RTL, anime-skip.com, busca por microfone, pontos do carrossel estilo Apple TV, serviços na barra lateral** (#135, #260, #343, #347, #348, #304, #342): Cada um é grande ou depende de terceiros; nenhum tem branch.
- **Passo de quadro no player (sugestao)** (#397): Pedido no #397 ("frame skipping", ambíguo); não existe, só saltos de 10 a 120 s. Sugestão do agente, não aprovado pelo dono; depende de a TV aceitar avanço por quadro. Origem: agente 09/10.

### nao vamos fazer - fora do escopo do produto

PC, iOS, e itens que não são trabalho de código.

- **App para PC (#354), .ipa para iOS (#307), relatório automático de logs (#324), aviso de desempenho em 4K (#292)** (#354, #307, #324, #292): Plataformas que o projeto não atende ou issues que não pedem mudança.

Plano de refatoração: `docs/plans/refatoracao-geral.md` (branch `agente/refatoracao-plano`, ainda não integrado).

### Issues abertas por alvo

| Alvo | Qtd | Issues |
|---|---|---|
| 2.0.3 | 5 | #246, #280, #283, #334, #356 |
| 2.0.4 | 10 | #294, #344, #378, #390, #392, #402, #409, #410, #411, #412 |
| 2.0.5 | 48 | #266, #286, #288, #302, #306, #310, #313, #315, #316, #326, #328, #329, #331, #337, #338, #345, #346, #349, #350, #352, #353, #355, #357, #358, #360, #362, #364, #365, #366, #367, #369, #372, #373, #379, #382, #385, #386, #387, #388, #393, #394, #400, #401, #403, #404, #405, #406, #407 |
| 2.1 | 4 | #250, #333, #374, #397 |
| 2.2 | 0 |  |
| futuro | 8 | #135, #260, #304, #342, #343, #347, #348, #389 |
| nao vamos fazer | 4 | #292, #307, #324, #354 |
| ja-lancada | 30 | #144, #252, #269, #284, #287, #290, #293, #296, #298, #300, #303, #305, #312, #319, #320, #321, #322, #323, #330, #332, #335, #339, #340, #341, #361, #363, #368, #370, #383, #384 |

## Fechar com a 2.0.3

Issues ABERTAS no GitHub cujo conserto saiu na v2.0.3 (commits contidos na tag). Cada uma tem uma resposta curta em inglês para colar; "autor confirmou" diz se quem abriu já testou. O dono decide quando fechar.

| # | Título | Autor confirmou? | Resposta curta (EN) |
|---|---|---|---|
| [#269](https://github.com/iqui27/nuvio-native-legacy/issues/269) | Embedded subtitles are not being detected or not being displ | NÃO: o autor agradeceu o conserto parcial da 2.0.2; nao testou a 2.0.3 | Thanks for reporting this! Embedded subtitles on big MKV files (over 2 GB) are fixed in 2.0.3. Please update, and reopen this if they still don't show up. |
| [#284](https://github.com/iqui27/nuvio-native-legacy/issues/284) | Source result ("Best for this tv") missing resolution | NÃO: sem comentarios do autor | Thanks! The "Best for this TV" source line now shows the resolution in 2.0.3. Please reopen if you still don't see it. |
| [#305](https://github.com/iqui27/nuvio-native-legacy/issues/305) | Hide player ui when pressing up | NÃO: sem confirmacao do autor | Done in 2.0.3: pressing Up on the seek bar now hides the player controls (any key brings them back). Reopen if it doesn't work for you. |
| [#312](https://github.com/iqui27/nuvio-native-legacy/issues/312) | few minor bugs, none affect use | NÃO: correcao do texto publicada 09/10; autor nao respondeu | The Dolby Vision correction is posted above: Dolby Vision in MKV is off by default in 2.0.3 (Settings > Playback > More options). Thanks, and reopen if anything is still off. |
| [#319](https://github.com/iqui27/nuvio-native-legacy/issues/319) | Home row still not updating properly | NÃO: sem confirmacao do autor | Thanks for the log! 2.0.3 fixes the home rows not refreshing (rows of switched-off add-ons are now cleaned up properly). Please reopen if it still happens. |
| [#320](https://github.com/iqui27/nuvio-native-legacy/issues/320) | Arabic language in Subtitle shows no glyph font | NÃO: sem confirmacao do autor | This is fixed in 2.0.3: the Arabic subtitle font is now found even when the update only replaced the app's code. Please reopen if you still see empty boxes. |
| [#321](https://github.com/iqui27/nuvio-native-legacy/issues/321) | The "not started" label does not shift downwards. | NÃO: sem confirmacao do autor | Fixed in 2.0.3: the section label now moves down with the expanded card. Reopen if you still see it. |
| [#322](https://github.com/iqui27/nuvio-native-legacy/issues/322) | Can't delete from Continue watching on the home screen | NÃO: sem confirmacao do autor | 2.0.3 adds "Remove from Continue Watching" for the "Up next" items too (plus an option to turn them off). Please reopen if it doesn't work for you. |
| [#323](https://github.com/iqui27/nuvio-native-legacy/issues/323) | App crashes on playback | NÃO: o autor confirmou que sem o passo de varias fontes nao cai; nao testou a 2.0.3 | The crash in the "check several sources at once" step is fixed in 2.0.3, so you can turn that option back on. Please reopen if it crashes again. |
| [#330](https://github.com/iqui27/nuvio-native-legacy/issues/330) | [Bug]extremely laggy after repeated playback | NÃO: sem comentarios do autor | Thanks for the report! 2.0.3 includes a fix for the lag after repeated playback. Please update and reopen if it's still slow. |
| [#332](https://github.com/iqui27/nuvio-native-legacy/issues/332) | Nuvio didn't finish opening | NÃO: sem confirmacao do autor; suspeita de mesma raiz do #266, sem log do aparelho | Thanks for the report! 2.0.3 includes the startup fixes for these Android TV devices (and the poster glass outline fix). Please update and reopen if it still doesn't finish opening. |
| [#335](https://github.com/iqui27/nuvio-native-legacy/issues/335) | Extend Arabic subtitle size scale (200%-250%), ASS support,  | NÃO: sem confirmacao do autor | 2.0.3 raises the subtitle size up to 250%, uses a real bold Arabic font, and draws ASS subtitles at the screen's real resolution. Please reopen if anything is still off. |
| [#339](https://github.com/iqui27/nuvio-native-legacy/issues/339) | Pantalla de ajustes | NÃO: sem confirmacao do autor | 2.0.3 adds a "Settings layout" option (Settings > Appearance) with the List style, categories and options one below the other as before. Thanks for the feedback, and reopen if it's not what you wanted. |
| [#340](https://github.com/iqui27/nuvio-native-legacy/issues/340) | Quicker Seek/skipping | NÃO: sem comentarios do autor | 2.0.3 makes seeking and skipping quicker. Please try it and reopen if it still feels slow. |
| [#361](https://github.com/iqui27/nuvio-native-legacy/issues/361) | Poster URL Max Character Length Too Short | NÃO: sem comentarios do autor | Long poster URLs now come through whole in 2.0.3 (background, details, Saved and account). Please reopen if you still see broken posters. |
| [#363](https://github.com/iqui27/nuvio-native-legacy/issues/363) | Unable to Send Recommendation to a Friend | NÃO: o autor mandou o log; nao confirmou o conserto | Fixed in 2.0.3: recommending an episode from Continue Watching now sends the show correctly. Please reopen if it still fails. |
| [#368](https://github.com/iqui27/nuvio-native-legacy/issues/368) | Tab enhancement | NÃO: sem confirmacao do autor | 2.0.3 makes search wait for you to stop typing (300 ms) and adds the option to hide add-ons in the TV guide. Please reopen if search is still slow on your TV. |
| [#370](https://github.com/iqui27/nuvio-native-legacy/issues/370) | Subtitles sync issues | NÃO: sem confirmacao do autor; o sincronismo pode continuar fora | 2.0.3 fixes Arabic plain-text subtitles showing boxes on the Samsung .wgt. If the timing is still off now that the text is readable, please reopen and tell us which title. |
| [#384](https://github.com/iqui27/nuvio-native-legacy/issues/384) | Embedded ASS subtitles stopped rendering after anime intro | NÃO: o autor mandou logs; nao confirmou na 2.0.3 (a queda de conexao que ele citou segue sem log proprio) | Fixed in 2.0.3: the embedded ASS subtitle index now covers the whole file, so the subtitles no longer vanish after the intro. Please update and reopen if they still do. |

### Não fechar ainda

| # | Título | Motivo |
|---|---|---|
| [#294](https://github.com/iqui27/nuvio-native-legacy/issues/294) | 🐛 Home collection order resets after switching profiles | A #392 (rawldon, 09/10, v2.0.3, Samsung Tizen 6.0 .tpk) diz que trocar de perfil e voltar ainda reordena a Home (coleções, catálogos e Continuar). Conserto da 2.0.3 não basta; agente/2031-ordemperfil investigando (logs VV8JG2/Z4HDY2). |
| [#266](https://github.com/iqui27/nuvio-native-legacy/issues/266) | Bug: Doesn't open on ATv | Varios relatos no mesmo fio: o QR do Shield foi confirmado (charles474), mas ele trouxe queixa nova de legenda diferente do Nuvio oficial e ha log de outro aparelho (Airtel Xtreme) sem leitura. |
| [#286](https://github.com/iqui27/nuvio-native-legacy/issues/286) | Performance on older Samsung UA40N5300 | O autor disse "a bit better"; ainda ha trabalho de desempenho no Mali-400 em andamento (poster/GPU, ordenacao do sync). |
| [#302](https://github.com/iqui27/nuvio-native-legacy/issues/302) | Live TV issue persists with 2.1 tpk65 | O autor segue dizendo que a TV ao vivo congela o filme (log 2TAVFN); a leitura nossa diz que e a rede, sem confirmacao dele. |
| [#350](https://github.com/iqui27/nuvio-native-legacy/issues/350) | Major bug | Mesmo autor do #302: filme travando durante a TV ao vivo ainda sem resposta; fechar so depois de resolver esse ponto. |
| [#328](https://github.com/iqui27/nuvio-native-legacy/issues/328) | 🐛 Bug Report: Episodes Are Being Duplicated | Vai para a 2.0.5 com a parte 2 do #372 (numeração Cinemeta + camada de tradução): suspeita de mesma raiz, não confirmada; o commit de duplicados da 2.0.3 não cobre a causa provada. Autor não testou. |
| [#341](https://github.com/iqui27/nuvio-native-legacy/issues/341) | Aspect Ratio / Crop feature to fill screen does not work on  | SUSPEITA: nenhum commit cita o #341 e nao foi testado em Tizen 5; o pedido de crop de barras pretas segue como decisao aberta. |
| [#369](https://github.com/iqui27/nuvio-native-legacy/issues/369) | [Bug] Multiple issues/Missing Features on Android TV version | Relatorio de 9 itens; so 2 tem commit (tailandes e ocultar nao lancados), os outros 7 ficam abertos (alvo 2.0.5). |
| [#383](https://github.com/iqui27/nuvio-native-legacy/issues/383) | [port] subtitle memory same as nuvio | SUSPEITA de que a 2.0.3 cobre o pedido (memoria por perfil/titulo); pedir ao autor que confirme que bate com o Nuvio antes de fechar. |
| [#385](https://github.com/iqui27/nuvio-native-legacy/issues/385) | Internet Connection Drop and Reconnecting Bug to no end | Suspeitas abertas: verificacao paralela de fontes e vazamento de eventos do trailer no .tpk; o conserto da 2.0.3 e so a pausa da leitura lateral. Alvo 2.0.5. |
| [#334](https://github.com/iqui27/nuvio-native-legacy/issues/334) | P2P filled the TV's free space" error when playing large/4K  | Limite de P2P e limpeza na 2.0.3, mas a janela de streaming (armazenamento rotativo) so na 2.0.5; avisar na issue. |
| [#360](https://github.com/iqui27/nuvio-native-legacy/issues/360) | Sync addons | Mescla do sync na 2.0.3, mas o botao "Sincronizar addons" (e2f1eede) e 2.0.5. |
| [#246](https://github.com/iqui27/nuvio-native-legacy/issues/246) | HEVC anime videos can’t be “seeked in player” | precisa-log: nao confirmado que o conserto da 2.0.3 cobre o relato HEVC/anime; falta log do autor. |
| [#280](https://github.com/iqui27/nuvio-native-legacy/issues/280) | Home Row does not update properly (Bingecat Addon) and Some  | precisa-log: correcao generica de fileiras na 2.0.3, sem log do autor para confirmar. |
| [#356](https://github.com/iqui27/nuvio-native-legacy/issues/356) | Upcoming shows not appearing in continue watching | precisa-log: leitura do log T5JF2G indica causa nao coberta pela 2.0.3; precisa de log novo. |
| [#392](https://github.com/iqui27/nuvio-native-legacy/issues/392) | Profile switching still rearrange my home screen | Conserto em agente/2031-ordemperfil, ainda não integrado nem lançado (2.0.4). |

## Decisões para o dono

### Pendentes

1. `dec-256-proximo-episodio` [pendente] Próximo episódio automático após 5 s: entra na 2.0.5 junto da contagem do #331, ligado por padrão ou desligado? Recomendação: 2.0.5, desligado por padrão, valor configurável. O #256 ainda pede que o card volte depois de dispensado; isso fica fora.
2. `dec-360-botao-sincronizar` [pendente] #360 (botão Sincronizar addons) fica em 2.0.5 mesmo com a mescla do sync já na 2.0.3? Recomendação: Sim, 2.0.5; corrigir a resposta que perguntou "on 2.0.3?" dizendo que a mescla vem agora e o botão depois.
3. `dec-334-p2p-limite` [pendente] #334 sai como 2.0.3 só com o limite de P2P e a limpeza, e a janela de streaming (armazenamento rotativo, agente/203-334-janela) fica para a 2.0.5? Recomendação: Sim: limite e limpeza já estão em 2.0.3; a janela muda o motor de P2P e a 2.0.3 está congelada. Avisar na issue que o rotativo vem na 2.0.5.
4. `dec-326-promessas-next-build` [pendente] #326 e #338 foram prometidos para "próxima atualização/next build" e o código só está em branches de 2.0.5. Responder já avisando 2.0.5? Recomendação: Sim, corrigir a promessa agora.
5. `dec-bugs-sem-log` [pendente] Bugs sem log (#288, #315, #316, #344-346, #353, #357, #367, #372, #373, #378, #379) ficam todos em 2.0.5? Recomendação: Sim como alvo de triagem, mas sem prometer: pedir log primeiro; os que não chegarem com log em 30 dias viram "precisa-log" e saem do alvo.
6. `dec-354-nao-vamos-fazer` [pendente] "nao vamos fazer": #354 (PC), #307 (.ipa iOS), #324 (relatório de logs), #292 (aviso 4K Android). Confirma? Recomendação: Confirmar #354 e #307 (fora de escopo, já classificados fora-do-escopo). #324 e #292 não são pedidos de recurso: fechar como informativos em vez de "não vamos fazer".
7. `dec-304-pedidos-interface` [pendente] Pedidos de interface (#304 serviços na barra lateral, #347 microfone, #348 pontos do carrossel/botão Reproduzir, #342 botão Pular cena) ficam em "futuro" ou "nao vamos fazer"? Recomendação: Futuro para #304, #348 e #347. #342 (conteúdo explícito) depende de base de dados de terceiros que não existe: recomendo nao vamos fazer.
8. `dec-135-vidaa-rtl` [pendente] #135 (porte VIDAA) e #260 (RTL) em "futuro" ou "nao vamos fazer"? Recomendação: Futuro; há trabalho feito em feat/vidaa e uma resposta pública dizendo "depois" para o RTL.
9. `dec-144-aguardando-fechamento` [pendente] Issues abertas só aguardando fechamento (#144, #252, #287, #290, #293, #296, #298, #300, #303): fechar? Recomendação: Fechar com comentário, depois que o dono aprovar o texto. Alvo registrado como ja-lancada (valor extra além dos cinco pedidos).
10. `dec-341-blackbar-crop` [pendente] Blackbar/crop (remover barras pretas embutidas) é 2.1, 2.2 ou 2.0.5? Não consta do roadmap aprovado. Recomendação: Sugestão do agente, não aprovada: manter fora das versões aprovadas; se o dono quiser, 2.2 ou futuro, pois é recurso novo, sem branch e toca os três pipelines de vídeo.
11. `dec-dv-mkv-samsung` [pendente] Dolby Vision em MKV na Samsung: manter na 2.1, passar para 2.2 ou futuro? Não consta do roadmap aprovado. Recomendação: Sugestão do agente, não aprovada: futuro, só como spike de pesquisa; não prometer ao público até haver prova num modelo.
12. `dec-352-limites-trakt` [pendente] Limites maiores do Trakt/Biblioteca: entram dentro do redesign aprovado da 2.1 ou ficam fora? Recomendação: Sugestão do agente, não aprovada: dentro do redesign (o plano biblioteca-ilimitada é parte dele). O redesign em si já está aprovado; a 2.0.5 fica só com indicadores de visto (#352) e esconder assistidos (#366).
13. `dec-refatoracao-geral` [pendente] Plano de refatoração geral (docs/plans/refatoracao-geral.md): quando? Não consta do roadmap aprovado. Recomendação: Sugestão do agente, não aprovada: depois da 2.0.3, na 2.1 como trabalho próprio; na 2.0.5 no máximo os itens 01-04 (apagar morto). Não misturar com recursos.
14. `dec-250-arabe-autosync` [pendente] Menus em árabe (#250, #333) e auto sync de legenda (#374) seguem na 2.1? Não constam do roadmap aprovado; o árabe foi prometido na resposta do #325. Recomendação: Sugestão do agente, não aprovada: manter na 2.1 só se sobrar espaço; senão futuro. Corrigir a promessa pública do #325 se for para futuro.
15. `dec-fechar-203-sem-confirmacao` [pendente] Fechar com a 2.0.3 (bloco acima): fechar já as issues sem confirmação do autor ou esperar o retorno? Recomendação: Fechar já #317 e #318 (autores confirmaram). As demais, esperar alguns dias pelo retorno; a resposta curta já pede para reabrir se persistir, então fechar também é aceitável.
16. `dec-perfil-sem-trakt` [pendente] Perfil sem Trakt (usuários com Simkl veem "Trakt desconectado"): entra em qual versão? Recomendação: 2.0.5: perfil com dados do Simkl e mensagem certa quando só o Simkl está ligado.

### Decididas

- 2026-10-09 (dono (Henrique)), `dec-400-ordem-fontes-addon` [decidida]: Ordem das fontes do add-on (#400, AIOStreams): entra na 2.0.4 ou fica para a 2.0.5? **Entra na 2.0.4. Implementação delegada ao Codex em agente/204-ordem400.**
- 2026-10-09 (dono (Henrique)), `dec-408-atualizar-pr` [decidida]: Atualizar o PR #408 (review only) com KM7, consertos do Copilot/ultrareview, OpenSubtitles e mapa? **Sim. Push feito (063f381b).**
- 2026-10-09 (dono (Henrique)), `dec-204-card-novidades` [decidida]: Card de novidades da 2.0.4 (hotfix): fazer ou não? **Fazer. Delegado ao Codex em agente/204-novidades (até 3 cenas, só o que NOTAS.md prova).**
- 2026-10-09 (dono (Henrique)), `dec-painel-sempre-atualizado` [decidida]: Painel de issues: sempre atualizado? **Sim, faz parte do fluxo: hook post-commit/post-merge republica o painel (tools/painel-issues).**
- 2026-10-09 (dono (Henrique)), `dec-km7-se-hdr` [decidida]: Erros de KM7 SE (Reddit): consertar na 2.0.4? **Sim, 2.0.4 (98929593, ccd37565).**
- 2026-10-09 (dono (Henrique)), `dec-dvp5-lg-hdr10-ou-mp4` [decidida]: Dolby Vision perfil 5 na LG: variante HDR10 ou MP4 primeiro (agente/2031-dvp5-hdr10 x agente/2031-dvp5-mp4)? **MP4 (agente/2031-dvp5-mp4) juntado na 2.0.4; C9 rodou o build, sem título DV perfil 5 em MP4 para exercitar.**
- 2026-10-09 (dono (Henrique)), `dec-canal-comunidade` [decidida]: Canal de comunidade (Telegram ou Discord) pedido por usuário? **Discord. Servidor "Nuvio Legacy" criado em 09/10 com modo Comunidade e estrutura no estilo do oficial (Info & Updates, Discussion por plataforma, Feedback em fóruns, Testing, Off-Topic). Convite permanente: https://discord.gg/9NWr6SHyzJ (vai no cartão de novidades com QR, notas e README).**
- 2026-10-09 (dono (Henrique)), `dec-410-vidro-profundidade-tpk` [decidida]: #410 (vidro, Profundidade e Reflexo no .tpk) entra em qual versão? **2.0.4: é regressão da 2.0.3.**
- 2026-10-09 (dono (Henrique)), `dec-addons-principal-off` [decidida]: "Usar os addons do perfil principal" ligado ou desligado por padrão? **Desligado por padrão (a6ae4f64); quem já escolheu mantém.**
- 2026-10-09 (dono (Henrique)), `dec-central-pi` [decidida]: Central de comando: base dos executores? **Sim: pi como harness dos executores; Sol estudando o RPC/sessões/fork.**

## Resumo

Por status:

| Status | Qtd |
|---|---|
| lancada | 268 |
| aberta | 32 |
| respondida | 24 |
| consertada-nao-lancada | 22 |
| por-desenho | 14 |
| precisa-log | 8 |
| fechada-sem-resposta | 6 |
| fora-do-escopo | 3 |
| duplicada | 2 |

Por release (grupo de planejamento):

| Grupo | Qtd |
|---|---|
| lançadas em tag v* (qualquer versão) | 268 |
| sem release | 85 |
| 2.0.5 (branches) | 11 |
| 2.0.3 lançada, com pendência | 5 |
| futuro (2.1/2.2) | 5 |
| 2.0.4 (hotfix, sem tag) | 5 |

Lançadas por versão: 1.0.7: 2, 1.0.10: 1, 1.0.13: 1, 1.0.15: 1, 1.0.16: 1, 1.0.21: 1, 1.0.23: 1, 1.0.29: 1, 1.0.30: 3, 1.0.31: 1, 1.0.32: 1, 1.0.34: 1, 1.0.35: 1, 1.0.36: 1, 1.0.38: 2, 1.0.41: 1, 1.0.43: 5, 1.0.44: 4, 1.0.45: 1, 1.0.51: 4, 1.0.53: 1, 1.0.54: 1, 1.0.55: 1, 1.0.56: 1, 1.1.0: 2, 1.1.2: 2, 1.2.1: 4, 1.3.0: 1, 1.3.2: 4, 1.3.4: 6, 1.3.4-comparacao1: 1, 1.3.5: 1, 1.3.7: 1, 1.3.10: 1, 1.3.11: 2, 1.3.12: 4, 1.4: 6, 1.4.1: 1, 1.4.2: 9, 1.4.3: 9, 1.4.4: 2, 1.4.5: 2, 1.4.6: 8, 1.4.7: 2, 1.5.0: 1, 1.5.1: 5, 1.5.2: 8, 1.5.3: 3, 1.5.4: 4, 1.6.0: 11, 1.6.1: 1, 1.6.2: 3, 1.6.3: 2, 1.6.4: 5, 1.6.5: 4, 1.7.0: 11, 1.7.1: 4, 1.7.2: 3, 1.7.4: 4, 2.0.0: 28, 2.0.1: 12, 2.0.2: 18, 2.0.3: 35.

Abertas no GitHub: 109. Fechadas: 270.
Abertas sem nenhum comentário nosso: 60.

## 2.0.3 lançada com pendência (precisa-log, respondida ou conserto parcial)

5 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#246](https://github.com/iqui27/nuvio-native-legacy/issues/246) | HEVC anime videos can’t be “seeked in player” | ? | bug | precisa-log | 2.0.3 | 2.0.3 | a7f23592, a8048e0d, 023497dd | nós 10-06 | pedir log (pedido em 06/10, sem retorno) |
| [#280](https://github.com/iqui27/nuvio-native-legacy/issues/280) | Home Row does not update properly (Bingecat Addon) and Some  | ? | bug | precisa-log | 2.0.3 | 2.0.3 | 3c51d5b7 | autor 10-06 | pedir log (autor mandou só captura "See logs", sem código) |
| [#334](https://github.com/iqui27/nuvio-native-legacy/issues/334) | P2P filled the TV's free space" error when playing large/4K  | ? | bug | consertada-nao-lancada | 2.0.3 | 2.0.3 | c3e19d00, d3ca720b, 8e09dad2 | nós 10-07 | avisar na issue: limite de P2P e limpeza saíram na 2.0.3; janela de streaming (armazenamen |
| [#356](https://github.com/iqui27/nuvio-native-legacy/issues/356) | Upcoming shows not appearing in continue watching | ? | bug | precisa-log | 2.0.3 | 2.0.3 | d47a78c8, 9af09e0c | autor 10-09 | ler o log T5JF2G (09/10) e confirmar a leitura abaixo; se for isso, pedir novo log na 2.0. |
| [#385](https://github.com/iqui27/nuvio-native-legacy/issues/385) | Internet Connection Drop and Reconnecting Bug to no end | Samsung .tpk | bug | respondida | 2.0.3 | 2.0.5 | c29c6f96, 4151683c | autor 10-09 | responder ao autor: não é regressão da 2.0.2, é bloqueio do nó/IP do TorBox (outro aparelh |

Notas:

- **#246**: NOTAS 2.0.3: "LG: resume no longer drops the source on a failed seek (#246)". Não confirmado que cobre o relato HEVC/anime; plataforma do relato desconhecida. ALVO 2.0.3 = SUSPEITA: o conserto está nas NOTAS mas não está confirmado que cobre o relato; falta log do autor. COMMITS vs v2.0.3 (09/10): a7f23592, a8048e0d, 023497dd estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor.
- **#280**: SUSPEITA: único commit que cita #280 é 3c51d5b7 (remontagens da descoberta, 2.0.3); a correção é genérica de fileiras. ALVO 2.0.3 = SUSPEITA: o conserto está nas NOTAS mas não está confirmado que cobre o relato; falta log do autor. COMMITS vs v2.0.3 (09/10): 3c51d5b7 estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor.
- **#334**: Limite de P2P e limpeza integrados em 2.0.3 (d3ca720b, c3e19d00, merge 891a72a9 de agente/203-334b; agente/203-334 tem os mesmos dois commits com outro hash). Só a janela de streaming ("armazenamento rotativo", prometido como planejado: d453fd1c e 8e09dad2 em agente/203-334-janela) NÃO está integrada; alvo 2.0.5 para essa parte. COMMITS vs v2.0.3 (09/10): dentro da tag: c3e19d00, d3ca720b; fora: 8e09dad2, d453fd1c.
- **#356**: d47a78c8 (Cinemeta antes da ficha Nuvio, 2.0.3) cobre uma suspeita, sem confirmação. Config "quanto à frente" planejada 2.0.5 (sem commit). LOG T5JF2G (autor, 09/10): leitura informada pelo coordenador, SUSPEITA e sem conserto: a fileira Continuar Assistindo é montada antes de a lista de vistos da conta chegar e não é refeita depois (o passo "a seguir da conta" nunca aparece no log). Precisa de log da 2.0.3 para confirmar. Alvo 2.0.3 mantido (a tag já saiu: reavaliar para 2.0.4/2.0.5 se o log da 2.0.3 confirmar). COMMITS vs v2.0.3 (09/10): d47a78c8, 9af09e0c estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor. REDDIT hboinay 09/10 (item 5, "próximas temporadas apareceram uma vez e sumiram"): mesmo sintoma; pedir o log da 2.0.3.
- **#385**: LOG LIDO (09/10, registro 64551, 2.0.2): a plataforma é tizen-tpk, NÃO .wgt (formulário errado; a linha [tv] com a versão do Tizen não aparece no trecho). O que o log prova: (1) a fonte escolhida a mão, um link de loja do TorBox via StremThru, não chegou a abrir: o "vídeo de 129 s" NÃO era placa do provedor, era o TRAILER do IMDb ("tpk evento 1 (129036)" vem sempre depois de "[trailer] ... no fundo com som" e aparece até sem episódio aberto). No log 1 o evento de duração do trailer chegou depois de "abrir: url ao pipeline" e o posplay o leu como duração do episódio ("[posplay] duracao suspeita: pipeline diz 129s, catalogo diz 2820s"). CORREÇÃO de leitura de 09/10: a hipótese de placa do provedor está descartada. (2) Com o vídeo aberto, toda conexão nova nossa ao nó do CDN do TorBox foi recusada na hora: 11 vezes "Failed to connect to <nó tb-cdn> port 443: Connection refused" (curl 7), vindas da pré-busca do mkvass (5 Ranges, cada um repetido em conexão nova, "3 conexao(oes) extra(s)", 13 a 15 s) e da sonda do MKV pela rede. (3) Cerca de 5 s depois o player da Samsung perde a conexão dele (ConnectionFailed, 0xfe6c0026), entra na reconexão 1/3 e 2/3 e o Prepare falha. (4) Numa segunda tentativa o player caiu em 5,6 s SEM nenhum pedido nosso antes; então o nó recusa por conta própria, e não está provado que as nossas conexões extras causam a queda. (5) No mesmo log, um add-on https (4KHDHub) abriu e tocou, a pré-busca leu o cabeçalho em 684 ms e parou (3 legendas, nenhuma em coreano). VEREDITO: a recusa é do lado do TorBox/CDN (um nó só, sempre o mesmo). Do nosso lado havia um endurecimento pequeno, não provado como causa: a pausa do leitor lateral da #308 (b65c3324, já na 2.0.3) só disparava com curl 28, 429 e 5xx; conexão recusada (curl 7) continuava sendo repetida 5 vezes em conexão nova com o vídeo aberto, e a sonda do MKV também insistia. Esse endurecimento entrou na 2.0.3 (ver VEREDITO FINAL). Histórico da triagem anterior, feita sem log e achando que era .wgt: PLATAFORMA NÃO CONFIRMADA (.wgt declarado; pode ser .tpk40: um UT8000 com Tizen 5.5 também roda o .tpk 4/5). Perguntar qual arquivo ele instalou (.wgt ou .tpk). Sem log. HIPÓTESE #308 ENFRAQUECIDA (09/10): o autor pôs o idioma da legenda em coreano (nenhuma faixa é escolhida) e "caiu na hora de novo". Lido no código (483a73b8): no .wgt a pré-busca do mkvass NEM EXISTE (player.c:1548, #ifndef __EMSCRIPTEN__) e, sem faixa escolhida, a automática dá LING_AUTO_NADA e o mkvass não colhe nada por Range; o que roda em TODO MKV no .wgt é só a sonda do cabeçalho (video_tizen.c lerMkv -> mkv_faixas_e_caps: um trecho inicial, mais um ou dois Ranges se Tracks/Chapters ficarem fora dele), uma vez por abertura. Logo, no .wgt, sem legenda ligada o app não enche o CDN de pedidos. SE FOR O .tpk40 a conta muda um pouco: lá a pré-busca existe (NV_TPK não é Emscripten) e roda em todo MKV com idioma de legenda definido diferente de "none" (prebuscaCabe, player.c), mas quando nenhuma faixa casa com o idioma (coreano) o fio termina depois do cabeçalho, um Range (mkvass.h); somam-se a sonda do cabeçalho (video_tpk.c) e os capítulos do capmkv (uma janela de 320 KB, 4 s depois de abrir, capmkv.c). Continua sendo meia dúzia de pedidos por abertura, não uma colheita. O que pesa no .tpk é o caso COM legenda: lá toda legenda embutida de TEXTO (não só ASS) vai para o overlay do app e é colhida por Range (faixas.c, FX_TEXTO_OVERLAY=1 só no .tpk), então antes de trocar para coreano a #308 encaixa melhor no .tpk40 do que no .wgt. Resta, sem prova, o bloqueio do CDN do debrid àquele IP/conta ainda valendo de antes. Alvo volta para 2.0.5 (triagem); plano: as três suspeitas da triagem no fim desta nota. Hipótese anterior, mantida para registro: provavelmente coberta pelo conserto da #308 que já está na 2.0.3 (b65c3324 leitor lateral mais gentil: menos Ranges, uma conexão, pausa quando o CDN aperta; 73ce1648). Namer03 comentou (09/10) que a leitura da legenda embutida enche o CDN do TorBox de pedidos; bate com o relato: o CDN do debrid passa a recusar, o vídeo reconecta em laço e TODA fonte de debrid falha, enquanto add-on https segue. CONDIÇÃO: no .wgt o mkvass (leitura por Range) só entra para legenda embutida ASS/SSA (faixas.c: FX_TEXTO_OVERLAY é 1 só no .tpk; SRT/texto embutido a TV desenha), e ele roda no .wgt (tizen.sh compila com NV_ASS_LIBASS, video_tizen.c tem a sonda do MKV). Então só é a #308 se o usuário estava com uma faixa ASS embutida ligada num MKV: perguntar isso (título, se a legenda era embutida, se desligando a embutida a queda some). Não medido quantos Ranges o mkvass fazia no .wgt antes do b65c3324, e nenhum registro .wgt do D1 mostra o laço. Se persistir na 2.0.3, volta para precisa-log com as suspeitas abaixo (plano 2.0.5). Triagem anterior: Samsung UT8000 (Tizen 5.5), .wgt, Nuvio 2.0.2; sem log. Relato: VOD de debrid cai no meio com "reconectando" em laço; depois TODA fonte dá "não deu para carregar", só add-on https segue tocando. Triagem (09/10) no D1 e no código, sem TV: NÃO provado. No D1 (.wgt, ~32 mil ids recentes, ~300 registros) só 2 registros (2 pessoas, ambos 2.0.1) mostram a reconexão de VOD começar ("conexao caiu ... tentativa 1/3"), nenhum chega a "reconexao: desistiu" e nenhum tem o par "queda e depois toda fonte falha"; os outros ~25 registros com PLAYER_ERROR_CONNECTION_FAILED são falha de abertura de uma fonte, e a próxima fonte abriu (sem envenenamento). Código: a reconexão do .wgt (video_tizen.c:1180-1215, video_reconexao.h) são 3 tentativas por queda, e o contador zera 10 s depois do ponto da queda, então rede instável gera laço de "reconectando" por desenho; o 2.0.3 só mexeu na reconexão de TV ao vivo (#302/#350), VOD não mudou. Descartado por leitura: negcache (só 4 APIs de metadados), flag offline (redesaude.c só pinta a ilha), pool de threads (strict=0, [fios] estável nos logs), lista de fontes recusadas (zera a cada lista nova), XHR síncrono (estado por fio). "Só o https funciona" contraria um AVPlay quebrado (os dois passam por ele) e aponta para a conta do debrid/links do host, que é do lado de lá. SUSPEITA, não provada: (1) o open de reconexão não tem prazo: se o prepareAsync nunca responder, video_reconectando() fica 1 e tentarProximaFonteVOD (app.c) volta cedo, então fica "reconectando" sem fim; (2) a op "abrir" do JS faz stop() e close() no MESMO try (video_tizen.c:264-267): se o stop() levantar, o close() não roda e o próximo open() falha; (3) debrid.c marca conta sem plano para a SESSÃO inteira (semPlano), mas só com corpo "PLAN_RESTRICTED/not premium", não com 429. Essas três ficam como plano da 2.0.5 se a 2.0.3 não resolver. VEREDITO FINAL (09/10, segundo log, HSV8YA): NÃO é regressão da 2.0.2. É bloqueio do nó/IP do TorBox: o player falhou com ZERO pedidos nossos no Comet, no StremThru Torz e no Torrentio, e o 4KHDHub tocou. O conserto que saiu na 2.0.3 é a pausa da leitura lateral quando o host do vídeo recusa conexão (agente/203-385, commits c29c6f96 teste e 4151683c conserto; na tag v2.0.3). SUSPEITA ABERTA: a 2.0.2 passou a verificar fontes em paralelo por padrão (45deaa14, 2c399b99), o que segue redirecionamentos dos add-ons até nós do TorBox várias de uma vez; sem prova, a conferir na 2.0.5. BUG SEPARADO (SUSPEITA, alvo 2.0.5): eventos do trailer vazando para a sessão do episódio no .tpk (duração e eventos do trailer lidos como se fossem do episódio, o "duracao suspeita 129s" do log 1); ver o item de roadmap da 2.0.5. COMMITS vs v2.0.3 (09/10): c29c6f96, 4151683c estão na tag; status mantido porque o conserto é suspeita/parcial e sem confirmação do autor.

## 2.0.4 (hotfix em integracao/2.0.3.1, sem tag)

5 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#344](https://github.com/iqui27/nuvio-native-legacy/issues/344) | Live TV search crash | Android | bug | consertada-nao-lancada | 2.0.4 | 2.0.4 | 9d78c75a, 8dd273f3, c9af64b1 | sem comentários | responder com a causa quando o hotfix 2.0.4 sair |
| [#372](https://github.com/iqui27/nuvio-native-legacy/issues/372) | [Bug] Incorrect Season/Episode Metadata Mapping for TV Anime | Samsung .tpk | bug | consertada-nao-lancada | 2.0.4 | 2.0.5 | 038a7dc7, c2112b61, a120b67c | sem comentários | parte 1 (abas de temporada) sai na 2.0.4: responder ao autor com a causa quando sair; part |
| [#392](https://github.com/iqui27/nuvio-native-legacy/issues/392) | Profile switching still rearrange my home screen | Samsung .tpk | bug | consertada-nao-lancada | 2.0.4 | 2.0.4 | bb583b60, 9023ebdb, bdd49eb6 | sem comentários | responder ao autor com a causa quando a 2.0.4 sair |
| [#409](https://github.com/iqui27/nuvio-native-legacy/issues/409) | The source didn't answer in time error even though the serve | Android | bug | consertada-nao-lancada | 2.0.4 | 2.0.4 | - | sem comentários | juntado na integração (2051869e); validar na TCL; responder ao autor |
| [#412](https://github.com/iqui27/nuvio-native-legacy/issues/412) | Playback makes UI extremely laggy | Samsung .tpk | bug | consertada-nao-lancada | 2.0.4 | 2.0.4 | - | sem comentários | juntado na integração (f7a24dc7); validar numa Samsung .tpk (host .NET mudou: precisa inst |

Notas:

- **#344**: CAUSA PROVADA (09/10) por tombstone simbolizado (log AX1R49): leitura fora do limite em buscaFazer (src/guia.c ~3965, EpgProg ps[24] enquanto epg_faixa/xtepg_faixa devolvem o total). Existe desde a v1.7.0, continua na 2.0.3 e vale para todas as plataformas com o guia. Conserto em agente/2031-guiabusca, alvo 2.0.4. Primeira rodada integrada em integracao/2.0.3.1 (merge c9af64b1; teste 9d78c75a falhava com ASan, conserto 8dd273f3); ainda sem tag v*.
- **#372**: CAUSA PROVADA (09/10): regressão 9677065b (v2.0.1, catálogo do Nuvio primeiro para a lista de episódios; o TMDB junta anime longo em 1-2 temporadas: Bleach no Nuvio {1:366,2:50} contra S1-16 no Cinemeta) somada a um defeito antigo: as abas de temporada saem do corpo do Nuvio mesmo quando a lista de episódios é a do add-on (descoberta.c ~7069-7087, detail.c ~742-765). O log BZB857 (2.0.3, webOS) prova. A teoria do autor sobre o Trakt está errada.  Relacionado: #328 (episódios duplicados), mesma raiz (numeração). A parte SIMKL do título não foi tratada. DECISÃO DO DONO (09/10), duas partes. PARTE 1, conserto na 2.0.4: as abas de temporada saem da lista de episódios publicada (não da ficha do Nuvio) e a cauda de enriquecimento do detalhe não repõe abas velhas; integrada em integracao/2.0.3.1 (merge 21077ab5; 038a7dc7, c2112b61, a120b67c, 167310e6, 8eb5ce90), ainda sem tag v*. PARTE 2, 2.0.5, opção (b): lista de episódios na numeração Cinemeta/TVDB mais uma camada de tradução por número absoluto do episódio para o progresso/scrobble do Trakt, a conta Nuvio, o Simkl, os ids de "a seguir"/Continuar assistindo e as marcas de visto (vistonao/vistoep). Como a issue tem um alvo só, o alvo é 2.0.5 (parte aberta). Origem: dono 09/10.
- **#392**: Samsung .tpk Tizen 6.0, v2.0.3 (rawldon, log VV8JG2): ao trocar de perfil e voltar, a ordem das fileiras da Home muda (coleções, catálogos e Continuar assistindo). CAUSA PROVADA pelo log VV8JG2: o build da Home registra os catálogos dos add-ons do perfil anterior no arquivo de ordem do perfil novo, e isso expulsa fileiras. Conserto em agente/2031-ordemperfil (teste bb583b60, conserto 9023ebdb: a lista de add-ons do perfil que saiu não é registrada no arquivo do perfil novo), ainda não integrado. Mesma queixa do #294, que persiste na 2.0.3.
- **#409**: Log 4B891A (Changhong AI PONT, MStar, Android 11, tela 1080p): automático escolhe 4K (VidFast), 15 s sem nenhum evento do player, depois erro 4003 (decodificação). Foto do autor: vídeo tocando atrás da mensagem. Causa provável: troca de fonte abre player novo antes do velho soltar o decoder (NvPlayer.liberar só encolhe a SurfaceView e solta em outra thread; overlay MStar segue visível). Conserto em agente/204-decoder409 (capacidade 4K do decoder, sem repetir codec que falhou, Voltar fecha o erro) + rodada 2 (esperar o release, logs do player). | 10/10: outro usuário (Namer03) lembrou no fio que já existe Reprodução › seleção manual de fonte como contorno.
- **#412**: Log QHQRVE (S95C, .tpk): relógio do player congelado, UI com upd=120 ms por quadro, parar retorna 0 ms mas o player nativo segue tocando. Conserto em agente/204-tpkpreso412 (71c7d615): ASS deixa de segurar o quadro, recupera relógio congelado, parada confirmada antes de abrir outro player. Revisão Codex achou P1 (Voltar durante PrepareAsync encerraria o processo numa TV sã): rodada 2 em andamento.

## Planejado na 2.0.5 (com branch)

11 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#313](https://github.com/iqui27/nuvio-native-legacy/issues/313) | [suggestion] downmix DTS to AC3 5.1 | Samsung | feature | aberta | 2.0.5 (agente/204-samsung-dts, só plano) | 2.0.5 | bf3e71e4, f45ddc14 | autor 10-07 | nada (aguardar fase 1 da 2.0.5); autor se ofereceu para testar |
| [#326](https://github.com/iqui27/nuvio-native-legacy/issues/326) | Add continue watching options | ? | feature | consertada-nao-lancada | 2.0.5 (agente/203-326, agente/203-331) | 2.0.5 | ac424deb, 56868756 | nós 10-07 | corrigir a promessa: dissemos "próxima atualização"; código só em branch 2.0.5 |
| [#329](https://github.com/iqui27/nuvio-native-legacy/issues/329) | Add skip intro, recaps and credits. | ? | feature | consertada-nao-lancada | 2.0.5 (agente/203-331) | 2.0.5 | a3cd9757, 7a69fda5, a0a392fb | sem comentários | responder (sem resposta nossa) |
| [#331](https://github.com/iqui27/nuvio-native-legacy/issues/331) | 💡 Feature Request: Netflix-Style Next Episode Countdown | ? | feature | consertada-nao-lancada | 2.0.5 (agente/203-331) | 2.0.5 | a3cd9757, 9e065265, 4c17e680 | sem comentários | responder (sem resposta nossa) |
| [#338](https://github.com/iqui27/nuvio-native-legacy/issues/338) | Add Stats for Nerds and Custom Buffer Size | all | feature | consertada-nao-lancada | 2.0.5 (agente/203-338) | 2.0.5 | c446b6a1, 8c856535 | nós 10-07 | corrigir a promessa: dissemos "next build"; código só em branch 2.0.5 |
| [#352](https://github.com/iqui27/nuvio-native-legacy/issues/352) | [Feature Request] Library Watched Indicators, Watched/Unwatc | ? | feature | consertada-nao-lancada | 2.0.5 (agente/204-biblioteca-visto) | 2.0.5 | 402e2438 | nós 10-08 | nada (aguardar 2.0.5) |
| [#355](https://github.com/iqui27/nuvio-native-legacy/issues/355) | Option to view full/untruncated text and metadata for addon  | ? | feature | consertada-nao-lancada | 2.0.5 (agente/204-fonte-detalhes) | 2.0.5 | b176eff5 | autor 10-08 | nada (aguardar 2.0.5) |
| [#358](https://github.com/iqui27/nuvio-native-legacy/issues/358) | 🐛 Bug Report: Some Add-on Catalogs Are Incorrectly Moved to  | Samsung .tpk | bug | consertada-nao-lancada | 2.0.5 (agente/204-conta-catalogos) | 2.0.5 | da8fdee7, 6212d5b8, 29c640b7 | autor 10-08 | ler log MD8G2R e responder |
| [#360](https://github.com/iqui27/nuvio-native-legacy/issues/360) | Sync addons | ? | feature | consertada-nao-lancada | 2.0.5 (agente/204-sync-addons) | 2.0.5 | 46507ea5, 6b305295, e2f1eede | autor 10-08 | postar correção |
| [#362](https://github.com/iqui27/nuvio-native-legacy/issues/362) | Collection View Options | ? | feature | consertada-nao-lancada | 2.0.5 (agente/204-colecao-vitrine) | 2.0.5 | 22320582, f49af7d4 | nós 10-08 | nada (aguardar 2.0.5) |
| [#364](https://github.com/iqui27/nuvio-native-legacy/issues/364) | Sources unbable to open | ? | bug | consertada-nao-lancada | 2.0.5 (agente/204-fonte-recusada) | 2.0.5 | bf666aa7, 90abb8aa | sem comentários | responder (sem resposta nossa) |

Notas:

- **#313**: bf3e71e4 (2.0.2) converte DTS->AC3 só no webOS; f45ddc14 (2.0.2) apenas detecta e avisa na tpk. Conversão na Samsung = fase 2 do plano docs/plans/samsung-dts.md (sem código).
- **#326**: PROMESSA SEM CÓDIGO NA 2.0.3: resposta "Both in the next update", mas ac424deb/56868756 só estão em agente/203-326 e agente/203-331 (2.0.5).
- **#329**: Pular intro/recap automático em agente/203-331. 7a69fda5 é de PR externo (pr-351).
- **#338**: PROMESSA SEM CÓDIGO NA 2.0.3: estatísticas ao vivo (e52360ed, agente/203-central/203-veloc) e buffer Android (c446b6a1, agente/203-338) não estão em integracao/2.0.3.
- **#352**: Selo de assistido e ordenar na Biblioteca; menu em árabe separado (futuro).
- **#358**: 29c640b7 em agente/204-conta-catalogos. Commits da8fdee7 e 6212d5b8 citam #358 mas não estão em nenhuma branch local.
- **#360**: Mescla do sync em 2.0.3 (46507ea5, 6b305295); botão "Sincronizar addons" só 2.0.5 (e2f1eede). Namer03 perguntou "on 2.0.3?"; rascunho de correção. Alvo 2.0.5 porque a issue pede o botão "Sincronizar addons" (e2f1eede, fora da 2.0.3); a mescla do sync já está em 2.0.3. COMMITS vs v2.0.3 (09/10): dentro da tag: 46507ea5, 6b305295; fora: e2f1eede.

## Futuro (2.1, 2.2, depois)

3 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#250](https://github.com/iqui27/nuvio-native-legacy/issues/250) | Add Arabic language support | all | feature | respondida | 2.1 | 2.1 | - | nós 10-06 | nada |
| [#260](https://github.com/iqui27/nuvio-native-legacy/issues/260) | Add an optional layout switch to change UI direction from Ri | all | feature | respondida | futuro | futuro | - | autor 10-06 | nada |
| [#343](https://github.com/iqui27/nuvio-native-legacy/issues/343) | [suggestion] anime-skip.com | ? | feature | respondida | futuro (anime-skip.com) | futuro | - | nós 10-08 | nada |

Notas:

- **#250**: Metadados em árabe na 2.0.1; menus em árabe planejados para 2.1 (segundo a resposta em #325).
- **#260**: Layout RTL "planejado para depois"; sem branch.
- **#343**: AniSkip/TheIntroDB já na 2.0.1 (3a02817d); anime-skip.com "na atualização seguinte".

## Aberta sem plano

49 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#135](https://github.com/iqui27/nuvio-native-legacy/issues/135) | Looking for testers: experimental Hisense VIDAA port (Hisens | VIDAA | feature | respondida | - | futuro | faacf493, 08d21c20, a81639d2 | autor 10-06 | responder (feedback do gabo748 em 06/10 sem retorno) |
| [#144](https://github.com/iqui27/nuvio-native-legacy/issues/144) | Align UI with nuvioTV (android) | Android | feature | respondida | - | ja-lancada | e1a31298, b7d10412, 581b2f82 | autor 10-08 | nada (autor disse que não vê mais os problemas; pode fechar) |
| [#252](https://github.com/iqui27/nuvio-native-legacy/issues/252) | Add default audio language setting for anime | ? | feature | por-desenho | - | ja-lancada | - | autor 10-08 | nada (autor confirmou que funciona; pode fechar) |
| [#288](https://github.com/iqui27/nuvio-native-legacy/issues/288) | Profile selection background is grainy dark video | LG? (TCL C6K no formulário) | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#292](https://github.com/iqui27/nuvio-native-legacy/issues/292) | GoogleTV performance UPDATE | Android | question | por-desenho | - | nao vamos fazer | - | sem comentários | nada |
| [#304](https://github.com/iqui27/nuvio-native-legacy/issues/304) | Apple tv dynamic homeacreen | ? | feature | aberta | - | futuro | - | sem comentários | responder (sem resposta) |
| [#306](https://github.com/iqui27/nuvio-native-legacy/issues/306) | Refresh Live TV | ? | feature | aberta | - | 2.0.5 | - | sem comentários | responder (sem resposta) |
| [#307](https://github.com/iqui27/nuvio-native-legacy/issues/307) | Request | iOS | feature | fora-do-escopo | - | nao vamos fazer | - | autor 10-07 | responder (sem resposta; pedido de .ipa) |
| [#310](https://github.com/iqui27/nuvio-native-legacy/issues/310) | [port] regex/options for autoplay | ? | feature | aberta | 2.0.2 | 2.0.5 | sem commit | autor 10-09 | responder: (a) não é mais bug (a escolha manual de outro episódio já aplica o binge group) |
| [#315](https://github.com/iqui27/nuvio-native-legacy/issues/315) | .avi media files fail to play ("Could not open the source") | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#316](https://github.com/iqui27/nuvio-native-legacy/issues/316) | Live tv schedule | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#324](https://github.com/iqui27/nuvio-native-legacy/issues/324) | Relatório de logs | all | meta | por-desenho | - | nao vamos fazer | 83c2cedd | nós 10-08 | nada |
| [#333](https://github.com/iqui27/nuvio-native-legacy/issues/333) | Arabic language | all | feature | duplicada | 2.1 | 2.1 | - | autor 10-07 | responder (apontar #250/#260; sem resposta nossa) |
| [#337](https://github.com/iqui27/nuvio-native-legacy/issues/337) | Be able to hide the "skip" intro button | ? | feature | aberta | - | 2.0.5 | - | sem comentários | responder (sem resposta) |
| [#342](https://github.com/iqui27/nuvio-native-legacy/issues/342) | Add an optional "Skip Scene" floating button for explicit co | all | feature | aberta | - | futuro | - | sem comentários | responder (sem resposta) |
| [#345](https://github.com/iqui27/nuvio-native-legacy/issues/345) | Audio non-existent on startup | Android | bug | aberta | - | 2.0.5 | - | sem comentários | ler log DEPCJ0 e responder |
| [#346](https://github.com/iqui27/nuvio-native-legacy/issues/346) | Continue Watching unselectable | Android | bug | aberta | - | 2.0.5 | - | sem comentários | ler log e responder |
| [#347](https://github.com/iqui27/nuvio-native-legacy/issues/347) | [suggestion] search by microphone | ? | feature | aberta | - | futuro | - | sem comentários | responder (sem resposta) |
| [#348](https://github.com/iqui27/nuvio-native-legacy/issues/348) | Dots y imdb | ? | feature | aberta | - | futuro | - | sem comentários | responder (sem resposta) |
| [#349](https://github.com/iqui27/nuvio-native-legacy/issues/349) | Source Preference Option for MP4 and P2P | ? | feature | aberta | - | 2.0.5 | - | sem comentários | responder (sem resposta); relacionado a #364 |
| [#353](https://github.com/iqui27/nuvio-native-legacy/issues/353) | Hero page trailer only plays once and has no sound | LG | bug | aberta | - | 2.0.5 | - | nós 10-08 | responder / pedir log (sem resposta humana) |
| [#354](https://github.com/iqui27/nuvio-native-legacy/issues/354) | Bring the Full TV Experience to PC - Same App, No Compromise | PC | feature | fora-do-escopo | - | nao vamos fazer | - | sem comentários | responder (sem resposta) |
| [#357](https://github.com/iqui27/nuvio-native-legacy/issues/357) | Nuvio v2.0.1/2.0.2 – Crashes on Zidoo Z9X 8K and Ugoos AM9 P | Android | bug | precisa-log | - | 2.0.5 | - | autor 10-08 | responder: como tirar log sem abrir o app (adb logcat) |
| [#365](https://github.com/iqui27/nuvio-native-legacy/issues/365) | 🐛 Bug Report: Ghost Catalogs Still Remain in “Not on Home Sc | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#366](https://github.com/iqui27/nuvio-native-legacy/issues/366) | [Feature request] Optional setting to hide watched movies fr | ? | feature | aberta | - | 2.0.5 | - | sem comentários | responder (sem resposta) |
| [#367](https://github.com/iqui27/nuvio-native-legacy/issues/367) | [Bug][webOS] Audio control sometimes opens Subtitles on firs | LG | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#373](https://github.com/iqui27/nuvio-native-legacy/issues/373) | [bug] missing info after continue watching | Samsung .tpk | bug | aberta | - | 2.0.5 | - | autor 10-09 | responder (autor deu mais detalhes em 08 e 09/10; pedir log com o código) |
| [#374](https://github.com/iqui27/nuvio-native-legacy/issues/374) | [port] sync subtitles by line "auto sync" | ? | feature | aberta | - | 2.1 | - | sem comentários | responder (sem resposta) |
| [#378](https://github.com/iqui27/nuvio-native-legacy/issues/378) | “From Account” subtitle setting defaults to “NONE” on playba | Samsung .tpk | bug | consertada-nao-lancada | - | 2.0.4 | 28aa5c4b | autor 10-08 | responder ao autor quando a 2.0.4 sair |
| [#379](https://github.com/iqui27/nuvio-native-legacy/issues/379) | Movie or TV show at the end never return to homescreen. | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log (sem resposta) |
| [#382](https://github.com/iqui27/nuvio-native-legacy/issues/382) | No internet error despite having internet | LG | bug | precisa-log | - | 2.0.5 | - | sem comentários | pedir log (o formulário veio sem código); perguntar se a TV está em Wi-Fi ou cabo e se o a |
| [#386](https://github.com/iqui27/nuvio-native-legacy/issues/386) | [bug] glitchy info in playback at the top left when pressing | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir log: o vídeo anexado mostra o defeito; o campo de log veio vazio |
| [#387](https://github.com/iqui27/nuvio-native-legacy/issues/387) | [bug] instant long press in library | Samsung .wgt | bug | aberta | - | 2.0.5 | - | sem comentários | responder pedindo o codigo do log logo depois de acontecer; com o log, conferir pausa long |
| [#388](https://github.com/iqui27/nuvio-native-legacy/issues/388) | Profile picture on side bar is squashed | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder / pedir captura de tela e log |
| [#389](https://github.com/iqui27/nuvio-native-legacy/issues/389) | Card depth effect | Samsung (tpk/wgt?) | feature | aberta | - | futuro | - | sem comentários | responder: pedir exemplo do efeito de profundidade desejado (sombra, inclinação, escala no |
| [#390](https://github.com/iqui27/nuvio-native-legacy/issues/390) | Custom poster source not setting | Samsung .tpk | bug | consertada-nao-lancada | - | 2.0.4 | beab28ec | sem comentários | responder ao autor quando a 2.0.4 sair |
| [#393](https://github.com/iqui27/nuvio-native-legacy/issues/393) | [Feature Request] Option to change the library source from T | ? | feature | aberta | - | 2.0.5 | - | sem comentários | sugestao: perguntar ao autor qual tela e qual TV (a Biblioteca já lista o Simkl em Listas  |
| [#394](https://github.com/iqui27/nuvio-native-legacy/issues/394) | ASS subtitles: lag on large tracks, shadow/color shift, occa | Samsung .tpk | bug | aberta | - | 2.0.5 | - | nós 10-09 | investigar renderizacao libass no .tpk (sombra/anel, cor, lentidao em faixa grande, dessin |
| [#397](https://github.com/iqui27/nuvio-native-legacy/issues/397) | [Feature Request] Playback Engine Robustness: Native Codec O | LG | feature | respondida | - | 2.1 | - | sem comentários | postar o rascunho (EN, nas notas): o que já existe e o caminho; o que falta (atraso do áud |
| [#400](https://github.com/iqui27/nuvio-native-legacy/issues/400) | feature-request: allow using default addon sources sort orde | all | feature | consertada-nao-lancada | - | 2.0.5 | a46e1a4c | sem comentários | responder: hoje nao existe; opcao "manter a ordem do add-on" na aba de cada add-on, para a |
| [#401](https://github.com/iqui27/nuvio-native-legacy/issues/401) | Long update time on Homebrew channel (15 mins +) | LG | bug | respondida | - | 2.0.5 | - | nós 10-09 | conserto na 2.0.5 (dados fora da pasta do app + limpeza de sobras; strip do binário) |
| [#402](https://github.com/iqui27/nuvio-native-legacy/issues/402) | Wrong sorting in sources | Samsung .tpk | bug | consertada-nao-lancada | - | 2.0.4 | ca8f29b4 | sem comentários | responder ao autor quando a 2.0.4 sair |
| [#403](https://github.com/iqui27/nuvio-native-legacy/issues/403) | feature-request: Add a next button to go to next episode | all | feature | aberta | - | 2.0.5 | - | sem comentários | postar o rascunho (EN); botao Proximo na fileira de controles do player entra no item do p |
| [#404](https://github.com/iqui27/nuvio-native-legacy/issues/404) | Continue watching mixes up | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | responder pedindo log + fonte do Continuar + um título que não deu play; investigar na 2.0 |
| [#405](https://github.com/iqui27/nuvio-native-legacy/issues/405) | Quick search doesn't list all results | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | ler o log ABB125 e comparar a busca rapida (canal -) com a busca do menu para "Silo" |
| [#406](https://github.com/iqui27/nuvio-native-legacy/issues/406) | Passing through ratings on title page | Samsung .tpk | feature | aberta | - | 2.0.5 | - | sem comentários | decidir com o dono: notas da pagina do titulo como um bloco so na navegacao vertical |
| [#407](https://github.com/iqui27/nuvio-native-legacy/issues/407) | Top menu open when scrolling up on the title page | Samsung .tpk | bug | aberta | - | 2.0.5 | - | sem comentários | reproduzir no .tpk: cima a partir do Play na pagina do titulo abre o menu do topo (o mesmo |
| [#410](https://github.com/iqui27/nuvio-native-legacy/issues/410) | Settings interface lower resolution; glass, depth, edge glow | Samsung .tpk | bug | consertada-nao-lancada | - | 2.0.4 | cc8bbdcd | sem comentários | juntar a rodada 4 do nível de GPU quando o Codex liberar; responder ao autor; menu em 4K n |
| [#411](https://github.com/iqui27/nuvio-native-legacy/issues/411) | Samsung "Not Available" toast on Play/Pause (Tizen 4/5) | Samsung .tpk | bug | consertada-nao-lancada | - | 2.0.4 | a066e6c8 | sem comentários | gerar .tpk Tizen 4/5 da 2.0.4 para o relator testar (precisa OK do dono) |

Notas:

- **#135**: Porte experimental VIDAA; issue de chamada de testadores. Commits faacf493 (2.0.2) e feat/vidaa.
- **#144**: Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#252**: Opção já existe: Ajustes > Idiomas e legendas > Idioma do áudio. Alvo ja-lancada: a opção já existe.
- **#288**: Formulário diz LG webOS mas o modelo é TCL C6K: plataforma incerta.
- **#292**: Aviso nosso (virou issue a partir da discussão #291): resolução da interface em 4K deixa o Android lento. Alvo nao vamos fazer: é aviso/pergunta, não há mudança de código planejada (ver decisões).
- **#307**: SUSPEITA de fora-do-escopo: pedido de .ipa, app é C/SDL para TVs. O dono decide.
- **#310**: REABERTA em 09/10 pelo autor (Namer03) com novo comentário; no GitHub o estado é aberta (motivo REOPENED), então a issue volta ao mapa como aberta com alvo. A regex/exigir/preferir da 2.0.2 continua entregue (sem commit #310). Três pedidos novos: (a) JÁ NÃO É BUG (revisão de código em 09/10): a abertura manual de outro episódio já aplica o binge group (teste 29d2c248 passa sem conserto); a lacuna real era que só a fonte ESCOLHIDA À MÃO era lembrada. FEATURE 2.0.5, origem dono 09/10 ("sim vamos adicionar na 2.0.4"; a 2.0.4 planejada foi renumerada para 2.0.5 em 09/10): lembrar também o binge group da fonte que o automático tocou. Branch agente/204-binge (29d2c248, 02753401, 6345e9ff), ainda não integrado. (b) FEATURE, alvo 2.1: "reusar último link" + "Last Link Cache Duration". Não existe cache de link: fontepref guarda só a identidade da fonte (bingeGroup, provedor, trilha) por título e perfil, por 180 dias, e resolve a URL de novo (fontepref.h:112-140 diz isso de propósito); fontecache é prefetch de lista com validade de 30 s (fontecache.h:166, FONTECACHE_VALIDADE_MS). Guardar a URL exige arquivo novo, validade configurável, linha nos Ajustes e queda para a busca quando o link de debrid (assinado, expira em minutos) falhar: médio, com risco de tocar link morto; por isso 2.1. (c) FEATURE, alvo 2.1: regex separada por tipo (filmes / séries / anime) e modo preferir x exigir por tipo. Hoje há UMA regex e UM modo para tudo: padrao único em fonteregra.c:15, FonteRegraCfg com um regexModo (fonteregra.h:47), um único modo em ajustes.c:1301 (V_FONTE_REGEX), um fonteregra.txt por TV com cópia por perfil, e a chave do oficial stream_auto_play_regex é uma só na conta (sync). Por tipo pede três regex compiladas, três modos, arquivo e blob novos sem quebrar o sync com o oficial, três linhas a mais nos Ajustes e uma regra para dizer o que é anime (o app não tem esse tipo hoje: suspeita, não conferido): grande; por isso 2.1. ALVO da issue = 2.0.5 (o mais cedo, pelo item (a), agora recurso); (b) e (c) só saem na 2.1.
- **#324**: Relatório automático de triagem de logs (comentários nossos); não é bug. Alvo nao vamos fazer: não é bug nem pedido.
- **#333**: Duplicata de #250 (Arabic). Comentário no commit c824675c (v1.0.1) é falso positivo.
- **#345**: Log code no corpo; sem resposta nossa.
- **#346**: Sem resposta nossa. Possível relação com #373 (Samsung).
- **#353**: Os 2 comentários nossos na issue são relatórios de triagem automática, não resposta ao autor. REDDIT hboinay 09/10 (itens 9 e 10): (a) tocar uma vez por título e sessão é DECISÃO DO DONO de 30/09 (home.c:460-466, heroTrailerJaTocou em home.c:4588); (b) o trailer iniciado com o foco num cartaz (Trailer do cartaz em foco) abre sempre mudo, o som só vale com o foco no destaque (home.c:4646 focoHero && ajustes_trailer_hero_som()), comentário "o do cartaz em foco segue mudo". Falta decidir se o som segue o ajuste também no cartaz.
- **#354**: SUSPEITA: versão desktop está fora do escopo (app é para TVs).
- **#357**: Zidoo Z9X 8K (Android 11) fecha ao abrir; Ugoos AM9 Pro. Autor comentou que não achou o aparelho no relatório #324 e não consegue enviar log.
- **#365**: Autor diz que o problema principal da Home foi resolvido; fantasmas permanecem em "Fora da Home". Relacionado a #358.
- **#373**: Namer03 (09/10): o botão azul do controle abre o menu de retomar, mas no controle novo da Samsung é difícil de alcançar, então ele mantém o pedido de um jeito mais fácil de voltar ao player, mantendo a ilha. Ao escolher "Retomar", o app recarregou a fonte em vez de retomar, ou o clique não registrou. Suposição do autor, sem prova: "manter o vídeo pronto ao sair" pode estar quebrado (08/10: voltar pelo caminho início > cartaz > ver título > retomar exigiu reabrir a fonte). Sem log lido por nós; precisa de mais testes e logs.
- **#379**:  09/10: .tpk S90C, 2.0.2: no fim do filme/episodio o player fica pausado no ultimo segundo em vez de voltar. SUSPEITA: o evento de fim (tpk evento 4) nao chega ou chega antes do ultimo quadro; conferir no log as linhas [video] tpk evento perto do fim.
- **#382**: LG 50UT8050PSB, Nuvio 2.0.2 (.ipk). Relato: depois de ligar a TV a frio aparece "sem internet" mesmo com internet, some, volta várias vezes até parar de vez. O campo de log ficou sem código. SUSPEITA, sem prova: o aviso nasce do teste de rede do app (redesaude) logo após o boot, quando a rede da TV ainda sobe; não lido no código nem em log.
- **#386**: S95C, .tpk, Nuvio 2.0.2, autor Namer03. Passos: abrir vídeo, mostrar a interface do player, apertar esquerda/direita; informação "glitchada" no canto superior esquerdo (ver vídeo anexado, não assistido por nós). Sem log. Causa desconhecida.
- **#387**: S95C, .wgt, Nuvio 2.0.2, autor Namer03. Aleatório, sem padrão exato: na Biblioteca, ao abrir um dos 3 primeiros títulos, o item é selecionado na hora (como um toque longo) em vez de abrir a ficha. Sem log. SUSPEITA, não lida no código: o OK sendo lido como segurar (repetição de tecla do controle novo da Samsung); conferir o tratamento de toque longo da Biblioteca. LIDO 09/10 (biblioteca.c:957-971 e 1079): KEYDOWN arma okDesde com SDL_GetTicks() e biblioteca_atualizar abre o menu quando passam NV_HOLD_MS sem KEYUP processado. SUSPEITA: no .wgt o fio principal trava quando os primeiros cartazes decodificam (longtasks de varios segundos ja medidas no Tizen web); o KEYUP chega atrasado e o limiar vence antes dele, por isso acontece nos 3 primeiros. Conserto possivel (2.0.5): nao disparar o segurar no quadro que veio depois de uma pausa longa, deixando os eventos pendentes chegarem; o mesmo padrao existe em home.c, busca.c, ctxmenu.c, avisos.c, entao o conserto deve ficar num ajudante comum. VIDEO do autor (9 s, visto 09/10): em sequencia, OK em 4 cartazes da fileira de cima de Salvos; em 3 deles o menu do cartaz (Remove from Saved / Mark as watched...) abre em menos de ~0,5 s, com as capas ja carregadas; no 4o (Horimiya) abre a ficha. Isso ENFRAQUECE a suspeita da pausa longa (as capas ja estavam prontas) e aponta para o KEYUP do OK nao ser reconhecido no .wgt (codigo de tecla do keyup diferente do keydown, ou keyup que nao chega), o que faria todo OK virar segurar. Precisa do log com as linhas de tecla.
- **#388**: QN70F, Tizen 9.0, .tpk, Nuvio 2.0.2 (escrito "2.02"). A foto do perfil na barra lateral aparece achatada. Sem log nem imagem. Não investigado no código.
- **#389**: Pedido de efeito de profundidade em cartões, trailers e elenco. Não existe opção com esse nome. SUSPEITA de alvo: futuro, sem plano nem aprovação do dono; pode ser ligado à Biblioteca/Glass UI da 2.1.
- **#390**: QN74F, Tizen 9.0, .tpk, Nuvio 2.0.2 (escrito "2.02"). Ao colocar a URL de cartaz própria (pelo celular e à mão) e apertar "Concluído", volta para as opções sem gravar a URL. Mesmo autor do #388 e do #389. Não investigado. Relação com a URL de cartaz longa do #361 (2.0.3) não conferida: SUSPEITA.
- **#393**: Issue de uma linha, sem texto. O que existe hoje (09/10): a Biblioteca tem três modos (Salvos, Coleção, Listas; biblioteca.c:267-269) e em Listas as fontes Trakt, Simkl, Nuvio, Fixadas e Públicas (biblioteca.c:282-284, lst_pedir LST_SIMKL em :685); "Coleção" só existe no Trakt. O que o "+" salva é Ajustes › Conta e serviços › Trakt e Simkl › "Onde o + salva" (AJ_SALVOS_DEST, ajustes.c:1105; destinos Esta TV/Trakt/Simkl), e o Plan to Watch do Simkl aparece como "SIMKL" na aba Salvos (biblioteca.c:2055); o "Continuar assistindo" tem "Fonte do Continuar assistindo" (Todas as fontes, Conta Nuvio, Trakt, Simkl; ajustes.c:711, 1034). Não há UMA chave "fonte da biblioteca" que troque a aba Salvos inteira do Trakt para o Simkl: se é isso que o autor quer, é pequeno (a aba Salvos passar a seguir AJ_SALVOS_DEST), mas o pedido pode já estar coberto pelas Listas › Simkl. Sugestão de alvo: 2.0.5, depois de o autor dizer qual tela e qual aparelho. Sem commit.
- **#394**: Aberta pelo dono em 09/10 ao fechar a #308: depois da 2.0.3 o Namer03 (.tpk S95C) viu as legendas ASS funcionando, mas com sombra/anel e cor alterada em relacao ao original, lentidao (fps baixo) em legenda grande/avancada e um clipe fora de sincronia. Causa nao investigada (suspeita: renderizacao libass/composicao no .tpk). Alvo 2.0.5.
- **#397**: Pedido em quatro partes (formulário diz LG webOS / All; o texto fala de Android/Fire TV). Caminhos conferidos NO CÓDIGO (09/10), sem prova em TV. (a) HDR/Dolby Vision e passthrough: JÁ HÁ o que o app controla. Preferência de fonte em Ajustes › Reprodução › "Dolby Vision e HDR" (AJ_FONTE_HDR) e "Preferir som Dolby Atmos" (AJ_ATMOS, ajustes.c:4452-4453); "Dolby Vision em MKV (experimental)" só LG webOS 4+, desligado por padrão (AJ_DV_MKV, ajustes.c:5399): na 2.0.3 troca TrueHD por E-AC-3/AC-3 no idioma ouvido para manter o Dolby Vision (docs/releases/2.0.3/NOTAS.md:17). "Converter DTS para a TV ouvir" (AJ_DTS_AC3, ajustes.c:4464): só LG, converte o DTS em estéreo (AAC) ou 5.1 (Dolby Digital) via src/dts/; na Samsung o app só AVISA quando a TV recusa DTS/TrueHD (video_tpk.c:130-141, #313; esconder as faixas é 2.0.5, fase 1). LIMITADO PELA TV/PLATAFORMA, não dá para prometer: na LG e na Samsung quem decodifica HDR/DV e faz o passthrough é a TV (uMS / AVPlay / player .tpk), o app só escolhe a fonte e o caminho; no Android o Media3 usa o passthrough por AudioCapabilities (NvPlayer.kt:374-386; video_android.c:27) e com bitstream o app não mexe no áudio (sem reforço de volume, sem velocidade: video_android.c:260, 836-841). Não há "robustez garantida" a implementar sem um caso de TV com log. (b) Estilo da legenda na hora: JÁ EXISTE no player. Botão Legendas (PLR_CC, player.c:189) › aba "Estilo" (legendasui.c:824; faixas.c:1489, corpo faixas.c:533-600), com Tamanho (50% a 250%, de 10 em 10, padrão 120%, faixas.c:590), Fonte OpenSubtitles, Cor, Opacidade (100/75/50/25%), Fundo (Nenhum, Escuro 25/50/75/100%), Posição (1 a 8), Borda (Nenhuma/Contorno/Sombra), Atraso, Negrito (Ligado/Desligado) e Restaurar padrão; OK cicla o valor e aplica na hora. Ressalvas: com ASS desenhado pelo app, fonte/cor/fundo/posição/borda/negrito ficam "Preservado pelo ASS" (faixas.c:536-541) e o tamanho vira escala; o negrito vale só para legenda externa/OpenSubtitles, a faixa que a TV desenha segue o peso da TV (faixas.c:571-572). Os valores são cíclicos (sem slider). (c) Atraso: legenda JÁ EXISTE: Legendas › linha "Atraso" (LR_ATRASO, legendasui.c:671-676), régua de -10 s a +10 s, 0,1 s por toque e 0,5 s segurando a seta (legendasui.c:444-451); a legenda secundária tem o seu; e "Sincronização automática" (AutoSync, linha LR_SYNC) que acerta pelo áudio (Ajustes "Sincronizar pelo áudio", AJ_LEG_SYNC_AUDIO, só onde o player entrega PCM, hoje Android: audsync.h:1-6). ATRASO DO ÁUDIO NÃO EXISTE (sem backend nem linha: grep audio_atraso/audioDelay vazio). (d) Proporção JÁ EXISTE: botão Proporção (PLR_ASPECTO) ou tecla 0, 8 modos: Original, Recortar, Esticar, Zoom leve, Zoom cinema, Zoom ultra, Ajustar altura, Ajustar largura (player.c:877-881, player.h:218-226); padrão em Ajustes › Reprodução › "Proporção padrão" (AJ_PROPORCAO_PADRAO). Em TVs que não recortam a imagem os modos de zoom caem para Original (player.c:1173); o .wgt Samsung (AVPlay/WebAssembly) não tem o zoom que corta a barra preta. Velocidade JÁ EXISTE: folha de Áudio, linha "Velocidade" (faixas.c:1448; "Playback speed" no i18n), 0,75x 1x 1,25x 1,5x 1,75x 2x (velocidade.c:7), por vídeo; disponível na LG, no .tpk e no Android; AUSENTE na Samsung .wgt (AVPlay só tem trick play inteiro, video_tizen.c:1531-1539) e bloqueada no Android com áudio em passthrough ("Indisponível com áudio pelo receptor", faixas.c:1449, #202). Salto de QUADROS (frame step/skip) NÃO EXISTE (só saltos de 10/30/60/120 s); o pedido é ambíguo (passo de quadro ou pular quadros?). FALTA, sugestao: atraso do áudio na 2.1 (começar onde o backend deixa, Android/Media3; LG e Samsung podem não ter API, a confirmar); passo de quadro em futuro (precisa de pausa + avanço por quadro nos 4 backends, sem prova de que a TV deixa); faixa de velocidade mais larga (0,25x-3x) e slider de tamanho/opacidade da legenda em 2.0.5 se o dono quiser (pequeno). Alvo geral = o mais cedo entre as peças que faltam (atraso do áudio, 2.1). Origem: pedido do autor somprakashpathakoneplus-cell. Rascunho (EN): The subtitle style (size up to 250%, color, opacity, background, bold), the subtitle delay ruler (-10 s to +10 s) and AutoSync are already in the player under Subtitles, tab Style; aspect modes are on the Aspect ratio button (or key 0) and playback speed (0.75x-2x) is on the Audio sheet. Audio delay and frame stepping are not there yet and are on the roadmap, while HDR, Dolby Vision and passthrough depend on what each TV or player decodes, so we can only choose the source and path. Which device are you on, so we know which of these you are missing?
- **#400**: Vidhin05 (tambem a discussao #376): quer a ordem do AIOStreams (ranked stream expressions). Hoje a lista sempre agrupa por resolucao e ordena do maior arquivo para o menor, uma aba por add-on (texto de ajuda em idioma_tab.h). As opcoes de ordem existentes valem so para o automatico.
- **#401**: SUSPEITA nova (09/10, relato do autor: 'Updating 1%..99%' lento, instalação nova da 2.0.2 em segundos): dados do app dentro da pasta do app (.nuvio 359 MB/3980 arquivos na C9, incluindo cache.suspeito-1853 de 195 MB nunca apagado). LG C5 webOS 26, 2.0.32 para 2.0.3 pelo Homebrew Channel, 15+ min. NAO e regressao de tamanho (ipk 2.0.3 61,1 MB/1218 arquivos; 2.0.2 60,7 MB/1215). MEDIDO no ipk 2.0.4 (58,3 MB, 1190 arquivos, 99,5 MB descomprimido): o binário nuvio-proto tem 55,3 MB e VEM COM debug_info e símbolos (file: not stripped), 30,8 MB de seções .debug*/.symtab/.strtab que gzip reduz a 10,5 MB; a arte (art/) tem 39 MB em 1142 arquivos, já comprimida (JPG/PNG/WebP). Estimativa (nao medida): strip tira ~10 MB do ipk (58,3 para ~48 MB). O Homebrew Channel baixa o ipk inteiro para /tmp, confere sha256 e chama appInstallService/dev/install (services/service.ts); a barra de download usa o content-length do servidor, nao o ipkSize (tools/hb-repo.sh ja grava ipkSize). Onde estao os 15 min (download do GitHub pela TV, gravacao em /tmp, ou appInstallService instalando 1190 arquivos) NAO foi medido: SUSPEITA. Pedir ao autor a duracao de cada fase (a barra mostra Downloading/Verifying/Installing). Alvo 2.0.5.
- **#402**: Namer03, S95C, .tpk 2.0.3. A imagem mostra, na aba NMR, o cabecalho do grupo "Other SDR" com fileiras cujo nome (formatter do addon) diz "FHD | REMUX | SDR" e "FHD | SDR"; so a ultima diz "N/A | SDR" (essa realmente nao declara resolucao). CAUSA (lida no codigo, sem teste ainda): o grupo vem de grupoRes (src/streams.c:2105) que usa s->altura ou os selos r-4k/r-1080/r-720; a altura sai de stream_parse.c:281-283, que so procura os numeros 2160/1440/1080/720/480 e 4k/uhd, e badges_detectar (badges.c:40-42) so tem 1080p/1080i/1080. A palavra FHD sozinha (comum em formatter de AIOStreams) nao casa, entao altura=0 e o grupo e Outras (GRUPO_NOME, streams.c:2072). O app ja conhece FHD para canal ao vivo (nv_res_do_texto, livetv_regras.h:38) mas nao usa no filme/serie. NAO e ordenacao (nivelHdr, MP4 primeiro da LG e perfil 5 nao entram: e a Samsung .tpk e o problema e o grupo, nao a ordem dentro dele). Nao e configuracao (fonteregra.h trata auto-play, nao grupos). Cuidado no conserto: nv_res_do_texto aceita "hd" solto como 720, o que casaria DTS-HD; copiar so fhd/fullhd/full hd. Candidato a 2.0.4 (muda so o parser, 2 linhas + teste), decisao do dono. SUSPEITA: o JSON bruto do autor pode ter a resolucao em outro campo; sem o log/JSON da fonte nao foi provado.
- **#403**: Existe em parte: o cartao de proximo episodio (posplay.c, player_regra_proximo player.c:2192) so sobe nos creditos/ultimos segundos (marcador de creditos ou intro_fim_estimado, 50 s) e no fim; se o autor dispensa com Voltar ou pula os creditos, a dispensa gruda (posplay.c:104 e 257) e o unico caminho e o botao Episodios da fileira de controles (player.c:2846-2851, abre a lista). Nao ha botao Proximo na fileira de controles, nem durante a reproducao fora da janela. Codigo comum a todas as plataformas (LG, Samsung, Android); canal ao vivo tem Canal +/- (AV_B_PROX), nao conta. Conferido no codigo, sem prova em TV.
- **#404**: Namer03, S95C, 2.0.3: Continuar assistindo registra títulos que ele não deu play; ele acha que é o debrid ou os amigos. SUSPEITA: debrid não escreve no Continuar; vem da conta Nuvio (outros apps/aparelhos) ou do Trakt/Simkl (qualquer app ligado). Sem log.
- **#405**: mackojanko, QE65Q80A, 2.0.3: a busca rapida nao mostra Silo (2023); a busca do menu mostra. Log ABB125. Sem causa ainda.
- **#406**: mackojanko: subir/descer na pagina do titulo passa nota por nota; pede que as notas sejam um bloco unico.
- **#407**: mackojanko, QE65Q80A, 2.0.3: acima do Play o foco abre o menu do topo; ele acha que nao deveria abrir. Sem log.
- **#410**: Namer03, S95C, 2.0.3. Diz que a tela de Ajustes ficou com resolução mais baixa, o vidro quase não aparece, o contorno do vidro não faz nada, Profundidade e as opções dela (brilho de borda, cobertura) mudam pouco, e o Reflexo (sheen) a 0% ainda deixa o brilho forte no cartaz em foco (fotos no issue). Sem log. SUSPEITA não lida: o .tpk pode estar com nível de GPU mais baixo (gpu-nivel) que desliga passadas de vidro, ou a renderização dos Ajustes em textura menor. DONO 09/10: regressão da 2.0.3, entra na 2.0.4. | 10/10, autor depois de testar: efeitos completos + menu 4K não melhoraram a nitidez dos Ajustes (bate com o achado de que o .tpk enxerga a TV 4K como 1920x1080: não é regressão da 2.0.3, vai para a 2.0.5); os ajustes recomendados melhoraram o vidro da Home; Profundidade e os outros quase não mudam ligado/desligado (conferir na TV com o conserto cc8bbdcd, que só entra na 2.0.4). Opinião: visual "menos moderno" que o anterior, organização melhor.
- **#411**: Q7FN Tizen 4. Host 4/5 passa a reservar teclas de mídia (keygrab TOPMOST via ecore_wayland/ecore_wl2, varre janelas 0-63) — merge a066e6c8. Relator sugeriu ElmSharp WinKeyGrab/eext_win_keygrab_set e se ofereceu para testar numa Q7FN.

## Já lançado

268 issues.

### Lançadas e ainda abertas no GitHub (36)

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#266](https://github.com/iqui27/nuvio-native-legacy/issues/266) | Bug: Doesn't open on ATv | Android | bug | lancada | 2.0.3 | 2.0.5 | eabe3afd, b03aa2f0, 35421d1a | autor 10-08 | responder (confirmou QR com teste-318.x; nova queixa: legenda diferente do Nuvio oficial) |
| [#269](https://github.com/iqui27/nuvio-native-legacy/issues/269) | Embedded subtitles are not being detected or not being displ | Samsung (tpk/wgt?) | bug | lancada | 2.0.3 | ja-lancada | 80569112, 6ec3907b, b35bcbcf | autor 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#283](https://github.com/iqui27/nuvio-native-legacy/issues/283) | Minha tv lg nao consegue abrir nenhum canal dos meus addons | LG | bug | lancada | 2.0.2 | 2.0.3 | 1424e409, 6de601a5, b68bad96 | autor 10-07 | ler log MWASFG e responder |
| [#284](https://github.com/iqui27/nuvio-native-legacy/issues/284) | Source result ("Best for this tv") missing resolution | ? | feature | lancada | 2.0.3 | ja-lancada | df492356, a63ec982 | sem comentários | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#286](https://github.com/iqui27/nuvio-native-legacy/issues/286) | Performance on older Samsung UA40N5300 | Samsung .tpk | bug | lancada | 2.0.3 | 2.0.5 | 325ed7a0, e03080dd | nós 10-08 | postar correção |
| [#287](https://github.com/iqui27/nuvio-native-legacy/issues/287) | Auto select forced embedded sub | ? | feature | lancada | 2.0.2 | ja-lancada | 0702a143 | autor 10-07 | nada (autor confirmou; pode fechar) |
| [#290](https://github.com/iqui27/nuvio-native-legacy/issues/290) | Few minor issues on 2.1 | ? | bug | lancada | 2.0.2 | ja-lancada | 75cee76f, 4843e5b0, b5deeb25 | autor 10-06 | nada (pedir confirmação se quiser) |
| [#293](https://github.com/iqui27/nuvio-native-legacy/issues/293) | Audio codec details on player | ? | feature | lancada | 2.0.2 | ja-lancada | 8c856535, db1f5891 | nós 10-07 | nada |
| [#294](https://github.com/iqui27/nuvio-native-legacy/issues/294) | 🐛 Home collection order resets after switching profiles | ? | bug | lancada | 2.0.3 | 2.0.4 | 66e6ae9e, 26d845c6, abd0770a | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#296](https://github.com/iqui27/nuvio-native-legacy/issues/296) | Si podrías agregar estas visitas seria grandioso | Samsung .wgt | feature | lancada | 2.0.2 | ja-lancada | sem commit | autor 10-07 | nada (pode fechar) |
| [#298](https://github.com/iqui27/nuvio-native-legacy/issues/298) | The Arabic subtitles | Samsung (Tizen 6) | feature | lancada | 2.0.2 | ja-lancada | sem commit | autor 10-07 | responder (autor informou Tizen 6; dizer em qual versão testar) |
| [#300](https://github.com/iqui27/nuvio-native-legacy/issues/300) | Default Aspect Ratio Option | ? | feature | lancada | 2.0.2 | ja-lancada | sem commit | nós 10-07 | nada (pode fechar) |
| [#302](https://github.com/iqui27/nuvio-native-legacy/issues/302) | Live TV issue persists with 2.1 tpk65 | Samsung .tpk | bug | lancada | 2.0.3 | 2.0.5 | 6c4d3863, 8fea5018, 0021b573 | nós 10-08 | postar correção |
| [#303](https://github.com/iqui27/nuvio-native-legacy/issues/303) | how to remove continue watching from opening screen | ? | feature | lancada | 2.0.2 | ja-lancada | 1300a834 | nós 10-07 | nada (pode fechar) |
| [#305](https://github.com/iqui27/nuvio-native-legacy/issues/305) | Hide player ui when pressing up | ? | feature | lancada | 2.0.3 | ja-lancada | 98bee79f | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#312](https://github.com/iqui27/nuvio-native-legacy/issues/312) | few minor bugs, none affect use | LG | bug | lancada | 2.0.3 | ja-lancada | a86e55c2, 317cb179 | nós 10-08 | nada (correcao publicada 09/10: Dolby Vision em MKV vem DESLIGADO por padrao); pode fechar |
| [#319](https://github.com/iqui27/nuvio-native-legacy/issues/319) | Home row still not updating properly | ? | bug | lancada | 2.0.3 | ja-lancada | 66e6ae9e, 3c51d5b7, 0fcfa601 | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#320](https://github.com/iqui27/nuvio-native-legacy/issues/320) | Arabic language in Subtitle shows no glyph font | Samsung .tpk | bug | lancada | 2.0.3 | ja-lancada | 22ecac46, 8ab3b78f | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#321](https://github.com/iqui27/nuvio-native-legacy/issues/321) | The "not started" label does not shift downwards. | Samsung .tpk | bug | lancada | 2.0.3 | ja-lancada | b347004c | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#322](https://github.com/iqui27/nuvio-native-legacy/issues/322) | Can't delete from Continue watching on the home screen | Samsung .tpk | bug | lancada | 2.0.3 | ja-lancada | b8d110de | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#323](https://github.com/iqui27/nuvio-native-legacy/issues/323) | App crashes on playback | Android | bug | lancada | 2.0.3 | ja-lancada | 1b6c9a89, 696afa74 | autor 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#328](https://github.com/iqui27/nuvio-native-legacy/issues/328) | 🐛 Bug Report: Episodes Are Being Duplicated | ? | bug | lancada | 2.0.3 | 2.0.5 | 9c5d0027, 7edab4b0 | sem comentários | responder ao autor; reavaliar com a parte 2 do #372 (2.0.5); o conserto de duplicados da 2 |
| [#330](https://github.com/iqui27/nuvio-native-legacy/issues/330) | [Bug]extremely laggy after repeated playback | ? | bug | lancada | 2.0.3 | ja-lancada | 2f047898 | sem comentários | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#332](https://github.com/iqui27/nuvio-native-legacy/issues/332) | Nuvio didn't finish opening | Android | bug | lancada | 2.0.3 | ja-lancada | eabe3afd, b03aa2f0, 35421d1a | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#335](https://github.com/iqui27/nuvio-native-legacy/issues/335) | Extend Arabic subtitle size scale (200%-250%), ASS support,  | Samsung (tpk/wgt?) | feature | lancada | 2.0.3 | ja-lancada | 01016036, 531e344a, bbc31cb2 | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#339](https://github.com/iqui27/nuvio-native-legacy/issues/339) | Pantalla de ajustes | ? | feature | lancada | 2.0.3 | ja-lancada | 607b25bf, b1b53000, 30e13fa4 | nós 10-07 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#340](https://github.com/iqui27/nuvio-native-legacy/issues/340) | Quicker Seek/skipping | ? | feature | lancada | 2.0.3 | ja-lancada | bc63d8ca | sem comentários | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#341](https://github.com/iqui27/nuvio-native-legacy/issues/341) | Aspect Ratio / Crop feature to fill screen does not work on  | Samsung .tpk | bug | lancada | 2.0.3 | ja-lancada | beb5007a | sem comentários | responder (sem resposta nossa; pedir teste) |
| [#350](https://github.com/iqui27/nuvio-native-legacy/issues/350) | Major bug | Samsung .tpk | bug | lancada | 2.0.3 | 2.0.5 | 4b8ba24b | autor 10-08 | postar correção |
| [#361](https://github.com/iqui27/nuvio-native-legacy/issues/361) | Poster URL Max Character Length Too Short | all | feature | lancada | 2.0.3 | ja-lancada | 9ac3531d, 2220b019, 81f7128b | sem comentários | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#363](https://github.com/iqui27/nuvio-native-legacy/issues/363) | Unable to Send Recommendation to a Friend | Android | bug | lancada | 2.0.3 | ja-lancada | 54cf2fab, 9bb93887, 08e58b14 | autor 10-08 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#368](https://github.com/iqui27/nuvio-native-legacy/issues/368) | Tab enhancement | ? | feature | lancada | 2.0.3 | ja-lancada | dbec4f44, 1af6067e | nós 10-08 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#369](https://github.com/iqui27/nuvio-native-legacy/issues/369) | [Bug] Multiple issues/Missing Features on Android TV version | Android | bug | lancada | 2.0.3 | 2.0.5 | 712557ca, 5f04c36b | sem comentários | responder (sem resposta); relatório de 9 itens, só 2 com commit |
| [#370](https://github.com/iqui27/nuvio-native-legacy/issues/370) | Subtitles sync issues | Samsung .wgt | bug | lancada | 2.0.3 | ja-lancada | 2aee231b, 3fe3c8ca, 56c9832e | nós 10-08 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |
| [#383](https://github.com/iqui27/nuvio-native-legacy/issues/383) | [port] subtitle memory same as nuvio | all | feature | lancada | 2.0.3 | ja-lancada | a89987c1 | autor 10-09 | responder: a 2.0.3 lembra a legenda escolhida a mão por perfil (a mesma faixa ou idioma no |
| [#384](https://github.com/iqui27/nuvio-native-legacy/issues/384) | Embedded ASS subtitles stopped rendering after anime intro | LG | bug | lancada | 2.0.3 | ja-lancada | 9ee093ac, b03b86db | autor 10-09 | fechar com a resposta curta do bloco "fechar com a 2.0.3" (o dono decide; saiu na v2.0.3) |

Notas:

- **#266**: Vários relatos juntos (Shield/TCL/BRAVIA). Reporter charles474 confirmou QR com build de teste em 08/10. Parte do conserto já na 2.0.2 (a1d5e039). LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#269**: MKV >2 GB rejeitado pelo leitor de legendas; conserto 80569112 só na 2.0.3 (2.0.1/2.0.2 tiveram partes). Resposta também prometeu "setting para o usuário escolher este comportamento": item vago, sem commit identificado. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#283**: Canais de add-on passam pelo proxy com headers na 2.0.2 (b68bad96); 1424e409/6de601a5 na 2.0.3. Autor mandou log MWASFG depois da resposta: não confirmado que resolveu. Alvo 2.0.3: NOTAS 2.0.3 cita #283 (guia de TV por add-on); aguarda confirmação do autor.
- **#284**: a63ec982: linha "Melhor para esta TV" mostra resolução. Sem comentário nosso na issue. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#286**: Rascunho de correção em 203-respostas-correcao.md: nome certo é "Resolução da interface > 720p (leve)". Mali-400 (Utgard). LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#287**: Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#290**: 7 commits #290 na 2.0.2 (gradiente do hero sobre trailer em janela, aspecto, etc.). Sem confirmação do autor; base08 confirmou o ponto 1 antes do release. Título original "Few minor issues on 2.1". Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#293**: Codec/canais do áudio no player na 2.0.2 (db1f5891). Estatísticas completas em 8c856535 (agente/203-338 = 2.0.5). Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#294**: Parte na 2.0.2 (b4186acd); 66e6ae9e/26d845c6/abd0770a na 2.0.3. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. PERSISTE NA 2.0.3 (09/10): a #392 relata a mesma reordenação ao trocar de perfil em v2.0.3 (.tpk Tizen 6.0); investigação em agente/2031-ordemperfil. Conserto do #392 (2.0.4): bb583b60, 9023ebdb em agente/2031-ordemperfil, não integrado.
- **#296**: Coberto pela "Elección de la fuente" (#310) na 2.0.2; nenhum commit cita #296. Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#298**: #273 (legendas árabes, 546c7418/57e24174) está na 2.0.2; #335 refina na 2.0.3. Nossa resposta de 07/10 disse "próxima release"; já estava na 2.0.2. Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#300**: Resposta cita 2.0.2; nenhum commit cita #300. Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#302**: Rascunho de correção: o conserto do live TV NÃO está na 2.0.2 (resposta anterior dizia que sim). 0021b573 (2.0.2) é parcial. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#303**: Alvo ja-lancada: já saiu numa versão publicada; só falta fechar.
- **#305**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#312**: Nossa resposta disse que "Dolby Vision in MKV" vem LIGADO por padrão: ERRADO, é DESLIGADO (docs 203-dvmkv-decisao.md; ajustes_ux_padrao.inc). Resumo traduzido e "All sources" saem na 2.0.3, não na 2.0.2. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. 09/10: correcao publicada na issue (DV em MKV desligado por padrao), com OK do dono.
- **#319**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#320**: Nenhum commit cita #320; 22ecac46/8ab3b78f ("tpk: arabic plain subtitles find Noto Naskh when the installed res/ predates 2.0.2") batem com a promessa "o próximo update busca a fonte sozinho". LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#321**: b347004c "rotulo de secao desce com o cartao aberto" (cita #203 = versão); NOTAS 2.0.3 o lista. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#322**: b8d110de: up next removível + opção para desligar (cita "#203" = versão). LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#323**: Tudo integrado em 2.0.3. O SIGSEGV em verificarOuParar (Conferencia na pilha) está consertado por 696afa74, que veio no merge 1b6c9a89: a Conferencia vai para o heap com contagem e é solta pelo último fio. f349c2f7/4d22e4f7 são uma versão anterior do mesmo conserto (sem liberar) e não precisam entrar; SANITIZE=1 tests/fonteparalela.sh passa em c3c1032e. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#328**: 09/10: SUSPEITA de mesma raiz do #372 (numeração de temporadas/episódios do catálogo do Nuvio primeiro, regressão 9677065b), não confirmada. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. DECISÃO (dono 09/10): mantém a nota de mesma raiz e vai para a 2.0.5 junto da parte 2 do #372 (numeração Cinemeta + camada de tradução). O conserto de episódios duplicados da 2.0.3 (9c5d0027, 7edab4b0) já saiu, mas o autor não o testou e a parte 1 do #372 (2.0.4) só mexe nas abas de temporada, então não há base para dizer que a 2.0.3 já fecha o caso. Origem: dono 09/10.
- **#330**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#332**: SUSPEITA: relato Sony BRAVIA preso em "rede _preparar"; commits citados são os do #266. A resposta final do dono fala de outro assunto (contorno do vidro dos pôsteres, citado como "corrigido para a próxima versão"). teste-shield-266.2 não resolveu segundo EzequielS04. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#335**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#339**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#340**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#341**: SUSPEITA: nenhum commit cita #341; beb5007a "tpk 4/5: botão de aspecto/zoom do player passa a ter efeito" está na 2.0.3 e NOTAS cita "aspect/zoom button on Tizen 4/5". LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#350**: Rascunho de correção: nome do ajuste é "OK no card"; "Ver detalhes" no menu de segurar sai na 2.0.3. Autor relatou depois filme travando durante TV ao vivo: responder. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#361**: NOTAS 2.0.3: posters com URL longa (>~500 caracteres) chegam inteiros ao fundo, detalhe, Salvos e conta (merge 9ac3531d, agente/203-361). Pedido original pedia limite maior; URL de fundo/logo longa demais é descartada com linha de log. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#363**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#368**: Rascunho de correção: ocultar add-ons no guia sai na 2.0.3, não na 2.0.2; busca espera 300 ms. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#369**: Parcial: tailandês (712557ca) e ocultar não lançados (5f04c36b). Os outros itens do relatório de 9: sem commit identificado. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#370**: LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#383**: Pedido: a legenda lembrar a faixa entre episódios (referência: NuvioMedia/NuvioTVSmart#1025). SUSPEITA de que a 2.0.3 já cobre: legmemoria (a89987c1, na tag v2.0.3) guarda por perfil a faixa exata por título (imdb sem temporada/episódio, então vale para os episódios da série) e a última escolha a mão em outro título; só a escolha manual grava. Não conferido contra a regra exata do Nuvio nem testado em TV; se o autor disser que falta algo (por exemplo lembrar por idioma de áudio), reabrir como 2.0.5. O comentário de base08 (não é do projeto) só concorda.
- **#384**: LG C5, webOS 26, Nuvio 2.0.2; Re:Zero S1 (Seadex): legenda embutida do ep. 3 funcionou, a do ep. 4 parou de aparecer depois da abertura. O autor (Vidhin05) JÁ mandou dois códigos de log no corpo (DDRROH e 98ZQED), ainda não lidos por nós; por isso o status é aberta e não precisa-log. Um comentário do próprio autor fala de "quedas de conexão e falhas de rede, talvez queda do debrid": relato solto, sem log próprio, tratar como segundo sintoma e checar nos mesmos logs. Namer03 (não é do projeto) respondeu "mesma coisa do #308, sai na 2.0.3": SUSPEITA, o #308 é legenda que bloqueia a fonte, o sintoma aqui (legenda some no meio) pode ser outro; não confirmado. Candidatos já na 2.0.3: #269 (legenda embutida) e #335 (ASS). ALVO 2.0.5 = triagem: pode virar 2.0.3 se os logs mostrarem que é o #308. CAUSA ACHADA (09/10, nos logs do próprio autor): NÃO é o #308 e NÃO está corrigido na 2.0.3 até agora. O coletor de ASS embutido indexa no máximo 8000 blocos (MKVASS_MAX_PONTOS, src/mkvass.c:61) NA ORDEM DO ARQUIVO; numa release com muito typeset (letreiros, karaokê) os 8000 acabam logo depois da abertura, e o resto do episódio fica sem legenda (log: "cobertura=7360-217270ms" com exatamente 8000 eventos). O branch agente/203-384 trabalha nisso; o alvo continua 2.0.5 a menos que esse branch entre na 2.0.3. Próximo passo: integrar agente/203-384 (ou deixar para a 2.0.5) e avisar o autor. O comentário de "queda do debrid" segue sem log próprio. CORRIGIDO (09/10): o conserto de agente/203-384 entrou na 2.0.3 (merge 2b4c3cea; b03b86db teste e 9ee093ac conserto, ambos contidos na tag v2.0.3): o índice cobre a faixa inteira e a faixa grande é colhida só na janela do playhead, sem carga extra no CDN. O texto acima sobre "não está corrigido" e "alvo 2.0.5" é histórico. A "queda do debrid" do autor segue sem log próprio.

### Lançadas e fechadas (232)

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#1](https://github.com/iqui27/nuvio-native-legacy/issues/1) | poster dont load on tiles | Samsung (tpk/wgt?) | ? | lancada | 1.1.2 | - | 47473f36, 8a1d6789, a9b251f4 | nós 09-08 | nada (fechada) |
| [#2](https://github.com/iqui27/nuvio-native-legacy/issues/2) | Main Nuvio icon not showing in apps list | LG | bug | lancada | 2.0.0 | - | 6e4bdfb3, 80d1350f, 94e9a6eb | nós 09-07 | nada (fechada) |
| [#3](https://github.com/iqui27/nuvio-native-legacy/issues/3) | Language still showing spanish | ? | ? | lancada | 1.1.2 | - | 8a1d6789, 6eef9cda, 7716f148 | nós 09-07 | nada (fechada) |
| [#4](https://github.com/iqui27/nuvio-native-legacy/issues/4) | Nuvio Keyboard Help | ? | ? | lancada | 1.1.0 | - | a9b251f4, 828731ba, 04c37f87 | autor 09-07 | nada (fechada) |
| [#5](https://github.com/iqui27/nuvio-native-legacy/issues/5) | Nuvio Sync Issue | ? | bug | lancada | 2.0.0 | - | 1f00125d, a9b251f4, 30b2c079 | nós 09-07 | nada (fechada) |
| [#6](https://github.com/iqui27/nuvio-native-legacy/issues/6) | Video Black Screen Issue | ? | bug | lancada | 1.1.0 | - | 7b9e641a, 96ccdab1, d30344ab | autor 09-22 | nada (fechada) |
| [#7](https://github.com/iqui27/nuvio-native-legacy/issues/7) | UI Resolution & Torrent Menu Lag | ? | bug | lancada | 2.0.0 | - | d4eae363, 04c37f87 | autor 09-07 | nada (fechada) |
| [#8](https://github.com/iqui27/nuvio-native-legacy/issues/8) | Profile Switching & Metadata Language Bug | ? | bug | lancada | 1.0.7 | - | 04c37f87 | autor 09-07 | nada (fechada) |
| [#9](https://github.com/iqui27/nuvio-native-legacy/issues/9) | Missing Subtitle Languages Bug | ? | bug | lancada | 1.0.7 | - | 04c37f87 | autor 09-07 | nada (fechada) |
| [#10](https://github.com/iqui27/nuvio-native-legacy/issues/10) | Bug: Collection is completely empty on Tizen | Samsung (tpk/wgt?) | bug | lancada | 1.0.21 | - | b202e3e3, 7791a043, 04c37f87 | autor 09-07 | nada (fechada) |
| [#11](https://github.com/iqui27/nuvio-native-legacy/issues/11) | Feedback for v1.0.7: Profile Management Bug, Syncing & Setti | ? | bug | lancada | 1.0.31 | - | bbfc4bd1, 90743102 | nós 09-07 | nada (fechada) |
| [#12](https://github.com/iqui27/nuvio-native-legacy/issues/12) | Bug: Some UI text is not translated to English on Tizen | Samsung (tpk/wgt?) | bug | lancada | 1.0.43 | - | 1142ee84, 4daaf18a, 0bc78c61 | autor 09-08 | nada (fechada) |
| [#13](https://github.com/iqui27/nuvio-native-legacy/issues/13) | Collections Installed from Nuvio Account Do Not Appear on Ti | Samsung (tpk/wgt?) | bug | lancada | 1.0.10 | - | 7716f148 | nós 09-09 | nada (fechada) |
| [#14](https://github.com/iqui27/nuvio-native-legacy/issues/14) | Bug: Next Episode Does Not Play After Confirming “Play Next” | ? | bug | lancada | 1.0.16 | - | e6cf02a9, 7716f148 | autor 09-08 | nada (fechada) |
| [#16](https://github.com/iqui27/nuvio-native-legacy/issues/16) | Bug: Detail Page Flickers and Randomly Opens a Different Tit | ? | bug | lancada | 1.0.13 | - | 7791a043, b28db878 | autor 09-08 | nada (fechada) |
| [#17](https://github.com/iqui27/nuvio-native-legacy/issues/17) | Bug: Posters Do Not Load When Using Better Posters URL | ? | bug | lancada | 1.0.15 | - | 4723f22f | autor 09-21 | nada (fechada) |
| [#18](https://github.com/iqui27/nuvio-native-legacy/issues/18) | Bug: Xperience Collections Are Split Into Multiple Home Rows | ? | bug | lancada | 1.4.2 | - | 6a3f3a05, 820e5fdb, 2d4e0568 | nós 09-14 | nada (fechada) |
| [#19](https://github.com/iqui27/nuvio-native-legacy/issues/19) | Bug: Random Profile Data Appears Briefly Before My Trakt Pro | ? | bug | lancada | 1.0.23 | - | 5413db8a, 45ae240c, 6aba13b6 | autor 09-09 | nada (fechada) |
| [#21](https://github.com/iqui27/nuvio-native-legacy/issues/21) | Backdrop/Hero image takes too long to load after moving focu | ? | ? | lancada | 1.5.2 | - | cda85061, f59496e6, 6aeb62a9 | autor 09-09 | nada (fechada) |
| [#22](https://github.com/iqui27/nuvio-native-legacy/issues/22) | Unable to remove items from Continue Watching | ? | bug | lancada | 1.0.30 | - | 19b68a33, 65962f31, 9f57ca70 | autor 09-09 | nada (fechada) |
| [#23](https://github.com/iqui27/nuvio-native-legacy/issues/23) | Seeing Portuguese strings in settings and Home Screen | LG | ? | lancada | 1.0.36 | - | 52f43f60, 7ef20234 | nós 09-14 | nada (fechada) |
| [#24](https://github.com/iqui27/nuvio-native-legacy/issues/24) | 🐛 BingeCat Catalogs Not Showing in the Fork | ? | bug | lancada | 1.0.29 | - | 8f1f7a97, ced5436f | autor 09-09 | nada (fechada) |
| [#25](https://github.com/iqui27/nuvio-native-legacy/issues/25) | Bug: Continue Watching Does Not Update Until App Restart | ? | bug | lancada | 1.0.30 | - | sem commit | autor 09-09 | nada (fechada) |
| [#26](https://github.com/iqui27/nuvio-native-legacy/issues/26) | Support question WebOS 26 (LG B4 2024) | LG | question | lancada | 1.0.30 | - | de71b7f9 | nós 09-09 | nada (fechada) |
| [#27](https://github.com/iqui27/nuvio-native-legacy/issues/27) | Log debug | ? | bug | lancada | 1.6.5 | - | 46b1d1b7 | nós 09-15 | nada (fechada) |
| [#28](https://github.com/iqui27/nuvio-native-legacy/issues/28) | Viewport resolution | ? | ? | lancada | 1.0.32 | - | 2de0c2ec, 82b16916 | autor 09-10 | nada (fechada) |
| [#29](https://github.com/iqui27/nuvio-native-legacy/issues/29) | Collection Focus GIFs Do Not Play on Tizen | Samsung (tpk/wgt?) | bug | lancada | 1.0.44 | - | 24f8e451, f3d40b21 | nós 09-14 | nada (fechada) |
| [#30](https://github.com/iqui27/nuvio-native-legacy/issues/30) | Connecting Trakt Causes Collections to Disappear From Home | ? | ? | lancada | 1.0.34 | - | 6559dff3, d34d9d1d, f59dc32c | nós 09-10 | nada (fechada) |
| [#31](https://github.com/iqui27/nuvio-native-legacy/issues/31) | Content Advisory Badges Not Showing Consistently | ? | bug | lancada | 1.0.43 | - | bacddc9c, 6f975f57, 1173250a | autor 09-14 | nada (fechada) |
| [#32](https://github.com/iqui27/nuvio-native-legacy/issues/32) | Bug: “More Like This” menu appears when a movie starts inste | ? | bug | lancada | 1.0.35 | - | sem commit | autor 09-11 | nada (fechada) |
| [#33](https://github.com/iqui27/nuvio-native-legacy/issues/33) | Bug Report: Severe UI stutters / freezes during navigation o | Samsung (tpk/wgt?) | bug | lancada | 1.0.41 | - | 6f975f57, 1173250a, 1444a771 | autor 09-11 | nada (fechada) |
| [#34](https://github.com/iqui27/nuvio-native-legacy/issues/34) | Play next  triggers too soon | ? | ? | lancada | 1.4.6 | - | 8d36a1f2, 0e43c9ad | nós 09-14 | nada (fechada) |
| [#35](https://github.com/iqui27/nuvio-native-legacy/issues/35) | Episodes listed when clicking a show with multiple seasons. | ? | ? | lancada | 1.0.38 | - | 2d995944 | nós 09-14 | nada (fechada) |
| [#36](https://github.com/iqui27/nuvio-native-legacy/issues/36) | Remove from continue watching not highlighting | ? | bug | lancada | 1.0.38 | - | 87d986be | nós 09-14 | nada (fechada) |
| [#37](https://github.com/iqui27/nuvio-native-legacy/issues/37) | Support for New Addon Types | ? | feature | lancada | 1.6.0 | - | 2e9bbce8, 1b84d4bd, bee9db0a | nós 09-15 | nada (fechada) |
| [#38](https://github.com/iqui27/nuvio-native-legacy/issues/38) | Continue Watching row does not update or takes a long time t | ? | bug | lancada | 1.0.44 | - | 24f8e451 | nós 09-14 | nada (fechada) |
| [#39](https://github.com/iqui27/nuvio-native-legacy/issues/39) | Hero/backdrop is delayed | ? | ? | lancada | 1.0.44 | - | 24f8e451 | nós 09-14 | nada (fechada) |
| [#40](https://github.com/iqui27/nuvio-native-legacy/issues/40) | Play/Pause button not working | Samsung (tpk/wgt?) | bug | lancada | 1.0.43 | - | a3c48b28, d04298af | nós 09-15 | nada (fechada) |
| [#41](https://github.com/iqui27/nuvio-native-legacy/issues/41) | Portuguese text in subtitle menu | ? | ? | lancada | 1.0.43 | - | 1142ee84 | nós 09-15 | nada (fechada) |
| [#42](https://github.com/iqui27/nuvio-native-legacy/issues/42) | Issues with version 1.0.42 | ? | bug | lancada | 1.0.51 | - | 06358c1a, 820e5fdb, 2d4e0568 | nós 09-15 | nada (fechada) |
| [#43](https://github.com/iqui27/nuvio-native-legacy/issues/43) | Continue watching row. Always sets you to the first episode  | ? | ? | lancada | 1.0.43 | - | 31ad46da | nós 09-15 | nada (fechada) |
| [#44](https://github.com/iqui27/nuvio-native-legacy/issues/44) | Collections Installed from Nuvio Website Do Not Appear in Ap | Samsung (tpk/wgt?) | bug | lancada | 1.0.44 | - | 24f8e451 | autor 09-14 | nada (fechada) |
| [#45](https://github.com/iqui27/nuvio-native-legacy/issues/45) | Support for gifs as profile picture | ? | feature | lancada | 1.6.5 | - | 46b1d1b7, 24f8e451 | nós 09-14 | nada (fechada) |
| [#46](https://github.com/iqui27/nuvio-native-legacy/issues/46) | Start from the beginning | ? | ? | lancada | 1.0.45 | - | 3267613a | nós 09-15 | nada (fechada) |
| [#48](https://github.com/iqui27/nuvio-native-legacy/issues/48) | Startup log screen on startup | ? | ? | lancada | 1.7.4 | - | e0596c71 | autor 09-15 | nada (fechada) |
| [#49](https://github.com/iqui27/nuvio-native-legacy/issues/49) | GIF plays briefly then flickers and reverts to a static imag | ? | bug | lancada | 1.0.54 | - | 21f68756, 6efca7e5 | autor 09-16 | nada (fechada) |
| [#50](https://github.com/iqui27/nuvio-native-legacy/issues/50) | Player loading screen briefly shows the wrong title when swi | ? | bug | lancada | 1.0.51 | - | sem commit | autor 09-15 | nada (fechada) |
| [#51](https://github.com/iqui27/nuvio-native-legacy/issues/51) | Active catalogs/collections appear in the middle or bottom o | ? | ? | lancada | 1.0.51 | - | sem commit | autor 09-15 | nada (fechada) |
| [#52](https://github.com/iqui27/nuvio-native-legacy/issues/52) | Portugues text in integration tab and studio tab | ? | ? | lancada | 1.0.51 | - | sem commit | nós 09-15 | nada (fechada) |
| [#54](https://github.com/iqui27/nuvio-native-legacy/issues/54) | Feature request | ? | feature | lancada | 1.0.53 | - | 78793586 | autor 09-16 | nada (fechada) |
| [#55](https://github.com/iqui27/nuvio-native-legacy/issues/55) | Bug | ? | bug | lancada | 1.3.2 | - | 8732c235, 758aa5e3 | nós 09-19 | nada (fechada) |
| [#56](https://github.com/iqui27/nuvio-native-legacy/issues/56) | Feature Request – Remember the selected source between episo | ? | feature | lancada | 1.0.56 | - | 98c5bbfb, 1e6a770e | autor 09-16 | nada (fechada) |
| [#57](https://github.com/iqui27/nuvio-native-legacy/issues/57) | Resume opens the source panel instead of the previously used | ? | ? | lancada | 1.0.55 | - | 1e6a770e | nós 09-18 | nada (fechada) |
| [#60](https://github.com/iqui27/nuvio-native-legacy/issues/60) | Trailers of movie under a different one | ? | ? | lancada | 1.2.1 | - | 699e244e | nós 09-19 | nada (fechada) |
| [#61](https://github.com/iqui27/nuvio-native-legacy/issues/61) | bug | ? | bug | lancada | 1.2.1 | - | 699e244e | nós 09-19 | nada (fechada) |
| [#62](https://github.com/iqui27/nuvio-native-legacy/issues/62) | bug | ? | bug | lancada | 1.2.1 | - | 699e244e | nós 09-19 | nada (fechada) |
| [#65](https://github.com/iqui27/nuvio-native-legacy/issues/65) | Application crashes after reaching end of row | ? | bug | lancada | 1.2.1 | - | 699e244e, ecaa6af5 | autor 09-19 | nada (fechada) |
| [#66](https://github.com/iqui27/nuvio-native-legacy/issues/66) | 'Continue Watching' missing shows from Trakt | ? | bug | lancada | 1.3.0 | - | 8e64134a | nós 09-21 | nada (fechada) |
| [#67](https://github.com/iqui27/nuvio-native-legacy/issues/67) | Posters not loading | ? | bug | lancada | 1.3.7 | - | 68f87611, 967fa084, 8732c235 | autor 09-21 | nada (fechada) |
| [#68](https://github.com/iqui27/nuvio-native-legacy/issues/68) | Crashes | ? | bug | lancada | 1.3.2 | - | a8970d44 | autor 09-20 | nada (fechada) |
| [#69](https://github.com/iqui27/nuvio-native-legacy/issues/69) | Hero/backdrop | ? | bug | lancada | 1.3.4 | - | 60bcfc02, a8970d44 | nós 09-19 | nada (fechada) |
| [#70](https://github.com/iqui27/nuvio-native-legacy/issues/70) | Mark as watched issues | ? | bug | lancada | 1.3.2 | - | 701f3db1 | nós 09-19 | nada (fechada) |
| [#71](https://github.com/iqui27/nuvio-native-legacy/issues/71) | Unable to change value on memory used by images | ? | bug | lancada | 1.3.2 | - | 097ef52f | nós 09-19 | nada (fechada) |
| [#72](https://github.com/iqui27/nuvio-native-legacy/issues/72) | Navigating now strutting | Samsung (tpk/wgt?) | bug | lancada | 1.3.4 | - | 47c7b45b, bdca515f, 9a8b220e | nós 09-20 | nada (fechada) |
| [#73](https://github.com/iqui27/nuvio-native-legacy/issues/73) | 'Up next' appearing before the credits again | ? | bug | lancada | 1.4 | - | 876742e2 | nós 09-22 | nada (fechada) |
| [#74](https://github.com/iqui27/nuvio-native-legacy/issues/74) | Very low resolution ticks | ? | ? | lancada | 1.3.4 | - | f4e96196 | nós 09-20 | nada (fechada) |
| [#76](https://github.com/iqui27/nuvio-native-legacy/issues/76) | Build not reading the catalogue correctly | ? | bug | lancada | 1.3.4 | - | 91450a5f | nós 09-20 | nada (fechada) |
| [#77](https://github.com/iqui27/nuvio-native-legacy/issues/77) | Tyzen os v 1.3.3 - TV got struck | ? | bug | lancada | 1.3.5 | - | 3b2e154a, 2764aa05 | autor 09-21 | nada (fechada) |
| [#78](https://github.com/iqui27/nuvio-native-legacy/issues/78) | Unable to scroll through trakt ratings for TV Show | ? | bug | lancada | 1.3.4 | - | 9e388586 | autor 09-20 | nada (fechada) |
| [#79](https://github.com/iqui27/nuvio-native-legacy/issues/79) | Issue with TV Show seasons | ? | bug | lancada | 1.3.4 | - | 9e388586 | nós 09-20 | nada (fechada) |
| [#80](https://github.com/iqui27/nuvio-native-legacy/issues/80) | Samsung: Resume opens source list, slow first stream load, a | Samsung (tpk/wgt?) | bug | lancada | 1.3.4-comparacao1 | - | b43ffef0 | autor 09-20 | nada (fechada) |
| [#82](https://github.com/iqui27/nuvio-native-legacy/issues/82) | Trailer | Samsung (tpk/wgt?) | ? | lancada | 1.3.11 | - | 58d491a7, b3b635d4 | nós 09-21 | nada (fechada) |
| [#83](https://github.com/iqui27/nuvio-native-legacy/issues/83) | Certain addons not showing | ? | bug | lancada | 1.4 | - | 876742e2 | autor 09-22 | nada (fechada) |
| [#84](https://github.com/iqui27/nuvio-native-legacy/issues/84) | GIF not smooth | ? | bug | lancada | 1.4.7 | - | 15e8e1a5, 2883053e | autor 09-25 | nada (fechada) |
| [#85](https://github.com/iqui27/nuvio-native-legacy/issues/85) | Minor visual bugs + settings not saved | Samsung (tpk/wgt?) | bug | lancada | 1.3.10 | - | 6f2eb5f7 | autor 09-21 | nada (fechada) |
| [#86](https://github.com/iqui27/nuvio-native-legacy/issues/86) | Trailer don't play | ? | bug | lancada | 1.3.11 | - | 58d491a7 | autor 09-21 | nada (fechada) |
| [#87](https://github.com/iqui27/nuvio-native-legacy/issues/87) | IMDB rating not shown on continue watching and on episodes | ? | bug | lancada | 1.3.12 | - | 6542d85e | nós 09-21 | nada (fechada) |
| [#88](https://github.com/iqui27/nuvio-native-legacy/issues/88) | App working fine now | ? | ? | lancada | 1.4.1 | - | d683d997 | nós 09-23 | nada (fechada) |
| [#89](https://github.com/iqui27/nuvio-native-legacy/issues/89) | Posters from xperience are cropped (not sized correctly  | ? | bug | lancada | 1.3.12 | - | 9cc8d044 | nós 09-21 | nada (fechada) |
| [#92](https://github.com/iqui27/nuvio-native-legacy/issues/92) | Anime Embedded subtitles broken | LG | bug | lancada | 1.5.0 | - | b61d74a6, 57644d6a, b7515cb8 | autor 09-25 | nada (fechada) |
| [#93](https://github.com/iqui27/nuvio-native-legacy/issues/93) | Autoplay doesnt work on continue watching | ? | bug | lancada | 1.3.12 | - | 8882ce89 | nós 09-21 | nada (fechada) |
| [#94](https://github.com/iqui27/nuvio-native-legacy/issues/94) | Cast list only shows three cast members | ? | ? | lancada | 1.5.1 | - | e3ae1533, ebdf2cd3 | nós 09-22 | nada (fechada) |
| [#95](https://github.com/iqui27/nuvio-native-legacy/issues/95) | Home catalog rows remember previous tile focus after restart | ? | bug | lancada | 1.4.6 | - | 89eab64c, c2f5cef3, 876742e2 | nós 09-24 | nada (fechada) |
| [#97](https://github.com/iqui27/nuvio-native-legacy/issues/97) | All english is in portugues | ? | ? | lancada | 1.3.12 | - | 2b94b895 | autor 09-21 | nada (fechada) |
| [#99](https://github.com/iqui27/nuvio-native-legacy/issues/99) | Possibility of having LG magic mouse cursor support? | LG | question | lancada | 1.4.2 | - | ba8b96c6, 81842912, 37a1a868 | autor 09-23 | nada (fechada) |
| [#100](https://github.com/iqui27/nuvio-native-legacy/issues/100) | Episodes not being marked as watched when completed | LG | bug | lancada | 1.4 | - | 876742e2 | autor 09-23 | nada (fechada) |
| [#101](https://github.com/iqui27/nuvio-native-legacy/issues/101) | Playing an episode sometimes loads sources of previous watch | LG | bug | lancada | 1.4 | - | 876742e2 | autor 09-23 | nada (fechada) |
| [#102](https://github.com/iqui27/nuvio-native-legacy/issues/102) | Episode carousel auto scrolls to first episode of the season | LG | bug | lancada | 1.4 | - | 876742e2 | autor 09-23 | nada (fechada) |
| [#103](https://github.com/iqui27/nuvio-native-legacy/issues/103) | 🐛 Catalog row: Last focused tile gets clipped at the right e | ? | ? | lancada | 1.4 | - | 876742e2 | autor 09-22 | nada (fechada) |
| [#104](https://github.com/iqui27/nuvio-native-legacy/issues/104) | Theme colors | ? | feature | lancada | 1.4.2 | - | e7453e91, 6a61e67e | autor 09-23 | nada (fechada) |
| [#105](https://github.com/iqui27/nuvio-native-legacy/issues/105) | Bug: Auto-selected source can get stuck loading indefinitely | ? | bug | lancada | 1.4.2 | - | 6a61e67e | autor 09-23 | nada (fechada) |
| [#106](https://github.com/iqui27/nuvio-native-legacy/issues/106) | Catalog order doesn't persist between app updates | ? | bug | lancada | 1.4.2 | - | 6a3f3a05, 6a61e67e | nós 09-23 | nada (fechada) |
| [#108](https://github.com/iqui27/nuvio-native-legacy/issues/108) | Request. Pressing 'Season' brings up option to mark all as w | ? | feature | lancada | 1.4.2 | - | sem commit | nós 09-23 | nada (fechada) |
| [#109](https://github.com/iqui27/nuvio-native-legacy/issues/109) | Media player focus | ? | ? | lancada | 1.4.3 | - | 9cc535e8, d683d997 | autor 09-23 | nada (fechada) |
| [#110](https://github.com/iqui27/nuvio-native-legacy/issues/110) | SIMKL no Option for continue list | ? | feature | lancada | 1.4.2 | - | a00f2e23, ae51d9b0 | nós 09-23 | nada (fechada) |
| [#111](https://github.com/iqui27/nuvio-native-legacy/issues/111) | Movie starts with black screen | LG | bug | lancada | 1.4.2 | - | 6a3f3a05 | autor 09-24 | nada (fechada) |
| [#112](https://github.com/iqui27/nuvio-native-legacy/issues/112) | iptv channel don't work in samsung | Samsung (tpk/wgt?) | bug | lancada | 1.4.2 | - | 1c9f7c59 | nós 09-23 | nada (fechada) |
| [#113](https://github.com/iqui27/nuvio-native-legacy/issues/113) | Diagnostic | Samsung (tpk/wgt?) | ? | lancada | 1.4.3 | - | 10e4c3c0, 9cacb9c5 | autor 09-23 | nada (fechada) |
| [#114](https://github.com/iqui27/nuvio-native-legacy/issues/114) | Artwork / Backdrop Loading Regression in v1.4.2 | Samsung (tpk/wgt?) | ? | lancada | 1.4.3 | - | eedf7c81 | autor 09-23 | nada (fechada) |
| [#115](https://github.com/iqui27/nuvio-native-legacy/issues/115) | More like this appears too early  | ? | bug | lancada | 1.4.6 | - | 89eab64c, 8d36a1f2 | nós 09-24 | nada (fechada) |
| [#116](https://github.com/iqui27/nuvio-native-legacy/issues/116) | icons appear to be glitched or incorrect | ? | ? | lancada | 1.4.3 | - | 0fde2217 | nós 09-23 | nada (fechada) |
| [#117](https://github.com/iqui27/nuvio-native-legacy/issues/117) | App appears to be slower in newer version  | ? | bug | lancada | 1.4.3 | - | sem commit | nós 09-23 | nada (fechada) |
| [#118](https://github.com/iqui27/nuvio-native-legacy/issues/118) | incorrect hero artwork in continue watching | ? | bug | lancada | 1.4.3 | - | 10e4c3c0, a994a0fc | nós 09-23 | nada (fechada) |
| [#119](https://github.com/iqui27/nuvio-native-legacy/issues/119) | Update screen: scroll problem and no update button | Samsung (tpk/wgt?) | bug | lancada | 1.4.6 | - | sem commit | autor 09-25 | nada (fechada) |
| [#120](https://github.com/iqui27/nuvio-native-legacy/issues/120) | Send log screen popup on every start | Samsung (tpk/wgt?) | bug | lancada | 1.4.3 | - | d1427b55, a8eb6296 | autor 09-24 | nada (fechada) |
| [#121](https://github.com/iqui27/nuvio-native-legacy/issues/121) | Rewind, forward focus problem | Samsung (tpk/wgt?) | bug | lancada | 1.4.5 | - | 6cce1253, d1427b55, 9cc535e8 | autor 09-24 | nada (fechada) |
| [#122](https://github.com/iqui27/nuvio-native-legacy/issues/122) | Embedded subtitles isn't displayed | Samsung (tpk/wgt?) | bug | lancada | 1.4.3 | - | d1427b55, bae63187, f4f05699 | autor 09-24 | nada (fechada) |
| [#123](https://github.com/iqui27/nuvio-native-legacy/issues/123) | TV Shows / Series — Trailer Support | ? | feature | lancada | 1.4.3 | - | bd405a92, 9293bf5b | nós 09-24 | nada (fechada) |
| [#124](https://github.com/iqui27/nuvio-native-legacy/issues/124) | Trailer Playback in Hero / Backdrop | Samsung (tpk/wgt?) | feature | lancada | 1.5.2 | - | 72e3378f | nós 09-27 | nada (fechada) |
| [#125](https://github.com/iqui27/nuvio-native-legacy/issues/125) | Metadata sync delay | ? | ? | lancada | 1.4.4 | - | sem commit | nós 09-24 | nada (fechada) |
| [#126](https://github.com/iqui27/nuvio-native-legacy/issues/126) | Missing catalogs | ? | bug | lancada | 1.4.6 | - | b61d74a6, 32a83118, 8874e60f | autor 09-24 | nada (fechada) |
| [#127](https://github.com/iqui27/nuvio-native-legacy/issues/127) | No separate upcoming row | ? | bug | lancada | 1.4.7 | - | 563ccc81 | nós 09-27 | nada (fechada) |
| [#128](https://github.com/iqui27/nuvio-native-legacy/issues/128) | (QOL) Hide ui except for progress bar while seeking | ? | ? | lancada | 1.4.5 | - | 6cce1253 | nós 09-24 | nada (fechada) |
| [#129](https://github.com/iqui27/nuvio-native-legacy/issues/129) | Subtitle/audio language doesnt save | ? | bug | lancada | 1.4.4 | - | 94c59a3e, 725a0cbc, 0e6f0da6 | nós 09-24 | nada (fechada) |
| [#130](https://github.com/iqui27/nuvio-native-legacy/issues/130) | Source selection loading multiple files before playing | ? | ? | lancada | 2.0.2 | - | 46a340a3, cd440320, 89c77368 | nós 09-24 | nada (fechada) |
| [#131](https://github.com/iqui27/nuvio-native-legacy/issues/131) | Number of Titles limited to 205 | ? | bug | lancada | 1.4.6 | - | sem commit | nós 09-24 | nada (fechada) |
| [#132](https://github.com/iqui27/nuvio-native-legacy/issues/132) | Only 1 source is listed | Samsung (tpk/wgt?) | bug | lancada | 1.4.6 | - | 52316a09, cd440320 | autor 09-27 | nada (fechada) |
| [#133](https://github.com/iqui27/nuvio-native-legacy/issues/133) | Poster unwatched blur effect doesn't work | Samsung (tpk/wgt?) | bug | lancada | 1.6.0 | - | 340e5791, 3a6960ca, 91ceeeea | autor 09-25 | nada (fechada) |
| [#134](https://github.com/iqui27/nuvio-native-legacy/issues/134) | Please support plugin | ? | feature | lancada | 2.0.0 | - | sem commit | nós 10-06 | nada (fechada) |
| [#136](https://github.com/iqui27/nuvio-native-legacy/issues/136) | Trailer playback issues by source on Samsung AU7000 | Samsung (tpk/wgt?) | bug | lancada | 1.4.6 | - | a81639d2, ea872c3b | autor 09-25 | nada (fechada) |
| [#137](https://github.com/iqui27/nuvio-native-legacy/issues/137) | Samsung: help test a native (.tpk) Nuvio — 2 minutes, TVs fr | Samsung (tpk/wgt?) | ? | lancada | 1.5.4 | - | 6dac818e, 4278426c, dde1abd2 | nós 09-30 | nada (fechada) |
| [#138](https://github.com/iqui27/nuvio-native-legacy/issues/138) | Samsung 2018/2019 (Tizen 4/5): experimental build available, | ? | ? | lancada | 1.6.0 | - | sem commit | nós 09-30 | nada (fechada) |
| [#141](https://github.com/iqui27/nuvio-native-legacy/issues/141) | Some catalog GIFs not playing | ? | bug | lancada | 1.5.3 | - | b8b7a114, d7c0adbb, 68521ca4 | autor 09-28 | nada (fechada) |
| [#142](https://github.com/iqui27/nuvio-native-legacy/issues/142) | Possibility of manually selecting hero artwork | ? | feature | lancada | 1.6.4 | - | 9a352549, 86c5a1c9 | autor 09-25 | nada (fechada) |
| [#145](https://github.com/iqui27/nuvio-native-legacy/issues/145) | The image only uses a small part of the window on the upper  | LG | bug | lancada | 1.6.0 | - | c79995b6 | nós 09-30 | nada (fechada) |
| [#146](https://github.com/iqui27/nuvio-native-legacy/issues/146) | Add more languages | Android | feature | lancada | 1.6.0 | - | sem commit | nós 09-30 | nada (fechada) |
| [#147](https://github.com/iqui27/nuvio-native-legacy/issues/147) | Remote Button Input Delay — Volume and Home Buttons | Samsung (tpk/wgt?) | bug | lancada | 1.5.2 | - | 378127d9, dc18b5e1 | nós 09-30 | nada (fechada) |
| [#149](https://github.com/iqui27/nuvio-native-legacy/issues/149) | Send logs automatically doesn't stay activated | ? | bug | lancada | 1.5.1 | - | 400dd41d, ea0a1373 | nós 09-27 | nada (fechada) |
| [#150](https://github.com/iqui27/nuvio-native-legacy/issues/150) | Episode Descriptions Remain in English Despite Portuguese La | ? | ? | lancada | 1.5.1 | - | 035c538a | nós 09-27 | nada (fechada) |
| [#151](https://github.com/iqui27/nuvio-native-legacy/issues/151) | The next episode doesn't play automatically / disappears fro | Samsung (tpk/wgt?) | bug | lancada | 1.5.3 | - | b274752b, 61fbfa8c | autor 09-28 | nada (fechada) |
| [#153](https://github.com/iqui27/nuvio-native-legacy/issues/153) | Cast artwork does not match names of actors  | ? | bug | lancada | 1.5.1 | - | e3ae1533 | nós 09-27 | nada (fechada) |
| [#156](https://github.com/iqui27/nuvio-native-legacy/issues/156) | Long single line subtitles overflow as ellipsis instead of w | LG | ? | lancada | 1.5.1 | - | 96f5abe4 | nós 09-27 | nada (fechada) |
| [#158](https://github.com/iqui27/nuvio-native-legacy/issues/158) | Live TV (Xtream): channels don't play and guide is empty (LG | LG | bug | lancada | 2.0.2 | - | b68bad96, aad1c947, 31a3da42 | nós 10-06 | nada (fechada) |
| [#159](https://github.com/iqui27/nuvio-native-legacy/issues/159) | Source panel lags while textures are loading | ? | bug | lancada | 1.5.2 | - | 821b39f0 | autor 09-27 | nada (fechada) |
| [#160](https://github.com/iqui27/nuvio-native-legacy/issues/160) | Hero catalogs | ? | ? | lancada | 1.5.2 | - | ebf1a825 | nós 09-27 | nada (fechada) |
| [#162](https://github.com/iqui27/nuvio-native-legacy/issues/162) | Allow removal of certain options from sidebar | ? | feature | lancada | 1.5.2 | - | d4037a42 | nós 09-27 | nada (fechada) |
| [#163](https://github.com/iqui27/nuvio-native-legacy/issues/163) | Number of catalog options shown | ? | feature | lancada | 1.5.2 | - | 6b05521d, 426b0296 | nós 09-27 | nada (fechada) |
| [#164](https://github.com/iqui27/nuvio-native-legacy/issues/164) | Hero "Loading artwork…" placeholder makes focus transitions  | ? | ? | lancada | 1.5.2 | - | cda85061 | nós 09-27 | nada (fechada) |
| [#165](https://github.com/iqui27/nuvio-native-legacy/issues/165) | native app works prefectly | Samsung (tpk/wgt?) | ? | lancada | 1.7.0 | - | 94690ba3, c4e4d922 | autor 09-29 | nada (fechada) |
| [#169](https://github.com/iqui27/nuvio-native-legacy/issues/169) | Can P2P content play on LG? No option to enable it | LG | question | lancada | 1.6.0 | - | sem commit | autor 10-05 | nada (fechada) |
| [#170](https://github.com/iqui27/nuvio-native-legacy/issues/170) | The Tyzen os Native app (tpk - experimental 1.5.2) not openi | Samsung (tpk/wgt?) | bug | lancada | 1.5.4 | - | f7fee47c, 95557d50, 2456b563 | autor 09-30 | nada (fechada) |
| [#171](https://github.com/iqui27/nuvio-native-legacy/issues/171) | P2P addons (Torrentio) missing from sources but shown in the | Samsung (tpk/wgt?) | question | lancada | 2.0.0 | - | 775f9db9 | nós 09-30 | nada (fechada) |
| [#172](https://github.com/iqui27/nuvio-native-legacy/issues/172) | UI: port ideas from the Corby7 fork | ? | feature | lancada | 1.6.2 | - | 2b99d09b, a65080ae, 414cd61e | nós 09-29 | nada (fechada) |
| [#173](https://github.com/iqui27/nuvio-native-legacy/issues/173) | "Update now" not appearing on version 1.5.2 | ? | bug | lancada | 1.5.3 | - | sem commit | autor 09-30 | nada (fechada) |
| [#174](https://github.com/iqui27/nuvio-native-legacy/issues/174) | Episodes missing of some Tv series | Samsung (tpk/wgt?) | bug | lancada | 1.6.0 | - | 582bc2e6 | autor 09-30 | nada (fechada) |
| [#175](https://github.com/iqui27/nuvio-native-legacy/issues/175) | Allow users to choose different Search sources | ? | feature | lancada | 1.6.0 | - | 582bc2e6 | nós 09-30 | nada (fechada) |
| [#176](https://github.com/iqui27/nuvio-native-legacy/issues/176) | LG G5 (webOS 10) feedback: languages, half-size video with 4 | LG | bug | lancada | 1.7.0 | - | 0da72c2a, 6def852c, 3e90ae7f | autor 09-30 | nada (fechada) |
| [#177](https://github.com/iqui27/nuvio-native-legacy/issues/177) | Next episode thumbnail isn't blurred | ? | bug | lancada | 1.6.0 | - | 3f834522, 340e5791, 3a6960ca | autor 10-03 | nada (fechada) |
| [#178](https://github.com/iqui27/nuvio-native-legacy/issues/178) | Trailer zoom options + fullscreen audio issues | Samsung (tpk/wgt?) | bug | lancada | 1.6.3 | - | 5b0e4957, cc3a9d19, 9efe205f | autor 09-30 | nada (fechada) |
| [#179](https://github.com/iqui27/nuvio-native-legacy/issues/179) | Trakt playback scrobbling not working (Now Watching, progres | LG | bug | lancada | 1.6.0 | - | 4d6f3461, 3f834522, 14e69356 | nós 09-30 | nada (fechada) |
| [#180](https://github.com/iqui27/nuvio-native-legacy/issues/180) | Tizen 4/5 native: app loads but exits during sign-in | Samsung (tpk/wgt?) | bug | lancada | 1.6.0 | - | be115c3f, c121f7d1, 6f581853 | nós 09-30 | nada (fechada) |
| [#181](https://github.com/iqui27/nuvio-native-legacy/issues/181) | Native self-update via memfd (download new libnuvio.so, no r | Samsung (tpk/wgt?) | feature | lancada | 1.5.4 | - | ae0ba290 | nós 09-30 | nada (fechada) |
| [#182](https://github.com/iqui27/nuvio-native-legacy/issues/182) | Addons not loading sometimes in LG web os 26 | LG | bug | lancada | 1.5.4 | - | 3f834522, abc5408e | autor 09-30 | nada (fechada) |
| [#184](https://github.com/iqui27/nuvio-native-legacy/issues/184) | Auto-update on native samsung tizen not working | Samsung .tpk | bug | lancada | 1.6.4 | - | 0da4861a | nós 10-01 | nada (fechada) |
| [#185](https://github.com/iqui27/nuvio-native-legacy/issues/185) | colored line bug | Samsung .tpk | bug | lancada | 1.6.3 | - | 359937e6, e9406ad3 | autor 09-30 | nada (fechada) |
| [#186](https://github.com/iqui27/nuvio-native-legacy/issues/186) | Stream Badges are B&W | LG | bug | lancada | 1.6.1 | - | 53d01871 | sem comentários | nada (fechada) |
| [#187](https://github.com/iqui27/nuvio-native-legacy/issues/187) | The Guide feature does not open a title | LG | bug | lancada | 1.6.5 | - | 4c3bcdce, 3951e747, 5aebadf3 | nós 10-01 | nada (fechada) |
| [#188](https://github.com/iqui27/nuvio-native-legacy/issues/188) | Video playback bug | Samsung .tpk | bug | lancada | 2.0.2 | - | a6d357c8, b5deeb25, bfeffd13 | nós 09-30 | nada (fechada) |
| [#190](https://github.com/iqui27/nuvio-native-legacy/issues/190) | Incorrect Title and Epsiode List | LG | bug | lancada | 1.6.2 | - | 000015d7 | autor 10-02 | nada (fechada) |
| [#191](https://github.com/iqui27/nuvio-native-legacy/issues/191) | 1.6.1 and 1.6.0 (.wgt) is extremely lagging | Samsung .wgt | bug | lancada | 1.6.2 | - | 2b99d09b | nós 10-01 | nada (fechada) |
| [#193](https://github.com/iqui27/nuvio-native-legacy/issues/193) | Black screen when exit from app | Samsung .tpk | bug | lancada | 1.7.0 | - | sem commit | autor 10-02 | nada (fechada) |
| [#194](https://github.com/iqui27/nuvio-native-legacy/issues/194) | TMDB Collections Not Working | LG | bug | lancada | 1.6.4 | - | f25d2e7d, 34db0d1a | autor 09-30 | nada (fechada) |
| [#195](https://github.com/iqui27/nuvio-native-legacy/issues/195) | Tpk 1.6.1 - trailer autoplay bug | Samsung (tpk/wgt?) | bug | lancada | 2.0.2 | - | a6d357c8, b5deeb25, 2aa724f4 | autor 10-02 | nada (fechada) |
| [#196](https://github.com/iqui27/nuvio-native-legacy/issues/196) | Not Available toast message upon pressing Play/Pause button | Samsung .tpk | bug | lancada | 1.6.4 | - | 46a80368 | autor 10-01 | nada (fechada) |
| [#197](https://github.com/iqui27/nuvio-native-legacy/issues/197) | Catalogue Not showing | Samsung .tpk | bug | lancada | 1.6.4 | - | f5813122, b98ae904 | nós 10-01 | nada (fechada) |
| [#198](https://github.com/iqui27/nuvio-native-legacy/issues/198) | Stream Badge | LG | feature | lancada | 1.6.5 | - | f26efc21, 46b1d1b7 | autor 10-01 | nada (fechada) |
| [#199](https://github.com/iqui27/nuvio-native-legacy/issues/199) | Continue watching is not showing unaired up next | LG | bug | lancada | 1.7.0 | - | dc9ca3bf, 6e38bb5b, 149f1235 | autor 10-08 | nada (fechada) |
| [#200](https://github.com/iqui27/nuvio-native-legacy/issues/200) | Custom posters not working correctly | LG | bug | lancada | 1.7.0 | - | 98c61346, f0735c7b | autor 10-02 | nada (fechada) |
| [#201](https://github.com/iqui27/nuvio-native-legacy/issues/201) |  Top 10 view inconsistent with automatic view | ? | bug | lancada | 2.0.1 | - | cda0b4fd, 522aba65, 47a3d55d | autor 10-02 | nada (fechada) |
| [#202](https://github.com/iqui27/nuvio-native-legacy/issues/202) | Matching accent colour in settings obscures text | LG | bug | lancada | 2.0.2 | - | 9768862e, 28ca12d9, 325f4e2b | nós 10-02 | nada (fechada) |
| [#203](https://github.com/iqui27/nuvio-native-legacy/issues/203) | Menu bar remain visible during play | Samsung .tpk | bug | lancada | 1.7.0 | - | sem commit | autor 10-02 | nada (fechada) |
| [#204](https://github.com/iqui27/nuvio-native-legacy/issues/204) | Automatic Trailer and LG Smart Magic Not Working | LG | bug | lancada | 1.7.0 | - | sem commit | nós 10-02 | nada (fechada) |
| [#205](https://github.com/iqui27/nuvio-native-legacy/issues/205) | Disappearing "Continue Watching" items and appearing "Resume | Samsung .tpk | bug | lancada | 1.7.0 | - | dc9ca3bf | autor 10-02 | nada (fechada) |
| [#206](https://github.com/iqui27/nuvio-native-legacy/issues/206) | Embedded subtitle and audio language list display during pla | Samsung .tpk | bug | lancada | 1.7.0 | - | 94690ba3 | autor 10-02 | nada (fechada) |
| [#208](https://github.com/iqui27/nuvio-native-legacy/issues/208) | Couldn't resume a movie from the tile | Samsung .tpk | bug | lancada | 1.7.0 | - | 39e4c794, e221d9e7 | autor 10-02 | nada (fechada) |
| [#209](https://github.com/iqui27/nuvio-native-legacy/issues/209) | Series Titles and Descriptions Displayed in English Despite  | LG | bug | lancada | 1.7.1 | - | 5658d0b0, 0da72c2a, f50b1915 | nós 10-02 | nada (fechada) |
| [#210](https://github.com/iqui27/nuvio-native-legacy/issues/210) | Username stucks on collapsed 'Modern sidebar' | Samsung .tpk | bug | lancada | 1.7.0 | - | 3e203fcf | autor 10-02 | nada (fechada) |
| [#211](https://github.com/iqui27/nuvio-native-legacy/issues/211) | LG UK6540PSB Crashes on startup. | LG | bug | lancada | 2.0.2 | - | 937f8e6b | nós 10-02 | nada (fechada) |
| [#212](https://github.com/iqui27/nuvio-native-legacy/issues/212) | Not being able to tell if I watched something or not. | LG | bug | lancada | 1.7.1 | - | 7a5896fa, 964bb741, 24a14ed0 | autor 10-03 | nada (fechada) |
| [#213](https://github.com/iqui27/nuvio-native-legacy/issues/213) | Continue watching shows incorrect titles | LG | bug | lancada | 1.7.1 | - | 7a5896fa, 8557e459, 880a4f76 | autor 10-03 | nada (fechada) |
| [#214](https://github.com/iqui27/nuvio-native-legacy/issues/214) | no carga el QR para iniciar sesión | LG | bug | lancada | 1.7.1 | - | sem commit | nós 10-02 | nada (fechada) |
| [#215](https://github.com/iqui27/nuvio-native-legacy/issues/215) | 1.7 introduced bugs on LG | LG | bug | lancada | 2.0.0 | - | b324c7d0, 0c3d2e4e, b6395e4c | nós 10-02 | nada (fechada) |
| [#216](https://github.com/iqui27/nuvio-native-legacy/issues/216) | Addition of touch controls | all | feature | lancada | 1.7.2 | - | 941ac3f0, 80bb3dd2, 7a9cf25b | autor 10-03 | nada (fechada) |
| [#221](https://github.com/iqui27/nuvio-native-legacy/issues/221) | Sources take too long to fetch links | Samsung .tpk | bug | lancada | 2.0.0 | - | c7278fc7, 98607591, 8ad11beb | autor 10-03 | nada (fechada) |
| [#222](https://github.com/iqui27/nuvio-native-legacy/issues/222) | Discord Rich Presence Integration | all | feature | lancada | 1.7.4 | - | 1ea1510b | autor 10-03 | nada (fechada) |
| [#223](https://github.com/iqui27/nuvio-native-legacy/issues/223) | [Android TV] Blank / black screen on launch after "Preparing | ? | bug | lancada | 2.0.0 | - | 9ca29519, 665646f6, 105e759b | nós 10-06 | nada (fechada) |
| [#224](https://github.com/iqui27/nuvio-native-legacy/issues/224) | WebOS crashing | LG | bug | lancada | 1.7.2 | - | 67401c73 | nós 10-03 | nada (fechada) |
| [#225](https://github.com/iqui27/nuvio-native-legacy/issues/225) | App does not launch after updating | LG | bug | lancada | 1.7.2 | - | 67401c73 | nós 10-03 | nada (fechada) |
| [#226](https://github.com/iqui27/nuvio-native-legacy/issues/226) | alt/old icon | ? | feature | lancada | 2.0.1 | - | sem commit | autor 10-07 | nada (fechada) |
| [#227](https://github.com/iqui27/nuvio-native-legacy/issues/227) | Adding localized movie/series title somewhere | LG | feature | lancada | 1.7.4 | - | sem commit | nós 10-03 | nada (fechada) |
| [#228](https://github.com/iqui27/nuvio-native-legacy/issues/228) | Auto trailer doesn't work | Samsung .tpk | bug | lancada | 2.0.1 | - | 3076c3a5, 1e03fc30 | autor 10-06 | nada (fechada) |
| [#229](https://github.com/iqui27/nuvio-native-legacy/issues/229) | Different IMDB score | Samsung .tpk | bug | lancada | 1.7.4 | - | sem commit | autor 10-03 | nada (fechada) |
| [#231](https://github.com/iqui27/nuvio-native-legacy/issues/231) | Option to disable Cinemata | Samsung (tpk/wgt?) | feature | lancada | 2.0.0 | - | d6495798 | nós 10-06 | nada (fechada) |
| [#232](https://github.com/iqui27/nuvio-native-legacy/issues/232) | Next episode thumbnail isn’t blurred | ? | ? | lancada | 2.0.0 | - | d6495798, 2495e39e | nós 10-06 | nada (fechada) |
| [#233](https://github.com/iqui27/nuvio-native-legacy/issues/233) | Nuvio account isn't synced by app | Samsung .tpk | bug | lancada | 2.0.0 | - | 3b625f58 | nós 10-06 | nada (fechada) |
| [#234](https://github.com/iqui27/nuvio-native-legacy/issues/234) | Customization options for app icon/splash screen, trailer UI | LG | feature | lancada | 2.0.0 | - | b8a900f5, 652b759d | nós 10-06 | nada (fechada) |
| [#235](https://github.com/iqui27/nuvio-native-legacy/issues/235) | Seeking | ? | ? | lancada | 2.0.0 | - | b8a900f5, 652b759d | nós 10-06 | nada (fechada) |
| [#237](https://github.com/iqui27/nuvio-native-legacy/issues/237) | Missing forward slash ('/') in TV channel source input (Stal | ? | bug | lancada | 2.0.0 | - | 8f8be06a, e5a848c3 | nós 10-06 | nada (fechada) |
| [#238](https://github.com/iqui27/nuvio-native-legacy/issues/238) | “Wait for add-ons” in playback is blocked by “depth effect” | ? | ? | lancada | 2.0.0 | - | d6495798, ebaef8ad, 73f93686 | nós 10-06 | nada (fechada) |
| [#239](https://github.com/iqui27/nuvio-native-legacy/issues/239) | Arabic Subtitle Letters Appear Disconnected | LG | bug | lancada | 2.0.0 | - | d00870f0, 6bb186ac | nós 10-06 | nada (fechada) |
| [#241](https://github.com/iqui27/nuvio-native-legacy/issues/241) | Trailer black bar are back | Samsung .tpk | bug | lancada | 2.0.2 | - | b5deeb25, 50df3f42, 9a0cce0f | autor 10-06 | nada (fechada) |
| [#243](https://github.com/iqui27/nuvio-native-legacy/issues/243) | IMDb rating missing on Continue Watching and a placeholder " | Samsung .tpk | bug | lancada | 2.0.0 | - | ccfe01a8, 8e7f4098 | nós 10-06 | nada (fechada) |
| [#244](https://github.com/iqui27/nuvio-native-legacy/issues/244) | Continue Watching: the same episode repeats 5 times and mark | Samsung .tpk | bug | lancada | 2.0.0 | - | d6495798, 8e7f4098 | nós 10-06 | nada (fechada) |
| [#245](https://github.com/iqui27/nuvio-native-legacy/issues/245) | Arabic subtitles not shaped/RTL — letters appear disconnecte | LG | bug | lancada | 2.0.0 | - | d00870f0, 6bb186ac | nós 10-06 | nada (fechada) |
| [#247](https://github.com/iqui27/nuvio-native-legacy/issues/247) | Arabic subtitle | LG | bug | lancada | 2.0.0 | - | a1cf2e46, d00870f0, 6bb186ac | nós 10-06 | nada (fechada) |
| [#249](https://github.com/iqui27/nuvio-native-legacy/issues/249) | [Feature]: Keep the current video full-size when the “Up Nex | ? | feature | lancada | 2.0.0 | - | 6c971d52 | autor 10-06 | nada (fechada) |
| [#253](https://github.com/iqui27/nuvio-native-legacy/issues/253) | Arabic subtitles/UI rendering as squares on Samsung Smart TV | Samsung .tpk | bug | lancada | 2.0.0 | - | a1cf2e46 | nós 10-06 | nada (fechada) |
| [#255](https://github.com/iqui27/nuvio-native-legacy/issues/255) | Home screen only displays 5 collections | Samsung .tpk | bug | lancada | 2.0.1 | - | 40bc9180 | nós 10-06 | nada (fechada) |
| [#258](https://github.com/iqui27/nuvio-native-legacy/issues/258) | Arabic subtitles not working | Samsung .tpk | bug | lancada | 2.0.0 | - | a1cf2e46 | nós 10-06 | nada (fechada) |
| [#261](https://github.com/iqui27/nuvio-native-legacy/issues/261) | Arabic Subtitles doesnt workining. | LG | bug | lancada | 2.0.0 | - | a1cf2e46 | nós 10-06 | nada (fechada) |
| [#262](https://github.com/iqui27/nuvio-native-legacy/issues/262) | Subtitle delay slider | ? | feature | lancada | 2.0.0 | - | sem commit | nós 10-06 | nada (fechada) |
| [#263](https://github.com/iqui27/nuvio-native-legacy/issues/263) | Add an option to toggle between 12-hour and 24-hour clock fo | Samsung (tpk/wgt?) | feature | lancada | 2.0.0 | - | sem commit | nós 10-06 | nada (fechada) |
| [#265](https://github.com/iqui27/nuvio-native-legacy/issues/265) | LG Smart TV – Freezing, Stuttering | LG | bug | lancada | 2.0.1 | - | sem commit | autor 10-07 | nada (fechada) |
| [#271](https://github.com/iqui27/nuvio-native-legacy/issues/271) | Intermittent App Crash When Navigating Settings | LG | bug | lancada | 2.0.0 | - | sem commit | autor 10-07 | nada (fechada) |
| [#272](https://github.com/iqui27/nuvio-native-legacy/issues/272) | Alternate App Icon Selection Does Not Change App Icon | LG | bug | lancada | 2.0.1 | - | sem commit | autor 10-06 | nada (fechada) |
| [#274](https://github.com/iqui27/nuvio-native-legacy/issues/274) | Arabic Language, TMDb Metadata, and Logos/Backdrops Issues | Samsung .tpk | bug | lancada | 2.0.1 | - | sem commit | autor 10-07 | nada (fechada) |
| [#275](https://github.com/iqui27/nuvio-native-legacy/issues/275) | Data Saving Option (Max/Min File Size Limit for Playback) | Samsung (tpk/wgt?) | feature | lancada | 2.0.1 | - | sem commit | nós 10-06 | nada (fechada) |
| [#276](https://github.com/iqui27/nuvio-native-legacy/issues/276) | Integrating QuickJS Engine into TPK Build to Enable External | Samsung (tpk/wgt?) | feature | lancada | 2.0.0 | - | sem commit | nós 10-06 | nada (fechada) |
| [#277](https://github.com/iqui27/nuvio-native-legacy/issues/277) | Addon not showing | LG | bug | lancada | 2.0.1 | - | sem commit | nós 10-06 | nada (fechada) |
| [#278](https://github.com/iqui27/nuvio-native-legacy/issues/278) | Arabic subtitle symbols not shown | LG | bug | lancada | 2.0.1 | - | sem commit | autor 10-07 | nada (fechada) |
| [#281](https://github.com/iqui27/nuvio-native-legacy/issues/281) | Auto-trailer doesn’t have sound on Tizen 9 | Samsung .tpk | bug | lancada | 2.0.1 | - | 3076c3a5 | nós 10-06 | nada (fechada) |
| [#282](https://github.com/iqui27/nuvio-native-legacy/issues/282) | Collection images/AIO regex patterns | Samsung .tpk | bug | lancada | 2.0.1 | - | sem commit | autor 10-06 | nada (fechada) |
| [#289](https://github.com/iqui27/nuvio-native-legacy/issues/289) | Eliminate “OK” button on profile pin entry | all | feature | lancada | 2.0.2 | - | 3eb5aef5 | nós 10-07 | nada (fechada) |
| [#295](https://github.com/iqui27/nuvio-native-legacy/issues/295) | Profile picker background — “Profile art” option not working | Samsung .tpk | bug | lancada | 2.0.2 | - | 881f7cae | nós 10-07 | nada (fechada) |
| [#297](https://github.com/iqui27/nuvio-native-legacy/issues/297) |  Issue: P2P stream stops due to full TV storage. | Samsung .tpk | bug | lancada | 2.0.2 | - | 325f4e2b, 1b6a5439 | nós 10-07 | nada (fechada) |
| [#308](https://github.com/iqui27/nuvio-native-legacy/issues/308) | [bug] subtitles block source | ? | bug | lancada | 2.0.3 | - | e52fae3f, 6269f306, b65c3324 | autor 10-09 | nada (fechada 09/10; renderizacao ASS segue na #394) |
| [#311](https://github.com/iqui27/nuvio-native-legacy/issues/311) | [suggestion] option to disable "from nuvio search" | ? | feature | lancada | 2.0.3 | - | 607b25bf, a86e55c2, 0d5ca11b | nós 10-08 | nada: o autor confirmou em 10/10 ("Fully functional verified, closing") e fechou |
| [#317](https://github.com/iqui27/nuvio-native-legacy/issues/317) | App doesn't  open on my LG webOS Tv UK6550PSB | LG | bug | lancada | 2.0.3 | - | 9a17b810, 8cf12920, d423b051 | nós 10-07 | - |
| [#318](https://github.com/iqui27/nuvio-native-legacy/issues/318) | screen flickers and stops responding when pressing OK (andro | Android | bug | lancada | 2.0.3 | - | 142ae407, 773658c8, 730556e1 | autor 10-08 | - |
| [#327](https://github.com/iqui27/nuvio-native-legacy/issues/327) | Hero doesn’t working / Catalog bug | Samsung .wgt | bug | lancada | 2.0.3 | - | 8ab1b19b, d268b069 | autor 10-07 | nada (fechada; saiu na v2.0.3) |
| [#359](https://github.com/iqui27/nuvio-native-legacy/issues/359) | 🐛 Bug Report: Floating Sidebar Overlaps/Interferes With Cont | ? | bug | lancada | 2.0.3 | - | 1860216b | autor 10-09 | nada (fechada) |
| [#371](https://github.com/iqui27/nuvio-native-legacy/issues/371) | [bug] Ui issue viewing "profile & stats" | Samsung .tpk | bug | lancada | 2.0.3 | - | b39f4149, 1cdd1cbb, 2fa6e51a | autor 10-08 | nada (fechada; saiu na v2.0.3) |

Notas:

- **#1**: commits em várias versões (1.0.7 a 1.1.2).
- **#2**: commits em várias versões (1.0.7 a 2.0.0).
- **#3**: commits em várias versões (1.0.7 a 1.1.2).
- **#4**: commits em várias versões (1.0.7 a 1.1.0).
- **#5**: commits em várias versões (1.0.7 a 2.0.0).
- **#6**: commits em várias versões (1.0.7 a 1.1.0).
- **#7**: commits em várias versões (1.0.7 a 2.0.0).
- **#10**: commits em várias versões (1.0.7 a 1.0.21).
- **#11**: commits em várias versões (1.0.8 a 1.0.31).
- **#12**: commits em várias versões (1.0.10 a 1.0.43).
- **#14**: commits em várias versões (1.0.10 a 1.0.16).
- **#16**: commits em várias versões (1.0.11 a 1.0.13).
- **#18**: commits em várias versões (1.0.11 a 1.4.2).
- **#19**: commits em várias versões (1.0.11 a 1.0.23).
- **#21**: commits em várias versões (1.0.24 a 1.5.2).
- **#22**: commits em várias versões (1.0.22 a 1.0.30).
- **#23**: commits em várias versões (1.0.32 a 1.0.36).
- **#25**: Sem commit #25; versão 1.0.30 citada na nossa resposta.
- **#29**: commits em várias versões (1.0.34 a 1.0.44).
- **#30**: commits em várias versões (1.0.31 a 1.0.34).
- **#31**: commits em várias versões (1.0.31 a 1.0.43).
- **#32**: Sem commit #32; versão 1.0.35 citada na nossa resposta.
- **#33**: commits em várias versões (1.0.39 a 1.0.41).
- **#34**: commits em várias versões (1.0.37 a 1.4.6).
- **#37**: commits em várias versões (1.0.42 a 1.6.0).
- **#42**: commits em várias versões (1.0.43 a 1.0.51).
- **#45**: commits em várias versões (1.0.44 a 1.6.5).
- **#49**: commits em várias versões (1.0.52 a 1.0.54).
- **#50**: Sem commit #50; versão 1.0.51 citada na nossa resposta.
- **#51**: Sem commit #51; versão 1.0.51 citada na nossa resposta.
- **#52**: Sem commit #52; versão 1.0.51 citada na nossa resposta.
- **#55**: commits em várias versões (1.0.55 a 1.3.2).
- **#56**: commits em várias versões (1.0.55 a 1.0.56).
- **#65**: commits em várias versões (1.2.0 a 1.2.1).
- **#67**: commits em várias versões (1.3.2 a 1.3.7).
- **#69**: commits em várias versões (1.3.2 a 1.3.4).
- **#82**: commits em várias versões (1.3.9 a 1.3.11).
- **#84**: commits em várias versões (1.3.8 a 1.4.7).
- **#92**: commits em várias versões (1.4.2 a 1.5.0).
- **#94**: commits em várias versões (1.3.12 a 1.5.1).
- **#95**: commits em várias versões (1.4 a 1.4.6).
- **#108**: Sem commit #108; versão 1.4.2 citada na nossa resposta.
- **#109**: commits em várias versões (1.4.1 a 1.4.3).
- **#117**: Sem commit #117; versão 1.4.3 citada na nossa resposta.
- **#119**: Sem commit #119; versão 1.4.6 citada na nossa resposta.
- **#121**: commits em várias versões (1.4.3 a 1.4.5).
- **#125**: Sem commit #125; versão 1.4.4 citada na nossa resposta.
- **#130**: commits em várias versões (1.4.4 a 2.0.2).
- **#131**: Sem commit #131; versão 1.4.6 citada na nossa resposta.
- **#133**: commits em várias versões (1.4.6 a 1.6.0).
- **#134**: Sem commit #134; versão 2.0.0 citada na nossa resposta.
- **#137**: commits em várias versões (1.4.6 a 1.5.4).
- **#138**: Substituída: .tpk Tizen 4/5 em toda release desde 1.6.0 (resposta); acompanhamento em #180.
- **#141**: commits em várias versões (1.5.0 a 1.5.3).
- **#142**: commits em várias versões (1.5.0 a 1.6.4).
- **#146**: Sem commit #146; versão 1.6.0 citada na nossa resposta.
- **#151**: commits em várias versões (1.5.1 a 1.5.3).
- **#158**: commits em várias versões (1.5.2 a 2.0.2).
- **#165**: commits em várias versões (1.5.4 a 1.7.0).
- **#169**: Sem commit #169; versão 1.6.0 citada na nossa resposta.
- **#172**: commits em várias versões (1.6.0 a 1.6.2).
- **#173**: Sem commit #173; versão 1.5.3 citada na nossa resposta.
- **#176**: commits em várias versões (1.6.0 a 1.7.0).
- **#177**: commits em várias versões (1.5.4 a 1.6.0).
- **#178**: commits em várias versões (1.5.4 a 1.6.3).
- **#179**: commits em várias versões (1.5.4 a 1.6.0).
- **#180**: commits em várias versões (1.5.4 a 1.6.0).
- **#187**: commits em várias versões (1.6.1 a 1.6.5).
- **#188**: commits em várias versões (1.6.3 a 2.0.2).
- **#193**: Sem commit #193; versão 1.7.0 citada na nossa resposta.
- **#194**: commits em várias versões (1.6.2 a 1.6.4).
- **#195**: commits em várias versões (1.6.3 a 2.0.2).
- **#199**: commits em várias versões (1.6.5 a 1.7.0).
- **#200**: commits em várias versões (1.6.5 a 1.7.0).
- **#201**: commits em várias versões (1.7.0 a 2.0.1).
- **#202**: commits em várias versões (1.7.0 a 2.0.2).
- **#203**: Sem commit #203; versão 1.7.0 citada na nossa resposta.
- **#204**: Sem commit #204; versão 1.7.0 citada na nossa resposta.
- **#209**: commits em várias versões (1.7.0 a 1.7.1).
- **#214**: Sem commit #214; versão 1.7.1 citada na nossa resposta.
- **#215**: commits em várias versões (1.7.1 a 2.0.0).
- **#221**: commits em várias versões (1.7.2 a 2.0.0).
- **#223**: commits em várias versões (1.7.2 a 2.0.0).
- **#226**: Sem commit #226; versão 2.0.1 citada na nossa resposta.
- **#227**: Sem commit #227; versão 1.7.4 citada na nossa resposta.
- **#228**: commits em várias versões (2.0.0 a 2.0.1).
- **#229**: Sem commit #229; versão 1.7.4 citada na nossa resposta.
- **#232**: commits em várias versões (1.7.4 a 2.0.0).
- **#241**: commits em várias versões (2.0.0 a 2.0.2).
- **#262**: Sem commit #262; versão 2.0.0 citada na nossa resposta.
- **#263**: Sem commit #263; versão 2.0.0 citada na nossa resposta.
- **#265**: Sem commit #265; versão 2.0.1 citada na nossa resposta.
- **#271**: Sem commit #271; versão 2.0.0 citada na nossa resposta.
- **#272**: Sem commit #272; versão 2.0.1 citada na nossa resposta.
- **#274**: Sem commit #274; versão 2.0.1 citada na nossa resposta.
- **#275**: Sem commit #275; versão 2.0.1 citada na nossa resposta.
- **#276**: Sem commit #276; versão 2.0.0 citada na nossa resposta.
- **#277**: Sem commit #277; versão 2.0.1 citada na nossa resposta.
- **#278**: Sem commit #278; versão 2.0.1 citada na nossa resposta.
- **#282**: Sem commit #282; versão 2.0.1 citada na nossa resposta.
- **#308**: O defeito do relato original (fonte com ASS que não abre e cai no CDN) está corrigido na 2.0.3: o autor confirma em 09/10 "got them working now". DECISÃO: a issue segue aberta no mapa, com status lancada e alvo 2.0.5, porque o autor trouxe sintomas NOVOS no mesmo fio: todas as legendas ASS com sombra/anel e cor diferente da original (exemplo do Stremio com anel branco), legendas grandes/avançadas lentas (baixo fps) e um clipe fora de sincronia (vídeos e captura anexados, não vistos por nós). SUSPEITA, sem leitura de código nem log: desenho do libass (sombra/contorno/cor) e custo de desenho das legendas grandes na TV. Se o dono preferir, fechar a #308 como resolvida e abrir issue nova para o desenho. FECHADA 09/10 com OK do dono: o problema original saiu na 2.0.3; os sintomas novos de renderizacao ASS (sombra/anel, cor, lentidao, dessincronia) foram para a #394.
- **#311**: Resposta corrigiu: opções de busca não estão na 2.0.2, saem na 2.0.3. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#317**: LG webOS 4 com pouca RAM; partes na 2.0.2 (0fcfa601, 937f8e6b). LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. FECHADA no GitHub pelo coordenador em 09/10. Origem: dono 09/10.
- **#318**: Tudo integrado em 2.0.3. O relator de queda nativa no Android < 12 (c8793c11 em agente/203-318-play) entrou como 142ae407, o mesmo commit com outro hash; tests/queda.sh passa em c3c1032e. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag. FECHADA no GitHub pelo coordenador em 09/10. Origem: dono 09/10.
- **#327**: Fechada no GitHub antes do release 2.0.3. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.
- **#359**: Fechada pelo autor em 09/10 com "Fixed". 1860216b está na tag v2.0.3.
- **#371**: Fechada no GitHub antes do release 2.0.3. LANÇADA na v2.0.3 (09/10): todos os commits do conserto estão contidos na tag.

## Fechado sem conserto (por-desenho, fora-do-escopo, duplicada, respondida, sem resposta)

38 issues.

| # | Título | Plat. | Tipo | Status | Release | Alvo | Conserto | Última resposta | Próximo passo |
|---|---|---|---|---|---|---|---|---|---|
| [#15](https://github.com/iqui27/nuvio-native-legacy/issues/15) | Bug: Trailer Does Not Play When Selected on Detail Page | ? | bug | respondida | - | - | - | autor 09-16 | nada (fechada) |
| [#20](https://github.com/iqui27/nuvio-native-legacy/issues/20) | Library does not populate from account | LG | bug | respondida | - | - | - | nós 09-08 | nada (fechada) |
| [#47](https://github.com/iqui27/nuvio-native-legacy/issues/47) | Feature request | ? | feature | respondida | - | - | - | nós 09-15 | nada (fechada) |
| [#53](https://github.com/iqui27/nuvio-native-legacy/issues/53) | Sources not showing up | ? | bug | respondida | - | - | - | autor 09-16 | nada (fechada) |
| [#58](https://github.com/iqui27/nuvio-native-legacy/issues/58) | Trakt Library titles are not appearing | Samsung (tpk/wgt?) | bug | por-desenho | - | - | - | nós 09-19 | nada (fechada) |
| [#59](https://github.com/iqui27/nuvio-native-legacy/issues/59) | "Meu Futebol" add-on not working | Samsung (tpk/wgt?) | bug | por-desenho | 1.2.1 (LG); Samsung sem solução | - | - | nós 09-19 | nada (fechada) |
| [#63](https://github.com/iqui27/nuvio-native-legacy/issues/63) | Support for Tizen 5 | Samsung (tpk/wgt?) | feature | por-desenho | - | - | - | nós 09-19 | nada (fechada) |
| [#64](https://github.com/iqui27/nuvio-native-legacy/issues/64) | no highcache version for 1.1.2? | ? | question | respondida | - | - | - | nós 09-19 | nada (fechada) |
| [#75](https://github.com/iqui27/nuvio-native-legacy/issues/75) | Request: settings menu navigation | ? | feature | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#81](https://github.com/iqui27/nuvio-native-legacy/issues/81) | Seek not functioning properly  | ? | bug | respondida | 1.3.7 | - | - | nós 09-20 | nada (fechada) |
| [#90](https://github.com/iqui27/nuvio-native-legacy/issues/90) | feature request | ? | feature | respondida | - | - | - | nós 09-23 | nada (fechada) |
| [#91](https://github.com/iqui27/nuvio-native-legacy/issues/91) | MP4 container toggle | ? | feature | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#96](https://github.com/iqui27/nuvio-native-legacy/issues/96) | Request | Samsung (tpk/wgt?) | feature | respondida | - | - | - | autor 09-27 | nada (fechada) |
| [#98](https://github.com/iqui27/nuvio-native-legacy/issues/98) | Dunno how github does its uploads but the there's no release | ? | ? | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#107](https://github.com/iqui27/nuvio-native-legacy/issues/107) | Not searching files from addon | ? | bug | precisa-log | - | - | - | autor 09-23 | nada (fechada) |
| [#140](https://github.com/iqui27/nuvio-native-legacy/issues/140) | trailer no sound | ? | ? | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#148](https://github.com/iqui27/nuvio-native-legacy/issues/148) | Sudden crash by moving through the app | ? | bug | precisa-log | - | - | - | autor 09-27 | nada (fechada) |
| [#154](https://github.com/iqui27/nuvio-native-legacy/issues/154) | Relatório de Logs | ? | meta | por-desenho | - | - | - | nós 09-27 | nada (relatório automático de logs) |
| [#155](https://github.com/iqui27/nuvio-native-legacy/issues/155) | Thanks for building this! | LG | meta | respondida | - | - | - | autor 10-06 | nada (fechada) |
| [#161](https://github.com/iqui27/nuvio-native-legacy/issues/161) | Relatório de logs | ? | meta | por-desenho | - | - | - | nós 09-30 | nada (relatório automático de logs) |
| [#167](https://github.com/iqui27/nuvio-native-legacy/issues/167) | Tizen 4.0 | Samsung (tpk/wgt?) | ? | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#168](https://github.com/iqui27/nuvio-native-legacy/issues/168) | Trailer plays without sound | ? | bug | fechada-sem-resposta | - | - | - | sem comentários | nada (fechada) |
| [#183](https://github.com/iqui27/nuvio-native-legacy/issues/183) | trailer resolution | ? | ? | respondida | - | - | - | nós 09-30 | nada (fechada) |
| [#189](https://github.com/iqui27/nuvio-native-legacy/issues/189) | Actors UI | ? | feature | respondida | - | - | - | autor 09-30 | nada (fechada) |
| [#192](https://github.com/iqui27/nuvio-native-legacy/issues/192) | Relatório de logs | ? | meta | por-desenho | - | - | 77227ac1, b61d74a6 | nós 10-07 | nada (relatório automático de logs) |
| [#217](https://github.com/iqui27/nuvio-native-legacy/issues/217) | Building on a Linux host: three small blockers (and one targ | ? | bug | respondida | - | - | - | nós 10-02 | nada (fechada) |
| [#240](https://github.com/iqui27/nuvio-native-legacy/issues/240) | tardan demasiado en cargar una serie o pelicula | LG | bug | precisa-log | - | - | - | autor 10-06 | nada (fechada) |
| [#242](https://github.com/iqui27/nuvio-native-legacy/issues/242) | API QUESTION | ? | question | respondida | - | - | - | nós 10-06 | nada (fechada) |
| [#248](https://github.com/iqui27/nuvio-native-legacy/issues/248) | Catalog sequencing mismatched | Samsung .tpk | bug | por-desenho | - | - | - | autor 10-04 | nada (fechada) |
| [#254](https://github.com/iqui27/nuvio-native-legacy/issues/254) | Home row does not update properly after installing/removing  | Samsung .tpk | bug | respondida | 2.0.0 | - | - | nós 10-06 | nada (fechada) |
| [#256](https://github.com/iqui27/nuvio-native-legacy/issues/256) | Add Autoplay for Next Episode & Allow Customizable "Up Next" | ? | feature | respondida | - | - | - | autor 10-08 | nada (fechada pelo autor em 09/10); a opção de ajustar o tempo do card segue só como recur |
| [#257](https://github.com/iqui27/nuvio-native-legacy/issues/257) | FYI: included in Tizen Community Packages | Samsung (tpk/wgt?) | meta | respondida | - | - | - | nós 10-06 | nada (fechada) |
| [#267](https://github.com/iqui27/nuvio-native-legacy/issues/267) | No blue button on newer Samsung remote | Samsung (tpk/wgt?) | feature | por-desenho | - | - | - | autor 10-06 | nada (fechada) |
| [#270](https://github.com/iqui27/nuvio-native-legacy/issues/270) | 📊 Relatório de logs | ? | meta | por-desenho | - | - | d560db8c, 666428cc | nós 10-07 | nada (relatório automático de logs) |
| [#279](https://github.com/iqui27/nuvio-native-legacy/issues/279) | Match framerate | Samsung (tpk/wgt?) | feature | fora-do-escopo | - | - | - | autor 10-06 | nada (fechada) |
| [#309](https://github.com/iqui27/nuvio-native-legacy/issues/309) | [suggestion] sources formatter | ? | feature | por-desenho | - | - | - | autor 10-07 | nada (fechada) |
| [#325](https://github.com/iqui27/nuvio-native-legacy/issues/325) | Add Arabic Language Support | all | feature | duplicada | 2.1 | - | - | autor 10-07 | nada (fechada) |
| [#336](https://github.com/iqui27/nuvio-native-legacy/issues/336) | App wont launch | Samsung .tpk | meta | por-desenho | - | - | - | autor 10-08 | nada (relatório automático de logs) |

Notas:

- **#15**: Resposta: plano de passar o trailer ao app de YouTube da TV; rawldon confirmou depois que toca (com atraso). Sem commit #15.
- **#20**: "Fixed on master; next release" (08/09); nenhum commit cita #20.
- **#58**: Salvos = união da lista Nuvio e Trakt watchlist, de propósito.
- **#59**: LG manda Referer desde 1.2.1; AVPlay da Samsung não permite header.
- **#63**: Tizen 5.0 sem WebAssembly threads; depois o .tpk 4/5 (1.6.0) resolveu por outro caminho.
- **#81**: v1.3.7 apenas registra o tempo de seek na Samsung (diagnóstico).
- **#90**: "Not in 1.4.2 yet — still on the list".
- **#189**: "Vou tentar ajustes em breve", sem commit #189.
- **#217**: Contribuidor oferece PRs de build em Linux; resposta aceita.
- **#248**: Resposta diz que é comportamento da plataforma Tizen.
- **#254**: Resposta: busca atualiza ao mudar add-ons na 2.0; a parte da Home não confirmada.
- **#256**: Parte do pedido tem relação com #331 (contagem estilo Netflix, em agente/203-331 = 2.0.5). Ajuste de tempo do card "ainda não existe" (nossa resposta). Fechada pelo autor em 09/10 (COMPLETED) depois de confirmar que o card aparece nos créditos e o próximo episódio toca sozinho; o pedido de ajuste de tempo fica sem issue própria (roadmap 2.0.5, item do próximo episódio automático).
- **#267**: Resposta: CH+/CH- já atribuídos.
- **#279**: Tizen não tem API pública para trocar a taxa de atualização.
- **#309**: Já existe: Fontes e add-ons > Texto da fonte.
- **#325**: Duplicata de #250/#260.

## Auditoria de validações

Quem validou o quê, em que ref, com que resultado e onde está a prova; mais recente primeiro. Registro começa em 2026-10-09; validações anteriores estão só nos commits e nas notas.

| Data | Quem | O que | Issues | Ref | Resultado | Evidência |
|---|---|---|---|---|---|---|
| 2026-10-09 23:50 | Codex gpt-6-astra (revisão) | Revisão de confirmação da rodada 4 do nível de GPU | #410 | agente/204-gpunivel410 52dbde71 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/410b-r4-codex.txt: P2 residual resolvido, sem achados novos |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | QR do Discord colorido com logo: leitura | - | agente/204-novidades cecbe108 | passou | zxing-cpp leu os 3 códigos (Discord, Ko-fi, Patreon) na captura do cartão, inclusive borrado/escurecido e em metade do tamanho; capturas em /Volumes/ExternalSSD/tmp/nov204-shots4 |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | IntroDB na TCL | - | APK fd86a673 | passou | log 23:01: '[intro] 2 marcadores (tmdb)' e '[credits] source=introdb'; os 0 marcadores eram de tt6048596, que a API responde 'media not found' |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Spotlight: busca 'silo' na TCL | - | APK b8d07d3d | passou | Silo 2023 (série do dono) virou melhor resultado; antes era o documentário de 2015; revisão Codex em 3 rodadas, última sem achados (/Volumes/ExternalSSD/nv-203-tmp/rev/spotlight-r3-codex.txt) |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Vistos do Silo depois de reinstalar por cima | - | APK b8d07d3d | passou | '[vistoep] tt14688458: grafico ... vistos=22/30 T1=10/10 T2=5/10 T3=7/10': igual a antes da reinstalação, nada voltou |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Gráfico 'Seu progresso' remontando a cada chamada | - | APK b8d07d3d -> ce4426f9 | passou | antes: ~1900 linhas/s de '[vistoep] grafico' na página do Silo (cache nunca batia para id com :T:E); depois do conserto 6c6d1668: 3 linhas na abertura, FPS 60.0; teste FAIL->PASS do Codex |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Biblioteca 129 salvos x 8 na grade | - | APK 710541ab -> 25e9c3aa | passou | antes: '[biblioteca] ocultar=1 excl_meta=121 grade=8'; depois do conserto 251d5e0f: 'excl_meta=0 grade=129', tela mostra '129 titles'; Codex sem achados (/Volumes/ExternalSSD/nv-203-tmp/rev/bib129-fix-codex.txt). Não era regressão: biblioteca.c idêntico ao da v2.0.3 |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Reprodução Dolby Vision na TCL (Silo T2E6) | #409 | APK ce4426f9 | passou | abre em 11,5 s, hdr=DolbyVision, 3 recriações da superfície registradas pelo [dvtrace], sem erro; saída limpa. Brilho não dá para medir por captura: o relato 'abre escuro' segue sem prova |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Ajuste 'Source order' presente | #400 | APK ce4426f9 | passou | Ajustes > Playback > More options: 'Source order: By quality / From the addon' com cena; captura t14 |
| 2026-10-09 23:50 | Claude Opus 5.5 (sessão de coordenação), TCL via adb | Suíte completa (em andamento, árvore mudou no meio) | - | integracao/2.0.3.1 710541ab..25e9c3aa | inconclusivo | /Volumes/ExternalSSD/tmp/suite-204-7105.log; falhas até agora: cachearte-wasm (módulo node ausente), central_rotulos e colecoes_teto (não linkam src/dts; video.c já chamava dts_overlay_draw na v2.0.3, não é regressão) |
| 2026-10-09 22:50 | Codex gpt-6-astra (revisão) | Revisão de confirmação da rodada 3 do #409 | #409 | agente/204-decoder409 c51dc9ec | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/409-r3-codex.txt: sem achados P1/P2 |
| 2026-10-09 22:50 | Codex gpt-6-astra (revisão) | Revisão de confirmação da rodada 2 do #412 | #412 | agente/204-tpkpreso412 6ec34f52 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/412-r2-codex.txt: sem achados P1/P2; sem TV física |
| 2026-10-09 22:50 | Codex gpt-6-astra (revisão) | Revisão de confirmação da rodada 3 do prefetch do próximo episódio | - | agente/204-proxprefetch 61b7db4d | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/prox-r3-codex.txt: sem achados P1/P2 |
| 2026-10-09 22:50 | Codex gpt-6-astra (revisão) | Revisão do cartão de novidades com Discord + QR | - | agente/204-novidades c0c2d1a8 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/nov-r3-codex.txt: sem achados P1/P2; capturas em /Volumes/ExternalSSD/tmp/nov204-shots3 conferidas pela coordenação |
| 2026-10-09 22:50 | Codex gpt-6-astra (revisão) | Revisão de confirmação da rodada 3 do nível de GPU | #410 | agente/204-gpunivel410 db3c1242 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/410b-r3-codex.txt: P2 residual (quadros alternando 1200/900 ms nunca terminam a referência). Rodada 4 pedida. |
| 2026-10-09 22:50 | Claude Opus 5.5 (sessão de coordenação) | Testes-alvo na integração depois dos 4 merges | #409, #412 | integracao/2.0.3.1 fd86a673 | passou | decoder409, proxprefetch, addonslista, tpk-preso, stream_parser, android_decoder409.py, novidades_cartao: rc=0 |
| 2026-10-09 22:50 | Claude Opus 5.5 (sessão de coordenação) | Onboarding do Discord ativado | - | guild 1558285501067296922 | passou | 9 canais padrão, aviso de boas-vindas, 3 tarefas (rules, bugs-and-issues, announcements); 'Onboarding is Enabled'. Ícone: dono sobe à mão. |
| 2026-10-09 22:35 | Codex gpt-6-astra (revisão) | Revisão da rodada 2 do nível de GPU adaptativo | #410 | agente/204-gpunivel410 020fb8f8 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/410b-r2-codex.txt: 2 P2 (Mali-400 sem saída segura; interrupção perde reavaliação do legado). Rodada 3 pedida. |
| 2026-10-09 22:35 | Codex gpt-6-astra (revisão) | Revisão da rodada 2 do decoder 4K / espera do release | #409 | agente/204-decoder409 6d46e336 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/409-r2-codex.txt: 1 P2 (volume volta a 100% após a espera) + 1 P3 de teste. Rodada 3 pedida. |
| 2026-10-09 22:35 | Codex gpt-6-astra (revisão) | Revisão do conserto do player preso no .tpk | #412 | agente/204-tpkpreso412 71c7d615 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/412-codex.txt: P1 (Voltar durante PrepareAsync encerra o processo em TV sã). Rodada 2 pedida. |
| 2026-10-09 22:35 | Claude Opus 5.5 (sessão de coordenação) | Conferência mecânica da documentação de funções (identificadores citados existem no código) | - | agente/doc-funcoes 29a340b3 | passou | 490 de 493 identificadores existem; os 3 errados corrigidos à mão (player.md, trakt.md, video.md) |
| 2026-10-09 22:35 | Claude Opus 5.5 (sessão de coordenação) | Servidor Discord reestruturado e convite permanente gerado | - | guild 1558285501067296922 | passou | 5 categorias e 10 canais novos conferidos pela lista de canais; convite https://discord.gg/9NWr6SHyzJ com 'nunca irá expirar'. Pendente: dono apagar os 10 canais antigos (Canais de Texto/Voz). |
| 2026-10-09 22:00 | Claude Opus 5.5 (sessão de coordenação) | Implantação da API de respostas do painel no ZimaOS | - | f9fa03b2 + painel v3 | passou | /api/saude {"ok": true}; containers versionados no ar, nuvio-painel antigo só parado; publicar.sh rc=0 |
| 2026-10-09 21:51 | Codex gpt-6-astra (revisão estática) | #409 rodada 1 (decoder 4K, bloqueio por codec, Voltar) | #409 | agente/204-decoder409 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/409-codex.txt (3 P2: erro de áudio bloqueia vídeo; codec desconhecido = HEVC; falha <2160p não bloqueia) — rodada 2 em andamento |
| 2026-10-09 21:51 | Codex gpt-6-astra (revisão estática) | Pré-carregamento do próximo episódio rodada 1 | - | agente/204-proxprefetch | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/prox-codex.txt (3 P2: TTL expira antes da contagem; cancelado conta como mudo; cacheia sem plugins) — rodada 2 em andamento |
| 2026-10-09 21:51 | Codex gpt-6-astra (revisão estática) | #410 parte 2 rodada 1 (nível de GPU volta se não ganhar fps) | #410 | agente/204-gpunivel410 | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/410b-codex.txt (3 P2: GPU fraca grava nível 1 sem medir 0; LG zera nível salvo e nunca reavalia; interrupção compara cenas diferentes) — rodada 2 em andamento |
| 2026-10-09 21:30 | Codex gpt-6-astra (revisão estática) | #410 parte 1 e #411 | #410, #411 | cc8bbdcd, a066e6c8 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/410-codex.txt e 411-codex.txt (no findings) |
| 2026-10-09 21:00 | Claude Opus 5.5 (sessão de coordenação) | Leitura do código: o perfil depende só do Trakt (relato de usuário com Simkl) | - | integracao/2.0.3.1 | passou | src/app.c carregarPerfil -> trakt_perfil; sem trakt_ativo() o estado é "Trakt desconectado" |
| 2026-10-09 20:40 | Codex gpt-6-astra (revisão estática) | OpenSubtitles "sem resposta": versão só-leitura, rodadas 2 e 3 | - | agente/204-sostream | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/sostream2-codex.txt e sostream3-codex.txt (P2 de leitura sem sincronização e 200 vazio corrigidos; último P2 = republicação pela descoberta, corrigido com atômico em fonte; limite documentado: sonda terminando depois do resumo) |
| 2026-10-09 20:35 | Claude Opus 5.5 (sessão de coordenação) | Teste do add-on só de legenda com a sonda terminando no meio da busca (reproduz "OpenSubtitles v3 não respondeu" da TCL) | - | ee2c64eb (FAIL) -> agente/204-sostream (PASS) | passou | tests/addonslista.sh: 2 pedidos de stream e motivo "OpenSubtitles v3 não respondeu" antes; 1 pedido e motivo vazio depois; autoplay_alvo e addonurl rc=0 |
| 2026-10-09 20:30 | Codex gpt-6-astra (pesquisa) | Estudo do app oficial NuvioTV Android 1.1.0-beta.5 (add-ons, plugins, ordem das fontes, player, abertura, HDR) | #400 | NuvioMedia/NuvioTV 6adf0251; APK sha256 09865e1a… | passou | /Volumes/ExternalSSD/tmp/nuvio-oficial/relatorio.md (oficial preserva a ordem interna do add-on; não recria a superfície no HDR; buffer padrão 50 s; plugins até 10 scrapers) |
| 2026-10-09 20:20 | Codex gpt-6-astra (revisão estática) | OpenSubtitles "sem resposta": 1a versão (manifesto lido no fio da busca) | - | 4f5780ba (agente/204-sostream, descartado) | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/sostream-codex.txt (P1 corrida no parser do manifesto; P2 tentativa em andamento; P2 cancelamento) |
| 2026-10-09 19:40 | Claude Opus 5.5 (sessão de coordenação) | Logs da KM7 SE do relato do Reddit (15 sessões) e contagem de quedas nativas no Android por versão (D1 ids 62000..66300) | - | v2.0.2 e v2.0.3 (Android) | inconclusivo | D1 66008/66017 (ExoPlayer AudioTrack.getTimestamp), 65736 (GC da ART); Android geral: 2.0.2 70/391 e 219/1192 com queda, 2.0.3 25/400; KM7: 2.0.2 4/49, 2.0.3 5/13; arquivos em /Volumes/ExternalSSD/tmp/km7 |
| 2026-10-09 19:30 | Claude Opus 5.5 (sessão de coordenação) | Busca da sessão da LG 32LJ600B (webOS 3) do relato do Reddit no D1 | - | v2.0.3 (webOS) | inconclusivo | D1 id>64500: nenhuma linha com LJ600; webOS 3.9.3 na 2.0.3: 65SJ800V (65304, 65363, 65374 arranque queda), OLED55B7P (65317), 49UJ6300 (66016) |
| 2026-10-09 19:25 | Claude Opus 5.5 (sessão de coordenação) | Instalação da 2.0.4 (com KM7, consertos do #408) na TCL e na C9: abre, linha [tv] app=2.0.4, sem queda | - | APK de 06419b6b; ipk de bed3534c | passou | TCL logcat: "[player] recriar superficie no HDR: sim" (TCL segue recriando); C9 /tmp/nuvio.log: OLED65C9PSA webos-4.10.2 app=2.0.4, 0 SIGSEGV |
| 2026-10-09 19:20 | Codex gpt-6-astra (revisão estática) | Consertos das revisões do #408 (gancho webOS, nyx x starfish, aviso do modelo de pôster) | #390 | fc61b3aa (agente/204-ultranit) | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/ultranit-codex.txt (no findings); testes FAIL->PASS: webosver, poster_modelo_longo |
| 2026-10-09 19:10 | Claude Opus 5.5 (sessão de coordenação) | #387: testes do segurar e testes que compilam as telas tocadas | #387 | 0f98379f (agente/205-segurar) | passou | tests/segurar.sh (caso "mesmo quadro" falha na r1); ilha_shot, ctxmenu_contract, busca_debounce, avisos_toast_shot, salvospainel_perf rc=0; ctxlista falha igual na base |
| 2026-10-09 19:05 | Claude Opus 5.5 (sessão de coordenação) | Vídeo do #387 (menu do cartaz abre em vez da página do título) e 30 logs .tpk recentes procurando OK sem mapa | #387 | v2.0.2/v2.0.3 (.tpk) | inconclusivo | vídeo do autor: menu em 3 de 4 OKs, <0,5 s, capas já carregadas; logs D1 .tpk: só volume/power/exit sem mapa, o OK está mapeado; NV_HOLD_MS=700 (layout.h:119); a Biblioteca não loga o segurar |
| 2026-10-09 19:05 | Codex gpt-6-astra (revisão estática) | Conserto KM7: Amlogic não recria a superfície de vídeo | - | 98929593 (agente/204-km7) | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/km7-codex.txt (no findings; nota: log de "recria" enganoso, corrigido em ccd37565; chama de mitigação candidata, sem prova em aparelho) |
| 2026-10-09 19:05 | OpenCode glm-5.3 (revisão estática) | Conserto KM7: Amlogic não recria a superfície de vídeo | - | 98929593 | inconclusivo | /Volumes/ExternalSSD/nv-203-tmp/rev/km7-opencode.txt (parou sem veredito) |
| 2026-10-09 19:00 | GitHub Copilot (revisão do PR) | Revisão do PR #408 | #390 | 68e89c70 | falhou | PR #408: card de novidades ainda 2.0.3; webosver.c:197 nyx descartado por erro no starfish (corrigido e6da2772); ajustes.c:3414 aviso errado do modelo longo (corrigido fc61b3aa) |
| 2026-10-09 19:00 | Codex gpt-6-astra (revisão estática) | #387 r1: segurar OK com espera após pausa longa | #387 | 03213598 (agente/205-segurar) | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/387-codex.txt (P2 relógio compartilhado entre telas; P2 menu/guia/spotlight de fora; P3 log a cada quadro) — corrigidos em 0f98379f |
| 2026-10-09 18:58 | Claude Opus 5.5 (sessão de coordenação) | Teste do conserto KM7 (RecriaHdr): Amlogic e MStar dispensados, TCL c2.mtk recria | - | f354b7f6 (FAIL) -> 98929593 (PASS) | passou | tests/android_recria_hdr.py; tests/android_window_layout.py segue passando |
| 2026-10-09 18:50 | Claude Opus 5.5 (sessão de coordenação) | Log ABB125 do #405 (busca rápida sem Silo) lido | #405 | v2.0.3 (.tpk) | inconclusivo | D1 registro 66134: nenhuma linha da busca rápida |
| 2026-10-09 18:43 | Claude Opus 5.5 (sessão de coordenação) | Build do .ipk da LG 2.0.4 com as chaves do local.properties (o primeiro saiu sem chaves e foi descartado) | - | 68e89c70 | passou | /Volumes/ExternalSSD/tmp/b204-lg.log sem "aviso: vazio"; space.nuvio.native.legacy_2.0.4_arm.ipk em nv-2031-int |
| 2026-10-09 18:40 | ultrareview (nuvem, multiagente) | Revisão do PR #408 (2.0.4 sobre v2.0.3) | #344, #372, #378, #390, #392, #402 | 68e89c70 | passou | https://github.com/iqui27/nuvio-native-legacy/pull/408 — sem bug; 3 nits: gancho nv_webos_testar sem guarda (corrigido da6bece2), guarda duplicada em catalogo.c e separadores do #402 em 3 lugares (refatoração, 2.0.5) |
| 2026-10-09 18:38 | Claude Opus 5.5 (sessão de coordenação) | Integração da 2.0.4 depois dos merges do #402 e das notas: testes-alvo | #402 | 68e89c70 (integracao/2.0.3.1) | passou | tests/fhd402.sh e tests/stream_parser.sh rc=0 no worktree nv-2031-int |
| 2026-10-09 18:36 | OpenCode glm-5.3 (revisão estática) | Revisão r5 do #402 (faixa E2 80..AF) | #402 | b781dae1 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/402r5-opencode.txt (dois P3: borda U+2BFF sem teste e comentário; resolvidos em eb9f3226) |
| 2026-10-09 18:33 | Codex gpt-6-astra (revisão estática) | Revisão r5 do #402 (faixa E2 80..AF) | #402 | b781dae1 | passou | /Volumes/ExternalSSD/nv-203-tmp/rev/402r5-codex.txt (no findings) |
| 2026-10-09 18:30 | Claude Opus 5.5 (sessão de coordenação) | Teste do #402 com letras de U+2C00..U+2FFF antes de FHD: falha antes do conserto, passa depois; stream_parser segue passando | #402 | 077fd275 (teste, FAIL) -> b781dae1 (conserto, PASS) | passou | tests/fhd402.sh rc=1 em 077fd275, rc=0 em b781dae1; tests/stream_parser.sh rc=0 |
| 2026-10-09 18:25 | Claude Opus 5.5 (sessão de coordenação) | Relato do Reddit "2.0.3 trava ao tocar série (Android TV)": varredura das 40 sessões 2.0.3 Android mais recentes no D1 | - | v2.0.3 | nao-reproduziu | D1 registro 66138..66221; quadros de 10-75 s (66213, 66219) são app em segundo plano (ev=); 66144 pausou e perdeu a superfície; erros 2001/2004 em 66220 são canais ao vivo; o autor não estava nessa amostra: é a KM7 (ver entrada de 19:40) |
| 2026-10-09 18:15 | Claude Opus 5.5 (sessão de coordenação) | Teste do APK 2.0.4 na TCL do dono: Home, página do título, The Mentalist S1E2 tocando 3+ min, avanço no vídeo, relógio da legenda e RSS estáveis | - | APK 2.0.4 de 18:03 (integracao/2.0.3.1 antes do merge do #402) | passou | logcat da TCL 192.168.1.128 (sem queda no buffer de crash; [relogio] estável; rss ~600 MB) |
| 2026-10-09 17:42 | Codex gpt-6-astra (revisão estática) | Revisão r4 do #402: prefixo só de símbolo antes de FHD | #402 | 824e9e9d (agente/204-fhd402) | falhou | /Volumes/ExternalSSD/nv-203-tmp/rev/402r4-codex.txt (P2: letras georgianas U+2D0B/U+2D04 aceitas como símbolo); conserto em b781dae1 |

## Fora do GitHub (Reddit)

### Relatório Shield (22 itens, Reddit)

- Plataforma: Android (Shield)
- Status: desconhecido
- Release: 2.0.3 (parcial)
- Conserto: d30344ab/96ccdab1 (#6 Shield: tetos do Android e paginação da watchlist do Trakt, 2.0.3; 7b9e641a, o revert do 96ccdab1, não está em nenhuma branch); 5413db8a (Shield #19: guia "Mostrando N de M canais", Android guarda 3000 canais/128 categorias, 2.0.3); 6b3aaae1 (Shield 318.3: card do Continuar com o still do episódio; filme assistido diz Reproduzir, 2.0.3); eabe3afd/b03aa2f0/35421d1a/73c4ab4c (#266/#332: curl fora do fio principal, /dev/urandom, login por HttpURLConnection, 2.0.3)
- Próximo passo: mapeado em 09/10: o relatório de 20 itens do hboinay (provável versão do de 22) está nas entradas reddit-hboinay-NN abaixo; postar o rascunho docs/issues/rascunhos/reddit-hboinay-2026-10-09.md
- Notas: O texto dos 22 itens NÃO está no repositório nem nas issues. Só três grupos de commits citam "Shield". Mapeamento item a item: desconhecido. ATUALIZAÇÃO 09/10: chegou um relatório de 20 itens do mesmo autor e da mesma build (teste-318.3); cada item virou uma entrada reddit-hboinay-NN (ou uma nota em #353/#356). Falta confirmar se é o mesmo relatório dos 22.

### Relatório Android TV (9 itens) = #369

- Plataforma: Android
- Status: consertada-nao-lancada (parcial)
- Release: 2.0.3
- Conserto: 712557ca (tailandês); 5f04c36b (ocultar não lançados)
- Próximo passo: responder no #369 e listar os 7 itens sem commit
- Notas: Só 2 dos 9 itens têm commit identificado. #369 não tem resposta nossa.

### Samsung: faixa de áudio escondida pela TV (DTS/TrueHD)

- Plataforma: Samsung .tpk (e .wgt na fase 1)
- Status: consertada-nao-lancada (casar faixa) + planejado (aviso)
- Release: 2.0.3 (casar faixa); 2.0.5 (aviso, fase 1)
- Conserto: 4112f3f7 + efd1014b (agente/203-casarfaixa, em integracao/2.0.3): TV com menos faixas que o MKV casa por idioma, codec e canais; 0ef8ddc9 (agente/204-samsung-dts): só o plano docs/plans/samsung-dts.md, sem código; f45ddc14 (2.0.2, #313): aviso só quando TODAS as faixas são recusadas
- Próximo passo: implementar fase 1 do plano (aviso + faixa cinza na folha de áudio, 1,5-2 dias); fase 2 (ESPlayer) depende de teste de assinatura Partner em TV
- Notas: Relato do Reddit: 2 faixas no box, 1 na TV. Relacionado a #313. ESPlayer exige privilégio Partner (no 4/5 isso já quebrou a instalação, 118014).

### Samsung: pacote de 80 GB

- Plataforma: Samsung
- Status: desconhecido
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: perguntar ao dono o que é
- Notas: Nada encontrado em commits, docs, notas de release ou issues (buscas por "80 GB"/"80GB"). Não preencher sem a fonte.

### Home: mais de 40 fileiras e ordem dos catálogos (hboinay item 1)

- Plataforma: Android
- Status: precisa-log
- Release: alvo sugerido 2.0.5 (teto) / sem alvo (ordem)
- Conserto: 8e058817 (agente/204-fileiras-android, não integrado): Android com fileiras ilimitadas, teto 200
- Próximo passo: teto: integrar agente/204-fileiras-android na 2.0.5 (já no roadmap); ordem: pedir log
- Notas: TETO por desenho: CAT_FIL_MAX 40 (catalogo.h:394) e FIL_LIMITE_MAX 40 (fileiras.h:60); a branch 204-fileiras-android o sobe para 200 só no Android. ORDEM: não há como afirmar sem log; ligação provável com #358 (catálogos que a Home move para "Fora da Home", 29c640b7 em agente/204-conta-catalogos). Pedir o log para ver a ordem da conta contra a da Home. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Continuar com fonte Nuvio mostra 5-6 itens e depois 1; a conta tem 30 (hboinay item 3)

- Plataforma: Android
- Status: precisa-log
- Release: 2.0.3 (parcial, 507d3a6b); teto alvo sugerido 2.0.5
- Conserto: 507d3a6b (v2.0.3): fonte Conta semeia o "a seguir" mesmo com Trakt/Simkl, descarta vistos velhos
- Próximo passo: pedir log da 2.0.3 (Ajustes > Sobre > Enviar log)
- Notas: O teto de cada fonte é CONT_MAX 12 (descoberta.c:2605), então 30 na conta nunca aparecem inteiros, e o "a seguir" ainda passa por filtros (episódio visto em outra fonte, série parada há mais de 60 dias com Trakt/Simkl vinculado, descoberta.c). 5-6 e depois 1 pode ser o filtro ou a lista de vistos da conta ainda não puxada (mesma suspeita do #356). Sem log é suspeita. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Com Trakt e Simkl vinculados, Continuar mostra vistos antigos mesmo com fonte Nuvio (hboinay item 4)

- Plataforma: Android
- Status: precisa-log
- Release: 2.0.3 (parcial, 507d3a6b + 005b3095 + c0f4054c)
- Conserto: 507d3a6b (v2.0.3); 005b3095 (v2.0.3): desmarcar ganha; c0f4054c (v2.0.3): a seguir recua para o primeiro episódio desmarcado
- Próximo passo: pedir log da 2.0.3 com a fonte em Conta Nuvio
- Notas: Na leitura do código a fonte Conta não consulta Trakt nem Simkl (descoberta.c: querTrakt só em Trakt/Todas, querSimkl idem), então os "vistos antigos" teriam de vir de continuarLocal ou do "a seguir" da conta; a 2.0.3 descarta o visto em outra fonte e o parado há mais de 60 dias. Não reproduzido. Sem log é suspeita. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Biblioteca: Trakt 213 e Simkl 483 de 1600+ (hboinay item 6)

- Plataforma: Android
- Status: aberta
- Release: alvo sugerido 2.0.5
- Conserto: d30344ab (v2.0.3): watchlist do Trakt paginada; 96ccdab1 (v2.0.3): tetos do Android voltam a 400/300; 7b9e641a (órfão, nenhuma branch): reverte o 96ccdab1 e devolve os tetos 2000/1000 no Android
- Próximo passo: decidir o 7b9e641a: reaproveitá-lo na 2.0.5 (Android com teto 2000 Trakt e 1000 Simkl) ou manter o teto de hoje
- Notas: Na 2.0.3 o Trakt pagina (?page=&limit=500, trakt.c:1873-1960) mas para em TRAKT_LISTA_MAX 400 (trakt.h:133) em todas as plataformas, e o Simkl em SIMKL_PTW_MAX 300 (simkl.h): quem tem 1600+ passa a ver 400/300 em vez de 213/483. O teto maior só existe no commit órfão 7b9e641a. O Simkl não pagina (all-items numa resposta), então o teto é só memória. Nota da 2.0.3 diz "até 400 itens" e vale para todas as TVs. Relacionado: #393. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Selo de assistido não aparece em séries nos cartazes (só em filmes) (hboinay item 7)

- Plataforma: Android
- Status: aberta
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: ler o /sync/watched/shows do Trakt uma vez (como o mapa de filmes vistos em trakt.c:1317) para marcar as séries inteiras
- Notas: cat_visto (catalogo.c:432) devolve 0 para série sem histórico; o histórico da série só é escrito quando a PÁGINA do título é aberta (extras.c:586-597, contadores de /shows/<id>/progress/watched) ou ao marcar à mão. Cartaz de série em fileira não tem selo antes disso. Vale para todas as plataformas. #352 (agente/204-biblioteca-visto, 402e2438) só leva o selo para a Biblioteca. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Trailers do destaque e da ficha começam pequenos e depois enchem a tela (hboinay item 8)

- Plataforma: Android
- Status: precisa-log
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: reproduzir na Shield com o log de trailer; ver a ordem posição/recorte no NvPlayer.kt
- Notas: No Android o Kotlin posiciona o recorte (video_android.c:9, video_recorte_fonte=1) e trailer_mostra_video (trailer.c:467) só espera o recorte no .tpk; no Android o plano aparece assim que toca. O pedido é esconder até o recorte assentar, como o .tpk faz (trailer.h:54-60). Hipótese, não medida. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Cartazes escuros demais; pede desligar o brilho animado (hboinay item 11)

- Plataforma: Android
- Status: aberta
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: dois ajustes pequenos: luz do foco (varredura) liga/desliga e força do véu do cartaz
- Notas: A varredura de luz no foco (revela_varre, revela.h:83, home.c:5426) só some com "Animações reduzidas" global (anim_politica_reduzida), que também tira todo o resto do movimento; não há ajuste próprio. O escurecimento vem do véu sobre a arte (veusDoCard, home.c:4844) e do efeito de profundidade (AJ_PROF_*), este com liga/desliga em Ajustes. Contorno de hoje: Animações reduzidas e Efeito de profundidade desligados. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Selos: URL do manifesto diz "isso não é um JSON de selos"; não usa os selos da conta (hboinay item 12)

- Plataforma: Android
- Status: precisa-log
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: pedir a URL do manifesto e o log; testar o JSON no selospacote_adicionar
- Notas: A mensagem sai de SELOS_ERR_JSON (ajustes.c:3047; selospacote.c:639-648): corpo que não é objeto/string JSON ou resposta maior que 4 MB. Os selos da conta só vêm de features.stream_badge_settings.stream_badge_rules no blob de ajustes do perfil (selospacote.c:588-612): se a conta não tem esse campo, nada é usado. Sem a URL e o log não dá para saber qual dos dois é. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Só um servidor Jellyfin e um Emby por perfil (hboinay item 13)

- Plataforma: Android
- Status: por-desenho
- Release: alvo sugerido 2.1
- Conserto: nenhum encontrado
- Próximo passo: avaliar vários servidores por tipo (hoje instância 0 = Jellyfin, 1 = Emby, mais Plex)
- Notas: Por desenho: um servidor por tipo e por perfil (jellyfin.h:5-10, JfInst; token em jellyfin-p<N>.txt). Vários exigem lista de instâncias, ids com namespace por servidor (jfid.h) e Home com mais fileiras (SRV_FIL_MAX, servidores.h). Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Jellyfin/Emby fora das listas de fontes: add-ons permitidos, ordem, lista (hboinay item 14)

- Plataforma: Android
- Status: aberta
- Release: alvo sugerido 2.1
- Conserto: nenhum encontrado
- Próximo passo: decidir como o servidor pessoal entra em fonteregra (permitidos/ordem)
- Notas: Para título de servidor pessoal a lista de fontes é só a do servidor (addons.c:493-510, servidores_fontes_colher + stream_definir_lista) e as regras de add-on permitido e ordem (fonteregra.h) só conhecem add-ons/plugins. O servidor vem sempre primeiro por desenho (9025bfa9). Confirmar com o autor o que ele esperava ver. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Estilo da legenda só para a segunda legenda nos Ajustes; sincronia automática só por áudio (hboinay item 16)

- Plataforma: Android
- Status: aberta
- Release: alvo sugerido 2.0.5 (estilo)
- Conserto: nenhum encontrado
- Próximo passo: estilo da principal em Ajustes; perguntar quais legendas ele testou na sincronia
- Notas: Ajustes tem Tamanho/Cor/Fundo/Borda só da segunda legenda (ajustes.c:1268-1272; a dica diz que a da principal "é definida no player"). Sincronia: além do áudio (AJ "Sincronia por áudio") existe o AutoSync por texto (legsync.c/autosync.c, em v2.0.3), mas só em legenda EXTERNA, tendo como referência a faixa de texto do MKV (legsync.h:11-16); legenda embutida ou nativa fica indisponível. Pode não ser bug, e sim limite. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Só uma conta Xtream por perfil (hboinay item 17)

- Plataforma: Android
- Status: por-desenho
- Release: alvo sugerido 2.1
- Conserto: nenhum encontrado
- Próximo passo: avaliar várias contas (lista por perfil, canais com id por conta)
- Notas: Por desenho: um servidor, usuário e senha por perfil (xtream.h, arquivo xtream-p<N>.txt; id "xtream:<stream_id>" sem conta). Várias contas pedem mudar o id do canal e o cadastro nos Ajustes. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### IPTV sem VOD/séries do Xtream (hboinay item 18)

- Plataforma: Android
- Status: por-desenho
- Release: futuro
- Conserto: nenhum encontrado
- Próximo passo: registrar como pedido; não planejado
- Notas: O módulo só lê get_live_categories e get_live_streams (xtream.c, xtream.h); não existe get_vod_* nem get_series. Seria um módulo novo (catálogo + ficha + resolução de URL /movie/ e /series/). Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### TV ao vivo só com canais do Reino Unido e EUA, de dezenas de países (hboinay item 19)

- Plataforma: Android
- Status: precisa-log
- Release: 2.0.3 (provável, 5413db8a)
- Conserto: 5413db8a (v2.0.3): guia diz "Mostrando N de M canais"; Android guarda 3000 canais e 128 categorias; 45ae240c (teste)
- Próximo passo: pedir para atualizar para a 2.0.3 e, se persistir, o log
- Notas: Antes da 2.0.3 o guia cortava em 900 canais e 48 categorias sem avisar e sumia categorias inteiras (45ae240c); com a lista ordenada isso deixaria só os primeiros países. Hipótese forte, sem confirmação do autor. Se persistir na 2.0.3, a fonte passa a ser o catálogo do add-on (Ajustes > Guia, canais por add-on, #283). Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### TV ao vivo com fps variando entre 24 e 50 (33, 34, 48) (hboinay item 20)

- Plataforma: Android
- Status: precisa-log
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: pedir log com o medidor de desempenho ligado durante um canal ao vivo
- Notas: Sem leitura de código que explique. O medidor (AJ_MEDIDOR) mede quadros da interface, não do vídeo; o vídeo ao vivo roda no Media3 (video_android.c). Pode ser decodificação, rede ou troca de taxa da tela. Só dá para dizer com o log. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Canais piscando em verde na TV ao vivo (hboinay item 21)

- Plataforma: Android
- Status: precisa-log
- Release: alvo sugerido 2.0.5
- Conserto: nenhum encontrado
- Próximo passo: pedir log com o canal que pisca e a saída de vídeo da Shield
- Notas: Tela verde em vídeo costuma ser decodificador/superfície (buffer YUV sem quadro). No Android a superfície é recriada em partida HDR e mudança de aspecto (notas da 2.0.3). Não localizado no código; sem log é suspeita. Origem: Reddit u/hboinay, NVIDIA SHIELD TV Pro 2019, build teste-318.3 (base 2.0.2), 09/10/2026.

### Painel lateral (side bar) pisca depois da atualização (Ragingmuncher)

- Plataforma: desconhecida
- Status: precisa-log
- Release: sem alvo (aguardando informação do autor); relatado na 2.0.3
- Conserto: nenhum encontrado
- Próximo passo: responder no Reddit pedindo: modelo da TV e pacote (.tpk/.wgt/versão do webOS); se o ponteiro estava na tela ou só setas; em qual tela/item e se já acontecia na 2.0.2; enviar o log logo depois, com a hora aproximada (Ajustes > Sobre > Enviar log); vídeo pelo celular se for só visual
- Notas: Relato: o painel lateral (side bar) pisca depois da atualização para a 2.0.3. TV e plataforma desconhecidas. ANÁLISE de 09/10: em ~310 logs da 2.0.3 (webos, tizen, tizen-tpk, a partir de 09/10/2026 15:00 UTC) não há laço de troca de tela (no máximo 18 linhas "[transicao] tela" por log, espaçadas por segundos). O painel lateral não escreve linha de log, então isso só descarta um laço, NÃO um pisca visual. src/menu.c não mudou entre a v2.0.2 e a v2.0.3; src/ponteiro.c ganhou 11 linhas (só aparo de alvo do ponteiro). SUSPEITA, não provada: Magic Remote com o ponteiro sobre o painel com o fundo de vidro. Sem log ou vídeo é suspeita. Origem: Reddit u/Ragingmuncher, versão 2.0.3, 09/10/2026.

### Reddit (DueResult1983): 2.0.3 trava ao tocar série no Android TV; caixa inteira trava, teve de desinstalar

- Plataforma: Android (Mecool KM7 SE, Amlogic, Android 12)
- Status: aberta
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: reproduzir com vídeo 4K HEVC numa caixa Amlogic de 2 GB; medir no logcat as linhas "[player] superficie" da 0b8f9290 (recriação da superfície fora do fio principal) e decidir se a 2.0.4 volta ao caminho antigo nesse tipo de aparelho
- Notas: Pessoa nuvio:266479b1 (registros D1 66153..65330). Queda nativa em 5 de 13 sessões 2.0.3 contra 4 de 49 na 2.0.2 (no geral do Android a 2.0.3 MELHOROU: 25/400 contra 70/391). Pilhas: 65736 = GC da ART (HeapTaskDaemon, ConcurrentCopying, ponteiro nulo); 66017 relata a queda da sessão 66008 = fio ExoPlayer:Playback em AudioTrack.getTimestamp -> GetPrimitiveArrayCritical (SIGSEGV 0xfffffffe); 66021/66025/66054 sem tombstone. As duas pilhas são dentro da ART, padrão de heap Java corrompido ou memória esgotada. Memória: avisos onTrimMemory 5/10/15/20/40 em várias sessões, teto de textura já em 128 MB. Sessão 66008: série tt26545992 T1E1 4K 3840x1920 retomando em 22%; a superfície é recriada 2 vezes logo no início (3x "primeiro quadro") e o log para de repente sem linha de FPS. SUSPEITA principal: 0b8f9290 (2.0.3) manda MSG_SET_VIDEO_OUTPUT null ao renderer antes de recriar a superfície; em Amlogic isso pode manter/reabrir o decoder 4K e somar memória. Outras mudanças do player Android entre v2.0.2 e v2.0.3: 2a351b76 (aspecto espera superfície estável), c3726d2d (capítulos MKV por fio lateral), ce7bcd0f (retomada). Seekr já existia na 2.0.2, descartado como novidade.

### Reddit (nossascamerasip): LG 32LJ600B (webOS 3), 2.0.3: trailer com imagem e sem som; filme com som e sem imagem

- Plataforma: LG webOS 3
- Status: precisa-log
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: pedir o código do log (Ajustes > Sobre > Enviar registro) logo depois de abrir um filme; com o log, ver a linha [tv] e o caminho do vídeo (DTS, hdr, player) no webOS 3
- Notas: A mesma pessoa usa a C1 sem problema. Em 09/10 não há sessão da 32LJ600B no D1 (busca por LJ600 e por host=webos-3 desde o id 64500); há outras webOS 3.9.3 na 2.0.3 com "arranque queda" (65SJ800V 65304/65363/65374, OLED55B7P 65317). A 2.0.4 já leva a queda do webOS 3 no 2º vídeo (09ce19a0) e a leitura estrita da versão (webosver), mas nada prova que seja este defeito.

### Pedido: "Meu perfil" e status funcionando sem Trakt (usa Simkl e dá erro); sugere Telegram/Discord

- Plataforma: all
- Status: aberta
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: 2.0.5: perfil com Simkl (stats do Simkl, ou montado do histórico local + Simkl) e mensagem certa quando só o Simkl está ligado; Telegram/Discord é decisão do dono
- Notas: LIDO 09/10: a tela Meu perfil chama só trakt_perfil (src/app.c carregarPerfil, ~350) e, sem Trakt, mostra "Trakt desconectado. Vincule a conta para ver seu perfil." (app.c ~3273) mesmo com o Simkl conectado. perfil.c só mostra Simkl na linha de contas (~770). "Status" do relato não está claro (status de amigos/assistindo agora também vem do Trakt).

### Estudo do fork ysosrs123/NuvioTV-Fork (pré-carregamento e outras melhorias)

- Plataforma: all
- Status: aberta
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: pré-carregar fontes do próximo episódio em agente/204-proxprefetch (rodada 2); pós-créditos, lista de fontes mortas e ytId na 2.0.5
- Notas: Relatório do MiniMax M3 em /Volumes/ExternalSSD/tmp/nuvio-fork-ysosrs/relatorio.md, CONFERIDO pela coordenação: 3 afirmações erradas do M3 (já temos Preparar fonte ao abrir, foco fixo quando a lista cresce, medição passiva/StreamFit). Verdadeiros: preload do próximo (#3795), segurar cartão até pós-créditos, failover com fontes mortas persistentes, autoplay com ytId, 4 conexões fixas no ParaleloDataSource.

### Biblioteca mostra 129 salvos e 8 títulos

- Plataforma: Android TV
- Status: consertada-nao-lancada
- Release: 2.0.4
- Conserto: nenhum encontrado
- Próximo passo: nada; validado na TCL
- Notas: 'Ocultar não lançados' escondia todo título sem metadados (121 de 129). Conserto 251d5e0f.

### Spotlight: título do usuário não vinha primeiro; item cortado

- Plataforma: Android TV
- Status: consertada-nao-lancada
- Release: 2.0.4
- Conserto: nenhum encontrado
- Próximo passo: dono dizer qual foto estava errada (arte não reproduzida)
- Notas: Busca 'silo': série 2023 em 4º. Conserto em agente/204-spotlight (1de8ce4e..63a41b4b). Arte trocada não comprovada.

### Dolby Vision às vezes abre escuro na TCL; clareia ao mudar o aspecto

- Plataforma: Android TV
- Status: aberta
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: dono avisar na hora em que abrir escuro para puxar o [dvtrace]
- Notas: Sem causa provada. Logs da superfície HDR em 817d3c3a. Suspeita: recriação da abertura marcada como feita antes de terminar; mudar aspecto força outra recriação.

### 'Seu progresso' errado depois de desmarcar episódios

- Plataforma: Android TV
- Status: aberta
- Release: -
- Conserto: nenhum encontrado
- Próximo passo: decidir se entra 'desmarcar daqui em diante' no menu do episódio
- Notas: No log, 'até aqui desmarcar' em T3E2 tirou T1, T2 e T3E1-2 e deixou T3E3+ marcados; nada voltou sozinho. Dois buracos de sync achados e não provados (ENTREGA-progdesmarca.md). Achado lateral consertado: cache do gráfico (6c6d1668).
