// Fileiras da home: a escolha que a pessoa faz NA TV.
//
// Ate agora a ordem e a ocultacao das fileiras so existiam vindas da CONTA
// (catordem.c) e o nativo nao tinha tela nenhuma: para mexer na home da TV era
// preciso abrir o app do celular. Aqui ficam as quatro coisas que a tela de
// Ajustes passou a oferecer — LIMITE de fileiras, ORDEM, liga/desliga por
// fileira, e a forma e o tamanho do card de cada uma.
//
// NADA DISTO E ENVIADO PARA A CONTA, e nao e esquecimento nem trabalho pela
// metade. A trava esta escrita no topo de catordem.h: a lista que a TV consegue
// montar e MENOR que a real sempre que um addon demora a responder, e empurrar
// de volta apagaria a configuracao da pessoa nos outros aparelhos — medido como
// `{"localItems":54,"remoteItems":43}` em todo arranque na OLED65C9. Por isso a
// escolha feita aqui e LOCAL, vive em dados_dir()/fileirasui.txt e VENCE a da
// conta quando existir.
//
// PARA QUEM FOR "COMPLETAR O SYNC" DEPOIS: o que falta nao e a chamada de push.
// E uma lista COMPLETA de catalogos para empurrar — enquanto a TV so conhece os
// addons que responderam neste arranque, todo push apaga o que ela nao viu.
// Sem resolver isso, acrescentar o envio troca um recurso local que funciona
// por uma perda de dados silenciosa nos outros aparelhos.
#ifndef NV_FILEIRAS_H
#define NV_FILEIRAS_H

// 64 como o PREF_MAX de descoberta.c e o CATORD_MAX de catordem.c. O Xperience
// sozinho declara 605 catalogos: a tela de Ajustes lista os 64 PRIMEIROS na
// ordem efetiva da home, que sao os unicos que disputam as (no maximo 16)
// posicoes desenhadas. Listar centenas numa lista de D-pad seria inutilizavel e
// o resto nunca chegaria perto de virar fileira.
// Quantas fileiras a folha dos Ajustes consegue LISTAR para a pessoa ordenar.
//
// Era 64 e ficou pequeno quando a cota por addon (descoberta.c) passou a deixar
// todo addon declarar: naquela medicao eram 135 catalogos declarados, e os do
// addon lido por ultimo — o Bingecat — nao entravam na lista. Ou seja, alem de
// nao aparecerem na home, eles nao podiam nem ser LIGADOS, que e a outra metade
// do relato.
//
// 192 NAO COBRIU, e o sintoma foi o mesmo com outra cara. Medido na C9 em
// 15/09/2026: a conta declara 279 catalogos, `fileirasui-p1.txt` tinha exatas
// 192 linhas (o teto, cheio) e SEIS fileiras que estavam desenhadas na home —
// In Theaters, Dragon Ball (filme e serie), Evangelion (filme e serie) e Ghost
// in the Shell — nao existiam na tela de fileiras. Nao dava para move-las nem
// para desliga-las. O relato do dono foi exatamente esse: "algumas fileiras que
// aparecem na home sao diferentes das que estao na tela de reordenar".
//
// 320 e o teto de hoje (~96 KB de tabela, ~38 KB de arquivo), mas o numero
// sozinho nunca resolve: o Xperience declara 605 catalogos por conta propria.
// Por isso fil_registrar passou a DESPEJAR uma entrada dispensavel quando a
// tabela enche, em vez de recusar a nova em silencio — a regra esta la.
// 768 desde 20/09/2026: a C9 do dono lotou os 320 so com o Xperience (283
// linhas, todas com forma vinda da conta e por isso "escolhidas"), e o
// despejo nao tinha o que despejar. ~230 KB de tabela; cabe nos dois alvos.
#define FIL_MAX      768
#define FIL_CHAVE   192
#define FIL_TITULO   96

// Limite de fileiras da home. 7 e o pedido do dono; o teto e o CAT_FIL_MAX
// (40, o do navegador de mesa), e abaixo de 3 a home deixa de ser uma home.
// Era 16. Acima de FIL_LIMITE_SEGURO (o teto antigo, que rodou em TV de verdade)
// a tela de Ajustes mostra o aviso de memoria e o modo seguro (seguro.h) passa
// a vigiar a mudanca.
#define FIL_LIMITE_MIN     3
#define FIL_LIMITE_MAX    40
#define FIL_LIMITE_PADRAO  7
// Ate aqui e o que sempre foi permitido sem aviso.
#define FIL_LIMITE_SEGURO 16
// Acima disto a mudanca entra no diario do modo seguro (reverte sozinha se o
// app cair logo depois).
#define FIL_LIMITE_VIGIADO 12

// Formas de card que a home JA implementa (o enum TipoFileira de home.h). A
// tela de Ajustes so oferece o que o desenho sabe fazer, e a traducao para
// TipoFileira acontece em home.c, no unico lugar que conhece as medidas.
typedef enum {
  FIL_TIPO_AUTO = 0,   // como a home decide hoje (nome do catalogo + prefs)
  FIL_TIPO_CARTAZ,     // FILEIRA_NORMAL   — cartaz em pe 2:3
  FIL_TIPO_DESTAQUE,   // FILEIRA_DESTAQUE — arte deitada grande
  FIL_TIPO_COLECAO,    // FILEIRA_COLECAO  — deitada intermediaria
  FIL_TIPO_SERVICO,    // FILEIRA_SERVICO  — deitada compacta
  FIL_TIPO_TOP10,      // FILEIRA_TOP10    — ranking
  // Fica no fim para nao alterar os numeros ja gravados para os tipos acima.
  FIL_TIPO_DESTAQUE_QUADRADO, // FILEIRA_DESTAQUE_QUADRADO — 4:3 maior
  // Issue #201: o ranking de numeral grande que a Dinamica so dava ao primeiro
  // catalogo "Top"/"Em alta" em Automatico, agora escolhivel em qualquer
  // fileira de catalogo e em qualquer layout. No fim pelo mesmo motivo.
  FIL_TIPO_RANKING,           // FILEIRA_TOP10_NUM — numeral grande ao lado do cartaz
  // A faixa deitada com o titulo dentro que a Dinamica alterna com o cartaz em
  // Automatico. Desenhada em qualquer layout (home.c nao a prende a Dinamica);
  // so nao tinha numero para ser escolhida. No fim pelo mesmo motivo.
  FIL_TIPO_LARGA,             // FILEIRA_LARGA — 16:9 com o nome dentro do cartao
  // Os dois tamanhos MAIORES do Destaque 4:3 (dono, 01/10: "o maior do tamanho
  // dos cards da Apple TV"). A mesma forma (FILEIRA_DESTAQUE_QUADRADO), so mais
  // larga e mais alta: o fator mora em fil_tipo_fator. No fim pelo mesmo
  // motivo dos outros — o 6 gravado continua sendo o 4:3 de sempre.
  FIL_TIPO_DESTAQUE_QUADRADO_M, // 4:3 x1,25 — dois cards e meio por tela
  FIL_TIPO_DESTAQUE_QUADRADO_G, // 4:3 x1,5  — dois cards e um pedaco
  FIL_TIPO_N
} FilTipo;

// Tamanho do card da fileira, como fator sobre a medida do tipo. Nao e uma
// medida em px porque cada forma tem a sua: um fator preserva a proporcao
// medida no app web em vez de inventar um segundo conjunto de numeros.
typedef enum {
  FIL_TAM_COMPACTO = 0, FIL_TAM_PADRAO, FIL_TAM_GRANDE, FIL_TAM_N
} FilTam;

const char *fil_tipo_rotulo(int t);
const char *fil_tam_rotulo(int t);
float       fil_tam_escala(int t);
// Fator de TAMANHO que a propria forma carrega: 1 em todas, menos nos dois
// Destaques 4:3 maiores. Multiplica o fator de Tamanho da fileira (fil_escala).
float       fil_tipo_fator(int t);

// DE ONDE A FILEIRA VEIO. Sem isto a tela de Ajustes lista quinze nomes soltos
// e nao ha como saber que "A24" e um grupo de colecoes, que "Popular" e um
// catalogo do Cinemeta e que "Entre amigos" nao vem de addon nenhum — foi
// exatamente o relato ("mostre o que e lista e o que e catalogo").
//
// SAI DA CHAVE, e nao de um campo novo no arquivo. As tres origens ja estao
// codificadas nela e sempre estiveram: as fileiras que o proprio app monta tem
// chave SINTETICA (fixa, escrita em home.c), um grupo de colecoes tem o prefixo
// `collection_` que col_chave_grupo escreve, e todo o resto e homeCatalogKey de
// um catalogo declarado por addon. Derivar em vez de guardar significa que o
// fileirasui.txt de quem ja usa o app continua valendo byte a byte — e que a
// classificacao nao pode ficar velha.
typedef enum {
  FIL_ORIGEM_APP = 0,   // Continuar assistindo, Entre amigos, Retomar agora
  FIL_ORIGEM_COLECAO,   // grupo de colecoes (chave collection_<id>)
  FIL_ORIGEM_CATALOGO,  // catalogo declarado por um addon
  FIL_ORIGEM_N
} FilOrigem;

// Pura: so olha a chave. Chave vazia conta como FIL_ORIGEM_APP, que e o mesmo
// tratamento conservador que formaFixa ja dava.
int         fil_origem_de(const char *chave);
const char *fil_origem_rotulo(int origem);   // "Do app" | "Coleção" | "Catálogo"
// Nome do arquivo em art/icones para o selo desta origem. Sao icones REAIS
// (SVG do app web rasterizado), nao forma desenhada a mao — ver gfx_icone.
const char *fil_origem_icone(int origem);
// Frase que explica a origem na area de ajuda, ja em portugues.
const char *fil_origem_ajuda(int origem);

// --- destaque ----------------------------------------------------------------
// O QUE ALIMENTA O DESTAQUE da home. Tres respostas, e a terceira e a que o
// dono pediu ("poder substituir e colocar o que quiser la"):
//   ""   automatico — os primeiros titulos do catalogo, como sempre foi
//   "*"  sorteio do catalogo
//   ...  a chave de UMA fileira: o destaque passa a mostrar os titulos dela
//
// A chave pode apontar para uma fileira que nao existe mais (addon removido).
// Quem le trata isso como "automatico" em vez de apagar a escolha: o addon pode
// voltar, e apagar em silencio faria a preferencia sumir sem ninguem pedir.
const char *fil_hero_fonte(void);
void        fil_definir_hero_fonte(const char *chave);

// --- limite ------------------------------------------------------------------
int  fil_limite(void);
// O que esta GRAVADO, sem o teto do perfil seguro. fil_limite() e o que a home
// usa; a tela de Ajustes mostra e edita este, senao editar durante o perfil
// seguro gravaria o teto de emergencia por cima da escolha da pessoa.
int  fil_limite_gravado(void);
// Teto SO desta sessao (0 = sem teto). O perfil seguro (seguro.h) usa para a
// home montar menos fileiras sem tocar em fileirasui-p<N>.txt.
void fil_definir_teto_sessao(int teto);
// Ao BAIXAR o limite, as ligadas que ficaram alem dele viram "fora da home"
// (ocultas), nao fila. Decisao do dono; ver o comentario na definicao.
void fil_definir_limite(int n);
// A SETA DA TELA DE AJUSTES (issue #197): cada passo so muda o numero; quem
// ficou alem do valor FINAL vira "fora" em fil_confirmar_limite, chamado quando
// a edicao da linha termina. Sem rajada em curso, confirmar nao faz nada.
void fil_ajustar_limite(int n);
void fil_confirmar_limite(void);

// --- na home, na fila, fora --------------------------------------------------
// A ORDEM E A FILA. As primeiras `limite` linhas ligadas, na ordem local, sao
// as que a home monta; as ligadas depois disso estao NA FILA e entram sozinhas
// quando alguem antes delas e removido; as ocultas estao fora. Nao ha campo
// novo no arquivo: e a mesma ordem e o mesmo `oculta` de sempre, lidos de
// outro jeito — e e assim que a fila sobrevive byte a byte ao arquivo antigo.
typedef enum { FIL_NA_HOME = 0, FIL_NA_FILA, FIL_FORA } FilEstado;
int  fil_estado(int i);
// Reconcile the account collection identities immediately, including an empty
// authoritative snapshot. Only removed collection rows are pruned; local
// catalogue/app choices and styles on surviving collection IDs remain intact.
void fil_colecoes_reconciliar(const char *const *chaves,
                              const char *const *titulos,
                              const int *ocultas, int n, int autoritativo);
int fil_copiar_chaves(char (*saida)[FIL_CHAVE], int max);
// Complete known-row projection of account order/visibility. Personal local
// order and preferences survive; remote flags are kept only in memory.
void fil_conta_reconciliar(const char *const *chaves, const int *ocultas,
                           const int *emColecao, int n);
void fil_colecao_catalogo_removido(const char *chave);
void fil_colecao_catalogo_restaurado(const char *chave);
// Scope only account-derived automatic flags; does not clear personal settings.
int fil_conta_dono(const char *usuario);
int  fil_linha_sem_addon(int i); // #327: nenhum addon ligado declara mais este catalogo
int  fil_adicionada_na_tv(const char *chave); // #327: adicionada a mao, vence a colecao
int  fil_estado_chave(const char *chave);   // -1 = desconhecida
int  fil_n_na_home(void);
int  fil_n_fila(void);
int  fil_n_capacidade(void); // catalogue slots used, excluding app/collections
// Liga e poe no fim do bloco ligado. Devolve o indice NOVO (a linha se move) e
// escreve em `estado` onde ela caiu — NA_HOME quando coube, NA_FILA quando a
// home estava cheia. Quem chama mostra "home cheia" nesse caso.
int  fil_adicionar(int i, int *estado);
void fil_remover(int i);
// Ligada alem do limite que NAO foi posta na fila pela pessoa vira "fora".
// Chamar ao abrir a folha. Ver a definicao para a protecao da vaga garantida.
void fil_normalizar(void);

// --- perfil e poda -----------------------------------------------------------
// A escolha e POR PERFIL (fileirasui-p<N>.txt; 0 = o arquivo antigo, que serve
// de semente ao primeiro arquivo do perfil 1, e so dele). Chamar na troca de
// perfil.
void fil_definir_perfil(int perfil);
// Bumps on every real profile switch (#294): snapshots taken for the previous
// profile are stale once this changes.
unsigned fil_perfil_geracao(void);

// #392: a lista de addons que a descoberta tem na mao e deste perfil? 0 (pacote,
// sem conta) vale; o perfil cuja conta mandou a lista (addons_perfil_da_lista)
// diferente do perfil das fileiras, nao: na janela depois de uma troca a lista
// ainda e a do perfil que saiu, e registrar os catalogos dela no arquivo do
// perfil novo despeja as fileiras dele e acrescenta as do outro.
int fil_lista_e_deste_perfil(int perfilDaLista);

// #392: FOTO DE UMA PASSADA. A descoberta tira a foto (perfil em uso da lista +
// geracao do perfil das fileiras) quando le a lista e registra com ela: a
// comparacao com o estado de agora e feita DENTRO do mutex do registro, entao
// uma passada velha (a troca de perfil aconteceu enquanto ela rodava) e recusada
// de forma atomica.
typedef struct { int perfilLista; unsigned geracao; } FilPassada;
FilPassada fil_passada_ler(void);
int  fil_passada_valida(const FilPassada *p);
void fil_registrar_de(const FilPassada *p, const char *chave, const char *titulo,
                      const char *addon, const char *conteudo, int itens);
void fil_registrar_se_couber_de(const FilPassada *p, const char *chave,
                                const char *titulo, const char *addon,
                                const char *conteudo);
// Tira da lista os catalogos de addons que ja nao estao na conta. `ids` e
// `bases` sao os ids de manifesto e as URLs base dos addons ATUAIS; so vale
// depois de todos os manifestos da volta terem sido lidos. `perfilDaLista` e o
// perfil cuja CONTA mandou essa lista (addons_perfil_da_lista): diferente do
// perfil desta escolha, ou 0, nao poda nada. Linha com escolha da pessoa
// (desligada, na fila, forma, tamanho, destaque) nunca e podada. Devolve
// quantas sairam.
int  fil_podar_catalogos(const char *const *ids, const char *const *bases, int n,
                         int perfilDaLista);

// #319: marca (so em memoria) as linhas de catalogo cujo addon saiu da conta ou
// foi desligado, para elas nao ocuparem vaga do limite nem aparecerem na home.
// `ativos[k]` = addon k ligado. Devolve quantas linhas mudaram de estado.
int  fil_marcar_sem_addon(const char *const *ids, const char *const *bases,
                          const int *ativos, int n, int perfilDaLista);

// O addon (id do manifesto, ou base sem id) e NOVO para este perfil nesta TV:
// nenhuma fileira dele veio do arquivo do perfil e nenhuma carrega escolha. E o
// que decide a vaga garantida (cota_vaga_garantida, cotacat.h).
int  fil_addon_novo(const char *id, const char *base);

// --- registro das fileiras que EXISTEM --------------------------------------
// Chamado por quem monta a lista (descoberta.c com os catalogos declarados,
// home.c com os grupos de colecao). Idempotente, e o PRIMEIRO nome registrado e
// o que fica — ver a nota dentro de fil_registrar. Existe porque a tela de
// Ajustes precisa mostrar TAMBEM as fileiras desligadas: sem o registro,
// desligar uma seria irreversivel pela TV, ja que ela deixa de aparecer em
// cat_fileira().
//
// `addon` e o NOME do addon que declara o catalogo ("Xperience"); `conteudo` e
// o "movie"/"series" do catalogo; `itens` e quantos titulos a fileira tem AGORA
// (-1 quando quem registra ainda nao sabe — a descoberta registra os candidatos
// antes de pedir qualquer um deles). Os tres sao vazios/-1 para o que nao e
// catalogo, e os tres podem chegar de dois registradores diferentes: quem
// souber primeiro preenche.
//
// ESTES TRES NAO VAO PARA O ARQUIVO, de proposito, e sao duas razoes:
//   1. O formato de fileirasui.txt e posicional por tabulacao com o titulo no
//      fim; acrescentar campos faria o arquivo de quem ja usa o app ser
//      DESCARTADO linha a linha (ver carregar), perdendo ordem e liga/desliga.
//   2. `itens` muda a cada ciclo de sync. Gravar aqui reescreveria o arquivo e
//      bumparia `revisao` por nada — o mesmo defeito que o nome piscando ja
//      causou. Por isso nenhum dos tres marca `registroSujo`.
void fil_registrar(const char *chave, const char *titulo,
                   const char *addon, const char *conteudo, int itens);

// Igual a fil_registrar, mas NUNCA despeja: com a tabela cheia a chave nova
// fica de fora, calada. E para os catalogos que a cota por addon de
// descoberta.c nao leu nesta volta (issue #126, Ultra MAX com 174 catalogos e
// 32 de cota): eles entram na lista para a pessoa poder escolher um, e escolher
// e o que faz a cota da volta seguinte le-lo. Nao valem a vaga de ninguem.
void fil_registrar_se_couber(const char *chave, const char *titulo,
                             const char *addon, const char *conteudo);

// A pessoa ESCOLHEU esta fileira na TV? Devolve a posicao dela entre as ligadas
// (0 = primeira) quando ela esta na home pela escolha local — ligada e dentro do
// limite, ou posta na fila pela pessoa —, e -1 quando nao (desconhecida,
// oculta, ou ligada so por ter entrado no fim). E a pergunta que a cota de
// declaracoes de descoberta.c faz antes de decidir quais catalogos de um addon
// grande ler.
int fil_escolhida(const char *chave);

// Grava o que fil_registrar acumulou, se houver. Chamar UMA VEZ no fim da
// varredura que registra: registrar 64 chaves novas com gravacao a cada uma sao
// 64 reescritas do arquivo no primeiro arranque, e no webOS cada uma passa
// ainda pelo flush de dados.c. As mutacoes da tela de Ajustes continuam
// gravando na hora — la e uma tecla, uma escrita.
void fil_gravar_registro(void);

int         fil_n(void);
const char *fil_chave(int i);
const char *fil_titulo(int i);
int         fil_linha_oculta(int i);
int         fil_linha_tipo(int i);
int         fil_linha_tam(int i);
// Origem desta linha, e o que a acompanha. `fil_linha_addon` devolve "" quando
// nenhum registrador soube dizer (addon que sumiu da conta, fileira lida so do
// arquivo); `fil_linha_conteudo` devolve "Filmes"/"Séries"/""; `fil_linha_itens`
// devolve -1 quando a fileira ainda nao foi montada nesta sessao.
int         fil_linha_origem(int i);
const char *fil_linha_addon(int i);
const char *fil_linha_conteudo(int i);
int         fil_linha_itens(int i);
// `fil_linha_na_home`: a ultima home montada tinha esta fileira.
// `fil_linha_vista`: algum registrador a viu nesta sessao — catalogo que a
// descoberta declarou mas o limite cortou tem vista=1 e naHome=0; linha de
// addon que foi embora tem as duas zeradas e a folha a marca "Fora da Home".
int         fil_linha_na_home(int i);
int         fil_linha_vista(int i);
// 0 quando a forma do card NAO e escolha desta fileira: "Continuar assistindo"
// tira a forma de `continueWatchingCardStyle` e o feed dos amigos precisa do
// card com autoria. Um grupo de colecao ACEITA, mas so as formas dele
// (paisagem, quadrado, pôster — ver fil_estilos).
int         fil_aceita_tipo(int i);

// --- mutacao (tela de Ajustes) ----------------------------------------------
void fil_alternar(int i);
// Numa colecao o ciclo e so o das formas dela (fil_estilos).
void fil_ciclar_tipo(int i);

// --- ESTILO DA FILEIRA pelo menu do cartaz (ctxmenu.c) ----------------------
// O mesmo `tipo` que a tela de Ajustes grava, no mesmo fileirasui-p<N>.txt: as
// duas telas leem e escrevem o mesmo campo, entao nunca discordam.
//
// As opcoes do menu para esta chave, na ordem de exibicao, com o rotulo em pt
// (passa por i18n no desenho). Catalogo: TODAS as formas de FilTipo —
// Automatico, Posteres, Paisagem pequena, media e grande, Faixa com titulo,
// Destaque 4:3 (e o medio e o grande), Ranking numerado e empilhado. Colecao:
// Automatico (a forma que a conta mandou), Paisagem, Quadrado, Poster
// (FIL_TIPO_COLECAO/DESTAQUE_QUADRADO/CARTAZ). Fileira do app: 0 opcoes.
int  fil_estilos(const char *chave, int *tipos, const char **rotulos, int max);

// AS LINHAS DO MODAL DE ESTILO. Formas que so diferem em TAMANHO dividem uma
// linha so (dono, 01/10: "Paisagem pequena/media/grande vira UM item"): a linha
// diz o nome da forma e os tamanhos dela, do menor ao maior, e o modal escolhe
// o tamanho com esquerda/direita. `nomes` e o nome inteiro de cada tamanho
// ("Paisagem grande"), em pt — e o titulo da previa. fil_estilos e esta mesma
// lista achatada, na mesma ordem.
#define FIL_ESTILO_TAMS 3
typedef struct {
  const char *rotulo;
  int n;
  int tipos[FIL_ESTILO_TAMS];
  const char *nomes[FIL_ESTILO_TAMS];
} FilEstiloLinha;
int  fil_estilo_linhas(const char *chave, FilEstiloLinha *linhas, int max);
// Palavra do tamanho k (0 pequeno, 1 medio, 2 grande) em pt. O modal desenha
// so a PRIMEIRA LETRA da traducao (P M G, S M L, K M G...).
const char *fil_estilo_tam_palavra(int k);
// Rotulo do tipo `t` PARA ESTA CHAVE: numa colecao o numero quer dizer outra
// palavra (FIL_TIPO_CARTAZ e "Pôster", nao "Cartaz em pé").
const char *fil_estilo_rotulo(const char *chave, int t);
// Uma frase, em pt, que diz o que a forma `t` e NESTA chave (passa por i18n no
// desenho). E a legenda da previa do modal de estilo (ctxmenu.c).
const char *fil_estilo_ajuda(const char *chave, int t);
const char *fil_linha_tipo_rotulo(int i);
// Grava a forma da fileira. 0 quando a chave nao e conhecida, e fixa, ou o
// tipo nao vale para ela.
int  fil_definir_tipo(const char *chave, int t);
void fil_ciclar_tam(int i);
// Troca a linha com a vizinha e devolve o novo indice dela (o mesmo, se nao deu
// para mover). E o gesto de "pegar e mover" da tela de reordenar.
// Alinha a lista de Ajustes com a ordem que a home desenha. Sem ordem local
// reordena o bloco da tela na ordem dela; com ordem local a ordem da pessoa
// fica e as chaves novas entram no fim. Linhas que a home nao tem ficam onde
// estavam e ganham naHome=0 — mover uma colecao que so monta tarde para o fim
// apagaria a posicao que a pessoa deu a ela (a folha as marca "Fora da Home").
// Chave da tela que a lista nao conhece (tabela cheia) toma a vaga de uma
// linha morta sem escolha. `titulos` pode ser NULL — sem ele a linha resgatada
// mostra a chave ate o proximo registro.
void fil_espelhar_ordem(const char *const *chaves,
                        const char *const *titulos, int n);
int  fil_mover(int i, int direcao);
// Move o BLOCO inteiro de um addon para cima ou para baixo, trocando com o
// bloco vizinho inteiro. Devolve o novo indice da linha `i`, ou `i` se nao
// deu para mover.
int  fil_mover_grupo(int i, int direcao);
// Reordena a lista inteira por addon: fixas do app, colecoes, depois catalogos
// agrupados pelo nome do addon. Marca ordemLocal — a ordenacao e escolha da
// pessoa.
void fil_ordenar_por_addon(void);

// --- consulta por chave (descoberta.c e home.c) ------------------------------
int   fil_oculta(const char *chave);
int   fil_tipo(const char *chave);    // FIL_TIPO_AUTO quando nao foi escolhido
float fil_escala(const char *chave);  // 1.0 quando nao foi escolhido
// O fator que a fileira teria COM a forma `t`: o Tamanho dela vezes o fator da
// forma (fil_tipo_fator). E o que a previa do modal precisa — a forma em foco
// ainda nao foi gravada. fil_escala(chave) == fil_escala_tipo(chave, tipo dela).
float fil_escala_tipo(const char *chave, int t);

// 1 quando existe ordem LOCAL gravada. Sem ela, fil_unir e a identidade e a
// ordem da conta (catordem.c) continua valendo sozinha.
int  fil_tem_ordem(void);

// Sobe a cada mudanca desta escolha. Quem cacheia a lista de fileiras remonta
// quando este numero mudar — sem isto, desligar uma fileira em Ajustes so
// valeria depois de a rede trocar o catalogo, que e o mesmo defeito que a
// assinatura de preferencias de home.c ja teve.
unsigned fil_revisao(void);

// A regra, no mesmo formato de catordem_unir: recebe as chaves na ordem em que
// ja estao e escreve em `saida` os INDICES delas na ordem final — primeiro as
// que a escolha local conhece, na ordem local; depois todas as outras, no fim,
// na ordem original. Nada e descartado.
int  fil_unir(const char *const *chaves, int n, int *saida, int max);

// Esquece a escolha e o registro. Chamar no logout junto do resto: a home e da
// conta de quem saiu.
void fil_esquecer(void);

// LIMPEZA UNICA DO #197 (ver a definicao): desfaz, so no arquivo com o padrao
// exato do defeito, a rajada do limite e os catalogos fora da cota que entraram
// ligados. `contaLigadas` sao as chaves que a ordem da CONTA tem e nao desligou.
// Roda uma vez por arquivo de perfil (marca "migracao 197"); devolve quantas
// linhas mudaram.
int fil_migrar_197(const char *const *contaLigadas, int n);

#endif
