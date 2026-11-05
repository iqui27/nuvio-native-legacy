> **Superado em 06/11 (dono):** a sincronia por áudio em C (`audsync.c`, `audvad.c`, `silero.c`) foi apagada. No Samsung .tpk ela saiu inteira; no Android a reserva por áudio agora é a do NuvioTV-Reshaped, em Kotlin (`android/.../audiosync/`, `ReservaAudio.kt`; ver `src/legsync.h`). Este documento fica como histórico.

# Sync de legenda por áudio (F06): capacidade por backend, implementação e modelo opcional

04/10/2026, branch `f06-audio-sync` (base `50afbae3`). Este documento separa o que foi **provado no código** do que **precisa de TV**. Nada aqui foi medido em aparelho.

## 1. Matriz de capacidade: o app consegue ler o PCM decodificado do áudio que está tocando?

| Backend | Capacidade | Aparelho | Evidência | Bloqueado por |
|---|---|---|---|---|
| **Android (Media3 1.8.0 / ExoPlayer)** | **Habilitado no código** (prova de compilação + replay JVM; sem prova em TV) | Android TV (TCL C755 é o alvo citado no F03; não testado) | `AudioSyncSink.kt`: `ForwardingAudioSink` sobre o `DefaultAudioSink` do `DefaultRenderersFactory.buildAudioSink`. `handleBuffer(buffer, presentationTimeUs, …)` entrega o PCM de entrada do sink **com o tempo de mídia** de cada buffer. `configure(Format)` diz se a entrada é `audio/raw` (PCM) ou bitstream. Compila contra android-35 + Media3 1.8.0 (`tests/audiosync_android.py`). | Nada estrutural. Bitstream (passthrough AC3/E-AC3/DTS/TrueHD, ou offload) **não passa por PCM** no app: reportado como “indisponível com passthrough ligado”, nunca desligado. |
| Android: alternativa `TeeAudioProcessor` | Descartada | — | `javap` do Media3 1.8.0: `AudioBufferSink.flush(int,int,int)`/`handleBuffer(ByteBuffer)` sem timestamp; `AudioProcessor.StreamMetadata` não existe na 1.8.0. Sem tempo de mídia o alinhamento dependeria de contagem de amostras desde o flush. | — |
| **LG webOS (uMS / starfish, `video.c`)** | **Indisponível** | LG C9 (não testado) | O app entrega a URI ao uMediaServer via Luna; o pipeline decodifica e toca no hardware. Nenhuma API do uMS/ACB devolve PCM ao processo do app. O NDL é caminho de *saída* (o app empurra áudio/vídeo), não de captura. | Plataforma fechada. Caminho possível só com **decodificação auxiliar**: baixar e decodificar a faixa de áudio em paralelo no app (Range + decoder próprio), com orçamento de CPU/rede e prova de que não degrada a reprodução. Não feito. |
| **Samsung .wgt (AVPlay)** | **Indisponível** | — | A API AVPlay não tem callback de dados de áudio; o objeto AVPlay não é um `HTMLMediaElement`, então Web Audio (`createMediaElementSource`) não se aplica. | Plataforma fechada; mesma alternativa de decodificação auxiliar (no WASM, sem sockets próprios, ainda mais cara). |
| **Samsung .tpk Tizen 6+ (Tizen.Multimedia.Player, `video_tpk.c` + `Video.cs`)** | **Indisponível no produto; candidato pendente de prova** | — | TizenFX tem `Player.EnableExportingAudioData(AudioMediaFormat, PlayerAudioExtractOption)` + evento `AudioDataDecoded` (since Tizen 6.0). Restrições documentadas: chamar em `Idle`, indisponível com offload de áudio e para alguns codecs. A documentação não deixa claro se o áudio exportado **continua sendo reproduzido** (o modo `Default` “sincroniza com o relógio”), nem se o player de TV (pipeline de hardware) suporta. | Precisa de experimento em TV: o áudio continua audível? A/V sync e HDR intactos? custo de CPU? Até lá, `audsync_backend` não é registrado no .tpk e a UI diz “indisponível nesta plataforma”. |
| Samsung .tpk Tizen 4/5 (`NuvioTpk40`) | **Indisponível** | — | A API acima é Tizen 6.0+. | Versão da plataforma. |

Regra: a capacidade é anunciada **pelo backend em tempo de execução** (`audsync_backend`, registrado pela primeira chamada `nativeAudioEstado` da casca Kotlin), não por macro de plataforma. Uma casca Android antiga sem o tap continua “indisponível nesta plataforma”.

## 2. O que esta fatia entrega (Android, ponta a ponta no código)

1. **Ajuste** `Idiomas e legendas › Sincronia por áudio` (`AJ_LEG_SYNC_AUDIO`, último do `OpcaoId`, chave `legendaSyncAudioLocal`, local por TV, padrão **desligado**, ícone Lucide `audio-lines`). Desligado = nada sobre áudio aparece.
2. **Tap** (`AudioSyncTap.kt`, Kotlin puro): só copia com `ligado` (o C liga por `escolher(3,1)`); downmix de todos os canais + reamostragem por média de caixa para **mono 16 kHz int16**; PCM 8/16/24/32/float; lê o `ByteBuffer` por índice absoluto (posição/limite do player intactos) e escreve num `ShortArray(2048)` fixo — **nenhuma alocação por buffer**. O mesmo `(buffer, pts)` reoferecido pelo sink é copiado uma vez. `flush()` = descontinuidade.
3. **JNI** (`video_android.c`): `nativeAudioPcm` copia para a pilha e entrega a `audsync_pcm`; `nativeAudioEstado` informa PCM/bitstream/nenhum.
4. **Sessão** (`src/audsync.c`): anel limitado de 4 s (`AUDSYNC_RING`) + 512 descritores, sob lock curto; produtor nunca espera — anel cheio descarta e quebra a janela. Um fio roda o VAD até **300 s contíguos** de mídia (a engine exige ≥180 s de sobreposição após o offset); seek/salto de pts > 200 ms, troca de formato ou pausa por seek/buffer reiniciam a janela. Bitstream durante a escuta → “indisponível com passthrough ligado”.
5. **VAD** (`src/audvad.c`): quadros de 20 ms; energia da banda 300–3400 Hz acima de um piso adaptativo (mínimo de 5 s) + fração de banda + fluxo espectral ponderado por energia (FFT 512). Onset 3 quadros, soltura 15, segmento termina no último quadro ativo; junta lacunas < 300 ms, descarta < 200 ms.
6. **Alinhamento candidato**: atividade de fala × atividade das cues (só diálogo) em grade de 50 ms, Jaccard corrigido pelo acaso, ±30 s. Pré-recusas: sem fala, áudio contínuo, sem atividade de legenda, confiança < 0,30, margem < 0,08, janela curta.
7. **Engine decide**: com o offset candidato, escolhe pontos de corte em silêncio nas duas linhas do tempo e monta dois `LegendaDocumento` da **mesma janela**: a fala (rótulos distintos `fala NNNN`, origem `audio`) e a externa recortada. `legsync.c` seleciona o recorte e pede `autosync_solicitar` com `raio` 30 s e os **limiares de sempre** (0,78/0,09, regiões, bordas ±250 ms). Só ACEITO muda o offset; Desfazer mantém o manual. “Outra referência” não se aplica ao áudio.
8. **Seletor de legendas** (linha de AutoSync do F04/F05): ação “Por áudio” entre ‹ › quando disponível; “Ouvindo as falas… N%” com Parar; aceite/recusa como no F05; “Por áudio: sem falas claras; nada foi alterado”; em repouso, com o ajuste ligado e sem PCM, o motivo (“indisponível nesta plataforma” / “com passthrough ligado” / “sem áudio decodificado”).
9. **Cancelamento**: fechar o player, troca de fonte, troca de legenda, troca da faixa de áudio (`faixas.c` → `legsync_audio_trocou`), desligar o ajuste, Parar. O tap é desarmado no fio da UI (`audsync_passo`).

## 3. Verificação

```sh
bash tests/audsync.sh                 # 23 casos; SANITIZE=1 e SANITIZE=thread: sem erros
python3 tests/audiosync_android.py    # compile-check NvPlayer/AudioSyncSink/AudioSyncTap + replay JVM do tap
python3 tests/streamfit_passiva_android.py   # continua passando com os arquivos novos
bash tests/legsync.sh; bash tests/autosync.sh; bash tests/legref.sh   # + SANITIZE=1/thread
bash tests/ajustes_ux_dados.sh; bash tests/ajustes_secoes.sh; python3 tools/idiomas.py
bash tests/audsync_shot.sh /Volumes/ExternalSSD/nv-f06-shots/audsync   # capturas GL, Montserrat
```

Casos sintéticos: VAD com rajadas de fala em ruído (bordas ≤100 ms), ruído/música/silêncio sem fala; offsets +2,5/−1,2/+12/−25/0 s recuperados ≤50 ms; janela só de música recusada; fala de outro “filme” recusada; sessão completa aceita +2,5 s e −1,2 s (com seek no meio reiniciando a janela), música recusada sem mudar nada; plataforma sem PCM, ajuste desligado, passthrough antes e durante a escuta; cancelamentos acima; anel com produtor sem espera (exatamente o que cabe é aceito, o resto contado como descartado e a janela reinicia). NDK 27.2 `-fsyntax-only` arm64/ARMv7 de `video_android.c`/`audsync.c`/`audvad.c`/`legsync.c`. Compile integral do núcleo no host.

## 4. Limites honestos

- **Sem prova em TV.** Falta: o tap recebe PCM de verdade no DefaultAudioSink da TV; custo de CPU do VAD em ARM; latência real; comportamento com o decoder FFmpeg da extensão.
- **Fala sob música alta não é detectada** (o piso sobe junto) → recusa. Legendas com tempos de leitura longos/cues que juntam falas vão bater na regra de bordas ±250 ms da engine e serão **recusadas**; isso é conservador de propósito. Melhorar recall exige corpus real anotado antes de relaxar qualquer limiar.
- Escuta de 300 s: a correção chega depois de ~5 min assistindo.
- Só o slot principal; sem segundo idioma.
- Logs novos em inglês: `[audsync] audio_sync window reason=…`, `[legsync] audio_sync rejected reason=…`; sem texto, idioma, URL ou identidade da mídia.

## 5. Modelo ASR local opcional — SÓ DESENHO (não implementado)

Objetivo: quando o VAD sozinho não basta (fala sob música, cues muito diferentes da fala), transcrever trechos em inglês e alinhar **texto** com a legenda (forced alignment simples), devolvendo o mesmo par de documentos à engine.

- **Candidatos**: whisper.cpp `tiny.en`/`base.en` (MIT; ~75/142 MB ggml, q5 ~31/57 MB) ou sherpa-onnx com Zipformer pequeno (Apache-2.0; ~70 MB int8). Silero VAD (MIT, ~2 MB ONNX) pode substituir o VAD heurístico. Conferir a licença de **cada peso** antes de oferecer.
- **Download explícito, uma vez**: ação em Ajustes “Baixar modelo de fala (inglês) — N MB”, mostrando tamanho antes; download em arquivo temporário com Range/retomada, **SHA-256 fixo no app** (falha de hash = apaga e recusa), progresso, **Cancelar** e **Remover** (libera o espaço). Nunca automático, nunca em reprodução; checa espaço livre (statvfs) com folga de 2× antes de começar. Local por TV.
- **Orçamento**: RAM de pico ≤ 150 MB (tiny/base q5 cabem; medir em ARM de TV de 1,5–2 GB); CPU em 1 fio de baixa prioridade, só 3–6 trechos de 20 s com fala (escolhidos pelo VAD), alvo < 30 s de processamento total para Quick; aborta se o player perder quadros/buffer (mesma pausa do F05).
- **Inglês primeiro**: só oferecido quando a faixa de áudio e a legenda são `en`; outros idiomas só com modelo multilíngue e nova avaliação.
- **Gate**: comparar VAD × ASR por latência, aceites corretos, falsas correções e RAM no mesmo corpus antes de ligar por padrão; sem ganho medido, não entra.
