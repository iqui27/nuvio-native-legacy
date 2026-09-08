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
void fil_registrar(const char *chave, const char *titulo);

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
