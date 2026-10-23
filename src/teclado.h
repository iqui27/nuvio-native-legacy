// TECLADO DE TELA, a superficie de digitacao compartilhada.
//
// POR QUE EXISTE: a tela de busca ja tinha uma grade 6x6 de `a-z0-9`
// (busca.c:106), feita exatamente para este alfabeto e para o D-pad. Quando o
// codigo de pareamento precisou de digitacao, a saida obvia era copiar aquela
// grade para dentro da tela de amigos — e uma segunda grade e o comeco de duas
// que divergem: uma ganha a tecla de apagar maior, a outra nao; uma troca de
// alfabeto, a outra fica para tras.
//
// O QUE E COMPARTILHADO DE VERDADE: o ALFABETO (teclado_alfabeto, que busca.c
// tambem usa) e esta MODAL. A grade embutida na tela de busca continua la
// porque ela nao e modal — ela divide o foco com as fileiras de resultado a
// direita, e transformar aquilo numa camada por cima mudaria a tela de busca
// inteira, que nao e o assunto de quem so quer digitar seis caracteres.
//
// A FORMA: uma ilha de duas colunas. A ESQUERDA o contexto, o titulo, a dica,
// o campo e os modos (teclado da TV | falar | digitar pelo celular) — e, com o
// celular aberto, o QR embaixo deles, dentro da modal. A DIREITA a grade e a
// fileira apagar / limpar / concluir.
//
// O FOCO SEGUE O DESENHO: ESQUERDA na primeira coluna de qualquer fileira da
// grade entra na coluna da esquerda; DIREITA volta para a tecla de onde saiu.
// La dentro, o campo fica em cima e os segmentos embaixo (CIMA/BAIXO), e
// ESQUERDA/DIREITA andam entre os segmentos. CIMA da primeira fileira da grade
// nao faz nada.
//
// O ALFABETO E DO CHAMADOR, sempre: so se digita o que ele passou. A modal
// nasceu para codigo de pareamento (seis caracteres de `a-z0-9`) e esse
// teclado continua igual. O espaco existe onde o alfabeto o tem.
#ifndef NV_TECLADO_H
#define NV_TECLADO_H
#include <SDL2/SDL.h>
#include "arteurl.h"

// Os 36 caracteres da grade, em ordem de leitura (6 fileiras de 6).
const char *teclado_alfabeto(void);

// 64 e nao 24: o endereco de um portal IPTV ("meu-portal.exemplo.tv:8080") nao
// cabe em 24, e o campo de texto que a modal ja usa quando as caixas ficam
// estreitas rola pelo fim — texto longo sempre foi desenhavel aqui.
#define TECLADO_MAX 64
// Teto do campo LONGO: quem pede `max` acima de TECLADO_MAX ate isto. Existe
// para o token de configuracao do SpatialPosters (centenas de caracteres) e para
// a regex do auto-play, que o oficial guarda ate 500 (FR_REGEX_MAX): com 400, uma
// regex de 450 vinda da conta era cortada so de abrir o campo e dar OK, e o corte
// voltava para a conta. Em BYTES (o texto e UTF-8). Quem passa TECLADO_MAX ou
// menos continua igual.
// Posters Plus templates share the poster URL limit, including native/phone input.
#define TECLADO_LONGO (NV_ARTE_URL_MAX - 1)

// Abre a modal. `titulo` e a linha de cima ("Código do amigo"), `dica` a linha
// de apoio logo abaixo, e `max` o teto de caracteres (limitado a TECLADO_MAX).
// As duas frases passam por i18n no desenho, como todo texto do app.
void teclado_abrir(const char *titulo, const char *dica, int max);

// Mesma modal com ALFABETO proprio e valor inicial. `alfabeto` NULL cai no
// padrao a-z0-9; um alfabeto maior que 36 ganha fileiras (ate 7 de caractere)
// e, passando de 42, colunas (13, depois ate 20 — ver TE_COLS_LONGO em
// teclado.c e a #88); um menor encolhe a grade. `inicial` NULL comeca vazio.
//
// ALFABETO COM AS DUAS CAIXAS VIRA CAMADAS (ver CAMADAS em teclado.c): em vez
// de a-z, A-Z, digitos e sinais todos a vista, a grade mostra 0-9 e a-z em 10
// colunas, com uma tecla de MAIUSCULAS (um OK = so a proxima letra, dois =
// travada, tres = desliga) e, se os sinais nao couberem ao lado do "z", uma
// tecla que troca para a camada de SINAIS. O conjunto que pode ser digitado
// continua sendo exatamente o do alfabeto: maiuscula que ele nao tem nao sai,
// e alfabeto sem as duas caixas nao ganha tecla nenhuma.
//
// Existe porque endereco de portal precisa de ponto, dois pontos e hifen, e
// MAC precisa so de 0-9a-f e dois pontos — grades diferentes, uma modal so. Ver
// a nota no topo: a saida errada seria uma segunda modal.
void teclado_abrir_com(const char *titulo, const char *dica, int max,
                       const char *alfabeto, const char *inicial);
// Opt-in after opening an optional field: Concluir may submit empty text to
// clear its saved value. Every new keyboard resets this to disabled.
void teclado_permitir_vazio(int permitir);
// Depois de teclado_abrir_com: que campo e este. SENHA mostra um ponto por
// caractere; EMAIL e SENHA pedem o teclado certo ao sistema (Android). A
// proxima abertura volta a TEXTO.
// EMAIL e SENHA (#216) tambem tiram o modo do celular: quem entra por e-mail
// e quem nao tem celular a mao. EMAIL ganha 13 colunas e uma fileira de
// atalhos (.com, @gmail.com...); SENHA ganha "mostrar/ocultar". Onde o teclado
// do sistema abre sozinho (Android) ele ja abre aqui.
enum { TECLADO_TIPO_TEXTO = 0, TECLADO_TIPO_EMAIL, TECLADO_TIPO_SENHA };
void teclado_tipo(int tipo);
// Senha: pontos (1) ou texto (0). Quem abre pode comecar mostrando; quem
// fecha le o que a pessoa escolheu.
void teclado_mascarar(int liga);
// Depois de teclado_abrir_com: o texto que chega do CELULAR ou do teclado do
// sistema mantem os caracteres fora do ASCII (n, acentos, emoji) inteiros, em vez
// de descarta-los. Para a regex: "Espa(ñ|n)ol|🇪🇸" sem eles virava
// "Espa(|n)ol||", e a alternativa vazia casa com TUDO. A grade da TV continua
// com o alfabeto do chamador. A proxima abertura volta a so ASCII.
void teclado_aceitar_utf8(int liga);
// Linha de contexto da PROXIMA modal ("Contas e serviços · Chaves"), em caixa
// alta acima do titulo. Consumida pela abertura seguinte.
void teclado_contexto(const char *kicker);
int  teclado_mascarado(void);
int  teclado_aberto(void);
// Para testes: foco na coluna da esquerda (0 na grade; 1 campo, 2 Falar,
// 3 Digitar pelo celular, 4 o segmento Teclado da TV).
int  teclado_foco_campo(void);
// Para testes: camada a vista (0 letras, 1 sinais) e maiusculas (0 desligada,
// 1 so a proxima letra, 2 travada).
int  teclado_camada(void);
int  teclado_caixa(void);
// Para testes: finge o que existe neste aparelho (teclado da TV, voz,
// celular); -1 devolve cada um ao que o aparelho diz.
void teclado_teste_modos(int ime, int voz, int cel);
// Para testes: em que parte da modal o foco esta.
enum { TECLADO_FOCO_ESQUERDA = 0, TECLADO_FOCO_CARACTERE, TECLADO_FOCO_MEIO, TECLADO_FOCO_ACOES };
int  teclado_foco_tipo(void);
// Para testes: finge em que fileira do segmentado cada modo caiu (a quebra de
// verdade e medida no desenho e depende do idioma). Vale ate o proximo desenho.
void teclado_teste_quebra(int f0, int f1, int f2);
void teclado_evento(const SDL_Event *e);
void teclado_atualizar(float dt, Uint32 agora);
void teclado_desenhar(Uint32 agora);

// O que aconteceu, CONSUMIDO NA LEITURA (mesmo contrato de
// recomenda_pediu_abrir): quem pergunta duas vezes recebe TECLADO_NADA na
// segunda, e nao age duas vezes sobre o mesmo OK.
enum { TECLADO_NADA = 0, TECLADO_PRONTO, TECLADO_CANCELOU };
int  teclado_resultado(void);
// O que foi digitado. Continua valido depois de teclado_resultado(); so a
// proxima abertura o zera.
const char *teclado_texto(void);
// Zera o que foi digitado (senha): quem leu teclado_texto() e nao precisa mais.
void teclado_esquecer(void);

#endif
