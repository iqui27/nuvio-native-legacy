// O CARTAO DE NOVIDADES DA VERSAO ATUAL — um motor so, um conteudo por versao.
//
// ATE A 2.0.2 cada versao tinha o proprio novidadesXXX.c (500 a 700 linhas,
// copia da anterior) e uns oito ganchos no app.c. Daqui em diante o desenho, as
// paginas, os botoes, o foco, o ponteiro do Magic Remote, a animacao e a marca
// de "ja visto" moram aqui (novidades_cartao.c), e o que muda a cada release
// fica num arquivo de CONTEUDO: src/novidades/<versao>.inc (titulo, subtitulo,
// grupos de mudancas e as cenas da previa). O app.c chama SEMPRE estas
// funcoes; a proxima versao troca o .inc incluido em novidades_cartao.c e
// nada mais.
//
// O VISUAL e o da 2.0.2 (aprovado): a previa viva a esquerda, as mudancas em
// grupos a direita, os botoes no canto de baixo e, no fim, "Apoie o projeto"
// com os QRs (apoio.h). Quando a lista nao cabe numa pagina, o motor quebra
// por GRUPO em mais paginas (alturas medidas e guardadas ao abrir/trocar idioma).
// Sem cenas, usa uma coluna centrada, sem previa, com altura pela lista.
//
// QUANDO ABRE: uma vez por versao, na Home pronta, sem player nem pagina do
// titulo por cima (novcartao_decidir), para quem JA viu o guia da 2.0. Quem
// ainda vai ver o guia (instalacao nova ou vindo da 1.x) recebe o guia e esta
// marca gravada. Ao decidir, grava tambem as marcas dos cartoes antigos
// (2.0.1, 2.0.2): eles nunca abrem depois deste.
#ifndef NV_NOVIDADES_CARTAO_H
#define NV_NOVIDADES_CARTAO_H
#include <SDL2/SDL.h>

// As plataformas de um grupo, item ou cena (0 = todas).
#define NOV_LG       0x1u
#define NOV_TPK      0x2u   // Samsung .tpk (nativo)
#define NOV_WGT      0x10u  // Samsung .wgt (WebAssembly)
#define NOV_SAMSUNG  (NOV_TPK | NOV_WGT)
#define NOV_ANDROID  0x4u
#define NOV_OUTRAS   0x8u   // Mac/Linux de desenvolvimento

// Pasta da arte do pacote (a mesma de app_iniciar): fundos da previa, sem rede.
void novcartao_dir(const char *dirArte);
// Chamar em todo quadro, ANTES de novidades202/201/20_primeira_vez. So decide
// com a Home pronta e nada por cima (`homePronta` ja diz tela == Home); com o
// player ou a pagina do titulo abertos espera, sem gastar a decisao.
void novcartao_decidir(int homePronta, int playerAberto, int detalheAberto);
int  novcartao_aberto(void);
void novcartao_abrir(void);
void novcartao_evento(const SDL_Event *e);
void novcartao_atualizar(float dt, Uint32 agora);
void novcartao_desenhar(Uint32 agora);

const char *novcartao_versao(void);    // "2.0.4"
const char *novcartao_arquivo(void);   // a marca de "ja visto" desta versao

// ---- Para a captura e os testes.
int  novcartao_pagina(void);        // 0.. paginas de novidades, depois "Apoie o projeto"
int  novcartao_paginas(void);       // total, contando a de apoio
int  novcartao_foco(void);
int  novcartao_previa_pronta(void); // as artes da previa carregaram
int  novcartao_cenas(void);         // cenas visiveis nesta plataforma
int  novcartao_itens_visiveis(void);
// Pula o relogio da previa para `seg` segundos (cena e momento dela).
void novcartao_teste_relogio(float seg);
// A cena `i` (das visiveis) comeca em que segundo do ciclo.
float novcartao_teste_inicio_cena(int i);
void novcartao_teste_esquecer(void);       // a decisao volta a valer
void novcartao_teste_plataforma(unsigned p); // 0 = a do binario
float novcartao_teste_folga(void);   // rodape - fim da lista, pior pagina desenhada
int  novcartao_teste_cortadas(void); // frases com reticencias, ultimo quadro
int  novcartao_teste_desenhados(void); // itens realmente desenhados, ultimo quadro
float novcartao_teste_altura_item(int i); // altura reservada do i-esimo visivel
float novcartao_teste_vao_min(void); // vaos reais entre itens do mesmo grupo
float novcartao_teste_vao_max(void);
int novcartao_teste_discord(void); // QR desenhado no ultimo quadro
// Liga a medida das frases de varias linhas (custa um bloco invisivel por frase).
void novcartao_teste_medir(int sim);
#endif
