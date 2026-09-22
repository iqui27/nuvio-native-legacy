// Cache local da ordem de catalogos que vem da conta.
//
// A RPC e somente leitura no nativo: a TV recebe a ordem do web, mas nao tem
// tela para envia-la de volta. Guardar a ultima resposta aceita e o que faz a
// preferencia sobreviver a uma atualizacao do pacote ou a um boot sem rede.
#ifndef NV_CATORD_CACHE_H
#define NV_CATORD_CACHE_H

// Carrega o cache do perfil, se existir e pertencer ao usuario informado.
// Devolve 1 quando uma ordem valida mudou o estado do catordem.c.
int catordem_cache_carregar(int perfil, const char *usuario);

// Guarda uma resposta remota que catordem_ler() acabou de aceitar.
// Devolve 1 quando o arquivo foi gravado.
int catordem_cache_gravar(int perfil, const char *usuario, const char *resposta);

// Remove os caches de todos os perfis. Chamado ao sair para nao vazar a ordem
// da conta anterior para a proxima conta neste aparelho.
void catordem_cache_esquecer(void);

#endif
