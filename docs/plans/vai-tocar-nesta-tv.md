# Vai tocar nesta TV (pesquisa de mercado, seção 4.2)

Estado do branch `agente/21-vai-tocar`. Nada aqui foi testado em TV física e o
servidor não foi publicado.

## Etapa 1 — troca de fonte que retoma no mesmo ponto: já existia

Não foi refeita. `tentarProximaFonteVOD` (`src/app.c`) já troca de fonte em
falha do player, prazo de abertura, buffer parado e rede lenta, guarda a
posição (`guardarPosicaoFonteFalha` → `player_definir_retomada_fonte`) e não
depende de servidor (#570, `src/fontefalha.c`). O ajuste para desligar é
"Outra fonte se falhar". A troca avisa com o toast "Fonte falhou, tentando a
próxima"; tirar o aviso é decisão de produto e ficou como está.

## Etapa 2 — registro anônimo de compatibilidade: este branch

| Peça | Onde |
|---|---|
| Regra pura: vocabulário fechado, filtro de modelo/firmware, faixas, JSON | `src/vtreg.{h,c}`, `tests/vtreg.c` |
| Coleta e envio fora do fio de quadro | `src/vtregenvio.{h,c}`, `tests/vtregenvio.c` |
| Consentimento unificado no Farol (linha "Compartilhar compatibilidade" na tela "Vai junto / Nunca vai", issue 53; nenhum cartão próprio no arranque) | `src/farolconsentui.{h,c}` |
| Ajuste "Compartilhar compatibilidade" (Sobre e ajuda; local; padrão desligado) | `AJ_VAITOCAR`, chave `vaiTocarLocal` |
| Tabela de contagem e rota `POST /v1/vaitocar` | `servidor/recomendacoes/migracao-022-vaitocar.sql`, `src/vtreg.js`, `teste-vaitocar.mjs` |

O que sai: plataforma, modelo, firmware, contêiner, codec de vídeo, codec de
áudio, HDR, faixa de bitrate, faixa de altura, resultado e se veio depois de
uma troca. Título, hash, endereço, nome de fonte, conta e perfil não têm campo.
O servidor não guarda linha por pessoa nem IP (o IP só entra, como SHA-256
truncado, na cota de 60 pedidos por hora).

Quando registra: `tocou` quando a fonte prova que tocou
(`registrarFonteVODTocando`); a falha em `tentarProximaFonteVOD`
(`nao_abriu`, `caiu`, `prazo`, `travou`, `lenta`, `decoder`); `tela_preta`
quando a pessoa aperta "Sem HDR". Nada sai em perfil infantil, com o ajuste
desligado ou em pacote sem `NV_REC_URL`.

Teste no Linux: `bash tests/vtreg.sh` (regra, portão do envio e, com Node 22+,
a rota recebendo os corpos que o app gera) e `bash tests/i18n.sh`.

## Falta

- Publicar: aplicar a migração 022 e o Worker (só o dono).
- Samsung: `.wgt` (WASM) não envia — falta o POST sem thread; `.tpk` envia
  mas sem modelo e firmware (`?`).
- `sem_audio` existe no vocabulário e não tem sinal automático.
- As 28 tabelas de idioma receberam o texto em inglês nas 9 chaves novas
  (pt-PT em português); falta traduzir.
- Etapa 3, o veredito por fonte, depois de haver dados: rota de leitura
  agregada e o selo na lista de fontes (`src/vaitocar.c` já desenha o chip).
