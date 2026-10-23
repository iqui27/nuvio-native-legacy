# #345 — áudio ausente na abertura (Android TV)

Branch `agente/205-audio345`, base `bddd5ca5` (2.0.6). Investigação local dos dois logs de 2.0.5 fornecidos pelo usuário; sem GitHub, push ou deploy.

## Resultado

**Causa do silêncio ainda não comprovada.** Os registros mostram entrada codificada no sink, seleção automática de inglês e vídeo tocando, mas não identificam o codec nem provam que o AudioTrack iniciou/avançou ou que o som voltou. Não foi alterada a política de passthrough, seleção ou reabertura do sink sem essa evidência.

| Registro | Evidência |
| --- | --- |
| `log-345-Cf4whk.txt` (20:53), linhas 643–684 | T1E3 abre do início; 3 faixas de áudio/7 legendas; inglês 3/3 (664); passthrough (667); primeiro quadro (669–670); pronto/tocando (672–675). |
| `log-345-6HMRKA.txt` (22:37), linhas 469–506 | T1E5 abre do início; 3/7; inglês 3/3 (490); passthrough (492); primeiro quadro (494–495); pronto/tocando (498–506). |
| Mesmo registro, linhas 963–1021 | T1E5 reabre em aproximadamente 196 s (8%); 3/7, inglês 3/3 e passthrough novamente; retomada aceita na preparação (1018). |
| Mesmo registro, linhas 1095–1129 | Nova abertura em aproximadamente 196 s; agora 1 faixa de áudio/32 legendas; passthrough também; pronto/tocando e retomada aceita. O log não identifica o motivo da mudança de lista nem confirma som. |

Leitura integral: 899 linhas em Cf4whk, 1172 em 6HMRKA. Ambos começam durante uma reprodução já em andamento; não contêm o cabeçalho de inicialização com modelo/versão. Não há erro de player (evento 5), áudio sem decoder (evento 9), erro AudioTrack/AAudio/SDL ou nome EAC3/AC3/DTS/TrueHD nos registros. `atmos=0` não identifica o codec: não exclui EAC3 comum, AC3 ou DTS. Os erros HTTP de Trakt/Simkl/imagens e um timeout de resolução de fonte alternativa não demonstram falha de áudio.

Cf4whk:214–229 registra pausa, novo primeiro quadro e retomada do T1E2; não informa se foi seek ou mudança de superfície/retorno do sistema, nem se havia som. Não há confirmação de recuperação após seek/troca de faixa em nenhum registro. `seekr` é a busca de miniaturas, não prova de seek de reprodução.

## Fluxo conferido

- `src/video_android.c:303`: preferência compara idioma e pede a faixa correspondente. `nativeFaixa` recebe o índice zero-based do Kotlin; `video_escolher_audio` envia `faixaAudio[i].numero` ao mesmo array `audios`. A indicação humana 3/3 corresponde ao índice 2, sem erro de base encontrado.
- `android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt`: `publicarFaixas` filtra faixas suportadas; `escolherMain` usa o par grupo/índice e `TrackSelectionOverride`. `buildAudioSink` usa `DefaultAudioSink.Builder(context)` e delega capacidades ao Media3. Não há decisão própria de passthrough baseada num booleano inicial.
- `android/app/src/main/java/space/nuvio/nativelegacy/AudioSyncSink.kt:17`: o tap informa formato antes de `super.configure`; a linha antiga “passthrough” prova entrada codificada oferecida ao sink, não inicialização bem-sucedida nem som audível.
- `src/android.c:101`: espelha stdout/stderr **para** logcat; não importa `Log.i`/`Log.w` do Kotlin para o arquivo enviado. Os detalhes relevantes permaneciam só no logcat.
- `android/app/src/main/cpp/CMakeLists.txt:26`: SDL sem áudio; reprodução pelo Media3. AAudio/SDL não são o sink deste caminho. `NuvioActivity.kt` cuida do ciclo de vida; nenhum mute inicial próprio foi encontrado ali.

## Passo entregue

`10b2ad0` — `diagnostics(android): include startup audio details in app logs`

Ponte `nativeAudioLog` reutiliza o formato do log nativo. Eventos Media3 passam a registrar sessão, faixa realmente selecionada/idioma/MIME/canais/taxa, pedido de troca e seek, decoder, entrada do renderer, configuração real do AudioTrack (encoding/offload/tunneling), avanço do AudioTrack, volume/supressão no READY e erros de sink/codec. Callbacks de sessão obsoleta são descartados pelo guard já existente. Sem log por buffer, nova dependência ou alteração de reprodução. Avanço do AudioTrack ainda não prova som audível no receptor.

Validação offline após a alteração:

| Comando | rc |
| --- | --- |
| `python3 tests/audiosync_android.py` | 0 |
| `bash tests/video_android_cacheboost.sh` | 0 |
| `git diff --check` | 0 |

O primeiro compila NvPlayer/AudioSyncSink/AudioSyncTap e dependências contra Android 35/Media3 1.8.0 e executa PCM/passthrough na JVM. O segundo compila o backend C real com JNI simulado e valida abertura/cache/ganho/velocidade. Não há FAIL→PASS da decisão de áudio: nenhuma decisão foi alterada e os logs não permitem reproduzir o silêncio headless. Sem build de APK ou execução física.

## Verificação pendente no aparelho

1. Na TV do reporter, com a mesma conexão de saída (alto-falantes/HDMI ARC/eARC/receptor) e mesmo ajuste de áudio do Android, abrir T1E3/T1E5 do início com preferência inglês. Anotar se há som nos primeiros 15 s e coletar o registro desde antes da abertura.
2. Ainda sem som, fazer seek +10 s; anotar se/quanto demora para voltar. Trocar para outra faixa e retornar ao inglês; anotar o resultado de cada troca. O registro novo deve mostrar MIME selecionado, decoder/entrada, encoding do AudioTrack, avanço e erros antes/depois.
3. Reabrir em aproximadamente 196 s e testar avanço automático do episódio anterior, além da abertura manual. Comparar com a fonte de 1 áudio/32 legendas vista no segundo registro.
4. Comparar saída automática/passthrough do Android com PCM, mantendo fonte e faixa. Se só PCM funciona, verificar compatibilidade efetiva do receptor para o MIME/encoding registrado; se o AudioTrack não inicia/avança, coletar também logcat AudioTrack/AudioFlinger/MediaCodec desde a abertura. Não basta o evento “tocando”.

Esses resultados distinguem rota/passthrough incompatível, seleção/renderização e sink/HAL parado. A correção de reprodução depende dessa evidência.
