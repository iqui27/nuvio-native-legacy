---
name: android-release
description: Gerar, testar na TV e publicar o Nuvio nativo para Android TV / Google TV (.apk, host SDL2 + Media3 em android/, nucleo C com -DNV_ANDROID). Use para qualquer build .apk, release ou preview Android, auto-atualizacao do Android, ou mudanca em android/, src/android.c, src/video_android.c, NvPlayer.kt, ParaleloDataSource.kt.
---

# Android TV (.apk)

Passagem para quem faz a release: `docs/android/RELEASE.md`.

Mesmo nucleo C da LG e da Samsung; so a camada de plataforma muda:

| Peca | Onde |
|---|---|
| Activity (ambiente, teclas, despedida, instalar APK) | `android/app/src/main/java/space/nuvio/nativelegacy/NuvioActivity.kt` |
| Player (Media3 1.8 + FFmpeg de audio da Jellyfin) | `NvPlayer.kt` + `src/video_android.c` (mesma ABI de eventos do `.tpk`) |
| Video em 4 conexoes (janela TCP do Android) | `ParaleloDataSource.kt` |
| `[tv]`, logcat, pilha de 8 MB, superficie 4K, instalador | `src/android.c` |
| curl + mbedTLS, libjpeg-turbo, libwebp (.so para o dlopen) | `tools/android/deps.sh` -> `~/.cache/nuvio-android/prefix` |
| Build (debug + release) | `tools/android.sh` |
| Release completa | `tools/release.sh X.Y.Z` |

Android 7+ (minSdk 24), arm64-v8a + armeabi-v7a. Muita TV (a TCL do dono) so
roda 32 bits: as duas ABIs vao sempre.

## Ambiente do Mac

- JDK 17 em `~/.local/jdks/jdk-17*` (o cask do Homebrew pede sudo); SDK em
  `~/Library/Android/sdk` (NDK 27.2.12479018, cmake 3.22.1, build-tools 35).
- Primeira vez numa maquina: `bash tools/android/deps.sh` (curl+mbedTLS com
  `MBEDTLS_THREADING_C` — sem ela o app cai com "Scudo race on chunk header").
- Chave de release: `~/.nuvio-android/release.jks` + `release.env` (chmod 600,
  copia no Vaultwarden). O `android.sh` e o `release-android.sh` leem sozinhos.
  CERT SHA-256 fixado no `release-android.sh`. PERDER A CHAVE = ninguem
  atualiza por cima.

## TV de teste do dono

TCL Smart TV Pro, Android 14, so 32 bits, Mali-G52, `adb connect 192.168.1.128:5555`
(depuracao de rede; se recusar, a depuracao desligou: pedir para religar e
autorizar). Na TV esta o APK de RELEASE (assinatura nova): build debug por
cima exige desinstalar.

- Log do app inteiro: `adb shell "run-as space.nuvio.nativelegacy cat files/dados/nuvio.log"`
  (e `nuvio-anterior.log`). So funciona com APK debuggable; no release, use o
  envio de log do app (D1, `plataforma` `android`).
- logcat da TCL tem 256 KB e roda em segundos: `adb logcat -G 16M` antes.
- Captura de tela NAO pega o video (plano de hardware); geometria do video:
  `adb shell dumpsys SurfaceFlinger` (Disp Frame / Source Crop).
- Instalar por cima sem o modo seguro desfazer ajustes: `adb shell input keyevent HOME`
  antes do `adb install -r` (o onStop grava a despedida).

## Armadilhas ja pagas (nao refazer)

- SDLActivity sem `SDL_HIDAPI` crasha; hidapi e C++ -> `ANDROID_STL=c++_static`.
- Pilha: SDLThread nasce com 16 MB (filtro na copia do SDLActivity no Gradle) e
  todo `pthread_create` do nucleo passa por `--wrap` com 8 MB. O bionic da ~1 MB
  e o guia de TV estourava.
- Home/troca de app mata o processo sem saida limpa: `dados/despedida.txt`
  ("oculto") no onStop, senao o modo seguro desfaz tema/vidro/4K.
- Plano de video da TCL PRENDE a primeira geometria e so liga HDR/DV quando a
  Surface nasce com o decoder ja em HDR: o NvPlayer recria a Surface (GONE ->
  VISIBLE) no primeiro quadro HDR e nas trocas de aspecto (juntadas em 350 ms).
  TextureView foi tentado e recusado: perde HDR.
- Janela lisa (`video_janela`) ENCAIXA com tarja; recorte (`video_janela_fonte`)
  e exato.
- Decoder DV da MediaTek: `c2.mtk.dvhe.*` / `c2.mtk.dvav.*` (sem "dolby").
- 18 Mbps no teste de velocidade = janela TCP por conexao do Android
  (tcp_rmem max 2 MB, ~20 Mbps a 250 ms do debrid). Por isso o video e o teste
  usam 4 conexoes. Redirecionamento do AIOStreams troca de protocolo: o
  ParaleloDataSource segue o 302 a mao.
- Interface 4K: a TCL concede 3840x2160 mas a Mali-G52 cai para 13-48 fps e nao
  se ve diferenca: desligada e escondida no Android. Trailer da Apple limitado
  a 1080p (4K travou a UI 20 s).
- CH+/CH-: com canal na tela trocam canal; fora disso CH+ = AZUL (Salvos) e
  CH- = registro (`app_zap_ativo`, main.c).

## Release (dentro do vX.Y.Z normal)

1. Versao como sempre (`deploy/app/appinfo.json` + `tools/tizen-config.xml`).
2. Na worktree limpa: `NUVIO_PROPERTIES="/caminho/local.properties" bash tools/release.sh X.Y.Z`.
   LG, Android e Samsung rodam em paralelo, com logs/tempos próprios. Entrega
   os 10 artefatos, SHA256SUMS unificado e Homebrew em `build/release-<v>/`.
   Recusa chaves vazias, versão divergente e árvore alterada. Não publica nem
   instala. Android gera só release; debug continua em `tools/android.sh`.
3. Publicação manual no MESMO `gh release create vX.Y.Z`, incluindo o APK.
   Sem ele no latest, nenhum Android se atualiza sozinho. Detalhes e
   `--jobs 2` / `--ensaio`: `docs/android/RELEASE.md`.
4. Enquanto o dono nao liberar a divulgacao do Android: o `.apk` entra como
   anexo, SEM linha na tabela "Which file do I need?", sem aviso no app
   (`avisos.json` plataforma `android`) e sem README. Quando liberar, a linha e:
   `| Android TV / Google TV | Android 7 or newer | \`Nuvio-X.Y.Z-android.apk\` |`

## Cartao de novidades da versao (um .inc por release)

O cartao "Novidades da X.Y.Z" NAO se copia mais por versao. O motor
(`src/novidades_cartao.c/.h`: desenho, paginas, botoes, foco, ponteiro, os QRs
de `apoio.h`, a marca de "ja visto") e unico; o que muda e o CONTEUDO em
`src/novidades/<versao>.inc`. O `app.c` tem um gancho so (`novcartao_*`) e nao
se mexe. Para o cartao da versao X:

1. `cp src/novidades/<anterior>.inc src/novidades/X.inc` e troque `.versao`,
   `.arquivo` (`novidades-X.txt`: a marca de "ja visto" desta versao),
   `.subtitulo`, a lista `ITENS_*` (grupo, icone `aj_*` que existe em
   `deploy/app/art/icones`, nome, frase, `.linhas`, `.plataformas` com
   `NOV_LG|NOV_TPK|NOV_WGT|NOV_ANDROID`) e as cenas da previa (`CENAS_*`).
   Se a previa e uma tela do app, chame o desenho DELA (como `cenaDv` chama
   `dvtela_previa_desenhar`), nunca um desenho a parte.
2. Em `src/novidades_cartao.c` troque a linha `#include "novidades/<anterior>.inc"`.
3. Toda frase nova entra na tabela: `src/idioma_tab.h` (ordem por bytes) e as 28
   irmas (`python3 tools/idiomas.py --sincronizar` cria as linhas vazias; preencha
   e rode `python3 tools/idiomas.py` ate passar). Frase de UMA linha tem de caber
   em ~45 letras latinas (menos em cirilico, grego e japones): a captura reprova
   reticencias.
4. `tests/novidades_cartao.c`: a versao esperada em `main` e as contas por
   plataforma em `plataformas()`.
5. `bash tests/novidades_cartao.sh` e a captura, com a fonte da TV:
   `NUVIO_SHOT_FONTE=3 bash tests/novidades_cartao_shot.sh /tmp/nuvio-novcartao`
   (mede os 30 idiomas em 3 plataformas; manda o PNG ao dono para aprovar; o da
   2.0.3 esta em `docs/releases/2.0.3/`).
6. `docs/releases/X/NOTAS.md`: a PRIMEIRA linha de conteudo e a frase-resumo da
   versao, a mesma do `.subtitulo`.

## Previews (fora do vX.Y.Z)

`gh release create android-preview.N --prerelease --latest=false` com o APK
renomeado `Nuvio-<v>-android-preview.N.apk` (o nome com "preview" nunca casa
com a auto-atualizacao). Substituir o anexo de uma preview existente:
`git tag -f` + `git push -f origin refs/tags/android-preview.N`,
`gh release delete-asset` + `gh release upload`, e atualizar o SHA-256 das
notas. Nunca marcar preview como latest.

## Auto-atualizacao

`src/atualizacao.c` (`acharApk`, `fioInstalarApk`) + `NuvioActivity.instalarApk`
(FileProvider + ACTION_VIEW). Primeira vez a TV pede "permitir instalar apps
desta fonte" e o cartao pede para tentar de novo. So funciona entre APKs com a
MESMA chave. Prova de ponta a ponta: TV com o release N, publicar N+1 como
latest com o `-android.apk`, "Atualizar agora".
