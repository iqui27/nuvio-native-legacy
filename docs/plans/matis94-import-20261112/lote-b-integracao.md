# Lote B integrado — DV, DTS, TrueHD e faixas

Pedido do dono: importar recursos ausentes do fork Matis94 para integração de TV. Candidato `f3a4a42a2bb8072532d7e63c658a0d564b025674`, base `027065ea065e93f08d8efd1c69874a5520b42729`, diff SHA-256 `6a2d4abfb9ad84b539288ec92b694780daa512662af610701f451c0ae325ab45`.

Implementação: mapping P7 opt-in OFF, experimento MP4 OFF com retry único após erro 204 inicial e filas limitadas, saída DV Original/AAC/DD, conversão TrueHD, DTS nativo condicionado à declaração positiva da TV, opção DTS Off acrescentada sem reinterpretar valores antigos, recuperação de múltiplas faixas DTS omitidas, identidade por idioma/codec/canais e memória de áudio. Configuração efetiva do SDK exige decoders DCA/TrueHD/AC-3/E-AC-3 e encoders AAC/AC-3; imagem antiga incompleta falha explicitamente.

Revisor `j-rev-b3-zima` e verificador distinto `j-verify-b3-zima`: completos, OK, 86 arquivos e adaptador Starfish real inspecionados. Revisores não executaram testes. Dois P1 históricos retirados independentemente: label derivado da faixa selecionada a cada revisão e SDK sem codecs necessários. P2 histórico de recusa Off/loop MP4 retirado. Contagem correta: 17 achados, dois P1 retirados, 15 P2 dos quais um retirado e 14 abertos.

Seis checks reportados pelo executor B10 no candidato: dv7_settings, ajustes_nuvem, ajustes_sync, matis_b, i18n e build-local. Correções B10 restritas aos testes: prefixo integrado G/E seguido do grupo B; lista independente de dez opções locais mantém ausência no upload, ignora injeção legada e preserva reabertura local.

Primeiro replay do gate falhou 6/6 antes de compilar por TMPDIR inexistente. Central reproduziu erro Clang 'unable to make temporary file: No such file or directory', criou diretório no SSD e repetiu gate sem alterar código ou relatórios. Segundo gate passou: seis declarados/executados/obrigatórios/passados, zero falhas, skips, timeouts ou divergências, agreement verdadeiro. Merge `a521dd91f1c5f08d046fd7ce8d58bdfa25be90d4`; i18n do integrador passou. Digest de verificação `ef1d4b33d5d3bc8e603747ab3dcf68c1a121f30cdce0a0e1fc5b7891745e226b`.

## Notas P2 confirmadas

- B-P2-2: faixa DTS recuperada não é restaurada automaticamente antes de as faixas omitidas serem acrescentadas.
- B-P2-3: descoberta DV tardia conserva snapshots de rota não DV; campo DV compartilhado entre threads.
- B-P2-4: DTS Off pode deixar áudio nativo sem som sem aviso específico.
- B-P2-5: escrita de memória de binge a cada loop mantém janela de acesso sem sincronização no caminho Android compartilhado.
- B-P2-6: PATH de Rust com caminho específico do host nos testes.
- B2-P2-7: label ampliado excede buffer de retorno TPK de 48 bytes e pode impedir restauração de faixa longa.
- B2-P2-8: escolha de faixa DTS recuperada durante busy bind pode ficar preparando; falha imediata pode rearmar tentativa.
- B2-P2-9: preview/help LG ainda não explicam corretamente DTS Off.
- B2-P2-10: troca para TrueHD durante sessão DV iniciada com outra faixa pode voltar a HDR10; limitação preexistente.
- B3-P2-11: DTS Off recusa DV antes de tentar substituição E-AC-3 disponível.
- B3-P2-12: MP4 experimental não recupera AAC-only nem interleave de mais de oito amostras BL antes do EL; retry limitado, sem regressão do caminho nativo já falho.
- B3V-P2-13: recusa da ABI DTS nativa pelo Starfish volta a HDR10 sem nova tentativa convertida; prova de firmware pendente.
- B3V-P2-14: experimento MP4 habilita conversão P7 e descarta EL/FEL mesmo com chave DV7 desligada; ajuda não explicita essa consequência.
- B3V-P2-15: TrueHD convertido por padrão pode resultar em AAC estéreo onde antes se escolhia E-AC-3 com surround/Atmos; pedido explícito preservado, trade-off registrado.

## Limites

Controle negativo hevc_annexb_engine retorna 1 no FFmpeg 9 host; execução no FFmpeg 7.1.5 fixado não comprovada nem dispensada. Sem ARM, ABI, imagem SDK recompilada, TV, MP4 real, custo de CPU TrueHD ou aceite visual físico. Verificador conferiu presença das seis strings nas 30 tabelas e leu uma tradução inteira; não julgou qualidade das demais 28. Nenhuma issue encerrada, push, release, instalação ou serviço novo publicado.

## Pós-merge

Três checks passaram no merge exato, com HEAD idêntico antes/depois, TMPDIR no SSD e NUVIO_PROPERTIES=/dev/null.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/dvaudio_lg.sh` | 0 | `2fdd31d368ae4e7b2e9857daf02f9e9b3f20f85580fbc1c1463aaa0d43de8325` |
| `bash tests/ajustes_nuvem.sh` | 0 | `855dc71af1da9b09755b144cf68e1ef3e76e62043e80fb85e3223be7ba48c00e` |
| `bash tests/i18n.sh` | 0 | `2e66179d7b379b1f02d802133f821c72fcef652756c07462923a2fedf76bd339` |

Evidência privada em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`: relatórios B3/verificador B3, B10, gates inicial/retry, b-integration.log e b-postmerge-evidence.json. C e D ainda precisam combinar a nova base B e validar sobreposições de engine/controller e semântica DTS Off; esta integração não comprova essa combinação.
