# Modo noite / clareza de diálogo (2.1) — spike e primeira etapa

Origem: pesquisa de mercado, seção 3 ("Modo noite / clareza de diálogo"). O
relatório pedia um spike: Android via Media3; LG/Samsung só preferir a faixa
estéreo ou acionar o modo da própria TV.

## O que existe agora

Ajuste **Modo noite** (Reprodução › Som e vídeo desta TV), `Desligado` (padrão),
`Leve`, `Forte`. Local desta TV (`nightModeLocal`), vale a partir do próximo
vídeo. Desligado, nada muda em relação ao que o app já fazia.

| Plataforma | O que o modo faz | Por quê |
|---|---|---|
| Android TV / Google TV | Processa o PCM: sobe o canal central, baixa surround e LFE, comprime a dinâmica e limita o pico. `NoiteAudioProcessor` no `DefaultAudioSink`, antes do `GanhoAudioProcessor` (volume 0–200 %). | O Media3 entrega o PCM decodificado ao app. |
| LG (webOS) e Samsung (.tpk / .wgt) | Só a escolha automática de faixa: se a faixa escolhida por idioma tem mais de 2 canais e existe outra do **mesmo idioma** em 1–2 canais, tocável e sem conversão de DTS, ela é a escolhida. | O áudio vai direto ao decodificador da TV; o app nunca vê o PCM. |
| Linux (desktop) | Igual a LG/Samsung (a regra de faixa). | mpv tem `af=`, mas o desktop é só bancada. |

Regras que não mudam:

- Faixa **escolhida à mão** para o título (lembrada) nunca é trocada.
- Comentário do diretor e audiodescrição não servem de "estéreo" (rótulo com
  `comment`, `coment`, `descri`, `director`, `diretor`).
- Sem idioma na faixa escolhida, o modo não chuta.
- No Android a faixa **não** é trocada: o 5.1 continua 5.1 (sem downmix, o
  receiver recebe o que negociou).
- Passthrough/bitstream (AC-3, E-AC-3, DTS, TrueHD para o receiver) não passa
  por processador nenhum: o modo não age, e a ajuda do ajuste diz isso.

## O que já existia e não foi refeito

- **Normalizar volume** (`AJ_AUDIO_NORMALIZAR`): nivela o volume do áudio do
  player Android e do DTS convertido para PCM. É outro ajuste, independente:
  ele iguala o nível geral, o modo noite reduz a diferença entre fala e
  explosão. No Android os dois convivem (`NoiteAudioProcessor` vem antes do
  `GanhoAudioProcessor`, que normaliza e aplica o volume); a combinação dos
  dois ligados **não foi medida**.
- Volume 0–200 % (`GanhoAudioProcessor`/`GanhoMath`), de onde vem o joelho do
  limitador reutilizado aqui.
- Escolha de faixa por idioma e a memória da faixa escolhida à mão
  (`audioescolha`, `audiomemoria`): a regra de estéreo entra depois delas.

## Parâmetros (src/modonoite.c = NoiteMath.kt)

| | Limiar | Razão | Compensação | Centro | Surround | LFE |
|---|---|---|---|---|---|---|
| Leve | −24 dBFS | 2:1 | +6 dB | +3 dB | −3 dB | −3 dB |
| Forte | −32 dBFS | 4:1 | +12 dB | +6 dB | −6 dB | −9 dB |

Compressor feed-forward ligado entre canais (envelope de pico, ataque 5 ms,
soltura 300 ms), seguido do mesmo limitador suave (tanh acima de 0,891) do
reforço de volume. Layouts sem canal central (1, 2 e 4 canais) recebem só o
compressor.

## Medidas (Linux, sinal sintético)

`bash tests/modonoite.sh`: 2 s de diálogo baixo (300 Hz, −30 dBFS, só no centro
no 5.1) seguidos de 2 s de explosão (−3 dBFS em todos os canais). RMS em regime
do que uma TV estéreo toca (downmix Lo/Ro):

| Entrada | Nível | Diálogo | Explosão | Distância | Pico |
|---|---|---|---|---|---|
| 5.1 | Leve | −36,0 → −27,0 dB | −3,0 → −8,3 dB | 33,0 → 18,7 dB | 0,999 |
| 5.1 | Forte | −36,0 → −23,7 dB | −3,0 → −15,1 dB | 33,0 → 8,7 dB | 0,998 |
| estéreo | Leve | −33,0 → −27,0 dB | −6,0 → −10,4 dB | 27,0 → 16,6 dB | 0,942 |
| estéreo | Forte | −33,0 → −22,2 dB | −6,0 → −15,6 dB | 27,0 → 6,6 dB | 0,884 |

`python3 tests/modonoite_android.py` roda o Kotlin **real** na JVM contra o
Media3 1.8.0: 4248 amostras iguais à referência C (pior diferença 3e−7), mais o
processador em 16 bits, float, 5.1, troca de nível ao vivo, sem clipe, e
bitstream recusado.

São medidas de senoide, não de filme: dizem que a conta está certa, não que
soa bem.

## O que NÃO foi verificado

- Nenhuma TV física, nenhum APK instalado. O APK **não foi montado**. O
  pacote Kotlin inteiro foi passado pelo `kotlinc` (`tests/cacheboost_android.py`)
  nesta máquina: nenhum erro em `NvPlayer.kt` nem nos arquivos novos; a
  compilação não termina aqui por faltarem bibliotecas (okhttp, tvprovider)
  usadas por `ThumbnailTransport.kt` e `TvHome.kt`. O processador foi
  exercitado na JVM, não num aparelho.
- A ordem dos canais que o decodificador do aparelho entrega (assumida a do
  Android: FL FR FC LFE BL BR SL SR). Um decodificador de fabricante que
  entregue outra ordem subiria o canal errado.
- Custo de CPU do processador em aparelho fraco (é um laço por amostra, mesma
  ordem do reforço de volume, mas não foi medido lá).
- O som em filme de verdade: bombeamento do compressor, sibilância, o `Forte`
  em música.
- Em LG/Samsung, quantos arquivos reais têm uma segunda faixa estéreo do mesmo
  idioma (em release de cena é raro: o modo ali muitas vezes não terá o que
  fazer).

## O que falta (próximas etapas)

1. **Ouvir no aparelho** (Android TV) e ajustar os números; decidir se `Forte`
   fica.
2. **Modo da própria TV** (LG/Samsung). Não implementado e **não
   pesquisado a fundo**: falta levantar se webOS e Tizen expõem ao app o modo
   noite/voz clara do fabricante, e testar em TV. Sem isso, nenhuma promessa.
3. **DSP na conversão de DTS** (LG/Samsung), a investigar: onde o app já
   converte o som (ajuste de conversão de DTS/TrueHD) o áudio passa pelo app, e
   seria o único ponto de processamento real nessas TVs.
4. **Controle dentro do player** (painel de áudio), aplicando ao vivo no
   Android: o processador já lê o nível por buffer; hoje só o ajuste o muda, na
   abertura do vídeo.
5. Aviso no player quando o modo está ligado e o som sai em passthrough.
