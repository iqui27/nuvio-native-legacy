#include "inicio.h"

unsigned inicio_abre_prazo_ms(const InicioAbertura *g) {
  if (g && g->pesado) return INICIO_ABRE_PESADO_MS;
  if (!g || !g->proximaSemPerda || g->dadoChegando) return INICIO_ABRE_COM_DADO_MS;
  return INICIO_ABRE_SEM_SINAL_MS;
}

int inicio_abre_vencida(const InicioAbertura *g) {
  if (!g || g->tocou || g->quadro) return 0;
  return g->desdeMs > inicio_abre_prazo_ms(g);
}

int inicio_motivo(const InicioSinais *s) {
  if (!s) return INI_NADA;
  // A troca de fonte se explica na hora: a pessoa acabou de ver a imagem
  // (ou a tela) mudar e nao sabe por que.
  if (s->tentativa >= 2) return INI_FALLBACK;
  if (s->desdeMs < INICIO_EXPLICA_MS) return INI_NADA;
  switch (s->fase) {
    case INI_FASE_BUSCA:
      if (s->pendentes > 0 && s->fontesChegaram) return INI_ESPERA_VARIOS;
      if (s->pendentes == 1) return s->pendenteMudo ? INI_ESPERA_MUDO : INI_ESPERA_UM;
      if (s->pendentes > 1) return INI_ESPERA_VARIOS;
      return s->semResposta == 1 && !s->fontesChegaram ? INI_NAO_RESPONDEU : INI_NADA;
    case INI_FASE_VERIFICA: return INI_VERIFICANDO;
    case INI_FASE_ABRE:     return s->viaDebrid ? INI_ABRINDO_DEBRID : INI_NADA;
    default: return INI_NADA;
  }
}
