# Paridade Matis94 — reprodução webOS

Snapshot: `8221bb897110271313db2864d2f0d15d1857b61b`, em `/Volumes/ExternalSSD/nv-21-int`. Comparação com as 11 releases de [releases.jsonl](releases.jsonl). Inspeção estática; testes citados foram lidos, **não executados**. Presença não comprova reprodução em TV nem presença na versão pública 2.0.8. Linhas referem-se ao snapshot.

O fork anuncia principalmente comportamento **LG/webOS**. Implementação Samsung/Android TV não comprova paridade webOS.

| Feature consolidada | Origem | Estado | Evidência e diferença material |
|---|---|---|---|
| P7 → P8.1, base HEVC preservada, RPU convertido, EL/FEL descartada | 2.0.5–2.0.8.1 | Presente | `src/dts/dv7.h:38`, `src/dts/dts_engine.c:989`, `tools/arm.sh:155`. Não reconstrói FEL. Nosso default OFF (`src/ajustes_ux_padrao.inc:345`), diferente do primeiro teste do fork. |
| Seletor experimental de mapping P7 | 2.0.8.1 | Ausente | `tools/dv7/src/lib.rs:20` usa `convert_with_mode(2)` fixo. Apenas liga/desliga em `src/ajustes.c:1938`. |
| Dolby Vision MP4 experimental | 2.0.8.1 | Ausente | `src/video.c:3118` exclui MP4 do caminho de conversão DV. Demux MOV/MP4 ou reprodução MP4 nativa não equivalem à opção anunciada. |
| Saída de áudio DV fixa: Original / AAC / Dolby Digital | 2.0.8.1 | Ausente | `src/dts/dts_engine.c:1015` mantém AC-3/E-AC-3 e converte DTS. `src/ajustes.c:1783` define saída da conversão DTS, não saída fixa para todos codecs DV. |
| TrueHD → AAC/AC-3 preservando DV e aviso próprio | 2.0.8–2.0.8.1 | Ausente no webOS | TrueHD fora dos elegíveis em `src/dts/dts_engine.c:965`. `src/video.c:3158` troca por AC-3/E-AC-3 do mesmo idioma ou retorna ao nativo. Decoder TrueHD incluído só para TPK (`tools/build-dts-ffmpeg.sh:26`). |
| Consultar DTS na TV, aguardar resposta e indicar DTS nativo | 2.0.8.7.test2 | Presente | EDID, espera, fallback após 2s e reuso no processo em `src/video.c:687`; routing em `:2555`; aviso `src/player.c:7007`. ABI do callback ainda sem prova física (`src/video.c:698`). |
| Conversão DTS Off, preservando preferências antigas e DV com AC-3/E-AC-3 | 2.0.8.7 | Parcial | Original e migração em `src/ajustes.c:4096,5356`. Evita conversão standalone proativa, mas DTS ainda é convertido dentro do pipeline DV; fallback por áudio incompatível permanece (`src/video.c:2570`). Não é proibição global. |
| DV + DTS-HD MA/DTS:X integral nativo com conversão Off | 2.0.8.7.test2 | Ausente | Pipeline DV sempre decodifica DTS (`src/dts/dts_engine.c:1015`); adapter aceita AAC/AC-3/E-AC-3, não DTS (`src/dts/adapter/starfish.cpp:197`). EDID sozinho não entrega passthrough integral. |
| Startup/resume DTS e recuperação limitada de gaps/descontinuidades | 2.0.6.2–2.0.8.1 | Presente | Recuperação com orçamento de duas tentativas (`src/dts/dts_playback.c:584`); alvo pendente preservado (`src/video.c:3217`). Sem prova da ausência de tela preta em TV. |
| Retry de ranges, recuperação antecipada e retenção de download útil | 2.0.6.2, 2.0.8.5 | Presente | Leituras aguardadas, prioridade e cooldown em `src/dts/dts_engine.c:255`; integração contém correções posteriores de refill. |
| Reusar CDN resolvida e recuperar link expirado em SDR/DTS | 2.0.8.5 | Parcial | Reuso de URL final na ramificação DV/remux (`src/dts/dts_engine.c:267`); DTS standalone não DV ainda usa URL original (`:257`). |
| Buffer HTTP nativo em RAM, ranges/auth, seeks e fallback | 2.0.5–2.0.8 | Presente | Caller `src/video.c:2919`; cache progressivo e exclusões manifest/loopback em `src/moviebuffer.c:213`; leitor contínuo nativo em `src/dts/dts_engine.c:842`. Sem promessa para HLS/DASH/live/P2P. |
| Presets 64/128/256 MiB e opção de desligar redução por RAM baixa | 2.0.5, 2.0.7 | Parcial | Presets em `src/ajustes.c:938`. Piso de 100 MiB sempre protegido (`src/cacheboost.c:146`); não localizada chave para desligar redução. Automático não equivale exatamente a System/off. |
| Limites HTTP separados 1–4 e conversão Standard(1) contínua como novo padrão | 2.0.8.5, alterado em 2.0.8.7 | Ausente | Cache nativo usa conexão contínua, conversão usa AVIO por ranges (`src/dts/dts_engine.c:938`) com quatro workers (`:83`). Sem controles independentes. Default Standard(1) da 2.0.8.7 substitui recomendação antiga de quatro conexões da 2.0.8.5. |
| URLs do conversor e fonte DV lembrada até 8.191 bytes | 2.0.8.5 | Parcial | URL geral suporta 8.191 (`src/mediaurl.h:8`), mas conversor rejeita ≥4.096 (`src/dts/dts_playback.c:793`) e fonte DV lembrada tem 1.024 (`src/video.c:121`). |
| Reusar fonte/latest seek e troca DTS sem recarregar imagem | 2.0.6.2–2.0.8.7 | Parcial | Reuso de fonte e último controle existem, mas seek faz novo Load nativo (`src/dts/dts_playback.c:508`); troca de áudio faz prepare completo (`:538`). Teste atual exige três criações para dois seeks (`tests/dts_playback.c:437`), não continuidade de imagem. |
| Recuperar fila cheia do decoder nativo sem travar para sempre | 2.0.8.7 | Presente | Detecção/recarga limitada em `src/dts/dts_playback.c:469`. Recuperação pode recarregar imagem; não comprova mecanismo seamless do fork. |
| Preservar VPS/SPS/PPS in-band atuais em HDR10/DV8 com DTS | 2.0.8.7 | Ausente | `src/dts/dts_engine.c:978` ainda usa `hevc_mp4toannexb`. Conversor dedicado do commit upstream `3e607e1eddaa` e regressão `hevc_annexb` não encontrados. |
| Recuperar faixas DTS omitidas pela LG, inclusive várias do mesmo idioma | 2.0.8.7 | Parcial | Recuperação nativa limitada a arquivo de faixa única (`src/video.c:2138`). Enumeração posterior na conversão (`:3362`) não resolve lista nativa anterior à conversão. |
| DV8 descoberto tardiamente mantém routing de áudio/imagem alinhado | 2.0.8.7 | Parcial | Sonda/retry/RPU em `src/video.c:2289,2300`; HDR nativo observado em `:3355`. Handoff equivalente ao mais recente não localizado. |
| Tratamento experimental de HDR10+ SEI | 2.0.8.7 | Presente | Filtro `src/dts/dts_engine.c:1349`, caller `src/video.c:3256`; opt-in OFF (`src/ajustes_ux_padrao.inc:346`). Sem prova física HDR10+/DV. |
| PGS e fila de legenda cheia sem encerrar sessão DV/DTS | 2.0.8.7.test2 | Parcial — correção ausente | Decoder/overlay em `src/dts/dts_overlay.c:20`, `src/video.c:169`. Fila cheia de 64 cues/16 MiB ainda falha sessão (`src/dts/dts_playback.c:714`). Upstream `a0b04d3b2080` descarta cues antigos/oversize. Existência de decoder não comprova desenho de PGS durante conversão DV; fork declara essa limitação também. |
| Sem callback/overlay/loading antigo; aviso DTS uma vez por fonte | 2.0.6.2–2.0.8.5 | Parcial | Guardas de sessão (`src/video.c:1070`), callbacks aposentados (`src/dts/dts_playback.c:175`) e HDR preservado (`src/video.c:3355`). Aviso DTS temporizado por transição de estado (`src/player.c:6989`), não uma vez por fonte. Equivalência visual não provada. |
| Menu áudio por idioma/codec/canais, exceção FLAC, labels longos e badge DV | 2.0.8.7 | Parcial | Labels convertidos `%.26s · %.12s`, sem título/canais (`src/video.c:3381`), limite público 48 bytes (`src/video.h:369`); menu divide label (`src/faixas.c:1531`). Badge DTS continua representado (`src/player.c:632`). |
| Áudio de outra versão: MKV/MP4/AVI/TS, sync ±60min, passos manuais e correção de frame rate | 2.0.8.7.test | Ausente | Sem pipeline/fonte/estado/ajustes equivalentes encontrados. `src/video.c:3928` seleciona faixas do próprio arquivo; offset de sync comum é função diferente. |
| Seguir filme inteiro entre cortes, fallback próprio e correção de freeze após intro skip | 2.0.8.7.test, corrigido em test2 | Ausente | Depende do pipeline de áudio externo ausente. Sem mapeamento entre versões nem readiness de áudio alternativo (`src/dts/dts_playback.c:414`); sem equivalentes aos commits `40b01efe1c2a`, `686a6a4faaab`. |

## Itens compartilhados com os outros mapas

- Barra/range de buffer existe; tela dedicada, valores e switches independentes permanecem parciais: [diagnosticos.md](diagnosticos.md).
- Memória de áudio por série/perfil existe; identidade da faixa no mesmo índice é parcial: [fontes.md](fontes.md).
- Matching da release em reprodução existe. Continuação automática **inclusive no modo manual** é parcial: integração ainda exige folha de escolha; memória não equivale a consentimento. Essa diferença foi reconciliada entre as duas análises.
- Visibilidade independente dos botões intro/recap/créditos está ausente; autoskip não é a mesma opção: [fontes.md](fontes.md).

## Testes inspecionados, sem execução

- DV: `tests/dv7.c`, `tests/dv7_engine.c`, `tests/dv7_opt_in.sh`, `tests/dvsonda_contentor.c`, `tests/dvaudio_lg.c`, `tests/dvaudio_lg.sh`, `tests/dvretry_lg.c`, `tests/dvtela.c`.
- DTS/EDID: `tests/dts_edid_pending_lg.c`, `tests/dts_tv.c`, `tests/dts_timestamp_engine.c`, `tests/dts_recovery_scope.c`, `tests/dts_playback.c`.
- Rede/buffer: `tests/dts_range_retry.sh`, `tests/dts_shared_retry.sh`, `tests/moviebuffer.py`, `tests/video_moviebuffer.c`, `tests/video_http_buffer.sh`, `tests/buffer_tamanho.c`, `tests/dts_buffer_reporting.c`.
- Legendas/áudio: `tests/dts_subtitles.c`, `tests/audioescolha.c`, `tests/audiomemoria.c`, `tests/audioinfo.c`.
- Não localizados equivalentes às regressões novas `hevc_annexb`, `subflood` e áudio externo/filme inteiro.

## Proveniência: tag não representa todas as notas

A tag pública `2.0.8.7.test2` resolve para `f3d30f345e2d…` em master, cujo conteúdo não representa tudo anunciado nas notas. A pesquisa também consultou as branches públicas:

- `dv-dts-native-test-2.0.8.7` em `93c2a28ebc37…`;
- `external-audio-test-2.0.8.7` em `6ef2d551452b…`.

As diferenças de PGS e áudio externo usam commits concretos dessas branches. Não basta comparar árvores das tags para atribuir todos os binários experimentais. Origem das notas: https://github.com/Matis94/nuvio-native-legacy/releases.

## Limites e decisões

Relatos do fork sobre C3/C5/soundbar são relatos atribuídos, não testes nossos. EDID físico, DTS-HD/DTS:X na soundbar, acionamento Dolby Vision, longos filmes sincronizados e PGS sem interrupção seguem sem prova nesta comparação. Não foi executado teste/build nem feita instalação.

Ausência de áudio externo/saída DV fixa/TrueHD webOS não foi justificada como política do dono. São lacunas técnicas. Preservar desenho aprovado do player e countdown automático do próximo episódio OFF (`DECISOES.md:69,73`). Comparação não autoriza substituir defaults de P7/RAM pelos do fork.
