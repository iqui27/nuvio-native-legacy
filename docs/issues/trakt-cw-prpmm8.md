# Trakt: "a seguir" fora do Continuar a Assistir e selo de visto so apos abrir (log PRPMM8)

Relato (Reddit, u/Habibi-1, app 2.0.5 Android): episodios "a seguir" do Trakt nao
aparecem no Continuar a Assistir; cartaz de serie so ganha o selo de visto depois
de abrir o titulo e voltar. Log analisado: `log-PRPMM8.txt` (1891 linhas, so lido,
sem rede; nenhum token/URL de addon copiado).

## Linha do tempo no log

- Conta Trakt conectada: acoes autenticadas funcionam —
  `[trakt] watchlist add tt33311069 (movie) -> confirmado (HTTP 201)` (linha 600) e
  `watchlist del ... confirmado (HTTP 200)` (274, 447).
- O log comeca ~12 s depois do arranque (`hero: foco/abertura` em `[t] 12253`) e
  NAO tem nenhuma linha do sync de arranque do Trakt: nem `[trakt] historico: N
  serie(s)...`, nem `[trakt] a seguir: ...`, nem `[trakt] N em andamento`. Limite
  do log, nao prova de falha; so da para afirmar o que acontece ao abrir titulos.
- Ao abrir o detalhe de uma serie, e so ali que o progresso por serie e lido:
  `[vistoep] tt9288030: consulta trakt progress/watched` (1110), 32 linhas
  `T1E1 ... T4E8: trakt completed=true`, e
  `[visto] marcado por trakt-importacao ... id=tt9288030 n=32 (sem reproducao local)`
  (1163). Idem `tt0981456` (1634). So `progress/watched` aparece 2 vezes no log todo,
  ambas disparadas pela abertura do titulo.
- Antes disso, `[cw] tt20234568:2:5 terminado sem lista de episodios: pedindo para achar o proximo`
  (144) registra a tentativa de avancar um episodio local, nao uma lista Trakt de up next.
  `[t] 49624 extras: parte trakt pronta` (229) pertence aos detalhes.
- O segundo detalhe importa 48 episodios de `tt0981456` (1698): junto dos 32 do
  primeiro, sao 80 episodios explicitamente importados por detalhes neste trecho.
- `[sync] progresso: 95 linhas, 0 aceitas, 0 no catalogo` (1777) e o sync da conta
  Nuvio, nao uma resposta de `/sync/playback`. As linhas de lista (275, 607–608)
  tambem sao da conta, nao o historico Trakt.
- Nenhum `/sync/watched/shows`, `/sync/playback` ou total de up next e registrado
  no trecho. Seus totais no arranque sao **desconhecidos**, nao zero; o pareamento
  tambem nao esta presente. As operacoes HTTP 200/201 confirmam uso autenticado
  posterior. O mecanismo de arranque foi confirmado pelo codigo e pelos testes.

## Causas

A. "A seguir" ausente. A lista de candidatas vinha so de `/sync/history?limit=100`
   (ultimas 100 reproducoes, no maximo 64 series em `ult[]`). Serie vista no app do
   Trakt (ou importada) e fora dessas 100 nunca virava candidata, entao
   `consultarProximos` (`/shows/<id>/progress/watched`) nem era chamado para ela.
B. Selo sem abrir. O mapa de "visto" de serie (`cat_visto`) so era escrito em
   `extras.c` ao abrir o detalhe (contadores aired/completed de progress/watched).
   O sync de arranque nunca lia contadores por serie, e `/sync/history` nao marca
   serie inteira (so episodios).

## Correcao (src/trakt.c)

- `carregarSeriesVistas`: `/sync/watched/shows?extended=noseasons` paginado (leve,
  sem mapa de episodios), junto do historico. Junta todas as series com IMDb que
  o historico recente nao cobriu, sem o limite anterior de 64 candidatas, em
  memoria dinamica. Temporada -1 = episodio desconhecido; o "proximo" so vem do
  `/progress/watched` (sem ele, sem palpite). Pagina invalida nao publica lista parcial.
- `fioProximo`: o mesmo GET de progresso grava o selo (`aired`/`completed` do
  cabecalho => serie vista), sem abrir o detalhe. O GET sai mesmo com o toggle
  "Proximo episodio" desligado; o card continua respeitando os toggles do Trakt no
  Continuar e os filtros de ocultar vistos.
- `descoberta.c` faz o sync mesmo com outra fonte de CW escolhida; descarta os
  cards Trakt nesse caso, preservando a escolha e atualizando o mapa dos cartazes.
  Series pausadas tambem recebem consulta de progresso; resposta positiva em
  cache reaplica o estado incompleto. Os filtros existentes continuam consumindo
  o mesmo mapa `cat_visto`.
- Teste: `bash tests/trakt_cw_vistas.sh` (rede falsa, sem detalhes nem historico
  local): episodio exato `tt2000002:1:2`, selo completo/incompleto, segunda pagina
  com 70 series e toggle desligado. A regressao alem da 64a serie falhou com rc 134
  no commit inicial `28d9a957` e passou apos remover esse limite.
