// ICONE DO APP — galeria de icones alternativos, SO PARA APOIADORES.
//
// Pedido do dono (01/10/2026): icones para trocar pelo original, so para quem
// apoia no Patreon, "so deixar no codigo funcionando, depois a gente libera e
// pensa como". Entao:
//
//  - O PORTAO E UM SO: apoiador_ativo(). Hoje devolve 0 para todo mundo; o dono
//    forca com NUVIO_APOIADOR=1 no ambiente ou com um arquivo `apoiador.txt`
//    comecando por "1" na pasta de dados (dados_dir()). Sem o portao a linha
//    "Ícone do app" nao aparece nos Ajustes e o icone em vigor e o Original,
//    mesmo que o ajustes.txt guarde outro — quem deixa de apoiar volta ao
//    original sozinho.
//  - O QUE TROCA: a marca desenhada DENTRO do app (iconeapp_desenhar) em toda
//    plataforma; no Android tambem o icone e o banner do launcher (um
//    <activity-alias> por icone, ver AndroidManifest.xml e
//    NuvioActivity.trocarIcone) — trocado quando o app SAI da frente, porque
//    desligar o alias em uso fecha a tarefa (medido); no .wgt da Samsung tambem a imagem da abertura
//    em HTML (tools/tizen-shell.html le localStorage "nuvio-icone"). O icone do
//    PACOTE na LG (appinfo.json) e na Samsung (config.xml / manifest do .tpk) e
//    fixo na instalacao: la so muda dentro do app.
//  - Com o Original a marca NAO aparece nos lugares novos (barra lateral,
//    abertura do catalogo, login): o app fica exatamente como era. So a galeria
//    e o "Sobre" mostram o Original.
//
// Arte: deploy/app/art/icones-app/<id>.png (tools/icones-app.sh), marca com fundo
// transparente no enquadramento do icone original; o ladrilho escuro e desenhado.
#ifndef NV_ICONEAPP_H
#define NV_ICONEAPP_H
#include "gfx.h"

#define ICONEAPP_N 10

// 1 se a pessoa apoia (hoje: so forcado, ver acima). Barato: guarda a resposta.
int  apoiador_ativo(void);
// Esquece a resposta guardada (testes; o dono gravou apoiador.txt com o app aberto).
void apoiador_reler(void);

void iconeapp_iniciar(const char *dirArte);
// Indice 0..ICONEAPP_N-1 do icone EM VIGOR (0 = Original sem o portao).
int  iconeapp_atual(void);
const char *iconeapp_id(int i);                 // "original", "fenix", ...
const char *iconeapp_caminho(int i);            // arte absoluta, "" fora da faixa
// Marca do icone `i` em `r` (quadrado). `ladrilho` = 1 pinta o fundo escuro
// arredondado do icone do launcher por baixo. Devolve 0 enquanto a textura nao
// chegou (quem chama pode desenhar outra coisa).
int  iconeapp_desenhar(int i, GfxRect r, int ladrilho, float alpha);
// A marca nos lugares NOVOS (barra, abertura, login): so com um icone
// alternativo em vigor. Devolve 1 se desenhou.
int  iconeapp_marca(GfxRect r, float alpha);
// Leva o icone em vigor para fora do nucleo: alias do launcher no Android,
// localStorage da abertura no Tizen. Idempotente; chamar no arranque e a cada troca.
void iconeapp_aplicar_plataforma(void);
// 1 se a arte empacotada do icone `i` difere de `gravado` (conteudo de
// iconeapp-arte.txt, NULL = nunca aplicado): hora de aplicar de novo.
const char *iconeapp_arte_atual(int i);   // "id:hash" do pacote
int  iconeapp_arte_mudou(int i, const char *gravado);
#endif
