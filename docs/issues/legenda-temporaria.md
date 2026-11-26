# Legenda temporária (mudo / voltar alguns segundos)

Origem: pesquisa de mercado, seção 3 ("o que ele disse?" sem abrir menu).

## O que faz

Sem legenda ligada, a legenda completa do idioma aparece sozinha quando:

- a pessoa volta entre 2 s e 45 s — some 2 s depois de a cena passar do ponto em que ela estava;
- o aparelho informa que o som está mudo — some quando o som volta.

Não grava na memória de legendas. Quem já assiste com legenda não vê diferença. Uma escolha à mão
no meio toma a posse: a temporária não desliga o que a pessoa escolheu. Canal ao vivo fica de fora.

Idioma, nesta ordem: preferência de legenda (ajuste / memória), idioma do app, idioma do áudio.
Só a legenda completa serve (a forçada não cobre o diálogo).

Ajuste para desligar: Idiomas > Faixas > "Legenda ao silenciar ou voltar" (`legendaTemporariaLocal`,
local, padrão Ligado).

## Onde está

- `src/legtemp.[ch]`: a regra pura (`tests/legtemp.sh`).
- `faixas_legenda_temporaria()` em `src/faixas.c`: escolhe e liga pela mesma `escolherLegenda()` da
  escolha automática (`tests/legtemp_faixas.sh`).
- `legTempQuadro()` / `recuarAutomatico()` em `src/player.c`: os gatilhos.
- `video_mudo()` (só Android): `NvPlayer.mudo()` lê o `AudioManager` (`STREAM_MUSIC`).

## Estado

- Verificado: regra pura e cola de `faixas.c` em teste no Linux (com ASan/UBSan na regra); o app
  compila no Linux e o APK debug compila.
- NÃO verificado: nenhuma TV física, nenhuma reprodução real (o vídeo do build Linux é stub).
- Mudo: só Android tem fonte, e ela não foi testada em aparelho. Com som por HDMI-CEC/ARC o mudo
  do receiver pode não chegar ao `AudioManager`. webOS e Tizen não têm fonte de mudo ainda; neles
  só o gatilho de voltar funciona.
