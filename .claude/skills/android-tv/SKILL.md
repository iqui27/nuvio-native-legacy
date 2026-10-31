---
name: android-tv
description: Compilar, instalar, depurar e simbolizar o Nuvio nativo no Android TV / Google TV (APK com SDL2 + JNI, libmain.so). Use para qualquer build Android, instalação na TCL do dono, leitura de logcat, queda nativa (tombstone / crash-nativo), relato de Shield/Sony/Xiaomi/TCL, ou mudança em src/android.c, src/video_android.c, android/.
---

# Android TV (APK nativo)

O mesmo C do app (`src/*.c`, `-DNV_ANDROID`) vira `libmain.so` dentro de um
APK SDL2 (`android/`, Activity `NuvioActivity.kt`). O vídeo é Media3/ExoPlayer
(`src/video_android.c` + Kotlin), a rede é libcurl + mbedTLS embutidas.
Pacote: `space.nuvio.nativelegacy`.

## Regras do dono (não negociar)

- **Nunca** trabalhar no checkout principal; usar worktree em
  `/Volumes/ExternalSSD/nv-<algo>`. TMPDIR, cache do npm e Gradle no SSD
  (disco do sistema vive cheio).
- **Não instalar em TV sem pedido do dono nessa tarefa.** Compilar e avisar.
- Na TCL do dono: **só APK de release assinado**, `adb install -r`.
  **Nunca** `uninstall`, `pm clear` nem apagar dados (perde login/add-ons).
- Nunca imprimir valores de `~/.nuvio-android/release.env` nem do
  `local.properties`.
- Não pedir comandos ao usuário que reportou: o app se relata sozinho
  (relato de queda/arranque, "Send log"). No máximo um pedido.

## Build

```bash
cd /Volumes/ExternalSSD/nv-<algo>
export TMPDIR=/Volumes/ExternalSSD/tmp
export NUVIO_PROPERTIES="/Users/hrocha/Projetos/Pessoal/LG WEB/NuvioWeb-0.3.38-beta/local.properties"
bash tools/android.sh
```

- Sai em `build/android/Nuvio-<versao>-android.apk` (release, assinado) e
  `...-android-debug.apk`. O script carrega `~/.nuvio-android/release.env`
  sozinho; sem ele o release fica de fora.
- **Sem `NUVIO_PROPERTIES` o APK sai sem login, add-ons, logs e Discord.**
- O debug **não instala por cima** do release (assinatura diferente). Use o
  release.
- Gradle "Could not read workspace metadata": `pkill -f GradleDaemon; pkill -f KotlinCompileDaemon` e repetir.
- **Cache das dependências** (`NUVIO_ANDROID_CACHE`, padrão
  `~/.cache/nuvio-android`): a libcurl/mbedTLS antiga lê `/dev/random` e trava
  a rede em Android com kernel antigo (#266 Shield, #332 Sony). O
  `tools/android/deps.sh` da branch `agente/203-shield` corrige para
  `/dev/urandom`. Antes de APK de release: rodar esse `deps.sh` (ele refaz a
  libcurl) ou usar `NUVIO_ANDROID_CACHE=/Volumes/ExternalSSD/nv-203-shield-cache`.
  Conferir com `strings libmain.so | grep -c urandom` ou o log `[rede] libcurl pronta`.

## Conferir o APK antes de publicar

```bash
apksigner verify --print-certs build/android/Nuvio-*-android.apk
```

SHA-256 do certificado de release:
`c3c967d4fac138de15126da01ea3ae43b52e6aa23106e336c2acc607ae0f2add`.
Se não bater, não instala por cima de quem já tem o app. O versionCode
precisa ser >= ao instalado (2.0.2 = 20002).

## TV do dono (TCL, Android 11, Mali)

```bash
adb connect 192.168.1.128:5555
adb -s 192.168.1.128:5555 install -r build/android/Nuvio-<v>-android.apk
adb -s 192.168.1.128:5555 shell "am force-stop space.nuvio.nativelegacy; monkey -p space.nuvio.nativelegacy -c android.intent.category.LAUNCHER 1"
adb -s 192.168.1.128:5555 shell pidof space.nuvio.nativelegacy
```

- adb dentro do sandbox dá "No route to host": rodar fora do sandbox
  (`adb kill-server` e de novo).
- Tela: `adb -s 192.168.1.128:5555 exec-out screencap -p > shot.png`
  (julgar visual pela captura da TV, não pelo fixture do Mac, que usa Inter;
  a TV usa Montserrat).
- Teclas: `adb shell input keyevent KEYCODE_DPAD_CENTER|DPAD_LEFT|BACK ...`
  (uma por vez, ~1 s entre elas).
- A interface 4K pode estar ligada na TCL (`[4k]` no log): fps baixo ali
  não é regressão.
- Log do app: `adb logcat -d | grep -E "nuvio|SDL"` — o app também grava o
  próprio log e manda ao servidor.

## Quedas e travas

- **"sinal(2) status=9" / "cch CRE" não é queda:** o app se mata ao sair
  (`NuvioActivity.onDestroy` → `Process.killProcess`).
- **Queda nativa** aparece no log do servidor como `crash-nativo SIGSEGV`
  com `libmain.so+0x…`. Simbolizar com os símbolos guardados de cada release
  (não publicados):
  `/Volumes/ExternalSSD/nv-202-int/build/release-<v>/simbolos/android/libmain-<abi>.so(.nm)`
  — ver a ABI na linha `[tv]` do log (arm64-v8a ou armeabi-v7a).
  `llvm-addr2line -f -C -e libmain-<abi>.so 0x<offset>` (NDK) ou busca no .nm.
- **"Nuvio didn't finish opening / Stuck at: <etapa>"** é o vigia de
  arranque (`ArranqueVigia.kt`); a etapa diz onde travou. O relato vai para
  o servidor como pessoa `arranque:<modelo>`.

## Logs no servidor (D1)

- Código do usuário (ex. `TENEY9`) → id do registro:
  `servidor/recomendacoes/codigo-registro.mjs`.
- Consulta (só leitura; textos grandes vão para arquivo):

```bash
cd "/Users/hrocha/Projetos/Pessoal/LG WEB/nuvio-native-legacy/servidor/recomendacoes"
TMPDIR=/Volumes/ExternalSSD/tmp WRANGLER_LOG_PATH=/Volumes/ExternalSSD/tmp/wr \
  npx -y wrangler d1 execute nuvio-recomendacoes --remote --json \
  --command "SELECT id,pessoa,versao,plataforma,quando,texto FROM registro WHERE id=<id>" > /Volumes/ExternalSSD/tmp/log.json
```

  Ler com `json.JSONDecoder().raw_decode` a partir do primeiro `[`. Filtrar
  sempre por id (consulta ampla estoura o limite de CPU do D1). Nunca
  imprimir tokens nem URLs com chave.

## Armadilhas conhecidas do Android

- Superfície do app translúcida (`setZOrderMediaOverlay` + `TRANSLUCENT`):
  qualquer pixel com alfa < 1 mostra a camada de baixo; compositores de
  fabricante (MStar, Mali) piscam. Três mudanças GL da 2.0.1 (descarte de
  framebuffer, gpu-tempo, fundo dinâmico adiado) causaram o pisca da #318;
  a `teste-318.1` as desliga.
- Decoders MStar começam com `OMX.MS.*`.
- Muitas quedas antigas no Android vinham de fio lendo memória já liberada
  (fonteparalela, #323) — conferir com ASAN no Mac antes de culpar o aparelho.
