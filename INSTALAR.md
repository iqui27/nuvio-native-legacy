# Instalar em outra TV

O pacote sai de:

```bash
bash tools/arm.sh --ipk
```

`space.nuvio.native.legacy_<versao>_arm.ipk`, **~47 MB**, sem credencial nenhuma
dentro — `tools/testa-ipk.sh` prova isso, e o proprio `arm.sh` aborta e apaga o
pacote se alguma voltar.

---


---

## Hisense VIDAA (Experimental)

Apps em VIDAA sao hospedadas numa URL, nao sao pacotes de sistema. O Nuvio esta em:

**`https://nuvio-recomendacoes.henriquef29.workers.dev/tv/`**

Dois builds disponiveis; a TV escolhe automaticamente:
- **Multifio (mt)** — usa SharedArrayBuffer, mais rapido se suportado
- **Um fio (st)** — fallback para TVs antigas

### Opcao A: Bookmark (mais simples)

Abre o navegador da TV e coloca essa URL nos favoritos. Nao precisa de nada.

### Opcao B: Sideload com Developer Mode

Para sistemas que exigem o app estar na lista oficial:

```bash
# Ativa Developer Mode na TV: Configuracoes > Sistema > Sobre > 1234
# Depois roda o instalador:
sudo python3 tools/vidaa-instalar/instalar.py

# Ou com IP customizado:
sudo python3 tools/vidaa-instalar/instalar.py --ip 192.168.1.100
```

Cria um servidor DNS/HTTPS local que se passa por vidaahub.com, permitindo que a TV
chame `Hisense_installApp()`. Depois de instalar, volta o DNS para automatico.

Ver `tools/vidaa-instalar/README.md` para instrucoes detalhadas.

## Precisa de root? Nao. Mas leia a ressalva do LS2.

| | Developer Mode | Homebrew Channel | Root (o caso desta TV) |
|---|---|---|---|
| Root | nao | nao, para instalar o canal | sim |
| Validade | **sessao de 50h**, renovavel | permanente | permanente |
| Conta LG de desenvolvedor | precisa | so para instalar o canal | nao |
| Instala com | `ares-install` | pela propria TV | `bash tools/arm.sh` |

### Quais versoes de webOS

webOS **4.x** e medido, numa C9. webOS **5+** e relatado funcionando por
usuarios. webOS **3.x** tem uma build experimental na branch `webos3` — a secao
de webOS 3 do [README](README.md) explica o que mudou, como foi verificado
contra dumps de simbolo de firmware retail, e as tres coisas que essa
verificacao NAO responde. Ninguem aqui tem uma TV webOS 3, entao trate como
experimento: para esses aparelhos o
[fork web](https://github.com/iqui27/NuvioTVSmart-legacy-webos) e a escolha mais
segura.

### Developer Mode

```bash
ares-setup-device                 # cadastra IP e chave do Dev Mode
ares-install space.nuvio.native.legacy_<versao>_arm.ipk -d <nome>
ares-launch space.nuvio.native.legacy -d <nome>
```

O `tools/arm.sh` **nao serve** aqui: ele usa root na porta 22 com senha, e o
`ares-install` espera `prisoner@<ip>:9922`, que e o ssh do Developer Mode.

### Homebrew Channel

Instalar o `.ipk` pelo canal. Ele poe os apps em
`/media/developer/apps/usr/palm/applications/`, que e **exatamente** de onde
este app roda hoje na TV de desenvolvimento — mesmo diretorio, mesmo ambiente.

---

## A ressalva que importa: LS2

O risco nao e a instalacao, e a **permissao de barramento**.

Este app fala direto com `luna://com.webos.media` e usa a `libAcbAPI` para ligar
o plano de video. O `src/video.c` ja registra que a chamada direta ao
`com.webos.service.tv.display` e **recusada pelo hub** mesmo na TV rooteada
("Not permitted to send to com.webos.service.tv.display"), por causa do papel
com que o app se registra — por isso ele passa pela libAcbAPI, que e o caminho
que o proprio navegador da TV usa.

E o pacote **nao leva arquivo de papel LS2**: so `appinfo.json`, com
`requiredPermissions: ["all"]`. Ou seja, ele depende do que o diretorio de
instalacao concede. Numa TV rooteada isso e permissivo. **Numa TV sem root, nao
foi medido.**

**Sintoma provavel se nao for concedido:** a interface abre normal e o video da
tela preta, com ou sem audio — que e exatamente o modo de falha que o
`video.c` descreve quando o ACB nao consegue ligar o plano.

O que joga a favor, e e evidencia real, nao promessa: nesta TV convivem
`com.limelight.webos` (Moonlight, app **nativo** com pipeline de midia) e
`org.webosbrew.hbchannel`, no mesmo diretorio. Um app nativo de video
distribuido por essa via funciona no aparelho.

**Como sair da duvida:** rodar numa TV sem root. Nao da para responder isso a
partir daqui — esta TV nao tem o app Developer Mode instalado e a porta 9922
esta fechada (conferido).

---

## O que a pessoa encontra na primeira execucao

1. **Tela de login por QR.** O codigo da conta tem 32 digitos hexadecimais, por
   isso e QR e nao codigo digitavel. A sessao fica gravada e o refresh se
   renova sozinho: loga uma vez.
2. **Escolha de perfil**, se a conta tiver mais de um.
3. Addons, ajustes de layout, chave do TMDB e progresso vem **da conta**.
4. **Trakt precisa ser vinculado na propria TV**: Ajustes -> Conta -> Trakt. A
   conta do Nuvio nao guarda credencial de Trakt (medido:
   `sync_pull_provider_credentials` devolve tmdb, mdblist, animeskip, introdb e
   os debrid, e nunca `trakt`), e o `art/trakt.txt` nao vai mais no pacote. O
   codigo do Trakt tem 8 caracteres e da para digitar no celular.
5. **Simkl** tambem pode ser vinculado, mas nenhuma tela deste app consome Simkl
   ainda — serve para a credencial chegar na conta e dali no app web.

## O que ainda incomoda

- **~47 MB**, quase tudo arte pre-assada, para a home ter o que mostrar antes
  do login. Ja foram 172 MB e a arte era do dono do pacote — quem instalava via
  o catalogo dele antes de logar. Hoje o `arm.sh` exclui todo arquivo pessoal e
  CONFERE a exclusao no pacote pronto, apagando-o se algum aparecer.
- **50h** no Developer Mode. Limite da LG, nao do app.
- O pacote usa o id `space.nuvio.native.legacy` para conviver com o app web
  (`space.nuvio.webos`) na mesma TV. Ver PORT-LEGACY.md.
