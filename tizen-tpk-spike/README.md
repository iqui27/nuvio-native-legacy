# tizen-tpk-spike

Spike da Fase 0 (plano `~/.claude/plans/fazer-uma-versao-nativa-parsed-twilight.md`):
descobrir se um `.tpk` .NET consegue rodar `.so` própria, com GL dentro do
processo, em TVs Samsung de anos diferentes, instalado via Apps2Samsung.
Fora de `src/`; nada do app WASM/webOS muda.

## Build

```bash
NUVIO_EXIGIR_ENVIO=1 bash tools/tizen-tpk-spike.sh
```

O token vem de `NUVIO_DIAG_TOKEN` ou, sem ela, de `~/.config/nuvio/diag-token`.

Gera três pacotes em `out/`, um por faixa de TV. O relatório sobe sozinho
para `/v1/registro` no modo "diagnóstico" da build #77 (grava como
`diag:spike-<modelo>`, com um código de 6 letras que também aparece na tela).
Sem `NUVIO_DIAG_TOKEN` o build sai com envio desligado e a tela pede foto;
`NUVIO_EXIGIR_ENVIO=1` transforma isso em erro. O token vai para
`dotnet/Segredos.g.cs` (fora do git). Se o TLS falhar por CA vencida em TV
antiga, o envio tenta de novo sem validar o certificado: não expõe nada novo,
o token já está dentro do `.tpk`.

Todo build também carrega a `.so` num container `arm32v5/debian:buster`
(ARM soft-float, glibc 2.28) e roda `nv_ping`, `nv_thread_test` e o
`spikebin` (`native/teste/carrega.c`); se falhar, o build para. O `.tpk` sai com a
assinatura de desenvolvedor padrão do SDK; o Apps2Samsung reassina por DUID
na instalação (`TizenInstallerService.cs`, `manualResign`).

| Pacote | TFM | api-version | TVs | Testes |
|---|---|---|---|---|
| `NvSpikeLegacy` | `tizen40` (ElmSharp) | 4 | 2018–2020 (Tizen 4–5.5) | 1, 2, 4, 5 |
| `NvSpikeNui60` | `tizen80` (NUI) | 6 | 2021 (Tizen 6.0) | 1, 2, 3b, 4, 5 |
| `NvSpikeNui65` | `tizen90` (NUI) | 6.5 | 2022–2023 (Tizen 6.5–7) | 1, 2, 3b, 4, 5 |
| `NvSpikeNui` | `net6.0-tizen8.0` (NUI) | 8 | 2024+ | 1, 2, 3, 3b, 4, 5 |

Testes (uma linha na tela cada, OK/FALHOU + mensagem; usuário manda foto):
1. `DllImport` da `.so` própria (`nv_ping` = 42)
2. pthread dentro da `.so`
3. `GLView` (widget GL na árvore NUI) desenhando pela `.so`
3b. `GLWindow` (janela GLES inteira, o modelo do SDL de hoje) desenhando pela `.so`
4. `Process.Start` do `spikebin` estático (copiado para `data/` + `chmod`)
5. versão do Tizen e modelo

3 e 3b dão veredito objetivo após 3 s: quadros desenhados pela `.so` e
`glGetError`, não "olhe se animou".

## O que o build revelou (2026-09-24)

- **O workload .NET moderno (`net6.0-tizen`) só aceita Tizen ≥ 8.0**
  (`Samsung.Tizen.Sdk.Versions.targets`: plataformas 8.0–11.0). TV 2018–2022
  só com o SDK antigo (`Tizen.NET` + `Tizen.NET.Sdk` via NuGet, TFMs `tizenNN`).
- **`GLView` só existe a partir da API11 (Tizen 8).** O plano assumia 6.5.
  `GLWindow` existe desde a API8 (Tizen 6.0) e casa melhor com o Nuvio (janela
  GLES inteira, como o SDL). API7 (Tizen 5.5) e anteriores não têm nenhum.
- **A API do `GLWindow` muda a cada versão** (conferido por reflexão):
  API8 `SetEglConfig(.., GLWindow.GLESVersion.Version_2_0)` + quadro `void` e
  sem `RenderingMode`; API9 `SetEglConfig(.., GLESVersion.Version20)` + quadro
  `int`; API11 `SetGraphicsConfig` + `RegisterGLCallbacks`. Por isso há um
  pacote por API: o binário da API8 numa TV API9 poderia falhar no registro
  do callback sem que a Samsung tivesse bloqueado nada. Um port de verdade
  teria de resolver isso em tempo de execução (ou por reflexão).
- **glibc**: linkando contra o rootstrap do Tizen 10 (glibc 2.39) a `.so`
  exigia `pthread_create@GLIBC_2.34` e não carregaria em TV com glibc
  anterior. O build compila com headers do 10 e linka contra o 9 (glibc 2.30);
  o script falha se aparecer `GLIBC_2.34+`. Verificado em container
  `arm32v5/debian:buster-slim` (ARM soft-float, glibc 2.28): `dlopen` OK,
  `nv_ping=42`, `nv_thread_test=1`, `spikebin` roda.

## Preparar o Mac (Apple Silicon)

1. Tizen Studio: `NativeCLI`, `NativeToolchain-Gcc-14.2`,
   `TIZEN-9.0-NativeAppDevelopment-CLI`, `TIZEN-10.0-NativeAppDevelopment-CLI`
   (`package-manager-cli.bin install ... --accept-license`, um por vez; em lote
   morreu com exit 137).
2. O `gcc-14.2` é binário x86_64 (roda por Rosetta) e procura `libisl.23`,
   `libintl.8` e `libzstd.1` em `/usr/local/opt/<pkg>/lib`. Homebrew não
   instala mais cópia Intel lado a lado, e o CLT desta máquina não tem fatia
   x86_64 do `libxcrun` (não dá para compilá-las). Solução: baixar as bottles
   x86_64 (`sonoma`) direto do GHCR (`ghcr.io/v2/homebrew/core/<pkg>`, token
   anônimo), copiar a `.dylib`, `install_name_tool -id/-change` para o caminho
   absoluto e `codesign --force --sign -`. `isl` 0.28, `gmp` 6.3.0 (dependência
   do isl), `gettext` 1.0, `zstd` 1.5.7. `/usr/local/opt` precisou de
   `sudo chown` uma vez.
3. `gcc-9.2` fica sem uso: pede `libisl.19` e o GHCR só tem isl ≥ 0.23.
4. .NET: `dotnet-install.sh --channel 8.0 --install-dir ~/.dotnet` e o
   workload da Samsung (`Samsung/Tizen.NET/workload/scripts/workload-install.sh`).

## Falta

- **TV real** (plano, passo 3): pré-lançamento pedindo TVs 2018, 2019/20,
  2021/22 e 2023+; a tabela ano × teste decide entre port .NET e NaCl.
  TV 2023 (Tizen 7) fica com o pacote 6.5 (`net6.0-tizen` exige 8.0).
- Emulador no winpc: não feito. É x86 (a `.so` ARM falharia lá de qualquer
  jeito) e o Tizen 10 recusou pacote por cadeia de certificado. Nenhuma das
  três telas foi vista rodando: a UI .NET só vai ser exercitada na TV.
