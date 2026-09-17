// O despejo do cache de textura: arte QUENTE (na tela) sai por ultimo, e a
// promocao que nao termina devolve a textura pequena em vez de perde-la.
//
// POR QUE ESTE TESTE EXISTE. Log da LG de 16/09/2026: texturas=152 a 95.9 MB
// de 96 em quase toda amostra — o cache vivia no teto. O LRU cru escolhia o
// menor `uso` entre os PRONTOS, e o primeiro card desenhado num quadro e
// sempre o de menor `uso` entre os VISIVEIS: cada arte que entrava despejava
// uma que estava na tela, o desenho pedia de novo, e o ciclo era o "pisca"
// que o dono relatou. Nenhuma tela mostra "despejei o que voce esta olhando";
// a regra so se prova aqui.
#include "../src/tex_cache.c"
#include <assert.h>
#include <stdio.h>

static void limpar(void) {
  memset(itens, 0, sizeof itens);
  nMax = 8; quadroAtual = 100; bytesUsados = 0;
  tex_despejos = 0; tex_despejos_quentes = 0;
}

// Um PRONTO de 100x100 (40 KB, sem piramide para a conta ficar redonda: w>=1024
// nao tem, entao usa 1024x10). `tex` fica 0 de proposito: o teste nao tem
// contexto GL e o glDeleteTextures so roda quando ha textura.
static void pronto(int i, unsigned long uso, unsigned long quadro) {
  itens[i].estado = PRONTO; itens[i].uso = uso; itens[i].ultimoQuadro = quadro;
  itens[i].w = 1024; itens[i].h = 10;
  snprintf(itens[i].caminho, sizeof itens[i].caminho, "a%d", i);
  bytesUsados += bytesTextura(1024, 10);
}

int main(void) {
  // 1. FRIO SAI ANTES DE QUENTE, mesmo com `uso` maior. O item 0 e o mais
  //    velho por `uso` (o primeiro card da fileira) mas foi desenhado neste
  //    quadro; o 1 foi desenhado ha 10 quadros. Sai o 1.
  limpar();
  pronto(0, 1, 100);
  pronto(1, 50, 90);
  pronto(2, 60, 99);
  assert(despejar(0) == 1);
  assert(itens[1].estado == VAZIO);
  assert(tex_despejos == 1 && tex_despejos_quentes == 0);
  puts("ok  frio sai antes do quente, mesmo com uso maior");

  // 2. ENTRE FRIOS, LRU de sempre.
  limpar();
  pronto(0, 5, 10); pronto(1, 3, 10); pronto(2, 9, 10);
  assert(despejar(0) == 1);
  puts("ok  entre frios continua LRU");

  // 3. SO QUENTES: podar() NAO despeja (estoura o orcamento por um quadro em
  //    vez de apagar a tela), e slotLivre() despeja o LRU e CONTA como quente.
  limpar();
  pronto(0, 7, 100); pronto(1, 2, 99); pronto(2, 9, 100);
  assert(despejar(0) == -1);
  assert(itens[1].estado == PRONTO);
  orcamento = 1; podar();
  assert(itens[0].estado == PRONTO && itens[1].estado == PRONTO && itens[2].estado == PRONTO);
  assert(tex_despejos == 0);
  assert(despejar(1) == 1);
  assert(tex_despejos == 1 && tex_despejos_quentes == 1);
  puts("ok  so quentes: podar segura, slotLivre forca e conta");

  // 4. O QUADRO ANTERIOR ainda e quente (o bombear roda antes de o quadro
  //    avancar); dois atras nao e.
  limpar();
  pronto(0, 1, 99); pronto(1, 2, 98);
  assert(despejar(0) == 1);
  puts("ok  quente = este quadro ou o anterior");

  // 5. PROMOCAO DESFEITA devolve PRONTO com a textura pequena, sem VAZIO e sem
  //    mexer em bytesUsados — antes disso virava VAZIO com a textura orfa e os
  //    bytes dela somados para sempre.
  limpar();
  pronto(0, 1, 100);
  itens[0].tex = 77; itens[0].limite = 1920; itens[0].estado = PENDENTE;
  itens[0].urgente = 1;
  { long antes = bytesUsados;
    desistir(0);
    assert(itens[0].estado == PRONTO && itens[0].tex == 77);
    assert(itens[0].urgente == 0 && itens[0].caminho[0] == 'a');
    assert(bytesUsados == antes); }
  puts("ok  promocao desfeita volta a PRONTO com a textura antiga");

  // 6. PEDIDO NOVO desfeito vira VAZIO, como sempre foi.
  limpar();
  itens[0].estado = PENDENTE; snprintf(itens[0].caminho, 8, "x");
  desistir(0);
  assert(itens[0].estado == VAZIO && itens[0].caminho[0] == 0);
  puts("ok  pedido novo desfeito vira VAZIO");

  // 7. O DESENHO RECEBE A TEXTURA ANTIGA DURANTE A PROMOCAO quando ela tem
  //    pelo menos metade da largura pedida (poster 288 -> card aberto 544),
  //    e NAO quando e miniatura (128 -> hero 1920: seria um borrao de tela
  //    cheia; ali o certo e esperar, como antes).
  limpar();
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  pronto(0, 1, 100); itens[0].tex = 77; itens[0].limite = 288; itens[0].w = 288;
  itens[0].hash = hashCaminho(itens[0].caminho);
  assert(tex_obter_limite("a0", 288, 0, 0) == 77);
  // Promove a 544: volta a PENDENTE (a fila recebe) e o desenho SEGUE com a 77.
  assert(tex_obter_limite("a0", 544, 0, 0) == 77);
  assert(itens[0].estado == PENDENTE && itens[0].limite == 544);
  assert(tex_obter_limite("a0", 544, 0, 0) == 77);
  assert(tex_aspecto("a0") > 20.0f);    // 288/10, mesmo PENDENTE
  // Miniatura de 128 pedida como hero: sem textura ate a grande chegar.
  limpar();
  pronto(1, 1, 100); itens[1].tex = 78; itens[1].limite = 128; itens[1].w = 128;
  itens[1].hash = hashCaminho(itens[1].caminho);
  assert(tex_obter_limite("a1", 1920, 1, 0) == 0);
  assert(itens[1].estado == PENDENTE && itens[1].tex == 78);
  puts("ok  promocao: textura antiga serve se tem metade da largura; miniatura nao");

  // 8. ARTE DE PASSAGEM FRIA ALEM DE TRES SAI ANTES DO CARTAZ, mesmo sendo
  //    mais recente por `uso`. Dois herois (limite 1920) e dois quadros de
  //    sequencia (passageiro) frios, mais um cartaz frio bem velho: sai o de
  //    passagem mais velho, o cartaz fica. Com tres de passagem, volta a ser
  //    LRU comum e o cartaz velho sai.
  limpar();
  pronto(0, 1, 10);                       // cartaz, o mais velho de todos
  pronto(1, 20, 10); itens[1].limite = 1920;
  pronto(2, 30, 10); itens[2].passageiro = 1;
  pronto(3, 40, 10); itens[3].limite = 1920;
  pronto(4, 50, 10); itens[4].passageiro = 1;
  assert(despejar(0) == 1);
  assert(itens[0].estado == PRONTO);
  assert(despejar(0) == 0);
  puts("ok  arte de passagem fria alem de tres sai antes do cartaz");

  // 9. tex_obter_passageira MARCA o slot; tex_obter_larg nao.
  limpar();
  assert(tex_obter_passageira("q1", 480) == 0);
  assert(tex_obter_larg("c1", 212) == 0);
  { int q = -1, c = -1;
    for (int i = 0; i < nMax; i++) {
      if (!strcmp(itens[i].caminho, "q1")) q = i;
      if (!strcmp(itens[i].caminho, "c1")) c = i;
    }
    assert(q >= 0 && itens[q].passageiro == 1);
    assert(c >= 0 && itens[c].passageiro == 0); }
  puts("ok  passageira marca o slot; cartaz nao");

  // 10. PROMOCAO COM A FILA CHEIA TENTA DE NOVO NO PEDIDO SEGUINTE.
  //
  // O DEFEITO que este caso prende: `limite` era escrito no PEDIDO e a
  // re-decodificacao so era enfileirada se houvesse vaga na fila naquele
  // instante. Sem vaga, o pedido caia no chao com `limite` ja em 1920 — e a
  // condicao de promocao (`limite > itens[i].limite`) nunca mais era
  // verdadeira. O heroi ficava com a textura do cartaz esticada ate o item ser
  // despejado, o que numa home que nao despeja e a sessao inteira.
  limpar();
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  pronto(0, 1, 100); itens[0].tex = 91;
  itens[0].w = 544; itens[0].limite = 544; itens[0].tetoUsado = 544;
  itens[0].fonteW = 1920;   // a fonte tem mais pixels: promover vale a pena
  itens[0].hash = hashCaminho(itens[0].caminho);
  filaIni = 0; filaFim = MAX_FILA - 1;   // cheia: (filaFim+1)%MAX_FILA == filaIni
  // Sem vaga: o desenho segue com a textura pequena (melhor que cinza) e o
  // item continua PRONTO — nada foi enfileirado.
  assert(tex_obter_limite("a0", 1920, 1, 0) == 91);
  assert(itens[0].estado == PRONTO);
  filaIni = 0; filaFim = 0;              // fila vazia de novo
  // Com vaga, o MESMO pedido enfileira. O retorno vira 0 porque 544 nao chega a
  // metade de 1920: ver o caso 7 — miniatura esticada a tela cheia e pior que
  // esperar o crossfade.
  assert(tex_obter_limite("a0", 1920, 1, 0) == 0);
  assert(itens[0].estado == PENDENTE);
  puts("ok  promocao com fila cheia tenta de novo no pedido seguinte");

  // 11. FONTE ESGOTADA NAO E RE-DECODIFICADA. Cartaz da Cinemeta com 250px de
  //     origem pedido a 320: refazer devolveria os mesmos 250.
  limpar();
  mtx = SDL_CreateMutex(); cond = SDL_CreateCond();
  pronto(1, 1, 100); itens[1].tex = 92;
  itens[1].w = 250; itens[1].limite = 250; itens[1].tetoUsado = 250;
  itens[1].fonteW = 250;
  itens[1].hash = hashCaminho(itens[1].caminho);
  filaIni = 0; filaFim = 0;
  assert(tex_obter_limite("a1", 320, 0, 0) == 92);
  assert(itens[1].estado == PRONTO);
  puts("ok  fonte esgotada nao vira decode novo");

  puts("texquente: tudo ok");
  return 0;
}
