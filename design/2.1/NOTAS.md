# Nuvio 2.1: mockups do redesign

Mockups estáticos em HTML, sem código do app. Abra `index.html`. Cada tela é um quadro de TV 1920×1080 que escala para caber na janela. As setas do teclado movem o foco como o D-pad. A barra no canto troca o acento e o material (vidro ou sólido).

Títulos, artes, códigos e QR são inventados. O QR não codifica nada.

## Princípios

1. **O app explica o que fez.** Fonte escolhida, origem de cada fileira, o que foi importado, por que um vídeo falhou: tudo aparece em uma linha legível, no lugar onde a dúvida nasce.
2. **Padrão pronto, ajuste depois.** Toda escolha vem preenchida. A pessoa só age onde não dá para adivinhar (o nome do perfil, o login no celular).
3. **Um título, uma vez.** Trakt, Simkl, Conta Nuvio e add-on se juntam. As marcas T/S/N/A dizem onde o título está.
4. **Digitar é no celular.** Login e texto longo vão por QR ou código curto. A TV só confirma.
5. **Foco do norte "Cinema contemporâneo".** Linha e cartão em foco ficam um degrau mais claros, sem aro. Botão, chip e tecla em foco viram pílula cheia no acento. Cartaz em foco sobe com sombra. O acento é dinâmico e só marca estado. Selos de qualidade ficam em tinta branca.
6. **Nada da ilha rejeitada.** Os conceitos de `design/ilha` não foram reaproveitados. O material é o da ilha de vidro já aprovada no app (relógio e rail).

## Telas, o que muda e issues

### 1 · Home com Coleções e fileiras (`home.html`)

- Hoje: Continuar assistindo existe, mas a origem não aparece; coleções do add-on não viram fileira.
- Muda: Continuar e Próximos episódios ficam logo abaixo do destaque, com a origem ao lado do título da fileira. Coleções do AIOMetadata viram uma fileira de cartões; uma coleção fixada vira fileira própria com ordem, vistos e "Próximo". Segurar OK fixa, move ou oculta.
- Issues: #366 (ocultar vistos nas fileiras, chip "vistos ocultos"), #466 (watch_state para Continuar e Próximos). Roadmap: Coleções + fileiras.

### 2 · Biblioteca (`biblioteca.html`)

- Hoje: abas Salvos / Coleção / Listas, uma origem por vez.
- Muda: abas por estado (Para ver, Em andamento, Assistidos, Coleções, Listas). Painel de origens à esquerda, com "Todas · sem repetir títulos". Filtros em chips. Modo lista para Assistidos, com data, vezes e origens; quando Trakt e Simkl discordam, a linha mostra e resolve ali.
- Issues: #393 (origem da biblioteca Trakt ou Simkl), #352 (marca de visto e ordenação), #466. Roadmap: Biblioteca redesign.

### 3 · Começar (`comecar.html`)

- Hoje: "Começar" é um capítulo do guia nos Ajustes; não há primeiro uso guiado.
- Muda: 4 passos. Perfil; trazer add-ons e biblioteca (Conta Stremio, AIOManager, Conta Nuvio ou do zero) com login no celular; trackers com padrões preenchidos; resumo do que foi feito sozinho, inclusive o que a TV aceita, e tour opcional de 1 minuto.
- Issues: #393 (escolha do tracker padrão), #313 (DTS para 5.1 compatível aparece no resumo). Roadmap: Começar, perfis + tour.

### 4 · Título e Fontes (`titulo.html`)

- Hoje: o botão toca sem dizer o quê; a lista de fontes vem crua, com cópias repetidas.
- Muda: uma faixa "Vai tocar" abaixo dos botões diz resolução, HDR, áudio, idioma, tamanho e cache. Outra linha diz o que dá para pular no episódio e de onde vêm as marcas. Cada episódio tem as marcas A/R/C. A folha de Fontes abre com a escolha automática e cada critério, o que ficou de fora e as cópias agrupadas.
- Issues: #406 (notas do add-on), #457 (informação da fonte já resolvida), #458 (fontes duplicadas), #349 (preferência MP4 e P2P), #400 (ordem do add-on, no chip Ordem), #430 (origem das marcas de pular).
- Não desenhado: o Ajuste de #337 (a tela mostra a cobertura de pular, não o Ajuste) e o de #434 (esconder gêneros). Os dois são uma linha em Ajustes.

### 5 · Resolver um problema (`resolver.html`)

- Hoje: um capítulo de texto no guia; o envio de log não devolve nada à pessoa.
- Muda: a pessoa escolhe o sintoma. O app testa na ordem em que o vídeo passa (internet, add-ons, debrid, fonte, TV, áudio), para no primeiro que falha e oferece o conserto. O envio de registro devolve um código curto, lista o que vai e o que não vai e guarda os registros enviados.
- Issues: #436 (ver registros enviados). Roadmap: Resolver um problema.

## Fora deste lote

Watch Together, Diário + sono, Trava, Caça e Conquistas, busca nos Ajustes, árabe (#250, #333), botões do player (#403, #460), progresso parcial no Simkl (#459), sincronia automática de legenda (#374).

## Perguntas em aberto

1. **Menu.** Fica o rail à esquerda ou entra a barra no topo de #461? Os mockups mantêm o rail. Na página do título ele some, o que também evita #407.
2. **Começar é obrigatório?** Proposta: aparece só na primeira abertura, com "Pular este passo" em 2 e 3. Quem já usa o app ganha só o tour.
3. **Origem padrão.** Com Trakt e Simkl ligados, quem alimenta o Continuar? Proposta: Trakt. E o "+" salva nos dois?
4. **Coleções na Home.** Entram todas como uma fileira "Coleções" e a pessoa fixa as que quer, ou as mais assistidas já chegam fixadas?
5. **Diferenças entre trackers.** O app só mostra e a pessoa resolve (como no 2B), ou corrige sozinho em um sentido?
6. **Registro.** 30 dias de guarda serve? O código basta, ou o registro deve abrir também por QR no celular?
7. **Testes guiados.** Podem tocar 2 segundos da fonte que falhou para confirmar o motivo, ou só leem o último erro?
8. **Marcas A/R/C nos episódios.** Aparecem sempre ou só com "Pular" ligado? Com muitos episódios pode pesar a busca de marcas.
9. **Nome do arquivo na folha de Fontes.** Mostrar o nome cru ajuda quem entende e assusta quem não entende. Trocar por uma frase ("WEB-DL · 4K Dolby Vision") e deixar o nome no detalhe?
10. **AniList.** Entra como terceiro tracker no passo 3 já na 2.1?
11. **Perfil infantil / filtro de idade.** O app não tem filtro de classificação (catálogo nem reprodução), então a opção "Perfil infantil" saiu do Começar para não prometer o que não se cumpre. Você decide: construir um filtro parental/de idade ("só classificação livre e 10 anos") e então devolver a opção ao passo 1, ou deixar fora da 2.1?
