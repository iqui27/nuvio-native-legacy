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
#define FIL_MAX      64
#define FIL_CHAVE   192
#define FIL_TITULO   96

// Limite de fileiras da home. 7 e o pedido do dono; o teto continua sendo o
// CAT_FIL_MAX (16) do web para este runtime, e abaixo de 3 a home deixa de ser
// uma home.
#define FIL_LIMITE_MIN     3
#define FIL_LIMITE_MAX    16
#define FIL_LIMITE_PADRAO  7

// Formas de card que a home JA implementa (o enum TipoFileira de home.h). NAO
// ha tipo novo aqui: a tela de Ajustes so oferece o que o desenho sabe fazer, e
// a traducao para TipoFileira acontece em home.c, no unico lugar que conhece as
// medidas de cada forma.
typedef enum {
  FIL_TIPO_AUTO = 0,   // como a home decide hoje (nome do catalogo + prefs)
  FIL_TIPO_CARTAZ,     // FILEIRA_NORMAL   — cartaz em pe 2:3
  FIL_TIPO_DESTAQUE,   // FILEIRA_DESTAQUE — arte deitada grande
  FIL_TIPO_COLECAO,    // FILEIRA_COLECAO  — deitada intermediaria
  FIL_TIPO_SERVICO,    // FILEIRA_SERVICO  — deitada compacta
  FIL_TIPO_TOP10,      // FILEIRA_TOP10    — ranking
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

// --- limite ------------------------------------------------------------------
int  fil_limite(void);
void fil_definir_limite(int n);

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
// 0 quando a forma do card NAO e escolha desta fileira: "Continuar assistindo"
// tira a forma de `continueWatchingCardStyle`, o feed dos amigos precisa do
// card com autoria e um grupo de colecao desenha atalhos, nao titulos. Oferecer
// os cinco tipos nelas seria oferecer um ajuste sem efeito.
int         fil_aceita_tipo(int i);

// --- mutacao (tela de Ajustes) ----------------------------------------------
void fil_alternar(int i);
void fil_ciclar_tipo(int i);
void fil_ciclar_tam(int i);
// Troca a linha com a vizinha e devolve o novo indice dela (o mesmo, se nao deu
// para mover). E o gesto de "pegar e mover" da tela de reordenar.
// Alinha a lista de Ajustes com a ordem que a home desenha, ENQUANTO a pessoa
// nunca tiver reordenado. Depois do primeiro fil_mover nao faz nada: dali em
// diante quem manda e a escolha dela. Ver a nota longa em fileiras.c.
void fil_espelhar_ordem(const char *const *chaves, int n);
int  fil_mover(int i, int direcao);

// --- consulta por chave (descoberta.c e home.c) ------------------------------
int   fil_oculta(const char *chave);
int   fil_tipo(const char *chave);    // FIL_TIPO_AUTO quando nao foi escolhido
float fil_escala(const char *chave);  // 1.0 quando nao foi escolhido

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

#endif
