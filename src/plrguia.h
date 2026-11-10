// A GUIA PARENTAL NASCE DA ILHA DO RELOGIO (Glass UI, pedido do dono 04/10:
// "colocar a indicacao de conteudo do titulo tambem aparecer da ilha do
// relogio; hoje ele ja aparece no mesmo lugar so que separado").
//
// Era uma ilha propria desenhada logo abaixo da pilula da hora. Agora e um
// pedido a plrilha: a pilula vira o cabecalho ("Guia parental · 12", mais a
// hora) e cresce, com a mesma mola das folhas de Audio e Legendas, ate o corpo
// com as linhas "categoria ... gravidade". Yields to loading, errors and
// interactive panels; ordinary notices wait for the guide to finish.
#ifndef NV_PLRGUIA_H
#define NV_PLRGUIA_H

// Chamada a cada quadro enquanto a janela da guia esta aberta. `tg` = segundos
// desde que abriu (escalona a entrada das linhas); `osd` = alfa do OSD (o veu
// de cima so entra sem ele); `classificacao` = a faixa etaria do titulo ("12")
// ou "". Le as linhas de parental.h.
void plrguia_pedir(float tg, float osd, const char *classificacao);

#endif
