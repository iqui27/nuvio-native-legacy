# Servidores locais — STATUS

Branch `feat/servidores-locais` (a partir de `origin/master`). Plano: `servidores-locais-plano.md`; pesquisa: `servidores-locais-pesquisa.md`.
Nada aqui foi provado em TV nem em servidor real. Tudo abaixo foi provado só contra os servidores falsos de `tests/*_server.py`.

## F0 — consertos e segurança (feito)

| Item | Commit | Prova |
|---|---|---|
| Jellyfin 12: `ApiKey=` nas URLs do player (Jellyfin 10.8+ e 12.x); `api_key=` só no Emby e no Jellyfin < 10.8. O `api_key=` que o servidor põe no `TranscodingUrl` é renomeado. | `313a6054` | `tests/jellyfin.sh`: o servidor falso lê o token da query por versão (12.0 recusa `api_key`, 10.7 só lê `api_key`); o teste abre cada URL sem cabeçalho, como o player da TV. |
| Plex: Direct play só quando a tabela de codecs da TV **e** o `/video/:/transcode/universal/decision` aprovam. DTS/TrueHD/PGS nunca viram Direct play; a versão recusada ganha a entrada HLS ("Direct stream" quando a TV abre o vídeo). | `f1a4225e` | `tests/plex.sh`: versão DTS sem decision e só como HLS; servidor recusando a versão 1. |
| Redirect só para o mesmo host (`RedePedido.mesmo_host`) em Jellyfin/Emby/Plex. | `e84c5208` | `tests/rede_pedido.sh` (`/otherhost`). |
| Selos (`badges_detectar`) também nas fontes casadas; título que o servidor tem nunca entra no cache de fontes (`servidores_tem`). | `1afc60a7` | `tests/plex.sh`, `addonurl`, `addonslista`, `fontecache_vod`. |
| Teste que varre o log de todos os testes de servidor atrás de token. | `541dc806` | `tests/servidores_log.sh`. |

### TLS autoassinado (servidor pessoal)
`RedePedido.tls_pessoal` **não está no master**: está em `agente/202-issue290` (`4843e5b0`, "accept self-signed certificates on the configured personal server only"). Não foi duplicado aqui. Ao juntar, o conflito esperado é pequeno: `rede.h` (campo novo ao lado de `mesmo_host`), `rede_pedido.inc` (laço de redirect) e o `pedir()` de `jellyfin.c`/`plex.c`.

### Precisa do servidor real do dono
- Jellyfin 12.x: confirmar que `ApiKey=` abre `/Videos/{id}/stream`, `master.m3u8`, as playlists filhas e os segmentos HLS, e as legendas externas. No falso só a rota principal é conferida.
- Plex: confirmar o formato do `X-Plex-Client-Profile-Extra` (`add-direct-play-profile(...)`) e que o `directPlayDecisionCode` vem como no falso. Se o PMS ignorar o perfil, o pior caso é HEVC/MKV cair em Direct stream (toca, mas converte).
- Docker do Jellyfin não foi usado: o Docker deste Mac guarda imagens no disco interno, não no SSD.

## F1 — casar com o catálogo (feito, só com servidores falsos)

| Item | Commit | Prova |
|---|---|---|
| `srvidx`: índice por perfil e servidor (IMDb → TMDB → TVDB → título+ano só com candidato único; episódio por série + T/E), em disco (`srvidx-p<N>-<jf\|em\|px>.txt`, só ids), incremental com checagem de contagem. | `31692dc2` | `tests/srvidx.sh` (regras, títulos, disco, 30 mil linhas). |
| Jellyfin/Emby: índice por `/Items` (ProviderIds, DateLastSaved, `MinDateLastSaved`), episódios sob demanda, `*_consultar`/`*_tem`; itens das fileiras com IMDb/TMDB viram o título canônico. | `3fd1d01f` | `tests/jellyfin.sh`, `tests/emby.sh` (incremental, apagado → reconstrução, troca de perfil, logout apaga o índice). |
| Plex no mesmo `srvidx` (Guid imdb/tmdb/tvdb, `updatedAt>>`, allLeaves sob demanda, título+ano). | `9ac1733e` | `tests/plex.sh`. |
| Uma origem só para os servidores; fonte "Local · <servidor>"/"Remoto · <servidor>" com selos reais, **sempre primeiro** (folha e automático, sem teto de qualidade; lembrada de addon não passa na frente); selo "No seu servidor · <servidor> · 4K …" na página do título; QR do Quick Connect e do PIN do Plex; atualização a cada 30 min; índice fora dos pacotes. | `9025bfa9` | `tests/emby.sh` (fachada), testes de addons; build Mac sem aviso novo; `-DNV_TPK` nos arquivos tocados; build Android OK. |

### Decisões de implementação
- "Tocar do servidor": o Reproduzir já toca a cópia de casa (ela é a 1ª fonte e ganha o automático). Não há botão separado.
- Itens sem IMDb/TMDB continuam nas fileiras com id opaco; o filtro D9 (só na Visão do servidor) fica para a F2, junto com a tela.
- Progresso/assistido para Trakt/conta de itens casados: tocado pela fonte do servidor, o título já é canônico e segue o caminho normal; o anti-duplo (D3/F4) não foi feito.

### Precisa do servidor real do dono (não verificado)
- Jellyfin 12: `ApiKey=` em todas as rotas de mídia (HLS filho, segmentos, legendas).
- Jellyfin/Emby: `MinDateLastSaved` e o campo `Height` na listagem; chaves de ProviderIds reais.
- Plex: formato do `X-Plex-Client-Profile-Extra` e do `decision`; filtro `updatedAt>>=`; `videoResolution` na listagem.
- QR do Quick Connect: a rota `<base>/web/#/quickconnect` na versão do dono; URL > 134 bytes fica só com o código.
- Nada foi visto em TV: selo na página, cartão "Local ·", folha e QR precisam de captura real (LG C9 / TCL).
