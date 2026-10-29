// "MANTER ESTA FONTE" NO AVISO DE DURACAO (dono, 16/10/2026).
//
// O player compara a duracao do arquivo com a do catalogo. Muito diferente
// (metade ou o dobro, e mais de 10 min) quer dizer, quase sempre, outra obra;
// no automatico a fonte era trocada na hora. Mas ha lancamentos de verdade
// assim: o filme Kaguya-sama "The First Kiss That Never Ends" (96 min) saiu
// tambem cortado em 4 episodios de 25 min, e o automatico pulava todos.
//
// Agora o automatico mostra o aviso com contagem e o botao "Manter esta fonte"
// (OK). Manter guarda o TITULO (o filme, ou a serie inteira) para a conta e o
// perfil, no disco: dali em diante a duracao diferente desse titulo nao pula
// nem avisa. Ate DURACAOOK_MAX titulos; o mais antigo sai.
#ifndef NV_DURACAOOK_H
#define NV_DURACAOOK_H

#define DURACAOOK_MAX 200
#define DURACAOOK_ARQUIVO "duracao-ok.txt"

// 1 se a pessoa ja mandou manter fonte de duracao diferente neste titulo.
int  duracaook_tem(const char *conta, int perfil, const char *titulo);
// Guarda (e grava) o titulo para a conta/perfil.
void duracaook_guardar(const char *conta, int perfil, const char *titulo);

// Para teste: esquece o que esta em memoria (o proximo acesso rele o disco).
void duracaook_zerar(void);

#endif
