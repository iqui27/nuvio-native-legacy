# Instalar Nuvio em Hisense VIDAA

Sideload do Nuvio usando DNS spoofing. Cria um servidor DNS e HTTPS que intercepta
requisicoes para vidaahub.com e serve uma pagina de instalacao.

## Como funciona

A Hisense restringe `Hisense_installApp()` para ser chamado APENAS de um servidor que
responde por vidaahub.com. O script funciona assim:

1. Inicia um servidor DNS que responde vidaahub.com com o IP desta maquina
2. Inicia um servidor HTTPS com certificado auto-assinado
3. Quando a TV acessa https://vidaahub.com/, entrega uma pagina HTML que chama
   `Hisense_installApp()` para instalar o Nuvio
4. Voce muda o DNS da TV de volta para automatico depois

## Requisitos

- Linux, macOS ou Windows com Python 3
- Root ou privileges de administrador (portas 53 e 443)
- TV Hisense VIDAA com internet
- Developer Mode habilitado na TV

## Instalacao

```bash
sudo python3 tools/vidaa-instalar/instalar.py
```

Opcoes:

```bash
# Com IP customizado (util em redes com multiplos subnets)
sudo python3 tools/vidaa-instalar/instalar.py --ip 192.168.1.100

# Com URL customizada (se hospedado em outro lugar)
sudo python3 tools/vidaa-instalar/instalar.py --url https://seu-dominio.com/app/

# Pular DNS, apenas HTTPS (se a TV ja tem DNS manual)
sudo python3 tools/vidaa-instalar/instalar.py --https-only

# Ver todas as opcoes
python3 tools/vidaa-instalar/instalar.py --help
```

## Passo a passo na TV

1. **Ativar Developer Mode:**
   - Vá para Settings > System > About
   - Digite **1234** usando o controle remoto
   - Developer Mode deve aparecer como "ON"

2. **Mudar DNS:**
   - Vá para Settings > Network > DNS
   - Coloque o IP da maquina (o script imprime na tela)
   - Salve

3. **Instalar Nuvio:**
   - Abra o navegador da TV
   - Vá para `https://vidaahub.com/`
   - Ignore o aviso de certificado (auto-assinado)
   - Clique no botao "Install"
   - A instalacao e feita pela TV

4. **Voltar DNS para automatico:**
   - Vá para Settings > Network > DNS
   - Coloque "Automatic"

## Desinstalar

1. Abra `https://vidaahub.com/` de novo
2. Clique em "Uninstall"

(So funciona se `Hisense_uninstallApp()` estiver suportado nesta versao do VIDAA)

## Avisos de seguranca

- Certificado auto-assinado: a TV vai reclamar que nao confia. E normal.
- DNS spoofing: voce esta interceptando vidaahub.com. NINGUEM deve mudar o DNS
  da TV enquanto isso roda, ou a instalacao pode dar problema.
- Desfaca o DNS depois: mude de volta para automatic assim que terminar.

## Arquivo de certificado

Na primeira execucao, o script gera:
```
tools/vidaa-instalar/certs/vidaahub.com.crt
tools/vidaa-instalar/certs/vidaahub.com.key
```

Pode deletar e o script regenera na proxima vez.

## Referencia

Implementacao baseada em pesquisa dos repositorios:
- https://github.com/trialuser/vidaa-appstore
- https://github.com/NoobyGains/stremio-vidaa-tv

Ambos sem licenca explicita. Esta implementacao e codigo novo, escrito de scratch.

## Creditos

VIDAA e marca registrada da Hisense. Nuvio e marca registrada da NuvioMedia.
Este e um port nao-oficial.
