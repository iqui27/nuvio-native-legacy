# plx item 6 — legendas de imagem

Decisão: não ampliar o suporte neste item. Consulta local ao código da base
integracao/2.0.5 e à pesquisa plx-native (itens 6/7 e «Não portar»); sem aparelho,
rede ou scripts do clone. Nenhuma função de terceiros foi copiada.

| Backend | Comportamento atual de PGS / VobSub / DVB |
| --- | --- |
| webOS, pipeline nativo uMS | A lista vem de subtitleTrackInfo e a escolha usa setSubtitleEnable. O suporte depende do firmware; o app não sintetiza faixas omitidas. faixasmkv apenas casa/rótula as faixas existentes: o filtro de imagem é usado quando as contagens diferem, não remove toda imagem da lista. Não há evidência local de renderização para cada codec/TV. |
| webOS, caminho próprio DV/DTS | Já lista as faixas FFmpeg, decodifica PGS/VobSub em dts_subtitles e entrega RGBA ao dts_overlay, desenhado por player.c. Há limites de retângulos, bytes e fila. O build de FFmpeg habilita pgssub/dvdsub; DVB não está habilitado. Esse suporte depende da sessão DtsPlayback, não é um leitor lateral genérico. |
| Android | Media3 publica as faixas; NvPlayer.onCues encaminha somente Cue.text ao JNI. Cues com bitmap chegam sem texto e não aparecem no overlay. O transporte existente leva texto/duração; não leva bitmap, posição e canvas. A build C não habilita NV_DTS_FFMPEG. |
| Samsung .tpk | Tizen.Multimedia fornece SubtitleUpdated textual. O registro p2p-20 citado em faixasmkv.c tem MKV com uma PGS e zero faixas publicadas. O coletor mkvass aceita ASS/SSA/UTF8/WebVTT, não imagem; não cria as faixas que o host omite. Não há ponte de bitmap nem decoder FFmpeg habilitado neste backend. |

Referências: src/video.c (dtsAplicar/seleção e video_dts_legenda_desenhar),
src/dts/dts_subtitles.c, src/dts/dts_playback.c, src/dts/dts_overlay.c,
tools/build-dts-ffmpeg.sh, src/faixasmkv.c, src/faixas.c,
android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt (onCues),
android/app/src/main/cpp/CMakeLists.txt e src/video_tpk.c.

Há benefício para usuários de remux, especialmente no .tpk, mas não uma
implementação pequena dentro do renderer atual: requer publicar faixas ausentes,
ler pacotes binários/CodecPrivate (incluindo estado de paleta e clear), decodificar
com limites, transportar canvas/posição/RGBA e descartar imagens de sessões e
seeks antigos. No Android é possível reutilizar Cue.bitmap, mas ainda falta a
ponte e composição/clear sincronizada; isso não resolve VobSub/DVB não expostos
pelo demux/decoder. Habilitar apenas DVB no FFmpeg cobre somente DV/DTS webOS,
não os usuários que hoje ficam sem legenda nos outros caminhos.

Portanto, manter o suporte DV/DTS existente e tratar a extensão como trabalho
próprio com fixtures de cada codec e verificação nos backends. Não abrir um
recurso parcialmente funcional só por haver um upload RGBA no caminho DTS.
