#include "arteurl.h"
// ILHA — o relogio no topo da tela, numa pilula, que se ABRE para os avisos.
//
// Pedido do dono (01/10/2026): "ilha dinamica estilo iOS". Em repouso a
// pilula mostra a hora; quando o app tem algo a dizer (aviso novo na central,
// erro, lembrete, versao nova, atualizacao baixando) a MESMA pilula cresce com
// mola, troca o conteudo por icone + frase curta e, vencido o prazo, encolhe
// de volta para o relogio. Um lugar so para o app falar, em vez de cada coisa
// abrir o proprio cartao num canto diferente.
//
// O QUE ELA NAO FAZ: dentro do player quem desenha a pilula e plrilha.c (a
// MESMA pilula, no mesmo canto, com a hora e o "termina as" do titulo, e os
// componentes pequenos do player nascendo dela — Glass UI, 03/10); app.c so
// desenha ESTA com o player fechado. Nao pega teclado fora do modal (o
// AZUL/CH+ da central continua em avisos_evento; o do modal, ver abaixo) e
// nao e camada de tela cheia — e uma sombra pequena, uma pilula e o texto,
// mesmo durante a mola, e o modal e a mesma pilula crescida.
//
// QUATRO CONTEUDOS, por prioridade: aviso (fila curta, um de cada vez) >
// atividade (algo em andamento, renovado a cada quadro por quem a tem) >
// cartao (atividade ao vivo / estreia, ao lado do relogio, so com ele) >
// relogio (so onde app.c disse que cabe: ilha_relogio_visivel).
//
// FIO PRINCIPAL em tudo.
#ifndef NV_ILHA_MODULO_H
#define NV_ILHA_MODULO_H
#include <SDL2/SDL.h>
#include "gfx.h"

enum { ILHA_INFO = 0, ILHA_OK, ILHA_ERRO, ILHA_ACENTO };

// Aviso curto. `chave` identifica o aviso: repetir a chave do que esta na
// tela troca o texto NO LUGAR (a pilula remorfa) e renova o prazo, em vez de
// entrar na fila de novo; NULL/"" = um aviso avulso. `icone` e um nome de
// art/icones (NULL = o do tipo). `ms` conta a partir do primeiro quadro em que
// ele aparece. `tecla` = 1 desenha a tecla da central (disco AZUL / CH+) com
// "abre" na ponta direita.
void ilha_avisar(const char *chave, int tipo, const char *icone,
                 const char *texto, unsigned ms, int tecla);
// Fecha a abertura anterior: tira avisos, cartoes, atividade e pedidos
// privados. Mantem avisos identificados do aparelho (atualizacao/queda/rede)
// e a configuracao de posicao do relogio. Nao cancela a atualizacao em curso.
void ilha_abrir_sessao(void);
// Tira o aviso desta chave da tela e da fila (ex.: a central foi aberta).
void ilha_retirar(const char *chave);
// 1 enquanto o aviso desta chave esta na tela ou na fila.
int  ilha_tem(const char *chave);
// 1 quando um aviso com esta chave APARECEU na pilula (primeiro quadro na
// tela, sem nada cobrindo) desde a ultima pergunta; a resposta e consumida.
// E o "ja dito" de verdade: quem marca um evento como visto no disco
// (avisodisp) pergunta aqui, e nao no instante em que mandou o aviso — um
// aviso que a fila descartou, ou que venceu debaixo do painel, nao foi lido.
int  ilha_mostrou(const char *chave);

// --- PRIORIDADE, CONTADOR E A CENTRAL (mockup aprovado em 02/10) ------------------
//
// A FILA ERA FIFO e um erro esperava atras de tres avisos informativos — ate
// 4 x 20 s se fossem toasts da central (mapa.md, secao 10). Agora:
//   - P1 (erro que trava a pessoa) FURA A FILA: entra ja, e o que estava na
//     tela volta para a frente da fila e reaparece depois, com o prazo inteiro
//     (o da central nao: ele continua na lista de avisos);
//   - dentro da mesma prioridade, ordem de chegada;
//   - com dois ou mais esperando, a pilula mostra "+N" na ponta;
//   - os avisos DA CENTRAL (grupo = 1) nao passam um por um: o primeiro diz o
//     assunto ("Ana recomendou Fallout"); chegando outro enquanto ele ainda
//     espera, os que esperam viram UM so, "N avisos novos", que abre a central.
// Prioridade 0 = pelo tipo: ERRO e P1, ACENTO e P2, OK e INFO sao P3.
enum { ILHA_P1 = 1, ILHA_P2, ILHA_P3 };

// MODAL GENERICO: a mesma pilula crescida (o "a pilula cresce" dos cartoes),
// com 1 a 3 botoes e, no lugar da arte 16:9, o ROSTO de alguem (pedido de
// amizade) ou um ICONE num ladrilho (versao nova, Trakt, aviso do dono). Todos
// os campos sao opcionais menos `titulo` e um botao; vazio = a linha some.
#define ILHA_MODAL_BOTOES 3
typedef struct {
  char kicker[80];          // linha pequena em maiusculas acima do titulo
  char titulo[160];         // grande (a "logo" do modal)
  char linha[160];          // logo abaixo do titulo ("Série · 2022 · IMDb 8,7")
  char nota[120];           // apagada, logo abaixo ("Você está na 1.7.1")
  char texto[420];          // corrido, ate 3 linhas (bio, explicacao)
  char fala[240];           // citacao com fio a esquerda (o recado do amigo)
  char lista[3][96];        // itens com bolinha (notas da versao)
  char chips[3][32];        // generos, em pilulas
  char estado[80];          // na base da coluna ("há 12 min")
  char rodape[120];         // a direita dos botoes ("Recusar não avisa a pessoa.")
  char arte[NV_ARTE_URL_MAX];          // 480x270; com `rosto`, ele vai no canto da arte
  char rosto[256];          // url do avatar (vazio = so a inicial de rostoNome)
  char rostoNome[64];
  char icone[32];           // sem arte nem rosto: ladrilho 150 com este icone
  int  tipo;                // a luz do canto (ILHA_ERRO no Trakt), como na pilula
  int  salvos;              // 1 = "Salvos ›" na ponta, como nos cartoes
  int  nBotoes;
  char botao[ILHA_MODAL_BOTOES][40];       // rotulo JA traduzido
  char botaoIcone[ILHA_MODAL_BOTOES][32];  // "" = sem icone
  // A ILHA DO RELOGIO CRESCIDA (mockup do registro, quadro 14): 880 de largura,
  // cabecalho de 64 com o icone, o `kicker` e a hora, e embaixo titulo, texto
  // e botoes, sem ladrilho. A queda da sessao anterior usa.
  int  cabecalho;
  // RESULTADO (enquete, N3): com `resultado` = 1, `lista[i]` vira linha de
  // resultado — texto, `pct[i]` (0..100) na ponta e um trilho de 6 px; `escolha`
  // (1..3, 0 = nenhuma) destaca a opcao da pessoa.
  int  resultado;
  int  pct[3];
  int  escolha;
} IlhaModal;

// O aviso completo. `texto` aceita ENFASE: o trecho entre dois ILHA_FORTE sai
// em negrito (o nome da pessoa, o titulo) — use ilha_forte para embrulhar o
// argumento, nunca o formato traduzido.
#define ILHA_FORTE "\x02"
typedef struct {
  const char *chave;
  int tipo;
  const char *icone;        // NULL = o do tipo; ignorado com rosto/capa
  const char *texto;
  unsigned ms;
  int tecla;                // 1 = a tecla AZUL com "abre" (vira 1 sozinho com modal)
  int prior;                // ILHA_P1..P3; 0 = pelo tipo
  int grupo;                // 1 = item da central (ver acima)
  const char *rosto;        // avatar 36 (url; "" = a inicial de rostoNome)
  const char *rostoNome;
  const char *capa;         // mini capa 30x44; com rosto vira o "duo"
  const char *meta;         // sufixo apagado depois da frase ("T2E4")
  int vivo;                 // ponto vermelho de "agora"
  int cartao;               // AZUL abre o modal deste cartao (ILHA_VIVO + 1 ...); 0 = nao
  const IlhaModal *modal;   // AZUL abre este modal; NULL = nao
  // AVISO DE DUAS LINHAS (Glass UI v2, 03/10 — o da atualizacao, mockup
  // ajustes-v2 "v2-upd-aviso"/"v2-upd-em-dia"). Com `titulo` a pilula fica com
  // 84 de altura: o icone num disco de 56 na cor do tipo a 22%, `titulo` em
  // 26/700 e `texto` embaixo em 20, apagado. Com `kicker` (sem titulo) o disco
  // e de 40 e a marca em caixa alta, na cor do tipo, vai em cima do `texto`
  // em 26/600 ("EM DIA"). NULL nos dois = o aviso de uma linha de sempre.
  const char *titulo;
  const char *kicker;
  const char *dica;         // rotulo ao lado da tecla no lugar de "abre"
  // 1 = a tecla (AZUL/CH+, ou o clique) ENTREGA o aviso a quem o pos, sem
  // modal: ilha_aviso_pediu devolve 1 com a chave, e o aviso sai da fila.
  int acao;
  // 1 = a pilula cresce ate o modal sozinha, sem esperar a AZUL (a pergunta de
  // primeira vez do "+", ilhasalvar.c). Precisa de `modal`.
  int abrir;
} IlhaAvisoEx;
void ilha_avisar_ex(const IlhaAvisoEx *a);
// Embrulha `s` em ILHA_FORTE em `dst` (devolve dst), para passar como %s.
const char *ilha_forte(char *dst, size_t tam, const char *s);
// A central abriu: os avisos dela (grupo = 1) saem da tela e da fila.
void ilha_retirar_grupo(void);
// 1 = o aviso na tela leva a tecla e ela e da CENTRAL (sem modal nem cartao):
// e quando AZUL/CH+ abre a lista de avisos (avisos_evento).
int  ilha_tecla_central(void);
// Botao escolhido num modal de AVISO, entregue uma vez: devolve 1..3 (o
// indice + 1) e copia a chave do aviso em `chave`; 0 = nada. Voltar nao conta.
int  ilha_aviso_pediu(char *chave, size_t tam);
// Para o teste da fila (tests/ilhafila.c) e para o log: a chave do aviso da
// vez ("" sem aviso) e quantos esperam atras dele (o numero do "+N").
const char *ilha_aviso_vez(void);
int  ilha_esperando(void);

// AVISO DE ACAO FEITA (pedido do dono, 03/10: "toda confirmacao tem que
// aparecer na ilha; se clicar, mostra o que foi feito e se quer desfazer").
// Um aviso com `icone` e `texto` ja traduzido ("Salvo em Lista do Nuvio",
// "Marcado como assistido"). Com `poster`, a capa do titulo VOA ate a pilula
// (mesma mola do ilha_minimizar, ilha_voo.h) e pousa virando o icone; `de` =
// de onde ela parte (NULL = o meio da tela, 150x225). `modal` (ou NULL) e o que
// a AZUL/OK/clique abre: o que foi feito e os botoes (ilha_aviso_pediu com a
// chave ILHA_ACAO_CHAVE). Animacoes reduzidas: so o aviso, sem voo. Quem posta
// e ilhaacao.c; as telas chamam ilhaacao_feita.
#define ILHA_ACAO_CHAVE "acao-aviso"
void ilha_acao(const char *icone, const char *texto, int tipo, const char *poster,
               const GfxRect *de, const IlhaModal *modal);

// ATIVIDADE em andamento: chame A CADA QUADRO enquanto durar; sem renovacao
// por ~0,4 s ela sai sozinha. `progresso` de 0 a 1, ou < 0 quando nao ha numero.
void ilha_atividade(const char *texto, float progresso);
// A MESMA ATIVIDADE NO DESENHO v2 (mockup ajustes-v2, 03/10: "Procurando
// atualização…", "Baixando a atualização..."): pilula de 72. `icone` "" = o
// ponto que respira e o texto em 26/600; com nome de icone, o icone no acento,
// o texto em 24/600 e o trilho de 180 x 6 com a porcentagem AO LADO do texto
// (nao embaixo). NULL = ilha_atividade.
void ilha_atividade_ex(const char *texto, float progresso, const char *icone);
// Optional live details: NULL removes the activity's expandable panel.
void ilha_atividade_detalhes(const char *titulo, const char *texto);
// Compact stats for the expandable panel (Home loading): the panel then draws
// a thin progress bar, a live m:ss counter and stat chips instead of text lines.
// Set it right AFTER ilha_atividade_detalhes() (which clears it); NULL = none.
typedef struct { const char *etapa; unsigned ms; int prontos, total, fileiras, falhas, ativo; } IlhaAtvCarga;
void ilha_atividade_carga(const IlhaAtvCarga *c);
int ilha_atividade_expansivel(void);

// Bolinha de acento no relogio em repouso: "ha enquete aberta" (enquete.c).
// Com ela, AZUL/CH+ no relogio parado nao abre a central: ilha_ponto_pediu()
// devolve 1 uma vez para quem a liga reabrir a enquete.
void ilha_ponto_enquete(int aberto);
int  ilha_ponto_pediu(void);

// Onde o relogio pode ficar, decidido por quadro por app.c (a home tem o topo
// esquerdo livre; Ajustes e Explorar tem titulo ali).
void ilha_relogio_visivel(int visivel);
// O roteador concede foco ao sair pelo topo de uma pagina; nao altera os
// cartoes nem o caminho existente de toque/AZUL/CH+.
void ilha_relogio_focar(int foco);
int  ilha_relogio_focado(void);

// PONTO UNICO DA POSICAO. Sem chamada, a ilha fica no topo esquerdo, na coluna
// do conteudo — ou no topo direito no layout Dinamica, onde a pilula fechada
// da barra lateral ocupa o canto esquerdo. Quem expuser uma area (ex. a pilula
// do menu) chama isto a cada quadro antes de ilha_desenhar: (x, y) e o canto
// de cima da ilha; `daDireita` = 1 faz x ser a borda DIREITA e a ilha crescer
// para a esquerda. Vale para um quadro.
void ilha_ancorar(float x, float y, int daDireita);

// Aplica a escolha de Ajustes (Posicao do relogio) ao quadro: chamar antes de
// ilha_desenhar. `guia` = 1 na tela do Guia (titulo a esquerda: vai a direita).
void ilha_posicionar(int guia);

void ilha_desenhar(Uint32 agora);
// 1 quando ha aviso ou atividade aberta (o relogio sozinho nao conta).
int  ilha_ocupada(void);

// --- CARTOES PERSISTENTES: atividade ao vivo e estreia (pedido do dono, 01/10) ---
//
// SO COM O RELOGIO NA TELA (Ajustes > Relogio na tela e onde app.c diz que ele
// cabe). Um cartao fica AO LADO do relogio, na mesma pilula, ate quem o poe
// tira-lo (ilhacart.c): a sessao interrompida do player (ILHA_VIVO) e o
// episodio novo de uma serie com lembrete ainda nao lido (ILHA_ESTREIA). Os
// avisos e a atividade continuam passando na frente; dois cartoes alternam a
// cada ILHA_ALTERNA_MS. AZUL/CH+ com um cartao na pilula abre o MODAL — a
// pilula cresce ate um cartao maior com a arte e os botoes (ilha_evento).
// ILHA_AMIGO (02/10, mockup aprovado): o terceiro cartao, "Ana · Severance ·
// agora", com o rosto dela e a capa — o evento de inicio mais novo do feed
// enquanto ele tiver menos de 15 min (a regra de RecAmigo.temAgora).
enum { ILHA_VIVO = 0, ILHA_ESTREIA, ILHA_AMIGO, ILHA_N_CARTOES };
#define ILHA_ALTERNA_MS 6000u
typedef struct {
  char chave[80];        // muda = outro conteudo (a pilula remorfa)
  char imdb[64];
  int  serie, t, e;      // t/e = 0 em filme
  char titulo[160], epNome[120], sinopse[420];
  char poster[NV_ARTE_URL_MAX], logo[512], arte[512];   // arte: still do episodio ou fundo
  float progresso;       // 0..1 na ILHA_VIVO; < 0 sem barra
  int  restanteMin;      // ILHA_VIVO
  char quando[24];       // ILHA_ESTREIA: "hoje" (ja traduzido) ou ""
  char avisoId[72];      // ILHA_ESTREIA: id do aviso em avisos.c
  char pessoa[64], rosto[256];   // ILHA_AMIGO: nome e avatar de quem esta vendo
} IlhaCartao;
int ilha_retomar_focado(IlhaCartao *c);
// NULL tira o cartao. A copia e da ilha; quem chama pode descartar o seu.
void ilha_cartao(int qual, const IlhaCartao *c);
// Troca de identidade: apaga este cartao e suas copias no modal, no pedido e
// na animacao. Os outros cartoes, avisos e atividade nao sao alterados.
void ilha_cartao_invalidar(int qual);
// 1 quando ha cartao e o relogio esta na tela: e quando AZUL/CH+ abre o modal.
int  ilha_cartao_na_tela(void);

// O MODAL. Abrir pega o cartao que a pilula mostra; Voltar recolhe para ela.
int  ilha_modal_abrir(void);
void ilha_modal_fechar(int seco);   // seco = 1: some sem recolher (virou o painel)
int  ilha_modal_aberto(void);
int  ilha_modal_visivel(void);
// Teclado do modal (come tudo enquanto aberto). Fora dele devolve 0.
int  ilha_evento(const SDL_Event *e);
// Acao pedida no modal, entregue uma vez (o contrato de avisos_pediu). `c`
// recebe o cartao em que ela foi pedida.
//
// OS TRES GESTOS DE SAIDA (06/10, "muito alerta sem dispensar"), iguais em
// todo modal de cartao e de aviso:
//   - VOLTAR recolhe o modal para a pilula e NAO muda nada: o cartao fica, o
//     aviso continua na lista como estava;
//   - "Depois" / "Agora nao" ADIA: o cartao da estreia sai da pilula ate o app
//     fechar (ILHA_PEDIU_DEPOIS; avisodisp_sessao_*), o item fica na lista;
//   - "Dispensar" (ou "Fechar" no cartao ao vivo e no do amigo, que nao
//     guardam nada) DISPENSA: some da pilula e da lista e a mesma chave nao
//     volta, nem depois de reiniciar (ILHA_PEDIU_DISPENSAR; avisodisp.h).
// Estreia: Assistir · Depois · Dispensar.
enum { ILHA_PEDIU_NADA = 0, ILHA_PEDIU_TOCAR, ILHA_PEDIU_DETALHES,
       ILHA_PEDIU_DISPENSAR, ILHA_PEDIU_SALVOS, ILHA_PEDIU_DEPOIS };
int  ilha_pediu(IlhaCartao *c, int *qual);

// MINIMIZAR NA ILHA: o player saiu no meio e a sessao virou o cartao
// ILHA_VIVO. A arte do cartao (still do episodio; `fundoReserva` = o fundo do
// titulo, quando o still nao esta decodificado) nasce em tela cheia e encolhe
// em 400 ms ate a mini capa da pilula, com a tela de baixo aparecendo por tras.
// Chamar com o relogio na tela (quem chama ja trocou para a home) e o cartao
// posto. Animacoes reduzidas: nada voa, a pilula ja aparece com o cartao.
// 0 = sem cartao ou sem relogio: nada a fazer.
int  ilha_minimizar(const char *fundoReserva);
// Logo depois do ilha_minimizar: 1 = o video pausado segue no plano de baixo
// (sessao retida, Android) e o voo nasce dissolvendo a partir dele.
void ilha_minimizar_dissolver(int sim);
int  ilha_minimizando(void);

// Retangulo da pilula (ou do modal, enquanto ele esta na tela) no ultimo
// quadro: e de onde o painel de Salvos nasce e para onde ele recolhe. 0 quando
// a ilha nao esta na tela.
int  ilha_rect(float *x, float *y, float *w, float *h);
// Largura do relogio em pixels reais, incluindo recuos e ponto de enquete.
float ilha_relogio_largura(void);
// 1 = outra superficie nasceu da pilula e esta no lugar dela (o painel de
// Salvos): a ilha continua medindo, mas nao se desenha. Vale um quadro.
void ilha_coberta(int coberta);

// A PILULA VIRA UM PAINEL (a Central de controle, central.h). Pedido POR
// QUADRO, antes de ilha_desenhar: a propria ilha estica da pilula ate um
// retangulo w x h ancorado no canto dela, na mesma mola e no mesmo material do
// modal, com a hora parada no lugar de sempre (vira o cabecalho). `corpo`
// desenha o conteudo no retangulo inteiro (a faixa de cima, NV_ILHA_H, e da
// hora) com o alfa que a ilha da: entra quando a forma assenta e sai antes
// dela encolher. Sem pedido a ilha volta a pilula, chamando o ultimo corpo
// com o alfa caindo. Efeitos leves: mola sem repique; minimos: sem animacao.
typedef void (*IlhaCorpo)(GfxRect r, float a, void *u);
void  ilha_corpo(float w, float h, IlhaCorpo corpo, void *u);
// 0 = pilula, 1 = painel assentado (passa um pouco de 1 no repique).
float ilha_corpo_t(void);
// Chamar logo depois de ilha_corpo: este corpo (e a saida dele) sem sombra caida.
void  ilha_corpo_sem_sombra(void);

#endif
