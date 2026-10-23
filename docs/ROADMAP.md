# Roadmap geral do Nuvio Native Legacy

## 2.0.3 → 2.2 (decisão do dono de 06/10/2026 e 07/10/2026)

- **2.0.3**: congelada, só o que já está em `integracao/2.0.3`. **2.0.4**: pequenos recursos e triagem de bugs que já têm branch.
- **2.1**: Watch Together, Diário + sono, Trava, Caça/Conquistas, redesign da Biblioteca, Coleções + fileiras; mais perfis com tour, "Começar", "Resolver um problema" e busca com i18n nos Ajustes (decisão de 07/10).
- **2.2**: servidores locais (Jellyfin / Plex / Emby), guia de TV (guia pro) e música (álbum da trilha, link do Spotify).
- Itens que o agente pôs no mapa sem aprovação estão marcados "suspeita" em [issues/MAPA.md](issues/MAPA.md), que é a fonte viva (gerada de `issues/mapa.json`). O conteúdo abaixo é histórico, até a 1.8.

## Planejamento vigente — 03/10/2026

A1.7.4 já está publicada. O plano atual para integrar, otimizar e entregar a1.8 está em [releases/1.8.0/PLANO.md](releases/1.8.0/PLANO.md), com [inventário Git](releases/1.8.0/INVENTARIO.md). As etapas/versões abaixo são o registro histórico de02/10 e não definem o estado atual.

Atualizado em 02/10/2026 21:39 BRT. Planejamento local sobre `release/1.7.2`, com candidata compilada de `b71471bc`. Este é o mapa entre versões; a auditoria detalhada de logs, branches e sessões fica em [releases/1.7.2/ROADMAP.md](releases/1.7.2/ROADMAP.md).

O dono rejeitou o mockup da ilha e pediu continuidade no roadmap. As quatro funcionalidades daquele mockup saem da fila. A ilha já integrada e seu bugfix de isolamento por conta/perfil permanecem. Não há novo desenho da ilha aprovado.

**Inclusão autorizada na 1.7.2:** fix do screensaver LG `8a49956`, cherry-pick `857f83e3` + ajuste `11835ffa`. Teste focado e i18n PASS; falta confirmação da API na TV e novo empacotamento. Os pacotes `b71471bc` mencionados abaixo são o lote anterior, sem essa inclusão.

## Distribuição entre versões

| Destino | Objetivo e conteúdo | Estado / condição |
|---|---|---|
| **1.7.2** | Correções, resiliência, consistência, retomada em dois minutos, Ajustes funcionais, toque/login, fontes incrementais e otimizações já implementadas. Investigar os relatos atuais de arranque, reprodução, catálogo e proporção antes de declarar os cenários resolvidos. | Código integrado e pacotes locais conferidos; validação física e triagem dos novos relatos ainda pendentes. Não é uma release publicada. |
| **1.7.3 — proposta** | Social/recomendações/reações, Discord, TopN configurável e portabilidade dos scripts Linux. Melhorias pequenas de player podem ser selecionadas por custo e compatibilidade. | Existem branches para Social/Discord; os demais itens não são implementação pronta. Selecionar escopo e revisar sobre a base estabilizada. Nenhum merge adicional autorizado apenas por este planejamento. |
| **1.8 — plugins, P2P e Glass UI** | Integrar `plugins2` e `p2p2`, cancelamento por chamada e orçamento de disco/memória; candidata visual `feat/glass-ilha`, com superfícies vidro/sólido e reorganização de Fontes/menu/painéis. | Branches candidatas, não features aceitas. Provar LG/Android/TPK; WGT não recebe motor P2P de sockets. |
| **Portes e exploração — sem versão prometida** | VIDAA, webOS3 e experimentos NaCl/Tizen 4; ideias visuais/launcher ainda não escolhidas. | Cada porte precisa aparelho/testador e matriz própria. Experimentos históricos não são pendências de merge da 1.7.2. |

1.7.3 é uma recomendação de agrupamento, não compromisso de publicar todos os candidatos juntos. Não há datas inventadas. Correção comprovada que impeça usar uma plataforma suportada continua sendo estabilização da 1.7.2; não empurrar automaticamente um bug para a versão de features.

## 1.7.2 — o que entra

**Ajustes novos já estão nesta versão:** merge `f9083b10`, 184 opções em 11 categorias, busca/avançados e navegação reorganizada. Incluídos nos pacotes locais de `b71471bc`. [Captura nativa local](releases/1.7.2/ajustes-tela-inicial.png) · [Validação e outras capturas](releases/1.7.2/VALIDACAO-LOCAL.md). Falta conferência no controle/painel das TVs.

Já integrado e exercitado por testes focados:

- **Ajustes funcionais:** categorias, busca, avançados e navegação; enum/defaults e espera de fontes preservados. `agente/ajustesux` já mesclada.
- **Fontes/retomada:** fontes incrementais e prazo de escolha (#221), última fonte com fallback, Android recebendo ponto salvo na preparação e retenção de exatamente dois minutos. `abrirrapido` inclui `ilhavolta` e `i221`.
- **Conta/offline:** cache/sessão conservados na indisponibilidade; fila durável de addons por conta/perfil, ACK exato e retomada após reinício. HTTP500 remoto não foi corrigido pelo cliente.
- **Consistência:** catálogo concorrente, selo/histórico de assistidos por identidade, rejeição de resposta antiga, cartão/modal/pedido da ilha isolados entre contas/perfis.
- **Entradas e arte:** toque/login (#216), ícone do celular embutido para atualização do núcleo Samsung. Recursos da 1.7.1, como entrada pelo celular, traduções e Seekr, são base; não contar como novo merge.
- **Screensaver LG:** assinatura no load válido; manter desligado durante reprodução efetiva e liberar em pausa/fim/erro/parada. Pedido do dono incorporado; prova física pendente.
- **Sonda/build:** validação HTTP/cabeçalhos/URL sem truncamento, corpo limitado; temporários no SSD e exclusão de arquivos pessoais. APK, dois IPKs, WGT, quatro TPKs, dois núcleos, metadados e checksums conferidos.

### O que impede chamar a 1.7.2 de pronta

| Prioridade | Casos | Trabalho necessário |
|---|---|---|
| **P0: arranque/login** | #224 LG 65UT7300LA/webOS 25 e #225 LG 50NANO75SPA (1.7.1); #223 TCL/Mali-G31/Android 11 (1.7.0); #211 UK6540 | **#224/#225, causa no pacote:** os dois `.ipk` publicados da 1.7.1 trazem `nuvio-proto` com modo `-rwx---r--` dentro do `data.tar.gz` (na 1.7.0 era `-rwxr-xr-x`). O app roda como uid 5152, sem permissão de execução, e fecha ao abrir. A C9 não denunciou porque lá o binário é trocado com `chmod 755` à mão. Origem: `cp` por cima de um `deploy/app/nuvio-proto` existente preserva o modo antigo. Os `.ipk` 1.7.2 locais (`nuvio-rel-172/final`) já saem `755`. `tools/arm.sh` agora força `755` e aborta se o modo no pacote pronto divergir. Falta instalar o `.ipk` pelo instalador numa TV não-root. Quem está na 1.7.1 não abre o app para atualizar: reinstalar pelo Homebrew Channel. **#223/#211:** sem relação demonstrada; #223 descreve tela preta com loop ativo, não crash. |
| **P1: reprodução** | Xtream #158; Android3003/2004; Samsung vídeo/áudio #188; ASS embutida | Reproduzir a mesma fonte/cabeçalhos/aparelho; verificar versão efetiva e distinguir diagnóstico de uso normal. Correção Xtream da segunda conexão já é base 1.7.1; confirmar continuidade atual. |
| **P1: catálogo e proporção** | #195/#197, histórico/selo, catálogo vazio e seleção ao rolar | Testar cenário/configuração do relator. Corridas corrigidas não comprovam todos os sintomas. Não reintroduzir canários antigos de janela/ROI. |
| **Validação de UX existente** | #144 CW/bordas/cintilação, #202 contraste, #209 idioma, #216 toque, #221 espera | Captura/controle real e mesma escala/configuração. Ajustes reorganizados não equivalem a resolver a cintilação dos posters. |
| **Gates por aparelho** | C9, Samsung TPK4/5 e6+, WGT, TCL e Android toque | Retenção 119/120 s, fonte vencida, perfil/logout, arranque, HDR, legendas e navegação. Medir velocidade na mesma mídia; fixture não prova ganho físico. |

A consulta GitHub desta atualização encontrou **23 issues abertas**, contra 20 na auditoria anterior. #223/#224/#225 são a diferença. Relato novo na 1.7.1 não prova regressão da candidata1.7.2, mas exige triagem antes de prometer resolução.

Consulta direcionada aos logs Android 19066/19107/19164 foi somente leitura (`rows_written=0`, `changed_db=false`). Confirmado: os seis erros 3003 de 19066 pertencem às tentativas A/B/C do diagnóstico Live TV; não são seis sessões ou seis filmes. Os outros dois registros não têm causa Java/HTTP detalhada no corpo centralizado. A causa de 3003/2004 continua não demonstrada. Dados privados permanecem fora do Git.

## 1.7.3 — proposta de próximo incremento

| Ordem | Recurso / issue | Origem concreta | Antes de aceitar |
|---|---|---|---|
|1 | **Social, recomendações, reações e atividade entre amigos** | `feat/social`; agrega `agente/reacao`, `agente/socialsrv`, `agente/socialui` | Revisar uma única agregadora sobre 1.7.2. Perfil/conta, compatibilidade do servidor já ativo, falha de rede, listas/reações e LG/Samsung/Android. Não fazer quatro merges redundantes nem redeployar servidor antigo. |
|2 | **Discord Rich Presence — #222** | `feat/discord-presenca` | Vincular, publicar presença, pausar/retomar, trocar perfil e desvincular em TV real; revisar armazenamento e contrato OAuth. Protocolo no Mac não é presença comprovada na TV. |
|3 | **TopN configurável — #201** | Pedido de produto; primeira fileira já corrigida | Escolher limites/paginação e medir custo em TV antiga. Não confundir contagem automática corrigida com nova opção 10/12/18/24. |
|4 | **Build Linux — #217** | Quatro sugestões do relator; nenhuma PR na auditoria anterior | GNU/BSD sed, fallback ffmpeg, subconjunto TPK e identidade NV_WEBOS; testar Linux/Mac e preprocessing por alvo. Não duplicar contribuição externa sem verificar. |
|5 | **Melhorias pequenas do player — parte dos pedidos #195** | Ajustes de legenda/logo como candidatos de produto | Escopo individual, capacidade do backend e revisão visual do dono. Não incluir automaticamente thumbnails/media server nesse grupo. |
|6 | **Triagem/observabilidade — #192** | Agregador atual mistura diagnósticos, versões e códigos auxiliares | Separar tentativa/contexto/versão/erro real; causas seguras sem URL, credencial ou nome de mídia privado. Não transformar ausência de despedida em crash comprovado. |

Ícones/launcher (`agente/icones`) e alternativas A/B (`agente/ajustesvisual`) continuam sem direção aprovada; não são entregas prometidas da 1.7.3. O mockup rejeitado da ilha não volta como dependência desses itens.

## 1.8 — plugins, P2P e Glass UI

| Frente | Candidato | Escopo e critério |
|---|---|---|
| **Glass UI / painéis como ilhas** | `feat/glass-ilha` (`c5269ed4`) | Destino 1.8 indicado pelo dono. Portar a camada visual sobre a base estabilizada, preservando Ajustes funcionais, fontes incrementais #221, enum/defaults e identidade. Vidro/sólido, D-pad/toque, contraste, movimento reduzido e orçamento GPU/memória em C9/TCL/Samsung. |
| **Plugins — #134** | `agente/plugins2` | Integração incremental sem repetir #221; scraper cancelável, prazo/limite de resposta, isolamento de perfil e matriz LG/Android/TPK/WGT conforme capacidade. |
| **P2P — #171** | `agente/p2p2` | Motor e fallback nos alvos que o suportem; cancelamento real, cache/disco limitado, limpeza, memória e validação TPK real. WGT fica fora do motor de sockets. |
| **Pré-cache e abertura normal em paralelo** | Trabalho futuro, não novo merge demonstrado | Depende de cancelamento por chamada, efeito do GETRange/debrid e orçamento. Última fonte guardada já abre em paralelo na 1.7.2. Nenhuma garantia de nova mídia instantânea. |
| **Media server/thumbnails — pedidos #195** | Avaliação, sem código demonstrado nesta auditoria | Definir contrato, fonte dos frames, custo de rede/disco/GPU e cancelamento. Pode aproveitar essa infraestrutura, mas não é promessa automática da 1.8. |

### Glass UI: conteúdo e unidade de integração

Em 02/10/2026 o dono indicou `feat/glass-ilha` para 1.8. Conferência Git nesta rodada: ponta `c5269ed4`, com `feat/social` (`46e9b24f`) como ancestral. Há **12 commits não-merge próprios sobre Social**, alterando 59 arquivos; o diff amplo contra 1.7.2 também inclui Social e não representa somente a camada visual.

Conteúdo demonstrado nos commits:

- Folha de Fontes como ilha: grupos por resolução/HDR/SDR, filtros Em cache/Dublado, Melhor para esta TV, navegação entre abas e título do conteúdo.
- Menu/rail e painel Salvos/Social como superfícies vidro/sólido; abas segmentadas e foco por superfície.
- Menu do cartaz, estilo de fileira, Spotlight, Agenda, Biblioteca, Perfil/estatísticas no mesmo padrão.
- Arte/ícones, traduções e fixtures de captura associados.

**Dependência:** se Social entrar na 1.7.3, revisar depois somente o delta visual sobre essa base. Se Social não entrar, separar/revisar a parte que depende de Salvos/Social antes de integrar; não puxar Social/servidor automaticamente pelo merge dessa branch. Também não substituir os arquivos atuais por cópias da árvore 1.7.1: a 1.7.2 tem Ajustes, sincronização, fonte parcial e isolamento mais novos.

O destino 1.8 é planejamento, sem merge nesta rodada. Exige revisão visual do dono e prova nas TVs; relato externo de folha de Fontes na TCL não valida toda essa UI nem LG/Samsung. Este trabalho nativo é distinto do mockup HTML da ilha do relógio rejeitado; aquele mockup permanece fora da fila.

Não usar `agente/plugins` nem `agente/p2p`: são versões antigas supersedidas pelos candidatos2. Preservar seus commits; não mesclar novamente.

## Portes, propostas e histórico

| Grupo | Destino |
|---|---|
| `feat/vidaa` / #135 | Frente independente até existir prova no aparelho e testador. Não atribuir versão estável antes disso. |
| `webos3` | Compatibilidade dedicada em aparelho antigo; escolher baseline/budget. Não aplicar cinco patches em todas as plataformas por padrão. |
| `feat/tizen4-coop`, `nacl/app-completo`, `spike/nacl-tizen45`, `worktree-agent-a03fb2e226f125a82` | Experimentos históricos; não substituir TPK4/5 atual. Preservar; revisar somente se uma limitação atual exigir essa rota. |
| `agente/i195`, `i195b`, `i195c`, `i195d`, `i203`, `canario/tpk-audio-foco`, `canary/tpk-youtube-retoma` | Canários anteriores à correção publicada. Não compõem uma release futura só porque git cherry mostra patch+. Comparação semântica antes de reutilizar qualquer trecho. |
| `agente/guiaunicode` | Correção já adaptada à1.7.1. Sem novo merge/release. |
| `experimento/universo-2026` | Exploração por patch; 36 patches não equivalem a 36 features aceitas. Sem versão antes de separar conteúdo atual/duplicado/experimental. |
| `worktree-agent-a53ac8d8d82f17106`, `worktree-agent-a62e3f13ac3f493e5`, `worktree-agent-af383a462c2f04823` | Preservações/WIP; auditar conteúdo antes de atribuir feature ou apagar. |
| #172 ideias do fork, `agente/ajustesvisual`, `agente/icones` | Escolhas visuais futuras. Ajustes funcionais já estão na 1.7.2. Mockup da ilha rejeitado, fora da fila. |
| #190 título/episódio resolvido; #155 agradecimento | Administração, não novas funcionalidades. Respostas/fechamentos públicos dependem de autorização. |

## Ordem de trabalho e regra de publicação

1. Investigar arranque/login novos e reproduções pendentes da 1.7.2; preservar candidata atual e seus hashes enquanto não há fix novo comprovado.
2. Fechar matriz curta nos aparelhos. Fix novo recebe teste focado e novo pacote identificado; sem refazer suíte longa sem necessidade.
3. Dono escolhe o conjunto da 1.7.3; revisar unidades de integração Social/Discord separadamente e conservar o servidor externo atual.
4. Preparar contratos e provas da 1.8 para plugins/P2P; portar/revisar Glass UI após resolver a dependência de Social e conservar os fluxos da 1.7.2.
5. Publicar/responder issues somente após validação e autorização. Tudo deste planejamento permanece local.

Não há merge pendente de `abrirrapido`, `ilhavolta`, `i221`, `i216`, `ajustesux`, `offline`, `i212`, `i213a`, `i213b`, `quedas171`: integração já conferida na auditoria. Mapeamento detalhado das 29 refs e sessões está no documento de auditoria; não contar as branches Social agregadas ou experimentos equivalentes mais de uma vez.
