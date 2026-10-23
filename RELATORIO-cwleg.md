# FASE 2 — CW e legendas AIOStreams

Worktree `/Volumes/ExternalSSD/nv-205-cw-progresso`; branch `agente/205-cw-progresso`; base `40a2f768b446a5d5a86a626e88c9c946ab2fac3c` (`integracao/2.0.5`). Ponytail full: sem dependencias novas, helpers pequenos nos modulos existentes. Rodada 1 abaixo preservada como historico; na rodada 2 este relatorio passa a ser versionado conforme solicitado.

## Commits separados

- `bbbcab5216e56adc0a98a3ab0f72a462071438a8` — `fix: preserve paused progress before next-up suggestions`
- `d6cb246a969c7e03e4274f4a8fc4c0b8f21e2799` — `fix: merge subtitle extras without losing existing choices`

## Resultado e pontos alterados

**CW:** OFF e aplicado na origem das sementes e antes do merge; pausados validos precedem sugestoes, que so ocupam lugares restantes. Mesma obra exige prova de conclusao do episodio posterior ao progresso para uma sugestao elimina-lo. Recencia e preservada dentro dos grupos; futuros continuam separados nos modos correspondentes. Nenhuma limpeza de historico remoto.

- `src/trakt.c:402`: consulta de prova do mesmo episodio, pelo `watched_at` lido, estritamente posterior ao progresso.
- `src/trakt.c:1453`: limites de progresso antes de admitir/deduplicar/cortar playback; `:1516` deduplica durante a leitura, evitando gastar o teto com repetidos.
- `src/trakt.c:1525`: conclusao posterior remove playback obsoleto; `:1542` e `:1555` nao geram sugestoes com OFF; `:1575` substituicao restrita a sugestoes; `:1599` pausados antes das sugestoes, recencia por grupo.
- `src/trakt.h:102`: contrato da consulta de prova.
- `src/simkl.c:148`: limites antes do teto; `:504` respeita OFF antes da disputa; `:511` nao substitui pausados; `:530` ordena grupos.
- `src/descoberta.c:2714`: desempate por obra com prova posterior, inclusive caso inverso (pausado contra sugestao).
- `src/descoberta.c:2809` e `:2875`: OFF antes da deduplicacao e depois da reconciliacao local; `:2918` conta nao semeia com OFF; `:2977` conta so troca sugestoes entre si.
- `src/descoberta.c:3068`, `:3076`, `:3106`: aplica o mesmo desempate a Trakt/Simkl/conta.
- `src/descoberta.c:3147`: eliminada a escrita automatica `trakt_progresso_ocultar(...,0)` ao reexibir oculto local. A montagem agora nao dispara POST; a remocao local continua funcionando.
- `src/descoberta.c:3167`, `:3195`, `:3232`: grupos antes da ordenacao/corte; reserva de futuros somente nos lugares restantes depois dos pausados.
- `src/cwordem.c:12`, `src/cwordem.h:15`: particao estavel pausados/sugestoes/futuros; recencia dentro de cada grupo.

**Legendas:** segunda busca soma ao final da lista, sem mover nem sobrescrever entradas atuais; dedup por URL ou id+indice do addon. Vazio, erro HTTP e pseudo-legenda mantem a primeira lista. Erros `error.*` sao descartados antes do filtro de idioma e logados uma vez por resposta, inclusive sem URL. A escolha ativa nao e resetada.

- `src/addons.h:165`: id e origem para dedup; `:176` documenta mesclagem.
- `src/addons.c:1008`: predicado unico de duplicatas, usado no lote, distribuicao e mesclagem.
- `src/addons.c:1082`: publicacao com extras so no fim; `:1090` acrescenta sem mover indices.
- `src/addons.c:1138`: descarta/avisa `error.*`, sem contar como legenda; `:1156` impede entrada no lote; `:1168` conserva identidade e origem.
- `src/addons.c:1865`: mesmo titulo mescla inclusive se a primeira busca ainda roda; outro titulo continua com lista propria.
- `tests/addons_legendas_merge.c:1`, `tests/addons_legendas_merge.sh:1`: regressao real com rede falsa; 8 boas + so-erro => 8; +2 novas/1 repetida => 10; mesma URL/id+addon, vazio e NULL; identidade, URL e indice escolhidos pelo seletor real permanecem.
- `tests/addons_legendas_prazo.c:57`: acesso de teste ao seletor real para confirmar escolha ativa, sem alterar producao.

## Testes antigos alterados — justificativa de cada arquivo

- `tests/cwordem.c`: ordem global por recencia e ordem de futuros por estreia contrariavam prioridade de pausados e recencia por grupo. Esperados passam a verificar os grupos; testes puros de corte/estreias continuam.
- `tests/cwordem_desc.c`: guarda ja entregue para injetar OFF/ON; dublê de prova posterior. Esperados antigos de Brothers e conta exigiam expulsar pausados por sugestoes; agora exigem preservar os 12. Fileira curta usa recencia dentro do grupo futuro. Episodio concluido que semeia proximo fica depois de pausado. Acrescentada prova posterior/anterior/ausente e conta com 10 pausados + 2 lugares livres, para garantir que sugestoes ainda entram.
- `tests/cwdesmarcado.sh`: 4 logs passam a 3 porque OFF descarta a sugestao ANTES de ajustar desmarcacao. As assercoes de conteudo continuam intactas.
- `tests/simkl_cw.c`: esperado antigo tomava o lugar do anime pausado pela sugestao; agora preserva todos os pausados. Acrescentados ajustes falsos e caso OFF; cache de atividades, credenciais e escritas falsas continuam verificados.
- `tests/trakt_arranque.c`: ordem global por recencia substituida por pausado preservado + recencia entre sugestoes; contagem, cache e mapa continuam verificados.
- `tests/stub_fichameta.c`: ajustes padrao fracos para testes que compilam Trakt isolado; diagnostico sobrescreve OFF. Nenhuma expectativa relaxada.
- `tests/cwdesmarcado.c`, `tests/cwdup244.c`, `tests/cwremover.c`, `tests/montagem_cedo.c`: apenas dublê da nova consulta de prova, retornando desconhecido; nenhuma assercao alterada.
- `tests/addons_legendas_prazo.c`: apenas acesso adicional de teste ao seletor real. `tests/addons_legendas.c` e suas expectativas nao foram alterados.
- `tests/cw205_diagnostico.c/.sh`: testes ja entregues incluidos no commit CW; corrigida a assercao de identidade que era fixa em Ted Lasso mesmo no cenario MESMA_OBRA (ela so era alcancada depois do conserto). Acrescentadas conclusao anterior/posterior e OFF.

## Prova antes/depois

Base extraida por `git archive 40a2f768` em `/tmp/nv-cwleg.SLamAn`, sem checkout/stash da worktree. Nessa copia entraram somente os testes de diagnostico entregues e o novo teste de mesclagem. Testes de regressao compilam funcoes reais com rede falsa; nenhum pedido a Trakt/Simkl/servidor real. Testes HTTP de legendas usam apenas servidor local 127.0.0.1. Scripts existentes executados com `bash`; capturas que exigem prefixo tambem executadas com prefixo explicito.

Os seis cenarios exigidos de CW passam, incluindo os tres que falhavam. O teste novo de legendas aborta na base na primeira assercao de preservacao (rc 134) e passa depois. Nenhum teste com rc 0 na base regrediu na rodada comparativa. Logs integrais locais: `/tmp/nv-cwleg-logs/before/`, `/tmp/nv-cwleg-logs/final/`.

| Teste (bash tests/... salvo comando explicito) | rc base | rc final | Resultado |
|---|---:|---:|---|
| `CW205_CONCLUSAO=1 bash tests/cw205_diagnostico.sh` | 134 | 0 | PASS |
| `CW205_PROXIMO=1 CW205_CONCLUSAO=1 CW205_CONCLUSAO_ANTIGA=1 bash tests/cw205_diagnostico.sh` | 0 | 0 | PASS |
| `CW205_PROXIMO=1 CW205_CONCLUSAO=1 bash tests/cw205_diagnostico.sh` | 134 | 0 | PASS |
| `addons_aviso_legal.sh` | 0 | 0 | PASS |
| `addons_diferenca.sh` | 0 | 0 | PASS |
| `addons_legendas.sh` | 0 | 0 | PASS |
| `addons_legendas_merge.sh` | 134 | 0 | PASS |
| `addons_paralelo.sh` | 0 | 0 | PASS |
| `addons_sync_ui.sh` | 0 | 0 | PASS |
| `addonslista.sh` | 0 | 0 | PASS |
| `cw205_diagnostico.sh` | 1 | 0 | PASS |
| `cw205_diagnostico.sh CW205_MESMA_OBRA=1 montagem` | 1 | 0 | PASS |
| `cw205_diagnostico.sh CW205_PAUSADO_MAIS_NOVO=1` | 0 | 0 | PASS |
| `cw205_diagnostico.sh CW205_PROXIMO=1 montagem` | 1 | 0 | PASS |
| `cw205_diagnostico.sh CW205_SEM_HISTORICO=1` | 0 | 0 | PASS |
| `cw205_diagnostico.sh montagem` | 0 | 0 | PASS |
| `cw_pedidos.sh` | 0 | 0 | PASS |
| `cwdesmarcado.sh` | 0 | 0 | PASS |
| `cwdup244.sh` | 0 | 0 | PASS |
| `cwfrente.sh` | 0 | 0 | PASS |
| `cwinicio.sh` | 1 | 1 | falha preexistente |
| `cwlocal.sh` | 0 | 0 | PASS |
| `cwminimo.sh` | 0 | 0 | PASS |
| `cwoculto.sh` | 0 | 0 | PASS |
| `cwordem.sh` | 0 | 0 | PASS |
| `cwremover.sh` | 0 | 0 | PASS |
| `cwretido.sh` | 1 | 1 | falha preexistente |
| `cwthumb.sh` | 0 | 0 | PASS |
| `home-trailer-timer.sh` | 0 | 0 | PASS |
| `home.sh` | 0 | 0 | PASS |
| `home_hero_layout.sh` | 0 | 0 | PASS |
| `home_progressiva.sh` | 0 | 0 | PASS |
| `homecarga_ilha.sh` | 0 | 0 | PASS |
| `homecarga_painel_shot.sh` | 1 | 1 | falha preexistente |
| `homeestado.sh` | 0 | 0 | PASS |
| `homejanelas.sh` | 1 | 1 | falha preexistente |
| `homelayouts_shot.sh` | 0 | 0 | PASS |
| `homepos.sh` | 0 | 0 | PASS |
| `legauto.sh` | 1 | 1 | falha preexistente |
| `legauto_shot.sh` | 1 | 1 | falha preexistente |
| `legauto_shot.sh (prefixo captura)` | 0 | 0 | PASS |
| `legconta378.sh` | 0 | 0 | PASS |
| `legenda2.sh` | 0 | 0 | PASS |
| `legenda_ass.sh` | 0 | 0 | PASS |
| `legenda_ass_shot.sh` | 134 | 134 | falha preexistente |
| `legenda_embutida_tamanho.sh` | 0 | 0 | PASS |
| `legenda_negrito_shot.sh` | 0 | 0 | PASS |
| `legenda_tamanho.sh` | 0 | 0 | PASS |
| `legendas_shot.sh` | 1 | 1 | falha preexistente |
| `legendas_shot.sh (prefixo captura)` | 0 | 0 | PASS |
| `legendasui.sh` | 0 | 0 | PASS |
| `legendasui_425.sh` | 0 | 0 | PASS |
| `legextras.sh` | 0 | 0 | PASS |
| `legmem_383.sh` | 0 | 0 | PASS |
| `legmemoria.sh` | 0 | 0 | PASS |
| `legref.sh` | 0 | 0 | PASS |
| `legref_fixtures.sh` | 0 | 0 | PASS |
| `legsync.sh` | 0 | 0 | PASS |
| `legsync_shot.sh` | 134 | 134 | falha preexistente |
| `player_visto_trailer.sh` | 0 | 0 | PASS |
| `simkl_cw.sh` | 0 | 0 | PASS |
| `stream_parser.sh` | 0 | 0 | PASS |
| `trakt_arranque.sh` | 0 | 0 | PASS |
| `trakt_contracts.sh` | 0 | 0 | PASS |
| `trakt_cw_dup.sh` | 0 | 0 | PASS |
| `trakt_ep_cinemeta.sh` | 0 | 0 | PASS |
| `trakt_play_corrida.sh` | 0 | 0 | PASS |
| `trakt_scrobble.sh` | 0 | 0 | PASS |

Falhas preexistentes preservadas: `cwinicio` chama `prog_gravar_local` com assinatura antiga; `cwretido` tem simbolos de textura ausentes; `homejanelas` tem dubles incompletos; `homecarga_painel_shot` falta `dts_overlay_draw`; `legauto` falta `fontesmortas_idade`/`fontevolta_esquecer`; `legenda_ass_shot` aborta apos capturas; `legsync_shot` aborta esperando LEGSYNC_DESFEITA. Os scripts `legauto_shot`/`legendas_shot` sem argumento retornam 1 por `$1` ausente; as linhas com “prefixo captura” mostram a execucao correta. Nao foram corrigidas falhas anteriores.

## Build e limites

| Comando | rc antes | rc final | Evidencia |
|---|---:|---:|---|
| `bash tools/android.sh` | nao executado na base | 0 | BUILD SUCCESSFUL; APK debug e release; arm64-v8a e armeabi-v7a conferidas |
| `NUVIO_SSH_OPTS="-o ProxyJump=none" bash tools/arm.sh` | nao executado | nao executado | script instala/copia/lanca na TV por padrao; omitido conforme instrucao |
| `git diff --check` | — | 0 | sem erros de whitespace |

Build final: `/tmp/nv-cwleg-android-final.log`, rc registrado em `/tmp/nv-cwleg-android-final.rc`; APKs em `build/android/Nuvio-2.0.5-android-debug.apk` e `build/android/Nuvio-2.0.5-android.apk`. O script encontrou a configuracao local de assinatura e gerou tambem release; nada foi publicado ou instalado. Nenhuma execucao de push, gh, stash, instalacao em TV ou escrita em servico remoto.

Riscos/limites: sem validacao em TV, por instrucao. Teto existente de 12 CW e 36 legendas preservado; com lista de legendas cheia, novas entradas nao expulsam as atuais. Identidade do addon usa seu indice na lista da sessao; IDs de legenda guardados ate 255 bytes, limite maior que os IDs usados na reproducao. A prova Trakt consulta o ultimo episodio do historico lido (janela existente de 100 eventos/64 series); ausencia de prova mantem o pausado, sem inferir conclusao pelo numero do proximo episodio. Simkl `next_to_watch` isolado nao confirma conclusao de um pausado da mesma obra: este permanece ate haver prova local/Trakt. Reexibicao de ocultos agora e somente local: oculto remoto nao e desfeito automaticamente. Na rodada 1, avisos `error.*` eram texto do addon no log; a rodada 2 abaixo elimina essa exposicao. Testes de rede falsa nao comprovam disponibilidade atual do AIOStreams/OpenSubtitles.

## Rodada 2 — tres achados da revisao

Revisao de origem: `/Volumes/ExternalSSD/tmp/rev/REV-cwleg.md`; reproducoes em `/Volumes/ExternalSSD/tmp/rev/cwleg-evidencias/`. Primeiro foi executado `git merge integracao/2.0.5`: merge `922c6cc0`, sem conflitos, trazendo poster #390 e seekr. Os arquivos `src/posterprov.c`, `src/posterprov.h` e `tests/poster_modelo_longo.c` continuam identicos aos da integracao: aliases `{imdb_id}`, `{tmdb_id}` e `{id}` e suas assercoes preservados. Nenhuma reversao desses recursos.

### Correcoes minimas

1. **Trakt e teto de pausados:** o historico e aguardado antes de percorrer o playback; a prova de conclusao posterior do mesmo episodio e aplicada antes de registrar/admitir o candidato. O leitor continua ate obter `max` pausados validos. Os GETs iniciais continuam disparados em paralelo; apenas o parse aguarda o historico. Sem buffers novos ou segundo percurso.
2. **Fallback com OFF:** a copia de `cat_copiar_fileira` e compactada com a preferencia atual, mantendo apenas itens em andamento quando “A seguir” esta desligado. Os pausados preservam identidade, progresso e ordem.
3. **Log de pseudo-legenda:** `error.*` continua descartado e avisado, mas o aviso usa texto fixo (`erro na resposta de legendas`); nenhum `lang`/`id` bruto e interpolado. URL, token, configuracao e quebras de linha do upstream nao chegam ao diagnostico.

### Provas FAIL -> PASS em tests/

- `tests/cwleg_corte.c` adapta diretamente `repro_corte.c`, com include relativo e o mesmo cenario de 13 playbacks. OFF antes: rc 1, 11 pausados; ON antes: rc 1, 11 pausados + 1 sugestao. Depois: 12 pausados, zero sugestoes e `tt7000011` presente em OFF e ON.
- `tests/cwleg_fallback.c` adapta `repro_fallback.c`, incluindo verificacao dos dois pausados e de seus percentuais. Antes: rc 1, OFF com 5 cards/3 sugestoes. Depois: 2 cards/zero sugestoes, `tt1` a 40% e `tt2` a 60%.
- `tests/cwleg.sh` compila as funcoes reais com os dubles de rede existentes e executa as duas reproducoes; `corte`/`fallback` permitem provas separadas.
- `tests/addons_legendas.c` incorpora `repro_log.c` no worker real: erro com URL/token em `lang`, mais erro sem `lang` com URL/configuracao no `id`; ambos incluem tentativa de injetar linha. `tests/addons_legendas.sh` amplia a verificacao de ausencia de vazamento e exige que o aviso continue presente. Antes: rc 1 por vazamento; depois: rc 0.
- `tests/addons_legendas_merge.sh` exigia literalmente os dois textos brutos dos erros. Foi ajustado para exigir dois avisos e rejeitar conteudo upstream; as assercoes de 8/10 legendas, duplicatas e selecao ativa continuam intactas. A primeira rodada ampla registrou rc 1 por essa expectativa antiga; a execucao final passa.

Os FAILs foram medidos depois do merge e antes das correcoes de producao. A prova ON anterior do corte foi executada tambem contra `src/trakt.c` extraido de `922c6cc0`, sem reverter a worktree. Logs `fail-corte.log`, `fail-corte-on.log`, `fail-fallback.log`, `fail-legendas.log`, `pass-cwleg.log` e `pass-legendas.log` em `/Volumes/ExternalSSD/tmp/rev/cwleg-rodada2/`.

### Bateria solicitada e verificacoes finais

41 comandos/cenarios, com **39 rc 0 e 2 falhas preexistentes** no resultado final. Scripts com mais de uma etapa cobrem tambem os binarios internos (Home, montagem, menu). Resultados completos: `/Volumes/ExternalSSD/tmp/rev/cwleg-rodada2/resultados-finais.txt`; runner: `rodar.py` no mesmo diretorio. O resultado final substitui apenas o rc inicial de `addons_legendas_merge` pelo reteste apos atualizar sua expectativa.

| Grupo / comando | Resultado |
|---|---|
| `cw205_diagnostico`, `cw_pedidos`, `cwdesmarcado`, `cwdup244`, `cwfrente`, `cwleg`, `cwlocal`, `cwminimo`, `cwoculto`, `cwordem`, `cwremover`, `cwthumb` | PASS |
| `cwinicio` | rc 1: chamada antiga de `prog_gravar_local`, com argumento ausente; ja registrado na rodada 1 |
| `cwretido` | rc 1: dubles ausentes de `marco`, `tex_obter_larg` e `tex_obter_logo_larg_qualquer`; falha de link preexistente |
| Diagnostico CW: Trakt ON; montagem OFF/ON; mesma obra OFF; conclusao posterior OFF/ON e anterior ON | Todos PASS |
| `addons_legendas`, `addons_legendas_merge` (reteste final) | PASS |
| `SANITIZE=1 bash tests/cwleg.sh` | PASS, ASan/UBSan nas duas reproducoes |
| `SANITIZE=1 bash tests/addons_legendas.sh` | PASS, ASan/UBSan e ausencia de vazamento |
| `poster_modelo_longo`, `posterprov` | PASS, incluindo aliases, aviso e log redigido da integracao |
| `seekr`, `seekr_estados`, `seekr_pacote`, `seekr_recorte`, `seekretry`, `seekrquota`, `seekrquota_pipeline`, `seekrquota_settings` | PASS |
| `seekr_vivo` sem chave pessoal | PASS: validacao recusa chave ficticia e lookup HTTP 401 resulta em chave recusada |
| `seekr_shot` sem chave pessoal | rc 0: caminho “sem SEEKR_API_KEY, nada a fazer”; nenhuma captura real gerada |
| `i18n` | PASS |
| Extras: `trakt_cw_dup`, `trakt_arranque`, `simkl_cw` | PASS |
| `git diff --check` | PASS |

`cwinicio` e `cwretido` nao tiveram expectativas alteradas; seus testes e interfaces envolvidas permanecem iguais ao merge `922c6cc0`. Sem consertos fora dos tres achados. A compilacao do app inteiro feita por `poster_modelo_longo` e `seekr_shot` concluiu. Esta rodada nao refez APK/IPK.

Limites: sem validacao em TV; `seekr_shot` nao validou miniaturas com credencial real, apenas compilacao e caminho sem chave. O teste vivo usou exclusivamente chave ficticia e diretorio de dados temporario. Sem escrita Trakt/Simkl, push ou publicacao. Este relatorio, antes ignorado por `/*.md`, e incluido explicitamente no commit da rodada 2 conforme solicitado.
