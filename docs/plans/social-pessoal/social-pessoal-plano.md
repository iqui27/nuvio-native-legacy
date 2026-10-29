# Social & Pessoal: plano de produto e implementação

Frente nova do Nuvio Legacy: **pessoas na mesma TV, amigos, conquistas, diário e reviews, e "mais sobre você"**. O ponto de partida é o pedido do dono:

> "Com os amigos e pessoas na mesma TV ia ser legal. Criar achievements ia ser legal: fechar uma diretora, uma filmografia, uma trilha de curadoria. Queria algo também tipo o Letterboxd, talvez poder linkar uma review ou fazer uma review mais detalhada do filme que viu, talvez preencher pelo celular, algo divertido. Eu quero tudo muito funcional, seamless, inovador e divertido, não podemos perder o foco."

Código lido (somente leitura): `/Volumes/ExternalSSD/nv-202-amigosdup` (HEAD 18847c70, o social mais novo). O plano do Watch Together está em `/Volumes/ExternalSSD/nv-watch-together/docs/plans/watch-together/`. A pesquisa com links está em [social-pessoal-pesquisa.md](social-pessoal-pesquisa.md), e as citações [R1]…[R9] remetem a ela.

**Status: proposta. Nada foi alterado em repositório nenhum e nada foi medido em TV.** Onde digo "existe", li no código. Onde digo "proposto", é desenho.

---

## 1. Resumo em 10 linhas

1. **Trava de série** (o diferencial; não achei concorrente [R0]): perfis da mesma TV e, depois, amigos de outras TVs andam juntos numa série. No modo **Juntos**, um episódio visto conta para todos. No modo **Separados**, quem está à frente espera, com o episódio borrado e "Esperando Jerry e Sandra". "Assistir mesmo assim" pode pedir o PIN de outro membro.
2. **Diário** é a fundação invisível: um registro por perfil, com data, do que foi visto, montado do player, da conta Nuvio e do Trakt. É o que faz **Perfil e Stats funcionar sem Trakt** (hoje a tela diz "Trakt desconectado").
3. **Caça à filmografia e conquistas**: "fechar uma diretora" com progresso 7/12 e degraus (Primeiro passo, Metade, Quase lá, Completa, Em ordem). Também sagas, Oscar, trilhas de curadoria e coleções da própria conta. **A pessoa escolhe a caçada**, e nenhum badge sai só por abrir o app.
4. **Nota e review**: meia estrela (1 a 10, a mesma escala de Trakt, Simkl e Letterboxd). Na TV, tags rápidas. A **review longa é escrita no celular pelo QR**, com spoiler, visibilidade e "também no Trakt".
5. **Letterboxd sem quebrar o ToS** [R1]: exportar o diário em CSV no formato de importação deles, importar o ZIP que a própria pessoa baixou e colar o link de uma review. Nada de RSS nem de robô. A API oficial é só por pedido.
6. **Reviews de amigos na página do título**, dentro da linha de amigos que já existe (`amigostitulo`), com borrão de spoiler até você ver, e curtida.
7. **"Mais sobre você"**: Pegou no sono (com **"Voltar para onde você dormiu"**, o que é útil, não só divertido), Relógio de sessão, Parceiros de sofá, Ano em cores (a cor que o app já extrai da arte), e depois Galáxia do gosto e Retrospectiva mensal e anual levada ao celular.
8. **Uma página de celular para tudo**: a casca, o bilhete e o QR do Watch Together servem a review, a importação e exportação do Letterboxd e a retrospectiva.
9. **Privacidade igual à de hoje**: tudo é privado por padrão. Quem vê o quê obedece ao `alcance` que já existe. Os membros de uma trava veem o progresso uns dos outros naquela série, porque aceitaram. As conquistas engraçadas nunca saem sozinhas.
10. **Ordem recomendada**: **Onda 1, Sofá** (Diário + Trava na mesma TV + Pegou no sono, ≈16 dias), depois **Onda 2, Caça** (≈11 dias). Juntas formam o **MVP, ≈27 dias**. Seguem Celular & Reviews (≈11), Trava com amigos + Watch Together (≈8), Mais sobre você (≈8) e Retrospectiva + Galáxia (≈10). **Total ≈64 dias de trabalho.**

---

## 2. Princípios ("não perder o foco")

| Princípio | Como vira regra |
|---|---|
| **Funcional antes de bonito** | Cada recurso precisa de uma ação útil além do enfeite. A trava evita spoiler e organiza a noite. A Caça vira **fileira na Home** ("faltam 3 da Greta"). O "Pegou no sono" **retoma de onde você dormiu**. A retrospectiva vira **cartão no celular** |
| **Seamless** | Nada pede cadastro novo. O Diário se monta sozinho das fontes que já existem. A conquista aparece **depois** dos créditos ou na volta à Home, nunca no meio do filme. Digitar texto só no celular |
| **Controle remoto** | Setas + OK + Voltar fazem tudo. **Sem teclas coloridas novas** (a mesma decisão do Watch Together, que guardou a AZUL para a ilha). No máximo duas paradas de foco por cartão. Nenhum texto obrigatório na TV |
| **Divertido sem spam** [R5] | Sem badge de presença. O progresso aparece **antes** do prêmio. Streak em **semanas**, com uma semana de folga. **No máximo 1 aviso de conquista por sessão**, e os outros vão juntos ("+2 conquistas"). As engraçadas ficam privadas |
| **Honesto** | Nenhum número inventado (a regra do `perfil.c`). Onde o dado é inferido ("Talvez vocês tenham visto juntos"), a tela diz isso. O "Pegou no sono" pergunta antes de afirmar |
| **Leve na TV** | Nada de rede no quadro. Cálculo num fio próprio. A sondagem pega carona no ciclo social de 60 s (ETag), que já existe. Orçamento na seção 13 |
| **Glass UI** | Cartões de vidro, a ilha do relógio para avisos (`ilhaacao`, `ilhasinais`), modais que nascem da pílula, Frost de fundo, o acento do tema. Mockup aprovado antes de implementar (regra do dono) |

---

## 3. O que já existe, e as lacunas

| Peça | Estado (lido no código) | O que este plano faz |
|---|---|---|
| Perfil e Stats 2.0 (`perfil.c`) | Uma tela sem rolagem, com 3 paradas de foco e duelo com amigo. **Os dados vêm só do Trakt** (`trakt_perfil`). Sem Trakt: "Trakt desconectado" | O Diário alimenta a tela para quem não tem Trakt. Entram abas e uma segunda "página" (seção 7.1) |
| "Jornada" | **Só protótipo** (`design/universo-2026`: "Sua jornada nas histórias", grade de episódios, "Rafa está no mesmo ponto") | A trava realiza a parte "no mesmo ponto". A aba "Em andamento / Para outro momento" pode virar a v2 da trava |
| Conquistas | **Não existem** | Tudo novo (seção 8) |
| Episódios vistos (`vistoep.c`) | Mapa por título, **só do perfil ativo** (conta Nuvio + Trakt) | Ganha leitura de outros perfis para a trava |
| Progresso (`progresso.c`) | `progresso.txt` com coluna de perfil, **todos os perfis da TV**, `prog_continuar_de_perfil` | Base local da trava na mesma TV |
| Conta Nuvio | PIN por perfil (`temPin`, `perfis_verificar_pin`, que **bloqueia**). `sync_push_watched_items` e `sync_push_watch_progress` com `p_profile_id` | O modo Juntos marca visto e posição nos outros perfis |
| Borrão de spoiler | `AJ_DET_BLUR_NAO_VISTOS`, `gfx_desfocado` | Reaproveitado para o episódio "à frente da trava" |
| Reação nos créditos (`reacao.c`) | 3 botões → Trakt 8/5/2, por perfil | Vira o **atalho** da nota (4/2,5/1 estrelas) |
| Amigos no título (`amigostitulo.c`) | Quem viu, gostou, nota, resposta, % de gosto | Ganha a review e a curtida |
| Cor da arte (`corviva.c`) | Cor dominante dos pixels que o decode já tem | Alimenta o "Ano em cores" sem custo |
| Pessoa e diretor (`pessoa.c`, `diretor.c`) | Ficha + filmografia TMDB na TV | Porta da Caça ("Você viu 7 de 12 · Caçar") |
| Fileiras (`colecoes.c`, `colfileiras.c`) | Fontes TMDB `PERSON`, `DIRECTOR`, `COLLECTION` | "Fazer fileira" da Caça |
| Celular (`celular.c`, `celbotao.c`, `qr.c`) | Servidor na LAN para digitar (LG e Samsung **não medidos**). QR até a **versão 6, que comporta 134 bytes** | Este plano usa a página na **nuvem**. **Atenção: a URL do QR precisa caber em 134 bytes** (seção 9.3) |
| Servidor (`nuvio-recomendacoes`, D1) | Pessoa por perfil (`nuvio:<sub>:<i>`), amigos, bloqueio, alcance, eventos, agregados, identidades (Trakt/Simkl/Letterboxd declarado), enquete. Migrações 001 a 009 (a 010 é do Watch Together) | Migrações **011** (diário + meta), **012** (trava), **013** (review + conquista) |
| Watch Together | Planejado: Worker `nuvio-juntos`, DO `Sala`, bilhete HMAC, página `/s/<código>#<bilhete>`. **Só o marco 1 (servidor) feito; nada publicado** | Reuso da casca da página e do bilhete. "Travar + assistir juntos" |

---

## 4. Fundação: o Diário (F0)

Antes de qualquer tela, o app precisa saber **o que cada perfil viu, e quando**. Hoje isso está espalhado: o histórico por título do catálogo, o `vistoep` só do perfil ativo, o progresso e o Trakt, que só quem vinculou tem.

### 4.1 O que é uma entrada

| Campo | Tipo | Origem |
|---|---|---|
| `chave` | `tt123` ou `tt123_s2e5` (o mesmo formato de `prog_chave`) | todas |
| `tmdb` | número (0 = não se sabe) | Trakt traz; meta cache do servidor (4.4) completa |
| `quando` | epoch do fim (ou o `watched_at`) | player / conta / Trakt |
| `inicioHora` | hora local em que começou (0..23) + dia da semana | **só do player** (Relógio de sessão) |
| `seg` | segundos tocando de verdade | player (o mesmo `seg` que o `atividade.c` já soma) |
| `nota10` | 0..10 (0 = sem nota) | reação, nota na TV, celular, Trakt, import do Letterboxd |
| `flags` | rever, dormiu, gostei (coração), juntos, importado | — |
| `cor` | 0xRRGGBB | `corviva` no player |
| `com` | lista curta de ids (perfis da trava Juntos, parceiros do Watch Together) | trava / sala |
| `origem` | `p` player, `c` conta, `t` Trakt, `l` Letterboxd | — |

**Arquivo**: `diario-p<N>.txt`, uma linha por entrada, no padrão de texto do app. Teto de **20 mil entradas por perfil** (cerca de 1,2 MB em texto; os episódios são a maioria). Na memória fica só o índice de **títulos** (hash imdb → contagem, última data, nota), cerca de 40 bytes cada. Os episódios são lidos por *stream* quando uma tela pede.

### 4.2 Como ele se monta (barato)

1. **Player** (sempre, mesmo com `alcance` 0, porque é local): no `fim` do `atividade.c` (90% ou créditos), grava a entrada com `seg`, `inicioHora`, `cor`, e `com` se houver trava ou sala.
2. **Conta Nuvio**: `sync_pull_watched_items` já baixa `content_id, season, episode, watched_at` (`contalib.c`). Hoje só vira "visto". Passa a virar entrada `c`, sem pedido novo.
3. **Trakt**, quando vinculado: **não** paginar `/sync/history` inteiro, porque são 100 por página e quem tem 20 mil plays faria 200 pedidos. Faz assim:
   - `/sync/watched/movies` (250 por página, uma linha por filme com `last_watched_at` e `plays`) e `/sync/watched/shows` (um pedido). Isso dá **completude**.
   - `/sync/history?start_at=<1º de janeiro>` só para o **ano corrente**, que é o que dá as **datas** do Diário e da Retrospectiva.
   - Depois, incremental: `start_at=<último quando>`.
   - Fila de baixa prioridade, com o cursor salvo para retomar e sem disputar com a Home.
4. **Letterboxd**: importação pelo celular (seção 6.5), entrada `l`.

**Fusão:** a mesma `chave` com `quando` a menos de 6 h de distância é a mesma vez. A ordem de confiança é `p` > `t` > `c` > `l`. Rever conta como outra entrada.

### 4.3 Na nuvem (opcional e privado)

Para o Diário não morrer ao trocar de TV, e para que quem não tem Trakt tenha histórico, o diário sobe para o nosso servidor (tabela `diario`, seção 10.1). Isso **não é compartilhamento**: é cofre. Sobem em lote (até 200 por pedido) as entradas novas e as notas e flags editadas. Na primeira abertura do Diário a TV pergunta uma vez: "Guardar seu diário na sua conta do Nuvio Legacy, para não perder ao trocar de TV? Só você vê." ([P3](#17-perguntas-para-o-dono)). Sem `NUVIO_REC_URL` no pacote, ele fica só local.

### 4.4 Meta cache no servidor (para conquistas e Galáxia)

Para saber **quem dirigiu** os filmes que você viu, a TV precisaria de um pedido TMDB por filme. Em vez disso, o servidor guarda uma tabela **compartilhada** `filme_meta` (imdb → tmdb, ano, duração, gêneros, países, diretores, coleção), preenchida sob demanda:

- `GET /v1/meta?imdb=a,b,c` (até 100 por pedido): devolve o que tem. O que falta vai para uma fila que o Worker resolve com a chave TMDB (**secret novo `TMDB_KEY`**; o Worker hoje não tem) e responde na próxima vez.
- `GET /v1/filmografia?pessoa=<tmdb>&papel=dir|ator`: a filmografia já classificada (longa, documentário pelo gênero 99, curta por duração < 40 min, filme de TV) com o imdb de cada filme, guardada por 14 dias.
- Custo: o primeiro usuário paga os pedidos TMDB, e os seguintes leem do D1. Os termos de cache do TMDB precisam ser conferidos [R7].

---

## 5. A) Trava de série

### 5.1 Regras

**Modos** (copiados da captura e afinados):

- **Juntos**: "Vocês assistem juntos. Um episódio que conta como visto é marcado para todos, não importa qual perfil estava na TV."
  - Ao terminar um episódio (a regra de `fim`), a TV grava **visto para cada membro** na conta Nuvio (`sync_push_watched_items` com o `p_profile_id` de cada um) e copia a posição de retomada (`sync_push_watch_progress`), para o "Continuar" de todos apontar para o mesmo lugar.
  - No Trakt, só marca nos perfis cujo token está **nesta TV** ([P1](#17-perguntas-para-o-dono)).
  - A ilha confirma, com Desfazer: "Marcado para Jerry e Sandra" (`ilhaacao`, com `sync_delete_watched_items` como inverso).
  - **Só entre perfis da mesma conta.** Entre amigos de casas diferentes, "Juntos" só vale **dentro de uma sala do Watch Together** (5.6).
- **Separados**: "Cada um assiste no próprio perfil. Quem está à frente espera os outros."
  - O **limite** de cada membro é o episódio mais adiantado que **todos os outros** já viram, mais um: dá para ver o próximo, mas não o seguinte.
  - Ordem por (temporada, episódio). Os especiais (temporada 0) ficam fora da regra.
  - Membro que ainda não começou segura todos no E1 ("Jerry ainda não começou").

**Posição de cada membro** = o episódio mais adiantado visto (não a contagem), vinda de:

- mesma TV: `progresso.txt` + `vistoep` do perfil ativo;
- mesma conta em outra TV ou em outro app Nuvio: `sync_pull_watched_items` do perfil do membro, filtrado pela série, no máximo de hora em hora e só com a trava existindo;
- amigos: o servidor (`trava_membro`, 10.2).

**"Assistir mesmo assim"**: sempre possível (ninguém fica preso), mas:

- com "Pedir PIN" ligado, pede o **PIN de qualquer outro membro que tenha PIN** (`perfis_verificar_pin`; ele bloqueia, então roda num fio com "Conferindo…"). Membros sem PIN aparecem com o selo **SEM PIN**, como na captura, para ninguém achar que está protegido ([P2](#17-perguntas-para-o-dono));
- entre amigos, não há PIN: vira **"Pedir licença"**, que manda um pedido para a ilha do amigo ("Henrique quer ver o T1E4 de Severance antes de você — Deixar / Não"). Sem resposta em 10 min, a TV oferece "Assistir mesmo assim" e avisa o grupo ("Henrique passou à frente"). É divertido e transparente;
- passar à frente **não quebra a trava**: o limite dos outros não muda, porque ele é calculado pelos **outros**.

**Pausar a trava** (férias): congela as esperas sem desfazer nada.

### 5.2 Telas (remote-first, Glass)

**T1. "Travar série com…"** (modal que nasce da ilha)
- Entradas: botão **"Travar"** na linha de ações da página do título (só em série); menu de contexto (segurar OK) no cartaz de série; "Nova trava" no Perfil.
- Corpo (de cima para baixo, uma parada de foco por bloco):
  1. **Pessoas**: rostos dos perfis da conta (o ativo vem marcado e fixo como "Você"), cada um com nome, onde está na série ("T1E3", "não começou") e o selo SEM PIN. **Na Onda 4** entra uma sub-linha "Amigos" com quem tem a série em andamento primeiro. OK alterna.
  2. **Modo**: dois cartões lado a lado, Juntos e Separados, com a explicação embaixo (o texto do 5.1). Com amigos selecionados, Juntos fica desabilitado com "Com amigos de outra casa, usem o Watch Together".
  3. **"Pedir PIN para Assistir mesmo assim"**: chave. Só aparece se algum membro tem PIN.
  4. **Linha de estado** viva: "Vocês ainda não começaram." / "Vocês estão no mesmo episódio, T1E1. Bom começo." / "Sandra está 2 episódios atrás: você espera por ela no T1E5."
  5. **Cancelar / Travar série**.
- Perfil infantil: entra em trava só com perfis da mesma conta (a regra do Watch Together).

**T2. Página do título travada**
- **Chip no hero**: rostos + "Travada · Separados" (ou "Juntos"). OK abre Gerenciar (T7).
- **Lista de episódios**: para quem está à frente, os episódios além do limite ficam com **miniatura e sinopse borradas** (`gfx_desfocado`, o mesmo do `AJ_DET_BLUR_NAO_VISTOS`), os rostos de quem falta e a linha **"Esperando Jerry e Sandra"**. OK abre uma folha com:
  - **"Mostrar fontes mesmo assim"** (PIN ou licença);
  - **"Me avisar quando alcançarem"** (já fica ligado por padrão; aqui dá para desligar);
  - **"Assistir juntos"** (Watch Together), se um membro amigo estiver online.
- O episódio **dentro** do limite fica normal, com os rostos de quem também já viu (pequenos, embaixo da miniatura).

**T3. "Continuar assistindo"**
- Série em que você **está esperando**: o cartão vai para o **fim** da fileira, com o selo "Esperando Jerry". Ele não some, mas não incomoda.
- Quando **todos estão prontos**: o cartão volta para a **frente** com "Todos prontos para o T1E5".
- Modo Juntos: o cartão é o mesmo em todos os perfis (a posição foi copiada).

**T4. Ilha**
- "Sandra alcançou você em Severance. Próximo: T1E5" → Abrir. Aparece **fora do player**, no máximo 1 por série por dia.
- Na **troca de perfil** (`perfilsel`, abaixo do rosto, no espaço do `perfilcont`): "Severance: Henrique está te esperando no T1E4". É o lembrete mais natural para quem divide a TV.

**T5. Fim de episódio (player)**
- **Separados**, e você chegou ao limite: o cartão de "Próximo episódio" (`player_regra_proximo`) vira **"Você alcançou Jerry. O T1E5 espera por ele."** com as ações [Assistir mesmo assim] e [Ver outra coisa]. Sem contagem automática.
- **Juntos**: o cartão normal, mais a pílula "Marcado para Jerry e Sandra" (Desfazer).

**T6. Perfil › "Travas de série"** (a segunda página do Perfil, seção 7.1)
- Um cartão por trava: arte da série, chip do modo, rostos com barra de progresso por membro, uma frase ("Vocês estão em passo" / "Jerry está 3 atrás") e **Gerenciar**.

**T7. Gerenciar trava**: trocar o modo, adicionar ou tirar membro, PIN, Pausar, Sair (você), Desfazer trava (só quem criou ou, na mesma conta, qualquer adulto). Toda ação passa pela ilha com Desfazer quando há inverso.

### 5.3 Dados

| Onde | O quê |
|---|---|
| **Servidor** (`trava`, `trava_membro`, migração 012) | Definição da trava, membros, aceite, posição de cada um (temporada, episódio, quando). É **uma fonte só para mesma TV, mesma conta em várias TVs e amigos**: o mesmo modelo nas três situações |
| **Local** (`travas.txt` por conta) | Cópia da última resposta (ETag) para funcionar sem rede, e a fila de "passos" a enviar |
| **Sem `NUVIO_REC_URL`** | Trava só local, só nesta TV, só com perfis da conta. Funciona, mas não viaja |
| **Conta Nuvio** | Só escrita de visto e posição no modo Juntos (já existe RPC). Nada de tabela nova no Supabase, que não é nosso |

### 5.4 Sincronização (custo)

- **Passo**: ao terminar um episódio de série travada, `POST /v1/trava/passo` com `{trava, t, e}`. Também ao marcar visto pelo menu. É um pedido por episódio.
- **Leitura**: a TV **não ganha sondagem nova**. A resposta de `/v1/rec`, que já é sondada a cada 60 s com ETag, passa a trazer `travaRev`. Se ele mudou, a TV faz um `GET /v1/travas` (com ETag).
- **Reconciliação pela conta** (membros da mesma conta que assistiram em outro app Nuvio): de hora em hora, só para as séries travadas.
- O **"alcançou você"** sai da comparação entre a resposta velha e a nova. Não há push.

### 5.5 Casos de borda

- **Episódio visto fora de ordem** (pulou o E3): a posição é o mais adiantado, então o E3 continua "não visto", mas não segura ninguém.
- **Metadados diferentes entre TVs** (addon A numera diferente do B): a comparação é por (temporada, episódio) do TMDB/Cinemeta. Se a contagem divergir, o app usa o menor e avisa no Gerenciar.
- **Membro sumiu** (perfil apagado, amigo desfeito ou bloqueado): ele sai da trava sozinho e a ilha avisa.
- **Duas travas na mesma série** com pessoas diferentes: permitido. O limite efetivo é o mais restritivo, e o episódio diz quem falta em cada uma.
- **Rever** a série com trava: travas só olham episódios ainda não vistos pelo próprio perfil.

### 5.6 Com Watch Together (Onda 4)

- **"Travar + assistir juntos"**: na página do título travada, com membros amigos online, o botão principal vira **"Assistir T1E5 juntos"** e cria a sala já com os membros convidados (rota `POST /v1/sala` do plano do Watch Together, `convidados` = membros).
- **Episódio terminado na sala** = passo para **todos os membros da sala** que são membros da trava, inclusive no modo Juntos entre casas diferentes. É a única situação em que Juntos atravessa casas.
- O evento `junto` do feed (plano do Watch Together, decisão 8) alimenta os **Parceiros de sofá** (7.4).

---

## 6. C) Diário, notas e reviews (+ celular + Letterboxd)

### 6.1 Nota

- **Escala única: 1 a 10, mostrada como 5 estrelas com meia** (0,5 = 1). É a mesma do Trakt (`/sync/ratings`), do Simkl e do Letterboxd (`Rating10`) [R1, R2] ([P4](#17-perguntas-para-o-dono)).
- **Atalho**: a reação dos créditos continua com 3 botões (Gostei = 8 = 4 estrelas, Mais ou menos = 5, Não gostei = 2) e já vira nota.
- **Nota fina na TV**: na página do título, o campo "Sua nota" (estrelas). ESQUERDA/DIREITA anda meia estrela, OK confirma, e a ilha confirma com Desfazer. Se o Trakt estiver vinculado, a nota vai para lá também (substituindo a da reação).
- **Coração** (gostei, à moda do Letterboxd): independe da nota. OK no coração.

### 6.2 Review na TV: tags rápidas

Depois da nota, o cartão oferece **até 3 tags** de uma lista fechada e traduzida: Visual · Trilha · Atuações · Roteiro · Final · Ritmo lento · Chorei · Ri muito · Assustador · Quero rever · Superestimado · Subestimado. Mais "Escrever no celular" (QR). As tags viram uma mini-review legível ("Visual · Final · Chorei") para os amigos. **Nada de teclado na TV.**

### 6.3 Review longa no celular

**Fluxo**: "Escrever no celular" (na página do título, no Diário ou no cartão de nota) → **QR** num cartão de vidro com o pôster ao lado → o celular abre a página → a pessoa escreve e envia → a ilha da TV diz "Review de Duna publicada" (Desfazer = apagar).

**A página do celular** (`/m/r`, seção 9):
- pôster, título, "visto em 04/10" (editável), estrelas com meia (arrastar), coração e as tags que vieram da TV, já marcadas;
- texto de até **5.000 caracteres**, chave **Contém spoiler**, visibilidade (**Só eu** / **Amigos**, e Público fica para a v2);
- **"Também no Trakt"**, se esta TV tem Trakt. Quem publica é a **TV**, que tem o token: o servidor só guarda o texto e a TV, ao receber, faz `POST /comments`. Regras do Trakt: mínimo de 5 palavras, e com 200 ou mais vira review [R2]. A página avisa;
- **"Levar ao Letterboxd"**: copia o texto para a área de transferência e abre a página do filme no Letterboxd, pelo link de tmdb (a conferir [R1]), para a pessoa colar e publicar lá **ela mesma**. Depois ela pode colar de volta o **link** da review dela (6.5);
- **"Nota relâmpago"** (divertido, v2 da Onda 3): os cartões de tudo que você viu e não avaliou, em fila. Arraste para dar estrelas. Zera o atraso de notas em 1 minuto.

**Episódio e temporada**: a review também pode ser de episódio ou de temporada (Serializd/TV Time [R3]). Para o MVP do recurso, só título e episódio.

### 6.4 Diário (tela)

- **Lista agrupada por mês** (vidro, sem rolagem livre: a página anda por mês). Cada linha tem o dia em número grande, a miniatura, o título, as estrelas e os selos (rever, dormiu, com Jerry, review).
- Filtros no topo: Filmes · Séries · Tudo, mais o ano.
- OK numa entrada abre uma folha: mudar data, nota, tags, coração, "Escrever no celular", **"Apagar do diário"**, que só apaga a entrada do diário, nunca o "visto" (essa diferença é dita na tela).
- Entrada vinda do Trakt ou da conta sem hora aparece com "data aproximada".

### 6.5 Letterboxd, dentro do ToS [R1]

| Caminho | Como | Por que pode |
|---|---|---|
| **Exportar para o Letterboxd** | No celular, "Baixar CSV do Letterboxd" gera, **no navegador**, o arquivo `Title, Year, imdbID, tmdbID, WatchedDate, Rating10, Rewatch, Tags, Review` (até 1 MB, dividido em partes se passar) a partir do diário que veio do servidor. A pessoa importa em letterboxd.com/import | É o importador oficial deles, feito pela própria pessoa |
| **Importar do Letterboxd** | A pessoa baixa o ZIP dela (Settings › Data), sobe no celular, e a página lê `diary.csv`, `ratings.csv` e `reviews.csv` **no navegador** e manda as linhas ao servidor. O casamento é por título + ano no meta cache (o CSV não traz tmdb/imdb). O que não casou aparece numa lista "Não achei 12 filmes" | É o dado **dela**, enviado por ela. Não há acesso automatizado |
| **Ligar uma review** | Colar a URL da review (`letterboxd.com/<user>/film/<slug>/`) no celular. Guardamos **só o link**. Na TV, o QR "Ler no Letterboxd" | Não lemos a página |
| **Identidade** | O `identidade` com `provedor = letterboxd`, `metodo = 'declarado'` já existe (migração 008) | — |
| **API oficial** | Pedido por e-mail, sem garantia de resposta ([P8](#17-perguntas-para-o-dono)) | Só se aprovarem |
| **RSS** | **Não.** A decisão de 02/10 continua (ToS: "monitor") | — |

### 6.6 Reviews de amigos (página do título)

- A linha de amigos do título (`amigostitulo_ui`) ganha, por amigo, as estrelas e a **primeira linha da review**: "Fabi ★★★★½ — o final me pegou".
- OK abre o **leitor**: vidro, texto em corpo grande, as tags, a data e **Curtir** (OK no coração).
- Review com spoiler fica **borrada até você ter visto** (`cat_visto` / `vistoep`). Quem quiser ler mesmo assim aperta OK duas vezes.
- **Responder**: só pelo celular (QR no leitor), na v2.
- Ordem: quem você tem mais "gosto parecido" (`gosto` do `AmigoTit`) primeiro.

### 6.7 Dados

- **Local**: nota, tags e coração no `diario-p<N>.txt`. O rascunho da review fica no servidor até ser publicado.
- **Servidor** (migração 013): `review`, `review_curtida`. A visibilidade é conferida a cada leitura: Amigos = contato confirmado, não bloqueado, e o autor com `alcance` ≥ 1.

---

## 7. D) "Mais sobre você", Galáxia e Retrospectiva

### 7.1 Onde mora: o Perfil ganha abas e uma segunda página

O `perfil.c` é **uma tela sem rolagem** por decisão. Isso continua:

- **Abas no topo** (pílulas de vidro, ESQUERDA/DIREITA): **Visão geral · Diário · Caça · Galáxia · Retrospectiva**.
- **Visão geral, página 1** = a tela 2.0 de hoje (números, ritmo, mais vistos, amigos/duelo), agora alimentada pelo Diário quando não há Trakt.
- **Visão geral, página 2**: BAIXO depois do último cartão **troca de página** com um encaixe (não é rolagem livre). Contém "**Travas de série**" (T6) e "**Mais sobre você**" (4 cartões em grade). CIMA volta.

### 7.2 Pegou no sono (útil + divertido)

**Detecção** (pura e testável, `dormiu_regra`):
- tocando, sem **nenhuma tecla** há ≥ 40 min (e ≥ 35% da duração), **e** uma destas:
  1. a sessão acabou sem despedida: a TV desligou ou o app morreu (a marca `sessao-viva.txt` já existe, de `avisos.c`) com o título ainda tocando;
  2. passou dos créditos e a próxima tecla veio ≥ 10 min depois;
  3. ao retomar depois, a pessoa voltou ≥ 10 min.
- **Ponto provável** = a posição na **última tecla**, mais uma margem de 5 min. A tela diz "por volta de".

**UX:**
- Na próxima vez que o título aparece (Home ou página): a ilha pergunta **"Você pegou no sono em Duna? Voltar para ~1:12"** com [Voltar] [Não dormi]. "Voltar" põe a retomada lá. "Não dormi" apaga a marca (e ensina o limiar).
- O cartão **"Pegou no sono"** em Mais sobre você mostra a lista dos confirmados ("Você dormiu 4 vezes em outubro. Recorde: 31 min de Interestelar"), e OK em um deles retoma.
- **Privado sempre.** Nunca vai para amigos, a menos que a pessoa compartilhe uma conquista engraçada de propósito.

### 7.3 Relógio de sessão (Watch Clock)

- Mapa 7 dias × 24 h com 3 intensidades do acento (a linguagem do "ritmo" do perfil) e uma frase: **"Você é da sessão de sexta, 22h."** / "Domingo à tarde é das séries."
- Fonte: `inicioHora` + `seg` do Diário, com entradas do player apenas (Trakt e conta não dão hora de início confiável; a tela diz "das sessões nesta TV e nas suas TVs com Nuvio Legacy").
- OK: alterna "filmes / séries".

### 7.4 Parceiros de sofá (Couch Buddies)

- Rostos de com quem você assiste, com contagem: travas Juntos (perfis da casa), salas do Watch Together (amigos) e, como "talvez", o mesmo título no mesmo dia nesta TV por dois perfis.
- **Acordo**: com ≥ 3 títulos avaliados pelos dois, "Vocês concordam em 78%". O **maior desacordo** vira piada: "Tenet: você 9, Jerry 3."
- OK num rosto: o duelo do Perfil 2.0, que já existe, para amigo. Para perfil da casa, uma versão local do duelo (os dois diários estão nesta TV).

### 7.5 Ano em cores (Colour Year)

- Uma **faixa de película**: uma tira vertical por filme do ano, em ordem de data, na cor dominante da arte (`corviva`, guardada no Diário no momento do play).
- Entradas importadas sem cor: a tela pede **backdrops pequenos (w300)** em fila lenta e extrai no fio de decode que já existe. No máximo cerca de 365 por ano, uma vez, e a cor fica guardada.
- OK: anda pela faixa e mostra o pôster daquele dia. Frase: "Seu outubro foi azul-petróleo."

### 7.6 Galáxia do gosto (Onda 6)

- Gêneros = sóis, diretores e atores = planetas (o tamanho é a contagem), filmes = estrelas. Posições **determinísticas** (espiral por hash, sem física). As setas andam entre corpos próximos e OK abre o diretor, a pessoa ou a Caça.
- No máximo ~400 sprites de um atlas só (um círculo com brilho) e deriva lenta. Com GPU fraca (`ptv_gpu_fraca`), fica estática.
- Dados: Diário + meta cache (4.4).

### 7.7 Retrospectiva (Wrapped)

- **Mensal**: no dia 1, a ilha avisa "Seu setembro está pronto". São 5 slides: horas, título do mês, diretor do mês, **cor do mês**, conquista do mês e parceiro de sofá.
- **Anual**: em dezembro, "Retrospectiva 2026" com 8 a 10 slides, incluindo uma **"corrida"** dos seus diretores [R6], "Você vs. amigos" (o duelo) e o primeiro e o último filme do ano.
- **TV**: slides em tela cheia sobre Frost. Avançam sozinhos em 6 s, ESQUERDA/DIREITA anda, OK pausa.
- **O último slide é o QR "Levar para o celular"**: a página monta os **cartões 9:16 no navegador** (canvas → PNG, `navigator.share`). O servidor não renderiza imagem.
- Dados: calculados **na TV** a partir do Diário. Ao gerar o QR, sobe um resumo JSON (sem o diário inteiro) guardado por 30 dias.

---

## 8. B) Conquistas e Caça à filmografia

### 8.1 Regras de desenho [R5]

1. **A pessoa escolhe a meta.** A Caça é um gesto ("Caçar Greta Gerwig"). O app **sugere** ("Você está a 2 filmes de fechar Villeneuve"), mas não impõe.
2. **Progresso sempre visível** (7/12, um anel). O prêmio é a consequência.
3. **Nada trivial.** Não há badge de "primeiro login", "primeiro play" nem "abriu o app 7 dias".
4. **Degraus com nome**: Primeiro passo (1) · Metade (50%) · Quase lá (falta 1, ou ≥ 80% em filmografias com mais de 10) · **Completa** (100%) · **Em ordem** (completa vista em ordem de lançamento, ou na ordem da história quando a trilha define).
5. **Raridade entre amigos**, não global: "Só você e a Fabi fecharam Kubrick."
6. **Engraçadas são privadas por padrão** (Coruja: um filme depois das 2h; Pegou no sono 5×; Maratona).
7. **Aviso**: no máximo 1 por sessão, **depois dos créditos ou na volta à Home**, nunca durante o filme. Os outros vão juntos ("+2").
8. **Nada de perder**: um streak que quebra não "perde" a conquista já ganha. A semana de folga existe.

### 8.2 Catálogo inicial (v1)

| Família | Exemplos | Fonte do conjunto |
|---|---|---|
| **Filmografia** (diretor, diretora, ator, atriz) | "Greta Gerwig: Completa", "Kubrick: Em ordem" | `/v1/filmografia` (4.4). Escopo padrão: **longas**. Chaves "+ Documentários" e "+ Curtas" (como na captura) |
| **Sagas** | "O Senhor dos Anéis em ordem", "Toy Story completa" | coleções TMDB (`/collection/{id}`) |
| **Prêmios** | "Oscar de Melhor Filme: anos 90", "10 vencedores" | Wikidata (CC0) [R7], gerado por script, versão no servidor |
| **Trilhas de curadoria** | "Introdução ao cinema coreano", "Noir essencial" (escritas pelo dono) e **as coleções da conta Nuvio da própria pessoa** ("Feche sua coleção Xperience: Ghibli") | `/v1/trilhas` (curadoria) + `colecoes.c` (local) |
| **Listas famosas** | "Os 250 mais bem avaliados" (TMDB ou lista pública do Trakt, **não** "IMDb Top 250" [R7]) ([P5](#17-perguntas-para-o-dono)) | servidor |
| **Exploração** | 10 décadas, 15 países, 5 idiomas, todos os gêneros | Diário + meta cache |
| **Hábito** (gentil) | "4 semanas seguidas com um filme", "Uma temporada num fim de semana" | Diário |
| **Social** | "Viu junto pela primeira vez" (trava Juntos ou sala), "Sua recomendação virou favorito" (rec + reação positiva), "Primeira review" | trava, sala, `rec` |
| **Engraçadas** (privadas) | Coruja, Pegou no sono, "Começou e terminou no mesmo dia uma série inteira" | Diário |

### 8.3 Tela "Caça" (aba do Perfil)

```
┌ Caça ───────────────────────────────────────────────────────────────┐
│ [Sugestões]  Villeneuve · faltam 2   Gerwig · faltam 1   + Caçar novo│  ← fileira 1 (foco)
│                                                                     │
│ ┌ Suas caçadas ─────────┐  ┌ Denis Villeneuve ─────────────── 8/10 ─┐│
│ │ ● Villeneuve   8/10 ◔ │  │ [pôster][pôster][pôster]…  (ordem de   ││
│ │ ● Kubrick     11/13 ◕ │  │  lançamento; visto em cor, falta em    ││
│ │ ● Senhor dos Anéis 3/3│  │  cinza com "Onde ver")                 ││
│ └───────────────────────┘  │ Selos: Primeiro passo ✓ Metade ✓        ││
│                            │        Quase lá … Completa … Em ordem   ││
│                            │ [Fazer fileira] [Fazer coleção]         ││
│                            │ [+ Documentários] [+ Curtas] [Parar]    ││
│                            └────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────────┘
```

- **Foco**: sugestões → lista à esquerda (CIMA/BAIXO) → DIREITA entra nos pôsteres → BAIXO nos botões.
- OK num pôster que falta: **página do título** (com "Parte da caçada Villeneuve" no hero).
- **Fazer fileira**: cria a fileira **"Caçada: Villeneuve — faltam 2"** na Home, só com o que falta, e ela some sozinha quando a caçada termina (com uma última aparição "Completa ✓" por um dia). Usa o mesmo caminho das fileiras de coleção (`colfileiras`/`colecoes` com fonte TMDB `DIRECTOR`), local por perfil.
- **Fazer coleção**: grava como coleção **local** do perfil (não empurra para a conta, que não temos como escrever com segurança: `colfileiras.h` diz "does not push anything to the account").
- **Caçar novo**: busca de Pessoas, que já existe (Busca › Pessoas), com o destino "caçar".

**Outras entradas:**
- Página da pessoa (`pessoa.c`) e do diretor (`diretor.c`): "Você viu 7 de 12 · **Caçar**".
- Página do título: chip "Caçada Villeneuve 8/10" quando o título faz parte de uma caçada ativa.
- Pós-créditos: "Faltam 2 para fechar Villeneuve" (só se a caçada estiver ativa).

### 8.4 Aviso de conquista (ilha)

- Pílula: **"Conquista: Villeneuve completo"**, com o voo do selo até a pílula (`ilhaacao` com `voar`).
- OK/AZUL abre o modal: o selo grande, o que significa ("Você viu os 10 longas de Denis Villeneuve, de 1998 a 2024"), a raridade entre amigos e as ações [Mostrar aos amigos] (só se `alcance` ≥ 1; padrão marcado, [P6](#17-perguntas-para-o-dono)) e [Ver caçada].

### 8.5 Selos sem arte por selo

O selo é **procedural**: a forma diz o degrau (círculo = Primeiro passo, meia-lua = Metade, hexágono = Quase lá, estrela de 8 pontas = Completa, fita = Em ordem), o centro é a **foto da pessoa ou o pôster da saga**, e a cor é o acento. Nenhum PNG novo por conquista, só 5 máscaras.

### 8.6 Cálculo (barato)

- **Motor puro** `conquista.c`: entra o conjunto de títulos vistos do Diário (hash) + definições (listas de imdb com ordem). Sai o progresso por definição. Custo O(itens das definições): ~50 trilhas × 100 itens = 5 mil consultas de hash, **< 1 ms**.
- Roda num **fio**, com 2 s de *debounce*, quando o Diário muda. Guarda o estado em `conquistas-p<N>.txt`. Compara com o anterior para detectar o desbloqueio.
- **"Em ordem"**: as datas do Diário dos itens da trilha estão em ordem não-decrescente (rever não conta).
- **Definições**: `/v1/trilhas` (JSON com ETag, cache de 7 dias, ~50 KB) + filmografias sob demanda (uma por caçada, cache de 14 dias) + coleções da conta (local).
- **Sugestões**: as 10 pessoas mais frequentes no Diário (pelo meta cache) com progresso ≥ 50% e falta ≤ 3.

### 8.7 Dados

- **Local**: progresso e caçadas ativas por perfil.
- **Servidor** (migração 013): `conquista(pessoa, chave, nivel, quando, visivel)`, enviada **só no desbloqueio**. `caca(pessoa, alvo, escopo)` só se o diário na nuvem estiver ligado (para a caçada viajar entre TVs).
- **Amigos**: `/v1/amigo` passa a trazer até 6 conquistas recentes visíveis, e o feed ganha `ev: conquista` (respeitando o `alcance`).

---

## 9. O celular, uma página para tudo

### 9.1 Reuso do Watch Together

O plano do Watch Together já desenha a página do celular (HTML + JS inline, sem framework, Safari/Chrome móveis), o **bilhete HMAC** e o QR no cartão. Este plano **reusa a casca** (CSS de vidro, i18n, leitura do bilhete no `#`, padrão de erro e expiração) e acrescenta **rotas de página**:

| Rota | Para | Onde é servida |
|---|---|---|
| `/s/<código>` | sala (Watch Together) | `nuvio-juntos` (plano WT) |
| `/m/r` | review e nota | **`nuvio-recomendacoes`** (proposta), porque precisa do D1 |
| `/m/lb` | Letterboxd: exportar CSV, importar ZIP, ligar link | `nuvio-recomendacoes` |
| `/m/w` | Retrospectiva (cartões 9:16) | `nuvio-recomendacoes` |
| `/m/n` | Nota relâmpago (v2) | `nuvio-recomendacoes` |

A casca vira um módulo compartilhado no repositório (`servidor/celular-comum/`: CSS, i18n, `bilhete.js`), embutido nos dois Workers no build. **Deploy separado continua separado**, conforme a decisão 5 do Watch Together ([P7](#17-perguntas-para-o-dono)).

### 9.2 Fluxo

1. A TV pede `POST /v1/celular/sessao {acao:"review", imdb, t, e}` → `{id, k}` (id curto + segredo de 16 caracteres; validade de 15 min até o primeiro uso, 2 h depois).
2. O QR mostra `https://<host>/m/r/<id>#<k>`. O segredo vai no fragmento (não aparece em log nem em Referer, a mesma regra do Watch Together).
3. O celular lê o contexto (`GET /v1/celular/<id>` com `k`): pôster, título, nota e tags vindas da TV.
4. Envia (`POST /v1/celular/<id>/review`).
5. A TV, **só enquanto o cartão do QR está aberto**, consulta `GET /v1/celular/<id>/estado` a cada 3 s. Fora disso, o aviso chega pela sondagem normal de 60 s.

### 9.3 Restrição técnica: o QR tem 134 bytes

O `qr.c` codifica só até a **versão 6 (134 bytes)**. Uma URL com bilhete HMAC completo no fragmento (JSON + assinatura em base64) **não cabe**. Por isso a proposta é id curto + segredo curto, com o estado no servidor. **Este ponto vale também para o plano do Watch Together** (`/s/<código>#<bilhete-celular>`), que deve conferir o tamanho. Alternativa: estender o `qr.c` até a versão 10 (exige o bloco de informação de versão com BCH, que o cabeçalho evitou de propósito).

### 9.4 Por que não o servidor da LAN (`celular.c`)

Ele funciona no Android e no Mac, mas **não foi medido na LG nem na Samsung**, não existe no .wgt, e o celular precisaria estar na mesma rede. A página na nuvem funciona em qualquer rede e em todas as plataformas, e o texto chega ao servidor de qualquer jeito, já que a review é guardada lá. O `celular.c` continua para digitar chaves.

---

## 10. Servidor (`nuvio-recomendacoes`)

### 10.1 Migração 011: diário e meta (Ondas 1 e 2)

```sql
-- Cofre do diario (opcional, so com o consentimento da pessoa). Episodios sao a
-- maioria; uma linha por vez vista.
CREATE TABLE IF NOT EXISTS diario (
  pessoa  TEXT NOT NULL,
  chave   TEXT NOT NULL,          -- tt123 | tt123_s2e5
  quando  INTEGER NOT NULL,       -- epoch do fim
  nota10  INTEGER NOT NULL DEFAULT 0,
  flags   INTEGER NOT NULL DEFAULT 0,   -- 1 rever, 2 dormiu, 4 coracao, 8 juntos, 16 importado
  tags    TEXT NOT NULL DEFAULT '',     -- ids da lista fechada
  cor     INTEGER NOT NULL DEFAULT 0,
  seg     INTEGER NOT NULL DEFAULT 0,
  hora    INTEGER NOT NULL DEFAULT -1,  -- dia*24+hora local do inicio; -1 = nao se sabe
  origem  TEXT NOT NULL DEFAULT 'p',
  PRIMARY KEY (pessoa, chave, quando)
);
CREATE INDEX IF NOT EXISTS diario_pessoa ON diario(pessoa, quando);

-- Cache COMPARTILHADO de metadados (TMDB), preenchido sob demanda.
CREATE TABLE IF NOT EXISTS filme_meta (
  imdb TEXT PRIMARY KEY, tmdb INTEGER, ano INTEGER, dur INTEGER,
  generos TEXT, paises TEXT, diretores TEXT,   -- "tmdb:nome|tmdb:nome"
  colecao INTEGER, atualizado INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS filmografia (
  pessoa INTEGER NOT NULL, papel TEXT NOT NULL,    -- dir | ator
  corpo TEXT NOT NULL,                             -- JSON pronto
  atualizado INTEGER NOT NULL,
  PRIMARY KEY (pessoa, papel)
);
```

Rotas: `POST /v1/diario/lote` (até 200), `GET /v1/diario?desde=` (restaurar, paginado), `DELETE /v1/diario` (apagar tudo), `GET /v1/meta`, `GET /v1/filmografia`, `GET /v1/trilhas`.

### 10.2 Migração 012: trava (Onda 1; amigos na Onda 4)

```sql
CREATE TABLE IF NOT EXISTS trava (
  id TEXT PRIMARY KEY,               -- 128 bits aleatorios
  serie TEXT NOT NULL, titulo TEXT NOT NULL DEFAULT '', poster TEXT NOT NULL DEFAULT '',
  modo INTEGER NOT NULL,             -- 0 juntos, 1 separados
  pin INTEGER NOT NULL DEFAULT 0,
  criador TEXT NOT NULL, criado INTEGER NOT NULL, atualizado INTEGER NOT NULL,
  pausada INTEGER NOT NULL DEFAULT 0
);
CREATE TABLE IF NOT EXISTS trava_membro (
  trava TEXT NOT NULL, pessoa TEXT NOT NULL,
  estado INTEGER NOT NULL DEFAULT 0, -- 0 convidado, 1 dentro, 2 saiu
  temporada INTEGER NOT NULL DEFAULT 0, episodio INTEGER NOT NULL DEFAULT 0,
  passo_em INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (trava, pessoa)
);
CREATE INDEX IF NOT EXISTS trava_membro_pessoa ON trava_membro(pessoa, estado);
CREATE TABLE IF NOT EXISTS trava_licenca (
  trava TEXT NOT NULL, de TEXT NOT NULL, ep TEXT NOT NULL,
  criado INTEGER NOT NULL, resposta INTEGER NOT NULL DEFAULT -1,
  PRIMARY KEY (trava, de, ep)
);
```

| Rota | O quê |
|---|---|
| `GET /v1/travas` | minhas travas + membros + posições (ETag). `travaRev` sai em `/v1/rec` |
| `POST /v1/trava` | criar `{serie, titulo, poster, modo, pin, membros}`. **Mesma conta** (mesmo `sub`) entra direto com `estado = 1`. **Amigo** entra como convidado (contato confirmado, sem bloqueio, `limitar()`) |
| `POST /v1/trava/aceitar` · `/sair` · `/modo` · `/pausar` · `/apagar` | gestão |
| `POST /v1/trava/passo` | `{trava, t, e}`. Só avança (nunca volta), exceto com `desfazer:1` vindo do Desfazer |
| `POST /v1/trava/licenca` · `/licenca/responder` | pedido entre amigos |

**Privacidade**: a posição de um membro só é lida pelos **outros membros daquela trava**. **Independe do `alcance`**, porque é um gesto explícito, como a resposta a uma rec (migração 007). Isso é dito no aceite: "Jerry e Sandra vão ver em que episódio você está **nesta série**".

**Limites**: 30 travas ativas por pessoa, 8 membros por trava, 20 convites por hora, 1 licença por episódio.

### 10.3 Migração 013: review, conquista e celular (Ondas 2 e 3)

```sql
CREATE TABLE IF NOT EXISTS review (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  pessoa TEXT NOT NULL, chave TEXT NOT NULL,      -- titulo ou tt_sNeM
  nota10 INTEGER NOT NULL DEFAULT 0, tags TEXT NOT NULL DEFAULT '',
  texto TEXT NOT NULL DEFAULT '', spoiler INTEGER NOT NULL DEFAULT 0,
  visivel INTEGER NOT NULL DEFAULT 0,             -- 0 so eu, 1 amigos
  link TEXT NOT NULL DEFAULT '',                  -- so letterboxd.com
  criado INTEGER NOT NULL, editado INTEGER NOT NULL DEFAULT 0,
  UNIQUE (pessoa, chave)
);
CREATE INDEX IF NOT EXISTS review_chave ON review(chave);
CREATE TABLE IF NOT EXISTS review_curtida (
  review INTEGER NOT NULL, pessoa TEXT NOT NULL, criado INTEGER NOT NULL,
  PRIMARY KEY (review, pessoa)
);
CREATE TABLE IF NOT EXISTS conquista (
  pessoa TEXT NOT NULL, chave TEXT NOT NULL, nivel INTEGER NOT NULL,
  quando INTEGER NOT NULL, visivel INTEGER NOT NULL DEFAULT 0,
  PRIMARY KEY (pessoa, chave)
);
CREATE TABLE IF NOT EXISTS celular_sessao (
  id TEXT PRIMARY KEY, segredo_hash TEXT NOT NULL, pessoa TEXT NOT NULL,
  acao TEXT NOT NULL, contexto TEXT NOT NULL, estado INTEGER NOT NULL DEFAULT 0,
  expira INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS retro (                -- resumo da retrospectiva para o celular
  pessoa TEXT NOT NULL, periodo TEXT NOT NULL, corpo TEXT NOT NULL, expira INTEGER NOT NULL,
  PRIMARY KEY (pessoa, periodo)
);
```

| Rota | O quê |
|---|---|
| `POST /v1/review` | TV (tags e nota) ou celular (texto). Texto: até 5.000 caracteres, saneado. `link` aceita só `https://letterboxd.com/...` e `boxd.it` |
| `GET /v1/reviews?chave=` | reviews de amigos visíveis para mim, em ordem de gosto parecido |
| `POST /v1/review/curtir` | alterna |
| `POST /v1/conquista` | desbloqueio `{chave, nivel, visivel}` |
| `POST /v1/celular/sessao`, `GET /v1/celular/<id>`, `POST /v1/celular/<id>/<acao>`, `GET /v1/celular/<id>/estado` | 9.2 |
| `GET /v1/retro/<id>` | para a página `/m/w` |

O `scheduled()` que já existe ganha a limpeza de `celular_sessao`, `retro` e `trava_licenca` vencidas.

### 10.4 Volume e custo (estimativa)

- **Diário**: uma pessoa ativa vê ~300 filmes e episódios por ano, ~80 bytes por linha → ~24 KB/ano. **10 mil pessoas com o cofre ligado ≈ 240 MB/ano.** O D1 do plano pago aguenta 10 GB por banco (conferir), mas o D1 de logs **já encheu uma vez** (memória "D1 dos logs encheu (500 MB)"). Medidas: (a) o cofre é opcional; (b) episódios de uma mesma série no mesmo dia viram uma linha só com `flags` de contagem, se o volume pedir; (c) `wrangler d1 info` no checklist de release.
- **Leituras**: a trava pega carona na sondagem que já existe. As reviews do título são lidas **uma vez ao abrir a página** (cache de 10 min na TV). As conquistas não têm leitura periódica.
- **TMDB pelo Worker**: limitado pela fila (no máximo N por minuto). O cache compartilhado faz o custo cair com o uso.

---

## 11. Cliente: módulos

### 11.1 Novos em `src/`

| Arquivo | Responsabilidade | Teste no Mac |
|---|---|---|
| `diario.c/.h` | Entradas, fusão das fontes, arquivo por perfil, índice de títulos, fila de envio ao cofre | `tests/diario.c` (fusão, 6 h, rever) |
| `diariofontes.c` | Conta (`contalib`), Trakt (`watched/*` + `history?start_at`), player (`atividade`) → `diario` | parcial |
| `trava.c/.h` | Modelo, **regra pura do limite** (`trava_limite`), estado local, passo, ETag, "alcançou você" | `tests/trava.c` |
| `travaui.c/.h` | T1, a folha do episódio, T5, T6, T7 | capturas (só as telas novas) |
| `dormiu.c/.h` | `dormiu_regra` pura + a pergunta na ilha + a retomada | `tests/dormiu.c` |
| `conquista.c/.h` | Motor puro, definições, desbloqueio, aviso | `tests/conquista.c` |
| `caca.c/.h` + `cacaui.c` | Caçadas ativas, filmografia, fileira e coleção, tela Caça | capturas |
| `nota.c/.h` | Nota fina, coração e tags (TV). Ponte com `reacao` e Trakt | `tests/nota.c` (mapa 3 → 10) |
| `review.c/.h` | Reviews de amigos por título (cache), curtir, publicar no Trakt ao receber do celular | parcial |
| `celsessao.c/.h` | Sessão de celular na nuvem (9.2), QR, consulta curta | parcial |
| `voce.c` | Cartões "Mais sobre você" (Relógio, Sofá, Cores, Sono) | capturas |
| `galaxia.c`, `retro.c` | Ondas 5 e 6 | capturas |

### 11.2 Mudanças em módulos existentes

- `perfil.c`: abas, segunda página, dados do Diário sem Trakt (é **a** mudança que conserta o "Trakt desconectado").
- `detail.c` / `episodios.c`: chip da trava, borrão e folha do episódio, "Sua nota", tags, linha de reviews, chip da caçada.
- `player.c` / `posplay.c`: limite no cartão de próximo, pílula "Marcado para…", última tecla (para o `dormiu`), cor no fim.
- `continuar.c` / `cwordem.c`: ordem dos cartões travados (fim da fila quando esperando, frente quando todos prontos).
- `perfilsel.c` / `perfilcont.c`: linha "Henrique está te esperando…".
- `amigostitulo*.c`: review curta + leitor.
- `pessoa.c`, `diretor.c`: "Você viu X de Y · Caçar".
- `reacao.c`: a resposta também grava `nota10` no Diário.
- `ilhasinais.c`: sinais `trava:*`, `conquista:*`, `retro:*`, `dormiu:*`.
- `recomenda.c`: lê `travaRev` da resposta de `/v1/rec`.
- `idioma_*.h`: chaves (seção 13).

### 11.3 Fios

Tudo que fala com a rede segue a disciplina do `recomenda.c`: um fio próprio, estado atrás de mutex, e o desenho só **lê**. Trocar de perfil invalida o que está em voo (geração), como no `enquete.c` e no `app.c:carregarPerfil`.

---

## 12. Privacidade (padrões)

| Dado | Quem vê por padrão | Como mudar |
|---|---|---|
| Diário (local) | Só o perfil | — |
| Diário na nuvem | Ninguém (cofre) | Perguntado uma vez. Ajustes › Social › "Guardar diário na conta" |
| Posição numa trava | **Os membros daquela trava** (aceito no convite) | Sair da trava |
| Nota (estrelas) | Amigos, se `alcance` ≥ 1 (como a reação hoje) | `alcance` |
| Review | Escolhido **na hora de escrever**. Padrão: "Amigos" se `alcance` ≥ 1, senão "Só eu" | Na própria review |
| Conquistas | Amigos, se `alcance` ≥ 1 **e** "Mostrar aos amigos" marcado ([P6](#17-perguntas-para-o-dono)) | No modal, por conquista. Ajustes › Social |
| Conquistas engraçadas, Pegou no sono, Relógio | **Só você, sempre** (só sai uma conquista engraçada que a pessoa mandar de propósito) | — |
| Parceiros de sofá | Só você | — |
| Retrospectiva | Só você. Compartilhar é gesto no celular | — |
| **Perfil infantil** | Trava só com perfis da conta. Review só com tags (sem texto pelo celular, a menos que o responsável libere em Controle parental). Conquistas sim, nunca enviadas a amigos | Ajustes › Controle parental |
| Perfil com PIN | Não mostra a linha "te esperando" de si mesmo na tela de perfis (a regra do `perfilcont`: "PIN não mostra nada") | — |

---

## 13. Desempenho (orçamento por TV)

| Item | Teto |
|---|---|
| Rede no quadro | **zero** (tudo em fio) |
| Abrir perfil (ler o Diário) | ≤ 30 ms do disco (só o índice de títulos; os episódios por *stream*) |
| Memória nova estática | ≤ 1,5 MB no total (o Tizen .wgt vive em 256 MiB de WASM: `perfiltv.h`) |
| Diário em memória | ~40 B × títulos (5 mil títulos ≈ 200 KB) |
| Recalcular conquistas | ≤ 5 ms num fio, *debounce* de 2 s |
| Tela Caça | ≤ 40 pôsteres w185 visíveis, carregados sob demanda pelo `tex_cache` (orçamento do `ptv_perfil`) |
| Ano em cores | 1 tira por filme (retângulos, sem textura). Backdrops w300 só para o que falta cor, 1 por vez |
| Galáxia | ≤ 400 sprites de um atlas, 1 *draw call* por camada. Estática com GPU fraca |
| Retrospectiva | 1 arte de fundo por slide (heroi do `ptv_heroi_max`), pré-carregada 1 à frente |
| Pedidos novos por hora, com o app aberto | 0 de sondagem nova (carona em `/v1/rec`). Mais 1 por episódio de série travada, 1 por desbloqueio e 1 ao abrir a página de título (reviews, com cache de 10 min) |
| Trakt | Importação do Diário só no 1º uso e de forma incremental. `watched/*` em vez do histórico completo |

---

## 14. i18n

- Tudo por chave em `idioma_*.h` (27 idiomas), conferido pela `tools/varredura-i18n.py`. O texto que aparece na TV **nunca vem do servidor**, exceto a review e o nome da trilha de curadoria (este com uma chave por idioma no JSON de `/v1/trilhas`, e o inglês como reserva).
- **Modelos com parâmetro** para não explodir chaves nem errar plural: "%s de %s", "Esperando %s", "Esperando %s e %s", "Esperando %s e mais %d". Os nomes de conquista são **compostos** (degrau + alvo: "Completa · Greta Gerwig"), sem uma chave por conquista.
- Estimativa: **Onda 1 ≈ 55 chaves**, Onda 2 ≈ 70, Onda 3 ≈ 45, Onda 4 ≈ 20, Onda 5 ≈ 40, Onda 6 ≈ 50. **≈ 280 no total.**
- **Página do celular**: um JSON de i18n compartilhado com a página do Watch Together (pt-BR, en e es completos; os outros caem no en até a varredura).
- **Datas e hora** respeitam o formato do idioma (`horafmt.h`). O "Relógio de sessão" usa o fuso da TV.

---

## 15. Fases, estimativas e ordem recomendada

Dias de trabalho, uma pessoa ou um agente por vez, **sem** bateria de medição em TV (regra "menos teste, mais rápido": testes puros do que mudou + 1 compilação + capturas só das telas novas; a TV quem testa é o dono). O critério é o mesmo do plano do Watch Together (MVP ≈ 30 dias).

| Onda | Conteúdo | Dias | Depende de |
|---|---|---|---|
| **1. Sofá** | F0 Diário (local + conta + Trakt incremental + player) **5** · Trava na mesma TV/conta (T1–T7, Juntos/Separados, borrão, PIN, ilha, `perfilsel`, Continuar, migração 012 só com mesma conta) **9** · Pegou no sono + "Voltar para onde dormiu" **2** | **16** | nada |
| **2. Caça** | Meta cache + filmografia + trilhas no servidor (migração 011, `TMDB_KEY`, script Wikidata) **3** · Motor de conquistas **2** · Tela Caça + entradas (pessoa, diretor, título) **3** · Aba e página 2 do Perfil, ilha, fileira e coleção **2** · i18n **1** | **11** | Onda 1 (Diário) |
| **= MVP** | **Ondas 1 + 2** | **≈ 27** | |
| 3. Celular & Reviews | Casca compartilhada + sessão curta + `/m/r` **3** · Nota fina, coração e tags na TV **2** · Reviews de amigos no título + leitor + curtir **3** · Letterboxd (CSV de ida, ZIP de volta, link) **2** · Trakt comments **1** | **11** (+3 se a casca do Watch Together ainda não existir) | MVP. Melhor depois do marco do celular do Watch Together |
| 4. Trava com amigos | Convite, aceite e licença pela ilha **4** · "Travar + assistir juntos" e passo da sala **3** · i18n e borda **1** | **8** | Onda 1 + **Watch Together MVP** |
| 5. Mais sobre você | Relógio **2** · Parceiros de sofá **2** · Ano em cores **2** · Diário (tela) **2** | **8** | Ondas 1 e 3 |
| 6. Retrospectiva + Galáxia | Retrospectiva mensal e anual na TV **4** + cartões no celular **2** · Galáxia **4** | **10** | Ondas 2, 3 e 5 |
| | **Total** | **≈ 64** (até 67) | |

### Por que esta ordem

1. **A trava é o primeiro pedido do dono** ("com os amigos e pessoas na mesma TV") e o que mais aparece nas capturas. Na mesma TV ela **não depende do Watch Together** nem de amigos aceitarem nada. Dá para mostrar em casa, com os perfis da família, na primeira semana de prova.
2. **O Diário entra junto porque é invisível e destrava tudo**: o Perfil sem Trakt, as conquistas, a retrospectiva. Feito antes, nenhuma onda seguinte precisa voltar nele.
3. **O "Pegou no sono" custa 2 dias e é útil desde o primeiro uso** (retomar). É o "presente" do MVP.
4. **A Caça é o segundo pedido explícito** ("fechar uma diretora") e é o que mais **gera uso** (a fileira "faltam 2" na Home).
5. **O celular espera o Watch Together**: a casca, o QR e o bilhete nascem lá. Fazer antes seria construir duas vezes.
6. **A trava com amigos depende da sala** para o modo Juntos fazer sentido entre casas.
7. A **Retrospectiva e a Galáxia** precisam de meses de Diário para ficarem bonitas. Lançar a retrospectiva anual em **dezembro** pede a Onda 6 pronta até o fim de novembro.

**Modelo dos agentes** (preferência do dono): Sonnet para telas, i18n, capturas e as páginas do celular. Opus só para o contrato da trava (servidor + fusão de posições) e a fusão do Diário.

---

## 16. Riscos

| Risco | Efeito | Mitigação |
|---|---|---|
| **A RPC da conta escrever "visto" em outro perfil** (`sync_push_watched_items` com outro `p_profile_id`) pode ser recusada ou ter efeito colateral nos apps oficiais | O modo Juntos fica pela metade | Medir na conta do dono **antes** da Onda 1. Plano B: Juntos marca só no nosso servidor e na TV, e o "visto" da conta fica por perfil |
| `perfis_verificar_pin` bloqueia e precisa de rede | "Assistir mesmo assim" trava a tela ou falha sem rede | Fio próprio + "Conferindo…". Sem rede, oferece "Pedir licença" para depois |
| Metadados de episódio diferentes entre TVs ou addons | Limite errado | Comparar por (temporada, episódio) TMDB; avisar a divergência no Gerenciar |
| **Filmografias do TMDB sujas** (não creditados, TV, curtas de estudante) | "Completa" impossível ou fácil demais | Escopo padrão = longas lançados; documentários e curtas são opt-in; filme de TV fora; lista de exclusão no servidor |
| Importação do Trakt pesada para quem tem histórico gigante | Rede e tempo na 1ª vez | `watched/*` para completude e `history` só do ano; fila de baixa prioridade com cursor |
| **D1 crescer** (o de logs já encheu) | 500 no social inteiro | Cofre opcional, compactação de episódios, `d1 info` na receita de release |
| Termos de cache do TMDB no Worker | Bloqueio ou chave revogada | Conferir os termos; TTL; atribuição TMDB na tela Caça |
| Letterboxd mudar o formato do CSV ou o ToS | Exportação ou importação quebra | O formato é documentado [R1]; versão do parser; nenhum acesso automatizado |
| Avisos demais (trava + conquista + enquete + rec) | Irritação, o que o dono não quer | Orçamento da ilha: 1 aviso social a cada 10 min, nada no player, fila com prioridade |
| Pressão social / privacidade da trava | Alguém se sente vigiado | Aceite explícito com a frase do que é visto; "Sair" a um OK; posição só daquela série |
| **QR de 134 bytes** | URL do celular não cabe | id curto + segredo curto (9.3); avisar o plano do Watch Together |
| Escopo crescer ("não perder o foco") | Nada termina | Ondas fechadas; o que não está na onda vai para a lista da próxima; MVP = 2 ondas |
| Tizen .wgt (WASM, 256 MiB) | Memória | Índice só de títulos; teto de 20 mil entradas; Galáxia estática |

---

## 17. Perguntas para o dono

1. **Juntos e o Trakt dos outros perfis.** No modo Juntos, além da conta Nuvio, marcamos visto também no **Trakt de cada perfil** cujo token esteja nesta TV? Ou só na conta?
2. **PIN do "Assistir mesmo assim".** Basta o PIN de **qualquer outro membro** que tenha PIN (proposta)? Ou tem de ser o do dono da conta?
3. **Diário na nuvem**: ligado por padrão (privado, como um cofre) ou perguntar na primeira vez (proposta)?
4. **Escala**: 5 estrelas com meia (= 1 a 10, igual a Letterboxd, Trakt e Simkl), mantendo os 3 botões dos créditos como atalho (8/5/2)? Está bom?
5. **"Top 250"**: aceitamos "Os 250 mais bem avaliados" do TMDB ou de uma lista pública do Trakt? O "IMDb Top 250" tem licença não comercial e não vem pronto.
6. **Conquistas para amigos**: aparecem para os amigos por padrão quando o `alcance` ≥ 1 (proposta), com as engraçadas sempre privadas?
7. **Onde mora a página do celular** de review, Letterboxd e retrospectiva: no `nuvio-recomendacoes` (tem o D1; proposta) ou tudo no `nuvio-juntos`?
8. **Letterboxd**: mandamos o pedido de acesso à API oficial (sem garantia de resposta), ou ficamos só com o CSV de ida e volta e o link colado?
