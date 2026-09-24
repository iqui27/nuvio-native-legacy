// QUAIS CATALOGOS DE UM ADDON A COTA LE (issue #126).
//
// A cota por addon de descoberta.c deixa cada addon declarar ate N catalogos.
// Ate a 1.4.5 os N eram os PRIMEIROS do manifesto, e o Ultra MAX do relato
// declara 174 com cota 32: o catalogo que a pessoa queria estava entre os 142
// cortados, e nada que ela fizesse na TV ou na conta o trazia de volta.
//
// Aqui fica so a REGRA, pura (sem rede, sem fileiras.c, sem catordem.c), para
// ser testada sozinha em tests/cotacat.sh. Quem sabe o nivel de cada catalogo
// e descoberta.c (prioCatalogo); quem decide quais entram e esta funcao.
#ifndef NV_COTACAT_H
#define NV_COTACAT_H

// Nivel de prioridade de um catalogo, do mais forte para o mais fraco. E a
// mesma precedencia com que a home e montada (ordenarCandidatos).
enum {
  COTA_ESCOLHIDO_TV = 0,   // ligado e na home pela escolha feita NA TV
  COTA_ORDEM_CONTA,        // na ordem de catalogos da conta, na posicao dela
  COTA_ORDEM_LOCAL,        // na ordem do fileiras.txt antigo
  COTA_MANIFESTO,          // nenhuma escolha: a ordem do manifesto
  COTA_DESLIGADO           // desligado em alguma das escolhas: so sobra
};

typedef struct {
  int nivel;   // COTA_*
  int pos;     // posicao dentro do nivel (a da escolha); 0 quando nao ha
} CotaPrio;

// Marca em `escolhido[i]` (1/0) os `max` catalogos que a cota le, dos `n`
// ELEGIVEIS em ordem de manifesto. Empate de nivel e posicao fica com a ordem
// do manifesto. Com n <= max marca todos. Devolve quantos dos marcados estao
// alem da posicao `max` do manifesto — os que a regra antiga teria cortado.
int cota_escolher(const CotaPrio *pr, int n, int max, char *escolhido);

// UMA VAGA GARANTIDA NA HOME PARA O ADDON NOVO (#37), e SO para ele.
//
// A regra do #37 era "addon com zero fileira na janela toma a vaga da segunda
// fileira de quem tem duas". Valia para QUALQUER addon, e na C9 do dono (24/09)
// ela puxou para as posicoes 11-13 fileiras de Pluto TV, Minha TV, FrostView e
// Fenix TV — addons DESLIGADOS na conta — e de Meu Futebol, que ele tinha
// tirado da home na TV, por cima de uma ordem propria. O relato: "a minha ordem
// nao e seguida; tiro as fileiras e elas voltam".
//
// Decisao do dono, e a regra daqui:
//   - com ORDEM PROPRIA (a pessoa moveu algo na TV) nao ha vaga garantida
//     nenhuma: a ordem dela vence inteira;
//   - sem ela, so ganha vaga o addon NOVO: ligado na conta, que este perfil
//     nunca viu nesta TV e sobre o qual a pessoa nao decidiu nada (nenhuma
//     fileira dele desligada, na TV ou na conta). Quem decide `novo` e quem
//     chama (descoberta.c, com fileiras.c e catordem.c);
//   - a fileira que entra nunca e uma desligada.
//
// `ordem` sao os `nOrdem` candidatos (indices) na ordem final; `addonDe[c]` e o
// addon do candidato c (-1 sem addon) e `desligado[c]` diz se ele esta fora
// por escolha. `janela` e quantas posicoes a home pede. Mexe em `ordem` no
// lugar e escreve, para o log, o addon e o candidato de cada vaga dada em
// `vagaAddon`/`vagaCand` (ate `maxVagas`; podem ser NULL). Devolve quantas.
typedef struct { int ativo, novo; } CotaAddon;
int cota_vaga_garantida(int *ordem, int nOrdem, const int *addonDe,
                        const char *desligado, const CotaAddon *ad, int nAd,
                        int janela, int ordemPropria,
                        int *vagaAddon, int *vagaCand, int maxVagas);

#endif
