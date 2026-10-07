// PEGOU NO SONO: a regra e o acompanhamento (dormiu.h). A pergunta na ilha e a
// retomada moram em dormiuilha.c, para este arquivo ser testado sem SDL.
//
// ESTADO: um vao (segundos TOCANDO sem tecla) e a foto da ultima tecla
// (episodio, posicao, duracao). A tecla vem de app_evento, de qualquer tela —
// e por isso o caso 2 funciona tambem depois que o player fechou sozinho.
// Arquivos, por perfil:
//   dormiu-vivo-p<N>.txt  so existe enquanto um vao longo esta aberto (gravado a
//                          cada 5 min depois de 30 min sem tecla). Achado no
//                          arranque = a sessao morreu tocando (caso 1).
//   dormiu-p<N>.txt       o limiar aprendido e o candidato do caso 3.
#include "dormiu.h"
#include "dados.h"
#include "perfis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define VIVO_APOS_S   (30 * 60)
#define VIVO_CADA_S   (5 * 60)
#define RETOMAR_JANELA_S 300.0

long long dormiu_relogio_teste;
static long long agoraParede(void) {
  return dormiu_relogio_teste ? dormiu_relogio_teste : (long long)time(NULL);
}

// Uma "foto" do vao: de onde (ultima tecla) e quanto.
typedef struct {
  int ativo;
  char id[DIARIO_ID_MAX]; char tipo;
  int t, e;                // episodio da ultima tecla
  double kPos, kDur;       // posicao e duracao na ultima tecla
  double vao;              // segundos tocando sem tecla
  long long quando;        // parede: fim da sessao / creditos
  int concluiu;            // o episodio da tecla chegou ao fim?
} Foto;

static struct {
  int ativo;
  char id[DIARIO_ID_MAX]; char tipo;
  int t, e;
  double pos, dur;
  double vao;
  int kt, ke; double kPos, kDur;
  int cred, teclaPosCred; long long credQuando; Foto credFoto;
  double vivoEm; int vivoGravado;
  double inicioPos, tocado;
  double ultimoVao; int ukt, uke; double ukPos, ukDur;
  int concluiuAgora;
} S;

static Foto aguardaCred;    // caso 2 com o player ja fechado
static Foto ultima;         // candidato do caso 3
static int limiarExtra = -1, perfilLido = -1;

static const char *arq(char *b, size_t t, const char *base) {
  int p = perfis_ativo();
  snprintf(b, t, "%s-p%d.txt", base, p > 0 ? p : 1);
  return b;
}

static void fotoLinha(const Foto *f, char *b, size_t t) {
  snprintf(b, t, "%s\t%c\t%d\t%d\t%.0f\t%.0f\t%.0f\t%lld\t%d\n", f->id, f->tipo ? f->tipo : 'm',
           f->t, f->e, f->kPos, f->kDur, f->vao, f->quando, f->concluiu);
}

static int fotoLer(const char *s, Foto *f) {
  memset(f, 0, sizeof *f);
  if (!s || sscanf(s, "%31[^\t]\t%c\t%d\t%d\t%lf\t%lf\t%lf\t%lld\t%d", f->id, &f->tipo, &f->t, &f->e,
                   &f->kPos, &f->kDur, &f->vao, &f->quando, &f->concluiu) != 9) return 0;
  f->ativo = 1;
  return 1;
}

static void gravarEstado(void) {
  char b[256], n[40];
  size_t k = (size_t)snprintf(b, sizeof b, "limiar=%d\n", limiarExtra < 0 ? 0 : limiarExtra);
  if (ultima.ativo) { k += (size_t)snprintf(b + k, sizeof b - k, "ultima="); fotoLinha(&ultima, b + k, sizeof b - k); }
  dados_gravar(arq(n, sizeof n, "dormiu"), b);
}

static void lerEstado(void) {
  char n[40], *b, *s;
  if (perfilLido == perfis_ativo() && limiarExtra >= 0) return;
  perfilLido = perfis_ativo();
  limiarExtra = 0;
  memset(&ultima, 0, sizeof ultima);
  b = dados_ler(arq(n, sizeof n, "dormiu"));
  if (!b) return;
  for (s = b; s && *s;) {
    if (!strncmp(s, "limiar=", 7)) limiarExtra = atoi(s + 7);
    else if (!strncmp(s, "ultima=", 7)) fotoLer(s + 7, &ultima);
    s = strchr(s, '\n'); if (s) s++;
  }
  if (limiarExtra < 0) limiarExtra = 0;
  if (limiarExtra > DORMIU_LIMIAR_MAX_MIN) limiarExtra = DORMIU_LIMIAR_MAX_MIN;
  free(b);
}

// ------------------------------------------------------------------ puro

int dormiu_regra(const DormiuSinal *s, int *ponto) {
  double limiar;
  int sinal;
  if (ponto) *ponto = 0;
  if (!s || s->durSeg <= 0.0) return 0;
  limiar = DORMIU_SEM_TECLA_S + 60.0 * (s->limiarExtraMin > 0 ? s->limiarExtraMin : 0);
  if (s->semTeclaSeg < limiar || s->semTeclaSeg < DORMIU_FRACAO * s->durSeg) return 0;
  sinal = s->fimSemDespedida || s->credTardeSeg >= DORMIU_TARDE_S || s->voltouSeg >= DORMIU_TARDE_S;
  if (!sinal) return 0;
  if (ponto) {
    double p = s->posTeclaSeg + DORMIU_MARGEM_S;
    if (p > s->durSeg) p = s->durSeg;
    if (p < 0) p = 0;
    *ponto = (int)(p + 0.5);
  }
  return 1;
}

void dormiu_rotulo(int seg, char *d, unsigned tam) {
  if (seg < 0) seg = 0;
  // Sempre horario (h:mm): a posicao no filme, nao uma duracao ("25 min").
  snprintf(d, tam, "%d:%02d", seg / 3600, (seg % 3600) / 60);
}

// --------------------------------------------------------------- registro

static void registrar(const Foto *f, int fimSemDespedida, double credTarde, double voltou) {
  DormiuSinal s;
  DiarioEntrada d;
  int ponto;
  long long q = 0;
  memset(&s, 0, sizeof s);
  s.durSeg = f->kDur; s.semTeclaSeg = f->vao; s.posTeclaSeg = f->kPos;
  s.fimSemDespedida = fimSemDespedida; s.credTardeSeg = credTarde; s.voltouSeg = voltou;
  lerEstado();
  s.limiarExtraMin = limiarExtra;
  if (!dormiu_regra(&s, &ponto)) return;
  memset(&d, 0, sizeof d);
  snprintf(d.id, sizeof d.id, "%s", f->id);
  d.tipo = f->tipo ? f->tipo : DIARIO_FILME;
  d.temporada = (short)f->t; d.episodio = (short)f->e;
  d.origem = DIARIO_ORIG_PLAYER;
  d.hora = -1;
  d.dormiuSeg = ponto;
  d.durSeg = (int)(f->kDur + 0.5);
  d.flags = DIARIO_F_DORMIU_SUS;
  // Chegou ao fim: a marca vai NA vez que o player ja gravou (mesmo `quando`,
  // a fusao junta). Nao chegou: uma vez PARCIAL, so para guardar o ponto.
  if (f->concluiu && diario_episodio(0, f->id, f->t, f->e, &q) > 0 && q) d.quando = q;
  else { d.quando = f->quando ? f->quando : agoraParede(); if (!f->concluiu) d.flags |= DIARIO_F_PARCIAL; }
  diario_adicionar(0, &d);
  printf("[dormiu] suspeita T%dE%d ponto %ds (vao %.0f min)\n", f->t, f->e, ponto, f->vao / 60.0);
  fflush(stdout);
}

static void vivoApagar(void) {
  char n[48];
  if (!S.vivoGravado && !aguardaCred.ativo) return;
  dados_apagar(arq(n, sizeof n, "dormiu-vivo"));
  S.vivoGravado = 0;
}

static void vivoGravar(const Foto *f) {
  char n[48], b[256];
  fotoLinha(f, b, sizeof b);
  dados_gravar(arq(n, sizeof n, "dormiu-vivo"), b);
}

static Foto fotoAgora(int concluiuEp) {
  Foto f;
  memset(&f, 0, sizeof f);
  f.ativo = 1;
  snprintf(f.id, sizeof f.id, "%s", S.id);
  f.tipo = S.tipo; f.t = S.kt; f.e = S.ke; f.kPos = S.kPos; f.kDur = S.kDur > 0 ? S.kDur : S.dur;
  f.vao = S.vao; f.quando = agoraParede();
  // Se o player ja passou para outro episodio sem tecla, o da tecla acabou.
  f.concluiu = (S.kt != S.t || S.ke != S.e) ? 1 : concluiuEp;
  return f;
}

// ------------------------------------------------------------------ ganchos

void dormiu_iniciar(void) {
  char n[48], *b;
  Foto f;
  lerEstado();
  b = dados_ler(arq(n, sizeof n, "dormiu-vivo"));
  if (!b) return;
  if (fotoLer(b, &f)) registrar(&f, 1, -1, -1);   // caso 1: morreu tocando
  free(b);
  dados_apagar(n);
}

int dormiu_limiar_extra(void) { lerEstado(); return limiarExtra; }

void dormiu_aprender_nao_dormi(void) {
  lerEstado();
  limiarExtra += 10;
  if (limiarExtra > DORMIU_LIMIAR_MAX_MIN) limiarExtra = DORMIU_LIMIAR_MAX_MIN;
  gravarEstado();
}

void dormiu_player_passo(const char *id, char tipo, int t, int e, double pos,
                         double dur, int tocando, int concluiu, float dt) {
  char base[DIARIO_ID_MAX];
  if (!id || !id[0]) return;
  diario_separar_id(id, base, sizeof base, NULL, NULL);
  if (!S.ativo) {
    memset(&S, 0, sizeof S);
    S.ativo = 1;
    snprintf(S.id, sizeof S.id, "%s", base);
    S.kt = t; S.ke = e; S.kPos = pos; S.kDur = dur; S.inicioPos = pos;
    lerEstado();
  } else if (strcmp(S.id, base)) {
    // Outro titulo na mesma sessao (trocou pela lista): comeca do zero.
    S.ativo = 0;
    dormiu_player_passo(id, tipo, t, e, pos, dur, tocando, concluiu, dt);
    return;
  }
  if (S.t != t || S.e != e) { S.cred = 0; S.teclaPosCred = 0; }   // episodio novo
  S.tipo = tipo ? tipo : DIARIO_FILME;
  S.t = t; S.e = e; S.pos = pos; S.dur = dur;
  S.concluiuAgora = concluiu;
  if (S.kt == t && S.ke == e) S.kDur = dur;
  if (tocando && dt > 0 && dt < 5) { S.vao += dt; S.tocado += dt; }

  // Caso 3: retomou o candidato e voltou >= 10 min nos primeiros 5 min.
  if (ultima.ativo && !strcmp(ultima.id, S.id) && ultima.t == t && ultima.e == e) {
    double voltou = S.inicioPos - pos;
    if (voltou >= DORMIU_TARDE_S) {
      registrar(&ultima, 0, -1, voltou);
      ultima.ativo = 0; gravarEstado();
    } else if (S.tocado > RETOMAR_JANELA_S) { ultima.ativo = 0; gravarEstado(); }
  }

  // Creditos: guarda a foto do vao; o caso 2 decide na proxima tecla.
  if (concluiu && !S.cred) {
    S.cred = 1; S.teclaPosCred = 0;
    S.credQuando = agoraParede();
    S.credFoto = fotoAgora(1);
  }

  // Vao longo: o arquivo "vivo" e o que prova o caso 1 se a TV desligar.
  if (S.vao >= VIVO_APOS_S && S.vao - S.vivoEm >= VIVO_CADA_S) {
    Foto f = fotoAgora(concluiu);
    S.vivoEm = S.vao;
    vivoGravar(&f);
    S.vivoGravado = 1;
  }
}

void dormiu_tecla(void) {
  long long agora = agoraParede();
  if (aguardaCred.ativo) {
    registrar(&aguardaCred, 0, (double)(agora - aguardaCred.quando), -1);
    memset(&aguardaCred, 0, sizeof aguardaCred);
    { char n[48]; dados_apagar(arq(n, sizeof n, "dormiu-vivo")); }
  }
  if (!S.ativo) return;
  if (S.cred && !S.teclaPosCred) {
    S.teclaPosCred = 1;
    registrar(&S.credFoto, 0, (double)(agora - S.credQuando), -1);
  }
  // Fecha o vao. Guardado para o caso 3 (se a saida vier logo depois).
  S.ultimoVao = S.vao; S.ukt = S.kt; S.uke = S.ke; S.ukPos = S.kPos; S.ukDur = S.kDur;
  S.vao = 0; S.vivoEm = 0;
  S.kt = S.t; S.ke = S.e; S.kPos = S.pos; S.kDur = S.dur;
  vivoApagar();
}

void dormiu_player_saiu(double pos, double dur, int concluiu) {
  if (!S.ativo) return;
  S.pos = pos; if (dur > 0) S.dur = dur;
  if (S.vao >= 60.0) {
    // Fechou SEM tecla (fim do arquivo, TV desligando o app...).
    if (S.cred && !S.teclaPosCred) {
      aguardaCred = S.credFoto;
      aguardaCred.quando = S.credQuando;
      vivoGravar(&aguardaCred);           // se o app morrer antes da tecla: caso 1
    } else {
      Foto f = fotoAgora(concluiu);
      registrar(&f, 1, -1, -1);
      vivoApagar();
    }
  } else {
    // Saiu por tecla. Se a tecla fechou um vao longo, pode ser o caso 3 depois.
    lerEstado();
    if (S.ultimoVao >= DORMIU_SEM_TECLA_S) {
      memset(&ultima, 0, sizeof ultima);
      ultima.ativo = 1;
      snprintf(ultima.id, sizeof ultima.id, "%s", S.id);
      ultima.tipo = S.tipo; ultima.t = S.ukt; ultima.e = S.uke;
      ultima.kPos = S.ukPos; ultima.kDur = S.ukDur; ultima.vao = S.ultimoVao;
      ultima.quando = agoraParede();
      ultima.concluiu = (S.ukt != S.t || S.uke != S.e) ? 1 : concluiu;
      gravarEstado();
    }
    vivoApagar();
  }
  S.ativo = 0;
}

// ------------------------------------------------------------------ respostas

typedef struct { DiarioEntrada e; int achou; } Achado;
static int primeira(const DiarioEntrada *e, void *c) {
  Achado *a = c;
  a->e = *e; a->achou = 1;
  return 0;
}

int dormiu_suspeita(const char *id, DiarioEntrada *saida) {
  DiarioFiltro f;
  Achado a;
  memset(&f, 0, sizeof f);
  memset(&a, 0, sizeof a);
  f.id = id; f.comFlags = DIARIO_F_DORMIU_SUS; f.decrescente = 1;
  diario_percorrer(0, &f, primeira, &a);
  if (a.achou && saida) *saida = a.e;
  return a.achou;
}

void dormiu_confirmar(const DiarioEntrada *e) {
  if (!e) return;
  diario_definir_flags(0, e->id, e->temporada, e->episodio, e->quando,
                       DIARIO_F_DORMIU, DIARIO_F_DORMIU_SUS);
}

void dormiu_negar(const DiarioEntrada *e) {
  if (!e) return;
  if (e->flags & DIARIO_F_PARCIAL) diario_apagar(0, e->id, e->temporada, e->episodio, e->quando);
  else diario_definir_flags(0, e->id, e->temporada, e->episodio, e->quando, 0, DIARIO_F_DORMIU_SUS);
  dormiu_aprender_nao_dormi();
}

// So para o teste: esquece o estado lido (como numa troca de perfil).
void dormiu_teste_reler(void) { perfilLido = -1; limiarExtra = -1; }
