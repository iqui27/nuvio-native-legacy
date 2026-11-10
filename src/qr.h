// Gerador de QR Code, so o necessario para a tela de login.
//
// POR QUE ELE EXISTE — e nao era o plano. O plano dizia "mostra o codigo em
// letras grandes, QR depois". MEDIDO contra o servidor de verdade, a resposta
// de start_tv_login_session e:
//   {"code":"fa0010cad8b5d2f512e58646ab82ca6b",
//    "web_url":"https://nuvio.tv/tv-login?code=fa0010cad8b5d2f512e58646ab82ca6b"}
// Trinta e dois digitos hexadecimais. Ninguem le isso da TV e digita no celular
// sem errar. O QR deixou de ser enfeite e virou o unico jeito de a pessoa
// entrar — por isso ele entrou na Fase A.
//
// ESCOPO DE PROPOSITO ESTREITO: modo BYTE, correcao L, versoes 1 a 6 (ate 41x41
// modulos, 134 bytes de dados). A partir da versao 7 o simbolo passa a exigir
// um bloco de informacao de versao com o proprio BCH, e nada aqui precisa
// disso: a URL de login tem ~55 caracteres e cabe na versao 4. Menos codigo,
// menos lugar para errar.
//
// CONFERIDO contra a implementacao `segno` (Python), modulo a modulo, em varias
// entradas — inclusive a URL real de login. Ver tools/qr_conferir.py.
#ifndef NV_QR_H
#define NV_QR_H

#define QR_MAX_LADO 41   // versao 6: 17 + 4*6
#define QR_MAX_BYTES 134

typedef struct {
  int lado;                                  // 0 quando nao coube
  unsigned char m[QR_MAX_LADO * QR_MAX_LADO];// 1 = modulo escuro
} Qr;

// Codifica `texto`. Devolve 1 em sucesso; 0 quando o texto nao cabe na versao 6
// (mais de 134 bytes) ou e vazio.
int qr_gerar(Qr *q, const char *texto);

static inline int qr_modulo(const Qr *q, int x, int y) {
  if (!q || x < 0 || y < 0 || x >= q->lado || y >= q->lado) return 0;
  return q->m[y * q->lado + x];
}

#endif
