# Nuvio — Universo pessoal e social

Proposta exploratória de 10/09/2026, criada após a rejeição do primeiro estudo. Preserva a composição das referências: arte cinematográfica em tela cheia, logos dos títulos, ação branca, botões circulares, cards horizontais com gradiente, elenco circular e posters. As telas novas usam o mesmo vocabulário visual.

## Plano visual

Preto #0b0b0c, superfície #1c1b20, texto #f7f7f7, secundário #b9b7bf, social lavanda #cbb7eb, progresso branco #ffffff. Inter Display do próprio app. Margem esquerda ampla; informação alinhada à esquerda; arte ocupa o lado direito do hero. Roxo discreto deriva da área social das capturas. Não reaproveitar o azul do estudo rejeitado.

Composição base:

    marca   início  amigos  universo  jornada    perfil
    logo do filme                     arte original
    contexto + sinopse
    assistir  +  momentos
    continuar assistindo em cards horizontais
    pessoas e recomendações pessoais

A revisão do plano removeu dashboard genérico de métricas, pontuações de produtividade e feed social infinito. A descoberta social aparece associada a filmes, episódios e motivos pessoais. O perfil se expressa por imagens, não por um placar de horas assistidas.

## Telas e interações

- Início: base atual com retomada, cards e entrada de recomendação de um amigo.
- Detalhes: recomendações por aspecto do título, universo até o episódio, personagens e momentos.
- Entre amigos: escolher algo em comum, votar localmente, recomendações com motivo e conversa com barreira de episódio.
- Meu universo: mosaico de gosto, retrato mensal, memórias marcadas e controles de privacidade/modo visita.
- Minha jornada: mapa visual de episódios, séries em andamento, pausadas e concluídas; pausar e retomar sem perder progresso.
- Player: simulação de linha do tempo, repetir fala com legendas temporárias, guardar momento, painel de personagens, retorno após pausa e encerramento de sessão.

## Dados e limites

Todas as pessoas, atividades, progresso, estatísticas, recomendações e comentários são fictícios de demonstração. Artes e logos vêm dos arquivos do app; não representam consulta atual a provedores. Nenhuma mensagem é enviada. Preferências e votos ficam apenas na memória da página; recarregar reinicia. O player usa imagem estática e relógio simulado, sem áudio/vídeo. “Universo até aqui” demonstra navegação e barreira por episódio, não geração de conteúdo. A sincronização social, catálogo editorial, detecção de falas/cenas e estatísticas reais exigem serviços e validação antes da implementação nativa.

Comentários de exemplo não revelam trama. Proteção por episódio é conservadora: libera após concluir o episódio inteiro. Histórico privado por padrão; modo visita suspende a gravação na demonstração. Dados são marcados como ilustrativos na moldura do protótipo.

## Execução

`python3 -m http.server 8767 --bind 127.0.0.1 --directory design/universo-2026`

Abrir http://127.0.0.1:8767. Mouse ou Tab/setas e Enter. Esc fecha painel ou volta à home. HTML de estudo, sem alteração do app C/SDL2, e sem reivindicação de desempenho em TV.

## Verificação

JavaScript passou em `node --check` e diff sem erros de whitespace. Revisão visual no navegador de detalhes, perfil e social, incluindo captura ampla. Verificados: três votos geram consenso; pausar Fallout o move para “Para outro momento” e o remove da prateleira; guardar momento em 15:10 produz marcador 00:15:10 no perfil. Imagem estática e dados de demonstração permanecem identificados. Não houve teste em televisão física.

## Expansão dos detalhes: conteúdo

Após aprovação da direção visual, foram adicionadas as abas Por dentro, Imagem e som e Trilhas, preservando Conexões, Universo até aqui e Momentos. `content.js` reúne 12 estudos editoriais de demonstração por título, temas marcáveis, guias antes/depois da sessão, paletas propostas a partir das artes e trilhas entre obras. Essas conexões são interpretações para o protótipo, não relações de franquia nem descrições oficiais de produção. Imagens são artes de catálogo, não fotogramas. Áudio, entrevistas e bastidores permanecem espaços de curadoria explicitamente identificados, sem vídeos inventados. Notas e trilhas duram apenas nesta página.

Verificados no navegador: guardar tema, alternar para Imagem e som, guardar trilha e navegar de Fallout para o conteúdo específico de 3 Body Problem. Revisão visual da área Imagem e som concluída. Sintaxe dos dois scripts e diff sem problemas de whitespace.
