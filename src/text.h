// Texto: SDL_ttf rasteriza para textura, com cache por (fonte,tamanho,string).
// Sem cache, cada quadro rasterizaria os mesmos titulos de fileira de novo —
// rasterizacao de texto e cara e o conteudo aqui muda pouco.
#ifndef NV_TEXT_H
#define NV_TEXT_H
#include <stddef.h>
#include "gl_compat.h"

// Escala do tvOS. Cada estilo carrega tamanho E peso: no aparelho a diferenca
// entre um titulo e um subtitulo vem tanto do peso quanto do corpo, e usar um
// peso so achata a hierarquia inteira — foi o que deixava a tela com cara de
// "tudo do mesmo tamanho, uns maiores".
typedef enum {
  TXT_TITULO1, TXT_TITULO2, TXT_TITULO3, TXT_HEADLINE,
  TXT_BODY, TXT_CALLOUT, TXT_CAPTION, TXT_CAPTION2, TXT_MINI,
  // Os dois do player vem do app web, nao da escala do tvOS. Ficam no FIM do
  // enum de proposito: a tabela ESTILOS em text.c e indexada por esta ordem, e
  // inserir no meio desloca todos os estilos seguintes em silencio.
  TXT_PLR_TITULO, TXT_PLR_CORPO, TXT_ROW_TITULO, TXT_HERO_SEC,
  // Tela de DETALHE, medidos no app web. Nao reaproveitam nenhum estilo do
  // tvOS porque nenhum bate: a sinopse la e 26/400 e o TXT_CAPTION daqui e
  // 22/400 — quatro pixels que mudam quantas linhas cabem no bloco.
  TXT_DET_BOTAO,   // .series-primary-btn      25 / 600
  TXT_DET_META,    // .series-detail-support   25 / 400
  TXT_DET_SIN,     // .series-detail-description 26 / 400
  TXT_DET_META2,   // .detail-meta-row.secondary 23 / 400
  // Linha de meta do HERO: 21 / 500, rgb(179,179,179). Nao e o TXT_CAPTION
  // (22/400) nem o TXT_CALLOUT (28/500) — um erra o peso, o outro o corpo, e a
  // linha ficava ou apagada demais ou grossa demais contra a arte.
  TXT_HERO_META,
  // Sinopse do hero: .home-hero-description, 22/400 branco cheio. O TXT_CAPTION
  // tem o mesmo corpo mas e cinza — a cor vem de quem desenha, o estilo nao.
  TXT_HERO_SIN,
  // Canto superior do PLAYER, do bloco #playerUiRoot do web:
  //   .player-clock          26 / 600
  //   .player-ends-at        20 / 400
  //   .player-parental-label 22 / 600
  //   .player-parental-severity e .player-parental-separator 22 / 400
  TXT_PG_RELOGIO, TXT_PG_FIM, TXT_PG_ROTULO, TXT_PG_GRAV,
  TXT_PAINEL_TITULO, TXT_PAINEL_ITEM,
  TXT_CW_TITULO, TXT_CW_META, TXT_CW_BADGE,
  TXT_RANK,
  // Legenda externa: 50%..250% (#335), em passos de 10. O firmware da C9 oferece
  // apenas cinco degraus; estas fontes pertencem ao overlay do proprio app.
  TXT_LEG_50, TXT_LEG_60, TXT_LEG_70, TXT_LEG_80,
  TXT_LEG_90, TXT_LEG_100, TXT_LEG_110, TXT_LEG_120,
  TXT_LEG_130, TXT_LEG_140, TXT_LEG_150, TXT_LEG_160,
  TXT_LEG_170, TXT_LEG_180, TXT_LEG_190, TXT_LEG_200,
  TXT_LEG_210, TXT_LEG_220, TXT_LEG_230, TXT_LEG_240, TXT_LEG_250,
  // Numeral do Top 10 da home Dinamica (NV_TOP10_NUM_CORPO). No FIM, depois das
  // legendas: TXT_LEG_* e contado por aritmetica a partir de TXT_LEG_50.
  TXT_RANK_GRANDE,
  // ESCALA DAS ILHAS (Glass UI, mockup "ilha" tela 3 — o painel Social): os
  // corpos do CSS aprovado, em px de 1080p, que nenhum estilo acima tinha. O
  // painel misturava CALLOUT 28, CAPTION 22 e MINI 15 e saia "parecido" com o
  // mockup, nunca igual: o dono comparou lado a lado e viu (02/10, "o mockup
  // ta bem mais polido que a build"). No FIM, pela mesma razao do RANK_GRANDE.
  TXT_ILHA_TITULO,   // .ttl            40 / 700  ("Social")
  TXT_ILHA_SECAO,    // .sec b          22 / 800  ("Hoje", "Esta semana")
  TXT_ILHA_NOME,     // nome na linha / cabecalho do menu do cartaz 24 / 600-700
  TXT_ILHA_CORPO,    // verbo na linha  24 / 400  ("te mandou")
  TXT_ILHA_SEG,      // .sg             19 / 600  (abas segmentadas)
  TXT_ILHA_SUB,      // titulo da linha 19 / 400
  TXT_ILHA_NUM,      // .sg .n          16 / 400  (contagem da aba)
  TXT_ILHA_HORA,     // quando          15 / 400  ("há 15 min")
  TXT_ILHA_INICIAL,  // .av             20 / 700  (inicial no disco de 52)
  // Spotlight e menu do cartaz (mockup telas 6 e 7): o nome numa linha de
  // resultado (22/600, em Bold: a Inter embarcada nao tem 600), a meta do
  // melhor resultado (19/400), o genero (17/400) e o apoio das linhas (16/400).
  TXT_ILHA_ITEM, TXT_ILHA_META, TXT_ILHA_GENERO, TXT_ILHA_APOIO,
  // A CONFIRMACAO da ilha (mockup tela 7): a pergunta em 36/700 e o texto
  // corrido em 20/400. PERGUNTA e nao TITULO: o 40/700 do Social ja tem o nome.
  TXT_ILHA_PERGUNTA, TXT_ILHA_TEXTO,
  // O TRECHO EM DESTAQUE da frase da ilha do relogio ("Ana recomendou
  // Fallout", ilha.c): o corpo do TXT_BODY (25) em Bold, porque o 600 do
  // mockup em texto claro sobre o miolo escuro e Bold pela regra optica de
  // text.c. No FIM, pela mesma razao do TXT_RANK_GRANDE.
  TXT_ILHA_FORTE,
  // AJUSTES NO GLASS UI (mockup ajustes-mockup.html, 03/10): os corpos que a
  // escala das ilhas nao tinha. Indice 21/500, rotulo da linha 23/500, valor
  // 20/500, chip Ligado 17/600, titulo do inspetor 32/700, tecla 14/700,
  // estado 18/400, caixa alta 13/700, e os corpos minusculos da home em
  // miniatura (9) e os numeros grandes dos paineis. ABAIXO DE ~24 px O CORPO E
  // O DO MOCKUP + ~8%: a InterDisplay embarcada e mais estreita que a Inter do
  // navegador (DESIGN.md §4: meca largura) — "Estilo, fileiras..." em 19 px
  // media 399 px contra 442 no mockup.
  TXT_AJ_ITEM, TXT_AJ_ROTULO, TXT_AJ_VALOR, TXT_AJ_CHIP, TXT_AJ_INSP,
  TXT_AJ_KBD, TXT_AJ_ESTADO, TXT_AJ_CAPS13, TXT_AJ_MINI9, TXT_AJ_MINI9B,
  TXT_AJ_VALOR18, TXT_AJ_NUM58, TXT_AJ_NUM64, TXT_AJ_NUM110, TXT_AJ_TIT28,
  TXT_AJ_MINI12, TXT_AJ_MINI13, TXT_AJ_MINI14, TXT_AJ_16B,
  TXT_AJ_SUB, TXT_AJ_SEG, TXT_AJ_18, TXT_AJ_SECAO, TXT_AJ_TEXTO, TXT_AJ_NOME,
  // ESCALA DO PLAYER NO GLASS UI (mockup aprovado em 03/10, player-mockup.html):
  // corpo e peso do CSS, com 600/800 em Bold pela regra optica de text.c.
  // TXT_G<corpo><R|M|B> = Regular, Medium, Bold. No FIM do enum, como manda o
  // comentario do TXT_RANK_GRANDE.
  TXT_G14B,   // kbd das dicas                    14 / 700
  TXT_G16B,   // rosto do idioma, rotulo da grade 16 / 600-800
  TXT_G18R,   // apoio a direita ("AIOStreams")   18 / 400
  TXT_G18M,   // o que falta no Seekr             18 / 500
  TXT_G19M,   // "termina as" da pilula           19 / 500
  TXT_G20B,   // tempo dos vizinhos no Seekr      20 / 600
  TXT_G20M,   // valor a direita na linha         20 / 500
  TXT_G21B,   // rotulo de botao (.pb, .btn)      21 / 600
  TXT_G22M,   // linha do guia parental           22 / 500
  TXT_G23B,   // linha do menu de visto           23 / 600
  TXT_G26B,   // tempo do Seekr, "Abrindo fonte"  26 / 600-700
  TXT_G28B,   // titulo do lembrete               28 / 700
  TXT_G30B,   // "T1 E3" do OSD                   30 / 600
  TXT_G30M,   // "· The Head" do OSD              30 / 500
  TXT_G52B,   // programa no OSD do canal         52 / 700
  // REGISTRO DO APP NO GLASS UI (logs-mockup.html, 03/10). TXT_MONO* sao a
  // JetBrains Mono embarcada (fonts/JetBrainsMonoNL-*.ttf, a variante SEM ligaduras: o "->" do log tem de sair como dois caracteres): SO as linhas do log
  // e o que a pessoa digita/le como codigo usam. Sem o arquivo, caem na fonte
  // da interface. Os corpos sao os do CSS: a mono e a mesma fonte do mockup,
  // entao nao leva o +8% da InterDisplay. TXT_LOG* sao os corpos Inter que a
  // escala acima nao tinha. No FIM do enum, como manda o TXT_RANK_GRANDE.
  TXT_MONO18,   // linha do log (.lgl)              18 / 400
  TXT_MONO18B,  // numero da linha "quadros"        18 / 600
  TXT_MONO16,   // area do log (.ar 15,5/500), host 16 / 400
  TXT_MONO15,   // tempo da etapa, linha [tv]       15 / 400
  TXT_MONO14,   // log no pacote (.pacote .lgl)     14 / 400
  TXT_MONO13,   // area no pacote, etiquetas [tex]  13 / 400
  TXT_MONO19,   // comando no painel vazio          19 / 400
  TXT_LOG_N44,  // fps no inspetor                  44 / 800
  TXT_LOG_T34,  // titulo do vazio e das dicas      34 / 700
  TXT_LOG_COD,  // codigo do registro (ladrilhos)   88 / 800
  TXT_LOG_19B,  // chip "Enviar agora", contagem    18 / 600
  TXT_LOG_18B,  // numero forte no medidor          17 / 600
  TXT_MONO14B,  // numero da linha "quadros" no pacote 14 / 600
  TXT_LOG_T31,  // "Registro do app" ao lado do relogio 30 / 700
  // NOVIDADES DA 1.8.0 E GUIA DE USO (mockups aprovados em 03/10). No FIM,
  // como manda o comentario do TXT_RANK_GRANDE.
  TXT_NOV_TITULO,   // .c-tit do cartao de novidades  50 / 700
  TXT_G28R,   // "Você está na" do cartao da atualizacao (mockup 26/400; a
              // InterDisplay e ~9% mais estreita que a Inter do navegador)
  // AJUSTES V2 (ajustes-v2.html, aprovado em 03/10): o menu grande, a lista
  // de 96 px e o inspetor de 40/26. Corpos medidos contra o mockup (a
  // InterDisplay e mais estreita que a Inter do navegador). No FIM do enum.
  TXT_V2_MENU,     // rotulo do menu de categorias (36/500)
  TXT_V2_MENU_B,   // rotulo da categoria aberta/em foco (36/600)
  TXT_V2_26,       // descricao, sub, meta, dicas grandes (24/400)
  TXT_V2_TIT,      // titulo da lista e do editor (52/700)
  TXT_V2_KICK,     // kicker da lista e do inspetor (18/700)
  TXT_V2_CHIP,     // chip Ligado/Avancados (22/600)
  TXT_V2_GRUPO,    // cabecalho de grupo (26/800)
  TXT_V2_24,       // contagem do grupo, dicas do inspetor (22/400)
  TXT_V2_ROT,      // rotulo da linha (32/500)
  TXT_V2_28,       // valor da linha, ajuda do inspetor (28 e 26/400)
  TXT_V2_SEG,      // segmentado e chips de grupo (24/600)
  TXT_V2_INSP,     // titulo do inspetor (40/700)
  TXT_V2_KBD20,    // tecla das dicas do resumo (20/700)
  TXT_V2_KBD18,    // tecla das dicas do inspetor (18/700)
  TXT_V2_NUM,      // contagem do resumo (52/800)
  TXT_V2_NUM150,   // numero do editor (150/800)
  TXT_V2_36B,      // unidade do editor (34/600)
  TXT_V2_18,       // posicao na ordem da Home (17/400)
  // Ajustes A3 (04/10): a lista compacta e o titulo da coluna da arte.
  TXT_V2_LN,       // rotulo da linha compacta (24/500)
  TXT_V2_LN_B,     // rotulo da linha em foco (24/600)
  TXT_V2_A3TIT,    // titulo da opcao sob a arte (46/700)
  // LISTA COM CAPA dos Salvos (painel lateral), ~17 % acima da escala da ilha.
  TXT_ILHA_NOME_L, TXT_ILHA_SUB_L, TXT_ILHA_HORA_L,
  // GUIA DA 2.0 (novidades20.c): "Nuvio 2.0" do hero (mockup 120/800) e o
  // negrito das frases dos capitulos (24/600, par do TXT_V2_24).
  TXT_W20_HERO, TXT_W20_24B,
  TXT_DESC_HORA,   // relogio da tela de descanso (230, Montserrat ExtraLight; so digitos e ':')
  // AJUSTES 2.0.3 (mockup v3, aprovado em 07/10): os corpos do mockup a 1920 com
  // os Ajustes no padrao de 80% (corpo = px do mockup / 0,8). No FIM do enum.
  TXT_V3_NOME,     // nome do cartao de categoria (33 / 700 no mockup)
  TXT_V3_VALOR,    // valor do cartao (26 / 400)
  TXT_V3_CHIP,     // pilulas do alto (23 / 600)
  TXT_V3_CHIP_R,   // a parte de estado da pilula (23 / 400)
  TXT_V3_DESC,     // titulo do cartao de descoberta (29 / 700)
  TXT_V3_SUB,      // frase do cartao de descoberta, grupo do indice (23 / 400)
  TXT_V3_LN,       // rotulo da linha (26 / 500)
  TXT_V3_LN_B,     // rotulo da linha em foco (26 / 700)
  TXT_V3_GRP,      // cabecalho de bloco da lista (26 / 700)
  TXT_V3_VAL,      // valor da linha (26 / 400)
  TXT_V3_BEN,      // frase de beneficio da linha em foco (20 / 400)
  TXT_V3_PILL,     // "Ligado"/"Desligado" escritos (21 / 600)
  TXT_V3_SELO,     // ESTA TV, ESTE PERFIL, AVANCADO, SO LG (14 / 700, caixa alta)
  TXT_V3_INSP,     // texto do inspetor (22 / 400)
  TXT_V3_INSP_B,   // "O que muda:" (22 / 700)
  TXT_NFONTES
} TxtEstilo;

// w/h em unidades de layout. pw/ph = tamanho da textura em pixels, preenchido
// so para linhas da camada ampliada (gfx_escala); 0 no caminho normal.
typedef struct { GLuint tex; int w, h; int pw, ph; } TxtLinha;

// A selecao de interface e legenda usa IDs compartilhados, mas preferencias
// independentes. Preserve os IDs legados: ficam gravados em dados existentes.
typedef enum {
  TXT_FAMILIA_INTER = 0,
  TXT_FAMILIA_LG,
  TXT_FAMILIA_DROID,
  TXT_FAMILIA_MONTSERRAT,
  TXT_FAMILIA_ROBOTO,
  TXT_FAMILIA_ATKINSON,
  TXT_FAMILIA_N
} TxtFamilia;

extern const char *const TXT_FAMILIAS_PT[TXT_FAMILIA_N];

// A interface e a legenda mantem preferencias independentes. A familia da
// interface afeta somente txt_linha()/os blocos e invalida as texturas de
// texto quando muda; chamadas txt_linha_familia continuam usando a familia
// que o chamador escolheu para a legenda.
void txt_definir_fonte_interface(TxtFamilia familia);
TxtFamilia txt_fonte_interface(void);
// Que fonte desenharia a linha `s` no estilo dado, em texto ("principal",
// "inter", "reserva:CJK:/caminho"). Para teste e diagnostico; NULL = nenhuma.
// Linha de legenda ja quebrada -> ordem visual, arabe moldado so se a fonte tem
// as formas de apresentacao (bidi.c). Devolve como bidi_visual_utf8.
int txt_bidi_legenda(TxtFamilia familia, TxtEstilo estilo, const char *in, char *out, size_t tam);
const char *txt_fonte_da_linha(TxtFamilia familia, TxtEstilo estilo, const char *s);
// O texto VISUAL que a linha desenharia (arabe/hebraico moldado e em ordem da
// direita para a esquerda; latim igual a `s`), em quantas corridas de fonte
// (a reserva arabe nao tem latim: o resto sai da fonte da interface) e, no
// retorno, quantos caracteres ficam sem glifo (o quadradinho). -1 sem fonte.
// Para o teste e o diagnostico.
int txt_visual_da_linha(TxtFamilia familia, TxtEstilo estilo, const char *s,
                        char *out, size_t tam, int *nCorridas);

// Instrumentacao: quantas linhas foram RASTERIZADAS (nao vieram do cache) no
// quadro e quanto tempo isso custou. Rasterizar texto e a operacao mais cara
// que acontece dentro de um quadro, e sem contador nao da para saber se um
// jank veio dai ou do upload de textura.
extern int    txt_rasterizadas;
// Despejos do cache de linhas. Diferente de zero com a tela parada = a tabela
// nao cabe no que a tela desenha, e o texto pisca.
extern int    txt_despejos;
extern double txt_ms;
// Linhas recusadas por falta de orcamento de rasterizacao (voltaram vazias).
// Leia a diferenca antes/depois de desenhar um bloco: zero = o bloco esta
// inteiro na tela; diferente de zero = ainda faltam linhas (proximo quadro).
extern int    txt_pendentes;
// Largura em unidades de layout que txt_linha() daria, SEM rasterizar nem
// gastar orcamento. Para medir/quebrar texto; nao desenha nada.
int  txt_largura(TxtEstilo estilo, const char *s);

// `dirRecursos` e a pasta que contem fonts/. No aparelho e a pasta do app; no
// Mac, a pasta do pacote — sem esse parametro a fonte so era procurada ao lado
// do executavel, e rodar local caia direto no fallback.
// `escala` e a razao entre o buffer e o canvas de layout (2 numa TV 4K, 1 em
// 1080p). As fontes sao abertas nesse tamanho e a linha devolvida continua
// medindo em unidades de layout — ver text.c.
int  txt_iniciar(const char *dirRecursos, float escala);
void txt_encerrar(void);

// Devolve linha cacheada. Cor em 0..255. Nunca devolve NULL; em falha, w/h = 0.
// Zera o orcamento de rasterizacao do quadro. Chamar uma vez por quadro, antes
// de desenhar; sem isso o orcamento se esgota e o texto some.
void txt_novo_quadro(void);

TxtLinha txt_linha(TxtEstilo estilo, const char *s, int r, int g, int b, int a);

// Igual a txt_linha, mas escolhe uma familia explicita. Usado pela legenda
// para manter sua preferencia independente da interface. Familia indisponivel
// cai no fallback ativo e registra o problema uma vez no log.
TxtLinha txt_linha_familia(TxtEstilo estilo, const char *s, int r, int g,
                           int b, int a, TxtFamilia familia);

// Linha que NUNCA passa de `maxW`: corta por palavra (ou por caractere, se uma
// palavra so ja estourar) e fecha com "…". Conteudo que vem de fora (nome de
// addon, genero do TMDB) nao tem comprimento garantido, e sem corte ele invade
// a coluna vizinha — foi o que apareceu no Top 10 e na folha de faixas.
TxtLinha txt_linha_corta(TxtEstilo estilo, const char *s, int r, int g, int b,
                         int a, float maxW);
TxtLinha txt_linha_corta_familia(TxtEstilo estilo, const char *s, int r, int g,
                                 int b, int a, float maxW, TxtFamilia familia);

// ENFASE, so para a legenda ASS. Negrito e italico aqui sao SINTETICOS (o
// pacote nao embarca face italica); ver a nota longa em linhaFamilia, em
// text.c. Para o resto do app o peso continua vindo do estilo, que e um
// arquivo de fonte de verdade — nao use isto para dar negrito a um titulo.
#define TXT_ENF_NEGRITO 1
#define TXT_ENF_ITALICO 2
TxtLinha txt_linha_corta_enfase(TxtEstilo estilo, const char *s, int r, int g,
                                int b, int a, float maxW, TxtFamilia familia,
                                int enfase);

// Desenha no canto superior esquerdo (x,y).
void txt_desenhar(TxtLinha l, float x, float y);
void txt_desenhar_alpha(TxtLinha l, float x, float y, float alpha);

// Desenha com ESPACAMENTO entre letras (tracking) e devolve a largura total.
// SDL_ttf nao tem tracking, e o titulo da pagina do tvOS depende dele: sem o
// espacamento largo o mesmo texto em maiusculas fica com cara de grito, nao de
// cabecalho. Passe x = -1 para so medir, sem desenhar.
float txt_tracking(TxtEstilo estilo, const char *s, int r, int g, int b,
                   float x, float y, float alpha, float tracking);

// Desenha texto QUEBRADO em linhas que cabem em `larg`, devolvendo a altura
// usada. Sem isso, qualquer texto de tamanho variavel (sinopse de episodio,
// nome de titulo) vaza para a coluna vizinha — nao existe "escrever curto o
// suficiente" quando o conteudo vem de fora.
// Tamanho em bytes do proximo TOKEN de uma quebra de linha que comeca em `s` (0 se
// `s` acaba, ou comeca em espaco ou \n): a palavra ate o espaco, ou UM caractere
// CJK com a pontuacao que nao pode abrir linha. Quem quebra texto por conta
// propria (agendaui.c) usa isto em vez de procurar o espaco, senao japones e
// chines viram uma "palavra" so e estouram a coluna. Ao juntar tokens, so poe
// espaco entre dois que vinham separados por espaco no texto.
size_t txt_token_tam(const char *s);

// Com alpha = 0, os blocos so medem: mesma quebra, sem rasterizacao nem GL.
float txt_bloco(TxtEstilo estilo, const char *s, int r, int g, int b,
                float x, float y, float larg, float leading, float alpha, int maxLinhas);

// Igual a txt_bloco, mas indica com reticencias quando `maxLinhas` omite texto.
// Uma palavra maior que a linha tambem e cortada com reticencias sem estourar
// a largura. maxLinhas <= 0 mantem o comportamento sem limite de linhas.
float txt_bloco_corta(TxtEstilo estilo, const char *s, int r, int g, int b,
                      float x, float y, float larg, float leading,
                      float alpha, int maxLinhas);

// Mesmo bloco, mas ALINHADO A DIREITA: cada linha termina em `xDir`. Os
// creditos do canto inferior direito precisam disso — alinhados a esquerda,
// eles ficam com a borda picotada contra a margem do cartao.
float txt_bloco_dir(TxtEstilo estilo, const char *s, int r, int g, int b,
                    float xDir, float y, float larg, float leading,
                    float alpha, int maxLinhas);

#endif
