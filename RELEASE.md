# Como publicar uma versão

Escrito em 18/09/2026, depois de a receita viver só na cabeça de quem publicava
e ter sido reconstruída à mão lendo o commit da tag anterior. Errar um passo
daqui publica um pacote que se identifica errado na TV, ou que troca a variante
de alguém sem avisar.

## Regra: release com novidade precisa de "o que há de novo"

**Toda versão que muda alguma coisa que a pessoa vê ou sente precisa anunciar
isso dentro do app**, em `src/novidades.c` — não só nas notas do GitHub.

Vale para funcionalidade nova E para mudança de comportamento do que já
existia. O caso que fez esta regra existir: a v1.1.2 trocou o padrão de áudio
para "idioma original do título". Quem atualizou passou a ouvir outra faixa sem
nada na tela explicando por quê — e do lado de fora isso se parece com defeito,
não com decisão.

Como o cartão funciona hoje (ver o cabeçalho de `src/novidades.c`):

- A marca é **por conteúdo, não por versão**: `novidades-guia.txt` cobre a
  rodada do Guia de TV. A rodada seguinte ganha o **seu próprio** arquivo-marca
  e o seu próprio cartão. Quem já viu o anterior não vê de novo; quem nunca viu
  recebe só o novo.
- O cartão é **desenhado**, não é imagem: não há renderizador de SVG no app. A
  figura sai das mesmas primitivas da tela que ela anuncia.
- Texto nas duas línguas, com chave em `src/idioma_tab.h` — `tests/i18n.sh`
  reprova literal de tela sem chave.

Correção honesta pendente: `novidades.c` tem um cartão cravado em código. Uma
rodada nova exige código novo. Se isso virar atrito, o caminho é torná-lo
orientado a dados — mas até lá **a regra continua valendo**, e o custo de
escrever o cartão faz parte do custo da funcionalidade.

## Os passos

### 1. Versão, nos DOIS arquivos

`deploy/app/appinfo.json` (`"version"`) e `tools/tizen-config.xml` (`version=`).
O commit de versão é só isso, com a mensagem sendo apenas `v1.1.N`.

`tools/env.sh` lê o appinfo, **aborta se o tizen-config discordar** e injeta
`-DNV_VERSAO`. A tela de Ajustes mostra esse valor.

### 2. Pacotes

São **três**, e a ordem importa:

```bash
bash tools/arm.sh --ipk --build          # LG normal
# arm.sh faz `rm -f ./*.ipk`: TIRE o .ipk da frente antes da próxima build
bash tools/arm.sh --alto-cache --ipk --build   # LG cache grande
bash tools/tizen.sh
NUVIO_WGT_NOME="NuvioTV-1.1.N-tizen" bash tools/tizen-wgt.sh
```

`--build` empacota **sem** tocar na TV. Sem ele, `arm.sh` instala.

**Não existe `.wgt` de cache grande.** Não é esquecimento: medido na
QN85Q70AAGXZD, o teto de 300 MB ficou MAIS LENTO que o normal (o mesmo decode
foi de 1481 ms para 3575 ms), e `tex_cache.c` passou a ignorar
`NV_TEX_MB_FIXO` no alvo Tizen. Um pacote highcache de Samsung se comportaria
igual ao normal — publicá-lo seria vender diferença que não existe.

### 3. O nome do anexo é contrato, não enfeite

`src/atualizacao.c` escolhe o anexo pelo **sufixo** que a build espera:
`_arm.ipk` para a normal, `-highcache.ipk` para a de cache grande. Renomear os
pacotes quebra o "Atualizar agora" — ou pior, faz a pessoa trocar de variante
sem saber, que foi exatamente o defeito de 18/09.

**Aconteceu de novo na 1.3.1 e na 1.3.2**: os anexos subiram como
`NuvioTV-1.3.N-webos.ipk`, que não termina em `_arm.ipk`, e toda LG normal
ficou sem o botão por duas versões (a highcache não sentiu). Desde a 1.3.3 o
app aceita também `-webos.ipk`, mas quem está na 1.3.1/1.3.2 normal só ganha
o botão se o anexo se chamar `..._arm.ipk`. Então os nomes são:

```
space.nuvio.native.legacy_1.3.N_arm.ipk
space.nuvio.native.legacy_1.3.N_arm-highcache.ipk
NuvioTV-1.3.N-tizen.wgt
```

### 4. Conferir credenciais ANTES de subir

A lista de exclusão dos scripts é intenção; isto é fato:

```bash
ar p <ipk> data.tar.gz | tar tz | grep 'art/<arquivo>$'
unzip -l <wgt> | grep -E '\.txt|collections'
```

Para `trakt.txt`, `addons.txt`, `tmdb.txt`, `mdblist.txt`, `sessao.txt`,
`ajustes.txt`, `nuvem.txt`, `perfil.txt`, `cliente.txt` e `collections.json`.
Tudo tem de dar **0**.

### 5. Notas da release

Inglês, Markdown, `## Added` / `## Fixed`. O app mostra as notas num cartão
(`src/atualizacao.c`) com **3 linhas por bala** — bala longa é cortada na TV.

**O corte em `## Notes` leva junto tudo que vier depois.** Se a informação
importa para quem está na frente da TV, ela fica ACIMA dessa seção.

### 6. Publicar e conferir o que o servidor entregou

```bash
git push origin master
git tag v1.1.N && git push origin v1.1.N
gh release create v1.1.N --title "Nuvio 1.1.N" --notes-file <notas> <os três arquivos>
```

Depois **baixe de volta** e compare sha256 com o local. Timestamp não
identifica build — já se publicou o pacote errado conferindo só a data.

## Armadilhas que já morderam

- **`tools/tizen.sh` depende do `npx`**, e `~/.npm` é um symlink para um SSD
  externo. Com o SSD desmontado o build falha; o `.wgt` daquele momento não
  sai, mas um `tizen-wgt.sh` rodado em seguida empacotaria o glue velho. Há
  guarda contra isso desde 18/09, e ela recusa o pacote.
- **O glue tem de ser rebaixado para `chrome69`**, não 76: `required_version
  5.5` é Chromium M69. A conferência é por idempotência — rebaixar de novo não
  pode mudar nada — porque contar construções por grep já deixou passar
  centenas de class fields dizendo "0 restante".
- **Os `.wgt` e `.ipk` estão no `.gitignore`.** Apagar um pacote publicado é
  irreversível; só refazendo o build daquela tag.
- **Não diga "corrigido" num issue antes de a release existir** com os pacotes
  anexados. `master` não é o que a pessoa instala.
