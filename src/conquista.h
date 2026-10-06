// CONQUISTAS: o motor PURO da Caca (plano social-pessoal, secao 8).
//
// Nada aqui fala com rede, disco ou tela. Entra o que foi visto (por item de
// uma definicao: visto e quando) e sai o progresso, o degrau e o que acabou de
// ser desbloqueado. Por ser puro, cada regra do dono tem teste no Mac
// (tests/conquista.c) sem janela nem rede.
//
// REGRAS (secao 8.1 do plano, aprovadas pelo dono):
//   - degraus com nome, cumulativos: Primeiro passo (1 visto) . Metade (50%) .
//     Quase la (falta 1, ou >= 80% quando o alvo tem mais de 10) . Completo
//     (100%) . Em ordem (completa E vista na ordem de lancamento, ou na ordem
//     da historia quando a trilha define);
//   - "Em ordem" precisa de PROVA: a data da primeira vez de cada item. Um item
//     sem data (fonte que nao sabe quando, ex.: historico do Trakt antigo) nao
//     prova nada e o degrau nao sai. Rever nao conta: vale a primeira vez;
//   - nada se perde: um degrau ganho fica ganho mesmo que o alvo cresca (um
//     filme novo do diretor) e o progresso caia;
//   - no maximo UM aviso por sessao, depois dos creditos ou na volta a Home;
//     o resto entra junto ("+2");
//   - visivel para amigos so com alcance >= 1 E "Mostrar aos amigos" ligado;
//     as engracadas (e tudo de perfil infantil) nunca saem sozinhas.
#ifndef NV_CONQUISTA_H
#define NV_CONQUISTA_H

// Degraus, como bits (cumulativos: Completa implica os de baixo).
enum {
  CONQ_PRIMEIRO = 1 << 0,
  CONQ_METADE   = 1 << 1,
  CONQ_QUASE    = 1 << 2,
  CONQ_COMPLETA = 1 << 3,
  CONQ_ORDEM    = 1 << 4,
  CONQ_N_DEGRAUS = 5
};

// Um item de uma definicao (filme de uma filmografia, parte de uma saga...).
// `ordem` e a chave da ordem esperada: a data de lancamento AAAAMMDD, ou a
// posicao na historia quando a trilha define. `quando` e o epoch da PRIMEIRA
// vez vista; 0 = visto sem data conhecida (ou nao visto).
typedef struct {
  long ordem;
  long long quando;
  int  visto;
} ConqItem;

typedef struct {
  int vistos, total;
  int degraus;        // mascara CONQ_* alcancada AGORA (sem memoria)
} ConqProgresso;

// Mascara de degraus para `vistos` de `total`, com `emOrdem` ja calculado.
int  conq_degraus(int vistos, int total, int emOrdem);
// O degrau mais alto de uma mascara (o bit), 0 se vazia.
int  conq_maior(int degraus);
// Indice 0..4 do bit (para rotulo e forma do selo), -1 se nao e um bit so.
int  conq_indice(int degrau);
// "Em ordem": todos vistos, todos com data, e para todo par com ordem[i] <
// ordem[j] vale quando[i] <= quando[j]. Itens com a MESMA ordem podem ser
// vistos em qualquer ordem entre si. 0 sem itens.
int  conq_em_ordem(const ConqItem *v, int n);
// Progresso completo de uma definicao.
ConqProgresso conq_calcular(const ConqItem *v, int n);
// Quantos faltam (nunca negativo).
int  conq_faltam(const ConqProgresso *p);

// DESBLOQUEIO: o que `atual` tem e `guardado` nao tinha. O guardado nunca
// perde bits (nada se perde) — use conq_guardar para o novo valor guardado.
int  conq_novos(int guardado, int atual);
int  conq_guardar(int guardado, int atual);

// ORCAMENTO DO AVISO (por sessao). Um aviso por sessao; o que chegar depois
// acumula e sai junto no MESMO aviso se ele ainda nao foi mostrado ("+2").
typedef struct {
  int mostrados;      // avisos ja mostrados nesta sessao (0 ou 1)
  int pendentes;      // desbloqueios esperando o momento certo
  int primeiro;       // indice (do chamador) do primeiro pendente, -1 sem
} ConqAviso;
void conq_aviso_zerar(ConqAviso *a);
// Um desbloqueio novo. `id` e o indice do chamador (a caca), guardado se for
// o primeiro pendente.
void conq_aviso_somar(ConqAviso *a, int id);
// O momento e bom (sem player, depois dos creditos ou na Home)? Devolve 1 e
// preenche `id` e `extras` (quantos alem do primeiro: o "+N") quando o aviso
// deve sair AGORA; depois disso, nada mais sai nesta sessao.
int  conq_aviso_tirar(ConqAviso *a, int momentoBom, int *id, int *extras);

// PRIVACIDADE: a conquista sai para amigos? `alcance` e o do recomenda.h.
int  conq_visivel_amigos(int alcance, int mostrar, int engracada, int infantil);

// Rotulo em portugues (a chave do i18n) do degrau (bit).
const char *conq_rotulo(int degrau);

#endif
