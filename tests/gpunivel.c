// A REGRA DO NIVEL DE GPU ADAPTATIVO (src/gpunivel.h), sem TV e sem GL.
//
// O quadro chega como main.c o entrega (dt, espera = clr+swap, cpu) e a
// regra tem de: ficar no 0 quando o FPS e bom (a Tizen 6 a 60 nunca desce);
// conservar 0 -> 1 -> 2 so com ganho medido; voltar e bloquear sem ganho;
// migrar niveis legados uma vez; nao descer por CPU; ignorar fora da home. gfx e dados entram como dubles: o que se testa e a regra.
#include "gpunivel.h"
#include "gfx.h"
#include "dados.h"
#include "perfiltv.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void gpun_teste_reiniciar(void);
int  gpun_teste_decidido(void);

// --- dubles ------------------------------------------------------------------
float gfx_tex_aspect_atual, gfx_opacidade_grupo = 1.0f;
static int leves = -1;
void gfx_definir_efeitos_leves(int l) { leves = l; }
static int minimos = -1;
void gfx_definir_efeitos_minimos(int m) { minimos = m; }
int  gfx_efeitos_leves(void) { return leves; }
void gfx_tex_esquecer(GLuint t) { (void)t; }
void gfx_tamanho_alvo(int w, int h) { (void)w; (void)h; }
void gfx_rect(GfxRect r, GLuint tex, GfxModo modo, float foco, float px, float py, float raio,
              float cr, float cg, float cb, float ca) {
  (void)r; (void)tex; (void)modo; (void)foco; (void)px; (void)py; (void)raio;
  (void)cr; (void)cg; (void)cb; (void)ca;
}
static char gravado[512];
int dados_gravar(const char *nome, const char *c) {
  assert(!strcmp(nome, "gpu-nivel.txt"));
  snprintf(gravado, sizeof gravado, "%s", c);
  return 1;
}
char *dados_ler(const char *nome) { (void)nome; return gravado[0] ? strdup(gravado) : NULL; }

// Inicializacao real, sem contexto GL: GPU fraca reconhecida pelo perfiltv.
const GLubyte *glGetString(GLenum e) {
  return (const GLubyte *)(e == GL_RENDERER ? "Mali-400" : "");
}
void glGetIntegerv(GLenum e, GLint *v) { (void)e; *v = 0; }

// `seg` segundos de quadros iguais.
static void rodar(double seg, double dt, double espera, double cpu, int naHome, int cheia) {
  double t;
  for (t = 0; t < seg * 1000.0; t += dt) gpun_medir(dt, espera, cpu, naHome, cheia);
}

static void inicioFraco(void) {
  gravado[0] = 0;
  gpun_iniciar(1920, 1080);
  assert(ptv_gpu_fraca_atual());
  rodar(8, 31.25, 25, 4, 1, 1);
  assert(gpun_nivel() == 1 && !gpun_teste_decidido() && !gravado[0]);
  rodar(7, 31.25, 25, 4, 1, 1);
  assert(gpun_nivel() == 0 && strstr(gravado, "bloqueado=1"));
  gpun_iniciar(1920, 1080);
  assert(gpun_nivel() == 0 && gpun_teste_decidido());
  puts("ok  R2 GPU fraca: 32 -> 32 fps nao valida reducao no arranque");
}

static void interrupcao(void) {
  // Sair da Home, perder as artes ou suspender, durante ambos os candidatos.
  for (int degrau = 1; degrau <= 2; degrau++) {
    for (int motivo = 0; motivo < 3; motivo++) {
      gpun_teste_reiniciar(); gravado[0] = 0;
      rodar(8, degrau == 1 ? 1000.0 / 30 : 100, 90, 4, 1, 1);
      if (degrau == 2) rodar(7, 50, 40, 4, 1, 1);
      assert(gpun_nivel() == degrau && !gpun_teste_decidido());
      gpun_medir(motivo == 2 ? 1001 : 40, 30, 4, motivo != 0, motivo != 1);
      assert(gpun_nivel() == degrau - 1 && !gpun_teste_decidido() && !gravado[0]);
      // Outra cena a 50 fps nao deve confirmar o candidato interrompido.
      rodar(8, 20, 15, 4, 1, 1);
      assert(gpun_nivel() == degrau - 1 && gpun_teste_decidido());
      assert(strstr(gravado, "bloqueado=0"));
    }
  }
  // Nova comparacao depois de interrupcao nao usa os 30 fps antigos.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(8, 1000.0 / 30, 25, 4, 1, 1);
  gpun_medir(1001, 0, 0, 1, 1);
  rodar(8, 50, 40, 4, 1, 1);   // referencia nova: 20
  rodar(7, 40, 30, 4, 1, 1);   // 25: +5 e +25%, deve conservar
  assert(gpun_nivel() == 1 && strstr(gravado, "bloqueado=0"));
  assert(strstr(gravado, "fps0=20.000") && strstr(gravado, "fps1=25.000"));
  puts("ok  R2 interrupcao: cancela candidato, preserva nivel anterior e mede nova referencia");
}

static void referenciaTravada(void) {
  for (int legado = 0; legado <= 1; legado++) {
    gravado[0] = 0;
    gpun_iniciar(1920, 1080);
    if (legado) {
      rodar(8, 20, 15, 4, 1, 1); // obtem a chave real da Mali-400
      strstr(gravado, "nivel=")[6] = '2';
      *strstr(gravado, "avaliacao=") = 0;
      gpun_iniciar(1920, 1080);
    }
    char salvo[sizeof gravado]; strcpy(salvo, gravado);
    assert(ptv_gpu_fraca_atual() && gpun_nivel() == 0);
    // Suspensao isolada, fora da Home, artes ausentes e CPU nao rebaixam.
    for (int motivo = 0; motivo < 5; motivo++) {
      gpun_medir(1200, 1100, 4, 1, 1);
      gpun_medir(1200, 1100, 4, 1, 1);
      if (motivo == 4) ptv_definir_gpu_fraca(0);
      gpun_medir(motivo == 0 ? 20 : 1200, motivo == 0 ? 3 : 1100, motivo == 3 ? 1150 : 4,
                 motivo != 1, motivo != 2);
      ptv_definir_gpu_fraca(1);
      assert(gpun_nivel() == 0 && !gpun_teste_decidido());
    }
    for (int i = 0; i < 3; i++) gpun_medir(1200, 1100, 4, 1, 1);
    assert(gpun_nivel() == 2 && minimos && gpun_teste_decidido());
    rodar(30, 20, 15, 4, 1, 1);
    assert(!strcmp(salvo, gravado)); // protecao nao inventa avaliacao=1
    gpun_preferencia(1);
    rodar(5, 1200, 1100, 4, 1, 1);
    assert(gpun_nivel() == 0); // escolha manual prevalece
    gpun_preferencia(0);
    gpun_medir(1200, 1100, 4, 1, 1);
    assert(gpun_nivel() == 0); // reinicio limpa o contador
  }
  puts("ok  R3 referencia travada: Mali-400 nova/legada protegida, sem gravar avaliacao");
}

static void legadoInterrompido(void) {
  for (int degrau = 1; degrau <= 2; degrau++) {
    for (int motivo = 0; motivo < 3; motivo++) {
      gpun_teste_reiniciar();
      snprintf(gravado, sizeof gravado, "versao=1\nchave=0\nnivel=%d\n", degrau);
      char salvo[sizeof gravado]; strcpy(salvo, gravado);
      gpun_preferencia(0);
      rodar(8, 20, 15, 4, 1, 1); // 50 fps no 0 ainda exige comparar o legado
      assert(gpun_nivel() == degrau);
      for (int tentativa = 0; tentativa < 2; tentativa++) {
        gpun_medir(motivo == 2 ? 1001 : 20, 15, 4, motivo != 0, motivo != 1);
        assert(gpun_nivel() == 0 && !strcmp(salvo, gravado));
        rodar(8, 20, 15, 4, 1, 1);
        assert(gpun_nivel() == degrau && !gpun_teste_decidido() && !strcmp(salvo, gravado));
      }
      rodar(7, 1000.0 / 60, 10, 4, 1, 1);
      assert(gpun_nivel() == degrau && gpun_teste_decidido());
      assert(strstr(gravado, "avaliacao=1") && strstr(gravado, "bloqueado=0"));
      assert(strstr(gravado, "fps0=50.000"));
      assert(strstr(gravado, degrau == 1 ? "fps1=60.000" : "fps2=60.000"));
      gpun_preferencia(0);
      assert(gpun_nivel() == degrau); // so a comparacao completa migra o salvo
    }
  }
  puts("ok  R3 legado interrompido: retoma 0 -> 1/2 e so grava depois de 50 -> 60 fps");
}

static void referenciaAlternada(void) {
  for (int legado = 0; legado <= 1; legado++) {
    gpun_teste_reiniciar(); gravado[0] = 0;
    if (legado) strcpy(gravado, "versao=1\nchave=0\nnivel=2\n");
    ptv_definir_gpu_fraca(ptv_gpu_fraca("Mali-400"));
    gpun_preferencia(0);
    char salvo[sizeof gravado]; strcpy(salvo, gravado);
    for (int i = 0; i < 3; i++) {
      assert(gpun_nivel() == 0 && !gpun_teste_decidido());
      gpun_medir(1200, 1100, 4, 1, 1);
      gpun_medir(900, 800, 4, 1, 1);
    }
    assert(gpun_nivel() == 2 && minimos && gpun_teste_decidido());
    assert(!strcmp(salvo, gravado));
  }
  // Uma janela completa nao carrega descartes para a tentativa seguinte.
  gpun_teste_reiniciar(); gravado[0] = 0;
  gpun_medir(1200, 1100, 4, 1, 1);
  gpun_medir(1200, 1100, 4, 1, 1);
  rodar(8, 40, 30, 4, 1, 1);
  assert(gpun_nivel() == 1 && !gpun_teste_decidido());
  gpun_medir(1200, 1100, 4, 1, 1);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido() && !gravado[0]);
  puts("ok  R4: Mali-400 1200/900 ms aciona protecao sem gravar avaliacao");
}

int main(int argc, char **argv) {
  if (argc == 2 && !strcmp(argv[1], "inicio")) { inicioFraco(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "interrupcao")) { interrupcao(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "referencia")) { referenciaTravada(); return 0; }
  if (argc == 2 && !strcmp(argv[1], "legado")) { legadoInterrompido(); return 0; }
  // #410B: reduzir efeitos sem ganhar FPS deve devolver o visual anterior.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(8, 1000.0 / 43.0, 18.0, 4.0, 1, 1);
  assert(gpun_nivel() == 1);
  rodar(7, 1000.0 / 43.0, 18.0, 4.0, 1, 1);
  assert(gpun_nivel() == 0 && gpun_teste_decidido());
  assert(strstr(gravado, "bloqueado=1"));
  puts("ok  #410B: 43 -> 43 fps devolve efeitos completos e bloqueia descida");

  // 1. Tizen 6: 60 fps (a espera e o vsync, grande, mas o FPS e bom) -> fica.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(12, 16.7, 12.0, 4.0, 1, 1);
  assert(gpun_nivel() == 0 && gpun_teste_decidido());
  assert(strstr(gravado, "nivel=0"));
  puts("ok  60 fps: fica no nivel 0 e grava");

  // 2. 25 -> 32 fps: ganho comprovado, conserva o 1 sem ir ao 2.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(8, 40.0, 34.0, 5.0, 1, 1);          // aquece 3 s + janela de 4 s
  assert(gpun_nivel() == 1 && leves == 1 && !gpun_teste_decidido());
  assert(!gravado[0]);   // candidato ainda nao foi validado
  rodar(7, 1000.0 / 32.0, 25.0, 5.0, 1, 1);          // assenta 2 s + janela de 4 s
  assert(gpun_nivel() == 1 && gpun_teste_decidido());
  assert(!strstr(gravado, "nivel=2"));
  puts("ok  25 -> 32 fps: desce 0 -> 1, confirma, grava e para");

  // 3. Desce ate onde o FPS fica bom e para ali.
  gpun_teste_reiniciar();
  rodar(8, 40.0, 34.0, 5.0, 1, 1);
  assert(gpun_nivel() == 1);
  rodar(7, 16.7, 10.0, 4.0, 1, 1);
  assert(gpun_nivel() == 1 && gpun_teste_decidido());
  puts("ok  com o nivel 1 a 60 fps: fica no 1");

  // 3b. GPU lenta: 12 -> 20 -> 25 fps comprova ambos os ganhos; o 720p
  //     (3) nunca e automatico. 20 -> 25 tambem cobre o piso exato de +5 fps.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(8, 83.0, 76.0, 5.0, 1, 1);          // 12 fps: 0 -> 1
  assert(gpun_nivel() == 1 && !gpun_teste_decidido());
  rodar(7, 50.0, 43.0, 5.0, 1, 1);          // ganha: 12 -> 20 fps, ainda critico: -> 2
  assert(gpun_nivel() == 2 && minimos == 1 && !gravado[0]);
  rodar(7, 40.0, 30.0, 5.0, 1, 1);          // no 2 a 25 fps: nao vai ao 3 (720p)
  assert(gpun_nivel() == 2 && gpun_teste_decidido());
  puts("ok  12 -> 20 -> 25 fps: desce ao 2 (efeitos minimos, 1080p) e para");

  // 4. Lento pela CPU (des domina): menos pixel nao ajuda -> nao desce.
  gpun_teste_reiniciar();
  rodar(8, 40.0, 3.0, 36.0, 1, 1);
  assert(gpun_nivel() == 0 && gpun_teste_decidido());
  puts("ok  lento pela CPU: nao desce");

  // 5. Fora da home, ou com a home vazia: nada conta.
  gpun_teste_reiniciar();
  rodar(30, 40.0, 34.0, 5.0, 0, 1);
  rodar(30, 40.0, 34.0, 5.0, 1, 0);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido());
  // Entrar e sair no meio da janela zera a janela: 3 s de aquece + 3 s, sai,
  // volta: 2 s de aquece + 3 s ainda nao fecham uma janela de 4 s.
  rodar(6, 40.0, 34.0, 5.0, 1, 1);
  rodar(1, 40.0, 34.0, 5.0, 0, 1);
  rodar(5, 40.0, 34.0, 5.0, 1, 1);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido());
  puts("ok  fora da home ou home vazia nao mede; sair zera a janela");

  // 6. Cada piso importa, mesmo que o candidato cruze os 45 fps.
  const double base[] = {40, 10, 43};
  const double depois[] = {45.5, 14, 46};
  for (int i = 0; i < 3; i++) {
    gpun_teste_reiniciar(); gravado[0] = 0;
    rodar(8, 1000 / base[i], 20, 4, 1, 1);
    rodar(7, 1000 / depois[i], 20, 4, 1, 1);
    assert(gpun_nivel() == 0 && gpun_teste_decidido());
  }
  // 7. Ganho so no primeiro degrau: volta ao 1, nao perde o ganho provado.
  gpun_teste_reiniciar(); gravado[0] = 0;
  rodar(8, 100, 90, 4, 1, 1);
  rodar(7, 50, 40, 4, 1, 1);
  assert(gpun_nivel() == 2);
  rodar(7, 50, 40, 4, 1, 1);
  assert(gpun_nivel() == 1 && strstr(gravado, "bloqueado=1"));
  assert(strstr(gravado, "fps0=10.000") && strstr(gravado, "fps1=20.000") && strstr(gravado, "fps2=20.000"));
  gpun_teste_reiniciar(); gpun_preferencia(0); // rele o mesmo arquivo
  assert(gpun_nivel() == 1 && gpun_teste_decidido());
  rodar(30, 100, 90, 4, 1, 1);
  assert(gpun_nivel() == 1);
  gpun_preferencia(1); assert(gpun_nivel() == 0 && !gpun_efeitos_automaticos());
  gpun_preferencia(2); assert(gpun_nivel() == 1 && !gpun_efeitos_automaticos());
  gpun_preferencia(0); assert(gpun_nivel() == 1 && gpun_efeitos_automaticos());
  puts("ok  trava persiste ao reiniciar; escolha manual continua funcionando");

  // 8. Legado salvo no 2: mede 0 e compara o 2 uma vez (#410/66108).
  gpun_teste_reiniciar();
  strcpy(gravado, "versao=1\nchave=0\nnivel=2\n");
  gpun_preferencia(0);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido());
  rodar(8, 1000.0 / 43, 18, 4, 1, 1);
  assert(gpun_nivel() == 2 && gpun_efeitos_automaticos());
  rodar(7, 1000.0 / 43, 18, 4, 1, 1);
  assert(gpun_nivel() == 0 && !gpun_efeitos_automaticos());
  assert(strstr(gravado, "versao=1\nchave=0\nnivel=0\n"));
  gpun_teste_reiniciar(); gpun_preferencia(0);
  assert(gpun_nivel() == 0 && gpun_teste_decidido());
  // Firmware/GPU diferente invalida a trava.
  strcpy(gravado, "versao=1\nchave=ff\nnivel=1\navaliacao=1\nbloqueado=1\n");
  gpun_preferencia(0);
  assert(gpun_nivel() == 0 && !gpun_teste_decidido());
  puts("ok  legado 43 -> 43 volta ao 0; mesma chave nao repete, outra chave mede");

  // 9. Legado que ajuda permanece; novo arranque nao volta a testar o 0.
  strcpy(gravado, "versao=1\nchave=0\nnivel=2\n");
  gpun_preferencia(0);
  rodar(8, 50, 40, 4, 1, 1);
  rodar(7, 25, 18, 4, 1, 1);
  assert(gpun_nivel() == 2 && gpun_teste_decidido());
  gpun_teste_reiniciar(); gpun_preferencia(0);
  assert(gpun_nivel() == 2 && !gpun_teste_decidido());
  rodar(8, 25, 18, 4, 1, 1);
  assert(gpun_nivel() == 2 && gpun_teste_decidido());
  // Forcar 720p continua manual e nao mede nem sobrescreve a decisao.
  char salvo[sizeof gravado]; strcpy(salvo, gravado);
  gpun_forcar_720(); rodar(30, 100, 90, 4, 1, 1);
  assert(gpun_nivel() == 3 && !gpun_efeitos_automaticos() && !strcmp(salvo, gravado));
  puts("ok  legado com ganho fica; 720p forcado nao participa");
  legadoInterrompido();
  referenciaAlternada();
  puts("gpunivel: tudo ok");
  return 0;
}
