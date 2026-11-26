// Nuvio Casa, a parte de rede (ver casa.h para o que e e o que nao e).
//
// CLIENTE (TV sem motor): um fio pergunta por UDP a cada CASA_SONDA_S e guarda
// quem respondeu; casa_ajudante_base() entrega o endereco sem bloquear.
// AJUDANTE (aparelho com motor): um fio responde a descoberta e aceita HTTP;
// cada conexao tem o seu fio (ate CASA_CONEXOES). O video e repassado do
// servidor do motor em 127.0.0.1, byte a byte, com o Range do pedido.
//
// QUEM ENTRA no ajudante: origem em rede privada, Host IPv4 literal com a
// porta certa, sem cabecalho Origin (pagina de navegador nao entra) e o token
// da descoberta no caminho. O token e sorteado a cada subida; qualquer aparelho
// da rede o recebe, entao ele NAO separa pessoas da mesma rede: separa a rede
// de fora dela. Nunca vai para o log, nem hash nem endereco inteiro.
//
// PLATAFORMAS: sockets POSIX. No .wgt (Emscripten) tudo aqui e vazio.
// MEDIDO: so Linux, com motor falso (tests/casa_ponta.c). NAO MEDIDO: TV
// nenhuma, Android (o Wi-Fi de alguns aparelhos filtra broadcast sem
// MulticastLock), webOS/Tizen (firewall para porta de entrada nao se aplica ao
// cliente, mas o broadcast de saida nao foi provado).
#ifndef NV_CASA_REDE_H
#define NV_CASA_REDE_H
#include <stdint.h>

#define CASA_CONEXOES 8

// O motor do ajudante. Tudo pode ser chamado de qualquer fio.
typedef struct {
  // BLOQUEIA. 0 = pronto, com a URL local (http://127.0.0.1:...) e o tamanho;
  // senao um P2pErro (p2p.h).
  int  (*resolver)(const char *hash, int idx, const char *fontes, int temporada,
                   int episodio, char *url, unsigned n, uint64_t *tam);
  void (*segurar)(int s);      // 1 = manter o motor de pe sem player aberto
  int  (*ocupado)(void);       // 1 = o motor esta em uso (por este aparelho)
} CasaMotor;

// A cada quadro, fio da tela; so liga/desliga fios quando o estado muda.
// `motor` NULL = este aparelho nao pode ajudar.
void casa_configurar(int usar, int ajudar, const CasaMotor *motor);
// 1 e a base (casa_base) do ajudante escolhido; 0 sem nenhum. Nao bloqueia.
int  casa_ajudante_base(char *out, unsigned n);
// BLOQUEIA ate `ms`: uma pergunta agora. Quantos ajudantes validos ha.
int  casa_procurar(int ms);
// 1 com o ajudante no ar; a porta HTTP dele (0 fora do ar).
int  casa_ajudando(void);
int  casa_porta(void);
// Para tudo e espera os fios (saida do app, testes).
void casa_parar(void);

// Testes: porta UDP (0 mantem) e um destino a mais para a pergunta ("127.0.0.1").
void casa_teste(int portaUdp, const char *sonda);
#endif
