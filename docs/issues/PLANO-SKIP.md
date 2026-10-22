# PLANO-SKIP — cobertura de abertura, resumo, créditos e prévia

**Data:** 12/10/2026  
**Worktree:** `/Volumes/ExternalSSD/nv-205-skip`  
**Branch:** `agente/205-skip`  
**Base auditada:** `c0e22824cdf8d0fc28c44258d4ca98a87873469b`  
**Estado:** proposta; sem implementação, execução de testes, rede ou commit.

## 1. Objetivo e critério ponytail full

Cobrir as lacunas de **intro/OP**, **recap temporal**, **credits/outro/ED** e **preview** em anime, séries e filmes, independentemente da origem de reprodução, sem transformar estimativas em saltos automáticos perigosos.

**Recomendação:** primeiro completar a cadeia existente **por tipo e validade**, melhorar identidade/numeração e preservar capítulos. Depois adicionar uma fonte externa de cada vez, somente quando demonstrar ganho marginal de cobertura.

Não prometer cobertura universal: ausência de dados, montagens diferentes, episódios especiais/duplos e fontes indisponíveis continuarão existindo. “Fechar cobertura” deve significar **cada lacuna tem tratamento explícito, seguro e mensurável**, não inventar marcadores quando nenhum existe.

### Vocabulário de evidência

- **COMPROVADO LOCAL:** observado no código, fixture ou log auditado.
- **DECLARAÇÃO DOCUMENTAL:** consta no snapshot local; não confirma o serviço externo hoje.
- **SUPOSIÇÃO:** proposta, extrapolação ou contrato ainda não verificado.
- **S/M/L:** estimativa relativa de implementação, não prazo nem preço.

### Separar três produtos

1. Pular um **intervalo temporal** dentro do episódio.
2. Ignorar um **episódio inteiro** classificado como recap/filler.
3. Ocultar o **botão manual**, sem necessariamente desligar marcadores ou automação.

Não misturar esses controles.

## 2. Auditoria do estado atual

### 2.1 Fontes e identidade

| Área | Estado comprovado e lacuna |
|---|---|
| TheIntroDB | Consulta `api.theintrodb.org/v3/media` por TMDB ou IMDb, acrescentando temporada/episódio e duração real quando disponível: `src/intro.c:337-342`. Extrai arrays `intro`, `recap`, `credits`, `preview`, em milissegundos: `src/intro.c:37-78`. Filmes passam sem temporada/episódio: `src/intro.c:506-521`. |
| AniSkip | MAL + episódio; solicita `op`, `ed`, `recap`. Escolhe resultado de duração mais próxima por tipo, recusando diferença superior a 10% quando ambas as durações são conhecidas: `src/intro.c:94-130`. Sem duração comparável, não aplica esse filtro: `src/intro.c:111-118`. |
| MAL direto/Kitsu | `mal:` é usado diretamente; Kitsu consulta mappings e mantém cache RAM de 16 entradas: `src/intro.c:144-163`. Para IDs `kitsu:`/`mal:`, temporada maior que 1 é recusada: `src/intro.c:430-432`. |
| IMDb/TMDB de anime | Evidência local TMDB: animação + idioma/país japonês; ficha tardia dispara novo pedido: `src/intro.c:794-817`. Fribb resolve temporada e offset, rejeitando ambiguidades empatadas: `src/intro.c:239-252,289-322`. |
| Fribb | Índice persistente semanal, leitura em fluxo, preservação do índice antigo e retry de 10 min/1 h/6 h quando não há índice útil: `src/intro.c:174-193,254-322`. **Desabilitado em WGT** nesse caminho por Asyncify: `src/intro.c:435-440`. |
| Outras identidades | `intro_pedir_ids` não aceita `anilist:` diretamente sem TMDB/IMDb: `src/intro.c:506-511`. O app reconhece esse namespace em outros subsistemas, mas isso não fornece MAL ao skip. |
| Par TMDB | O player usa `CatEp.tmdbSerie/tmdbT/tmdbE` confirmado: `src/player.c:608-622`; estrutura: `src/catalogo.h:184-189`. Converter só o ID da série não resolve numeração de episódios. |
| ARM/AniList existentes | ARM converte IDs de anime em IMDb de obra, não episódio: `src/descoberta.c:7067-7101`. AniList GraphQL já existe para arte, não para skip: `src/artereserva.c:923-954`. |

### 2.2 A principal lacuna: fallback encerra cedo demais

`baixar()` consulta TheIntroDB **somente quando AniSkip retornou zero trechos**: `src/intro.c:454-465`.

Consequências:

- AniSkip com apenas OP impede buscar recap, ED e preview no TheIntroDB.
- Um resultado que existe, mas será recusado pela janela, também pode impedir a fonte seguinte.
- Não há proveniência independente por trecho: `trechosAni` é uma flag global, usada para classificar créditos no player: `src/intro.c:19,477`; `src/player.c:1777-1778`.

**Primeira mudança recomendada:** cobertura por tipo **aceito**, não por “resposta contém qualquer trecho”. Para isso, preservar origem e elegibilidade automática de cada trecho selecionado; não criar framework genérico de provedores.

### 2.3 Capítulos e prioridade

Capítulos substituem rede por grupo: abertura, resumo, **créditos+prévia juntos**: `src/intro.c:530-550`.

Isso preserva a prioridade do arquivo, mas apresenta dois riscos:

- Um capítulo inválido pode eliminar um marcador remoto válido antes da validação.
- ED de capítulo pode eliminar preview remoto, embora o capítulo não tenha identificado preview.

A conversão atual publica:

- abertura nomeada;
- crédito nomeado até preview, ou aberto até o fim;
- preview final aberto.

Não publica recap de capítulo: `src/capmkv.c:73-91`.

**Capítulo do arquivo é mais aderente ao corte, mas não é infalível.** Nomes heurísticos ainda precisam ser validados.

### 2.4 Duração tardia: já há reavaliação

**Não há evidência de recusa permanente no caso fornecido.**

- `valido()` recalcula a janela em cada chamada; `veredito` controla o log, não bloqueia nova avaliação: `src/intro.c:651-662`.
- Duração recebida/alterada provoca até dois pedidos adicionais; falha transitória permite até três retries, espaçados por 30 s: `src/intro.c:414-422,559-572`.
- O player recebe duração em atualização: `src/player.c:3424-3444`.
- Entretanto, informa essa duração ao módulo intro **no desenho das ações**: `src/player.c:4354-4357`.

**Log real**, prefixo  
`/Volumes/ExternalSSD/nv-205-tpk412b/.diag/log-412-AS0D80.txt`:

- `:393-396`: AniSkip disponível; créditos recusados com `dur=0`.
- `:422`: pipeline informa duração.
- `:436-440`: mesma janela aceita com `dur=1446`; pós-play aceita créditos.
- `:445`: pedido com duração conhecida concluído.
- `:461-467`: capítulos chegam e assumem prioridade.
- `:1308-1310` → `:1349-1356`: outra sequência de recusa → aceitação.

**Lacuna remanescente:** desacoplar estado/duração do desenho, testar troca de título/fonte e preservar capítulos em remapeamento tardio. Não implementar um segundo mecanismo de reavaliação.

### 2.5 Créditos, aprendizado e próximo episódio

Há duas cadeias diferentes:

- **Botão/auto-skip:** trechos selecionados em `intro.c`.
- **Início efetivo para next:** capítulo > TheIntroDB > AniSkip > vizinho > aprendido; depois estimativa: `src/credfonte.c:6-27`; integração `src/player.c:1770-1783`.

Já existem:

- Vizinhos E−1/E+1, mesma temporada, até duas consultas; reaproveitamento do resto até o fim: `src/intro.c:389-411`; `src/credfonte.c:14-18`.
- Aprendizado persistente em `credits-offsets.txt`, até 64 séries: `src/credaprende.c:8-34`.
- Aprendizado manual limitado a duração ≥600 s, resto 15–420 s e ≤20% da duração; proteção antideriva: `src/credaprende.c:50-65`.
- Último capítulo sem nome útil, depois de 75% da duração: `src/capmkv.c:117-122`.
- Estimativa **fixa**, não percentual: série 50 s; episódio <10 min, 15 s; filme 180 s; filme <1 h, 90 s: `src/intro.h:92-125`.
- Janela de sanidade proporcional: série 15%, 120–300 s; filme 12%, 300–900 s: `src/intro.c:609-629`.
- Estabilidade da duração por 8 s antes de mostrar pós-play: `src/posplay.c:385-410`.
- Autoplay antecipado após 4 s de cartão exige `marcadorReal`: `src/posplay.c:340-354`.

**Lacunas:**

1. Aprendizado é por série, sem temporada/cour/corte.
2. `credaprende` e caches de `credfonte` cortam no primeiro `:`, causando colisão entre IDs como `kitsu:12` e `kitsu:34`: `src/credaprende.c:37-40,62`; `src/credfonte.c:43-51`. Reutilizar `idbase.h:33-47`.
3. Skip manual de créditos com OSD oculto aprende; caminho com OSD visível não faz a mesma chamada: `src/player.c:3100-3102,3145-3150`.
4. `marcadorReal` para antecipação de next aceita capítulo/TheIntroDB, mas exclui AniSkip: `src/player.c:3675-3677`.
5. “Último capítulo heurístico” chega como fonte capítulo. Não equiparar automaticamente **origem no arquivo** a **créditos comprovados**.

### 2.6 Preview e pós-créditos

- Manual pode emendar créditos+preview adjacente, com tolerância de 2 s: `src/intro.c:665-674`.
- Preview isolado serve ao cartão next, sem botão próprio: `src/intro.c:754-763`.
- Auto-skip termina no fim explícito dos créditos, sem emendar preview/cena: `src/intro.c:688-702`.
- Auto-créditos exige limites explícitos e fim pelo menos 5 s antes do fim da mídia: `src/intro.c:839-847`.
- Sinal de pós-créditos não contém intervalo da cena; bloqueia contagem e permite decisão manual: `src/posplay.c:414-420,506-517`.

Essas proteções devem valer para **todas** as fontes novas.

### 2.7 #430 e #337

- #430 consta como lançada 2.0.5, mas a evidência local descreve AniSkip via IMDb/TMDB entre temporadas, **não um seletor configurável de provedores**: `docs/issues/MAPA.md:470,509`.
- “Prioridade das fontes” de reprodução não equivale à prioridade de marcadores.
- #337 continua pendente, alvo 2.1: `docs/issues/MAPA.md:348,384`.
- Auto-hide de 10 s não é opção de ocultar: `src/intro.c:715-740`.
- Ajustes atuais governam auto-skip e next, não visibilidade: `src/ajustes.c:1402-1406`.

O hash `3a02817d` citado na nota de #343 não comprova integração de skip: seus metadados Git locais dizem redesign de “Recommend to a friend”. Não repetir esse hash como prova de entrega.

## 3. Matriz de candidatos

### 3.1 Fontes externas

| Candidato | Cobertura e IDs | Chave/custo | Encaixe e lacuna fechada | Tamanho e risco |
|---|---|---|---|---|
| **anime-skip.com — GraphQL** | Candidato a intervalos de anime. **SUPOSIÇÃO:** tipos disponíveis, identidade de show/episódio/release e conversão dos IDs atuais; não há schema local. | **SUPOSIÇÃO:** exigência e modalidade de chave, cotas, preço e licença. Não confundir com AniSkip sem chave. | Anime, depois das fontes existentes, preenchendo tipos ainda ausentes. Promover apenas com ganho comprovado. Fecha marcações exclusivas dessa comunidade, se existirem. | **L provisório.** Contrato, IDs, releases e auth desconhecidos. Não escrever queries/mutations inventadas. |
| **PublicMetaDB** | Snapshot declara origem para skip intro/recap/outro. Namespaces aceitos e cobertura de filmes/preview: **SUPOSIÇÃO**. | Chave exigida, conforme `.mercado/aiometadata-jellyfin.md:114-115`. Preço/cotas/licença desconhecidos. | Primeiro aproveitar via AIOMetadata já conectado; acesso direto opcional depois de medir ganho. Pode preencher séries/anime sem dados atuais. | **M–L provisório.** Falta contrato, unidade temporal e identificação do corte. |
| **IntroDB antigo** | Repo distingue `api.introdb.app` de TheIntroDB; dependência histórica de temporada/episódio: `src/intro.h:3-8`. Cobertura atual: **SUPOSIÇÃO**. | Snapshot diz “IntroDB sem chave”; não prova que seja esse endpoint. Preço/cotas desconhecidos. | Última fonte direta para séries/anime, somente se distinta e com dados exclusivos. Não priorizar para filmes com base no contrato antigo. | **M provisório; baixo valor demonstrado.** Risco de duplicação e retorno de limitações antigas. |
| **AIOMetadata media segments** | Documento declara intro, recap e outro via Jellyfin, cadeia PublicMetaDB → AniSkip → IntroDB: `.mercado/aiometadata-jellyfin.md:108-120`. Preview não é garantido ali. Precisa identidade do item/episódio e versão no servidor conectado. | Reutilizar conexão autenticada do usuário; custo da instância desconhecido. Chave PublicMetaDB pode permanecer no AIOMetadata. | Fonte agregada para reprodução originada desse servidor. Evita três integrações diretas e consultas duplicadas. | **M**, bloqueado por endpoint/DTO/unidades/fixture. Ter o addon Stremio instalado **não prova** acesso a segmentos Jellyfin. |

**IntroDB versus TheIntroDB:** os serviços antigo e atual são distintos no repo. A identidade do “IntroDB” mencionado no snapshot AIOMetadata permanece **SUPOSIÇÃO**; resolver pelo endereço efetivamente configurado/documentado, não pelo nome.

O transporte Jellyfin autenticado já existe: `src/jellyfin.c:127-169`. IDs `jf:` são opacos e vinculados ao servidor/usuário: `src/jfid.h:4-12`. Não enviá-los a APIs públicas.

### 3.2 Mapeamento, metadata e capítulos

| Candidato | Cobertura e IDs | Chave/custo | Encaixe e lacuna fechada | Tamanho e risco |
|---|---|---|---|---|
| **AniSkip: temporadas/cours/absoluto** | OP/ED/recap existentes; MAL correto da entrada/cour + episódio local. Fontes candidatas: IDs do metadata, Kitsu, Fribb, AniList e Anime-Lists. | AniSkip/Kitsu atuais sem chave. Custos/licenças das extensões não verificados. Reutilizar transporte; não adicionar SDK. | Resolver **antes** de consultar AniSkip. Fecha `anilist:`, IDs diretos multitemporada e offsets não representados, quando há mapeamento comprovável. | **M** para extensões pontuais; **L** para relações complexas. Não remover a guarda T>1 e “tentar o mesmo MAL”. Especiais, episódios duplos e relações não 1:1 devem falhar fechados. |
| **Flags recap/filler** | Episódios inteiros. Stremio: `meta.videos[].recap/filler`; Jellyfin: objeto `aiostreams`. Precisa identidade exata do episódio. | Sem nova chave quando aproveita metadata já recebido; custos herdados da fonte. | Caminho de metadata → lista/seleção de próximo. **Não entra na cadeia temporal.** | **M.** Parser Stremio não transporta esses campos: `src/descoberta.c:6583-6623`; Jellyfin também não: `src/jellyfin.c:560-593`. Não marcar visto o episódio ignorado. |
| **Heurísticas de capítulos MKV** | OP/ED/Opening/Ending/Preview já existem. Adicionar recap explícito e aliases comprovados. Sem IDs externos; usa capítulos deste arquivo. | Sem chave/API; reutiliza cabeçalho/Range existentes. | Primeira fonte quando há nome inequívoco e janela válida. Fecha recaps e grafias locais ainda ignoradas. | **S–M.** `Avant`, `Part A/B`, `Main`, `Prologue` podem conter história. **SUPOSIÇÃO:** Avant=recap ou Part A=OP; não pular por isso. |
| **Capítulos MP4** | Mesma classificação temporal se plataforma/demuxer fornecer capítulos. Nenhum suporte MP4 no `capmkv` atual. | Sem chave; possível custo de I/O adicional. | Mesmo nível de capítulo explícito, sem nova preferência. | **M** se expostos pela plataforma; **L** para parser próprio. `.mp4/.m4v` são descartados pela sonda atual: `src/capmkv.c:190-198`. Não criar demuxer antes de confirmar necessidade. |

**Nomes atuais:** abertura `opening/intro/abertura/OP`; créditos `credit/crédit/end title/outro/encerrament/ending/ED`; preview `preview/next episode/next time/yokoku/avance/previa/prévia`: `src/mkv.c:440-463`.

Lowercase é apenas ASCII; substrings podem aceitar `Discredited` e confundir “Post Credits Scene”: `src/mkv.c:419-436,452-463`. Ampliar nomes e corrigir falsos positivos no mesmo teste tabelado.

Anime-Lists/AniBridge aparecem em pesquisa local explicitamente não reconferida: `docs/plans/metadados-alternativas.md:3-9`. Não copiar preços, campos ou licenças como fatos atuais.

### 3.3 Aprendizado, backend e fallbacks operacionais

| Candidato | Cobertura e IDs | Chave/custo | Encaixe e lacuna fechada | Tamanho e risco |
|---|---|---|---|---|
| **Aprendizado de intro por temporada/cour** | Intervalo ensinado explicitamente; candidatos semelhantes entre episódios. Identidade completa da série + temporada/cour + compatibilidade de corte. | Local, sem API. | Depois de marcadores explícitos, antes de “sem marcador”. Inicialmente manual, com confirmação; não aprender qualquer seek. | **M.** Cold open, OP deslocado, finais especiais e duração igual com montagem distinta. “Mesmo offset em todos os episódios” é **SUPOSIÇÃO**. |
| **Skips manuais do usuário** | Créditos já aprendidos; estender captura explícita de intro/recap. | Local; armazenamento existente primeiro. | Reutilizar `credaprende`; corrigir namespaces e separar temporadas antes de ampliar. | **S** para correções; **M** para novos intervalos. Não inferir intenção de cada avanço do controle. |
| **Marcadores comunitários no backend próprio** | Potencial para qualquer tipo/corte sem cobertura externa. Precisa identidade canônica, episódio, tipo, limites e evidência de duração/corte. | Auth Nuvio existente; custo adicional de banco, tráfego e moderação. | Fonte experimental posterior às fontes verificadas; não substituir contribuição privada nem explicitamente confirmada. | **L.** Opt-in, envenenamento, bots/Sybil, licenças, remoção, retenção. Não existe API de marcadores no serviço auditado: `servidor/recomendacoes/src/index.js:1071-1122`; `schema.sql:1-55`. |
| **Créditos sem dados** | Último capítulo útil, vizinho, resto aprendido e timing fixo do overlay já existentes. | Local; vizinhos limitados usam TheIntroDB atual. | Somente sinal de fim/next, **não autorização de auto-skip temporal**. | **S–M** para distinguir confiança e consolidar consumidores. Ratio de duração como início universal contraria regra atual; depende de aprovação do dono. |
| **Duração tardia** | Todos os tipos sujeitos à sanidade/corte. Usa duração real da reprodução corrente. | Sem API nova; pedidos adicionais já limitados. | Atualizar estado antes de auto/next, revalidar candidatos e consultar somente lacunas necessárias. | **S–M.** Já funciona no log; testar redesenho ausente, nova fonte, TMDB tardio e recuo. |
| **Caching** | Todos os provedores e mapeamentos; reduz repetição e melhora operação após falhas. | RAM/disco locais; sem serviço novo. | Cache por fonte/identidade/episódio/corte, revalidado ao uso. | **M.** Negativo não pode bloquear consulta com duração nova; 5xx/timeout não são “não conhece”. Conta/servidor/credencial alterados precisam invalidar escopo. |

### Privacidade do backend próprio

**SUPOSIÇÃO de desenho, não contrato existente:**

- Contribuição explícita e desligada por padrão.
- Nunca enviar URL assinada, token, nome do arquivo completo, legenda, áudio ou histórico automático.
- Corpo limitado; IDs e tempos finitos validados; quota por identidade autenticada.
- Submissão bruta não vira marcador público imediatamente.
- Concordância entre contribuidores independentes, denúncias e reversão de versões.
- Separar armazenamento comunitário de feed social/recomendações.
- Política de retenção e exclusão de contribuições antes do lançamento.
- Não redistribuir dados de terceiros sem verificar licença.

## 4. Cadeia recomendada por conteúdo

### Regra compartilhada

Selecionar por **tipo temporal**, apenas entre candidatos compatíveis e válidos.

1. Preservar todos os intervalos úteis de um tipo dentro da capacidade atual.
2. Não sobrescrever fonte superior com resposta tardia.
3. Não eliminar fallback válido por candidato superior recusado.
4. Duração desconhecida é **pendente**, não ausência definitiva.
5. Proveniência/confiança viajam com o trecho; cache não muda a prioridade.
6. Consultas externas são sequenciais e limitadas; publicar resultados disponíveis cedo.
7. Sem relação exata de episódio, não consultar por tentativa cega.
8. Não relaxar janelas atuais para aumentar artificialmente cobertura.

### Anime

**Proposta de padrão:**

Capítulos explícitos válidos  
→ segmentos do servidor da reprodução, quando disponíveis  
→ AniSkip com MAL/episódio corretamente resolvidos  
→ TheIntroDB para tipos ainda ausentes/recusados  
→ anime-skip.com validado  
→ PublicMetaDB direto opcional  
→ IntroDB distinto, se demonstrar ganho  
→ aprendizado local confirmado  
→ derivação apenas para fim/next  
→ nenhum marcador temporal.

Se AIOMetadata já agregou PublicMetaDB/AniSkip/IntroDB, evitar repetir consultas equivalentes sem necessidade.

Preview continuará lacuna específica quando a fonte só fornece OP/ED/recap.

### Séries não anime

Capítulos explícitos válidos  
→ segmentos do servidor conectado  
→ TheIntroDB  
→ PublicMetaDB opcional  
→ IntroDB distinto com ganho demonstrado  
→ aprendizado local confirmado  
→ vizinho/resto aprendido/último capítulo útil/timing fixo para next  
→ nenhum marcador temporal.

### Filmes

Capítulos explícitos válidos  
→ segmentos do servidor conectado  
→ TheIntroDB por IMDb/TMDB  
→ PublicMetaDB **somente após confirmar suporte a filmes**  
→ aprendizado manual deste filme/corte, se aprovado  
→ timing fixo de relacionados.

Não copiar intro/créditos de filmes vizinhos ou de uma franquia. Não consultar AniSkip pela coincidência de título; suporte a anime-filme exigiria mapeamento/contrato específico.

### Prioridade configurável — #430

**Ponytail:** iniciar com ordem fixa comprovável. Se o dono quiser completar o pedido de configuração:

- Usar a mesma decisão por tipo; não criar outra cadeia só para o seletor.
- Oferecer poucos modos ou uma ordem curta das fontes realmente disponíveis.
- Diferenciar “preferida” de “somente esta fonte”.
- “Desligado” desliga busca/uso conforme decisão explícita; #337 governa apenas botão.
- Não permitir que reordenar fontes desative sanidade, identidade ou proteção de pós-créditos.

## 5. Cache e ciclo de vida

### O que reaproveitar

- Kitsu RAM: `src/intro.c:144-163`.
- Fribb em disco/retry: `src/intro.c:289-322`.
- Negativo TheIntroDB por episódio, 30 min: `src/credfonte.c:83-109`.
- Capítulos por URL completa e geração: `src/capmkv.c:162-208`.

AIOMetadata documenta cache positivo/negativo de sete dias e timeout de 5 s: `.mercado/aiometadata-jellyfin.md:260-262`. Não adotar automaticamente sete dias de negativo no cliente.

### Lacunas a resolver

- Negativo atual não distingue duração/corte: `src/intro.c:361-370`.
- Resposta de outro episódio vira negativo: `src/intro.c:376-382`; registrar causa distinta.
- Sem cache positivo explícito de marcadores remotos no módulo auditado.
- Cache deve preservar candidatos e revalidar após duração conhecida.
- Guardar erro transitório separadamente de vazio válido.
- Correção de MAL/offset, provedor ou conta invalida a chave relevante.
- Não persistir URL credenciada para construir identidade pública.
- **SUPOSIÇÃO inicial de TTL:** positivo 24 h, vazio válido 30 min, erro usando retry existente. Ajustar por medição, não por novo subsistema de cache.
- Limites de bytes/entradas e escrita atômica no armazenamento já existente.

Remapeamento TMDB tardio chama pedido “novo”, que zera capítulos: `src/player.c:3245-3246`; `src/intro.c:498-501`. Planejar preservação **somente quando comprovadamente é a mesma reprodução/corte**, sem restaurar dados de fonte anterior.

## 6. Métricas de cobertura

### 6.1 Baseline do log fornecido

Contagem local, sem consulta externa:

- **12 snapshots** `[intro] fonte=...`
  - AniSkip: 4;
  - capítulos: 5;
  - capítulos + AniSkip: 3;
  - nenhuma: 0.
- Apenas **um conteúdo/episódio distinto**: `tt9584920 S01E03`.
- 15 recusas por duração desconhecida.
- 9 aceites de janela.

Isso **não é cobertura de 100%**: são reproduções/respostas repetidas de um episódio. Os 15 registros não representam 15 falhas finais.

### 6.2 Indicadores necessários

| Métrica | Definição |
|---|---|
| `fonte=nenhuma` bruto | Snapshots sem marcador ÷ snapshots concluídos. Apenas diagnóstico inicial; não cobertura por reprodução. |
| Sem fonte por reprodução | Reproduções elegíveis que encerram resolução sem qualquer marcador aceito ÷ reproduções elegíveis. |
| Cobertura por tipo | Reproduções com marcador aceito de intro/recap/credits/preview ÷ amostra anotada em que esse tipo realmente existe. |
| Cobertura completa | Reproduções com todos os tipos presentes na obra atendidos. Não exigir recap onde não existe. |
| Ganho marginal | Tipos aceitos acrescentados exclusivamente por cada fallback. |
| Disponibilidade útil | Marcador disponível antes de o playhead passar pelo intervalo. |
| Duração tardia | Tempo de duração real recebida até revalidação; pendente→aceito versus pendente→recusado por outro corte. |
| Erros | Sem ID, mapeamento ambíguo, vazio válido, timeout/5xx, episódio errado, outro corte e truncamento separados. |
| Precisão | Marcadores auditados que coincidem com intervalo real; falsos positivos, saltos indevidos e perda de cena. |
| Operação | Pedidos/reprodução, latência p50/p95, bytes, hit de cache, retries e memória. |
| Uso | Botão mostrado/acionado, auto-skip, recuo após salto e cancelamento de next. Recuo não é prova isolada de erro. |

Deduplicar por **reprodução + episódio + fonte/corte**, não só IMDb/T/E. Instrumentar identificador efêmero local; não exportar URL de vídeo.

Separar anime/série/filme, namespace, temporada/cour, plataforma e origem. WGT precisa coorte própria por suas restrições.

**SUPOSIÇÃO de observabilidade:** um resumo final local por resolução contendo tipos aceitos, origem e causas de ausência. Os campos seriam nossos, não campos de API.

## 7. Fatias prioritárias e testes FAIL→PASS

**Não executados nesta auditoria.** “FAIL→PASS” abaixo descreve regressões a criar na implementação. Casos já atendidos devem permanecer PASS; não fabricar falha nem reescrever mecanismo existente.

### 2.0.6 — completar o que já existe

#### F1 — fallback por tipo/validade e proveniência — M, prioridade máxima

**Arquivos:** `src/intro.c`, `src/intro.h`, `src/player.c`; testes de intro/créditos.

- [ ] Fixture sintética AniSkip apenas OP + TheIntroDB recap/ED/preview: **FAIL atual** — segunda fonte não consultada → **PASS** — preenche ausentes, mantém OP.
- [ ] AniSkip ED recusado por corte + crédito remoto compatível: não encerrar a cadeia pelo trecho recusado.
- [ ] Capítulo ED inválido + rede válida: inválido não remove alternativa válida.
- [ ] Capítulo OP válido chegando antes/depois da rede: permanece prioritário.
- [ ] ED de capítulo sem preview: preview remoto compatível não desaparece só por pertencer ao grupo créditos.
- [ ] Proveniência distinta por tipo e limites explícitos preservados no auto-skip.
- [ ] Rede atrasada de outra reprodução, 5xx, múltiplos intervalos e limite de oito trechos mantêm segurança.

Reutilizar `intro_definir_buscador` e dublês locais: `tests/intro.c:71-117`; testes concorrentes: `tests/creditos_limitados.c:12-54,149-158`.

#### F2 — duração/ciclo de vida independentes do desenho — S–M

**Arquivos:** `src/player.c`, `src/intro.c`; testes existentes.

- [ ] Mesmo crédito, `dur=0` → duração real compatível: recusa temporária vira aceite **sem depender de renderizar botão**.
- [ ] Mesmo crédito com duração de outro corte continua recusado.
- [ ] TMDB tardio para mesma reprodução não perde capítulos já publicados.
- [ ] Novo título/fonte não recebe duração/trechos antigos.
- [ ] Refetch de duração/IDs não rearma auto-skip já consumido nem apaga proteção de recuo na mesma reprodução.
- [ ] 404 sem duração não bloqueia indefinidamente consulta útil com duração nova.
- [ ] Regressão do log 1340–1430/1446 permanece PASS.

A reavaliação pura já está parcialmente coberta: `tests/intro.c:237-246`; `tests/creditos_limitados.c:111-118`. Não implementar “novo reavaliador”.

#### F3 — identidade e aprendizado existentes — S–M

**Arquivos:** `src/credfonte.c`, `src/credaprende.c`, `src/player.c`; reutilizar `src/idbase.h`.

- [ ] Cache `kitsu:12` não atende `kitsu:34`; repetir com `mal:` e `anilist:`.
- [ ] IMDb com sufixo de episódio continua recuperando corretamente.
- [ ] Skip manual de créditos com OSD visível/oculto produz comportamento equivalente.
- [ ] Persistência mantém limites, antideriva e compatibilidade com linhas antigas.
- [ ] Não migrar automaticamente chave antiga ambígua `kitsu` para um anime arbitrário.
- [ ] Aprendido/vizinho/último capítulo heurístico não autorizam auto-créditos nem antecipação como marcador comprovado.
- [ ] Decisão sobre AniSkip→next testada junto de preview, pós-créditos e recuo.

#### F4 — capítulos MKV conservadores e métricas — S–M

**Arquivos:** `src/mkv.c`, `src/capmkv.c`, logs em `src/intro.c`; testes existentes.

- [ ] `Recap`/`Resumo` explícito vira intervalo até o próximo capítulo.
- [ ] OP/ED/Opening/Ending/Preview existentes permanecem PASS.
- [ ] `Avant`, `Part A/B`, “Discredited” e “Post Credits Scene” não viram saltos por inferência.
- [ ] 75% exatos, capítulos desordenados/truncados e >64 entradas não promovem falso “último real”.
- [ ] Snapshot `fonte` não substitui resumo de tipos aceitos.
- [ ] Erro transitório e ausência válida aparecem como causas diferentes.

**Regra de corte 2.0.6:** F1/F2 antes de novos provedores. Sem API nova, parser MP4 próprio, backend comunitário ou aprendizado automático entre episódios.

### 2.1 — ampliar após contratos e medição

#### F5 — MAL por metadata/AniList e temporadas/cours — M; relações complexas L

**Arquivos:** `src/intro.c`, metadata/`CatEp` somente conforme contrato confirmado; testes de mapeamento.

- [ ] `anilist:` com relação MAL comprovada resolve corretamente.
- [ ] T2/cour seguinte resolve outra entrada MAL e número local.
- [ ] Absoluto com offset conhecido resolve; especiais/episódios duplos ambíguos recusam.
- [ ] Fribb já atendido permanece PASS; nenhuma busca textual cega.
- [ ] WGT usa transporte compatível comprovado ou declara indisponibilidade e segue fallback — sem liberar Asyncify genericamente.

#### F6 — AIOMetadata/Jellyfin segments — M, bloqueado por contrato

**Arquivos:** `src/jellyfin.c/.h`, ligação ao player/intro; testes Jellyfin existentes.

- [ ] Antes: segmento recebido pelo servidor não chega ao player → depois: DTO **real, sanitizado** converte tipos/unidades corretamente.
- [ ] Ausência de endpoint não impede reprodução.
- [ ] Troca de conta/servidor/episódio invalida resposta/cache.
- [ ] ID `jf:` nunca é enviado a API pública.
- [ ] Duração incompatível recusa; segmentos abertos não ganham fim inventado.
- [ ] Não duplicar upstream quando agregação já cobre os tipos.

#### F7 — anime-skip.com e PublicMetaDB — uma fonte por vez

**Arquivos:** módulo intro e transporte existente; nenhum SDK novo.

- [ ] Gate anterior: contrato oficial, auth, IDs, unidades, cotas, licença e fixture disponíveis localmente.
- [ ] Fixture com tipo exclusivo: **FAIL** sem adaptador → **PASS** acrescenta somente lacuna válida.
- [ ] Auth inválida, timeout, erro GraphQL e resposta vazia são distintos.
- [ ] Resultado de episódio/corte errado nunca publica.
- [ ] Logs não contêm chave.
- [ ] Promover na ordem somente após medir ganho marginal.

IntroDB antigo fica fora se não demonstrar dados exclusivos.

#### F8 — #337 e configuração #430 — S/M

**Arquivos:** `src/ajustes.c/.h`, `src/player.c`, `src/intro.c`; testes de ajustes.

- [ ] “Ocultar botão” remove visibilidade, foco, hitbox e consumo de OK.
- [ ] Marcadores, auto-skip e next continuam conforme opções independentes.
- [ ] Reativar visibilidade não deixa foco obsoleto.
- [ ] Preferência de fonte produz ordem determinística e fallback por tipo.
- [ ] Reordenar não permite marcador inválido nem substituição por resposta tardia.

#### F9 — flags recap/filler — M

**Arquivos:** `src/catalogo.h`, `src/descoberta.c`, `src/jellyfin.c`, seletor compartilhado de próximo, ajustes.

- [ ] Flags documentadas sobrevivem parser/cópias/cache.
- [ ] Ausente/false não equivale a true.
- [ ] Autoplay ignora episódio marcado **somente com opção ligada**.
- [ ] Reprodução manual continua disponível.
- [ ] Ignorado não é marcado visto.
- [ ] Filler final, temporada inteira marcada e falta de próximo não causam loop/cartão vazio.
- [ ] `recap: true` não gera intervalo temporal.

Contratos locais: `.mercado/aiostreams-metadata.md:86-98`; `.mercado/aiostreams-jellyfin-extensions.md:123-128`.

#### F10 — aprendizado por temporada e MP4 — M/L, condicionais

- [ ] Intro manual de T1 não é aplicado cegamente a T2/cour/corte diferente.
- [ ] Cold open/final especial recusa reutilização sem confirmação.
- [ ] Aprendizado não supera marcador explícito válido.
- [ ] Para MP4, fixture/platforma realmente expondo capítulos → mesma classificação e proteção do MKV.
- [ ] MP4 sem capítulos não provoca scan ilimitado; reprodução não espera por isso.

#### F11 — comunidade própria — L, pós-gate; não compromisso de release

- [ ] Sem consentimento: zero submissões.
- [ ] Usuário não pode editar/apagar contribuição alheia.
- [ ] Tempos inválidos, corpo excessivo e flooding recusados.
- [ ] Uma contribuição maliciosa não vira marcador público confiável.
- [ ] Exclusão/moderação invalida versão publicada/cache.
- [ ] Nenhuma URL/credencial/histórico vaza.
- [ ] Mesmo com serviço indisponível, cadeia local funciona.

Só iniciar se aprendizado local e provedores existentes deixarem lacuna relevante medida e houver capacidade de moderação.

### Comandos de regressão para implementação futura

Executar offline, com dublês, não nesta auditoria:

```bash
bash tests/intro.sh
bash tests/creditos_limitados.sh
bash tests/credfonte.sh
bash tests/capmkv.sh
bash tests/capmkv_url_longa.sh
bash tests/creditosauto.sh
bash tests/posplay.sh
bash tests/posplay_recuo.sh
bash tests/ajustes_creditos.sh
```

Onde o script suporta, repetir com `SANITIZE=1`. Suites novas devem reutilizar os testes/fixtures existentes e deixar binaries temporários fora do repo.

## 8. Perguntas para o dono

1. **Preview:** quer botão próprio ou manter manual emendado aos créditos e auto preservando preview?
2. **Créditos:** AniSkip e segmentos verificados podem antecipar next, ou autoplay deve esperar preview/fim? Manter decisão separada de auto-créditos.
3. **#337:** ocultar apenas intro ou todos os botões temporais? Preferência por aparelho ou perfil? Ocultar deve manter auto-skip?
4. **#430:** ordem fixa por conteúdo, poucos modos ou lista reordenável? “Somente uma fonte” deve permitir capítulos?
5. **AIOMetadata:** integrar só quando reprodução vem do servidor Jellyfin conectado, ou oferecer conexão adicional a quem usa apenas addon Stremio?
6. **Custos:** aceitar provedores com chave do usuário? Há orçamento para chave compartilhada/backend? Sem confirmação, nenhuma integração paga.
7. **Aprendizado:** aceitar apenas marcação explícita de intervalo? Permitir sugestões por temporada, sempre confirmadas?
8. **Fallback de fim:** preservar 50/15 s para séries e 180/90 s para filmes? Ratio deve continuar apenas sanidade, não início estimado?
9. **Recap/filler:** ignorar apenas no autoplay, desligado por padrão, preservando escolha manual e estado não visto?
10. **Comunidade:** há consentimento, política de retenção e responsável por moderação? Sem isso, manter fora da release.
11. **Aceitação:** quais títulos/plataformas representam as lacunas reais — incluindo WGT, T2+, absoluto, filmes, MP4 e episódios com cenas pós-créditos?

## 9. Condição de parada

Entregar 2.0.6 quando a cadeia existente preencher lacunas por tipo, a duração/ciclo de vida estiverem testados e nenhum derivado for promovido indevidamente a salto automático.

Adicionar cada fallback de 2.1 somente quando:

- contrato e direitos estão verificados;
- regressão FAIL→PASS demonstra lacuna real;
- ganho marginal compensa custo/latência;
- proteção de corte, identidade, recuo e pós-créditos permanece;
- desligar a fonte restaura a cadeia anterior sem migração destrutiva.

**Não adicionar mais infraestrutura para compensar uma cadeia que hoje para no primeiro OP encontrado.**
