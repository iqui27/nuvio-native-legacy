# Paridade Matis94 — fontes, formatter, perfis e episódios

Snapshot da integração: `8221bb897110271313db2864d2f0d15d1857b61b`. Notas das 11 releases: [releases.jsonl](releases.jsonl). Inspeção estática; nenhum teste/build executado. Referências apontam ao código desse snapshot em `/Volumes/ExternalSSD/nv-21-int`. Não representam necessariamente o checkout onde este documento foi salvo. Presença de código não comprova performance nem execução em TV.

| Feature consolidada | Origem | Estado | Evidência e diferença material |
|---|---|---|---|
| Cache de layout, contagens e recomendações da lista | 2.0.7–2.0.8.6 | Presente | `src/streams.c:4835,4902`: revisões evitam avaliações/layout repetidos; `:4889`: busca binária do início visível. |
| Formatter lazy/bounded e redução de trabalho fora da tela | 2.0.8.5–6 | Parcial | Cache de 128 blocos em `src/text.c:2248`; linhas de fontes fora da tela puladas em `src/streams.c:6702`. Formatter ainda desenha todas linhas de um bloco parcialmente visível (`src/text.c:2333`). Não prova resultado com 2.000 fontes. |
| Copiar dados dos badges no worker, reduzindo bloqueio/memória | 2.0.8.6 | Parcial | Matching tem worker, mas chamador aloca/copia array completo de `Stream` antes de criá-lo (`src/streams.c:4478`). |
| VSync enquanto espera fonte, fallback se VSync falha e pacing do player | 2.0.8.6 | Parcial | `src/main.c:2477,2493`: swap interval e frame wait existentes; retorno de swap interval ignorado. Equivalência do estado de espera/fallback/retained-video não demonstrada. |
| Menos redraws, fluidez com 2.000+ fontes e remoção de diagnóstico temporário | 2.0.7–2.0.8.6 | Não comprovado | Mecanismos de cache existem; não há medição comparável nesta auditoria. Diagnóstico de filtro de áudio em `src/streams.c:4868` não pode ser confundido automaticamente com instrumentação temporária do fork. |
| Texto do add-on, emoji offline, flags, DUAL/MULTI e alinhamento | 2.0.7–2.0.8.6 | Parcial | Caller em `src/streams.c:4103`; atlas em `src/text.c:1365,1722`; texto irmão preservado em `src/stream_parse.c:1165`. Não encontrada chave independente para flags de idioma; equivalência visual/cobertura de emoji não comprovada. |
| Inferir idioma quando declaração falta, sem misturar legendas | 2.0.8.5 | Presente | Metadado explícito tem precedência, inferência como fallback (`src/stream_parse.c:217,247`); marcadores áudio/legenda separados em `:528`. |
| Pacotes de badges: cores, fundo, contorno, transparência e estabilidade | 2.0.8.6 | Parcial | Parser em `src/selospacote.c:254`; alpha em `src/streams.c:5856`. `tagStyle` armazenado sem consumidor de render demonstrado. Imagem pendente reserva largura genérica, carregada usa proporção real (`:5834`), permitindo mudança de largura. |
| Expressões longas e modificadores regex com escopo | 2.0.8.6 | Parcial | Padrões dinâmicos em `src/selospacote.c:254`, limite 30.000 instruções em `src/regexjs.c:423`. Grupo como `(?i:...)` é rejeitado como desconhecido (`:231`). |
| Badges no modo Do add-on: Topo/Embaixo, Topo padrão | 2.0.8.6 | Ausente | Ramo Do add-on continua antes do renderer normal de badges (`src/streams.c:6834`); não localizada opção/caller equivalente. |
| Nome completo com wrapping em ambos modos, sem ellipsis na animação e sem duplicar prefixo Cloud | 2.0.5–2.0.8.5 | Parcial | Add-on faz wrap UTF-8 (`src/text.c:2281`); modo Nuvio usa marquee (`src/streams.c:4066`) e copia nome em 512 bytes (`:6952`). Dedup só de texto exatamente igual (`src/stream_parse.c:1167`), sem normalização Cloud equivalente encontrada. |
| Unificar pedidos duplicados de fontes, preservar async/refresh/episódios diferentes | 2.0.8.6 | Presente | Canonização/identidade ativa em `src/addons.c:4016,4030`; pedidos separados de fontes/metadados em `src/app.c:6581`. Foco no mesmo título tem debounce intencional de 1,2s (`:6564`). |
| Próximo episódio segue release inclusive em modo manual, sem abrir folha | 2.0.8.5 | Parcial — política diferente | `bingeGroup` priorizado (`src/app.c:6854`), mas modo manual abre folha (`:6872`) e não pré-carrega (`src/proximopreparo.c:141`). Integração trata fonte lembrada como foco, sem consentimento automático de reprodução. Não portar mecanicamente. |
| Título de episódio substitui nome de release e mantém IDs/numeração | 2.0.8.5 | Parcial | Título de metadados em `src/descoberta.c:7485`, identidade preservada em `:7713`. Nome não vazio pode continuar filename; fallback só com vazio (`src/episodios.c:1076`). Rejeição completa de release filenames não demonstrada. |
| Disponibilidade de temporadas e atualização de cache de episódios | 2.0.8.5 | Parcial | Temporadas derivadas da lista real, incluindo especiais 0 (`src/descoberta.c:8150`); TTL de 6h e expiração (`:6235,6305`). Notas do fork não especificam toda política adicional. |
| Memória de áudio por série/perfil, próximo episódio e fallback por idioma sem confundir índice | 2.0.8.6 | Parcial | Persistência/normalização em `src/audiomemoria.c:21,38`; wiring `src/app.c:4027`, `src/perfis.c:535`, `src/faixas.c:955`. Atalho por índice confere grupo/número/idioma, sem confirmar nome/codec/canais (`src/audioescolha.c:134`): faixa diferente do mesmo idioma pode ganhar. |
| Lembrar último perfil: opt-in OFF, não entrar em bloqueado/indisponível | 2.0.8.6 | Ausente | Perfil salvo só sugere foco/continuação; startup multi-perfil continua pedindo escolha (`src/perfis.c:557`, `src/app.c:4154`). Sem opção dedicada encontrada. |
| Mostrar botões intro/recap/créditos independentemente, ON e por perfil | 2.0.8.6 | Ausente | Opções atuais controlam autoskip (`src/ajustes.c:3125`), não visibilidade. OSD pode reexibir botão (`src/intro.c:1118`); sem três preferências no caller (`src/player.c:6201`). |
| Desligar avisos automáticos de atualização mantendo busca manual | 2.0.8.5 | Ausente | Home consulta/apresenta automaticamente (`src/app.c:5893`, `src/atualizacao.c:1083`); consulta manual separada em `:1018`. Sem gate por preferência encontrado. |
| Continuar assistindo local antes do sync remoto | 2.0.8.5 | Parcial | Snapshot local carregado/publicado em `src/app.c:3998`, `src/catalogo.c:1282`; CW existente preservado (`src/descoberta.c:4728`). Reconstrução consulta trackers antes de `continuarLocal` (`:3594,3651`); caso sem snapshot local válido não demonstrado. |

## Cobertura inspecionada

`tests/audioescolha.c:58` cobre índice na mesma release e rejeição por idioma diferente; não cobre troca de faixa no mesmo índice com mesmo idioma. `tests/fonte_addon_texto_shot.c:188` verifica cache de medição em quadro estável e alturas animadas. Nenhum deles foi executado nesta comparação.

## Diferenças intencionais / decisões

- Modo manual conserva escolha explícita. Adotar continuação automática mesmo nesse modo muda política; não é apenas recuperar patch.
- Correção de borda superior da primeira fonte anunciada pelo fork não é aplicável literalmente: `DESIGN.md:55` proíbe contornos de painel/fileira/pôster, e `src/streams.c:6732` implementa foco por superfície.
- Cores de badges importados não autorizam trocar tratamento branco padrão aprovado (`DESIGN.md:66`, `src/streams.c:6913`).
- Ausência de conflito explícito para novos controles de perfil/skip/atualização/posição de badges não significa autorização para implementá-los neste pedido.
