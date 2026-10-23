// OS SINAIS DA ILHA DO RELOGIO — quem liga o que o app ja sabe aos avisos da
// pilula (ilha.h). Mockup aprovado pelo dono em 02/10 (design/ilha), listas
// (a) e (b) de design/ilha/oportunidades.md.
//
// POR QUE UM MODULO SO: cada sinal mora num lugar (recomenda.c, traktauth.c,
// debrid.c, sync.c, perfis.c, rede.c, addons.c), e espalhar "uma vez por
// evento" por todos eles seria ensinar cada um a falar com a ilha e a lembrar
// do que ja disse. Aqui fica a SONDAGEM (barata: leitura de estado, sem rede,
// no maximo a cada 2 s) e a memoria do que ja foi dito; os modulos so expoem o
// estado que ja tinham. Tambem e daqui que sai o que cada botao de modal faz.
//
// O QUE ESTA LIGADO (texto, tipo, prazo e chave do mockup):
//   pedido de amizade   ACENTO  6 s "pedido:<pub>"  modal Aceitar / Recusar;
//                       "dito" (avisodisp) so depois de APARECER (ilha_mostrou)
//   amizade nova        OK      6 s "amigo:<id>"    (contatos que entraram)
//   Trakt desconectado  ERRO    9 s "trakt"         modal Reconectar / Depois
//   debrid sem plano    ERRO    8 s "debrid-plano"  uma vez por servico/sessao
//   fonte baixando      INFO    9 s "debrid-baixa"  dito na home depois do Voltar
//   sync falhou         ERRO    6 s "sync"          uma vez por falha (rearma no ok)
//   sincronizando       atividade, so depois de 3 s rodando
//   perfil trocado      INFO    3 s "perfil"        com o rosto do perfil
//   sem internet        ERRO   fica "rede"          ate voltar
//   internet de volta   OK      3 s "rede"          a mesma chave: troca no lugar
//   addon fora do ar    ERRO    6 s "addon:<nome>"  uma vez por queda do addon
// FORA, DE PROPOSITO: o PROGRESSO do torrent no debrid ("TorBox baixando Duna
// 12%", estado 29 do mockup). Os servicos expoem o numero (TorBox mylist,
// Real-Debrid torrents/info), mas debrid.c nao guarda o id do torrent depois
// do DEBRID_BAIXANDO, e perguntar de novo pelo caminho que existe
// (debrid_resolver_escolhido) RE-ADICIONA o magnet a cada volta. Fazer direito
// e uma sonda nova por servico, com o id guardado e um intervalo que nao
// encoste no limite de pedidos de cada um — fica para quando houver como
// provar numa conta de verdade. Hoje a ilha diz uma vez que ficou baixando.
//
// Os da CENTRAL (recomendacao, episodio novo, versao, aviso do dono, queda)
// moram em avisos.c (anunciarItem); o cartao e o aviso do amigo vendo agora,
// em ilhacart.c.
//
// FIO PRINCIPAL.
#ifndef NV_ILHASINAIS_H
#define NV_ILHASINAIS_H
#include <SDL2/SDL.h>

// Uma vez no arranque: liga o ouvinte da saude da rede (rede_avisar_saude).
void ilhasinais_iniciar(void);
// Por quadro, em qualquer tela (dentro do player tambem: o aviso espera na
// fila e aparece quando a pilula voltar). Le os estados e executa os botoes.
void ilhasinais_passo(Uint32 agora);
// app.c: o perfil acabou de trocar (o `trocou` da escolha de perfil).
void ilhasinais_perfil_trocado(void);
// app.c: o debrid ficou baixando o torrent escolhido (DEBRID_BAIXANDO).
// `servico` "TorBox"...; `titulo` o nome do filme ou serie.
void ilhasinais_debrid_baixando(const char *servico, const char *titulo);
// app.c: o "Reconectar" do Trakt pediu os Ajustes (consumido uma vez).
int  ilhasinais_pediu_trakt(void);
// OK no aviso agregado "N pedidos de amizade": app.c abre a aba Amigos.
int  ilhasinais_pediu_amigos(void);

#endif
