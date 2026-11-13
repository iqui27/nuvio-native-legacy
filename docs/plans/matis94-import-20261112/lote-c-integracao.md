# Lote C integrado — buffer, seek/reuso e telemetria

Escopo autorizado: TV; adaptar lacunas do fork preservando visual aprovado, consentimento manual e preferências. Padrão novo autorizado: conversão Standard(1) em configuração ausente/reset; valores salvos 1–4 preservados. Nenhuma instalação, release, push, escrita em conta ou deploy de serviço novo nesta importação.

## Implementação combinada

- Limites HTTP nativo/conversão independentes 1–4. Standard(1) usa leitor contínuo com ring limitado, mínimo de 16 MiB; caminhos 2–4 mantêm ranges paralelos. Reuso de CDN resolvida e recuperação de autorização têm fixtures offline. Limites, cancelamento e geração continuam separados por sessão.
- Seek e troca de faixa reconstroem somente cadeia de áudio quando formato permite, preservando demux/index/BSF/cache. Latest command wins; `load_revision` distingue revisão de comandos de um novo Load. Flush/formato recusado e falta de progresso têm fallback limitado; não é prova de continuidade física.
- Dados de buffer, overlay de RAM e atalho GREEN independentes, OFF por padrão. Redução opcional por RAM baixa mantém padrão ON. Piso obrigatório de 100 MiB e overhead de 4 MiB continua mesmo com redução opcional OFF; tamanho inseguro retorna Automático.
- Telemetria usa bytes contíguos medidos, range com soma saturada e clamp no EOF, tempo à frente de dados reais e indicação de indisponível. Handlers e desenho existem no release webOS normal; instrumentação detalhada de rede continua sob flag de diagnóstico. GREEN ignora repetição e key-up, persiste escolha e reverte falha de gravação.
- Aviso DTS solicitado em cada frame durante janela de 5/10 s, uma vez por estado/fonte, com reset por identidade e geração do player.
- Prefixo dos ajustes da integração, incluindo nove opções D, preservado; seis opções C acrescentadas no fim. C segue `AJ_BUFFER`: backup do aparelho permitido, sync portátil/conta excluído. D mantém exclusões próprias.
- Combinação B/D preserva rotas DTS nativo/TrueHD/saída fixa/Off/MP4, setup de doador e filtro AAC ADTS, retorno ao áudio próprio com posição/pausa/uma tentativa. Troca de doador não entra em seek seamless: requer Load real.

## Correções reproduzidas

C5 corrigiu quatro P1: aviso de um frame, controles restritos ao diagnóstico, bypass do piso de RAM com redução OFF e texto de buffer sem range/tempo. Reproduções e resultados estão em `j-fix-c5-review-report.md`; revisões C3 completas e distintas confirmaram ausência das falhas no candidato final.

C6 acrescentou regressões de readiness: revisão sem Load preserva estado; Load real reseta e aguarda callback. Uniu roteamento B, seek `audio_at`, DTS Off e saída fixa sem duplicar configuração.

C7 corrigiu troca de áudio externo perdida no caminho seamless. Cenário real do controller `ext-seamless` aborta sem guard (rc 134) e passa com correção. Controller usa doubles de engine/pipeline; suíte de doador separada usa motor real.

C8 `e55541b81f23f9704d7ade9c5d81db81b6c03fea` confirmou P1 C-R13 no callback AVIO efetivamente selecionado por Standard(1). MKV de `mkvmerge` aponta elementos do SeekHead para Cues/Tags no EOF, indisponíveis no motor P2P local. Antes: watchdog, 6,218 s, rc 134; depois: 1,122 s, rc 0. `read_fluxo_pending` aplica orçamento aproximado de 1 s do caminho anterior de ranges, com polling de 100 ms e cancelamento. Controle não loopback continua espera normal (~6,2 s); teste direto de ranges continua ~1,006 s. Teste também comprova seek e troca de faixa no motor real Standard(1), preservando `fmt`/`io`. Não cobre Standard(1) com doador.

## Revisão e limites

Candidato final `e55541b81f23f9704d7ade9c5d81db81b6c03fea`, base `67c5c6ddea069d471ff90b565da163df72c731f3`, diff SHA-256 `94f7d940c5d18284ea6c3f8a5e35a318af283f52a51534703d30a00eb89b1a6b`. Revisor `j-rev-c3-zima` e verificador distinto `j-verify-c3-zima`: cobertura completa de 89 arquivos e callers, ambos OK, nenhum P0/P1 confirmado. Leitura de fonte; nenhum deles executou testes. Metadados de commit são fornecidos pelo snapshot; central confere objetos Git/cleanliness e identidade no gate.

Oito P2 confirmados e mantidos, sem cortar escopo:

| ID | Limitação |
|---|---|
| C-R5 | Seamless habilitado nas sessões DTS/DV; flush e continuidade em firmware real não comprovados. |
| C-R6 | Timer sem progresso é renovado durante preroll sem frame; não há teto explícito próprio para esse estado. |
| C-R8 | GREEN tem rota de fonte/SDK/fixture; entrega física por controle LG não comprovada. |
| C-R10 | Tabelas chinesas têm labels vazios, ajuda em inglês e tradução inadequada de RAM. |
| C-R11 | Builds de diagnóstico mantêm hostname DNS completo nos registros de ranges. OSD não mostra URL/hostname. |
| C-R12 | Seek/faixa Standard(1) real coberto em C8; combinação Standard(1)+doador permanece sem teste real. |
| C-R14 | Leitor contínuo ignora Retry-After em 429 e usa backoff fixo 1/2/4/8/15 s, seis tentativas; ranges respeitam cooldown até 30 s. |
| C-V1 | Orçamento de cauda baseado em posição também pode abortar MP4 com moov obrigatório no fim e P2P lento. Paridade com ranges anterior, agora aplicada a Standard(1). |

`tests/dts_p2p_tail.sh` requer `mkvmerge` e FFmpeg CLI com libx264/dca; ausência falha visivelmente, sem skip. Testes host usam FFmpeg 9; não provam FFmpeg 7.1.5 fixado, ABI ARM ou TV. `hevc_annexb_engine.sh` é controle negativo herdado rc 1 (`bad=0` no host); continua sem prova, não passou nem foi dispensado. Não confundir com `hevc_annexb.sh`, que é outro teste.

Inspeção de pré-requisito ARM: Apple Container ativo, imagem `nuvio-webos-sdk:latest` digest `2e3b92eca405` contém headers e `libavcodec.a`, mas falta `/opt/nuvio-dts/nuvio-audio-config` exigido pelo novo guard. Nenhum pacote ARM/IPK/ABI foi validado a partir dessa imagem; não houve rebuild/overwrite da imagem, instalação ou teste físico.

## Gate, merge e pós-merge

Gate final 13/13, nenhum skip/fail/timeout/divergência/erro, agreement true; digest `7f50a9b71dfd5897ea4819faeb937adad29fde68525abe146d8848d0ba49f432`. Doze checks complementares C9 também passaram no candidato final, incluindo os checks com evidência originalmente histórica em C7. Total de 25 comandos distintos cobertos no candidato (inclui build); não são 25 testes declarados pelo revisor como executados pessoalmente. C8 reportou 12 suítes + build; gate refez esses 13; C9 executou os outros 12.

Merge protegido `0cefb7bc4c28e787c20a0b3353b37f2dac85d6f1`; árvore idêntica ao candidato. i18n do integrador rc 0. Cinco checks pós-merge passaram com mesmo HEAD antes/depois, TMPDIR no SSD e NUVIO_PROPERTIES=/dev/null.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/dts_p2p_tail.sh` | 0 | `bf429afc7d53780f45acf0a45389cd8252408b36f4f7e1be36c5c06e79eb6119` |
| `bash tests/dts_playback.sh` | 0 | `29583aab1db8fc0930f6ee9dc35642c75d1968467a9474b6afdf63c33c6f79fb` |
| `bash tests/buffer_ram_reduz.sh` | 0 | `69af11375bf65c2cceb5a7480c573a0038350dfac6bb7c3def485711729f00a6` |
| `bash tests/ajustes_nuvem.sh` | 0 | `c1def9a24788e462765a686af1fee3c196cbf2644bb35938273c57060b95ddd5` |
| `bash tests/i18n.sh` | 0 | `c9e8edbb1bed30b4e421752ae27c86bb719eb562d7d0a6d70e1a89f061feb17f` |

Evidência privada: `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`, relatórios C5–C8, j-rev-c3-zima.log, j-verify-c3-zima.log, c9-host-evidence.json e logs com hashes, c-final-gate.log, c-integration.log, c-postmerge-evidence.json. Apenas hooks documentais existentes atualizam painel LAN e roadmap; nenhum novo serviço de aplicação publicado.
