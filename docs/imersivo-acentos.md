# Imersiva: acentos alem do vidro tingido (proposta, 14/10/2026)

Pedido do dono: "Imersivo tem que repensar o que e imersivo. Pq nao sei se o
vidro tinted e muito premium, temos que pensar em outros accents no imersivo
ao longo da interface."

Hoje o "Imersiva" (modo 4 de Cor de destaque) faz duas coisas: a luz ambiente
de quatro regioes (GFX_AMBIENTE) e o vidro tingido com 16% do matiz do
destaque (gfx_vidro_matiz). E so isso: o resto da interface continua cinza.
A proposta e espalhar o MESMO matiz da luz (nv_luz_viva, L 0,42 croma <= 0,11
— sempre escuro o bastante para o texto branco por cima) em elementos que hoje
sao cinza, na exata medida do nv_ambiente_forca (entram e saem junto com a
luz, sem novo ajuste):

1. **Barra de progresso do episodio (detalhe)** — o preenchimento cinza
   rgb(158,158,158) ganha o matiz da luz. IMPLEMENTADO (detail.c).
2. **Trilho do progresso do player** — o mesmo tratamento na barra fina da
   retomada, onde ja ha procentagem.
3. **Chips em repouso** — a peca sem texto de "Pilulas: chips" leva uma lavagem
   de 8-12% do matiz (o foco ja e o acento cheio).
4. **Linha de rolagem** — a barra fina que indica a fileira atual (rail)
   lavada no matiz, alfa junto do forca.
5. **Dinamica (home)** — o topo de vidro da peca central pode levar a mesma
   lavagem do vidro tingido, ja que e a unica ilha que nao passa por
   gfx_vidro_matiz.

Regra de ouro para todos: so o MATIZ da luz (nunca o acento cheio — o botao
continua sendo o lugar da cor viva), contraste do texto branco preservado
(L 0,42 no pior caso), e nada novo em cima de arte (so sobre o fundo #0D0D0D
e o vidro). O vidro tingido em si NAO sai: a duvida "se e muito premium" se
responde na pratica, com os acentos dando o contraponto.
