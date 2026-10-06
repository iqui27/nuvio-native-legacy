# Servidores locais (Plex, Jellyfin, Emby) — plano de integração profunda

Base lida: `/Volumes/ExternalSSD/nv-202-amigosdup` (master pós-2.0.1), só leitura. Referência: NuvioWeb 0.3.38-beta. Pesquisa com links em `servidores-locais-pesquisa.md` (mesma pasta).
Data: 06/10/2026. Nada aqui foi provado em TV nem em servidor real: é plano.

---

## Decisões do dono (06/10/2026)
| # | Decisão | Efeito no plano |
|---|---|---|
| D1 | **A lista de servidores fica só na TV.** Nada sincroniza pela conta Nuvio. | Sem sync de lista. Cada TV/perfil configura o seu. Tokens e endereços só no aparelho (§2.3, §8). |
| D2 | **Fonte do servidor local SEMPRE em primeiro** quando o título existe no servidor e nos add-ons debrid/torrent. Sem limite de qualidade. | `pontos()` coloca a fonte do servidor no topo. Auto-play escolhe o servidor (§3.1). |
| D3 | **Progresso e assistido de itens casados vão também para o Trakt e a conta Nuvio**, sem nunca scrobblar duas vezes. | Item casado vira canônico (`tt…`). Regras anti-duplo em §5.3. |
| D4 | **Sem Samsung .wgt.** Foco em LG, Samsung .tpk e Android TV. | O WGT continua "indisponível nesta plataforma". Nenhuma fase cuida dele. |
| D5 | **O dono tem servidor Jellyfin/Plex real.** | Fase de teste com servidor real na **LG C9** e na **TCL Android** (§9.3). Pedir a URL ao dono quando a implementação começar. **Nunca gravar credencial, token ou URL privada em docs, commits ou memória.** |
| D6 | **O progresso do servidor entra no Continuar assistindo principal**, fundido. Por padrão não existe fileira "Continuar no <servidor>" separada. | `montarContinuar` funde as entradas casadas por id canônico (a mais recente vence). A fileira separada existe só como opção desligada no editor (§3.5). |
| D7 | **Vários servidores por perfil: depois** (fica na F6). | MVP com 1 servidor de cada tipo por perfil, como hoje. O `srvidx` já guarda o índice do servidor para não refazer (§10). |
| D8 | **Assistido nos dois sentidos LIGADO por padrão.** | O ajuste "Sincronizar assistidos" vem em "Dois sentidos" (§5.4). |
| D9 | **Itens sem IMDb/TMDB não aparecem na Home**, só na Visão do servidor. | As fileiras da Home filtram itens não casados. Esses itens aparecem só na tela `srvvisao`, que fica com id opaco e privada (§3.5, §3.6). |
| D10 | **O Plex é testado só no servidor real do dono.** Sem conta Plex de teste em Docker. | Docker só para Jellyfin (10.10 e 12.x) e Emby. Plex: fixtures (`tests/plex_server.py`) + servidor real (§9). |

---

## 0. Resumo executivo

1. **Não partimos do zero.** Desde a 1.8 o app tem uma fatia experimental e desligada por padrão: `src/jellyfin.c` (Jellyfin + Emby no mesmo módulo), `src/plex.c`, a fachada `src/servidores.c`, os ids opacos `jf:` / `em:` / `px:` (`src/jfid.h`), fileiras de biblioteca na Home, a página do título preenchida pelo servidor, PlaybackInfo (Jellyfin/Emby), check-ins de progresso e testes com servidor falso. As notas da 2.0.0 já anunciam isso.
2. **O que falta é exatamente o que o dono pediu.** Hoje o servidor é uma "ilha" separada do resto do app:
   - só o Plex casa um item com o filme normal (índice imdb/tmdb);
   - Jellyfin e Emby jogam fora os ProviderIds;
   - nada aparece na busca, na Biblioteca/Salvos, nem como selo "Disponível no servidor" na página do título;
   - não existem fileiras de Continuar / Próximos / Adicionados recentemente.
3. **Há dois problemas urgentes antes de tudo:**
   - O **Jellyfin 12.0 (lançado em 07/09/2026) removeu o `api_key` na query**. Nossa URL do player usa `api_key=` (`jellyfin.c` `anexarApiKey`). Num servidor atualizado, a reprodução tende a dar 401. Correção: `ApiKey=` (a forma de query que o 12 ainda aceita). Falta provar que todas as rotas de stream, HLS e legenda aceitam `ApiKey` no 12.x: é o primeiro teste da F0 contra o servidor real ou o Docker 12.
   - **O Plex oferece "Direct play" de qualquer arquivo sem perguntar ao servidor** (não usa `/video/:/transcode/universal/decision`). Arquivo com DTS, TrueHD ou PGS tende a dar tela preta ou ficar sem som.
4. **Ideia central: um índice de casamento único**, novo módulo `srvidx`. Cada item de cada servidor vira uma linha com `(imdb, tmdb, tvdb, título normalizado, ano, tipo) → (servidor, itemId, resumo de qualidade)`.
   - Fica em memória e em disco, por perfil.
   - Atualiza de forma incremental.
   - Consulta sem rede.
   - Esse índice alimenta quatro superfícies: fonte, busca, biblioteca e selo na página do título.
5. **Identidade (D3).** Um item do servidor **com** id externo passa a ser o título canônico (`tt…`) em todo o app: metadados TMDB, Continuar assistindo, Trakt, conta Nuvio. O servidor vira mais uma *fonte* desse título. Item **sem** id externo (vídeo caseiro, gravação) continua com o id opaco `jf:`/`px:`, isolado como hoje.
6. **Fonte (D2).** A cópia do servidor entra na lista de fontes **sempre em primeiro lugar**, com o selo "Local · <nome do servidor>" e selos reais de qualidade/HDR/áudio (MediaStreams). Isso atende o pedido nº 1 da comunidade (#249 aqui; NuvioTV#1913/#2217).
7. **Fileiras.** A Home ganha, por servidor:
   - progresso fundido no Continuar assistindo principal (D6);
   - "Próximos episódios";
   - "Adicionados recentemente";
   - fileiras por biblioteca, coleção e playlist.

   Todas passam pelo editor de fileiras, com origem própria "Servidor". Na Home só entram itens com IMDb/TMDB; o resto fica na Visão do servidor (D9). O Continuar assistindo principal pode unificar o progresso do servidor com o da conta e do Trakt, sem duplicar.
8. **Visão do servidor.** Uma tela própria em ilhas mostra as bibliotecas como o servidor organiza (Filmes, Séries, Coleções, Playlists), com abas e grade paginada. O "Ver tudo" de cada fileira leva para ela.
9. **Progresso e assistido nos dois sentidos**, com regra de "última intenção vence" e um jornal por servidor (`srvpend`) no mesmo modelo do `contapend.c`. Uma regra anti-scrobble-duplo cobre o caso de o servidor também ter plugin Trakt.
10. **MVP em ~3 semanas de desenvolvimento** (fases F0 e F1), mais a prova com o servidor real do dono na C9 e na TCL. Plano completo em ~10–12 semanas. Plataformas: LG, Samsung .tpk e Android TV. Sem .wgt (D4).
    - **F0:** consertos e segurança.
    - **F1:** índice de casamento + fonte local em primeiro com selo + selo na página do título + QR no login + decision do Plex.
11. **Concorrência mudou em outubro:** o NuvioTV oficial tem um PR em rascunho (#3728) com Jellyfin/Emby nativos e casamento com add-ons. Não tem Plex nem LG/Samsung. Nosso diferencial passa a ser: Plex + LG/Samsung nativos + fonte local sempre primeiro + progresso unificado sem scrobble duplo.

---

## 1. O que existe hoje (verificado no código)

### 1.1 Módulos e contratos
| Arquivo | Papel |
|---|---|
| `src/jellyfin.c/.h` (1907 l.) | Protocolo + integração de Jellyfin **e** Emby (`JfConta.tipo`), em duas instâncias (`instJf`, `instEm`). Cada uma tem trabalhador de controle, trabalhador de check-in, `RedeGrupo` com geração e snapshot de fileiras. |
| `src/plex.c/.h` (2015 l.) | Fluxo de PIN do plex.tv, `/api/v2/resources`, escolha de conexão (LAN http → LAN uri → remoto → relay), fileiras, ficha, fontes, timeline, **índice imdb/tmdb** (`PxIdx`, até 20 mil títulos). |
| `src/servidores.c/.h` | Fachada que distribui para os três por prefixo de id. `servidores_passo()` roda a cada quadro (`app.c:2399`). |
| `src/plexaddons.c` | Registra `plex_consultar` como "origem extra" de fontes no `addons.c`. |
| `src/jfid.h` | Id opaco `jf:<8hex srv>.<32hex item>` (e `em:`, `px:`). `jfid_e()` é a barreira de privacidade: Trakt, conta, Discord, social e add-ons nunca recebem esses ids. |
| `tests/jellyfin.c`, `emby.c`, `plex.c` + `*_server.py` | Servidores falsos e fixtures. |
| `docs/plans/media-servers-1.8/README.md`, `STATUS-F11.md` | Pesquisa e status da 1.8. |

### 1.2 Conexão e conta
- **Ajustes → Integrações → "Servidores pessoais (experimental)"** (`ajustes.c` ~4043–4209). Um único interruptor (`AJ_JF_LIGADO`) liga os três tipos.
- **Jellyfin:** URL digitada, com o prefixo de proxy reverso preservado. Depois Quick Connect (o código aparece **em texto**, sem QR) ou usuário e senha. A senha é apagada logo após o pedido.
- **Emby:** só usuário e senha.
- **Plex:** PIN de 4 letras para plex.tv/link (o PIN é "não-forte"). Escolhe o servidor e permite trocar entre os servidores da conta (`plex_proximo_servidor`).
- **Limite:** um servidor de cada tipo, por perfil, nesta TV.
- **Token:** fica em `jellyfin-p<N>.txt` / `emby-p<N>.txt` / `plex-p<N>.txt` (0600). Nunca vai para a conta nem para os logs. É apagado no logout da conta (`sync.c:2008`) e respeitado na troca de perfil (`perfis.c:381`).
- **Samsung .wgt:** fica **indisponível** (`jellyfin_disponivel()`), porque a rede síncrona do WGT não tem `REDE_CAP_JOB`.

### 1.3 Home
- Uma fileira fixa por biblioteca de filmes/séries: até 6 por servidor, 24 itens, ordem `DateCreated` / `addedAt:desc`. Chaves `jellyfin_<lib>`, `emby_<lib>`, `plex_<sec>`.
- Juntadas em `descoberta.c:4363-4390`, depois das fileiras fixas do topo.
- Aparecem no editor de fileiras, mas com a etiqueta **"Catálogo"** (`fileiras.c:140-147`, `FIL_ORIGEM_CATALOGO`). Não têm "Ver tudo" (exige `base`+`catId`).
- **Não há** Continuar / Próximos / On Deck / Recentes, nem Coleções ou Playlists.

### 1.4 Página do título e fontes
- Id `jf:`/`px:` → `servidores_ficha` (`descoberta.c:6294`) preenche só com dados do servidor. Nenhuma chamada a TMDB, Cinemeta ou add-on.
- Fontes de item do servidor: `buscarPedido` (`addons.c:2317`) → `servidores_fontes_pedir`. O `badges_detectar` é aplicado no `addons_estado` (`addons.c:470`).
- **Fontes de título normal (`tt…`):** só o Plex participa, via origem extra (`plexaddons.c`). Os 2 slots de `ORIGENS_EXTRA_MAX` já estão ocupados (Plugins + Plex). Nesse caminho **não se chama `badges_detectar`** (a confirmar na tela).
- **Ordem das fontes:** `streams.c pontos()` (`:720`) não dá nenhum bônus a servidor local.

### 1.5 Reprodução e progresso
- **Jellyfin/Emby:**
  - `POST /Items/{id}/PlaybackInfo` com `DeviceProfile` fixo por plataforma (`jf_perfil_dispositivo`): h264/hevc; aac/mp3/ac3/eac3; sem DTS/TrueHD; legendas só `External`.
  - Teto de 120 Mbps.
  - Sempre `StartTimeTicks=0`, e o player faz o seek de retomada.
  - Lista DirectPlay e a alternativa de transcode (HLS) quando existe.
- **Plex:** lista o Part de cada versão como "Direct play" **sem decision** e um `start.m3u8` (transcode) só para a 1ª versão (`px_ler_fontes`).
- **Token na URL:** Jellyfin `api_key=` (**quebra no Jellyfin 12**), Plex `X-Plex-Token=`. Isso acontece porque LG e Samsung não deixam o player mandar cabeçalho.
- **Check-ins:**
  - Jellyfin/Emby: Playing / Progress a cada 10 s / pausa / Stopped.
  - Plex: `/:/timeline` + `transcode/universal/stop`.
- O servidor é a autoridade do progresso dos itens `jf:`/`px:`. **Nada volta para a conta Nuvio nem para o Trakt** (`progresso.c:276`, `player.c:2956`).
- **Título `tt` tocado por fonte do Plex:** reporta ao Plex (o tick casa pela URL) **e** segue o caminho normal de conta e Trakt. Isso é desejável, mas sem regra anti-duplicação.

### 1.6 O que não está ligado (lacunas)
1. Jellyfin/Emby não casam com `tt` (sem índice, ProviderIds descartados em `itemDe`).
2. Busca (`busca.c`, `desc_buscar`) não consulta servidor.
3. Biblioteca/Salvos não sabe de servidor.
4. Nenhum selo "Disponível em <servidor>" na página do título.
5. Sem Continuar/Próximos/Recentes.
6. Sem "preferir local" na ordem das fontes.
7. Sem QR para Quick Connect e PIN (o `qr.c` já existe e é usado no login Nuvio).
8. Sem descoberta na LAN.
9. Um servidor por tipo; um interruptor para tudo.
10. Plex sem decision; token do Plex aparece nas URLs de pôster.
11. WGT sem servidores.
12. Nenhuma prova em TV nem em servidor real (`STATUS-F11.md`).

---

## 2. Arquitetura proposta

```
             +--------------------- servidores.c (fachada) ----------------------+
 Ajustes --> | contas (srvconta) | índice (srvidx) | fileiras | fontes | progresso (srvpend) |
             +------+--------------------+-------------------+----------------------+
                    |                    |                   |
              jellyfin.c (jf/em)      plex.c              (futuro: kodi? não)
                    |                    |
                 rede_pedir (RedeJob, geração por perfil/servidor)
```

### 2.1 Módulos novos ou alterados
| Módulo | Novo? | Responsabilidade |
|---|---|---|
| `src/srvconta.c/.h` | novo | Lista de servidores **por perfil** (N servidores, qualquer tipo). Substitui "um arquivo por tipo". Arquivo `servidores-p<N>.txt` (0600), **só nesta TV (D1)**: tipo, base(s) local/remota, serverId, userId, deviceId, nome amigável, prioridade, flags. Tokens em `srvtok-p<N>-<tag>.txt` (0600), separados da lista, para facilitar apagar e excluir dos pacotes. Migra os arquivos atuais `jellyfin-p<N>.txt` etc. na primeira leitura. |
| `src/jellyfin.c` | altera | As instâncias deixam de ser fixas (`instJf`/`instEm`) e viram um vetor dinâmico (`JfInst *` por servidor). Lê `ProviderIds`. Ganha as rotas Resume / NextUp / Latest / Search / UserPlayedItems. `ApiKey=`. DeviceProfile vindo de `srvperfil`. |
| `src/plex.c` | altera | Vetor de servidores, decision universal, hubs (continueWatching, onDeck, recentlyAdded), `/hubs/search`, `/:/scrobble` e `/:/unscrobble`, `tvdb://` no índice. Pôster por `/photo/:/transcode` (ver §7). |
| `src/srvidx.c/.h` | novo | Índice unificado de casamento (ver §4). Persistido em `srvidx-p<N>-<tag>.bin`. |
| `src/srvperfil.c/.h` | novo | Capacidades do aparelho → DeviceProfile (Jellyfin/Emby) e `X-Plex-Client-Profile-Extra` (Plex). Uma tabela por backend e ano/versão (ver §6). |
| `src/srvpend.c/.h` | novo | Jornal de assistido/progresso por perfil e servidor, no padrão do `contapend.c` (ver §5). |
| `src/srvvisao.c` | novo | Tela "Visão do servidor" (ilhas, abas e grade). |
| `src/addons.c` | altera | `ORIGENS_EXTRA_MAX` 2 → 3 **ou**, melhor, uma origem extra única "servidores", com `servidores_consultar()` distribuindo para todos. `badges_detectar` também no caminho de origem extra. Não guardar "sem fonte" do servidor em cache (lição do NuvioTV#3175). |
| `src/streams.c` | altera | `pontos()`: prioridade absoluta para `Stream.origemServidor` (campo novo, ou `bingeGroup` com prefixo `srv|`), conforme D2. Filtro/aba "Meu servidor" na folha de fontes. |
| `src/busca.c`, `descoberta.c` | altera | Alvo de busca "servidores" (§3.3). |
| `src/fileiras.c/.h` | altera | `FIL_ORIGEM_SERVIDOR` com ícone e texto próprios. "Ver tudo" para fileira de servidor. |
| `src/detail.c` | altera | Ilha/selo "Disponível em <servidor>" + qualidade (§3.4). |
| `src/biblioteca.c`, `salvospainel.c` | altera | Filtro e aba "No meu servidor" (§3.5). |
| `src/ajustes.c` | altera | Seção "Servidores" redesenhada (§2.3), um interruptor por servidor, QR. |

### 2.2 Identidade (decisão central)
- **Item casado** (tem imdb, tmdb ou tvdb): o `CatItem` mostrado na Home ou na visão do servidor usa o **id canônico** (`tt…`, ou `tmdb:` quando não há imdb). O vínculo com o servidor fica no `srvidx`, e não no id.
  - Consequência: página do título completa (TMDB, elenco, trailers, avaliações), Continuar assistindo unificado, Trakt, social e conta funcionam sem código especial.
  - A cópia do servidor aparece como fonte "Local".
- **Item não casado:** continua `jf:`/`em:`/`px:`, com a ficha só do servidor e o isolamento de privacidade atual (`jfid_e`).
- **Episódio:** o casamento é por série (tvdb/tmdb/imdb da série) + temporada/episódio (`ParentIndexNumber`/`IndexNumber` no Jellyfin; `parentIndex`/`index` no Plex). Ordem de exibição diferente (DVD vs aired) é risco conhecido, ver §10.
- **Decidido (D3):** o item casado vira canônico, e o progresso dele vai para a conta e o Trakt. Hoje o código evita isso de propósito (`progresso.c:276`, `player.c:2956`). Essas barreiras continuam valendo **só** para ids opacos, que permanecem privados.

### 2.3 Contas e conexão (UX de TV)
Ajustes → **Servidores** (sai do "experimental" quando F1 estiver provada):

```
╭─ Servidores ─────────────────────────────────────────────╮
│  ● Casa (Jellyfin 12.2)      Conectado · LAN · 2 bibliot. │  ← ilha por servidor
│  ● Plex do João (Plex)       Conectado · Remoto (relay)   │
│  ○ Emby antigo               Token expirou — entrar de novo│
│  ＋ Adicionar servidor                                     │
╰───────────────────────────────────────────────────────────╯
```

- **Adicionar servidor:**
  1. **Procurar na rede**: Jellyfin/Emby por UDP 7359 ("who is JellyfinServer?" / "who is EmbyServer?"); Plex por GDM, ou direto pela conta plex.tv.
  2. **Digitar endereço** (como hoje).
  3. **Plex**.
  - Descoberta só sugere endereço. Nada autentica sozinho, e o servidor é conferido por `/System/Info/Public` ou `/identity` antes de qualquer credencial (regra já existente).
- **Login com QR** (reaproveita `qr.c`, como o login Nuvio):
  - Jellyfin Quick Connect: QR para `<base>/web/#/quickconnect`, com o código de 6 dígitos grande ao lado ("aprove no app do Jellyfin já logado").
  - Plex: QR para `https://plex.tv/link` com o código. Avaliar o PIN forte + `app.plex.tv/auth#?code=…`, que dispensa digitar.
  - Emby: usuário e senha (Emby Connect fica para a V2).
- **Várias URLs por servidor:** `local` (LAN) e `remota` (proxy/HTTPS).
  - A escolha é automática, por sonda curta (4 s, já existe no Plex): LAN primeiro.
  - A troca acontece sem novo login (mesmo `serverId`). `serverId` diferente = outro servidor → recusa.
- **Saúde:** cada ilha mostra estado, LAN/Remoto/Relay, versão, latência e último erro (texto traduzido, nunca o URL).
  - Ação "Testar agora".
  - Plex com relay mostra o aviso "limitado a ~2 Mbps pelo Plex".
  - Plex remoto sem passe mostra "Este servidor exige Plex Pass / Remote Watch Pass para assistir fora de casa" (ver pesquisa).
- **Interruptor por servidor** e um mestre "Servidores pessoais".
- **Por perfil:** cada perfil tem sua lista (crianças não veem o servidor do adulto). Opção "usar também em: [perfis]" para não repetir login.

---

## 3. Superfícies (o "casar com o filme")

### 3.1 Fonte na folha de fontes
- **Origem:** `servidores_consultar(id, tipo, temporada, episodio)` roda no fio de origem extra.
  1. Consulta o `srvidx` sem rede.
  2. Só se houver casamento, abre o servidor: PlaybackInfo (Jellyfin/Emby) ou metadata + decision (Plex).
  - Sem casamento: zero custo de rede.
- **Cartão da fonte** (padrão atual de selos + linha 2):
  `2160p · Direct play` | `Casa (Jellyfin) · HEVC · Dolby Vision · TrueHD 7.1 → EAC3 · MKV`
  - Selo **"Local"** (ícone casa) ou **"Remoto"** (ícone nuvem) + nome do servidor.
  - Selos de qualidade/HDR/áudio vindos dos **MediaStreams reais** (campos `altura`, `dolbyVision`, `dolbyAtmos`, `badges`), nunca de texto.
  - Método explícito: "Direct play", "Direct stream (áudio convertido)", "Transcode 1080p".
  - Várias versões (4K + 1080p) = vários cartões. Agrupadas por padrão; expansível.
- **Ordem (D2, decidido): fonte do servidor SEMPRE em primeiro**, sem limite de qualidade.
  - Em `pontos()`, um bloco de prioridade acima de qualquer pontuação de add-on (ex.: +10.000.000 para `Stream.origemServidor`), aplicado antes das penalidades de cache/P2P.
  - **Entre as cópias do servidor:** Direct play > Direct stream > Transcode, e maior qualidade primeiro. Servidor LAN vem antes de remoto e relay.
  - A versão Direct play/stream do servidor fica no topo mesmo que o debrid tenha 4K e o servidor 1080p. A pessoa ainda vê as outras fontes logo abaixo.
  - **Única exceção técnica, não de qualidade:** quando o servidor está inalcançável naquele momento (sonda falhou), o cartão aparece desabilitado "Servidor fora do ar" e não bloqueia o auto-play.
- **Auto-play** (`fonteauto`, `stream_primeira_boa`): escolhe o servidor sempre que ele tem o título e respondeu. A fonte lembrada (`fontepref`) casa por `provedor` + servidor.
- **Filtro:** aba "Meu servidor" ao lado das abas de add-on (o padrão de chips do NuvioWeb `orderSourceNames`).
- **Sem cache de "não tem":** resultado negativo do servidor **não** entra em `fontecache_*`. O índice é atualizado no máximo a cada N minutos, e há "atualizar" manual.

### 3.2 Página do título
- Ilha discreta abaixo do meta: **"▶ No seu servidor · Casa · 4K Dolby Vision"**.
  - OK: toca direto (atalho que pula a folha, com a fonte do servidor já escolhida).
  - Direita: "Outras fontes".
- **Série:** em cada episódio, um ponto/selo "no servidor" (pedido do NuvioTV#3003), calculado pelo índice de episódios (§4.3).
  - "Temporada 2: 8/10 no servidor".
- **Progresso:** se o servidor tem posição mais nova que a conta, o botão diz "Continuar 42 min (Casa)".

### 3.3 Busca
- Novo alvo em `desc_busca_alvo_*`: "Servidores".
  1. **Primeiro, sem rede:** filtra o `srvidx` por título normalizado (prefixo/tokens). Resposta instantânea, inclusive offline da internet.
  2. **Depois (opcional):** `GET /Items?searchTerm=` (Jellyfin/Emby) e `/hubs/search?query=` (Plex) para itens sem id externo e coleções.
- **Mesclagem:** dedupe por id canônico (`cat_indice_por_imdb`). Um resultado que existe no catálogo **e** no servidor ganha o selo "No seu servidor" no pôster. Não aparece duas vezes.
- **Linha própria opcional:** "No seu servidor" no topo dos resultados (o padrão Infuse).

### 3.4 Biblioteca / Salvos
- Nova aba ou filtro **"Meus servidores"**: tudo o que está nos servidores (via `srvidx`), com filtros Filmes/Séries/Não assistidos/4K/HDR.
- Nos Salvos: selo "no servidor" nos itens que o usuário salvou e já tem em casa ("já está disponível").
- **Não** sincronizar a biblioteca do servidor para a conta Nuvio (é inventário pessoal).

### 3.5 Home ("criar fileiras")
| Fileira | Jellyfin/Emby | Plex | Padrão |
|---|---|---|---|
| Continuar (D6) | `GET /UserItems/Resume` (10.9+) / Emby `/Users/{id}/Items/Resume` | `/hubs/home/continueWatching` | **fundida no CW principal** (sem fileira própria por padrão) |
| Próximos episódios | `/Shows/NextUp` | `/library/onDeck` (ou hub onDeck) | ligada |
| Adicionados recentemente | `/Items/Latest?ParentId=` | `/hubs/home/recentlyAdded` / `/library/recentlyAdded` | ligada |
| <Biblioteca> | `/Items?ParentId=` (já existe) | `/library/sections/{k}/all` (já existe) | ligada |
| Coleções / Playlists | `IncludeItemTypes=BoxSet` / `/Playlists` | `/library/sections/{k}/collections`, `/playlists` | fora (aparecem na visão) |

- **Integração ao sistema de fileiras atual:**
  - Chaves estáveis: `srv:<tag>:resume`, `srv:<tag>:nextup`, `srv:<tag>:latest:<lib>`, `srv:<tag>:lib:<lib>`.
  - `fil_origem_de` → `FIL_ORIGEM_SERVIDOR` (ícone de servidor, nome do servidor no editor).
  - O editor de fileiras lista tudo, permite reordenar e ocultar (o `fil_registrar` já cobre), e ganha um grupo "Servidores".
- **Continuar assistindo unificado (D6, padrão):**
  - `montarContinuar` funde as entradas do servidor casadas pelo id canônico com `mesmaObra`/`retomadoMs` (a mais recente vence). Vale para qualquer `ajustes_cw_fonte`.
  - Item não casado **não** entra (D9): fica no "Continuar" da Visão do servidor.
  - A fileira "Continuar no <srv>" existe só como opção, desligada, no editor.
- **Filtro D9:** todas as fileiras do servidor na Home (Próximos, Recentes, bibliotecas) mostram só itens casados (com IMDb/TMDB). O resto fica na Visão do servidor.
- **"Ver tudo"** abre a visão do servidor naquela biblioteca.
- **Hero/destaque:** a fonte do hero pode ser "Adicionados recentemente no servidor" (`fil_hero_fonte`), pedido do NuvioMobile#1537.

### 3.6 Visão do servidor (tela nova `srvvisao.c`)
```
╭─ Casa · Jellyfin ───────────────── [LAN · 12 ms] ─╮
│ [Início] [Filmes] [Séries] [Coleções] [Playlists] │  ← abas (ilha superior)
╰────────────────────────────────────────────────────╯
  Continuar  ▸ ▢ ▢ ▢
  Próximos   ▸ ▢ ▢ ▢
  (aba Filmes) grade 6×N paginada, ordenar: Adicionados | A-Z | Ano | Nota
               filtros (ilha lateral Frost): Não assistidos · 4K · HDR · Gênero
```
- Grade paginada de 60 em 60 (`StartIndex/Limit`, `X-Plex-Container-Start/Size`), com pré-carga da próxima página no fio.
- Coleção/playlist abre como fileira expandida.
- Entrada pela Home ("Ver tudo"), pelo menu (item "Servidores", visível só quando conectado) e pelos Ajustes.

---

## 4. Casamento (`srvidx`)

### 4.1 Estrutura
```c
typedef struct {
  char  imdb[12];        // "tt0111161" ou ""
  int   tmdb, tvdb;      // 0 = sem
  char  tipo;            // 'm' filme, 's' série, 'e' episódio
  short ano;
  unsigned tituloHash;   // hash do título normalizado (sem acento, minúsculo, sem artigos)
  unsigned char srv;     // índice do servidor no vetor do perfil
  char  item[33];        // id do item (hex Jellyfin, ratingKey Plex, numérico Emby)
  unsigned short altura; // melhor versão: 2160/1080/720
  unsigned char hdr;     // 0 SDR, 1 HDR10, 2 HDR10+, 3 DV, 4 HLG
  unsigned char audio;   // bitmask atmos/truehd/dts/eac3
  unsigned char visto;   // Played / viewCount>0
  int   posSeg;          // posição de retomada (para CW sem rede)
  unsigned atualizado;   // DateLastSaved/updatedAt (incremental)
} SrvIdx;                // ~90 B → 20 mil itens ≈ 1,8 MB
```
- Episódios ficam num índice à parte por série (`serieItem → [(temp, ep, item, visto)]`). É carregado sob demanda ao abrir a série e guardado em cache em disco.
- Busca por imdb e tmdb com 2 vetores ordenados (`bsearch`). Título+ano por hash.

### 4.2 Construção e atualização
- **Jellyfin:** não existe filtro por ProviderId (issue #1990; o `AnyProviderIdEquals` é do Emby).
  - Paginar `/Items?userId=&Recursive=true&IncludeItemTypes=Movie,Series&Fields=ProviderIds,MediaSources?…`. Pedir MediaSources pesa demais: usar `Fields=ProviderIds,DateLastSaved,Width,Height` + `EnableUserData=true`.
  - Páginas de 200, na sequência "fileiras visíveis primeiro, índice depois" (padrão do Plex hoje).
  - Incremental: `MinDateLastSaved=<último>`.
- **Emby:** igual, mais `AnyProviderIdEquals=imdb.tt…` para consulta pontual quando o índice ainda não ficou pronto.
- **Plex:** `/library/sections/{k}/all?includeGuids=1` (já existe), mais `tvdb://`. Incremental com `updatedAt>=`.
- **Qualidade no índice:**
  - Jellyfin: `Width`/`Height` do item e `VideoRangeType` quando `Fields=MediaStreams` cabe na página (avaliar o custo). Sem isso, o selo aparece só depois do PlaybackInfo.
  - Plex: `Media[0].videoResolution` + `Stream.DOVIPresent` (exige item), ou `videoResolution` da listagem.
- **Disco:** `srvidx-p<N>-<tag>.bin` com versão. Ao abrir o app, carrega do disco (Home e selos instantâneos) e atualiza em segundo plano.
- **Quando:** ao conectar; a cada 30 min com o app aberto; ao voltar do player; e no "atualizar" manual. O Jellyfin tem WebSocket de `LibraryChanged`, e o Plex tem `/:/websockets/notifications`. Isso fica para a V2.

### 4.3 Regras de casamento
1. imdb igual e tipo igual → casa (confiança alta).
2. tmdb igual e tipo igual → casa.
3. tvdb igual (séries) → casa. O app usa imdb/tmdb, então é preciso tvdb→tmdb pelo TMDB `/find`, que fica em cache.
4. Sem ids: título normalizado + ano ±1 + tipo, **só** quando existe exatamente 1 candidato no TMDB com aquele título/ano. Confiança média: o item não vira canônico sozinho, só ganha a fonte "Local" quando a pessoa abre aquele título.
5. Episódio: série casada + (temporada, episódio). Especiais (temporada 0) e ordem DVD/absoluta: só por `ProviderIds.Tvdb` do episódio quando existir.
6. Várias cópias (mesmo título em 2 servidores ou 2 versões): todas viram fontes, e o índice guarda a melhor para o selo.

---

## 5. Reprodução, progresso e assistido

### 5.1 Negociação
- **Jellyfin/Emby:** mantém o PlaybackInfo, com o DeviceProfile vindo de `srvperfil` (§6). Mudanças:
  - `ApiKey=` (Jellyfin 12) e `api_key=` (Emby).
  - `AudioStreamIndex`/`SubtitleStreamIndex` quando a pessoa troca faixa e a troca exige transcode.
  - `MaxStreamingBitrate` por conexão: LAN = 120 Mbps; remoto = medido ou o ajuste "Qualidade remota".
- **Plex:** passar a usar `GET /video/:/transcode/universal/decision?path=/library/metadata/<rk>&mediaIndex=&partIndex=&protocol=hls&directPlay=1&directStream=1&directStreamAudio=1&…` com `X-Plex-Client-Profile-Extra` (ou `X-Plex-Client-Profile-Name`).
  - A resposta diz directplay / copy / transcode por stream.
  - Só oferecer "Direct play" quando a decision aprovar.
  - Hoje o app oferece sempre: é a correção nº 2 de F0.
- **Resume:** mantém "sempre do zero + seek do player" (evita o deslocamento duplo do transcode). Escolha da posição:
  - `max(atualizado)` entre servidor (`LastPlayedDate` / `lastViewedAt`) e conta/Trakt.
  - Botão "Continuar de 42:10 (Casa)" vs "(Nuvio)" só quando divergem mais de 2 min.

### 5.2 Legendas e áudio
- **Texto (srt/subrip/vtt/ass/ssa):** `External`. O app baixa por `/Videos/{id}/{msid}/Subtitles/{idx}/Stream.{srt|vtt|ass}` (Plex: `/library/streams/{id}`). Entra no **pipeline de legenda atual** (estilos, AutoSync, `assrender`, `mkvass`) como origem "Servidor", ao lado de embutida e add-on.
- **Embutida no MKV (Direct play):** o player já lê as faixas (`mkvass`/faixas). Nada muda.
- **PGS/VobSub (imagem):** se o player não renderiza, é `Encode` (burn-in) **só quando a pessoa escolhe a faixa**. Aviso: "Esta legenda exige converter o vídeo no servidor".
- **AutoSync:** com transcode, a linha do tempo começa no 0 (StartTimeTicks=0), então não há correção dupla. Já é a regra atual.
- **Áudio:** Direct play → a troca é local. Áudio não suportado (DTS na Samsung, TrueHD) → `DirectStream` com áudio convertido para EAC3/AC3 5.1, sem mexer no vídeo. Selo "áudio convertido".

### 5.3 Para onde vai o progresso (sem scrobble duplo)
| Evento | Servidor | Conta Nuvio | Trakt/Simkl |
|---|---|---|---|
| Tocar título casado pela fonte do servidor | check-in (já existe) | sim (id canônico) | scrobble do Nuvio, **exceto** se o servidor estiver marcado "já envia ao Trakt" |
| Tocar título casado por add-on/debrid | `srvpend`: marca assistido/posição no servidor ao terminar (opção "Manter servidor em dia", padrão ligado) | sim | sim |
| Tocar item não casado (`jf:`) | check-in | não | não |
| Marcar/desmarcar assistido na UI (casado) | `srvpend` → `POST/DELETE /UserPlayedItems/{id}` (Jellyfin 10.9+), Emby `/Users/{u}/PlayedItems/{id}`, Plex `/:/scrobble` / `/:/unscrobble` | `contapend` (já existe) | já existe |

- **Anti-duplo (D3: o progresso vai para servidor, conta e Trakt, mas cada destino recebe UMA vez):**
  1. **Um único emissor por destino.** Quem fala com o Trakt e com a conta é o caminho normal do player (`player.c`/`progresso.c`) pelo id canônico. O módulo do servidor **nunca** fala com o Trakt. O check-in do servidor fala **só** com o servidor.
  2. **Eco do pull.** Quando o pull do servidor traz "assistido" de um item que o Nuvio acabou de reproduzir, ele não vira um novo gesto para a conta/Trakt, porque já foi enviado. A regra é por chave `(id canônico, temporada, episódio)` + janela de 6 h na memória do `srvpend`/`contapend`. Só um "assistido" que nasceu fora do Nuvio (no app oficial do Jellyfin, por exemplo) sobe para a conta, uma vez, com `watched_at` do servidor.
  3. **Plugin Trakt no servidor.** Jellyfin (plugin Trakt) e Plex (integração própria) podem também scrobblar. Ao conectar, o app pergunta "Este servidor já envia ao Trakt?" e guarda uma flag por servidor (dá para detectar no Jellyfin via `/Plugins`, a confirmar). Com a flag ligada e reprodução **pela fonte do servidor**, o Nuvio pula o scrobble do Trakt e mantém conta Nuvio + servidor. Com reprodução por add-on, o Nuvio scrobba normalmente e o `srvpend` marca no servidor por `/UserPlayedItems` / `/:/scrobble`. Esses endpoints não geram scrobble de plugin Trakt (a confirmar no teste real).
  4. **Rede de segurança:** o Trakt devolve 409 para scrobble repetido em janela curta. O app trata 409 como sucesso e não tenta de novo.
  5. **Teste obrigatório** (F4): assistir 1 filme pela fonte do servidor e conferir 1 entrada no histórico do Trakt, 1 na conta e "assistido" no servidor.

### 5.4 Assistido nos dois sentidos (`srvpend`)
- **Jornal por perfil × servidor**, gravado antes da rede, com última intenção por item (cópia do modelo `contapend`). Sai do jornal só com 2xx.
- **Puxar** (no ciclo do índice):
  - O `Played`/`viewCount` do servidor alimenta `vistoep` **só** para itens casados e **só** se não houver entrada mais nova no `contapend`/`srvpend` (`contapend_visto_oculto` com o horário remoto do servidor: `LastPlayedDate` / `lastViewedAt`).
- **Conflito, regra única:** o carimbo de tempo mais novo vence.
  - Servidor sem carimbo confiável (Plex unscrobble não tem data) → vence o gesto local.
  - Nunca desmarcar em massa: se o pull trouxer mais de X itens "não assistidos" que antes eram "assistidos", ignora e registra (proteção contra servidor reinstalado).
- **Opção por servidor** "Sincronizar assistidos": Desligado / Só servidor → Nuvio / Dois sentidos. **Padrão: dois sentidos (D8).**

---

## 6. Perfis de dispositivo (`srvperfil`)
- Base por backend, refinada pela versão do SO (o app já registra modelo e versão: linha `[tv]` no tpk; webOS pela versão do firmware).
- **Nunca anunciar o que não foi provado:** começa conservador (o perfil atual) e cada item novo só entra com prova em TV.

| Backend | Contêiner | Vídeo | Áudio | HDR | Observação |
|---|---|---|---|---|---|
| LG webOS 4–5 (2018–20) | mp4, mkv, ts, mov | h264, hevc (10-bit) | aac, mp3, ac3, eac3 (DTS fora nos modelos 2020+) | HDR10, HLG; DV só MP4/TS (MKV DV dá tela preta, ver LEGACY-WEBOS) | uMediaServer |
| LG webOS 6–25 | + av1 (6+) | + av1, vp9 | + DTS/DTS-HD em modelos 23+ (só "specific models") | DV em MP4; MKV DV a provar | |
| Samsung .tpk Tizen 6+ | mp4, mkv, ts | h264, hevc, av1 (2020+?) | aac, ac3, eac3; **sem DTS desde 2018** | HDR10/HDR10+; **sem DV** | AVPlay/ESPlayer |
| Samsung .tpk 4/5 | idem, mais conservador | h264, hevc | aac, ac3, eac3 | HDR10 | |
| Samsung .wgt | — | — | — | — | **fora do escopo (D4)**: segue "indisponível nesta plataforma" |
| Android TV (Media3) | mp4, mkv, webm, ts | h264, hevc, vp9, av1 (por decoder) | aac, ac3, eac3, opus, flac; DTS/TrueHD **só passthrough** se a saída HDMI aceitar (consultar `AudioCapabilities`) | por `Display.HdrCapabilities` | perfil gerado em tempo de execução |

- **Legendas:** `External` para srt/vtt/ass/ssa; `Embed` para Direct play de MKV; PGS `Encode` sob demanda.
- **Teste de banda remota:** Jellyfin `/Playback/BitrateTest?size=…` uma vez por conexão remota, com cache de 1 h.

---

## 7. Desempenho e memória na TV
- **Nenhum pedido no fio de UI.** Continua o modelo atual de trabalhadores + `RedeJob` com geração. Um trabalhador de controle por servidor, um de check-in compartilhado e um de índice com prioridade baixa.
- **Orçamento de rede:**
  - Na abertura: só as fileiras visíveis (≤6 pedidos por servidor).
  - O índice entra depois de 5 s de ociosidade, em páginas de 200, com intervalo de 300 ms entre páginas.
  - Nada durante a reprodução (só check-in).
- **Imagens:**
  - Jellyfin/Emby: `maxHeight=…&quality=90&tag=` (anônimo, já existe).
  - Plex: `/photo/:/transcode?width=342&height=513&minSize=1&upscale=1&url=<thumb>`. **Ainda leva token**: o cache de texturas (`tex_cache`) precisa trocar a chave para uma sem token (hash de `servidor+thumb`), para o token não ir para o disco.
  - Tamanhos iguais aos do catálogo, para reaproveitar o `texfila`.
- **Memória:** índice ~2 MB / 20 mil itens. As fileiras ficam no snapshot atual (`JF_POR_FILEIRA` 24 → 40 com paginação "ver tudo"). Grade da visão: janela de 3 páginas.
- **Limites:** biblioteca de 50 mil+ itens → o índice fica com teto (`PX_INDICE_MAX` vira um ajuste por RAM do aparelho) e consulta pontual (Emby `AnyProviderIdEquals`; Jellyfin `searchTerm`+ano) para o resto.

## 8. Privacidade e segurança
- Tokens: arquivo separado `srvtok-p<N>-<tag>.txt` (0600). Nunca na conta, na telemetria, nos logs, na memória m3 nem nos pacotes (o glob de exclusão do `arm.sh`/`tpk.sh` já existe; estender para `srvtok-*`).
- **A lista de servidores não sai da TV (D1).** Nada vai para a conta Nuvio: nem nome, nem URL, nem usuário, nem token. Endereço de LAN e nome de servidor são dado pessoal. O logout da conta Nuvio continua apagando tokens (`servidores_esquecer_todos`). A lista sem token pode ficar, para facilitar reentrar (a confirmar na implementação).
- **Logs:** continuam operação/status/latência. Revisar se algum ponto novo imprime URL (decision, foto, legenda). Teste automático que procura `X-Plex-Token=`, `ApiKey=`, `api_key=` e `Token=` em toda a saída dos testes.
- **Redirecionamento:** não seguir redirect para outro host levando token (`p.seguir = 1` hoje: limitar ao mesmo host).
- **TLS:**
  - `plex.direct` tem certificado válido. TVs antigas podem falhar na cadeia ISRG/Let's Encrypt → tentar LAN http primeiro (já é a ordem).
  - Certificado autoassinado: aceitar só com confirmação explícita e "fixar" a impressão digital do certificado (TOFU). Nunca desligar a verificação globalmente.
  - webOS < 5 não confia na ISRG Root X1. O `rede.c` já tem mecanismo de CA próprio (`OPT_CAINFO` com `discordCa`). Dá para estender com um bundle que inclua a ISRG Root X1 para servidores atrás de proxy Let's Encrypt. Isso vale para o curl. O player da LG continua dependendo do sistema, então para tocar o preferido é LAN http.
- **Perfis:** a troca de perfil cancela os jobs (já existe). O índice e as fileiras de um perfil nunca aparecem em outro.

## 9. Testes
- **Unidade/fixture (host):** estender `tests/jellyfin.c`, `emby.c`, `plex.c` + os `*_server.py`:
  - ProviderIds e Guid → `srvidx`, regras de casamento (imdb/tmdb/tvdb/título+ano/episódio/especiais);
  - incremental (`MinDateLastSaved`/`updatedAt`); decision do Plex (directplay/copy/transcode);
  - `ApiKey` vs `api_key`; Jellyfin 12 recusando `api_key` (o servidor falso responde 401);
  - jornal `srvpend` (offline, última intenção, proteção de desmarcar em massa);
  - ordem das fontes "preferir local"; dedupe da busca;
  - ausência de token em log, cache e disco.
- **Integração no Mac** (Docker, `TMPDIR` no SSD):
  - `jellyfin/jellyfin:12.x` e `10.10.x` (as duas versões, por causa do auth);
  - `emby/embyserver`;
  - **Plex não entra no Docker (D10):** só fixtures em `tests/plex_server.py` + o servidor real do dono.
  - Biblioteca de teste com arquivos livres (Big Buck Bunny/Sintel/Tears of Steel em várias combinações de contêiner, codec, HDR e áudio, gerados com ffmpeg). Script `tools/servidores-docker.sh` sobe, popula e roda o app host contra eles.
- **9.3 Servidor real do dono (D5), gate de cada fase:** o dono tem Jellyfin/Plex de verdade.
  - Quando a implementação começar, **pedir a URL ao dono no chat** e fazer o login por QR/PIN na TV. **Nunca** gravar URL, usuário, token ou nomes de títulos pessoais em docs, commits, logs ou memória (m3/auto-memória).
  - Rodar o app na **LG C9** (deploy `arm.sh --alto-cache`, só quando o dono pedir a instalação) e na **TCL Android**.
  - Roteiro por fase:
    - **F0:** reproduzir no Jellyfin 12 (ApiKey) e no Plex (decision) um MKV HEVC e um com DTS.
    - **F1:** casar 10 filmes e 2 séries; fonte local em 1º; selo na ficha; QR.
    - **F2:** fileiras e visão.
    - **F3:** busca e biblioteca.
    - **F4:** histórico Trakt/conta/servidor sem duplicata.
    - **F5:** matriz de codecs.
  - Prova por captura real da TV (adb screencap na TCL, captura .bmp na LG) e linhas `[jellyfin]`/`[plex]` do log, sem URL.
- **TV (gate separado):** LG C9 (alto-cache), TCL Android, Samsung .tpk.
  - Matriz mínima: Direct play MKV HEVC 10-bit HDR10, MP4 DV, MKV DTS (deve virar DirectStream na Samsung), PGS (burn-in), legenda srt externa + AutoSync, troca de áudio, retomada e marcação nos dois sentidos.
  - Capturas reais (adb screencap / captura .bmp da LG), não simulador.
  - Por preferência do dono: só testar o que mudou.

---

## 10. Fases, ordem e estimativas
Estimativa em dias de desenvolvimento de um agente/dev que conhece a base, sem contar a prova em TV do dono.

| Fase | Conteúdo | Valor | Esforço |
|---|---|---|---|
| **F0 — Consertos e segurança** | `ApiKey=` no Jellyfin (12.0 quebrou o `api_key`; Emby continua com `api_key`/`X-Emby-Token`). Se alguma rota de stream recusar `ApiKey`, o plano B é proxy local em loopback que injeta o cabeçalho (o `media-http` do NuvioWeb faz algo parecido); Plex só oferece Direct play depois da decision (ou, no mínimo, filtra pelo perfil de codecs); `badges_detectar` no caminho de origem extra; não guardar "sem fonte" de servidor em cache; redirect só para o mesmo host; teste de "token em log". | Evita tela preta e 401 para quem já usa | **2–3 d** |
| **F1 — MVP "casar"** | `srvidx` (Jellyfin/Emby/Plex, disco + incremental, 1 servidor por tipo por perfil, D7); origem extra única "servidores" (libera o slot); fonte "Local · <srv>" **sempre em 1º** (D2) com selos reais; selo na página do título + "tocar do servidor"; QR para Quick Connect e PIN Plex; itens casados viram canônicos (D3). | Atende "aparecer na source" e o pedido nº 1 da comunidade | **10–12 d** |
| **F2 — Fileiras e visão** | CW principal com progresso do servidor fundido (D6); Próximos/Recentes por servidor só com itens casados (D9); `FIL_ORIGEM_SERVIDOR` no editor; "Ver tudo"; tela Visão do servidor (abas, grade, filtros, coleções, playlists); hero de recentes. | "Visão do server, criar fileiras" | **8–10 d** |
| **F3 — Busca e biblioteca** | Busca local no índice + busca remota; selo no resultado; aba "Meus servidores" na Biblioteca; selo nos Salvos; selo de episódio disponível. | "Nas buscas, na biblioteca" | **5–7 d** |
| **F4 — Assistido nos dois sentidos (ligado por padrão, D8)** | `srvpend`; pull de Played/posição; regra de conflito; anti-scrobble-duplo; "manter servidor em dia" ao assistir por add-on. | Confiança: nada se perde | **6–8 d** |
| **F5 — Reprodução avançada** | `srvperfil` por ano/modelo; troca de áudio e legenda com renegociação; legenda do servidor no pipeline (AutoSync/estilos); PGS sob demanda; banda remota; aviso de relay e de passe Plex. | Qualidade de imagem e som | **8–10 d** |
| **F6 — Multi-servidor e alcance** | N servidores por perfil (`srvconta`), migração dos arquivos atuais; descoberta na LAN (UDP 7359 / GDM); várias URLs por servidor (LAN/remota); Emby Connect; JWT do Plex (renovação de 7 dias). Sem sync de lista (D1) e sem WGT (D4). | Casas com vários servidores | **5–7 d** |
| **Prova com servidor real** (D5) | Roteiro §9.3 na LG C9 e na TCL Android, com o servidor do dono; Samsung .tpk quando houver TV | Gate | 1–2 d por fase |

**Total:** ~45–60 dias de desenvolvimento + prova em TV.

**Ordem de construção:** F0 → F1 → F2 → F3 → F4 → F5 → F6.
- F2 pode andar em paralelo com F1 depois que o `srvidx` existir.
- F6 (multi-servidor) fica para depois (D7). Para não refazer: desde a F1, o `srvidx`, as chaves de fileira (`srv:<tag>:…`) e o `srvpend` já são indexados por servidor (tag), não por tipo.

**MVP (F0+F1, ~3 semanas):**
- Quem tem Jellyfin, Emby ou Plex conecta com QR.
- Abre qualquer filme ou série do catálogo normal e vê "No seu servidor · 4K DV".
- A cópia de casa vem primeiro na lista de fontes, com selos corretos.
- Toca em Direct play quando a TV aguenta e em transcode quando não.
- O progresso volta ao servidor.

### Riscos
1. **Mudanças de API:**
   - Jellyfin 12 já quebrou o `api_key` e o `/emby/`, e muda o OpenAPI.
   - Plex migra para JWT de 7 dias (PIN com JWK Ed25519); token legado pode ser desligado no futuro.
   - Mitigação: fixtures por versão, testes contra Docker 10.10 e 12.x, e camada de auth isolada.
2. **Plex Remote Watch Pass:** fora de casa, clientes de terceiros podem exigir passe do dono ou do usuário (aplicado a partir do fim de 2025/2026). O app precisa mostrar o motivo, nunca contornar.
3. **Casamento errado:** ordem de episódios DVD vs aired, remakes com mesmo título/ano, especiais. Mitigação: ids primeiro; título+ano só com candidato único; "Não é este título" no cartão da fonte desfaz o vínculo.
4. **Codecs:** a capacidade real por TV é desconhecida (DV em MKV na LG, DTS só em alguns LG 23+, passthrough no Android). Começar conservador e liberar com prova.
5. **Desempenho:** bibliotecas enormes, TV com pouca RAM (C9) e servidor lento. Mitigação: teto do índice, páginas, disco, prioridade baixa.
6. **Token na URL do player:** é inevitável no LG e na Samsung. Fica em memória e nunca em log. Risco residual em dump de memória ou captura de rede na LAN.
7. **TLS em TV antiga:** cadeia Let's Encrypt e autoassinado. Mitigação: LAN http primeiro, TOFU com confirmação.
8. **Emby:** licença/Premiere pode limitar clientes de terceiros (Emby libera LG/Samsung/Android TV oficiais; terceiros não são citados). Validar com um servidor real.
9. **WGT fora (D4):** os usuários Samsung do .wgt não terão servidores. A mensagem "indisponível nesta plataforma" deve sugerir o .tpk.
10. **"Sempre primeiro" (D2) com servidor remoto lento ou relay de 1–2 Mbps:** o auto-play pega o servidor e a imagem fica ruim. Mitigação dentro da decisão: a ordem não muda, mas o cartão mostra "Remoto · relay ~2 Mbps", e uma falha de início (timeout) cai na próxima fonte sem loop.

---

## 11. Perguntas abertas
Nenhuma no momento. As perguntas do dono foram respondidas e registradas como D1–D10 no topo. Pendências técnicas a verificar estão no §9 de `servidores-locais-pesquisa.md`.

---

## 12. Como isso supera os outros
- **Nuvio oficial:**
  - **NuvioTV (Android TV):** PR **em rascunho** #3728 (aberto, não mesclado em 03/10/2026) traz Jellyfin 10.9+ e Emby 4.7+: fileiras, busca, aba Servidores, casamento com títulos dos add-ons como "fonte alternativa", direct play/transcode e progresso. **Não tem Plex**, não roda em LG/Samsung, e a fonte do servidor entra como "alternativa", não em primeiro.
  - **NuvioMobile e web (NuvioTVSmart/NuvioWeb):** nada nativo. Os pedidos estão abertos (NuvioTV#2617, #1913, #2217, #3003; NuvioMobile#1537, #1035, #938).
  - Os usuários dependem de pontes como add-on Stremio (Plexio, StreamBridge, stremio-jellyfin/Jellio+), que:
  - só entregam stream e catálogo genérico, perdem a visão do servidor e as coleções;
  - não sincronizam progresso;
  - guardam "sem fonte" em cache (NuvioTV#3175);
  - não deixam pôr o servidor em primeiro (#249 aqui).
- **Clientes Jellyfin/Plex puros (Swiftfin, Findroid, Plezy, Moonfin, apps oficiais):** têm ótima visão do servidor, mas não misturam com o catálogo, os add-ons, o debrid e o Trakt. O Infuse junta bibliotecas, mas não tem add-ons/debrid.
- **Nós:**
  - um único app em que o mesmo título tem a cópia de casa **e** as fontes da internet, com escolha automática inteligente;
  - selos reais de qualidade;
  - progresso unificado (servidor + conta + Trakt) sem duplicar;
  - fileiras e visão do servidor no padrão Glass (ilhas, Frost);
  - nativo em LG, Samsung (.tpk) e Android TV, inclusive TVs de 2018.
