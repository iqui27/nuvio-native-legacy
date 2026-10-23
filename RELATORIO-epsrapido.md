# Episodios sem espera pelo addon

## Revisao P1 para 2.0.6

Mescla `integracao/2.0.5`: `ea22b6be`, 204 commits incorporados sem conflitos.
Os testes precisaram de dubles das APIs novas de manifesto/lista/watchstate (`502db1c`).
Depois disso, `epsrapido`, `detalheanime` e `episodiosdup`: rc=0.

- P1.1 (`142bfca`): conserva todos os pares publicados, atualiza os comuns e acrescenta novos. Teste deterministico com conjuntos divergentes nas duas ordens verifica pares, posicao, texto e abas. Antes rc=134; depois ASan/UBSan rc=0. Validacao de listas de arquivos preservada.
- P1.2 (`2b45f26`): `epsParciais` distingue publicacao antecipada de mescla concluida. Voltar a A permite retomar sem apagar a lista visivel; garantir/precarregar seguem a mesma regra. Rede falsa com Cinemeta bloqueado por condicao prova A/B/A e conclusao sem pedido repetido. Antes rc=134; depois ASan/UBSan rc=0. O campo muda sizeof(CatItem), invalidando o cache binario antigo pela regra existente.
- P1.3 (`172ef81`): deduplicacao e carregando comparam o ID atual com o ID do pedido sob epPedidoTrava. Teste deterministico reutiliza o indice de A para B e depois move B para outro indice. Antes rc=134; depois ASan/UBSan rc=0.
- P1.4: geracao, resolucao final do alvo e escritas de episodios/ficha/abas compartilham epPedidoTrava, inclusive na cauda TMDB e no servidor pessoal. Rede e parse ficam fora da trava. Teste com pthread/condicao tenta invalidar entre validacao e escrita: final e antecipada; verifica tambem a recusa de pedido ja invalidado. Ambos falham sem a correcao (rc=134) e passam depois. Sem memoria para mesclar, conserva lista/abas visiveis.

Validacao final: `SANITIZE=1 bash tests/epsrapido.sh`, `detalheanime.sh` e `episodiosdup.sh`: rc=0, sem diagnostico ASan/UBSan. `git diff --check`: rc=0. Sem push, deploy ou aparelho.

Na branch `agente/205-epsrapido`, sem push, publicacao, TV ou stash.

`src/descoberta.c`: para series/itens de tipo incerto com IMDb no caminho de ficha do addon, o pedido Nuvio/Cinemeta roda em paralelo. A primeira lista real publica episodios e abas imediatamente. A ficha continua preferindo o addon; quando as duas respostas chegam, a lista maior conserva os episodios e recebe nome/sinopse do addon. A republicacao conserva a ordem dos pares ja presentes e acrescenta novos pares ao fim. Listas de arquivos continuam sujeitas a validacao de temporadas/cobertura.

Cada pedido leva uma copia do item e uma geracao. Abrir B inicia seu pedido imediatamente e invalida A; A pode terminar a transferencia e guardar o cache, mas abandona a publicacao. Nao ha cancelamento forcado de pthread/curl. Cache em memoria/disco, chaves e TTL permanecem iguais. Toda rede continua fora da thread principal. IDs sem IMDb continuam usando o mapeamento existente antes do complemento.

## Prova com rede falsa

`bash tests/epsrapido.sh` reutiliza os dubles de detalhe existentes. Antes da alteracao, falhou por ausencia de episodios dentro do limite de 2 s. Depois: Cinemeta 1 s/addon 4 s publicou 20 episodios em aproximadamente 1,01 s; aos 4 s conservou os 20 com texto do addon nos 13 comuns. B, aberto 200 ms depois de A, publicou em aproximadamente 1,01 s a partir da propria abertura; A nao publicou. Ordem inversa tambem passou: addon publica 13 em 1 s, Cinemeta amplia a 20 sem mover os pares anteriores. ASan/UBSan passou.

## Regressao

Passaram: `detail_layout`, `detail_remonta`, `detail_eps_repede`, `detail_eps`, `detail_notas`, `vistoep`, `vistoep_corrida`, `cwleg`, `cateps`, `cateps_corrida`, `abrirtitulo`, `detalheanime`, `episodiosdup`, `desc_iniciar_corrida`, `descdebounce`, `cwordem`, `cwremover`, `metaprov`, `manifesto_cache`, `manifesto_wait`, `trakt_ep_cinemeta`, `fileiras_android`, `montagem_cedo`, `posterprov`.

Duas expectativas antigas em `detalheanime`/`episodiosdup` exigiam reduzir a lista ao addon menor; atualizadas para conservar os episodios do Cinemeta, conforme solicitado. Testes de layout/foco existentes passaram; nenhuma verificacao em aparelho foi feita. Sem dependencias novas.
