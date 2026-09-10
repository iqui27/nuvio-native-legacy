# Nuvio — Minha noite

Estudo de tendências e proposta de produto · 10 de setembro de 2026.

## Direção

A oportunidade é transformar um catálogo que exige exploração em uma sessão que ajuda a decidir. Preservar imagens cinematográficas e navegação familiar, mas colocar retomada, contexto e escolha compartilhada mais perto do primeiro clique.

Esta é uma proposta exploratória, não uma decisão aprovada de redesign. As cinco capturas fornecidas foram tratadas como referência visual. O app de produção não foi modificado. O protótipo HTML é um instrumento de avaliação; o produto continua C99/SDL2/GLES2, com webOS e Tizen.

## Diagnóstico das capturas

| Observação | Efeito provável, a validar | Proposta |
|---|---|---|
| Hero cinematográfico e foco branco no Continue Watching | Boa orientação visual e reconhecimento | Manter imagem do próprio título e foco explícito |
| Texto e várias seções sobre o mesmo backdrop detalhado | Contraste variável; leitura compete com a arte | Hero com máscara de contraste; superfície sólida nas seções inferiores |
| Detalhes exigem rolar entre sinopse, elenco, trailers, recomendações e ficha | Informação útil para decidir fica espalhada | Primeiro bloco com ação, duração e sinopse curta; abas para profundidade |
| Ícones de olho e nuvem sem rótulo visível | Significado depende de aprendizado | Dar nome às ações; não inferir o significado de um ícone apenas pela captura |
| Home mistura português e inglês | Experiência de idioma inconsistente | PT-BR consistente, incluindo tempo restante e episódios |
| Área social vazia ocupa espaço de destaque | Sem atividade, não ajuda a escolher | Entrada acionável para escolher em dupla; manter Trakt com seus estados reais |
| Sinopses completas dos episódios seguintes | Podem antecipar acontecimentos | Ocultar imagem e sinopse futura por preferência do perfil |
| Duração aparece repetida em detalhes | Dado pouco acionável | Transformar duração em filtro de sessão e hora prevista de término |

São hipóteses de UX com base nas imagens, não resultados de teste com usuários. As capturas têm cortes de tela; não atribuir esses cortes a um defeito do layout nativo.

## Tendências verificadas e tradução para o Nuvio

### 1. Descoberta que responde ao momento

A Netflix anunciou em maio de 2025 uma home de TV com atalhos mais visíveis, contexto para escolher títulos e recomendações sensíveis aos interesses do momento. A busca conversacional anunciada naquele texto era um beta opt-in em iOS, não uma funcionalidade universal de TV. Fonte: [anúncio oficial da nova experiência](https://about.netflix.com/en/news/unveiling-our-innovative-new-tv-experience).

**Aplicação proposta:** “Quanto tempo cabe na sua noite?” com tempo e clima selecionáveis pelo controle. Motivo legível da recomendação. Começar com regras e metadados existentes; avaliar semântica depois. Não mostrar percentuais de afinidade sem modelo validado.

### 2. Clipes como porta de entrada, com papéis diferentes para TV e celular

Em 30 de abril de 2026, a Netflix anunciou navegação móvel atualizada e o feed vertical Clips. Isso confirma continuidade da aposta em descoberta por trechos, mas em uma superfície móvel. Fonte: [anúncio oficial de Clips](https://about.netflix.com/pt_br/news/introducing-exciting-new-ways-to-find-and-enjoy-your-next-favorite-on-mobile).

**Aplicação proposta:** futuro “Me mostre o clima”: um trailer horizontal curto por ação explícita; celular pode ser a área de exploração vertical. Não transportar um feed infinito para a home da TV sem testar seu efeito sobre o início de reprodução. Não está implementado no mockup.

### 3. Retomada com contexto

A Amazon apresentou Video Recaps como beta para títulos Prime Original selecionados em inglês nos EUA, seguindo os resumos X-Ray. É evidência de investimento em retomada assistida, não comprovação de disponibilidade universal nem de precisão editorial. Fonte: [anúncio oficial de Video Recaps](https://www.aboutamazon.com/news/entertainment/ai-plot-summary-video-recaps-prime-video).

**Aplicação proposta:** “Relembrar antes”, limitado ao episódio e ao ponto assistido. Priorizar material editorial autorizado e revisado; se o ponto não tiver recap seguro, informar indisponibilidade. No estudo, o botão abre uma explicação do conceito; não inventa um resumo.

### 4. Celular como complemento da TV

O YouTube descreveu em fevereiro de 2025 sua aposta na TV e uma experiência de segunda tela para interagir pelo telefone. Os números de audiência citados pela empresa referem-se ao YouTube e ao mercado indicado; não dimensionam a audiência do Nuvio. Fonte: [prioridades oficiais para 2025](https://blog.youtube/inside-youtube/our-big-bets-for-2025/).

**Aplicação proposta:** evoluir a sessão em dupla para votos no celular. Primeiro testar com um único controle, sem infraestrutura adicional. Watch party sincronizada é outro produto, com dependências diferentes; não está implícita nesta proposta.

## Cinco telas entregues

| Tela | Componentes e interação disponível |
|---|---|
| Minha noite | Hero de retomada, ação de recap, cards com progresso, entrada por tempo |
| Descobrir | Busca por título, filtros combináveis por duração e clima, contagem e estado vazio; sugestão guiada local |
| O título | Adicionar/remover da lista em memória, abas, estados de episódio e proteção de prévia, entrada para fontes |
| Em dupla | Alternância entre dois votantes, rejeitar sugestão, concordância e abertura do título escolhido |
| Player | Composição de controles, painéis de áudio/legendas, recap e comportamento ao terminar; vídeo é estático |

Biblioteca de componentes demonstrada: navegação superior, hero, CTA de retomada, card de progresso, chip selecionado, card com motivo, estado vazio, aba, episódio protegido, indicador de votante, diálogo, controles de sessão e foco direcional.

## Direção visual e revisão do plano

Paleta: fundo #090E16, superfície #172235, superfície de ação #202A38, texto #F4F6FB, texto secundário #BAC7D8 e seleção #D8E6FF. Inter Display já distribuída no app; regular para leitura e bold para títulos. Sem fontes remotas.

Composição: alinhamento à esquerda, margem de 6%, hero que entrega a arte à direita, prateleiras em superfície estável. A direção evita usar a mesma imagem atrás de toda a ficha. O branco mantém o papel conhecido de ação/foco, enquanto azul claro identifica seleção. A marca textual é uma interpretação de conceito.

Esboço escolhido:

    navegação persistente
    contexto + título + ação      arte do próprio título
    retomada   retomada   retomada   retomada
    tempo disponível                      escolher

A revisão do plano descartou uma home de painéis iguais e um chatbot grande. Ambos enfraqueceriam o caráter cinematográfico e aumentariam o trabalho no controle. A ousadia está no fluxo de escolha da noite, não em efeitos decorativos. A navegação inclui atalhos de avaliação para as cinco telas; não é uma arquitetura final de informação.

## Ordem sugerida de implementação

| Prioridade | Entrega | Esforço relativo / dependências | Como avaliar |
|---|---|---|---|
| P0 | PT-BR consistente, ações nomeadas, contraste e foco | Baixo a médio; idioma e renderização existentes | Sucesso de tarefas com controle; erros de foco; leitura a 3 m |
| P1 | Escolha por tempo + motivo da sugestão | Médio; duração confiável, taxonomia editorial, busca/catálogo | Mediana de tempo até reproduzir; taxa de resultado vazio |
| P1 | Detalhe compacto + episódios protegidos | Médio; progresso por identidade e preferência do perfil | Retomada correta; exposição acidental de spoiler |
| P2 | Sessão em dupla local | Médio; estado de sessão e duas escolhas explícitas | Tempo até consenso, abandonos e rejeições |
| P2 | Parar ao terminar e painéis do player | Médio; integrar comportamento existente de próximo episódio | Respeito ao encerramento; zero interrupções ao ajustar |
| P3 | Recap até o ponto assistido | Alto; conteúdo autorizado, revisão, granularidade temporal | Zero informação além do progresso no conjunto de avaliação |
| P3 | Busca semântica e votos no celular | Alto; backend, consentimento e sessão vinculada | Relevância, latência, custo, compreensão e acesso correto |

Esforços são comparativos, não estimativas em dias. Dependem de leitura aprofundada dos módulos antes de implementação. Ganhos são hipóteses, sem prometer melhoria percentual.

## Aderência técnica

O README e PRODUCT.md estabelecem TV com controle remoto, foco inequívoco e continuidade de reprodução. O README já lista busca, biblioteca, perfis, coleções, Trakt e Simkl; não apresentar essas capacidades como novidades. `src/social.h` já diferencia carregamento, ausência de atividade, privacidade, desconexão e indisponibilidade. `src/busca.h` já preserva contexto de busca ao voltar.

A futura implementação deve reaproveitar essas capacidades. Não portar CSS/DOM para dentro do renderer nativo. Usar os layouts como especificação visual, preservando pipeline de vídeo, carregamento assíncrono, cache e orçamento de memória. Evitar várias prévias simultâneas e blur em tela inteira. As medições de 60 fps do README pertencem ao app existente; não são evidência de desempenho deste redesign.

Para produção: foco volta ao acionador de um painel; Back fecha o nível atual antes de sair; título em foco, item selecionado e conteúdo em reprodução têm estados distintos. Validar em TV física webOS/Tizen, não apenas em navegador.

## Arquivos, execução e limites

Abra `index.html` ou execute na raiz do repositório:

```bash
python3 -m http.server 8766 --bind 127.0.0.1 --directory design/noite-2026
```

Acesse http://127.0.0.1:8766. Use mouse, Tab/Enter ou setas. Esc fecha um diálogo; fora dele retorna à home. Filtros, votos e lista são locais e se perdem ao recarregar. A busca é por título; a sugestão por intenção é explicitamente guiada, sem IA conectada.

Artes copiadas de `deploy/app/art/{00,01,02,04,06,10}.jpg`; títulos e duração vêm do catálogo local. Motivos, progresso e votos são demonstrativos; não foram consultadas disponibilidade regional nem fontes de reprodução. Manter estes arquivos como estudo local; eventual publicação exige revisar permissão das artes. Nenhum dado privado de conta foi copiado. Bitwarden estava sem autenticação; não foi necessário acessar credenciais para este estudo.

## Verificação realizada

Sintaxe de JavaScript e `git diff --check` passaram. No navegador: home revisada visualmente em painel estreito; filtro de 45 minutos retornou Widow’s Bay; dois votos produziram consenso em The Prestige; o título abriu e a lista mudou para “Na minha lista”; painel de áudio abriu a partir do player. O foco no botão de votação permaneceu após atualizar o votante. Não foi medido desempenho em TV física. A captura com viewport de 1920×1080 ficou parcialmente preta por limitação do painel de captura; não é usada como prova visual em Full HD. A configuração temporária de viewport foi restaurada.
