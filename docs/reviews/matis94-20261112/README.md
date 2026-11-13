# Matis94 × Nuvio Legacy — mapa de features

> Inventário histórico do snapshot de 12/11. A importação posterior dos oito lotes foi concluída na integração TV: [estado atual e evidências](../../plans/matis94-import-20261112/fechamento.md). Classificações abaixo preservam a comparação original; não descrevem o estado posterior aos merges.

**Não temos tudo anunciado nessas releases.** A base de DV7/DTS, buffer HTTP, recuperação e caches de fontes já existe. As novidades mais recentes de reprodução e vários controles ainda estão ausentes ou parciais.

Comparação em **12/11/2026**, com as **11 releases** públicas de [Matis94/nuvio-native-legacy](https://github.com/Matis94/nuvio-native-legacy/releases), da 2.0.5 até 2.0.8.7.test2. Nosso código foi fixado no commit [`8221bb897110271313db2864d2f0d15d1857b61b`](https://github.com/iqui27/nuvio-native-legacy/commit/8221bb897110271313db2864d2f0d15d1857b61b), branch de integração `integracao/2.1` em `/Volumes/ExternalSSD/nv-21-int`.

A versão pública consultada continua sendo [v2.0.8](https://github.com/iqui27/nuvio-native-legacy/releases/tag/v2.0.8). **Implementado na integração não significa publicado nem testado em TV.** O fork concentra novidades em LG/webOS; equivalentes Samsung/Android TV não comprovam paridade webOS.

## Resultado

| Situação | Itens | Como interpretar |
|---|---:|---|
| Presente | 13 | Implementação correspondente rastreada no snapshot; ressalvas de escopo descritas em cada linha. |
| Parcial | 27 | Existe mecanismo relacionado, com diferença concreta de comportamento, cobertura ou controle. |
| Ausente | 17 | Não localizado caminho equivalente; alguns pertencem à mesma feature maior, como áudio externo. |
| Não comprovado | 2 | Resultado de performance/build histórico não demonstrável só por leitura de código. |
| **Total** | **59** | Itens de comparação consolidados, sem repetir linhas compartilhadas entre dimensões. |

Esses números não são porcentagem de produto nem contagem universal de features. Uma linha pode agrupar controles relacionados; outra separa mecanismo e correção posterior. Repetições das notas foram consolidadas. Testes existentes foram **inspecionados, não executados**. Nenhum build, instalação, upload ou publicação foi feito nesta auditoria.

## Mapa completo e evidências

| Área | Presente | Parcial | Ausente | Não comprovado | Documento |
|---|---:|---:|---:|---:|---|
| Reprodução webOS | 7 | 10 | 9 | 0 | [26 itens: DV, DTS, TrueHD, PGS, buffer e áudio externo](reproducao.md) |
| Fontes, perfis e episódios | 3 | 12 | 4 | 1 | [20 itens: formatter, badges, áudio lembrado e controles](fontes.md) |
| Diagnósticos e ajustes | 3 | 5 | 4 | 1 | [13 itens: logs, QR, rede, buffer e opacidade](diagnosticos.md) |

Cada linha contém origem nas releases, evidência `arquivo:linha` do snapshot e diferença material. Os caminhos pertencem à integração fixada, não necessariamente ao checkout onde este relatório foi salvo. [Notas originais preservadas](releases.jsonl).

## O que já temos

- **P7 → P8.1:** base HEVC preservada, RPU convertido, enhancement layer descartada; sem reconstrução FEL.
- **DTS nativo por capacidade da TV:** consulta EDID, estado de espera, fallback e indicação implementados. Entrega real à TV/soundbar não verificada.
- **Recuperação DTS:** startup/resume, descontinuidades, ranges e filas nativas com tentativas limitadas.
- **Buffer HTTP nativo em RAM:** ranges, autenticação, leitura antecipada e fallback para fontes incompatíveis.
- **HDR10+ SEI experimental:** implementação opt-in.
- **Fontes:** cache de layout/avaliações, inferência de idioma separada de legenda e deduplicação de requisições do mesmo episódio.
- **Diagnósticos:** página local com log redigido, suporte CGNAT 100.64/10 e cooldown Retry-After compartilhado no escopo da conversão.

## Ausências localizadas

| Área | Falta equivalente no snapshot |
|---|---|
| DV / áudio | Seletor de mapping P7; DV MP4 experimental; saída de áudio DV fixa; conversão TrueHD no webOS; DV + DTS-HD MA/DTS:X integral nativo. |
| Buffer / vídeo | Controles separados de conexões HTTP 1–4 e modo contínuo de conversão Standard(1); correção dedicada de parameter sets HEVC in-band. |
| Áudio externo | Áudio de outra versão com sync automático/manual e correção de frame rate; seguir filme inteiro entre cortes com fallback e correção após intro skip. |
| Fontes / preferências | Posição Topo/Embaixo dos badges no modo Do add-on; lembrar último perfil; visibilidade independente intro/recap/créditos; desativar aviso automático de atualização mantendo busca manual. |
| Logs / legenda | QR local com snapshot; motivo de falha ao abrir página local; link público/QR de log válido por 72h; opacidade em passos de 5% com migração. |

## Parciais que merecem atenção primeiro

| Item | Diferença concreta | Encaminhamento técnico sugerido |
|---|---|---|
| **PGS pode interromper DV/DTS** | Queue cheia de 64 cues/16 MiB ainda falha a sessão. O test2 descarta cues antigos/oversize. | Avaliar correção `a0b04d3b2080` e regressão de saturação antes de dizer que test2 foi absorvido. |
| **HDR10/DV8 com DTS: parameter sets** | Falta conversor dedicado que preserva VPS/SPS/PPS atuais dos pacotes. | Avaliar `3e607e1eddaa` com regressão HEVC correspondente. |
| **URLs longas** | Limite geral 8.191, conversor <4.096 e URL DV lembrada 1.024 bytes. | Unificar capacidade real de ponta a ponta; teste só da URL geral não basta. |
| **Conversão DTS Off** | Original não impede conversão em todos os caminhos DV/fallback. | Definir e testar semântica única antes de anunciar equivalência. |
| **Seeks e troca de áudio** | Reuso da fonte existe, mas pipeline nativo é recarregado. | Separar recuperação funcional de continuidade de imagem; fixture atual exige recargas. |
| **Faixas omitidas e memória de áudio** | Recuperação nativa limitada a uma faixa; atalho por índice/idioma não confirma codec/canais/título. | Regressões com várias faixas do mesmo idioma e substituição no mesmo índice. |
| **Badges / formatter** | Modificadores regex com escopo ausentes; estilos e largura durante carregamento parciais; wrapping difere entre modos. | Comparar parser e render reais, preservando desenho aprovado. |
| **Performance da lista** | Copiar array ainda ocorre antes do worker; fallback por falha do VSync não localizado; fluidez 2.000+ sem medição. | Provar com carga equivalente em TV; não deduzir FPS por existência de cache. |

Prioridade é sugestão de avaliação, não implementação iniciada ou tarefa publicada. Demais diferenças estão nas linhas completas dos três documentos.

## Diferenças intencionais e mudanças de defaults

- **Modo manual:** matching da release existe, mas a integração conserva escolha explícita. Fonte lembrada serve de foco; não constitui consentimento automático. Copiar o comportamento do fork sem essa distinção mudaria política.
- **Visual:** borda superior das fontes anunciada pelo fork não é alvo literal. Nosso design usa foco por superfície e proíbe contornos nesses elementos. Preservar também tratamento aprovado dos badges.
- **P7:** nosso default OFF; primeira build do fork anunciava ON.
- **Buffer:** nosso padrão Automático, com proteção de RAM obrigatória; fork oferece desligar redução.
- **Conexões de conversão:** recomendação de quatro em 2.0.8.5 foi substituída por Standard(1) contínuo em 2.0.8.7. Não registrar defaults históricos como alvo atual.
- **Logs:** nossa página local/medidor começam OFF. Envio automático de logs começa ON na inicialização efetiva; não confundir com Diagnostics QR do fork.
- Ausência de TrueHD webOS, áudio externo, DV MP4 e saída DV fixa é lacuna técnica; não foi atribuída a veto do dono.

## Releases cobertas

| Tag do fork | Publicação UTC | Papel nas notas |
|---|---|---|
| [2.0.5](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.5) | 16/10/2026 | Base custom de buffer, P7, RAM e filenames; teste webOS. |
| [2.0.6](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.6) | 16/10/2026 | Rebase da base custom; execução ainda solicitada. |
| [2.0.6.2](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.6.2) | 16/10/2026 | Recovery DTS/ranges, QR local/snapshot e wrapping. |
| [2.0.7](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.7) | 21/10/2026 | RAM override, flags e caches de lista. |
| [2.0.8](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8) | 24/10/2026 | Build experimental v11: DV/TrueHD, seek, overlays, buffer. |
| [2.0.8.1](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.1) | 25/10/2026 | Saída DV fixa, MP4, mapping P7 e controles de diagnóstico. |
| [2.0.8.5](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.5) | 30/10/2026 | Notas acumulam etapa experimental e adições chamadas estáveis: fontes, CW, rede, notificações, conexões e URLs. |
| [2.0.8.6](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.6) | 03/11/2026 | Lista grande, memória de áudio, badges, perfil e botões skip. |
| [2.0.8.7](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.7) | 06/11/2026 | Off DTS, HEVC, filas, seek sem reload, buffer contínuo e nomes. |
| [2.0.8.7.test](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.7.test) | 08/11/2026 | Áudio externo e filme inteiro entre cortes; experimental. |
| [2.0.8.7.test2](https://github.com/Matis94/nuvio-native-legacy/releases/tag/2.0.8.7.test2) | 11/11/2026 | EDID, DV+DTS integral, fila PGS, intro skip e log público. |

**Cuidado com proveniência:** a tag test2 aponta para árvore que não contém tudo descrito nas notas. A pesquisa de reprodução também consultou branches públicas de DTS nativo e áudio externo e identificou commits concretos. Detalhes em [reproducao.md](reproducao.md). Alegações do fork sobre LG C3/C5 são atribuídas às releases; não foram reproduzidas por nós.

## Conclusão operacional

A integração absorveu a base e algumas correções recentes, mas **não está em paridade com 2.0.8.7/test2**. Há três grupos diferentes de trabalho: correções de reprodução existentes no fork, funcionalidades novas e diferenças de produto/defaults. Este inventário permite avaliá-los sem confundir implementação parcial com feature concluída nem build experimental com comportamento validado.
