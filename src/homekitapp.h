#ifndef NV_HOMEKITAPP_H
#define NV_HOMEKITAPP_H
// KIT DE HOME no app: baixa, guarda por perfil e poe as fileiras na Home.
// A regra (formato, tetos, o que e recusado) e homekit.h; aqui fica so a cola:
// rede num fio proprio, arquivo `kit-home-p<perfil>.json` e as pastas extras
// de colecoes.c (col_extra_definir_grupo), que a Home ja desenha.
//
// Um kit por perfil, neste aparelho. Nao vai para a conta. Remover o kit
// (homekit_remover, ou importar endereco vazio) devolve a Home de antes.
#include "homekit.h"

// Le o kit guardado do perfil ativo. Chamar depois de perfis e de colecoes.
void homekit_iniciar(void);
// Uma vez por quadro, no fio principal: recolhe o download que terminou e
// relê o kit quando o perfil ativo mudou.
void homekit_passo(void);

// Comeca a baixar `url` (http/https). "" ou so espacos REMOVE o kit atual.
// Nao bloqueia; o resultado sai em homekit_estado/homekit_estado_texto.
void homekit_importar(const char *url);
void homekit_remover(void);

// O mesmo caminho sem rede (teste e homekit_passo): valida, guarda e aplica.
HkResultado homekit_aplicar_json(const char *json);

int homekit_tem(void);
const char *homekit_nome(void);    // "" sem kit; o nome do kit, ou "Kit de Home"
int homekit_baixando(void);
// Texto do ultimo resultado, em portugues (chave de i18n); NULL quando nao ha
// nada a dizer alem do nome do kit.
const char *homekit_estado_texto(void);
// O que ficou de fora na ultima importacao (fontes + pastas + artes).
int homekit_ignorados(void);
#endif
