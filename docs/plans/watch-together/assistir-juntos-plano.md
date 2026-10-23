# Assistir Juntos — plano de produto e implementação

Plano para o Nuvio Legacy (código de referência: `/Volumes/ExternalSSD/nv-202-amigosdup`, master depois da 2.0.1). Nada foi alterado no código. A pesquisa com links está em [assistir-juntos-pesquisa.md](assistir-juntos-pesquisa.md); as citações no texto, como [P3], remetem às seções dela.

Status: proposta, com as decisões do dono de 06/10 (seção 0). Nada aqui foi medido em TV. Onde digo "medido", é medição que já está no código ou na pesquisa; o resto é desenho.

---

## 0. Decisões do dono (06/10/2026)

| # | Decisão | Efeito no plano |
|---|---|---|
| 1 | **Fonte parecida é aceita, com aviso.** Não se exige a mesma release | O veredito "mesma / parecida / diferente / nenhuma" decide só o aviso, nunca bloqueia (5.2, 6.9) |
| 2 | **Todos controlam por padrão**, e o anfitrião pode travar ("Só eu controlo") | 5.1 e 5.5 |
| 3 | **Reações só ao segurar OK.** Nenhuma tecla colorida | 5.4. Some o risco de conflito com as teclas coloridas |
| 4 | **O celular entra na primeira versão**: chat + controle remoto pelo QR | Sai da v1.1 e entra no MVP (5.8, 10). O MVP sobe de ~24 para **~30 dias** |
| 5 | **A conta Cloudflare já está no plano pago (Workers Paid).** Hospedagem: **Worker separado `nuvio-juntos`** com os Durable Objects. *É o padrão, e o dono pode mudar* | 7.1–7.7. Motivo: hoje um deploy da API feito da árvore errada derrubou o site da VIDAA. Isolar as salas evita acoplar ao deploy do `nuvio-recomendacoes`. A identidade é reaproveitada por um **bilhete assinado** emitido pelo `nuvio-recomendacoes` |

| 6 | **Sala de até 5 TVs** (anfitrião incluído). O limite é configurável no servidor (`SALA_MAX`, var do `nuvio-juntos`) | 7.4, escopo do MVP. Os celulares não contam como TV (no máximo 1 celular por TV) |
| 7 | **Perfil infantil participa**, com restrições (abaixo) | 7.6, 9 |
| 8 | **"Assistiram juntos" vai para o feed**, respeitando o alcance de cada um (quem tem alcance 0 não aparece, e os outros aparecem sem essa pessoa) | 5.7 |
| 9 | **Nome do recurso: "Watch Together"**, traduzido por idioma pelas chaves de `idioma_*.h` (em pt-BR, "Assistir Juntos"). "Party"/"sessão" só como termo informal no texto | 5, 8 |
| 10 | **Ordem das provas em TV: Android primeiro, depois LG** (Samsung por último) | 8.5, 10 |

**Restrições do perfil infantil (decisão 7):**
- **Convite só de quem a conta conhece**: de outro perfil da mesma conta Nuvio (`nuvio:<sub>:*`) ou de um amigo confirmado de algum perfil adulto da mesma conta. Convite de qualquer outra pessoa é descartado no servidor, e nada aparece na TV.
- **Sem entrada por código vinda de estranhos**: o perfil infantil não entra em sala por código se o anfitrião não cumprir a regra acima. Numa sala criada por um perfil infantil, pedidos de entrada por código de quem não cumpre a regra são recusados automaticamente, sem chegar ao modal.
- **Classificação**: o convite só aparece, e "Entrar" só funciona, se o título passar no limite do perfil (`parental.c`). Senão, a TV mostra "Este título não está liberado para este perfil" e responde "recusado" à sala, sem dizer o motivo aos outros.
- **Celular do perfil infantil**: reações e frases rápidas; o chat livre vem desligado (o responsável pode ligar em Ajustes > Controle parental). As mensagens de chat dos outros aparecem na TV infantil só se "Mensagens na TV" estiver em "Mostrar", e o padrão para o perfil infantil é "Só reações".
- **Como o servidor sabe**: a TV manda `infantil:1` nas rotas `/v1/sala*` (o controle parental mora na TV). O `nuvio-recomendacoes` aplica as regras de convite e de código com esse sinal. É confiança no aparelho, o mesmo modelo do controle parental de hoje.

Todas as perguntas da seção 12 estão respondidas.

---

## 1. Resumo executivo

- **O que é.** Duas a cinco TVs (LG, Samsung, Android) assistem ao mesmo título, sincronizadas, cada uma com **a própria fonte resolvida na própria conta** (debrid, addons). Ninguém passa link de ninguém. O servidor só coordena o relógio e o estado.
- **Por que dá para ser o melhor.** Os grandes desligaram o recurso: o Disney+ GroupWatch em 09/2023, o Prime Video Watch Party em 2024, e a Plex tirou o Watch Together dos apps novos de TV e celular em 02/2025. Quem sobrou é extensão de navegador (Teleparty) [P2]. No mundo Stremio/Nuvio, o que existe é desktop: Peario, WatchParty (BetterStremio) e o "Together" do Harbor, que exige que cada usuário implante o próprio Worker [P1]. O Peario não funciona com os players nativos do Nuvio (NuvioMobile #1787), e há **5 pedidos abertos** de watch party nos repos oficiais do Nuvio (Mobile, Desktop, TV) [P1]. **Ninguém faz isso na TV, com o controle remoto, sobre os addons e o debrid de cada um.** Esse é o espaço.
- **Como funciona.** Uma *sala* por sessão, num **Cloudflare Durable Object**, no Worker separado **`nuvio-juntos`**. A identidade vem do `nuvio-recomendacoes` por um bilhete assinado. A TV conecta por WebSocket com o cliente RFC 6455 que já existe (`discordws.c`, usado pelo Discord). O servidor guarda um único "relógio da sala" (posição + instante + estado). Cada TV estima o desvio do próprio relógio (estilo NTP), calcula onde deveria estar e corrige: velocidade levemente alterada onde a plataforma aceita, *seek* onde não aceita.
- **Mesma fonte, cada um na sua conta.** O anfitrião escolhe a fonte; a sala recebe a **impressão digital do release** (infoHash + fileIdx, ou nome do arquivo + tamanho em bytes, + duração), nunca a URL. Cada convidado procura esse mesmo release nos próprios addons e resolve no próprio debrid. Se não achar, o app sugere a fonte mais parecida e avisa se a duração diferir.
- **Estilo do app.** Convite chega pela **ilha do relógio** ("Rafa te chamou para assistir Duna — Entrar"). A sala é um **cartão que nasce da ilha**. No player, a pílula mostra os rostos de quem está junto e quem está carregando. Reações curtas pelo controle (segurar OK), sem teclado. O chat de texto e o controle remoto ficam no celular, pelo QR do cartão da sala, e já estão no MVP.
- **Prático.** Três teclas para entrar. Ninguém trava a sala por mais de 20 s: quem demora entra em "alcançando" e os outros seguem. Legenda, áudio e volume são de cada um.
- **MVP** (≈ 30 dias de trabalho, seção 10): filme e série sob demanda, sala por convite de amigo ou código, pronto e contagem, sync com tolerância por plataforma, carregando e esperando, reações (segurar OK), próximo episódio juntos e **o celular por QR, com chat e controle remoto**. Ficam para depois a TV ao vivo e o alinhamento automático entre versões diferentes (v2).

---

## 2. O que a galera quer (com evidência)

Resumo da pesquisa [P3]. Os links estão no arquivo de pesquisa. O Reddit bloqueou a leitura automática, então a evidência vem de issues do GitHub (Nuvio, Stremio, Jellyfin, Harbor), do fórum da Plex e de reviews. As linhas marcadas como *inferência* não têm fonte citável.

| Necessidade | Evidência | Decisão |
|---|---|---|
| **O núcleo é sync, o resto é opcional** | NuvioDesktop #359: o pedido é "assistir o mesmo conteúdo sem sincronizar à mão", e chat, voz e reações são dispensáveis. A Plex nunca teve chat [P1][P2] | O MVP é sync impecável. As reações são leves, e o chat fica no celular (no MVP, decisão 4) |
| **Funcionar na TV, não só no laptop** | Usuários da Plex: o app web é "too clunky" para quem só tem TV. O Hulu era só desktop, e o Android TV do Jellyfin nunca teve SyncPlay (#538) [P2][P3] | Nativo na TV, com o controle remoto, é o produto |
| **Cada um com a própria fonte** | Peario, WatchParty ("no addon sharing required") e Harbor fazem assim: cada um resolve com os próprios addons e debrid. O Harbor recusou retransmitir arquivo por risco legal (#833) [P1] | Cada um resolve na própria conta. O servidor só vê a impressão do release |
| **Fontes diferentes desalinham** | O Syncplay avisa quando os arquivos diferem (duração com mais de 2,5 s de diferença). No Jellyfin, "SyncPlay does not respect media version selection" (#7652) [P1][P2] | A impressão do release e o veredito "mesma/parecida/diferente" aparecem antes de entrar |
| **Sync que não trava todo mundo** | Jellyfin: a sala ficava presa em Waiting depois do buffering (#5557). Teleparty: dessincroniza e cai a cada ~2 min (Trustpilot) [P2] | Barreira de buffering com **prazo** (20 s) e modo "alcançando" |
| **Entrar no meio** | O NuvioDesktop #359 exige isso. O Harbor entrega o convite a quem chega depois. A Plex não deixava entrar depois do início, e os usuários não gostaram. O bug #8579 do Jellyfin calculava errado a posição de quem entrava [P1][P2] | Entrar a qualquer momento: a posição vem do relógio da sala, extrapolada certo |
| **Quem controla** | Stremio Community #133: "qualquer um controla, todos seguem". Discord e Meet: todos, com chave do anfitrião. Syncplay: operadores [P1][P2] | Padrão "todos controlam" (grupo pequeno de amigos), com chave "só o anfitrião" |
| **Convite sem atrito** | No Harbor, usuários travaram ao criar uma conta Cloudflare própria (#236), e o link de convite caía na home (#404) [P1] | Convite pelos amigos que já existem, ou código curto. Nada para configurar |
| **Celular como controle** | NuvioTV #1894 quer controlar a TV pelo celular via QR/código, com Worker + Durable Object [P1] | Celular no MVP, pelo QR do cartão da sala (decisão 4) |
| **Voz** | O Premium de voz do Teleparty é relatado como instável. O SharePlay deixa a voz no FaceTime, e o Discord é o caminho comum [P2] | Não fazemos voz |
| **Reações e eventos discretos** | O SharePlay mostra "fulano pausou" e faz *ducking* [P2]. *Inferência*: chat lateral cobre a tela | Eventos com rosto na ilha e reações curtas pelo controle |
| **Legenda/idioma por pessoa** | *Inferência* (nenhuma fonte direta). SharePlay e Peario não compartilham faixas | Áudio, legenda, estilo e volume nunca sincronizam |
| **Privacidade** | *Inferência* | Sala só por convite. "Assistiram juntos" no feed respeita o alcance de cada um. Nada de URL no servidor |
| **Uso emocional** | Na revolta contra a Plex: relacionamento a distância, família em outro país [P3] | Tom acolhedor nos textos. Funciona entre países (o relógio é monotônico, e o fuso não importa) |

---

## 3. Concorrentes

Detalhes e fontes em [P2]. Resumo:

| Produto | Onde roda | Fonte | Sync | Social | Destino / lição |
|---|---|---|---|---|---|
| Teleparty (ex-Netflix Party) | Extensão Chrome/Edge (+ iOS no Premium) | Assinatura de cada um | Eventos repassados | Chat lateral, reações; vídeo e voz no Premium (US$ 3,99/mês) | Vivo. Reclamações: desync e quedas a cada ~2 min |
| Disney+ GroupWatch | App Disney+ (incl. TVs) | Assinatura de cada um | Servidor | Só reações | **Removido em 18/09/2023**, sem motivo oficial claro (a especulação é custo × uso) |
| Prime Video Watch Party | Desktop e Fire TV (limitado) | Assinatura de cada um | Anfitrião | Chat | **Removido em silêncio em 2024**. Na enquete do Android Authority, 74% valorizavam |
| Plex Watch Together | Apps Plex | Servidor Plex do dono | Qualquer um pausa todos | Sem chat; não dava para entrar depois do início | **Fora dos apps novos (TV/celular) em 02/2025**: portar na reescrita "não era prático". Segue só no web. Revolta dos usuários |
| Hulu Watch Party | Navegador desktop | Assinatura | Pausa de um pausa todos | Chat + reações, até 8 | Sem TV |
| **Jellyfin SyncPlay** | Web, Android (não no Android TV oficial) | Mesmo servidor | Estados Idle/Waiting/Paused/Playing; NTP com 8 amostras; play agendado em `max(2×ping, 500 ms)`; SpeedToSync 60 ms–3 s; SkipToSync ≥ 400 ms (correção **desligada por padrão**) | Sem chat | Vivo, a referência técnica aberta. Bugs: Waiting preso, posição de quem entra no meio |
| Syncplay (desktop) | mpv/VLC/MPC | Arquivo de cada um | Slowdown 0,95x acima de 1,5 s; rewind acima de 4 s; aviso de duração diferente acima de 2,5 s; estado "pronto" | Chat, salas com operador | Vivo. Prova que "cada um com seu arquivo" funciona |
| Apple SharePlay | iOS/tvOS/macOS | Assinatura de cada um | `AVPlaybackCoordinator`, "esperando os outros" | Avisos "X pausou", *ducking*, conversa no FaceTime | Melhor UX de referência, fechado ao ecossistema Apple |
| Discord Watch Together | Discord | Só YouTube | Servidor; "Sharing mode" para todos controlarem | Voz/chat do Discord | Limite de 180 min por sessão |
| Google Meet live sharing | Android + Meet | YouTube (Premium do anfitrião) | Todos controlam | Chamada | Nicho |
| Kast / Rave / Scener | Apps próprios / navegador | Compartilhamento de tela ou URL | Variado | Voz/vídeo/chat | Evidência fraca (blogs). Compartilhar tela tem qualidade ruim |
| **Peario** (Stremio) | Navegador + Stremio rodando | Addons de cada um | *Seek* duro acima de 1 s | — | Pequeno. **Não funciona com o Nuvio nativo** (#1787) |
| **WatchParty / BetterStremio** | Plugin do Stremio desktop | Addons de cada um | *Seek* acima de 300 ms + latência; flag anti-eco | Chat | Pequeno (25 estrelas) |
| **Harbor "Together"** | Cliente Stremio customizado (desktop) | Addons de cada um | Relógio ajustado por RTT | Chat, cursores, desenho, convite a quem chega depois | 1,7 mil estrelas. **Cada usuário implanta o próprio Worker**, e muita gente travou nisso |
| Pedidos Nuvio | NuvioMobile #1156 e #1947, Desktop #359, TV #815 e #1894 | — | — | — | Abertos, nenhum implementado |

**Lições.** (1) Os grandes desligaram o recurso por custo de manter por plataforma e por reescrita, não por falha técnica [P2]. A nossa resposta é ter um custo marginal perto de zero (Durable Object hibernado, dentro do incluso do plano pago) e manter o protocolo **desacoplado do player** (módulos puros e testados), para sobreviver às reescritas. (2) Quem sobrevive tem sync bom e não pede configuração (o atrito do Harbor). (3) Nenhum concorrente roda **na TV, sobre as fontes de cada um**. (4) A primeira coisa que os usuários estranham é a fonte diferente; o Syncplay resolve com um aviso.

---

## 4. Nossos diferenciais ("ser o melhor")

1. **Na TV, pelo controle, sobre as fontes de cada um.** Ninguém compartilha link nem conta de debrid. A sala carrega só a *impressão digital do release* e cada TV resolve na própria conta, com reserva automática para outra fonte do mesmo release ou qualidade (reaproveita `fontepref.c`, `debrid.c` e `fontevolta.c`). Isso não existe em nenhum fork Stremio/Nuvio [P1].
2. **Sync que não trava a sala.** Relógio da sala no servidor, offset NTP por TV e correção por velocidade onde a plataforma prova que muda (o medidor `VelMedidor` da 2.0.1 já mede a velocidade efetiva). Onde não há velocidade, *seek* com compensação de latência. A barreira de buffering tem prazo: quem demora vira "alcançando", e a sala não fica refém.
3. **Social nativo e Glass UI.** O convite vem dos amigos que já existem (perfil = pessoa), e o aviso chega na ilha do relógio. Os rostos ficam na pílula, as reações saem da ilha, "Rafa pausou" aparece com o rosto, e "assistiram juntos" vai para o feed com o alcance de cada um. O celular é opcional, para texto e controle.

Diferenciais menores: próximo episódio juntos com a mesma fonte por `bingeGroup`, "pular abertura" vale para a sala, entrar no meio, e funcionar em LG, Samsung e Android no mesmo protocolo.

---

## 5. Fluxos de UX (tela a tela, na língua do app)

Vocabulário: **ilha** (pílula do relógio que vira aviso, cartão ou painel), **cartão** (`ilha_cartao`), **modal da ilha** (`ilha_modal_abrir`), **Frost** (fundo vidro fosco), **plrilha** (a ilha dentro do player), **Central** (Central de Controle, segurar CH+).

Nome do recurso: **"Watch Together"** (decisão 9), com uma chave traduzida por idioma (em pt-BR, "Assistir Juntos"; os rótulos abaixo estão em pt-BR). Internamente a sessão se chama **"sala"**, e na UI aparece como "Sessão com Rafa e Fabi".

### 5.1 Começar (anfitrião)

**A. Pela página do título (`detail.c`).** Um botão novo na linha de botões, depois de "Assistir/Retomar": ícone `users` + "Assistir juntos". Só aparece com a conta Nuvio ligada e o serviço social no pacote (`amigosfil` já tem esse teste).

1. OK abre a **folha "Assistir juntos"** sobre Frost, crescendo do botão:
   - Topo: capa pequena + título + "Temporada 2 · Ep. 3" (série: o episódio focado ou o próximo a assistir).
   - **Fonte**: a linha mostra a fonte que tocaria agora ("1080p · WEB-DL · 2,1 GB · Torrentio+RD"). OK abre a folha de fontes normal (`streams`), em modo "escolher para a sala".
   - **Quem**: uma fileira de rostos dos amigos (`socialvis`). Quem está online agora (conectado a qualquer sala ou com o app aberto, v2) tem anel verde. OK marca e desmarca.
   - **Código**: "Ou entre com o código **K7M-2QX**" + QR pequeno (o mesmo `qr.c` do login). O QR abre a página do celular (5.8).
   - **Controle**: "Todos controlam" (padrão) / "Só eu controlo" (a trava do anfitrião). É um alternador, e o valor é lembrado por perfil. Também pode ser mudado depois, no cartão da sala.
   - Botão principal: **"Abrir sala"**.
2. "Abrir sala": a folha recolhe para o **cartão da sala** (5.3), e os convites saem.

**B. Pelo player (já assistindo).** A Central (segurar CH+) ganha um atalho "Assistir juntos" (lista de atalhos editável, `centrallista.c`). Também é possível pela plrilha: tecla azul no player com o OSD de pé, "Chamar alguém". Abre a mesma folha com a fonte travada (a que está tocando) e a posição atual. Os convidados entram no ponto em que você está.

### 5.2 Ser convidado

1. **Aviso na ilha** (`ilha_avisar_ex`, tipo ACENTO, 12 s, chave `sala:<id>`): rosto do anfitrião + "**Rafa** te chamou para assistir **Duna: Parte 2**" + dica "OK para ver".
   - Se a pessoa está **no player** assistindo outra coisa, o aviso vai para a plrilha (pílula de 64 com o ícone `users`) e nunca pausa nada.
   - Se o app estava fechado, aparece na próxima abertura, se a sala ainda estiver ativa (o convite expira junto com a sala).
2. OK abre o **modal da ilha** (como a enquete e o pedido de amizade): capa, título, quem já está ("Rafa, Fabi"), a fonte do anfitrião e o **veredito da fonte** calculado na hora:
   - ✓ "Você tem a mesma fonte" (achou o mesmo infoHash/arquivo nos seus addons).
   - ≈ "Fonte parecida: 1080p, duração igual" (mesmo título, outra release com duração dentro de ±2 s).
   - ! "Sua fonte tem duração diferente (–1 min 40 s). Pode desalinhar em cenas." Mostra assim mesmo, porque a pessoa pode aceitar.
   - ✕ "Nenhuma fonte encontrada nos seus addons" + "Ver fontes" (abre a folha de fontes) ou "Entrar só pelo celular" (v2, convidado só-celular).
   - Botões: **Entrar** · Agora não · (segurar) Não receber convites de Rafa.
3. "Agora não" deixa um **ponto de acento** no relógio enquanto a sala vive (mesmo mecanismo de `ilha_ponto_enquete`). A tecla azul reabre.

**Entrar com código.** Na página Amigos (painel da tecla azul, aba Amigos), um item "Entrar numa sala" leva ao teclado em modo código (6 caracteres, o alfabeto do `codigo.js`). Por código, o anfitrião recebe na ilha "**Fulano** quer entrar — Aceitar / Recusar" (mesmo modal do pedido de amizade). Amigos não precisam de aceite.

### 5.3 Sala (antes de começar)

O **cartão da sala** é um cartão da ilha (`ilha_cartao`) expandido, sobre Frost da arte do título:

```
╭──────────────────────────────────────────────────────────────╮
│  [capa]  Duna: Parte 2                         Código K7M-2QX │
│          Sessão com Rafa e Fabi                               │
│                                                              │
│   (R) Rafa      ✓ pronto      1080p WEB-DL                    │
│   (F) Fabi      ◌ carregando  42%                             │
│   (H) Você      ✓ pronto      1080p WEB-DL                    │
│                                                              │
│   [ Começar ]   Convidar mais   Fonte   Sair                  │
╰──────────────────────────────────────────────────────────────╯
```

- Cada linha mostra o estado de verdade, reportado pela TV da pessoa: `entrando` → `procurando fonte` → `carregando NN%` (buffer) → `pronto` | `sem fonte` | `erro`.
- **"Pronto"** quer dizer: a fonte abriu, o vídeo carregou na posição inicial e está **pausado** no primeiro quadro (pré-buffer, via `video_tocar_posicao` + `video_pausar`).
- **Começar**: o anfitrião aperta (ou qualquer um, se "todos controlam"). Quem ainda não está pronto: o cartão pergunta "Fabi ainda está carregando. Começar mesmo assim?" e Fabi entra depois, no modo "alcançando". Se todos já estão prontos há 10 s, o botão pulsa uma vez. Não há início automático, porque início surpresa irrita.
- **Contagem**: os três números aparecem na pílula (3 · 2 · 1, 1,5 s no total), e o servidor marca o play para um instante comum (6.4).

### 5.4 No player

- **Pílula da plrilha**: hora · "termina às" (como hoje) **+ rostos empilhados** (até 4, o resto vira "+2"). Ela aparece com o OSD de pé ou em eventos.
- **Eventos da sala** (aviso da plrilha, 2,5 s, o rosto como ícone):
  - "Rafa pausou" / "Rafa continuou" / "Rafa voltou 30 s" / "Fabi pulou a abertura".
  - "Fabi está carregando…" (com spinner). Passados 3 s, todos pausam juntos: "Esperando Fabi · 4 s". Aos 20 s: "Fabi continua carregando. Seguindo sem ela" e Fabi passa a "alcançando".
  - "Fabi saiu" / "Fabi voltou".
- **Ajuste de sync**: invisível quando é velocidade. Quando é *seek* (plataforma sem velocidade), aparece um aviso pequeno "Sincronizando…" só se o pulo passar de 2 s.
- **Reações**: **segurar OK** (meio segundo) com o OSD escondido abre uma **régua de 5 reações** saindo da ilha (ícones próprios, desenhados como os da Lucide, não emoji do sistema, porque as fontes das TVs não têm emoji colorido): risada, susto, coração, palmas, "hã?". ESQ/DIR escolhe, OK manda, VOLTAR fecha. A reação recebida sobe da ilha como uma bolha com o rosto + ícone por 2,5 s. Muitas ao mesmo tempo viram "Rafa e +3 ❤". Há limite de 1 reação a cada 2 s por pessoa.
  - **Só segurar OK** (decisão 3). Nenhuma tecla colorida. Segurar OK com o OSD de pé continua sendo o que é hoje no player: a régua só abre com o OSD escondido. Conferir no mapa de teclas do `player.c` se o toque curto de OK (mostrar OSD) é entregue só quando a tecla sobe, como o CH+ faz com o `chsegura.h`.
- **Frases rápidas** (opcional, segunda aba da régua): "Pausa rapidinho", "Volta um pouco", "Legenda?", "Já volto". Enviam texto pré-traduzido (vai a chave da frase, não o texto, então cada TV mostra na própria língua).
- **Legenda/áudio**: a folha de faixas (CIMA) funciona como sempre, só para você. A linha **Velocidade** some durante a sala (a sala é 1x; a velocidade interna é do sync).
- **VOLTAR** no player: pergunta "Sair da sessão?" com **Sair** · Ficar · **Minimizar** (o mini player e a retenção já existem: `player_minimizar`, `player_retido`). Ao sair, o cartão "Voltar para a sessão" fica na ilha da Home enquanto a sala vive.

### 5.5 Controles

- **Todos controlam** (padrão, decisão 2): play, pausa, *seek* (avanço, retrocesso, pular abertura) e próximo episódio de qualquer um valem para todos.
- **Só o anfitrião** (trava opcional): nos convidados, play/pausa/*seek* mostram "Só Rafa controla nesta sessão", com a opção **"Pedir pausa"**, que manda um aviso na ilha do anfitrião. A pausa local continua possível como **"pausa só para mim"** (segurar PAUSA)? Recomendo **não** fazer no MVP: quem pausa sozinho vira "alcançando" e alinha depois.
- **Rajada de seek**: o D-pad repete (+10 s, +10 s, +10 s...). O cliente só manda o *seek* final 700 ms depois da última tecla (a barra anda localmente). Isso evita 6 *seeks* na sala.

### 5.6 Episódios

- No **fim do episódio**, o cartão de próximo episódio (`player_regra_proximo`) é **da sala**: a contagem é a do anfitrião, e "Assistir agora" de qualquer um (no modo "todos") avança todo mundo.
- Cada TV resolve o próximo episódio pela **mesma fonte** (`bingeGroup` / assinatura do `fontepref`) e há uma **mini-barreira** de pronto (máximo de 15 s) antes do play comum.
- Se alguém não acha o episódio, "Fabi não achou o episódio 4 na mesma fonte. Usando a mais parecida."

### 5.7 Fim

- No fim do título, ou quando o anfitrião escolhe "Encerrar sessão" (os convidados só podem sair), o cartão "Sessão encerrada · 2 h 14 min com Rafa e Fabi" aparece na ilha. O "O que achou?" (`reacao.c`) aparece como sempre, para cada um.
- No feed (decisão 8; vale o alcance de cada um): "Henrique, Rafa e Fabi assistiram **Duna 2** juntos". O evento novo é `junto` em `social.js`.
- Se o anfitrião sai, a sala **passa o controle** para o próximo que entrou e avisa ("Agora Fabi controla"). Ela não morre.

### 5.8 Celular (no MVP, decisão 4)

O QR do cartão da sala (e da folha "Assistir juntos" no player) abre `https://<domínio do nuvio-juntos>/s/<código>#<bilhete-celular>` no celular. O bilhete vai no fragmento (`#`), que não chega a logs de servidor nem a cabeçalhos *Referer*.
- Mostra quem está, o título e a posição (ao vivo), as reações e o **chat de texto** (200 caracteres). A mensagem aparece na TV como aviso da plrilha com o rosto, e há um ajuste "Mostrar mensagens na TV: Sim/Só reações/Não".
- **Controle remoto da sala** (play/pausa/±10 s/−30 s, pular abertura), respeitando o modo de controle. Com a trava do anfitrião, o celular de um convidado só tem "Pedir pausa".
- **Página única**: HTML + JS inline, sem framework e sem build, servida pelo próprio `nuvio-juntos`. Ela abre o WebSocket da sala com o bilhete do celular e reusa o mesmo protocolo (7.3) com `papel:"celular"`. Funciona em Safari e Chrome móveis. Não pede login, porque o bilhete já carrega a identidade.
- **Na TV**: as mensagens aparecem como aviso da plrilha (rosto + 1 linha, 4 s, fila de no máximo 3, "+N mensagens" depois). O ajuste "Mensagens na TV: Mostrar / Só reações / Esconder" fica no cartão da sala e em Ajustes > Social.
- Quem escaneia **o QR da própria TV** vira "o celular de Henrique": o bilhete do celular é emitido para a cadeira daquela TV, vale 10 min para o primeiro uso e depois dura a sala. O mesmo QR escaneado por outra pessoa entra como o mesmo Henrique, e por isso o QR **só aparece na TV dele** e é renovado ao ser usado (botão "Novo QR" no cartão). Convidado sem TV (só celular) fica para a v2.

---

## 6. Motor de sincronização

### 6.1 Modelo

O servidor (Durable Object da sala) é a **fonte da verdade** de um único estado:

```
SalaRelogio {
  ep:      "tt1160419" | "tt0944947:2:3"   // o que se assiste (imdb[:temp:ep])
  estado:  "lobby" | "tocando" | "pausado" | "esperando" | "fim"
  pos0:    segundos de mídia no instante t0
  t0:      ms do relógio do SERVIDOR
  taxa:    1.0 (fixo no MVP)
  seq:     inteiro monotônico (cada mudança incrementa)
  autor:   id de quem causou (para o aviso "Rafa pausou")
}
posição_alvo(agoraServidor) = estado == tocando ? pos0 + (agoraServidor − t0)/1000 : pos0
```

Esse é o modelo do SyncPlay e do `AVPlaybackCoordinator`: ninguém transmite posição a cada segundo, todos derivam do mesmo par (pos0, t0) [P2][P4].

### 6.2 Relógio (offset estilo NTP)

- Ao conectar: **8 pings** (`{t:"ping", c: cliente_ms}` → `{t:"pong", c, s: servidor_ms}`) espaçados por 150 ms. Para cada um: `rtt = agora − c`, `offset = s − (c + rtt/2)`. Fica o offset da amostra de **menor RTT** entre as 8 últimas. É o mesmo desenho do `TimeSync.js` do Jellyfin, que tem rajada inicial e depois uma medição a cada 60 s [P2][P4]. O erro fica ≤ rtt/2 da melhor amostra, ~20–60 ms numa rede doméstica (estimativa).
- Re-sincroniza com 3 pings a cada 60 s, e sempre depois de reconectar. O offset é suavizado com EMA (α = 0,3), e saltos acima de 250 ms entram direto (TV que acordou do repouso).
- Relógio local = `SDL_GetTicks64()` (monotônico), nunca a hora de parede: a TV pode estar com o fuso ou a hora errados (isso também resolve "fuso horário" dos casos de borda).

### 6.3 Posição local precisa

O app lê posição em cadências diferentes por plataforma:

| Plataforma | Fonte da posição (código) | Cadência | O que fazer |
|---|---|---|---|
| LG webOS (`video.c`) | evento `currentTime` do uMS | ~200 ms (comentário em `video.c:848`) | Interpolar: `pos = posEvento + (agora − tEvento)·taxaEfetiva` quando tocando |
| Samsung .wgt (`video_tizen.c`) | `oncurrentplaytime` + `getCurrentTime` | variável | Interpolar igual; `S.tPos` já guarda quando leu |
| Samsung .tpk (`video_tpk.c`) | `hPos()` do host .NET | sob demanda | Leitura direta, com a latência da chamada desprezível |
| Android (`video_android.c`) | `posMs` (evento/consulta ao ExoPlayer) | ~por quadro | Direto |

**Novo em `video.h`:** `double video_pos_amostra(Uint64 *ticksDaAmostra)`, que devolve a última posição e quando ela foi lida. A interpolação fica num lugar só (`juntossync.c`) e não em cada backend.

### 6.4 Início e retomada comuns ("play agendado")

Para começar ou continuar, o servidor manda `{estado:"tocando", pos0, t0 = agoraServidor + max(2 × maiorRTT, 600 ms)}`, ou seja, **no futuro**. É a regra do Jellyfin (`max(2×ping, 500 ms)`) com 100 ms a mais pela lentidão do play nas TVs [P2]. Cada TV:
1. Já está pausada em `pos0` (pré-buffer).
2. Agenda `video_pausar(0)` para `t0 − latênciaPlayMedida`. A latência do play é medida por plataforma na primeira vez (tempo entre o pedido e o primeiro avanço de posição) e lembrada em sessão.
3. A correção fina (6.5) cuida do resto.

Se a mensagem chegar atrasada (t0 já passou), a TV entra em `tocando` e alinha por *seek*. Quem entra no meio recebe o mesmo par (pos0, t0) e extrapola. É exatamente o ponto em que o Jellyfin errou (#8579), então há um teste unitário dedicado.

### 6.5 Correção de desvio

`desvio = posLocal − posAlvo` (positivo = adiantado). A cada 500 ms, só com `estado == tocando`, vídeo realmente tocando (`video_tocando`), sem *seek* em curso e sem buffering:

| \|desvio\| | Plataforma com velocidade (nível A) | Plataforma sem velocidade (nível B) |
|---|---|---|
| < 80 ms | nada (zona morta) | nada |
| 80 ms – 1,5 s | **velocidade**: 0,95x (adiantado) ou 1,05x (atrasado) por `desvio / 0,05` segundos, então volta a 1x. O Syncplay usa 0,95x. O Jellyfin usa `1 + desvio/1 s` por 1 s, mais agressivo. ±5% com time-stretch é pouco perceptível (*inferência*; validar ouvindo na TV) | < 600 ms: nada (tolerância do nível B). 600 ms – 1,5 s: *seek* para `alvo + latênciaSeek` |
| 1,5 s – 5 s | *seek* para `alvo + latênciaSeek` | *seek* para `alvo + latênciaSeek` |
| > 5 s | *seek* + aviso "Sincronizando…" | idem |

Os valores são o ponto de partida, entre as referências [P2]: Jellyfin SpeedToSync de 60 ms a 3 s e SkipToSync ≥ 400 ms; Syncplay slowdown ≥ 1,5 s, rewind ≥ 4 s; Peario *seek* ≥ 1 s; WatchParty *seek* ≥ 300 ms + latência. Os definitivos saem do simulador e da TV (seção 8.5). Diferente do Jellyfin, onde a correção vem **desligada** por padrão, aqui ela vem **ligada**: o produto é o sync.

Truque do Jellyfin que vale copiar: em *seek* comandado, somar ±50 ms aleatórios, para quem já está "no lugar" também buscar e todos caírem no mesmo keyframe [P2].

**Proteções:**
- **Cooldown** de 3 s depois de qualquer *seek* de sync, e no máximo 3 *seeks* de sync por minuto. Se ultrapassar, a TV entra em "sync frouxo" (tolerância de 2 s) e registra no log `[juntos] sync frouxo`. Isso evita o *loop* de *seek* em fonte que pula para keyframe.
- **latênciaSeek** é medida: depois de cada *seek*, compara onde caiu com onde pediu, e quanto tempo levou até voltar a tocar. Média móvel por sessão. Num MKV/HLS que cai no keyframe, o *seek* pode cair a segundos do alvo. Por isso o *seek* é seguido de nova medição e, no nível B, de um segundo *seek* curto se o resíduo passar de 1 s, uma vez só.
- **Velocidade provada**: reaproveita o `VelMedidor` (`velocidade.h`). Se ele disser `VELMED_NAO_ANDOU` (ex.: áudio em passthrough no Android, `video_velocidade_bloqueada`), a TV cai para o nível B pelo resto da sala e o log diz isso.

**Níveis por plataforma, hoje (lido do código 2.0.1):**

| Plataforma | Velocidade hoje | Nível previsto | Observação |
|---|---|---|---|
| Android (Media3) | `setPlaybackSpeed`, sim | A, exceto com passthrough | Com passthrough (`CB_GANHO_PASSTHROUGH`), a velocidade não anda (medido na TCL, comentário em `velocidade.h`): nível B. Para o sync, usar `SeekParameters.EXACT` [P4] |
| LG webOS | `setPlayRate` do uMS (doc: 0,1–2,0) quando a TV aceita | A em MP4/MKV progressivo se aceitar; **B em HLS** (a LG documenta HLS só em 1,0x) | A recusa some com a linha pela execução [P4] |
| Samsung .tpk | `Player.SetPlaybackRate`, mas a doc diz que lança exceção em *streaming* | Provável B | A primeira tentativa é a prova (`EV_VELOCIDADE`) |
| Samsung .wgt (AVPlay) | **não**: `setSpeed` só aceita −16…−2, 1, 2…16 | **B** | Só *seek*. O `seekTo` é assíncrono e bloqueia outras chamadas até o callback [P4] |
| Mac (fixture) | simulado | A/B configurável | Para o simulador |

O nível B fica bom com 600 ms de tolerância: em conversa, ninguém percebe meio segundo. O que irrita é quem está 3 s na frente rindo antes. Isso deve ser dito na tela de ajuda: "Samsung (navegador): sincronização dentro de 1 s".

### 6.6 Buffering e espera

- A TV reporta `{t:"estado", st:"carregando"}` quando:
  - `video_bufferando_ms() > 1000` (Android, tpk, LG); ou
  - **parada de posição**: tocando, mas `pos` não andou em 1,5 s (o .wgt não tem sinal de buffering, `video_tizen.c:1452`).
- O servidor só move a sala para **`esperando`** se o membro continuar carregando **3 s** (uma travada curta não para todo mundo). Em `esperando`, ele grava `pos0 = posAlvo atual` e manda pausar (todos param no mesmo quadro).
- Quando o membro volta a `pronto`, o servidor faz o play agendado (6.4).
- **Prazo**: 20 s em `esperando` (configurável pelo anfitrião: 10/20/60 s). Ao expirar, o membro passa a `alcançando` e a sala volta a tocar. Quem está alcançando segue o relógio da sala sozinho, com *seek*, e não pode mais travar a sala até ficar 30 s estável. Teto de 2 esperas por membro a cada 10 min: na terceira, a pessoa vira "alcançando" direto.
- Isso evita o problema conhecido do SyncPlay: a sala presa em Waiting por causa de um membro (#5557) [P2]. O SharePlay mostra o mesmo estado como "esperando os outros" (`waitingForCoordinatedPlayback`).

### 6.7 Máquina de estados

**Da sala (servidor):**

```
            criar
              │
              ▼
        ┌──────────┐  começar (todos prontos ou forçado)
        │  lobby   │────────────────────────────┐
        └──────────┘                            ▼
              ▲                           ┌──────────┐
   trocar ep. │                           │ tocando  │◄────────────┐
   (barreira) │          pausar           └──────────┘             │
              │      ┌────────────────────────┤  │ membro carregando > 3 s
              │      ▼                         │  ▼
        ┌──────────┐  continuar (play agend.) ┌──────────┐ pronto / prazo 20 s
        │ pausado  │─────────────────────────►│esperando │──────────┘
        └──────────┘                          └──────────┘
              │ fim do título / encerrar / vazia 10 min
              ▼
           ┌──────┐
           │ fim  │ → apaga estado (alarm) 
           └──────┘
```

**Do membro (cliente, `juntos.c`):**
`conectando → sincronizando_relogio → procurando_fonte → carregando → pronto → (tocando | pausado | esperando_outros | alcançando) → saiu`, com `reconectando` saindo de qualquer estado. Em `reconectando`, o vídeo **continua tocando** localmente pelo último relógio conhecido (o relógio da sala é determinístico) por até 60 s. Ao voltar, reconcilia pelo `seq`.

### 6.8 Concorrência de comandos

- Cada comando de controle leva `seqBase` (o `seq` que o cliente via). O servidor aplica em ordem de chegada, e o último vence. Um comando com `seqBase` muito antigo (mais de 2 mudanças atrás, em menos de 1 s) é **descartado**, para não ressuscitar uma pausa velha.
- Pausa + play de duas pessoas no mesmo segundo: vence a ordem de chegada, e todos veem o aviso de quem fez o último.
- Otimista local: quem aperta pausa pausa na hora (sem esperar o servidor). Se o servidor discordar (pedido descartado), o cliente reconcilia.

### 6.9 Fontes diferentes

**Impressão digital do release** (`juntosfonte.c`), em ordem de força:
1. `infoHash` + `fileIdx` (torrents via Torrentio/Comet/MediaFusion etc.; o debrid de cada um resolve com `debrid_resolver`).
2. `tamanhoBytes` exato (behaviorHints.videoSize) + `arquivo` (nome do arquivo normalizado).
3. `bingeGroup` + altura + nome do arquivo parecido (mesmo addon de outra conta).
4. Só o título + altura (fallback, "fonte parecida").

O servidor recebe `{hash?, idx?, bytes?, arquivo?, binge?, altura, durMs}` (o nome do arquivo é truncado para 120 caracteres). **Nunca** recebe a URL, a chave do addon ou o link do debrid. Esses links são da conta da pessoa e muitas vezes presos ao IP. Compartilhar quebraria e provavelmente violaria os termos dos serviços (*inferência*; não conferimos os termos de cada debrid). Todo o ecossistema (Peario, WatchParty, Harbor) também não compartilha [P1].

**Convidado:**
1. Pede streams aos próprios addons (o caminho normal do `streams.c`) e casa com a impressão (1 → 4).
2. Se casar em 1 ou 2, a duração deve bater. Se não casar, oferece a **mais parecida** pela pontuação do `stream_automatico` com a altura do anfitrião como preferência.
3. **Comparação de duração** depois do load: `|durConvidado − durSala| > 2 s` → aviso "Versão diferente". Casos:
   - **Razão ~1,0427 (25 / 23,976)**: lançamento PAL acelerado. **v2**: a sala aplica `taxa` por membro (o relógio de mídia vira `pos·razão`). Exige velocidade de verdade (nível A). No nível B, só aviso.
   - Diferença fixa (logo de estúdio, cena extra): **v2**: "Ajustar sync" manual (±0,5 s por toque, guardado por release) e, depois, detecção automática por legenda: comparar o primeiro diálogo da legenda do mesmo idioma das duas versões e achar o deslocamento. Reaproveita a infraestrutura de `legsync.c` e `autosync.c`, que já alinham legenda contra áudio.
4. Se a fonte falhar no meio (a fonte "morreu"), o fluxo de reserva que já existe (`fontevolta.c`, "Fonte 2 de 3") roda **com a impressão da sala como preferência**. Depois de abrir, é *seek* para a posição da sala.

**P2P (servidor Stremio local, `p2p.c`):** mesma impressão (infoHash). Cada um baixa o seu. Torrent demora para encher, então o prazo de pronto é maior (60 s) e o app avisa "fonte P2P, pode demorar".

**Jellyfin/Plex/Emby:** fonte de servidor pessoal só funciona se todos tiverem acesso ao mesmo servidor. MVP: título de Jellyfin só entra se o convidado tiver uma fonte equivalente em addon. v3: falar SyncPlay com o próprio Jellyfin.

**TV ao vivo:** não há posição para sincronizar (cada TV tem atraso próprio de HLS). v2: "assistir juntos ao vivo" = mesmo canal + reações + chat, sem promessa de sync (aviso "ao vivo, cada um com seu atraso"). Sincronizar canal ao vivo por PDT do HLS (`#EXT-X-PROGRAM-DATE-TIME`) é possível quando o servidor IPTV manda, e fica para v3.

---

## 7. Servidor

### 7.1 Onde

**Worker novo e separado: `nuvio-juntos`** (`servidor/juntos/`), com Cloudflare Durable Objects (classe `Sala`, backend SQLite) e a **WebSocket Hibernation API**: o DO pode ficar com os sockets abertos sem cobrar duração enquanto ninguém fala [P4]. *Este é o padrão; o dono pode mudar* (decisão 5).

**Por que separado.** Em 06/10, um deploy da API feito da árvore errada derrubou o site da VIDAA. Num Worker próprio, um deploy do `nuvio-recomendacoes` não derruba salas abertas, e um deploy do `nuvio-juntos` não toca no social. O custo é ter dois lugares e um segredo compartilhado.

**Divisão de responsabilidades:**

| `nuvio-recomendacoes` (já existe) | `nuvio-juntos` (novo) |
|---|---|
| Identidade (`quemE`, perfil = pessoa), amigos, bloqueios, convites (D1), limites por pessoa, evento `junto` no feed | Durable Object `Sala`, WebSocket, relógio, estados, reações, chat, página do celular `/s/…` |
| **Emite o bilhete** assinado (HMAC) | **Só valida** o bilhete, e não tem D1 nem acesso ao Supabase |

O `nuvio-juntos` não precisa de banco: a sala vive no storage do DO e some no fim. O D1 dos convites continua no `nuvio-recomendacoes`, que já tem as tabelas de amigos e bloqueios.

Por que não D1 + polling: o D1 é região única, com latência de escrita de dezenas a centenas de ms, e polling de 1 s em 5 TVs por 2 h dá 43 mil leituras por sala, além do atraso médio de 500 ms em cada comando. O DO dá um ponto único de ordenação (o `seq`), relógio único e *push* imediato.

`servidor/juntos/wrangler.toml`:

```toml
name = "nuvio-juntos"
main = "src/index.js"
compatibility_date = "2026-09-01"

[[durable_objects.bindings]]
name = "SALA"
class_name = "Sala"

[[migrations]]
tag = "v1-sala"
new_sqlite_classes = ["Sala"]

# Segredo (wrangler secret put): SALA_SEGREDO, o mesmo valor no nuvio-recomendacoes.
```

Publicação só por um script próprio (`tools/juntos-publicar.sh`) que confere se está na pasta `servidor/juntos/`, a mesma lição do `vidaa-publicar.sh`.

A conta está no **Workers Paid** (decisão 5): 1 milhão de requests de DO por mês inclusos (depois US$ 0,15 por milhão), 400 mil GB-s por mês e 50 milhões de linhas escritas inclusas [P4]. Desde 07/2026, namespaces novos só podem ser SQLite. Cada `setAlarm()` conta como 1 linha escrita.

Referência no próprio ecossistema: o NuvioTV #1894 propõe exatamente Worker + DO como sala, para o controle pelo celular, e o Harbor usa um Worker como relay [P1]. A diferença é que o nosso é central, e ninguém configura nada.

### 7.2 Rotas HTTP

No **`nuvio-recomendacoes`** (autenticadas por `quemE`, como as outras):

| Rota | O quê |
|---|---|
| `POST /v1/sala` | cria a sala: `{ep, titulo, poster, impressao, controle:"todos"\|"anfitriao", convidados:[ids]}` → `{id, codigo, bilhete}`. Gera o código de 6 caracteres (alfabeto de `codigo.js`), grava `convite` no D1 para cada amigo (só contatos de verdade e não bloqueados: `bloqueado()` de `amigos.js`) |
| `GET /v1/sala/convites` | os convites pendentes para mim (D1, sala viva). A TV consulta **a cada 20 s com o app em primeiro plano e fora do player**, e a cada 60 s dentro do player. O D1 aguenta: `SELECT` por índice |
| `POST /v1/sala/entrar` | `{id}` ou `{codigo}` → `{id, bilhete, resumo}`. Por código e sem amizade: o anfitrião precisa aceitar (o DO segura o pedido) |
| `POST /v1/sala/recusar` | some o convite |
| `POST /v1/sala/celular` | `{id}` → bilhete de celular (`papel:"celular"`, primeiro uso em até 10 min) para o QR |

No **`nuvio-juntos`** (autenticadas só pelo bilhete):

| Rota | O quê |
|---|---|
| `GET /ws?b=<bilhete>` | *upgrade* WebSocket → DO `SALA.idFromName(sala)` |
| `GET /s/<código>` | a página do celular (o bilhete vem no `#` e é lido pelo JS) |
| `GET /saude` | saúde |

O `id` da sala é gerado no `nuvio-recomendacoes` (128 bits aleatórios). O DO nasce na primeira conexão com bilhete de `papel:"anfitriao"` e recebe a configuração inicial (título, impressão, controle) **dentro do bilhete do anfitrião**. Assim os dois Workers não precisam conversar por rede. O fim da sala chega ao `nuvio-recomendacoes` pela expiração (`sala.expira`) e por um `POST /v1/sala/fim` que a TV do anfitrião manda ao sair (o melhor esforço basta, porque a expiração cobre).

**Bilhete**: o `discordws.c` não manda cabeçalhos customizados, e o WebSocket do navegador (.wgt, celular) também não. Então a autenticação é por **bilhete**: HMAC-SHA256, com o segredo `SALA_SEGREDO` compartilhado pelos dois Workers, de `{v, sala, pessoa, nome, avatar, papel: anfitriao|convidado|celular, cfg?, exp, nonce}`. Para TV, `exp` é +60 s e o uso é único (o DO guarda o `nonce` até expirar). Para o celular, ver 5.8. O Worker valida e repassa ao DO, e o DO confere a assinatura de novo. Trocar o segredo derruba só os bilhetes, não as salas abertas.

### 7.3 Mensagens (JSON, uma por frame)

Cliente → servidor:

```jsonc
{"t":"ping","c":123456}
{"t":"ola","v":1,"plat":"lg|wgt|tpk|android|mac","nivel":"A|B","app":"2.1.0"}
{"t":"estado","st":"procurando|carregando|pronto|tocando|semfonte|erro","pct":42,"pos":812.4,"dur":9930.1,"fonte":{"tipo":"mesma|parecida|diferente"}}
{"t":"cmd","op":"play|pausa|seek|proximo|comecar|encerrar","pos":812.4,"seqBase":17}
{"t":"reacao","k":"risada"}            // chave, nunca texto livre
{"t":"frase","k":"volta_um_pouco"}
{"t":"chat","txt":"..."}               // só do celular, ≤200 chars
{"t":"aceitar","quem":"nuvio:..."}     // anfitrião, entrada por código
```

Servidor → cliente:

```jsonc
{"t":"pong","c":123456,"s":1759750000123}
{"t":"sala","seq":18,"rel":{"ep":"tt1160419","estado":"tocando","pos0":812.4,"t0":1759750001323,"autor":"nuvio:...","op":"play"},
 "membros":[{"id":"...","nome":"Rafa","av":"https://...","st":"pronto","nivel":"B","anfitriao":true}],
 "controle":"todos","impressao":{...}}
{"t":"membro","id":"...","st":"carregando","pct":30}
{"t":"reacao","de":"...","k":"risada"}
{"t":"aviso","k":"esperando","quem":"...","prazo":20}
{"t":"pedido","quem":{...}}           // entrada por código esperando aceite
{"t":"fim","motivo":"anfitriao|vazia|tempo"}
```

O texto que aparece na TV sai de **chaves traduzidas** (`idioma_*.h`), nunca do servidor. A única exceção é o chat do celular.

### 7.4 Durable Object `Sala`

- **Estado em memória + `ctx.storage` (SQLite)**: relógio, membros, impressão, controle. `ctx.storage` só grava em mudança de estado (não a cada ping).
- **Hibernação**: `ctx.acceptWebSocket(ws, [pessoaId])`, `webSocketMessage`, `webSocketClose`. O `ping` de aplicação vai de 60 em 60 s. Fora isso, o DO dorme entre eventos.
- **Alarmes**: prazo da espera (20 s), sala vazia (10 min → fim), teto absoluto (12 h → fim), e limpeza (apaga o storage e o convite no D1).
- **Limites (por sala)**: **5 TVs** (`SALA_MAX = 5`, var do Worker; decisão 6), mais 1 celular por TV. A 6ª TV recebe "Sessão cheia"; reação de 1 a cada 2 s por pessoa; chat de 1 por segundo e 200 caracteres; `cmd` 5/s por pessoa; mensagem de mais de 2 KB fecha o socket (código 1009).
- **Limites (`nuvio-recomendacoes`/D1)**: criar sala, 10 por hora por pessoa; convites, 30 por hora por pessoa; entrar por código, 10 tentativas erradas por hora por pessoa (a `limitar()` de `amigos.js`, com a tabela `limite`). Código de 6 caracteres de alfabeto 32 = ~10⁹, então adivinhar com 10/h é inviável.

### 7.5 D1 (migração `migracao-010-juntos.sql`)

```sql
CREATE TABLE sala (
  id TEXT PRIMARY KEY, codigo TEXT UNIQUE, anfitriao TEXT NOT NULL,
  ep TEXT NOT NULL, titulo TEXT, poster TEXT, criado INTEGER, expira INTEGER
);
CREATE TABLE convite (
  sala TEXT NOT NULL, para TEXT NOT NULL, de TEXT NOT NULL, criado INTEGER,
  estado INTEGER DEFAULT 0,   -- 0 pendente, 1 entrou, 2 recusou
  PRIMARY KEY (sala, para)
);
CREATE INDEX convite_para ON convite(para, estado);
```

O `scheduled()` que já existe ganha `DELETE ... WHERE expira < ?`. O D1 guarda **só o necessário para o convite chegar**, e nada do que se fez na sala. O "assistiram juntos" no feed vai pelo `social.js` normal (`ev:"junto"`) e respeita o `alcance`.

### 7.6 Privacidade e segurança

- O servidor não vê URLs, chaves nem links de debrid. A impressão do release (infoHash, nome do arquivo) só existe no DO enquanto a sala vive e é apagada no fim.
- O poster passa pelo `posterOk()` e o avatar pelo `avatarPerfilOk()`. Isso evita IP-tracking entre convidados (o mesmo motivo que já existe no `social.js`).
- Convite só de contato confirmado. Os bloqueios valem nos dois sentidos. "Não receber convites de X" e "Não receber convites" (geral) ficam em Ajustes > Social.
- O chat não vai para log nem para D1 (o `nuvio-juntos` nem tem D1). No DO, só os últimos 50 em memória, para quem entra depois.
- Perfil infantil: ver as restrições da decisão 7 (seção 0).

### 7.7 Custo (estimativa; confirmar com a página de preços [P4])

Sala típica: 4 TVs, 2 h.
- Mensagens recebidas: ping de 60 s (8 por TV) + estado/eventos (~60 por TV) + reações (~40 no total) ≈ 4×70 + 40 ≈ **320 mensagens**. Os WebSockets de entrada são faturados a 20:1 como *requests* [P4], então ≈ **16 requests** + 4 *upgrades* + rotas HTTP (~10) ≈ **30 requests por sala**.
- Duração: com hibernação, o DO só fica acordado nos eventos, alguns segundos de CPU por sala.
- Com o celular, somam-se ~2 conexões e ~100 mensagens por sala, ou seja, mais ~7 requests. Ordem de grandeza: **10 mil salas/mês ≈ 370 mil requests de DO**, dentro do 1 milhão por mês incluso no plano pago [P4]. Na prática, **custo zero adicional** até dezenas de milhares de salas por mês. A conta é nossa, com os preços oficiais. O objeto elegível à hibernação não acumula duração, e o ping/pong de *protocolo* é respondido sem acordá-lo [P4]. Os convites custam D1: 1 leitura a cada 20 s por TV ativa. Com 2 mil TVs ativas 4 h/dia, isso dá ~1,4 milhão de leituras/dia, dentro do incluso do D1 no plano pago (conferir o valor na página de preços do D1). Ainda assim, **é o item que mais cresce**: medir e, se precisar, trocar por um DO "caixa de entrada" por pessoa com WebSocket hibernado (v2).

---

## 8. Arquitetura do cliente

### 8.1 Módulos novos em `src/`

| Arquivo | Responsabilidade | Testável no Mac |
|---|---|---|
| `juntos.h/.c` | Estado da sala no cliente, máquina de estados do membro, ciclo de vida (criar, entrar, sair, reconectar), API para UI e player | sim, com transporte falso |
| `juntossync.h/.c` | **Puro** (sem SDL/vídeo): offset de relógio, interpolação de posição, decisão de correção (nada/velocidade/seek), cooldowns, latência medida. Entra `(agora, posAmostra, tAmostra, relogioSala, nivel)`, sai uma ação | sim (`tests/juntossync.c`) |
| `juntosproto.h/.c` | Montar e ler as mensagens JSON (reaproveita o parser de JSON do app) | sim |
| `juntosrede.h/.c` | Transporte: `dws_abrir/enviar/receber` (`discordws.c`) com reconexão exponencial (1, 2, 4… 30 s); rotas HTTP pelo `rede_pedir`. No .wgt, o `discordws` já usa o WebSocket do navegador | parcial |
| `juntosfonte.h/.c` | Impressão do release a partir de `Stream`, casamento com a lista do convidado, veredito (mesma/parecida/diferente/nenhuma), comparação de duração | sim (`tests/juntosfonte.c`) |
| `juntosui.h/.c` | Folha "Assistir juntos", cartão da sala, modal do convite, avisos da plrilha, rostos na pílula, régua de reações | capturas no Mac (`fontepref_shot` como modelo) |
| `juntosconvite.c` | Consulta de convites (20/60 s) e entrega para `ilhasinais.c` | sim |

Nome alternativo, se o dono preferir: `sala*.c`.

### 8.2 Mudanças em módulos existentes

- **`video.h`** + os quatro backends: `video_pos_amostra()` (6.3); `video_taxa_sync(int milesimos)`, separada de `video_velocidade` (a da pessoa). O backend multiplica as duas, e a da pessoa fica travada em 1x durante a sala. Também é preciso **medir a latência do play/seek** (já há `cronPediu`/`msDesdePedido` no `video.c`, que vira função).
- **`player.c`**: ganchos `juntos_ao_pausar/continuar/buscar/proximo` nos lugares em que a pessoa age. O player passa a **perguntar** ao `juntos` antes de agir quando o modo é "só o anfitrião". A rajada de *seek* (5.5) é coalescida aqui. A contagem do próximo episódio obedece à sala.
- **`plrilha.c`**: pedido novo "rostos na pílula" + avisos da sala.
- **`ilhasinais.c`**: sinal `sala:<id>` (convite) e `sala-pedido:<pub>` (entrada por código).
- **`detail.c`**: botão "Assistir juntos".
- **`central.c`/`centrallista.c`**: atalho.
- **`streams.c`/`fontepref.c`**: modo "escolher para a sala" e preferência pela impressão.
- **`faixas.c`**: esconder a Velocidade com a sala ativa.
- **`social.js`** (servidor) + `atividade.c`: evento `junto`.
- **`discord.c`**: presença "Assistindo junto (3)" (o campo `party.size` do Rich Presence), opcional.
- **`idioma_*.h`**: ~45 chaves novas (passa pela `tools/varredura-i18n.py`).
- **`app.c`**: `juntos_atualizar()` no laço principal, **inclusive com o player aberto e minimizado**.

### 8.3 Fios

O `discordws` já roda a conexão num fio próprio e é *non-blocking* no principal. O `juntos_atualizar()` roda no fio principal a cada quadro: drena as mensagens, avança o estado e, a cada 500 ms, chama `juntossync` e aplica a ação no `video_*`. Nenhuma chamada de rede bloqueia o desenho.

### 8.4 Integração por plataforma (o que conferir)

- **LG**: o plano de vídeo é separado. O `video_pausar` e o `video_buscar` são assíncronos pelo LS2, então a resposta e a posição chegam depois. Medir a latência real do `seek` e do `play` na C9 e em uma webOS mais nova. O `setPlayRate` só vai quando está tocando (`velBombear`): a velocidade do sync tem que respeitar isso.
- **Samsung .wgt**: AVPlay `seekTo` em ms, com o callback de sucesso. Sem velocidade (nível B). Sem sinal de buffering, o que exige a detecção por parada de posição.
- **Samsung .tpk**: `SetPlaybackRate` provavelmente recusado em *streaming*, então nível B, a menos que a primeira tentativa prove o contrário. O `hPos` é direto.
- **Android**: Media3 `setPlaybackSpeed` com time-stretch (Sonic). Com passthrough, nível B (`video_velocidade_bloqueada`).
- **Mac**: o coto de vídeo (`SIM.pos`) vira um **player simulado** com latência de *seek*, keyframe e buffering configuráveis (8.5).

### 8.5 Testes (enxutos, conforme a regra "menos teste, mais rápido")

1. **Unitários puros** (rápidos, no `tools/testa-tudo.sh`):
   - `tests/juntossync.c`: offset com RTT assimétrico e *outliers*; decisão por faixa de desvio e nível; cooldown; *loop* de keyframe vira "sync frouxo"; play agendado no passado.
   - `tests/juntosfonte.c`: casamento por hash, por bytes+nome, por binge, fallback; PAL 25/23,976 detectado; duração diferente.
   - `servidor/recomendacoes/teste-sala.mjs`: no estilo de `teste-enquete.mjs`, contra `wrangler dev` (o Miniflare roda DO local). Cobre a ordem do `seq`, o prazo de espera, a passagem de anfitrião, os limites, o bilhete expirado ou forjado e os bloqueios.
2. **Simulador de pares no Mac** (`tests/juntos_sim.sh`): sobe o `wrangler dev`, um **proxy WebSocket em Node** que injeta atraso (20–300 ms), jitter (±80 ms), perda e corte, e N=4 processos do app em modo sem janela (o binário de teste já existe para outros módulos), cada um com o player simulado com perfis diferentes (nível A; nível B com keyframe de 2 s; Android com buffering aleatório). **Métrica**: p95 do |desvio| entre pares durante 10 min simulados; tempo até alinhar depois de *seek*; número de esperas. Critério: nível A p95 < 150 ms, nível B p95 < 800 ms, nenhuma espera > prazo.
3. **TV, mínimo** (ordem da decisão 10): um par por vez, sem medição longa. **Primeiro Android** (TCL + uma segunda TV/box Android, ou a TCL contra o simulador do Mac); **depois LG** (C9 + TCL); Samsung .tpk + .wgt por último. É uma sessão de 15 min com pausa, *seek*, pular abertura e próximo episódio, e o log `[juntos]` de cada TV com o desvio a cada 10 s. **Não instalar na TV do dono sem pedido.**

---

## 9. Casos de borda

| Caso | Comportamento |
|---|---|
| Plataformas diferentes | Mesmo protocolo. Cada membro declara o nível A/B e o cartão mostra "sync ±1 s" ao lado de quem é B |
| Legenda/áudio diferentes | Locais. A legenda externa (Seekr/OpenSubtitles) de cada um é independente |
| Pular abertura / créditos | É um *seek* da sala, com o aviso "Fabi pulou a abertura". O marcador de créditos (`player_aprender_creditos`) de cada um é local |
| "Termina às" | Calculado pelo relógio da sala, sempre 1x |
| Fonte com anúncio (MP4 de aviso de 120 s, ver `streams.h`) | A duração não bate, e a pessoa recebe "versão diferente". O `juntosfonte` trata como "nenhuma" se a duração for < 50% da sala |
| Alguém troca de fonte no meio | Permitido. Ao abrir, *seek* para a posição da sala, com a nova impressão |
| Perfil trocado na TV | Sai da sala (a pessoa é o perfil) |
| TV entra em repouso / app em segundo plano | O WebSocket cai e o membro vira `ausente`. A sala segue. Ao voltar, o cartão "Voltar para a sessão" |
| Rede cai no anfitrião | A sala vive no servidor e o anfitrião passa ao próximo após 30 s sem socket |
| Dois perfis da mesma TV | Uma TV, uma cadeira (o perfil ativo) |
| Fuso horário | Irrelevante (relógio monotônico + offset do servidor) |
| Parental | Perfil infantil participa (decisão 7): convite só da mesma conta ou de amigos da conta, sem código de estranhos, o título precisa passar na classificação do perfil, e o chat vem desligado no celular dele |
| Episódio que um já viu | Cada um grava o próprio progresso e o Trakt scrobble normalmente (`traktscrobble.c`) |
| Mini player | A sala continua no mini (o sync roda igual) |
| Sala cheia (5 TVs) e uma rede ruim | Fica em "alcançando" depois de 2 esperas. Ninguém congela mais de 20 s |

---

## 10. Fases e esforço

Estimativas em dias de trabalho de uma pessoa com agente. Não incluem espera de validação em TV.

### MVP — "Assistir juntos 1.0" (≈ 30 dias; era 24, e o celular entrou)

| Item | Dias |
|---|---|
| Servidor: Worker `nuvio-juntos` (DO `Sala`, WS, alarmes, `SALA_MAX`, script de publicação) + rotas, bilhete, convites D1, regras do perfil infantil, migração e limites no `nuvio-recomendacoes` + `teste-sala.mjs` | 5 |
| Cliente: `juntosproto` + `juntosrede` (WS, reconexão) + `juntos` (estados) | 4 |
| `juntossync` + `video_pos_amostra`/`video_taxa_sync` nos 4 backends + latências | 4 |
| `juntosfonte` (impressão, casamento, veredito, duração) + reserva no meio | 3 |
| UI: folha, cartão da sala, modal do convite, avisos e rostos na plrilha, régua de reações (segurar OK), Voltar/sair, próximo episódio | 5 |
| **Celular**: página `/s/…` (HTML/JS inline), bilhete de celular + QR na TV, chat, controle remoto, mensagens na TV (fila e ajuste), testes em Safari/Chrome móveis | 5,5 |
| Simulador de pares + proxy de jitter | 2 |
| TV: Android primeiro, depois LG+Android, Samsung por último; ajuste de limiares, i18n | 2 |

**Escopo do MVP:** filmes e séries sob demanda; convite de amigo e código (com aceite); 2–5 TVs (limite configurável no servidor); perfil infantil com restrições; todos controlam por padrão, com a trava do anfitrião; fonte parecida aceita com aviso; pronto e contagem; barreira com prazo e "alcançando"; entrar no meio; sair, voltar e passagem de anfitrião; próximo episódio juntos; 5 reações + 4 frases rápidas ao segurar OK; aviso de versão diferente; evento "assistiram juntos" no feed (padrão ligado, respeitando o alcance); **celular por QR com chat e controle remoto da sala**.

**Fora do MVP:** convidado só-celular, TV ao vivo, alinhamento automático de versões, PAL, Jellyfin, voz.

Soma: 5 + 4 + 4 + 3 + 5 + 5,5 + 2 + 2 = **30,5 dias (~30)**. As regras do perfil infantil somam meio dia ao servidor; na TV, o filtro por classificação reaproveita o `parental.c`. Calendário: ~6 semanas, mais a espera das provas em TV.

### v2 (≈ 14 dias)
Convidado só-celular (assiste em outro aparelho e participa pelo chat); ajuste manual de sync por release; detecção de offset por legenda; PAL (`taxa` por membro); TV ao vivo "junto sem sync"; DO caixa de entrada por pessoa (convite instantâneo e "online agora" nos rostos); chave de sala aberta para amigos ("amigos podem entrar sem convite", aparece na fileira Amigos assistindo como "Entrar").

### v3 (exploratório)
SyncPlay do Jellyfin; canal ao vivo sincronizado por PDT; integração Discord "Pedir para entrar".

---

## 11. Riscos

| Risco | Prob. | Impacto | Mitigação |
|---|---|---|---|
| *Seek* impreciso (keyframe) no LG/Samsung gera *loop* de correção | Alta | Alto | Cooldown, segundo *seek* único, "sync frouxo", medição no simulador com keyframe |
| `setPlayRate` muda o vídeo mas não o áudio (ou vice-versa) em algum modelo | Média | Médio | `VelMedidor` prova; se falhar, nível B |
| Releases diferentes entre membros (o mais comum sem debrid) | Alta | Médio | Impressão e veredito claros antes de entrar; v2 com offset |
| Leituras de convite no D1 escalam | Média | Baixo | Cadência adaptativa; v2 com DO caixa de entrada |
| WebSocket bloqueado por proxy/rede corporativa ou TLS antigo na TV | Baixa | Alto | `discordws` já roda em produção (Discord) nas mesmas TVs; fallback de polling HTTP de 1 s (`GET /v1/sala/estado?seq=`) atrás de uma chave |
| Segurar OK conflitar com o toque curto de OK no player | Média | Baixo | Entregar o toque curto só quando a tecla sobe (padrão `chsegura.h`); a régua só abre com o OSD escondido; ajuste para desligar reações |
| Um membro com fonte P2P lenta trava a entrada | Média | Médio | Prazo maior, "Começar mesmo assim" |
| Expectativa "igual ao Teleparty com chat na tela" | Média | Baixo | Chat no celular desde o MVP; mensagens discretas na ilha |
| QR do celular fotografado ou repassado entra como o dono da TV | Baixa | Médio | Bilhete de celular de primeiro uso em 10 min, renovável ("Novo QR"), visível só na TV dele; o anfitrião pode expulsar o celular |
| Dois Workers fora de sincronia (segredo, versão do bilhete) | Baixa | Médio | O bilhete leva o campo `v`; o `nuvio-juntos` aceita a versão atual e a anterior; o segredo é trocado nos dois de uma vez pelo script |
| O recurso morrer numa reescrita do player (o que aconteceu com a Plex) | Média | Alto | Lógica pura em `juntossync`/`juntosfonte` com testes; o player só expõe `video_pos_amostra`/`video_taxa_sync` |
| Legal: sincronizar fonte pirata de terceiros | — | — | O servidor não transporta mídia nem link, só relógio. É o mesmo nível do Syncplay. Avaliar com o dono |

---

## 12. Perguntas para o dono

Todas respondidas em 06/10 (seção 0, decisões 1–10). Continua **padrão, que o dono pode mudar**: o Worker separado `nuvio-juntos` (decisão 5).

Para decidir durante a implementação, sem bloquear:
- Os 5 ícones de reação e as 4 frases rápidas (proposta na 5.4).
- O prazo da espera por quem trava (20 s proposto, configurável pelo anfitrião).

---

## Desvios (implementação, 06/10)

Mudanças em relação ao texto acima, com o motivo. O resto segue o plano.

1. **A impressão do release também fica na linha `sala` do D1** (não só no DO). O modal do convite mostra o veredito da fonte *antes* de entrar (5.2), e o convidado ainda não tem bilhete nem socket. Ela não tem URL (passa por `limparImpressao`, a mesma função do DO) e some com a sala (`/v1/sala/fim` e `scheduled`).
2. **O endereço do `nuvio-juntos` vem do `nuvio-recomendacoes`** (`JUNTOS_URL` em `[vars]`, devolvido como `ws`/`web` nas respostas de `/v1/sala*`), em vez de um `-D` novo no build. A TV só precisa do `NUVIO_REC_URL` que já tem, e trocar o domínio das salas não exige build nova.
3. **Um arquivo de bilhete para os dois Workers**: `servidor/juntos/src/bilhete.js`, importado pelo `nuvio-recomendacoes` (o esbuild do wrangler junta). Duas cópias divergiriam em silêncio.
4. **Tabela `sala_infantil`** (migração 010): o servidor precisa saber quais perfis da conta são adultos para a regra "amigo de um perfil adulto da mesma conta". A TV declara `infantil:1` nas rotas `/v1/sala*`; sem o sinal, a marca sai.
5. **Entrada por código com aceite**: o pedinte fica num socket "pendente"; o aceite fecha esse socket com o código 4100 e a TV pede um bilhete novo em `/v1/sala/entrar`, que agora entra direto (o DO lembra o aceite). Evita trocar a identidade de um socket hibernado.
6. **Rota extra `POST /v1/sala/convidar`** para o "Convidar mais" do cartão da sala.
7. **O QR do celular é curto e não leva bilhete**: `https://nuvio-juntos…/j/<sala 32 hex><código 8>` (~87 bytes). O `qr.c` das TVs só codifica até ~134 bytes, e um bilhete assinado não cabe. A TV pede o código ao próprio DO pelo websocket (`{t:"celular", chat}`); ele vale 10 min e uma vez, e no perfil infantil o chat sai desligado no servidor. No primeiro uso o DO entrega ao celular uma chave de sessão (`/ws?s=<sala>&k=<chave>`) para reconectar. A rota `POST /v1/sala/celular` do plano saiu, e o bilhete passou a ser só de TV.
8. **Nível B segura em vez de só buscar**: quem está adiantado pausa e solta no instante em que o relógio da sala chega; quem está atrasado busca um pouco além (margem de keyframe aprendida) e depois segura. Só com seek, o keyframe de 2 s mantinha o .wgt a até 2 s no simulador.
9. **Folga do play agendado inclui `prep`** (latência seek + play medida pela TV mais lenta), além de 2 × RTT.
10. **Convite sem veredito da fonte** (o veredito vem depois de entrar) e **perfil infantil só no servidor** até a TV ter a marca: ver STATUS.md.
