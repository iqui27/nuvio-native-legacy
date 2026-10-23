# #205 — vistos sem reprodução: continuação da investigação

## Escopo e resultado

Continuação das alterações não commitadas da execução interrompida, sobre `772cd83b` (já contém `4afa7b93`). Nenhuma escrita/leitura autenticada na conta real, nenhum push e nenhum stash. Os testes usam dados temporários e dublês; os testes públicos de Apple/IMDb só consultam trailers.

**Foram reproduzidas e corrigidas duas brechas no cliente C. Não foi provado que elas geraram todos os lotes do histórico real.** A coincidência de títulos e minutos não substitui um log dos POSTs responsáveis.

## Evidência lida

- `/Volumes/ExternalSSD/tmp/ev412/trakt-historico-dono.txt`: em 10/10/2026, grupos de filmes e episódios entre 08:01 e 09:10 UTC; `action=scrobble`. Em 11/10, episódios de Halt and Catch Fire de temporadas diferentes. O histórico não informa neste arquivo qual aparelho/cliente emitiu cada POST.
- `log-C9-67214.txt:88,134,180,208`: **resolução** de trailers de Law & Order, Hunters, Instinct e Bull. Não são logs de `trailer_abrir` nem prova de que esses trailers tocaram; não há stop desses títulos nesse trecho.
- `log-C9-67214.txt:381,390,404,406,410,447,454`: start de Dexter Resurrection S1E2, pipeline perto de 99 s, atividade `fim ... 17s`, seek para 2734 s, depois saída a 2768 s / 2843 s e stop 100%. **A posição final é real, mas 2768 s não foram assistidos continuamente nessa sessão.** O seek explica esse stop sem precisar de trailer. O log registra depois a desmarcação manual do episódio.
- `log-TCL-70826.txt:489,525,554,566,684,711,738,745`: duas sessões de The Invite retomadas a ~30%, com pause a 30,9% / 31,3%; nenhum stop no arquivo. Preparo termina depois da primeira saída sem abrir uma fonte por conta própria.

## Caminhos até a escrita

### 1. Posição/seek sem prova de tempo assistido (reproduzido)

Antes desta continuação, `fecharSessao` exigia `comVideo`, pronto, duração >=120 s, não canal e duração não suspeita contra o catálogo. **Não exigia reprodução real nem vínculo com a fonte atual.** Uma posição de fim/seek era suficiente:

```
player_abrir / player_definir_fonte
  -> player_atualizar lê video_pos / video_duracao
  -> player_encerrar -> fecharSessao
  -> concluiuAgora arredonda pos para dur
  -> cat_salvar_progresso_ep
  -> trakt_marcar -> trakt_scrobble(SCR_EV_SAIU)
  -> scrobble_decidir: >=90% => SCR_STOP
  -> scrPend -> fioScrobble -> enviarScrobble
  -> POST /scrobble/stop
```

O mesmo encerramento também chama `visto_episodios` ou `visto_titulo` quando concluiu: Simkl e jornal da conta. Referências após a correção: `src/player.c:1733,1795,1822,1835,1843,1874,1877`; `src/trakt.c:2036,2051,2082`; `src/traktscrobble.c:26`.

Sem minutagem de catálogo, 69/129 s eram arredondados para 100% pela folga de conclusão, mesmo sem tempo tocado. O caso novo falha no HEAD anterior (assert de nenhum destino/progresso) e passa corrigido. Outro caso simula seek até 2525/2580 s sem avanço contínuo e não grava.

### 2. Pipeline substituída por trailer (reproduzido como interleaving)

`src/trailer.c:381` usa o mesmo `video_tocar` nativo. Se uma sessão principal ainda existente perde a pipeline para o trailer, o player antigo apenas verificava `comVideo` (estado da abertura antiga) e `video_ativo/pronto` (estado global do trailer). Copiava posição/duração do trailer; o fechamento executava a cascata acima.

O teste usa **trailer_abrir real**, backend falso, trailer de 180 s e player ainda aberto. Falha no HEAD anterior e passa corrigido. Também cobre `video_terminou -> trailer_atualizar -> trailer_fechar`, inclusive a mesma URL e valores antigos preservados pelo backend. Isto prova a vulnerabilidade desse interleaving, **não prova que a navegação normal das TVs do dono fez esse interleaving**.

Agora `fontePrincipal` (`src/player.c:343`) exige a URL capturada pelo player, ausência de trailer e a mesma geração de trailer (`src/trailer.c:397,456`). Abrir um trailer invalida permanentemente a prova da sessão anterior, mesmo depois de fechá-lo. O player só lê posição/duração dessa fonte principal (`src/player.c:3278`); o fallback de animação não avança uma pipeline substituída.

Uma série sem T/E recebe T1E1 em `src/player.c:765-766`. Isso explica **como** uma sessão indevida poderia carregar esse identificador, não prova que a sessão indevida foi aberta. S4E1 pode vir do alvo/CW escolhido; não foi encontrada uma promoção automática de S3 para S4 que emita stop sem player.

## Hipóteses e os lotes

| Hipótese | Resultado no código e nos arquivos fornecidos |
|---|---|
| (a) Home/cartaz/detalhe termina trailer e fecha player | Trailer fecha por `trailer_fechar` (`src/trailer.c:421,564`), que só para vídeo e limpa estado. Não chama player/vistos/progresso. A pipeline compartilhada é uma brecha **se** houver sessão principal coexistente; teste reproduz essa condição. Navegação sozinha não mostrou caminho direto. |
| (b) FJOB_PREPARO/pre-busca abre player | `src/app.c:1158-1178,1548-1574`: preparo guarda índice/resultado e só entrega quando há Play pendente (`aguardandoFonte==2`). Não toca pipeline nem fecha sessão sozinho. Busca de metadata/trailer só resolve mídia. |
| (c) pendência Trakt reenviada no sync de 5 min | **Descartada no cliente C.** `scrPend` (`src/trakt.c:2010`) é uma única vaga em memória, último pedido vence; trabalhador envia imediatamente, limpa antes do POST, sem persistência nem retry de erro. Não é fila de 40 títulos nem timer de 5 min. |
| (c) jornal da conta | `sync.c:1185` reenvia `contapend` a cada ciclo (`sync.h:95`: 300000 ms). Agrupa vistos em RPC `sync_push_watched_items` (`contapend.c:392`), **não** `/scrobble/stop`. Pode explicar lotes na conta, não `action=scrobble` no Trakt por este C. Não inventa marca: recebe gesto manual ou conclusão do player. |
| (d) clipe curto/fonte com duração falsa | <120 s continua barrado. Duração incompatível com catálogo continua barrada por `4afa7b93`; catálogo desconhecido não fornece essa prova. Acrescentada prova de tempo real antes da conclusão. Sem catálogo e com duração incorreta mas plausível, ainda não se pode identificar a obra somente pelo relógio. |
| (e) endOfStream nativo do trailer | webOS recebe evento em `video.c:1310`, mas `eventoPayload` já rejeita sessão antiga (`video.c:938`). Trailer fecha sem scrobble. Fim sobreposto a player aberto/substituído é coberto pela nova guarda e teste. TPK tem `ativo` no callback C sem selo, mas o host atual já verifica sessão depois do Prepare e nos callbacks (`tizen-tpk/Video.cs:400-440`); isso não prova a revisão instalada anteriormente. |

**Não há uma sequência comprovada que explique os lotes de 3–8 scrobbles a cada 5–15 minutos.** O timer de conta não pode ser atribuído ao endpoint Trakt por semelhança de horários. As marcas manuais usam `/sync/history`, não stop; imports Trakt/conta só atualizam os espelhos locais, não scrobblam.

## Política aplicada

- Progresso automático: fonte principal, duração real/finita >=120 s, sem erro, >=5 s de avanço confirmado.
- Visto/stop automático: as mesmas condições **e mídia tocada >= max(120 s, metade do que faltava quando a fonte começou a tocar)** (`scrobble_assistiu`, `src/traktscrobble.c`). O ponto salvo da retomada conta como já assistido. Na conclusão pelo cartão (créditos/próximo), o trecho entre a posição e o fim só é creditado quando os segundos **reais** já provam o trecho da retomada até a saída (a mesma regra, com a saída no lugar do fim): a cauda nunca preenche o mínimo de 120 s. Sem essa prova a **posição real** continua gravada (barra, Continuar assistindo, conta); só o arredondamento para 100%, o visto e o stop ficam de fora, e o selo é barrado por um "não visto" no histórico local (só quando não há histórico conhecido e a posição passa de 90%). Não confundir avanço de seek com tempo tocado.
- Medição: `CLOCK_MONOTONIC`, avanço do pipeline, limitado pelo tempo decorrido; amostra inválida em pausa/buffer/seek/espera/erro; intervalos >2 s descartados. Amostras discretas de ~200 ms na LG não perdem todo o tempo por serem lidas a 60 fps.
- Reinicia prova a cada fonte/load; retomada retida mantém o acumulado da mesma fonte, mas reinicia a amostra para não contar a pausa.
- A entrada de `trakt_scrobble` valida tempo/duração antes de alterar `scrPend`. Teste cobre pedidos inválidos com trabalhador parado e com POST real em voo. Saída recusada zera `scrEstado.ativo`: a próxima abertura manda `/scrobble/start` de novo (pause acima de 80% o Trakt recusa, então não há chamada a mandar; o "assistindo agora" antigo expira ou é trocado pelo start seguinte).
- Gestos explícitos de “Já assisti” continuam válidos sem reprodução; são origem `*-manual`, não prova automática.
- Logs `[visto] marcado por <origem> pos=… dur=… assistido=…s`: conclusão do player, stop Trakt, detalhe/menu/desfazer/episódios/agenda, imports e reenvio do jornal. Import/reenvio diz explicitamente que não é reprodução local; a prova real permanece na linha original do player. Encerramento sem prova também informa IMDb/T/E e o motivo numérico.

**Segunda revisão (bloqueada, corrigida):** a primeira versão exigia 90% da duração em tempo de relógio. Isso desfazia o #100 — aceitar o próximo episódio no marcador de créditos (88,9%), pular a abertura ou tocar a 1,25x nunca concluía — e uma saída acima de 90% sem prova não gravava nem a posição. A prova agora só segura o "seek até o fim" do log do C9 (133 s tocados, saída a 2768 s): conta **mídia** tocada (a 1,5x o título inteiro conta inteiro, teto de 2x), exige metade do trecho que faltava e credita a retomada. Limite assumido: quem toca metade e busca o fim marca; duas aberturas (seek até 85%, sair, reabrir e ver o resto) também. Nada tenta apagar automaticamente os vistos já existentes da conta do dono.

## Validação

- Red/green em cópia temporária criada com `git archive HEAD`, sem alterar o worktree: teste sem runtime falha no HEAD anterior; teste isolado de `trailer_abrir` sobre player aberto também falha. Ambos passam com a correção. Adaptação apenas do protótipo do contador Trakt à ABI anterior.
- `player_visto_trailer`: filme/episódio realmente tocados concluem; abandono inicial grava progresso; seek, pausa, buffer, preparo sem fonte, troca/fim de trailer não marcam. Inclui runtime desconhecido e amostra LG em degraus a 60 fps.
- Rodados: player_fonte_duracao, progresso/progresso_serie, visto, vistoep/vistoep_corrida, syncprog, trakt_scrobble/trakt_contracts/trakt_filmes_vistos, simkl_cw, contapend/contalib, trailer-fonte/native-state/proxy/recorte/serie/hero-contract, home-trailer-timer, ajustes_trailer_fonte, trailerapple-midia, trailercinema, trailerimdb e trailerapple.
- `trailer-hero-contract`: falha idêntica no HEAD anterior, assert “OK no cartao sem fonte toca o YouTube ... Samsung” (`tests/trailer-hero-contract.cjs:364`); detalhe não foi alterado. Não mascarada nem corrigida fora do escopo.
- `trailerapple`: primeira tentativa falhou por timeout público em play-edge.itunes.apple.com; repetição passou. Não usa conta do dono.
- `SANITIZE=1 player_visto_trailer`: **não validado neste ambiente**. A instrumentação agora é aplicada de fato (o antigo array `pure` não era usado). O binário ficou preso antes de `main`, no inicializador de `libSDL2-2.0.0.dylib -> dllinit -> error_dialog`, confirmado por `sample` em `/tmp/nv205-sanitize-sample.txt`; encerrado com SIGTERM. O teste normal passou com a mesma lógica e relógio isolado só no player. Não se afirma ASan/UBSan verde.
- Logs de execução desta investigação: `/tmp/nv205-*.log`; resultados agregados `/tmp/nv205-suite-results`, `/tmp/nv205-final-results` e `/tmp/nv205-extra-results`.

## Dado que falta pedir

Um **registro contínuo do aparelho durante um lote falso**, do foco/abertura do primeiro título até depois do sync seguinte (idealmente 15 min), não só a sessão anterior ou o último buffer de log. Deve incluir:

1. modelo/build/revisão e identificador de sessão (sem tokens);
2. abertura/fechamento de player e trailer, IMDb/T/E, identidade/geração da fonte (URL pode ser redigida);
3. sourceInfo/PRONTO, playing, currentTime, seek/seekDone, endOfStream;
4. novas linhas `[visto]` com tempo real e linhas `[trakt] stop ... -> HTTP`;
5. ciclo/RPC de conta no mesmo intervalo, em UTC correlacionável;
6. se aparecer visto no Trakt sem POST dessa TV, identificar o **cliente/aplicativo conectado que gravou aquele histórico** (auditoria/leitura, sem revogar acesso ou modificar histórico), incluindo outros aparelhos/app web.

Só esse cruzamento permite atribuir os lotes ao C, a outra versão/aparelho ou a outro cliente, em vez de assumir uma fila que não existe neste código.
