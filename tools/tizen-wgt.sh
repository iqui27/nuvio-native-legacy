#!/bin/bash
# Monta o .wgt a partir do que tools/tizen.sh compilou.
#
# O .wgt E O ENTREGAVEL. Nao existe .tpk nativo para TV Samsung: o perfil TV do
# Tizen Studio so gera widget, .tpk nativo e perfil mobile/wearable e o firmware
# recusa sem certificado de parceiro, Tizen .NET para TV acabou e o NaCl foi
# encerrado em 2021.
#
# ---------------------------------------------------------------------------
# QUEM ASSINA E QUEM INSTALA, E ISSO NAO E ESCOLHA NOSSA
#
# O certificado de DISTRIBUIDOR do Tizen carrega uma LISTA BRANCA DE DUIDs — o
# identificador de cada TV. Cabem ate 50, e ela NAO PODE SER ALTERADA depois de
# criada. Um .wgt assinado aqui so instalaria nas TVs que estivessem nessa lista
# no momento em que o certificado foi gerado; na TV de qualquer outra pessoa ele
# e recusado.
#
# Ou seja: distribuir um pacote assinado por nos e IMPOSSIVEL, nao inconveniente.
# O entregavel e o .wgt SEM ASSINATURA, e quem instala assina com o proprio
# certificado, gerado com o DUID da propria TV. E o que Jellyfin e o resto do
# catalogo da comunidade fazem.
#
# Este script produz o pacote sem assinatura SEMPRE. Se a CLI `tizen` estiver no
# PATH e TIZEN_PERFIL apontar para um perfil de certificado, produz TAMBEM um
# assinado — util so para testar na propria TV de quem compilou.
# ---------------------------------------------------------------------------
set -e
cd "$(dirname "$0")/.."

ENTRADA="${NUVIO_SAIDA:-build/tizen}"
ESTAGIO="build/wgt-stage"
NOME="${NUVIO_WGT_NOME:-NuvioTV-native}"

[ -f "$ENTRADA/index.html" ] || { echo "tizen-wgt.sh: rode tools/tizen.sh antes" >&2; exit 2; }

rm -rf "$ESTAGIO"
mkdir -p "$ESTAGIO"
cp "$ENTRADA"/index.html "$ENTRADA"/index.js "$ENTRADA"/index.wasm "$ENTRADA"/decodificador.js "$ESTAGIO"/
[ -f "$ENTRADA/index.data" ] && cp "$ENTRADA/index.data" "$ESTAGIO"/
cp tools/tizen-config.xml "$ESTAGIO"/config.xml
# ICONE OFICIAL DO SAMSUNG, e nao o deploy/app/icon.png do LG.
#
# O do LG e 80x80, que e o tamanho que o webOS pede e que na grade de apps da
# Samsung apareceria interpolado para cima. (Ele era um marcador chapado de 211
# bytes ate a versao 1.0.7, quando virou a marca de verdade — mas o tamanho
# continua sendo o do outro alvo.) deploy/app/tizen/icon.png e o
# store-assets/samsung/icon-512x423.png do app web, que e a arte oficial no
# tamanho que a Samsung especifica para a grade (512 de largura).
if [ -f deploy/app/tizen/icon.png ]; then
  cp deploy/app/tizen/icon.png "$ESTAGIO"/icon.png
else
  echo "tizen-wgt.sh: AVISO — sem deploy/app/tizen/icon.png, usando o icone do LG" >&2
  cp deploy/app/icon.png "$ESTAGIO"/icon.png
fi

# CONFERE ANTES DE FECHAR. Um .wgt sem o .wasm instala, abre e fica preto — o
# mesmo tipo de falha muda que ja mordeu o empacotamento Tizen do fork em
# JavaScript, que saiu sem player.chunk.js e so falhou na TV.
for f in index.html index.js index.wasm decodificador.js config.xml icon.png; do
  [ -s "$ESTAGIO/$f" ] || { echo "tizen-wgt.sh: FALTA $f no estagio" >&2; exit 1; }
done
# O GLUE TEM DE ESTAR REBAIXADO PARA CHROME76.
#
# tools/tizen.sh passa o index.js pelo esbuild porque o Chromium da TV nao
# entende "?.", "??" nem "||=" — com eles a TV instala o pacote, abre e fica
# PRETA, sem erro em lugar nenhum.
#
# Esta guarda existe porque o passo pode falhar SEM derrubar o estagio. Em
# 17/09 o cache do npm apontava para um SSD desmontado: o tizen.sh abortou
# certo (set -e, exit 1), mas deixou build/tizen/index.js com 109 ocorrencias
# de sintaxe nova. Nada aqui reclamaria disso — os arquivos existem e nao estao
# vazios —, e o .wgt sairia pronto para dar tela preta. Rodar o empacotador
# depois de um build que falhou e o caminho normal de quem nao viu o erro
# passar na tela.
NOVA=$(grep -oE '\?\.|\?\?|\|\|=' "$ESTAGIO/index.js" 2>/dev/null | wc -l | tr -d ' ')
if [ "${NOVA:-0}" -gt 0 ]; then
  echo "tizen-wgt.sh: index.js tem $NOVA ocorrencias de sintaxe pos-chrome76 —" >&2
  echo "  o glue NAO foi rebaixado e este .wgt daria tela preta na TV." >&2
  echo "  Rode tools/tizen.sh de novo e confira a linha 'glue rebaixado'." >&2
  exit 1
fi

# A arte empacotada entra pelo index.data (--preload-file em tools/tizen.sh), que
# tools/tizen-art.sh ja filtra: sem collections/, sem cache/, sem *.txt de
# credencial. Aqui so se confere que ninguem contornou aquilo copiando a mao.
if find "$ESTAGIO" -name '*.txt' | grep -q .; then
  echo "tizen-wgt.sh: arquivo .txt solto no estagio — pode ser credencial" >&2
  exit 1
fi

# .wgt e um zip com config.xml na raiz. Sem assinatura, de proposito.
rm -f "$NOME.wgt"
( cd "$ESTAGIO" && zip -q -r -X "../../$NOME.wgt" . )
echo "tizen-wgt.sh: $NOME.wgt ($(du -h "$NOME.wgt" | cut -f1)) — SEM ASSINATURA"

# ROTACAO DOS PACOTES ANTIGOS.
#
# Cada entrega usa um NUVIO_WGT_NOME novo ("NuvioTV-1.0.57-tizen"), entao nada
# aqui sobrescreve nada: os .wgt so se EMPILHAM na raiz do repositorio. Em
# 17/09 eram 49 deles, 1,1 GB, e o disco do Mac encheu — com o disco cheio o
# harness nao conseguia nem abrir o arquivo de saida de um comando, ou seja,
# nenhuma ferramenta rodava. O arm.sh ja fazia o equivalente para os .ipk
# ("rm -f ./*.ipk"); aqui faltava.
#
# CONSERVADOR DE PROPOSITO, porque *.wgt esta no .gitignore e apagar aqui e
# irreversivel — nao ha copia no git, so refazendo o build daquela tag:
#   - so mexe em nome COM VERSAO (NuvioTV-<n>.<n>.<n>...), nunca nos batizados a
#     mao como NuvioTV-native.wgt ou NuvioTV-teste-samsung.wgt;
#   - guarda os NUVIO_WGT_MANTER mais recentes (5 por padrao; 0 desliga);
#   - imprime cada arquivo que apaga, para aparecer no log da entrega.
MANTER="${NUVIO_WGT_MANTER:-5}"
if [ "$MANTER" -gt 0 ] 2>/dev/null; then
  # -t ordena por mtime (mais novo primeiro); o tail corta a cauda velha. O
  # `|| true` cobre o caso de nao haver nenhum, em que o ls falha.
  # `while read`, e nao `for v in $(...)`: o for quebra em ESPACO, entao um
  # "NuvioTV-1.0.49 beta.wgt" virava dois fragmentos, `rm -f` de fragmento
  # inexistente devolve 0 e o log imprimia "apagado" duas vezes sem ter apagado
  # nada. Log que mente sobre o que apagou e pior que nao ter log.
  #
  # `rm -f` tambem devolve 0 para arquivo ausente, entao o teste e a AUSENCIA
  # depois da remocao, nao o codigo de saida.
  ls -t NuvioTV-[0-9]*.[0-9]*.[0-9]*.wgt 2>/dev/null | tail -n +$((MANTER + 1)) |
  while IFS= read -r v; do
    [ -n "$v" ] || continue
    rm -f -- "$v"
    if [ -e "$v" ]; then
      echo "tizen-wgt.sh: rotacao — NAO consegui apagar $v" >&2
    else
      echo "tizen-wgt.sh: rotacao — apagado $v"
    fi
  done
fi

if command -v tizen >/dev/null && [ -n "${TIZEN_PERFIL:-}" ]; then
  echo "tizen-wgt.sh: assinando tambem com o perfil '$TIZEN_PERFIL' (so serve nas SUAS TVs)"
  tizen package -t wgt -s "$TIZEN_PERFIL" -- "$ESTAGIO"
  mv "$ESTAGIO"/*.wgt "$NOME-assinado.wgt" 2>/dev/null || true
fi

cat <<'FIM'

COMO INSTALAR (quem instala assina, ver o cabecalho deste script):

  1. Tizen Studio + TV extension. No Certificate Manager, criar um certificado
     Samsung: perfil de autor, depois o de distribuidor com o DUID DA SUA TV.
     A TV mostra o DUID em Apps > digitar 12345 > Developer Mode > About, e o
     sdb tambem responde:  sdb devices
     ESSA LISTA NAO PODE SER MUDADA DEPOIS. Registre todas as TVs de uma vez.

  2. Na TV: Apps > digitar 12345 > Developer Mode ON > IP desta maquina.

  3. Assinar e instalar:
       unzip NuvioTV-native.wgt -d nuvio-wgt
       tizen package -t wgt -s <seu-perfil> -- nuvio-wgt
       sdb connect <ip-da-tv>
       tizen install -n nuvio-wgt/NuvioTV-native.wgt -t $(sdb devices | awk 'NR==2{print $1}')

  Privilegios do config.xml sao todos de nivel PUBLIC. Um privilegio acima do
  nivel do certificado NAO degrada: faz a instalacao falhar inteira com
  MISMATCHED_PRIVILEGE_LEVEL.

  Ressalva conhecida: em firmwares novos o app instalado por Developer Mode para
  de abrir depois de um tempo e precisa ser reinstalado.
FIM
