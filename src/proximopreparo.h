#ifndef NV_PROXIMOPREPARO_H
#define NV_PROXIMOPREPARO_H

#include "fontecache.h"

// A proxima reproducao tem uma lista e um orcamento proprios. Nada daqui
// instala streams, abre o player, troca legendas ou escreve progresso.
// Todas as chamadas publicas sao do fio da UI, salvo o cancelamento atomico
// interno dos workers. encerrar() espera rede somente no fechamento do app.
typedef struct {
  Stream *fontes;
  int n, indiceFonte;
  int catalogo, temporada, episodio;
  FonteFalhaSessao *politica;
  unsigned token;
  uint64_t sessaoPerfil;
  Uint32 idadeMs;
} ProximoPreparoResultado;

// O posplay decide o proximo ID e a janela antes dos creditos.
// Captura addons/configuracao/conta/perfil antes de qualquer worker. Uma
// segunda chamada para o mesmo alvo apenas observa o trabalho em andamento.
int proximopreparo_agendar(int catalogo, int temporada, int episodio,
                          const char *atual, const char *alvo,
                          const FontecacheEscopo *escopo);
// Por quadro, inclusive enquanto Next espera. Valida o contexto, recebe os
// metadados, captura o resolvedor no fio da UI e prepara apenas o video nativo
// do proximo. O episodio atual continua sendo o dono do player/legendas.
void proximopreparo_passo(const char *atual);
int proximopreparo_em_curso(const char *alvo);
// Perto do fim: conserva buffer pronto/carregando ainda valido. 1 = aceitou
// a unica renovacao/tentativa nativa; 0 = preservou, adiou ou recusou. A mesma
// politica/limites continuam; uma falha real segue a proxima candidata.
int proximopreparo_renovar(const char *atual, const char *alvo);
// ANTES de player_encerrar/player_abrir. Consome uma vez so, por ID e contexto
// exatos. indiceFonte=-1 e esgotado; -2 exige continuar a MESMA politica no
// roteador (ex.: link envelhecido). Mesmo sem fonte valida, entrega a politica
// ja gasta para nao recomecar seu limite. O chamador passa a ser o dono.
// sessaoPerfil acompanha a lista/token: validar o mesmo ator antes de adotar.
int proximopreparo_consumir(const char *alvo, ProximoPreparoResultado *saida);
void proximopreparo_liberar_resultado(ProximoPreparoResultado *resultado);
// Recuar, Stop/Back, outro alvo ou conta/perfil/configuracao novos descartam
// tanto o buffer pronto quanto workers tardios, sem esperar HTTP na UI.
void proximopreparo_cancelar(void);
// O roteador captura ao armar Next: recuo/Back/Stop invalidam tambem a
// intencao que aguardava este worker, em vez de abrir um Next frio depois.
unsigned proximopreparo_geracao(void);
void proximopreparo_encerrar(void);

#endif
