// REGISTRO NA REDE LOCAL (dono, 16/10/2026): o log desta TV numa pagina da
// rede de casa — http://<ip-da-tv>:8099/log — para ver ao vivo, do celular ou
// do computador, por que a escolha da fonte saiu como saiu, sem Tizen Studio.
//
// Regras:
//   - desligado por padrao (Ajustes > Sobre e ajuda > Registro); ligado, sobe
//     um servidor HTTP minimo num fio proprio, que nunca toca o fio de desenho;
//   - so responde a enderecos da rede local (10/8, 172.16/12, 192.168/16) e ao
//     proprio aparelho; so GET; so /log (a pagina) e /log.txt (o texto);
//   - o texto sai do MESMO arquivo que o painel de registro le
//     (registro_arquivo) e e limpo linha a linha antes de sair: URL cortada no
//     host, valores de apikey/token/senha/Authorization trocados por "…",
//     sequencias longas de letras e numeros (chaves) cortadas;
//   - no .wgt nao existe (o navegador nao abre porta).
#ifndef NUVIO_LOGREDE_H
#define NUVIO_LOGREDE_H

#include <stddef.h>

#define LOGREDE_PORTA 8099   // ocupada: uma livre (logrede_url diz qual)

// Chamado a cada quadro: liga ou desliga o servidor conforme o ajuste.
void logrede_configurar(int ligado, const char *arquivo);
// "http://192.168.1.20:8099/log" com o servidor de pe; "" desligado.
const char *logrede_url(void);
// Limpa uma linha de log no lugar (exposta para teste). Nunca cresce a linha.
void logrede_limpar(char *linha);

#endif
