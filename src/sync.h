// Sincronizacao com a conta: e isto que faz o app nativo se comportar como o
// oficial na TV de outra pessoa.
//
// O QUE ELE SUBSTITUI: hoje os addons e o token do Trakt sao arquivos de texto
// dentro do pacote (art/addons.txt, art/trakt.txt), o que torna o .ipk
// indistribuivel — ele entrega as credenciais de quem o montou. Depois deste
// modulo, os dois vem da conta de quem logou.
//
// ONDE ELE ESCREVE E ONDE NAO ESCREVE — a distincao mais importante do arquivo:
//
//   PUXA E EMPURRA (o app tem a informacao de verdade):
//     credenciais (Trakt, TMDB, mdblist), progresso de reproducao e —
//     desde o #85 — os AJUSTES DO PERFIL, costurados no blob que a conta ja
//     tinha (ver sync_proteger_ajustes_locais e ajustes_mesclar_blob).
//   ADDONS SAO MAO UNICA (#360, 11/10/2026): a TV so PUXA a lista da conta.
//     Nada daqui sobe — sync_push_addons e a pendencia de edicao local de
//     addons deixaram de existir: o push substituia a lista inteira do perfil
//     e era a origem das duplicatas e remocoes cruzadas do relato. A mudanca
//     local vale nesta TV ate o proximo pull.
//
// MEDIDO na conta do dono: sync_pull_provider_credentials devolve animeskip,
// debrid:premiumize, debrid:realdebrid, debrid:torbox, introdb, mdblist e tmdb
// — e NENHUM "trakt". O leitor de trakt fica aqui porque a RPC e a mesma e a
// linha aparece sozinha assim que o app web a escrever; ate la o vinculo Trakt
// deste app continua saindo de art/trakt.txt, que NAO pode ir no pacote.
//   SO PUXA (o app le, mas nao tem edicao local para empurrar):
//     perfis, vistos, biblioteca, colecoes, catalogos da home.
//
// "SO PUXA" NAO QUER DIZER "SO CONTA". Era o que acontecia com a biblioteca e
// com os vistos: as duas RPC eram chamadas, as linhas contadas para o resumo e
// o corpo liberado. O relato do @Haylefal (webOS 4) — "Library is empty and not
// carrying over from other instances", com o selo "Local" no canto — era isso.
// Hoje as duas passam por contalib.c e chegam ao catalogo. A lista de "salvos"
// saiu desta enumeracao porque ela nunca foi uma superficie separada: e a mesma
// biblioteca (ver a nota em contalib.h).
//
// Isto NAO e preguica, e a regra de seguranca numero 2 da secao 1.6 do plano.
// Empurrar uma superficie que o app nao edita significaria mandar uma lista
// VAZIA para o servidor, e uma lista vazia apaga o que existe nos outros
// aparelhos da pessoa. Empurrar so o que o app realmente possui e a unica
// forma segura de participar de um sync bidirecional sem ter todas as telas.
//
// Todas as chamadas de rede acontecem num fio proprio. A UI so consulta estado.
#ifndef NV_SYNC_H
#define NV_SYNC_H
#include <stdint.h>

typedef enum {
  SYNC_PARADO = 0,
  SYNC_RODANDO,
  SYNC_PRONTO,
  SYNC_FALHOU
} SyncEstado;

// Dispara um ciclo completo (puxa e, onde faz sentido, empurra). Volta na hora.
// Idempotente enquanto um ciclo estiver em andamento.
void sync_iniciar(void);
// Confirmed profile opening: clear live private data, keep durable settings.
void sync_abrir_sessao(int perfilAnterior);

typedef struct {
  uint64_t sessao;
  int perfil;
  char usuario[80];
} SyncCredencialPedido;
int sync_credencial_pedido(SyncCredencialPedido *pedido);
int sync_empurrar_credencial_pedido(const SyncCredencialPedido *pedido,
                                  const char *provider, const char *credJson);

// SINCRONIZAR ADD-ONS A MAO (#360). Pede uma volta da conta ja (nao espera os 5
// minutos) e guarda o resultado para a tela de Add-ons dizer o que mudou. A volta
// e a mesma de sempre, so que marcada: quando acaba, compara a lista de add-ons de
// antes com a de depois. Sem conta logada nao faz nada (devolve 0).
int  sync_manual_addons(void);
// 0 = nada pedido; 1 = rodando; 2 = acabou bem (add/rem/mud preenchidos: quantos
// entraram, sairam e mudaram de estado ou ordem); 3 = falhou ou o servidor esta
// fora. Os estados 2 e 3 sao lidos UMA vez (ilhasinais.c, que avisa) e voltam a 0.
int  sync_manual_addons_estado(int *add, int *rem, int *mud);
// So consulta, nao consome: a tela de Add-ons desenha o botao ocupado.
int  sync_manual_addons_rodando(void);

SyncEstado  sync_estado(void);
const char *sync_resumo(void);   // uma linha para a tela de ajustes

// Marca uma superficie como suja: o proximo ciclo empurra. Chamar quando o
// usuario mexe em algo local.
void sync_sujar_progresso(void);
// The user toggled the addon at `url`: queue that flag for a row-level write
// and keep it across restart. Membership and order remain the account's.
void sync_sujar_addons(const char *url);

// Ultimo instante em que um ciclo terminou bem (SDL_GetTicks); 0 se nunca.
unsigned sync_ultimo_ok(void);

// Intervalo entre ciclos automaticos. Ate agora o sync so rodava no arranque,
// depois do login e ao trocar de perfil — entao parar um episodio no celular
// nao aparecia na TV sem fechar e reabrir o app, que e o oposto do que a conta
// promete.
//
// 5 minutos, e nao 30 segundos: o ciclo sao ~8 requisicoes (perfis, travas,
// addons, credenciais, progresso e as so-leitura). A TV fica LIGADA horas na
// mesma tela, entao um intervalo curto vira um martelo constante no backend —
// e ja houve um episodio de estouro de cota neste projeto em que o efeito
// colateral (login impossivel, sessao anonima, sync da conta errada) pareceu
// bug do app. O ciclo tambem so roda com o app em uso, nunca durante o player.
#define SYNC_INTERVALO_MS 300000u
// Com o servidor da conta fora do ar (sync_servidor_fora), a proxima volta
// sai 1 minuto depois, e nao 5: e ela que substitui a copia local pela conta
// quando ele voltar. O freio de nuvem.c continua valendo por cima.
#define SYNC_RETENTA_MS 60000u

// Chamar uma vez por quadro. Nao bloqueia: so recolhe o resultado do fio e
// aplica no app (lista de addons, credencial do Trakt, progresso).
void sync_passo(unsigned agoraMs);

// Dispara um ciclo se ja passou SYNC_INTERVALO_MS desde o ultimo que deu certo.
// Nao roda com um ciclo em andamento, com o freio ativo, nem antes do primeiro
// sucesso. 1 quando disparou.
int  sync_periodico(unsigned agoraMs);

// Apaga do aparelho tudo que pertence a quem estava logado. Chamar JUNTO com
// sessao_sair() — a sessao sozinha nao basta.
//
// O defeito que isto conserta: sair da conta apagava o token e mais nada. A
// lista de addons continuava em memoria (com as chaves de debrid embutidas nas
// URLs), o token do Trakt continuava valido e ESCREVENDO o que a proxima
// pessoa assistisse na conta de quem saiu, o perfil ativo continuava gravado —
// entao o primeiro sync da conta seguinte escreveria progresso no
// `p_profile_id` da anterior — e o progresso.txt da anterior seguia no disco.
//
// Numa TV de sala, "sair" e a unica barreira entre duas pessoas. Ela tem de
// apagar de verdade.
void sync_esquecer_usuario(void);

// Faz o PROXIMO ciclo reaplicar os ajustes vindos da conta. Chamar ao entrar e
// ao trocar de perfil.
//
// Por que nao aplicar em TODO ciclo: o app nativo le os ajustes do perfil mas
// nao os escreve de volta. Reaplicar sempre desfaria, na volta seguinte, tudo
// que a pessoa mudasse na propria TV — ela mexeria numa opcao e veria a opcao
// voltar sozinha. Aplicando so na primeira volta depois de entrar (ou de trocar
// de perfil), a conta define o ponto de partida e a mudanca local vale pelo
// resto da sessao.
void sync_reaplicar_ajustes(void);

// TROCA DE PERFIL: o sync_reaplicar_ajustes da troca, mais os ajustes POR
// PERFIL desta TV. Chamar com o perfil ativo JA trocado e `antes` = o anterior.
//   - guarda os ajustes do perfil que sai (ajustes_perfil_guardar) e se ele
//     tinha mudanca local pendente (ajustes-locais-p<N>.txt);
//   - o que entra volta aos ajustes que tinha nesta TV; na primeira visita,
//     parte dos do perfil principal (se o principal ja foi usado aqui);
//   - o blob da conta do perfil que entra, quando existir, vem por cima como
//     sempre, a nao ser que ele tenha mudanca local pendente — ai a mudanca
//     local fica e sobe no proximo ciclo, costurada no blob dele.
// Nada disto sobe sozinho: sem blob da conta para costurar, nada e enviado
// (ver empurrarAjustes), entao um perfil sem ajustes na conta nunca recebe um
// blob montado do zero.
void sync_trocar_perfil(int antes);

// 1 quando o ultimo ciclo terminado e aplicado e do perfil ativo e nao ha
// ciclo no ar. E a metade "conta" de "a home do perfil novo esta pronta".
int  sync_perfil_pronto(void);
// Addons, colecoes e ordem deste perfil ja podem alimentar a descoberta;
// progresso e pushes podem continuar sem prender a Home.
int  sync_home_fontes_prontas(void);

// A pessoa mudou um ajuste NESTA TV. Faz duas coisas, e as duas de proposito:
//
//   1. MARCA PARA SUBIR. O proximo ciclo costura os valores locais no blob da
//      conta e empurra (sync_push_profile_settings_blob). E isto que resolve a
//      divergencia do issue #85 na origem: mudanca feita na TV passa a existir
//      na conta, e os dois lados convergem em vez de o blob ser so web -> TV.
//      O que sobe e o que fica esta em ajustes_mesclar_blob.
//   2. SEGURA O BLOB da conta ate lá. O proximo arranque nao pode reaplicar o
//      blob por cima — sem isto o restart desfazia largura, arredondamento,
//      profundidade e cor de destaque. A protecao NAO foi removida com a
//      chegada do push, e o motivo e concreto: o push pode nao acontecer (sem
//      rede, 5xx, ou o servidor sem a funcao — medido que varias RPC deste
//      plano nao existem neste servidor), e nesses casos ela e a unica defesa.
//      Removê-la exigiria provar que o push sempre chega antes do proximo
//      arranque, e isso nao se prova numa TV que pode ser desligada da tomada.
//
// Limpa-se em sync_reaplicar_ajustes (login / troca de perfil / "Atualizar"),
// que tambem descarta a pendencia de subida — ela e do perfil anterior.
void sync_proteger_ajustes_locais(void);
// 2.1: marca ajustes pendentes de OUTRO perfil (copia/edicao do administrador).
// So escreve a marca local; o push acontece quando aquele perfil for o ativo.
void sync_marcar_ajustes_pendentes_perfil(int perfil);

// Manda uma credencial de servico para a CONTA, para os outros aparelhos da
// pessoa herdarem o vinculo. `credJson` e o objeto pronto (o servidor guarda o
// que vier). BLOQUEIA — chamar de um fio, ou aceitar o custo de uma viagem.
//
// Existe por causa do Trakt: a conta do dono nao tinha a linha `trakt`, entao
// vincular na TV nao ajudava o celular. Vincular aqui passa a ESCREVER na
// conta, que e o que o app web faz.
// 1 quando a conta aceitou; 0 sem resposta ou 5xx (vale tentar depois); -1 em
// 4xx (o servidor recusou de proposito — repetir nao muda). MEDIDO: para
// "trakt" este servidor responde 400 22023 "Unsupported provider credential",
// e o proprio app web documenta a mesma recusa (traktCredentialSyncService.js).
int  sync_empurrar_credencial(const char *provider, const char *credJson);

// SERVIDOR DA CONTA FORA DO AR (#215). Estado do ultimo ciclo aplicado:
//   sync_servidor_fora: 0 quando o servidor respondeu; senao o HTTP da falha
//     (5xx, 429), ou -1 quando nao houve resposta;
//   sync_usando_copia: 1 quando, por causa disso, os addons/colecoes/biblioteca/
//     vistos em uso sao a ULTIMA copia boa guardada neste aparelho
//     (contacache.h); sync_copia_quando = de quando ela e (epoch).
// Volta a 0 sozinho no primeiro ciclo em que o servidor responde.
int  sync_servidor_fora(void);
int  sync_usando_copia(void);
long sync_copia_quando(void);
// Os addons em uso, pelo mesmo motivo: 0 = vieram da conta; 1 = da copia
// (servidor fora); 2 = o servidor falhou e nao ha copia neste aparelho.
int  sync_addons_fora(void);

void sync_encerrar(void);

#endif
