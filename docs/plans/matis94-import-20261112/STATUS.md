# Importação do fork Matis94 — 12/11/2026

Pedido literal do dono: “vamos trazer o que falta do fork pra nossa integração”.

Destino: `integracao/2.1`. Base inicial: `8221bb897110271313db2864d2f0d15d1857b61b`. TV apenas; nenhum trabalho mobile/desktop de produto. Build host é gate técnico. Sem push, publicação, deploy, instalação ou escrita em conta. Preservar visual, consentimento manual e preferências/defaults existentes. Novos controles usam componentes atuais; tela nova precisa seguir fluxo de aprovação de prancha.

Fonte e critérios: [mapa de paridade](../../reviews/matis94-20261112/README.md). Cada item parcial/ausente deve terminar com commit, regressão/build, revisão independente e merge, ou diferença de produto explícita com evidência. Não marcar prova física com teste host.

## Lotes

| Lote | Escopo | Estado |
|---|---|---|
| A | Segurança de filas PGS, HEVC in-band, URLs longas | Integrado em `c0abc4a28`; revisão + verificador independentes, gate 5/5 e pós-merge 5/5. [Evidência](lote-a-integracao.md). TV pendente. |
| B | Modos DV/DTS/TrueHD, faixas e memória de áudio | Correções de rótulo por faixa e capacidade do SDK importadas; suite B, idiomas e build host passaram em `c9a8c3a3a`. Nova revisão e atualização da base pendentes. |
| C | Buffer contínuo, controles, recovery/seek e telemetria | C1 transporte e padrão Standard(1) testados em `7a1c53d22`, escolhas salvas preservadas. C2 reutilização de fonte/pipeline implementada e testada no Mac mini; patch recebido, ainda não importado/revisado. C3 telemetria na fila. |
| D | Áudio de outra versão e cortes/intro skip | Engine com mídia codificada, menu, controlador e build host passaram; atualização A/H/F reconciliada em `0e2b690a8`. Revisão final e combinação B/C pendentes. |
| E | Formatter, badges, regex e lista de fontes | Renderização completa UTF-8, formatter, regex e build host passaram; base A/H/F atualizada. Regressões explícitas de backup/localidade passaram após correção do teste. Atualização G e nova revisão pendentes. |
| F | Perfil, skip, atualização e opacidade | Integrado em `26eec9dbb`; revisão + verificador independentes, gate 8/8 e pós-merge 3/3. [Evidência](lote-f-integracao.md). TV pendente. |
| G | Diagnósticos local/QR, motivos e exportação de nuvem | Integrado em `6aa346ec3`; revisão + verificador independentes, gate 8/8 e pós-merge 3/3. [Evidência e P2](lote-g-integracao.md). Serviço de nuvem sem deploy; TV pendente. |
| H | CW local, temporadas e títulos de episódios | Integrado em `8a16234bc`; revisão + verificador independentes, gate 15/15 e pós-merge 3/3. [Evidência](lote-h-integracao.md). TV pendente. |

Execução: implementações iniciais `nv-impl medium`; correções e revisões `nv-revisao medium`. Modelo servido desconhecido sem prova do gateway. A/F/H/G integrados; B/C/D/E ainda fora. Worktrees `/Volumes/ExternalSSD/nv-21-matis-*`; briefs e evidência em `/Volumes/ExternalSSD/tmp/rev/matis94-20261112/`. Estado acima registra o corte documental, não atividade instantânea de processos. Uma escrita por worktree; sem contornar limites do executor.

## Cobertura item a item

| ID | Item auditado | Estado inicial | Estado da importação |
|---|---|---|---|
| R01 | P7 → P8.1, base HEVC preservada, RPU convertido, EL/FEL descartada | Presente | Manter; prova de TV separada |
| R02 | Seletor experimental de mapping P7 | Ausente | Pendente de adaptação/prova |
| R03 | Dolby Vision MP4 experimental | Ausente | Pendente de adaptação/prova |
| R04 | Saída de áudio DV fixa: Original / AAC / Dolby Digital | Ausente | Pendente de adaptação/prova |
| R05 | TrueHD → AAC/AC-3 preservando DV e aviso próprio | Ausente no webOS | Pendente de adaptação/prova |
| R06 | Consultar DTS na TV, aguardar resposta e indicar DTS nativo | Presente | Manter; prova de TV separada |
| R07 | Conversão DTS Off, preservando preferências antigas e DV com AC-3/E-AC-3 | Parcial | Pendente de adaptação/prova |
| R08 | DV + DTS-HD MA/DTS:X integral nativo com conversão Off | Ausente | Pendente de adaptação/prova |
| R09 | Startup/resume DTS e recuperação limitada de gaps/descontinuidades | Presente | Manter; prova de TV separada |
| R10 | Retry de ranges, recuperação antecipada e retenção de download útil | Presente | Manter; prova de TV separada |
| R11 | Reusar CDN resolvida e recuperar link expirado em SDR/DTS | Parcial | Pendente de adaptação/prova |
| R12 | Buffer HTTP nativo em RAM, ranges/auth, seeks e fallback | Presente | Manter; prova de TV separada |
| R13 | Presets 64/128/256 MiB e opção de desligar redução por RAM baixa | Parcial | Pendente de adaptação/prova |
| R14 | Limites HTTP separados 1–4 e conversão Standard(1) contínua como novo padrão | Ausente | Pendente de adaptação/prova |
| R15 | URLs do conversor e fonte DV lembrada até 8.191 bytes | Parcial | Código integrado em `c0abc4a28`; regressão de limites e memória DV passou. TV pendente. |
| R16 | Reusar fonte/latest seek e troca DTS sem recarregar imagem | Parcial | Pendente de adaptação/prova |
| R17 | Recuperar fila cheia do decoder nativo sem travar para sempre | Presente | Manter; prova de TV separada |
| R18 | Preservar VPS/SPS/PPS in-band atuais em HDR10/DV8 com DTS | Ausente | Código integrado em `c0abc4a28`; SEI preservado e regressões/sanitizadores passaram. Paridade FFmpeg 7.1.5 e TV pendentes. |
| R19 | Recuperar faixas DTS omitidas pela LG, inclusive várias do mesmo idioma | Parcial | Pendente de adaptação/prova |
| R20 | DV8 descoberto tardiamente mantém routing de áudio/imagem alinhado | Parcial | Pendente de adaptação/prova |
| R21 | Tratamento experimental de HDR10+ SEI | Presente | Manter; prova de TV separada |
| R22 | PGS e fila de legenda cheia sem encerrar sessão DV/DTS | Parcial — correção ausente | Código integrado em `c0abc4a28`; decoder PGS real e playback até EOF passaram. N1 P2: cue anterior pode permanecer durante cue descartada. TV pendente. |
| R23 | Sem callback/overlay/loading antigo; aviso DTS uma vez por fonte | Parcial | Pendente de adaptação/prova |
| R24 | Menu áudio por idioma/codec/canais, exceção FLAC, labels longos e badge DV | Parcial | Pendente de adaptação/prova |
| R25 | Áudio de outra versão: MKV/MP4/AVI/TS, sync ±60min, passos manuais e correção de frame rate | Ausente | Pendente de adaptação/prova |
| R26 | Seguir filme inteiro entre cortes, fallback próprio e correção de freeze após intro skip | Ausente | Pendente de adaptação/prova |
| F01 | Cache de layout, contagens e recomendações da lista | Presente | Manter; prova de TV separada |
| F02 | Formatter lazy/bounded e redução de trabalho fora da tela | Parcial | Pendente de adaptação/prova |
| F03 | Copiar dados dos badges no worker, reduzindo bloqueio/memória | Parcial | Pendente de adaptação/prova |
| F04 | VSync enquanto espera fonte, fallback se VSync falha e pacing do player | Parcial | Pendente de adaptação/prova |
| F05 | Menos redraws, fluidez com 2.000+ fontes e remoção de diagnóstico temporário | Não comprovado | Pendente de adaptação/prova |
| F06 | Texto do add-on, emoji offline, flags, DUAL/MULTI e alinhamento | Parcial | Pendente de adaptação/prova |
| F07 | Inferir idioma quando declaração falta, sem misturar legendas | Presente | Manter; prova de TV separada |
| F08 | Pacotes de badges: cores, fundo, contorno, transparência e estabilidade | Parcial | Pendente de adaptação/prova |
| F09 | Expressões longas e modificadores regex com escopo | Parcial | Pendente de adaptação/prova |
| F10 | Badges no modo Do add-on: Topo/Embaixo, Topo padrão | Ausente | Pendente de adaptação/prova |
| F11 | Nome completo com wrapping em ambos modos, sem ellipsis na animação e sem duplicar prefixo Cloud | Parcial | Pendente de adaptação/prova |
| F12 | Unificar pedidos duplicados de fontes, preservar async/refresh/episódios diferentes | Presente | Manter; prova de TV separada |
| F13 | Próximo episódio segue release inclusive em modo manual, sem abrir folha | Parcial — política diferente | Preservar consentimento manual; diferença intencional |
| F14 | Título de episódio substitui nome de release e mantém IDs/numeração | Parcial | Código integrado em `8a16234bc`; gate 15/15 e pós-merge 3/3. P2 de títulos genéricos/numeração documentados. TV pendente. |
| F15 | Disponibilidade de temporadas e atualização de cache de episódios | Parcial | Código integrado em `8a16234bc`; temporadas zero por último e limite preservado. TV pendente. |
| F16 | Memória de áudio por série/perfil, próximo episódio e fallback por idioma sem confundir índice | Parcial | Pendente de adaptação/prova |
| F17 | Lembrar último perfil: opt-in OFF, não entrar em bloqueado/indisponível | Ausente | Código integrado em `26eec9dbb`; gate 8/8, pós-merge 3/3. F4 P2 documentado; TV pendente. |
| F18 | Mostrar botões intro/recap/créditos independentemente, ON e por perfil | Ausente | Código integrado em `26eec9dbb`; F5 P2 de herança inicial em perfil antigo documentado. TV pendente. |
| F19 | Desligar avisos automáticos de atualização mantendo busca manual | Ausente | Código integrado em `26eec9dbb`; avisos enfileirados suprimidos e busca manual validada offline. TV pendente. |
| F20 | Continuar assistindo local antes do sync remoto | Parcial | Código integrado em `8a16234bc`; progresso do perfil publicado antes da reconciliação, inclusive com fileira anterior não vazia. TV pendente. |
| D01 | Página local estável e download de log redigido | Presente | Manter; prova de TV separada |
| D02 | QR local RED → BLUE com snapshot enquanto tela fica aberta | Ausente | Código integrado em `6aa346ec3`; QR local real e refresh comprovados no render host. TV pendente. |
| D03 | Explicar por que página local não abriu | Ausente | Código integrado em `6aa346ec3`; P2 F2: expiração/bloqueio ainda podem mostrar motivo `?`. TV pendente. |
| D04 | Rede 100.x | Presente, com escopo | Manter; prova de TV separada |
| D05 | Link público/QR de log na nuvem válido por 72h; OK/RIGHT como fallback | Ausente | Código integrado em `6aa346ec3`; auth restrita, TTL e limites testados offline. Serviço sem deploy/provisionamento; nenhum upload real. |
| D06 | Chave Diagnostics QR OFF e liberação de recursos ao fechar | Parcial | Código integrado em `6aa346ec3`; OFF padrão e fechamento normal testados. P2 F9: snapshot pode persistir ao encerrar processo. TV pendente. |
| D07 | Overlay RAM e GREEN, depois três chaves independentes de RAM/dados/atalho | Parcial | Pendente de adaptação/prova |
| D08 | Dados do buffer: MiB pronto/capacidade, tempo estimado, modo sistema e limite HTTP da sessão | Parcial | Pendente de adaptação/prova |
| D09 | Diagnóstico de rede redigido: redirects, timings, rate limit e reuso | Parcial | Pendente de adaptação/prova |
| D10 | Cooldown 429 compartilhado com Retry-After | Presente, com escopo | Manter; prova de TV separada |
| D11 | Traduções 29 idiomas: mapping P7, DV MP4, saída DV e tela Buffer | Parcial | Pendente de adaptação/prova |
| D12 | Opacidade de legenda em passos de 5%, com migração | Ausente | Código integrado em `26eec9dbb`; migração por perfil comprovada FAIL 134 → PASS 0, IDs antigos e precedência preservados. TV pendente. |
| D13 | ARM completo, ABI, IPK com P2P e limpeza de atalhos de build | Não comprovado como resultado | Pendente de adaptação/prova |
