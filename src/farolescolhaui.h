// FAROLESCOLHAUI — o modo perguntas do Farol (P5 sem LLM, issue 50; PLANO-GOSTO
// secoes 4.2 P5, 4.3 e 4.5). A casca do motor puro src/farolescolha.h.
//
// ENTRADA. Farol › "Achar o que ver" (o 5o item da coluna ACHAR_X em
// resolver.c) ou farolescolhaui_abrir() direto (a dica P2 `acao: escolha`).
// Os candidatos saem do catalogo vivo (farolacha_listar_rev FA_SURPRESA, ate
// FESC_CAND_MAX, semente fixa: mesma lista = mesmas perguntas). So entra obra
// com identidade (IMDb ou TMDB): o resultado e revalidado pela identidade no
// catalogo vivo (farolachaui_resolver_pedido), nunca por indice nu.
//
// A TELA. Kicker "Farol › Achar o que ver", ponto de luz de 10 px no acento +
// "1 de 3" (FT_ESC_PASSO), a pergunta e 2 a 4 pilulas grandes numa linha; o
// botao Voltar embaixo.
//
// D-PAD (4.5). ←/→ entre as opcoes; ↓ vai ao Voltar e ↑ volta a opcao; OK
// responde (OK com key.repeat != 0 e ignorado: segurar o OK nunca responde
// duas perguntas); Voltar desfaz a ultima resposta com o foco na opcao que a
// pessoa tinha escolhido; Voltar na 1a pergunta pede a saida (pediuSair) e o
// resolver restaura o foco no item de entrada.
//
// O FIM.
//   - 2 ou 3 titulos: farolcompara_abrir_titulos (P4) com o tempo respondido;
//     abriu = pede para fechar o Farol (farolescolhaui_pediu_fechar). Se o P4
//     recusar (player aberto, obra sumiu), cai nos cartazes daqui — sem beco.
//   - 1 titulo: o cartaz com OK = abrir (farolescolhaui_pediu_abrir, mesmo
//     contrato de FarolAchaPedido da Tela 7) e Voltar.
//   - 0: "Nada exato…" e Voltar (desfaz a ultima resposta).
//
// Sem log de titulo, perfil ou gosto; sem estado por fio; fio principal so.
#ifndef NV_FAROLESCOLHAUI_H
#define NV_FAROLESCOLHAUI_H

#include <SDL2/SDL.h>
#include "farolachaui.h"
#include "farolescolha.h"

#define FESCUI_FECHADO   0
#define FESCUI_PERGUNTA  1
#define FESCUI_RESULTADO 2   // 0, 1 ou (P4 recusado) 2..3 cartazes + Voltar
#define FESCUI_COMPARAR  3   // entregue ao P4 (o Farol fecha)
#define FESCUI_OUVINDO   4   // ditado ativo (#54, st_voz_iniciar + parciais)
#define FESCUI_PREVIA    5   // previa obrigatoria ANTES de enviar (0 POSTs)
#define FESCUI_ESPERA    6   // aguardando POST /v1/farol/escolha (cancelavel)
#define FESCUI_DIGITANDO 7   // teclado/IME da TV ou voz pelo teclado (.tpk/.wgt) (#55)
#define FESCUI_QR        8   // digitar pelo celular via QR + servidor local (#55)
#define FESCUI_AVISO     9   // aviso honesto sem beco (erro/expiracao QR ou falha IME) (#55)

#define FESCUI_FOCO_VOLTAR    (-1)
#define FESCUI_FOCO_VOZ       (-2)
#define FESCUI_FOCO_CONFIRMAR (-3)
#define FESCUI_FOCO_ALT       (-4)

void farolescolhaui_reset(void);
void farolescolhaui_fechar(void);
// Monta os candidatos do catalogo vivo e comeca (pode ja terminar: < 3
// candidatos ou nada que divida). Depois de abrir, confira
// farolescolhaui_pediu_fechar (o P4 pode ter aberto direto).
void farolescolhaui_abrir(void);
// Gancho do #54: pre-responde por ids ("tempo", "t45"). So antes da 1a
// resposta da pessoa. 1 = aceita.
int  farolescolhaui_pre_responder(const char *perguntaId, const char *opcaoId);
// Gancho limpo do #54/#55 (voz Android, teclado Samsung, LG QR, WASM):
// normaliza `frase` e abre a previa (FESCUI_PREVIA) com 0 POSTs.
// 1 = entrou na previa; 0 = frase vazia/invalida ou fora da 1a pergunta.
int  farolescolhaui_aplicar_frase(const char *frase);

// Processa um evento. `pediuSair`: Voltar sem nada a desfazer (sai para o
// Inicio do Farol). `pediuAbrir`: OK num cartaz (identidade em
// farolescolhaui_pediu_abrir). Devolve 1 se consumiu.
int  farolescolhaui_evento(const SDL_Event *e, int *pediuSair, int *pediuAbrir);
void farolescolhaui_atualizar(float dt, Uint32 agora);
void farolescolhaui_desenhar(float ox, float oy, float alpha, Uint32 agora);

// 1 UMA vez com a identidade do cartaz escolhido.
int  farolescolhaui_pediu_abrir(FarolAchaPedido *pedido);
// 1 UMA vez quando o P4 abriu: quem chama fecha o Farol.
int  farolescolhaui_pediu_fechar(void);

// ------------------------------------------------------------ inspecao
int  farolescolhaui_modo(void);          // FESCUI_*
int  farolescolhaui_foco(void);          // 0..n-1 ou FESCUI_FOCO_VOLTAR
int  farolescolhaui_n_candidatos(void);
const FarolEsc *farolescolhaui_motor(void);
int  farolescolhaui_n_resultados(void);  // cartazes no modo RESULTADO
int  farolescolhaui_resultado_indice(int i);   // indice vivo no catalogo; -1 fora
int         farolescolhaui_saida(void);         // FESC_SAIDA_* do ultimo fim
int         farolescolhaui_voz_visivel(void);   // 1 se botao de frase visivel (voz/IME/QR)
const char *farolescolhaui_rotulo_botao(void);  // FT_ACAO_* do modo primario
const char *farolescolhaui_icone_botao(void);   // icone do modo primario
const char *farolescolhaui_frase(void);         // frase na previa/espera (ou "")
const char *farolescolhaui_parcial(void);       // parcial enquanto ouve/digita (ou "")
const char *farolescolhaui_aviso(void);         // ultimo aviso de entrada (ou "")

// ------------------------------------------------------------ ganchos de teste
void farolescolhaui_teste_hora(int hora);        // -2 = relogio real
void farolescolhaui_teste_infantil(int infantil); // -1 = perfil real
void farolescolhaui_teste_modos(int voz, int ime, int cel); // -1 = deteccao real
unsigned int farolescolhaui_teste_qr_tex(void); // GLuint atual da textura QR (0 se destruida)

#endif
