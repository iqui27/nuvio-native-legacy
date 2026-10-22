#ifndef TIRARREMOTO_H
#define TIRARREMOTO_H
// Os pedidos remotos de "tirar de Continuar assistindo" e de "marcar como
// assistido" (sincronos: chamar de fio de trabalho).
//
// `ocultar` = 1 SO no gesto explicito "Tirar de Continuar assistindo": ele
// esconde a serie no progress_watched do Trakt (vale para todos os aparelhos e
// para episodios futuros). Marcar como assistido NAO e pedido para esconder a
// serie: passa 0 e so apaga a retomada (playback do Trakt + syncprog da conta).
void tirarremoto_executar(const char *imdb, const char *chave, int ocultar);
#endif
