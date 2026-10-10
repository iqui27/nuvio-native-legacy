# `src/webosver.c` — Detecção da versão do webOS

## Para que serve

Lê a versão maior do webOS da TV (webOS 3.x, 4.x, 5.x...) para que outras partes
do app decidam comportamentos específicos por versão. Usa uma única fonte:
primeiro `/var/run/nyx/os_info.json` (chave `webos_release`), depois
`/etc/starfish-release`. O valor é cacheado no primeiro uso e é seguro entre
fios via mutex. Fora de `NV_WEBOS` devolve 0 sem abrir arquivos.

## Plataformas

- **LG webOS**: compilado com `NV_WEBOS`.
- **Outros alvos**: funções existem, mas `nv_webos_major()` devolve 0 e
  `nv_webos_fonte()` devolve `"-"`.

## Funções públicas (de `src/webosver.h`)

| Função | Linha | O que faz | Pré-condições | Fio | Efeitos colaterais |
|---|---|---|---|---|---|
| `int nv_webos_major(void)` | 207 | Devolve a versão maior do webOS (0 = desconhecida). | Nenhuma; cacheia no primeiro uso. | Qualquer | Pode abrir/ler arquivos webOS. |
| `const char *nv_webos_fonte(void)` | 215 | `"nyx"`, `"starfish"` ou `"-"`. | `nv_webos_major` deve ter sido chamado (ou chama internamente). | Qualquer | — |
| `void nv_webos_starfish_linha(char *out, size_t cap)` | 223 | Copia a linha bruta do starfish-release. | Principalmente log. | Qualquer | Escreve em `out`. |
| `int nv_webos_parse_nyx(const char *json)` | 91 | Parser puro do JSON do nyx. | Testes e produção. | Qualquer | Não lê arquivo. |
| `int nv_webos_parse_starfish(const char *texto, char *linha, size_t cap)` | 132 | Parser puro da linha do starfish. | Testes e produção. | Qualquer | Escreve `linha`. |
| `void nv_webos_testar(const char *nyx, const char *starfish)` | 232 | Troca caminhos para teste (só com `AJUSTES_TESTE`). | Testes. | Principal | Reseta cache. |

## Estado global (static)

| Estado | Linha | Tipo | Semântica | Quem lê/escreve |
|---|---|---|---|---|
| `trava` | 7 | `pthread_mutex_t` | Protege a resolução/cache. | `resolver`, `nv_webos_major`. |
| `caminhoNyx`, `caminhoStarfish` | 9-10 / 12 | `char[256]` | Caminhos dos arquivos. | Padrão em produção; sobrescrito por `nv_webos_testar`. |
| `lido`, `major` | 14 | `int`, `int` | Cache: já leu? Qual versão? | `resolver` escreve; `nv_webos_major` lê. |
| `fonte` | 15 | `const char *` | `"nyx"` / `"starfish"` / `"-"`. | `resolver` escreve. |
| `linhaStarfish` | 16 | `char[128]` | Linha lida do starfish. | `resolver`, `nv_webos_starfish_linha`. |

## Grafo de chamadas

```mermaid
flowchart TD
    main[main.c] -->|opcional| webosver[webosver.c]
    video[video.c] -->|decide pipeline| webosver
    player[player.c] -->|compatibilidade| webosver
    webosver -->|ler| nyx[/var/run/nyx/os_info.json]
    webosver -->|ler| starfish[/etc/starfish-release]
```

## Fluxo principal: resolução da versão

```mermaid
sequenceDiagram
    participant C as Caller
    participant W as webosver.c
    participant FS as arquivos webOS

    C->>W: nv_webos_major()
    W->>W: lock; if (lido) return major
    W->>FS: ler /var/run/nyx/os_info.json
    FS-->>W: JSON
    W->>W: nv_webos_parse_nyx
    alt sucesso
        W->>W: fonte="nyx", major=N
    else falha
        W->>FS: ler /etc/starfish-release
        FS-->>W: texto
        W->>W: nv_webos_parse_starfish
        W->>W: fonte="starfish" ou "-"
    end
    W->>W: lido=1
    W-->>C: major
```

## IMPACTOS

- **Se mexer no parser JSON (91)**, ele é muito rígido de propósito: só aceita
  chave `"webos_release"` no nível 1, string `"N.N[.x][-x]"`, documento fechado
  sem sobra, sem duplicatas. Relaxar pode aceitar versões erradas.
- **Se mexer na ordem das fontes**, `resolver` (184) tenta nyx primeiro porque
  webOS 3.9 (2017) não tem starfish-release.
- **Se mexer no cache**, `lido`/`major` são protegidos por `trava`; o valor não
  muda com o app aberto. Erro de E/S no starfish NÃO descarta a versão lida do
  nyx (`e6da2772`).
- **Se mexer nos caminhos**, `nv_webos_testar` (232) só existe com
  `AJUSTES_TESTE`; em produção os caminhos são fixos (9-10).
- **Tests**: `tests/webosver.sh` compila e testa `src/webosver.c` isolado,
  incluindo o parser puro. É o teste mais direto deste módulo.

## Regressões já acontecidas

Do `git log --oneline -- src/webosver.c`:

| Commit | Issue | Resumo | Onde no arquivo |
|---|---|---|---|
| `e6da2772` | #408 | webOS: erro de E/S no starfish não descarta a versão lida do nyx. | 184. |
| `da6bece2` | #408 | webOS: `nv_webos_testar` só existe com `AJUSTES_TESTE`. | 28-30. |
| `6c39c62b` | — | webosver: valida o JSON fora da chave (tipo do fechador, primitivos, vírgulas, escape). | 91. |
| `5303244f` | — | webosver: "N.N" exato, só chave do nível 1 em documento fechado, erro de E/S não guarda, guarda `NV_WEBOS`, starfish estrito. | 207, 132. |
| `79f23456` | — | webOS: uma fonte só para a versão maior (nyx webos_release, depois starfish-release). | 184. |
