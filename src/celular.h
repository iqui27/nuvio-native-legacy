// DIGITAR PELO CELULAR: um servidor HTTP minimo, na LAN, enquanto o cartao do
// botao do celular (celbotao.c) esta aberto — em qualquer campo de texto.
//
// POR QUE EXISTE (pedido do dono, 02/10/2026): a chave do Seekr tem 64
// caracteres, e digitar 64 simbolos de a-zA-Z0-9_- no D-pad e a pior parte de
// configurar o app. A modal mostra um QR com http://<ip-da-TV>:<porta>/<token>;
// o celular abre uma pagina com um campo grande, a pessoa cola e envia, e o
// texto entra no campo da modal pelo mesmo caminho do teclado do sistema (valor
// inteiro, filtrado pelo alfabeto do campo).
//
// SEGURANCA (o texto e chave de API na maioria dos campos):
//   - token aleatorio de /dev/urandom, de uso unico: o primeiro POST valido
//     consome o token e o servidor fecha;
//   - expira em CEL_VALIDADE_S ou quando o cartao fecha (celular_fechar);
//   - so a pagina (GET) e o envio (POST) no caminho exato do token; o resto e
//     404, e CEL_ERROS_MAX caminhos errados derrubam o servidor (adivinhar o
//     token custaria 2^39 tentativas, nao 30);
//   - corpo de no maximo CEL_CORPO_MAX bytes, Content-Length obrigatorio;
//   - Host tem de ser o endereco que o QR mostra (ou 127.0.0.1), e um Origin,
//     se vier, tem de ser o mesmo: sem CORS, e sem DNS rebinding;
//   - nada sai da TV: o servidor so responde, nunca chama ninguem;
//   - o texto recebido NUNCA vai para o log — so "recebido N bytes".
//
// PLATAFORMAS: sockets POSIX (LG webOS, Samsung .tpk, Android, Mac). No .wgt
// (Emscripten) o navegador nao escuta socket: celular_disponivel() = 0 e a
// modal fica como era (sem painel).
//   MEDIDO (02/10/2026): Mac (curl + Chrome) e Android na TCL (APK de release,
//   curl do Mac para a TV: pagina 200, envio entregue ao campo, servidor fecha
//   com o Voltar).
//   NAO MEDIDO: LG (o jail ou o firewall do webOS podem barrar conexao de fora
//   em porta alta; o proxy de TS so escuta em 127.0.0.1) e Samsung .tpk (tem
//   privilegio internet; escuta nao testada). O log diz: "[celular] servidor no
//   ar" = bind/listen deram certo; "[celular] pagina aberta" = o celular chegou.
//   Se a TV nao deixar entrar, o painel aparece mas o celular nao abre a pagina.
#ifndef NV_CELULAR_H
#define NV_CELULAR_H
#include <stddef.h>

#ifndef CEL_VALIDADE_S
#define CEL_VALIDADE_S 300
#endif
#define CEL_CORPO_MAX  4096
#define CEL_ERROS_MAX  30

enum { CEL_PARADO = 0, CEL_ESPERANDO, CEL_RECEBIDO, CEL_EXPIROU, CEL_FALHOU };

int  celular_disponivel(void);
// Sobe o servidor com um token novo. `titulo` (ja traduzido ou nao: passa por
// i18n aqui) vai para o cabecalho da pagina. 1 = no ar; 0 = sem rede/sem IP.
// Thread do app (usa i18n).
int  celular_abrir(const char *titulo);
void celular_fechar(void);
int  celular_estado(void);
// http://<ip>:<porta>/<token>, ou "" fora do ar.
const char *celular_url(void);
// 1 UMA VEZ, quando o texto chegou (ja sem espacos nas pontas). `dst` recebe
// ate n-1 bytes.
int  celular_pegar(char *dst, size_t n);

// --- testes ---
int  celular_porta(void);
#endif
