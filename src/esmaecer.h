// PROTECAO DE OLED: esmaecer a interface parada, e o brilho da interface do player.
//
// DOIS ASSUNTOS, UM ARQUIVO, porque os dois existem pelo mesmo motivo: pixel
// aceso e parado queima o OLED (a C9 do dono), e a interface do Nuvio e feita
// de coisas paradas (barra, logo, relogio, cartazes).
//
// 1. ESMAECER QUANDO PARADO. Fora da reproducao (Home, titulo, Ajustes, menus,
//    E o player com o video PAUSADO), depois de N minutos sem tecla a tela vai
//    a um veu preto de 60% (estagio 1) e, mais NO_FIM_MS depois, a 92% com um
//    relogio pequeno que anda devagar (estagio 2): nada parado fica aceso.
//    Qualquer tecla acorda NA HORA e e CONSUMIDA (nao age). Com video tocando de
//    verdade nunca esmaece. E uma passada so de tela cheia no fim do quadro.
//
// 2. BRILHO DA INTERFACE NO PLAYER. Multiplica a COR (nao o alfa: forma e
//    legibilidade ficam) do que o player desenha por cima do video; mais um
//    degrau automatico quando a barra fica parada com o filme tocando.
//
// A maquina de estados e as contas sao puras (sem SDL nem GL) para o teste.
#ifndef ESMAECER_H
#define ESMAECER_H

// Escolhas do ajuste (indices de V_ESMAECER): 0 desligado, 1 = 30 s, 2 = 1 min,
// 3 = 2 min (padrao), 4 = 5 min, 5 = 10 min.
#define ESM_ESCOLHAS 6
#define ESM_PADRAO   3
// TELA DE DESCANSO (2.0): o que aparece depois que a tela escurece. Indices de
// V_DESCANSO_ESTILO. VITRINE e RELOGIO escurecem de vez em poucos segundos e
// desenham por cima (descanso.c); ESCURECER e o comportamento de antes (veu de
// 60% e, 3 min depois, 92% com o relogio pequeno andando).
#define ESM_ESTILO_VITRINE   0
#define ESM_ESTILO_RELOGIO   1
#define ESM_ESTILO_ESCURECER 2
#define ESM_ESTILOS          3
// Com VITRINE/RELOGIO: quanto o veu de 60% fica antes de ir ao preto.
#define ESM_DESCANSO_MS 5000u
#define ESM_ALFA_DESCANSO 1.0f
// Estagios.
#define ESM_ACESO   0
#define ESM_VEU     1   // ~60% preto
#define ESM_ESCURO  2   // ~92% preto + relogio andando
#define ESM_ALFA_VEU    0.60f
#define ESM_ALFA_ESCURO 0.92f
// Tempo entre o estagio 1 e o 2.
#define ESM_NO_FIM_MS   (3u * 60u * 1000u)
// Velocidade do esmaecer (alfa por segundo): 0 -> 60% em 1,5 s.
#define ESM_VELOCIDADE  0.40f

// Milissegundos ate o estagio 1 para a escolha (0 = nunca).
unsigned esmaecer_ms(int escolha);
// PURAS: estagio para `ocioMs` sem tecla, e o alfa do veu nesse estagio.
int  esmaecer_estagio_para(unsigned ocioMs, int escolha, int estilo);
float esmaecer_alfa_do_estagio(int estagio, int estilo);

// Estado. `agora` em ms (SDL_GetTicks).
void  esmaecer_reiniciar(void);
void  esmaecer_escolha(int escolha);
void  esmaecer_estilo(int estilo);
int   esmaecer_estilo_atual(void);
// 1 = a tela de descanso (vitrine ou relogio) esta no ar: estagio 2 com um
// desses estilos e o veu ja no preto. descanso.c desenha por cima.
int   esmaecer_descanso(void);
// 1 = a tela de descanso do Nuvio esta ligada (tempo e estilo vitrine/relogio):
// o screensaver da TV deve ser segurado (video.c, webOS).
int   esmaecer_segura_protetor_tv(void);
// webOS registra o publicador do ACK; setters avisam mesmo sem desenho.
void  esmaecer_protetor_publicador(void (*publicar)(void));
// Chamar para CADA evento de pessoa (tecla, ponteiro). `consumivel` = o evento
// age se passar (KEYDOWN, clique). Devolve 1 se o evento deve ser engolido (a
// primeira tecla que acorda a tela, e a repeticao dela logo depois).
int   esmaecer_entrada(unsigned agora, int consumivel);
// Uma vez por quadro. `reproduzindo` = video tocando de verdade agora.
void  esmaecer_quadro(unsigned agora, float dt, int reproduzindo);
int   esmaecer_estagio(void);     // estagio ALVO atual
float esmaecer_veu(void);         // alfa corrente do veu (0..0,92)
int   esmaecer_apagado(void);     // 1 = no estagio 2 e o veu ja chegou la
// 1 uma vez quando o estagio 2 comeca e 1 uma vez ao sair dele, para a
// plataforma soltar/segurar o "manter tela ligada". Consome o aviso.
int   esmaecer_mudou_escuro(int *escuro);

// BRILHO DO PLAYER. `escolha` indexa V_BRILHO_PLAYER (100/80/65/50%).
float esmaecer_brilho_base(int escolha);
// Fator final: base e, com o filme tocando e a interface parada ha mais de
// BRILHO_AUTO_MS, um degrau a mais, com transicao suave. Nunca abaixo de 0,40.
#define BRILHO_AUTO_MS     2000u
#define BRILHO_AUTO_RAMPA  1500u
#define BRILHO_AUTO_PASSO  0.85f
float esmaecer_brilho_osd(int escolha, unsigned parado_ms, int tocando);

// 3. UI EM HDR (#205). Com o video em HDR/DV a TV compoe a camada de UI (SDR)
//    sobre o video HDR: o branco da UI vai ao branco de pico e o preto
//    translucido dos veus fica cinza leitoso — o "esbranquicado" que o dono viu
//    na TCL. Enquanto o video esta em HDR, app.c poe em gfx_hdr_ui o fator
//    abaixo: a cor de TUDO do player (texto e legenda inclusive) cai para 80%
//    e as superficies escuras ficam mais densas. Sem passada extra: e a cor de
//    cada retangulo (gfx_rect) e um multiplicador no shader de texto.
//    0,80: o branco SDR de referencia (~203 nits) num painel que mapeia o
//    grafico perto do pico; abaixo disso o texto pequeno perde contraste.
#define HDR_UI_BRILHO 0.80f
// Densidade extra das superficies escuras: a' = a + D*a*(1-a). Material da
// ilha .80 -> .88, disco do OSD .52 -> .64; opaco e furo (0) nao mudam.
#define HDR_UI_DENSO  0.50f
// HDR_UI_BRILHO quando `hdr` (video_hdr()) diz HDR10/HLG/DolbyVision ou `dv`;
// 1 em SDR e quando a plataforma nao sabe ("none", "desconhecido", vazio).
float esmaecer_brilho_hdr(const char *hdr, int dv);
// A cor de um retangulo com o fator `k` (1 = nada muda). Inline: gfx.c usa sem
// depender de esmaecer.c.
static inline void esmaecer_cor_hdr(float k, float *r, float *g, float *b, float *a) {
  float m = *r > *g ? *r : *g;
  if (k >= 0.999f) return;
  if (*b > m) m = *b;
  if (m < 0.25f) *a += HDR_UI_DENSO * *a * (1.0f - *a);
  *r *= k; *g *= k; *b *= k;
}

#ifndef ESMAECER_SEM_GFX
// O veu (e o relogio andando no estagio 2), no FIM do quadro.
void esmaecer_desenhar(unsigned agora);
#endif

#endif
