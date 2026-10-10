# Release Android, LG e Samsung

Na worktree limpa, com `deploy/app/appinfo.json` e `tools/tizen-config.xml`
na mesma versão:

```bash
export NUVIO_PROPERTIES="/caminho/local.properties"
export NUVIO_ANDROID_CACHE="/caminho/cache-android"
export NUVIO_ARES_PACKAGE="/caminho/ares-package"
export TMPDIR="/caminho/tmp"
bash tools/release.sh 2.0.4
```

Um comando compila LG normal/highcache, Android e Samsung em paralelo,
espera todos e falha se qualquer etapa falhar. Não publica nem instala.
`--jobs 2` limita a duas plataformas; `NUVIO_BUILD_JOBS` limita os compiladores
nativos por container (padrão LG 3, Samsung 6).

Saída: `build/release-<v>/`, com 2 IPK, APK, WGT, 4 TPK, 2 SO de atualização,
`repo.json`, `webosbrew.manifest.json` e `SHA256SUMS` unificado.
`logs/*.log` contém prefixo/tempo por plataforma; `logs/timings.json` contém
segundos e RC. `verification.json` registra hashes/tamanhos dos binários e
configuração conferida, sem valores de chaves.

A release aborta com chave vazia, versão divergente, árvore suja ou alterada
durante o build. Também mantém as verificações de certificado Android,
privacidade dos pacotes, TLS do TPK40 e igualdade dos anexos SO.
`build/ass-wasm` é achado automaticamente em outra worktree e reutilizado por
symlink; se nenhuma tiver as bibliotecas, prepare com `tools/build-ass-wasm.sh`.
Dependências Android/TPK prontas e objetos nativos são preservados.

`--ensaio` serve somente para medir alterações locais sem commit: registra
`dirty`/`ensaio`, exige conteúdo inalterado durante o build e recusa instalação.
Não é evidência de árvore limpa. `--instalar` é separado e, após a conferência,
chama `arm.sh` para instalar **LG normal** na TV configurada. Publicação por
`gh release create` permanece manual, fora do script.

## Pré-requisitos Android

- JDK 17 (`JAVA_HOME`), Android SDK 35, NDK 27.2.12479018 e CMake 3.22.1.
- `tools/android/deps.sh`: curl, libjpeg e libwebp, uma vez por cache.
- `~/.nuvio-android/release.jks` e `release.env`, ou variáveis `NUVIO_KEYSTORE*`.
  Cópia no Vaultwarden; o certificado fixado em `release-android.sh` não muda.
- Release gera somente `assembleRelease`, sempre com arm64-v8a e armeabi-v7a.
  `tools/android.sh` continua disponível para debug; `release-android.sh`,
  para conferir somente Android.

A atualização busca exatamente `Nuvio-<v>-android.apk` na mesma release de LG
 e Samsung. Debug/preview não casam. Publique todos os artefatos juntos e
confira os hashes após o upload. Instalação e comportamento na TV exigem
validação separada.
