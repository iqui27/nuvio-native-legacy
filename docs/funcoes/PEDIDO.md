# Pedido: documentação de funções com grafos e impactos (anti-regressão)

Objetivo: documentação ROBUSTA do código para que cada mudança saiba o que pode
quebrar. Só DOCUMENTAÇÃO: não altere nada fora de docs/funcoes/. Não faça commit.

Escopo, por prioridade (faça na ordem; o que não der tempo fica listado no índice
como "pendente"):
1. Player e vídeo: src/player.c, src/video.c, src/video_android.c, src/video_tpk.c,
   src/video_tizen.c, android/app/src/main/java/space/nuvio/nativelegacy/NvPlayer.kt
2. Fontes e add-ons: src/streams.c, src/stream_parse.c, src/addons.c, src/fonteauto.c,
   src/fontepref.c, src/fonteparalela.c, src/plugins.c, src/pluginjs.c
3. Home, detalhe e Biblioteca: src/home.c, src/detail.c, src/biblioteca.c, src/catalogo.c
4. Sync e conta: src/sync.c, src/syncprog.c, src/trakt.c, src/simkl.c, src/nuvem.c,
   src/progresso.c, src/vistoep.c
5. Entrada e foco: src/segurar.c, src/ctxmenu.c, src/ctxlista.c, src/focus.c, src/tpkteclas.c
6. Plataforma: src/main.c, src/app.c, src/android.c, src/tpk.c, src/webosver.c, src/rede.c

Para CADA módulo, um arquivo docs/funcoes/<modulo>.md com:
- Para que serve (3-5 linhas) e em que plataformas roda (#ifdef NV_WEBOS, Android, .tpk, .wgt).
- Funções públicas (as do .h): assinatura, o que faz, pré-condições, fio em que
  roda (principal, rede, decode, player), travas que pega, efeitos colaterais
  (arquivos gravados, eventos, estado global).
- Estado global (static) que o módulo guarda e quem o lê/escreve.
- Grafo de chamadas em Mermaid (flowchart): quem chama este módulo e quem ele chama,
  só o que der para provar com grep; marque arestas por ponteiro de função/callback.
- Fluxo principal em Mermaid (sequenceDiagram) quando houver ordem que importa
  (ex.: abrir fonte -> conferir -> tocar; KEYDOWN -> segurar -> KEYUP).
- IMPACTOS: "se você mexer em X, confira Y" — invariantes, ordem de chamada,
  limites de buffer, contratos com Kotlin/.NET/JS, e QUAIS TESTES em tests/ cobrem
  (nome do .sh/.c/.py). Diga o que NÃO tem teste.
- Regressões já acontecidas: procure no git log (git log --oneline -- <arquivo>) e em
  docs/issues/mapa.json / MAPA.md por consertos que tocaram o módulo; cite hash e issue.

Regras:
- Toda afirmação aponta arquivo:linha. Sem certeza = escreva "SUSPEITA".
- Português, direto. Nada de prosa de marketing.
- docs/funcoes/README.md: índice com um grafo Mermaid de módulos (quem depende de
  quem), a lista do que foi feito e do que ficou pendente, e como manter atualizado
  (quem mexe no módulo atualiza o .md no mesmo commit).
