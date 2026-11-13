# Lote D integrado — áudio de outra versão

Pedido do dono: importar recursos ausentes do fork Matis94 para integração de TV. Candidato `a346fb12eef438c2e480732a4f0ff955e71e45d5`, base `40df7dfb2a964a5e2c5a8e835e74408e562722d0`, diff SHA-256 `a398cef1a5109efc2897ee61f7a7a20ce1e3c0ae7203ce8b297a31fbf96779a3`.

## Comportamento incorporado

- Áudio de outra fonte escolhida manualmente, com seleção de faixa, perfis e memória por série. Reutiliza lista, menu e Ajustes existentes; recurso e follow desligados por padrão.
- Doadores MKV/MP4/AVI/TS, offset até ±60 minutos, ajustes manuais, velocidade/frame rate, matching, acompanhamento entre cortes, prefetch e recuperação após intro. Probe com worker único, cancelamento por requisição e publicação protegida por geração.
- AAC MPEG-TS normalizado pelo filtro existente `aac_adtstoasc` para contrato raw AAC do Starfish. Regressão real compara pacotes byte a byte com cópia MP4 antes/depois de seek; AAC raw configurado preservado. SDK webOS acrescenta filtro e demuxers necessários; ausência do filtro para AAC sem configuração usa conversão.
- Ao retirar doador ou receber falha/recusa de troca, áudio próprio retorna com posição, pausa e faixa preservadas. Conversão DTS própria tem uma tentativa quando permitida; opção DTS Off impede essa tentativa.
- Combinação com B mantém getter DTS Off único, política atômica separada do doador, prefixo integrado dos Ajustes e nove opções locais D depois dele. Doadores AAC continuam permitidos sobre arquivo com DTS próprio sob Off; doador DTS permanece bloqueado em candidatos, probe, faixa forçada, menu e engine.

## Revisão e gate

Revisor `j-rev-d5-zima` e verificador distinto `j-verify-d5-zima`: cobertura completa de 72 arquivos e fronteiras com B, incluindo adaptador Starfish real. Nenhum P0/P1 atual confirmado. Dois P1 históricos retirados com confirmação independente: retorno ao DTS próprio e ADTS enviado como AAC raw. Revisores fizeram leitura de fonte; não executaram testes/builds.

Duas regressões antes/depois: retorno ao áudio próprio (D8) e framing AAC (D10). Terceira regressão combinada D11: AAC escolhido sob Off era recusado por causa da faixa DTS própria; teste LG real da linha 3 passou de abort 134 para sucesso após limitar recusa a sessão sem doador.

Gate final: 12 declarados/executados/obrigatórios/passados, zero falhas, skips, timeouts ou divergências, agreement verdadeiro e errors vazio. Reexecutados no candidato exato: audioext_lg, dvaudio_lg, dv7_settings, ajustes_nuvem, dts_external_audio, dts_playback, build-local, audioext_menu, audioext, ajustes_sync, ajustes_ux_interacao e i18n. Inclui suíte estrita de engine com áudio codificado real, identidade do doador, múltiplas dublagens, ausência de trocas indevidas, timestamps monotônicos além de 118 segundos, offset, fill/resume em zero, cortes/PAL/intro, cancelamento e casos AAC TS/raw.

Cinco checks do relatório D11 haviam rodado antes da última linha de video.c; sua atribuição histórica foi preservada. Gate final os repetiu no candidato e eliminou essa lacuna. `matis_b.sh` não rodou como agregado nesta adaptação D; partes foram testadas individualmente. `hevc_annexb.sh` passando não equivale a `hevc_annexb_engine.sh`: controle negativo herdado retorna 1 no FFmpeg 9 e permanece sem prova no FFmpeg 7.1.5 fixado.

## Sete P2 confirmadas

- D5-ZIMA-001: DV com faixa própria DTS/TrueHD e doador marcado pode cair para HDR10 sob DTS Off, embora áudio próprio seja descartado pela sessão externa.
- D5-ZIMA-002: Off bloqueia doador DTS, mas ainda converte doador TrueHD e áudio DTS/TrueHD próprio no fill de follow. Caminhos exigem consentimento explícito, follow OFF por padrão; divergência semântica registrada.
- D2-ZIMA-002: SDL repeat é flag 0/1, não contagem; segurar CH+/CH− não alcança aceleração dos passos maiores.
- D2-ZIMA-003: correção de follow não atualiza offset na memória de série, podendo usar valor anterior no próximo episódio.
- D2-ZIMA-004: Ajustes externos aparecem no desktop Apple/Linux, onde menu de doador está excluído. Nenhum novo trabalho de produto desktop iniciado.
- D3-ZIMA-002: dois textos de marcador/dica permanecem em inglês sem chave de tradução; marcador usa cor fixa em vez do acento configurado.
- D4-ZIMA-001: doador AAC raw sem extradata pode ter pacotes descartados até EOF pelo filtro, sem falha que acione fallback. Classe rara, sem fixture nesta rodada.

## Limites

Sem prova de TV, ABI do adaptador, compilação ARM/SDK webOS, imagem FFmpeg 7.1.5 recompilada, continuidade física de imagem, desempenho ou aceite visual. SDK anterior a D também não contém demuxer TS e retorna ao áudio próprio; SDK de D anterior a D10 pode usar conversão sem filtro. Essas são verificações de fonte, sem imagem antiga executada. Sem instalação, push, release, escrita em conta ou serviço novo. Nenhuma issue encerrada. C ainda precisa ser combinado e validado com esta implementação.

## Merge e pós-merge

Merge `fe82fe5e49967383126fe0ab59a7a8d33e00fa53`; digest de verificação `d5a5b342a2395b3a1ae74ac56a605ec398a1b1935f9bf43ff172a5998587f96d`. Checagem i18n do integrador passou. Três checks pós-merge passaram no mesmo HEAD antes/depois; TMPDIR no SSD e NUVIO_PROPERTIES=/dev/null.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/audioext_lg.sh` | 0 | `02f2814bb2a6ffe705a6eb7ca91b3b1153aae6adaf3459bbfedf5bde1a812a2f` |
| `bash tests/ajustes_nuvem.sh` | 0 | `9be3957c06be0a55ebb9a413aade284326b667e6cf1e8047d0ae9841262def14` |
| `bash tests/i18n.sh` | 0 | `d162d8c3ab7f586c041f54a10dd6cbcaa132085ac0edfaab87b033ca10e285c9` |

Evidência privada em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`: j-rev-d5-zima.log, j-verify-d5-zima.log, d-final-gate.log, d-integration.log, d-postmerge-evidence.json e relatórios D8/D10/D11. Hooks documentais existentes atualizam painel LAN e roadmap público; nenhum serviço de aplicação novo publicado.
