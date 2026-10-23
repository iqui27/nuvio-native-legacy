# PR #491 — restante por commit

Base: `integracao/2.0.5`. Fonte: `git log --reverse integracao/2.0.5..pr-491` (44 commits, incluindo três merges).

A triagem antiga não viu a importação `2e19c071` (head `c2b1d895`, sem `.github`). A remoção deliberada `e242d848` descartou planos de sessão, laboratório, opções de entrega de teste e listener LAN; esses itens continuam descartados. Os patches foram comparados ao código atual e às reversões posteriores no PR, sem merge da branch antiga.

**IN**: comportamento final já presente; uma parte histórica pode ter sido revogada, indicada na evidência. **MISSING**: parte aplicável ausente na base, portada nesta branch. **OBSOLETE**: revertido/superado ou material temporário removido. As classes descrevem a base antes deste porte.

| Commit | Subject | Classe | Evidência / destino |
|---|---|---|---|
| `65025c14` | docs: plan for DTS/TrueHD to AC3 5.1 conversion on NuvioTpk | OBSOLETE | Plano de sessão removido deliberadamente por e242d848; documentação vigente em docs/features/dts/README.md. |
| `c179130c` | docs: plan: drop ESPlayer (Partner-only) and DV; add video-request-file test method | OBSOLETE | Revisão do mesmo plano removido por e242d848. |
| `67f97b5f` | docs: brief for architecture session on DTS/TrueHD to AC3 | OBSOLETE | Brief de sessão removido por e242d848. |
| `b1f7d255` | docs: DTS/TrueHD to AC3 design (loopback HLS, on-demand segments) and PC test lab | OBSOLETE | Design e laboratório temporário removidos por e242d848; motor preservado. |
| `6a988a9f` | Merge branch 'iqui27:master' into docs/design-dts-truehd-ac3-tizen | OBSOLETE | Merge de sincronização com master; nenhum porte de merge necessário. |
| `1200a8d1` | Merge branch 'iqui27:master' into docs/design-dts-truehd-ac3-tizen | OBSOLETE | Merge de sincronização com master; nenhum porte de merge necessário. |
| `5324c664` | docs: re-check DTS/TrueHD design against upstream 2.0.5 (TS proxy now live on .tpk) | OBSOLETE | Revisão do design temporário removido por e242d848. |
| `1c8f99d8` | tpk: FFmpeg 7.1.5 for DTS/TrueHD -> AC3 in the 6+ libnuvio.so (S1) | IN | tools/build-dts-ffmpeg.sh alvo tpk e tools/tpk.sh prefix-dts/NV_DTS_FFMPEG (2e19c071). |
| `ae07cc48` | tpk: keep the FFmpeg muxers on (S1 fix) | IN | tools/build-dts-ffmpeg.sh: muxers habilitados depois de --disable-everything. |
| `d612430a` | dts: remux core for the Samsung .tpk (S2) | IN | src/dts/dts_remux.c e .h: sonda, índice, geradores e saída AC3. |
| `6ebb0be1` | dts: loopback server for the converted stream on the .tpk (S3) | IN | src/dts/dts_hls_tpk.c: servidor loopback e sessões; endurecido posteriormente. |
| `b3a71f9c` | tpk: play DTS/TrueHD through the converter; Settings and log endpoint (S4) | IN | src/video_tpk.c: convPossivel/convBombear, áudio convertido e AJ_DTS_AC3; superfície de debug obsoleta por e242d848. |
| `d112144b` | dts: S5 hardening of the .tpk loopback converter | IN | Cancelamento/interrupção, prazos e liberação em dts_engine.c/dts_remux.c/dts_hls_tpk.c; reforçados por 6e4401bd/b689e1dc. |
| `4f6c3e60` | Create Jd | OBSOLETE | Arquivo .github/workflows/Jd apagado no próprio PR por 70714b0e. |
| `0576faa0` | Add files via upload | OBSOLETE | Workflow com extensão .yml.txt apagado no próprio PR por 98c640f7. |
| `70714b0e` | Delete .github/workflows/Jd | IN | Arquivo Jd ausente, como pede a exclusão. |
| `83c07428` | Add files via upload | MISSING | Workflow excluído da importação 2e19c071; portado nesta branch sem copiar secrets, com contents: read. |
| `98c640f7` | Delete .github/workflows/build-tpk.yml.txt | IN | Arquivo build-tpk.yml.txt ausente, como pede a exclusão. |
| `88bf0119` | tpk: convert DTS/TrueHD in MP4 sources; MKV without Cues falls back to TS | IN | dts_engine_open_demux: MP4; dts_hls_tpk.c semIndice e video_tpk.c fallback HLS→TS para MKV sem Cues. |
| `8f1d15db` | tests: stub the DTS converter in the .tpk fake-host tests | IN | tests/dts_tpk_stub.c usado nos testes fake-host tpk-escolha/roi/aspecto. |
| `4b590fcc` | tpk: keep ASS/SSA/SRT subtitles right while DTS is converted | IN | src/video_tpk.c: legendas do original preservadas durante conversão; tests/tpk-dts-legendas.c. |
| `39655c29` | Merge docs/design-dts-truehd-ac3-tizen: the Build TPK workflow | OBSOLETE | Merge do workflow; conteúdo rastreado nas linhas 83c07428/cd26d52a/c2b1d895. |
| `cd26d52a` | ci: cache the DTS FFmpeg build in the Build TPK workflow | MISSING | Cache prefix-dts com hash do script e .carimbo; portado junto ao workflow. |
| `c6ab47be` | tests: subtitle menu while DTS/TrueHD is converted on the .tpk | IN | tests/tpk-dts-legendas.c e tests/tpk-dts-legendas.sh. |
| `d56db643` | tpk: write the FFmpeg stamp inside the container (CI: Permission denied) | IN | tools/tpk.sh: .carimbo gravado dentro do container após build completo. |
| `50047368` | dts: read the source over one long connection on the .tpk | IN | src/dts/dts_engine.c: read_fluxo/fx_worker e rede_fluxo64 em src/rede.c; conexão sequencial e anel. |
| `d17969bd` | dts: a converted reopen is a seek for the app, not a source opening (.tpk) | IN | src/video_tpk.c: convReabrir/reabrindo; src/player.c e tests/tpk-dts-reabrir.c preservam posição e estado. |
| `dc6afccd` | dts: convert DTS/TrueHD next to VC-1 and MPEG-2 video too (.tpk) | OBSOLETE | Conversão VC-1/MPEG-2 revertida por 8a0158a0; videoCodecOk mantém original. |
| `12defa01` | dts: convert the DTS/TrueHD track when it is the only one in the preferred language (.tpk) | IN | src/video_tpk.c: trilhaPreferida decide áudio pelo idioma; tests/tpk-dts-reabrir.c caso English DTS/Japanese AAC. |
| `27690ba9` | dts: continuous TS by default; HLS gets a master playlist; log page shows dts lines (.tpk) | OBSOLETE | TS por padrão revertido por e7819f0f; log LAN removido por e242d848. Master HLS preservado em dts_hls_tpk.c. |
| `e7819f0f` | dts: HLS by default again; 24.000 fps sources switch to the continuous TS (.tpk) | IN | HLS por padrão em src/video_tpk.c; exceção 24 fps revogada por feb7a661. |
| `e39061c4` | dts: start buffer for the converted stream on slow sources (.tpk) | OBSOLETE | Buffer de início removido pelo próprio PR em de5ea5d4; não ressuscitar pausa automática. |
| `e59c1b34` | player: cover the black while the converted stream reopens (.tpk) | IN | src/video_tpk.c: video_tpk_capa/reabrindo; src/player.c cobre reabertura. |
| `7037122d` | dts: start buffer counts nothing ahead while HLS still sends seg0 (log showed -3934 s) | OBSOLETE | Correção da contagem de buffer removida junto ao buffer por de5ea5d4. |
| `57eda3de` | dts: right Dolby Vision profile in the log, GOP line, start buffer waits for the HLS seek (.tpk) | IN | src/dts/dts_remux.c: perfil Dolby Vision e diagnóstico GOP; espera de buffer obsoleta por de5ea5d4. |
| `dc13db0f` | dts: evenly spaced video DTS in the converted stream (.tpk) | IN | src/dts/dts_remux.c: emitVideo/lastDts, passo DTS uniforme. |
| `feb7a661` | dts: drop the 24.000 fps -> TS rule for now (the DTS fix may be what cured it; Round 2 retest on HLS) | IN | src/dts/dts_hls_tpk.c não força TS por framerate de 24 fps. |
| `7933f59a` | dts: no start buffer on the continuous TS (.tpk) | OBSOLETE | Restrição de buffer ao HLS superada pela remoção total em de5ea5d4. |
| `28171252` | dts: review fixes for the .tpk conversion (watchdog, cover, refused pause, GOP log race) | IN | video_bufferando_ms limita reabertura a 25000 ms; video_tpk_capa até alvo; GOP atomic_int e contadores por gerador. Pausa recusada obsoleta com o buffer. |
| `b63965b7` | tpk: FFmpeg (DTS) with zlib, for zlib-compressed Matroska tracks | IN | tools/build-dts-ffmpeg.sh: --enable-zlib após --disable-autodetect no alvo tpk; tools/tpk.sh já liga -lz. |
| `0ad51173` | dts: HEVC to Annex B keeps the stream's own parameter sets (Interstellar glitches) | IN | src/dts/dts_remux.c: dts_remux_hevc_annexb preserva parameter sets próprios e insere só os ausentes. |
| `de5ea5d4` | dts: remove the start buffer (.tpk) | IN | Sem folgaVigiar/entregue_ate/emitted_s; tests/tpk-dts-reabrir.c verifica ausência de pausa automática em HLS e TS. |
| `8a0158a0` | dts: VC-1 and MPEG-2 video stay on the original again (.tpk) | IN | src/dts/dts_remux.c: videoCodecOk só aceita H264/HEVC; VC-1/MPEG-2 ficam no original. |
| `c2b1d895` | dts: conversion on Tizen 6/6.5/7 too (NuvioTpk60/65); CI builds all three 6+ packages | MISSING | Runtime já IN: tizen>=6, anel 16 MiB em 6/6.5/7. Faltava CI dos NuvioTpk60/65/NuvioTpk; portado nesta branch. |

Contagem: 26 IN, 15 OBSOLETE, 3 MISSING.

Porte mínimo: `.github/workflows/build-tpk.yml` derivado de 83c07428, cd26d52a e c2b1d895, com créditos Namer03 e Claude. Build dos três pacotes 6+, cache estático e cache FFmpeg por hash; não copia `LOCAL_PROPERTIES` de secrets. O runtime, zlib, Annex B e watchdog já estavam dentro. Buffer de início está OBSOLETE por de5ea5d4, com teste explícito da remoção.

Prova dirigida: `bash tests/pr491-tpk-build.sh` **FAIL rc 1** antes do porte (workflow ausente), **PASS rc 0** depois. O teste verifica seleção de pacotes, chamada do builder, artefatos e cache concluído de FFmpeg. Não executa build ARM nem Actions; validação em TV não foi feita.

Checks adicionais: `bash tests/tpk-dts-reabrir.sh` rc 0 (inclui HLS/TS sem buffer de início), `bash tests/tpk-dts-legendas.sh` rc 0, `bash tests/tpk-dts-servidor.sh` rc 0 (cancelamento, Range/HEAD, teto de conexões e teardown), `python3 tools/varredura-i18n.py` rc 0 (zero ocorrências nas três categorias). Workflow parseado como YAML e todos os passos `run` conferidos com `bash -n`.

`bash tools/mac.sh` rc 0 com `TMPDIR=/Volumes/ExternalSSD/tmp/jobtmp`, `NUVIO_MAC_SO_COMPILAR=1` e `NUVIO_MAC_BIN=/Volumes/ExternalSSD/tmp/jobtmp/pr491-mac` exportados (compilação apenas, binário isolado). O aviso de configuração de servidor incompleta é do ambiente local; não impediu a compilação. Nenhum push ou deploy.
