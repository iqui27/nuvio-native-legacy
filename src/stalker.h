// PORTAL IPTV STALKER (Ministra), como mais uma fonte de canais do guia.
//
// O QUE E: portais Stalker/Ministra falam um protocolo proprio de set-top box
// — handshake por MAC, token de sessao, lista de canais paginada por genero, e
// um link de reproducao pedido UM DE CADA VEZ, na hora de tocar. Nada disso e
// Stremio; nao passa por addons.c nem por manifesto nenhum.
//
// POR QUE COUBE BARATO: o guia (guia.c) ja sabe montar grade por categoria,
// favoritos, zap de CH+/-, overlay sobre o player e salto de categoria. O EPG
// (epg.c) ja casa a grade XMLTV por nome. Este modulo so ENTREGA canais na
// mesma forma e resolve a URL quando pedem — o resto da tela ja existia.
//
// AS TRES COISAS QUE TORNAM ISTO DIFERENTE DE UM ADDON:
//
// 1. O LINK MORRE. `create_link` devolve uma URL valida por minutos. Ela NAO
//    pode ser guardada em lugar nenhum que sobreviva a reproducao — nem cache
//    de catalogo, nem favoritos, nem progresso. O que se guarda e o `cmd`, que
//    e estavel. E o mesmo desenho de Stream.infoHash + debrid_resolver: a URL
//    nasce vazia e so existe no instante de tocar.
//
// 2. O TOKEN EXPIRA. Toda chamada passa por um wrapper que, ao ver 401/403 ou
//    corpo sem `js`, refaz o handshake UMA vez e repete UMA vez. O mutex nao e
//    zelo: o fio do guia (lista) e o fio da reproducao (create_link) chamam
//    concorrente, e dois handshakes em paralelo invalidam o token um do outro.
//
// 3. O MAC E CREDENCIAL. Ele autentica a assinatura de quem configurou, do
//    mesmo jeito que uma senha. Mora em stalker-p<N>.txt (por PERFIL, como
//    listas.c), nunca sai em printf — registro.c le o stdout do app e o mostra
//    NA TELA, num arquivo legivel por qualquer um com acesso a TV — e nunca
//    entra no pacote nem no git. Ver a nota de ARQ_DE_PESSOA em tools/arm.sh:
//    a lista de exclusao ja deixou vazar um segredo uma vez.
#ifndef NV_STALKER_H
#define NV_STALKER_H
#include <stdint.h>

// ---------------------------------------------------------------- cadastro

// Le a configuracao do perfil ATIVO. Barato e idempotente: quem ja leu o
// perfil atual volta na hora. Chamar antes de qualquer uso.
void stalker_carregar(void);

// 1 quando ha portal e MAC configurados neste perfil.
int  stalker_configurado(void);

// Gravam a configuracao do perfil ativo, um campo por vez. Sao separados de
// proposito: a tela edita um de cada vez, e um setter unico obrigaria a camada
// de desenho a LER o MAC atual em claro so para reescreve-lo ao trocar o
// endereco. `mac` no formato 00:1a:79:xx:xx:xx.
//
// Qualquer um dos tres derruba a sessao em memoria: token e tabela de canais
// pertencem ao cadastro que os produziu.
int stalker_definir_portal(const char *portal);
int stalker_conta_definir(const char *portal,const char *mac);
void stalker_definir_mac(const char *mac);
// Opcionais: a maioria dos portais nao confere, e os que conferem pedem
// exatamente estes dois campos.
void stalker_definir_aparelho(const char *deviceId, const char *serial);

// Apaga a configuracao deste perfil e a sessao em memoria. Chamado no logout e
// na troca de perfil, como debrid_esquecer().
void stalker_esquecer(void);

// Para a tela de Ajustes: nunca devolvem o valor inteiro. O MAC sai como
// "··:··:··:··:E4:2A" e o portal sem esquema nem caminho. Os dois apontam para
// memoria do modulo, validos ate a proxima chamada.
const char *stalker_portal_curto(void);
const char *stalker_mac_mascarado(void);

// ----------------------------------------------------------------- canais

typedef struct {
  char id[80];        // "stalker:<id do canal no portal>"
  char nome[140];
  char logo[480];
  char categoria[64];
} StalkerCanal;

// Baixa generos + lista ordenada e preenche `saida`. Devolve quantos, 0 quando
// nao ha portal configurado ou quando o portal nao respondeu. BLOQUEIA: e para
// ser chamada do fio do guia, nunca do fio de desenho.
//
// Efeito colateral proposital: guarda internamente o `cmd` de cada canal, que e
// o que stalker_resolver() precisa depois. Por isso a lista tem de ser baixada
// pelo menos uma vez antes de tocar qualquer canal — o que o guia ja faz.
int stalker_canais(StalkerCanal *saida, int max);
int stalker_canais_sessao(StalkerCanal *saida, int max, uint64_t sessao);

// Stream every page before Guide selection. Return 0 from cb to stop.
// Commands use a bounded cache; playback reloads an evicted command on demand.
typedef int (*StalkerCb)(const StalkerCanal *canal, void *u);
int stalker_canais_cb_sessao(StalkerCb cb, void *u, uint64_t sessao);

// ------------------------------------------------------------ reproducao

// 1 quando o id veio deste modulo (prefixo "stalker:").
int stalker_e_id(const char *id);

// Pede um link NOVO para este canal e escreve em `url`. Devolve 0 quando o
// canal e desconhecido (lista ainda nao baixada nesta sessao) ou quando o
// portal recusou. BLOQUEIA por ~200-600 ms na TV: fio proprio, sempre.
//
// Chamar A CADA reproducao, inclusive ao trocar de fonte depois de um
// travamento. Guardar o resultado e o unico erro que este modulo nao perdoa.
int stalker_resolver(const char *id, char *url, unsigned n);
int stalker_resolver_sessao(const char *id, char *url, unsigned n, uint64_t sessao);

void stalker_fonte_selecionar(unsigned id);
int stalker_fonte_configurado(unsigned id);
int stalker_fonte_canais(unsigned id,StalkerCanal *out,int max,uint64_t sessao);
int stalker_fonte_canais_cb(unsigned id, StalkerCb cb, void *u, uint64_t sessao);
void stalker_fonte_esquecer(unsigned id);
void stalker_fontes_invalidar(void);
#endif
