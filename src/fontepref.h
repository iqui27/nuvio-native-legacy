// A FONTE QUE A PESSOA ESCOLHEU, lembrada por titulo e por perfil.
//
// O DEFEITO QUE ISTO CONSERTA sao os issues #56 e #57, e eles sao o MESMO
// buraco visto de dois lados: o app nunca guardou qual fonte a pessoa escolheu
// na folha. `stream_automatico()` pontua a lista e devolve a melhor pela regra
// do dono (MP4 4K DV primeiro), e essa regra NAO SABE que a fonte que a pessoa
// escolheu a mao era a unica com audio dublado. Entao:
//
//   #56  no episodio seguinte a pontuacao escolhe outra fonte — legendada,
//        ou de outro provedor — e a pessoa volta a folha e procura a dela de
//        novo, entre dezenas de linhas.
//   #57  "Retomar" na pagina do titulo nunca retoma na MESMA fonte, porque
//        nao ha nenhuma fonte guardada para retomar.
//
// --- A REGRA DE IGUALDADE, E POR QUE ESTA E NAO OUTRA -----------------------
//
// "A mesma fonte" no episodio SEGUINTE nao pode ser a url: ela muda em todo
// episodio, e nos links de debrid ela ainda e assinada e expira em minutos
// (ver streams.h). Nao pode ser `arquivo` nem `tamanhoMB` pelo mesmo motivo.
// Nao pode ser o indice na lista: a lista vem de addons em paralelo e a ordem
// nao se repete.
//
// O que sobra em `Stream` e que NAO muda de um episodio para o outro:
//
//   provedor  o addon que devolveu a fonte (Torrentio, AIOStreams, MediaFusion
//             ...). E o campo mais estavel da struct — vem do cadastro de
//             addons, nao da resposta.
//   rotulo    o `name` do stream. No Torrentio e "Torrentio\n1080p", no
//             AIOStreams e o servico + a qualidade: muda pouco entre
//             episodios, mas MUDA (alguns addons enfiam o tamanho ali).
//             Por isso ele e so DESEMPATE, nunca requisito.
//   altura    a resolucao. Estavel por trilha de lancamento; tambem desempate.
//
// E o que o issue #56 diz em letras maiusculas: "a fonte que escolhi tem uma
// dublagem especifica que as outras nao tem". Esse dado NAO tem campo proprio
// na struct — ele vem escrito dentro de `rotulo` e `descricao`, em bandeira
// (🇧🇷) ou por extenso ("Dual Audio", "Dublado", "Latino"). Por isso a chave e
// PROVEDOR + TRILHA, onde a trilha e a assinatura de idioma/audio extraida
// desses textos (ver fontepref_trilha). Sem a trilha, "mesmo provedor" devolve
// a primeira fonte do Torrentio que aparecer — que e exatamente a legendada
// que a pessoa nao quer.
//
// CASAMENTO ESTRITO, DE PROPOSITO: provedor E trilha tem de bater. Uma segunda
// camada "so o provedor" foi considerada e RECUSADA — ela entrega uma fonte do
// addon certo com o audio errado, que e a queixa do issue, e faz isso
// parecendo que o app lembrou. Nao casou nada: cai calado em
// stream_automatico(), o comportamento de hoje.
//
// --- E ANTES DE TUDO ISSO, O bingeGroup ------------------------------------
//
// A assinatura acima nasceu porque o app NUNCA LIA o campo em que o addon ja
// diz a resposta. O protocolo do Stremio tem `behaviorHints.bingeGroup`: um
// rotulo que o addon poe em todo stream que ele considera A MESMA FONTE entre
// episodios. Quem manda o campo resolve o casamento sem heuristica nenhuma —
// nao ha o que adivinhar sobre idioma, resolucao ou grupo de lancamento,
// porque quem produziu a lista ja agrupou.
//
// A ordem, entao, e:
//
//   1. bingeGroup IGUAL. O addon declarou; acabou a conversa.
//   2. provedor + trilha de audio. Para os addons que nao mandam o campo, que
//      sao muitos, e para o episodio em que o addon MUDOU o bingeGroup (troca
//      de servico de debrid, de cache, de versao do addon). A heuristica nao
//      foi substituida: ela virou a rede embaixo.
//   3. -1, e stream_automatico() assume, como sempre.
//
// A camada 1 NAO EXIGE provedor: o bingeGroup e a declaracao do addon sobre
// identidade de fonte, e e assim que o app web o usa (um `find` por
// bingeGroup, sem olhar mais nada). A camada 2 continua exigindo as duas
// coisas, pelo motivo do paragrafo anterior.
//
// --- POR QUE A PREFERENCIA VENCE --------------------------------------------
//
// Ver FONTEPREF_VALIDADE_S. Uma preferencia de meses atras aponta para um
// provedor que talvez nem esteja mais instalado, e faz isso escondendo a folha
// de fontes (detail.c consulta fontepref_tem para decidir se pergunta ou toca).
//
// SO A ESCOLHA MANUAL E GRAVADA. O que o automatico escolhe nao vira
// preferencia: quem nunca abriu a folha continua com a regra da pontuacao,
// intocada, para sempre.
//
// ONDE ELE PARA: nao fala com a rede e nao verifica link nenhum. Quem confere
// se a fonte resolve continua sendo stream_primeira_boa; este modulo so diz
// QUAL tentar primeiro.
#ifndef NV_FONTEPREF_H
#define NV_FONTEPREF_H

#include "streams.h"

// Teto da tabela. 200 titulos x ~400 bytes = ~80 KB. Estatico, como salvos.c e
// pelo mesmo motivo (esta TV ja bateu no limite de 128 MiB do WebAssembly).
// Cheia, sai a preferencia MAIS ANTIGA — quem voltou a assistir um titulo
// reescreve a linha dele e volta para o fim da fila sozinho.
#define FONTEPREF_MAX 200

// Tamanho da assinatura de idioma/audio. Oito marcas de 4 bytes mais os
// separadores cabem com folga; o que passar disso e cortado, e cortar os dois
// lados igual continua casando.
#define FONTEPREF_TRILHA 64

// Espelha Stream.bingeGroup. Guardado igual ao que veio, cortado igual dos
// dois lados — como a trilha.
#define FONTEPREF_BINGE 128

// VALIDADE DA PREFERENCIA — 180 dias.
//
// O app web tambem expira a dele, e a comparacao importa porque os dois
// guardam COISAS DIFERENTES:
//
//   - la, `getValid(..., streamReuseLastLinkCacheHours * 3600e3)` com padrao
//     de 24 h (playerSettingsStore.js) protege um LINK guardado. Link de
//     debrid e assinado e expira em minutos; 24 h ja e generoso para isso.
//   - aqui nao ha link guardado nenhum. O que se guarda e IDENTIDADE
//     (bingeGroup, provedor, assinatura de audio) e a url e sempre resolvida
//     de novo, na lista de hoje, por stream_primeira_boa. O prazo curto do
//     link nao tem o que fazer neste arquivo.
//
// Entao o que 180 dias protege e outra coisa: um provedor que sumiu, um addon
// desinstalado, uma assinatura de debrid que acabou. O dano de uma preferencia
// velha nao e tocar errado — e detail.c NAO PERGUNTAR (issue #57): com
// fontepref_tem verdadeiro o toque curto no episodio reproduz direto, e se a
// fonte lembrada nao existe mais a pessoa recebe o que a pontuacao escolher,
// calada. Passado meio ano, perguntar de novo e o certo.
//
// POR QUE NAO MENOS: uma temporada de lancamento semanal com 24 episodios leva
// ~168 dias do primeiro ao ultimo. Um prazo mais curto expiraria NO MEIO da
// serie, que e exatamente o caso que o issue #56 existe para atender.
//
// POR QUE NAO MAIS: seis meses ja e mais tempo do que a lista de addons desta
// TV costuma ficar parada.
#define FONTEPREF_VALIDADE_S (180LL * 24LL * 3600LL)

typedef struct {
  char id[24];                      // IMDb do TITULO, sem ":temporada:episodio"
  char provedor[96];
  char trilha[FONTEPREF_TRILHA];
  char bingeGroup[FONTEPREF_BINGE]; // o que o addon declarou; vazio e comum
  char rotulo[192];                 // desempate e log
  int  altura;
  long long quandoS;                // time(NULL) da escolha
} FontePref;

// Le o arquivo do perfil corrente. Chamar no arranque, depois de
// dados_iniciar. Sem pasta de dados e no-op silencioso (dados.c ja explicou no
// log por que nao ha pasta) e a tabela comeca vazia.
void fontepref_iniciar(void);

// TROCA DE PERFIL: solta a tabela e le o arquivo do perfil novo na proxima
// consulta. Mesma forma de fil_definir_perfil, e pelo mesmo motivo — numa TV
// de sala a fonte dublada que a mae escolheu nao e a que o filho quer.
void fontepref_definir_perfil(int perfil);

// Apaga do aparelho. Chamar de sync_esquecer_usuario, junto de salvos_esquecer
// e recomenda_esquecer: o que a pessoa assiste e com que audio e tao pessoal
// quanto a lista dela.
void fontepref_esquecer(void);

// "tt1234567:2:4" -> "tt1234567". A preferencia e do TITULO e nao do episodio:
// e a serie inteira que tem de continuar no mesmo audio (issue #56).
void fontepref_id_base(const char *id, char *dst, unsigned tam);

// Assinatura de idioma/audio da fonte: as marcas encontradas em rotulo,
// descricao e arquivo, em codigo de duas ou tres letras, ORDENADAS e unidas por
// "+" ("BR+DUB+POR"). Vazia quando a fonte nao declara nada — e duas fontes sem
// declaracao nenhuma casam entre si, que e o certo: sem dado para distinguir,
// "mesmo provedor" e a melhor resposta possivel.
void fontepref_trilha(const Stream *s, char *dst, unsigned tam);

// Guarda a escolha e GRAVA. `id` pode vir com episodio; e cortado aqui.
// Devolve 1 quando a tabela mudou. Fonte sem provedor nao entra.
int fontepref_guardar(const char *id, const Stream *s);

// A preferencia deste titulo, ou NULL. VENCIDA CONTA COMO INEXISTENTE (ver
// FONTEPREF_VALIDADE_S): devolve NULL e a linha fica no arquivo, para ser
// reescrita na proxima escolha ou descartada como a mais antiga quando a
// tabela encher. Expirar nao grava — leitura que escreve em disco e como se
// descobre, meses depois, que o arquivo foi reescrito num desligamento.
const FontePref *fontepref_do_titulo(const char *id);
int fontepref_tem(const char *id);

// Indice, NA LISTA DE STREAMS CORRENTE, da fonte que casa com a preferencia
// deste titulo. -1 quando nao ha preferencia ou quando nenhuma fonte de hoje
// casa — e -1 quer dizer "siga o automatico", nunca "nao toque".
int fontepref_escolher(const char *id);

#endif
