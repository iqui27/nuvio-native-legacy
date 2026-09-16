# Recomendar um titulo a um amigo — plano

Pedido do dono (15/09/2026): "criar a opcao de mandar uma recomendacao de filme
para um amigo, ai chega uma notificacao no app"; a aba nova mora no painel de
Salvos (tecla AZUL), chamada **Social**; a mensagem pode ser um **template
bonito** ou um **texto curto** digitado.

Tudo marcado FATO abaixo foi medido nesta sessao. O que nao foi, esta marcado
SUPOSICAO.

---

## Parte 0 — A pergunta que decidia tudo: o Nuvio ja tem isso?

Nao tem. FATO, medido em 15/09/2026 contra o Supabase de producao do Nuvio
(`GET /rest/v1/` com a anon key, que e o indice OpenAPI do PostgREST e lista
TODA superficie exposta):

    tabelas  avatar_catalog, collections, home_catalog_settings, library_items,
             profile_settings_blobs, profile_tracker_settings, profiles,
             tracker_tv_login_sessions, tv_login_sessions, user_tracker_tokens,
             watch_progress, watched_item_events, watched_items

    rpc      approve_tv_login_session, cleanup_anonymous_users,
             clear_profile_pin[_with_account_password], clear_tracker_tokens,
             emit_sync_invalidation, expire_old_tv_login_sessions,
             get_avatar_catalog, get_profile_tracker_settings,
             get_supporter_wall, get_sync_overview, get_sync_owner,
             get_tracker_tokens, health_ping, normalize_tv_subtitle_colors,
             poll_[tracker_]tv_login_session, random_tv_login_code,
             record_activity_event, set_profile_pin, start_device_login_session,
             start_[tracker_]tv_login_session, sync_* (25), upsert_*, verify_profile_pin

Nao ha `friends`, `recommendations`, `messages`, `notifications` nem nada
equivalente — nem tabela, nem RPC. O app web (`NuvioWeb-0.3.38-beta`) tambem nao
chama nenhuma: a lista completa de RPC que ele usa e so a familia `sync_*` mais
o login de TV.

E este repositorio e um fork NAO OFICIAL (README: "not affiliated with
NuvioMedia"): a chave anonima que ele carrega da leitura e escrita nas tabelas
DA PESSOA, e nada mais. Criar tabela no projeto deles nao e uma opcao tecnica —
e um pedido a outro dono, com prazo que nao controlamos.

**Decisao: servico proprio, minimo, que nao toca em nada do Nuvio.**

## Parte 1 — Realtime ou "na proxima vez que abrir"? As duas, por sondagem

FATO: este app so tem HTTP. `rede.c` carrega a libcurl do aparelho por `dlopen`
e expoe GET/POST/DELETE (`rede.h`); nao ha WebSocket, nao ha SSE, e o Supabase
Realtime — que e WebSocket — nunca foi portado (`nuvem.h`: "Nao ha WebSocket nem
SDK"). No Tizen o mesmo codigo roda em WebAssembly, onde um socket novo custaria
outro caminho ainda.

Entao "realtime" aqui significa **sondagem curta**, e para este uso ela basta:

| quando | o que acontece |
|---|---|
| ao abrir o app | uma consulta, junto do ciclo que `atualizacao_verificar()` ja dispara |
| app aberto | uma consulta a cada **60 s**, no fio de sync que ja existe |
| ao abrir a aba Social | consulta imediata, para nao mostrar lista velha |

Custo por TV: 1 requisicao/minuto, resposta `304 Not Modified` no caso comum
(o servidor devolve `ETag`; o cliente manda `If-None-Match`). Uma recomendacao
enviada aparece na TV do amigo em **ate 60 s** com o app aberto, e na hora se
ele abrir o app depois. Ninguem percebe a diferenca para um push de verdade.

REJEITADO — long-poll (segurar a conexao 30 s): prende um fio de curl por TV,
atravessa mal proxy de operadora, e a TV suspende a rede ao dormir; o ganho
seria de segundos num aviso que nao e urgente.

REJEITADO — push nativo do webOS/Tizen: exige app registrado na loja de cada
fabricante. Este app e instalado por sideload.

## Parte 2 — Quem e "amigo"

O dono escolheu **os dois caminhos**, porque nenhum cobre todo mundo sozinho.

### 2a. Amigos do Trakt (quem tem Trakt vinculado)

FATO: o app ja fala com esses endpoints — `users/me/following` (trakt.c:677),
`users/me/friends/activities` (trakt.c:728), `users/settings` (trakt.c:879), e
`social.c` ja desenha a atividade de UMA pessoa. Ou seja, a lista de contatos ja
existe no aparelho; falta so poder escrever para ela.

Identidade no nosso servidor: `trakt:<slug>`. O servidor NAO confia no que o
cliente diz — ele chama `GET https://api.trakt.tv/users/settings` com o token
que veio no pedido e usa o slug que o Trakt responder. Token nao e guardado.

### 2b. Codigo de pareamento (quem so tem conta Nuvio)

Muita gente nao vinculou Trakt — foi exatamente por isso que `salvos.c` existe
("QUEM NAO TEM TRAKT VINCULADO NAO CONSEGUIA SALVAR NADA"). Para essas pessoas:
cada conta tem um **codigo de 6 caracteres** em `a-z0-9` (o teclado da TV e
`busca.c:106`, so minusculas e digitos — codigo com simbolo seria indigitavel).
O amigo digita uma vez e vira contato permanente, nos dois sentidos.

Identidade: `nuvio:<sub>`. Verificada sem ter o segredo do projeto deles: o
servidor chama `GET <SUPABASE_URL>/auth/v1/user` com o access token recebido e
le o `id`. Se o Supabase recusar, o pedido morre ali.

Em nenhum dos dois casos o servidor guarda token — so o identificador estavel.

## Parte 3 — O servico (`servidor/recomendacoes/`)

Cloudflare Worker + D1 (SQLite). Motivos: sempre de pe sem maquina para cuidar,
faixa gratuita folgada para a escala real (dezenas de TVs), TLS e dominio
prontos, e o deploy e um comando. Roda igual em Node se um dia precisar mudar —
a logica nao usa nada exclusivo da plataforma alem do binding do D1.

Tabelas:

    pessoa(id TEXT PK,            -- "trakt:slug" | "nuvio:<sub>"
           nome TEXT, codigo TEXT UNIQUE, criado INT, visto INT)

    contato(a TEXT, b TEXT, criado INT, PRIMARY KEY(a,b))   -- simetrico, 2 linhas

    rec(id INTEGER PK AUTOINCREMENT,
        de TEXT, para TEXT, criado INT,
        imdb TEXT, tipo TEXT, titulo TEXT, poster TEXT, ano TEXT,
        modelo INT,        -- indice do template; -1 = texto livre
        texto TEXT,        -- <= 60 chars, so quando modelo = -1
        visto INT, aberto INT)

Rotas (todas com `Authorization: Bearer <token>` + `X-Nuvio-Auth: trakt|nuvio`):

    POST /v1/eu            -> registra/atualiza a pessoa, devolve id, nome, codigo
    GET  /v1/contatos      -> lista de contatos (id, nome, origem)
    POST /v1/contatos      -> {codigo} vincula os dois lados
    POST /v1/rec           -> envia {para, imdb, tipo, titulo, poster, ano, modelo, texto}
    GET  /v1/rec?desde=N   -> recebidas com id > N  (ETag; 304 quando nada mudou)
    POST /v1/rec/visto     -> {ids:[...]} marca lidas

Limites, porque o servidor passa a hospedar dado de estranho:

- so contato envia para contato; para quem nao e contato, 403;
- 20 recomendacoes enviadas por pessoa por dia; 5 por par por dia;
- texto livre: 60 caracteres, `a-z0-9` e espaco, nada mais entra no banco;
- retencao 90 dias (`DELETE FROM rec WHERE criado < ?`, no cron do Worker);
- bloquear contato: remove as duas linhas de `contato` e apaga as rec pendentes;
- nada de token gravado, nada de e-mail, nada de IP em log.

## Parte 4 — O cliente

### 4a. Modulo novo `src/recomenda.c/.h`

Espelha a disciplina de `atualizacao.c`: fio proprio para a rede, estado atras de
mutex, o laco de desenho so le. API:

    void recomenda_iniciar(void);                 // le cursor e cache do disco
    void recomenda_verificar(void);               // dispara consulta (idempotente)
    int  recomenda_n_novas(void);                 // para o selo na aba
    int  recomenda_n(void);  const Rec *recomenda_item(int i);
    int  recomenda_enviar(const CatItem *ci, const char *paraId, int modelo,
                          const char *texto);     // enfileira e envia no fio
    int  recomenda_contatos(RecContato *saida, int max);

Cache em `recomendacoes.txt` pela mesma `dados_gravar` atomica de `salvos.c`, e
cursor em `recomendacoes-cursor.txt`. Sem rede, a aba abre com o que ja tinha —
a mesma razao pela qual `salvos.c` guarda titulo e poster, e nao so o id.

`sync_esquecer_usuario` apaga os dois arquivos: recomendacao e tao pessoal
quanto a lista de salvos.

### 4b. Enviar — de dentro do detalhe

No menu de contexto do detalhe entra **"Recomendar a um amigo"**, e o fluxo tem
tres telas curtas, todas em D-pad:

1. **Para quem** — lista de contatos com avatar (Trakt ja da avatar; conta Nuvio
   usa a inicial). Ultimo item: "Adicionar amigo por codigo".
2. **O que dizer** — grade de cartoes com os modelos prontos, e um ultimo cartao
   "Escrever" que abre o teclado.
3. **Confirmacao** — um toast curto, sem tela nova.

Modelos (texto final traduzido por `i18n`, como todo o resto):

    "Assiste isso hoje"      "Melhor do ano"        "Confia em mim"
    "Voce vai chorar"        "Da pra ver junto?"    "Terminei, sua vez"

Texto livre exige uma mudanca em `busca.c`: o teclado de 36 teclas precisa de
ESPACO e de uma tecla de apagar visivel. Fica numa funcao reaproveitavel
(`teclado_abrir(titulo, maxChars)`) em vez de copiar a grade.

### 4c. Receber — a aba Social no painel de Salvos

`salvospainel.c` hoje nao tem abas. Entram duas, no topo da camada:

    [ SALVOS ]   [ SOCIAL (2) ]

Esquerda/direita na primeira linha troca de aba; o resto do D-pad nao muda. O
selo com numero some quando a aba e aberta.

Cada recomendacao e uma linha com poster (a mesma `tex_cache`), titulo, quem
mandou, a frase, e o tempo ("ha 2 h"). OK abre o detalhe do titulo — que e a
acao que importa. Botao de apagar no menu de contexto.

### 4d. O aviso

Duas camadas, e nenhuma delas interrompe quem esta assistindo:

1. **Selo** na aba Social e um ponto na tecla AZUL da barra inferior: e o estado
   permanente, e nao exige nenhuma decisao de quem esta na sala.
2. **Cartao ao abrir o app**, no mesmo ponto em que `atualizacao_mostrar_se_houver()`
   e chamado (app.c, bloco do `homePronta`) e obedecendo as MESMAS guardas
   (registro, intro, novidades, pipintro, atualizacao) — "Gustavo te recomendou
   Whiplash", poster, frase, e OK abre o detalhe. So quando ha recomendacao nao
   vista, e uma vez por recomendacao.

NAO entra: toast durante a reproducao. Interromper filme com aviso social e
exatamente o que uma TV nao deve fazer.

## Parte 5 — Ordem de entrega

1. Servidor + esquema, com teste local (`wrangler dev` + D1 local).
2. `recomenda.c` com identidade, contatos e leitura. Aba Social lendo, sem
   enviar.
3. Envio a partir do detalhe, so com modelos.
4. Cartao ao abrir + selo.
5. Teclado com espaco e texto livre de 60 chars.
6. Pareamento por codigo (o caminho Trakt sai antes porque nao precisa de tela
   nova).

Cada passo e testavel sozinho na TV; nenhum depende do seguinte para ser util.

---

## Parte 6 — Aparecer para outras pessoas, e as sugestoes (16/09/2026)

Pedido do dono: "na parte social quando entrar a primeira vez, perguntar se quer
ser discoverable, pq ai podemos adicionar as pessoas que instalaram o app,
mostrar os que tem gosto parecido, ver o historico e adicionar como amigo".

Entregue: a **pergunta**, o **sinalizador** e as **sugestoes**. NAO entregue:
gosto parecido e historico — ver "O que ficou de fora" no fim.

### 6a. A pergunta, uma vez, com o NAO como padrao

Na primeira vez que a aba Social abre, ela E a pergunta: o texto ocupa o painel
e as duas respostas sao as duas unicas linhas. Nao da para rolar por cima dela.

Sao TRES estados no cliente (`REC_APARECER_NAO_PERGUNTADO` / `_NAO` / `_SIM`) e
nao dois. "Nao perguntado" e o que faz a tela aparecer; "nao" e uma resposta
dada, que nao se pergunta de novo. Com um sinalizador de dois valores, "ela
recusou" e "ela ainda nao viu" seriam o mesmo estado, e a pergunta voltaria a
cada arranque — que e como um consentimento vira um obstaculo a ser clicado.

A resposta mora em `recomendacoes-aparecer.txt`, e gravada ANTES de qualquer
rede, e some com `sync_esquecer_usuario` como o resto. Falha de rede nao a
desfaz: o aviso ao servidor fica pendente e sai no proximo ciclo.

O servidor e a autoridade POR IDENTIDADE (`/v1/eu` devolve `descobrivel`), e a
reconciliacao e em um sentido so: um aparelho que NUNCA perguntou adota o "sim"
respondido em outra TV da mesma pessoa; um aparelho que ja tem resposta NAO e
virado pelo servidor — ele e que corrige o servidor no ciclo seguinte.

### 6b. O sinalizador no servidor

`pessoa.descobrivel INTEGER NOT NULL DEFAULT 0` (migracao 002, so aditiva).
Escrito por UMA rota, `POST /v1/descobrivel`, que **nao aceita um id**: quem e
escrito e sempre quem o token identificou. Nao existe pedido possivel que ligue
o sinalizador de terceiro — a ausencia do campo e o mecanismo, nao uma validacao
que alguem possa esquecer de rodar.

Ele governa UMA coisa: aparecer na lista de sugestoes dos outros. Nao governa
receber recomendacao, nem o codigo de pareamento, nem os contatos que ja
existem. Por isso a tela pode dizer "se voce recusar, nada muda alem disso" sem
ressalva — e por isso desligar nunca desfaz um vinculo.

### 6c. As sugestoes, e o que elas NAO precisam

`POST /v1/sugestoes` devolve ate 20, de duas fontes:

1. **Trakt** — quem a pessoa ja segue e tambem usa o servico. Os slugs vao no
   corpo; e a MESMA lista que `/v1/contatos/trakt` ja recebia, e o Trakt a
   publica. Nenhum dado novo sai da TV.
2. **Amigo de amigo** — `JOIN` de `contato` com ele mesmo no servidor. O cliente
   nao manda nada, e o que volta e "fulano, alcancavel por Gustavo" — nunca a
   lista de contatos do Gustavo, nem quantos contatos em comum existem.

`descobrivel = 1` filtra as DUAS. O filtro podia valer so para (2) — em (1) a
pessoa ja segue o outro no Trakt e portanto ja sabe que ele existe — e vale para
as duas assim mesmo, porque a tela de consentimento promete isso e uma excecao
silenciosa faria daquela frase uma mentira.

Adicionar e UMA acao: `POST /v1/contatos/sugerido` recalcula as sugestoes com a
MESMA funcao que as desenhou e recusa (403) qualquer id que nao esteja nelas.
Sem isso a rota seria "vincule-me a qualquer id" e quem soubesse um `nuvio:<sub>`
viraria contato sem passar por codigo, por Trakt, nem pelo consentimento.

**A varredura automatica dos seguidos do Trakt saiu do arranque.** Ela
transformava em contato, sem ninguem apertar nada e a cada arranque, toda pessoa
que o dono segue no Trakt e que usa o servico. "Mostrar ... e adicionar como
amigo" sao dois passos, e o segundo e de quem esta olhando. A rota continua
existindo e o botao "procurar amigos do Trakt" da tela de amigos continua
vinculando na hora — o que deixou de existir e a varredura silenciosa. Reverter
e recolocar uma linha em `ciclo()`, e o comentario la diz qual.

### 6d. O que ficou de fora, e por que e outra decisao

"Mostrar os que tem gosto parecido" e "ver o historico" exigem que a TV mande
para o nosso servidor **o que a pessoa assiste** — coisa que ela nunca fez: o
servico guarda recomendacoes e contatos, e mais nada. Isso e uma escalada de
privacidade, nao um recurso a mais, e contradiz a regra escrita do proprio
projeto (`PLANO-CONTA-SYNC.md`, contrato de RPC social): *"circle_list devolve
ultima_atividade como marca de tempo, NUNCA o que a pessoa assistiu. So aparece
o que ela indicou de proposito."*

O dimensionamento das opcoes (que dado minimo cada uma exige, onde ficaria, o
que um estranho inferiria e como se revoga) foi entregue no relatorio da sessao
que escreveu esta parte. Nenhuma linha de codigo dela existe.
