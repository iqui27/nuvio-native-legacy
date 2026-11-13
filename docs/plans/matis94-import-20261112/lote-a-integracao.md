# Lote A integrado — segurança de playback

Pedido do dono: trazer as funcionalidades ausentes do fork Matis94 para a integração de TV.

O merge `c0abc4a28f5ead0f69d39f2550e07900b5577133` incorpora o candidato `7d40f7debbba7ce3a41373bc30cb61360b289bf3` sobre a base `f2126002ff7240f4c4477e49586764cacd962b64`. Cobre R15, R18 e R22 do [plano](STATUS.md). Não houve push, publicação, deploy, instalação nem acesso a contas.

## Comportamento incorporado

- Fila de legendas cheia descarta cues dentro dos limites em vez de encerrar playback. PGS válido acima do orçamento é validado e descartado antes de alocar RGBA; entradas malformadas continuam sendo erros.
- Conversão HEVC preserva VPS/SPS/PPS atuais do stream e SEI exclusivo do contêiner, sem duplicar payloads idênticos. A leitura de NAL malformado que terminava em start code recebeu correção de limite.
- URLs de mídia e memória de áudio DV suportam até 8.191 bytes mais terminador; comprimentos superiores são rejeitados pelo limite existente.

Commits de código: `bdec190a8`, `9830b0cfe`, `429ba8b23`, `786ba79d8`, `4188a8e76`, `f1a22d43e`. Créditos upstream foram preservados. As adaptações iniciais vêm de `a0b04d3b2080`, `3e607e1eddaa` e `c83c81d42` do fork.

## Revisão e gate

Revisor independente: `j-rev-a3`. Verificador distinto: `j-verify-a3`. Ambos concluíram sem P0/P1; os três achados históricos foram encerrados com evidência. N1 P2 permanece registrado abaixo.

SHA-256 do diff revisado entre base e candidato: `cddf04086663b590a1b088146af24a2512ee4933e5282341b46eb8c5e4028977`.

`central.py verifica` retornou `passed`, agreement verdadeiro, 5 testes executados/5 aprovados, nenhum ignorado, divergente ou em timeout. O integrador conferiu base, HEAD, worktree limpo, conclusão do job, identidades de revisão/verificação e digest antes do merge. A varredura i18n automática após merge passou.

Relatórios privados, sem valores de segredos:

- `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/j-rev-a3.log`
- `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/j-verify-a3.log`
- Livro: `/Volumes/ExternalSSD/tmp/rev/jobs.jsonl`, evento de merge com digest de verificação `c54a484bc942a2115f7518124091aba2788317a34fdcac0fef9f5b511c980920`.

## Testes pós-merge

Todos executados sobre `c0abc4a28f5ead0f69d39f2550e07900b5577133`, sem mudança de HEAD durante os testes. TMPDIR exclusivo no SSD externo e `NUVIO_PROPERTIES=/dev/null`.

| Comando | rc | SHA-256 do log |
|---|---|---|
| `bash tests/hevc_annexb.sh` | 0 | `50aff821a499c285ac3cc3a22e5ee6119095c4d64eb5cdcdc9e77b600826a853` |
| `bash tests/dts_pgs.sh` | 0 | `0da13b9228916942685f06d51f43dbf68d83887be561903110285e6ed3366a79` |
| `bash tests/dts_playback.sh` | 0 | `5dbe50c597ba2ecdd9d963e87db45dcb0a0bee850f86eb2c934b3814c1e80c42` |
| `bash tests/video_acb_lg.sh` | 0 | `de67602e4afa2c3bfdb164c7cc4bfb8736587e16c77daf2492a21b16caa2f661` |
| `bash tests/i18n.sh` | 0 | `07c16d17d396124e3a114322c0ccda63d0990cd2145e8f9fb383b60ebff0a14c` |

Manifesto de execução: `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/a-postmerge-evidence.json`; logs individuais no mesmo diretório, prefixo `a-postmerge-`.

Antes do merge, revisor e verificador também executaram o build host e sanitizadores. A regressão HEVC contra o header anterior reproduziu heap-buffer-overflow de um byte; no candidato passou. A regressão de SEI falhou contra o header anterior e passou no candidato. O teste PGS usa decoder real e controller de playback até EOF.

## Limites e nota P2

- N1 confirmado: ao descartar uma cue PGS grande, a cue anterior sem término pode continuar aparecendo até a próxima cue/clear. Não bloqueou integração. Uma correção futura precisa preservar o timestamp no caminho de descarte, pois o decoder limpa a saída; fechar a cue anterior usando o timestamp já zerado seria incorreto.
- Decoder real no host: Homebrew FFmpeg 9/libavcodec 63. Paridade de bytes com FFmpeg 7.1.5 e `hevc_annexb_engine.sh` não foram executados por ausência dessa versão/libx265 no ambiente.
- O build host não compila o caminho FFmpeg completo do engine. O revisor fez uma checagem sintática separada contra FFmpeg 9, desabilitando uma categoria de warning já presente na base; isso não substitui build ARM/ABI.
- Nenhum teste em TV nem prova de ABI/pacote ARM nesta rodada. Não há alegação de correção já verificada em aparelho.
