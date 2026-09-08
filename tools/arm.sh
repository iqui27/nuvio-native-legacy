#!/bin/bash
# Compila para ARM e instala na TV por COPIA DIRETA. Ciclo completo (~30s).
#
#   bash tools/arm.sh            # compila, copia, confere e lanca
#   bash tools/arm.sh --build    # so compila
#   bash tools/arm.sh --ipk      # tambem gera o .ipk (para distribuir)
#
# NAO usa o appInstallService, e a razao esta escrita no ponto do envio: ele
# responde sucesso e nao troca o binario.
#
# A imagem vem de tools/Dockerfile:
#   docker build --platform linux/arm64 -t nuvio-webos-sdk tools/
#
# -ldl e obrigatorio: video.c abre libluna-service2 por dlopen. A linha de
# compilacao do runbook antigo nao tinha, e o link falhava em 'dlsym@@GLIBC_2.4'.
#
# NAO acrescente -lcurl. rede.c abre libcurl por dlopen em execucao, tentando
# so.5 e depois so.4. Linkar curl cria dependencia rigida na libcurl.so.4 do
# SDK, e a TV so tem /usr/lib/libcurl.so.5 — o binario nem inicia, com
# "libcurl.so.4 => not found" e nenhuma mensagem visivel na tela.
set -e
cd "$(dirname "$0")/.."

TV_IP="${NUVIO_TV_IP:-192.168.1.32}"
TV_PASS="${NUVIO_TV_PASS:-alpine}"
APP_ID="space.nuvio.native.legacy"
ARES="../NuvioWeb-0.3.38-beta/node_modules/.bin/ares-package"

echo "==> compilando para ARM"
# A configuracao do servidor entra por VARIAVEL DE AMBIENTE e os -D sao montados
# dentro do container. Passa-los na linha de `sh -c` exigiria aspas dentro de
# aspas e o erro seria MUDO: a macro chega vazia, o binario compila, instala,
# abre — e a tela de login diz "pacote montado sem servidor". Foi o que
# aconteceu no primeiro deploy desta funcionalidade.
# Uma limpeza SO, com tudo dentro: `trap` nao acumula — um segundo `trap ... EXIT`
# substitui o primeiro, e o arquivo com a chave anonima ficaria para tras em
# /tmp toda vez que o modo --ipk fosse usado.
LIXO=""
limpar() { [ -n "$LIXO" ] && rm -rf $LIXO; }
trap limpar EXIT
ENVF=$(mktemp); LIXO="$LIXO $ENVF"
tools/env.sh --env-file "$ENVF"
docker run --rm --platform linux/arm64 --env-file "$ENVF" \
  -v "$PWD":/work nuvio-webos-sdk sh -c '
  SR=$NUVIO_SYSROOT
  arm-webos-linux-gnueabi-gcc src/*.c -o nuvio-proto.arm -O2 \
    -DNV_SUPABASE_URL="\"$NV_SUPABASE_URL\"" \
    -DNV_SUPABASE_ANON_KEY="\"$NV_SUPABASE_ANON_KEY\"" \
    -DNV_TV_LOGIN_BASE="\"$NV_TV_LOGIN_BASE\"" \
    -DNV_TRAKT_CLIENT_ID="\"$NV_TRAKT_CLIENT_ID\"" \
    -DNV_TRAKT_CLIENT_SECRET="\"$NV_TRAKT_CLIENT_SECRET\"" \
    -DNV_SIMKL_CLIENT_ID="\"$NV_SIMKL_CLIENT_ID\"" \
    -DNV_SIMKL_APP="\"$NV_SIMKL_APP\"" \
    -DNV_TMDB_API_KEY="\"$NV_TMDB_API_KEY\"" \
    -I$SR/usr/include -I$SR/usr/include/SDL2 \
    -lSDL2 -lSDL2_image -lSDL2_ttf -lGLESv2 -lEGL -ldl -lpthread -lm'

# CONFERE que a configuracao entrou MESMO no binario. Sem isto o unico sintoma
# e a tela de login dizendo que o pacote saiu sem servidor, ja na TV.
# A conferencia checa CADA chave, lendo os VALORES DO ENV-FILE.
#
# Ela ja leu do ambiente do SHELL, e isso a tornava inutil sem parecer: as
# variaveis so existem DENTRO do container (o --env-file as injeta la), entao
# no host todas davam vazias, cada uma caia no "aviso: vazio" e a checagem era
# PULADA. Uma guarda que se auto-desliga e pior que guarda nenhuma, porque
# tranquiliza.
while IFS='=' read -r NOME VALOR; do
  [ -z "$NOME" ] && continue
  if [ -z "$VALOR" ]; then
    echo "    aviso: $NOME vazio em local.properties"
    continue
  fi
  if ! strings nuvio-proto.arm 2>/dev/null | grep -qF "$VALOR"; then
    echo "    ABORTADO: $NOME nao entrou no binario ARM"
    exit 1
  fi
done < "$ENVF"

cp nuvio-proto.arm deploy/app/nuvio-proto
rm -f ./*.ipk

# O .ipk so interessa para DISTRIBUIR (instalar em outra TV, publicar). O ciclo
# de desenvolvimento nao passa por ele — ver a nota abaixo.
#
# EMPACOTA DE UMA COPIA LIMPA, nunca de deploy/app direto. Motivo concreto:
# `ares-package deploy/app` leva a pasta INTEIRA, e art/ tem credencial de
# PESSOA — o token do Trakt, as URLs de addon com a chave do debrid embutida, a
# chave do TMDB e a do mdblist (esta ate com modo 0600, de tao secreta). Um
# .ipk gerado assim entrega tudo isso para quem instalar. Ate a versao com
# login isso nao tinha como ser diferente, porque o app dependia dos arquivos;
# agora ele nao depende mais, e continuar embarcando-os seria so descuido.
#
# ajustes.txt sai pelo mesmo motivo, com dano menor: e a preferencia de LAYOUT
# de quem montou, e ela chegaria como se fosse a de quem instalou.
ARQ_DE_PESSOA="trakt.txt addons.txt tmdb.txt mdblist.txt ajustes.txt
               progresso.txt nuvem.txt sessao.txt perfil.txt cliente.txt"

# O ACERVO DE QUEM EMPACOTOU, que nao e credencial de login e vaza igual.
#
# collections.json E CREDENCIAL, apesar da extensao. Cada fonte de pasta guarda
# a URL do addon do dono, e nessas URLs a chave viaja no CAMINHO
# ("https://host/manifest/<pid>/<jwt>"). Um .ipk publicado com esse arquivo
# entrega a instalacao de addon do dono a todo mundo que instalar. A lista de
# .txt acima nunca o pegou porque ele nao termina em .txt — e a conferencia
# tambem nao, porque ela so procurava aquela lista. Isto e o mesmo defeito que
# ja custou o vazamento de art/trakt.txt uma vez, com outra extensao.
#
# collections/ sao 165 MB de quadros de animacao do catalogo CURADO do dono:
# quem instalasse veria a colecao de outra pessoa como se fosse sua. Era
# pendencia conhecida deste .ipk (tools/tizen-art.sh ja a recusa no .wgt e diz
# isso por escrito) e sai agora.
#
# NAO E PERDA DE FUNCIONALIDADE: sem collections.json o app nao tem pasta LOCAL
# nenhuma e as colecoes passam a vir so da CONTA, que e exatamente como o alvo
# Tizen ja funciona. A arte editorial embarcada tambem so era usada por pastas
# locais (col_definir_json reaproveita arte apenas do que tem `local`), e as
# chaves dela sao os ids das colecoes DO DONO — para outra conta ela nunca
# casaria.
#
# catalogo-rede.bin CONTINUA NA LISTA, e agora tambem o .tmp dele.
#
# O cache do catalogo passou a ser gravado em dados_dir() e nao mais na pasta da
# arte (ver a nota em caminhoCache, src/catalogo.c), o que em quase todo
# aparelho ja o tira daqui sozinho. "Quase" nao serve para uma conferencia de
# credencial: dados_iniciar tem `dirArte` como ULTIMO candidato, entao num
# aparelho onde nenhuma outra pasta aceita escrita o arquivo volta a nascer
# exatamente aqui. Alem disso qualquer .ipk montado de uma copia antiga do
# deploy/ ainda tem o arquivo velho no lugar velho.
#
# O .tmp e novo na lista e nao e zelo excessivo: cat_gravar_cache escreve num
# temporario e renomeia, e uma escrita interrompida deixa um
# catalogo-rede.bin.tmp com as fileiras — e portanto com a `base` de cada
# catalogo, que no Xperience leva um JWT dentro do CAMINHO. A conferencia casa
# nome exato ("art/$f$"), entao o .tmp precisava do proprio item; foi assim que
# collections.json escapou uma vez.
ACERVO_DE_PESSOA="collections.json catalogo-rede.bin catalogo-rede.bin.tmp"
DIR_DE_PESSOA="collections"

if [ "$1" = "--ipk" ]; then
  echo "==> empacotando (sem credenciais)"
  PALCO=$(mktemp -d); LIXO="$LIXO $PALCO"
  cp -R deploy/app "$PALCO/app"
  # cache/ e cache de EXECUCAO, nao arte do pacote: sao megabytes de imagem
  # baixada que o app rebaixa sozinho.
  rm -rf "$PALCO/app/art/cache"
  for f in $ARQ_DE_PESSOA $ACERVO_DE_PESSOA; do rm -f "$PALCO/app/art/$f"; done
  for d in $DIR_DE_PESSOA; do rm -rf "$PALCO/app/art/$d"; done

  "$ARES" "$PALCO/app" -o .
  IPK=$(ls -t ./*.ipk | head -1)

  # CONFERE o pacote PRONTO, e nao a pasta de onde ele saiu. A lista de
  # exclusao acima e uma intencao; o teste abaixo e o fato.
  #
  # ARMADILHA MEDIDA: o .ipk e um pacote Debian (arquivo `ar` com
  # debian-binary + control.tar.gz + data.tar.gz). `tar tzf pacote.ipk` LISTA,
  # sem erro nenhum, apenas esses tres nomes — nunca os arquivos do app. Uma
  # conferencia escrita assim passa sempre, inclusive quando o segredo esta
  # dentro. Tem de desempacotar o `ar` e listar o data.tar.gz.
  LISTA=$(cd "$PALCO" && ar x "$OLDPWD/$IPK" 2>/dev/null && tar tzf data.tar.gz 2>/dev/null)
  if [ -z "$LISTA" ]; then
    echo "    ABORTADO: nao consegui LER o pacote para conferir; nao vou dizer que esta limpo"
    rm -f "$IPK"
    exit 1
  fi
  VAZOU=""
  for f in $ARQ_DE_PESSOA $ACERVO_DE_PESSOA; do
    printf '%s\n' "$LISTA" | grep -q "art/$f$" && VAZOU="$VAZOU $f"
  done
  # Diretorio: qualquer caminho DENTRO dele conta como vazamento, nao so a
  # entrada da pasta — o tar pode listar os arquivos sem listar o diretorio.
  for d in $DIR_DE_PESSOA; do
    printf '%s\n' "$LISTA" | grep -q "art/$d/" && VAZOU="$VAZOU $d/"
  done
  if [ -n "$VAZOU" ]; then
    echo "    ABORTADO: o pacote leva credencial ->$VAZOU"
    rm -f "$IPK"
    exit 1
  fi
  echo "    $IPK ($(du -h "$IPK" | cut -f1)) — sem art/{$(echo $ARQ_DE_PESSOA $ACERVO_DE_PESSOA $DIR_DE_PESSOA | tr ' ' ',')}"
fi

[ "$1" = "--build" ] && exit 0

# A TV ESTA ROOTEADA: copia direta, sem passar pelo instalador.
#
# O appInstallService responde `"returnValue": true` e `statusValue: 264`
# (instalado) e NAO SUBSTITUI O BINARIO — escreve art/, deixa nuvio-proto e
# appinfo.json intactos. Perdi tres deploys achando que tinha subido: o app na
# TV ficou 2h30 rodando uma versao antiga enquanto o log dizia sucesso.
#
# Com root nao ha motivo para o intermediario. scp + mv + chmod faz o mesmo em
# dois segundos, e o md5 no fim PROVA que subiu — a licao real e essa: deploy
# sem verificacao e torcida, nao entrega.
#
# ares-install tambem nao serve aqui: ele espera prisoner@<ip>:9922 do Developer
# Mode, e esta TV nao roda o Developer Mode — e root na 22 com senha alpine.
APPDIR=/media/developer/apps/usr/palm/applications/$APP_ID
SSH="sshpass -p $TV_PASS ssh -o StrictHostKeyChecking=no"
SCP="sshpass -p $TV_PASS scp -o StrictHostKeyChecking=no -q"

# O DIRETORIO PODE NAO EXISTIR: o dono pode ter desinstalado o app pela TV, e
# ai todo scp abaixo falha com "No such file or directory" — que foi exatamente
# o que aconteceu. Criar antes torna o deploy capaz de REINSTALAR, nao so
# atualizar.
$SSH "root@$TV_IP" "mkdir -p $APPDIR"

echo "==> enviando binario para $TV_IP"
$SCP nuvio-proto.arm "root@$TV_IP:$APPDIR/nuvio-proto.novo"
# Renomear em vez de sobrescrever: se o app estiver rodando, o executavel esta
# mapeado e a escrita direta falha com ETXTBSY. O rename troca o inode.
$SSH "root@$TV_IP" "cd $APPDIR && mv -f nuvio-proto.novo nuvio-proto && chmod 755 nuvio-proto"

# A ARTE muda pouco, mas quando muda (icone novo, fonte) tem de ir junto.
# CARIMBO DE BUILD NO TITULO.
#
# "ainda e build antiga" nao tem como ser respondido olhando a tela: o md5 do
# binario prova o que esta no DISCO, nao o que foi LANCADO, e a TV tem dois apps
# Nuvio (este e o web "Nuvio TV") — abrir o tile errado da exatamente o mesmo
# sintoma. Com os 8 primeiros digitos do md5 no titulo, o launcher responde
# sozinho qual build esta ali.
echo "==> carimbando titulo com a build"
STAMP=$(md5 -q nuvio-proto.arm 2>/dev/null || md5sum nuvio-proto.arm | cut -d' ' -f1)
STAMP=${STAMP:0:8}
sed "s/(BUILD)/($STAMP)/" deploy/app/appinfo.json > /tmp/appinfo.stamped.json
cp /tmp/appinfo.stamped.json deploy/app/appinfo.json.stamped

$SCP /tmp/appinfo.stamped.json "root@$TV_IP:$APPDIR/appinfo.json"

echo "==> sincronizando arte"
# --exclude art/cache: e CACHE DE EXECUCAO, nao arte do pacote. Com ele o tar
# passava de 49 MB (27 MB so de posteres baixados no Mac) e a extracao no
# busybox da TV morria no meio — e o que vinha DEPOIS de "cache/" na ordem
# alfabetica, "marcas/" inclusive, sumia em silencio. Foi assim que o wordmark
# do Trakt "subiu" tres vezes sem nunca chegar la.
#
# Sem o cache o tar cai para poucos MB. A TV reconstroi o dela sozinha, e nao
# herda mais o uid do Mac — a mesma armadilha que o chown abaixo remedia.
#
# 2>&1 e nao 2>/dev/null: erro de tar escondido foi exatamente o que fez o
# deploy mentir. A licao ja estava escrita aqui para o binario (o md5 no fim) e
# a arte tinha ficado de fora dela.
# O filtro do ruido de xattr fica no MAC: a sh da TV e busybox ash e nao tem
# PIPESTATUS — tentar usar la dava "bad substitution" e derrubava o deploy.
#
# E o status vem de um `set -o pipefail` local em torno do pipe, nao de indices
# de PIPESTATUS, que ficam ambiguos assim que se acrescenta um `|| true`.
if ! ( set -o pipefail
       tar czf - -C deploy/app --exclude 'art/cache' \
           --exclude 'appinfo.json.stamped' \
           art fonts icon.png icon-large.png \
         | $SSH "root@$TV_IP" "tar xzf - -C $APPDIR" ) 2>&1 \
     | grep -v 'unknown extended header keyword'; then
  :
fi
if ! $SSH "root@$TV_IP" "test -f $APPDIR/art/marcas/trakt.png"; then
  echo "    FALHOU: a arte nao chegou na TV"; exit 1
fi
rm -f deploy/app/appinfo.json.stamped
# O `core` de um crash antigo fica no diretorio do app e pesa 118 MB numa
# particao de 4,2 G. Nao serve para nada depois que o relatorio foi gerado.
$SSH "root@$TV_IP" "rm -f $APPDIR/core; rm -f /tmp/nuvio-shot-req /tmp/nuvio-shot.bmp" || true

# DONO DA PASTA DE CACHE. O tar e feito no Mac e extraido como ROOT na TV, entao
# art/cache herda o uid do Mac (13888160) com modo 755. O app roda como uid 5152
# e NAO CONSEGUE GRAVAR ali: toda imagem baixada era descartada em silencio, e os
# dois fios de decode ficavam rebaixando o que nunca poderia ser guardado.
#
# MEDIDO: 91 "decode falhou" no log com ZERO erro de rede. Depois do chown, 15 —
# e as texturas subiram de 87 para 99. Sem esta linha o defeito volta a cada
# deploy, e o sintoma e "poster que nao aparece", que ja custou meia sessao.
$SSH "root@$TV_IP" "mkdir -p $APPDIR/art/cache && chown -R 5152:5000 $APPDIR/art && chmod -R u+rwX $APPDIR/art"

echo "==> conferindo"
LOCAL=$(md5 -q nuvio-proto.arm 2>/dev/null || md5sum nuvio-proto.arm | cut -d' ' -f1)
REMOTO=$($SSH "root@$TV_IP" "md5sum $APPDIR/nuvio-proto | cut -d' ' -f1" 2>/dev/null | tr -d '\r')
if [ "$LOCAL" != "$REMOTO" ]; then
  echo "    FALHOU: local $LOCAL != TV $REMOTO"
  exit 1
fi
echo "    ok ($LOCAL)"

# launch NAO reinicia um app que ja esta rodando: so o traz para frente (ver
# FERRAMENTAS.md). Dois deploys desta tarde foram lidos no log de um processo
# antigo por isso. Matar antes; o SAM relanca o binario novo.
PID=$($SSH "root@$TV_IP" "pidof nuvio-proto" 2>/dev/null | tr -d "\r")
if [ -n "$PID" ]; then echo "==> encerrando processo antigo ($PID)"; $SSH "root@$TV_IP" "kill $PID"; sleep 2; fi
echo "==> lancando"
( sleep 2
  printf 'luna-send -n 1 -f luna://com.webos.applicationManager/launch '"'"'{"id":"%s"}'"'"'\n' "$APP_ID"
  sleep 3
  printf 'exit\n'
) | nc -w20 "$TV_IP" 23 | tr -d '\0' | grep returnValue
