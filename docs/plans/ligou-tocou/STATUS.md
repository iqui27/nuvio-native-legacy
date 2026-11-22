# Ligou, tocou — canal pessoal "Minha noite" (2.1)

Origem: pesquisa de mercado, seção 4.1. Decisão do dono: **opcional** — o
ajuste "Abrir tocando" nasce desligado e nada conta como assistido até um
gesto da pessoa.

## Etapa 1 (este recorte)

| Peça | Onde | O que faz |
|---|---|---|
| Grade determinística | `src/minhanoite.c` (regra pura) | Junta Continuar assistindo, séries em andamento e a lista; a ordem do dia sai da data e do id (o que está pela metade abre o dia), blocos em múltiplos de 5 min, sem rede e sem estado. Mesma conta, mesma grade em qualquer aparelho. |
| Canal no guia | `src/guiav5ui.c` (chip 1107), `src/guia.c` (chip do cabeçalho antigo) | Chip "Minha noite" ao lado de Multiview; OK sintoniza. |
| Zapping | `src/app.c`, `guia_zap_da_volta` / `guia_zap_ponta` | A Minha noite fica entre o último e o primeiro canal da vista: CH+/- na ponta da lista entra nela, CH+/- nela volta para a ponta. Sem grade, a lista dá a volta como antes. |
| Sessão passiva | `src/player.c` (`player_passivo_*`) | A sessão aberta pelo canal entra na fração do programa que a grade indica e não tem identidade para progresso, visto, Trakt/Simkl, atividade de amigos nem check-in de servidor pessoal (`identidadeValida()`). |
| OK oferece | `src/app.c` (`mnEvento`, `mnDesenhar`) | Primeiro OK abre a folha "Assistir do início" / "Assistir daqui". Só a escolha é o gesto: dali em diante a sessão conta, e o que tocou sozinho antes não entra na prova do visto. |
| Voltar | `src/app.c` (`mnAtualizar`) | Voltar fecha o player e a página do título usada como caminho: cai na tela de onde a sintonia saiu (a Home no "Abrir tocando", o guia pelo chip). |
| Reabertura | `reabrirGuardar` / `reabrirTitulo` + `player_passivo_repor` | Troca de fonte e próximo episódio reabrem o player sem perder a marca: continuam sem contar até o gesto. |
| Abrir tocando | `AJ_ABRIR_TOCANDO` (`abrirTocandoLocal`, local, desligado) | Ligado, o app sintoniza a Minha noite uma vez por abertura, com a Home parada; a primeira tecla cancela. Fica em Ajustes › TV ao vivo › Guia de TV. |
| Tempo de zap | `src/app.c` (`zapMarcar`, `zapMedir`) | `[zap] pedido: A -> B` e `[zap] A -> B: imagem em N ms` (ou `sem imagem depois de N ms`), também no zap entre canais de IPTV. |
| Porta de teste | `src/main.c` | `minhanoite`, `minhanoite:oferta`, `ch+`, `ch-` em `/tmp/nuvio-key`. |

A reprodução usa o caminho da página do título (fontes, legendas, trava de
série, perfil infantil), então a Minha noite herda as regras que já existiam.

## Verificado no Linux (vídeo de mentira)

- `bash tests/minhanoite.sh` (também com `SANITIZE=1`): grade, virada do dia,
  fuso, entrada no meio, ordem de zapping, regra do gesto.
- `bash tests/i18n.sh`: tabela mestra e as 29 irmãs conferem com as 6 chaves
  novas (a varredura de literais já falhava na base, com os mesmos números).
- No app (`NUVIO_UI_PREVIEW=1`, lista M3U local de 3 canais): chip no guia,
  aviso de canal vazio, sintonia com entrada no meio, folha do OK e o gesto,
  CH+ da Minha noite para o primeiro canal, Voltar até a Home, "Abrir
  tocando" ligado e desligado, linhas `[zap]`.

## Não verificado

- Vídeo de verdade: no Linux a sessão para na tela de fonte. A entrada no
  meio (seek por percentual), "Assistir do início" com vídeo no ar e a medida
  `imagem em N ms` só rodam numa TV.
- CH+/- de um canal de IPTV **para** a Minha noite: o zap de canal só recebe a
  tecla com o canal tocando, o que o Linux não faz. A decisão (`mn_zap`) está
  no teste; a cola em `app.c` não foi exercitada.
- Nenhuma TV física (LG, Samsung, Android).

## Limites conhecidos

- Samsung `.wgt`: o CH+ é a única AZUL e nunca zapeia (`docs/plans/teclas-ch.md`);
  ali só o CH- sai da Minha noite.
- O PiP de canal (mini player) zapeia só entre canais de IPTV.
- A duração do bloco é nominal (110 min filme, 45 min episódio): o catálogo não
  guarda a real. A entrada é uma fração do programa, limitada a 85%.
- Série da lista sem progresso entra como T1E1.

## Falta (próximas etapas)

1. Linha da Minha noite na grade do guia com "agora / a seguir" (`mn_proximo`
   já existe) e no overlay do mini guia.
2. Troca automática para o próximo bloco quando o programa acaba.
3. Pré-resolver a fonte do canal vizinho e mostrar arte enquanto abre; sem
   isso nas TVs fracas, como o relatório pede.
4. Outros canais pessoais (coleções, recomendações de amigos).
5. Medir o zap nas plataformas com as linhas `[zap]` antes de prometer número.
