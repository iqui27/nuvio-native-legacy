// TROCA DE MOTOR EM PAUSA (revisao PR734CD-r1, bloqueio 2): motor novo ligado,
// placa ASS em tela, video pausado — desligar a chave dos Ajustes apagava a
// legenda ate o proximo dialogo. A invalidacao da confirmacao so consumia um
// assTexResetar pendente: a epoca e o ultimo pedido ficavam, o "render so em
// mudanca" suprimia o pedido no mesmo pts e o caminho antigo nao tinha o
// quadro ja consumido pelo novo. Os recursos do motor que saiu so eram
// liberados se o desenho caisse de novo no ramo dele.
//
// assrender_motor_invalidar (o que a confirmacao chama agora) sobe a epoca,
// zera o ultimo pedido — o mesmo pts renderiza de novo — e solta quadro e
// texturas dos DOIS caminhos na hora.
//
//   bash tests/assmotor_pausa.sh
#include "../src/assrender.c"
#define main main_ass_area
#include "ass_area.c"
#undef main

/* O leitor da chave como a thread de render o tem: atomico, como o espelho
 * publicado pelo ajustes.c (defeito 1). */
static int ligado = 1;
static int ler_motor(void) { return __atomic_load_n(&ligado, __ATOMIC_RELAXED); }

int main(void) {
  size_t tamanho;
  char *fonte = ler("deploy/app/fonts/InterDisplay-Regular.ttf", &tamanho);
  if (!fonte) return 2;
  ass_novo_registrar(ler_motor);
  assrender_geracao(11);
  assrender_limpar_fontes();
  assrender_adicionar_fonte("InterDisplay-Regular.ttf", fonte, tamanho);
  free(fonte);
  if (assrender_carregar(DOC43, strlen(DOC43), 11) != 1) return 2;
  assrender_definir_layout(240, 0, 1440, 1080, 640, 480, 1.0);
  int novo = caixa(2.0);
  /* O ponto do player antes de desenhar a legenda (player.c). */
  assrender_aplicar_invalidacao();
  (void)caixa(2.0);

  /* Pausa com a placa em tela e a chave vira: e a troca dos Ajustes. */
  __atomic_store_n(&ligado, 0, __ATOMIC_RELAXED);
  assrender_motor_invalidar();
  int antigo_pausado = caixa(2.0);
  int motor_pausado;
  pthread_mutex_lock(&assTrava);
  motor_pausado = assApMotor;
  pthread_mutex_unlock(&assTrava);
  int antigo_mesmo_evento = caixa(2.2);
  int antigo_outro_evento = caixa(5.5);
  printf("novo=%d antigo pausado=%d motor do worker em pausa=%d\n",
         novo, antigo_pausado, motor_pausado);
  printf("antigo mesmo evento=%d antigo outro evento=%d\n",
         antigo_mesmo_evento, antigo_outro_evento);
  assrender_limpar();
  assrender_aplicar_invalidacao();
  printf("apos fechar a faixa no motor antigo: quadro novo=%zu bytes, cache novo=%zu bytes, texturas=%d\n",
         assMain.bytes, assTcBytes, assTcN);
  if (!novo || !antigo_outro_evento) return 2;
  if (!(antigo_pausado > 0 && motor_pausado == 0 && antigo_mesmo_evento > 0)) return 1;
  /* O motor que saiu soltou o dele sem voltar ao ramo novo. */
  return assMain.bytes == 0 && assTcBytes == 0 && assTcN == 0 ? 0 : 1;
}
