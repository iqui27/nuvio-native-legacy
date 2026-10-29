# Triagem das branches velhas (fora de integracao/2.0.5)

Base: `integracao/2.0.5`. Para cada branch, os commits únicos (`integracao/2.0.5..BRANCH`)
foram comparados com o código atual: arquivos novos checados por existência no integ,
identificadores e strings buscados com `git grep`. Só leitura: nada foi mergeado, portado
ou apagado.

Classes: **JA_DENTRO** (equivalente já no integ, com `arquivo:linha`), **OBSOLETO**
(código substituído/removido, ou experimento parado), **PORTAR** (útil e ainda faltando;
tamanho S/M/L e release sugerida), **DOC** (só plano/doc; diz se o doc já existe no integ).

Decisão do dono (14/10): `feat/*` ficam guardadas para 2.2. Para elas a triagem só anota
o que já está dentro do integ.

## Tabela

| Branch | Commits | Classe | Evidência / destino |
|---|---|---|---|
| agente/202-biblioteca | 7 | PORTAR L (2.1) | Biblioteca por proveniência, "Listas" por fonte, fila Trakt paginada. Integ tem a ordem antiga Trakt/Simkl/Nuvio/Fixadas/Públicas (`src/biblioteca.c:288`). Arquivos novos ausentes: `biblioteca_hub.inc`, `biblioteca_lista.inc`, `biblioteca_vis.inc`, `listas_rede.c`. |
| agente/203-318-play | 2 | JA_DENTRO (relator) + PORTAR S (#323) | Relator de queda Android <12 já está: `src/android.c:146` (`drenarNaQueda`), `src/android.c:171`, `src/queda.c:327`. A parte do `streams.c` é a mesma do 203-323. |
| agente/203-323 | 1 | NÃO PORTAR | `src/streams.c:1824` ainda tem `c.rodada = fa_rodada_atual()` com `c` na pilha no caminho de play único. Mas o caminho paralelo já não usa `c`: aloca `ConfDona` no heap com refcount (`src/streams.c:1866-1873`), `fonteparalela_soltando` + `soltarConf`. O `malloc` do branch não tem `free` correspondente na saída do erro. Como conserto do 2.0.6 regrediria o caminho com refcount: portar só a título de estudo. |
| agente/203-perfilsel | 1 | JA_DENTRO | Variante D2 ("continuar sob cada avatar") em `src/perfilsel.c:61` e `:585`; strings `T%d · E%d` e `Nada a continuar` nos `idioma_*.h`. |
| agente/2031-dvp5-hdr10 | 2 | JA_DENTRO (ao contrário) | Perfil 5 em MP4 na LG já vale nível 4 com HDR10 da faixa, de propósito: `src/streams.c:862` (`nivel = 4` quando `ajustes_dolby_vision() && telaDv && stream_e_mp4(s)`); `classeMp4` (`src/streams.c:1014`) já o põe acima do HDR10; `mp4PrimeiroNaFaixa` (`src/streams.c:1024`) não tem exceção HDR10. Ambas as peças existem, na direção inversa. `tests/fonte_lg_mp4.c:121` espera "MP4 DV Profile 5 2160p" para "LG, com MP4 primeiro: MP4 DV perfil 5 x MKV HDR10". Portar reintroduziria regra que o integ apagou e quebraria esse teste. |
| agente/204-arabe370 | 1 | DOC | Mexe em `docs/issues/MAPA.md` e `mapa.json` (#370, sincronismo). O rascunho `docs/issues/rascunhos/resposta-370.md` não existe no integ. |
| agente/204-pr381 | 2 | JA_DENTRO | Scratch merge de pr-381. `src/creditosauto.h` e `tests/creditosauto.c` já estão no integ; o ajuste equivalente é `ajustes_pular_creditos` (`src/ajustes.c:2250`). O commit extra só mexe em registro de ícone e asserção. |
| agente/204-samsung-dts | 1 | DOC | `docs/plans/samsung-dts.md` não existe no integ. Parte do conteúdo está em `docs/features/dts/README.md:75-79` e `docs/funcoes/streams.md:152` (#313). |
| agente/204-webos-versao | 1 | JA_DENTRO | Ordem nyx > starfish > acb já em `src/webosver.c` (lê `/var/run/nyx/os_info.json`) e `src/video.c:752-764`. |
| agente/ajustesvisual | 2 | OBSOLETO (parado) | Direções A/B (`ajVisual`, `AJV_A`) não estão no integ. `docs/ROADMAP.md:70` diz "sem direção aprovada". Nada a portar sem aprovação. |
| agente/biblioteca-ilimitada-plano | 1 | DOC | `docs/plans/biblioteca-ilimitada.md` não existe no integ. O "sem teto" já foi feito (`docs/funcoes/focus.md:113`, `5dc6eda1`). |
| agente/codex-contexto | 1 | DOC | `docs/glass-ui-contexto.md` não existe no integ. Contexto de UI Glass, sem código. |
| agente/guiaunicode | 1 | JA_DENTRO | Genre decodifica `\uXXXX` via `js_cadeia`: `src/guia.c:659-665`. |
| agente/icones | 5 | JA_DENTRO | Activity-alias por ícone em `android/app/src/main/AndroidManifest.xml:66-75`; `ajustes_icone_app` em `src/ajustes.c`. |
| agente/p2p | 6 | JA_DENTRO | Os 7 arquivos novos existem: `src/p2p.c`, `src/p2pmotor.c`, `tests/p2p*`. `NUVIO_P2P_MOTOR` em `tools/arm.sh` (4 ocorrências no integ). |
| agente/plugins | 14 | JA_DENTRO | Os 28 arquivos novos existem: `src/plugins.c`, `src/pluginjs.c`, `src/pluginsui.c`, `tests/plugin_rodar.c`, `tests/plugins_folha_shot.c`, `tests/pluginsui_shot.c`. O teto "origem extra 6 s" (`1a3dafcd`) está removido: `src/addons.c:2518` só dá `pthread_join` na origem extra, sem corte de 6 s. |
| agente/pr259-merge | 4 | OBSOLETO | Fallback DTS do PR 259 foi substituído pelo pipeline `src/dts/` (`dts_engine.c`, `dts_playback.c`). `NV_APP_ID` já está em `src/app_id.h`. |
| agente/rastreio | 6 | PORTAR M (2.1) | Ferramenta de processo (`tools/rastreio/`: grafo, guarda, falhas conhecidas). Não existe no integ; `tools/auditoria-release.sh` não tem o gancho. Não toca o app. |
| agente/refatoracao-plano | 1 | DOC | `docs/plans/refatoracao-geral.md` não existe no integ. |
| especial/amigo-tpk60 | 2 | OBSOLETO | Especial para uma pessoa (`NV_ESPECIAL_AMIGO`, `fil_especial_amigo`). Não existe no integ e não está em release. |
| experimento/dv-webos | 4 | NÃO OBSOLETO (vivo) | As flags `/tmp/nuvio-dv.conf` e `/tmp/nuvio-dv-forcar` continuam no integ (`src/video.c:2543`, `:2770`). Os 4 commits únicos mechem em `src/dts/dts_engine.c` e `src/dts/dts_playback.c` (171 inserções, 27 remoções) — trabalho de DV ainda em cima do pipeline `src/dts/`, não substituído por ele. Mantém o experimento; descartar como obsoleto perde trabalho vivo. |
| experimento/universo-2026 | 36 | OBSOLETO (experimento) | 0 de 32 arquivos novos existem. `docs/ROADMAP.md:108` diz "sem versão antes de separar"; revisão por patch fica para depois. |
| feat/ajustes-perfil | 5 | 2.2 (guardada) | Dentro: 0 de 8 arquivos novos (`perfilinfantil.c`, `admin.c`, `ajustes_escopo.c` ausentes). Só anota: nada dentro. |
| feat/colecoes-fileiras | 5 | 2.2 (guardada) | Dentro: 1 dos 5 arquivos novos (`src/colfileiras.c`; `collections_data.c` só existe em `tests/`). Os 3 commits de conteúdo seguem únicos e fora do integ: `c397f16d` (layouts), `0032f548` (i18n), `bbf286b7` (Vitrine). Nada a anotar como dentro além do `colfileiras.c`. |
| feat/guia-tv | 12 | 2.2 (guardada) | Dentro: nada (0 de 6 novos; `ltv.c`, `xmltv.c` ausentes). |
| feat/servidores-locais | 11 | 2.2 (guardada) | Dentro: nada (0 de 6; `srvidx.c`, `servidoresaddons.c` ausentes). |
| feat/sp-conquistas | 6 | 2.2 (guardada) | Dentro: só o servidor (`servidor/recomendacoes/src/conquista.js`, `migracao-013-conquista.sql`, `trilhas-dados.js`, `tools/trilhas-wikidata.py`). Cliente `caca*.c`/`conquista.c` ausente. |
| feat/sp-diario | 15 | 2.2 (guardada) | Dentro: nada (0 de 28; `diario*.c`, `dormiu*.c` ausentes). |
| feat/sp-trava | 4 | 2.2 (guardada) | Dentro: nada (0 de 10; `trava*.c` ausentes; `serie_trava` não existe). |
| feat/tizen4-coop | 8 | 2.2 (guardada) | Dentro: nada no código novo (0 de 11; `coop.c` ausente). `docs/ROADMAP.md:105` e `docs/releases/1.7.2/PLANO.md:86` já citam a branch como "preservar". |
| feat/vidaa | 10 | 2.2 (guardada) | Dentro: parte do servidor (`servidor/tv/src/index.js`, `servidor/tv/wrangler.toml`, `tools/vidaa-publicar.sh`). Cliente `fio1*.c`/`video_html5.c` ausente. |
| feat/watch-together | 16 | 2.2 (guardada) | Dentro: parte do servidor (`servidor/juntos/src/{index,sala,logica,celular}.js`, `servidor/juntos/wrangler.toml`, `tools/juntos-publicar.sh`). Cliente `juntos*.c` ausente. |
| fix/frost-ambiente-lg | 1 | OBSOLETO | WIP de agente "rescue uncommitted agent work (not reviewed)". Só harness de captura (`PERF_ALFA`, `PERF_BMP`), sem efeito no app. Base `NV_GLES_NO_MAC` já existe. |
| fix/player-retido-flaky | 1 | JA_DENTRO | Só teste. `tests/player_retido.c:38` já tem `esperar_catalogo()` (espera `desc_montando()` antes de `cat_definir`), chamado em `:45`. WIP não revisado: nada a portar. |
| fix/tpk-selo-hdr | 2 | JA_DENTRO | Fade da grade já usa `topo + alturaLinha() - gcy`: `src/biblioteca.c:2399`. Tela de protetor (`registerScreenSaverRequest`) já está em `src/`. |
| nacl/app-completo | 2 | OBSOLETO | Spike NaCl/PPAPI para Tizen 4/5 (`src/plat_nacl.c` ausente). Caminho substituído por tpk40/wgt. `docs/ROADMAP.md:105` já lista como spike. |
| perf/trim-fileira | 1 | JA_DENTRO | `onTrimMemory` já em `android/.../NuvioActivity.kt:268`, `src/tex_cache.c:793-816`, `src/tex_cache.h:228`, `tests/texpressao.c`. Desenho diferente, mesma função. |
| pr-489 | 18 | JA_DENTRO | Modos Exigir/Preferir em `src/ajustes.c:642`; "último link vale mais" em `src/fonteauto.c:33`; `src/regexjs.c` existe. O commit de workflow `build-tpk.yml.txt` é lixo. |
| pr-490 | 1 | JA_DENTRO | `ajustes_ocultar_filmes_vistos` e `desc_filme_visto` existem (`src/ajustes.c`, `src/descoberta.c`). |
| pr-491 | 44 | PORTAR M (2.0.6, verificar) | Parte já dentro: VC-1/MPEG-2 ficam no original (`src/dts/dts_remux.c:230`). Falta o resto dos commits: watchdog, buffer de início, Annex B do HEVC, FFmpeg com zlib, build Tizen 6/6.5/7. Sem detalhe linha a linha. |
| pr-493 | 1 | PORTAR L (2.1) | Reset de sessões de perfil e isolamento de fios. `src/perfisessao.h` ausente; `src/addons.c` do integ usa outro desenho (`listaTrava`, `progTrava`, `versaoLista`). 123 arquivos. |
| pr-493-novo | 1 | PORTAR L (2.1) | Mesmo commit de pr-493 (`4cc89807`). Duplicata. |
| webos3 | 7 | JA_DENTRO (fix de addons) + pendente (arte) | Fix `uses_primary_addons` em `src/perfis.c:268`; `NV_SDLCOMPAT_H` em `src/sdlcompat.h`. Os commits de arte/teto de textura (`312d2202`, `28ac6906`) não foram verificados. |
| agente/205-dicatoque | 1 | JA_DENTRO | Dicas do rodapé clicáveis (`aj2DicaOk`, `aj2DicaVoltar`) e ligação com `ponteiro_alvo` já em `src/ajustes_ux_v2.inc:513-528`; teste em `tests/ajustes_ux_toque_modo.c`. Nada a portar. |
| agente/205-bench-mm | 1 | JA_DENTRO | Medidor some com trailer: `src/desempenho.c:33,40`; chamado em `src/main.c:1922` via `trailer_medidor_esconde()`. |

## Pendências

- Verificar linha a linha: pr-491 (DTS Tizen 6+), webos3 (arte).
- `agente/2031-dvp5-hdr10` e `agente/203-323` ficam fora do 2.0.6: o integ já resolve os dois de outro jeito.
- `experimento/dv-webos` não está morto (flags `/tmp/nuvio-dv-*` vivas em `src/video.c`, 4 commits com código em `src/dts/`).
- Commits WIP "rescue uncommitted agent work (not reviewed)" (fix/frost-ambiente-lg) precisa de revisão antes de qualquer porte.
- Nada foi portado nem apagado. feat/* seguem para 2.2.
