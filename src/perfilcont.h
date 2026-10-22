#include "arteurl.h"
// "O que essa pessoa estava vendo", sob o perfil em foco na escolha de perfil
// (2.0, variante B). So escolhe e descreve o item; quem desenha e perfilsel.c.
//
// O ITEM e o registro local mais recente ainda nao terminado daquele perfil
// (progresso.c, prog_continuar_de_perfil), pelos mesmos limites de "Continuar
// assistindo". Le so o disco: nenhuma rede para escolher nem para descrever.
//
// TITULO E CAPA. O catalogo em memoria e o do perfil ATIVO, entao o ultimo
// item de OUTRA pessoa quase nunca esta la. Por isso, sempre que o catalogo
// conhece o titulo de um perfil, uma copia minima (titulo, capa, nome do
// episodio) vai para perfilcont.txt; a escolha de perfil usa essa copia quando
// o catalogo nao tem o titulo. Sem nenhuma das duas o cartao nao aparece:
// "Filme" solto sob um rosto nao diz nada. A copia e apagada no logout
// (perfilcont_esquecer, junto de prog_esquecer_tudo).
//
// PERFIL COM PIN NAO MOSTRA NADA. O que alguem viu e conteudo do perfil, e o
// PIN existe para o aparelho nao abri-lo sozinho. Cartao ausente, e nao uma
// linha "Protegido": a linha ja avisaria que ha o que esconder.
#ifndef NV_PERFILCONT_H
#define NV_PERFILCONT_H
#include "perfis.h"

typedef struct {
  char  titulo[160];
  char  poster[NV_ARTE_URL_MAX];     // vazio: cartao so de texto
  char  epNome[96];      // so serie, e so quando se sabe o nome deste episodio
  int   serie, t, e;
  float progresso;       // 0..1
  int   restanteMin;
} PerfilCont;

// 1 e preenche `saida` quando ha cartao para este perfil.
int  perfilcont_de(const ContaPerfil *p, PerfilCont *saida);
// Guarda a copia minima do titulo de cada perfil que o catalogo ja conhece.
// Chamar ao abrir a escolha de perfil (o catalogo e o do perfil ativo).
void perfilcont_registrar(void);
void perfilcont_esquecer(void);

#endif
