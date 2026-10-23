// A FONTE QUE ESTAVA TOCANDO, para o Retomar nao repetir a busca nos addons.
//
// O que ela encurta. Desde 05/10 sair do player NAO retem a sessao por padrao
// (a retencao de PLR_RETIDO_MS, player.c, virou o ajuste avancado "Manter o
// video pronto ao sair"): este e o caminho NORMAL do Retomar. Sem ela, o
// Retomar era uma reproducao nova: busca em todos os addons, verificacao, abertura — 8 a
// 14 s ate o primeiro quadro na TCL (medido). Mas o link que tocava ainda e,
// quase sempre, o mesmo que a busca escolheria de novo. Aqui fica UMA entrada:
// a fonte da ultima sessao de filme/episodio que tocou de verdade, com quem
// estava assistindo, para o Retomar do MESMO titulo/episodio abrir direto nela.
//
// REGRAS, todas visiveis daqui:
//   - So em memoria. Nunca vai para disco nem para o log (a URL de debrid e
//     credencial na pratica): o log fala de alvo e de idade, nao de link.
//   - Uma entrada so, da ultima sessao boa. Sessao que falhou, que terminou o
//     titulo ou que mostrou erro APAGA a entrada: nao se guarda o que nao serve.
//   - Chave = alvo (id do stream: "tt1" ou "tt1:T:E") + conta + perfil. Trocar
//     de conta ou de perfil invalida sem precisar de aviso de ninguem.
//   - Validade conta de quando o LINK saiu do addon (`obtido`), nao do ultimo
//     uso: um link assinado expira contado da emissao.
//   - A entrada e um atalho, nunca a unica chance: quem usa (app.c) cai para a
//     busca normal se a conferencia ou o player falharem.
#ifndef NV_FONTEVOLTA_H
#define NV_FONTEVOLTA_H
#include "streams.h"

// VALIDADE: 3 horas a partir da emissao do link.
//
// Nao e o prazo de nenhum servico em particular: e o teto do custo de errar.
// Um link vencido custa uma conferencia que falha (um GET de 64 bytes, que
// volta em centenas de ms com 4xx) e a busca normal logo atras — a pessoa
// paga o caminho de sempre mais esse pedaco. Ate 3 h cobre o caso que motivou
// isto (pausar o filme, sair, voltar depois de jantar) e fica abaixo da vida
// de link que os debrids costumam dar; o prazo exato de cada servico NAO foi
// medido aqui. Muitos addons (Torrentio, Comet, AIOStreams) mandam uma URL de
// resolucao que pede o link ao debrid na hora do play — essas nem vencem.
#define FONTEVOLTA_VALIDADE_MS (3u * 3600u * 1000u)

// Guarda a fonte da sessao que acabou de fechar bem. `s->url` e a URL que o
// player tocava (torrent ja resolvido). `idadeLink` = ha quantos ms o link saiu
// do addon (stream_idade_ms). Substitui qualquer entrada anterior. Vazio/NULL
// em alvo, conta ou url nao guarda nada.
void fontevolta_guardar(const char *alvo, const char *conta, int perfil,
                        const Stream *s, Uint32 idadeLink, Uint32 agora);

// 1 e copia em *saida quando ha entrada para este alvo/conta/perfil dentro da
// validade. Entrada vencida ou de outra pessoa e apagada aqui (0).
int  fontevolta_pegar(const char *alvo, const char *conta, int perfil,
                      Uint32 agora, Stream *saida);

// 1 when the saved URL may start before the source sheet. escolherManual is
// "Escolher a fonte ao reproduzir": the sheet picks, so a live URL from the
// last session must not start underneath it. The other three are the old
// guards of tocarFonteGuardada (app.c).
int  fontevolta_toca_guardada(int escolherManual, int falhaAtiva,
                              int canal, int playerAberto);
// "REUSAR O ULTIMO LINK" (Ajustes > Reproducao): ligado, uma entrada POR
// titulo/episodio e perfil (ate FONTEVOLTA_MAX), gravada no disco e valida por
// `validadeS` contados no relogio de parede desde que o link saiu do addon;
// desligado (padrao), o de sempre descrito acima (uma entrada, so memoria,
// FONTEVOLTA_VALIDADE_MS). Trocar de modo comeca do zero, e desligar apaga o
// arquivo. app.c chama a cada quadro; so muda algo quando o modo muda.
#define FONTEVOLTA_MAX 50
void fontevolta_configurar(int persistir, unsigned validadeS);
// O disco, injetado por app.c (dados_ler/dados_gravar/dados_apagar), para este
// modulo continuar sem dependencia de dados.c nos testes. Sem isto, ligado
// fica so em memoria.
void fontevolta_definir_arquivo(char *(*ler)(const char *), int (*gravar)(const char *, const char *),
                                int (*apagar)(const char *));
// Apaga a entrada deste alvo/conta/perfil (ligado); desligado, a unica, como
// fontevolta_esquecer.
void fontevolta_esquecer_alvo(const char *alvo, const char *conta, int perfil, const char *porque);
// Tudo, inclusive o arquivo: ao sair da conta.
void fontevolta_esquecer_tudo(void);

// Ha entrada (qualquer alvo) com esta URL? player.c usa para nao apagar a
// entrada que esta tocando quando a lista de fontes foi trocada por baixo.
int  fontevolta_tem_url(const char *url);

// Apaga a entrada (desligado). Ligado nao faz nada: as entradas ja sao por
// conta e perfil, e e so a troca de perfil que chama isto. `porque` vai para o
// log (sem URL).
void fontevolta_esquecer(const char *porque);

// CONFERENCIA EM PARALELO com a abertura: um GET de 64 bytes seguindo os
// redirecionamentos (rede_url_final), em fio proprio. Nao atrasa o video — ela
// corre junto, e serve para pegar o 4xx/5xx e o endereco de aviso do debrid
// antes de o player desistir sozinho (no webOS isso leva dezenas de segundos).
enum { FV_NADA = 0, FV_CONFERINDO, FV_OK, FV_FALHOU };
void fontevolta_conferir(const char *url, const char *cabecalhos);
int  fontevolta_conferencia(void);
// A sonda e trocavel para os testes do Mac. NULL volta para a de verdade
// (stream_url_serve).
void fontevolta_definir_sonda(int (*sonda)(const char *url, const char *cab));

// A DECISAO DO VIGIA (app.c), separada para o teste do Mac. Por quadro, com a
// fonte guardada abrindo: esperar, deu certo, ou recuar para a busca normal
// (*motivo diz por que, para o log). O player manda sobre a sonda: com o
// titulo ja tocando, uma conferencia que falhou nao derruba nada.
enum { FV_ESPERAR = 0, FV_ABRIU, FV_RECUAR };
typedef struct {
  int falhou;        // video_falhou() || player_fonte_falhou()
  int pronto;        // video_pronto()
  double duracao;    // video_duracao(), 0 = ainda nao sabe
  int carregando;    // player_carregando()
  int quadro;        // o player ja desenhou quadro: o prazo nao vale mais
  int conferencia;   // fontevolta_conferencia()
  unsigned desdeMs;  // desde o pedido
} FontevoltaSinais;
int fontevolta_decidir(const FontevoltaSinais *g, const char **motivo);

// PRAZO para a fonte guardada provar que abre (pronto). Passou disso
// carregando, a busca normal assume. O MESMO teto do caminho normal
// (VOD_FONTE_PRAZO_MS, app.c), e nao um "curto": MEDIDO na TCL em 02/10, um
// MKV 4K de 10 GB levou 10,7 e 12,5 s da URL ao pronto — com 10 s o recuo
// matou uma abertura que ia dar certo e custou a busca inteira por cima. A
// falha RAPIDA (4xx/5xx, aviso, erro do player) ja sai pela conferencia e
// pelo erro; o prazo so cobre a fonte que trava calada.
#define FONTEVOLTA_PRAZO_MS 30000u

// PRAZO DA ESCOLHA MANUAL (folha de Fontes, OK numa fonte): sem o automatico
// nao havia prazo nenhum, e uma fonte que nunca chega ao "pronto" deixava
// "Abrindo fonte" na tela para sempre (TV do dono, 04/10: Happy Valley S1E2
// passou de 25 s). Mais curto que o do automatico porque aqui nao ha proxima
// fonte para a qual recuar: ao vencer, o player mostra o erro, com as saidas
// de sempre (Abrir Fontes, Voltar). So conta ate o primeiro quadro: depois
// de pronto, buffering lento nao entra aqui.
#define FONTE_MANUAL_PRAZO_MS 25000u
// 1 quando a abertura vencida deve virar erro: carregando, sem "pronto", sem
// erro ja mostrado, e passou do prazo. Usa os mesmos sinais do vigia acima.
int fontevolta_abertura_vencida(const FontevoltaSinais *g, unsigned prazoMs);

#endif
