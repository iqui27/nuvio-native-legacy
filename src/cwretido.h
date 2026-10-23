// UM TITULO, UM LUGAR (dono, 03/10: "o que ta no continue playing nao pode ta
// no continue watching; so entra la quando sair dessa fileira" e "ou ele ta na
// ilha, ou ta no resume playing, ou no continue watching").
//
// Ao sair do player no meio, o titulo vai para o CARTAO DA ILHA (relogio
// ligado, ilhacart.c) ou para a faixa "Retomar agora" (relogio desligado,
// home.c FILEIRA_RETORNO). Enquanto um deles o segura, a fileira "Continuar
// assistindo" da home (e a secao "Continuar" dos Salvos) NAO o mostra. Quando
// o cartao/faixa o solta (dispensado, 30 min sem tecla, outro titulo no
// lugar, conta trocada, titulo terminado, relogio trocado), ele volta a valer
// na fileira — e app.c o poe na FRENTE (desc_continuar_otimista).
//
// SO EXIBICAO. O catalogo publicado continua com o item (progresso, Trakt,
// conta e a faixa "Retomar agora" leem dele); quem desenha a fileira pula a
// identidade retida. Mesma identidade de cwfrente/montarContinuar: o id base
// (idbase.h) — numa serie, qualquer episodio da obra sai.
//
// Fio de desenho apenas (app.c define; home.c e salvospainel.c leem).
#ifndef NV_CWRETIDO_H
#define NV_CWRETIDO_H
#include "idbase.h"

// Quem segura agora: com o relogio, o cartao da ilha; sem ele, a faixa.
static inline const char *cw_retido_escolher(int relogio, const char *vivo, const char *retomar) {
  const char *s = relogio ? vivo : retomar;
  return s ? s : "";
}

// 1 quando a identidade retida mudou (e a revisao andou).
int cw_retido_definir(const char *imdb);
// O id base retido ("" quando nada).
const char *cw_retido(void);
unsigned cw_retido_rev(void);
// 1 quando `imdb` e a mesma obra que a retida (sempre 0 com "tambem em
// Continuar assistindo" ligado: a ilha/faixa o seguram e a fileira o mostra).
int cw_retido_exclui(const char *imdb);
// Ajustes -> Home -> "Tambem em Continuar assistindo". Anda a revisao quando
// muda, para a home e os Salvos remontarem a fileira.
void cw_retido_tambem_definir(int ligado);
#endif
